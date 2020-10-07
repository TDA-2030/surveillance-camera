/*

  */
#ifndef _APP_MAIN_H_
#define _APP_MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
    SYS_STATUS_SMARTCONFIG       = 0x0004,
    SYS_STATUS_CONNECTED         = 0x0008,

}SysStatus_t;
extern SysStatus_t SystemStatus;
#define SYS_STATUS_SET(status) SystemStatus|=(status)
#define SYS_STATUS_CLR(status) SystemStatus&=~(status)
#define SYS_STATUS_GET(status) (SystemStatus&(status))

#ifdef __cplusplus
}
#endif

#endif
