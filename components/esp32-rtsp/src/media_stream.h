

#ifndef _MEDIA_STREAM_H_
#define _MEDIA_STREAM_H_

#include <sys/queue.h>
#include "rtp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTSP_BUFFER_SIZE       2048    // for incoming requests, and outgoing responses
#define RTSP_PARAM_STRING_MAX  128

typedef struct media_stream_t {
    rtp_profile_t rtp_profile;
    uint8_t *rtp_buffer;
    uint32_t sample_rate;
    rtp_session_t *rtp_session;
    void (*delete_media)(struct media_stream_t *stream);
    void (*get_description)(struct media_stream_t *stream, char *buf, uint32_t buf_len, uint16_t port);
    void (*get_attribute)(struct media_stream_t *stream, char *buf, uint32_t buf_len);
    int (*handle_frame)(struct media_stream_t *stream, const uint8_t *data, uint32_t len);
} media_stream_t;

typedef struct media_streams_t {
    media_stream_t *media_stream;
    uint32_t trackid;
    /* Next endpoint entry in the singly linked list */
    SLIST_ENTRY(media_streams_t) next;
} media_streams_t;


#ifdef __cplusplus
}
#endif

#endif
