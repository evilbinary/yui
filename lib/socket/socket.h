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

/* On Windows, _close conflicts with MSVCRT, so map it to closesocket */
// #ifndef _close
// #define _close closesocket
// #endif

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include<netinet/in.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#endif

#ifdef ANDROID
#include <arpa/inet.h>
#endif

#include <string.h>
#include <errno.h>
#include <signal.h>

#include <stdio.h>



int _socket(int domain, int type, int protocol);
int _close(int fd);
int _shutdown(int socket, int how);
int _connect(int socket, const struct sockaddr *address, socklen_t address_len);
int _bind(int socket, const struct sockaddr *address, socklen_t address_len);
int _listen(int socket, int backlog);
int _accept(int socket, struct sockaddr *address, socklen_t *address_len);
int _getsockname(int socket, struct sockaddr *address, socklen_t *address_len);
int _getpeername(int socket, struct sockaddr *address, socklen_t *address_len);
int _setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
int _getsockopt(int socket, int level, int option_name, void *option_value, socklen_t *option_len);
ssize_t _send(int socket, const void *buffer, size_t length, int flags);

#endif