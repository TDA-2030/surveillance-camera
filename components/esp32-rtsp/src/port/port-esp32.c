#if !__linux

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "port-esp32.h"

static const char *TAG = "port-esp32";

#define ERR_CHECK(a, str, ret_val)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

// inline void closesocket(SOCKET s) {
//     ESP_LOGE(TAG, "closing TCP socket\n");

//     close(s);
// }

void socketpeeraddr(SOCKET s, IPADDRESS *addr, IPPORT *port)
{

    struct sockaddr_in r;
    socklen_t len = sizeof(r);
    if (getpeername(s, (struct sockaddr *)&r, &len) < 0) {
        ESP_LOGE(TAG, "getpeername failed\n");
        *addr = 0;
        *port = 0;
    } else {
        //htons

        *port  = r.sin_port;
        *addr = r.sin_addr.s_addr;
    }
}

void udpsocketclose(UDPSOCKET s)
{
    ESP_LOGI(TAG, "closing UDP socket\n");
    close(s);
}

UDPSOCKET udpsocketcreate(unsigned short portNum)
{
    struct sockaddr_in addr;
    int sockfd;
    // int opt = 1;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        ESP_LOGW(TAG, "udp server create error, code: %d, reason: %s", errno, strerror(errno));
        return 0;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(portNum);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGW(TAG, "udp server bind error, code: %d, reason: %s", errno, strerror(errno));
        close(sockfd);
        return 0;
    }

    return sockfd;
}

// TCP sending
ssize_t socketsend(SOCKET sockfd, const void *buf, size_t len)
{
    // ESP_LOGE(TAG, "TCP send\n");
    return send(sockfd, buf, len, 0);
}

ssize_t udpsocketsend(UDPSOCKET sockfd, const void *buf, size_t len,
                      IPADDRESS destaddr, IPPORT destport)
{
    struct sockaddr_in addr;

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = destaddr;
    addr.sin_port = htons(destport);

    return sendto(sockfd, buf, len, 0, (struct sockaddr *)&addr, sizeof(addr));
}

/**
   Read from a socket with a timeout.

   Return 0=socket was closed by client, -1=timeout, >0 number of bytes read
 */
int socketread(SOCKET sock, char *buf, size_t buflen, int timeoutmsec)
{
    // Use a timeout on our socket read to instead serve frames
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeoutmsec * 1000; // send a new frame ever
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    int res = recv(sock, buf, buflen, 0);
    if (res > 0) {
        return res;
    } else if (res == 0) {
        return 0; // client dropped connection
    } else {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return -1;
        } else {
            return 0;    // unknown error, just claim client dropped it
        }
    };
}

int socketrecv(SOCKET sock, char *buf, size_t buflen)
{
    int res = recv(sock, buf, buflen, 0);
    if (res > 0) {
        return res;
    } else if (res == 0) {
        return 0; // client dropped connection
    } else {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return -1;
        } else {
            return 0;    // unknown error, just claim client dropped it
        }
    };
}


#endif
