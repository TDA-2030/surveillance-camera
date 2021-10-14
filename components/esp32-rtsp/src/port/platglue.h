#pragma once


#if !__linux
#include "port-esp32.h"
#include "esp_log.h"
#include "esp_pthread.h"
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#else
#include "port-posix.h"
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#endif
