#ifndef __MJPEG_H
#define __MJPEG_H 

#include "stdio.h"
#include "stdint.h"
#include <cdjpeg.h> 
// #include <sys.h> 
#include <setjmp.h>

#ifdef __cplusplus 
extern "C" {
#endif

struct my_error_mgr {
  struct jpeg_error_mgr pub;	
  jmp_buf setjmp_buffer;		//for return to caller 
}; 
typedef struct my_error_mgr * my_error_ptr;

uint8_t mjpegdec_init(uint16_t offx,uint16_t offy);
void mjpegdec_free(void);
uint8_t mjpegdec_decode(uint8_t* buf,uint32_t bsize);

void mjpegdraw(uint8_t *mjpegbuffer,uint32_t size);

#ifdef __cplusplus 
}
#endif

#endif

