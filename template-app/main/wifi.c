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

#define WIFI_CONNECTED_BIT BIT0 // AP에 연결됨
#define WIFI_GOTIP_BIT BIT1     // DHCP로 IP 획득

static const char *WifiConfigTag = "Wifi_config";

// 네트워크 ID, Password
const char *ssid = "ORBI96";
const char *password = "moderncurtain551";

// 서버 통신 설정
static EventGroupHandle_t s_wifi_evt; // 핸들의 이벤트를 담는 변수

// 네트워크 핸들러
static void wifi_event_handler(void *handler_arg, esp_event_base_t base, int32_t event_id, void *event_data)
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
static void wifi_init(void)
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

void app_main(void)
{
    // wifi 초기화
    wifi_init();
}
