#pragma once

#include <sys/queue.h>
#include <pthread.h>
#include "media_stream.h"
#include "rtsp_utility.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *url;
    uint16_t port;
    const char *session_name;
    cb_fun_t accept_cb_fn;
    cb_fun_t close_cb_fn;

} rtsp_server_cfg_t;

typedef struct {
    SOCKET listen_socket;                             // our masterSocket(socket that listens for RTSP client connections)
    SOCKET client_socket;                             // RTSP socket of that session
    transport_mode_t transport_mode;
    uint16_t rtp_channel;                             // only used for rtp over tcp
    uint16_t rtcp_channel;                            // only used for rtp over tcp
    IPPORT client_rtp_port;                           // client port for UDP based RTP transport
    IPPORT client_rtcp_port;                          // client port for UDP based RTCP transport

    rtsp_server_cfg_t cfg;

    uint8_t recvbuf[RTSP_BUFFER_SIZE];
    rtsp_method_t method;                             // method of the current request
    uint32_t cseq;                                    // RTSP command sequence number
    char session_id[32];
    char url[RTSP_PARAM_STRING_MAX];                  // stream url
    uint16_t url_port;                                // port in url
    char url_ip[20];
    char url_suffix[RTSP_PARAM_STRING_MAX];

    parse_state_t parse_state;
    size_t recv_offset;
    size_t rtcp_length;
    uint8_t state;

    pthread_t thread;

    SLIST_HEAD(s_media_streams_list_t, media_streams_t) media_list;
    uint8_t media_stream_num;
} rtsp_server_t;


rtsp_server_t *rtsp_server_create(rtsp_server_cfg_t *cfg);

int rtsp_server_delete(rtsp_server_t *session);

int rtsp_server_add_media_stream(rtsp_server_t *session, media_stream_t *media);


#ifdef __cplusplus
}
#endif
