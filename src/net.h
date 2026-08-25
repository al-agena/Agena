#ifndef net_h
#define net_h

/* default address */
#define NET_ADDRESS "localhost"
#ifndef __OS2__
#define NET_ADDRESS6 "::1"
#else
/* just to avoid compiler warnings */
#define NET_ADDRESS6 "localhost"
#define tonumericip6   tonumericip
#ifndef AF_INET6
#define AF_INET6 24  /* taken from sys/socket.h */
/* #define TCPV40HDRS */  /* uncomment if you want to use the old TCP/IP stack */
#endif
#endif

/* default port number */
#define NET_PORT 1234

#ifdef _WIN32
#include <mswsock.h>
#include <ws2tcpip.h>
#endif

/* in Linux socklen_t is overwritten although assigned */
#ifndef __linux__
#ifndef socklen_t
#ifdef __APPLE__
#define socklen_t unsigned int
#else
#define socklen_t int
#endif
#endif
#endif

/* misc. definitions */
#ifdef _WIN32
#define close_socket(x) closesocket((x))

/* taken from: https://gitlab.eterfund.ru/wine/wine-cw/blob/d1f6a5a376515654636c1aa51cedb4f2ec6de318/include/ws2def.h */
#ifndef WSA_CMSG_DATA
#define WSA_CMSG_DATA(cmsg)     ((UCHAR*)((WSACMSGHDR*)(cmsg)+1))
#endif
#ifndef WSA_CMSG_FIRSTHDR
#define WSA_CMSG_FIRSTHDR(mhdr) ((mhdr)->Control.len >= sizeof(WSACMSGHDR) ? (WSACMSGHDR *) (mhdr)->Control.buf : (WSACMSGHDR *) 0)
#endif
#ifndef WSA_CMSG_ALIGN
#define WSA_CMSG_ALIGN(len)     (((len) + sizeof(SIZE_T) - 1) & ~(sizeof(SIZE_T) - 1))
#endif
#ifndef WSA_CMSG_NXTHDR
#define WSA_CMSG_NXTHDR(mhdr,cmsg) \
        (!(cmsg) ? WSA_CMSG_FIRSTHDR(mhdr) : \
         ((mhdr)->Control.len < sizeof(WSACMSGHDR) ? NULL : \
         (((unsigned char*)(((WSACMSGHDR*)((unsigned char*)cmsg + WSA_CMSG_ALIGN(cmsg->cmsg_len)))+1) > ((unsigned char*)(mhdr)->Control.buf + (mhdr)->Control.len)) ? NULL : \
          (((unsigned char*)cmsg + WSA_CMSG_ALIGN(cmsg->cmsg_len)+WSA_CMSG_ALIGN(((WSACMSGHDR*)((unsigned char*)cmsg + WSA_CMSG_ALIGN(cmsg->cmsg_len)))->cmsg_len) > ((unsigned char*)(mhdr)->Control.buf + (mhdr)->Control.len)) ? NULL : \
           (WSACMSGHDR*)((unsigned char*)cmsg + WSA_CMSG_ALIGN(cmsg->cmsg_len))))))
#endif
/* end */

/* Taken from: https://github.com/sryze/ping/blob/master/src/ping.c
   Copyright (c) 2018-2021 Sergey Zolotarev, MIT licence */
#undef CMSG_SPACE
#define CMSG_SPACE WSA_CMSG_SPACE
#undef CMSG_FIRSTHDR
#define CMSG_FIRSTHDR WSA_CMSG_FIRSTHDR
#undef CMSG_NXTHDR
#define CMSG_NXTHDR WSA_CMSG_NXTHDR
#undef CMSG_DATA
#define CMSG_DATA WSA_CMSG_DATA

typedef SOCKET socket_t;
typedef WSAMSG msghdr_t;
typedef WSACMSGHDR cmsghdr_t;

/* Pointer to the WSARecvMsg() function. It must be obtained at runtime... */
/* static LPFN_WSARECVMSG WSARecvMsg; */

/* https://stackoverflow.com/questions/37334871/function-to-retrieve-the-header-destination-address-from-a-packet-in-windows-xp */
#ifndef WSAID_WSARECVMSG
#define WSAID_WSARECVMSG {0xf689d7c8,0x6f1f,0x436b,{0x8a,0x53,0xe5,0x4f,0xe3,0x51,0xc3,0x22}}
#endif

#elif defined(__OS2__)  /* 2.3.0 RC 2 eCS extension */
#define close_socket(x) soclose((x))
#else
#define close_socket(x) close((x))
#endif


#undef AGN_SOCKET
#undef AGN_PORT
#ifdef _WIN32
#  define AGN_SOCKET SOCKET
#  define AGN_PORT uint16_t
#else
#  define AGN_SOCKET int
#  define AGN_PORT in_port_t
#endif

typedef AGN_SOCKET *AGN_PSOCKET;  /* = `... *p_socket` in LuaSocket 3.1.0 */

#undef AGN_FAMILY
#if defined(_WIN32)       /* 2.3.0 RC 2 eCS change */
#  define AGN_FAMILY int
#elif defined(__OS2__)
#  if !defined __KLIBC__ || defined TCPV40HDRS
typedef unsigned short  sa_family_t;
#  else
typedef unsigned char   sa_family_t;
#  endif
#define AGN_FAMILY  sa_family_t
#else
#  define AGN_FAMILY sa_family_t
#endif

#ifndef _WIN32
#  ifndef INVALID_SOCKET
#    define INVALID_SOCKET ((int)(~0))
#  endif
#endif

#if defined(_WIN32)
#  define SHUTDOWN_RD   SD_RECEIVE
#  define SHUTDOWN_WR   SD_SEND
#  define SHUTDOWN_RDWR SD_BOTH
#elif defined(__OS2__)
#  define SHUTDOWN_RD   0
#  define SHUTDOWN_WR   1
#  define SHUTDOWN_RDWR 2
#else
#  define SHUTDOWN_RD   SHUT_RD
#  define SHUTDOWN_WR   SHUT_WR
#  define SHUTDOWN_RDWR SHUT_RDWR
#endif

#define ADDRESS_LENGTH 64

typedef struct socket_status {
  size_t family;  /* network protocol */
  int blocking;   /* blocking/non-blocking mode */
  int keepalive;  /* keep-alive mode */
  int connected;  /* socket connected ?, needed for Linux would crash in unconnected cases */
  int isserver;   /* server socket ? */
  int shutdown;   /* shutdown mode: SHUT_RD - no more receive's, SHUT_WR, no more send's, SHUT_RDWR - both */
  AGN_PORT port;  /* server port */
  char address[ADDRESS_LENGTH];  /* server address, 64 bytes is enough for IPv4/IPv6 */
} STATUS;


/* implementation of a binary tree */

typedef struct node {
  AGN_SOCKET index;
  STATUS *status;
  struct node *left;
  struct node *right;
} NODE;


typedef struct tree {
  struct node *root;
  unsigned int size;
} TREE;

#endif
