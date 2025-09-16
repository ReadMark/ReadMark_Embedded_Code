#include "sensors.h"
#include "driver/adc.h"
#include "driver/touch_pad.h"
#include "esp_log.h"

static const char *TAG = "SENSORS";

// 터치센서 설정
#define TOUCH_PIN 4
#define TOUCH_THRESHOLD 2000  // 터치 인식 임계값
#define TOUCH_HOLD_COUNT 200  // 200 * 10ms = 2초 이상 눌렀을 때

// 압력센서 설정
#define PRESSURE_ADC ADC1_CHANNEL_6
#define PRESSURE_THRESHOLD 3000

void sensors_init(void)
{
    ESP_LOGI(TAG, "Init sensors");

    // 터치 init
    touch_pad_init();
    touch_pad_config(TOUCH_PIN, 0);

    // ADC init
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(PRESSURE_ADC, ADC_ATTEN_DB_12);
}

int read_touch_sensor(void)
{
    uint16_t touch_val;
    touch_pad_read(TOUCH_PIN, &touch_val);
    return (int)touch_val;
}

bool is_touch_pressed(void)
{
    int val = read_touch_sensor();
    return val < TOUCH_THRESHOLD;
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