#ifndef _NET_DEVICE_H_
#define _NET_DEVICE_H_


//==========================================================
typedef enum
{
	NET_DEVICE_STATUS_NONE,
	NET_DEVICE_STATUS_CONNECTED,

}NET_DEVICE_STATUS_ENUM;

struct NET_SEND_LIST
{
	uint16_t dataLen;			//数据长度
	uint8_t *buf;				//数据指针
	struct NET_SEND_LIST *next;		//下一个
};

typedef struct
{
	NET_DEVICE_STATUS_ENUM status;
	struct NET_SEND_LIST *head, *end;
} NET_DEVICE_INFO;

extern NET_DEVICE_INFO net_device_info;

_Bool NET_DEVICE_Close(void);

_Bool NET_DEVICE_Connect(char *type, char *ip, char *port);

_Bool NET_DEVICE_SendData(const uint8_t *data, uint32_t len);

_Bool NET_DEVICE_Read(uint8_t *data, uint32_t *len);
_Bool NET_DEVICE_GetStatus(void);

#endif
