#include "sensors.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/adc.h"
#include "driver/touch_pad.h"
#include "esp_log.h"

static const char *TAG = "SENSORS";

#define TOUCH_PIN 4       // GPIO 4
#define PRESSURE_ADC ADC_CHANNEL_6 // GPIO34

static adc_oneshot_unit_handle_t adc1_handle;

void sensors_init(void)
{
    ESP_LOGI(TAG, "Init sensors");

    // 터치 init
    touch_pad_init();
    touch_pad_config(TOUCH_PIN, 0);

    // ADC init (oneshot)
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_cfg, &adc1_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    adc_oneshot_config_channel(adc1_handle, PRESSURE_ADC, &chan_cfg);
}

int read_touch_sensor(void)
{
    uint16_t touch_val;
    touch_pad_read(TOUCH_PIN, &touch_val);
    return (int)touch_val;
}

int read_pressure_sensor(void)
{
    int raw = 0;
    adc_oneshot_read(adc1_handle, PRESSURE_ADC, &raw);
    return raw;
}
