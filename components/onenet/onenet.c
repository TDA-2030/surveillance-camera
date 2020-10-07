/**
    ************************************************************
    ************************************************************
    ************************************************************
    *   文件名：    onenet.c
    *
    *   作者：       张继瑞
    *
    *   日期：       2017-05-27
    *
    *   版本：       V1.0
    *
    *   说明：       OneNET平台应用示例
    *
    *   修改记录：
    ************************************************************
    ************************************************************
    ************************************************************
**/

#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//网络设备
#include "net_device.h"

//协议文件
#include "onenet.h"
#include "mqttkit.h"


//C库
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "onenet";



//当正式环境注册码达到16个字符则启用自动创建功能，否则不启用
//如果要采用自动创建设备的方式，apikey必须为master-key，且正式环境注册码有效
ONETNET_INFO onenet_info =
{
    .dev_id = CONFIG_ONENET_DEV_ID,
    .api_key = "",  //master apikey
    .pro_id = CONFIG_ONENET_PRODUCT_ID,
    .auif = CONFIG_ONENET_AUTH_INFO,
    .reg_code = "",
    .ip = "183.230.40.39",
    .port = "6002",
    .protocol = ONENET_PROTOCOL_MQTT,
    .error_cnt = 0,
    .status = ONENET_NET_STATUS_CONNECT_NONE,
};

static void (*onnet_cmd_callback)(const char *payload) = NULL;



void RTOS_TimeDly(uint16_t t_ms)
{
    vTaskDelay(t_ms / portTICK_PERIOD_MS);
}

//==========================================================
//  函数名称： OneNET_RepetitionCreateFlag
//
//  函数功能： 允许重复注册设备
//
//  入口参数： apikey：必须是masterkey
//
//  返回参数： 0-成功        1-失败
//
//  说明：       允许重复注册，否则第一次创建成功之后，再次创建会失败
//==========================================================
_Bool OneNET_RepetitionCreateFlag(const char *apikey)
{

    _Bool result = 1;
    // char se---nd_buf[136];
    // uint32_t read_len = 100;

    // if (NET_DEVICE_Connect("TCP", "183.230.40.33", "80") == 0)
    // {
    //     snprintf(send_buf, sizeof(send_buf), "PUT /register_attr HTTP/1.1\r\napi-key:%s\r\nHost:api.heclouds.com\r\n"
    //              "Content-Length:19\r\n\r\n"
    //              "{\"allow_dup\": true}", apikey);

    //     if (!NET_DEVICE_SendData((uint8_t *)send_buf, strlen(send_buf)))
    //     {
    //         NET_DEVICE_Read((uint8_t *)send_buf, &read_len, 5);
    //         if (0 != read_len)
    //         {
    //             if (strstr(send_buf, "succ"))
    //             {
    //                 ESP_LOGI(TAG,  "Tips:  OneNET_RepetitionCreateFlag Ok\r\n");
    //                 result = 0;
    //             }
    //             else if (strstr(send_buf, "auth failed"))
    //             {
    //                 ESP_LOGI(TAG,  "WARN:  当前使用的不是masterkey 或 apikey错误\r\n");
    //             }
    //             else
    //                 ESP_LOGI(TAG,  "Tips:  OneNET_RepetitionCreateFlag Err\r\n");
    //         }
    //         else
    //         {
    //             ESP_LOGI(TAG, "Tips:    OneNET_RepetitionCreateFlag Time Out\r\n");
    //         }
    //     }

    //     NET_DEVICE_Close();
    // }

    return result;
}

//==========================================================
//  函数名称： OneNET_CreateDevice
//
//  函数功能： 在产品中创建一个设备
//
//  入口参数： reg_code：正式环境注册码
//              dev_name：设备名
//              auth_info：鉴权信息
//              devid：保存返回的devid
//              apikey：保存返回的apikey
//
//  返回参数： 0-成功        1-失败
//
//  说明：
//==========================================================
_Bool OneNET_CreateDevice(const char *reg_code, const char *dev_name, const char *auth_info, char *devid, char *apikey)
{

    _Bool result = 1;
    unsigned short send_len = 20 + strlen(dev_name) + strlen(auth_info);
    uint32_t read_len = 500;
    char *send_ptr = NULL, *data_ptr = NULL;


    send_ptr = MQTT_MallocBuffer(send_len + 140);
    if (send_ptr == NULL)
    {
        return result;
    }

    if (NET_DEVICE_Connect("TCP", "183.230.40.33", "80") == 0)
    {
        snprintf(send_ptr, 140 + send_len, "POST /register_de?register_code=%s HTTP/1.1\r\n"
                 "Host: api.heclouds.com\r\n"
                 "Content-Length:%d\r\n\r\n"
                 "{\"sn\":\"%s\",\"title\":\"%s\"}",

                 reg_code, send_len, auth_info, dev_name);

        if (!NET_DEVICE_SendData((uint8_t *)send_ptr, strlen(send_ptr)))
        {
            NET_DEVICE_Read((uint8_t *)send_ptr, &read_len);
            if (0 != read_len)
            {
                data_ptr = strstr(send_ptr, "device_id");

                if (strstr(send_ptr, "auth failed"))
                {
                    ESP_LOGE(TAG, "WARN:  正式环境注册码错误\r\n");
                }
            }

            if (data_ptr)
            {
                if (sscanf(data_ptr, "device_id\":\"%[^\"]\",\"key\":\"%[^\"]\"", devid, apikey) == 2)
                {
                    ESP_LOGI(TAG, "create device: %s, %s\r\n", devid, apikey);
                    result = 0;
                }
            }
        }

        NET_DEVICE_Close();
    }
    MQTT_FreeBuffer(send_ptr);

    return result;

}




//==========================================================
//  函数名称： OneNET_GetLinkIP
//
//  函数功能： 获取使用协议的登录IP和PORT
//
//  入口参数： protocol：协议号
//              ip：保存返回IP的缓存区
//              port：保存返回port的缓存区
//
//  返回参数： 0-成功        1-失败
//
//  说明：       1-edp   2-nwx   3-jtext     4-Hiscmd
//              5-jt808         6-modbus    7-mqtt
//              8-gr20          9-reg       10-HTTP(自定义)
//              获取IP本身不支持HTTP协议，这里自定义一个标志
//==========================================================
_Bool OneNET_GetLinkIP(ONENET_PROTOCOL_ENUM protocol, char *ip, char *port)
{
    _Bool result = 1;
    char *data_ptr = NULL;
    char *send_buf = NULL;
    uint32_t read_len = 256;

    if (protocol == ONENET_PROTOCOL_HTTP)                                                 //如果是HTTP协议
    {
        strcpy(ip, "183.230.40.33");
        strcpy(port, "80");

        return 0;
    }
    uint16_t send_buf_len = 256;
    send_buf = MQTT_MallocBuffer(send_buf_len);
    if (NULL == send_buf)
    {
        return result;
    }
    if (NET_DEVICE_Connect("TCP", "183.230.40.33", "80") == 0)
    {

        memset(send_buf, 0, send_buf_len);
        snprintf(send_buf, send_buf_len, "GET http://api.heclouds.com/s?t=%d HTTP/1.1\r\n"
                 "api-key:=sUT=jsLGXkQcUz3Z9EaiNQ80U0=\r\n"
                 "Host:api.heclouds.com\r\n\r\n",
                 protocol);

        NET_DEVICE_SendData((uint8_t *)send_buf, strlen(send_buf));
        memset(send_buf, 0, send_buf_len);
        NET_DEVICE_Read((uint8_t *)send_buf, &read_len);
        if (0 != read_len)
        {
            data_ptr = strstr(send_buf, "no-cache");             //找到最后的关键词
        }

        if (data_ptr != NULL)
        {
            if (strstr(data_ptr, "unsupportted") != NULL)                       //不支持的协议类型
            {
                ESP_LOGE(TAG,  "不支持该协议类型\r\n");
            }
            else if (strstr(data_ptr, "can't find a available") != NULL)        //不支持的协议类型
            {
                ESP_LOGE(TAG,  "can't find a available IP\r\n");
            }
            else
            {
                if (sscanf(data_ptr, "no-cache\r\n%[^:]:%s", ip, port) == 2)
                {
                    result = 0;
                    ESP_LOGI(TAG,  "Get ip: %s, port: %s\r\n", ip, port);
                }
            }
        }

        NET_DEVICE_Close();
    }
    MQTT_FreeBuffer(send_buf);

    return result;

}


//==========================================================
//  函数名称： OneNET_DevConnect
//
//  函数功能： 与onenet创建连接
//
//  入口参数： devid：创建设备的devid
//              proid：产品ID
//              auth_key：创建设备的masterKey或apiKey或设备鉴权信息
//
//  返回参数： 无
//
//  说明：       与onenet平台建立连接，成功或会标记oneNetInfo.netWork网络状态标志
//==========================================================
_Bool OneNET_DevConnect(const char *devid, const char *proid, const char *auth_info)
{
    _Bool result = 1;
    MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};                 //协议包

    if (ONENET_NET_STATUS_CONNECT_PLATFORM == onenet_info.status)        //已经连接了平台
    {
        return 0;
    }

    if (0 != NET_DEVICE_Connect("TCP", onenet_info.ip, onenet_info.port))
    {
        onenet_info.status = ONENET_NET_STATUS_CONNECT_NONE;
        return result;
    }
    else
    {
        onenet_info.status = ONENET_NET_STATUS_CONNECT_IP;
    }


    ESP_LOGI(TAG, "OneNET_DevLink"
             "PROID: %s,	AUIF: %s,	DEVID:%s\r\n"
             , proid, auth_info, devid);

    if (MQTT_PacketConnect(proid, auth_info, devid, 256, 0, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqtt_packet) == 0)
    {
        if (0 == NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len))          //上传平台
        {
            onenet_info.send_time = xTaskGetTickCount();
            onenet_info.send_handled = 0;
            result = 0;
        }
        else
        {
            ESP_LOGW(TAG,  "WARN:  OneNET_DevConnect MQTT_Packet Failed\r\n");
        }
        MQTT_DeleteBuffer(&mqtt_packet);      //删包
    }
    else
    {
        NET_DEVICE_Close();
        onenet_info.status = ONENET_NET_STATUS_CONNECT_NONE;
        ESP_LOGW(TAG,  "WARN:  MQTT_PacketConnect Failed\r\n");
    }

    return result;
}

//==========================================================
//  函数名称： OneNET_DisConnect
//
//  函数功能： 与平台断开连接
//
//  入口参数： 无
//
//  返回参数： 0-成功        1-失败
//
//  说明：
//==========================================================
_Bool OneNET_DevDisConnect(void)
{
    _Bool result = 1;
    MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};                            //协议包

    if (0 != NET_DEVICE_GetStatus())
    {
        return 0;
    }
    if (ONENET_NET_STATUS_CONNECT_PLATFORM != onenet_info.status)
    {
        return 0;
    }

    if (MQTT_PacketDisConnect(&mqtt_packet) == 0)
    {
        if (0 == NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len))          //向平台发送订阅请求
        {
            result = 0;
        }
        else
        {

        }

        MQTT_DeleteBuffer(&mqtt_packet);                                            //删包
    }
    NET_DEVICE_Close();

    return result;

}

//==========================================================
//  函数名称： OneNET_SendData
//
//  函数功能： 上传数据到平台
//
//  入口参数： type：发送数据的格式
//              devid：设备ID
//              apikey：设备apikey
//              streamArray：数据流
//              streamArrayNum：数据流个数
//
//  返回参数： SEND_TYPE_OK-发送成功   SEND_TYPE_DATA-需要重送
//
//  说明：
//==========================================================
uint8_t OneNET_SendData(FORMAT_TYPE type, char *devid, char *apikey, DATA_STREAM *streamArray, unsigned short streamArrayCnt)
{

    MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};                                            //协议包

    _Bool status = SEND_TYPE_OK;
    short body_len = 0;

    if (ONENET_NET_STATUS_CONNECT_PLATFORM != onenet_info.status)
    {
        return SEND_TYPE_DATA;
    }

    ESP_LOGI(TAG, "Tips:	OneNET_SendData-MQTT_TYPE%d\r\n", type);

    body_len = DSTREAM_GetDataStream_Body_Measure(type, streamArray, streamArrayCnt, 0);        //获取当前需要发送的数据流的总长度
    if (body_len > 0)
    {
        if (MQTT_PacketSaveData(devid, body_len, NULL, (uint8_t)type, &mqtt_packet) == 0)
        {
            body_len = DSTREAM_GetDataStream_Body(type, streamArray, streamArrayCnt, mqtt_packet._data, mqtt_packet._size, mqtt_packet._len);

            if (body_len > 0)
            {
                mqtt_packet._len += body_len;
                ESP_LOGI(TAG, "Send %d Bytes\r\n", mqtt_packet._len);
                if (0 == NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len))                   //上传数据到平台
                {

                }
                else
                {
                    onenet_info.error_cnt++;
                }

            }
            else
            {
                ESP_LOGE(TAG, "WARN:	DSTREAM_GetDataStream_Body Failed\r\n");
            }

            MQTT_DeleteBuffer(&mqtt_packet);                                                         //删包
        }
        else
        {
            ESP_LOGE(TAG, "WARN:	MQTT_NewBuffer Failed\r\n");
        }
    }
    else if (body_len < 0)
    {
        status = SEND_TYPE_OK;
    }
    else
    {
        status = SEND_TYPE_DATA;
    }

    return status;

}

//==========================================================
//  函数名称： OneNET_Send_BinFile
//
//  函数功能： 上传二进制文件到平台
//
//  入口参数： name：数据流名
//              file：文件
//              file_size：文件长度
//
//  返回参数： SEND_TYPE_OK-发送成功   SEND_TYPE_BINFILE-需要重送
//
//  说明：
//==========================================================
#define PKT_SIZE (4096)
uint8_t OneNET_Send_BinFile(char *name, const uint8_t *file, uint32_t file_size)
{

    MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};                    //协议包

    uint8_t status = SEND_TYPE_BINFILE;

    char *type_bin_head = NULL;                                             //图片数据头
    uint8_t *file_t = (uint8_t *)file;

    if (name == NULL || file == NULL || file_size == 0)
    {
        return status;
    }

    if (ONENET_NET_STATUS_CONNECT_PLATFORM != onenet_info.status)
    {
        return status;
    }

    type_bin_head = (char *)MQTT_MallocBuffer(13 + strlen(name));
    if (type_bin_head == NULL)
        return status;

    sprintf(type_bin_head, "{\"ds_id\":\"%s\"}", name);

    if (MQTT_PacketSaveBinData(name, file_size, &mqtt_packet) == 0)
    {
        do
        {
            if (0 == NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len))          //上传数据到平台
            {
                onenet_info.send_time = xTaskGetTickCount();
                onenet_info.send_handled = 0;
            }
            else
            {
                onenet_info.error_cnt++;
                break;
            }

            MQTT_DeleteBuffer(&mqtt_packet);                                    //删包

            ESP_LOGD(TAG, "Image Len = %d", file_size);

            while (file_size > 0)
            {
                RTOS_TimeDly(20);
                ESP_LOGD(TAG, "Image Reamin %d Bytes", file_size);

                if (file_size >= PKT_SIZE)
                {
                    if (0 == NET_DEVICE_SendData(file_t, PKT_SIZE))       //发送分片
                    {
                        file_t += PKT_SIZE;
                        file_size -= PKT_SIZE;
                    }
                    else
                    {
                        onenet_info.error_cnt++;
                        break;
                    }

                }
                else
                {
                    if (0 == NET_DEVICE_SendData(file_t, file_size)) //发送最后一个分片
                    {
                        file_size = 0;
                    }
                    else
                    {
                        onenet_info.error_cnt++;
                        break;
                    }
                }
            }
            ESP_LOGI(TAG, "Tips: File Send Ok");
            status = SEND_TYPE_OK;
        }
        while (0);
    }
    else
    {
        ESP_LOGE(TAG, "MQTT_PacketSaveData Failed");
    }

    MQTT_FreeBuffer(type_bin_head);

    return status;

}

//==========================================================
//  函数名称： OneNET_SendData_Heart
//
//  函数功能： 心跳检测
//
//  入口参数： 无
//
//  返回参数： SEND_TYPE_OK-发送成功   SEND_TYPE_DATA-需要重送
//
//  说明：
//==========================================================
uint8_t OneNET_SendData_Heart(void)
{
    uint8_t status = SEND_TYPE_HEART;
    MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};                //协议包

    if (0 == MQTT_PacketPing(&mqtt_packet))
    {
        ESP_LOGI(TAG, "MQTT_PacketPing ok");
        if (0 == NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len))    //向平台上传心跳请求
        {
            onenet_info.send_time = xTaskGetTickCount();
            onenet_info.send_handled = 0;
            status = SEND_TYPE_OK;
        }
        else
        {
            onenet_info.error_cnt++;
        }

        MQTT_DeleteBuffer(&mqtt_packet);                                    //删包
    }
    else
    {
        ESP_LOGE(TAG, "MQTT_PacketPing err");
    }

    return status;

}

//==========================================================
//  函数名称： OneNET_Publish
//
//  函数功能： 发布消息
//
//  入口参数： topic：发布的主题
//              msg：消息内容
//
//  返回参数： SEND_TYPE_OK-成功 SEND_TYPE_PUBLISH-需要重送
//
//  说明：
//==========================================================
uint8_t OneNET_Publish(const char *topic, const char *msg)
{
    uint8_t status = SEND_TYPE_PUBLISH;
    MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};                            //协议包

    if (ONENET_NET_STATUS_CONNECT_PLATFORM != onenet_info.status)
    {
        return status;
    }

    ESP_LOGI(TAG, "Publish Topic: %s, Msg: %s\r\n", topic, msg);

    if (MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, strlen(msg), MQTT_QOS_LEVEL2, 0, 1, &mqtt_packet) == 0)
    {
        if (0 == NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len))             //向平台发送订阅请求
        {
            onenet_info.send_time = xTaskGetTickCount();
            onenet_info.send_handled = 0;
            status = SEND_TYPE_OK;
        }
        else
        {
            onenet_info.error_cnt++;
        }

        MQTT_DeleteBuffer(&mqtt_packet);                                            //删包
    }

    return status;

}

//==========================================================
//  函数名称： OneNET_Subscribe
//
//  函数功能： 订阅
//
//  入口参数： topics：订阅的topic
//              topic_cnt：topic个数
//
//  返回参数： SEND_TYPE_OK-成功 SEND_TYPE_SUBSCRIBE-需要重发
//
//  说明：
//==========================================================
uint8_t OneNET_Subscribe(const char *topics[], uint8_t topic_cnt)
{
    uint8_t status = SEND_TYPE_SUBSCRIBE;
    uint8_t i = 0;

    MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};                            //协议包

    if (ONENET_NET_STATUS_CONNECT_PLATFORM != onenet_info.status)
    {
        return status;
    }

    for (; i < topic_cnt; i++)
    {
        ESP_LOGI(TAG, "Subscribe Topic: %s\r\n", topics[i]);
    }

    if (MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL2, topics, topic_cnt, &mqtt_packet) == 0)
    {
        if (0 == NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len))             //向平台发送订阅请求
        {
            onenet_info.send_time = xTaskGetTickCount();
            onenet_info.send_handled = 0;
            status = SEND_TYPE_OK;
        }
        else
        {
            onenet_info.error_cnt++;
        }


        MQTT_DeleteBuffer(&mqtt_packet);                                            //删包
    }

    return status;

}

//==========================================================
//  函数名称： OneNET_UnSubscribe
//
//  函数功能： 取消订阅
//
//  入口参数： topics：订阅的topic
//              topic_cnt：topic个数
//
//  返回参数： SEND_TYPE_OK-发送成功   SEND_TYPE_UNSUBSCRIBE-需要重发
//
//  说明：
//==========================================================
uint8_t OneNET_UnSubscribe(const char *topics[], uint8_t topic_cnt)
{
    uint8_t status = SEND_TYPE_UNSUBSCRIBE;
    uint8_t i = 0;

    MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};                            //协议包

    if (ONENET_NET_STATUS_CONNECT_PLATFORM != onenet_info.status)
    {
        return status;
    }

    for (; i < topic_cnt; i++)
    {
        ESP_LOGI(TAG, "UnSubscribe Topic: %s\r\n", topics[i]);
    }

    if (MQTT_PacketUnSubscribe(MQTT_UNSUBSCRIBE_ID, topics, topic_cnt, &mqtt_packet) == 0)
    {
        if (0 == NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len))          //向平台发送取消订阅请求
        {
            onenet_info.send_time = xTaskGetTickCount();
            onenet_info.send_handled = 0;
            status = SEND_TYPE_OK;
        }
        else
        {
            onenet_info.error_cnt++;
        }

        MQTT_DeleteBuffer(&mqtt_packet);                                            //删包
    }

    return status;

}



//==========================================================
//  函数名称： OneNET_RevPro
//
//  函数功能： 平台返回数据检测
//
//  入口参数： dataPtr：平台返回的数据
//
//  返回参数： 无
//
//  说明：
//==========================================================
void OneNET_RevPro(void)
{
    MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};                                        //协议包
    static uint8_t rev_buf[1024];
    uint32_t rev_len = 1024;

    char *req_payload = NULL;
    char *cmdid_topic = NULL;

    unsigned short topic_len = 0;
    unsigned short req_len = 0;

    uint8_t qos = 0;
    static unsigned short pkt_id = 0;

    if (ONENET_NET_STATUS_CONNECT_NONE == onenet_info.status)
    {
        return ;
    }
    if (0 != NET_DEVICE_Read((uint8_t *)rev_buf, &rev_len))
    {
        return;
    }
    if (0 == rev_len)
    {
        return;
    }
    if ((0 == onenet_info.send_handled) && (xTaskGetTickCount() > (onenet_info.send_time + 1000)))
    {
        onenet_info.error_cnt++;
        onenet_info.send_handled = 1;
        if (0 != NET_DEVICE_GetStatus())  //多次错误后检查是不是连接已经断开了
        {
            onenet_info.status = ONENET_NET_STATUS_CONNECT_NONE;//如果是则切换状态
        }
    }
    if (onenet_info.error_cnt > 3)
    {
        onenet_info.status = ONENET_NET_STATUS_CONNECT_NONE;
        return;
    }

    uint8_t *cmd = rev_buf;
    ESP_LOGI(TAG, "recever data %dbytes", rev_len);
    uint8_t type;
    type = MQTT_UnPacketRecv(cmd);
    if (255 != type)
    {
        onenet_info.error_cnt = 0;
    }
    switch (type)
    {
    case MQTT_PKT_CONNACK:

        switch (MQTT_UnPacketConnectAck(cmd))
        {
        case 0:
            ESP_LOGI(TAG, "Tips:	连接成功\r\n");
            onenet_info.status = ONENET_NET_STATUS_CONNECT_PLATFORM;
            break;

        case 1: ESP_LOGE(TAG, "WARN:	连接失败：协议错误\r\n"); break;
        case 2: ESP_LOGE(TAG, "WARN:	连接失败：非法的clientid\r\n"); break;
        case 3: ESP_LOGE(TAG, "WARN:  连接失败：服务器失败\r\n"); break;
        case 4: ESP_LOGE(TAG, "WARN:  连接失败：用户名或密码错误\r\n"); break;
        case 5: ESP_LOGE(TAG, "WARN:  连接失败：非法链接(比如token非法)\r\n"); break;

        default: ESP_LOGE(TAG, "ERR:  连接失败：未知错误\r\n"); break;
        }

        break;

    case MQTT_PKT_PINGRESP:

        ESP_LOGI(TAG, "Tips:	HeartBeat OK\r\n");

        break;

    case MQTT_PKT_CMD:                                                                  //命令下发

        if (MQTT_UnPacketCmd(cmd, &cmdid_topic, &req_payload, &req_len) == 0)           //解出topic和消息体
        {
            ESP_LOGI(TAG, "cmdid: %s, req: %s, req_len: %d\r\n", cmdid_topic, req_payload, req_len);

            //执行命令回调------------------------------------------------------------
            if (NULL != onnet_cmd_callback)
            {
                onnet_cmd_callback(req_payload);
            }

            if (MQTT_PacketCmdResp(cmdid_topic, req_payload, &mqtt_packet) == 0)        //命令回复组包
            {
                ESP_LOGI(TAG, "Tips:	Send CmdResp\r\n");
                NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len);             //回复命令
                MQTT_DeleteBuffer(&mqtt_packet);                                            //删包
            }

            MQTT_FreeBuffer(cmdid_topic);
            MQTT_FreeBuffer(req_payload);
        }

        break;

    case MQTT_PKT_PUBLISH:                                                              //接收的Publish消息

        if (MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len, &req_payload, &req_len, &qos, &pkt_id) == 0)
        {
            ESP_LOGI(TAG, "topic: %s, topic_len: %d, payload: %s, payload_len: %d\r\n",
                     cmdid_topic, topic_len, req_payload, req_len);

            //执行命令回调------------------------------------------------------------
            if (NULL != onnet_cmd_callback)
            {
                onnet_cmd_callback(req_payload);
            }

            switch (qos)
            {
            case 1:                                                                 //收到publish的qos为1，设备需要回复Ack

                if (MQTT_PacketPublishAck(pkt_id, &mqtt_packet) == 0)
                {
                    ESP_LOGI(TAG, "Tips:	Send PublishAck\r\n");
                    NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len);
                    MQTT_DeleteBuffer(&mqtt_packet);
                }

                break;

            case 2:                                                                 //收到publish的qos为2，设备先回复Rec
                //平台回复Rel，设备再回复Comp
                if (MQTT_PacketPublishRec(pkt_id, &mqtt_packet) == 0)
                {
                    ESP_LOGI(TAG, "Tips:	Send PublishRec\r\n");
                    NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len);
                    MQTT_DeleteBuffer(&mqtt_packet);
                }

                break;

            default:
                break;
            }

            MQTT_FreeBuffer(cmdid_topic);
            MQTT_FreeBuffer(req_payload);
        }

        break;

    case MQTT_PKT_PUBACK:                                                               //发送Publish消息，平台回复的Ack

        if (MQTT_UnPacketPublishAck(cmd) == 0)
        {
            ESP_LOGI(TAG, "Tips:	MQTT Publish Send OK\r\n");
        }

        break;

    case MQTT_PKT_PUBREC:                                                               //发送Publish消息，平台回复的Rec，设备需回复Rel消息

        if (MQTT_UnPacketPublishRec(cmd) == 0)
        {
            ESP_LOGI(TAG, "Tips:	Rev PublishRec\r\n");
            if (MQTT_PacketPublishRel(MQTT_PUBLISH_ID, &mqtt_packet) == 0)
            {
                ESP_LOGI(TAG, "Tips:	Send PublishRel\r\n");
                NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len);
                MQTT_DeleteBuffer(&mqtt_packet);
            }
        }

        break;

    case MQTT_PKT_PUBREL:                                                               //收到Publish消息，设备回复Rec后，平台回复的Rel，设备需再回复Comp

        if (MQTT_UnPacketPublishRel(cmd, pkt_id) == 0)
        {
            ESP_LOGI(TAG, "Tips:	Rev PublishRel\r\n");
            if (MQTT_PacketPublishComp(pkt_id, &mqtt_packet) == 0)
            {
                ESP_LOGI(TAG, "Tips:	Send PublishComp\r\n");

                NET_DEVICE_SendData(mqtt_packet._data, mqtt_packet._len);
                MQTT_DeleteBuffer(&mqtt_packet);
            }
        }

        break;

    case MQTT_PKT_PUBCOMP:                                                              //发送Publish消息，平台返回Rec，设备回复Rel，平台再返回的Comp

        if (MQTT_UnPacketPublishComp(cmd) == 0)
        {
            ESP_LOGI(TAG, "Tips:	Rev PublishComp\r\n");
        }

        break;

    case MQTT_PKT_SUBACK:                                                               //发送Subscribe消息的Ack

        if (MQTT_UnPacketSubscribe(cmd) == 0)
        {
            ESP_LOGI(TAG, "Tips:	MQTT Subscribe OK\r\n");
        }
        else
        {
            ESP_LOGE(TAG, "Tips:  MQTT Subscribe Err\r\n");
        }

        break;

    case MQTT_PKT_UNSUBACK:                                                             //发送UnSubscribe消息的Ack

        if (MQTT_UnPacketUnSubscribe(cmd) == 0)
        {
            ESP_LOGI(TAG, "Tips:	MQTT UnSubscribe OK\r\n");
        }
        else
        {
            ESP_LOGE(TAG, "Tips:  MQTT UnSubscribe Err\r\n");
        }

        break;

    default:

        break;
    }

}
