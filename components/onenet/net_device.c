
/* HTTP GET Example using plain POSIX sockets

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "net_device.h"


static const char *TAG = "net_device";


//网络设备
#include "net_device.h"

NET_DEVICE_INFO net_device_info =
{
    .status = NET_DEVICE_STATUS_NONE,
    .head = NULL,
    .end = NULL,
};




static int socket_s;  //

_Bool NET_DEVICE_Connect(char *type, char *ip, char *port)
{
    _Bool result = 0;

    const struct addrinfo hints =
    {
        .ai_family = AF_INET,  //IPV4
        .ai_socktype = SOCK_STREAM,  //TCP
    };
    struct addrinfo *res;

    do
    {
        if (NULL == strstr(type, "TCP"))
        {
            result = 1;
            break;
        }
        int err = getaddrinfo(ip, port, &hints, &res);

        if (err != 0 || res == NULL)
        {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            result = 1;
            break;
        }
        /* Code to print the resolved IP.

           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        struct in_addr *addr;
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        socket_s = socket(res->ai_family, res->ai_socktype, 0);
        if (socket_s < 0)
        {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            result = 1;
            freeaddrinfo(res);
            break;
        }
        if (connect(socket_s, res->ai_addr, res->ai_addrlen) != 0)
        {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(socket_s);
            net_device_info.status = NET_DEVICE_STATUS_NONE;
            freeaddrinfo(res);
            result = 1;
            break;
        }

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        
        if (setsockopt(socket_s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0)
        {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(socket_s);
            net_device_info.status = NET_DEVICE_STATUS_NONE;
            result = 1;
            break;
        }

        net_device_info.status = NET_DEVICE_STATUS_CONNECTED;
        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);
    }
    while (0);
    return result;
}

//==========================================================
//  函数名称： NET_DEVICE_Close
//
//  函数功能： 关闭socket套接字
//
//  入口参数： socket_fd：socket描述符
//
//  返回参数： 不知道
//
//  说明：
//==========================================================
_Bool NET_DEVICE_Close(void)
{

    _Bool result = 0;

    result = close(socket_s);   //关闭连接
    ESP_LOGI(TAG, "... close");
    net_device_info.status = NET_DEVICE_STATUS_NONE;
    socket_s = -1;
    
    return result;

}


//==========================================================
//  函数名称： NET_DEVICE_SendData
//
//  函数功能： 使网络设备发送数据到平台
//
//  入口参数： data：需要发送的数据
//              len：数据长度
//
//  返回参数： 0-发送完成  1-发送失败
//
//  说明：
//==========================================================
_Bool NET_DEVICE_SendData(const unsigned char *data, uint32_t len)
{
    _Bool result = 0;
    uint32_t write_len = 0;
    while (write_len != len)
    {
        int l = send(socket_s, data + write_len, len - write_len, 0);
        if (l < 0)
        {
            ESP_LOGE(TAG, "... socket send failed");
            result = 1;
            break;
        }
        else
        {
            write_len += l;
        }
    }
    if (write_len == len)
    {
        //ESP_LOGI(TAG, "... socket send success");
    }
    return result;
}

//==========================================================
//  函数名称： NET_DEVICE_Read
//
//  函数功能： 读取一帧数据
//
//  入口参数： 无
//
//  返回参数： 无
//
//  说明：
//==========================================================
_Bool NET_DEVICE_Read(uint8_t *data, uint32_t *len)
{
    _Bool result = 1;
    uint32_t read_len = 0;
    int32_t r;

    /* Read HTTP response */
    do
    {
        r = recv(socket_s, data, *len, 0);
        if (r < 0)
        {
            result = 0;
        }
        else if (r > 0)
        {
            result = 0;
            read_len += r;
        }
    }
    while (r > 0);
    *len = read_len;

    return result;
}

_Bool NET_DEVICE_GetStatus(void)
{
    return (net_device_info.status==NET_DEVICE_STATUS_CONNECTED) ? 0 : 1;
}
