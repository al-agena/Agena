#define network_c
#define LUA_LIB

/* Compatibility functions for Agena to successfully start up in Windows 2000 */

#include "network.h"

#ifdef _WIN32
pfn_getaddrinfo  fp_getaddrinfo  = NULL;
pfn_freeaddrinfo fp_freeaddrinfo = NULL;
pfn_getnameinfo  fp_getnameinfo  = NULL;

int init_network_api (lua_State *L, const char *procname) {
  HMODULE hWs2 = GetModuleHandle("ws2_32.dll");
  if (!hWs2) {
    luaL_error(L, "Error in " LUA_QS ": ws2_32.dll not found.", procname);
    return 0;
  }
  fp_getaddrinfo  = (pfn_getaddrinfo)GetProcAddress(hWs2, "getaddrinfo");
  fp_freeaddrinfo = (pfn_freeaddrinfo)GetProcAddress(hWs2, "freeaddrinfo");
  fp_getnameinfo  = (pfn_getnameinfo)GetProcAddress(hWs2, "getnameinfo");
  if (!fp_getaddrinfo || !fp_freeaddrinfo || !fp_getnameinfo) {
    luaL_error(L, "Error in " LUA_QS ": required network function(s) not found.", procname);
    return 0;
  }
  return 1;
}

/* Die Wrapper-Funktionen hier rein */
int safe_getaddrinfo (const char* node, const char* service,
                      const struct addrinfo* hints, struct addrinfo** res) {
  if (fp_getaddrinfo == NULL) return EAI_FAIL;
  return fp_getaddrinfo(node, service, hints, res);
}

void safe_freeaddrinfo (struct addrinfo *res) {
  if (fp_freeaddrinfo != NULL) fp_freeaddrinfo(res);
}

int safe_getnameinfo (const struct sockaddr *sa, socklen_t salen,
                      char *host, DWORD hostlen,
                      char *serv, DWORD servlen, int flags) {
  if (fp_getnameinfo == NULL) return EAI_FAIL;
  return fp_getnameinfo(sa, (int)salen, host, hostlen, serv, servlen, flags);
}
#endif
