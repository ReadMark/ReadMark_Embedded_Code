idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES oled_display wifi_client sensors
)