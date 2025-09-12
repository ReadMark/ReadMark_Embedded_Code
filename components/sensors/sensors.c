#include "sensors.h"
#include "driver/adc.h"
#include "driver/touch_pad.h"
#include "esp_log.h"

static const char *TAG = "SENSORS";

#define TOUCH_PIN 4       // GPIO 4
#define PRESSURE_ADC ADC1_CHANNEL_6 // GPIO34 (ADC1 채널 6)

void sensors_init(void)
{
    ESP_LOGI(TAG, "Init sensors");

    // 터치 init
    touch_pad_init();
    touch_pad_config(TOUCH_PIN, 0);

    // ADC init (legacy API)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(PRESSURE_ADC, ADC_ATTEN_DB_12);
}

int read_touch_sensor(void)
{
    uint16_t touch_val;
    touch_pad_read(TOUCH_PIN, &touch_val);
    return (int)touch_val;
}

int read_pressure_sensor(void)
{
    int raw = adc1_get_raw(PRESSURE_ADC);
    return raw;
}
