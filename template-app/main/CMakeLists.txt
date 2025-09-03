idf_component_register(
    SRCS "main.c" "ESP_wifi.c" "ESP_http.c" "ESP_cJson.c" "ESP_camera.c" 
    INCLUDE_DIRS "."
    REQUIRES esp_wifi wpa_supplicant nvs_flash esp_event esp_netif esp_http_client json
)