#pragma once
#include "esp_err.h"
#include "jbd_protocol.h"
#include <stdbool.h>
#include <stdint.h>
typedef enum { BMS_BLE_STARTING=0,BMS_BLE_SCANNING,BMS_BLE_CONNECTING,BMS_BLE_DISCOVERING,BMS_BLE_ONLINE,BMS_BLE_OFFLINE,BMS_BLE_ERROR } bms_ble_state_t;
typedef struct { bms_ble_state_t state; int error; int8_t rssi; uint32_t age_ms; uint32_t bad_frames; jbd_battery_data_t battery; } bms_ble_snapshot_t;
esp_err_t bms_ble_start(void);
bool bms_ble_get_snapshot(bms_ble_snapshot_t *snapshot);
