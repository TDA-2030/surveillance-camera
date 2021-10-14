
#ifndef __RTP_H__
#define __RTP_H__

#include <stdint.h>
#include <sys/queue.h>
#include "port/platglue.h"
#include "rtp_util.h"
#include "rtp_profile.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RTP_VERSION 2    // RTP version field must equal 2 (p66)
#define RTP_SEQ_MOD (1 << 16)
#define RTP_MAX_SDES 255 /* maximum text length for SDES */

#define RTP_HEADER_SIZE 12        // size of the RTP header
#define RTP_MAX_PAYLOAD_SIZE 1420 //1460  1500-20-12-8
#define RTP_TCP_HEAD_SIZE 4
#define RTCP_BUFFER_LENGTH 256

#define RTP_PROBATION 2
#define RTP_DROPOUT 500
#define RTP_MISORDER 100

typedef enum {
    RTP_OVER_UDP,
    RTP_OVER_TCP,
    RTP_OVER_MULTICAST,
} transport_mode_t;

typedef enum {
    RTCP_FIR = 192,
    RTCP_NACK = 193,
    RTCP_SMPTETC = 194,
    RTCP_IJ = 195,

    RTCP_SR = 200,
    RTCP_RR = 201,
    RTCP_SDES = 202,
    RTCP_BYE = 203,
    RTCP_APP = 204,

    RTCP_RTPFB = 205,
    RTCP_PSFB = 206,
    RTCP_XR = 207,
    RTCP_AVB = 208,
    RTCP_RSI = 209,
    RTCP_TOKEN = 210,
} rtcp_type_t;

typedef enum {
    RTCP_SDES_END = 0,
    RTCP_SDES_CNAME = 1,
    RTCP_SDES_NAME = 2,
    RTCP_SDES_EMAIL = 3,
    RTCP_SDES_PHONE = 4,
    RTCP_SDES_LOC = 5,
    RTCP_SDES_TOOL = 6,
    RTCP_SDES_NOTE = 7,
    RTCP_SDES_PRIVATE = 8,
    RTCP_SDES_MAX,
} rtcp_sdes_type_t;

typedef struct { // source description RTCP packet
    uint8_t pt; // chunk type
    uint8_t len;
    uint8_t *data;
} rtcp_sdes_item_t;

#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
//rtp头
typedef struct {
    // little-endian
    uint32_t seq : 16;    /* sequence number */
    uint32_t pt : 7;      /* payload type */
    uint32_t m : 1;       /* marker bit */
    uint32_t cc : 4;      /* CSRC count */
    uint32_t x : 1;       /* header extension flag */
    uint32_t p : 1;       /* padding flag */
    uint32_t version : 2; /* protocol version */
    uint32_t ts;          /* timestamp */
    uint32_t ssrc;        /* synchronization source */
} rtp_hdr_t;

typedef struct {
    uint32_t length : 16; /* pkt len in words, w/o this word */
    uint32_t pt : 8;      /* RTCP packet type */
    uint32_t count : 5;   /* varies by packet type */
    uint32_t p : 1;       /* padding flag */
    uint32_t version : 2; /* protocol version */
} rtcp_hdr_t;

/*
* Reception report block */
typedef struct {
    uint32_t ssrc;         /* data source being reported */
    int32_t lost : 24;     /* cumul. no. pkts lost (signed!) */
    uint32_t fraction : 8; /* fraction lost since last SR/RR */
    uint32_t last_seq;     /* extended last seq. no. received */
    uint32_t jitter;       /* interarrival jitter */
    uint32_t lsr;          /* last SR packet from this source */
    uint32_t dlsr;         /* delay since last SR packet */
} rtcp_rb_t;

typedef struct { //SDES
    uint32_t src;             // first SSRC/CSRC
    rtcp_sdes_item_t item[1]; //list of SDES items
} rtcp_sdes;

typedef struct { //bye
    uint32_t src[1]; //list of sources
} rtcp_bye;

#else
typedef struct {
    // big-endian
    uint32_t version : 2; /* protocol version */
    uint32_t p : 1;       /* padding flag */
    uint32_t x : 1;       /* header extension flag */
    uint32_t cc : 4;      /* CSRC count */
    uint32_t m : 1;       /* marker bit */
    uint32_t pt : 7;      /* payload type */
    uint32_t seq : 16;    /* sequence number */

    uint32_t ts;   /* timestamp */
    uint32_t ssrc; /* synchronization source */
} rtp_hdr_t;

typedef struct {
    uint32_t version : 2; /* protocol version */
    uint32_t p : 1;       /* padding flag */
    uint32_t count : 5;   /* varies by packet type */
    uint32_t pt : 8;      /* RTCP packet type */
    uint32_t length : 16; /* pkt len in words, w/o this word */
} rtcp_hdr_t;

typedef struct { // report block
    uint32_t ssrc;
    uint32_t fraction : 8;    // fraction lost
    uint32_t cumulative : 24; // cumulative number of packets lost
    uint32_t exthsn;          // extended highest sequence number received
    uint32_t jitter;          // interarrival jitter
    uint32_t lsr;             // last SR
    uint32_t dlsr;            // delay since last SR
} rtcp_rb_t;

/*
* Reception report block */
typedef struct {
    uint32_t ssrc;             /* data source being reported */
    unsigned int fraction : 8; /* fraction lost since last SR/RR */
    int lost : 24;             /* cumul. no. pkts lost (signed!) */
    uint32_t last_seq;         /* extended last seq. no. received */
    uint32_t jitter;           /* interarrival jitter */
    uint32_t lsr;              /* last SR packet from this source */
    uint32_t dlsr;             /* delay since last SR packet */
} rtcp_rr_t;

/*
*  SDES  item */
typedef struct {
    uint8_t type;   /* type of item (rtcp_sdes_type_t) */
    uint8_t length; /* length of item (in octets) */
    char data[1];   /* text, not null-terminated */
} rtcp_sdes_item_t;

#endif

typedef struct {

    rtp_hdr_t rtp;
    uint32_t csrc[16];
    uint8_t *data;
    uint32_t size; //RTP packet size in byte
    uint64_t timestamp;
    uint8_t type;
    uint8_t is_last;

} rtp_packet_t;

typedef enum {
    RTP_SENDER = 1,   /// send RTP packet
    RTP_RECEIVER = 2, /// receive RTP packet
} rtp_role_t;

typedef struct {
    const rtp_profile_t *profile;
    transport_mode_t transport_mode;
    SOCKET socket_tcp;     // tcp
    uint16_t rtp_port;     // port for udp
    uint16_t rtcp_port;    // port for udp
    uint16_t rtp_channel;  //channel for rtsp over tcp
    uint16_t rtcp_channel; //channel for rtsp over tcp
    int bandwidth;
    rtp_role_t role; //sender or receiver
} rtp_session_info_t;

typedef struct { // sender report
    uint32_t ssrc;
    uint32_t ntpmsw; // ntp timestamp MSW(in second)
    uint32_t ntplsw; // ntp timestamp LSW(in picosecond)
    uint32_t rtpts;  // rtp timestamp
    uint32_t spc;    // sender packet count
    uint32_t soc;    // sender octet count
    rtcp_rb_t rb[1]; // variavle-length list
} rtcp_sr_t;

typedef struct { // receiver report
    uint32_t ssrc;
    rtcp_rb_t rb[1]; // variavle-length list
} rtcp_rr_t;
typedef struct rtp_member_t {
    uint32_t ssrc; // ssrc == rtcp_sr.ssrc == rtcp_rb.ssrc
    rtcp_sr_t rtcp_sr;
    rtcp_rb_t rtcp_rb;
    rtcp_sdes_item_t sdes[RTCP_SDES_MAX]; // SDES item

    uint64_t rtcp_send_timer; //used for rtcp send timer
    uint64_t rtcp_clock; // last RTCP SR/RR packet clock(local time)
    uint32_t rtp_ts_offset;
    uint64_t rtp_first_clock;

    uint16_t rtp_seq;       // last send/received RTP packet RTP sequence(in packet header)
    uint32_t rtp_timestamp; // last send/received RTP packet RTP timestamp(in packet header)
    uint64_t rtp_clock;     // last send/received RTP packet clock(local time)
    uint32_t rtp_packets;   // send/received RTP packet count(include duplicate, late)
    uint64_t rtp_bytes;     // send/received RTP octet count

    double jitter;
    uint32_t rtp_packets0;  // last SR received RTP packets
    uint32_t rtp_expected0; // last SR expect RTP sequence number

    uint16_t rtp_probation;
    uint16_t rtp_seq_base;   // init sequence number
    uint32_t rtp_seq_bad;    // bad sequence number
    uint32_t rtp_seq_cycles; // high extension sequence number

    SLIST_ENTRY(rtp_member_t) next;
} rtp_member_t;

typedef struct {
    rtp_session_info_t info;
    uint8_t rtcp_recv_buf[RTCP_BUFFER_LENGTH];
    rtp_hdr_t rtphdr;
    pthread_t rtcp_thread;
    int rtp_port;
    int rtcp_port;
    int rtp_socket;
    int rtcp_socket;

    SLIST_HEAD(sender_list_t, rtp_member_t) sender_list;
    SLIST_HEAD(member_list_t, rtp_member_t) member_list;

    rtp_member_t *self;

    // RTP/RTCP
    uint16_t sn;
    int avg_rtcp_size;
    int rtcp_bw;
    int rtcp_cycle; // for RTCP SDES
    int init;

    // statistics
    uint64_t statistic_timer;
    uint64_t statistic_last_bytes;
    float statistic_speed;
} rtp_session_t;

typedef struct {
    float speed_kbs;
    size_t total_kb;
    uint32_t elapsed_sec;
    
}rtp_statistics_info_t;


rtp_session_t *rtp_session_create(rtp_session_info_t *session_info);
void rtp_session_delete(rtp_session_t *session);

uint16_t rtp_get_rtp_port(rtp_session_t *session);
uint16_t rtp_get_rtcp_port(rtp_session_t *session);
void rtp_set_rtp_port(rtp_session_t *session, uint16_t port);
int rtp_send_packet(rtp_session_t *session, rtp_packet_t *packet);
void rtp_get_statistics(rtp_session_t *session, rtp_statistics_info_t *info);

#ifdef __cplusplus
}
#endif

#endif
