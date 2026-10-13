#include "battery_format.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    char text[32];

    battery_format_fixed(text, sizeof(text), 79010, 1000, 2, "V", false);
    assert(strcmp(text, "79.01 V") == 0);
    battery_format_fixed(text, sizeof(text), 0, 1000, 2, "A", true);
    assert(strcmp(text, "+0.00 A") == 0);
    battery_format_fixed(text, sizeof(text), -40, 1000, 2, "A", true);
    assert(strcmp(text, "-0.04 A") == 0);
    battery_format_fixed(text, sizeof(text), 11600, 1000, 2, "Ah", false);
    assert(strcmp(text, "11.60 Ah") == 0);
    battery_format_fixed(text, sizeof(text), 11600, 1000, 2, "", false);
    assert(strcmp(text, "11.60") == 0);
    return 0;
}
