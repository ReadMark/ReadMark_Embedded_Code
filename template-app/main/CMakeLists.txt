idf_component_register(
    SRCS "cJson.c" "main.c" "wifi.c" "camera.c" "http.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi wpa_supplicant nvs_flash esp_event esp_netif esp_http_client json
)
