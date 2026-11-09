#include "display.h"
#include "pins.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

#define LCD_W     240
#define LCD_H     240
#define BUF_LINES 24

void display_backlight(uint8_t pct)
{
    if (pct > 100) pct = 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1023 * pct / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

lv_display_t *display_init(uint8_t backlight_pct)
{
    // Backlight off until the first frame is ready
    ledc_timer_config_t t = { .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_10_BIT,
                              .timer_num = LEDC_TIMER_0, .freq_hz = 5000, .clk_cfg = LEDC_AUTO_CLK };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    ledc_channel_config_t ch = { .gpio_num = PIN_TFT_BLK, .speed_mode = LEDC_LOW_SPEED_MODE,
                                 .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0, .duty = 0 };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));

    spi_bus_config_t bus = { .sclk_io_num = PIN_TFT_SCK, .mosi_io_num = PIN_TFT_SDA, .miso_io_num = -1,
                             .quadwp_io_num = -1, .quadhd_io_num = -1,
                             .max_transfer_sz = LCD_W * BUF_LINES * 2 + 8 };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = -1, .dc_gpio_num = PIN_TFT_DC,
        .spi_mode = 3,                 // no CS on this module
        .pclk_hz = TFT_SPI_HZ, .trans_queue_depth = 10,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io));

    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t pcfg = { .reset_gpio_num = PIN_TFT_RES,
                                        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
                                        .bits_per_pixel = 16 };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &pcfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));               // RES not wired -> SWRESET
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));  // IPS
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    lvgl_port_cfg_t lcfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lcfg));
    lvgl_port_display_cfg_t d = {
        .io_handle = io, .panel_handle = panel,
        .buffer_size = LCD_W * BUF_LINES, .double_buffer = true,
        .hres = LCD_W, .vres = LCD_H,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = { .buff_dma = 1, .swap_bytes = 1 },
    };
    lv_display_t *disp = lvgl_port_add_disp(&d);
    display_backlight(backlight_pct);
    return disp;
}
