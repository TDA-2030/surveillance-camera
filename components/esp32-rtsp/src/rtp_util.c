#include <stdint.h>
#include <string.h>
#include "port/platglue.h"

static const char *TAG = "rtp-util";

void mem_swap32(uint8_t *in, uint32_t length)
{
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
    if (length % 4) {
        ESP_LOGE(TAG, "length incorrect");
        return;
    }
    uint8_t m, n;
    for (size_t i = 0; i < length; i += 4) {
        m = in[i];
        n = in[i + 1];
        in[i] = in[i + 3];
        in[i + 1] = in[i + 2];
        in[i + 2] = n;
        in[i + 3] = m;
    }
#endif
}

uint8_t *nbo_mem_copy(uint8_t *out, const uint8_t *in, uint32_t length)
{
    if (length % 4) {
        ESP_LOGE(TAG, "length incorrect");
        return out;
    }
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
    for (size_t i = 0; i < length; i += 4) {
        out[i] = in[i + 3];
        out[i + 1] = in[i + 2];
        out[i + 2] = in[i + 1];
        out[i + 3] = in[i];
    }
#else
    memcpy(out, in, length);
#endif
    return out + length;
}

/// same as system_time except ms -> us
/// @return microseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
uint64_t rtp_time_now_us(void)
{
#if defined(OS_WINDOWS)
    uint64_t t;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    t = (uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
    return t / 10 - 11644473600000000; /* Jan 1, 1601 */
#elif defined(OS_MAC)
    uint64_t tick;
    mach_timebase_info_data_t timebase;
    tick = mach_absolute_time();
    mach_timebase_info(&timebase);
    return tick * timebase.numer / timebase.denom / 1000;
#else
    // POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}


/// us(microsecond) -> ntp
uint64_t clock2ntp(uint64_t clock)
{
    uint64_t ntp;

    // high 32 bits in seconds
    ntp = ((clock / 1000000) + 0x83AA7E80) << 32; // 1/1/1970 -> 1/1/1900

    // low 32 bits in picosecond
    // us * 2^32 / 10^6
    // 10^6 = 2^6 * 15625
    // => us * 2^26 / 15625
    ntp |= (uint32_t)(((clock % 1000000) << 26) / 15625);

    return ntp;
}

/// ntp -> us(microsecond)
uint64_t ntp2clock(uint64_t ntp)
{
    uint64_t clock;

    // high 32 bits in seconds
    clock = ((uint64_t)((unsigned int)(ntp >> 32) - 0x83AA7E80)) * 1000000; // 1/1/1900 -> 1/1/1970

    // low 32 bits in picosecond
    clock += ((ntp & 0xFFFFFFFF) * 15625) >> 26;

    return clock;
}

