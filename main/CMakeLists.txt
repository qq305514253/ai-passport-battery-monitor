idf_component_register(
    SRCS "main.c"
         "bms_ble.c"
         "jbd_protocol.c"
         "battery_format.c"
    INCLUDE_DIRS "."
    REQUIRES bsp bt esp_timer nvs_flash
)
