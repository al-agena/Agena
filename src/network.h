#ifndef NETWORK_H
#define NETWORK_H

#ifdef _WIN32
  #include "agena.h"
  #include "agnxlib.h"
  #include <winsock2.h>
  #include <ws2tcpip.h>

  typedef int (WSAAPI *pfn_getaddrinfo) (const char*, const char*, const struct addrinfo*, struct addrinfo**);
  typedef void (WSAAPI *pfn_freeaddrinfo) (struct addrinfo*);
  typedef int (WSAAPI *pfn_getnameinfo) (const struct sockaddr*, socklen_t, char*, DWORD, char*, DWORD, int);

  extern pfn_getaddrinfo  fp_getaddrinfo;
  extern pfn_freeaddrinfo fp_freeaddrinfo;
  extern pfn_getnameinfo  fp_getnameinfo;

  int init_network_api (lua_State *L, const char *procname);
  int safe_getaddrinfo (const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res);
  void safe_freeaddrinfo (struct addrinfo *res);
  int safe_getnameinfo (const struct sockaddr *sa, socklen_t salen, char *host, DWORD hostlen, char *serv, DWORD servlen, int flags);

#else

  #define safe_getaddrinfo getaddrinfo
  #define safe_freeaddrinfo freeaddrinfo
  #define safe_getnameinfo getnameinfo

#endif
#endif
