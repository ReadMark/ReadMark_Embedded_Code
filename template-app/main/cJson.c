#include <header.h>

static const char *cJsonParsingTag = "cJsonParsing";
#define MAX_JSON_BODY 1024

/*
함수는 각각
1. http 함수에서 json이 담긴 body를 받는다
2. content_len을 받는다
3. client 핸들을 받는다
*/

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