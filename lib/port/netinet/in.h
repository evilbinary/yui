/*
 * Mock netinet/in.h for ya ESP32 core build.
 * newlib does not ship netinet/in.h; lwip provides the real one in the
 * ESP-IDF project. Declarations here are only needed to compile static
 * libs (socket.c).
 */
#ifndef _PORT_NETINET_IN_H
#define _PORT_NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

#ifdef __cplusplus
}
#endif

#endif /* _PORT_NETINET_IN_H */
