/*
** $Id: net.c, by Alexander Walz - initiated April 05, 2012
** Network library for IPv4 connections
** See Copyright Notice in agena.h
** Largely based on procedures published in Juergen Wolf's book 'C von A bis Z', Galileo Computing, Bonn, 3rd edition 2009
** Without `Beej's Guide to Network Programming - Using Internet Sockets` the IPv6 extension would not have been possible.

** Test it this way:
import net
s := net.open():
net.bind(s, '127.0.0.1', 1300):
net.listen(s):
t, ip, port := net.accept(s):

net.receive(t, true):
#####
import net
d :=net.open()
net.connect(d, '127.0.0.1', 1300):
net.send(d, strings.repeat('#', 1000)):
*/

#if defined(_WIN32) || defined(__unix__) || defined(__linux__) || defined(__APPLE__) || defined(__OS2__)

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* for strerror */
#include <signal.h>  /* 1.6.4, to avoid crashes with closed connections, signal() */

#ifdef _WIN32
#define FD_SETSIZE 4096  /* must be put before the #include of Winsock */
#include <winsock2.h>
#include <io.h>
#include <wininet.h>   /* for InternetGetConnectedState@ */
#include <wspiapi.h>
#include <ws2tcpip.h>

#else  /* non-Windows */
/* header files for UNIX/Linux */
#include <sys/types.h>   /* to be put before sys/socket.h */
#ifdef __OS2__
/* better sure than sorry: on FreeBSD 6.4, <sys/socket.h> defines some macros that assume that NULL is defined. */
#include <stddef.h>
#endif

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>  /* must come before arpa/inet.h */
#include <arpa/inet.h>
#ifdef __OS2__
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include "getaddrinfo.h"
#endif
#include <unistd.h>
#endif

#include "network.h"

/* the following package ini declarations must be included after `#include <` and before `include #` ! */

#define net_c
#define LUA_LIB

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"
#include "agncmpt.h"  /* trunc */
#include "agnhlps.h"
#include "llimits.h"  /* for MAX_INT */
#include "net.h"

#define AGENA_LIBVERSION	"net 2.0.2 for Agena as of August 03, 2025\n"

#if !(defined(__DJGPP__) || defined(__OS2__))
#define AGENA_NETLIBNAME "net"
LUALIB_API int (luaopen_net) (lua_State *L);
#endif


static int so_reuseaddr = 1;

/* 7.6.8: safely copy the contents of string src into the char dest array, null-terminating it and
   truncating after 64 bytes. */
#define COPYADDRESS(dest, src) \
  snprintf((dest), sizeof(dest), "%s", (src))

/*** Auxiliary functions for socket administration ************************* */

struct tree *socketattribs;

/* initialise binary tree, returns tree or NULL in case of errors */
static TREE *treeinit (void) {
  TREE *tree = (TREE *)malloc(sizeof *tree);
  if (tree == NULL) {  /* no memory available */
    return NULL;
  }
  else {  /* initialise */
    tree->root = NULL;
    tree->size = 0;
    return tree;
  }
}

static int treeinsert (TREE *tree, AGN_SOCKET idx, STATUS *data) {
  NODE *node, **neu;
  neu = (NODE **)&tree->root;
  node = (NODE *)tree->root;
  while (node != NULL) {  /* Already exists ? Don't insert duplicates, 7.5.16 fix */
    if (idx == node->index) return 0;
    if (idx > node->index) {
      neu = &node->right;
      node = node->right;
    } else {
      neu = &node->left;
      node = node->left;
    }
  }
  /* Insert new node */
  node = *neu = (NODE *)malloc(sizeof *node);
  if (node == NULL) return 0;
  node->index = idx;
  node->status = (STATUS *)malloc(sizeof *data);
  if (node->status == NULL) { free(node); return 0; }
  tools_memcpy(node->status, data, sizeof *data);
  node->left = node->right = NULL;
  tree->size++;
  return 1;
}

/* search for a socket handle denoted by idx */
static STATUS *treesearch (const TREE *tree, AGN_SOCKET idx) {
  const NODE *node;
  node = (NODE*) tree->root;
  for (;;) {
    if (node == NULL) {
      return NULL;
    }
    if (idx == node->index) {
      return node->status;
    }
    else if(idx > node->index)
      node = node->right;
    else
      node = node->left;
  }
}

/* updates administrative socket data for the handle idx */
static int treeupdate (const TREE *tree, AGN_SOCKET idx, STATUS *data) {
  const NODE *node;
  node = (NODE*) tree->root;
  for (;;) {
    if (node == NULL) {
      return -1;
    }
    if (idx == node->index) {
      tools_memcpy(node->status, data, sizeof *data); /* 2.3.0 RC 2 fix, 2.21.5 tweak */
      return 0;
    }
    else if(idx > node->index)
      node = node->right;
    else
      node = node->left;
  }
}

/* deletes all administrative data for the given socket handle from the tree */
static int treedelete (TREE *tree, AGN_SOCKET idx) {
  /* pointer_z is the entry to be deleted  */
  NODE **pointer_q, *pointer_z, *pointer_y, *pointer_x;
  pointer_q = (struct node **)&tree->root;
  pointer_z = (struct node *)tree->root;
  for (;;) {
    if (pointer_z == NULL)
      return 0;
    else if (idx == pointer_z->index)  /* found idx that is to be deleted */
      break;
    else if (idx > pointer_z->index) {
      pointer_q = &pointer_z->right;
      pointer_z = pointer_z->right;
    }
    else { /* element to be deleted is smaller */
      pointer_q = &pointer_z->left;
      /* search left branch */
      pointer_z = pointer_z->left;
    }
  }
  /* process entry to be deleted */
  if (pointer_z->right == NULL)  /* right branch has no nodes */
    *pointer_q = pointer_z->left;
  else {  /* pointer_z has a right branch, but no left one */
    pointer_y = pointer_z->right;
    if (pointer_y->left == NULL) {
      /* pointer_z->right has no left branch */
      pointer_y->left = pointer_z->left;
      *pointer_q = pointer_y;
    }
    else {  /* there is a left branch */
      pointer_x = pointer_y->left;
      while (pointer_x->left != NULL) {  /* search as long as there is no further left branch */
        pointer_y = pointer_x;
        pointer_x = pointer_y->left;
      }
      /* now reconnect nodes */
      pointer_y->left = pointer_x->right;
      pointer_x->left = pointer_z->left;
      pointer_x->right = pointer_z->right;
      *pointer_q = pointer_x;
    }
  }
  tree->size--;
  xfree(pointer_z->status);  /* 2.3.0 RC 2 */
  xfree(pointer_z);
  return 1;
}

static void treetraverse (lua_State *L, NODE *k) {
  STATUS *s;
  if (k == NULL) return;
  s = k->status;
  lua_pushnumber(L, k->index);
  lua_createtable(L, 6, 0);
  lua_rawsetstringboolean(L, -1, "server", s->isserver);
  lua_rawsetstringstring(L, -1, "address", s->address);
  lua_rawsetstringnumber(L, -1, "port", s->port);
  lua_rawsetstringnumber(L, -1, "protocol", s->family);
  lua_rawsetstringboolean(L, -1, "blocking", s->blocking);
  lua_rawsetstringboolean(L, -1, "keepalive", s->keepalive);
  lua_rawsetstringboolean(L, -1, "connected", s->connected);
  switch (s->shutdown) {
    case SHUTDOWN_RD: lua_rawsetstringstring(L, -1, "mode", "write"); break;
    case SHUTDOWN_WR: lua_rawsetstringstring(L, -1, "mode", "read"); break;
    case SHUTDOWN_RDWR: lua_rawsetstringstring(L, -1, "mode", "shutdown"); break;
    case -MAX_INT: lua_rawsetstringstring(L, -1, "mode", "none"); break;
    case MAX_INT: lua_rawsetstringstring(L, -1, "mode", "readwrite"); break;
    default: luaL_error(L, "Error: invalid shutdown mode.");
  }
  lua_rawset(L, -3);
  treetraverse(L, k->left);
  treetraverse(L, k->right);
}

/* destructor for open sockets admin table, use only for cleanup, 1.6.6 */
static void nodepurge (TREE *tree, NODE *t, int flag) {
  if (t) {
    nodepurge(tree, t->left, 0);
    nodepurge(tree, t->right, 0);
    if (close_socket(t->index) == 0) {  /* socket closing has been successful ? 1.8.16 */
      t->index = INVALID_SOCKET;
    } else {  /* When terminating a process/programme, Windows seems to call WSACleanUp before running cleanup, so explicitly
       closing a socket in this function will always be unsuccessful. */
#ifndef _WIN32
      if (flag)
        fprintf(stderr, "\nError in `net` package during clean-up:\n");
      fprintf(stderr, "Could not close socket %d.\n", t->index);
      fflush(stderr);
#endif
    }
    xfree(t->status);
    xfree(t);
  }
}

static void treepurge (TREE *tree) {
  nodepurge(tree, tree->root, 1);
  xfree(tree);
}


/*** Some auxiliary functions ********************************************** */

/* Winsock and Winsock 2 error mapping, taken from:
   http://msdn.microsoft.com/en-us/library/windows/desktop/ms740668%28v=vs.85%29.aspx
   "Windows Sockets Error Codes"
*/
#ifdef _WIN32
static const char *wstrerror(int err) {
  switch (err) {
    case WSAEACCES: return "permission denied (10013)";
    case WSAEADDRINUSE: return "address already in use (10048)";
    case WSAEADDRNOTAVAIL: return "cannot assign requested address (10049)";
    case WSAEAFNOSUPPORT: return "address family not supported by protocol family (10047)";
    case WSAEALREADY: return "operation already in progress (10037)";
    case WSAEBADF: return "file handle is not valid (10009)";
    case WSAECANCELLED: return "call has been canceled (10103)";
    case WSAECONNABORTED: return "software caused connection abort (10053)";
    case WSAECONNREFUSED: return "connection refused (10061)";
    case WSAECONNRESET: return "connection reset by peer (10054)";
    case WSAEDESTADDRREQ: return "destination address required (10039)";
    case WSAEDISCON: return "graceful shutdown in progress (10101)";
    case WSAEDQUOT: return "disk quota exceeded (10069)";
    case WSAEFAULT: return "bad address (10014)";
    case WSAEHOSTDOWN: return "host is down (10064)";
    case WSAEHOSTUNREACH: return "no route to host (10065)";
    case WSAEINPROGRESS: return "operation now in progress (10036)";
    case WSAEINTR: return "interrupted function call (10004)";
    case WSAEINVAL: return "invalid argument (10022)";
    case WSAEINVALIDPROCTABLE: return "procedure call table is invalid (10104)";
    case WSAEINVALIDPROVIDER: return "service provider is invalid (10105)";
    case WSAEISCONN: return "socket is already connected (10056)";
    case WSAELOOP: return "cannot translate name (10062)";
    case WSAEMFILE: return "too many open files (10024)";
    case WSAEMSGSIZE: return "message too long (10040)";
    case WSAENAMETOOLONG: return "name too long (10063)";
    case WSAENETDOWN: return "network is down (10050)";
    case WSAENETRESET: return "network dropped connection on reset (10052)";
    case WSAENETUNREACH: return "network is unreachable (10051)";
    case WSAENOBUFS: return "no buffer space available (10055)";
    case WSAENOMORE: return "no more results (10102)";
    case WSAENOPROTOOPT: return "bad protocol option (10042)";
    case WSAENOTCONN: return "socket is not connected (10057)";
    case WSAENOTEMPTY: return "directory not empty (10066)";
    case WSAENOTSOCK: return "socket operation on nonsocket (10038)";
    case WSAEOPNOTSUPP: return "operation not supported (10045)";
    case WSAEPFNOSUPPORT: return "protocol family not supported (10046)";
    case WSAEPROCLIM: return "too many processes (10067)";
    case WSAEPROTONOSUPPORT: return "protocol not supported (10043)";
    case WSAEPROTOTYPE: return "protocol wrong type for socket (10041)";
    case WSAEPROVIDERFAILEDINIT: return "service provider failed to initialize (10106)";
    case WSAEREFUSED: return "database query was refused (10112)";
    case WSAEREMOTE: return "item is remote (10071)";
    case WSAESHUTDOWN: return "cannot send after socket shutdown (10058)";
    case WSAESOCKTNOSUPPORT: return "socket type not supported (10044)";
    case WSAESTALE: return "stale file handle reference (10070)";
    case WSAETIMEDOUT: return "connection timed out (10060)";
    case WSAETOOMANYREFS: return "too many references (10059)";
    case WSAEUSERS: return "user quota exceeded (10068)";
    case WSAEWOULDBLOCK: return "resource temporarily unavailable (10035)";
    case WSAHOST_NOT_FOUND: return "host not found (11001)";
    case WSANOTINITIALISED: return "successful WSAStartup not yet performed (10093)";
    case WSANO_DATA: return "valid name, no data record of requested type (11004)";
    case WSANO_RECOVERY: return "this is a nonrecoverable error (11003)";
    case WSASERVICE_NOT_FOUND: return "service not found (10108)";
    case WSASYSCALLFAILURE: return "system call failure (10107)";
    case WSASYSNOTREADY: return "network subsystem is unavailable (10091)";
    case WSATRY_AGAIN: return "nonauthoritative host not found (11002)";
    case WSATYPE_NOT_FOUND: return "class type not found (10109)";
    case WSAVERNOTSUPPORTED: return "winsock.dll version out of range (10092)";
    case WSA_E_CANCELLED: return "call was canceled (10111)";
    case WSA_E_NO_MORE: return "no more results (10110)";
    /* case WSA_INVALID_HANDLE: return "specified event object handle is invalid (6)";
    case WSA_INVALID_PARAMETER: return "one or more parameters are invalid (87)";
    case WSA_IO_INCOMPLETE: return "overlapped I/O event object not in signaledstate (996)";
    case WSA_IO_PENDING: return "overlapped operations will complete later (997)";
    case WSA_NOT_ENOUGH_MEMORY: return "insufficient memory available (8)";
    case WSA_OPERATION_ABORTED: return "overlapped operation aborted (995)"; */
    case WSA_QOS_ADMISSION_FAILURE: return "QoS admission error (11010)";
    case WSA_QOS_BAD_OBJECT: return "QoS bad object (11013)";
    case WSA_QOS_BAD_STYLE: return "QoS bad style (11012)";
    case WSA_QOS_EFILTERCOUNT: return "incorrect QoS filter count (11021)";
    case WSA_QOS_EFILTERSTYLE: return "invalid QoS filter style (11019)";
    case WSA_QOS_EFILTERTYPE: return "invalid QoS filter type (11020)";
    case WSA_QOS_EFLOWCOUNT: return "incorrect QoS flow count (11023)";
    case WSA_QOS_EFLOWDESC: return "invalid QoS flow descriptor (11026)";
    case WSA_QOS_EFLOWSPEC: return "QoS flowspec error (11017)";
    case WSA_QOS_EOBJLENGTH: return "invalid QoS object length (11022)";
    case WSA_QOS_EPOLICYOBJ: return "invalid QoS policy object (11025)";
    case WSA_QOS_EPROVSPECBUF: return "invalid QoS provider buffer (11018)";
    case WSA_QOS_EPSFILTERSPEC: return "invalid QoS provider-specific filterspec (11028)";
    case WSA_QOS_EPSFLOWSPEC: return "invalid QoS provider-specific flowspec (11027)";
    case WSA_QOS_ESDMODEOBJ: return "invalid QoS shape discard mode object (11029)";
    case WSA_QOS_ESERVICETYPE: return "QoS service type error (11016)";
    case WSA_QOS_ESHAPERATEOBJ: return "invalid QoS shaping rate object (11030)";
    case WSA_QOS_EUNKOWNPSOBJ: return "unrecognized QoS object (11024)";
    case WSA_QOS_GENERIC_ERROR: return "QoS generic error (11015)";
    case WSA_QOS_NO_RECEIVERS: return "QoS no receivers (11008)";
    case WSA_QOS_NO_SENDERS: return "no QoS senders (11007)";
    case WSA_QOS_POLICY_FAILURE: return "QoS policy failure (11011)";
    case WSA_QOS_RECEIVERS: return "QoS receivers (11005)";
    case WSA_QOS_REQUEST_CONFIRMED: return "QoS request confirmed (11009)";
    case WSA_QOS_SENDERS: return "QoS senders (11006)";
    case WSA_QOS_TRAFFIC_CTRL_ERROR: return "QoS traffic control error (11014)";
    default: return "unknown error";
  }
}

/* Taken from https://stackoverflow.com/questions/15660203/inet-pton-identifier-not-found, 5.1.0 */
int inet_pton (int af, const char *src, void *dst) {
  struct sockaddr_storage ss;
  int size = sizeof(ss);
  char src_copy[INET6_ADDRSTRLEN + 1]; /* Allocate space for null terminator */
  ZeroMemory(&ss, sizeof(ss));
  /* Copy source string safely into local buffer; strncpy does not guarantee null-termination
     if src is longer than the limit */
  strncpy(src_copy, src, INET6_ADDRSTRLEN);
  src_copy[INET6_ADDRSTRLEN] = 0; /* Ensure null termination */
  /* Convert string IP to binary form using Windows API */
  if (WSAStringToAddressA(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
    switch (af) {
      case AF_INET: *(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr; return 1;
      case AF_INET6: *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr; return 1;
    }
  } return 0; /* Conversion failed */
}

/* Taken from https://stackoverflow.com/questions/15660203/inet-pton-identifier-not-found, 5.1.0 */
const char *inet_ntop (int af, const void *src, char *dst, socklen_t size) {
  struct sockaddr_storage ss;
  unsigned long s = size;
  ZeroMemory(&ss, sizeof(ss));
  ss.ss_family = af;
  switch(af) {
    case AF_INET:
      ((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
      break;
    case AF_INET6:
      ((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
      break;
    default:
      return NULL;
  }
  /* cannot directly use &size because of strict aliasing rules */
  return (WSAAddressToStringA((struct sockaddr *)&ss, sizeof(ss), NULL, dst, &s) == 0) ? dst : NULL;
}
#endif

static void errornosock (lua_State *L, char *error_message, const char* procname) {  /* issue error message */
#ifdef _WIN32
  const char *message = wstrerror(WSAGetLastError());
  if (tools_streq(message, "unknown error"))  /* 2.16.12 tweak */
    luaL_error(L, "Error in " LUA_QS ": %s.", procname, message);
  else
    luaL_error(L, "Error in " LUA_QS ": %s (%d).", procname, error_message, WSAGetLastError());
#else
  luaL_error(L, "Error in " LUA_QS ": %s (%s).", procname, error_message, strerror(errno));
#endif
}

/* return `false' and error message */
#if defined( _WIN32)
#define agn_neterror(L) { \
  luaL_checkstack(L, 2, "not enough stack space"); \
  lua_pushfalse(L); \
  if (tools_strneq(wstrerror(WSAGetLastError()), "unknown error")) \
    lua_pushstring(L, wstrerror(WSAGetLastError())); \
   else \
    lua_pushstring(L, "unknown error"); \
  return 2; \
}
#else
#define agn_neterror(L) {\
  luaL_checkstack(L, 2, "not enough stack space"); \
  lua_pushfalse(L); \
  lua_pushstring(L, strerror(errno)); \
  return 2; \
}
#endif

#define agn_neterror2(L,msg) { \
  luaL_checkstack(L, 2, "not enough stack space"); \
  lua_pushfalse(L); \
  lua_pushstring(L, msg); \
  return 2; \
}

#define agn_neterrorfail(L,msg) { \
  luaL_checkstack(L, 2, "not enough stack space"); \
  lua_pushfail(L); \
  lua_pushstring(L, msg); \
  return 2; \
}

/* Taken from LuaSocket 2.0.2 license Copyright 2004-2007 Diego Nehab, file inet.c */
#ifndef inet_aton
int inet_aton (const char *cp, struct in_addr *inp) {
  unsigned int a = 0, b = 0, c = 0, d = 0;
  int n = 0, r;
  unsigned long int addr = 0;
  r = sscanf(cp, "%u.%u.%u.%u%n", &a, &b, &c, &d, &n);
  if (r == 0 || n == 0) return 0;
  cp += n;
  if (*cp) return 0;
  if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
  if (inp) {
    addr += a; addr <<= 8;
    addr += b; addr <<= 8;
    addr += c; addr <<= 8;
    addr += d;
    inp->s_addr = htonl(addr);
  }
  return 1;
}
#endif


/* converts a non-numeric (or numeric) IP address into a numeric IP address */
static char *tonumericip (const char *address) {  /* 1.6.4 */
  struct in_addr addr;
  if ((inet_aton(address, &addr)) != 0) {  /* valid address ? */
    return inet_ntoa(addr);
  } else {
    struct hostent *host_info;
    /* convert domain name to IP address */
    host_info = gethostbyname(address);
    if (host_info == NULL) {
      return NULL;
    }
    /* IP address of server */
    return host_info->h_addr;
  }
}

#ifndef __OS2__
static char *tonumericip6 (const char *address) {  /* 5.1.0 */
  unsigned char buf[sizeof(struct in6_addr)];
  if (inet_pton(AF_INET6, address, buf) > 0) {  /* valid address ? */
    char str[INET6_ADDRSTRLEN];
    return (char *)inet_ntop(AF_INET6, buf, str, INET6_ADDRSTRLEN);
  } else {  /* convert domain name to IP address */
    int rc;
    struct hostent *host_info;
    struct sockaddr_in6 addr6;
    /* taken from: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-gethostbyaddr */
    rc = inet_pton(AF_INET6, address, &addr6);
    if (!rc) return NULL;
    host_info = gethostbyaddr((char *)&addr6, 16, AF_INET6);
    return host_info->h_addr;  /* IP address of server */
  }
}
#endif

static void checkport (lua_State *L, int port, const char *procname) {
  if (port < 0 || port > 65535)
    luaL_error(L, "Error in " LUA_QS ": port must be in range [0, 65535], got %d.", procname, port);
}

/* check black and whitelist for outgoing or incoming connections */
static int checklist (lua_State *L, AGN_SOCKET sock, int family, const char *address, const char *list, const char *procname) {  /* 1.6.4 */
  if (agnL_gettablefield(L, "net", list, procname, 1) == LUA_TSET) {
    int isblacklist;
    char *ipaddress;
    isblacklist = tools_streq(list, "blacklist");
    ipaddress = (family == AF_INET) ? tonumericip(address) : tonumericip6(address);
    if (ipaddress) {  /* IP address could be resolved ? */
      lua_pushstring(L, ipaddress);
      lua_srawget(L, -2);
    }
    if (ipaddress == NULL || agnL_checkboolean(L, -1) == isblacklist) {
      AGN_SOCKET oldsock;
      oldsock = sock;
      /* net.accept & net.connect: now close (new) socket */
      if (close_socket(sock) == 0) {  /* socket closing has been successful ? */
        if (tools_streq(procname, "net.connect") && treedelete(socketattribs, sock) == 0)
          luaL_error(L, "Error in " LUA_QS ": could not delete data from socket administration table.", procname);
      } else {
        luaL_error(L, "Error in " LUA_QS ": could not close socket %d.", procname, sock);
      }
      sock = INVALID_SOCKET;
      if (ipaddress != NULL) {  /* IP address could be resolved ? */
        agn_poptoptwo(L);  /* pop true or false and list */
        luaL_error(L, "Error in " LUA_QS ": partner %sin %slist, closing socket %d.", procname,
          isblacklist ? "" : "not ", isblacklist ? "black" : "white", oldsock);
      } else {
        agn_poptop(L); /* pop black/whitelist */
        luaL_error(L, "Error in " LUA_QS ": could not resolve address %s for socket %d.", procname, address, sock);
      }
    } else
      agn_poptop(L);  /* pop true or false */
  }
  agn_poptop(L);  /* pop result of agnL_gettablefield */
  return 0;  /* does not leave anything on top of the stack */
}

/* with a valid socket handle, returns the family number of the protocol and the port number used
   from net.opensockets */
static STATUS *getsocketattribs (lua_State *L, AGN_SOCKET socket, const char *procname) {
  STATUS *s;
  s = treesearch(socketattribs, socket);
  if (s == NULL)
    luaL_error(L, "Error in " LUA_QS ": could not access socket status table.", procname);
  return s;
}

/* checks whether a socket handle is still valid by looking into net.opensockets or net.openserversockets
   (defined by argument *type) and either returns 1 or issues an error */
static void checksocket (lua_State *L, AGN_SOCKET socket, const char *procname) {
  if (treesearch(socketattribs, socket) == NULL)
    luaL_error(L, "Error in " LUA_QS ": invalid socket handle %d received.", procname, socket);
}

/* Blocking/non-blocking code taken from LuaSocket 2.0.2, Copyright 2004-2007 Diego Nehab, MIT licence;
   returns 0 on success and <> 0 on failure. */
#ifdef _WIN32
/* put socket into blocking mode */
static int setblocking (AGN_SOCKET s) {
  u_long argp = 0;
  return ioctlsocket(s, FIONBIO, &argp);
}

/* put socket into non-blocking mode */
static int setnonblocking (AGN_SOCKET s) {
  u_long argp = 1;
  return ioctlsocket(s, FIONBIO, &argp);
}
#elif defined(__OS2__)
/* OS/2 uses ioctl with FIONBIO for sockets */
static int setblocking (AGN_SOCKET s) {
  int argp = 0;
  return ioctl(s, FIONBIO, (char *)&argp, sizeof(argp));
}

static int setnonblocking (AGN_SOCKET s) {
  int argp = 1;
  return ioctl(s, FIONBIO, (char *)&argp, sizeof(argp));
}
#else  /* UNIX */
/* put socket into blocking mode */
static int setblocking (AGN_SOCKET s) {
  int flags = fcntl(s, F_GETFL, 0);
  flags &= (~(O_NONBLOCK));
  return fcntl(s, F_SETFL, flags);
}

/* put socket into non-blocking mode */
static int setnonblocking (AGN_SOCKET s) {
  int flags = fcntl(s, F_GETFL, 0);
  flags |= O_NONBLOCK;
  return fcntl(s, F_SETFL, flags);
}
#endif

/* create address of server socket: family, IP address, port number */
static void createaddress (lua_State *L, struct sockaddr_in *server,
  const char *address, AGN_PORT port, AGN_FAMILY family, const char *procname) {
  struct in_addr addr;
  size_t len = tools_strlen(address);
  if (len >= ADDRESS_LENGTH)
    luaL_error(L, "Error in " LUA_QS ": address " LUA_QS " is of wrong length %d.", procname, address, len);
  memset(server, 0, sizeof(struct sockaddr_in));
  if (tools_streq(address, "*")) {  /* 2.16.12 tweak */
    server->sin_addr.s_addr = htonl(INADDR_ANY);  /* INADDR_ANY binds the socket to all local interfaces */
    server->sin_family = AF_UNSPEC;
  } else if ((inet_aton(address, &addr)) != 0) {  /* valid address ? */
    tools_memcpy((char *)&server->sin_addr, &addr, sizeof(addr));  /* 2.21.5 tweak */
  } else {
    struct hostent *host_info;
    /* convert server name to IP address */
    host_info = gethostbyname(address);
    if (host_info == NULL) {
      errornosock(L, "unknown host", procname);
    }
    /* IP address of server */
    server->sin_addr = *(struct in_addr *)host_info->h_addr;
  }
  /* set network protocol */
  server->sin_family = family;
  /* port number, check it to not be sorry later */
  checkport(L, port, procname);  /* 5.1.1 */
  server->sin_port = htons(port);  /* convert to network byte order, that is Big Endian */
  /* return inet_ntoa(server->sin_addr); */
}

static AGN_SOCKET createsocket (lua_State *L, int family,
    int blocking, int keepalive, int server,
    char *address, int port,
    int *reusesuccess, const char *procname) {
  STATUS attrib;
  int connected = 0;
  AGN_SOCKET sock = socket(family, SOCK_STREAM, 0);
#ifndef _WIN32
  if (sock < 0) errornosock(L, "socket could not be created", procname);
#else
  if (sock == INVALID_SOCKET) errornosock(L, "socket could not be created", procname);
#endif
  /* 7.6.8: if the user explicitly requested to open a client (`server' = 0) or server (`server' = 0) socket,
     we will immediately call bind() with a client socket and connect() with a server socket.
     In OS/2 this is essential as it immediately closes a socket again if it is not bound or connected after
     the call to socket(). */
  /* set blocking/non-blocking mode */
  if (blocking)
    setblocking(sock);
  else  /* new 7.5.16 */
    setnonblocking(sock);
  /* set keep-alive mode if requested, new 7.6.8 */
  if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&keepalive, sizeof(keepalive)) < 0) {
    luaL_error(L, "Error in " LUA_QS ": setsockopt SO_KEEPALIVE failed.", procname);
  }
  if (server != -1) {
    struct sockaddr_in local_addr;
    if (address == NULL) {
      address = strdup("0.0.0.0");
      if (!address) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      port = 0;
    }
    createaddress(L, &local_addr, address, (AGN_PORT)port, family, procname);
    /* INADDR_ANY = "0.0.0.0": If your machine has multiple network cards or addresses
       — like 127.0.0.1 (localhost), 192.168.1.5 (local Wi-Fi), and 10.0.0.1 (VPN) —
       INADDR_ANY allows your server to accept requests from all of them automatically,
       which you might not want to happen.
       inet_addr("127.0.0.1") seems to be too error-prone, at least in OS/2 */
    switch (server) {
      case 0: { /* Client mode */
        int res = connect(sock, (struct sockaddr *)&local_addr, sizeof(local_addr));
        if (res < 0) {
          int err;
#ifdef _WIN32
          err = WSAGetLastError();
          if (!(err == WSAEWOULDBLOCK || err == WSAEINPROGRESS)) {
            xfree(address);
            errornosock(L, "client socket failed connect", procname);
          }
#else
          err = errno;
          if (!(err == EINPROGRESS || err == EALREADY)) {
            xfree(address);
            errornosock(L, "client socket failed connect", procname);
          }
#endif
        }
        connected = 1;
        break;
      }
      case 1:
        if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
          xfree(address);
          errornosock(L, "server socket failed kernel registration", procname);
        }
        connected = 1;
        break;
      default: {
        xfree(address);
        errornosock(L, "this should not happen", procname);
      }
    }
    COPYADDRESS(attrib.address, (char *)inet_ntoa(local_addr.sin_addr));
  }
  /* set reuse-address mode */
  *reusesuccess = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&so_reuseaddr, sizeof(so_reuseaddr));  /* 7.6.8 Windows fix */
  /* add new open socket in internal socket attribute tree */
  if (server == -1) {
    if (address) {
      COPYADDRESS(attrib.address, (char *)address);  /* 7.6.8 fix, we cannot directly assign a collectable (!) Agena string  */
    } else {
      memset(attrib.address, 0, ADDRESS_LENGTH);
    }
  }
  attrib.port = port;
  attrib.family = family;
  attrib.isserver = 0;  /* you must call net.listen to convert a socket into a server socket */
  attrib.blocking = blocking;
  attrib.keepalive = keepalive;
  attrib.connected = connected;
  attrib.shutdown = -MAX_INT;
  if (treeinsert(socketattribs, sock, &attrib) == 0) {
    xfree(address);
    luaL_error(L, "Error in " LUA_QS ": could not assign socket to administration table.", procname);
  }
  return sock;
}

#ifndef __OS2__
static void createaddress6 (lua_State *L, struct sockaddr_in6 *server,
  const char *address, AGN_PORT port, AGN_FAMILY family, const char *procname) {  /* new 5.1.0 */
  struct in6_addr addr = IN6ADDR_ANY_INIT;
  memset(server, 0, sizeof(struct sockaddr_in6));
  if (tools_streq(address, "*")) {  /* 2.16.12 tweak */
    server->sin6_addr = in6addr_any;  /* INADDR_ANY binds the socket to all local interfaces */
    server->sin6_family = AF_INET6;
  } else if ((inet_pton(AF_INET6, address, &addr)) != 0) {  /* valid address ? */
    tools_memcpy((char *)&server->sin6_addr, &addr, sizeof(addr));
  } else {
    int rc;
    struct hostent *host_info;
    struct sockaddr_in6 addr6;
    /* taken from: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-gethostbyaddr */
    rc = inet_pton(AF_INET6, address, &addr6);
    if (!rc) {
      errornosock(L, "unknown host", procname);
    }
    host_info = gethostbyaddr((char *)&addr6, 16, AF_INET6);
    server->sin6_addr = *(struct in6_addr *)host_info->h_addr;  /* IP address of server */
  }
  server->sin6_family = family;     /* set network protocol */
  server->sin6_port = htons(port);  /* set port number */
  /* eturn (char *)server->sin6_addr.s6_addr; */
}
#endif

/*********************************************************************************************************************
*   Agena C library functions                                                                                        *
*********************************************************************************************************************/

/* Opens a (client) socket using the IPv4 protocol. If the optional first argument is set to false, the socket is set to non-blocking mode.

The return is the socket handle (a number), the default address 'localhost' and default port 1234, the protocol (a number) and a Boolean indicating whether the handle can be reused by the system after the socket has been closed.

The procedure is a binding to C's socket function. */

/* Check options for linalg.vzero, linalg.unitvector, linalg.inverse 4.1.1 */
static void aux_checkoptions (lua_State *L, int pos, int *nargs,
    int *blocking, int *keepalive, int *server,
    AGN_FAMILY *family, char **address, AGN_PORT *port, const char *procname) {
  int checkoptions;
  *blocking = 1;       /* 1 = yes */
  *keepalive = 0;      /* 0 = no */
  *server = -1;        /* -1 = no explicit socket type, 0 = client socket, 1 = server socket */
  *family = AF_INET;   /* AF_INET = IPv4, AF_INET6 = IPv6 */
  *port = 0;
  *address = NULL;
  if (*nargs == 1 && lua_isboolean(L, *nargs)) {
    *blocking = lua_toboolean(L, *nargs);
    return;
  }
  /* check for options, here `map in-place` */
  checkoptions = 6;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 fix */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "blocking")) {
        *blocking = agn_checkboolean(L, -1);
      } else if (tools_streq(option, "keepalive")) {
        *keepalive = agn_checkboolean(L, -1);
      } else if (tools_streq(option, "address")) {
        *address = strdup(agn_checkstring(L, -1));
        if (!*address) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      } else if (tools_streq(option, "port")) {
        *port = agn_checknonnegint(L, -1);
      } else if (tools_streq(option, "mode")) {
        if (agn_isstring(L, -1)) {
          const char *str = lua_tostring(L, -1);
          if (tools_streq(str, "default")) {
            *server = -1;
          } else if (tools_streq(str, "client")) {
            *server = 0;
          } else if (tools_streq(str, "server")) {
            *server = 1;
          } else {
            xfree(*address);
            luaL_error(L, "Error in " LUA_QS ": unknown setting " LUA_QS " for " LUA_QS " option.", procname, str, option);
          }
        } else if (agn_isnumber(L, -1)) {
          int mode = agn_tonumber(L, -1);
          switch (mode) {
            case -1: case 0: case 1: break;
            default: {
              xfree(*address);
              luaL_error(L, "Error in " LUA_QS ": unknown setting %d for " LUA_QS " option.", procname, mode, option);
            }
          }
          *server = mode;
        } else {
          xfree(*address);
          luaL_error(L, "Error in " LUA_QS ": invalid setting for " LUA_QS " option.", procname, option);
        }
      } else if (tools_streq(option, "family") || tools_streq(option, "ipv")) {
        *family = agn_checkposint(L, -1);
        switch (*family) {
          case 4: *family = AF_INET;  break;
#ifndef __OS2__
          case 6: *family = AF_INET6; break;
#endif
          default: {
            xfree(*address);
#ifndef __OS2__
            luaL_error(L, "Error in " LUA_QS ": %s must be either 4 or 6.", procname, option);
#else
            luaL_error(L, "Error in " LUA_QS ": %s must be 4.", procname, option);
#endif
          }
        }
      } else {
        agn_poptoptwo(L);
        xfree(*address);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
}

static int net_open (lua_State *L) {
  int r, nargs, blocking, keepalive, server;
  /* STATUS and socket attribs */
  AGN_SOCKET sock;
  AGN_FAMILY family;
  AGN_PORT port;
  char *address;
  family = AF_INET;  /* by default use IPv4 protocol */
  nargs = lua_gettop(L);
  aux_checkoptions(L, 1, &nargs, &blocking, &keepalive, &server, &family, &address, &port, "net.open");
#ifdef __OS2__
  if (server == -1)
    luaL_error(L, "Error in " LUA_QS ": pass " LUA_QS " or " LUA_QS " with the " LUA_QS " option.", "net.open", "client", "server", "mode");
#endif
  if (address == NULL) {
    address = (family == AF_INET) ? NET_ADDRESS : NET_ADDRESS6;
    port = NET_PORT;
  }
  r = -1;
  /* create socket */
  sock = createsocket(L, family, blocking, keepalive, server, address, port, &r, "net.open");
  luaL_checkstack(L, 5, "not enough stack space");  /* 5.1.0 fix */
  lua_pushinteger(L, sock);      /* socket handle */
  lua_pushstring(L, address);    /* IP address */
  lua_pushinteger(L, -1);        /* port */
  lua_pushinteger(L, family);    /* network protocol */
  lua_pushboolean(L, r == 0);    /* REUSE SOCKET option could be set ? */
  return 5;
}

#ifdef __OS2__
/* Lua function: os2socket.init(), created by gemini.google.com.
   Initializes the OS/2 socket library. We must explicitly call this function after initialising the `net` package,
   and before creating a socket handle with `net.open`. Just calling sock_init during package initialisation
   through luaopen_net does not work. */

static int net_initialised = 0;

static int net_init (lua_State *L) {
  if (!net_initialised) {
    int result = sock_init();
    if (result != 0) {
      luaL_error(L, "Error in " LUA_QS ": sock_init() failed with code %d", "net.init", result);
    }
    net_initialised = 1;
  }
  lua_pushboolean(L, 1);
  return 1;  /* return true on success */
}
#endif

/* Connects the client denoted by it socket handle (first argument, a number) to a server at the
   specified IP address (second argument, a string) and its port (third argument) so that data
   can be sent later. If address is missing, the address is set to 'localhost', if port is missing,
   port 1234 will be used.

   If the client socket is set to blocking mode, the function waits until
   the server responds; if the client socket is set to non-blocking mode, it immediately returns
   without waiting for a server response.

   The return is either true or an error is issued at failure.

   The procedure is a binding to C's connect function. */

/* for AGN_NET* definition see top of file */

#define localhosttoip(family,str) { \
  if (tools_streq(str, "localhost")) { \
    str = (family == AF_INET) ? "127.0.0.1" : "::1"; \
  } \
}

static int net_connect (lua_State *L) {
  char *address, *dummy;
  AGN_PORT port, dummyport;
  STATUS *s;
  AGN_SOCKET sock;
  int blocking, keepalive, server, nargs;
  AGN_FAMILY family;
  nargs = lua_gettop(L);
  (void)family; (void)blocking; (void)server;
  aux_checkoptions(L, 3, &nargs, &blocking, &keepalive, &server, &family, &dummy, &dummyport, "net.connect");
  xfree(dummy);
  sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.connect");
  s = getsocketattribs(L, sock, "net.connect");
  address = (char *)luaL_optstring(L, 2, s->family == AF_INET ? NET_ADDRESS : NET_ADDRESS6);
  localhosttoip(s->family, address);  /* 5.1.1 extension */
  port = luaL_optinteger(L, 3, NET_PORT);
  checkport(L, port, "net.connect");
  /* create address of server socket: family, IP address, port number */
  if (s->family == AF_INET) {
    struct sockaddr_in server;
    createaddress(L, &server, address, (AGN_PORT)port, s->family, "net.connect");
    address = inet_ntoa(server.sin_addr);
    checklist(L, sock, s->family, address, "blacklist", "net.connect");  /* check black and white lists, if they exist, 1.6.4 */
    checklist(L, sock, s->family, address, "whitelist", "net.connect");
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
      agn_neterror(L);
    }
    /* the result of inet_ntoa could be overwritten outside the scope, so assign now. XXX UNSAFE */
    COPYADDRESS(s->address, (char *)address);
#ifndef __OS2__
  } else if (s->family == AF_INET6) {  /* ipv6, 5.1.0 */
    char str[INET6_ADDRSTRLEN];
    struct sockaddr_in6 server6;
    createaddress6(L, &server6, address, (AGN_PORT)port, s->family, "net.connect");
    address = (char *)inet_ntop(AF_INET6, server6.sin6_addr.s6_addr, str, INET6_ADDRSTRLEN);
    checklist(L, sock, s->family, address, "blacklist", "net.connect");  /* check black and white lists, if they exist, 1.6.4 */
    checklist(L, sock, s->family, address, "whitelist", "net.connect");
    if (connect(sock, (struct sockaddr*)&server6, sizeof(server6)) < 0) {
      agn_neterror(L);
    }
    /* the result of inet_ntoa could be overwritten outside the scope, so assign now. XXX UNSAFE */
    COPYADDRESS(s->address, (char *)address);
#endif
  } else {
    address = NULL;
    luaL_error(L, "Error in " LUA_QS ": this should not happen.", "net.connect");
  }
  s->port = (AGN_PORT)port; s->connected = 1; s->shutdown = MAX_INT;
  if (treeupdate(socketattribs, sock, s) != 0)
    luaL_error(L, "Error in " LUA_QS ": could not assign socket to administration table.", "net.connect");
  lua_pushtrue(L);
  return 1;
}


/* Sets a socket to blocking or not blocking mode. The functions expects the socket handle (a number) as its
   first argument and the mode (a Boolean) as its second argument. If the second argument is true, the socket
   is set to blocking mode, else to non-blocking mode. The return is true on success and false otherwise. */

static int net_block (lua_State *L) {
  int rc, blocking;
  AGN_SOCKET sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.block");
  blocking = agnL_checkboolean(L, 2);
  if (blocking < 0) blocking = 0;  /* 6.7.8 */
  rc = (blocking) ? setblocking(sock) : setnonblocking(sock);
  if (rc != 0) {
    luaL_error(L, "Error in " LUA_QS ": operation failed.", "net.block");
  } else {
    STATUS *s = getsocketattribs(L, sock, "net.block");
    s->blocking = blocking;
    if (treeupdate(socketattribs, sock, s) != 0)
      luaL_error(L, "Error in " LUA_QS ": could not change status in administration table.", "net.block");
  }
  lua_pushboolean(L, rc == 0);
  return 1;
}


/* Sets a socket to keep-alive mode or deactivates it. The functions expects the socket handle (a number) as its
   first argument and the mode (a Boolean) as its second argument. If the second argument is `true`, the socket
   is set to keep-alive mode. If it is `false`, keep-alive mode is turned off. The return is `true` on success
   and `false` otherwise. Based on net_block, 6.7.8 */

static int net_keep (lua_State *L) {
  int rc, keepalive;
  AGN_SOCKET sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.keep");
  keepalive = agnL_checkboolean(L, 2);
  if (keepalive < 0) keepalive = 0;
  rc = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&keepalive, sizeof(keepalive));
  if (rc < 0) {
    luaL_error(L, "Error in " LUA_QS ": operation failed.", "net.keep");
  } else {
    STATUS *s = getsocketattribs(L, sock, "net.keep");
    s->keepalive = keepalive;
    if (treeupdate(socketattribs, sock, s) != 0)
      luaL_error(L, "Error in " LUA_QS ": could not change status in administration table.", "net.keep");
  }
  lua_pushboolean(L, rc == 0);
  return 1;
}


/* Assigns an address specified to a given socket s and returns this address (a string) on success and issues
   an error otherwise. A port (a number) may be passed, as well, but you may need administrative rights.

   The procedure is a binding to C's bind function. */

static int net_bind (lua_State *L) {
  int blocking, keepalive, server, nargs;
  AGN_FAMILY family;
  char *address, *dummy;
  AGN_PORT port, dummyport;
  AGN_SOCKET sock;
  STATUS *s;
  (void)family; (void)blocking; (void)server; (void)keepalive;
  nargs = lua_gettop(L);
  aux_checkoptions(L, 3, &nargs, &blocking, &keepalive, &server, &family, &dummy, &dummyport, "net.bind");
  xfree(dummy);
  sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.bind");
  s = getsocketattribs(L, sock, "net.bind");
  address = (char *)luaL_optstring(L, 2, s->address);
  localhosttoip(s->family, address);  /* 5.1.1 extension */
  port = (AGN_PORT)luaL_optinteger(L, 3, s->port);  /* createaddress converts port# to network byte order (BE) */
  checkport(L, port, "net.bind");  /* 5.1.1 fix */
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.1.0 fix */
  if (s->family == AF_INET) {
    struct sockaddr_in server;
    createaddress(L, &server, address, port, s->family, "net.bind");
    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0) { /*  bind socket to server:port */
      agn_neterror(L);
    }
    /* enter new open socket in internal socket attribute tree, set own address */
    address = inet_ntoa(server.sin_addr);
    /* the return of inet_ntoa might get lost (!) if accessed outside the block, so push the results now */
    lua_pushstring(L, (const char *)address);
    COPYADDRESS(s->address, (char *)address);
#ifndef __OS2__
  } else if (s->family == AF_INET6) {  /* 5.1.0 */
    char str[INET6_ADDRSTRLEN];
    struct sockaddr_in6 server6;
    createaddress6(L, &server6, address, (AGN_PORT)port, s->family, "net.bind");
    if (bind(sock, (struct sockaddr*)&server6, sizeof(server6)) < 0) { /*  bind socket to server:port */
      agn_neterror(L);
    }
    address = (char *)inet_ntop(AF_INET6, server6.sin6_addr.s6_addr, str, INET6_ADDRSTRLEN);
    /* the return of inet_ntop might get lost (!) if accessed outside the block, so push the results now */
    lua_pushstring(L, address);
    COPYADDRESS(s->address, (char *)address);
#endif
  } else {
    address = NULL;
    luaL_error(L, "Error in " LUA_QS ": this should not happen.", "net.bind");
  }
  s->port = port;
  if (treeupdate(socketattribs, sock, s) != 0)
    luaL_error(L, "Error in " LUA_QS ": could not assign socket to administration table.", "net.bind");
  lua_pushinteger(L, port);
  return 2;
}


/* Converts the given socket to a server socket, enabling it to accept connections. You may optionally pass an integer in the
   range [1, 1024] determining the length of the queue for pending connections. The return is either true or an
   error is issued if listening failed. You must first run this function before querying the input from the
   client with net.serve.

   The procedure is a binding to C's listen function. */

static int net_listen (lua_State *L) {
  int blocking, keepalive, server, nargs;
  AGN_FAMILY family;
  size_t queuelen;
  AGN_SOCKET sock;
  AGN_PORT dummyport;
  char *dummy;
  STATUS *s;
  (void)family; (void)blocking; (void)server, (void)dummy; (void)dummyport;
  nargs = lua_gettop(L);
  aux_checkoptions(L, 2, &nargs, &blocking, &keepalive, &server, &family, &dummy, &dummyport, "net.listen");
  xfree(dummy);
  sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.listen");
  s = getsocketattribs(L, sock, "net.listen");
  queuelen = luaL_optinteger(L, 2, 5);  /* length of the queue for pending connections */
  if (queuelen < 1 || queuelen > 1024)
    luaL_error(L, "Error in " LUA_QS ": queue length too small or large, must be in [1, 1024].", "net.listen");
  if (listen(sock, queuelen) == -1) {
    agn_neterror(L);
  }
  s->isserver = 1;
  if (treeupdate(socketattribs, sock, s) != 0)
    luaL_error(L, "Error in " LUA_QS ": could not assign socket to administration table.", "net.listen");
  lua_pushtrue(L);
  return 1;
}


/* Accepts a connection request from a client on the given server socket handle. If the server socket has been set to blocking
   mode, it waits until there is an incoming connection. Use net.block(<socket>, false) on the server socket to set it to
   non-blocking mode.

   The function returns a new socket handle for the data to be received lateron, and the address and port of the
   client socket. Please note that the new socket created by accept must be closed separately to avoid too many open
   sockets.

   The procedure is a binding to C's accept function. */

/* for AGN_NET* definition see top of file */

static int net_accept (lua_State *L) {
  int blocking, keepalive, server, nargs;
  AGN_FAMILY family;
  socklen_t len;
  STATUS *s;
  AGN_SOCKET sock, fd;
  AGN_PORT port, dummyport;
  char *address, *dummy;
  (void)family; (void)blocking; (void)server; (void)address; (void)port;
  nargs = lua_gettop(L);
  aux_checkoptions(L, 2, &nargs, &blocking, &keepalive, &server, &family, &dummy, &dummyport, "net.accept");
  xfree(dummy);
  sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.accept");
  s = getsocketattribs(L, sock, "net.accept");
  luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
  if (s->family == AF_INET) {
    struct sockaddr_in client;
    len = (socklen_t)sizeof(client);
    fd = accept(sock, (struct sockaddr*)&client, &len);  /* cpu-friendly block as long as there is no connection */
#ifndef _WIN32
    if (fd < 0) {
      agn_neterror(L);
    }
#else
    if (fd == INVALID_SOCKET) {
      agn_neterror(L);
    }
#endif
    address = inet_ntoa(client.sin_addr);
    port = (unsigned)ntohs(client.sin_port);
    /* the return of inet_ntoa might get lost (!) if accessed outside the block, so push the results now */
    lua_pushinteger(L, fd);      /* return new socket handle */
    lua_pushstring(L, address);  /* client address */
    lua_pushinteger(L, (unsigned)ntohs(port));  /* client port */
#ifndef __OS2__
  } else if (s->family == AF_INET6) {  /* IPv6, 5.1.0 */
    char str[INET6_ADDRSTRLEN];
    struct sockaddr_in6 client;
    len = (socklen_t)sizeof(client);
    fd = accept(sock, (struct sockaddr*)&client, &len);  /* cpu-friendly block as long as there is no connection */
#ifndef _WIN32
    if (fd < 0) {
      agn_neterror(L);
    }
#else
    if (fd == INVALID_SOCKET) {
      agn_neterror(L);
    }
#endif
    address = (char *)inet_ntop(AF_INET6, client.sin6_addr.s6_addr, str, INET6_ADDRSTRLEN);
    port = client.sin6_port;
    /* the return of inet_ntop might get lost (!) if accessed outside the block, so push the results now */
    lua_pushinteger(L, fd);      /* return new socket handle */
    lua_pushstring(L, address);  /* client address */
    lua_pushinteger(L, (unsigned)ntohs(port));  /* client port */
#endif
  } else {
    address = NULL; fd = INVALID_SOCKET;
    luaL_error(L, "Error in " LUA_QS ": this should not happen.", "net.accept");
  }
  checklist(L, fd, s->family, address, "blacklist", "net.accept");  /* check black and white lists, if they exist, 1.6.4 */
  checklist(L, fd, s->family, address, "whitelist", "net.accept");
  s->connected = 1; s->shutdown = MAX_INT;
  if (treeinsert(socketattribs, fd, s) == 0)
    luaL_error(L, "Error in " LUA_QS ": could not assign socket to administration table.", "net.accept");
  return 3;
}


/* Allows a server socket to receive a string from a client. The function returns this string and its length (a number).

   The optional argument determines the maximum number of characters to be received.

   The procedure is a binding to C's recv function. */

static int net_receive (lua_State *L) {
  int recv_size, getall;
  int32_t n;
  lua_Number length, maxsize;
  char *buffer;
  AGN_SOCKET sock;
  n = agn_getbuffersize(L);  /* 2.2.0 */
  sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.receive");
  getall = agnL_optboolean(L, 2, 0);
  maxsize = luaL_optnumber(L, 3, HUGE_VAL);
  buffer = (char *)agn_malloc(L, (n + 1)*sizeof(char), "net.receive", NULL);  /* Agena 1.6.4 Valgrind; 1.9.1 */
  length = 0;
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.1.0 fix */
  lua_pushstring(L, "");
  while ((recv_size = recv(sock, buffer, n, 0)) != 0) {
    if (recv_size < 0) {
      xfree(buffer);
      agn_neterror2(L, "failure during receipt");  /* Agena 1.6.3 */
    }
    buffer[recv_size] = '\0';
    length += recv_size;
    if (length > maxsize) {
      xfree(buffer);
      agn_neterror2(L, "too many bytes received");
    }
    lua_pushstring(L, buffer);  /* lua_pushstring makes or reuses an internal copy of buffer, so buffer can be freed thereafter */
    lua_concat(L, 2);
    if (!getall) break;
  }
  lua_pushinteger(L, length);
  xfree(buffer);
  return 2;
}


/* Sends a string (second argument) from the client denoted by its socket handle (first argument, a number) to
   a server.

   The return is the number of the characters actually sent. If the kernel decides not to send all the data in
   one chunk, the function might not send the complete string. If an optional third argument, true, is given,
   net.send, however, tries to make sure that the complete string has been sent when it returns.

   The procedure is an extended binding to C's send function. */

static int net_send (lua_State *L) {
  const char *str;
  char forceall;
  int len;
  AGN_SOCKET sock;
  STATUS *s;
  sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.send");
  str = luaL_checkstring(L, 2);
  len = tools_strlen(str);  /* 2.17.8 tweak */
  forceall = agnL_optboolean(L, 3, 0);
  s = getsocketattribs(L, sock, "net.send");
  if (!s->connected) {  /* Linux would crash otherwise */
    agn_neterrorfail(L, "socket not connected");
  }
  /* send string including null terminator to server */
  if (!forceall) {
    int sendlen;
    sendlen = send(sock, str, len, 0);
    if (sendlen == -1) {
      s->connected = 0;
      if (treeupdate(socketattribs, sock, s) != 0)
        luaL_error(L, "Error in " LUA_QS ": could not change socket status in administration table.", "net.send");  /* 2.3.0 RC 2 error message fix */
      agn_neterrorfail(L, "socket not connected");
    } else if (sendlen != len) {
      luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
      lua_pushfalse(L);
      lua_pushstring(L, "transfer size mismatch");
      lua_pushnumber(L, sendlen);
      return 3;
    }
    len = sendlen;
  } else {  /* if less data has actually been sent, try sending the rest not yet transmitted,
    taken from `Beej's Guide to Network Programming, Using Internet Sockets`, Version 3.0.14,
    September 8, 2009, Chapter 7.3 */
    int total = 0;         /* how many bytes we've sent */
    int bytesleft = len;   /* how many we have left to send */
    int n;
    while (total < len) {
      n = send(sock, str + total, bytesleft, 0);
      if (n == -1) {
        agn_neterror2(L, "sending data failed");
      }
      total += n;
      bytesleft -= n;
    }
    len = total;  /* return number actually sent here */
  }
  lua_pushinteger(L, len);
  return 1;
}


/* returns the largest descriptor of all open sockets */
static AGN_SOCKET treesetfds (lua_State *L, NODE *k, fd_set *x, AGN_SOCKET *max, const char *procname) {
  AGN_SOCKET s;
  if (k == NULL) return *max;
  s = k->index;
  FD_SET(s, x);
  if (s == INVALID_SOCKET || *max < s)
    *max = s;
  treesetfds(L, k->left, x, max, procname);
  treesetfds(L, k->right, x, max, procname);
  return *max;
}

/* The function looks for activity on all open sockets, or of specific sockets. */
static int net_survey (lua_State *L) {
  int i, cread, cwrite, cexc, istimegiven, result, mode, throw, offset, *field;
  size_t size;
  const char *option;
  AGN_SOCKET max;
  struct timeval tv;
  fd_set read_fds, write_fds, exc_fds;  /* file descriptor lists for select() */
  max = istimegiven = mode = offset = 0;
  size = 0;
  field = NULL;
  /* this following should detect listeners, as well */
  if (socketattribs->size > FD_SETSIZE)
    luaL_error(L, "Error in " LUA_QS ": too many open sockets, %d allowed.", "net.survey", FD_SETSIZE);  /* 2.3.0 Rc 2 fix */
  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  FD_ZERO(&exc_fds);
  if (lua_gettop(L) > 0 && lua_isseq(L, 1)) {
    AGN_SOCKET val;
    offset = 1;
    size = agn_seqsize(L, 1);
    field = agn_malloc(L, size * sizeof(AGN_SOCKET), "net.survey", NULL);
    for (i=0; i < size; i++) {
      val = (AGN_SOCKET)lua_seqrawgetinumber(L, 1, i + 1);
#ifndef _WIN32  /* 7.9.6 change */
      if (val == HUGE_VAL || val < 0 || val >= FD_SETSIZE) {
#else
      if (val == HUGE_VAL || val >= FD_SETSIZE) {
#endif
        xfree(field);
        luaL_error(L, "Error in " LUA_QS ": invalid socket handle encountered.", "net.survey");
      }
      *field = val;
      field += sizeof(AGN_SOCKET);
      if (val > max) max = val;
    }
    field -= size * sizeof(AGN_SOCKET);  /* reset pointer to beginning of array */
  } else {  /* traverse socket tree to look for largest file descriptor, also assign fd_set */
    max = treesetfds(L, socketattribs->root, &read_fds, &max, "net.survey");
  }
  write_fds = exc_fds = read_fds;  /* also assign sockets to be scanned to write an exception fd_sets */
  if (lua_gettop(L) > offset && lua_isnumber(L, offset + 1)) {
    lua_Number d = lua_tonumber(L, offset + 1);  /* 2.3.0 RC 2 fix */
    if (d <= 0) {
      if (offset) xfree(field);
      luaL_error(L, "Error in " LUA_QS ": timeout must be positive, received %f seconds.", "net.survey", d);  /* 2.1 RC 1 fix */
    }
    if (d != HUGE_VAL) {  /* 1.8.16 */
      tv.tv_sec = (int)(d);
      tv.tv_usec = ((int)(d) - sun_trunc(d))*100000;
      istimegiven = 1;
    }
  }
  option = luaL_optstring(L, offset + 2, "all");
  if (tools_streq(option, "all"))  /* 2.16.12 tweak */
    mode = 8;
  else if (tools_streq(option, "read"))
    mode = 1;
  else if (tools_streq(option, "write"))
    mode = 2;
  else if (tools_streq(option, "except"))
    mode = 4;
  else {
    if (offset) xfree(field);
    luaL_error(L, "Error in " LUA_QS ": unknown option `%s`.", "net.survey", option);
  }
  throw = agnL_optboolean(L, offset + 3, 1);
  /*  select returns 0 at timeout, 1 if input available, -1 at error. */
  if (offset) {  /* set fd_sets, 1.8.16 */
    for (i=0; i < size; i++) {
      FD_SET(*field, &read_fds);
      FD_SET(*field, &write_fds);
      FD_SET(*field, &exc_fds);
      field += sizeof(AGN_SOCKET);
    }
    field -= size * sizeof(AGN_SOCKET);  /* reset pointer to beginning of array */
    xfree(field);
  }
  if ((result = select(max + 1, &read_fds, &write_fds, &exc_fds, (istimegiven) ? &tv : NULL)) == -1 && throw) {
    agn_neterror(L);
  }
  cread = cwrite = cexc = 0;
  if (mode == 8) {  /* scan all socket types ?  16.01.2013 */
    luaL_checkstack(L, 4, "not enough stack space");  /* 5.1.0 fix */
    for (i=0; i < 3; i++) agn_createseq(L, 0);
    for (i=0; i <= max; i++) {
      if (FD_ISSET(i, &read_fds)) {
        agn_seqsetinumber(L, -3, ++cread, i);
      }
      if (FD_ISSET(i, &write_fds)) {
        agn_seqsetinumber(L, -2, ++cwrite, i);
      }
      if (FD_ISSET(i, &exc_fds)) {
        agn_seqsetinumber(L, -1, ++cexc, i);
      }
    }
  } else {  /* scan individual socket types ?  16.01.2013 */
    luaL_checkstack(L, 2, "not enough stack space");  /* 5.1.0 fix */
    agn_createseq(L, 0);
    for (i=0; i <= max; i++) {
      if ((mode & 1) && FD_ISSET(i, &read_fds)) {
        agn_seqsetinumber(L, -1, ++cread, i);
      }
      else if ((mode & 2) && FD_ISSET(i, &write_fds)) {
        agn_seqsetinumber(L, -1, ++cwrite, i);
      }
      else if (FD_ISSET(i, &exc_fds)) {
        agn_seqsetinumber(L, -1, ++cexc, i);
      }
    }
  }
  agn_pushboolean(L, result);  /* 16.01.2013, true = input is available, false = timeout, fail in case of an exception */
  return ((mode == 8) ? 4 : 2); /* 16.01.2013 */
}


/* Terminates the server or client denoted by its socket handle and returns true on success, and false otherwise.
   The procedure is a binding to C's close function (closesocket in Windows). */
static int net_close (lua_State *L) {
  size_t i, top;
  AGN_SOCKET sock;
  top = lua_gettop(L);
  for (i=1; i <= top; i++) {
    sock = luaL_checkinteger(L, i);
    checksocket(L, sock, "net.close");
    if (close_socket(sock) == 0) {  /* socket closing has been successful ? */
      if (treedelete(socketattribs, sock) == 0)
        luaL_error(L, "Error in " LUA_QS ": could not delete data from socket administration table.", "net.close");
      sock = INVALID_SOCKET;
    } else {
      int flag = (i == top - 1);
      lua_pushfalse(L);
      lua_pushstring(L, "could not close socket ");
      lua_pushstring(L, lua_tostring(L, i));  /* 2.3.0 RC 2 fix */
      if (flag) lua_pushstring(L, ", aborting closing remaining ones");
      lua_concat(L, (flag) ? 3 : 2);
      return 2;  /* 2.3.0 RC 2 fix */
    }
  }
  lua_pushtrue(L);
  return 1;
}


#ifdef _WIN32
static int net_openwinsock (lua_State *L) {
  WORD wVersionRequested;
  WSADATA wsaData;
  /* initialise TCP for Windows (Winsock) */
  wVersionRequested = MAKEWORD(1, 1);
  if (WSAStartup (wVersionRequested, &wsaData) != 0) {
    if (lua_gettop(L) == 0)
      errornosock(L, "initialisation of Winsock failed.", "net.openwinsock");
    else {
      lua_pushfail(L);
      lua_pushstring(L, wstrerror(WSAGetLastError()));
      return 2;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int net_closewinsock (lua_State *L) {
  /* Cleanup Winsock */
  if (WSACleanup() != 0) {
    if (lua_gettop(L) == 0)
      errornosock(L, "Winsock cleanup failed", "net.closewinsock");
    else {
      lua_pushfail(L);
      lua_pushstring(L, wstrerror(WSAGetLastError()));
      return 2;
    }
  }
  lua_pushtrue(L);
  return 1;
}
#endif


/* stops further sends and receives on a socket denoted by its handle. */

static int net_shutdown (lua_State *L) {
  const char *mode;
  int r, m;
  AGN_SOCKET sock;
  STATUS *s;
  sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.shutdown");
  s = getsocketattribs(L, sock, "net.shutdown");
  if (!s->connected) {
    agn_neterrorfail(L, "socket not connected");
  }
  mode = luaL_checkstring(L, 2);
  if (tools_streq(mode, "read")) {  /* 2.16.12 tweak */
    r = shutdown(sock, SHUTDOWN_RD); m = SHUTDOWN_RD;
  } else if (tools_streq(mode, "write")) {
    r = shutdown(sock, SHUTDOWN_WR); m = SHUTDOWN_WR;
  } else if (tools_streq(mode, "readwrite")) {
    r = shutdown(sock, SHUTDOWN_RDWR); m = SHUTDOWN_RDWR;
  } else {
    m = r = -MAX_INT;
    luaL_error(L, "Error in " LUA_QS ": unknown shutdown mode `%s`." "net.shutdown", mode);
  }
  if (r == 0) {
    s->shutdown = m;
    if (treeupdate(socketattribs, sock, s) != 0) {
      luaL_error(L, "Error in " LUA_QS ": could not assign socket to administration table.", "net.shutdown");
    }
  }
  lua_pushboolean(L, r == 0);
  return 1;
}

static int isipv4 (const char *addr) {  /* rewritten 5.1.1, based on the inet_aton() function
  of the LuaSocket 3.1.0 package; license Copyright 2004-2022 Diego Nehab, file inet.c */
  unsigned int a = 0, b = 0, c = 0, d = 0;
  int n = 0, r;
  r = sscanf(addr, "%u.%u.%u.%u%n", &a, &b, &c, &d, &n);
  if (r == 0 || n == 0) return 0;
  addr += n;
  if (*addr) return 0;
  if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
  return 1;
}

/* Taken from:
   https://stackoverflow.com/questions/6253665/how-to-determine-if-a-string-is-a-valid-ipv6-address-in-c
   by Gene Vincent; 5.1.0 */
#ifndef __OS2__
static int isipv6 (const char *addr) {
  unsigned char buf[sizeof(struct in6_addr)];
  return inet_pton(PF_INET6, (const char *)addr, buf);
}
#endif

static int net_isipv4 (lua_State *L) {  /* new 5.1.0 */
  lua_pushboolean(L, isipv4(agn_checkstring(L, 1)));
  return 1;
}

#ifndef __OS2__
static int net_isipv6 (lua_State *L) {  /* new 5.1.0 */
  lua_pushboolean(L, isipv6(agn_checkstring(L, 1)));
  return 1;
}
#endif

/* returns a table containing information on a given host, slimmed 1.6.4, patched 1.6.5, extended 1.8.16 */

/* remark from http://msdn.microsoft.com/en-us/library/windows/desktop/ms738521%28v=vs.85%29.aspx:
   "An application should not try to release the memory used by the returned hostent structure. The application must
   never attempt to modify this structure or to free any of its components." */

static int net_lookup (lua_State *L) {
  int x;
  const char *entry, *ip;
  struct hostent *lu;
  ip = (lua_gettop(L) == 0) ? NET_ADDRESS : agn_checkstring(L, 1);
  if (isipv4(ip)) {  /* numeric IP ? */
    unsigned int addr;
    addr = inet_addr(ip);
    lu = gethostbyaddr((char *)&addr, sizeof(unsigned int), AF_INET);
#ifndef __OS2__
  } else if (isipv6(ip)) {  /* 5.1.0 */
    struct sockaddr_in6 addr6;
    /* taken from: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-gethostbyaddr */
    int rc = inet_pton(AF_INET6, ip, &addr6);
    if (!rc) {
       errornosock(L, "lookup failure", "net.lookup");
    }
    lu = gethostbyaddr((char *)&addr6, 16, AF_INET6);
#endif
  } else {
    lu = gethostbyname(ip);
  }
  if (!lu) {  /* Report lookup failure */
    errornosock(L, "lookup failure", "net.lookup");
  }
  lua_createtable(L, 0, 4);
  lua_rawsetstringstring(L, -1, "official", lu->h_name);
  lua_pushstring(L, "alias");
  lua_newtable(L);
  for (x=0; lu->h_aliases[x]; ++x) {
    entry = lu->h_aliases[x];
    lua_rawsetilstring(L, -1, x + 1, entry, tools_strlen(entry));  /* 2.17.8 tweak */
  }
  lua_rawset(L, -3);
  if (lu->h_addrtype == AF_INET) {
    lua_rawsetstringstring(L, -1, "type", "IPv4");
  } else if (lu->h_addrtype == AF_INET6) {  /* 5.1.1 fix */
    lua_rawsetstringstring(L, -1, "type", "IPv6");
  } else {
    lua_rawsetstringstring(L, -1, "type", "unknown");
  }
  lua_rawsetstringstring(L, -1, "type", lu->h_addrtype == AF_INET ? "IPv4" : "unknown");
  if (lu->h_addrtype == AF_INET || lu->h_addrtype == AF_INET6) {
    lua_pushstring(L, "networkaddress");
    lua_newtable(L);
    for (x=0; lu->h_addr_list[x]; ++x ) {
      if (lu->h_addrtype == AF_INET) {
        entry = inet_ntoa(*(struct in_addr *)lu->h_addr_list[x]);
#ifndef __OS2__
      } else if (lu->h_addrtype == AF_INET6) {
        unsigned char buf[sizeof(struct in6_addr)];
        char str[INET6_ADDRSTRLEN];
        entry = inet_ntop(AF_INET6, buf, str, INET6_ADDRSTRLEN);
#endif
      } else {
        entry = NULL;
        luaL_error(L, "Error in " LUA_QS ": this should not happen." "net.lookup");
      }
      lua_rawsetilstring(L, -1, x + 1, entry, tools_strlen(entry));  /* 2.17.8 tweak */
    }
    lua_rawset(L, -3);
  }
  return 1;
}


/* Returns all open sockets along with their respective attributes. The return is a table with its keys
   the open sockets, and their entries tables containing information whether the socket is
   a client (false) or server socket (true), their own address (a string), their own port (a number),
   the protocol used (a number), and whether the socket works in blocking (true) or non-blocking mode
   (false), in this order. */
static int net_opensockets (lua_State *L) {
  lua_createtable(L, 0, socketattribs->size);
  treetraverse(L, socketattribs->root);
  return 1;
}


/* In Windows, checks whether you are currently connected to the internet and returns `true` or `false`. The function
   is not available on other platforms. 2.16.10 */
static int net_isconnected (lua_State *L) {
  int rc;
#ifdef _WIN32
  DWORD flags;
  /* taken from: https://nsis-dev.github.io/NSIS-Forums/html/t-196325.html
     Windows NT Use version 4.0. Implemented as ANSI and Unicode functions.
     Windows Use Windows 95 and later. Implemented as ANSI and Unicode functions. */
  rc = InternetGetConnectedState(&flags, 0);
#else
  /* 5.1.0, idea taken from:
     https://www.linuxquestions.org/questions/programming-9/c-checking-internet-connection-248003/
     One of the few `net` things that actually work in OS/2, 7.5.16 change */
	rc = gethostbyname("www.nasa.gov") != NULL;
#endif
  lua_pushboolean(L, rc != 0);
  return 1;
}


/* The function converts a non-negative 2-byte integer value from host to network byte order. */
static int net_htons (lua_State *L) {
 uint16_t s = (uint16_t)agn_checknonnegint(L, 1);
  lua_pushnumber(L, htons(s));
  return 1;
}


/* The function converts a non-negative 2-byte integer value from network to host byte order. 7.6.10 */
static int net_ntohs (lua_State *L) {
  uint16_t s = (uint16_t)agn_checknonnegint(L, 1);
  lua_pushnumber(L, ntohs(s));
  return 1;
}


/* The function converts a non-negative 4-byte integer value from host to network byte order. 7.6.10 */
static int net_htonl (lua_State *L) {
  uint32_t s = (uint32_t)agn_checknonnegint(L, 1);
  lua_pushnumber(L, htonl(s));
  return 1;
}


/* The function converts a non-negative 4-byte integer value from network to host byte order. 7.6.10 */
static int net_ntohl (lua_State *L) {
  uint32_t s = (uint32_t)agn_checknonnegint(L, 1);
  lua_pushnumber(L, ntohl(s));
  return 1;
}


/* Taken from: LuaSocket 3.1.0 package; license Copyright 2004-2022 Diego Nehab, file inet.c; 5.1.1

   Converts from host name to address which can be an IPv4 or IPv6 address or host name.

   The function returns a table with all information returned by the resolver. In case of error,
   the function returns null followed by an error message.

  > net.getaddrinfo('localhost'):
  [[addr ~ ::1, family ~ ipv6], [addr ~ 127.0.0.1, family ~ ipv4]]

  > net.getaddrinfo('www.nasa.gov'):
  [[addr ~ 192.0.66.108, family ~ ipv4]] */
static int net_getaddrinfo (lua_State *L) {
  const char *hostname = luaL_checkstring(L, 1);
  struct addrinfo *iterator = NULL, *resolved = NULL;
  struct addrinfo hints;
  int i = 1, ret = 0;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
#ifdef _WIN32
  init_network_api(L, "net.getaddrinfo");
#endif
  ret = safe_getaddrinfo(hostname, NULL, &hints, &resolved);
  if (ret != 0) {
    agn_neterror(L)
  }
  lua_newtable(L);
  for (iterator = resolved; iterator; iterator = iterator->ai_next) {
    char hbuf[NI_MAXHOST];
    ret = safe_getnameinfo(iterator->ai_addr, (socklen_t) iterator->ai_addrlen,
          hbuf, (socklen_t) sizeof(hbuf), NULL, 0, NI_NUMERICHOST);
    if (ret) {
      safe_freeaddrinfo(resolved);
      agn_neterror(L);
    }
    lua_pushnumber(L, i);
    lua_newtable(L);
    switch (iterator->ai_family) {
      case AF_INET:
        lua_pushliteral(L, "family");
        lua_pushliteral(L, "ipv4");
        lua_settable(L, -3);
        break;
#ifndef __OS2__
      case AF_INET6:
        lua_pushliteral(L, "family");
        lua_pushliteral(L, "ipv6");
        lua_settable(L, -3);
        break;
#endif
      case AF_UNSPEC:
        lua_pushliteral(L, "family");
        lua_pushliteral(L, "unspec");
        lua_settable(L, -3);
        break;
      default:
        lua_pushliteral(L, "family");
        lua_pushliteral(L, "unknown");
        lua_settable(L, -3);
    }
    lua_pushliteral(L, "addr");
    lua_pushstring(L, hbuf);
    lua_settable(L, -3);
    lua_settable(L, -3);
    i++;
  }
  safe_freeaddrinfo(resolved);
  return 1;
}



/* The following functions have been taken from: LuaSocket 3.1.0 package; license Copyright 2004-2022 Diego Nehab,
   files inet.c, io.c, io.h, usocket.c, wsocket.c; 7.5.16 */

enum {
  IO_DONE = 0,        /* operation completed successfully */
  IO_TIMEOUT = -1,    /* operation timed out */
  IO_CLOSED = -2,     /* the connection has been closed */
	IO_UNKNOWN = -3
};

/* platform independent error messages */
#define PIE_HOST_NOT_FOUND "host not found"
#define PIE_ADDRINUSE      "address already in use"
#define PIE_ISCONN         "already connected"
#define PIE_ACCESS         "permission denied"
#define PIE_CONNREFUSED    "connection refused"
#define PIE_CONNABORTED    "closed"
#define PIE_CONNRESET      "closed"
#define PIE_TIMEDOUT       "timeout"
#define PIE_AGAIN          "temporary failure in name resolution"
#define PIE_BADFLAGS       "invalid value for ai_flags"
#define PIE_BADHINTS       "invalid value for hints"
#define PIE_FAIL           "non-recoverable failure in name resolution"
#define PIE_FAMILY         "ai_family not supported"
#define PIE_MEMORY         "memory allocation failure"
#define PIE_NONAME         "host or service not provided, or not known"
#define PIE_OVERFLOW       "argument buffer overflow"
#define PIE_PROTOCOL       "resolved protocol is unknown"
#define PIE_SERVICE        "service not supported for socket type"
#define PIE_SOCKTYPE       "ai_socktype not supported"

static const char *io_strerror (int err) {
  switch (err) {
    case IO_DONE: return NULL;
    case IO_CLOSED: return "closed";
    case IO_TIMEOUT: return "timeout";
    default: return "unknown error";
  }
}

#ifndef _WIN32
static int socket_gethostbyaddr (const char *addr, socklen_t len, struct hostent **hp) {
  *hp = gethostbyaddr(addr, len, AF_INET);
  if (*hp) return IO_DONE;
  else if (h_errno) return h_errno;
  else if (errno) return errno;
  else return IO_UNKNOWN;
}

static int socket_gethostbyname (const char *addr, struct hostent **hp) {
  *hp = gethostbyname(addr);
  if (*hp) return IO_DONE;
  else if (h_errno) return h_errno;
  else if (errno) return errno;
  else return IO_UNKNOWN;
}

static const char *socket_hoststrerror (int err) {
  if (err <= 0) return io_strerror(err);
  switch (err) {
    case HOST_NOT_FOUND: return PIE_HOST_NOT_FOUND;
    default: return gai_strerror(err);
  }
}
#else
static int socket_gethostbyaddr (const char *addr, socklen_t len, struct hostent **hp) {
  *hp = gethostbyaddr(addr, len, AF_INET);
  if (*hp) return IO_DONE;
  else return WSAGetLastError();
}

static int socket_gethostbyname (const char *addr, struct hostent **hp) {
  *hp = gethostbyname(addr);
  if (*hp) return IO_DONE;
  else return WSAGetLastError();
}

static const char *socket_hoststrerror (int err) {
  if (err <= 0) return io_strerror(err);
  switch (err) {
    case WSAHOST_NOT_FOUND: return PIE_HOST_NOT_FOUND;
    default: return wstrerror(err);
  }
}
#endif

static int inet_gethost (const char *address, struct hostent **hp) {
  struct in_addr addr;
  if (inet_aton(address, &addr))
    return socket_gethostbyaddr((char *) &addr, sizeof(addr), hp);
  else
    return socket_gethostbyname(address, hp);
}

/* Passes all resolver information to Lua as a table */
static void inet_pushresolved (lua_State *L, struct hostent *hp) {
  char **alias;
  struct in_addr **addr;
  int i, resolved;
  luaL_checkstack(L, 3, "not enough stack space");
  lua_newtable(L);
  resolved = lua_gettop(L);
  lua_pushstring(L, "name");
  lua_pushstring(L, hp->h_name);
  lua_settable(L, resolved);
  lua_pushstring(L, "ip");
  lua_pushstring(L, "alias");
  i = 1;
  alias = hp->h_aliases;
  lua_newtable(L);
  if (alias) {
    while (*alias) {
      luaL_checkstack(L, 2, "not enough stack space");
      lua_pushnumber(L, i++);
      lua_pushstring(L, *alias++);
      lua_settable(L, -3);
    }
  }
  lua_settable(L, resolved);
  i = 1;
  lua_newtable(L);
  addr = (struct in_addr **)hp->h_addr_list;
  if (addr) {
    while (*addr) {
      luaL_checkstack(L, 2, "not enough stack space");
      lua_pushnumber(L, i++);
      lua_pushstring(L, inet_ntoa(**addr++));
      lua_settable(L, -3);
    }
  }
  lua_settable(L, resolved);
}


/* Converts from IPv4 address to host name. The input `address` can be an IP address or host name.

   The function returns a string with the canonic host name of the given address, followed by a
   table with all information returned by the resolver. In case of error, the function returns
   `null` followed by an error message.

   See also: net.address, net.getaddrinfo, net.gethostname, net.toip. */

static int net_tohostname (lua_State *L) {
  const char *address = luaL_checkstring(L, 1);
  struct hostent *hp = NULL;
  int err = inet_gethost(address, &hp);
  luaL_checkstack(L, 2, "not enough stack space");
  if (err != IO_DONE) {
    lua_pushnil(L);
    lua_pushstring(L, socket_hoststrerror(err));
  } else {
    lua_pushstring(L, hp->h_name);
    inet_pushresolved(L, hp);
  }
  return 2;
}


/* Converts from host name to IPv4 address. The input `address` can be an IP address or host name.

   Returns a string with the first IP address found for address, followed by a table with all
   information returned by the resolver. In case of error, the function returns `null` followed
   by an error message.

   See also: net.address, net.getaddrinfo, net.gethostname, net.tohostname. 6.7.10 */

static int net_toip (lua_State *L) {
  const char *address = luaL_checkstring(L, 1);
  struct hostent *hp = NULL;
  int err = inet_gethost(address, &hp);
  luaL_checkstack(L, 2, "not enough stack space");
  if (err != IO_DONE) {
    lua_pushnil(L);
    lua_pushstring(L, socket_hoststrerror(err));
    return 2;
  }
  lua_pushstring(L, inet_ntoa(*((struct in_addr *)hp->h_addr)));
  inet_pushresolved(L, hp);
  return 2;
}


/* LuaSocket 3.1.0 end */


/* Gets the host name of the system on which it is called and returns a string. Note that this is not the DNS hostname.
   In case of error, the function returns `false` plus the error description as a string.
   Taken from: LuaSocket 3.1.0 package and adapted; license Copyright 2004-2022 Diego Nehab, file inet.c; 5.1.1 */
static int net_gethostname (lua_State *L) {
  char name[257];
  name[256] = '\0';
  if (gethostname(name, 256) < 0) {  /* name too long ? */
    char name2[513];
    name2[512] = '\0';
    if (gethostname(name2, 512) < 0) {
      agn_neterror(L);
    }
    lua_pushstring(L, name2);
    return 1;
  }
  lua_pushstring(L, name);
  return 1;
}


/* Returns the local address information associated to the given socket: a string with the local IP address,
   the port number, and a string with the family ("ipv4" or "ipv6", the protocol in use). In case of an
   error, the function returns `false` plus the error description as a string.
   `net.address` is the Agena pendant to the C function `getsockname`.
   Taken from: LuaSocket 3.1.0 package and adapted; license Copyright 2004-2022 Diego Nehab, file inet.c; 5.1.1 */
#ifndef __OS2__
static int net_address (lua_State *L) {
  int err;
  struct sockaddr_storage peer;
  socklen_t peer_len = sizeof(peer);
  char name[INET6_ADDRSTRLEN];
  char port[6];  /* 65535 = 5 bytes + \0 to terminate it */
  AGN_SOCKET sock;
  STATUS *s;
  sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.address");
  s = getsocketattribs(L, sock, "net.address");
  if (getsockname(sock, (struct sockaddr *)&peer, &peer_len) < 0) {
    agn_neterror(L);
  }
	err = getnameinfo((struct sockaddr *)&peer, peer_len,
		name, INET6_ADDRSTRLEN, port, 6, NI_NUMERICHOST | NI_NUMERICSERV);
  if (err) {
    agn_neterror(L);
  }
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushstring(L, name);
  lua_pushinteger(L, (int)strtol(port, (char **)NULL, 10));
  switch (s->family) {
    case AF_INET:   lua_pushliteral(L, "ipv4"); break;
    case AF_INET6:  lua_pushliteral(L, "ipv6"); break;
    case AF_UNSPEC: lua_pushliteral(L, "unspec"); break;
    default:        lua_pushliteral(L, "unknown");
  }
  return 3;
}
#else
static int net_address (lua_State *L) {
  socklen_t length;
  AGN_SOCKET sock;
  STATUS *s;
  sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.address");
  s = getsocketattribs(L, sock, "net.address");
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.1.0 fix */
  if (s->family == AF_INET) {
    struct sockaddr_in addr;
    length = sizeof(addr);
    if (getsockname(sock, (struct sockaddr*)&addr, &length) != 0) {
      agn_neterror2(L, "could not get address");
    }
    lua_pushstring(L, inet_ntoa(addr.sin_addr));
    lua_pushinteger(L, (unsigned)ntohs(addr.sin_port));
  } else {
    luaL_error(L, "Error in " LUA_QS ": this should not happen.", "net.address");
  }
  return 2;
}
#endif


/* Returns address information about the remote side of the connection, the `peer`: a string with the IP address of the peer,
   the port number the peer is using for the connection, and a string with the family ("ipv4" or "ipv6", the protocol
   in use). In case of error, the function returns `false` plus the error description as a string.
   `net.remoteaddress` is the Agena pendant to the C function `getpeername`.
   Taken from: LuaSocket 3.1.0 package and adapted; license Copyright 2004-2022 Diego Nehab, file inet.c; 5.1.1 */
#ifndef __OS2__
static int net_remoteaddress (lua_State *L) {
  AGN_SOCKET sock;
  int err;
  struct sockaddr_storage peer;
  socklen_t peer_len = sizeof(peer);
  char name[INET6_ADDRSTRLEN];
  char port[6]; /* 65535 = 5 bytes + 0 to terminate it */
  sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.remoteaddress");
  if (getpeername(sock, (struct sockaddr *)&peer, &peer_len) < 0) {
    agn_neterror(L);
  }
	err = safe_getnameinfo((struct sockaddr *)&peer, peer_len,
        name, INET6_ADDRSTRLEN,
        port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
  if (err) {
    agn_neterror(L);
  }
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushstring(L, name);
  lua_pushinteger(L, (int)strtol(port, (char **)NULL, 10));
  switch (peer.ss_family) {
    case AF_INET:   lua_pushliteral(L, "ipv4"); break;
    case AF_INET6:  lua_pushliteral(L, "ipv6"); break;
    case AF_UNSPEC: lua_pushliteral(L, "unspec"); break;
    default:        lua_pushliteral(L, "unknown");
  }
  return 3;
}
#else
static int net_remoteaddress (lua_State *L) {
  socklen_t length;
  AGN_SOCKET sock;
  STATUS *s;
  sock = luaL_checkinteger(L, 1);
  checksocket(L, sock, "net.remoteaddress");
  s = getsocketattribs(L, sock, "net.remoteaddress");
  if (s->family == AF_INET) {
    struct sockaddr_in addr;
    length = sizeof addr;
    if (getpeername(sock, (struct sockaddr*)&addr, &length) != 0) {
      agn_neterror2(L, "could not get address");
    }
    lua_pushstring(L, inet_ntoa(addr.sin_addr));
    lua_pushinteger(L, (unsigned)ntohs(addr.sin_port));
  } else {
    luaL_error(L, "Error in " LUA_QS ": this should not happen.", "net.remoteaddress");
  }
  return 2;
}
#endif


static void netcleanup (void) {  /* 1.6.5, cleanup is not called when pressing CTRL+C */
  treepurge(socketattribs);
#ifdef _WIN32
  WSACleanup();
#endif
}

static void netsigcleanup (int sig) {  /* 2.14.0, called when pressing CTRL+C */
  treepurge(socketattribs);
#ifdef _WIN32
  WSACleanup();
#endif
}


static const luaL_Reg netlib[] = {
  {"accept",        net_accept},            /* added on April 12, 2012 */
  {"address",       net_address},           /* added on April 09, 2012 */
  {"bind",          net_bind},              /* added on April 12, 2012 */
  {"block",         net_block},             /* added on April 14, 2012 */
  {"close",         net_close},             /* added on April 05, 2012 */
  {"connect",       net_connect},           /* added on April 05, 2012 */
  {"getaddrinfo",   net_getaddrinfo},       /* added on July 06, 2025 */
  {"gethostname",   net_gethostname},       /* added on July 06, 2025 */
  {"htonl",         net_htonl},             /* added on June 28, 2026 */
  {"htons",         net_htons},             /* added on July 03, 2025 */
#ifdef __OS2__
  {"init",          net_init},              /* added on August 02, 2025 */
#endif
  {"isconnected",   net_isconnected},       /* added 2.16.10, 25.11.2019 */
  {"isipv4",        net_isipv4},            /* added on July 02, 2025 */
#ifndef __OS2__
  {"isipv6",        net_isipv6},            /* added on July 02, 2025 */
#endif
  {"keep",          net_keep},              /* added on June 24, 2026 */
  {"listen",        net_listen},            /* added on April 05, 2012 */
  {"lookup",        net_lookup},            /* added on April 05, 2012 */
  {"ntohl",         net_ntohl},             /* added on June 28, 2026 */
  {"ntohs",         net_ntohs},             /* added on June 28, 2026 */
  {"open",          net_open},              /* added on April 05, 2012 */
  {"opensockets",   net_opensockets},       /* added on April 12, 2012 */
  {"receive",       net_receive},           /* added on April 05, 2012 */
  {"remoteaddress", net_remoteaddress},     /* added on April 05, 2012 */
  {"send",          net_send},              /* added on April 05, 2012 */
  {"shutdown",      net_shutdown},          /* added on May 07, 2012 */
  {"survey",        net_survey},            /* added on April 22, 2012 */
  {"tohostname",    net_tohostname},        /* added on June 03, 2026 */
  {"toip",          net_toip},              /* added on June 28, 2026 */
#ifdef _WIN32
  {"closewinsock",  net_closewinsock},      /* added on April 17, 2012 */
  {"openwinsock",   net_openwinsock},       /* added on April 17, 2012 */
#endif
  {NULL, NULL}
};


static int nopened = 0;  /* Agena 1.7.9a */

/*
** Open net library
*/
LUALIB_API int luaopen_net (lua_State *L) {
  if (++nopened == 1) { /* avoid segmentation faults when readlib'ing the net library more than once, at exit/bye of Agena, 1.7.9a, 2.3.0 RC 2 */
                      /* immediate exit, just not calling signal if nopened > 1 does not help, XXX could be improved. */
#ifdef _WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    /* initialise TCP for Windows (Winsock) */
    /* wVersionRequested = MAKEWORD(1, 1); */
    wVersionRequested = MAKEWORD(2, 2);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
      int err = WSAGetLastError();
      luaL_error(L, "Error in " LUA_QS " package: initialisation of Winsock failed:\n(%d) %s", "net", err, wstrerror(err));
    }
#else
    /* 1.6.4, some of the functions like `net.send` would crash if a socket has been disconnected */
    signal(SIGPIPE, SIG_IGN);  /* in UNIX, ignore signals and process the return values of the respective C network functions;
    if not called, Agena might crash, e.g. if send() tries to write data to a socket that discontinued a connection. */
#endif
    socketattribs = treeinit();
    if (socketattribs == NULL) {
#ifdef _WIN32
      WSACleanup();  /* 5.1.1 fix */
#endif
      luaL_error(L, "Error when initialising `net` package: could not create socket administration table.");
    }
    atexit(netcleanup);  /* 1.6.5, cleanup is not called when pressing CTRL+C */
    signal(SIGTERM, netsigcleanup);  /* 2.14.0 fix for CTRL+c */
  }
  luaL_register(L, AGENA_NETLIBNAME, netlib);
  lua_rawsetstringstring(L, -1, "initstring", AGENA_LIBVERSION);
  lua_rawsetstringnumber(L, -1, "defaultportraw", NET_PORT);
  lua_rawsetstringnumber(L, -1, "defaultportBigEndian", htons(NET_PORT));
  lua_createtable(L, 0, 2);
  lua_rawsetstringnumber(L, -1, "maxnsockets", FD_SETSIZE);  /* in Winsock2, FD_SETSIZE is not the number of
     _bits_ assigned to store the largest server socket handle, but the maximum number of open sockets;
     however, Windows does not query FD_SETSIZE when opening new sockets. XXX -> Solaris ??? */
  lua_createtable(L, 1, 0);
  lua_rawsetistring(L, -1, AF_INET, "IPv4");
#ifndef __OS2__
  lua_rawsetistring(L, -1, AF_INET6, "IPv6");
#endif
  lua_setfield(L, -2, "protocols");
  lua_setfield(L, -2, "admin");  /* table for information on internal network administration */
  return 1;
}

/* ====================================================================== */

#endif /* UNIX, Apple, Windows, OS/2 */

