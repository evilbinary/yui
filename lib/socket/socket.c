#include "socket.h"
#ifndef WIN32
#include <netdb.h>
#endif

/* extern int Sactivate_thread(void); */
/* extern Sdeactivate_thread(void); */
/* extern Slock_object(void*); */
/* extern Sunlock_object(void*); */


#define __socketcall 

// 字节序交换：与 lwip 的 lwip_htons 等语义一致（小端主机），不依赖任何库
uint16_t _htons(uint16_t v){
  return (uint16_t)((v >> 8) | (v << 8));
}

uint32_t _htonl(uint32_t v){
  return ((v & 0xFF) << 24) | (((v >> 8) & 0xFF) << 16) | (((v >> 16) & 0xFF) << 8) | (v >> 24);
}

uint16_t _ntohs(uint16_t v){
  return _htons(v);
}

char* _inet_ntoa(struct in_addr in){
  static char buf[16];
  const unsigned char* p = (const unsigned char*)&in.s_addr;
  snprintf(buf, sizeof(buf), "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
  return buf;
}

uint32_t _ntohl(uint32_t netlong){
  return _htonl(netlong);
}

in_addr_t _inet_addr(const char* strptr){
  unsigned a, b, c, d;
  if (sscanf(strptr, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 &&
      a < 256 && b < 256 && c < 256 && d < 256) {
    return _htonl((a << 24) | (b << 16) | (c << 8) | d);
  }
  return (in_addr_t)-1;
}

// 仅支持 IPv4 数字地址的主机名解析（嵌入式无 DNS 依赖）
int _getaddrinfo(const char* nodename, const char* servname,
                 const struct addrinfo* hints, struct addrinfo** res){
  unsigned a, b, c, d;
  unsigned port = 0;
  struct addrinfo* ai;
  struct sockaddr_in* sa;
  if (nodename == NULL || sscanf(nodename, "%u.%u.%u.%u", &a, &b, &c, &d) != 4 ||
      a > 255 || b > 255 || c > 255 || d > 255) {
    return -2; // EAI_NONAME
  }
  if (servname != NULL) {
    port = (unsigned)atoi(servname);
  }
  sa = (struct sockaddr_in*)malloc(sizeof(*sa));
  ai = (struct addrinfo*)malloc(sizeof(*ai));
  if (!sa || !ai) {
    free(sa);
    free(ai);
    return -4; // EAI_MEMORY
  }
  memset(sa, 0, sizeof(*sa));
  sa->sin_family = AF_INET;
  sa->sin_port = _htons((uint16_t)port);
  sa->sin_addr.s_addr = _htonl((a << 24) | (b << 16) | (c << 8) | d);
  memset(ai, 0, sizeof(*ai));
  ai->ai_family = AF_INET;
  ai->ai_socktype = hints ? hints->ai_socktype : SOCK_STREAM;
  ai->ai_protocol = hints ? hints->ai_protocol : 0;
  ai->ai_addrlen = sizeof(*sa);
  ai->ai_addr = (struct sockaddr*)sa;
  *res = ai;
  return 0;
}

const char* _gai_strerror(int errcode){
  switch (errcode) {
  case 0:   return "Success";
  case -2:  return "Name or service not known";
  case -3:  return "Unknown server host";
  case -4:  return "Memory allocation failure";
  default:  return "Unknown error";
  }
}

void _freeaddrinfo(struct addrinfo* res){
  if (!res) {
    return;
  }
  free(res->ai_addr);
  free(res);
}

void* make_sockaddr_in(int family,int addr,int port){
  struct sockaddr_in* addr_in=malloc(sizeof(struct sockaddr_in));
  memset(addr_in, 0, sizeof(struct sockaddr_in));  
  addr_in->sin_family = family;  
  addr_in->sin_addr.s_addr = _htonl(addr);//IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址。  
  addr_in->sin_port = _htons(port);//设置的端口为DEFAULT_PORT
  //printf("struct sockaddr_in=%d\n",sizeof(struct sockaddr_in));


  
  return addr_in;
}
#ifndef WIN32
// On Windows, _close conflicts with MSVCRT, so we don't define it
// Instead, we'll use closesocket directly for sockets
int _close(int fd){
  return close(fd);
}
#endif
 size_t
 _strlen(const char *s){
   return strlen(s);
 }

 ssize_t
 _read(int fildes, void *buf, size_t nbyte){
   //printf("read=>%c nbyte=%d\n",*(char*)buf,nbyte);
#ifdef WIN32
   return recv(fildes,buf,nbyte,0);
  #else
   return read(fildes,buf,nbyte);
#endif
 }


ssize_t
_write_all(int fd, const char *buf,size_t  nbyte){
  
  ssize_t i, m;
  //printf("content %d=>%s\n",nbyte,buf,strlen((char*)buf));
    //printf("len at %s\n",(char*)&buf[nbyte]);
  //nbyte=strlen(buf)-1;
  //printf("len===>%d\n",nbyte);
  
    m = nbyte;
    while (m > 0) {
      #ifdef WIN32
      if ((i = send(fd, buf, m,0)) < 0) { 
      #else
      if ((i = write(fd, buf, m)) < 0) {
	  #endif
            if (errno != EAGAIN && errno != EINTR)
                return i;
        } else {
            m -= i;
            buf += i;
        }
      //printf("i=>%d\n",i);
	      
    }
    return nbyte;
    
    //return write(fildes,buf,nbyte);
}

ssize_t
_write(int fd, const char *buf, size_t nbyte){
     

  ssize_t i, m;
  //printf("content %d=>%s\n",nbyte,buf,strlen((char*)buf));
    //printf("len at %s\n",(char*)&buf[nbyte]);
  fflush(stdout);  
    m = nbyte;
    while (m > 0) {
#ifdef WIN32
      if ((i = send(fd, buf, m,0)) < 0) {
#else
        if ((i = write(fd, buf, m)) < 0) {
#endif
	  if (errno != EAGAIN && errno != EINTR)
	    return i;
        } else {
            m -= i;
            buf += i;
        }
	//printf("i=>%d\n",i);
	      
    }
    return nbyte;


    
    //return write(fildes,buf,nbyte);
}

FILE * _fdopen(int fildes, const char *mode){
  return fdopen(fildes,mode);
}

int     _accept(int socket, struct sockaddr *address,
	       socklen_t *address_len){
  //Sdeactivate_thread();
  int ret= accept(socket,address,address_len);
  //Sactivate_thread();
  return ret;
}
int     _bind(int socket, const struct sockaddr *address,
             socklen_t address_len){
  // Sdeactivate_thread();
  int ret=  bind(socket,address,address_len);
  //Sactivate_thread();
  return ret;
}
int     _connect(int socket, const struct sockaddr *address,
		socklen_t address_len){
  //Sdeactivate_thread();
  int ret= connect(socket,address,address_len);
  //Sactivate_thread();
  return ret;
}
int     _getpeername(int socket, struct sockaddr *address,
		    socklen_t *address_len){
  return getpeername(socket,address,address_len);
}
int     _getsockname(int socket, struct sockaddr *address,
		    socklen_t *address_len){
  return getsockname(socket,address,address_len);
}
int     _getsockopt(int socket, int level, int option_name,
		   void *option_value, socklen_t *option_len){
  return getsockopt(socket,level,option_name,option_value,option_len);
  
}
int     _listen(int socket, int backlog){
  return listen(socket,backlog);
}
ssize_t _recv(int socket, void *buffer, size_t length, int flags){;
  //printf("recv start====%d %s\n",errno,strerror(errno));
  /*Slock_object(socket); 
   Slock_object(buffer); 
   Slock_object(length); 
   Slock_object(flags); */
  //Sdeactivate_thread();
   ssize_t ret=recv(socket, buffer,length,flags);
   //Sactivate_thread();
  /*Sunlock_object(socket);
   Sunlock_object(buffer);
   Sunlock_object(length);
   Sunlock_object(flags);*/
   //printf("recv end====%d\n",ret);

   return ret;
}
ssize_t _recvfrom(int socket, void *buffer, size_t length,
		 int flags, struct sockaddr *address, socklen_t *address_len){
  return  recvfrom( socket,buffer, length,
		    flags, address, address_len);
}
ssize_t _recvmsg(int socket, struct msghdr *message, int flags){
#if defined(WIN32) || defined(ESP_PLATFORM)
  (void)socket; (void)message; (void)flags;
  return -1;
#else
  return  recvmsg( socket,message, flags);
#endif
}
ssize_t _send(int socket, const void *message, size_t length, int flags){
  //printf("send start====%d %s\n",errno,strerror(errno));
  /* Slock_object(socket); */
  /* Slock_object(message); */
  /* Slock_object(length); */
  /* Slock_object(flags); */
  //Sdeactivate_thread();
  ssize_t ret=send( socket, message,length,flags);
  //Sactivate_thread();
  /*  Sunlock_object(socket); */
  /* Sunlock_object(message); */
  /* Sunlock_object(length); */
  /* Sunlock_object(flags); */
  //printf("send end====%d\n",ret);
  return ret;
}
ssize_t _sendmsg(int socket, const struct msghdr *message, int flags){
#if defined(WIN32) || defined(ESP_PLATFORM)
  (void)socket; (void)message; (void)flags;
  return -1;
#else
  return  sendmsg( socket, message,flags);
#endif
}
ssize_t _sendto(int socket, const void *message, size_t length, int flags,
	       const struct sockaddr *dest_addr, socklen_t dest_len){
  return sendto(socket,message, length,flags,
		dest_addr, dest_len);
}
int     _setsockopt(int socket, int level, int option_name,
		   const void *option_value, socklen_t option_len){
  return  setsockopt(socket,level,option_name,
		     option_value,option_len);
}
int     _shutdown(int socket, int how){
  #ifdef WIN32
    WSACleanup();
    #endif
  return shutdown(socket,how);
}
int     _socket(int domain, int type, int protocol){
#ifdef WIN32
   WSADATA wsadata;
  if(WSAStartup(MAKEWORD(1,1),&wsadata)==SOCKET_ERROR)
  {
    printf("WSAStartup() fail\n");
    exit(0);
  }
#endif
  return socket(domain,type,protocol);
}
int     _socketpair(int domain, int type, int protocol,
		   int socket_vector[2]){
    (void)domain; (void)type; (void)protocol; (void)socket_vector;
    errno = ENOSYS;
    return -1;
}
