#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void battery_format_fixed(char *out, size_t size, int64_t value,
                          unsigned scale, unsigned decimals,
                          const char *unit, bool show_positive_sign);
