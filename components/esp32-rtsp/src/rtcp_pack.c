
#include <assert.h>
#include "rtcp.h"
#include "rtcp_pack.h"
#include "rtp_util.h"

static const char *TAG = "RTCP_pack";

static int rtcp_report_block(rtp_member_t *sender, uint8_t *ptr, int bytes);

void rtcp_rr_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t *ptr)
{
    ESP_LOGI(TAG, "%s:%s", __FILE__, __FUNCTION__);
    printf("sender ssrc=%X\n", *(uint32_t *)(ptr + 4));
    rtcp_rr_t *rr = (rtcp_rr_t *)(ptr);
    printf("ssrc=%X\n", rr->ssrc);
    printf("cumulative lost=%d\n", rr->rb->lost);
    printf("fraction=%d\n", rr->rb->fraction);
    printf("jitter=%d\n", rr->rb->jitter);
    printf("last sr ts=%d\n", rr->rb->lsr);
    printf("delay since last sr=%d\n", rr->rb->dlsr);
}

int rtcp_rr_pack(rtp_session_t *session, uint8_t *ptr, int bytes)
{
    // RFC3550 6.1 RTCP Packet Format
    // An individual RTP participant should send only one compound RTCP packet per report interval
    // in order for the RTCP bandwidth per participant to be estimated correctly (see Section 6.2),
    // except when the compound RTCP packet is split for partial encryption as described in Section 9.1.
    rtcp_hdr_t header;

    // assert(rtp_member_list_count(session->senders) < 32);
    header.version = 2;
    header.p = 0;
    header.pt = RTCP_RR;
    header.count = MIN(31, rtcp_get_sender_num(session));
    header.length = (4/*sizeof(rtcp_rr_t)*/ + header.count * 24/*sizeof(rtcp_rb_t)*/) / 4;

    if ((uint32_t)bytes < 4 + header.length * 4) {
        return 4 + header.length * 4;
    }

    memcpy(ptr, &header, sizeof(rtcp_hdr_t));//header

    // receiver SSRC
    nbo_w32(ptr + 4, session->self->ssrc); //ssrc

    ptr += 8;
    // report block
    rtp_member_t *it;
    SLIST_FOREACH(it, &session->member_list, next) {
        if (0 == it->rtp_packets || it->ssrc == session->self->ssrc) {
            continue;    // don't receive any packet
        }

        ptr += rtcp_report_block(it, ptr, 24);
    }

    return (header.length + 1) * 4;
}

void rtcp_sr_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t *ptr)
{
    ESP_LOGI(TAG, "%s:%s", __FILE__, __FUNCTION__);
}

int rtcp_sr_pack(rtp_session_t *session, uint8_t *ptr, int bytes)
{
    rtcp_hdr_t header;
    header.version = 2;
    header.p = 0;
    header.pt = RTCP_SR;
    header.count = MIN(31, rtcp_get_sender_num(session));
    header.length = (24/*sizeof(rtcp_sr_t)*/ + header.count * 24/*sizeof(rtcp_rb_t)*/) / 4; // see 6.4.1 SR: Sender Report RTCP Packet

    if (bytes < (header.length + 1) * 4) {
        return 0; // buffer length is not enough
    }

    ptr = nbo_mem_copy(ptr, (uint8_t *)&header, sizeof(rtcp_hdr_t)); //header

    // RFC3550 6.4.1 SR: Sender Report RTCP Packet (p32)
    // Note that in most cases this timestamp will not be equal to the RTP
    // timestamp in any adjacent data packet. Rather, it must be calculated from the corresponding
    // NTP timestamp using the relationship between the RTP timestamp counter and real time as
    // maintained by periodically checking the wallclock time at a sampling instant.
    uint64_t ntp = rtp_time_now_us();

    uint32_t timestamp = rtp_wallclock2timestamp(ntp - session->self->rtp_first_clock, session->info.profile->frequency) + session->self->rtp_ts_offset;
    uint32_t rtp_bytes = (uint32_t)session->self->rtp_bytes;

    ntp = clock2ntp(ntp);
    nbo_w32(ptr, session->self->ssrc);
    nbo_w32(ptr + 4, (uint32_t)(ntp >> 32));
    nbo_w32(ptr + 8, (uint32_t)(ntp & 0xFFFFFFFF));
    nbo_w32(ptr + 12, timestamp);
    nbo_w32(ptr + 16, session->self->rtp_packets); // send packets
    nbo_w32(ptr + 20, rtp_bytes); // send bytes

    ptr += 24;
    // report block
    rtp_member_t *it;
    SLIST_FOREACH(it, &session->member_list, next) {
        if (0 == it->rtp_packets || it->ssrc == session->self->ssrc) {
            continue;    // don't receive any packet
        }

        ptr += rtcp_report_block(it, ptr, 24);
    }

    return (header.length + 1) * 4;
}

static int rtcp_report_block(rtp_member_t *sender, uint8_t *ptr, int bytes)
{
    uint64_t delay;
    int lost_interval;
    int lost;
    uint32_t fraction;
    uint32_t expected, extseq;
    uint32_t expected_interval;
    uint32_t received_interval;
    uint32_t lsr, dlsr;

    if (bytes < 24) {
        return 0;
    }

    extseq = sender->rtp_seq_cycles + sender->rtp_seq; // 32-bits sequence number
    assert(extseq >= sender->rtp_seq_base);
    expected = extseq - sender->rtp_seq_base + 1;
    expected_interval = expected - sender->rtp_expected0;
    received_interval = sender->rtp_packets - sender->rtp_packets0;
    lost_interval = (int)(expected_interval - received_interval);
    if (lost_interval < 0 || 0 == expected_interval) {
        fraction = 0;
    } else {
        fraction = (lost_interval << 8) / expected_interval;
    }

    lost = expected - sender->rtp_packets;
    if (lost > 0x007FFFFF) {
        lost = 0x007FFFFF;
    } else if (lost < 0) {
        // 'Clamp' this loss number to a 24-bit signed value:
        // live555 RTCP.cpp RTCPInstance::enqueueReportBlock line:799
        lost = 0;
    }

    delay = rtp_time_now_us() - sender->rtcp_clock; // now - Last SR time
    lsr = ((sender->rtcp_sr.ntpmsw & 0xFFFF) << 16) | ((sender->rtcp_sr.ntplsw >> 16) & 0xFFFF);
    // in units of 1/65536 seconds
    // 65536/1000000 == 1024/15625
    dlsr = (uint32_t)(delay / 1000000.0f * 65536);

    nbo_w32(ptr, sender->ssrc);
    ptr[4] = (unsigned char)fraction;
    ptr[5] = (unsigned char)((lost >> 16) & 0xFF);
    ptr[6] = (unsigned char)((lost >> 8) & 0xFF);
    ptr[7] = (unsigned char)(lost & 0xFF);
    nbo_w32(ptr + 8, extseq);
    nbo_w32(ptr + 12, (uint32_t)sender->jitter);
    nbo_w32(ptr + 16, lsr);
    nbo_w32(ptr + 20, 0 == lsr ? 0 : dlsr);

    sender->rtp_expected0 = expected; // update source prior data
    sender->rtp_packets0 = sender->rtp_packets;

    return 24; /*sizeof(rtcp_rb_t)*/
}

void rtcp_sdes_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t *ptr)
{
    uint32_t i;
    uint32_t ssrc;
    rtp_member_t *member;
    const unsigned char *p, *end;

    p = ptr;
    end = ptr + header->length * 4;
    assert(header->length >= header->count);

    for (i = 0; i < header->count && p + 8 /*4-ssrc + 1-PT*/ <= end; i++) {
        ssrc = nbo_r32(p);
        member = rtp_member_fetch(session, ssrc);
        if (!member) {
            continue;
        }

        p += 4;
        while (p + 2 <= end && RTCP_SDES_END != p[0] /*PT*/) {
            rtcp_sdes_item_t item;
            item.pt = p[0];
            item.len = p[1];
            item.data = (unsigned char *)(p + 2);
            if (p + 2 + item.len > end) {
                assert(0);
                return; // error
            }

            switch (item.pt) {
            case RTCP_SDES_CNAME:
            case RTCP_SDES_NAME:
            case RTCP_SDES_EMAIL:
            case RTCP_SDES_PHONE:
            case RTCP_SDES_LOC:
            case RTCP_SDES_TOOL:
            case RTCP_SDES_NOTE:
                break;

            case RTCP_SDES_PRIVATE:
                assert(0);
                break;

            default:
                assert(0);
            }

            // RFC3550 6.5 SDES: Source Description RTCP Packet
            // Items are contiguous, i.e., items are not individually padded to a 32-bit boundary.
            // Text is not null terminated because some multi-octet encodings include null octets.
            p += 2 + item.len;
        }

        // RFC3550 6.5 SDES: Source Description RTCP Packet
        // The list of items in each chunk must be terminated by one or more null octets,
        // the first of which is interpreted as an item type of zero to denote the end of the list.
        // No length octet follows the null item type octet,
        // but additional null octets must be included if needed to pad until the next 32-bit boundary.
        // offset sizeof(SSRC) + sizeof(chunk type) + sizeof(chunk length)
        p = (const unsigned char *)((p - (const unsigned char *)0 + 3) / 4 * 4);
    }
}

static size_t rtcp_sdes_append_item(unsigned char *ptr, size_t bytes, rtcp_sdes_item_t *sdes)
{
    assert(sdes->data);
    if (bytes >= (size_t)sdes->len + 2) {
        ptr[0] = sdes->pt;
        ptr[1] = sdes->len;
        memcpy(ptr + 2, sdes->data, sdes->len);
    }

    return sdes->len + 2;
}

int rtcp_sdes_pack(rtp_session_t *session, uint8_t *ptr, int bytes)
{
    int n;
    rtcp_hdr_t header;

    // must have CNAME
    if (!session->self->sdes[RTCP_SDES_CNAME].data) {
        return 0;
    }

    header.version = 2;
    header.p = 0;
    header.pt = RTCP_SDES;
    header.count = 1; // self only
    header.length = 0;

    n = (int)rtcp_sdes_append_item(ptr + 8, bytes - 8, &session->self->sdes[RTCP_SDES_CNAME]);
    if (bytes < 8 + n) {
        return 8 + n;
    }

    // RFC3550 6.3.9 Allocation of Source Description Bandwidth (p29)
    // Every third interval (15 seconds), one extra item would be included in the SDES packet
    if (0 == session->rtcp_cycle % 3 && session->rtcp_cycle / 3 > 0) { // skip CNAME
        assert(session->rtcp_cycle / 3 < RTCP_SDES_PRIVATE);
        if (session->self->sdes[session->rtcp_cycle / 3 + 1].data) { // skip RTCP_SDES_END
            n += rtcp_sdes_append_item(ptr + 8 + n, bytes - n - 8, &session->self->sdes[session->rtcp_cycle / 3 + 1]);
            if (n + 8 > bytes) {
                return n + 8;
            }
        }
    }

    session->rtcp_cycle = (session->rtcp_cycle + 1) % 24; // 3 * SDES item number

    header.length = (uint16_t)((n + 4 + 3) / 4); // see 6.4.1 SR: Sender Report RTCP Packet
    memcpy(ptr, &header, sizeof(rtcp_hdr_t));//header
    nbo_w32(ptr + 4, session->self->ssrc);

    return (header.length + 1) * 4;
}

void rtcp_bye_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t *ptr)
{
    ESP_LOGI(TAG, "%s:%s", __FILE__, __FUNCTION__);
}

int rtcp_bye_pack(rtp_session_t *session, uint8_t *ptr, int bytes)
{
    rtcp_hdr_t header;

    if (bytes < 8) {
        return 8;
    }

    header.version = 2;
    header.p = 0;
    header.pt = RTCP_BYE;
    header.count = 1; // self only
    header.length = 1;
    memcpy(ptr, &header, sizeof(rtcp_hdr_t));

    nbo_w32(ptr + 4, session->self->ssrc);

    assert(8 == (header.length + 1) * 4);
    return 8;
}

void rtcp_app_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t *ptr)
{
    ESP_LOGI(TAG, "%s:%s", __FILE__, __FUNCTION__);
}

int rtcp_app_pack(rtp_session_t *session, uint8_t *ptr, int bytes, const char name[4], const void *app, int len)
{
    rtcp_hdr_t header;

    if (bytes >= 12 + (len + 3) / 4 * 4) {
        header.version = 2;
        header.p = 0;
        header.pt = RTCP_APP;
        header.count = 0;
        header.length = (uint16_t)(2 + (len + 3) / 4);
        memcpy(ptr, &header, sizeof(rtcp_hdr_t));

        nbo_w32(ptr + 4, session->self->ssrc);
        memcpy(ptr + 8, name, 4);

        if (len > 0) {
            memcpy(ptr + 12, app, len);
        }
    }

    return 12 + (len + 3) / 4 * 4;
}
