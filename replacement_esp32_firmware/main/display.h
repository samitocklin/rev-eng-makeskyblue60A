// GMT130-V1.0 ST7789 panel + LVGL (esp_lvgl_port)
#pragma once
#include <stdint.h>
#include "lvgl.h"

lv_display_t *display_init(uint8_t backlight_pct);
void display_backlight(uint8_t pct);
