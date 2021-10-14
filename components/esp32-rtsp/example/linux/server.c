#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "v4l2uvc.h"
#include "yuv2jpg.h"

#include "rtsp_client.h"
#include "rtsp_server.h"
#include "media_mjpeg.h"
#include "media_g711a.h"
#include "media_l16.h"
#include "g711.h"

static const char *TAG = "linux";

#define USE_AUDIO 0
#define PUSHER 1

#define VIDEO_DEVICE "/dev/video0"

#if USE_AUDIO
char *wave_get(void);
uint32_t wave_get_size(void);
uint32_t wave_get_framerate(void);
uint32_t wave_get_bits(void);
uint32_t wave_get_ch(void);

static uint8_t *audio_p;
static uint8_t *audio_end;
static int64_t audio_last_frame = 0;
#endif

struct video_info *vd_info = NULL;

media_stream_t *mjpeg;
media_stream_t *pcma;
media_stream_t *l16;

static void read_img(const char *file, uint8_t *data, uint32_t *size)
{
    *size = 0;
    int fd = open(file, O_RDONLY);

    if (fd == -1) {
        printf("[%s]error is %s\n", file, strerror(errno));
        return;
    }

    while (1) {
        ssize_t r = read(fd, data, 8192);
        if (r > 0) {
            *size += r;
        } else {
            break;
        }
    }
    close(fd);
}

static int get_folder_recording_cnt(char *root, uint32_t *cnt)
{
    DIR *dir;
    struct dirent *ptr;
    uint32_t total = 0;
    char path[128];

    dir = opendir(root); /* 打开bai目录*/
    if (NULL == dir) {
        printf("fail to open dir\n");
    }
    errno = 0;
    while (NULL != (ptr = readdir(dir))) {
        //顺序读取每一个目录项；
        //跳过“duzhi..”和“.”两个目录
        if (0 == strcmp(ptr->d_name, ".") || 0 == strcmp(ptr->d_name, "..") ) {
            continue;
        }

        if (ptr->d_type == DT_DIR) {
            sprintf(path, "%s%s/", root, ptr->d_name);

        }
        if (ptr->d_type == DT_REG) {
            total++;
        }
    }
    if (0 != errno) {
        printf("fail to read dir\n"); //失败则输出提示信息
    }
    closedir(dir);

    *cnt = total;
    printf("total file num=%d\n", total);
    return 0;
}

static void streamImage(media_stream_t *mjpeg_stream)
{
    static uint8_t img_buf[1024 * 300];
    static uint32_t index = 1;
    static int64_t last_frame = 0;
    int64_t interval = (rtp_time_now_us() - last_frame) / 1000;
    {
        // printf("frame fps=%f\n", 1000.0f / (float)interval);
        uint8_t *p = img_buf;
        uint32_t len = 0;
        // char name[64] = {0};
        // sprintf(name, "../../simple/media/video/frames/hd_%03d.jpg", index);
        // read_img(name, p, &len);

        if (vd_info->is_quit) {
            v4l2_close(vd_info);
            pthread_exit(NULL);
        }
        if (v4l2_grab(vd_info) < 0) {
            ESP_LOGE(TAG, "Error grabbing ");
            return;
        }

        p = vd_info->frame_buffer;
        len = vd_info->frame_size;
        mjpeg_stream->handle_frame(mjpeg_stream, p, len);
        // index++;
        // if (index > pic_num) {
        //     index = 1;
        //     ESP_LOGW(TAG, "video over");
        //     pthread_exit(NULL);
        // }


        last_frame = rtp_time_now_us();
    }
}

#if USE_AUDIO
static void streamaudio(media_stream_t *audio_stream)
{
    static uint8_t buffer[8192];
    int64_t interval = (rtp_time_now_us() - audio_last_frame) / 1000;
    if (audio_last_frame == 0) {
        audio_last_frame = rtp_time_now_us();
        audio_p = (uint8_t *)wave_get();
        return;
    }
    if (vd_info->is_quit) {
        pthread_exit(NULL);
    }
    {
        uint32_t len = 0;
        if (RTP_PAYLOAD_PCMA == audio_stream->rtp_profile.pt) {
            len = interval * 32;
            if (len > sizeof(buffer)) {
                len = sizeof(buffer);
            }
            int16_t *pcm = (int16_t *)audio_p;
            if (audio_p + len >= audio_end) {
                len = audio_end - audio_p;
                audio_p = (uint8_t *)wave_get();
            }
            for (size_t i = 0; i < len; i++) {
                buffer[i] = linear2alaw(pcm[i]);
            }
            audio_stream->handle_frame(audio_stream, buffer, len / 2);
            audio_p += len;
        } else  if (RTP_PAYLOAD_L16_CH1 == audio_stream->rtp_profile.pt) {
            len = interval * 32;
            if (len > sizeof(buffer)) {
                len = sizeof(buffer);
            }
            int16_t *pcm = (int16_t *)audio_p;
            if (audio_p + len >= audio_end) {
                len = audio_end - audio_p;
                audio_p = (uint8_t *)wave_get();
                ESP_LOGW(TAG, "audeo over");
                pthread_exit(NULL);
            }
            for (size_t i = 0; i < len / 2; i++) {
                buffer[i * 2] = pcm[i] >> 8;
                buffer[i * 2 + 1] = pcm[i] & 0xff;
            }
            audio_stream->handle_frame(audio_stream, buffer, len);
            audio_p += len;
        }
        printf("audio fps=%f\n", 1000.0f / (float)interval);

        audio_last_frame = rtp_time_now_us();
    }
}
#endif

static void get_local_ip(char *ipaddr)
{
    int sock_get_ip;
    struct   sockaddr_in *sin;
    struct   ifreq ifr_ip;
    if ((sock_get_ip = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        ESP_LOGE(TAG, "socket create failse...GetLocalIp!");
        return;
    }

    memset(&ifr_ip, 0, sizeof(ifr_ip));
    strncpy(ifr_ip.ifr_name, "enp1s0", sizeof(ifr_ip.ifr_name) - 1);

    if ( ioctl( sock_get_ip, SIOCGIFADDR, &ifr_ip) < 0 ) {
        memset(&ifr_ip, 0, sizeof(ifr_ip));
        strncpy(ifr_ip.ifr_name, "wlan0", sizeof(ifr_ip.ifr_name) - 1);
        if ( ioctl( sock_get_ip, SIOCGIFADDR, &ifr_ip) < 0 ) {
            ESP_LOGE(TAG, "socket ioctl failse");
            close( sock_get_ip );
            return;
        }
    }
    sin = (struct sockaddr_in *)&ifr_ip.ifr_addr;
    strcpy(ipaddr, inet_ntoa(sin->sin_addr));
    close( sock_get_ip );
}

esp_err_t video_init()
{
    vd_info = (struct video_info *) calloc(1, sizeof(struct video_info));

    /* init the camera,you can change the last three params!!! */
    if (v4l2_init(VIDEO_DEVICE, vd_info, V4L2_PIX_FMT_MJPEG, 480, 320) < 0) {
        return EXIT_FAILURE;
    }

    if (v4l2_get_capability(vd_info) < 0) {
        exit(1);
    }
    if (v4l2_get_format(vd_info) < 0) {
        exit(1);
    }
}

static void sig_int(int signum)
{
    debug_msg("\ncatch a SIGINT signal, you may be press Ctrl+C.\n");
    vd_info->is_quit = 1;
    debug_msg("ready to quit\n");
    kill(0, SIGKILL);

}

#if USE_AUDIO
static void *send_audio(void *args)
{
#if PUSHER
    rtsp_client_t *session = (rtsp_client_t *)args;
#else
    rtsp_server_t *session = (rtsp_server_t *)args;
#endif
    while (1) {
        if (session->state & 0x02) {
            streamaudio(l16);
            usleep(20000);
        }
    }
}
#endif

static void *send_video(void *args)
{
#if PUSHER
    rtsp_client_t *session = (rtsp_client_t *)args;
#else
    rtsp_server_t *session = (rtsp_server_t *)args;
#endif
    while (!(session->state & 0x02)) {
        usleep(5000);
    }

    while (1) {
        streamImage(mjpeg);
        usleep(70000);
        if (!(session->state & 0x02)) {
            ESP_LOGW(TAG, "Delete video task");
            pthread_exit(NULL);
        }
    }
}

static void *rtsp_print_task(void *args)
{
#if PUSHER
    rtsp_client_t *rtsp = (rtsp_client_t *)args;
#else
    rtsp_server_t *rtsp = (rtsp_server_t *)args;
#endif
    while (1) {
        sleep(2);
        rtp_statistics_info_t info;
        media_streams_t *it;
        SLIST_FOREACH(it, &rtsp->media_list, next) {
            if (it->media_stream->rtp_session) {
                rtp_get_statistics(it->media_stream->rtp_session, &info);
                ESP_LOGI(TAG, "speed=%.3f KB/s, total size=%.3f MB, elapse=%d s", info.speed_kbs, (float)(info.total_kb) / 1024, info.elapsed_sec);
            }
        }
    }
    return NULL;
}

static int create_media_task(void *args)
{
#if USE_AUDIO
    audio_p = (uint8_t *)wave_get();
    audio_end = (uint8_t *)wave_get() + wave_get_size();
    audio_last_frame = 0;
    pthread_t new_thread = (pthread_t)NULL;
    pthread_create(&new_thread, NULL, send_audio, (void *) rtsp);
#endif
    pthread_t _thread = (pthread_t)NULL;
    pthread_create(&_thread, NULL, send_video, (void *) args);
    return 0;
}

#if PUSHER
static void pusher_video_run()
{
    rtsp_client_t *rtsp = rtsp_client_create();
    mjpeg = media_stream_mjpeg_create();
    pcma = media_stream_g711a_create(16000);
    l16 = media_stream_l16_create(16000);
    rtsp_client_add_media_stream(rtsp, mjpeg);
#if USE_AUDIO
    audio_p = (uint8_t *)wave_get();
    audio_end = (uint8_t *)wave_get() + wave_get_size();
    audio_last_frame = 0;
    rtsp_server_add_media_stream(rtsp, l16);
#endif
    #define IPADDR "192.168.202.51"
    int ret = rtsp_client_push_media(rtsp, "rtsp://" IPADDR ":8554/linux_rtsp", RTP_OVER_TCP);
    if (0 != ret) {
        ESP_LOGE(TAG, "push error");
        return;
    }

    create_media_task(rtsp);
    pthread_t _thread = (pthread_t)NULL;
    pthread_create(&_thread, NULL, rtsp_print_task, (void *) rtsp);
}

#else
static void server_run()
{
    rtsp_server_cfg_t rtsp_cfg = {
        .url = "mjpeg/1",
        .port = 8554,
        .accept_cb_fn = create_media_task,
        .session_name = "esp32-ipc",
    };
    rtsp_server_t *rtsp = rtsp_server_create(&rtsp_cfg);
    mjpeg = media_stream_mjpeg_create();
    pcma = media_stream_g711a_create(16000);
    l16 = media_stream_l16_create(16000);
    rtsp_server_add_media_stream(rtsp, mjpeg);
#if USE_AUDIO
    rtsp_server_add_media_stream(rtsp, l16);
#endif
    pthread_t print_thread = (pthread_t)NULL;
    pthread_create(&print_thread, NULL, rtsp_print_task, (void *) rtsp);
}
#endif

int main(void)
{
    // get_folder_recording_cnt("../../simple/media/video/frames", &pic_num);
    char ip_str[64] = {0};
    get_local_ip(ip_str);
    video_init();
    signal(SIGINT, sig_int);

    // int i=0;
    // while (!vd_info->is_quit) {
    //     if (v4l2_grab(vd_info) < 0) {
    //         printf("Error grabbing \n");
    //         break;
    //     }
    //     yuv422_to_jpg(vd_info->frame_buffer, 480, 320);
    //     // char fname[64] = {0};
    //     // sprintf(fname, "./hd_%3d.jpg", i++);
    //     // FILE *f = fopen(fname, "wb");
    //     // if (f) {
    //     //     msg_out("len=%d\n", vd_info->frame_size_in);

    //     //     fwrite(vd_info->tmp_buffer, vd_info->frame_size_in, 1, f);
    //     //     fclose(f);
    //     // }
    //     sleep(1);
    // }

    ESP_LOGI(TAG, "Creating RTSP session [rtsp://%s:%hu/%s]", ip_str, 8554, "mjpeg/1");

#if PUSHER
    pusher_video_run();
#else
    server_run();
#endif
    while (1) {
        sleep(1);
    }
    return 0;
}

