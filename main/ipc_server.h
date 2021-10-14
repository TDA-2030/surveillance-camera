#ifndef _IPC_SERVER_H_
#define _IPC_SERVER_H_

#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

void ipc_server_start(camera_fb_t *(*func_camera_get)(void),
                      void (*func_camera_return)(camera_fb_t *fb));

#ifdef __cplusplus
}
#endif

#endif

