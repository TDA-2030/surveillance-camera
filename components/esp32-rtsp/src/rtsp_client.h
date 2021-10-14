
#pragma once

#include <sys/queue.h>
#include <pthread.h>
#include "media_stream.h"
#include "rtsp_utility.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    SOCKET socket;                               // socket for connect server
    IPPORT server_rtp_port;                      // client port for UDP based RTP transport
    IPPORT server_rtcp_port;                     // client port for UDP based RTCP transport
    IPPORT client_rtp_port;
    IPPORT client_rtcp_port;
    transport_mode_t transport_mode;
    uint16_t rtp_channel;                        // only used for rtp over tcp
    uint16_t rtcp_channel;                       // only used for rtp over tcp

    uint8_t RecvBuf[RTSP_BUFFER_SIZE];
    char session_id[32];
    rtsp_method_t method;                        // method of the current request
    uint32_t response_code;
    uint32_t server_method_mask;
    uint32_t CSeq;                               // RTSP command sequence number
    char url[RTSP_PARAM_STRING_MAX];             // stream url
    uint16_t url_port;                           // port in url
    char url_ip[20];
    parse_state_t parse_state;
    uint8_t state;

    pthread_t thread;

    SLIST_HEAD(c_media_streams_list_t, media_streams_t) media_list;
    uint8_t media_stream_num;
} rtsp_client_t;


rtsp_client_t *rtsp_client_create(void);
int rtsp_client_delete(rtsp_client_t *session);
int rtsp_client_add_media_stream(rtsp_client_t *session, media_stream_t *media);
int rtsp_client_push_media(rtsp_client_t *session, const char *url, transport_mode_t transport_mode);

#ifdef __cplusplus
}
#endif
