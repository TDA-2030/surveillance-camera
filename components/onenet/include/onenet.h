#ifndef _ONENET_H_
#define _ONENET_H_


#include "dStream.h"



typedef enum
{
	ONENET_NET_STATUS_CONNECT_NONE,
	ONENET_NET_STATUS_CONNECT_IP,
	ONENET_NET_STATUS_CONNECT_PLATFORM,

}ONENET_NET_STATUS_ENUM;



typedef enum{
    ONENET_PROTOCOL_NONE,
    ONENET_PROTOCOL_EDP,
    ONENET_PROTOCOL_NWX,
    ONENET_PROTOCOL_JTEXT,
    ONENET_PROTOCOL_HISCMD,
    ONENET_PROTOCOL_JT808,
    ONENET_PROTOCOL_MODBUS,
    ONENET_PROTOCOL_MQTT,
    ONENET_PROTOCOL_GR20,
    ONENET_PROTOCOL_REG,
    ONENET_PROTOCOL_HTTP,
}ONENET_PROTOCOL_ENUM;


typedef struct
{

    char dev_id[16];
    char api_key[32];
	
	char pro_id[10];
	char auif[50];
	
	char reg_code[24];
	
	char ip[16];
	char port[8];
	
	
	const ONENET_PROTOCOL_ENUM protocol;	//协议类型号		1-edp	2-nwx	3-jtext		4-Hiscmd
									//				5-jt808			6-modbus	7-mqtt
									//				8-gr20			9-reg		10-HTTP(自定义)
	uint8_t error_cnt;      //错误计数
	uint32_t send_time;     //记录发送包的时间
	uint8_t send_handled;   //发送结果是否已经处理，配合send_time使用
	ONENET_NET_STATUS_ENUM status;

} ONETNET_INFO;

extern ONETNET_INFO onenet_info;


#define SEND_TYPE_OK			0	//
#define SEND_TYPE_DATA			1	//
#define SEND_TYPE_HEART			2	//
#define SEND_TYPE_PUBLISH		3	//
#define SEND_TYPE_SUBSCRIBE		4	//
#define SEND_TYPE_UNSUBSCRIBE	5	//
#define SEND_TYPE_BINFILE		6	//


_Bool OneNET_RepetitionCreateFlag(const char *apikey);

_Bool OneNET_CreateDevice(const char *reg_code, const char *dev_name, const char *auth_info, char *devid, char *apikey);

_Bool OneNET_GetLinkIP(ONENET_PROTOCOL_ENUM protocol, char *ip, char *port);

_Bool OneNET_DevConnect(const char* devid, const char *proid, const char* auth_info);

_Bool OneNET_DevDisConnect(void);

unsigned char OneNET_SendData(FORMAT_TYPE type, char *devid, char *apikey, DATA_STREAM *streamArray, unsigned short streamArrayCnt);

unsigned char OneNET_Send_BinFile(char *name, const unsigned char *file, unsigned int file_size);

unsigned char OneNET_Subscribe(const char *topics[], unsigned char topic_cnt);

unsigned char OneNET_UnSubscribe(const char *topics[], unsigned char topic_cnt);

unsigned char OneNET_Publish(const char *topic, const char *msg);

unsigned char OneNET_SendData_Heart(void);

_Bool OneNET_Check_Heart(void);

void OneNET_CmdHandle(void);

void OneNET_RevPro(void);

#endif
