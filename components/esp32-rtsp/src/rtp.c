
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include "rtp.h"
#include "rtcp_pack.h"
#include "rtcp.h"

static const char *TAG = "RTP";

#define RTP_CHECK(a, str, ret_val)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

#define RTP_CHECK_GOTO(a, str, label)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        goto label;                                         \
    }

#define RTP_CHECK_VOID(a, str)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return;                                                  \
    }

// RFC3550 6.2 RTCP Transmission Interval (p21)
// It is recommended that the fraction of the session bandwidth added for RTCP be fixed at 5%.
// It is also recommended that 1/4 of the RTCP bandwidth be dedicated to participants that are sending data
#define RTCP_BANDWIDTH_FRACTION         0.05
#define RTCP_SENDER_BANDWIDTH_FRACTION  0.25

#define RTCP_REPORT_INTERVAL            5000 /* milliseconds RFC3550 p25 */
#define RTCP_REPORT_INTERVAL_MIN        2500 /* milliseconds RFC3550 p25 */

static int rtp_init_udp_transport(rtp_session_t *session);
static void rtp_release_udp_transport(rtp_session_t *session);


rtp_session_t *rtp_session_create(rtp_session_info_t *session_info)
{
    rtp_session_t *session = (rtp_session_t *)calloc(1, sizeof(rtp_session_t));
    RTP_CHECK(NULL != session, "memory for RTP session is not enough", NULL);

    session->info = *session_info;

    // init RTSP Session transport type (UDP or TCP) and ports for UDP transport
    if (RTP_OVER_UDP == session->info.transport_mode) {
        int ret = rtp_init_udp_transport(session);
        RTP_CHECK_GOTO(0 == ret, "Can't create udp socket", err);
    }

    session->rtcp_bw = (int)(session_info->bandwidth * RTCP_BANDWIDTH_FRACTION);
    session->avg_rtcp_size = 0;
    session->init = 1;
    session->sn = GET_RANDOM() & 0xffff;

    session->rtphdr.version = RTP_VERSION;
    session->rtphdr.p = 0;
    session->rtphdr.x = 0;
    session->rtphdr.cc = 0;
    session->rtphdr.m = 0;
    session->rtphdr.pt = 0;
    session->rtphdr.seq = 0;
    session->rtphdr.ts = 0;
    /**
     * This identifier SHOULD be chosen randomly,
     * with the intent that no two synchronization sources within the same RTP session will have thesame SSRC identifier.
     * TODO: Generate absolutely different SSRC for different RTP sessions
     */
    session->rtphdr.ssrc = GET_RANDOM();
    ESP_LOGI(TAG, "Creating RTP session, %s, SSRC:%X",
             session_info->transport_mode == RTP_OVER_TCP ? "TCP" : (session_info->transport_mode == RTP_OVER_UDP ? "UDP" : "MULTICAST"),
             session->rtphdr.ssrc);

    SLIST_INIT(&session->sender_list);
    SLIST_INIT(&session->member_list);

    session->self = (rtp_member_t *) calloc(1, sizeof(rtp_member_t));
    RTP_CHECK_GOTO(NULL != session->self, "memory for rtp is not enough", err);

    session->self->ssrc = session->rtphdr.ssrc;
    session->self->rtp_ts_offset = 0;//GET_RANDOM() & 0xffff; // The initial value of the timestamp SHOULD be random
    session->self->rtp_packets = 0;
    SLIST_INSERT_HEAD(&session->member_list, session->self, next);

    return session;
err:
    rtp_session_delete(session);
    return NULL;
}

void rtp_session_delete(rtp_session_t *session)
{
    RTP_CHECK_VOID(NULL != session,  "Pointer of rtp session is invalid");

    rtp_member_t *it;
    while (!SLIST_EMPTY(&session->member_list)) {
        it = SLIST_FIRST(&session->member_list);
        SLIST_REMOVE_HEAD(&session->member_list, next);
        free(it);
    }

    while (!SLIST_EMPTY(&session->sender_list)) {
        it = SLIST_FIRST(&session->sender_list);
        SLIST_REMOVE_HEAD(&session->sender_list, next);
        free(it);
    }

    rtp_release_udp_transport(session);
    free(session);
}

uint16_t rtp_get_rtp_port(rtp_session_t *session)
{
    RTP_CHECK(NULL != session,  "Pointer of rtp session is invalid", 0);
    return session->rtp_port;
}

uint16_t rtp_get_rtcp_port(rtp_session_t *session)
{
    RTP_CHECK(NULL != session,  "Pointer of rtp session is invalid", 0);
    return session->rtcp_port;
}

void rtp_set_rtp_port(rtp_session_t *session, uint16_t port)
{
    RTP_CHECK_VOID(NULL != session,  "Pointer of rtp session is invalid");
    session->info.rtp_port = port;
}

int rtp_send_packet(rtp_session_t *session, rtp_packet_t *packet)
{
    RTP_CHECK(NULL != session,  "Pointer of rtp session is invalid", -EINVAL);
    RTP_CHECK(NULL != packet,  "Pointer of rtp packet is invalid", -EINVAL);

    int ret = -1;
    uint8_t *RtpBuf = packet->data;
    uint8_t *udp_buf = RtpBuf + RTP_TCP_HEAD_SIZE; // UDP transport should skip RTP_TCP_HEAD_SIZE

    // Initialize RTP header
    rtp_hdr_t *rtphdr = &session->rtphdr;

    rtphdr->m = packet->is_last;
    rtphdr->pt = packet->type;
    rtphdr->seq = session->sn;

    if (0 == session->self->rtp_packets) {
        session->self->rtp_first_clock = packet->timestamp; // set clock offset to first packet timestamp
    }

    /**
    * Send RTCP packet
    */
    uint8_t temp[128] = {0};
    rtcp_send(session, temp, sizeof(temp));

    rtphdr->ts = rtp_wallclock2timestamp(packet->timestamp - session->self->rtp_first_clock, session->info.profile->frequency) + session->self->rtp_ts_offset;

    nbo_mem_copy(udp_buf, (uint8_t *)rtphdr, RTP_HEADER_SIZE);
    uint32_t RtpPacketSize = packet->size + RTP_HEADER_SIZE;

    // Send RTP packet
    if (RTP_OVER_UDP == session->info.transport_mode) {
        IPADDRESS otherip;
        IPPORT otherport;
        socketpeeraddr(session->info.socket_tcp, &otherip, &otherport);
        udpsocketsend(session->rtp_socket, udp_buf, RtpPacketSize, otherip, session->info.rtp_port);
    } else if (RTP_OVER_TCP == session->info.transport_mode) {
        RtpBuf[0] = '$'; // magic number
        RtpBuf[1] = session->info.rtp_channel;   // RTSP interleaved, here the RTP channel
        RtpBuf[2] = (RtpPacketSize & 0x0000FF00) >> 8;
        RtpBuf[3] = (RtpPacketSize & 0x000000FF);

        // RTP over RTSP - we send the buffer + 4 byte additional header
        socketsend(session->info.socket_tcp, RtpBuf, RtpPacketSize + RTP_TCP_HEAD_SIZE);
    } else {
        /**
         * TODO: need to implement Multicast
         *
         */
    }
    session->sn++;
    session->self->rtp_packets++;
    session->self->rtp_bytes += packet->size;

    uint64_t t = rtp_time_now_us();
    if (t > session->statistic_timer + 1000000) {
        session->statistic_speed = (session->self->rtp_bytes - session->statistic_last_bytes) * 1000 / ((t - session->statistic_timer) / 1000.0f);
        session->statistic_timer = t;
        session->statistic_last_bytes = session->self->rtp_bytes;
    }

    return ret;
}

int rtp_rtcp_bye(rtp_session_t *session, void *data, int bytes)
{
    return rtcp_bye_pack(session, (uint8_t *)data, bytes);
}

void rtp_get_statistics(rtp_session_t *session, rtp_statistics_info_t *info)
{
    info->elapsed_sec = (rtp_time_now_us() - session->self->rtp_first_clock) / 1000000;
    info->speed_kbs = session->statistic_speed / 1024;
    info->total_kb = session->self->rtp_bytes / 1024;
}

static int rtp_init_udp_transport(rtp_session_t *session)
{
    uint16_t P = 0;
#define UDP_PORT_MIN 6970
#define UDP_PORT_MAM 7000

    for (P = UDP_PORT_MIN; P < UDP_PORT_MAM; P += 2) {
        session->rtp_socket = udpsocketcreate(P);
        if (session->rtp_socket) {
            // Rtp socket was bound successfully. Lets try to bind the consecutive Rtsp socket
            session->rtcp_socket = udpsocketcreate(P + 1);
            if (session->rtcp_socket) {
                // allocate port pairs for RTP/RTCP ports in UDP transport mode
                session->rtp_port = P;
                session->rtcp_port = P + 1;
                break;
            } else {
                udpsocketclose(session->rtp_socket);
            }
        }
    }
    if (P >= UDP_PORT_MAM) {
        ESP_LOGE(TAG, "Can't create udp socket for RTP and RTCP");
        return -1;
    }
    return 0;
}

static void rtp_release_udp_transport(rtp_session_t *session)
{
    session->rtp_port = 0;
    session->rtcp_port = 0;
    if (RTP_OVER_UDP == session->info.transport_mode) {
        udpsocketclose(session->rtp_socket);
        udpsocketclose(session->rtcp_socket);
    }

    session->rtp_socket = NULLSOCKET;
    session->rtcp_socket = NULLSOCKET;
}
