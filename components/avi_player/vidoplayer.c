#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "file_manage.h"
#include "pwm_audio.h"
#include "avifile.h"
#include "vidoplayer.h"
#include "mjpeg.h"

static const char *TAG = "avi player";

/**
 * TODO: how to recognize each stream id
 */
#define  T_vids  _REV(0x30306463)
#define  T_auds  _REV(0x30317762)

extern AVI_TypeDef AVI_file;

static uint32_t _REV(uint32_t value){
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 | 
        (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24; 
}

static void audio_init(void)
{
    pwm_audio_config_t pac;
    pac.duty_resolution    = LEDC_TIMER_10_BIT;
    pac.gpio_num_left      = 12;
    pac.ledc_channel_left  = LEDC_CHANNEL_0;
    pac.gpio_num_right     = -1;
    pac.ledc_channel_right = LEDC_CHANNEL_1;
    pac.ledc_timer_sel     = LEDC_TIMER_0;
    pac.tg_num             = TIMER_GROUP_0;
    pac.timer_num          = TIMER_0;
    pac.ringbuf_len        = 1024 * 32;
    pwm_audio_init(&pac);

    pwm_audio_set_volume(0);
}

static uint32_t read_frame(FILE *file, uint8_t *buffer, uint32_t *fourcc)
{
    AVI_CHUNK_HEAD head;
    fread(&head, sizeof(AVI_CHUNK_HEAD), 1, file);
    if (head.FourCC) {
        /* code */
    }
    *fourcc = head.FourCC;
    if (head.size % 2) {
        head.size++;    //奇数加1
    }
    uint32_t ret = fread(buffer, head.size, 1, file);
    return head.size;
}

void avi_play(const char *filename)
{
    FILE *avi_file;
    int ret;
    size_t  BytesRD;
    uint32_t  Strsize;
    uint32_t  Strtype;
    uint8_t *pbuffer;

    avi_file = fopen(filename, "rb");
    if (avi_file == NULL) {
        ESP_LOGE(TAG, "Cannot open %s", filename);
        return;
    }

    pbuffer = malloc(1024 * 30);
    if (pbuffer == NULL) {
        ESP_LOGE(TAG, "Cannot alloc memory for palyer");
        fclose(avi_file);
        return;
    }

    BytesRD = fread(pbuffer, 20480, 1, avi_file);
    ret = AVI_Parser(pbuffer, BytesRD);
    if (0 > ret) {
        ESP_LOGE(TAG, "parse failed (%d)", ret);
        return;
    }
    
    audio_init();
    pwm_audio_set_param(AVI_file.auds_sample_rate, AVI_file.auds_bits, AVI_file.auds_channels);
    pwm_audio_start();

    fseek(avi_file, AVI_file.movi_start, SEEK_SET); // 偏移到movi list
    Strsize = read_frame(avi_file, pbuffer, &Strtype);
    BytesRD = Strsize+8;

    while (1) { //播放循环
        if (Strtype == T_vids) { //显示帧
            mjpegdraw(pbuffer, Strsize);

        }//显示帧
        else if (Strtype == T_auds) { //音频输出
            size_t cnt;
            pwm_audio_write((uint8_t *)pbuffer, Strsize, &cnt, 500 / portTICK_PERIOD_MS);

        } else {
            ESP_LOGE(TAG, "unknow frame");
            break;
        }
        if (BytesRD >= AVI_file.movi_size) {
            ESP_LOGI(TAG, "paly end");
            break;
        }
        Strsize = read_frame(avi_file, pbuffer, &Strtype); //读入整帧
        
        ESP_LOGI(TAG, "type=%x, size=%d", Strtype, Strsize);
        BytesRD += Strsize+8;
    }
    free(pbuffer);
    fclose(avi_file);
}
