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
#ifdef ESP_PLATFORM
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
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

#include <netdb.h>
#endif
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
uint16_t _htons(uint16_t v);
uint32_t _htonl(uint32_t v);
uint16_t _ntohs(uint16_t v);
char* _inet_ntoa(struct in_addr in);
int _getaddrinfo(const char* nodename, const char* servname,
                 const struct addrinfo* hints, struct addrinfo** res);
void _freeaddrinfo(struct addrinfo* res);
const char* _gai_strerror(int errcode);

#endif