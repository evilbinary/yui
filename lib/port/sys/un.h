/*
 * Mock sys/un.h for ya ESP32 core build.
 * newlib does not ship sys/un.h; only the struct is referenced by socket.h.
 */
#ifndef _PORT_SYS_UN_H
#define _PORT_SYS_UN_H

#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[108];
};

#ifdef __cplusplus
}
#endif

#endif /* _PORT_SYS_UN_H */
