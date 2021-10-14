
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "rtsp_utility.h"
#include "rtsp_server.h"
#include "rtcp.h"

static const char *TAG = "rtsp_server";

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

static int ParseRequestLine(rtsp_server_t *session, const char *message)
{
    char method[32] = {0};
    char *url = session->url;
    char version[32] = {0};

    if (sscanf(message, "%s %s %s", method, url, version) != 3) {
        return 1;
    }
    char *url_end = &url[strlen(url) - 1];
    if (*url_end == '/') {
        *url_end = '\0'; // The character '/' at the end of url may cause some trouble in later processing, remove it.
    }

    // Get rtsp method
    if (strstr(method, rtsp_methods[RTSP_OPTIONS].str)) {
        session->method = RTSP_OPTIONS;
    } else if (strstr(method, rtsp_methods[RTSP_DESCRIBE].str)) {
        session->method = RTSP_DESCRIBE;
    } else if (strstr(method, rtsp_methods[RTSP_SETUP].str)) {
        session->method = RTSP_SETUP;
    } else if (strstr(method, rtsp_methods[RTSP_PLAY].str)) {
        session->method = RTSP_PLAY;
    } else if (strstr(method, rtsp_methods[RTSP_TEARDOWN].str)) {
        session->method = RTSP_TEARDOWN;
    } else if (strstr(method, rtsp_methods[RTSP_PAUSE].str)) {
        session->method = RTSP_PAUSE;
    } else if (strstr(method, rtsp_methods[RTSP_ANNOUNCE].str)) {
        session->method = RTSP_ANNOUNCE;
    } else if (strstr(method, rtsp_methods[RTSP_GET_PARAMETER].str)) {
        session->method = RTSP_GET_PARAMETER;
    } else if (strstr(method, rtsp_methods[RTSP_SET_PARAMETER].str)) {
        session->method = RTSP_SET_PARAMETER;
    } else {
        session->method = RTSP_UNKNOWN;
        return 1;
    }

    if (strncmp(url, "rtsp://", 7) != 0) {
        return 1;
    }

    // parse url
    if (sscanf(url + 7, "%[^:]:%hu/%s", session->url_ip, &session->url_port, session->url_suffix) == 3) {

    } else if (sscanf(url + 7, "%[^/]/%s", session->url_ip, session->url_suffix) == 2) {
        session->url_port = 554; // set to default port
    } else {
        return 1;
    }
    ESP_LOGD(TAG, "url:%s", session->url);
    ESP_LOGD(TAG, "url_suffix:%s", session->url_suffix);
    return 0;
}

static int ParseHeadersLine(rtsp_server_t *session, const char *message)
{
    ESP_LOGD(TAG, "<%s>", message);
    char *TmpPtr = NULL;
    TmpPtr = (char *)strstr(message, "CSeq: ");
    if (TmpPtr) {
        session->cseq  = atoi(TmpPtr + 6);
        return 0;
    }

    if (session->method == RTSP_DESCRIBE || session->method == RTSP_SETUP || session->method == RTSP_PLAY) {
        // ParseAuthorization(message);
    }

    if (session->method == RTSP_OPTIONS) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    if (session->method == RTSP_DESCRIBE) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    if (session->method == RTSP_SETUP) {
        TmpPtr = (char *)strstr(message, "Transport");
        if (TmpPtr) { // parse transport header
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

            char *ClientPortPtr = NULL;
            if (RTP_OVER_UDP == session->transport_mode) {
                ClientPortPtr = (char *)strstr(message, "client_port=");
            } else if (RTP_OVER_MULTICAST == session->transport_mode) {
                ClientPortPtr = (char *)strstr(message, "port=");
            }
            if (ClientPortPtr) {
                ClientPortPtr += (RTP_OVER_UDP == session->transport_mode) ? 12 : 5;
                char cp[16] = {0};
                char *p = strchr(ClientPortPtr, '-');
                if (p) {
                    strncpy(cp, ClientPortPtr, p - ClientPortPtr);
                    session->client_rtp_port  = atoi(cp);
                    session->client_rtcp_port = session->client_rtp_port + 1;
                    ESP_LOGI(TAG, "rtsp client port %d-%d", session->client_rtp_port, session->client_rtcp_port);
                } else {
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

static int parse_rtsp_request(rtsp_server_t *session, const char *request, uint32_t length)
{
    session->method = RTSP_UNKNOWN;
    session->cseq = 0;
    memset(session->url, 0x00, RTSP_PARAM_STRING_MAX);
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
                ret = ParseRequestLine(session, string);
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
                ret = ParseHeadersLine(session, string);
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

static char const *DateHeader(char *buf, uint32_t length)
{
    time_t tt = time(NULL);
    strftime(buf, length, "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

static void Handle_RtspOPTION(rtsp_server_t *session, char *Response, uint32_t *length)
{
    char time_str[64];
    if (0 != strcmp(session->url_suffix, session->cfg.url)) {
        ESP_LOGE(TAG, "[%s] Stream Not Found", session->url);
        // Stream not available
        int len = snprintf(Response, *length,
                           "%s %s\r\n"
                           "CSeq: %u\r\n"
                           "%s\r\n",
                           rtsp_get_version(),
                           rtsp_get_status_from_code(404),
                           session->cseq,
                           DateHeader(time_str, sizeof(time_str)));
        if (len > 0) {
            *length = len;
        }
        return;
    }

    int len = snprintf(Response, *length,
                       "%s %s\r\n"
                       "CSeq: %u\r\n"
                       "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n",
                       rtsp_get_version(),
                       rtsp_get_status_from_code(200),
                       session->cseq);
    if (len > 0) {
        *length = len;
    }
}

static void GetSdpMessage(rtsp_server_t *session, char *buf, uint32_t buf_len)
{
    snprintf(buf, buf_len,
             "v=0\r\n"
             "o=- 9%u 1 IN IP4 %s\r\n" //o=<username> <session id> <version> <network type> <address type> <address>
             "t=0 0\r\n"
             "a=control:*\r\n",
             GET_RANDOM(), session->url_ip);

    if (session->cfg.session_name) {
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "s=%s\r\n", session->cfg.session_name);
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
                     "c=IN IP4 %s/255\r\n", "0.0.0.0");
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

static void Handle_RtspDESCRIBE(rtsp_server_t *session, char *Response, uint32_t *length)
{
    char time_str[64];
    char SDPBuf[256];
    GetSdpMessage(session, SDPBuf, sizeof(SDPBuf));
    int len = snprintf(Response, *length,
                       "%s %s\r\n"
                       "CSeq: %u\r\n"
                       "%s\r\n"
                       "Content-Base: %s/\r\n"
                       "Content-Type: application/sdp\r\n"
                       "Content-Length: %d\r\n\r\n"
                       "%s",
                       rtsp_get_version(),
                       rtsp_get_status_from_code(200),
                       session->cseq,
                       DateHeader(time_str, sizeof(time_str)),
                       session->url,
                       (int) strlen(SDPBuf),
                       SDPBuf);

    if (len > 0) {
        *length = len;
    }
}

static void Handle_RtspSETUP(rtsp_server_t *session, char *Response, uint32_t *length)
{
    int32_t trackID = 0;
    char *p = strstr(session->url_suffix, "trackID=");
    if (p) {
        trackID = atoi(p + 8);
    } else {
        ESP_LOGE(TAG, "can't parse trackID");
    }

    ESP_LOGI(TAG, "trackID=%d", trackID);

    media_streams_t *it;
    SLIST_FOREACH(it, &session->media_list, next) {
        if (it->trackid == trackID) {
            rtp_session_info_t session_info = {
                .profile = &it->media_stream->rtp_profile,
                .transport_mode = session->transport_mode,
                .socket_tcp = session->client_socket,
                .rtp_port = session->client_rtp_port,
                .rtcp_port = session->client_rtcp_port,
                .rtp_channel = session->rtp_channel,
                .rtcp_channel = session->rtcp_channel,
                .bandwidth = 1000,
                .role = RTP_SENDER,
            };
            it->media_stream->rtp_session = rtp_session_create(&session_info);
            break; // has been created rtp session
        }
    }

    char Transport[128];
    char time_str[64];
    if (RTP_OVER_TCP == session->transport_mode) {
        snprintf(Transport, sizeof(Transport), "RTP/AVP/TCP;unicast;interleaved=%i-%i", session->rtp_channel, session->rtcp_channel);
    } else {
        snprintf(Transport, sizeof(Transport),
                 "RTP/AVP;unicast;client_port=%i-%i;server_port=%i-%i",
                 session->client_rtp_port,
                 session->client_rtcp_port,
                 rtp_get_rtp_port(it->media_stream->rtp_session),
                 rtp_get_rtcp_port(it->media_stream->rtp_session));
    }
    int len = snprintf(Response, *length,
                       "%s %s\r\n"
                       "CSeq: %u\r\n"
                       "%s\r\n"
                       "Transport: %s\r\n"
                       "Session: %s\r\n\r\n",
                       rtsp_get_version(),
                       rtsp_get_status_from_code(200),
                       session->cseq,
                       DateHeader(time_str, sizeof(time_str)),
                       Transport,
                       session->session_id);

    if (len > 0) {
        *length = len;
    }
}

static void Handle_RtspPLAY(rtsp_server_t *session, char *Response, uint32_t *length)
{
    char time_str[64];
    int len = snprintf(Response, *length,
                       "%s %s\r\n"
                       "CSeq: %u\r\n"
                       "%s\r\n"
                       "Range: npt=0.000-\r\n"
                       "Session: %s\r\n"
                       "\r\n",
                       rtsp_get_version(),
                       rtsp_get_status_from_code(200),
                       session->cseq,
                       DateHeader(time_str, sizeof(time_str)),
                       session->session_id);
    if (len > 0) {
        *length = len;
    }
}

rtsp_server_t *rtsp_server_create(rtsp_server_cfg_t *cfg)
{
    RTSP_CHECK(NULL != cfg->url, "url is invaild", NULL);

    rtsp_server_t *session = (rtsp_server_t *)calloc(1, sizeof(rtsp_server_t));
    RTSP_CHECK(NULL != session, "memory for rtsp session is not enough", NULL);

    session->cfg = *cfg;
    uint16_t port = (0 == cfg->port) ? 554 : cfg->port;

    struct sockaddr_in ServerAddr;  // server address parameters
    ServerAddr.sin_family      = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port        = htons(port); // listen on RTSP port
    session->listen_socket     = socket(AF_INET, SOCK_STREAM, 0);
    RTSP_CHECK_GOTO(session->listen_socket > 0, "Unable to create socket", err);

    int enable = 1;
    int res = 0;
    res = setsockopt(session->listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    RTSP_CHECK_GOTO(res == 0, "setsockopt(SO_REUSEADDR) failed", err);

    // bind our listen socket to the RTSP port and listen for a client connection
    res = bind(session->listen_socket, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr));
    RTSP_CHECK_GOTO(res == 0, "error can't bind port", err);
    res = listen(session->listen_socket, 5);
    RTSP_CHECK_GOTO(res == 0, "error can't listen socket", err);

    snprintf(session->session_id, sizeof(session->session_id), "%X",  GET_RANDOM()); // create a session ID
    session->client_rtp_port  =  0;
    session->client_rtcp_port =  0;
    session->state = 0;
    SLIST_INIT(&session->media_list);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 3072);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    res = pthread_create(&session->thread, &attr, rtsp_handle_task, (void *) session);
    RTSP_CHECK_GOTO(res == 0, "create rtsp_handle_task failed", err);

    return session;
err:
    ESP_LOGE(TAG, "errno=%d reason: %s", errno, strerror(errno));
    rtsp_server_delete(session);
    return NULL;
}

int rtsp_server_delete(rtsp_server_t *session)
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

    closesocket(session->listen_socket);
    free(session);
    return 0;
}

int rtsp_server_add_media_stream(rtsp_server_t *session, media_stream_t *media)
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

/**
   Read from our socket, parsing commands as possible.
 */
static void *rtsp_handle_task(void *args)
{
    rtsp_server_t *session = (rtsp_server_t *)args;
    while (1) {
        ESP_LOGI(TAG, "waiting for client");
        struct sockaddr_in clientaddr;                                   // address parameters of a new RTSP client
        socklen_t clientaddrLen = sizeof(clientaddr);
        session->client_socket = accept(session->listen_socket, (struct sockaddr *)&clientaddr, &clientaddrLen);
        session->state |= 0x01;
        ESP_LOGI(TAG, "Client connected. Client address: %s", inet_ntoa(clientaddr.sin_addr));
        if (session->cfg.accept_cb_fn) {
            session->cfg.accept_cb_fn(session);
        }
        fd_set total_read_set;
        FD_ZERO(&total_read_set);
        FD_SET(session->client_socket, &total_read_set);
        int maxfd = MAX(session->client_socket, 0);

        while (0 != session->state) {
            fd_set read_set = total_read_set;
            media_streams_t *it;
            int active_cnt = select(maxfd + 1, &read_set, NULL, NULL, NULL);
            if (active_cnt < 0) {
                ESP_LOGE(TAG, "error in select (%d)", errno);
            }

            if (session->state & 0x02) { // only rtp has been setup
                /**
                 * receive RTCP data over UDP
                 */
                SLIST_FOREACH(it, &session->media_list, next) {
                    rtp_session_t *rtp_s = (rtp_session_t *)it->media_stream->rtp_session;
                    if (FD_ISSET(rtp_s->rtcp_socket, &read_set)) {
                        struct sockaddr_storage source_addr;
                        socklen_t socklen = sizeof(source_addr);
                        int len = recvfrom(rtp_s->rtcp_socket, rtp_s->rtcp_recv_buf, sizeof(rtp_s->rtcp_recv_buf) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
                        if (len < 0) {
                            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                        } else if (len == 0) {
                            ESP_LOGW(TAG, "rtcp udp closed");
                        } else {
                            rtcp_parse(rtp_s, rtp_s->rtcp_recv_buf, len);
                        }
                    }
                }
            }

            /**
             * receive data over TCP
             */
            if (FD_ISSET(session->client_socket, &read_set)) {

                char *buffer = (char *)session->recvbuf;
                memset(buffer, 0x00, RTSP_BUFFER_SIZE);
                int res = socketrecv(session->client_socket, buffer + session->recv_offset, RTSP_BUFFER_SIZE);
                if (res > 0) {
                    if (buffer[0] == '$') { // RTSP interleaved magic number
                        // Received RTCP
                        printf("rtsp,len=%d[", res);
                        for (size_t i = 0; i < res; i++) {
                            printf("%x, ", buffer[i]);
                        } printf("]\n");
                        session->rtcp_length = buffer[2] << 8 | buffer[3];
                        session->rtcp_channel = buffer[1];
                        ESP_LOGI(TAG, "RTCP recv channel=%d, len=%d", session->rtcp_channel, session->rtcp_length);
                        if (session->recv_offset + res != session->rtcp_length + 4) {
                            // Only RTSP interleaved header, change the parse_state
                            session->recv_offset += res;
                            session->parse_state = PARSE_RTCP_HEADER;
                        } else {
                            media_streams_t *it;
                            SLIST_FOREACH(it, &session->media_list, next) {
                                if (it->media_stream->rtp_session->info.rtcp_channel == session->rtcp_channel) {
                                    rtcp_parse(it->media_stream->rtp_session, (const uint8_t *)buffer + 4, res - 4);
                                    break;
                                }
                            }
                        }
                    } else if (PARSE_RTCP_HEADER == session->parse_state) {

                        SLIST_FOREACH(it, &session->media_list, next) {
                            if (it->media_stream->rtp_session->info.rtcp_channel == session->rtcp_channel) {
                                rtcp_parse(it->media_stream->rtp_session, (const uint8_t *)buffer + 4, session->recv_offset + res - 4);
                                break;
                            }
                        }
                        session->recv_offset = 0; // reset offset 
                    } else {
                        // Received RTSP request
                        if (0 == parse_rtsp_request(session, buffer, res)) {
                            uint32_t length = RTSP_BUFFER_SIZE;
                            switch (session->method) {
                            case RTSP_OPTIONS: Handle_RtspOPTION(session, buffer, &length);
                                break;

                            case RTSP_DESCRIBE: Handle_RtspDESCRIBE(session, buffer, &length);
                                break;

                            case RTSP_SETUP: Handle_RtspSETUP(session, buffer, &length);
                                break;

                            case RTSP_PLAY: {
                                Handle_RtspPLAY(session, buffer, &length);
                                if (RTP_OVER_UDP == session->transport_mode) { // add udp socket to total_read_set when over udp
                                    media_streams_t *it;
                                    SLIST_FOREACH(it, &session->media_list, next) {
                                        FD_SET(it->media_stream->rtp_session->rtcp_socket, &total_read_set);
                                        maxfd = MAX(it->media_stream->rtp_session->rtcp_socket, maxfd);
                                    }
                                }
                                session->state |= 0x02;
                            } break;

                            case RTSP_TEARDOWN: {
                                session->state &= ~(0x02);
                            } break;

                            default: break;
                            }
                            socketsend(session->client_socket, buffer, length);
                        } else {
                            ESP_LOGE(TAG, "rtsp request parse failed");
                        }
                    }
                } else if (res == 0) {
                    ESP_LOGI(TAG, "client closed socket, exiting");
                    session->state = 0;
                    session->parse_state = 0;
                    session->rtcp_length = 0;
                    session->recv_offset = 0;
                    break;
                } else  {
                    ESP_LOGE(TAG, "rtsp read error errno=%d reason: %s", errno, strerror(errno));
                }
            }
        }
        ESP_LOGI(TAG, "closing RTSP session");
        if (session->cfg.close_cb_fn) {
            session->cfg.close_cb_fn(session);
        }
        // Delete rtp session
        media_streams_t *it;
        SLIST_FOREACH(it, &session->media_list, next) {
            rtp_session_delete(it->media_stream->rtp_session);
            it->media_stream->rtp_session = NULL;
        }
        closesocket(session->client_socket); // close socket of client

        /**
         * TODO: how to exit the thread when the rtsp session is deleted
         */
        if (0) {
            break;
        }

    }
    return NULL;
}
