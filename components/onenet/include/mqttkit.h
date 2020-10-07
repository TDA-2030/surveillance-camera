#ifndef _MQTTKIT_H_
#define _MQTTKIT_H_



//=============================配置==============================
//===========可以提供RTOS的内存管理方案，也可以使用C库的=========
#include "freertos/FreeRTOS.h"
#include "esp_system.h"


#define ROUND_UP(x, align) (((int) (x) + (align - 1)) & ~(align - 1))
#define ROUND_DOWN(x, align) ((int)(x) & ~(align - 1))


#if (0)
#define MQTT_MallocBuffer	pvPortMalloc
#define MQTT_FreeBuffer		vPortFree
#else
#define MQTT_MallocBuffer	malloc
#define MQTT_FreeBuffer		free
#endif



//==========================================================


#define MOSQ_MSB(A)         (uint8_t)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A)         (uint8_t)(A & 0x00FF)


/*--------------------------------内存分配方案标志--------------------------------*/
#define MEM_FLAG_NULL		0
#define MEM_FLAG_ALLOC		1
#define MEM_FLAG_STATIC		2


typedef struct Buffer
{
	
	uint8_t	*_data;		//协议数据
	
	uint32_t	_len;		//写入的数据长度
	
	uint32_t	_size;		//缓存总大小
	
	uint8_t	_memFlag;	//内存使用的方案：0-未分配	1-使用的动态分配		2-使用的固定内存
	
} MQTT_PACKET_STRUCTURE;


/*--------------------------------固定头部消息类型--------------------------------*/
enum MqttPacketType
{
	
    MQTT_PKT_CONNECT = 1, /**< 连接请求数据包 */
    MQTT_PKT_CONNACK,     /**< 连接确认数据包 */
    MQTT_PKT_PUBLISH,     /**< 发布数据数据包 */
    MQTT_PKT_PUBACK,      /**< 发布确认数据包 */
    MQTT_PKT_PUBREC,      /**< 发布数据已接收数据包，Qos 2时，回复MQTT_PKT_PUBLISH */
    MQTT_PKT_PUBREL,      /**< 发布数据释放数据包， Qos 2时，回复MQTT_PKT_PUBREC */
    MQTT_PKT_PUBCOMP,     /**< 发布完成数据包， Qos 2时，回复MQTT_PKT_PUBREL */
    MQTT_PKT_SUBSCRIBE,   /**< 订阅数据包 */
    MQTT_PKT_SUBACK,      /**< 订阅确认数据包 */
    MQTT_PKT_UNSUBSCRIBE, /**< 取消订阅数据包 */
    MQTT_PKT_UNSUBACK,    /**< 取消订阅确认数据包 */
    MQTT_PKT_PINGREQ,     /**< ping 数据包 */
    MQTT_PKT_PINGRESP,    /**< ping 响应数据包 */
    MQTT_PKT_DISCONNECT,  /**< 断开连接数据包 */
	
	//新增
	
	MQTT_PKT_CMD  		 /**< 命令下发数据包 */
	
};


/*--------------------------------MQTT QOS等级--------------------------------*/
enum MqttQosLevel
{
	
    MQTT_QOS_LEVEL0,  /**< 最多发送一次 */
    MQTT_QOS_LEVEL1,  /**< 最少发送一次  */
    MQTT_QOS_LEVEL2   /**< 只发送一次 */
	
};


/*--------------------------------MQTT 连接请求标志位，内部使用--------------------------------*/
enum MqttConnectFlag
{
	
    MQTT_CONNECT_CLEAN_SESSION  = 0x02,
    MQTT_CONNECT_WILL_FLAG      = 0x04,
    MQTT_CONNECT_WILL_QOS0      = 0x00,
    MQTT_CONNECT_WILL_QOS1      = 0x08,
    MQTT_CONNECT_WILL_QOS2      = 0x10,
    MQTT_CONNECT_WILL_RETAIN    = 0x20,
    MQTT_CONNECT_PASSORD        = 0x40,
    MQTT_CONNECT_USER_NAME      = 0x80
	
};


/*--------------------------------消息的packet ID，可自定义--------------------------------*/
#define MQTT_PUBLISH_ID			10

#define MQTT_SUBSCRIBE_ID		20

#define MQTT_UNSUBSCRIBE_ID		30


/*--------------------------------删包--------------------------------*/
void MQTT_DeleteBuffer(MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------解包--------------------------------*/
uint8_t MQTT_UnPacketRecv(uint8_t *dataPtr);

/*--------------------------------登录组包--------------------------------*/
uint8_t MQTT_PacketConnect(const char *user, const char *password, const char *devid,
						uint16_t cTime, _Bool clean_session, _Bool qos,
						const char *will_topic, const char *will_msg, int32_t will_retain,
						MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------断开连接组包--------------------------------*/
_Bool MQTT_PacketDisConnect(MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------连接响应解包--------------------------------*/
uint8_t MQTT_UnPacketConnectAck(uint8_t *rev_data);

/*--------------------------------数据点上传组包--------------------------------*/
_Bool MQTT_PacketSaveData(const char *devid, uint32_t send_len, char *type_bin_head, uint8_t type, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------二进制文件上传组包--------------------------------*/
_Bool MQTT_PacketSaveBinData(const char *name, uint32_t file_len, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------命令下发解包--------------------------------*/
uint8_t MQTT_UnPacketCmd(uint8_t *rev_data, char **cmdid, char **req, uint16_t *req_len);

/*--------------------------------命令回复组包--------------------------------*/
_Bool MQTT_PacketCmdResp(const char *cmdid, const char *req, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------订阅主题组包--------------------------------*/
uint8_t MQTT_PacketSubscribe(uint16_t pkt_id, enum MqttQosLevel qos, const char *topics[], uint8_t topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------订阅主题回复解包--------------------------------*/
uint8_t MQTT_UnPacketSubscribe(uint8_t *rev_data);

/*--------------------------------取消订阅组包--------------------------------*/
uint8_t MQTT_PacketUnSubscribe(uint16_t pkt_id, const char *topics[], uint8_t topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------取消订阅回复解包--------------------------------*/
_Bool MQTT_UnPacketUnSubscribe(uint8_t *rev_data);

/*--------------------------------发布主题组包--------------------------------*/
uint8_t MQTT_PacketPublish(uint16_t pkt_id, const char *topic,
						const char *payload, uint32_t payload_len,
						enum MqttQosLevel qos, int32_t retain, int32_t own,
						MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------发布消息回复解包--------------------------------*/
uint8_t MQTT_UnPacketPublish(uint8_t *rev_data, char **topic, uint16_t *topic_len, char **payload, uint16_t *payload_len, uint8_t *qos, uint16_t *pkt_id);

/*--------------------------------发布消息的Ack组包--------------------------------*/
_Bool MQTT_PacketPublishAck(uint16_t pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------发布消息的Ack解包--------------------------------*/
_Bool MQTT_UnPacketPublishAck(uint8_t *rev_data);

/*--------------------------------发布消息的Rec组包--------------------------------*/
_Bool MQTT_PacketPublishRec(uint16_t pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------发布消息的Rec解包--------------------------------*/
_Bool MQTT_UnPacketPublishRec(uint8_t *rev_data);

/*--------------------------------发布消息的Rel组包--------------------------------*/
_Bool MQTT_PacketPublishRel(uint16_t pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------发布消息的Rel解包--------------------------------*/
_Bool MQTT_UnPacketPublishRel(uint8_t *rev_data, uint16_t pkt_id);

/*--------------------------------发布消息的Comp组包--------------------------------*/
_Bool MQTT_PacketPublishComp(uint16_t pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);

/*--------------------------------发布消息的Comp解包--------------------------------*/
_Bool MQTT_UnPacketPublishComp(uint8_t *rev_data);

/*--------------------------------心跳请求组包--------------------------------*/
_Bool MQTT_PacketPing(MQTT_PACKET_STRUCTURE *mqttPacket);


#endif
