#pragma once
#include <stdint.h>
#include "esp_err.h"

esp_err_t oled_init(void);
void oled_clear(uint16_t color);
void oled_draw_string(int x, int y, const char *str, uint16_t color);