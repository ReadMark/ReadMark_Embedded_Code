#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <cJSON.h>
#include <sys/param.h>
#include <nvs_flash.h>
#include "unistd.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_wps.h"
#include "esp_http_client.h"

#include "driver/uart.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define BUF_SIZE 1024 // 입력 버퍼 사이즈
#define FLASH_PIN 4   // Flash 핀 설정

#define BOARD_ESP32CAM_AITHINKER // 해당 핀맵을 사용한다고 선언

// 카메라 초기화 핀맵 (ESP32-CAM)
#ifdef BOARD_ESP32CAM_AITHINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#endif

// LOG용 TAG 모음
static const char *captureTag = "Take_Capture";
static const char *flashTimerTag = "Flash_Timer";
static const char *flashChannelTag = "Flash_Channel";

// 함수의 원형
void cJsonParsing(esp_http_client_handle_t client);
static esp_err_t init_camera(void);

// 카메라 설정
#if ESP_CAMERA_SUPPORTED
static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sccb_sda = SIOD_GPIO_NUM,
    .pin_sccb_scl = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    .xclk_freq_hz = 20000000, // 20 MHz
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG, // 웹스트리밍/스냅샷 용
    .frame_size = FRAMESIZE_SVGA,   // SVGA, XGA
    .jpeg_quality = 10,             // 0(최고)~63(최저)
    .fb_count = 2,                  // 더 크게 하면 프레임 안정 (2가 빨랐음)
    .grab_mode = CAMERA_GRAB_LATEST,
    .fb_location = CAMERA_FB_IN_PSRAM,
};

static void tune_sensor_for_quality(void)
{
    sensor_t *s = esp_camera_sensor_get();

    // 자동 제어 (기본 On 권장)
    s->set_whitebal(s, 1);      // AWB
    s->set_exposure_ctrl(s, 1); // AEC
    s->set_gain_ctrl(s, 1);     // AGC
    s->set_ae_level(s, -1);
    s->set_gainceiling(s, GAINCEILING_16X);
    s->set_aec2(s, 1);

    // 렌즈/픽셀 보정 (체감효과 큼)
    s->set_lenc(s, 1); // Lens correction(비네팅 완화)
    s->set_bpc(s, 1);  // Bad Pixel Correction
    s->set_wpc(s, 1);  // White Pixel Correction

    // 톤/선명도 (상황 맞춰 살짝)
    s->set_brightness(s, 0); // -2~2
    s->set_contrast(s, 1);   // -2~2 (텍스트 대비↑에 도움)
    s->set_saturation(s, 0); // -2~2
    s->set_whitebal(s, 0);
    // (센서에 따라 지원될 때만)
    if (s->set_sharpness)
        s->set_sharpness(s, 2); // -2~2
}
#endif

// 통신 설정
const uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
};

// 타이머 설정
const ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_13_BIT,
    .timer_num = LEDC_TIMER_1,
    .freq_hz = 5000,
    .clk_cfg = LEDC_AUTO_CLK,
};

// 채널 설정
const ledc_channel_config_t ledc_channel = {
    .gpio_num = FLASH_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_1,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER_1,
    .duty = 0,
    .hpoint = 0,
};

// 카메라 초기화
static esp_err_t init_camera(void)
{
    esp_err_t err = esp_camera_init(&camera_config); // 설정값 넘기고 상태 받음

    if (err != ESP_OK)
    {
        ESP_LOGE(captureTag, "카메라 초기화 실패");
        return err;
    }

    return ESP_OK;
}

void app_main(void)
{
    // 기본 Uart 통신 설정
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, BUF_SIZE, 0, 0, NULL, 0);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // GPIO 설정
    gpio_pad_select_gpio(FLASH_PIN);
    gpio_set_direction(FLASH_PIN, GPIO_MODE_OUTPUT);

    // ledc timer 설정
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK)
        ESP_LOGE(flashTimerTag, "ERROR : %d", ret);

    // ledc channel 설정
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK)
        ESP_LOGE(flashChannelTag, "ERROR : %d", ret);

    uint8_t uart_buff[BUF_SIZE + 1] = {0};

#if ESP_CAMERA_SUPPORTED // 판 맵 설정이 ESP_CAMERA_SUPPORTED라면 실행
    if (ESP_OK != init_camera())
    {
        return;
    }
    tune_sensor_for_quality();

    while (1)
    {
        // Flash 코드
        int len = uart_read_bytes(UART_NUM_0, uart_buff, BUF_SIZE, 100 / portTICK_PERIOD_MS);

        if (len > 0 && len < BUF_SIZE)
        {
            uart_buff[len] = '\0';
            char *str = (char *)uart_buff;
            printf("input : {%s}\n", str);

            if (str[0] == '1')
            {
                // 카메라 사진 촬영
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 3500));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
                vTaskDelay(pdMS_TO_TICKS(50));

                camera_fb_t *pic = esp_camera_fb_get();

                ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));

                if (!pic)
                {
                    ESP_LOGE(captureTag, "사진 촬영 실패");
                }
                esp_camera_fb_return(pic->buf);
            }
        }

        else
        {
            uart_buff[BUF_SIZE] = '\0';
        }
    }
}

// 보드 고려 조건문
#else
    ESP_LOGE(captureTag, "이 보드는 카메라 지원이 안됩니다 .");
    return;
#endif