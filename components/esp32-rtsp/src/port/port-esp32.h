#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef int WiFiClient;
typedef int SOCKET;
typedef int UDPSOCKET;
typedef uint32_t IPADDRESS;
typedef uint16_t IPPORT;
#define NULLSOCKET 0


#define GET_RANDOM() (esp_random())

void socketpeeraddr(SOCKET s, IPADDRESS *addr, IPPORT *port);
void udpsocketclose(UDPSOCKET s);


UDPSOCKET udpsocketcreate(unsigned short portNum);

// TCP sending
ssize_t socketsend(SOCKET sockfd, const void *buf, size_t len);

ssize_t udpsocketsend(UDPSOCKET sockfd, const void *buf, size_t len, IPADDRESS destaddr, IPPORT destport);
/**
   Read from a socket with a timeout.

   Return 0=socket was closed by client, -1=timeout, >0 number of bytes read
 */
int socketread(SOCKET sock, char *buf, size_t buflen, int timeoutmsec);

int socketrecv(SOCKET sock, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif
