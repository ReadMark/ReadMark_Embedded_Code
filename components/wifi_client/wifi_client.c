#include "wifi_client.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_websocket_client.h"
#include "cJSON.h"

void websocket_app_start(void);

bool websocket_start = false;

static const char *TAG = "WIFI";

#define WIFI_SSID "KT_GiGA_5G_6F98"
#define WIFI_PASS "4dc00gk820"
#define SERVER_URL "??"

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "wifi re connecting..");
        esp_wifi_connect();
    }
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP : " IPSTR, IP2STR(&event->ip_info.ip));

        if(!websocket_start)
        {
            websocket_app_start();
            websocket_start = true;
        }
    }
}

void wifi_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
}

static void websocket_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t*)event_data;

    switch(event_id)
    {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "websocket connecting success");
            break;
        
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "websocket disconnect");
            break;

        case WEBSOCKET_EVENT_DATA:
        {
            ESP_LOGI(TAG, "message reception [%.*s]", data->data_len, (char*)data->data_ptr);

            char *msg = strndup((const char*)data->data_ptr, data->data_len);
            if (msg == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for message");
                break;
            }

            cJSON *root = cJSON_Parse(msg);
            if (root == NULL) {
                ESP_LOGE(TAG, "Invalid JSON: %s", msg);
                free(msg);
                break;
            }

            // 파싱 할꺼 여따 넣기

            cJSON_Delete(root);
            free(msg);
            break;
        }
    }
}

void websocket_app_start(void)
{
    esp_websocket_client_config_t websocket_cfg = {
        .uri = SERVER_URL,
        .disable_auto_reconnect = false,
        .cert_pem = NULL,
        .use_global_ca_store = false,
        .transport = WEBSOCKET_TRANSPORT_OVER_TCP,
    };

    esp_websocket_client_handle_t client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(client, ESP_EVENT_ANY_ID, websocket_event_handler, (void*)client);
    esp_websocket_client_start(client);
}