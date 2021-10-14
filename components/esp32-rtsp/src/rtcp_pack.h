#ifndef _RTCP_PACK_H_ 
#define _RTCP_PACK_H_

#include <stdint.h>
#include "rtp.h"

#ifdef __cplusplus
extern "C" {
#endif


rtp_member_t* rtp_sender_fetch(rtp_session_t *session, uint32_t ssrc);
rtp_member_t* rtp_member_fetch(rtp_session_t *session, uint32_t ssrc);

int rtcp_input_rtp(rtp_session_t *session, const void* data, int bytes);
int rtcp_input_rtcp(rtp_session_t *session, const void* data, int bytes);

int rtcp_rr_pack(rtp_session_t *session, uint8_t* data, int bytes );
int rtcp_sr_pack(rtp_session_t *session, uint8_t* data, int bytes );
int rtcp_sdes_pack(rtp_session_t *session, uint8_t* data, int bytes );
int rtcp_bye_pack(rtp_session_t *session, uint8_t* data, int bytes );
int rtcp_app_pack(rtp_session_t *session, uint8_t* ptr, int bytes, const char name[4], const void* app, int len );
void rtcp_rr_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t* data);
void rtcp_sr_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t* data);
void rtcp_sdes_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t* data);
void rtcp_bye_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t* data);
void rtcp_app_unpack(rtp_session_t *session, rtcp_hdr_t *header, const uint8_t* data);


#ifdef __cplusplus
}
#endif

#endif /* !_RTCP_PACK_H_ */
