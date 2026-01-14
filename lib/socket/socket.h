#ifndef SOCKET_H
#define SOCKET_H

#ifdef WIN32

#include <winsock2.h>
#include <winsock.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <sys/types.h>
#define in_addr_t uint32_t

/* IPX options */
#define IPX_TYPE	1

/* SUS symbolic values for the second parm to shutdown(2) */
#define SHUT_RD   0		/* == Win32 SD_RECEIVE */
#define SHUT_WR   1		/* == Win32 SD_SEND    */
#define SHUT_RDWR 2		/* == Win32 SD_BOTH    */
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include<netinet/in.h>
#include <unistd.h>
#endif

#ifdef ANDROID
#include <arpa/inet.h>
#endif

#include <string.h>
#include <errno.h>
#include <signal.h>

#include <stdio.h>


#endif