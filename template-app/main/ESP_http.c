#include "header.h"

#define BUF_SIZE 1024             // 입력 버퍼 사이즈
#define MAX_JSON_BODY (16 * 2024) // 서버 응답 메시지 제한

static const char *sendPhotoTag = "Send_Photo";
static const char *captureTag = "Capture";
char *cJsonBuffer; // cJson 파싱 값 저장 버퍼

char url[128] = "http://192.168.1.51:5000/upload/"; // 서버 접속 URL

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
    ESP_LOGI(sendPhotoTag, "RAW JSON (%dB): %.*s", total, total, resp_buf);

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

void app_main(void)
{

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