#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "soc/soc_caps.h"
#include "screen_driver.h"

// #define TEST_LCD_H_RES         (800)
// #define TEST_LCD_V_RES         (480)
// #define TEST_LCD_VSYNC_GPIO    (1)
// #define TEST_LCD_HSYNC_GPIO    (2)
// #define TEST_LCD_DE_GPIO       (40)
// #define TEST_LCD_PCLK_GPIO     (3)
// #define TEST_LCD_DATA0_GPIO    (4)  // B0
// #define TEST_LCD_DATA1_GPIO    (5)  // B1
// #define TEST_LCD_DATA2_GPIO    (6)  // B2
// #define TEST_LCD_DATA3_GPIO    (7)  // B3
// #define TEST_LCD_DATA4_GPIO    (8)  // B4
// #define TEST_LCD_DATA5_GPIO    (9)  // G0
// #define TEST_LCD_DATA6_GPIO    (10) // G1
// #define TEST_LCD_DATA7_GPIO    (11) // G2
// #define TEST_LCD_DATA8_GPIO    (12) // G3
// #define TEST_LCD_DATA9_GPIO    (13) // G4
// #define TEST_LCD_DATA10_GPIO   (14) // G5
// #define TEST_LCD_DATA11_GPIO   (15) // R0
// #define TEST_LCD_DATA12_GPIO   (16) // R1
// #define TEST_LCD_DATA13_GPIO   (17) // R2
// #define TEST_LCD_DATA14_GPIO   (18) // R3
// #define TEST_LCD_DATA15_GPIO   (19) // R4
// #define TEST_LCD_DISP_EN_GPIO  (-1)

// #define TEST_LCD_H_RES         (800)
// #define TEST_LCD_V_RES         (480)
// #define TEST_LCD_VSYNC_GPIO    (48)
// #define TEST_LCD_HSYNC_GPIO    (47)
// #define TEST_LCD_DE_GPIO       (45)
// #define TEST_LCD_PCLK_GPIO     (21)
// #define TEST_LCD_DATA0_GPIO    (3)  // B0
// #define TEST_LCD_DATA1_GPIO    (4)  // B1
// #define TEST_LCD_DATA2_GPIO    (5)  // B2
// #define TEST_LCD_DATA3_GPIO    (6)  // B3
// #define TEST_LCD_DATA4_GPIO    (7)  // B4
// #define TEST_LCD_DATA5_GPIO    (8)  // G0
// #define TEST_LCD_DATA6_GPIO    (9) // G1
// #define TEST_LCD_DATA7_GPIO    (10) // G2
// #define TEST_LCD_DATA8_GPIO    (11) // G3
// #define TEST_LCD_DATA9_GPIO    (12) // G4
// #define TEST_LCD_DATA10_GPIO   (13) // G5
// #define TEST_LCD_DATA11_GPIO   (14) // R0
// #define TEST_LCD_DATA12_GPIO   (15) // R1
// #define TEST_LCD_DATA13_GPIO   (16) // R2
// #define TEST_LCD_DATA14_GPIO   (17) // R3
// #define TEST_LCD_DATA15_GPIO   (18) // R4
// #define TEST_LCD_DISP_EN_GPIO  (-1)

#define TEST_LCD_H_RES         (480)
#define TEST_LCD_V_RES         (480)
#define TEST_LCD_VSYNC_GPIO    (3)
#define TEST_LCD_HSYNC_GPIO    (46)
#define TEST_LCD_DE_GPIO       (0)
#define TEST_LCD_PCLK_GPIO     (9)

#define TEST_LCD_DATA0_GPIO    (14)  // B0
#define TEST_LCD_DATA1_GPIO    (13)  // B1
#define TEST_LCD_DATA2_GPIO    (12)  // B2
#define TEST_LCD_DATA3_GPIO    (11)  // B3
#define TEST_LCD_DATA4_GPIO    (10)  // B4

#define TEST_LCD_DATA5_GPIO    (39)  // G0
#define TEST_LCD_DATA6_GPIO    (38) // G1
#define TEST_LCD_DATA7_GPIO    (45) // G2
#define TEST_LCD_DATA8_GPIO    (48) // G3
#define TEST_LCD_DATA9_GPIO    (47) // G4
#define TEST_LCD_DATA10_GPIO   (21) // G5

#define TEST_LCD_DATA11_GPIO   (1) // R0
#define TEST_LCD_DATA12_GPIO   (2) // R1
#define TEST_LCD_DATA13_GPIO   (42) // R2
#define TEST_LCD_DATA14_GPIO   (41) // R3
#define TEST_LCD_DATA15_GPIO   (40) // R4
#define TEST_LCD_DISP_EN_GPIO  (-1)

#if SOC_LCD_RGB_SUPPORTED
// RGB driver consumes a huge memory to save frame buffer, only test it with PSRAM enabled

static esp_lcd_panel_handle_t panel_handle = NULL;

static esp_err_t _init(const scr_controller_config_t *lcd_conf)
{
    esp_lcd_rgb_panel_config_t panel_config = {
        .data_width = 16,
        .disp_gpio_num = TEST_LCD_DISP_EN_GPIO,
        .pclk_gpio_num = TEST_LCD_PCLK_GPIO,
        .vsync_gpio_num = TEST_LCD_VSYNC_GPIO,
        .hsync_gpio_num = TEST_LCD_HSYNC_GPIO,
        .de_gpio_num = TEST_LCD_DE_GPIO,
        .data_gpio_nums = {
            TEST_LCD_DATA0_GPIO,
            TEST_LCD_DATA1_GPIO,
            TEST_LCD_DATA2_GPIO,
            TEST_LCD_DATA3_GPIO,
            TEST_LCD_DATA4_GPIO,
            TEST_LCD_DATA5_GPIO,
            TEST_LCD_DATA6_GPIO,
            TEST_LCD_DATA7_GPIO,
            TEST_LCD_DATA8_GPIO,
            TEST_LCD_DATA9_GPIO,
            TEST_LCD_DATA10_GPIO,
            TEST_LCD_DATA11_GPIO,
            TEST_LCD_DATA12_GPIO,
            TEST_LCD_DATA13_GPIO,
            TEST_LCD_DATA14_GPIO,
            TEST_LCD_DATA15_GPIO,
        },
        .timings = {
            .pclk_hz = 13000000,
            .h_res = TEST_LCD_H_RES,
            .v_res = TEST_LCD_V_RES,
            .hsync_back_porch = 88,
            .hsync_front_porch = 40,
            .hsync_pulse_width = 48,
            .vsync_back_porch = 32,
            .vsync_front_porch = 13,
            .vsync_pulse_width = 3,
        },
        .flags.fb_in_psram = 1,
    };
    TEST_ESP_OK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));
        
    return ESP_OK;
}

static esp_err_t _deinit(void)
{
    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    return ESP_OK;
}

static esp_err_t _set_direction(scr_dir_t dir)
{
    // esp_lcd_panel_mirror();
    // esp_lcd_panel_swap_xy();
    return ESP_OK;
}

static esp_err_t _set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    return ESP_OK;
}

static esp_err_t _write_ram_data(uint16_t color)
{
    return ESP_OK;
}

static esp_err_t _draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    return ESP_OK;
}

static esp_err_t _draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    // rgb_panel_wait_transaction_done(panel_handle, portMAX_DELAY);
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + w-1, y + h-1, bitmap);
    return ESP_OK;
}

static esp_err_t _get_info(scr_info_t *info)
{
    info->bpp = 16;
    info->color_type = SCR_COLOR_TYPE_RGB565;
    info->height = TEST_LCD_V_RES;
    info->width = TEST_LCD_H_RES;
    info->name = "RGB Panel";
    return ESP_OK;
}


void init_rgb_screen(scr_driver_t *lcd)
{
    lcd->deinit = _deinit;
    lcd->init = _init;
    lcd->get_info = _get_info;
    lcd->draw_bitmap = _draw_bitmap;
    lcd->set_direction = _set_direction;
    lcd->draw_pixel = _draw_pixel;
    lcd->set_window = _set_window;
    lcd->write_ram_data  =_write_ram_data;
    
}

#endif // SOC_LCD_RGB_SUPPORTED
