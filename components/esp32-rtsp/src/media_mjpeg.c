
#include <stdio.h>
#include <string.h>

#include "media_stream.h"
#include "media_mjpeg.h"
#include "rtp.h"
#include "time.h"

static const char *TAG = "rtp_mjpeg";

#define RTP_CHECK(a, str, ret_val)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

#define MAX_JPEG_PACKET_SIZE (RTP_MAX_PAYLOAD_SIZE - RTP_HEADER_SIZE - RTP_TCP_HEAD_SIZE)

/* JPEG marker codes */
enum JpegMarker {
    /* start of frame */
    SOF0  = 0xc0,       /* baseline */
    SOF1  = 0xc1,       /* extended sequential, huffman */
    SOF2  = 0xc2,       /* progressive, huffman */
    SOF3  = 0xc3,       /* lossless, huffman */

    SOF5  = 0xc5,       /* differential sequential, huffman */
    SOF6  = 0xc6,       /* differential progressive, huffman */
    SOF7  = 0xc7,       /* differential lossless, huffman */
    JPG   = 0xc8,       /* reserved for JPEG extension */
    SOF9  = 0xc9,       /* extended sequential, arithmetic */
    SOF10 = 0xca,       /* progressive, arithmetic */
    SOF11 = 0xcb,       /* lossless, arithmetic */

    SOF13 = 0xcd,       /* differential sequential, arithmetic */
    SOF14 = 0xce,       /* differential progressive, arithmetic */
    SOF15 = 0xcf,       /* differential lossless, arithmetic */

    DHT   = 0xc4,       /* define huffman tables */

    DAC   = 0xcc,       /* define arithmetic-coding conditioning */

    /* restart with modulo 8 count "m" */
    RST0  = 0xd0,
    RST1  = 0xd1,
    RST2  = 0xd2,
    RST3  = 0xd3,
    RST4  = 0xd4,
    RST5  = 0xd5,
    RST6  = 0xd6,
    RST7  = 0xd7,

    SOI   = 0xd8,       /* start of image */
    EOI   = 0xd9,       /* end of image */
    SOS   = 0xda,       /* start of scan */
    DQT   = 0xdb,       /* define quantization tables */
    DNL   = 0xdc,       /* define number of lines */
    DRI   = 0xdd,       /* define restart interval */
    DHP   = 0xde,       /* define hierarchical progression */
    EXP   = 0xdf,       /* expand reference components */

    APP0  = 0xe0,
    APP1  = 0xe1,
    APP2  = 0xe2,
    APP3  = 0xe3,
    APP4  = 0xe4,
    APP5  = 0xe5,
    APP6  = 0xe6,
    APP7  = 0xe7,
    APP8  = 0xe8,
    APP9  = 0xe9,
    APP10 = 0xea,
    APP11 = 0xeb,
    APP12 = 0xec,
    APP13 = 0xed,
    APP14 = 0xee,
    APP15 = 0xef,

    JPG0  = 0xf0,
    JPG1  = 0xf1,
    JPG2  = 0xf2,
    JPG3  = 0xf3,
    JPG4  = 0xf4,
    JPG5  = 0xf5,
    JPG6  = 0xf6,
    SOF48 = 0xf7,       ///< JPEG-LS
    LSE   = 0xf8,       ///< JPEG-LS extension parameters
    JPG9  = 0xf9,
    JPG10 = 0xfa,
    JPG11 = 0xfb,
    JPG12 = 0xfc,
    JPG13 = 0xfd,

    COM   = 0xfe,       /* comment */

    TEM   = 0x01,       /* temporary private use for arithmetic coding */

    /* 0x02 -> 0xbf reserved */
};

#define PREDICT(ret, topleft, top, left, predictor)\
    switch(predictor){\
        case 0: ret= 0; break;\
        case 1: ret= left; break;\
        case 2: ret= top; break;\
        case 3: ret= topleft; break;\
        case 4: ret= left   +   top - topleft; break;\
        case 5: ret= left   + ((top - topleft)>>1); break;\
        case 6: ret= top + ((left   - topleft)>>1); break;\
        default:\
        case 7: ret= (left + top)>>1; break;\
    }


/* Set up the standard Huffman tables (cf. JPEG standard section K.3) */
/* IMPORTANT: these are only valid for 8-bit data precision! */
static const uint8_t avpriv_mjpeg_bits_dc_luminance[17] =
{ /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t avpriv_mjpeg_val_dc[12] =
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

static const uint8_t avpriv_mjpeg_bits_dc_chrominance[17] =
{ /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };

static const uint8_t avpriv_mjpeg_bits_ac_luminance[17] =
{ /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
static const uint8_t avpriv_mjpeg_val_ac_luminance[] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

static const uint8_t avpriv_mjpeg_bits_ac_chrominance[17] =
{ /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };

static const uint8_t avpriv_mjpeg_val_ac_chrominance[] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
    0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
    0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
    0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
    0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};


static void media_stream_mjpeg_get_description(media_stream_t *stream, char *buf, uint32_t buf_len, uint16_t port)
{
    snprintf(buf, buf_len, "m=video %hu RTP/AVP %d", port, stream->rtp_profile.pt);
}

static void media_stream_mjpeg_get_attribute(media_stream_t *stream, char *buf, uint32_t buf_len)
{
    snprintf(buf, buf_len,
             "a=rtpmap:%d JPEG/90000",
            //  "a=framerate:22",
             stream->rtp_profile.pt);
}

static inline uint16_t AV_RB16(const uint8_t *v)
{
    return (uint16_t)v[0] << 8 | v[1];
}

static int decodeJPEGfile(const uint8_t **start, uint32_t *len, const uint8_t *(*qtables_)[4], int *nb_qtables, uint16_t *width, uint16_t *height)
{
    const uint8_t *(*qtables) = *qtables_;
    *nb_qtables = 0;
    int i;
    int default_huffman_tables = 0;
    const uint8_t *buf = *start;
    int size = *len;

    /* preparse the header for getting some info */
    for (i = 0; i < size; i++) {
        if (buf[i] != 0xff) {
            continue;
        }

        if (buf[i + 1] == DQT) {
            int tables, j;
            if (buf[i + 4] & 0xF0) {
                ESP_LOGW(TAG, "Only 8-bit precision is supported.\n");
            }

            /* a quantization table is 64 bytes long */
            tables = AV_RB16(&buf[i + 2]) / 65;
            if (i + 5 + tables * 65 > size) {
                ESP_LOGE(TAG, "Too short JPEG header. Aborted!\n");
                return 1;
            }
            if (*nb_qtables + tables > 4) {
                ESP_LOGE(TAG, "Invalid number of quantisation tables\n");
                return 1;
            }

            for (j = 0; j < tables; j++) {
                qtables[*nb_qtables + j] = buf + i + 5 + j * 65;
            }
            *nb_qtables += tables;
        } else if (buf[i + 1] == SOF0) {
            *height = (buf[i + 5] << 8) | (buf[i + 6]);
            *width  = (buf[i + 7] << 8) | (buf[i + 8]);

            if (buf[i + 14] != 17 || buf[i + 17] != 17) {
                ESP_LOGE(TAG, "Only 1x1 chroma blocks are supported. Aborted!\n");
                return 1;
            }
        } else if (buf[i + 1] == DHT) {
            int dht_size = AV_RB16(&buf[i + 2]);
            default_huffman_tables |= 1 << 4;
            i += 3;
            dht_size -= 2;
            if (i + dht_size >= size) {
                continue;
            }
            while (dht_size > 0)
                switch (buf[i + 1]) {
                case 0x00:
                    if (   dht_size >= 29
                            && !memcmp(buf + i +  2, avpriv_mjpeg_bits_dc_luminance + 1, 16)
                            && !memcmp(buf + i + 18, avpriv_mjpeg_val_dc, 12)) {
                        default_huffman_tables |= 1;
                        i += 29;
                        dht_size -= 29;
                    } else {
                        i += dht_size;
                        dht_size = 0;
                    }
                    break;
                case 0x01:
                    if (   dht_size >= 29
                            && !memcmp(buf + i +  2, avpriv_mjpeg_bits_dc_chrominance + 1, 16)
                            && !memcmp(buf + i + 18, avpriv_mjpeg_val_dc, 12)) {
                        default_huffman_tables |= 1 << 1;
                        i += 29;
                        dht_size -= 29;
                    } else {
                        i += dht_size;
                        dht_size = 0;
                    }
                    break;
                case 0x10:
                    if (   dht_size >= 179
                            && !memcmp(buf + i +  2, avpriv_mjpeg_bits_ac_luminance   + 1, 16)
                            && !memcmp(buf + i + 18, avpriv_mjpeg_val_ac_luminance, 162)) {
                        default_huffman_tables |= 1 << 2;
                        i += 179;
                        dht_size -= 179;
                    } else {
                        i += dht_size;
                        dht_size = 0;
                    }
                    break;
                case 0x11:
                    if (   dht_size >= 179
                            && !memcmp(buf + i +  2, avpriv_mjpeg_bits_ac_chrominance + 1, 16)
                            && !memcmp(buf + i + 18, avpriv_mjpeg_val_ac_chrominance, 162)) {
                        default_huffman_tables |= 1 << 3;
                        i += 179;
                        dht_size -= 179;
                    } else {
                        i += dht_size;
                        dht_size = 0;
                    }
                    break;
                default:
                    i += dht_size;
                    dht_size = 0;
                    continue;
                }
        } else if (buf[i + 1] == SOS) {
            /* SOS is last marker in the header */
            i += AV_RB16(&buf[i + 2]) + 2;
            if (i > size) {
                ESP_LOGE(TAG, "Insufficient data. Aborted!\n");
                return 1;
            }
            break;
        }
    }
    if (default_huffman_tables && default_huffman_tables != 31) {
        ESP_LOGE(TAG, "RFC 2435 requires standard Huffman tables for jpeg\n");
        return 1;
    }
    if (*nb_qtables && *nb_qtables != 2) {
        ESP_LOGW(TAG, "RFC 2435 suggests two quantization tables, %d provided\n", *nb_qtables);
    }

    /* skip JPEG header */
    buf  += i;
    size -= i;

    for (i = size - 2; i >= 0; i--) {
        if (buf[i] == 0xff && buf[i + 1] == EOI) {
            /* Remove the EOI marker */
            size = i;
            break;
        }
    }

    *start = buf;
    *len = size;

    return 0;
}

int media_stream_mjpeg_send_frame(media_stream_t *stream, const uint8_t *jpeg_data, uint32_t jpegLen)
{
    uint16_t w = 0;
    uint16_t h = 0;

    rtp_packet_t rtp_packet;
    rtp_packet.is_last = 0;
    rtp_packet.data = stream->rtp_buffer;

    // locate quant tables if possible
    const uint8_t *qtables[4] = {NULL};
    int qtable_num = 0;
    if (decodeJPEGfile(&jpeg_data, &jpegLen, &qtables, &qtable_num, &w, &h)) {
        ESP_LOGW(TAG, "can't decode jpeg data");
        return -1;
    }

    uint8_t q = (qtable_num != 0) ? 255 : 0x5e;
    uint8_t *mjpeg_buf = rtp_packet.data + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;
    ESP_LOGD(TAG, "(%dx%d), q=%d, qtable_num=%d", w, h, q, qtable_num);
    /**
     * Prepare the 8 byte payload JPEG header. Reference https://tools.ietf.org/html/rfc2435
     */
    jpeghdr_t jpghdr;
    jpghdr.tspec = 0; // type specific
    jpghdr.off = 0;
    jpghdr.type = 0; // fixed YUVJ422
    jpghdr.q = q;
    jpghdr.width = w / 8;
    jpghdr.height = h / 8;

    uint64_t cur_ts = rtp_time_now_us();
    int jpeg_bytes_left = jpegLen;
    while (jpeg_bytes_left != 0) {
        uint8_t *p_buf = mjpeg_buf;
        p_buf = nbo_mem_copy(p_buf, (uint8_t *)&jpghdr, sizeof(jpeghdr_t));

        // if (dri != 0) {
        //  jpeghdr_rst_t rsthdr;
        //         memcpy(ptr, &rsthdr, sizeof(rsthdr));
        //         ptr += sizeof(rsthdr);
        // }

        if (q >= 128 && jpghdr.off == 0) {
            // we need a quant header - but only in first packet of the frame
            int numQantBytes = 64; // Two 64 byte tables
            jpeghdr_qtable_t qtblhdr;
            qtblhdr.mbz = 0;
            qtblhdr.precision = 0; // 8 bit precision
            qtblhdr.length = qtable_num * numQantBytes;
            p_buf = nbo_mem_copy(p_buf, (uint8_t *)&qtblhdr, sizeof(jpeghdr_qtable_t));
            for (int i = 0; i < qtable_num; i++) {
                memcpy(p_buf, qtables[i], numQantBytes);
                p_buf += numQantBytes;
            }
        }

        uint32_t fragmentLen = MAX_JPEG_PACKET_SIZE - (p_buf - mjpeg_buf);
        if (fragmentLen >= jpeg_bytes_left) {
            fragmentLen = jpeg_bytes_left;
            rtp_packet.is_last = 1; // RTP marker bit must be set on last fragment
        }

        memcpy(p_buf, jpeg_data + jpghdr.off, fragmentLen);
        p_buf += fragmentLen;
        jpghdr.off += fragmentLen;
        jpeg_bytes_left -= fragmentLen;

        rtp_packet.size = p_buf - mjpeg_buf;
        rtp_packet.timestamp = cur_ts;
        rtp_packet.type = stream->rtp_profile.pt;
        rtp_send_packet(stream->rtp_session, &rtp_packet);
    }

    return 0;
}

static void media_stream_mjpeg_delete(media_stream_t *stream)
{
    if (NULL != stream->rtp_buffer) {
        free(stream->rtp_buffer);
    }
    free(stream);
}

media_stream_t *media_stream_mjpeg_create(void)
{
    media_stream_t *stream = (media_stream_t *)calloc(1, sizeof(media_stream_t));
    RTP_CHECK(NULL != stream, "memory for mjpeg stream is not enough", NULL);

    stream->rtp_buffer = (uint8_t *)malloc(RTP_MAX_PAYLOAD_SIZE);
    if (NULL == stream->rtp_buffer) {
        free(stream);
        ESP_LOGE(TAG, "memory for media mjpeg buffer is insufficient");
        return NULL;
    }
    memcpy(&stream->rtp_profile, rtp_profile_find(RTP_PAYLOAD_JPEG), sizeof(rtp_profile_t));
    // stream->rtp_profile = rtp_profile_find(RTP_PAYLOAD_JPEG);
    stream->delete_media = media_stream_mjpeg_delete;
    stream->get_attribute = media_stream_mjpeg_get_attribute;
    stream->get_description = media_stream_mjpeg_get_description;
    stream->handle_frame = media_stream_mjpeg_send_frame;
    return stream;
}

