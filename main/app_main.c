/* ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "string.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "onenet.h"
#include "mqttkit.h"
#include "app_main.h"
#include "app_sntp.h"
#include "app_camera.h"
#include "captive_portal.h"
#include "screen_driver.h"
#include "file_manage.h"
#include "file_server.h"
#include "vidoplayer.h"
#include "rgb.h"

static const char *TAG = "app_main";

#define CAM_CHECK(a, str, ret)  if(!(a)) {                                             \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);      \
        return (ret);                                                                   \
        }

static TaskHandle_t task_handle_misc;
static TaskHandle_t task_handle_camera;

SysStatus_t SystemStatus = 0;


scr_driver_t g_lcd;
static scr_info_t lcd_info;

static esp_err_t image_save(uint8_t *pdata, uint32_t length, const char *path)
{

    uint32_t written, len = length;
    int64_t fr_start = esp_timer_get_time();
    char name[64];
    sprintf(name, "/sdcard/picture/%s", path);
    FILE *f = fopen(name, "wb");
    int64_t fr_end = esp_timer_get_time();
    ESP_LOGI(TAG, "open [%s] time:%ums", name, (uint32_t)((fr_end - fr_start) / 1000));
    CAM_CHECK(NULL != f, "Failed to open file for writing", ESP_FAIL);
    
    do
    {
        written = fwrite(pdata, 1, len, f);printf("len=%d, written=%d\n", len, written);
        len -= written;
        pdata += written;

    }
    while ( written && len );
    fclose(f);
    fr_end = esp_timer_get_time();
    ESP_LOGI(TAG, "File written: %uB %ums", length, (uint32_t)((fr_end - fr_start) / 1000));
    
    return ESP_OK;
}

static void screen_clear(int color)
{
    g_lcd.get_info(&lcd_info);
    ESP_LOGI(TAG, "lcd clean to %x", color);
    uint16_t *buffer = malloc(lcd_info.width * sizeof(uint16_t));
    if (NULL == buffer) {
        for (size_t y = 0; y < lcd_info.height; y++) {
            for (size_t x = 0; x < lcd_info.width; x++) {
                g_lcd.draw_pixel(x, y, color);
            }
        }
    } else {
        for (size_t i = 0; i < lcd_info.width; i++) {
            buffer[i] = color;
        }

        for (int y = 0; y < lcd_info.height; y++) {
            g_lcd.draw_bitmap(0, y, lcd_info.width, 1, buffer);
        }

        free(buffer);
    }
}

static void lcd_init(void)
{
    void h4001_init();
    h4001_init();
    // spi_config_t spi_cfg = {
    //     .miso_io_num = -1,
    //     .mosi_io_num = 21, //txd
    //     .sclk_io_num = 22, //rxd
    //     .max_transfer_sz = 240 * 320*2+10,
    // };
    // spi_bus_handle_t spi_bus = spi_bus_create(VSPI_HOST, &spi_cfg);
    // ESP_LOGI(TAG, "lcd_init");

    // scr_interface_spi_config_t spi_lcd_cfg = {
    //     .spi_bus = spi_bus,
    //     .pin_num_cs = 5,
    //     .pin_num_dc = 19,
    //     .clk_freq = 80000000,
    //     .swap_data = 0,
    // };

    // scr_interface_driver_t *iface_drv;
    // scr_interface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &iface_drv);

    init_rgb_screen(&g_lcd);

    scr_controller_config_t lcd_cfg = {
        .interface_drv = 0,
        .pin_num_rst = 0,
        .pin_num_bckl = 0,
        .rst_active_level = 0,
        .bckl_active_level = 1,
        .offset_hor = 0,
        .offset_ver = 0,
        .width = 480,
        .height = 854,
        .rotate = SCR_SWAP_XY | SCR_MIRROR_Y, /** equal to SCR_DIR_BTLR */
    };
    esp_err_t ret = g_lcd.init(&lcd_cfg);

    screen_clear(COLOR_ESP_BKGD);vTaskDelay(500 / portTICK_PERIOD_MS);
    screen_clear(COLOR_BLUE);vTaskDelay(500 / portTICK_PERIOD_MS);
    screen_clear(COLOR_RED);vTaskDelay(500 / portTICK_PERIOD_MS);
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // app_camera_init();
    ESP_ERROR_CHECK(fm_init()); /* Initialize file storage */
    fm_mkdir("/sdcard/picture");
    lcd_init();
    
    // bool is_configured;
    // captive_portal_start("ESP_WEB_CONFIG", NULL, &is_configured);

    // if (is_configured) {
    //     wifi_config_t wifi_config;
    //     esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);
    //     ESP_LOGI(TAG, "SSID:%s, PASSWORD:%s", wifi_config.sta.ssid, wifi_config.sta.password);
    // }
    // captive_portal_wait(portMAX_DELAY);
    // SYS_STATUS_SET(SYS_STATUS_CONNECTED);
    // app_sntp_start();
    // esp_wifi_set_ps(WIFI_PS_NONE);

    // start_file_server();

    // vTaskDelay(pdMS_TO_TICKS(1000));
    avi_play("/sdcard/tom-480x480.avi");
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // avi_play("/sdcard/taylor.avi");
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // avi_play("/sdcard/Marshmello.avi");
 
}



