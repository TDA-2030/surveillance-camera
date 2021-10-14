#ifndef _APP_USB_CAMERA_H_
#define _APP_USB_CAMERA_H_

#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t usb_camera_init(void);

camera_fb_t* usb_camera_fb_get();

void usb_camera_fb_return(camera_fb_t * fb);


#ifdef __cplusplus
}
#endif

#endif

