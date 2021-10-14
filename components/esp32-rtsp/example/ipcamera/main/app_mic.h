#ifndef _APP_MICROPHONE_H_
#define _APP_MICROPHONE_H_

#include "esp_system.h"
#include "driver/i2s.h"

#ifdef __cplusplus
extern "C" {
#endif

void *mic_init(i2s_port_t i2s_port, int rate);
void mic_deinit(void *handle);
void mic_get_data(void *handle, uint8_t *in_buffer, size_t size, size_t *bytes_read);

#ifdef __cplusplus
}
#endif

#endif

