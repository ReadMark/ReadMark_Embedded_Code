#include "header.h"

#define BUF_SIZE 1024 // 입력 버퍼 사이즈
#define FLASH_PIN 4   // Flash 핀 설정

#define MAX_JSON_BODY 1024 // 서버 응답 메시지 제한

// 서버 통신 설정
static EventGroupHandle_t s_wifi_evt; // 핸들의 이벤트를 담는 변수

#define WIFI_CONNECTED_BIT BIT0  // AP에 연결됨
#define WIFI_GOTIP_BIT BIT1      // DHCP로 IP 획득
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
static const char *WifiConfigTag = "Wifi_config";
static const char *sendPhotoTag = "Send_Photo";
static const char *cJsonParsingTag = "cJsonParsing";

// 네트워크 ID, Password
const char *ssid = "ORBI96";
const char *password = "moderncurtain551";

char *cJsonBuffer;                                 // cJson 파싱 값 저장 버퍼
char url[128] = "http://192.168.1.51:5000/upload"; // 서버 접속 URL

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

// 네트워크 핸들러
void wifi_event_handler(void *handler_arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(WifiConfigTag, "STA시작 -> AP접속 시도");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
        {
            wifi_event_sta_connected_t *e = (wifi_event_sta_connected_t *)event_data;
            ESP_LOGI(WifiConfigTag, "AP연결됨 : ssid : %s, channel : %d", (char *)e->ssid, e->channel);
            xEventGroupSetBits(s_wifi_evt, WIFI_CONNECTED_BIT);
            break;
        }

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t *e = (wifi_event_sta_disconnected_t *)event_data;
            ESP_LOGE(WifiConfigTag, "AP연결 해제 :(해제 이유 : %d)", e->reason);
            xEventGroupClearBits(s_wifi_evt, WIFI_CONNECTED_BIT | WIFI_GOTIP_BIT); // 두 비트 모두 연결 없음으로 클리어
            esp_wifi_connect();
            break;
        }

        case WIFI_EVENT_AP_START:
            ESP_LOGI(WifiConfigTag, "SoftAP 시작");
            break;

        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(WifiConfigTag, "클라이언트 접속 :" MACSTR ", AID = %d", MAC2STR(e->mac), e->aid);
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(WifiConfigTag, "클라이언트 해제: " MACSTR ", AID=%d", MAC2STR(e->mac), e->aid);
            break;
        }
        }
    }
    else if (base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
        {
            ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(WifiConfigTag, "IP : " IPSTR, IP2STR(&e->ip_info.ip));
            xEventGroupSetBits(s_wifi_evt, WIFI_GOTIP_BIT);
            break;
        }

        case IP_EVENT_STA_LOST_IP:
            ESP_LOGI(WifiConfigTag, "IP손실");
            xEventGroupClearBits(s_wifi_evt, WIFI_GOTIP_BIT);
            break;
        }
    }
}

// 와이파이 초기화
void wifi_init(void)
{
    s_wifi_evt = xEventGroupCreate();
    ESP_ERROR_CHECK(nvs_flash_init());                // nvs 초기화 (Wi-Fi 설정을 저장)
    ESP_ERROR_CHECK(esp_netif_init());                // TCP/IP 네트워크 인터페이스 초기화 (lwIP 스택 준비)
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // 이벤트 루프를 만들고 Wi-Fi 핸들러를 위해 준비

    esp_netif_create_default_wifi_sta(); // 기본 Wi-Fi STA 인터페이스 생성

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT(); // Wi-Fi 드라이버 초기화 (기본 설정으로 초기화)
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    // 이벤트 핸들러
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL)); // 모든 Wi-Fi 이벤트 처리
    // 현재 핸들러에서 (IP_EVENT_STA_GOT_IP)를 처리하는 부분이 없음 사용 고려
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL)); // IP 획득 이벤트 처리

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "ORBI96",
            .password = "moderncurtain551",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // 연결할 AP의 최소 인증 방식 지정
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));               // STA 모드로 설정
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); // SSID/PASS 설정 적용
    ESP_ERROR_CHECK(esp_wifi_start());                               // Wi-Fi 연결

    ESP_LOGI(WifiConfigTag, "wifi_init finished. SSID:%s password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
}

// 이미지 HTTP 전송
int sendPhoto(const char *url, char *resp_buf, size_t resp_buf_sz)
{
    int rc = ESP_FAIL;
    camera_fb_t *pic = NULL;
    esp_http_client_handle_t client = NULL;

    // 카메라 사진 촬영
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 500));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
    vTaskDelay(pdMS_TO_TICKS(50));

    pic = esp_camera_fb_get();

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));

    if (!pic || !pic->buf || pic->len == 0)
    {
        ESP_LOGE(captureTag, "사진 촬영 실패");
        rc = -1;
        goto WRITE_FAIL;
    }

    ESP_LOGI(captureTag, "사진이 찍혔습니다 ! / 사진 크기 : %zu", pic->len);

    const char *boundary = "----ESP32CamBoundary1234"; // 멀티파트에서 각 파트를 구분하는 구분자 문자열
    // 멀티파트의 앞 부분을 담음 (Boundary의 여러 정보들을 담으며 넉넉하게 확보)
    char head[256];
    // 멀티파트의 각 헤더로 서버가 읽을 필드명과 파일명을 지정함 (헤더와 바디 사이 빈 줄)
    int head_len = snprintf(head, sizeof(head),
                            "--%s\r\n"
                            "Content-Disposition: form-data; name=\"image\"; filename=\"esp32-cam.jpg\"\r\n"
                            "Content-Type: image/jpeg\r\n\r\n",
                            boundary);

    // 멀티파이트를 닫는 부분이라 짧음 (넉넉하게 확보)
    char tail[64];
    int tail_len = snprintf(tail, sizeof(tail), "\r\n--%s--\r\n", boundary);

    if (head_len <= 0 || tail_len <= 0)
    {
        ESP_LOGE(sendPhotoTag, "snprintf 실패");
        rc = -2;
        goto WRITE_FAIL;
    }

    // 콘텐츠의 총 크기 (보내는 크기 + 사진 크기 + 보내기 종료 크기)
    size_t content_len = (size_t)head_len + pic->len + (size_t)tail_len;

    // HTTP 클라이언트 준비
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST, // 전송 방식 (method에 POST)
        .timeout_ms = 10000,        // waiting 시간
        .keep_alive_enable = false, // 메모리 누수 방지용으로 끔
    };

    // HTTP Client 초기화
    client = esp_http_client_init(&cfg);
    if (!client)
    {
        ESP_LOGI(sendPhotoTag, "http client초기화 실패");
        goto WRITE_FAIL;
        return -3;
    }

    // 헤더 설정
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    char ctype[128]; // HTTP 헤더 Content-Type에 넣을 문자열 버퍼이다
    // (결과 문자열을 쓸 목적지 버퍼, 버퍼의 최대 크기, 출력 템플릿)
    snprintf(ctype, sizeof(ctype), "multipart/form-data; boundary=%s", boundary);

    // 서버에게 (바디가 멀티파트임을 알림, 경계값 제공, Content-Type 헤더 값) 를 넘김
    esp_http_client_set_header(client, "Content-Type", ctype);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_http_client_set_header(client, "Connection", "close");

    // 서버와 TCP 연결을 맺고, 요청 라인 헤더 정보들을 보낼 준비를 한다
    esp_err_t err = esp_http_client_open(client, content_len);

    if (err != ESP_OK)
    {
        ESP_LOGI(sendPhotoTag, "open 실패 %s", esp_err_to_name(err));
        goto WRITE_FAIL; // 클라이언트 핸들 정리 (소켓 닫기, 내부 버퍼, 메모리 해제)
        return -4;
    } // 헤더 설정 코드 주석 & 이해 필요한 것 정리

    if (esp_http_client_write(client, head, head_len) != head_len)
    {
        rc = -5;
        goto WRITE_FAIL;
    }
    if (esp_http_client_write(client, (const char *)pic->buf, pic->len) != pic->len)
    {
        rc = -5;
        goto WRITE_FAIL;
    }
    if (esp_http_client_write(client, tail, tail_len) != tail_len)
    {
        rc = -5;
        goto WRITE_FAIL;
    }

    esp_http_client_fetch_headers(client);

    // 응답 바디를 resp_buf로 읽기
    int total = 0;
    while (1)
    {
        int r = esp_http_client_read(client, resp_buf + total, (int)resp_buf_sz - 1 - total);
        if (r <= 0)
            break;
        total += r;
        if ((size_t)total >= resp_buf_sz - 1)
            break;
    }
    resp_buf[total] = '\0';
    // ESP_LOGI(sendPhotoTag, "RAW JSON (%dB): %.*s", total, total, resp_buf);

    parse_json_body(resp_buf, total, client);
    rc = ESP_OK;

WRITE_FAIL: // 모든 설정들을 종료
    if (client)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    if (pic)
    {
        esp_camera_fb_return(pic);
    }
    return rc;
}

void parse_json_body(const char *body, size_t len, esp_http_client_handle_t client)
{
    if (!body || len == 0 || len > MAX_JSON_BODY)
    {
        ESP_LOGE(cJsonParsingTag, "조건이 부합하지 않습니다. (len : %zu)", len);
        return;
    }

    char *buffer = (char *)malloc(len + 1);
    if (!buffer)
    {
        ESP_LOGE(cJsonParsingTag, "동적 메모리 할당 실패");
        return;
    }

    memcpy(buffer, body, len);
    buffer[len] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    if (!root)
    {
        ESP_LOGE(cJsonParsingTag, "노드 얻어오기 실패");
        free(buffer);
        return;
    }

    cJSON *msg = cJSON_GetObjectItemCaseSensitive(root, "test_value");
    if (cJSON_IsNumber(msg))
    {
        // int value = msg->valueint;
        ESP_LOGI(cJsonParsingTag, "-----------------------\n valueInt : %d", msg->valueint);
    }
    else if (cJSON_IsString(msg))
    {
        // int value = atoi(msg->valueint);
        ESP_LOGI(cJsonParsingTag, "-----------------------\n valueString : %s", msg->valuestring);
    }

    cJSON_Delete(root);
    free(buffer);
}

void app_main(void)
{
    uart_driver_delete(UART_NUM_0);

    // 기본 Uart 통신 설정
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, BUF_SIZE, 0, 0, NULL, 0);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    esp_vfs_dev_uart_use_driver(UART_NUM_0);

    uart_flush_input(UART_NUM_0);

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

    // wifi 초기화
    wifi_init();

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
        int len = uart_read_bytes(UART_NUM_0, uart_buff, BUF_SIZE - 1, 100 / portTICK_PERIOD_MS);

        if (len > 0 && len < BUF_SIZE)
        {
            uart_buff[len] = '\0';
            char *str = (char *)uart_buff;
            printf("input : {%s}\n", str);

            while (*str == '\r' || *str == '\n')
                str++;
            if (str[0] == '1')
            {
                xEventGroupWaitBits(s_wifi_evt, WIFI_GOTIP_BIT, false, true, portMAX_DELAY);

                char resp[256];
                esp_err_t err = sendPhoto(url, resp, sizeof(resp));
                if (err != ESP_OK)
                {
                    ESP_LOGE(sendPhotoTag, "사진 전송 실패");
                }
            }
        }
        else
        {
            uart_buff[BUF_SIZE] = '\0';
        }

        // 반응 회복
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
// 보드 고려 조건문
#else
    ESP_LOGE(captureTag, "이 보드는 카메라 지원이 안됩니다 .");
    return;
#endif