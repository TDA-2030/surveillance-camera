

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <sys/param.h>
#include "esp_timer.h"
#include "rtsp_server.h"
#include "rtsp_client.h"
#include "media_mjpeg.h"
#include "media_g711a.h"
#include "media_l16.h"
#include "g711.h"
#include "esp_camera.h"
#include "app_mic.h"


static const char *TAG = "esp32-ipc";

#define USE_AUDIO CONFIG_RTSP_USE_AUDIO
#define PUSHER    CONFIG_ESP32_AS_CLIENT
#define SERVER_IP CONFIG_RTSP_SERVER_IP
#define SERVER_PORT CONFIG_RTSP_SERVER_PORT

#define AUDIO_RATE 8000

static media_stream_t *mjpeg;
static media_stream_t *m_audio;
static TaskHandle_t task_hdl_audio;
static TaskHandle_t task_hdl_video;
static void *mic_handle;

static camera_fb_t *(*camera_get)(void);
static void (*camera_return)(camera_fb_t *fb);


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
                         info.speed_kbs, (float)(info.total_kb) / 1024, info.elapsed_sec, 1000000.0f / (float)video_interval,
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


void ipc_server_start(camera_fb_t *(*func_camera_get)(void),
                      void (*func_camera_return)(camera_fb_t *fb))
{
    camera_get = func_camera_get;
    camera_return = func_camera_return;
#if USE_AUDIO
    mic_handle = mic_init(I2S_NUM_1, AUDIO_RATE);
#endif

#if PUSHER
    pusher_video_run();
#else
    server_run();
#endif
}
