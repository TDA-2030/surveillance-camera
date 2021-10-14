#ifndef _RTCP_H_ 
#define _RTCP_H_

#include "rtp.h"

#ifdef __cplusplus
extern "C" {
#endif

int rtcp_send(rtp_session_t *session , uint8_t *data, uint32_t len);
int rtcp_parse(rtp_session_t *session, const uint8_t *buffer, uint32_t len);

uint16_t rtcp_get_member_num(rtp_session_t *session);
uint16_t rtcp_get_sender_num(rtp_session_t *session);

#ifdef __cplusplus
}
#endif

#endif /* !_RTCP_H_ */
