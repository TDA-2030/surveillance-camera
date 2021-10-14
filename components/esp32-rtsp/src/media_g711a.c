
#include <stdio.h>
#include <string.h>
#include "media_stream.h"
#include "g711.h"
#include "media_g711a.h"

static const char *TAG = "rtp_g711a";

#define RTP_CHECK(a, str, ret_val)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

/**
 * https://datatracker.ietf.org/doc/html/rfc2327
 *
 */
static void media_stream_g711a_get_description(media_stream_t *stream, char *buf, uint32_t buf_len, uint16_t port)
{
    snprintf(buf, buf_len, "m=audio %hu RTP/AVP %d", port, stream->rtp_profile.pt);
}

static void media_stream_g711a_get_attribute(media_stream_t *stream, char *buf, uint32_t buf_len)
{
    snprintf(buf, buf_len,
             "a=rtpmap:%d PCMA/%hu/1\r\n"
             "a=framerate:10", // There should be no "\r\n" in the end
             stream->rtp_profile.pt, stream->sample_rate);
}

static int media_stream_g711a_send_frame(media_stream_t *stream, const uint8_t *data, uint32_t len)
{

#define MAX_PCMA_PACKET_SIZE (RTP_MAX_PAYLOAD_SIZE - RTP_HEADER_SIZE - RTP_TCP_HEAD_SIZE)

    rtp_packet_t rtp_packet;
    rtp_packet.is_last = 0;
    rtp_packet.data = stream->rtp_buffer;
    uint8_t *pcma_buf = rtp_packet.data + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;

    int data_bytes_left = len;
    uint32_t offset = 0;
    while (data_bytes_left != 0) {
        uint8_t *p_buf = (uint8_t *)pcma_buf;

        uint32_t fragmentLen = MAX_PCMA_PACKET_SIZE - (p_buf - pcma_buf);
        if (fragmentLen >= data_bytes_left) {
            fragmentLen = data_bytes_left;
            rtp_packet.is_last = 1; // RTP marker bit must be set on last fragment
        }

        memcpy(p_buf, data + offset, fragmentLen);
        p_buf += fragmentLen;
        offset += fragmentLen;
        data_bytes_left -= fragmentLen;

        rtp_packet.size = p_buf - pcma_buf;
        rtp_packet.timestamp = rtp_time_now_us();
        rtp_packet.type = stream->rtp_profile.pt;
        rtp_send_packet(stream->rtp_session, &rtp_packet);
    }
    return 1;
}

static void media_stream_g711a_delete(media_stream_t *stream)
{
    if (NULL != stream->rtp_buffer) {
        free(stream->rtp_buffer);
    }
    free(stream);
}

media_stream_t *media_stream_g711a_create(uint16_t sample_rate)
{
    media_stream_t *stream = (media_stream_t *)calloc(1, sizeof(media_stream_t));
    RTP_CHECK(NULL != stream, "memory for g711a stream is not enough", NULL);

    stream->rtp_buffer = (uint8_t *)malloc(RTP_MAX_PAYLOAD_SIZE);
    if (NULL == stream->rtp_buffer) {
        free(stream);
        ESP_LOGE(TAG, "memory for media mjpeg buffer is insufficient");
        return NULL;
    }
    memcpy(&stream->rtp_profile, rtp_profile_find(RTP_PAYLOAD_PCMA), sizeof(rtp_profile_t));
    // stream->rtp_profile = rtp_profile_find(RTP_PAYLOAD_PCMA);
    stream->sample_rate = sample_rate;
    stream->delete_media = media_stream_g711a_delete;
    stream->get_attribute = media_stream_g711a_get_attribute;
    stream->get_description = media_stream_g711a_get_description;
    stream->handle_frame = media_stream_g711a_send_frame;
    return stream;
}


