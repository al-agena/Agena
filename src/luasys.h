/* Lua System (LuaSys v1.8): System library for Lua. header file
   Written by: Nodir Temirkhodjaev.
   There are no copyright remarks in Mr. Temirkhodjaev source files, accompanying files or his GIT site.
   Taken from: https://github.com/tnodir/luasys, file common.n & lua_sys.h, as of July 28, 2017. */

#ifndef luasys_h
#define luasys_h

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef _WIN32
#include <sys/stat.h>
#endif

#ifndef _WIN32
#include <sys/ioctl.h>
#include <sys/wait.h>
#if !((defined(__SVR4) && defined (__sun)) || defined(__DJGPP__))
#if defined(__linux__) && defined(__GNUC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 31))
#include <linux/sysctl.h>  /* 2.25.5 fix since sys/sysctl.h has been removed with GLIBC 2.31 */
#else
#include <sys/sysctl.h>
#endif
#endif
#endif  /* of ifndef _WIN32 */

#define LUA_LIB

#ifndef LUA51
#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"
#else
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#endif


#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT	0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <mmsystem.h>	/* timeGetTime */

#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef SSIZE_T		ssize_t;
#endif

#ifndef ULONG_PTR
typedef SIZE_T		ULONG_PTR, DWORD_PTR;
#endif

#define SYS_ERRNO		GetLastError()

#else

#define _GNU_SOURCE	 /* pthread_*affinity_np */

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS	64
#define _LARGEFILE_SOURCE	1
#endif

#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/param.h>
#endif

#define SYS_ERRNO		errno

#endif  /* of ifdef _WIN32 */

/*
 * 64-bit integers
 */

#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64			int64_t;
typedef unsigned __int64	uint64_t;
#else
#include <stdint.h>
#endif

#define INT64_MAKE(lo,hi)	(((int64_t) (hi) << 32) | (unsigned int) (lo))
#define INT64_LOW(x)		((unsigned int) (x))
#define INT64_HIGH(x)		((unsigned int) ((x) >> 32))


/*
 * File and Socket Handles
 */

/* #define FD_TYPENAME	"sys.handle" */

#ifdef _WIN32
typedef HANDLE	fd_t;
typedef SOCKET	sd_t;
#else
typedef int	fd_t;
typedef int	sd_t;
#endif


/*
 * Error Reporting
 */

#define SYS_ERROR_MESSAGE	"SYS_ERR"

int sys_seterror (lua_State *L, int err);


#ifdef _WIN32

#if defined(_WIN32_WCE) || defined(WIN32_VISTA)
#define is_WinNT	1
#else
/* extern int is_WinNT; */
#define is_WinNT	0
#endif

/*
 * Convert Windows OS filenames to UTF-8
 */

void *utf8_to_filename (const char *s);
char *filename_to_utf8 (const void *s);

#endif

#endif
