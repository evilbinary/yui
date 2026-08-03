/*
 * Mock POSIX socket header for ya ESP32 core build.
 * newlib does not ship sys/socket.h; the real lwip implementation is
 * provided by the ESP-IDF project at link time. Static libs may keep
 * these symbols undefined, so only declarations are needed here.
 */
#ifndef _PORT_SYS_SOCKET_H
#define _PORT_SYS_SOCKET_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int socklen_t;

typedef unsigned short sa_family_t;

struct iovec {
    void *iov_base;
    size_t iov_len;
};

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct msghdr {
    void *msg_name;
    socklen_t msg_namelen;
    struct iovec *msg_iov;
    int msg_iovlen;
    void *msg_control;
    socklen_t msg_controllen;
    int msg_flags;
};

int _socket(int domain, int type, int protocol);
int socket(int domain, int type, int protocol);
int bind(int socket, const struct sockaddr *address, socklen_t address_len);
int connect(int socket, const struct sockaddr *address, socklen_t address_len);
int accept(int socket, struct sockaddr *address, socklen_t *address_len);
int listen(int socket, int backlog);
int getpeername(int socket, struct sockaddr *address, socklen_t *address_len);
int getsockname(int socket, struct sockaddr *address, socklen_t *address_len);
int getsockopt(int socket, int level, int option_name, void *option_value, socklen_t *option_len);
int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
int shutdown(int socket, int how);
int socketpair(int domain, int type, int protocol, int socket_vector[2]);

ssize_t recv(int socket, void *buffer, size_t length, int flags);
ssize_t recvfrom(int socket, void *buffer, size_t length, int flags,
                 struct sockaddr *address, socklen_t *address_len);
ssize_t recvmsg(int socket, struct msghdr *message, int flags);
ssize_t send(int socket, const void *message, size_t length, int flags);
ssize_t sendmsg(int socket, const struct msghdr *message, int flags);
ssize_t sendto(int socket, const void *message, size_t length, int flags,
               const struct sockaddr *dest_addr, socklen_t dest_len);

#ifdef __cplusplus
}
#endif

#endif /* _PORT_SYS_SOCKET_H */
