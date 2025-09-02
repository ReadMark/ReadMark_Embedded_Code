#include "header.h"

#define BOARD_ESP32CAM_AITHINKER // 해당 핀맵을 사용한다고 선언

// LOG용 TAG 모음
static const char *captureTag = "Take_Capture";

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

void tune_sensor_for_quality(void)
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

// 카메라 초기화
esp_err_t init_camera(void)
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
#if ESP_CAMERA_SUPPORTED // 판 맵 설정이 ESP_CAMERA_SUPPORTED라면 실행
    if (ESP_OK != init_camera())
    {
        return;
    }
    tune_sensor_for_quality();
#else
    ESP_LOGE(captureTag, "이 보드는 카메라 지원이 안됩니다 .");
    return;
#endif
}