#include "battery_format.h"

#include <stdio.h>

void battery_format_fixed(char *out, size_t size, int64_t value,
                          unsigned scale, unsigned decimals,
                          const char *unit, bool show_positive_sign)
{
    uint64_t magnitude = value < 0 ? (uint64_t)(-value) : (uint64_t)value;
    uint64_t whole = magnitude / scale;
    uint64_t fraction = magnitude % scale;
    unsigned divisor = scale;

    for (unsigned i = 0; i < decimals; ++i) {
        divisor /= 10U;
    }
    fraction /= divisor;
    if (unit[0] != '\0') {
        snprintf(out, size, "%s%llu.%0*llu %s",
                 value < 0 ? "-" : show_positive_sign ? "+" : "",
                 (unsigned long long)whole, (int)decimals,
                 (unsigned long long)fraction, unit);
    } else {
        snprintf(out, size, "%s%llu.%0*llu",
                 value < 0 ? "-" : show_positive_sign ? "+" : "",
                 (unsigned long long)whole, (int)decimals,
                 (unsigned long long)fraction);
    }
}
