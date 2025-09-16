#include "oled_display.h"
#include "font5x7.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *OledTag = "OLED";

#define oled_cs 5
#define oled_dc 16
#define oled_rst 17
#define oled_mosi 23
#define oled_sclk 18

#define width   128
#define height  128

static spi_device_handle_t oled_spi;

static void oled_send_cmd(uint8_t cmd)
{
    // 1bate씩 cmd에 보내기
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };

    // 0이면 명령 1이면 데이터
    gpio_set_level(oled_dc, 0);
    // 구조체 t에 의거하여 데이터 전송
    spi_device_polling_transmit(oled_spi, &t);
}

// 여긴 데이터 보내기
static void oled_send_data(const uint8_t *data, int len)
{
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    gpio_set_level(oled_dc, 1);
    spi_device_polling_transmit(oled_spi, &t);
}

static void oled_set_window(int x0, int y0, int x1, int y1)
{
    oled_send_cmd(0x15); // Column
    oled_send_cmd(x0);
    oled_send_cmd(x1);

    oled_send_cmd(0x75); // Row
    oled_send_cmd(y0);
    oled_send_cmd(y1);

    oled_send_cmd(0x5C); // Write RAM
}

static void oled_draw_pixel(int x, int y, uint16_t color)
{
    uint8_t data[2] = { color >> 8, color & 0xFF };
    oled_set_window(x, y, x, y);
    oled_send_data(data, 2);
}

static void oled_draw_char(int x, int y, char c, uint16_t color)
{
    if (c < 32 || c > 126) return;
    const uint8_t *bitmap = font5x7[c - 32];

    for (int col = 0; col < 5; col++) {
        uint8_t line = bitmap[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                oled_draw_pixel(x + col, y + row, color);
            }
        }
    }
}

void oled_draw_string(int x, int y, const char *str, uint16_t color)
{
    int orig_x = x;

    while (*str) {
        if (*str == '\n') {
            y += 8;    // 줄바꿈, 글자 높이(7) + 1
            x = orig_x; // x 위치 초기화
            str++;
            continue;
        }
        oled_draw_char(x, y, *str, color);
        x += 6; // 글자 폭(5) + 간격(1)
        str++;
    }
}

void oled_clear(uint16_t color) {
    oled_set_window(0, 0, width-1, height-1);

    size_t size = width * height * 2;
    uint8_t *buf = heap_caps_malloc(size, MALLOC_CAP_DMA);

    for (int i = 0; i < width * height; i++) {
        buf[2*i] = color >> 8;
        buf[2*i+1] = color & 0xFF;
    }
    
    oled_send_data(buf, size);
    free(buf);
}

static void init_sequence(void)
{
    oled_send_cmd(0xFD); // Command Lock
    uint8_t data1 = 0x12;
    oled_send_data(&data1, 1);

    oled_send_cmd(0xFD);
    data1 = 0xB1;
    oled_send_data(&data1, 1);

    oled_send_cmd(0xAE); // Display Off

    oled_send_cmd(0xB3); // Clock Div
    data1 = 0xF1;
    oled_send_data(&data1, 1);

    oled_send_cmd(0xCA); // MUX Ratio
    data1 = 0x7F;
    oled_send_data(&data1, 1);

    oled_send_cmd(0xA0); // Set Remap
    data1 = 0x74; // RGB, 65k color
    oled_send_data(&data1, 1);

    oled_send_cmd(0xA1); // Display Start Line
    data1 = 0x00;
    oled_send_data(&data1, 1);

    oled_send_cmd(0xA2); // Display Offset
    data1 = 0x00;
    oled_send_data(&data1, 1);

    oled_send_cmd(0xAB); // VDD Internal
    data1 = 0x01;
    oled_send_data(&data1, 1);

    oled_send_cmd(0xB1); // Precharge
    data1 = 0x32;
    oled_send_data(&data1, 1);

    oled_send_cmd(0xBE); // VCOMH
    data1 = 0x05;
    oled_send_data(&data1, 1);

    oled_send_cmd(0xC1); // Contrast
    uint8_t contrast[] = {0xC8, 0x80, 0xC8};
    oled_send_data(contrast, 3);

    oled_send_cmd(0xC7); // Master Contrast
    data1 = 0x0F;
    oled_send_data(&data1, 1);

    oled_send_cmd(0xB4); // Segment Low Voltage
    uint8_t segvol[] = {0xA0, 0xB5, 0x55};
    oled_send_data(segvol, 3);

    oled_send_cmd(0xB6); // Second Precharge
    data1 = 0x01;
    oled_send_data(&data1, 1);

    oled_send_cmd(0xAF); // Display ON
}

esp_err_t oled_init(void)
{
    ESP_LOGI(OledTag, "OLED init start");

    spi_bus_config_t buscfg = {
        .mosi_io_num = oled_mosi,
        .miso_io_num = -1,
        .sclk_io_num = oled_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    esp_err_t ret = spi_bus_initialize(VSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) return ret;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = oled_cs,
        .queue_size = 1,
    };
    ret = spi_bus_add_device(VSPI_HOST, &devcfg, &oled_spi);
    if (ret != ESP_OK) return ret;

    // Reset 핀
    gpio_reset_pin(oled_rst);
    gpio_set_direction(oled_rst, GPIO_MODE_OUTPUT);
    gpio_set_level(oled_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(oled_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // DC 핀
    gpio_reset_pin(oled_dc);
    gpio_set_direction(oled_dc, GPIO_MODE_OUTPUT);

    // SSD1351 초기화 시퀀스
    init_sequence();

    ESP_LOGI(OledTag, "OLED initialized");
    return ESP_OK;
}

void oled_display_text(const char *str, uint16_t color) {
    oled_clear(0x0000); // 검정색으로 화면 초기화
    oled_draw_string(0, 0, str, color);
}