#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "native_socket.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
#include "../../../socket/socket.h"
#define in_addr_t uint32_t

#define V_INT    1
#else
#include "../../../socket/socket.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#endif


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
extern ssize_t _read(int fildes, void *buf, size_t nbyte);
extern ssize_t _write(int fd, const char *buf, size_t nbyte);


/*=====socket native functions=========*/
var_t* native_socket_socket(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;

	int type = get_int(env, "type");
	int fd = -1;
	if(type == 0) { //tcp
		fd = _socket(PF_INET, SOCK_STREAM, 0);
	}
	else {
		fd = _socket(PF_INET, SOCK_DGRAM, 0);
	}

	return var_new_int(vm, fd);
}

var_t* native_socket_close(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;

	int fd = get_int(env, "fd");

	_close(fd);
	return NULL;
}

var_t* native_socket_shutdown(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;

	int fd = get_int(env, "fd");
	_shutdown(fd, SHUT_RDWR);
	return NULL;
}

var_t* native_socket_connect(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;

	int fd = get_int(env, "fd");
	const char* host = get_str(env, "host");
	int port = get_int(env, "port");
	int timeout = get_int(env, "timeout");

	int res = -1;
	if(fd < 0 || port < 0)
		return var_new_int(vm, res);

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
		return var_new_int(vm, -1);
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
		}
		else {
			unsigned long ul = 1;
			//ioctl(fd, FIONBIO, &ul); //TODO

			if(_connect(fd, rp->ai_addr, rp->ai_addrlen) < 0) {
				int error=-1, len;

				struct timeval tm;
				tm.tv_sec = timeout;
				tm.tv_usec = 0;

				fd_set set;
				FD_ZERO(&set);
				FD_SET(fd, &set);

				if( select(fd+1, NULL, &set, NULL, &tm) > 0) {
					_getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
					if(error == 0) {
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
			//ioctl(fd, FIONBIO, &ul); //TODO
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

	return var_new_int(vm, res);
}

var_t* native_socket_bind(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;

	int fd = get_int(env, "fd");
	const char* host = get_str(env, "host");
	int port = get_int(env, "port");

	int res = -1;
	if(fd < 0 || port <= 0) 
		return var_new_int(vm, res);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port=htons(port);
	if(host[0] != 0)
		addr.sin_addr.s_addr = _inet_addr(host);

	res = _bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr));
	return var_new_int(vm, res);
}

var_t* native_socket_accept(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;

	int fd = get_int(env, "fd");
	struct sockaddr_in in;
	memset(&in, 0, sizeof(in));
	in.sin_family = AF_INET;
	socklen_t size = sizeof(struct sockaddr);

	int cid = _accept(fd, (struct sockaddr *)&in, &size);
	return var_new_int(vm, cid);
}

var_t* native_socket_listen(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;

	int fd = get_int(env, "fd");
	int backlog = get_int(env, "backlog");

	int res = _listen(fd, backlog);
	return var_new_int(vm, res);
}

var_t* native_socket_write(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;

	int fd = get_int(env, "fd");

	node_t* n = var_find(env, "bytes");
	if(n == NULL || n->var == NULL || n->var->size == 0)
		return NULL;
	var_t* bytes = n->var;

	int bytesSize = bytes->size;
	if(bytes->type == V_STRING)
		bytesSize--;

	int size = get_int(env, "size");
	if(size == 0 || size > bytesSize)
		size = bytesSize;

	int res = _write(fd, (const char*)bytes->value, size);
	return var_new_int(vm, res);
}

var_t* native_socket_read(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;

	int fd = get_int(env, "fd");

	node_t* n = var_find(env, "bytes");
	if(n == NULL || n->var == NULL || n->var->size == 0)
		return NULL;
	var_t* bytes = n->var;

	int bytesSize = bytes->size;

	int size = get_int(env, "size");
	if(size == 0 || size > bytesSize)
		size = bytesSize;

	char* s = (char*)bytes->value;
	int res = _read(fd, s, size);
	if(res >= 0)
		s[res] = 0;
	return var_new_int(vm, res);
}

var_t* native_socket_setTimeout(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;

	int fd = get_int(env, "fd");
	int timeout = get_int(env, "timeout");

	struct timeval tv_timeout;
	tv_timeout.tv_sec = timeout;
	tv_timeout.tv_usec = 0;

	int res = _setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (void *) &tv_timeout, sizeof(struct timeval));
	return var_new_int(vm, res);
}

var_t* native_socket_inet_addr(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	const char* strptr = get_str(env, "strptr");
	if (!strptr) return var_new_int(vm, -1);
	
	uint32_t result = _inet_addr(strptr);
	return var_new_int(vm, result);
}

var_t* native_socket_ntohl(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	uint32_t netlong = get_int(env, "netlong");
	uint32_t result = _ntohl(netlong);
	return var_new_int(vm, result);
}

var_t* native_socket_getsockname(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int fd = get_int(env, "fd");
	if (fd < 0) return var_new_int(vm, -1);
	
	struct sockaddr_in addr;
	socklen_t len = sizeof(struct sockaddr_in);
	int result = _getsockname(fd, (struct sockaddr *)&addr, &len);
	
	if (result == 0) {
		// 返回包含地址和端口的对象
		var_t* obj = var_new_obj(vm, NULL, NULL);
		var_t* ip_var = var_new_str(vm, inet_ntoa(addr.sin_addr));
		var_t* port_var = var_new_int(vm, _ntohl(addr.sin_port));
		
		var_add(obj, "ip", ip_var);
		var_add(obj, "port", port_var);
		
		return obj;
	}
	
	return var_new_int(vm, result);
}

var_t* native_socket_getpeername(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int fd = get_int(env, "fd");
	if (fd < 0) return var_new_int(vm, -1);
	
	struct sockaddr_in addr;
	socklen_t len = sizeof(struct sockaddr_in);
	int result = _getpeername(fd, (struct sockaddr *)&addr, &len);
	
	if (result == 0) {
		// 返回包含地址和端口的对象
		var_t* obj = var_new_obj(vm, NULL, NULL);
		var_t* ip_var = var_new_str(vm, inet_ntoa(addr.sin_addr));
		var_t* port_var = var_new_int(vm, _ntohl(addr.sin_port));
		
		var_add(obj, "ip", ip_var);
		var_add(obj, "port", port_var);
		
		return obj;
	}
	
	return var_new_int(vm, result);
}

var_t* native_socket_socketpair(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int domain = get_int(env, "domain");
	int type = get_int(env, "type");
	int protocol = get_int(env, "protocol");
	
	int sv[2];
	int result = _socketpair(domain, type, protocol, sv);
	
	if (result == 0) {
		// 返回包含两个套接字描述符的数组
		var_t* arr = var_new_array(vm);
		var_array_add(arr, var_new_int(vm, sv[0]));
		var_array_add(arr, var_new_int(vm, sv[1]));
		
		return arr;
	}
	
	return var_new_int(vm, result);
}

var_t* native_socket_setsockopt(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int fd = get_int(env, "fd");
	int level = get_int(env, "level");
	int option_name = get_int(env, "option_name");
	
	node_t* n = var_find(env, "option_value");
	if(n == NULL || n->var == NULL)
		return var_new_int(vm, -1);
	
	var_t* option_value = n->var;
	int option_len = get_int(env, "option_len");
	
	int result;
	if (option_value->type == V_INT) {
		int ival = var_get_int(option_value);
		result = _setsockopt(fd, level, option_name, &ival, sizeof(int));
	} else if (option_value->type == V_STRING) {
		const char* sval = var_get_str(option_value);
		result = _setsockopt(fd, level, option_name, sval, strlen(sval));
	} else {
		return var_new_int(vm, -1);
	}
	
	return var_new_int(vm, result);
}

var_t* native_socket_getsockopt(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int fd = get_int(env, "fd");
	int level = get_int(env, "level");
	int option_name = get_int(env, "option_name");
	int option_len = get_int(env, "option_len");
	
	char option_value[256];
	socklen_t len = option_len;
	
	if (option_len > sizeof(option_value))
		len = sizeof(option_value);
	
	int result = _getsockopt(fd, level, option_name, option_value, &len);
	
	if (result == 0) {
		// 返回选项值
		return var_new_str2(vm, option_value, len);
	}
	
	return var_new_int(vm, result);
}

var_t* native_socket_sendmsg(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int fd = get_int(env, "fd");
	int flags = get_int(env, "flags");
	
	// 简化实现，仅处理消息中的数据部分
	node_t* n = var_find(env, "message");
	if(n == NULL || n->var == NULL)
		return var_new_int(vm, -1);
	
	var_t* message = n->var;
	if(message->type != V_STRING)
		return var_new_int(vm, -1);
	
	int result = _send(fd, (const char*)message->value, message->size, flags);
	return var_new_int(vm, result);
}

var_t* native_socket_recvmsg(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int fd = get_int(env, "fd");
	int flags = get_int(env, "flags");
	int size = get_int(env, "size");
	
	if (size <= 0)
		size = 1024; // 默认缓冲区大小
	
	char* buffer = (char*)malloc(size);
	if (!buffer)
		return var_new_int(vm, -1);
	
	int result = _recv(fd, buffer, size-1, flags);
	
	if (result >= 0) {
		buffer[result] = 0; // 确保字符串结束
		var_t* str = var_new_str2(vm, buffer, result);
		free(buffer);
		return str;
	}
	
	free(buffer);
	return var_new_int(vm, result);
}

var_t* native_socket_send(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int fd = get_int(env, "fd");
	int flags = get_int(env, "flags");
	
	node_t* n = var_find(env, "message");
	if(n == NULL || n->var == NULL)
		return var_new_int(vm, -1);
	
	var_t* message = n->var;
	if(message->type != V_STRING)
		return var_new_int(vm, -1);
	
	int result = _send(fd, (const char*)message->value, message->size, flags);
	return var_new_int(vm, result);
}

var_t* native_socket_recv(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int fd = get_int(env, "fd");
	int flags = get_int(env, "flags");
	int size = get_int(env, "size");
	
	if (size <= 0)
		size = 1024; // 默认缓冲区大小
	
	char* buffer = (char*)malloc(size);
	if (!buffer)
		return var_new_int(vm, -1);
	
	int result = _recv(fd, buffer, size-1, flags);
	
	if (result >= 0) {
		buffer[result] = 0; // 确保字符串结束
		var_t* str = var_new_str2(vm, buffer, result);
		free(buffer);
		return str;
	}
	
	free(buffer);
	return var_new_int(vm, result);
}

var_t* native_socket_sendto(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int fd = get_int(env, "fd");
	int flags = get_int(env, "flags");
	
	node_t* n_msg = var_find(env, "message");
	if(n_msg == NULL || n_msg->var == NULL)
		return var_new_int(vm, -1);
	
	var_t* message = n_msg->var;
	if(message->type != V_STRING)
		return var_new_int(vm, -1);
	
	node_t* n_host = var_find(env, "host");
	const char* host = n_host && n_host->var ? (char*)n_host->var->value : "127.0.0.1";
	
	int port = get_int(env, "port");
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = _inet_addr(host);
	addr.sin_port = htons(port);
	
	int result = _sendto(fd, (const char*)message->value, message->size, flags, 
					 (struct sockaddr *)&addr, sizeof(struct sockaddr));
	
	return var_new_int(vm, result);
}

var_t* native_socket_recvfrom(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int fd = get_int(env, "fd");
	int flags = get_int(env, "flags");
	int size = get_int(env, "size");
	
	if (size <= 0)
		size = 1024; // 默认缓冲区大小
	
	char* buffer = (char*)malloc(size);
	if (!buffer)
		return var_new_int(vm, -1);
	
	struct sockaddr_in from_addr;
	socklen_t from_len = sizeof(from_addr);
	
	int result = _recvfrom(fd, buffer, size-1, flags, 
				(struct sockaddr *)&from_addr, &from_len);
	
	if (result >= 0) {
		buffer[result] = 0; // 确保字符串结束
		
		// 创建返回对象，包含数据、发送方IP和端口
		var_t* obj = var_new_obj(vm, NULL, NULL);
		var_t* data_var = var_new_str2(vm, buffer, result);
		var_t* ip_var = var_new_str(vm, inet_ntoa(from_addr.sin_addr));
		var_t* port_var = var_new_int(vm, _ntohl(from_addr.sin_port));
		
		var_add(obj, "data", data_var);
		var_add(obj, "ip", ip_var);
		var_add(obj, "port", port_var);
		
		free(buffer);
		return obj;
	}
	
	free(buffer);
	return var_new_int(vm, result);
}

var_t* native_socket_make_sockaddr_in(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	
	int family = get_int(env, "family");
	int addr = get_int(env, "addr");
	int port = get_int(env, "port");
	
	// 分配并初始化sockaddr_in结构
	struct sockaddr_in* addr_in = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	if (!addr_in)
		return var_new_null(vm);
	
	memset(addr_in, 0, sizeof(struct sockaddr_in));
	addr_in->sin_family = family;
	addr_in->sin_addr.s_addr = htonl(addr);
	addr_in->sin_port = htons(port);
	
	// 在JS中返回这个结构的指针表示
		var_t* obj = var_new_obj(vm, NULL, NULL);
		var_t* ptr_var = var_new_int(vm, (uintptr_t)addr_in);
		var_t* size_var = var_new_int(vm, sizeof(struct sockaddr_in));
		
		var_add(obj, "ptr", ptr_var);
		var_add(obj, "size", size_var);
	
	return obj;
}

#define CLS_SOCKET "Socket"

void reg_native_socket(vm_t* vm) {
	var_t* cls = vm_new_class(vm, CLS_SOCKET);
	vm_reg_var(vm, cls, "TCP", var_new_int(vm, 0), true);
	vm_reg_var(vm, cls, "UDP", var_new_int(vm, 1), true);

	vm_reg_static(vm, cls, "socket(type)", native_socket_socket, NULL);
	vm_reg_static(vm, cls, "close(fd)", native_socket_close, NULL);
	vm_reg_static(vm, cls, "shutdown(fd)", native_socket_shutdown, NULL);
	vm_reg_static(vm, cls, "connect(fd, host, port, timeout)", native_socket_connect, NULL);
	vm_reg_static(vm, cls, "bind(fd, host, port)", native_socket_bind, NULL);
	vm_reg_static(vm, cls, "accept(fd)", native_socket_accept, NULL);
	vm_reg_static(vm, cls, "listen(fd, backlog)", native_socket_listen, NULL);
	vm_reg_static(vm, cls, "write(fd, bytes, size)", native_socket_write, NULL);
	vm_reg_static(vm, cls, "read(fd, bytes, size)", native_socket_read, NULL);
	vm_reg_static(vm, cls, "setTimeout(fd, timeout)", native_socket_setTimeout, NULL);

	vm_reg_static(vm, cls, "inetaddr(strptr)", native_socket_inet_addr, NULL);
	vm_reg_static(vm, cls, "ntohl(netlong)", native_socket_ntohl, NULL);
	vm_reg_static(vm, cls, "getsockname(fd)", native_socket_getsockname, NULL);
	vm_reg_static(vm, cls, "getpeername(fd)", native_socket_getpeername, NULL);
	vm_reg_static(vm, cls, "socketpair(domain, type, protocol)", native_socket_socketpair, NULL);
	vm_reg_static(vm, cls, "setsockopt(fd, level, option_name, option_value, option_len)", native_socket_setsockopt, NULL);
	vm_reg_static(vm, cls, "getsockopt(fd, level, option_name, option_len)", native_socket_getsockopt, NULL);
	vm_reg_static(vm, cls, "sendmsg(fd, message, flags)", native_socket_sendmsg, NULL);
	vm_reg_static(vm, cls, "recvmsg(fd, flags)", native_socket_recvmsg, NULL);
	vm_reg_static(vm, cls, "send(fd, message, flags)", native_socket_send, NULL);
	vm_reg_static(vm, cls, "recv(fd, flags, size)", native_socket_recv, NULL);
	vm_reg_static(vm, cls, "sendto(fd, message, flags, host, port)", native_socket_sendto, NULL);
	vm_reg_static(vm, cls, "recvfrom(fd, flags, size)", native_socket_recvfrom, NULL);
	vm_reg_static(vm, cls, "make_sockaddr_in(family, addr, port)", native_socket_make_sockaddr_in, NULL);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

