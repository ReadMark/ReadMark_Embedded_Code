idf_component_register(
    SRCS "wifi_client.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event nvs_flash esp_http_client esp_websocket_client json
)   