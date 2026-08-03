/*
 * Mock arpa/inet.h for ya ESP32 core build.
 * newlib does not ship arpa/inet.h; lwip provides the real one in the
 * ESP-IDF project. Declarations here are only needed to compile static
 * libs (socket.c).
 */
#ifndef _PORT_ARPA_INET_H
#define _PORT_ARPA_INET_H

#include <stdint.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t htonl(uint32_t hostlong);
uint16_t htons(uint16_t hostshort);
uint32_t ntohl(uint32_t netlong);
uint16_t ntohs(uint16_t netshort);

in_addr_t inet_addr(const char *cp);
int inet_aton(const char *cp, struct in_addr *addr);
char *inet_ntoa(struct in_addr in);

#ifdef __cplusplus
}
#endif

#endif /* _PORT_ARPA_INET_H */
