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
#include "app_httpd.h"
#include "esp_camera.h"
#include "app_led.h"
#include "app_sdcard.h"
#include "screen_driver.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

static const char *TAG = "app_main";

#define CAM_CHECK(a, str, ret)  if(!(a)) {                                             \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);      \
        return (ret);                                                                   \
        }

static TaskHandle_t task_handle_onenet;
static TaskHandle_t task_handle_misc;
static TaskHandle_t task_handle_camera;

SysStatus_t SystemStatus = 0;


static scr_driver_fun_t lcd;
static scr_info_t lcd_info;


static int do_mkdir(const char *path, mode_t mode)
{
  struct stat st;
  int status = 0;

  if (stat(path, &st) != 0) {
    /* Directory does not exist. EEXIST for race condition */
    ESP_LOGI(TAG, "Create dir [%s]", path);
    if (mkdir(path, mode) != 0 && errno != EEXIST) {
      status = -1;
      ESP_LOGE(TAG, "Create dir [%s] failed", path);
    }
  } else if (!S_ISDIR(st.st_mode)) {
    errno = ENOTDIR;
    status = -1;
    ESP_LOGE(TAG, "Exist [%s] but not dir", path);
  }

  return status;
}

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
    lcd.get_info(&lcd_info);
    ESP_LOGI(TAG, "lcd clean to %x", color);
    uint16_t *buffer = malloc(lcd_info.width * sizeof(uint16_t));
    if (NULL == buffer) {
        for (size_t y = 0; y < lcd_info.height; y++) {
            for (size_t x = 0; x < lcd_info.width; x++) {
                lcd.draw_pixel(x, y, color);
            }
        }
    } else {
        for (size_t i = 0; i < lcd_info.width; i++) {
            buffer[i] = color;
        }

        for (int y = 0; y < lcd_info.height; y++) {
            lcd.draw_bitmap(0, y, lcd_info.width, 1, buffer);
        }

        free(buffer);
    }
}

static void lcd_init(void)
{
    // spi_config_t spi_cfg = {
    //     .miso_io_num = 2,
    //     .mosi_io_num = 15,
    //     .sclk_io_num = 14,
    //     .max_transfer_sz = 320 * 480,
    // };
    // spi_bus_handle_t spi_bus = spi_bus_create(HSPI_HOST, &spi_cfg);
    ESP_LOGI(TAG, "lcd_init");

    iface_spi_config_t spi_lcd_cfg = {
        .spi_bus = NULL,
        .pin_num_cs = 12,
        .pin_num_dc = 4,
        .clk_freq = 40000000,
        .swap_data = 0,
    };

    scr_iface_driver_fun_t *iface_drv;
    scr_iface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &iface_drv);

    scr_controller_config_t lcd_cfg = {0};
    lcd_cfg.iface_drv = iface_drv,
    lcd_cfg.pin_num_rst = -1,
    lcd_cfg.pin_num_bckl = -1,
    lcd_cfg.rst_active_level = 0,
    lcd_cfg.bckl_active_level = 1,
    lcd_cfg.width = 240;
    lcd_cfg.height = 240;
    lcd_cfg.rotate = SCR_DIR_LRTB;
    scr_init(SCREEN_CONTROLLER_ST7789, &lcd_cfg, &lcd);

    screen_clear(COLOR_ESP_BKGD);vTaskDelay(500 / portTICK_PERIOD_MS);
}


static void camera_task(void *arg)
{
    uint8_t image_cnt = 0;
    camera_fb_t *image_fb = NULL;

    app_camera_init();
    int res = 0;
    sensor_t *s = esp_camera_sensor_get();
    res = s->set_framesize(s, FRAMESIZE_HQVGA);
    // res |= s->set_vflip(s, true);
    // res |= s->set_hmirror(s, true);
    if (res)
    {
        ESP_LOGE(TAG, "Camera set_framesize failed");
    }

    uint16_t img_width = resolution[FRAMESIZE_HQVGA].width;
    uint16_t img_height = resolution[FRAMESIZE_HQVGA].height;
    uint8_t *img_rgb888 = heap_caps_malloc(img_width*img_height*3, MALLOC_CAP_8BIT|MALLOC_CAP_SPIRAM);
    if (NULL == img_rgb888)
    {
        ESP_LOGE(TAG, "malloc for rgb888 failed");
    }
    
    while (1)
    {
        int64_t fr_start = esp_timer_get_time();
        image_fb = esp_camera_fb_get();
        if (!image_fb)
        {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        if (image_fb->format == PIXFORMAT_JPEG)
        {
            if (ONENET_NET_STATUS_CONNECT_PLATFORM == onenet_info.status)
            {
                if (++image_cnt > 1)
                {
                    image_cnt = 0;
                    ESP_LOGI(TAG, "send image to onenet");
                    OneNET_Send_BinFile("image", (const uint8_t *)image_fb->buf, image_fb->len);
                }
            }
            {
                jpg2rgb888((const uint8_t *)image_fb->buf, image_fb->len, img_rgb888, JPG_SCALE_NONE);
                uint32_t pix_count = img_width*img_height;
                for(uint32_t i=0; i<pix_count; i++) {
                    uint16_t b = img_rgb888[3*i];
                    uint16_t g = img_rgb888[3*i+1];
                    uint16_t r = img_rgb888[3*i+2];
                    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                    img_rgb888[2*i] = c>>8;
                    img_rgb888[2*i+1] = c&0xff;
                    // hb = *src_buf++;
                    // lb = *src_buf++;
                    // *rgb_buf++ = (lb & 0x1F) << 3;
                    // *rgb_buf++ = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
                    // *rgb_buf++ = hb & 0xF8;
                }
            }

            char strftime_buf[64];
            struct tm timeinfo;
            app_sntp_get_time(&timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%y-%m-%d_%H-%M-%S.jpg", &timeinfo);
            // image_save(image_fb->buf, image_fb->len,strftime_buf);
            lcd.draw_bitmap(0, 0, img_width, img_height, img_rgb888);
            // screen_clear(COLOR_ESP_BKGD);vTaskDelay(500 / portTICK_PERIOD_MS);
            // screen_clear(COLOR_BLUE);
            
        }
        
        ESP_LOGI(TAG, "JPG: %fKB %ums", ((float)image_fb->len/1024), (uint32_t)((esp_timer_get_time() - fr_start) / 1000));
        esp_camera_fb_return(image_fb);

        // vTaskDelay(500 / portTICK_PERIOD_MS);
    }

}


static void onenet_task(void *arg)
{
    uint16_t i = 0, step = 0;

    while (1)
    {
        OneNET_RevPro();
        
        switch (step)
        {
        case 0:
        {
            onenet_info.status = ONENET_NET_STATUS_CONNECT_NONE;
            ESP_LOGI(TAG, "OneNET_GetLinkIP");
            OneNET_GetLinkIP(onenet_info.protocol, onenet_info.ip, onenet_info.port);  //尝试获取所使用协议的IP和端口
            ESP_LOGI(TAG, "OneNET_DevConnect");
            OneNET_DevConnect(onenet_info.dev_id, onenet_info.pro_id, onenet_info.auif);
            step = 1;
        } break;
        case 1:
        {
            ESP_LOGI(TAG, "WAIT ONENET_NET_STATUS_CONNECT_PLATFORM");
            if (ONENET_NET_STATUS_CONNECT_PLATFORM == onenet_info.status)
            {
                ESP_LOGI(TAG, "ONENET_NET_STATUS_CONNECT_PLATFORM");
                step = 2;
                break;
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }break;
        case 2:
        {
            if (!SYS_STATUS_GET(SYS_STATUS_CONNECTED))
            {
                ESP_LOGI(TAG, "APPWIFI_DISCONNECTED_BIT");
                step = 0;
                onenet_info.status = ONENET_NET_STATUS_CONNECT_NONE;
                break;
            }

            if (ONENET_NET_STATUS_CONNECT_PLATFORM == onenet_info.status)
            {
                if (++i > 300)
                {
                    i = 0;
                    if (0 != OneNET_SendData_Heart())
                    {

                    }
                }
            }
            // ESP_LOGI(TAG, "Free heap: %u", xPortGetFreeHeapSize());
            // ESP_LOGI(TAG, "number of tasks: %u", uxTaskGetNumberOfTasks());
            // ESP_LOGI(TAG, "misc_task high Stack: %u", uxTaskGetStackHighWaterMark(task_handle_misc));
            // ESP_LOGI(TAG, "onenet_task high Stack: %u", uxTaskGetStackHighWaterMark(task_handle_onenet));

            vTaskDelay(300 / portTICK_PERIOD_MS);
        } break;
        default:
            break;
        }

    }
}

static void misc_task(void *arg)
{
    led_flash_set(1);
    while (1)
    {
        if(SYS_STATUS_GET(SYS_STATUS_CONNECTED))
        {
            if(ONENET_NET_STATUS_CONNECT_PLATFORM == onenet_info.status)
            {
                led_flash_set(3);
            }else
            {
                led_flash_set(2);
            }
        }else
        {
            led_flash_set(1);
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
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

    led_init();
    app_sdcard_init();
    do_mkdir("/sdcard/picture", 0755);
    lcd_init();
    
    bool is_configured;
    captive_portal_start("ESP_WEB_CONFIG", NULL, &is_configured);

    if (is_configured) {
        wifi_config_t wifi_config;
        esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);
        ESP_LOGI(TAG, "SSID:%s, PASSWORD:%s", wifi_config.sta.ssid, wifi_config.sta.password);
    }
    captive_portal_wait(portMAX_DELAY);
    SYS_STATUS_SET(SYS_STATUS_CONNECTED);
    app_sntp_start();

    xTaskCreate(camera_task,
                "camera_task",
                4096,
                NULL,
                7,
                &task_handle_camera
               );
    xTaskCreate(misc_task,
                "misc_task",
                4096,
                NULL,
                6,
                &task_handle_misc
               );
    // xTaskCreate(onenet_task,
    //             "onenet_task",
    //             4096,
    //             NULL,
    //             5,
    //             &task_handle_onenet
    //            );

    //app_httpd_main();
}



