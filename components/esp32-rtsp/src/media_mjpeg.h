
#ifndef _MEDIA_MJPEG_H_
#define _MEDIA_MJPEG_H_

#include "media_stream.h"

#ifdef __cplusplus
extern "C" {
#endif


#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
typedef struct {
    // little-endian
    uint32_t off : 24;  /* fragment byte offset */
    uint32_t tspec : 8; /* type-specific field */

    uint32_t height : 8; /* frame height in 8 pixel blocks */
    uint32_t width : 8;  /* frame width in 8 pixel blocks */
    uint32_t q : 8;      /* quantization factor (or table id) */
    uint32_t type : 8;   /* id of jpeg decoder params */
} jpeghdr_t;

typedef struct {
    // little-endian
    uint32_t count : 14;
    uint32_t l : 1;
    uint32_t f : 1;
    uint32_t dri : 16;
} jpeghdr_rst_t;

typedef struct {
    // little-endian
    uint32_t length : 16;
    uint32_t precision : 8;
    uint32_t mbz : 8;
} jpeghdr_qtable_t;

#else
typedef struct {
    // big-endian
    uint32_t type : 8;   /* id of jpeg decoder params */
    uint32_t q : 8;      /* quantization factor (or table id) */
    uint32_t width : 8;  /* frame width in 8 pixel blocks */
    uint32_t height : 8; /* frame height in 8 pixel blocks */
    uint32_t tspec : 8; /* type-specific field */
    uint32_t off : 24;  /* fragment byte offset */
} jpeghdr_t;

typedef struct {
    // big-endian
    uint32_t dri : 16;
    uint32_t f : 1;
    uint32_t l : 1;
    uint32_t count : 14;
} jpeghdr_rst_t;

typedef struct {
    // big-endian
    uint32_t mbz : 8;
    uint32_t precision : 8;
    uint32_t length : 16;
} jpeghdr_qtable_t;
#endif

media_stream_t *media_stream_mjpeg_create(void);

#ifdef __cplusplus
}
#endif

#endif
