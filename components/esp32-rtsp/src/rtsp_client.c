
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "rtsp_utility.h"
#include "rtp.h"
#include "rtsp_client.h"

static const char *TAG = "rtsp_client";


#define RTSP_CHECK(a, str, ret_val)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

#define RTSP_CHECK_GOTO(a, str, label)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        goto label;                                         \
    }

static void *rtsp_handle_task(void *args);

static int parse_response_line(rtsp_client_t *session, const char *message)
{
    char version[16] = {0};
    char str[32] = {0};

    if (sscanf(message, "%s %u %s", version, &session->response_code, str) != 3) {
        return 1;
        session->response_code = 500;
    }
    if (NULL == strstr(version, rtsp_get_version())) {
        session->response_code = 500;
        ESP_LOGE(TAG, "Does not support RTSP/V2.0 yet");
    }

    return 0;
}

static int parse_headers_line(rtsp_client_t *session, const char *message)
{
    ESP_LOGD(TAG, "<%s>", message);
    char *TmpPtr = NULL;
    TmpPtr = (char *)strstr(message, "CSeq: ");
    if (TmpPtr) {
        uint32_t cseq = atoi(TmpPtr + 6);
        if (session->CSeq != cseq) {
            ESP_LOGE(TAG, "CSeq mismatch");
            return -1;
        }
        return 0;
    }

    TmpPtr = (char *)strstr(message, "Session: ");
    if (TmpPtr) {
        strcpy(session->session_id, TmpPtr + 9);
        return 0;
    }

    if (session->method == RTSP_DESCRIBE || session->method == RTSP_SETUP || session->method == RTSP_PLAY) {
        // ParseAuthorization(message);
    }

    if (session->method == RTSP_OPTIONS) {
        TmpPtr = (char *)strstr(message, "Public: ");
        if (TmpPtr) {
            session->server_method_mask = 0;
            for (size_t i = 0; i < RTSP_UNKNOWN; i++) {
                if (strstr(TmpPtr, rtsp_methods[i].str)) {
                    session->server_method_mask |= 1 << i;
                }
            }
            session->parse_state = PARSE_STATE_GOTALL;
            return 0;
        }
    }

    if (session->method == RTSP_DESCRIBE) {
        // session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    if (session->method == RTSP_SETUP) {
        TmpPtr = (char *)strstr(message, "Transport: ");
        if (TmpPtr) {
            TmpPtr = (char *)strstr(TmpPtr, "RTP/AVP/TCP");
            if (TmpPtr) {
                session->transport_mode = RTP_OVER_TCP;
            } else {
                session->transport_mode = RTP_OVER_UDP;
            }

            TmpPtr = (char *)strstr(message, "multicast");
            if (TmpPtr) {
                session->transport_mode = RTP_OVER_MULTICAST;
                ESP_LOGD(TAG, "multicast");
            } else {
                ESP_LOGD(TAG, "unicast");
            }

            char *server_port_ptr = NULL;
            if (RTP_OVER_UDP == session->transport_mode) {
                server_port_ptr = (char *)strstr(message, "server_port=");
            } else if (RTP_OVER_MULTICAST == session->transport_mode) {
                server_port_ptr = (char *)strstr(message, "port=");
            }
            if (server_port_ptr) {
                server_port_ptr += (RTP_OVER_UDP == session->transport_mode) ? 12 : 5;
                char cp[16] = {0};
                char *p = strchr(server_port_ptr, '-');
                if (p) {
                    strncpy(cp, server_port_ptr, p - server_port_ptr);
                    session->server_rtp_port  = atoi(cp);
                    p += 1;
                    memset(cp, 0, sizeof(cp));
                    for (size_t i = 0; i < 6; i++) {
                        if (*p >= '0' && *p <= '9') {
                            cp[i] = *p;
                        } else {
                            break;
                        }
                        p++;
                    }
                    session->server_rtcp_port = atoi(cp);

                    ESP_LOGI(TAG, "rtsp server port %d-%d", session->server_rtp_port, session->server_rtcp_port);
                } else {
                    ESP_LOGE(TAG, "can't parse server port");
                    return 1;
                }
            }

            if (RTP_OVER_TCP == session->transport_mode) {
                TmpPtr = (char *)strstr(message, "interleaved=");
                if (TmpPtr) {
                    if (sscanf(TmpPtr += 12, "%hu-%hu", &session->rtp_channel, &session->rtcp_channel) == 2) {
                        ESP_LOGI(TAG, "RTP channel=%d, RTCP channel=%d", session->rtp_channel, session->rtcp_channel);
                    }
                }
            }

            session->parse_state = PARSE_STATE_GOTALL;
        }
        return 0;
    }

    if (session->method == RTSP_PLAY) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    if (session->method == RTSP_RECORD) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    if (session->method == RTSP_ANNOUNCE) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    if (session->method == RTSP_TEARDOWN) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    if (session->method == RTSP_GET_PARAMETER) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    return 1;
}

static int parse_rtsp_request(rtsp_client_t *session, const char *request, uint32_t length)
{
    printf("[%s]\n", request);
    if (request[0] == '$') {
        session->method = RTSP_UNKNOWN;
        return 0;
    }
    session->parse_state = PARSE_STATE_REQUESTLINE;

    int ret = 0;
    char *string = (char *)request;
    char const *end = string + length;
    while (string < end) {
        switch (session->parse_state) {
        case PARSE_STATE_REQUESTLINE: {
            char *firstCrlf = rtsp_find_first_crlf((const char *)string);
            if (firstCrlf != NULL) {
                firstCrlf[0] = '\0';
                ret = parse_response_line(session, string);
                string = firstCrlf + 2;
            }

            if (0 == ret) {
                session->parse_state = PARSE_STATE_HEADERSLINE;
            } else {
                string = (char *)end;
                ret = 1;
            }
        } break;

        case PARSE_STATE_HEADERSLINE: {
            char *firstCrlf = rtsp_find_first_crlf((const char *)string);
            if (firstCrlf != NULL) {
                firstCrlf[0] = '\0';
                ret = parse_headers_line(session, string);
                string = firstCrlf + 2;
            } else {
                string = (char *)end;
                ret = 1;
            }
        } break;

        case PARSE_STATE_GOTALL: {
            string = (char *)end;
            ret = 0;
        } break;

        default:
            ret = 1;
            break;
        }
    }

    return ret;
}

static void get_sdp_message(rtsp_client_t *session, char *buf, uint32_t buf_len, const char *session_name)
{
    snprintf(buf, buf_len,
             "v=0\r\n"
             "o=- 9%u 1 IN IP4 %s\r\n" //o=<username> <session id> <version> <network type> <address type> <address>
             "t=0 0\r\n"
             "a=control:*\r\n",
             GET_RANDOM(), session->url_ip);

    if (session_name) {
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "s=%s\r\n", session_name);
    } else {
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "s=Unnamed\r\n");
    }

    if (RTP_OVER_MULTICAST == session->transport_mode) {
        snprintf(buf + strlen(buf), buf_len - strlen(buf),
                 "a=type:broadcast\r\n"
                 "a=rtcp-unicast: reflection\r\n");
    }

    char str_buf[128];
    media_streams_t *it;
    SLIST_FOREACH(it, &session->media_list, next) {
        if (RTP_OVER_MULTICAST == session->transport_mode) {
            it->media_stream->get_description(it->media_stream, str_buf, sizeof(str_buf), 0);
            snprintf(buf + strlen(buf), buf_len - strlen(buf),
                     "%s\r\n", str_buf);

            snprintf(buf + strlen(buf), buf_len - strlen(buf),
                     "c=IN IP4 %s/255\r\n", "0.0.0.0"/*multicast_ip_.c_str()*/);
        } else {
            it->media_stream->get_description(it->media_stream, str_buf, sizeof(str_buf), 0);
            snprintf(buf + strlen(buf), buf_len - strlen(buf),
                     "%s\r\n", str_buf);
        }

        it->media_stream->get_attribute(it->media_stream, str_buf, sizeof(str_buf));
        snprintf(buf + strlen(buf), buf_len - strlen(buf),
                 "%s\r\n", str_buf);
        snprintf(buf + strlen(buf), buf_len - strlen(buf),
                 "a=control:trackID=%d\r\n", it->trackid);
    }
}



static int get_option_req(rtsp_client_t *session, char *buf, int buf_size)
{
    memset((void *)buf, 0, buf_size);
    int ret = snprintf(buf, buf_size,
                       "%s %s %s\r\n"
                       "CSeq: %u\r\n"
                       "User-Agent: %s\r\n"
                       "\r\n",
                       rtsp_methods[RTSP_OPTIONS].str, session->url, rtsp_get_version(),
                       session->CSeq,
                       rtsp_get_user_agent());

    session->method = RTSP_OPTIONS;
    return ret;
}

static int get_announce_req(rtsp_client_t *session, char *buf, int buf_size)
{
    memset((void *)buf, 0, buf_size);
    char SDPBuf[256];
    get_sdp_message(session, SDPBuf, sizeof(SDPBuf), NULL);
    int ret = snprintf(buf, buf_size,
                       "%s %s %s\r\n"
                       "Content-Type: application/sdp\r\n"
                       "CSeq: %u\r\n"
                       "User-Agent: %s\r\n"
                       "Session: %s\r\n"
                       "Content-Length: %d\r\n"
                       "\r\n"
                       "%s",
                       rtsp_methods[RTSP_ANNOUNCE].str, session->url, rtsp_get_version(),
                       session->CSeq,
                       rtsp_get_user_agent(),
                       session->session_id,
                       (int)strlen(SDPBuf),
                       SDPBuf);

    session->method = RTSP_ANNOUNCE;
    return ret;
}

static int get_describe_req(rtsp_client_t *session, char *buf, int buf_size)
{
    memset((void *)buf, 0, buf_size);
    int ret = snprintf(buf, buf_size,
                       "%s %s %s\r\n"
                       "CSeq: %u\r\n"
                       "Accept: application/sdp\r\n"
                       "User-Agent: %s\r\n"
                       "\r\n",
                       rtsp_methods[RTSP_DESCRIBE].str, session->url, rtsp_get_version(),
                       session->CSeq,
                       rtsp_get_user_agent());

    session->method = RTSP_DESCRIBE;
    return ret;
}

static int get_setup_req(rtsp_client_t *session, char *buf, int buf_size, int trackId)
{
    memset((void *)buf, 0, buf_size);
    int ret = 0;
    char transport[128] = {0};
    snprintf(transport, sizeof(transport), "RTP/AVP/%s;unicast;%s=%u-%u;mode=record",
             RTP_OVER_UDP == session->transport_mode ? "UDP" : "TCP",
             RTP_OVER_UDP == session->transport_mode ? "client_port" : "interleaved",
             RTP_OVER_UDP == session->transport_mode ? session->client_rtp_port : trackId * 2,
             RTP_OVER_UDP == session->transport_mode ? session->client_rtcp_port : (trackId * 2 + 1));

    ret = snprintf((char *)buf, buf_size,
                   "%s %s/trackID=%d %s\r\n"
                   "Transport: %s\r\n"
                   "CSeq: %u\r\n"
                   "User-Agent: %s\r\n"
                   "Session: %s\r\n"
                   "\r\n",
                   rtsp_methods[RTSP_SETUP].str, session->url, trackId, rtsp_get_version(),
                   transport,
                   session->CSeq,
                   rtsp_get_user_agent(),
                   session->session_id);

    session->method = RTSP_SETUP;
    return ret;
}

static int get_record_req(rtsp_client_t *session, const char *buf, int buf_size)
{
    memset((void *)buf, 0, buf_size);
    int ret = snprintf((char *)buf, buf_size,
                       "%s %s %s\r\n"
                       "Range: npt=0.000-\r\n"
                       "CSeq: %u\r\n"
                       "User-Agent: %s\r\n"
                       "Session: %s\r\n"
                       "\r\n",
                       rtsp_methods[RTSP_RECORD].str, session->url, rtsp_get_version(),
                       session->CSeq,
                       rtsp_get_user_agent(),
                       session->session_id);

    session->method = RTSP_RECORD;
    return ret;
}


static int rtsp_handle_requests(rtsp_client_t *session)
{
    if (!(session->state & 0x01)) {
        return -1;    // Already closed down
    }
    char *buffer = (char *)session->RecvBuf;
    memset(buffer, 0x00, RTSP_BUFFER_SIZE);
    int res = socketrecv(session->socket, buffer, RTSP_BUFFER_SIZE);
    if (res > 0) {
        if (0 == parse_rtsp_request(session, buffer, res)) {
        } else {
            ESP_LOGE(TAG, "rtsp request parse failed");
        }
        if (session->method == RTSP_RECORD) {
            session->state |= 0x02;
        } else if (session->method == RTSP_TEARDOWN) {
            session->state &= ~(0x02);
        }
    } else if (res == 0) {
        ESP_LOGI(TAG, "client closed socket, exiting");
        session->state = 0;
        return -3;
    } else  {
        return -2;
    }
    return 0;
}

rtsp_client_t *rtsp_client_create(void)
{
    rtsp_client_t *session = (rtsp_client_t *)calloc(1, sizeof(rtsp_client_t));
    RTSP_CHECK(NULL != session, "memory for rtsp session is not enough", NULL);

    SLIST_INIT(&session->media_list);

    // pthread_attr_t attr;
    // pthread_attr_init(&attr);
    // pthread_attr_setstacksize(&attr, 3072);
    // pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    // int res = pthread_create(&session->thread, &attr, rtsp_handle_task, (void *) session);
    // RTSP_CHECK_GOTO(res == 0, "create rtsp_handle_task failed", err);

    return session;
// err:
//     ESP_LOGE(TAG, "errno=%d reason: %s", errno, strerror(errno));
//     rtsp_client_delete(session);
//     return NULL;
}

int rtsp_client_delete(rtsp_client_t *session)
{
    RTSP_CHECK(NULL != session, "pointer of rtsp session is invaild", -EINVAL);

    media_streams_t *it;
    while (!SLIST_EMPTY(&session->media_list)) {
        it = SLIST_FIRST(&session->media_list);
        SLIST_REMOVE_HEAD(&session->media_list, next);
        rtp_session_delete(it->media_stream->rtp_session);
        it->media_stream->delete_media(it->media_stream);
        free(it);
    }

    free(session);
    return 0;
}

int rtsp_client_add_media_stream(rtsp_client_t *session, media_stream_t *media)
{
    RTSP_CHECK(NULL != session, "pointer of rtsp session is invaild", -EINVAL);

    media_streams_t *it = (media_streams_t *) calloc(1, sizeof(media_streams_t));
    RTSP_CHECK(NULL != it, "memory for rtsp media is not enough", -1);
    it->media_stream = media;
    it->trackid = session->media_stream_num++;
    it->next.sle_next = NULL;
    SLIST_INSERT_HEAD(&session->media_list, it, next);
    return 0;
}

int rtsp_client_push_media(rtsp_client_t *session, const char *url, transport_mode_t transport_mode)
{
    RTSP_CHECK(NULL != session, "pointer of rtsp session is invaild", -EINVAL);
    RTSP_CHECK(NULL != url, "url is invaild", -EINVAL);

    char host_ip[64] = {0};
    char url_suffix[32] = {0};
    uint16_t port = 0;

    if (strncmp(url, "rtsp://", 7) != 0) {
        ESP_LOGE(TAG, "url format error, only support rtsp://");
        return -EINVAL;
    }
    RTSP_CHECK(strlen(url) < 7 + sizeof(host_ip) + sizeof(url_suffix), "length of url is too long", -EINVAL);
    // parse url
    if (sscanf(url + 7, "%[^:]:%hu/%s", host_ip, &port, url_suffix) == 3) {

    } else if (sscanf(url + 7, "%[^/]/%s", host_ip, url_suffix) == 2) {
        port = 554; // set to default port
    } else {
        ESP_LOGE(TAG, "can't parse url");
        return -EINVAL;
    }
    sprintf(session->url, "rtsp://%s/%s", host_ip, url_suffix);

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(host_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    session->socket =  socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    RTSP_CHECK(session->socket > 0, "Unable to create socket", -1);
    int res = connect(session->socket, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
    RTSP_CHECK_GOTO(0 == res, "unable to connect server", err);
    ESP_LOGI(TAG, "Connected to %s:%d", host_ip, port);
    session->state |= 0x01;

    session->CSeq = 0;
    char *buffer = (char *)session->RecvBuf;
    memset(buffer, 0x00, RTSP_BUFFER_SIZE); session->CSeq++;
    int length = get_option_req(session, buffer, RTSP_BUFFER_SIZE);
    socketsend(session->socket, buffer, length);
    rtsp_handle_requests(session);
    if (session->response_code != 200) {
        ESP_LOGE(TAG, "response error");
        return -1;
    }

    memset(buffer, 0x00, RTSP_BUFFER_SIZE); session->CSeq++;
    length = get_announce_req(session, buffer, RTSP_BUFFER_SIZE);
    socketsend(session->socket, buffer, length);
    rtsp_handle_requests(session);
    if (session->response_code != 200) {
        ESP_LOGE(TAG, "response error");
        return -1;
    }

    session->transport_mode = transport_mode;

    media_streams_t *it;
    SLIST_FOREACH(it, &session->media_list, next) {
        rtp_session_info_t session_info = {
            .profile = &it->media_stream->rtp_profile,
            .transport_mode = session->transport_mode,
            .socket_tcp = session->socket,
            .rtp_port = session->server_rtp_port,
            .rtcp_port = session->server_rtcp_port,
            .rtp_channel = it->trackid * 2,
            .rtcp_channel = (it->trackid * 2) + 1,
            .bandwidth = 1000,
            .role = RTP_SENDER,
        };

        it->media_stream->rtp_session = rtp_session_create(&session_info);
        session->client_rtp_port = rtp_get_rtp_port(it->media_stream->rtp_session);
        session->client_rtcp_port = rtp_get_rtcp_port(it->media_stream->rtp_session);

        memset(buffer, 0x00, RTSP_BUFFER_SIZE); session->CSeq++;
        length = get_setup_req(session, buffer, RTSP_BUFFER_SIZE, it->trackid);
        socketsend(session->socket, buffer, length);
        rtsp_handle_requests(session);
        if (session->response_code != 200) {
            ESP_LOGE(TAG, "response error");
            return -1;
        }
        rtp_set_rtp_port(it->media_stream->rtp_session, session->server_rtp_port);
    }

    memset(buffer, 0x00, RTSP_BUFFER_SIZE); session->CSeq++;
    length = get_record_req(session, buffer, RTSP_BUFFER_SIZE);
    socketsend(session->socket, buffer, length);
    rtsp_handle_requests(session);
    if (session->response_code != 200) {
        ESP_LOGE(TAG, "response error");
        return -1;
    }

    return 0;

err:
    close(session->socket);
    return -1;
}


// static void *rtsp_handle_task(void *args)
// {
//     rtsp_client_t *session = (rtsp_client_t *)args;
//     while (1) {
//         sleep(1);
//     }
//     return NULL;
// }

