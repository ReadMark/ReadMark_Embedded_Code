#include <stdio.h> //표준 라이브러리
#include <stdbool.h> //
#include <unistd.h> //  POSIX 운영 체제 API에 대한 접근을 제공하는 파일
#include "driver/uart.h" // 통신용 드라이버 인터페이스 정의
#include "esp_log.h" // 로그 메시지 사용
#include "hal/uart_types.h"
#include <stdint.h> // 정확한 크기의 정수형 타입들을 정의
#include "freertos/FreeRTOS.h" //RTOS 시간 적용
#include "esp_err.h"
#include "esp_camera.h"
#include "esp_http_server.h" // http에 의존함

esp_err_t camera_init(void);
esp_err_t camera_capture(void);
void process_image(int width, int height, int format, uint8_t *buf, size_t len);
static esp_err_t post_handler(httpd_req_t *req);


#define BUF_SIZE 1024
static httpd_handle_t server = NULL;


#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    21
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      19
#define CAM_PIN_D2      18
#define CAM_PIN_D1       5
#define CAM_PIN_D0       4
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

static const char *TAG = "camera_app";

void app_main(void)
{
    uart_config_t uart_config = {
		.baud_rate = 115200, // 통신 속도
		.data_bits = UART_DATA_8_BITS, // 데이터 비트
 		.parity = UART_PARITY_DISABLE, // 패리티 없음
		.stop_bits = UART_STOP_BITS_1, // 정지 비트 1개
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // 하드웨어 흐름 제어 비활성
	};

    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);


    uint8_t uart_buff[BUF_SIZE + 1] = { 0 }; // Unsigned char로 선언 8비트 정수

    while (1) 
    {
        int len = uart_read_bytes(UART_NUM_0, uart_buff, BUF_SIZE, 100 / portTICK_PERIOD_MS);

        if (len > 0 && len < BUF_SIZE)
        {   
            uart_buff[len] = '\0'; 
            char *str = (char *)uart_buff; // 형변환이 필수

            printf("input : %s\r\n", str); // %s는 무조건 (char *) 타입만을 받음
        }
    }
}

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,

    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_UXGA,

    .jpeg_quality = 12,
    .fb_count = 1,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

esp_err_t camera_init(void) // 카메라 연결 & 초기화 함수
{
    esp_err_t err = esp_camera_init(&camera_config); //esp_err_t 는 32비트 정수형 자료 초기화 성공시 0, 실패시 음수 or 에러코드
    if (err != ESP_OK) { // 연결 성공시 0 ESP_OK(0)과 다를 경우 실패 문구 출력
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        // ESP_LOGE로 로그를 출력한다, TAG로 오류 출처를 문자열 상수로 나타냄
        return err;
    }
    ESP_LOGI(TAG, "Camera init successful"); // 성공 TAG 출력
    return ESP_OK;
}

esp_err_t camera_capture(void) // CAM 화면 캡쳐
{
    camera_fb_t *fb = esp_camera_fb_get();
    // camera_fb_t 는 카메라 프레임 버퍼 구조체이다, 프레임을 담고 있는 구조체 주소를 반환함
    // 이미지 데이터를 담는 버퍼 포인터, 버퍼의 길이, 가로 해상도, 세로 해상도, 픽셀 포맷, 캡쳐 시간 정보 등을 포함한다
    if (!fb) { // fb가 NULL 이라면 그 반대인 true가 되어 다음 코드가 실행됨
        ESP_LOGE(TAG, "Camera capture failed");
        return ESP_FAIL;
    }
    
    // 캡쳐가 성공되었다면?
    process_image(fb->width, fb->height, fb->format, fb->buf, fb->len);

    esp_camera_fb_return(fb);
    // 카메라 프레임 버퍼를 반환함
    // 반환하지 않는다면 다음에 메모리가 부족할 수 있음

    return ESP_OK;
}

// size_t 메모리나 배열을 관리하는 부호 없는 정수형 타입
// 임베디드에서의 format은 데이터가 저장되거나 표현되는 방식
void process_image(int width, int height, int format, uint8_t *buf, size_t len) {
    printf("Image captured: %dx%d, len: %d, format: %d\n", width, height, (int)len, format);

    uart_read_bytes(UART_NUM_0, (const char*)buf, len, 100 / portTICK_PERIOD_MS);
    
}

// HTTP POST요청에 따라 사진을 찍고 JPEG이미지로 응답을 보내는 함수
static esp_err_t post_handler(httpd_req_t *req) { // 구조체 정보를 받아옴 (ex, 사용자 요청, 세션)
    camera_fb_t *fb = esp_camera_fb_get(); // 카메라 정보 받아옴 성공시 주소 반환 

    if (!fb) { // 실패시 NULL, !(NULL)이므로 연결 실패시 오류 문구
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera capture failed...");
        // HTTP 오류 응답을 보내기 위함, 서버 내부 문제 로그와 함께 멘트 출력   
        // req로 구조체 정보를 넘김 (ex, 요청자, 세션 정보)
        return ESP_FAIL; // 오류 로그 출력
    }

    httpd_resp_set_type(req, "image/jpeg"); // 이미지의 콘텐츠 유형을 설정함 (응답되는 요청, 응답의 콘텐츠 유행);
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg"); // 다운로드 제어, 보안 헤더 추가, 브라우저 내에서 직접 보여주며 파일 이름을 지정
    httpd_resp_send(req, (const char *)fb->buf, fb->len); // 성공한 HTTP에 대한 응답을 보냄

    esp_camera_fb_return(fb); // 카메라 프레임 반환
    return ESP_OK; // 성공값 리턴
 }

static const httpd_uri_t post_uri = { // 웹 서버에 등록되는 설정 값
    .uri = "/post", // 클라이언트가 요청하는 경로
    .method = HTTP_POST, // POST방식 요청만 처리함
    // POST방식은 HTTP의 요청 방식중 하나로 데이터를 보낼 때 사용
    .handler = post_handler, // 실제 실행할 함수
    .user_ctx = NULL // 사용자 정의 데이터를 넘길 수 있음
};

void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); // HTTP의 서버의 기본 설정을 채워주는 구조체
    httpd_start(&server, &config); // HTTP 서버 핸들, 설정한 구조체 정보
    httpd_register_uri_handler(server, &post_uri); // URI경로에 어떤 함수가 요청을 처리할지 등록
    // post_uri에 정보들을 넣으며 설정 (ex: 붕어빵 틀에 크림을 넣을지, 팥을 넣을지)
}

// void app_main(void) {
//     // wifi초기화 추가
//     camera_init();
//     start_webserver();
//     ESP_LOGI(TAG, "HTTP server started. POST to /post to capture");
// }