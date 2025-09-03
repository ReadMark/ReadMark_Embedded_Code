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

// 서버 통신 설정
static EventGroupHandle_t s_wifi_evt; // 핸들의 이벤트를 담는 변수

#define WIFI_GOTIP_BIT BIT1 // DHCP로 IP 획득

// LOG용 TAG 모음
static const char *sendPhotoTag = "Send_Photo";
static const char *captureTag = "Captuer";

char url[128] = "http://192.168.1.51:5000/upload/"; // 서버 접속 URL

// 함수의 원형
int sendPhoto(const char *url, char *resp_buf, size_t resp_buf_sz);

// 통신 설정
const uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
};

// 이미지 HTTP 전송
int sendPhoto(const char *url, char *resp_buf, size_t resp_buf_sz)
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
        esp_camera_fb_return(pic);
        return -2;
    }

    // 콘텐츠의 총 크기 (보내는 크기 + 사진 크기 + 보내기 종료 크기)
    size_t content_len = (size_t)head_len + pic->len + (size_t)tail_len;

    // HTTP 클라이언트 준비
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST, // 전송 방식 (method에 POST)
        .timeout_ms = 10000,        // waiting 시간
    };

    // HTTP Client 초기화
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client)
    {
        ESP_LOGI(sendPhotoTag, "http client초기화 실패");
        esp_camera_fb_return(pic);
        return -3;
    }

    // 헤더 설정
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    char ctype[128]; // HTTP 헤더 Content-Type에 넣을 문자열 버퍼이다
    // (결과 문자열을 쓸 목적지 버퍼, 버퍼의 최대 크기, 출력 템플릿)
    snprintf(ctype, sizeof(ctype), "multipart/form-data; boundary=%s", boundary);

    // 서버에게 (바디가 멀티파트임을 알림, 경계값 제공, Content-Type 헤더 값) 를 넘김
    esp_http_client_set_header(client, "Content-Type", ctype);
    esp_http_client_set_header(client, "Content-Length", NULL);

    // 서버와 TCP 연결을 맺고, 요청 라인 헤더 정보들을 보낼 준비를 한다
    esp_err_t err = esp_http_client_open(client, content_len);

    if (err != ESP_OK)
    {
        ESP_LOGI(sendPhotoTag, "open 실패 %s", esp_err_to_name(err));
        esp_http_client_cleanup(client); // 클라이언트 핸들 정리 (소켓 닫기, 내부 버퍼, 메모리 해제)
        esp_camera_fb_return(pic);
        return -4;
    } // 헤더 설정 코드 주석 & 이해 필요한 것 정리

    // 본문 스트리밍
    int w = 0;
    // 지정한 버퍼의 데이터를 len 바이트만큼 HTTP요청 body로 전송한다
    w = esp_http_client_write(client, head, head_len);
    if (w != head_len)
    {
        ESP_LOGE(sendPhotoTag, "헤드 write 실패");
        goto WRITE_FAIL;
    }

    // JPEG 본문을 1024바이트씩 전송
    const uint8_t *p = pic->buf;
    size_t remain = pic->len;

    while (remain > 0)
    {
        size_t chunk = (remain > 1024) ? 1024 : remain;
        w = esp_http_client_write(client, (const char *)p, chunk);

        if (w != (int)chunk)
        {
            ESP_LOGE(sendPhotoTag, "write jpeg 실패");
            goto WRITE_FAIL;
        }
        p += chunk;      // 사진의 새로운 부분부터 보내야 하므로 이미 보낸 부분(chunk)를 제외
        remain -= chunk; // 사진을 다(0) 보낼 때 까지 반복, 사진의 1024바이트씩 보냄
    }

    w = esp_http_client_write(client, tail, tail_len);
    if (w != tail_len)
    {
        ESP_LOGE(sendPhotoTag, "write tail 실패");
        goto WRITE_FAIL;
    }

    // 응답 상태 / 헤더 / 바디 읽기
    int status = esp_http_client_fetch_headers(client);        // 헤더만 읽음, 에러 / 헤더 길이를 반환함
    int http_status = esp_http_client_get_status_code(client); // HTTP 상태 코드 가져오기 (ex : 404, 200)

    if (resp_buf && resp_buf_sz > 0) // 응답을 저장할 버퍼가 있고, 크기도 0보다 큰 경우
    {
        int total = 0;
        while (1)
        {
            // 서버 응답을 읽는 함수로 resp_buf + total부터 내용을 채움, - 읽은 범위 - NULL문자
            int r = esp_http_client_read(client, resp_buf + total, (int)resp_buf_sz - 1 - total);

            // 더 이상 읽을 데이터가 없을 때
            if (r <= 0)
            {
                break;
            }
            total += r;                           // r을 더해서 이미 읽은 부분은 제외
            if ((size_t)total >= resp_buf_sz - 1) // 다 읽었으면 종료
                break;
        }
        cJsonParsing(client);
        resp_buf[total] = '\0';
        ESP_LOGI(sendPhotoTag, "응답(%d) : %s", http_status, resp_buf);
    }
    else
    {
        ESP_LOGI(sendPhotoTag, "HTTP 상태 코드(%d)", status);
    }

    // 모든 설정 종료 후 HTTP 상태 코드 반환
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    esp_camera_fb_return(pic);
    return http_status;

WRITE_FAIL: // 모든 설정들을 종료
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    esp_camera_fb_return(pic);
    return -5;
}

void app_main(void)
{
    // 기본 Uart 통신 설정
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    while (1)
    {
        xEventGroupWaitBits(s_wifi_evt, WIFI_GOTIP_BIT, false, true, portMAX_DELAY);

        char resp[256];
        int code = sendPhoto(url, resp, sizeof(resp));

        if (code >= 200 && code < 300)
        {
            ESP_LOGI(sendPhotoTag, "업로드 성공 %d", code);
        }
        else
        {
            ESP_LOGI(sendPhotoTag, "업로드 실패 %d", code);
        }
    }
}