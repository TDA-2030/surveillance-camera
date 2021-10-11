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

#include "app_main.h"
#include "app_sntp.h"
#include "app_camera.h"
#include "captive_portal.h"
#include "web_portal.h"
#include "esp_camera.h"
#include "app_led.h"
#include "screen_driver.h"
#include "file_manage.h"
#include "web_portal.h"
#include "avi_recorder.h"
#include "vidoplayer.h"

static const char *TAG = "app_main";

#define CAM_CHECK(a, str, ret)  if(!(a)) {                                             \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);      \
        return (ret);                                                                   \
        }

#define USE_WIFI 1

SysStatus_t SystemStatus = 0;


static esp_err_t camera_init(uint32_t xclk_freq_hz, pixformat_t pixel_format, framesize_t frame_size, uint8_t fb_count)
{
    framesize_t size_bak = frame_size;
    if (PIXFORMAT_JPEG == pixel_format && FRAMESIZE_SVGA > frame_size) {
        frame_size = FRAMESIZE_HD;
    }
    camera_config_t camera_config = {
        .pin_pwdn = PWDN_GPIO_NUM,
        .pin_reset = RESET_GPIO_NUM,
        .pin_xclk = XCLK_GPIO_NUM,
        .pin_sscb_sda = SIOD_GPIO_NUM,
        .pin_sscb_scl = SIOC_GPIO_NUM,

        .pin_d7 = Y9_GPIO_NUM,
        .pin_d6 = Y8_GPIO_NUM,
        .pin_d5 = Y7_GPIO_NUM,
        .pin_d4 = Y6_GPIO_NUM,
        .pin_d3 = Y5_GPIO_NUM,
        .pin_d2 = Y4_GPIO_NUM,
        .pin_d1 = Y3_GPIO_NUM,
        .pin_d0 = Y2_GPIO_NUM,
        .pin_vsync = VSYNC_GPIO_NUM,
        .pin_href = HREF_GPIO_NUM,
        .pin_pclk = PCLK_GPIO_NUM,

        //EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
        .xclk_freq_hz = xclk_freq_hz,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = pixel_format, //YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = frame_size,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

        .jpeg_quality = 12, //0-63 lower number means higher quality
        .fb_count = fb_count,       //if more than one, i2s runs in continuous mode. Use only with JPEG
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY
    };

    //initialize the camera
    esp_err_t ret = esp_camera_init(&camera_config);
    sensor_t *s = esp_camera_sensor_get();
    if (ESP_OK == ret && PIXFORMAT_JPEG == pixel_format && FRAMESIZE_SVGA > size_bak) {

        s->set_framesize(s, size_bak);
    }
    s->set_vflip(s, 1);

    return ret;
}

float _camera_test_fps(uint16_t times)
{
    ESP_LOGI(TAG, "satrt to test fps");
    esp_camera_fb_return(esp_camera_fb_get());
    esp_camera_fb_return(esp_camera_fb_get());

    uint64_t total_time = esp_timer_get_time();
    for (size_t i = 0; i < times; i++) {
        uint64_t s = esp_timer_get_time();
        camera_fb_t *pic = esp_camera_fb_get();

        if (NULL == pic) {
            ESP_LOGW(TAG, "fb get failed");
            continue;
        }
        printf("fb_get: (%d x %d) %lluUS\n", pic->width, pic->height, esp_timer_get_time() - s); s = esp_timer_get_time();
        esp_camera_fb_return(pic);
    }
    total_time = esp_timer_get_time() - total_time;
    float fps = times / (total_time / 1000000.0f);
    return fps;
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

    do {
        written = fwrite(pdata, 1, len, f); printf("len=%d, written=%d\n", len, written);
        len -= written;
        pdata += written;

    } while ( written && len );
    fclose(f);
    fr_end = esp_timer_get_time();
    ESP_LOGI(TAG, "File written: %uB %ums", length, (uint32_t)((fr_end - fr_start) / 1000));

    return ESP_OK;
}

#if USE_LCD
scr_driver_t g_lcd;
static scr_info_t lcd_info;
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

    spi_config_t spi_cfg = {
        .miso_io_num = -1,
        .mosi_io_num = 15, //txd
        .sclk_io_num = 14, //rxd
        .max_transfer_sz = 240 * 240 * 2 + 10,
    };
    spi_bus_handle_t spi_bus = spi_bus_create(VSPI_HOST, &spi_cfg);
    ESP_LOGI(TAG, "lcd_init");

    scr_interface_spi_config_t spi_lcd_cfg = {
        .spi_bus = spi_bus,
        .pin_num_cs = 12,
        .pin_num_dc = 4,
        .clk_freq = 80000000,
        .swap_data = 0,
    };

    scr_interface_driver_t *iface_drv;
    scr_interface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &iface_drv);

    scr_controller_config_t lcd_cfg = {0};
    lcd_cfg.iface_drv = iface_drv;
    lcd_cfg.pin_num_rst = -1;
    lcd_cfg.pin_num_bckl = -1;
    lcd_cfg.rst_active_level = 0;
    lcd_cfg.bckl_active_level = 1;
    lcd_cfg.offset_hor = 0;
    lcd_cfg.offset_ver = 0;
    lcd_cfg.width = 240;
    lcd_cfg.height = 240;
    lcd_cfg.rotate = SCR_DIR_BTLR;
    scr_init(SCREEN_CONTROLLER_ST7789, &lcd_cfg, &g_lcd);

    screen_clear(COLOR_ESP_BKGD); vTaskDelay(500 / portTICK_PERIOD_MS);
    screen_clear(COLOR_BLUE); vTaskDelay(500 / portTICK_PERIOD_MS);
    screen_clear(COLOR_RED); vTaskDelay(500 / portTICK_PERIOD_MS);
}


static void camera_task(void *arg)
{
    uint8_t image_cnt = 0;
    camera_fb_t *image_fb = NULL;

    int res = 0;
    sensor_t *s = esp_camera_sensor_get();
    res = s->set_framesize(s, FRAMESIZE_HQVGA);
    // res |= s->set_vflip(s, true);
    // res |= s->set_hmirror(s, true);
    if (res) {
        ESP_LOGE(TAG, "Camera set_framesize failed");
    }

    uint16_t img_width = resolution[FRAMESIZE_HQVGA].width;
    uint16_t img_height = resolution[FRAMESIZE_HQVGA].height;
    uint8_t *img_rgb888 = heap_caps_malloc(img_width * img_height * 2, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (NULL == img_rgb888) {
        ESP_LOGE(TAG, "malloc for rgb888 failed");
    }

    while (1) {
        int64_t fr_start = esp_timer_get_time();
        image_fb = esp_camera_fb_get();
        if (!image_fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        if (image_fb->format == PIXFORMAT_JPEG) {

            jpg2rgb565((const uint8_t *)image_fb->buf, image_fb->len, img_rgb888, JPG_SCALE_NONE);

            char strftime_buf[64];
            struct tm timeinfo;
            app_sntp_get_time(&timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%y-%m-%d_%H-%M-%S.jpg", &timeinfo);
            // image_save(image_fb->buf, image_fb->len,strftime_buf);
            g_lcd.draw_bitmap(0, 0, img_width, img_height, img_rgb888);
            // screen_clear(COLOR_ESP_BKGD);vTaskDelay(500 / portTICK_PERIOD_MS);
            // screen_clear(COLOR_BLUE);

        }

        ESP_LOGI(TAG, "JPG: %fKB %ums", ((float)image_fb->len / 1024), (uint32_t)((esp_timer_get_time() - fr_start) / 1000));
        esp_camera_fb_return(image_fb);

        // vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
#endif

#include "esp_task_wdt.h"
esp_err_t WR_test(const char *path, const size_t length, float *out_read_speed, float *out_write_speed)
{
    const size_t block_size = 1024 * 4;

    char filename[64] = {0};
    strcat(filename, path);
    strcat(filename, "/hello.txt");

    // Open file for write
    int f = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    if (f == -1) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    uint8_t *data_buf = NULL;
    data_buf = malloc(block_size);

    if (NULL == data_buf) {
        ESP_LOGE(TAG, "Failed to malloc");
        return ESP_FAIL;
    }
    for (size_t i = 0; i < block_size; i++) {
        data_buf[i] = esp_random() >> 24;
    }

    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(xPortGetCoreID()));
    int64_t w_start = esp_timer_get_time();
    static int64_t w_t;
    size_t remain = length;

    // while (remain) {
    //     int wl = remain >= block_size ? block_size : remain;
    //     int res = write(f, data_buf, wl);
    //     remain -= res;
    // }
    remain = 160;
    while (remain) {
        // camera_fb_t *image_fb = esp_camera_fb_get();
        vTaskDelay(pdMS_TO_TICKS(40));
        write(f, data_buf, 8 * 1024);
        write(f, data_buf, 8 * 1024);
        // write(f, data_buf, 4*1024);
        // write(f, data_buf, 4*1024);

        // esp_camera_fb_return(image_fb);
        remain --;
    }
    w_t = esp_timer_get_time() - w_start;
    printf("t=%llu\n", w_t);

    close(f);
    memset(data_buf, 0, block_size);

    // Open file for reading
    f = open(filename, O_RDONLY);
    if (f == -1) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        goto err;
    }

    int64_t r_start;
    static int64_t r_t;
    remain = length;
    r_t = 0;

    while (remain) {
        int rl = remain >= block_size ? block_size : remain;
        r_start = esp_timer_get_time();
        int res = read(f, data_buf, rl);
        r_t += (esp_timer_get_time() - r_start);

        for (size_t i = 0; i < res; i++) {
            if (data_buf[i] != 0x36) {
                break;
            }
        }
        remain -= block_size;
    }
    close(f);
    free(data_buf);

    if (remain) {
        ESP_LOGE(TAG, "data error");
        goto err;
    }

    *out_read_speed = ((float)length / (float)r_t) * (1e6 / 1048576);
    *out_write_speed = ((float)length / (float)w_t) * (1e6 / 1048576);
    esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(xPortGetCoreID()));
    return ESP_OK;
err:
    esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(xPortGetCoreID()));
    return ESP_FAIL;
}

static int _get_frame(void **buf, size_t *len, int *w, int *h)
{
    camera_fb_t *image_fb = esp_camera_fb_get();
    if (!image_fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        return -1;
    } else {
        ESP_LOGI(TAG, "buf=%p", image_fb);
        *buf = &image_fb->buf;
        *len = image_fb->len;
        *w = image_fb->width;
        *h = image_fb->height;
    }
    return 0;
}

static int _return_frame(void *inbuf)
{
    camera_fb_t *image_fb = __containerof(inbuf, camera_fb_t, buf);
    esp_camera_fb_return(image_fb);
    return 0;
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    led_init(3);
    led_set_seq(_led_seq_1, sizeof(_led_seq_1));
    camera_init(20000000, PIXFORMAT_JPEG, FRAMESIZE_VGA, 3);
    ESP_LOGI(TAG, "fps=%f", _camera_test_fps(16));
    ESP_ERROR_CHECK(fm_init()); /* Initialize file storage */
#if USE_LCD
    lcd_init();
#endif

    // float ws;
    // float rs;
    // printf("| 次数 |       写      |       读      |\n");
    // for (size_t i = 0; i < 1; i++) {
    //     WR_test("/sdcard", 3 * 1024 * 1024, &rs, &ws);
    //     printf( "|  %d   |  %fMB/s |  %fMB/s |\n", i, ws, rs);
    // }

#if USE_WIFI
    bool is_configured;
    captive_portal_start("ESP_WEB_CONFIG", NULL, &is_configured);

    if (is_configured) {
        wifi_config_t wifi_config;
        esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);
        ESP_LOGI(TAG, "SSID:%s, PASSWORD:%s", wifi_config.sta.ssid, wifi_config.sta.password);
    }
    captive_portal_wait(portMAX_DELAY);
    SYS_STATUS_SET(SYS_STATUS_CONNECTED);
    app_sntp_init();
    esp_wifi_set_ps(WIFI_PS_NONE);

    portal_camera_start();
#endif
    vTaskDelay(pdMS_TO_TICKS(1000));
    // avi_play("/sdcard/tom-240.avi");
    led_set_seq(_led_seq_2, 4);
    avi_recorder_start("/sdcard/recorde.avi", _get_frame, _return_frame, 10 * 2, 1);
    led_set_seq(_led_seq_1, sizeof(_led_seq_1));
}
