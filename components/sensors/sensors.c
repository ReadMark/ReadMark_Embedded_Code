#include "sensors.h"
#include "driver/adc.h"
#include "driver/touch_pad.h"
#include "esp_log.h"
#include <stdio.h>
#include <sys/time.h>
#include "sdkconfig.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"

// 터치센서 설정
#define TOUCH_PIN GPIO_NUM_4
#define TOUCH_THRESHOLD 2000  // 터치 인식 임계값
#define TOUCH_HOLD_COUNT 200  // 200 * 10ms = 2초 이상 눌렀을 때

// 압력센서 설정
#define PRESSURE_ADC ADC1_CHANNEL_6
#define PRESSURE_THRESHOLD 3000

// 슬립모드 설정
#define WAKE_PIN GPIO_NUM_33

void sensors_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << TOUCH_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(PRESSURE_ADC, ADC_ATTEN_DB_12);
}

bool is_touch_pressed(void)
{
    return gpio_get_level(TOUCH_PIN) == 1;
}

// 압력센서 읽기
int read_pressure_sensor(void)
{
    return adc1_get_raw(PRESSURE_ADC);
}

bool is_book_closed(void)
{
    int raw = read_pressure_sensor();
    return raw > PRESSURE_THRESHOLD;
}

void sleep_mode(void)
{
    gpio_pullup_en(WAKE_PIN); // 풀업

    esp_sleep_enable_ext0_wakeup(WAKE_PIN, 1);

    esp_deep_sleep_start();
}