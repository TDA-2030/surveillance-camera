/**
    ************************************************************
    ************************************************************
    ************************************************************
    *   文件名：    MqttKit.c
    *
    *   作者：       张继瑞
    *
    *   日期：       2018-04-27
    *
    *   版本：       V1.6
    *
    *   说明：       MQTT协议
    *
    *   修改记录： V1.1：解决MQTT_PacketSubscribe订阅不为2个topic
    *                       个数时协议错误的bug
    *               V1.2：修复MQTT_PacketCmdResp的bug
    *               V1.3：将strncpy替换为memcpy，解决潜在bug
    *               V1.4：修复   MQTT_PacketPublishAck
    *                           MQTT_PacketPublishRel
    *                           函数封包错误的bug
    *               V1.5：增加   MQTT_UnPacketCmd
    *                           MQTT_UnPacketPublish
    *                           接口对消息内容长度的提取参数
    *               V1.6：增加二进制文件上传接口
    ************************************************************
    ************************************************************
    ************************************************************
**/

//协议头文件
#include "mqttkit.h"

//C库
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// #include "esp_system.h"
// #include "esp_log.h"

#define CMD_TOPIC_PREFIX        "$creq"

static const char *TAG = "mqttkit";

//==========================================================
//  函数名称： EDP_NewBuffer
//
//  函数功能： 申请内存
//
//  入口参数： edpPacket：包结构体
//              size：大小
//
//  返回参数： 无
//
//  说明：       1.可使用动态分配来分配内存
//              2.可使用局部或全局数组来指定内存
//==========================================================
void MQTT_NewBuffer(MQTT_PACKET_STRUCTURE *mqttPacket, uint32_t size)
{
    uint32_t i = 0;
    if (mqttPacket->_data == NULL)
    {
        mqttPacket->_memFlag = MEM_FLAG_ALLOC;
        mqttPacket->_data = (uint8_t *)MQTT_MallocBuffer(size);

        if (mqttPacket->_data != NULL)
        {
            mqttPacket->_len = 0;
            mqttPacket->_size = size;
            for (; i < mqttPacket->_size; i++)
                mqttPacket->_data[i] = 0;
        }
    }
    else
    {
        mqttPacket->_memFlag = MEM_FLAG_STATIC;
        for (i = 0; i < mqttPacket->_size; i++)
        {
            mqttPacket->_data[i] = 0;
        }

        mqttPacket->_len = 0;

        if (mqttPacket->_size < size)
        {
            mqttPacket->_data = NULL;
        }
    }
}

//==========================================================
//  函数名称： MQTT_DeleteBuffer
//
//  函数功能： 释放数据内存
//
//  入口参数： edpPacket：包结构体
//
//  返回参数： 无
//
//  说明：
//==========================================================
void MQTT_DeleteBuffer(MQTT_PACKET_STRUCTURE *mqttPacket)
{

    if (mqttPacket->_memFlag == MEM_FLAG_ALLOC)
        MQTT_FreeBuffer(mqttPacket->_data);

    mqttPacket->_data = NULL;
    mqttPacket->_len = 0;
    mqttPacket->_size = 0;
    mqttPacket->_memFlag = MEM_FLAG_NULL;

}

int32_t MQTT_DumpLength(size_t len, uint8_t *buf)
{

    int32_t i = 0;

    for (i = 1; i <= 4; ++i)
    {
        *buf = len % 128;
        len >>= 7;
        if (len > 0)
        {
            *buf |= 128;
            ++buf;
        }
        else
        {
            return i;
        }
    }

    return -1;
}

int32_t MQTT_ReadLength(const uint8_t *stream, int32_t size, uint32_t *len)
{

    int32_t i;
    const uint8_t *in = stream;
    uint32_t multiplier = 1;

    *len = 0;
    for (i = 0; i < size; ++i)
    {
        *len += (in[i] & 0x7f) * multiplier;

        if (!(in[i] & 0x80))
        {
            return i + 1;
        }

        multiplier <<= 7;
        if (multiplier >= 2097152)      //128 * *128 * *128
        {
            return -2;                  // error, out of range
        }
    }

    return -1;                          // not complete

}

//==========================================================
//  函数名称： MQTT_UnPacketRecv
//
//  函数功能： MQTT数据接收类型判断
//
//  入口参数： dataPtr：接收的数据指针
//
//  返回参数： 0-成功        255-失败
//
//  说明：
//==========================================================
uint8_t MQTT_UnPacketRecv(uint8_t *dataPtr)
{

    uint8_t status = 255;
    uint8_t type = dataPtr[0] >> 4;             //类型检查

    if (type < 1 || type > 14)
        return status;

    if (type == MQTT_PKT_PUBLISH)
    {
        uint8_t *msgPtr;
        uint32_t remain_len = 0;

        msgPtr = dataPtr + MQTT_ReadLength(dataPtr + 1, 4, &remain_len) + 1;

        if (remain_len < 2 || dataPtr[0] & 0x01)                //retain
            return 255;

        if (remain_len < ((uint16_t)msgPtr[0] << 8 | msgPtr[1]) + 2)
            return 255;

        if (strstr((char *)msgPtr + 2, CMD_TOPIC_PREFIX) != NULL)   //如果是命令下发
            status = MQTT_PKT_CMD;
        else
            status = MQTT_PKT_PUBLISH;
    }
    else
        status = type;

    return status;

}

//==========================================================
//  函数名称： MQTT_PacketConnect
//
//  函数功能： 连接消息组包
//
//  入口参数： user：用户名：产品ID
//              password：密码：鉴权信息或apikey
//              devid：设备ID
//              cTime：连接保持时间
//              clean_session：离线消息清除标志
//              qos：重发标志
//              will_topic：异常离线topic
//              will_msg：异常离线消息
//              will_retain：消息推送标志
//              mqttPacket：包指针
//
//  返回参数： 0-成功        其他-失败
//
//  说明：
//==========================================================
uint8_t MQTT_PacketConnect(const char *user, const char *password, const char *devid,
                           uint16_t cTime, _Bool clean_session, _Bool qos,
                           const char *will_topic, const char *will_msg, int32_t will_retain,
                           MQTT_PACKET_STRUCTURE *mqttPacket)
{

    uint8_t flags = 0;
    uint8_t will_topic_len = 0;
    uint16_t total_len = 15;
    int16_t len = 0, devid_len = strlen(devid);

    if (!devid)
        return 1;

    total_len += devid_len + 2;

    //断线后，是否清理离线消息：1-清理   0-不清理--------------------------------------------
    if (clean_session)
    {
        flags |= MQTT_CONNECT_CLEAN_SESSION;
    }

    //异常掉线情况下，服务器发布的topic------------------------------------------------------
    if (will_topic)
    {
        flags |= MQTT_CONNECT_WILL_FLAG;
        will_topic_len = strlen(will_topic);
        total_len += 4 + will_topic_len + strlen(will_msg);
    }

    //qos级别--主要用于PUBLISH（发布态）消息的，保证消息传递的次数-----------------------------
    switch ((unsigned char)qos)
    {
    case MQTT_QOS_LEVEL0:
        flags |= MQTT_CONNECT_WILL_QOS0;                            //最多一次
        break;

    case MQTT_QOS_LEVEL1:
        flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_QOS1); //最少一次
        break;

    case MQTT_QOS_LEVEL2:
        flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_QOS2); //只有一次
        break;

    default:
        return 2;
    }

    //主要用于PUBLISH(发布态)的消息，表示服务器要保留这次推送的信息，如果有新的订阅者出现，就把这消息推送给它。如果不设那么推送至当前订阅的就释放了
    if (will_retain)
    {
        flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_RETAIN);
    }

    //账号为空 密码为空---------------------------------------------------------------------
    if (!user || !password)
    {
        return 3;
    }
    flags |= MQTT_CONNECT_USER_NAME | MQTT_CONNECT_PASSORD;

    total_len += strlen(user) + strlen(password) + 4;

    //分配内存-----------------------------------------------------------------------------
    MQTT_NewBuffer(mqttPacket, total_len);
    if (mqttPacket->_data == NULL)
        return 4;

    memset(mqttPacket->_data, 0, total_len);

    /*************************************固定头部***********************************************/

    //固定头部----------------------连接请求类型---------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_CONNECT << 4;

    //固定头部----------------------剩余长度值-----------------------------------------------
    len = MQTT_DumpLength(total_len - 5, mqttPacket->_data + mqttPacket->_len);
    if (len < 0)
    {
        MQTT_DeleteBuffer(mqttPacket);
        return 5;
    }
    else
        mqttPacket->_len += len;

    /*************************************可变头部***********************************************/

    //可变头部----------------------协议名长度 和 协议名--------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = 0;
    mqttPacket->_data[mqttPacket->_len++] = 4;
    mqttPacket->_data[mqttPacket->_len++] = 'M';
    mqttPacket->_data[mqttPacket->_len++] = 'Q';
    mqttPacket->_data[mqttPacket->_len++] = 'T';
    mqttPacket->_data[mqttPacket->_len++] = 'T';

    //可变头部----------------------protocol level 4-----------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = 4;

    //可变头部----------------------连接标志(该函数开头处理的数据)-----------------------------
    mqttPacket->_data[mqttPacket->_len++] = flags;

    //可变头部----------------------保持连接的时间(秒)----------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(cTime);
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(cTime);

    /*************************************消息体************************************************/

    //消息体----------------------------devid长度、devid-------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(devid_len);
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(devid_len);

    strncat((char *)mqttPacket->_data + mqttPacket->_len, devid, devid_len);
    mqttPacket->_len += devid_len;

    //消息体----------------------------will_flag 和 will_msg---------------------------------
    if (flags & MQTT_CONNECT_WILL_FLAG)
    {
        unsigned short mLen = 0;

        if (!will_msg)
            will_msg = "";

        mLen = strlen(will_topic);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(mLen);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(mLen);
        strncat((char *)mqttPacket->_data + mqttPacket->_len, will_topic, mLen);
        mqttPacket->_len += mLen;

        mLen = strlen(will_msg);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(mLen);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(mLen);
        strncat((char *)mqttPacket->_data + mqttPacket->_len, will_msg, mLen);
        mqttPacket->_len += mLen;
    }

    //消息体----------------------------use---------------------------------------------------
    if (flags & MQTT_CONNECT_USER_NAME)
    {
        unsigned short user_len = strlen(user);

        mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(user_len);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(user_len);
        strncat((char *)mqttPacket->_data + mqttPacket->_len, user, user_len);
        mqttPacket->_len += user_len;
    }

    //消息体----------------------------password----------------------------------------------
    if (flags & MQTT_CONNECT_PASSORD)
    {
        unsigned short psw_len = strlen(password);

        mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(psw_len);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(psw_len);
        strncat((char *)mqttPacket->_data + mqttPacket->_len, password, psw_len);
        mqttPacket->_len += psw_len;
    }

    return 0;

}

//==========================================================
//  函数名称： MQTT_PacketDisConnect
//
//  函数功能： 断开连接消息组包
//
//  入口参数： mqttPacket：包指针
//
//  返回参数： 0-成功        1-失败
//
//  说明：
//==========================================================
_Bool MQTT_PacketDisConnect(MQTT_PACKET_STRUCTURE *mqttPacket)
{

    MQTT_NewBuffer(mqttPacket, 2);
    if (mqttPacket->_data == NULL)
        return 1;

    /*************************************固定头部***********************************************/

    //固定头部----------------------头部消息-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_DISCONNECT << 4;

    //固定头部----------------------剩余长度值-----------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = 0;

    return 0;

}

//==========================================================
//  函数名称： MQTT_UnPacketConnectAck
//
//  函数功能： 连接消息解包
//
//  入口参数： rev_data：接收的数据
//
//  返回参数： 1、255-失败      其他-平台的返回码
//
//  说明：
//==========================================================
uint8_t MQTT_UnPacketConnectAck(uint8_t *rev_data)
{

    if (rev_data[1] != 2)
        return 1;

    if (rev_data[2] == 0 || rev_data[2] == 1)
        return rev_data[3];
    else
        return 255;

}

//==========================================================
//  函数名称： MQTT_PacketSaveData
//
//  函数功能： 数据点上传组包
//
//  入口参数： devid：设备ID(可为空)
//              send_buf：json缓存buf
//              send_len：json总长
//              type_bin_head：bin文件的消息头
//              type：类型
//
//  返回参数： 0-成功        1-失败
//
//  说明：
//==========================================================
_Bool MQTT_PacketSaveData(const char *devid, uint32_t send_len, char *type_bin_head, uint8_t type, MQTT_PACKET_STRUCTURE *mqttPacket)
{

    if (MQTT_PacketPublish(MQTT_PUBLISH_ID, "$dp", NULL, send_len + 3, MQTT_QOS_LEVEL1, 0, 1, mqttPacket) == 0)
    {
        mqttPacket->_data[mqttPacket->_len++] = type;                   //类型

        mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(send_len);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(send_len);
    }
    else
        return 1;

    return 0;

}

//==========================================================
//  函数名称： MQTT_PacketSaveBinData
//
//  函数功能： 为禁止文件上传组包
//
//  入口参数： name：数据流名字
//              file_len：文件长度
//              mqttPacket：包指针
//
//  返回参数： 0-成功        1-失败
//
//  说明：
//==========================================================
_Bool MQTT_PacketSaveBinData(const char *name, uint32_t file_len, MQTT_PACKET_STRUCTURE *mqttPacket)
{

    _Bool result = 1;
    char *bin_head = NULL;
    uint8_t bin_head_len = 0;
    char *payload = NULL;
    uint32_t payload_size = 0;

    bin_head = (char *)MQTT_MallocBuffer(13 + strlen(name));
    if (bin_head == NULL)
        return result;

    sprintf(bin_head, "{\"ds_id\":\"%s\"}", name);

    bin_head_len = strlen(bin_head);
    payload_size = 7 + bin_head_len + file_len;

    payload = (char *)MQTT_MallocBuffer(payload_size - file_len);
    if (payload == NULL)
    {
        MQTT_FreeBuffer(bin_head);

        return result;
    }

    payload[0] = 2;                     //类型

    payload[1] = MOSQ_MSB(bin_head_len);
    payload[2] = MOSQ_LSB(bin_head_len);

    memcpy(payload + 3, bin_head, bin_head_len);

    payload[bin_head_len + 3] = (file_len >> 24) & 0xFF;
    payload[bin_head_len + 4] = (file_len >> 16) & 0xFF;
    payload[bin_head_len + 5] = (file_len >> 8) & 0xFF;
    payload[bin_head_len + 6] = file_len & 0xFF;

    uint8_t r = MQTT_PacketPublish(MQTT_PUBLISH_ID, "$dp", payload, payload_size, MQTT_QOS_LEVEL1, 0, 1, mqttPacket);
    if (r == 0)
    {
        result = 0;
    }else
    {
        
    }
    
    MQTT_FreeBuffer(bin_head);
    MQTT_FreeBuffer(payload);

    return result;

}

//==========================================================
//  函数名称： MQTT_UnPacketCmd
//
//  函数功能： 命令下发解包
//
//  入口参数： rev_data：接收的数据指针
//              cmdid：cmdid-uuid
//              req：命令
//
//  返回参数： 0-成功        其他-失败原因
//
//  说明：
//==========================================================
uint8_t MQTT_UnPacketCmd(uint8_t *rev_data, char **cmdid, char **req, uint16_t *req_len)
{

    char *dataPtr = strchr((char *)rev_data + 6, '/');  //加6是跳过头信息

    uint32_t remain_len = 0;

    if (dataPtr == NULL)                                //未找到'/'
        return 1;
    dataPtr++;                                          //跳过'/'

    MQTT_ReadLength(rev_data + 1, 4, &remain_len);      //读取剩余字节

    *cmdid = (char *)MQTT_MallocBuffer(37);             //cmdid固定36字节，多分配一个结束符的位置
    if (*cmdid == NULL)
        return 2;

    memset(*cmdid, 0, 37);                              //全部清零
    memcpy(*cmdid, (const char *)dataPtr, 36);          //复制cmdid
    dataPtr += 36;

    *req_len = remain_len - 44;                         //命令长度 = 剩余长度(remain_len) - 2 - 5($creq) - 1(\) - cmdid长度
    *req = (char *)MQTT_MallocBuffer(*req_len + 1);     //分配命令长度+1
    if (*req == NULL)
    {
        MQTT_FreeBuffer(*cmdid);
        return 3;
    }

    memset(*req, 0, *req_len + 1);                      //清零
    memcpy(*req, (const char *)dataPtr, *req_len);      //复制命令

    return 0;

}

//==========================================================
//  函数名称： MQTT_PacketCmdResp
//
//  函数功能： 命令回复组包
//
//  入口参数： cmdid：cmdid
//              req：命令
//              mqttPacket：包指针
//
//  返回参数： 0-成功        1-失败
//
//  说明：
//==========================================================
_Bool MQTT_PacketCmdResp(const char *cmdid, const char *req, MQTT_PACKET_STRUCTURE *mqttPacket)
{

    uint16_t cmdid_len = strlen(cmdid);
    uint16_t req_len = strlen(req);
    _Bool status = 0;

    char *payload = (char *)MQTT_MallocBuffer(cmdid_len + 6);
    if (payload == NULL)
        return 1;

    memset(payload, 0, cmdid_len + 6);
    memcpy(payload, "$crsp/", 6);
    strncat(payload, cmdid, cmdid_len);

    if (MQTT_PacketPublish(MQTT_PUBLISH_ID, payload, req, strlen(req), MQTT_QOS_LEVEL0, 0, 1, mqttPacket) == 0)
        status = 0;
    else
        status = 1;

    MQTT_FreeBuffer(payload);

    return status;

}

//==========================================================
//  函数名称： MQTT_PacketSubscribe
//
//  函数功能： Subscribe消息组包
//
//  入口参数： pkt_id：pkt_id
//              qos：消息重发次数
//              topics：订阅的消息
//              topics_cnt：订阅的消息个数
//              mqttPacket：包指针
//
//  返回参数： 0-成功        其他-失败
//
//  说明：
//==========================================================
uint8_t MQTT_PacketSubscribe(uint16_t pkt_id, enum MqttQosLevel qos, const char *topics[], uint8_t topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket)
{

    uint32_t topic_len = 0, remain_len = 0;
    int16_t len = 0;
    uint8_t i = 0;

    if (pkt_id == 0)
        return 1;

    //计算topic长度-------------------------------------------------------------------------
    for (; i < topics_cnt; i++)
    {
        if (topics[i] == NULL)
            return 2;

        topic_len += strlen(topics[i]);
    }

    //2 bytes packet id + topic filter(2 bytes topic + topic length + 1 byte reserve)------
    remain_len = 2 + 3 * topics_cnt + topic_len;

    //分配内存------------------------------------------------------------------------------
    MQTT_NewBuffer(mqttPacket, remain_len + 5);
    if (mqttPacket->_data == NULL)
        return 3;

    /*************************************固定头部***********************************************/

    //固定头部----------------------头部消息-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_SUBSCRIBE << 4 | 0x02;

    //固定头部----------------------剩余长度值-----------------------------------------------
    len = MQTT_DumpLength(remain_len, mqttPacket->_data + mqttPacket->_len);
    if (len < 0)
    {
        MQTT_DeleteBuffer(mqttPacket);
        return 4;
    }
    else
        mqttPacket->_len += len;

    /*************************************payload***********************************************/

    //payload----------------------pkt_id---------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);

    //payload----------------------topic_name-----------------------------------------------
    for (i = 0; i < topics_cnt; i++)
    {
        topic_len = strlen(topics[i]);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(topic_len);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(topic_len);

        strncat((char *)mqttPacket->_data + mqttPacket->_len, topics[i], topic_len);
        mqttPacket->_len += topic_len;

        mqttPacket->_data[mqttPacket->_len++] = qos & 0xFF;
    }

    return 0;

}

//==========================================================
//  函数名称： MQTT_UnPacketSubscrebe
//
//  函数功能： Subscribe的回复消息解包
//
//  入口参数： rev_data：接收到的信息
//
//  返回参数： 0-成功        其他-失败
//
//  说明：
//==========================================================
uint8_t MQTT_UnPacketSubscribe(uint8_t *rev_data)
{

    uint8_t result = 255;

    if (rev_data[2] == MOSQ_MSB(MQTT_SUBSCRIBE_ID) && rev_data[3] == MOSQ_LSB(MQTT_SUBSCRIBE_ID))
    {
        switch (rev_data[4])
        {
        case 0x00:
        case 0x01:
        case 0x02:
            //MQTT Subscribe OK
            result = 0;
            break;

        case 0x80:
            //MQTT Subscribe Failed
            result = 1;
            break;

        default:
            //MQTT Subscribe UnKnown Err
            result = 2;
            break;
        }
    }

    return result;

}

//==========================================================
//  函数名称： MQTT_PacketUnSubscribe
//
//  函数功能： UnSubscribe消息组包
//
//  入口参数： pkt_id：pkt_id
//              qos：消息重发次数
//              topics：订阅的消息
//              topics_cnt：订阅的消息个数
//              mqttPacket：包指针
//
//  返回参数： 0-成功        其他-失败
//
//  说明：
//==========================================================
uint8_t MQTT_PacketUnSubscribe(uint16_t pkt_id, const char *topics[], uint8_t topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket)
{

    uint32_t topic_len = 0, remain_len = 0;
    int16_t len = 0;
    uint8_t i = 0;

    if (pkt_id == 0)
        return 1;

    //计算topic长度-------------------------------------------------------------------------
    for (; i < topics_cnt; i++)
    {
        if (topics[i] == NULL)
            return 2;

        topic_len += strlen(topics[i]);
    }

    //2 bytes packet id, 2 bytes topic length + topic + 1 byte reserve---------------------
    remain_len = 2 + (topics_cnt << 1) + topic_len;

    //分配内存------------------------------------------------------------------------------
    MQTT_NewBuffer(mqttPacket, remain_len + 5);
    if (mqttPacket->_data == NULL)
        return 3;

    /*************************************固定头部***********************************************/

    //固定头部----------------------头部消息-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_UNSUBSCRIBE << 4 | 0x02;

    //固定头部----------------------剩余长度值-----------------------------------------------
    len = MQTT_DumpLength(remain_len, mqttPacket->_data + mqttPacket->_len);
    if (len < 0)
    {
        MQTT_DeleteBuffer(mqttPacket);
        return 4;
    }
    else
        mqttPacket->_len += len;

    /*************************************payload***********************************************/

    //payload----------------------pkt_id---------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);

    //payload----------------------topic_name-----------------------------------------------
    for (i = 0; i < topics_cnt; i++)
    {
        topic_len = strlen(topics[i]);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(topic_len);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(topic_len);

        strncat((char *)mqttPacket->_data + mqttPacket->_len, topics[i], topic_len);
        mqttPacket->_len += topic_len;
    }

    return 0;

}

//==========================================================
//  函数名称： MQTT_UnPacketUnSubscribe
//
//  函数功能： UnSubscribe的回复消息解包
//
//  入口参数： rev_data：接收到的信息
//
//  返回参数： 0-成功        其他-失败
//
//  说明：
//==========================================================
_Bool MQTT_UnPacketUnSubscribe(uint8_t *rev_data)
{

    _Bool result = 1;

    if (rev_data[2] == MOSQ_MSB(MQTT_UNSUBSCRIBE_ID) && rev_data[3] == MOSQ_LSB(MQTT_UNSUBSCRIBE_ID))
    {
        result = 0;
    }

    return result;

}

//==========================================================
//  函数名称： MQTT_PacketPublish
//
//  函数功能： Pulish消息组包
//
//  入口参数： pkt_id：pkt_id
//              topic：发布的topic
//              payload：消息体
//              payload_len：消息体长度
//              qos：重发次数
//              retain：离线消息推送
//              own：
//              mqttPacket：包指针
//
//  返回参数： 0-成功        其他-失败
//
//  说明：
//==========================================================
uint8_t MQTT_PacketPublish(uint16_t pkt_id, const char *topic,
                           const char *payload, uint32_t payload_len,
                           enum MqttQosLevel qos, int32_t retain, int32_t own,
                           MQTT_PACKET_STRUCTURE *mqttPacket)
{

    uint32_t total_len = 0, topic_len = 0;
    uint32_t data_len = 0;
    int32_t len = 0;
    uint8_t flags = 0;

    //pkt_id检查----------------------------------------------------------------------------
    if (pkt_id == 0)
        return 1;

    //$dp为系统上传数据点的指令--------------------------------------------------------------
    for (topic_len = 0; topic[topic_len] != '\0'; ++topic_len)
    {
        if ((topic[topic_len] == '#') || (topic[topic_len] == '+'))
            return 2;
    }

    //Publish消息---------------------------------------------------------------------------
    flags |= MQTT_PKT_PUBLISH << 4;

    //retain标志----------------------------------------------------------------------------
    if (retain)
        flags |= 0x01;

    //总长度--------------------------------------------------------------------------------
    total_len = topic_len + payload_len + 2;

    //qos级别--主要用于PUBLISH（发布态）消息的，保证消息传递的次数-----------------------------
    switch (qos)
    {
    case MQTT_QOS_LEVEL0:
        flags |= MQTT_CONNECT_WILL_QOS0;    //最多一次
        break;

    case MQTT_QOS_LEVEL1:
        flags |= 0x02;                      //最少一次
        total_len += 2;
        break;

    case MQTT_QOS_LEVEL2:
        flags |= 0x04;                      //只有一次
        total_len += 2;
        break;

    default:
        return 3;
    }

    //分配内存------------------------------------------------------------------------------
    if (payload[0] == 2)
    {
        uint32_t data_len_t = 0;

        while (payload[data_len_t++] != '}');
        data_len_t -= 3;
        data_len = data_len_t + 7;
        data_len_t = payload_len - data_len;

        MQTT_NewBuffer(mqttPacket, total_len + 3 - data_len_t);

        if (mqttPacket->_data == NULL)
            return 4;

        memset(mqttPacket->_data, 0, total_len + 3 - data_len_t);
    }
    else
    {
        MQTT_NewBuffer(mqttPacket, total_len + 3);

        if (mqttPacket->_data == NULL)
            return 4;

        memset(mqttPacket->_data, 0, total_len + 3);
    }

    /*************************************固定头部***********************************************/

    //固定头部----------------------头部消息-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = flags;

    //固定头部----------------------剩余长度值-----------------------------------------------
    len = MQTT_DumpLength(total_len, mqttPacket->_data + mqttPacket->_len);
    if (len < 0)
    {
        MQTT_DeleteBuffer(mqttPacket);
        return 5;
    }
    else
        mqttPacket->_len += len;

    /*************************************可变头部***********************************************/

    //可变头部----------------------写入topic长度、topic-------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(topic_len);
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(topic_len);

    strncat((char *)mqttPacket->_data + mqttPacket->_len, topic, topic_len);
    mqttPacket->_len += topic_len;
    if (qos != MQTT_QOS_LEVEL0)
    {
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
        mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);
    }

    //可变头部----------------------写入payload----------------------------------------------
    if (payload != NULL)
    {
        if (payload[0] == 2)
        {
            memcpy((char *)mqttPacket->_data + mqttPacket->_len, payload, data_len);
            mqttPacket->_len += data_len;
        }
        else
        {
            memcpy((char *)mqttPacket->_data + mqttPacket->_len, payload, payload_len);
            mqttPacket->_len += payload_len;
        }
    }

    return 0;

}

//==========================================================
//  函数名称： MQTT_UnPacketPublish
//
//  函数功能： Publish消息解包
//
//  入口参数： flags：MQTT相关标志信息
//              pkt：指向可变头部
//              size：固定头部中的剩余长度信息
//
//  返回参数： 0-成功        其他-失败原因
//
//  说明：
//==========================================================
uint8_t MQTT_UnPacketPublish(uint8_t *rev_data, char **topic, uint16_t *topic_len, char **payload, uint16_t *payload_len, uint8_t *qos, uint16_t *pkt_id)
{

    const int8_t flags = rev_data[0] & 0x0F;
    uint8_t *msgPtr;
    uint32_t remain_len = 0;

    const int8_t dup = flags & 0x08;

    *qos = (flags & 0x06) >> 1;

    msgPtr = rev_data + MQTT_ReadLength(rev_data + 1, 4, &remain_len) + 1;

    if (remain_len < 2 || flags & 0x01)                         //retain
        return 255;

    *topic_len = (uint16_t)msgPtr[0] << 8 | msgPtr[1];
    if (remain_len < *topic_len + 2)
        return 255;

    if (strstr((char *)msgPtr + 2, CMD_TOPIC_PREFIX) != NULL)   //如果是命令下发
        return MQTT_PKT_CMD;

    switch (*qos)
    {
    case MQTT_QOS_LEVEL0:                                   // qos0 have no packet identifier

        if (0 != dup)
            return 255;

        *topic = MQTT_MallocBuffer(*topic_len + 1);         //为topic分配内存
        if (*topic == NULL)
            return 255;

        memset(*topic, 0, *topic_len + 1);
        memcpy(*topic, (char *)msgPtr + 2, *topic_len);     //复制数据

        *payload_len = remain_len - 2 - *topic_len;         //为payload分配内存
        *payload = MQTT_MallocBuffer(*payload_len + 1);
        if (*payload == NULL)                               //如果失败
        {
            MQTT_FreeBuffer(*topic);                        //则需要把topic的内存释放掉
            return 255;
        }

        memset(*payload, 0, *payload_len + 1);
        memcpy(*payload, (char *)msgPtr + 2 + *topic_len, *payload_len);

        break;

    case MQTT_QOS_LEVEL1:
    case MQTT_QOS_LEVEL2:

        if (*topic_len + 2 > remain_len)
            return 255;

        *pkt_id = (uint16_t)msgPtr[*topic_len + 2] << 8 | msgPtr[*topic_len + 3];
        if (pkt_id == 0)
            return 255;

        *topic = MQTT_MallocBuffer(*topic_len + 1);         //为topic分配内存
        if (*topic == NULL)
            return 255;

        memset(*topic, 0, *topic_len + 1);
        memcpy(*topic, (int8_t *)msgPtr + 2, *topic_len);       //复制数据

        *payload_len = remain_len - 4 - *topic_len;
        *payload = MQTT_MallocBuffer(*payload_len + 1);     //为payload分配内存
        if (*payload == NULL)                               //如果失败
        {
            MQTT_FreeBuffer(*topic);                        //则需要把topic的内存释放掉
            return 255;
        }

        memset(*payload, 0, *payload_len + 1);
        memcpy(*payload, (int8_t *)msgPtr + 4 + *topic_len, *payload_len);

        break;

    default:
        return 255;
    }

    if (strchr((char *)topic, '+') || strchr((char *)topic, '#'))
        return 255;

    return 0;

}

//==========================================================
//  函数名称： MQTT_PacketPublishAck
//
//  函数功能： Publish Ack消息组包
//
//  入口参数： pkt_id：packet id
//              mqttPacket：包指针
//
//  返回参数： 0-成功        1-失败原因
//
//  说明：       当收到的Publish消息的QoS等级为1时，需要Ack回复
//==========================================================
_Bool MQTT_PacketPublishAck(uint16_t pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{

    MQTT_NewBuffer(mqttPacket, 4);
    if (mqttPacket->_data == NULL)
        return 1;

    /*************************************固定头部***********************************************/

    //固定头部----------------------头部消息-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBACK << 4;

    //固定头部----------------------剩余长度-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = 2;

    /*************************************可变头部***********************************************/

    //可变头部----------------------pkt_id长度-----------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
    mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;

    return 0;

}

//==========================================================
//  函数名称： MQTT_UnPacketPublishAck
//
//  函数功能： Publish Ack消息解包
//
//  入口参数： rev_data：收到的数据
//
//  返回参数： 0-成功        1-失败原因
//
//  说明：
//==========================================================
_Bool MQTT_UnPacketPublishAck(uint8_t *rev_data)
{

    if (rev_data[1] != 2)
        return 1;

    if (rev_data[2] == MOSQ_MSB(MQTT_PUBLISH_ID) && rev_data[3] == MOSQ_LSB(MQTT_PUBLISH_ID))
        return 0;
    else
        return 1;

}

//==========================================================
//  函数名称： MQTT_PacketPublishRec
//
//  函数功能： Publish Rec消息组包
//
//  入口参数： pkt_id：packet id
//              mqttPacket：包指针
//
//  返回参数： 0-成功        1-失败原因
//
//  说明：       当收到的Publish消息的QoS等级为2时，先收到rec
//==========================================================
_Bool MQTT_PacketPublishRec(uint16_t pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{

    MQTT_NewBuffer(mqttPacket, 4);
    if (mqttPacket->_data == NULL)
        return 1;

    /*************************************固定头部***********************************************/

    //固定头部----------------------头部消息-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBREC << 4;

    //固定头部----------------------剩余长度-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = 2;

    /*************************************可变头部***********************************************/

    //可变头部----------------------pkt_id长度-----------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
    mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;

    return 0;

}

//==========================================================
//  函数名称： MQTT_UnPacketPublishRec
//
//  函数功能： Publish Rec消息解包
//
//  入口参数： rev_data：接收到的数据
//
//  返回参数： 0-成功        1-失败
//
//  说明：
//==========================================================
_Bool MQTT_UnPacketPublishRec(uint8_t *rev_data)
{

    if (rev_data[1] != 2)
        return 1;

    if (rev_data[2] == MOSQ_MSB(MQTT_PUBLISH_ID) && rev_data[3] == MOSQ_LSB(MQTT_PUBLISH_ID))
        return 0;
    else
        return 1;

}

//==========================================================
//  函数名称： MQTT_PacketPublishRel
//
//  函数功能： Publish Rel消息组包
//
//  入口参数： pkt_id：packet id
//              mqttPacket：包指针
//
//  返回参数： 0-成功        1-失败原因
//
//  说明：       当收到的Publish消息的QoS等级为2时，先收到rec，再回复rel
//==========================================================
_Bool MQTT_PacketPublishRel(uint16_t pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{

    MQTT_NewBuffer(mqttPacket, 4);
    if (mqttPacket->_data == NULL)
        return 1;

    /*************************************固定头部***********************************************/

    //固定头部----------------------头部消息-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBREL << 4 | 0x02;

    //固定头部----------------------剩余长度-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = 2;

    /*************************************可变头部***********************************************/

    //可变头部----------------------pkt_id长度-----------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
    mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;

    return 0;

}

//==========================================================
//  函数名称： MQTT_UnPacketPublishRel
//
//  函数功能： Publish Rel消息解包
//
//  入口参数： rev_data：接收到的数据
//
//  返回参数： 0-成功        1-失败
//
//  说明：
//==========================================================
_Bool MQTT_UnPacketPublishRel(uint8_t *rev_data, uint16_t pkt_id)
{

    if (rev_data[1] != 2)
        return 1;

    if (rev_data[2] == MOSQ_MSB(pkt_id) && rev_data[3] == MOSQ_LSB(pkt_id))
        return 0;
    else
        return 1;

}

//==========================================================
//  函数名称： MQTT_PacketPublishComp
//
//  函数功能： Publish Comp消息组包
//
//  入口参数： pkt_id：packet id
//              mqttPacket：包指针
//
//  返回参数： 0-成功        1-失败原因
//
//  说明：       当收到的Publish消息的QoS等级为2时，先收到rec，再回复rel
//==========================================================
_Bool MQTT_PacketPublishComp(uint16_t pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{

    MQTT_NewBuffer(mqttPacket, 4);
    if (mqttPacket->_data == NULL)
        return 1;

    /*************************************固定头部***********************************************/

    //固定头部----------------------头部消息-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBCOMP << 4;

    //固定头部----------------------剩余长度-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = 2;

    /*************************************可变头部***********************************************/

    //可变头部----------------------pkt_id长度-----------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
    mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;

    return 0;

}

//==========================================================
//  函数名称： MQTT_UnPacketPublishComp
//
//  函数功能： Publish Comp消息解包
//
//  入口参数： rev_data：接收到的数据
//
//  返回参数： 0-成功        1-失败
//
//  说明：
//==========================================================
_Bool MQTT_UnPacketPublishComp(uint8_t *rev_data)
{

    if (rev_data[1] != 2)
        return 1;

    if (rev_data[2] == MOSQ_MSB(MQTT_PUBLISH_ID) && rev_data[3] == MOSQ_LSB(MQTT_PUBLISH_ID))
        return 0;
    else
        return 1;

}

//==========================================================
//  函数名称： MQTT_PacketPing
//
//  函数功能： 心跳请求组包
//
//  入口参数： mqttPacket：包指针
//
//  返回参数： 0-成功        1-失败
//
//  说明：
//==========================================================
_Bool MQTT_PacketPing(MQTT_PACKET_STRUCTURE *mqttPacket)
{

    MQTT_NewBuffer(mqttPacket, 2);
    if (mqttPacket->_data == NULL)
    {
        return 1;
    }

    /*************************************固定头部***********************************************/

    //固定头部----------------------头部消息-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PINGREQ << 4;

    //固定头部----------------------剩余长度-------------------------------------------------
    mqttPacket->_data[mqttPacket->_len++] = 0;

    return 0;

}
