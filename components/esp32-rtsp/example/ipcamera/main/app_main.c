

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <pthread.h>
#include "esp_timer.h"
#include "iot_button.h"
#include "app_wifi.h"
#include "rtsp_server.h"
#include "rtsp_client.h"
#include "media_mjpeg.h"
#include "media_g711a.h"
#include "media_l16.h"
#include "g711.h"
#include "esp_camera.h"
#include "app_mic.h"
#include "app_sntp.h"
#include "usb_camera.h"
#include "file_manage.h"
#include "web_portal.h"

// #define CONFIG_USB_UVC


static const char *TAG = "esp32-ipc";

#define USE_AUDIO CONFIG_RTSP_USE_AUDIO
#define PUSHER    CONFIG_ESP32_AS_CLIENT
#define SERVER_IP CONFIG_RTSP_SERVER_IP
#define SERVER_PORT CONFIG_RTSP_SERVER_PORT

#define AUDIO_RATE 8000

char *wave_get(void);
uint32_t wave_get_size(void);
uint32_t wave_get_framerate(void);
uint32_t wave_get_bits(void);
uint32_t wave_get_ch(void);

static media_stream_t *mjpeg;
static media_stream_t *m_audio;
static TaskHandle_t task_hdl_audio;
static TaskHandle_t task_hdl_video;
static void *mic_handle;

#ifdef CONFIG_USB_UVC
#define camera_get usb_camera_fb_get
#define camera_return usb_camera_fb_return
#else
#define camera_get esp_camera_fb_get
#define camera_return esp_camera_fb_return
#endif

esp_err_t camera_init(uint32_t mclk_freq, const pixformat_t pixel_fromat, const framesize_t frame_size)
{
#define CONFIG_CAMERA_MODEL_ESP_EYE 1
#include "app_camera.h"
    camera_config_t camera_config = {
        .pin_pwdn = PWDN_GPIO_NUM,
        .pin_reset = RESET_GPIO_NUM,
        .pin_xclk = XCLK_GPIO_NUM,
        .pin_sscb_sda = SIOD_GPIO_NUM,
        .pin_sscb_scl = SIOC_GPIO_NUM,

        .pin_d0 = Y2_GPIO_NUM,
        .pin_d1 = Y3_GPIO_NUM,
        .pin_d2 = Y4_GPIO_NUM,
        .pin_d3 = Y5_GPIO_NUM,
        .pin_d4 = Y6_GPIO_NUM,
        .pin_d5 = Y7_GPIO_NUM,
        .pin_d6 = Y8_GPIO_NUM,
        .pin_d7 = Y9_GPIO_NUM,
        .pin_vsync = VSYNC_GPIO_NUM,
        .pin_href = HREF_GPIO_NUM,
        .pin_pclk = PCLK_GPIO_NUM,

        //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
        .xclk_freq_hz = mclk_freq,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = pixel_fromat, //YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = frame_size,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

        .jpeg_quality = 12, //0-63 lower number means higher quality
        .fb_count = 2,       //if more than one, i2s runs in continuous mode. Use only with JPEG
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
        .fb_location = CAMERA_FB_IN_PSRAM,
    };

    // camera init
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1); //flip it back
    s->set_hmirror(s, 1);
    s->set_saturation(s, 1);
    //initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
        s->set_brightness(s, 1);  //up the blightness just a bit
        s->set_saturation(s, -2); //lower the saturation
    }
    return ESP_OK;
}
static uint64_t video_interval = 0;
static void streamImage(media_stream_t *mjpeg_stream)
{
    if (!mjpeg_stream) {
        vTaskDelay(pdMS_TO_TICKS(20));
        return;
    }

    static uint64_t last_frame = 0;
    video_interval = (esp_timer_get_time() - last_frame);
    uint64_t interval = video_interval / 1000;
    last_frame = esp_timer_get_time();
    camera_fb_t *pic = camera_get();

    // printf("frame fps=%f\n", 1000.0f / (float)interval);
    uint8_t *p = pic->buf;
    uint32_t len = pic->len;
    mjpeg_stream->handle_frame(mjpeg_stream, p, len);
    camera_return(pic);
}

#if USE_AUDIO
static uint8_t buffer[8 * AUDIO_RATE * 40 / 1000];
static void streamaudio(media_stream_t *audio_stream)
{
    if (!audio_stream) {
        vTaskDelay(pdMS_TO_TICKS(20));
        ESP_LOGE(TAG, "audio break\n");
        return;
    }
    static uint64_t audio_last_frame = 0;
    uint64_t interval = (esp_timer_get_time() - audio_last_frame) / 1000;
    audio_last_frame = esp_timer_get_time();
    // printf("audio fps=%f\n", 1000.0f / (float)interval);

    uint32_t len = 0;
    if (RTP_PAYLOAD_PCMA == audio_stream->rtp_profile.pt) {
        mic_get_data(mic_handle, buffer, 8 * AUDIO_RATE * 40 / 1000, &len);

        int16_t *pcm = (int16_t *)buffer;
        for (size_t i = 0; i < len; i++) {
            pcm[i] *= 8; // enlarge voice
            buffer[i] = linear2alaw(pcm[i]);
        }
        audio_stream->handle_frame(audio_stream, buffer, len / 2);
    } else  if (RTP_PAYLOAD_L16_CH1 == audio_stream->rtp_profile.pt) {


#if 1
        mic_get_data(mic_handle, buffer, 8 * AUDIO_RATE * 40 / 1000, &len);
#else
        static uint32_t audio_offset = 0;
        vTaskDelay(pdMS_TO_TICKS(40));
        len = 2 * AUDIO_RATE * 40 / 1000;
        if (len + audio_offset > wave_get_size()) {
            uint32_t before = wave_get_size() - audio_offset;
            memcpy(buffer, wave_get() + audio_offset, before);
            uint32_t remain = len + audio_offset - wave_get_size();
            audio_offset = 0;
            memcpy(buffer + before, wave_get() + audio_offset, remain);
        } else {
            memcpy(buffer, wave_get() + audio_offset, len);
        }
        audio_offset += len;
#endif
        int16_t *pcm = (int16_t *)buffer;
        for (size_t i = 0; i < len / 2; i++) {
            pcm[i] *= 8; // enlarge voice
            pcm[i] = pcm[i] >> 8 | pcm[i] << 8;

        }
        audio_stream->handle_frame(audio_stream, buffer, len);
    }
}

static void send_audio(void *args)
{
#if PUSHER
    rtsp_client_t *session = (rtsp_client_t *)args;
#else
    rtsp_server_t *session = (rtsp_server_t *)args;
#endif
    while (!(session->state & 0x02)) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    while (1) {
        streamaudio(m_audio);
        if (!(session->state & 0x02)) {
            vTaskDelay(pdMS_TO_TICKS(5));
            ESP_LOGW(TAG, "Delete audio task");
            vTaskDelete(NULL);
        }
    }
}
#endif

static void send_video(void *args)
{
#if PUSHER
    rtsp_client_t *session = (rtsp_client_t *)args;
#else
    rtsp_server_t *session = (rtsp_server_t *)args;
#endif
    while (!(session->state & 0x02)) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    while (1) {
        streamImage(mjpeg);
        if (!(session->state & 0x02)) {
            vTaskDelay(pdMS_TO_TICKS(5));
            ESP_LOGW(TAG, "Delete video task");
            vTaskDelete(NULL);
        }
    }
}

static void rtsp_print_task(void *args)
{
#if PUSHER
    rtsp_client_t *rtsp = (rtsp_client_t *)args;
#else
    rtsp_server_t *rtsp = (rtsp_server_t *)args;
#endif
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        // printf("Free heap: %d\n", esp_get_free_internal_heap_size());
        rtp_statistics_info_t info;
        media_streams_t *it;
        SLIST_FOREACH(it, &rtsp->media_list, next) {
            if (it->media_stream->rtp_session) {
                rtp_get_statistics(it->media_stream->rtp_session, &info);
                time_t now;
                struct tm timeinfo;
                // time(&now);
                now = rtp_time_now_us() / 1000000;
                localtime_r(&now, &timeinfo);
                ESP_LOGI(TAG, "speed=%.3f KB/s, total size=%.3f MB, elapse=%d s, fps=%.3f, (%d:%d:%d)",
                         info.speed_kbs, (float)(info.total_kb) / 1024, info.elapsed_sec, 1000000.0f/(float)video_interval, 
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            }
        }
    }
}

static int create_media_task(void *args)
{
    BaseType_t xReturned;
#if USE_AUDIO
    xReturned = xTaskCreate(send_audio, "audio", 2560, (void *) args, configMAX_PRIORITIES - 1, &task_hdl_audio);
    if (pdPASS != xReturned) {
        ESP_LOGE(TAG, "create audio task failed");
    }

#endif
    xReturned = xTaskCreate(send_video, "video", 2560, (void *) args, configMAX_PRIORITIES - 1, &task_hdl_video);
    if (pdPASS != xReturned) {
        ESP_LOGE(TAG, "create video task failed");
    }
    return 0;
}

#if !PUSHER
static void server_run()
{
    tcpip_adapter_ip_info_t if_ip_info;
    char ip_str[64] = {0};
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &if_ip_info);
    sprintf(ip_str, "rtsp://%d.%d.%d.%d", IP2STR(&if_ip_info.ip));
    ESP_LOGI(TAG, "Creating RTSP session [%s:%hu/%s]", ip_str, 8554, "mjpeg/1");

    rtsp_server_cfg_t rtsp_cfg = {
        .url = "mjpeg/1",
        .port = 8554,
        .accept_cb_fn = create_media_task,
        .session_name = "esp32-ipc",
    };
    rtsp_server_t *rtsp = rtsp_server_create(&rtsp_cfg);
    mjpeg = media_stream_mjpeg_create();
    // m_audio = media_stream_l16_create(AUDIO_RATE);
    m_audio = media_stream_g711a_create(AUDIO_RATE);

    rtsp_server_add_media_stream(rtsp, mjpeg);
#if USE_AUDIO
    rtsp_server_add_media_stream(rtsp, m_audio);
#endif
    xTaskCreate(rtsp_print_task, "print", 3000, (void *) rtsp, 1, NULL);

}

#else
static void pusher_video_run()
{
    uint8_t mac_data[6] = {0};
    char url[128] = {0};
    esp_read_mac(mac_data, ESP_MAC_WIFI_STA);
    sprintf(url, "rtsp://%s:%u/esp32_%02X%02X_rtsp", SERVER_IP, SERVER_PORT, mac_data[4], mac_data[5]);
    rtsp_client_t *rtsp = rtsp_client_create();
    mjpeg = media_stream_mjpeg_create();
    m_audio = media_stream_g711a_create(AUDIO_RATE);
    rtsp_client_add_media_stream(rtsp, mjpeg);
#if USE_AUDIO
    rtsp_client_add_media_stream(rtsp, m_audio);
#endif

    int ret = rtsp_client_push_media(rtsp, url, RTP_OVER_TCP);
    if (0 != ret) {
        ESP_LOGE(TAG, "push error");
        return;
    }

    create_media_task(rtsp);
    xTaskCreate(rtsp_print_task, "print", 3000, (void *) rtsp, 1, NULL);

}
#endif

static void button_long_press_start_cb(void *arg)
{
    ESP_LOGW(TAG, "Restore factory settings");
    nvs_flash_erase();
    esp_restart();
}


float _camera_test_fps(uint16_t times)
{
    ESP_LOGI(TAG, "satrt to test fps");
    camera_return(camera_get());
    camera_return(camera_get());

    uint64_t total_time = esp_timer_get_time();
    for (size_t i = 0; i < times; i++) {
        uint64_t s = esp_timer_get_time();
        camera_fb_t *pic = camera_get();

        if (NULL == pic) {
            ESP_LOGW(TAG, "fb get failed");
            continue;
        }
        printf("fb_get: (%d x %d) %lluUS\n", pic->width, pic->height, esp_timer_get_time() - s); s = esp_timer_get_time();
        camera_return(pic);
    }
    total_time = esp_timer_get_time() - total_time;
    float fps = times / (total_time / 1000000.0f);
    return fps;
}

void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU cores, WiFi%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);
    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    printf("Free heap: %d\n", esp_get_free_internal_heap_size());

    ESP_ERROR_CHECK(fm_init());
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = 0,
            .active_level = 0,
        },
    };
    button_handle_t btn = iot_button_create(&cfg);
    if (NULL == btn) {
        return;
    }
    iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_long_press_start_cb);
    printf("Free heap: %d\n", esp_get_free_internal_heap_size());

#ifdef CONFIG_USB_UVC
    usb_camera_init();
#else
    if (ESP_OK != camera_init(10000000, PIXFORMAT_JPEG, FRAMESIZE_SVGA)) {
        return;
    }
#endif

    printf("Free heap: %d\n", esp_get_free_internal_heap_size());

#if USE_AUDIO
    mic_handle = mic_init(I2S_NUM_1, AUDIO_RATE);
#endif

    app_wifi_main();
    portal_camera_start();
    ESP_LOGI(TAG, "fps=%f", _camera_test_fps(16));
    app_sntp_init();

    printf("Free heap: %d\n", esp_get_free_internal_heap_size());
#if PUSHER
    pusher_video_run();
#else
    server_run();
#endif
    printf("Free heap: %d\n", esp_get_free_internal_heap_size());
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
