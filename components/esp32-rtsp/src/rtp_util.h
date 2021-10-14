
#ifndef _RTP_UTIL_H_
#define _RTP_UTIL_H_

#include <stdint.h>

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

// The Internet Protocol defines big-endian as the standard network byte order
// nbo (network byte order)
#define nbo_r16 rtp_read_uint16
#define nbo_r32 rtp_read_uint32
#define nbo_w16 rtp_write_uint16
#define nbo_w32 rtp_write_uint32

//网络协议使用大端读16bit
static inline uint16_t rtp_read_uint16(const uint8_t* ptr)
{
	return (((uint16_t)ptr[0]) << 8) | ptr[1];
}

//网络协议使用大端读32bit
static inline uint32_t rtp_read_uint32(const uint8_t* ptr)
{
	return (((uint32_t)ptr[0]) << 24) | (((uint32_t)ptr[1]) << 16) | (((uint32_t)ptr[2]) << 8) | ptr[3];
}

//写16bit
static inline void rtp_write_uint16(uint8_t* ptr, uint16_t val)
{
	ptr[0] = (uint8_t)(val >> 8);
	ptr[1] = (uint8_t)val;
}

//写32bit
static inline void rtp_write_uint32(uint8_t* ptr, uint32_t val)
{
	ptr[0] = (uint8_t)(val >> 24);
	ptr[1] = (uint8_t)(val >> 16);
	ptr[2] = (uint8_t)(val >> 8);
	ptr[3] = (uint8_t)val;
}

static inline uint64_t rtp_wallclock2timestamp(uint64_t t_us, uint32_t clock_rate_hz)
{
	return (t_us*clock_rate_hz/1000000);
}

void mem_swap32(uint8_t *in, uint32_t length);
uint8_t *nbo_mem_copy(uint8_t *out, const uint8_t *in, uint32_t length);
uint64_t rtp_time_now_us(void);
uint64_t clock2ntp(uint64_t clock);
uint64_t ntp2clock(uint64_t ntp);


#endif /* !_RTP_UTIL_H_ */
