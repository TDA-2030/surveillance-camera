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

static const char *TAG = "app_main";

#define CAM_CHECK(a, str, ret)  if(!(a)) {                                             \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);      \
        return (ret);                                                                   \
        }

static TaskHandle_t task_handle_onenet;
static TaskHandle_t task_handle_misc;
static TaskHandle_t task_handle_camera;

SysStatus_t SystemStatus = 0;




static esp_err_t image_save(uint8_t *pdata, uint32_t length, const char *path)
{

    uint32_t written, len = length;
    int64_t fr_start = esp_timer_get_time();
    char name[64];
    sprintf(name, "/sdcard/%s", path);
    FILE *f = fopen(name, "wb");
    int64_t fr_end = esp_timer_get_time();
    ESP_LOGI(TAG, "open [%s] time:%ums", name, (uint32_t)((fr_end - fr_start) / 1000));
    CAM_CHECK(NULL != f, "Failed to open file for writing", ESP_FAIL);
    
    do
    {
        written = fwrite(pdata, 1, len, f);
        len -= written;
        pdata += written;

    }
    while ( written && len );
    fclose(f);
    fr_end = esp_timer_get_time();
    ESP_LOGI(TAG, "File written: %uB %ums", length, (uint32_t)((fr_end - fr_start) / 1000));
    
    return ESP_OK;
}

static void camera_task(void *arg)
{
    uint8_t image_cnt = 0;
    camera_fb_t *image_fb = NULL;

    app_camera_init();
    int res = 0;
    sensor_t *s = esp_camera_sensor_get();
    res = s->set_framesize(s, FRAMESIZE_UXGA);
    if (res)
    {
        ESP_LOGE(TAG, "Camera set_framesize failed");
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

            // char strftime_buf[64];
            // struct tm timeinfo;
            // app_sntp_get_time(&timeinfo);
            // strftime(strftime_buf, sizeof(strftime_buf), "%y-%m-%d_%H-%M-%S.jpg", &timeinfo);
            // image_save(image_fb->buf, image_fb->len,strftime_buf);
        }
        
        ESP_LOGI(TAG, "JPG: %fKB %ums", ((float)image_fb->len/1024), (uint32_t)((esp_timer_get_time() - fr_start) / 1000));
        esp_camera_fb_return(image_fb);

        vTaskDelay(500 / portTICK_PERIOD_MS);
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
        
        // struct tm timeinfo;
        // char strftime_buf[64];
        // app_sntp_get_time(&timeinfo);
        // strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        // ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
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
    
    bool is_configured;
    captive_portal_start("ESP_WEB_CONFIG", NULL, &is_configured);

    if (is_configured) {
        wifi_config_t wifi_config;
        esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);
        ESP_LOGI(TAG, "SSID:%s, PASSWORD:%s", wifi_config.sta.ssid, wifi_config.sta.password);
    }
    captive_portal_wait(portMAX_DELAY);
    SYS_STATUS_SET(SYS_STATUS_CONNECTED);

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
    xTaskCreate(onenet_task,
                "onenet_task",
                4096,
                NULL,
                5,
                &task_handle_onenet
               );

    //app_httpd_main();
}



