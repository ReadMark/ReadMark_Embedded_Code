#include "wifi_client.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"

static const char *TAG = "WIFI";

#define WIFI_SSID "KT_GiGA_5G_6F98"
#define WIFI_PASS "4dc00gk820"
#define SERVER_URL "??"

void wifi_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

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
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void fetch_server_data(int *battery, int *last_page, int *pages_today)
{
    esp_http_client_config_t config = {
        .url = SERVER_URL,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int content_length = esp_http_client_get_content_length(client);
        char *buffer = malloc(content_length + 1);
        esp_http_client_read(client, buffer, content_length);
        buffer[content_length] = '\0';

        cJSON *root = cJSON_Parse(buffer);
        if (root) {
            *battery = cJSON_GetObjectItem(root, "battery")->valueint;
            *last_page = cJSON_GetObjectItem(root, "last_page")->valueint;
            *pages_today = cJSON_GetObjectItem(root, "pages_today")->valueint;
            cJSON_Delete(root);
        }
        free(buffer);
    }
    esp_http_client_cleanup(client);
}
