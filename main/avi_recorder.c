#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "string.h"
#include "esp_log.h"
#include "file_manage.h"
#include "esp_heap_caps.h"
#include "avi_def.h"
#include "esp_camera.h"

static const char *TAG = "avi recorder";

#define MEM_ALIGNMENT 4
#define MEM_ALIGN_SIZE(size) (((size) + MEM_ALIGNMENT - 1) & ~(MEM_ALIGNMENT-1))

#define MAKE_FOURCC(a, b, c, d) ((uint32_t)(d)<<24 | (uint32_t)(c)<<16 | (uint32_t)(b)<<8 | (uint32_t)(a))

typedef struct {
    const char *fname;
    framesize_t rec_size;
    uint32_t rec_time;
} recorder_param_t;

typedef struct {
    char filename[64];  // filename for temporary
    int avifile;      // avi file
    int idxfile;      // storage the size of each image
    uint32_t nframes;   // the number of frame
    uint32_t totalsize; // all frame image size

    //buffer for preformence
    uint8_t *buffer;
    uint32_t buf_len;
    uint32_t write_len;
} jpeg2avi_data_t;

typedef enum {
    REC_STATE_IDLE,
    REC_STATE_BUSY,
} record_state_t;

static uint8_t g_force_end = 0;
static record_state_t g_state = REC_STATE_IDLE;

static int jpeg2avi_start(jpeg2avi_data_t *j2a, const char *filename)
{
    ESP_LOGI(TAG, "Starting an avi [%s]", filename);
    if (strlen(filename) > sizeof(j2a->filename) - 5) {
        ESP_LOGE(TAG, "The given file name is too long");
        return ESP_ERR_INVALID_ARG;
    }

    j2a->buf_len = 8 * 1024;
    j2a->buffer = heap_caps_malloc(j2a->buf_len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (NULL == j2a->buffer) {
        ESP_LOGE(TAG, "recorder mem failed");
        return ESP_ERR_NO_MEM;
    }
    j2a->write_len = 0;

    memset(j2a->filename, 0, sizeof(j2a->filename));
    strcpy(j2a->filename, filename);

    j2a->avifile = open(j2a->filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    if (j2a->avifile == -1)  {
        ESP_LOGE(TAG, "Could not open %s (%s)", j2a->filename, strerror(errno));
        return ESP_FAIL;
    }

    strcat(j2a->filename, ".idx");
    j2a->idxfile = open(j2a->filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    if (j2a->idxfile == -1)  {
        ESP_LOGE(TAG, "Could not open %s (%s)", j2a->filename, strerror(errno));
        close(j2a->avifile);
        unlink(filename);
        return ESP_FAIL;
    }

    uint32_t offset1 = sizeof(AVI_LIST_HEAD);  //riff head大小
    uint32_t offset2 = sizeof(AVI_HDRL_LIST);  //hdrl list大小
    uint32_t offset3 = sizeof(AVI_LIST_HEAD);  //movi list head大小

    //AVI文件偏移量设置到movi list head后，从该位置向后依次写入JPEG数据
    int ret = lseek(j2a->avifile, offset1 + offset2 + offset3, SEEK_SET);
    if (-1 == ret) {
        ESP_LOGE(TAG, "seek avi file failed");
        close(j2a->avifile);
        close(j2a->idxfile);
        unlink(filename);
        unlink(j2a->filename);
        return ESP_FAIL;
    }

    j2a->nframes = 0;
    j2a->totalsize = 0;
    return ESP_OK;
}

static int jpeg2avi_add_frame(jpeg2avi_data_t *j2a, uint8_t *data, uint32_t len)
{
    int ret;
    AVI_CHUNK_HEAD frame_head;
    uint32_t align_size = MEM_ALIGN_SIZE(len);/*JPEG图像大小4字节对齐*/
    const int CHUNK_SIZE = 4096;

    frame_head.FourCC = MAKE_FOURCC('0', '0', 'd', 'c'); //00dc = 压缩的视频数据
    frame_head.size = align_size;
    uint64_t t_s = esp_timer_get_time();

    uint32_t remain = j2a->write_len + align_size + sizeof(AVI_CHUNK_HEAD);
    uint32_t last_remain = j2a->write_len;
    while (remain) {
        // int wl = remain >= CHUNK_SIZE ? CHUNK_SIZE : remain;
        if (remain >= CHUNK_SIZE) {
            memcpy(&j2a->buffer[j2a->write_len], &frame_head, sizeof(AVI_CHUNK_HEAD));
            j2a->write_len += sizeof(AVI_CHUNK_HEAD);
            int _len = CHUNK_SIZE - last_remain - sizeof(AVI_CHUNK_HEAD);
            memcpy(&j2a->buffer[j2a->write_len], data, _len);
            j2a->write_len += _len;
            data += _len;
            ret = write(j2a->avifile, j2a->buffer, j2a->write_len);
            printf("l=%d\n", j2a->write_len);
            remain -= j2a->write_len;
            j2a->write_len = 0;
            if (remain >= CHUNK_SIZE) {
                // 大小对齐后的整块写入
                int count = remain / CHUNK_SIZE;
                for (size_t i = 0; i < count; i++) {
                    ret = write(j2a->avifile, data, CHUNK_SIZE);
                    printf("lc=%d\n", CHUNK_SIZE);
                    data += CHUNK_SIZE;
                    remain -= CHUNK_SIZE;
                }
            }
            memcpy(&j2a->buffer[j2a->write_len], data, remain);
            j2a->write_len += remain;
            remain = 0;
        } else {
            memcpy(&j2a->buffer[j2a->write_len], &frame_head, sizeof(AVI_CHUNK_HEAD));
            j2a->write_len += sizeof(AVI_CHUNK_HEAD);
            memcpy(&j2a->buffer[j2a->write_len], data, remain - sizeof(AVI_CHUNK_HEAD));
            j2a->write_len += remain - sizeof(AVI_CHUNK_HEAD);
            remain = 0;
        }
    }
    /*将4字节对齐后的JPEG图像大小保存*/
    write(j2a->idxfile, &align_size, 4);
    uint64_t t_e = esp_timer_get_time();
    printf("ts=%d, t=%d\n", align_size, (uint32_t)(t_e - t_s));

    j2a->nframes += 1;
    j2a->totalsize += align_size;
    return ESP_OK;
}

static int back_fill_data(jpeg2avi_data_t *j2a, uint32_t width, uint32_t height, uint32_t fps)
{
    size_t ret;

    AVI_LIST_HEAD riff_head = {
        .List = MAKE_FOURCC('R', 'I', 'F', 'F'),
        .size = 4 + sizeof(AVI_HDRL_LIST) + sizeof(AVI_LIST_HEAD) + j2a->nframes * 8 + j2a->totalsize + (sizeof(AVI_IDX1) * j2a->nframes) + 8,
        .FourCC = MAKE_FOURCC('A', 'V', 'I', ' ')
    };

    AVI_HDRL_LIST hdrl_list = {
        {
            .List = MAKE_FOURCC('L', 'I', 'S', 'T'),
            .size = sizeof(AVI_HDRL_LIST) - 8,
            .FourCC = MAKE_FOURCC('h', 'd', 'r', 'l'),
        },
        {
            .FourCC = MAKE_FOURCC('a', 'v', 'i', 'h'),
            .size = sizeof(AVI_AVIH_CHUNK) - 8,
            .us_per_frame = 1000000 / fps,
            .max_bytes_per_sec = (width *height * 2) / 10,
            .padding = 0,
            .flags = 0,
            .total_frames = j2a->nframes,
            .init_frames = 0,
            .streams = 1,
            .suggest_buff_size = (width *height * 2),
            .width = width,
            .height = height,
            .reserved = {0, 0, 0, 0},
        },
        {
            {
                .List = MAKE_FOURCC('L', 'I', 'S', 'T'),
                .size = sizeof(AVI_STRL_LIST) - 8,
                .FourCC = MAKE_FOURCC('s', 't', 'r', 'l'),
            },
            {
                .FourCC = MAKE_FOURCC('s', 't', 'r', 'h'),
                .size = sizeof(AVI_STRH_CHUNK) - 8,
                .fourcc_type = MAKE_FOURCC('v', 'i', 'd', 's'),
                .fourcc_codec = MAKE_FOURCC('M', 'J', 'P', 'G'),
                .flags = 0,
                .priority = 0,
                .language = 0,
                .init_frames = 0,
                .scale = 1,
                .rate = fps, //rate / scale = fps
                .start = 0,
                .length = j2a->nframes,
                .suggest_buff_size = (width *height * 2),
                .quality = 1,
                .sample_size = 0,
                .rcFrame = {0, 0, width, height},
            },
            {
                .FourCC = MAKE_FOURCC('s', 't', 'r', 'f'),
                .size = sizeof(AVI_VIDS_STRF_CHUNK) - 8,
                .size1 = sizeof(AVI_VIDS_STRF_CHUNK) - 8,
                .width = width,
                .height = height,
                .planes = 1,
                .bitcount = 24,
                .fourcc_compression = MAKE_FOURCC('M', 'J', 'P', 'G'),
                .image_size = width * height * 3,
                .x_pixels_per_meter = 0,
                .y_pixels_per_meter = 0,
                .num_colors = 0,
                .imp_colors = 0,
            }
        }
    };

    AVI_LIST_HEAD movi_list_head = {
        .List = MAKE_FOURCC('L', 'I', 'S', 'T'),
        .size = 4 + j2a->nframes * 8 + j2a->totalsize,
        .FourCC = MAKE_FOURCC('m', 'o', 'v', 'i')
    };

    //定位到文件头，回填各块数据
    lseek(j2a->avifile, 0, SEEK_SET);
    write(j2a->avifile, &riff_head, sizeof(AVI_LIST_HEAD));
    write(j2a->avifile, &hdrl_list, sizeof(AVI_HDRL_LIST));
    ret = write(j2a->avifile, &movi_list_head, sizeof(AVI_LIST_HEAD));
    // if (sizeof(AVI_LIST_HEAD) != ret) {
    //     ESP_LOGE(TAG, "avi head list write failed");
    //     return ESP_FAIL;
    // }
    return ESP_OK;
}

static int write_index_chunk(jpeg2avi_data_t *j2a)
{
    size_t ret;
    size_t i;
    uint32_t index = MAKE_FOURCC('i', 'd', 'x', '1');  //索引块ID
    uint32_t index_chunk_size = sizeof(AVI_IDX1) * j2a->nframes;   //索引块大小
    uint32_t offset = 4;
    uint32_t frame_size;
    AVI_IDX1 idx;

    j2a->idxfile = open(j2a->filename, O_RDWR);
    if (j2a->idxfile == -1)  {
        ESP_LOGE(TAG, "%d:Could not open %s. discard the idx1 chunk", __LINE__, j2a->filename);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "frame number=%d", j2a->nframes);
    write(j2a->avifile, &index, 4);
    write(j2a->avifile, &index_chunk_size, 4);

    idx.FourCC = MAKE_FOURCC('0', '0', 'd', 'c'); //00dc = 压缩的视频数据
    for (i = 0; i < j2a->nframes; i++) {
        read(j2a->idxfile, &frame_size, 4); //Read size of each jpeg image
        idx.flags = 0x10;//0x10表示当前帧为关键帧
        idx.chunkoffset = offset;
        idx.chunklength = frame_size;
        ret = write(j2a->avifile, &idx, sizeof(AVI_IDX1));

        offset = offset + frame_size + 8;
    }
    close(j2a->idxfile);
    unlink(j2a->filename);
    if (i != j2a->nframes) {
        ESP_LOGE(TAG, "avi index write failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void jpeg2avi_end(jpeg2avi_data_t *j2a, int width, int height, int fps)
{
    ESP_LOGI(TAG, "video info: width=%d | height=%d | fps=%d", width, height, fps);
    close(j2a->idxfile);
    if (j2a->write_len) { // 如果缓存区有数据，则全写到文件
        int ret = write(j2a->avifile, j2a->buffer, j2a->write_len);
    }

    //写索引块
    write_index_chunk(j2a);

    //从文件头开始，回填各块数据
    back_fill_data(j2a, width, height, fps);
    close(j2a->avifile);

    free(j2a->buffer);

    ESP_LOGI(TAG, "avi recording completed");
}

static void recorder_task(void *args)
{
    int ret;
    recorder_param_t *rec_arg = (recorder_param_t *)args;

    jpeg2avi_data_t avi_recoder;
    camera_fb_t *image_fb = NULL;
    ret = jpeg2avi_start(&avi_recoder, "/sdcard/recorde.avi");
    if (0 != ret) {
        ESP_LOGE(TAG, "start failed");
        vTaskDelete(NULL);
    }

    g_state = REC_STATE_BUSY;
    sensor_t *cam_sensor = esp_camera_sensor_get();
    cam_sensor->set_framesize(cam_sensor, rec_arg->rec_size);

    uint64_t fr_start = esp_timer_get_time() / 1000;
    uint64_t end_time = rec_arg->rec_time * 1000 + fr_start;
    uint64_t printf_time = fr_start;
    while (1) {
        image_fb = esp_camera_fb_get();
        if (!image_fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        ret = jpeg2avi_add_frame(&avi_recoder, image_fb->buf, image_fb->len);
        esp_camera_fb_return(image_fb);
        if (0 != ret) {
            break;
        }

        uint64_t t = esp_timer_get_time() / 1000;
        if (t - printf_time > 1000) {
            printf_time = t;
            ESP_LOGI(TAG, "recording %d/%d s", (uint32_t)((t - fr_start) / 1000), rec_arg->rec_time);
        }
        if (t > end_time || g_force_end) {
            break;
        }
    }
    uint32_t fps = avi_recoder.nframes * 1000 / (esp_timer_get_time() / 1000 - fr_start);
    jpeg2avi_end(&avi_recoder, resolution[rec_arg->rec_size].width, resolution[rec_arg->rec_size].height, fps);
    g_state = REC_STATE_IDLE;
    g_force_end = 0;
    vTaskDelete(NULL);
}

void avi_recorder_start(const char *fname, framesize_t rec_size, uint32_t rec_time)
{
    if (REC_STATE_IDLE != g_state) {
        ESP_LOGE(TAG, "recorder already running");
        return;
    }
    g_force_end = 0;
    static recorder_param_t rec_arg = {0};
    rec_arg.fname = fname;
    rec_arg.rec_size = rec_size;
    rec_arg.rec_time = rec_time;

    xTaskCreatePinnedToCore(recorder_task, "recorder", 1024 * 4, &rec_arg, configMAX_PRIORITIES - 2, NULL, 1);

    g_state = REC_STATE_BUSY;
}

void avi_recorder_stop(void)
{
    if (REC_STATE_BUSY != g_state) {
        ESP_LOGE(TAG, "recorder already running");
        return;
    }
    g_force_end = 1;
}