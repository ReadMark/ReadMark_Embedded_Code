#include "oled_display.h"
#include "sensors.h"
#include "wifi_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

// static int mode = 0, battery = 0, last_page = 0, pages_today = 0;

void app_main(void)
{
    ESP_LOGI(TAG, "ReadMark start");
    wifi_init();
    oled_init();
    sensors_init();

    oled_clear(0x0000);
    oled_draw_string(10, 10, "Hello ESP32", 0xFFFF);
}