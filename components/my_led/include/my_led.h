#pragma once

#include <stdbool.h>

void my_led_init(int gpio);
void my_led_set_override(bool active);
bool my_led_get_override(void);
void my_led_blink(int times, int delay_ms);
void my_led_set_wifi_status(bool connected);
void my_led_set_dmx_error(bool error);
