
#ifndef __V4L2UVC_H__
#define __V4L2UVC_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <stdint.h>


// #define DEBUG /* for debug */

/**
 * @def error_exit
 * @brief A macro that prints the @a error msg and exit.
 */

#define error_exit(error)   \
    do{                                         \
        fprintf(stderr, "%s\n", error);         \
        exit(0);                                \
    } while(0)

/**
 * @def error_ret
 * @brief A macro that prints the @a error msg and return -1.
 */
#define error_ret(error)                          \
    do{                                           \
        fprintf(stderr, "%s\n", error);           \
        return -1;                                \
    } while(0)

/**
 * @def unix_error_exit
 * @brief A macro that prints the @a error msg(with errno) and then exit.
 *        I put 'unix' before the 'function' name because I am using 'errno'.
 */
#define unix_error_exit(error)                  \
    do{                                         \
        fprintf(stderr, "%s Info[%d]:%s\n",     \
                error, errno, strerror(errno)); \
        exit(1);                                \
    } while(0)

/**
 * @def unix_error_ret
 * @brief A macro that prints the @a error msg(with errno) and then return -1.
 *        I put 'unix' before the 'function' name because I am using 'errno'.
 */
#define unix_error_ret(error)                   \
    do{                                         \
        fprintf(stderr, "%s Info[%d]:%s\n",     \
                error, errno, strerror(errno)); \
        return -1;                              \
    } while(0)

#ifdef DEBUG
#define debug_msg(fmt, ...) \
    fprintf(stdout, fmt, ##__VA_ARGS__)
#else
#define debug_msg(fmt,...)
#endif   /* DEBUG */

#define msg_out(format,...) \
    fprintf(stdout,format,##__VA_ARGS__)

/** buffer number */
#define NB_BUFFER 4


struct video_info {
    int camfd;                  /**< 摄像头文件描述符，由open系统调用指定 */
    struct v4l2_capability cap; /**< 摄像头capability(属性) */
    struct v4l2_format fmt; /**<  摄像头格式，使用该结构体对摄像头进行设置 */
    struct v4l2_requestbuffers rb;  /**< 请求缓冲，一般不超过5个 */
    struct v4l2_buffer buf; /**< buffer */
    enum v4l2_buf_type type;    /**< 控制命令字？ */
    void *mem[NB_BUFFER];       /**< main buffers */
    uint8_t *tmp_buffer;          /**< 临时缓冲区，针对MJPEG格式而设 */
    uint8_t *frame_buffer;        /**< 一帧图像缓冲区 */
    uint32_t frame_size;
    uint32_t frame_size_in; /**< 一帧图像大小(=宽x高x2) */

    uint32_t format;              /**< 摄像头支持的格式，如MJPEG、YUYV等 */
    int width;          /**< 图像宽 */
    int height;         /**< 图像高 */
    int is_streaming;   /**< 开始采集 */
    int is_quit;        /**< 退出显示 */

    enum v4l2_field field;
    uint32_t bytes_per_line;
    uint32_t size_image;
    enum v4l2_colorspace color_space;
    uint32_t priv;
};


int v4l2_init(const char *video_device, struct video_info *vd_info, uint32_t format,
              uint32_t width, uint32_t height);
int v4l2_on(struct video_info *vd_info);
int v4l2_off(struct video_info *vd_info);
int v4l2_close(struct video_info *vd_info);

int v4l2_grab(struct video_info *vd_info);

int v4l2_get_capability(struct video_info *vd_info);
int v4l2_get_format(struct video_info *vd_info);
int v4l2_set_foramt(struct video_info *vd_info,
                    uint32_t width, uint32_t height, uint32_t format);

#endif  /* __V4L2UVC_H__ */
