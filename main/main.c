#include "oled_display.h"
#include "sensors.h"
#include "wifi_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "MAIN";

#define TOUCH_HOLD_COUNT 200 // 200 * 10ms = 2초

void app_main(void)
{
    ESP_LOGI(TAG, "ReadMark start");
    oled_init();
    oled_clear(0xFFFF);

    wifi_init();

    sensors_init();
    int touch_hold_counter = 0;

    while (1)
    {
        // 터치센서 체크
        if (is_touch_pressed())
        {
            touch_hold_counter++;
            if (touch_hold_counter > TOUCH_HOLD_COUNT)
            {
                ESP_LOGI(TAG, "전원을 끕니다..");
                esp_deep_sleep_start();
            }
        }
        else
        {
            touch_hold_counter = 0;
        }

        // 압력센서 체크
        if (is_book_closed())
        {
            ESP_LOGI(TAG, "Book is closed!");
        }
        else
        {
            ESP_LOGI(TAG, "Book is open!");
        }

        vTaskDelay(pdMS_TO_TICKS(10));  
    }
}