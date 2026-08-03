/*
 * Mock netdb.h for ya embedded (esp32/stm32) core build.
 * newlib does not ship netdb.h; lwip provides the real one in the
 * ESP-IDF project. Static libs may keep these symbols undefined, so
 * only declarations needed.
 */
#ifndef _PORT_NETDB_H
#define _PORT_NETDB_H

#include <stddef.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};

#define AI_PASSIVE     0x01
#define AI_CANONNAME   0x02
#define AI_NUMERICHOST 0x04

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
const char *gai_strerror(int errcode);

#ifdef __cplusplus
}
#endif

#endif /* _PORT_NETDB_H */
