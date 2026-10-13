#include "bms_ble.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include <string.h>

static const char *TAG="bms_ble";
static const char TARGET_NAME[]="03HMB0GL0200CFG180600673";
static const ble_uuid16_t SERVICE_UUID=BLE_UUID16_INIT(0xFF00);
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_host_task,s_poll_task;
static bms_ble_snapshot_t s_snapshot;
static jbd_stream_t s_stream;
static uint8_t s_own_addr_type;
static uint16_t s_conn=BLE_HS_CONN_HANDLE_NONE,s_svc_start,s_svc_end,s_notify,s_write,s_cccd;
static int64_t s_last_data_us;

static int gap_event(struct ble_gap_event *event,void *arg);
static void scan_start(void);
static void set_state(bms_ble_state_t state,int error){if(s_lock&&xSemaphoreTake(s_lock,pdMS_TO_TICKS(20))==pdTRUE){s_snapshot.state=state;s_snapshot.error=error;xSemaphoreGive(s_lock);}}

static void disconnect_for_retry(int error){set_state(BMS_BLE_ERROR,error);if(s_conn!=BLE_HS_CONN_HANDLE_NONE)(void)ble_gap_terminate(s_conn,BLE_ERR_REM_USER_CONN_TERM);else scan_start();}

static int subscribe_cb(uint16_t conn,const struct ble_gatt_error *error,struct ble_gatt_attr *attr,void *arg){(void)conn;(void)attr;(void)arg;if(error->status!=0){disconnect_for_retry(error->status);return 0;}set_state(BMS_BLE_ONLINE,0);return 0;}
static int dsc_cb(uint16_t conn,const struct ble_gatt_error *error,uint16_t chr_val,const struct ble_gatt_dsc *dsc,void *arg){(void)chr_val;(void)arg;if(error->status==0&&dsc){if(ble_uuid_u16(&dsc->uuid.u)==0x2902)s_cccd=dsc->handle;return 0;}if(error->status==BLE_HS_EDONE&&s_cccd){uint16_t enable=1;int rc=ble_gattc_write_flat(conn,s_cccd,&enable,sizeof enable,subscribe_cb,NULL);if(rc)disconnect_for_retry(rc);return 0;}disconnect_for_retry(error->status);return 0;}
static int chr_cb(uint16_t conn,const struct ble_gatt_error *error,const struct ble_gatt_chr *chr,void *arg){(void)arg;if(error->status==0&&chr){uint16_t uuid=ble_uuid_u16(&chr->uuid.u);if(uuid==0xFF01)s_notify=chr->val_handle;else if(uuid==0xFF02)s_write=chr->val_handle;return 0;}if(error->status==BLE_HS_EDONE&&s_notify&&s_write){int rc=ble_gattc_disc_all_dscs(conn,s_notify,s_svc_end,dsc_cb,NULL);if(rc)disconnect_for_retry(rc);return 0;}disconnect_for_retry(error->status);return 0;}
static int svc_cb(uint16_t conn,const struct ble_gatt_error *error,const struct ble_gatt_svc *svc,void *arg){(void)arg;if(error->status==0&&svc){s_svc_start=svc->start_handle;s_svc_end=svc->end_handle;return 0;}if(error->status==BLE_HS_EDONE&&s_svc_start){int rc=ble_gattc_disc_all_chrs(conn,s_svc_start,s_svc_end,chr_cb,NULL);if(rc)disconnect_for_retry(rc);return 0;}disconnect_for_retry(error->status);return 0;}

static bool target_adv(const struct ble_gap_disc_desc *disc){struct ble_hs_adv_fields f;if(ble_hs_adv_parse_fields(&f,disc->data,disc->length_data)!=0||!f.name)return false;return f.name_len==strlen(TARGET_NAME)&&memcmp(f.name,TARGET_NAME,f.name_len)==0;}
static void scan_start(void){if(ble_gap_disc_active())return;struct ble_gap_disc_params p={0};p.filter_duplicates=1;p.passive=0;int rc=ble_gap_disc(s_own_addr_type,BLE_HS_FOREVER,&p,gap_event,NULL);set_state(rc?BMS_BLE_ERROR:BMS_BLE_SCANNING,rc);}

static int gap_event(struct ble_gap_event *event,void *arg){(void)arg;switch(event->type){
 case BLE_GAP_EVENT_DISC:if(target_adv(&event->disc)){if(s_lock&&xSemaphoreTake(s_lock,0)==pdTRUE){s_snapshot.rssi=event->disc.rssi;xSemaphoreGive(s_lock);}ble_addr_t addr=event->disc.addr;(void)ble_gap_disc_cancel();set_state(BMS_BLE_CONNECTING,0);int rc=ble_gap_connect(s_own_addr_type,&addr,10000,NULL,gap_event,NULL);if(rc){s_conn=BLE_HS_CONN_HANDLE_NONE;scan_start();}}return 0;
 case BLE_GAP_EVENT_CONNECT:if(event->connect.status){s_conn=BLE_HS_CONN_HANDLE_NONE;scan_start();return 0;}s_conn=event->connect.conn_handle;s_svc_start=s_svc_end=s_notify=s_write=s_cccd=0;jbd_stream_init(&s_stream);set_state(BMS_BLE_DISCOVERING,0);{int rc=ble_gattc_disc_svc_by_uuid(s_conn,&SERVICE_UUID.u,svc_cb,NULL);if(rc)disconnect_for_retry(rc);}return 0;
 case BLE_GAP_EVENT_DISCONNECT:s_conn=BLE_HS_CONN_HANDLE_NONE;set_state(BMS_BLE_OFFLINE,event->disconnect.reason);scan_start();return 0;
 case BLE_GAP_EVENT_NOTIFY_RX:if(event->notify_rx.attr_handle==s_notify){uint8_t buf[96];uint16_t len=0;if(ble_hs_mbuf_to_flat(event->notify_rx.om,buf,sizeof buf,&len)==0&&s_lock&&xSemaphoreTake(s_lock,pdMS_TO_TICKS(20))==pdTRUE){jbd_feed_result_t r=jbd_stream_feed(&s_stream,buf,len,&s_snapshot.battery);if(r==JBD_FEED_UPDATED)s_last_data_us=esp_timer_get_time();else if(r==JBD_FEED_BAD_FRAME)s_snapshot.bad_frames++;xSemaphoreGive(s_lock);}}return 0;
 default:return 0;}}
static void on_reset(int reason){set_state(BMS_BLE_ERROR,reason);}
static void on_sync(void){int rc=ble_hs_util_ensure_addr(0);if(!rc)rc=ble_hs_id_infer_auto(0,&s_own_addr_type);if(rc)set_state(BMS_BLE_ERROR,rc);else scan_start();}
static void host_task(void *arg){(void)arg;nimble_port_run();vTaskDelete(NULL);}
static void poll_task(void *arg){(void)arg;unsigned phase=0;for(;;){if(s_conn!=BLE_HS_CONN_HANDLE_NONE&&s_write&&s_snapshot.state==BMS_BLE_ONLINE){const uint8_t *q=(phase++%5==4)?JBD_QUERY_CELLS:JBD_QUERY_BASIC;int rc=ble_gattc_write_no_rsp_flat(s_conn,s_write,q,7);if(rc)ESP_LOGW(TAG,"query failed rc=%d",rc);}vTaskDelay(pdMS_TO_TICKS(1000));}}
esp_err_t bms_ble_start(void){s_lock=xSemaphoreCreateMutex();if(!s_lock)return ESP_ERR_NO_MEM;s_snapshot=(bms_ble_snapshot_t){.state=BMS_BLE_STARTING,.rssi=-127};jbd_stream_init(&s_stream);esp_err_t e=nvs_flash_init();if(e!=ESP_OK)return e;e=nimble_port_init();if(e!=ESP_OK)return e;ble_hs_cfg.reset_cb=on_reset;ble_hs_cfg.sync_cb=on_sync;if(xTaskCreatePinnedToCore(host_task,"nimble_host",NIMBLE_HS_STACK_SIZE,NULL,configMAX_PRIORITIES-4,&s_host_task,NIMBLE_CORE)!=pdPASS)return ESP_ERR_NO_MEM;if(xTaskCreate(poll_task,"bms_poll",3072,NULL,5,&s_poll_task)!=pdPASS)return ESP_ERR_NO_MEM;return ESP_OK;}
bool bms_ble_get_snapshot(bms_ble_snapshot_t *out){if(!out||!s_lock||xSemaphoreTake(s_lock,pdMS_TO_TICKS(50))!=pdTRUE)return false;*out=s_snapshot;if(s_last_data_us)out->age_ms=(uint32_t)((esp_timer_get_time()-s_last_data_us)/1000);xSemaphoreGive(s_lock);return true;}
