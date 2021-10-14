
#include <stdio.h>
#include <stdlib.h>
#include "rtcp.h"
#include "rtcp_pack.h"
#include "rtp_util.h"

static const char *TAG = "RTCP";

static int rtp_rtcp_interval(rtp_session_t *session);

static void rtp_seq_init(rtp_member_t *sender, uint16_t seq)
{
    sender->rtp_seq = seq;
    sender->rtp_seq_bad = (1 << 16) + 1; /* so seq == bad_seq is false */
    sender->rtp_seq_base = seq;
    sender->rtp_seq_cycles = 0;
    sender->rtp_packets0 = 0;
    sender->rtp_expected0 = 0;
    sender->rtp_bytes = 0;
    sender->rtp_packets = 0;
    sender->rtp_probation = 0;
    sender->jitter = 0.0;
}

static int rtp_seq_update(rtp_member_t *sender, uint16_t seq)
{
    uint16_t delta;
    delta = seq - sender->rtp_seq;

    if (sender->rtp_probation > 0) {
        if (sender->rtp_seq + 1 == seq) {
            sender->rtp_seq = seq;
            if (0 == --sender->rtp_probation) {
                rtp_seq_init(sender, seq);
                return 1;
            }
        } else {
            sender->rtp_probation = RTP_PROBATION;
            sender->rtp_seq = seq;
        }
        return 0;
    } else if ( delta < RTP_DROPOUT) {
        // in order, with permissible gap
        if (seq < sender->rtp_seq) {
            // sequence number wrapped
            sender->rtp_seq_cycles += (1 << 16);
        }

        sender->rtp_seq = seq;
    } else if ( delta <= (1 << 16) - RTP_MISORDER ) {
        /* the sequence number made a very large jump */
        if (sender->rtp_seq_bad + 1 == seq) {
            rtp_seq_init(sender, seq);
        } else {
            sender->rtp_seq_bad = seq;
            return 0;
        }
    } else {
        // duplicate or reordered packet
    }

    return 1;
}

rtp_member_t *rtp_member_fetch(rtp_session_t *session, uint32_t ssrc)
{
    rtp_member_t *p = NULL;

    return p;
}

rtp_member_t *rtp_sender_fetch(rtp_session_t *session, uint32_t ssrc)
{
    rtp_member_t *p = NULL;

    return p;
}


static int rtcp_pack(rtp_session_t *session, void *data, int bytes)
{
    int n;

    if (RTP_SENDER == session->info.role) {
        n = rtcp_sr_pack(session, (uint8_t *)data, bytes);
    } else {
        n = rtcp_rr_pack(session, (uint8_t *)data, bytes);
    }

    // compound RTCP Packet
    if (n < bytes) {
        n += rtcp_sdes_pack(session, (uint8_t *)data + n, bytes - n);
    }

    session->init = 0;
    return n;
}

int rtcp_send(rtp_session_t *session, uint8_t *buf, uint32_t len)
{
    uint8_t *udp_buf = buf + RTP_TCP_HEAD_SIZE;
    rtp_member_t *sender = session->self;

    uint64_t t = rtp_time_now_us();
    if (0 == sender->rtcp_send_timer) {
        sender->rtcp_send_timer = t;
    }
    if (t > sender->rtcp_send_timer) {
        sender->rtcp_send_timer = t + rtp_rtcp_interval(session);
        size_t n = rtcp_pack(session, udp_buf, len);
        sender->rtcp_clock = t;

        if (RTP_OVER_UDP == session->info.transport_mode) {
            IPADDRESS otherip;
            IPPORT otherport;
            socketpeeraddr(session->info.socket_tcp, &otherip, &otherport);
            udpsocketsend(session->rtcp_socket, udp_buf, n, otherip, session->info.rtcp_port);
        } else if (RTP_OVER_TCP == session->info.transport_mode) {
            buf[0] = '$'; // magic number
            buf[1] = session->info.rtcp_channel;
            buf[2] = (n & 0x0000FF00) >> 8;
            buf[3] = (n & 0x000000FF);

            // RTP over RTSP - we send the buffer + 4 byte additional header
            socketsend(session->info.socket_tcp, buf, n + RTP_TCP_HEAD_SIZE);
        }
    }
    return 0;
}

int rtcp_parse(rtp_session_t *session, const uint8_t *buffer, uint32_t len)
{
    // printf("RTCP Received %d bytes [", len);
    // for (size_t i = 0; i < len; i++) {
    //     printf("%x, ", buffer[i]);
    // }
    // printf("]\n");

    mem_swap32((uint8_t *)buffer, len);

    uint32_t deal_len = 0;
    while (deal_len < len) {
        rtcp_hdr_t *header = (rtcp_hdr_t *)buffer;
        size_t p_len = (header->length + 1) * 4;
        switch (header->pt) {
        case RTCP_SR:
            rtcp_sr_unpack(session, header, buffer + 4);
            break;

        case RTCP_RR:
            rtcp_rr_unpack(session, header, buffer + 4);
            break;

        case RTCP_SDES:
            rtcp_sdes_unpack(session, header, buffer + 4);
            break;

        case RTCP_BYE:
            rtcp_bye_unpack(session, header, buffer + 4);
            break;

        case RTCP_APP:
            // rtcp_app_unpack(session, header, buffer+4);
            break;

        default:
            ESP_LOGE(TAG, "Unknow RTCP packet (pt=%d,len=%d)", header->pt, p_len);
            break;
        }
        deal_len += p_len;
        buffer += deal_len;
        // RFC3550 6.3.3 Receiving an RTP or Non-BYE RTCP Packet (p26)
        session->avg_rtcp_size = (int)(session->avg_rtcp_size * 1.0 / 16 + p_len * 15.0 / 16);
    }

    if (deal_len != len) {
        ESP_LOGW(TAG, "RTCP receive length incorrect");
    }
    return 0;
}

int rtcp_input_rtp(rtp_session_t *session, const void *data, int bytes)
{
    rtp_packet_t pkt = {0};
    rtp_member_t *sender = NULL;

    /*
    if(0 != rtp_packet_deserialize(&pkt, data, bytes))
        return -1; // packet error

    assert(2 == pkt.rtp.v);
    sender = rtp_sender_fetch(session, pkt.rtp.ssrc);
    if(!sender)
        return -1; // memory error
    */

    uint64_t clock = rtp_time_now_us();

    // RFC3550 A.1 RTP Data Header Validity Checks
    if (0 == rtp_seq_update(sender, (uint16_t)pkt.rtp.seq)) {
        return 0;    // disorder(need more data)
    }

    // RFC3550 A.8 Estimating the Interarrival Jitter
    // the jitter estimate is updated:
    if (0 != sender->rtp_packets) {
        int D;
        D = (int)((unsigned int)((clock - sender->rtp_clock) * session->info.profile->frequency / 1000000) - (pkt.rtp.ts - sender->rtp_timestamp));
        if (D < 0) {
            D = -D;
        }
        sender->jitter += (D - sender->jitter) / 16.0;
    } else {
        sender->jitter = 0.0;
    }

    sender->rtp_clock = clock;
    sender->rtp_timestamp = pkt.rtp.ts;
    sender->rtp_bytes += pkt.size;
    sender->rtp_packets += 1;
    return 1;
}

uint16_t rtcp_get_member_num(rtp_session_t *session)
{
    uint16_t num = 0;
    rtp_member_t *it;
    SLIST_FOREACH(it, &session->member_list, next) {
        num++;
    }
    return num;
}

uint16_t rtcp_get_sender_num(rtp_session_t *session)
{
    uint16_t num = 0;
    rtp_member_t *it;
    SLIST_FOREACH(it, &session->sender_list, next) {
        num++;
    }
    return num;
}

const char *rtcp_get_cname(rtp_session_t *session, uint32_t ssrc)
{

    return "cname";
}

const char *rtcp_get_name(rtp_session_t *session, uint32_t ssrc)
{

    return "name";
}


static double rtcp_interval(int members,
                            int senders,
                            double rtcp_bw,
                            int we_sent,
                            double avg_rtcp_size,
                            int initial)
{
    /*
    * Minimum average time between RTCP packets from this site (in
    * seconds). This time prevents the reports from `clumping' when
    * sessions are small and the law of large numbers isn't helping
    * to smooth out the traffic. It also keeps the report interval
    * from becoming ridiculously small during transient outages like
    * a network partition.
    */
    double const RTCP_MIN_TIME = 5.0;

    /*
    * Fraction of the RTCP bandwidth to be shared among active
    * senders. (This fraction was chosen so that in a typical
    * session with one or two active senders, the computed report
    * time would be roughly equal to the minimum report time so that
    * we don't unnecessarily slow down receiver reports.) The
    * receiver fraction must be 1 - the sender fraction.
    */
    double const RTCP_SENDER_BW_FRACTION = 0.25;
    double const RTCP_RCVR_BW_FRACTION = (1 - RTCP_SENDER_BW_FRACTION);

    /*
    * To compensate for "timer reconsideration" converging to a
    * value below the intended average.
    */
    double const COMPENSATION = 2.71828 - 1.5;
    double t; /* interval */
    double rtcp_min_time = RTCP_MIN_TIME;
    int n; /* no. of members for computation */

    /*
    * Very first call at application start-up uses half the min
    * delay for quicker notification while still allowing some time
    * before reporting for randomization and to learn about other
    * sources so the report interval will converge to the correct
    * interval more quickly.
    */
    if (initial) {
        rtcp_min_time /= 2;
    }

    /*
    * Dedicate a fraction of the RTCP bandwidth to senders unless
    * the number of senders is large enough that their share is
    * more than that fraction.
    */
    n = members;
    if (senders <= members * RTCP_SENDER_BW_FRACTION) {
        if (we_sent) {
            rtcp_bw *= RTCP_SENDER_BW_FRACTION;
            n = senders;
        } else {
            rtcp_bw *= RTCP_RCVR_BW_FRACTION;
            n -= senders;
        }
    }

    /*
    * The effective number of sites times the average packet size is
    * the total number of octets sent when each site sends a report.
    * Dividing this by the effective bandwidth gives the time
    * interval over which those packets must be sent in order to
    * meet the bandwidth target, with a minimum enforced. In that
    * time interval we send one report so this time is also our
    * average time between reports.
    */
    t = avg_rtcp_size * n / rtcp_bw;
    if (t < rtcp_min_time) {
        t = rtcp_min_time;
    }

    /*
    * To avoid traffic bursts from unintended synchronization with
    * other sites, we then pick our actual next report interval as a
    * random number uniformly distributed between 0.5*t and 1.5*t.
    */
    t = t * (drand48() + 0.5);
    t = t / COMPENSATION;
    return t;
}

static int rtp_rtcp_interval(rtp_session_t *session)
{
    double interval;
    // rtp_session_t *session = (rtp_session_t *)rtp;
    // interval = rtcp_interval(rtp_member_list_count(session->members),//有多少个member
    //  rtp_member_list_count(session->senders) + ((RTP_SENDER==session->role) ? 1 : 0),
    //  session->rtcp_bw,
    //  (session->self->rtp_clock + 2*RTCP_REPORT_INTERVAL*1000 > rtp_time_now_us()) ? 1 : 0,
    //  session->avg_rtcp_size,
    //  session->init);

    return 5000 * 1000;
}

