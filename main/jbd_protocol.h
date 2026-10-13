#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define JBD_MAX_CELLS 32
#define JBD_RX_CAPACITY 128
typedef struct { uint16_t voltage_mv; int32_t current_ma; uint32_t remaining_mah; uint32_t nominal_mah; uint16_t cycles; uint8_t soc; uint8_t mos_state; uint8_t cell_count; uint16_t cell_mv[JBD_MAX_CELLS]; uint8_t cells_received; bool basic_valid; bool cells_valid; } jbd_battery_data_t;
typedef struct { uint8_t bytes[JBD_RX_CAPACITY]; size_t length; } jbd_stream_t;
typedef enum { JBD_FEED_NONE=0, JBD_FEED_UPDATED, JBD_FEED_BAD_FRAME } jbd_feed_result_t;
extern const uint8_t JBD_QUERY_BASIC[7];
extern const uint8_t JBD_QUERY_CELLS[7];
extern const uint8_t JBD_QUERY_VERSION[7];
void jbd_stream_init(jbd_stream_t *stream);
jbd_feed_result_t jbd_stream_feed(jbd_stream_t *stream, const uint8_t *bytes, size_t length, jbd_battery_data_t *data);
