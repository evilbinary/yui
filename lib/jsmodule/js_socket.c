#define CONFIG_CLASS_SOCKET
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef WIN32
#include "../../lib/socket/socket.h"

#define in_addr_t uint32_t
#else
#include "../../lib/socket/socket.h"
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <netdb.h>
#endif

#include "../../src/layer.h"
#include "js_module.h"
#include "mquickjs.h"
#include "mquickjs_build.h"

// 引入下划线版本的socket API以实现跨平台兼容
extern int _socket(int domain, int type, int protocol);
extern int _close(int fd);
extern int _shutdown(int socket, int how);
extern int _connect(int socket, const struct sockaddr *address, socklen_t address_len);
extern int _bind(int socket, const struct sockaddr *address, socklen_t address_len);
extern int _listen(int socket, int backlog);
extern int _accept(int socket, struct sockaddr *address, socklen_t *address_len);
extern int _getsockname(int socket, struct sockaddr *address, socklen_t *address_len);
extern int _getpeername(int socket, struct sockaddr *address, socklen_t *address_len);
extern int _setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
extern int _getsockopt(int socket, int level, int option_name, void *option_value, socklen_t *option_len);
extern ssize_t _send(int socket, const void *message, size_t length, int flags);
extern ssize_t _recv(int socket, void *buffer, size_t length, int flags);
extern ssize_t _sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
extern ssize_t _recvfrom(int socket, void *buffer, size_t length, int flags, struct sockaddr *address, socklen_t *address_len);
extern int _socketpair(int domain, int type, int protocol, int socket_vector[2]);
extern in_addr_t _inet_addr(const char* strptr);
extern uint32_t _ntohl(uint32_t netlong);

// 类ID定义
#define JS_CLASS_SOCKET (JS_CLASS_USER + 1)
#define JS_CLASS_COUNT (JS_CLASS_USER + 2)

// 函数索引定义
#define JS_CFUNCTION_socket (JS_CFUNCTION_USER + 100)
#define JS_CFUNCTION_socket_close (JS_CFUNCTION_USER + 101)
#define JS_CFUNCTION_socket_shutdown (JS_CFUNCTION_USER + 102)
#define JS_CFUNCTION_socket_connect (JS_CFUNCTION_USER + 103)
#define JS_CFUNCTION_socket_bind (JS_CFUNCTION_USER + 104)
#define JS_CFUNCTION_socket_listen (JS_CFUNCTION_USER + 105)
#define JS_CFUNCTION_socket_accept (JS_CFUNCTION_USER + 106)
#define JS_CFUNCTION_socket_getsockname (JS_CFUNCTION_USER + 107)
#define JS_CFUNCTION_socket_getpeername (JS_CFUNCTION_USER + 108)
#define JS_CFUNCTION_socket_socketpair (JS_CFUNCTION_USER + 109)
#define JS_CFUNCTION_socket_setsockopt (JS_CFUNCTION_USER + 110)
#define JS_CFUNCTION_socket_getsockopt (JS_CFUNCTION_USER + 111)
#define JS_CFUNCTION_socket_send (JS_CFUNCTION_USER + 112)
#define JS_CFUNCTION_socket_recv (JS_CFUNCTION_USER + 113)
#define JS_CFUNCTION_socket_sendto (JS_CFUNCTION_USER + 114)
#define JS_CFUNCTION_socket_recvfrom (JS_CFUNCTION_USER + 115)
#define JS_CFUNCTION_socket_inet_addr (JS_CFUNCTION_USER + 116)
#define JS_CFUNCTION_socket_ntohl (JS_CFUNCTION_USER + 117)
#define JS_CFUNCTION_socket_make_sockaddr_in (JS_CFUNCTION_USER + 118)

/* ====================== Socket 相关的 JS 函数 ====================== */

// 创建 socket
static JSValue js_socket_create(JSContext* ctx, JSValue* this_val, int argc,
                                JSValue* argv) {
  if (argc < 1) return JS_UNDEFINED;

  int32_t type;
  if (JS_ToInt32(ctx, &type, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  int fd = -1;

  if (type == 0) {  // tcp
    fd = _socket(PF_INET, SOCK_STREAM, 0);
  } else {
    fd = _socket(PF_INET, SOCK_DGRAM, 0);
  }

  return JS_NewInt32(ctx, fd);
}

// 关闭 socket
static JSValue js_socket_close(JSContext* ctx, JSValue* this_val, int argc,
                               JSValue* argv) {
  if (argc < 1) return JS_UNDEFINED;

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  _close(fd);
  return JS_UNDEFINED;
}

// shutdown socket
static JSValue js_socket_shutdown(JSContext* ctx, JSValue* this_val, int argc,
                                  JSValue* argv) {
  if (argc < 1) return JS_UNDEFINED;

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  _shutdown(fd, SHUT_RDWR);
  return JS_UNDEFINED;
}

// connect to remote host
static JSValue js_socket_connect(JSContext* ctx, JSValue* this_val, int argc,
                                 JSValue* argv) {
  if (argc < 4) return JS_UNDEFINED;

  int32_t fd, port, timeout;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  JSCStringBuf host_buf;
  size_t host_len;
  const char* host = JS_ToCStringLen(ctx, &host_len, argv[1], &host_buf);
  if (!host) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &port, argv[2]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &timeout, argv[3]) < 0) {
    return JS_UNDEFINED;
  }

  int res = -1;
  if (fd < 0 || port < 0) {
    return JS_NewInt32(ctx, res);
  }

  // 使用 getaddrinfo 解析主机名（支持域名和IP地址）
  struct addrinfo hints, *res_addrs, *rp;
  char port_str[16];
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;      // 只使用 IPv4
  hints.ai_socktype = SOCK_STREAM;
  
  snprintf(port_str, sizeof(port_str), "%d", port);
  
  int gai_res = getaddrinfo(host, port_str, &hints, &res_addrs);
  if (gai_res != 0) {
    // 解析失败，输出错误信息
    printf("JS(Socket): getaddrinfo failed for host '%s': %s\n", host, gai_strerror(gai_res));
    return JS_NewInt32(ctx, -1);
  }
  
   printf("JS(Socket): Successfully resolved host '%s', trying to connect...\n", host);

  // 遍历所有返回的地址，尝试连接
  int addr_index = 0;
  for (rp = res_addrs; rp != NULL; rp = rp->ai_next) {
    if (timeout <= 0) {
      res = _connect(fd, rp->ai_addr, rp->ai_addrlen);
      if (res < 0) {
        printf("JS(Socket): Connect attempt %d failed: %s\n", addr_index, strerror(errno));
      }
    } else {
      // 简化的超时连接实现
      unsigned long ul = 1;
      // ioctl(fd, FIONBIO, &ul); //TODO

      if (_connect(fd, rp->ai_addr, rp->ai_addrlen) < 0) {
        int error = -1, len;

        struct timeval tm;
        tm.tv_sec = timeout;
        tm.tv_usec = 0;

        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd, &set);

        if (select(fd + 1, NULL, &set, NULL, &tm) > 0) {
          _getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&len);
          if (error == 0) {
            res = 0;
          } else {
            printf("JS(Socket): Connect attempt %d failed with error: %d\n", addr_index, error);
          }
        } else {
          printf("JS(Socket): Connect attempt %d timed out or select failed\n", addr_index);
        }
      } else {
        res = 0;
      }

      ul = 0;
      // ioctl(fd, FIONBIO, &ul); //TODO
    }
    
    if (res == 0) {
      // 连接成功，跳出循环
      printf("JS(Socket): Successfully connected to address %d\n", addr_index);
      break;
    }
    addr_index++;
  }
  
  if (res != 0) {
    printf("JS(Socket): All %d connection attempts failed\n", addr_index);
  }
  
  // 释放地址信息链表
  freeaddrinfo(res_addrs);

  return JS_NewInt32(ctx, res);
}

// bind socket
static JSValue js_socket_bind(JSContext* ctx, JSValue* this_val, int argc,
                              JSValue* argv) {
  if (argc < 3) return JS_UNDEFINED;

  int32_t fd, port;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  JSCStringBuf host_buf;
  size_t host_len;
  const char* host = JS_ToCStringLen(ctx, &host_len, argv[1], &host_buf);
  if (!host) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &port, argv[2]) < 0) {
    return JS_UNDEFINED;
  }

  int res = -1;
  if (fd < 0 || port <= 0) {
    return JS_NewInt32(ctx, res);
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (host && host[0] != 0) addr.sin_addr.s_addr = _inet_addr(host);

  res = _bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr));

  return JS_NewInt32(ctx, res);
}

// listen for connections
static JSValue js_socket_listen(JSContext* ctx, JSValue* this_val, int argc,
                                JSValue* argv) {
  if (argc < 2) return JS_UNDEFINED;

  int32_t fd, backlog;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &backlog, argv[1]) < 0) {
    return JS_UNDEFINED;
  }

  int res = _listen(fd, backlog);
  return JS_NewInt32(ctx, res);
}

// accept connection
static JSValue js_socket_accept(JSContext* ctx, JSValue* this_val, int argc,
                                JSValue* argv) {
  if (argc < 1) return JS_UNDEFINED;

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  struct sockaddr_in in;
  memset(&in, 0, sizeof(in));
  in.sin_family = AF_INET;
  socklen_t size = sizeof(struct sockaddr);

  int cid = _accept(fd, (struct sockaddr*)&in, &size);
  return JS_NewInt32(ctx, cid);
}

// getsockname
static JSValue js_socket_getsockname(JSContext* ctx, JSValue* this_val,
                                     int argc, JSValue* argv) {
  if (argc < 1) return JS_UNDEFINED;

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  if (fd < 0) return JS_NewInt32(ctx, -1);

  struct sockaddr_in addr;
  socklen_t len = sizeof(struct sockaddr_in);
  int result = _getsockname(fd, (struct sockaddr*)&addr, &len);

  if (result == 0) {
    // 返回包含地址和端口的对象
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "ip",
                      JS_NewString(ctx, inet_ntoa(addr.sin_addr)));
    JS_SetPropertyStr(ctx, obj, "port", JS_NewInt32(ctx, _ntohl(addr.sin_port)));

    return obj;
  }

  return JS_NewInt32(ctx, result);
}

// getpeername
static JSValue js_socket_getpeername(JSContext* ctx, JSValue* this_val,
                                     int argc, JSValue* argv) {
  if (argc < 1) return JS_UNDEFINED;

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  if (fd < 0) return JS_NewInt32(ctx, -1);

  struct sockaddr_in addr;
  socklen_t len = sizeof(struct sockaddr_in);
  int result = _getpeername(fd, (struct sockaddr*)&addr, &len);

  if (result == 0) {
    // 返回包含地址和端口的对象
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "ip",
                      JS_NewString(ctx, inet_ntoa(addr.sin_addr)));
    JS_SetPropertyStr(ctx, obj, "port", JS_NewInt32(ctx, _ntohl(addr.sin_port)));

    return obj;
  }

  return JS_NewInt32(ctx, result);
}

// socketpair
static JSValue js_socket_socketpair(JSContext* ctx, JSValue* this_val, int argc,
                                    JSValue* argv) {
  if (argc < 3) return JS_UNDEFINED;

  int32_t domain, type, protocol;
  if (JS_ToInt32(ctx, &domain, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &type, argv[1]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &protocol, argv[2]) < 0) {
    return JS_UNDEFINED;
  }

  int sv[2];
  int result = _socketpair(domain, type, protocol, sv);

  if (result == 0) {
    // 返回包含两个套接字描述符的对象
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "0", JS_NewInt32(ctx, sv[0]));
    JS_SetPropertyStr(ctx, obj, "1", JS_NewInt32(ctx, sv[1]));

    return obj;
  }

  return JS_NewInt32(ctx, result);
}

// setsockopt
static JSValue js_socket_setsockopt(JSContext* ctx, JSValue* this_val, int argc,
                                    JSValue* argv) {
  if (argc < 5) return JS_UNDEFINED;

  int32_t fd, level, option_name;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &level, argv[1]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &option_name, argv[2]) < 0) {
    return JS_UNDEFINED;
  }

  int result;

  if (JS_IsNumber(ctx, argv[3])) {
    int32_t ival;
    if (JS_ToInt32(ctx, &ival, argv[3]) < 0) {
      return JS_UNDEFINED;
    }
    result = _setsockopt(fd, level, option_name, &ival, sizeof(int));
  } else {
    JSCStringBuf opt_buf;
    size_t optlen;
    const char* sval = JS_ToCStringLen(ctx, &optlen, argv[3], &opt_buf);
    if (!sval) {
      return JS_UNDEFINED;
    }
    result = _setsockopt(fd, level, option_name, sval, optlen);
    // 在 mQuickJS 中不需要手动释放字符串
  }

  return JS_NewInt32(ctx, result);
}

// getsockopt
static JSValue js_socket_getsockopt(JSContext* ctx, JSValue* this_val, int argc,
                                    JSValue* argv) {
  if (argc < 4) return JS_UNDEFINED;

  int32_t fd, level, option_name, option_len;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &level, argv[1]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &option_name, argv[2]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &option_len, argv[3]) < 0) {
    return JS_UNDEFINED;
  }

  char option_value[256];
  socklen_t len = option_len;

  if (option_len > sizeof(option_value)) len = sizeof(option_value);

  int result = _getsockopt(fd, level, option_name, option_value, &len);

  if (result == 0) {
    // 返回选项值
    return JS_NewStringLen(ctx, option_value, len);
  }

  return JS_NewInt32(ctx, result);
}

// send
static JSValue js_socket_send(JSContext* ctx, JSValue* this_val, int argc,
                              JSValue* argv) {
  if (argc < 3) return JS_UNDEFINED;

  int32_t fd, flags;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &flags, argv[2]) < 0) {
    return JS_UNDEFINED;
  }

  JSCStringBuf msg_buf;
  size_t msg_len;
  const char* message = JS_ToCStringLen(ctx, &msg_len, argv[1], &msg_buf);
  if (!message) {
    return JS_UNDEFINED;
  }

  int result = _send(fd, message, msg_len, flags);

  // 在 mQuickJS 中不需要手动释放字符串

  return JS_NewInt32(ctx, result);
}

// recv
static JSValue js_socket_recv(JSContext* ctx, JSValue* this_val, int argc,
                              JSValue* argv) {
  if (argc < 3) return JS_UNDEFINED;

  int32_t fd, flags, size;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &flags, argv[1]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &size, argv[2]) < 0) {
    return JS_UNDEFINED;
  }

  if (size <= 0) size = 1024;  // 默认缓冲区大小

  char* buffer = (char*)malloc(size);
  if (!buffer) return JS_NewInt32(ctx, -1);

  int result = _recv(fd, buffer, size - 1, flags);

  if (result >= 0) {
    buffer[result] = 0;  // 确保字符串结束
    JSValue str = JS_NewStringLen(ctx, buffer, result);
    free(buffer);
    return str;
  }

  free(buffer);
  return JS_NewInt32(ctx, result);
}

// sendto
static JSValue js_socket_sendto(JSContext* ctx, JSValue* this_val, int argc,
                                JSValue* argv) {
  if (argc < 5) return JS_UNDEFINED;

  int32_t fd, flags, port;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &flags, argv[2]) < 0) {
    return JS_UNDEFINED;
  }

  JSCStringBuf msg_buf, host_buf;
  size_t msg_len, host_len;
  const char* message = JS_ToCStringLen(ctx, &msg_len, argv[1], &msg_buf);
  if (!message) {
    return JS_UNDEFINED;
  }

  const char* host = JS_ToCStringLen(ctx, &host_len, argv[3], &host_buf);
  if (!host) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &port, argv[4]) < 0) {
    return JS_UNDEFINED;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = _inet_addr(host);
  addr.sin_port = htons(port);

  int result = _sendto(fd, message, msg_len, flags, (struct sockaddr*)&addr,
                      sizeof(struct sockaddr));

  // 在 mQuickJS 中不需要手动释放字符串

  return JS_NewInt32(ctx, result);
}

// recvfrom
static JSValue js_socket_recvfrom(JSContext* ctx, JSValue* this_val, int argc,
                                  JSValue* argv) {
  if (argc < 3) return JS_UNDEFINED;

  int32_t fd, flags, size;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &flags, argv[1]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &size, argv[2]) < 0) {
    return JS_UNDEFINED;
  }

  if (size <= 0) size = 1024;  // 默认缓冲区大小

  char* buffer = (char*)malloc(size);
  if (!buffer) return JS_NewInt32(ctx, -1);

  struct sockaddr_in from_addr;
  socklen_t from_len = sizeof(from_addr);

  int result = _recvfrom(fd, buffer, size - 1, flags,
                        (struct sockaddr*)&from_addr, &from_len);

  if (result >= 0) {
    buffer[result] = 0;  // 确保字符串结束

    // 创建返回对象，包含数据、发送方IP和端口
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "data", JS_NewStringLen(ctx, buffer, result));
    JS_SetPropertyStr(ctx, obj, "ip",
                      JS_NewString(ctx, inet_ntoa(from_addr.sin_addr)));
    JS_SetPropertyStr(ctx, obj, "port",
                      JS_NewInt32(ctx, ntohs(from_addr.sin_port)));

    free(buffer);
    return obj;
  }

  free(buffer);
  return JS_NewInt32(ctx, result);
}

// inet_addr
static JSValue js_socket_inet_addr(JSContext* ctx, JSValue* this_val, int argc,
                                   JSValue* argv) {
  if (argc < 1) return JS_UNDEFINED;

  JSCStringBuf str_buf;
  size_t str_len;
  const char* strptr = JS_ToCStringLen(ctx, &str_len, argv[0], &str_buf);
  if (!strptr) return JS_NewInt32(ctx, -1);

  uint32_t result = _inet_addr(strptr);

  // 在 mQuickJS 中不需要手动释放字符串

  return JS_NewInt32(ctx, result);
}

// ntohl
static JSValue js_socket_ntohl(JSContext* ctx, JSValue* this_val, int argc,
                               JSValue* argv) {
  if (argc < 1) return JS_UNDEFINED;

  uint32_t netlong;
  if (JS_ToInt32(ctx, (int32_t*)&netlong, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  uint32_t result = _ntohl(netlong);

  return JS_NewInt32(ctx, result);
}

// make_sockaddr_in
static JSValue js_socket_make_sockaddr_in(JSContext* ctx, JSValue* this_val,
                                          int argc, JSValue* argv) {
  if (argc < 3) return JS_UNDEFINED;

  int32_t family, port;
  uint32_t addr;

  if (JS_ToInt32(ctx, &family, argv[0]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, (int32_t*)&addr, argv[1]) < 0) {
    return JS_UNDEFINED;
  }

  if (JS_ToInt32(ctx, &port, argv[2]) < 0) {
    return JS_UNDEFINED;
  }

  // 分配并初始化sockaddr_in结构
  struct sockaddr_in* addr_in =
      (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
  if (!addr_in) return JS_NULL;

  memset(addr_in, 0, sizeof(struct sockaddr_in));
  addr_in->sin_family = family;
  addr_in->sin_addr.s_addr = htonl(addr);
  addr_in->sin_port = htons(port);

  // 在JS中返回这个结构的指针表示
  JSValue obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, obj, "ptr",
                    JS_NewInt64(ctx, (int64_t)(uintptr_t)addr_in));
  JS_SetPropertyStr(ctx, obj, "size",
                    JS_NewInt32(ctx, sizeof(struct sockaddr_in)));

  return obj;
}

/* ====================== Socket API 定义 ====================== */

static const JSPropDef js_socket_funcs[] = {
    JS_CFUNC_DEF("socket", 1, js_socket_create),
    JS_CFUNC_DEF("close", 1, js_socket_close),
    JS_CFUNC_DEF("shutdown", 1, js_socket_shutdown),
    JS_CFUNC_DEF("connect", 4, js_socket_connect),
    JS_CFUNC_DEF("bind", 3, js_socket_bind),
    JS_CFUNC_DEF("listen", 2, js_socket_listen),
    JS_CFUNC_DEF("accept", 1, js_socket_accept),
    JS_CFUNC_DEF("getsockname", 1, js_socket_getsockname),
    JS_CFUNC_DEF("getpeername", 1, js_socket_getpeername),
    JS_CFUNC_DEF("socketpair", 3, js_socket_socketpair),
    JS_CFUNC_DEF("setsockopt", 5, js_socket_setsockopt),
    JS_CFUNC_DEF("getsockopt", 4, js_socket_getsockopt),
    JS_CFUNC_DEF("send", 3, js_socket_send),
    JS_CFUNC_DEF("recv", 3, js_socket_recv),
    JS_CFUNC_DEF("sendto", 5, js_socket_sendto),
    JS_CFUNC_DEF("recvfrom", 3, js_socket_recvfrom),
    JS_CFUNC_DEF("inet_addr", 1, js_socket_inet_addr),
    JS_CFUNC_DEF("ntohl", 1, js_socket_ntohl),
    JS_CFUNC_DEF("make_sockaddr_in", 3, js_socket_make_sockaddr_in),
    JS_PROP_END,
};

/* ====================== Socket 类定义 ====================== */

typedef struct {
  int x;
  int y;
} SocketData;

// Socket 类的构造函数
static JSValue js_socket_constructor(JSContext* ctx, JSValue* this_val,
                                     int argc, JSValue* argv) {
  JSValue obj;
  SocketData* d;

  if (!(argc & FRAME_CF_CTOR))
    return JS_ThrowTypeError(ctx, "must be called with new");
  argc &= ~FRAME_CF_CTOR;
  obj = JS_NewObjectClassUser(ctx, JS_CLASS_SOCKET);
  d = malloc(sizeof(*d));
  JS_SetOpaque(ctx, obj, d);
  if (JS_ToInt32(ctx, &d->x, argv[0])) return JS_EXCEPTION;
  if (JS_ToInt32(ctx, &d->y, argv[1])) return JS_EXCEPTION;
  return obj;
}

// Socket 类的析构函数
static void js_socket_finalizer(JSContext* ctx, void* opaque) {
  // 释放 socket 资源
  SocketData* d = opaque;
  free(d);
}

// Socket 类原型
static const JSPropDef js_socket_proto[] = {
    JS_PROP_END,
};

// 定义全局 Socket 命名空间
static const JSPropDef js_socket[] = {
    JS_PROP_DOUBLE_DEF("TCP", 0, 0),
    JS_PROP_DOUBLE_DEF("UDP", 1, 0),
    JS_CFUNC_DEF("socket", 1, js_socket_create),
    JS_CFUNC_DEF("close", 1, js_socket_close),
    JS_CFUNC_DEF("shutdown", 1, js_socket_shutdown),
    JS_CFUNC_DEF("connect", 4, js_socket_connect),
    JS_CFUNC_DEF("bind", 3, js_socket_bind),
    JS_CFUNC_DEF("listen", 2, js_socket_listen),
    JS_CFUNC_DEF("accept", 1, js_socket_accept),
    JS_CFUNC_DEF("getsockname", 1, js_socket_getsockname),
    JS_CFUNC_DEF("getpeername", 1, js_socket_getpeername),
    JS_CFUNC_DEF("socketpair", 3, js_socket_socketpair),
    JS_CFUNC_DEF("setsockopt", 5, js_socket_setsockopt),
    JS_CFUNC_DEF("getsockopt", 4, js_socket_getsockopt),
    JS_CFUNC_DEF("send", 3, js_socket_send),
    JS_CFUNC_DEF("recv", 3, js_socket_recv),
    JS_CFUNC_DEF("sendto", 5, js_socket_sendto),
    JS_CFUNC_DEF("recvfrom", 3, js_socket_recvfrom),
    JS_CFUNC_DEF("inet_addr", 1, js_socket_inet_addr),
    JS_CFUNC_DEF("ntohl", 1, js_socket_ntohl),
    JS_CFUNC_DEF("make_sockaddr_in", 3, js_socket_make_sockaddr_in),
    JS_PROP_END,
};

static const JSClassDef js_socket_class =
    JS_CLASS_DEF("Socket", 2, js_socket_constructor, JS_CLASS_SOCKET, js_socket,
                 js_socket_proto, NULL, js_socket_finalizer);

// 注册 Socket API 到 JS（导出函数，不使用 static）
void js_module_register_socket_api(JSContext* ctx) {
  if (!ctx) return;

  // 获取全局对象
  JSValue global_obj = JS_GetGlobalObject(ctx);

  // 创建 Socket 对象
  JSValue socket_obj = JS_NewObject(ctx);
  JSValue yui_obj = JS_GetPropertyStr(ctx, global_obj, "YUI");
  if (JS_IsUndefined(yui_obj) || JS_IsNull(yui_obj)) {
    printf("JS(Socket): Socket object not found, will be created by js_yuistdlib\n");

    // 注册 Socket 常量
    JS_SetPropertyStr(ctx, socket_obj, "TCP", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, socket_obj, "UDP", JS_NewInt32(ctx, 1));

    // 注册 Socket 方法 - 使用正确的 mquickjs API
    JS_SetPropertyStr(ctx, socket_obj, "socket",
                      JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "close",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_close, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "shutdown",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_shutdown, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "connect",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_connect, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "bind",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_bind, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "listen",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_listen, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "accept",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_accept, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "getsockname",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_getsockname, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "getpeername",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_getpeername, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "socketpair",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_socketpair, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "setsockopt",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_setsockopt, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "getsockopt",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_getsockopt, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "send",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_send, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "recv",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_recv, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "sendto",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_sendto, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "recvfrom",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_recvfrom, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "inet_addr",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_inet_addr, JS_NULL));
    JS_SetPropertyStr(
        ctx, socket_obj, "ntohl",
        JS_NewCFunctionParams(ctx, JS_CFUNCTION_socket_ntohl, JS_NULL));
    JS_SetPropertyStr(ctx, socket_obj, "make_sockaddr_in",
                      JS_NewCFunctionParams(
                          ctx, JS_CFUNCTION_socket_make_sockaddr_in, JS_NULL));

  } else {
    printf("JS(Socket): Socket object exists, functions should be available\n");
  }

  // 将 Socket 对象添加到全局
  // JS_SetPropertyStr(ctx, global_obj, "Socket", socket_obj);

  printf("JS(Socket): Registered Socket API functions\n");
}

#ifdef CONFIG_CLASS_SOCKET
#include "js_socket.h"
#endif
