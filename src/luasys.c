/* Lua System (LuaSys v1.8): System library for Lua

   Written by: Nodir Temirkhodjaev.

   There are no copyright remarks in Mr. Temirkhodjaev source files, accompanying files or his GIT site.

   Taken from: https://github.com/tnodir/luasys, file sys/sys_comm.c, as of July 28, 2017. */

#define luasys_c

#include "luasys.h"

#define SYS_FILE_PERMISSIONS	0644  /* default file permissions */


/* ************************************************************************************ */

/* General API functions (and candidates for general inclusion into Agena XXX */

/*
 * Arguments: ..., [number]
 * Returns: string
 */
 
#define aux_pushmsg(buf) { \
  lua_pushliteral(L, buf); \
  return 1; \
}

int sys_strerror (lua_State *L) {
  const int err = luaL_optint(L, -1, SYS_ERRNO);
#ifndef _WIN32
#if defined(BSD) || (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600)
  char buf[512];
  if (!err) { aux_pushmsg("OK"); }
  if (!strerror_r(err, buf, sizeof(buf))) {
    lua_pushstring(L, buf);
    return 1;
  }
#endif
  lua_pushstring(L, strerror(err));
#else
  const int flags = FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM;
  WCHAR buf[512];
  if (!err) { aux_pushmsg("OK"); };
  if (is_WinNT
   ? FormatMessageW(flags, NULL, err, 0, buf, sizeof(buf) / sizeof(buf[0]), NULL)
   : FormatMessageA(flags, NULL, err, 0, (char *) buf, sizeof(buf), NULL)) {
    char *s = filename_to_utf8(buf);
    if (s) {
      char *cp;
      for (cp=s; *cp != '\0'; ++cp) {
        if (*cp == '\r' || *cp == '\n') *cp = ' ';
      }
      lua_pushstring(L, s);
      xfree(s);
      return 1;
    }
  }
  lua_pushfstring(L, "System error %d", err);
#endif
  return 1;
}

/* Returns: nil, string */
int sys_seterror (lua_State *L, int err) {
  if (err) {
#ifndef _WIN32
    errno = err;
#else
    SetLastError(err);
#endif
  }
  lua_pushnil(L);
  sys_strerror(L);
  lua_pushvalue(L, -1);
  lua_setglobal(L, SYS_ERROR_MESSAGE);
  return 2;
}


/* ************************************************************************************ */

/* Lua System: Win32 specifics: UTF-8 */

#ifdef _WIN32
/*
 * Convert microsoft unicode to UTF-8.
 */
static void *utf16_to_utf8 (const WCHAR *ws) {
  char *s;
  int n;
  n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, 0);
  s = malloc(n);
  if (!s) return NULL;
  n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, n, NULL, 0);
  if (!n) {
    free(s);
    s = NULL;
  }
  return s;
}


/* ************************************************************************************ */

/* Taken from: sys/win32/win32_utf8.c */

/*
 * Convert an ansi string to microsoft unicode, based on the
 * current codepage settings for file apis.
 */
static void *mbcs_to_utf16 (const char *mbcs) {
  WCHAR *ws;
  const int codepage = AreFileApisANSI() ? CP_ACP : CP_OEMCP;
  int n;
  n = MultiByteToWideChar(codepage, 0, mbcs, -1, NULL, 0);
  ws = malloc(n * sizeof(WCHAR));
  if (!ws) return NULL;
  n = MultiByteToWideChar(codepage, 0, mbcs, -1, ws, n);
  if (!n) {
    free(ws);
    ws = NULL;
  }
  return ws;
}


/*
 * Convert multibyte character string to UTF-8.
 */
static void *mbcs_to_utf8 (const char *mbcs) {
  char *s;
  WCHAR *ws;
  ws = mbcs_to_utf16(mbcs);
  if (!ws) return NULL;
  s = utf16_to_utf8(ws);
  free(ws);
  return s;
}


/*
 * Convert OS filename to UTF-8.
 */
char *filename_to_utf8 (const void *s) {
  return is_WinNT ? utf16_to_utf8(s) : mbcs_to_utf8(s);
}
#endif

/* ************************************************************************************ */


