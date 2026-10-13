#include "bms_ble.h"
#include "battery_format.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>

#define INPUT_QUEUE_DEPTH 8
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; } input_event_t;
static const char *TAG="battery_monitor";
static QueueHandle_t s_input_queue;
static lv_obj_t *s_soc,*s_soc_bar,*s_voltage,*s_current,*s_power,*s_capacity,*s_status,*s_cells,*s_own_battery;
static const lv_font_t *const CJK_FONT=&lv_font_source_han_sans_sc_16_cjk;

static const char *state_text(bms_ble_state_t state){switch(state){case BMS_BLE_SCANNING:return "查找中";case BMS_BLE_CONNECTING:return "接入中";case BMS_BLE_DISCOVERING:return "查找中";case BMS_BLE_ONLINE:return "已接";case BMS_BLE_OFFLINE:return "重接中";case BMS_BLE_ERROR:return "未接";default:return "未接";}}
static void update_ui(lv_timer_t *timer){
 (void)timer;bms_ble_snapshot_t s;if(!bms_ble_get_snapshot(&s))return;
 if(s.state==BMS_BLE_ONLINE)lv_label_set_text_fmt(s_status,"已接  %d dBm",s.rssi);else lv_label_set_text(s_status,state_text(s.state));
 int own=bsp_battery_soc();if(own>=0){lv_obj_remove_flag(s_own_battery,LV_OBJ_FLAG_HIDDEN);lv_label_set_text_fmt(s_own_battery,"本机%d%%",own);}else lv_obj_add_flag(s_own_battery,LV_OBJ_FLAG_HIDDEN);
 if(!s.battery.basic_valid||s.age_ms>5000){lv_label_set_text(s_soc,"--%");lv_bar_set_value(s_soc_bar,0,LV_ANIM_OFF);lv_label_set_text(s_voltage,"--.-- V\n电位");lv_label_set_text(s_current,"--.-- A\n电流");lv_label_set_text(s_power,"-- W   未接");lv_label_set_text(s_capacity,"容量  --.-- / --.-- Ah");lv_label_set_text(s_cells,"查找中");return;}
 char text[96],a[24],b[24];
 lv_label_set_text_fmt(s_soc,"%u%%",s.battery.soc);lv_bar_set_value(s_soc_bar,s.battery.soc,LV_ANIM_ON);
 battery_format_fixed(a,sizeof a,s.battery.voltage_mv,1000,2,"V",false);snprintf(text,sizeof text,"%s\n电位",a);lv_label_set_text(s_voltage,text);
 battery_format_fixed(a,sizeof a,s.battery.current_ma,1000,2,"A",true);snprintf(text,sizeof text,"%s\n电流",a);lv_label_set_text(s_current,text);
 int64_t watts=(int64_t)s.battery.voltage_mv*s.battery.current_ma/1000000;const char *mode=s.battery.current_ma>100?"充电":s.battery.current_ma<-100?"放电":"静置";snprintf(text,sizeof text,"%+lld W   %s",(long long)watts,mode);lv_label_set_text(s_power,text);
 battery_format_fixed(a,sizeof a,s.battery.remaining_mah,1000,2,"",false);battery_format_fixed(b,sizeof b,s.battery.nominal_mah,1000,2,"Ah",false);snprintf(text,sizeof text,"容量  %s / %s",a,b);lv_label_set_text(s_capacity,text);
 if(s.battery.cells_valid&&s.battery.cells_received){uint16_t lo=65535,hi=0;for(unsigned i=0;i<s.battery.cells_received;i++){if(s.battery.cell_mv[i]<lo)lo=s.battery.cell_mv[i];if(s.battery.cell_mv[i]>hi)hi=s.battery.cell_mv[i];}snprintf(text,sizeof text,"%u节  低%u.%03uV  高%u.%03uV\n差 %umV",s.battery.cells_received,lo/1000,lo%1000,hi/1000,hi%1000,(unsigned)(hi-lo));lv_label_set_text(s_cells,text);}
}
static lv_obj_t *label(lv_obj_t *parent,const char *text,const lv_font_t *font,uint32_t color){lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,text);lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);return o;}
static void build_ui(void){
 lv_obj_t *scr=lv_obj_create(NULL);lv_obj_set_style_bg_color(scr,lv_color_hex(0x071827),0);lv_obj_set_style_pad_all(scr,0,0);
 lv_obj_t *title=label(scr,"黄金电池 B",CJK_FONT,0xA8D8FF);lv_obj_align(title,LV_ALIGN_TOP_LEFT,14,10);
 s_own_battery=label(scr,"",CJK_FONT,0x9BE58C);lv_obj_align(s_own_battery,LV_ALIGN_TOP_RIGHT,-14,10);
 s_status=label(scr,"未接",CJK_FONT,0x6FB6E8);lv_obj_align(s_status,LV_ALIGN_TOP_LEFT,14,36);
 s_soc=label(scr,"--%",&lv_font_montserrat_48,0xE9FF70);lv_obj_align(s_soc,LV_ALIGN_TOP_MID,0,58);
 s_soc_bar=lv_bar_create(scr);lv_obj_set_size(s_soc_bar,210,8);lv_obj_align(s_soc_bar,LV_ALIGN_TOP_MID,0,113);lv_bar_set_range(s_soc_bar,0,100);lv_bar_set_value(s_soc_bar,0,LV_ANIM_OFF);lv_obj_set_style_bg_color(s_soc_bar,lv_color_hex(0x16384C),LV_PART_MAIN);lv_obj_set_style_bg_color(s_soc_bar,lv_color_hex(0xB8E34B),LV_PART_INDICATOR);lv_obj_set_style_radius(s_soc_bar,4,LV_PART_MAIN);lv_obj_set_style_radius(s_soc_bar,4,LV_PART_INDICATOR);
 s_voltage=label(scr,"--.-- V\n电位",CJK_FONT,0xFFFFFF);lv_obj_set_width(s_voltage,108);lv_obj_set_style_text_align(s_voltage,LV_TEXT_ALIGN_CENTER,0);lv_obj_align(s_voltage,LV_ALIGN_TOP_LEFT,8,132);
 s_current=label(scr,"--.-- A\n电流",CJK_FONT,0xFFFFFF);lv_obj_set_width(s_current,108);lv_obj_set_style_text_align(s_current,LV_TEXT_ALIGN_CENTER,0);lv_obj_align(s_current,LV_ALIGN_TOP_RIGHT,-8,132);
 s_power=label(scr,"-- W   未接",CJK_FONT,0x6FE7D5);lv_obj_align(s_power,LV_ALIGN_TOP_MID,0,180);
 s_capacity=label(scr,"容量  --.-- / --.-- Ah",CJK_FONT,0xC5D4DF);lv_obj_align(s_capacity,LV_ALIGN_TOP_MID,0,207);
 s_cells=label(scr,"查找中",CJK_FONT,0x8EB4D0);lv_obj_set_width(s_cells,220);lv_obj_set_style_text_align(s_cells,LV_TEXT_ALIGN_CENTER,0);lv_obj_align(s_cells,LV_ALIGN_TOP_MID,0,236);
 lv_obj_t *hint=label(scr,"自动重接",CJK_FONT,0x58758A);lv_obj_align(hint,LV_ALIGN_BOTTOM_MID,0,-8);
 lv_timer_create(update_ui,500,NULL);lv_screen_load(scr);
}
static void input_task(void *arg){(void)arg;input_event_t e;for(;;)if(xQueueReceive(s_input_queue,&e,portMAX_DELAY)==pdTRUE&&e.event==BSP_BTN_CLICK)bsp_display_backlight(100);}
static void on_key(bsp_btn_t button,bsp_btn_ev_t event,void *user){(void)user;if(s_input_queue){input_event_t e={button,event};(void)xQueueSend(s_input_queue,&e,0);}}
void app_main(void){ESP_LOGI(TAG,"Gotion monitor starting (single-pack validation build)");(void)bsp_i2c_init();if(bsp_display_init()!=ESP_OK||!bsp_lvgl_init()){ESP_LOGE(TAG,"display init failed");return;}bsp_display_backlight(100);(void)bsp_battery_init();s_input_queue=xQueueCreate(INPUT_QUEUE_DEPTH,sizeof(input_event_t));if(!s_input_queue||xTaskCreate(input_task,"monitor_input",3072,NULL,5,NULL)!=pdPASS||bsp_button_init(on_key,NULL)!=ESP_OK){ESP_LOGE(TAG,"button init failed");return;}if(bsp_lvgl_lock(1000)){build_ui();bsp_lvgl_unlock();}esp_err_t err=bms_ble_start();if(err!=ESP_OK)ESP_LOGE(TAG,"BLE start failed: %s",esp_err_to_name(err));}
