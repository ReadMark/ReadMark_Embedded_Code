// header.h
#ifndef HEADER_H
#define HEADER_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <cJSON.h>
#include <sys/param.h>
#include <nvs_flash.h>
#include <unistd.h>

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

// 함수 원형 선언
void wifi_init(void);
int sendPhoto(const char *url, char *resp_buf, size_t resp_buf_sz);
void cJsonParsing(esp_http_client_handle_t client);
esp_err_t init_camera(void);
void tune_sensor_for_quality(void);

#endif