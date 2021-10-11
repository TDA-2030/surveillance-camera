#ifndef _AVI_RECORDER_H_
#define _AVI_RECORDER_H_

#include "esp_log.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

void avi_recorder_start(const char *fname,
                        int (*get_frame)(void **buf, size_t *len, int *w, int *h),
                        int (*return_frame)(void *buf),
                        uint32_t rec_time,
                        bool block);

void avi_recorder_stop(void);


#ifdef __cplusplus
}
#endif

#endif