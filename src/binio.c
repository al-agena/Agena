/*
** $Id: biniolib.c,v 0.1 13.07.2008 $
** Library for reading and writing binary files
** See Copyright Notice in agena.h
*/

#include <errno.h>      /* for strerror */
#include <fcntl.h>      /* for open */
#include <stdio.h>      /* also for BUFSIZ */
#include <string.h>
#include <unistd.h>     /* for access, ftruncate and close */

#define binio_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncfg.h"     /* for Endianness */
#include "agncmpt.h"
#include "agnxlib.h"
#include "long.h"

/* ---------------- utilities taken from liolib.c ---------------------------- */

/*
** When creating file handles, always creates a `closed' file handle
** before opening the actual file; so, if there is a memory error, the
** file is not left opened.
*/
static FILE **newfile (lua_State *L) {  /* taken from liolib.c */
  FILE **pf = (FILE **)lua_newuserdata(L, sizeof(FILE *));
  *pf = NULL;  /* file handle is currently `closed' */
  luaL_getmetatable(L, AGN_FILEHANDLE);
  lua_setmetatable(L, -2);
  return pf;
}

/* ------------------------------ main part --------------------------------- */

static int binio_open (lua_State *L) {
  int readwrite, en, nargs, append;
  nargs = lua_gettop(L);
  FILE **hnd = newfile(L);
  const char *file;
  file = agn_checkstring(L, 1);  /* 0.13.4 patch: does not accept numbers */
  agn_checkvalidpath(L, file, 1, "binio.open");  /* 7.6.3 */
  readwrite = 0;
  /* no option given or option = 'append': open in readwrite mode; option <> 'append' given: open in readonly mode */
  append = 0;
  if (nargs == 2 && agn_isstring(L, 2)) {  /* appending instead of overwriting ?  2.17.8 extension */
    const char *s = agn_tostring(L, 2);
    append = tools_streq(s, "append") || tools_streq(s, "a");
    readwrite = tools_streq(s, "write") || tools_streq(s, "w");  /* 2.28.6 to avoid confusion when given explicit read/write request */
  }
  readwrite = readwrite || append || nargs == 1;
  /* if the file already exists, check whether you have the proper file rights */
  if (access(file, F_OK) == 0) {  /* file already exists ? */
    if (access(file, (readwrite) ? R_OK|W_OK : R_OK) == -1)
      luaL_error(L, "Error in " LUA_QS ": missing permissions for " LUA_QS ".", "binio.open", file);
  }
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  *hnd = (readwrite) ? my_fopen(file, append) : my_froopen(file);
  en = errno;  /* 1.12.2 */
  if (*hnd == NULL) {  /* file does not yet exist ? */
    if (readwrite) {  /* a new file shall be created ? */
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      *hnd = my_fcreate(file);
      en = errno;
      if (*hnd == NULL) {
        luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": %s.", "binio.open", file, my_ioerror(en));
      }
    } else
      luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": %s.", "binio.open", file, my_ioerror(en));
  }
  /* enter new open file to global binio.openfiles table */
  if (agnL_gettablefield(L, "binio", "openfiles", "binio.open", 1) == LUA_TTABLE) {  /* 0.20.2 & 1.6.4, avoid segmentation faults */
    /* DO NOT use lua_istable since it checks a value on the stack and not the return value of agn_gettablefield (a nonneg int) */
    lua_pushinteger(L, fileno(*hnd));
    lua_pushstring(L, file);
    lua_rawset(L, -3);
  }  else
    luaL_error(L, "Error in " LUA_QS ": 'binio.openfiles' table not found.", "binio.open");  /* Agena 1.6.0 */
  agn_poptop(L);  /* delete "openfiles" */
  return 1;  /* return userdata */
}


#define biniotopnfile(L, n)   ((FILE **)luaL_checkudata(L, n, AGN_FILEHANDLE))

static int binio_close (lua_State *L) {  /* rewritten 2.24.0 to fix problems with `invalid file handle` error messages */
  int nargs, hnd, i, en, result;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  result = 1;
  for (i=0; i < nargs; i++) {
    FILE **p = biniotopnfile(L, i + 1);
    hnd = agn_tofileno(L, i + 1, 0);
    if (hnd < 3) {  /* avoid standard streams to be closed; better be sure than sorry */
      result = 0;
      continue;
    }
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    int ok = (fclose(*p) == 0);
    if (!ok) {  /* 1.12.2 */
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.close", hnd, my_ioerror(en));
    } else {
      /* delete file from global binio.openfiles table */
      if (agnL_gettablefield(L, "binio", "openfiles", "binio.close", 1) == LUA_TTABLE) {  /* 0.20.2 & 1.6.4, avoid segmentation faults */
        lua_pushinteger(L, hnd);
        lua_pushnil(L);
        lua_rawset(L, -3);
      } else
        luaL_error(L, "Error in " LUA_QS ": 'binio.openfiles' table not found.", "binio.close");  /* Agena 1.6.0 */
      agn_poptop(L);  /* delete "openfiles" */
    }
    *p = NULL;
  }
  lua_pushboolean(L, result);
  return 1;
}


/* base_sync: flushes all unwritten content to the given file.
   Patched December 26, 2009, 0.30.0 */

static int binio_sync (lua_State *L) {
  int hnd, rc1, rc2, en;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.sync");  /* 2.37.5 */
  FILE **p = biniotopnfile(L, 1);  /* 2.37.8 */
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  rc1 = fflush(*p) == 0;           /* flush buffers, 2.37.8 */
  rc2 = tools_fsync(hnd) == 0;     /* 2.37.8 */
  en = errno;  /* 1.12.2 */
  if (rc1 && rc2)
    lua_pushtrue(L);
  else
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.sync", hnd, my_ioerror(en));
  return 1;
}


static int binio_filepos (lua_State *L) {
  int hnd, en, nargs;
  off64_t pos;
  nargs = lua_gettop(L);
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.filepos");  /* 2.37.5 */
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  if (nargs == 2 && agn_isnumber(L, 2)) {  /* 7.3.3 extension: set file position to offset, relative to
    the beginning of the file. */
    lua_Number x = agn_tonumber(L, 2);
    if (x < 0)
      luaL_error(L, "Error in " LUA_QS ": offset must be nonnegative.", "binio.filepos");
    pos = (off64_t)x;
    if (lseek64(hnd, pos, SEEK_SET) == -1) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.seek", hnd, my_ioerror(en));
    } else
      lua_pushtrue(L);  /* successfully changed file position */
    return 1;
  }
  pos = my_fpos(hnd);
  en = errno;
  if (pos == -1)
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.filepos", hnd, my_ioerror(en));
  else
    lua_pushnumber(L, pos);
  return 1;
}


static int binio_seek (lua_State *L) {
  int hnd, en;
  off64_t pos;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.seek");  /* 2.37.5 */
  pos = agnL_checkinteger(L, 2);
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  if (lseek64(hnd, pos, SEEK_CUR) == -1) {
    en = errno;
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.seek", hnd, my_ioerror(en));
  } else
    lua_pushtrue(L);  /* successfully changed file position */
  return 1;
}


static int binio_isfdesc (lua_State *L) {  /* 2.6.1 */
  int hnd, en, r;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.isfdesc");  /* 2.37.5 */
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  r = lseek64(hnd, 0L, SEEK_CUR);
  en = errno;
  lua_pushboolean(L, r != -1 || en != EBADF);  /* EBADF = file descriptor is invalid or not open */
  return 1;
}


static int binio_length (lua_State *L) {  /* faster than using fstat */
  int hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) {
    luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.length");  /* 2.37.5 */
  } else {
#ifdef _WIN32
    lua_pushnumber(L, _filelengthi64(hnd));  /* 2.37.5 fix */
#else
    int en, result;
    off64_t size, oldpos;
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    oldpos = lseek64(hnd, 0L, SEEK_CUR);  /* fstat* is slower */
    en = errno;
    if (oldpos == -1)
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.length", hnd, my_ioerror(en));
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    size = lseek64(hnd, 0L, SEEK_END);
    en = errno;
    if (size == -1)
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.length", hnd, my_ioerror(en));
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    result = lseek64(hnd, oldpos, SEEK_SET);  /* reset cursor to original file position */
    en = errno;
    if (result == -1)
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s", "binio.length", hnd, my_ioerror(en));
    else
      lua_pushnumber(L, size);
#endif
  }
  return 1;
}


static int binio_writechar (lua_State *L) {
  int hnd, en;
  size_t size, nargs, i;
  unsigned char data;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.writechar");  /* 2.37.5 */
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1)
    luaL_error(L, "Error in " LUA_QS ": expected at least a second argument.", "binio.writechar");
  for (i=2; i <= nargs; i++) {
    data = agn_checknumber(L, i);
    size = sizeof(data);
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (write(hnd, &data, size) != (ssize_t)size) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.writechar", hnd, my_ioerror(en));
    }
  }
  lua_pushtrue(L);
  return 1;
}


/* writes bytes stored in a sequence to a file, 0.25.0, 19.07.2009, extended 1.10.5, 05.04.2013 */
static int binio_writebytes (lua_State *L) {
  int hnd, en;
  size_t size, i, nargs, type;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.writebytes");  /* 2.37.5 */
  type = lua_type(L, 2);
  luaL_typecheck(L, type == LUA_TSEQ, 2, "sequence expected", type);
  nargs = agn_seqsize(L, 2);
  if (nargs == 0)
    lua_pushfail(L);  /* better sure than sorry */
  else {
    unsigned char buffer[nargs];
    size = sizeof(buffer);
    for (i=0; i < nargs; i++)
      buffer[i] = (unsigned char)lua_seqrawgetinumber(L, 2, i + 1);
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (write(hnd, &buffer, size) != (ssize_t)size) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.writebytes", hnd, my_ioerror(en));
    }
    else
      lua_pushtrue(L);
  }
  return 1;
}


static int binio_writenumber (lua_State *L) {  /* extended 1.10.5, 05.04.2013 */
  int hnd, en;
  size_t size, nargs, i;
  uint64_t ulong;
  double_cast x;  /* 2.16.3 */
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.writenumber");  /* 2.37.5 */
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  size = sizeof(ulong);
  if (nargs == 1)
    luaL_error(L, "Error in " LUA_QS ": expected at least a second argument.", "binio.writenumber");
  for (i=2; i <= nargs; i++) {
    x.f = agn_checknumber(L, i);
#if BYTE_ORDER != BIG_ENDIAN   /* 2.16.6 code optimisation, 2.17.1 change */
    x.i = tools_swapu64(x.i);  /* only swap Little Endian bytes */
#endif
    ulong = x.i;
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (write(hnd, &ulong, size) != (ssize_t)size) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with #%d: %s.", "binio.writenumber", hnd, my_ioerror(en));
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int binio_writelongdouble (lua_State *L) {  /* 2.35.0 */
  int hnd, en;
  size_t size, nargs, i;
  long double x;
#if BYTE_ORDER == BIG_ENDIAN
    luaL_error(L, "Error in " LUA_QS ": Big Endian platforms are not supported.", "binio.writelongdouble");
#endif
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.writelongdouble");  /* 2.37.5 */
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  size = sizeof(long double);
  if (nargs == 1)
    luaL_error(L, "Error in " LUA_QS ": expected at least a second argument.", "binio.writelongdouble");
  for (i=2; i <= nargs; i++) {
    x = agnL_checkdlongnum(L, i, "binio.writelongdouble");
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (write(hnd, &x, size) != (ssize_t)size) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with #%d: %s.", "binio.writelongdouble", hnd, my_ioerror(en));
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int binio_writelong (lua_State *L) {  /* extended 1.10.5, 05.04.2013 */
  int hnd, en;
  size_t size, nargs, i;
  int32_t data;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.writelong");  /* 2.37.5 */
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1)
    luaL_error(L, "Error in " LUA_QS ": expected at least a second argument.", "binio.writelong");
  for (i=2; i <= nargs; i++) {
    data = agn_checknumber(L, i);
#if BYTE_ORDER != BIG_ENDIAN
    tools_swapint32_t(&data);
#endif
    size = sizeof(data);
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (write(hnd, &data, size) != (ssize_t)size) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.writelong", hnd, my_ioerror(en));
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int binio_writestring (lua_State *L) {  /* extended 1.10.5, 05.04.2013; tuned 2.11.5 */
  int hnd, en;
  size_t size, nargs, i;
  const char *value;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.writestring");  /* 2.37.5 */
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1)
    luaL_error(L, "Error in " LUA_QS ": expected at least a second argument.", "binio.writestring");
  for (i=2; i <= nargs; i++) {
    value = agn_checklstring(L, i, &size);
    my_writel(hnd, size);
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (write(hnd, value, size) != (ssize_t)size) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.writestring", hnd, my_ioerror(en));
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int binio_writeshortstring (lua_State *L) {  /* 0.32.0; tuned 2.11.5 */
  int hnd, en;
  size_t size, nargs, i;
  const char *value;
  hnd = agn_tofileno(L, 1, 0);
    if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.writeshortstring");  /* 2.37.5 */
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1)
    luaL_error(L, "Error in " LUA_QS ": expected at least a second argument.", "binio.writeshortstring");
  for (i=2; i <= nargs; i++) {
    value = agn_checklstring(L, i, &size);
    if (size > 255)
      luaL_error(L, "Error in " LUA_QS " with writing data to file #%d: string too long", "binio.writeshortstring", hnd);
    my_writec(hnd, (unsigned char)size);
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (write(hnd, value, size) != (ssize_t)size) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.writeshortstring", hnd, my_ioerror(en));
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int binio_writeint64 (lua_State *L) {  /* 7.3.3 */
  int hnd, en;
  size_t size, nargs, i;
  int64_t x;
#if BYTE_ORDER == BIG_ENDIAN
    luaL_error(L, "Error in " LUA_QS ": Big Endian platforms are not supported.", "binio.writeint64");
#endif
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.writeint64");
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  size = sizeof(int64_t);
  if (nargs == 1)
    luaL_error(L, "Error in " LUA_QS ": expected at least a second argument.", "binio.writeint64");
  for (i=2; i <= nargs; i++) {
    x = agnL_checkint64(L, i, "binio.writeint64");
    set_errno(0);  /* reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (write(hnd, &x, size) != (ssize_t)size) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with #%d: %s.", "binio.writeint64", hnd, my_ioerror(en));
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int binio_writeuint64 (lua_State *L) {  /* 7.3.3 */
  int hnd, en;
  size_t size, nargs, i;
  uint64_t x;
#if BYTE_ORDER == BIG_ENDIAN
    luaL_error(L, "Error in " LUA_QS ": Big Endian platforms are not supported.", "binio.writeuint64");
#endif
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.writeuint64");
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  size = sizeof(uint64_t);
  if (nargs == 1)
    luaL_error(L, "Error in " LUA_QS ": expected at least a second argument.", "binio.writeuint64");
  for (i=2; i <= nargs; i++) {
    x = agnL_checkuint64(L, i, "binio.writeuint64");
    set_errno(0);  /* reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (write(hnd, &x, size) != (ssize_t)size) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with #%d: %s.", "binio.writeuint64", hnd, my_ioerror(en));
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int binio_readchar (lua_State *L) {
  off64_t pos;  /* 1.5.1 */
  int hnd, res, en;
  unsigned char data;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.readchar");  /* 2.37.5 */
  pos = agnL_optinteger(L, 2, 0);
  if (pos != 0 && lseek64(hnd, pos, SEEK_CUR) == -1) {
    lua_pushfail(L);
    return 1;
  }
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  res = read(hnd, &data, CHARSIZE);
  en = errno;
  if (res > 0)
    lua_pushnumber(L, data);
  else if (res == 0)  /* eof reached ? */
    lua_pushnil(L);
  else
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s or invalid position.", "binio.readchar", hnd, my_ioerror(en));
  return 1;
}


/* 0.25.0, 18.07.2009; reads n bytes from a file and returns a sequence of numbers, i.e. the bytes read;
   patched 20.07.2009, 0.25.1 */
static int binio_readbytes (lua_State *L) {
  int32_t n;
  int hnd, res, en, eof, nargs, eofalreadygiven, checkoptions;
  size_t ignorelen;
  unsigned char character;
  char *ignore;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1)
    luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.readbytes");  /* 2.37.5 */
  nargs = lua_gettop(L);
  /* check for options */
  ignore = NULL;
  ignorelen = 0;
  eof = 0;
  eofalreadygiven = 0;
  checkoptions = 2;  /* check two options */
  n = (agn_isinteger(L, 2)) ? agn_tointeger(L, 2) : agn_getbuffersize(L);
  if (n < 1) n = agn_getbuffersize(L);  /* 2.34.9 adaption */
  while (checkoptions-- && nargs != 0 && lua_ispair(L, nargs)) {  /* 2.28.6 */
    luaL_checkstack(L, 2, "not enough stack space");
    agn_pairgeti(L, nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("ignore", option)) {
        nargs--;
        xfree(ignore);  /* 5.3.1 fix */
        ignore = strdup(agn_checklstring(L, -1, &ignorelen));
        if (!ignore)
          luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "binio.readbytes");
      } else if (tools_streq("eof", option)) {
        nargs--; eofalreadygiven = 1;
        eof = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", "binio.readbytes", option);
      }
    }
    agn_poptoptwo(L);
  }
  if (lua_isboolean(L, 3) && !eofalreadygiven) {
    eof = agnL_optboolean(L, 3, 0);  /* 2.28.6 opti */
  }
  lua_settop(L, nargs);  /* remove options from argument stack */
  /* end of option check */
  unsigned char buffer[n];
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  res = read(hnd, buffer, n);
  en = errno;
  if (res > 0) {
    int i, j, c;
    c = 0;
    agn_createseq(L, res);
    for (i=0; i < res; i++) {
      character = buffer[i];  /* 2.6.1 extension */
      if (eof && character == 0) {  /* bail out if an embedded zero has been found ? */
        set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
        if (lseek64(hnd, i - res + 1, SEEK_CUR) == -1) {  /* reset file position to embedded zero */
          en = errno;
          xfree(ignore);
          luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.readbytes", hnd, my_ioerror(en));
        }
        break;  /* if an embedded zero has been found, quit */
      }
      if (ignorelen != 0) {
        int flag = 0;  /* 2.28.6 extension */
        for (j=0; j < ignorelen; j++) {
          if (ignore[j] == character) {
            flag = 1;
            break;
          }
        }
        if (flag) continue;  /* ignore read character */
      }
      agn_seqsetinumber(L, -1, ++c, character);
    }
  } else if (res == 0) {  /* end of file reached */
    lua_pushnil(L);
  } else {
    xfree(ignore);
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.readbytes", hnd, my_ioerror(en));
  }
  xfree(ignore);
  return 1;
}


static int binio_readstring (lua_State *L) {
  int hnd, en;
  ssize_t res;
  int32_t size;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.readstring");  /* 2.37.5 */
  res = read(hnd, &size, sizeof(int32_t));  /* first, read length of the string, patched 1.12.2, changed 2.11.5 */
#if BYTE_ORDER != BIG_ENDIAN
  tools_swapint32_t(&size);
#endif
  if (res == (ssize_t)sizeof(int32_t)) {
    /* Validation: Ensure size is not negative and fits in reasonable bounds, 7.3.0 fix */
    if (size < 0) {
      luaL_error(L, "Error in " LUA_QS ": invalid string length (%d) in file.", "binio.readstring", size);
    }
    char *data = agn_stralloc(L, size + 1, "binio.readstring", NULL);  /* 7.3.1 change */
    if (read(hnd, data, (size_t)size) == (ssize_t)size) {  /* now read string itself, 7.3.0 fix */
      data[size] = '\0';
      lua_pushlstring(L, data, size);
      xfree(data);  /* 7.3.3 fix */
      return 1;
    }
    xfree(data);
  } else if (res == 0) {
    lua_pushnil(L);
    return 1;
  }
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  en = errno;
  luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.readstring", hnd, my_ioerror(en));
  return 1;
}


static int binio_readshortstring (lua_State *L) {
  int hnd, en;
  unsigned char size;  /* 2.11.5 fix to prevent segfaults */
  ssize_t res;
  hnd = agn_tofileno(L, 1, 0);
    if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.readshortstring");  /* 2.37.5 */
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  res = read(hnd, &size, sizeof(unsigned char));  /* first, read length of the string, patched 1.12.2, changed 2.11.5 */
  if (res == (ssize_t)sizeof(unsigned char)) {
    /* Validation: Ensure size is not negative and fits in reasonable bounds, 7.3.3 fix */
    if (size < 0) {
      luaL_error(L, "Error in " LUA_QS ": invalid string length (%d) in file.", "binio.readstring", size);
    }
    char *data = agn_stralloc(L, size + 1, "binio.readstring", NULL);  /* 7.3.3 change */
    if (read(hnd, data, size) == (ssize_t)size) {  /* now read string itself */
      data[size] = '\0';
      lua_pushlstring(L, data, size);
      xfree(data);
      return 1;
    }
    xfree(data);
  } else if (res == 0) {
    lua_pushnil(L);
    return 1;
  }
  en = errno;
  luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.readshortstring", hnd, my_ioerror(en));
  return 1;
}


static int binio_readnumber (lua_State *L) {
  off64_t pos;  /* 2.11.5 */
  int hnd, en;
  ssize_t res;
  uint64_t data;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.readnumber");  /* 2.37.5 */
  pos = agnL_optinteger(L, 2, 0);  /* 2.11.5 */
  if (pos != 0 && lseek64(hnd, pos, SEEK_CUR) == -1) {
    lua_pushnil(L);
    return 1;
  }
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  res = read(hnd, &data, sizeof(uint64_t));
  en = errno;
  if (res == (ssize_t)sizeof(uint64_t)) {
#if BYTE_ORDER == BIG_ENDIAN  /* 2.16.6 code optimisation */
    lua_pushnumber(L, tools_unswapdouble(data));
#else
    double_cast x;  /* 2.16.3 */
    x.i = tools_unswapu64(data);  /* 2.17.1 change */
    lua_pushnumber(L, x.f);
#endif
  } else if (res == 0)  /* 2.11.5: eof reached ? */
    lua_pushnil(L);
  else
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s or invalid position.", "binio.readnumber", hnd, my_ioerror(en));
  return 1;
}


static int binio_readlongdouble (lua_State *L) {
  off64_t pos;
  int hnd, en;
  ssize_t res;
  long double data;
#if BYTE_ORDER == BIG_ENDIAN
    luaL_error(L, "Error in " LUA_QS ": Big Endian platforms are not supported.", "binio.readlongdouble");
#endif
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.readlongdouble");  /* 2.37.5 */
  pos = agnL_optinteger(L, 2, 0);
  if (pos != 0 && lseek64(hnd, pos, SEEK_CUR) == -1) {
    lua_pushnil(L);
    return 1;
  }
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  res = read(hnd, &data, sizeof(long double));
  en = errno;
  if (res == (ssize_t)sizeof(long double)) {
    createdlong(L, data);
  } else if (res == 0)  /* eof reached ? */
    lua_pushnil(L);
  else
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s or invalid position.", "binio.readlongdouble", hnd, my_ioerror(en));
  return 1;
}


static int binio_readint64 (lua_State *L) {  /* 7.3.3 */
  off64_t pos;
  int hnd, en;
  ssize_t res;
  int64_t data;
#if BYTE_ORDER == BIG_ENDIAN
    luaL_error(L, "Error in " LUA_QS ": Big Endian platforms are not supported.", "binio.readint64");
#endif
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.readint64");
  pos = agnL_optinteger(L, 2, 0);
  if (pos != 0 && lseek64(hnd, pos, SEEK_CUR) == -1) {
    lua_pushnil(L);
    return 1;
  }
  set_errno(0);  /* reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  res = read(hnd, &data, sizeof(int64_t));
  en = errno;
  if (res == (ssize_t)sizeof(int64_t)) {
    createint64(L, data);
  } else if (res == 0)  /* eof reached ? */
    lua_pushnil(L);
  else
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s or invalid position.", "binio.readint64", hnd, my_ioerror(en));
  return 1;
}


static int binio_readuint64 (lua_State *L) {  /* 7.3.3 */
  off64_t pos;
  int hnd, en;
  ssize_t res;
  uint64_t data;
#if BYTE_ORDER == BIG_ENDIAN
    luaL_error(L, "Error in " LUA_QS ": Big Endian platforms are not supported.", "binio.readuint64");
#endif
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.readuint64");
  pos = agnL_optinteger(L, 2, 0);
  if (pos != 0 && lseek64(hnd, pos, SEEK_CUR) == -1) {
    lua_pushnil(L);
    return 1;
  }
  set_errno(0);  /* reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  res = read(hnd, &data, sizeof(uint64_t));
  en = errno;
  if (res == (ssize_t)sizeof(uint64_t)) {
    createuint64(L, data);
  } else if (res == 0)  /* eof reached ? */
    lua_pushnil(L);
  else
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s or invalid position.", "binio.readuint64", hnd, my_ioerror(en));
  return 1;
}


static int binio_readlong (lua_State *L) {
  off64_t pos;  /* 2.11.5 */
  int hnd, en;
  ssize_t res;
  int32_t data;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.readlong");  /* 2.37.5 */
  pos = agnL_optinteger(L, 2, 0);  /* 2.11.5 */
  if (pos != 0 && lseek64(hnd, pos, SEEK_CUR) == -1) {
    lua_pushnil(L);
    return 1;
  }
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  res = read(hnd, &data, sizeof(int32_t));
  en = errno;
  if (res == (ssize_t)sizeof(int32_t)) {
#if BYTE_ORDER != BIG_ENDIAN
    tools_swapint32_t(&data);
#endif
    lua_pushnumber(L, data);
  } else if (res == 0)  /* 2.11.5: eof reached ? */
    lua_pushnil(L);
  else
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.readlong", hnd, my_ioerror(en));
  return 1;
}


/* Assumes that all values in the binary file are of the same type and reads the k-th one. By default, the
   function reads numbers (C doubles). You may pass the third argument type to determine another type. Valid
   types are the strings 'char' (see binio.writechar), 'long' (see binio.writelong), 'number' (the default,
   see binio.writenumber), 'shortstring' (see binio.writeshortstring) or 'string' (see binio.writestring).
   You may pass an optional offset from the beginning of the file as the fourth argument, which by default is
   0. If given, the file position is moved to the offset's +1 byte with regards to the file beginning before
   reading a value.

   See also: binio.readbytes, binio.readchar, binio.readlong, binio.readnumber, binio.readshortstring,
   binio.readstring, binio.writeindex. */
static int binio_readindex (lua_State *L) {  /* 2.11.5 */
  off64_t pos;
  int hnd, type, typesize, offset;
  ssize_t r, idx;  /* signed integers as read returns an ssize_t */
  static const char *const typenames[] = {"char", "long", "number", "shortstring", "string", NULL};
  static const int sizes[] = {sizeof(unsigned char), sizeof(int32_t), sizeof(lua_Number), 0, 0};
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.readindex");  /* 2.37.5 */
  idx = agn_checkposint(L, 2);
  if (idx < 1)  /* integer overflow */
    luaL_error(L, "Error in " LUA_QS " with file #%d: index overflow.", "binio.readindex", hnd);
  type = luaL_checkoption(L, 3, "number", typenames);
  typesize = sizes[type];
  offset = agnL_optposint(L, 4, 0);
  if (offset < 0)  /* integer overflow */
    luaL_error(L, "Error in " LUA_QS " with file #%d: offset overflow.", "binio.readindex", hnd);
  if (type < 3) {
    pos = offset + (idx - 1)*typesize;
    r = lseek64(hnd, pos, SEEK_SET);
    if (r == -1) {  /* from beginning of file */
      luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.readindex", hnd);
    }
  } else {
    lseek64(hnd, 0, SEEK_SET);  /* reset file position to its beginning */
  }
  switch (type) {
    case 0: {  /* char */
      unsigned char data;
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      r = read(hnd, &data, typesize);
      if (r == (ssize_t)typesize)
        lua_pushinteger(L, data);
      else if (r == 0)  /* in this context we have EOF, see GNU C lib docu */
        luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.readindex", hnd);
      else {
        int en = errno;
        luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.readindex", hnd, my_ioerror(en));
      }
      break;
    }
    case 1: {  /* long */
      int32_t data;
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      r = read(hnd, &data, typesize);
      if (r == (ssize_t)typesize) {
#if BYTE_ORDER != BIG_ENDIAN
        tools_swapint32_t(&data);
#endif
        lua_pushinteger(L, data);
      } else if (r == 0)  /* in this context we have EOF, see GNU C lib docu */
        luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.readindex", hnd);
      else {
        int en = errno;
        luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.readindex", hnd, my_ioerror(en));
      }
      break;
    }
    case 2: {  /* number */
      uint64_t data;
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      r = read(hnd, &data, typesize);
      if (r == (ssize_t)typesize) {
#if BYTE_ORDER == BIG_ENDIAN  /* 2.16.6 code optimisation */
        lua_pushnumber(L, tools_unswapdouble(data));
#else
        double_cast x;  /* 2.16.3 */
        x.i = tools_unswapu64(data);  /* 2.17.1 change */
        lua_pushnumber(L, x.f);
#endif
      } else if (r == 0)  /* in this context we have EOF, see GNU C lib docu */
        luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.readindex", hnd);
      else {
        int en = errno;
        luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.readindex", hnd, my_ioerror(en));
      }
      break;
    }
    case 3: {  /* shortstring */
      ssize_t i;
      size_t suchar;
      unsigned char size;
      suchar = sizeof(unsigned char);
      pos = 0;
      for (i=1; i < idx; i++) {  /* move file cursor forward */
        r = read(hnd, &size, suchar);
        if (r == (ssize_t)suchar) {
          pos += (size + suchar);
          r = lseek64(hnd, pos, SEEK_SET);
        } else
          luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.readindex", hnd);
      }
      /* now read entry */
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      r = read(hnd, &size, suchar);  /* read length of string, store into `size` */
      if (r == (ssize_t)suchar) {
        char data[size + 1];
        if (read(hnd, &data, size) == (ssize_t)size) {  /* string itself */
          data[size] = '\0';
          lua_pushlstring(L, data, size);
        } else
          luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.readindex", hnd);
      } else if (r == 0)  /* in this context we have EOF, see GNU C lib docu */
        luaL_error(L, "Error in " LUA_QS " with file #%d: invalid file position.", "binio.readindex", hnd);
      else {
        int en = errno;
        luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.readindex", hnd, my_ioerror(en));
      }
      break;
    }
    case 4: {  /* string */
      ssize_t i;
      int32_t size;
      size_t suchar;
      suchar = sizeof(int32_t);
      pos = 0;
      for (i=1; i < idx; i++) {  /* move file cursor forward */
        r = read(hnd, &size, suchar);
#if BYTE_ORDER != BIG_ENDIAN
        tools_swapint32_t(&size);
#endif
        if (r == (ssize_t)suchar) {
          pos += (size + suchar);
          r = lseek64(hnd, pos, SEEK_SET);
        } else
          luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.readindex", hnd);
      }
      /* now read entry */
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      r = read(hnd, &size, suchar);  /* length of string */
#if BYTE_ORDER != BIG_ENDIAN
      tools_swapint32_t(&size);
#endif
      if (r == (ssize_t)suchar) {
        /* Validation: Ensure size is not negative and fits in reasonable bounds */
        if (size < 0) {
          luaL_error(L, "Error in " LUA_QS ": invalid string length (%d) in file.", "binio.readindex", size);
        }
        char *data = agn_stralloc(L, size + 1, "binio.readindex", NULL);  /* 7.3.1 change */
        if (read(hnd, data, size) == (ssize_t)size) {  /* string itself, 7.3.0 fix */
          data[size] = '\0';
          lua_pushlstring(L, data, size);
        } else {
          xfree(data);
          luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.readindex", hnd);
        }
        xfree(data);
      } else if (r == 0)  /* in this context we have EOF, see GNU C lib docu */
        luaL_error(L, "Error in " LUA_QS " with file #%d: invalid file position.", "binio.readindex", hnd);
      else {
        int en = errno;
        luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.readindex", hnd, my_ioerror(en));
      }
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS " with file #%d: invalid type.", "binio.readindex", hnd);
  }
  return 1;
}


/* Assumes that all values in the binary file pointed to by filehandle are of the same type and writes the k-th one.

   The third third argument specifies the type to be written. Valid types are the strings 'char' (see binio.writechar),
   'long' (see binio.writelong), 'number' (the default, see binio.writenumber), 'shortstring' (see binio.writeshortstring)
   or 'string' (see binio.writestring).

   The fourth argument specifies the value to be written.

   You may pass an optional offset from the beginning of the file as the fifth argument, which by default is
   0. If given, the file position is moved to the offset's + 1 byte before writing a value. This feature allows for a
   self-defined file header.

   See also: binio.writebytes, binio.writechar, binio.writelong, binio.writenumber, binio.writeshortstring,
   binio.writestring, binio.readindex. */
static int binio_writeindex (lua_State *L) {  /* 2.11.5 */
  off64_t pos;
  int hnd, type, typesize, offset;
  ssize_t r, idx;  /* signed integers as read returns an ssize_t */
  static const char *const typenames[] = {"char", "long", "number", "shortstring", "string", NULL};
  static const int sizes[] = {sizeof(unsigned char), sizeof(int32_t), sizeof(lua_Number), 0, 0};
  hnd = agn_tofileno(L, 1, 0);
    if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.writeindex");  /* 2.37.5 */
  idx = agn_checkposint(L, 2);
  if (idx < 1)  /* integer overflow */
    luaL_error(L, "Error in " LUA_QS " with file #%d: index overflow.", "binio.writeindex", hnd);
  type = luaL_checkoption(L, 3, "number", typenames);
  typesize = sizes[type];
  /* value to be written is at index 4 */
  offset = agnL_optposint(L, 5, 0);
  if (offset < 0)  /* integer overflow */
    luaL_error(L, "Error in " LUA_QS " with file #%d: offset overflow.", "binio.writeindex", hnd);
  if (type < 3) {
    pos = offset + (idx - 1)*typesize;
    r = lseek64(hnd, pos, SEEK_SET);
    if (r == -1) {  /* from beginning of file */
      luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.writeindex", hnd);
    }
  } else {
    lseek64(hnd, 0, SEEK_SET);  /* reset file position to its beginning and later search for short/string index */
  }
  switch (type) {
    case 0: {  /* char */
      unsigned char data;
      data = agn_checkinteger(L, 4);
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      r = write(hnd, &data, typesize);
      if (r == (ssize_t)typesize)
        lua_pushtrue(L);
      else if (r == 0)  /* in this context we have EOF, see GNU C lib docu */
        luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.writeindex", hnd);
      else {
        int en = errno;
        luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.writeindex", hnd, my_ioerror(en));
      }
      break;
    }
    case 1: {  /* long */
      int32_t data;
      data = agn_checknumber(L, 4);
#if BYTE_ORDER != BIG_ENDIAN
      tools_swapint32_t(&data);
#endif
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      r = write(hnd, &data, typesize);
      if (r == (ssize_t)typesize) {
        lua_pushtrue(L);
      } else if (r == 0)  /* in this context we have EOF, see GNU C lib docu */
        luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.writeindex", hnd);
      else {
        int en = errno;
        luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.writeindex", hnd, my_ioerror(en));
      }
      break;
    }
    case 2: {  /* number */
      uint64_t data;
#if BYTE_ORDER == BIG_ENDIAN  /* 2.16.6 code optimisation */
      data = tools_swapdouble(agn_checknumber(L, 4));
#else
      double_cast x;  /* 2.16.3 */
      x.f = agn_checknumber(L, 4);
      data = tools_swapu64(x.i);  /* 2.17.1 change */
#endif
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      r = write(hnd, &data, typesize);
      if (r == (ssize_t)typesize) {
        lua_pushtrue(L);
      } else if (r == 0)  /* in this context we have EOF, see GNU C lib docu */
        luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.writeindex", hnd);
      else {
        int en = errno;
        luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.writeindex", hnd, my_ioerror(en));
      }
      break;
    }
    case 3: {  /* shortstring */
      ssize_t i;
      size_t suchar, l;
      unsigned char size;  /* 2.15.5a fix */
      const char *value;
      l = 0L;
      value = agn_checklstring(L, 4, &l);  /* 2.15.5a, fix for PowerPC Linux */
      if (l > 255)
        luaL_error(L, "Error in " LUA_QS " with writing data to file #%d: string too long", "binio.writeindex", hnd);
      suchar = sizeof(unsigned char);
      pos = 0ULL;
      for (i=1; i < idx; i++) {  /* move file cursor forward */
        r = read(hnd, &size, suchar);
        if (r == (ssize_t)suchar) {
          pos += (off64_t)size + (off64_t)suchar;
          r = lseek64(hnd, pos, SEEK_SET);
          if (r == -1)
            luaL_error(L, "Error in " LUA_QS " with file #%d: wrong internal file position.", "binio.writeindex", hnd);
        } else
          luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.writeindex", hnd);
      }
      /* now write entry */
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      my_writec(hnd, (unsigned char)l);
      r = write(hnd, value, l);
      if (r == (ssize_t)l)
        lua_pushtrue(L);
      else {
        int en = errno;
        luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.writeindex", hnd, my_ioerror(en));
      }
      break;
    }
    case 4: {  /* string */
      ssize_t i;
      int32_t size;
      size_t suchar, l;
      const char *value;
      suchar = sizeof(int32_t);
      pos = 0;
      value = agn_checklstring(L, 4, &l);
      if (l >= MAX_INT)
        luaL_error(L, "Error in " LUA_QS " with writing data to file #%d: string too long", "binio.writeindex", hnd);
      for (i=1; i < idx; i++) {  /* move file cursor forward */
        r = read(hnd, &size, suchar);
#if BYTE_ORDER != BIG_ENDIAN
        tools_swapint32_t(&size);
#endif
        if (r == (ssize_t)suchar) {
          pos += (size + suchar);
          r = lseek64(hnd, pos, SEEK_SET);
        } else
          luaL_error(L, "Error in " LUA_QS " with file #%d: invalid index or file position.", "binio.writeindex",
	  hnd);
      }
      /* now write entry */
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      my_writel(hnd, l);
      if (write(hnd, value, l) != (ssize_t)l) {
        int en = errno;
        luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.writeindex", hnd, my_ioerror(en));
      }
      lua_pushtrue(L);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS " with file #%d: invalid type.", "binio.writeindex", hnd);
  }
  return 1;
}


static int binio_toend (lua_State *L) {
  int hnd;
  off64_t size;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.toend");  /* 2.37.5 */
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  size = lseek64(hnd, 0L, SEEK_END);
  if (size == -1) {
    int en = errno;
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.toend", hnd, my_ioerror(en));
  } else
    lua_pushnumber(L, size);
  return 1;
}


static int binio_rewind (lua_State *L) {
  int hnd;
  off64_t size, pos;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.rewind");  /* 2.37.5 */
  pos = agnL_optinteger(L, 2, 0L);  /* 2.6.1 extension */
  if (pos < 0)
    luaL_error(L, "Error in " LUA_QS " with file #%d: position must be non-negative.", "binio.rewind");
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  size = lseek64(hnd, pos, SEEK_SET);
  if (size == -1) {
    int en = errno;
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.rewind", hnd, my_ioerror(en));
  } else
    lua_pushnumber(L, size);
  return 1;
}


static void binio_readobjecterror (lua_State *L, int hnd, int message) {  /* May 03, 2010 */
  /* close the file and delete file name from global binio.openfiles table */
  if (my_close(hnd) == 0) {
    if (agnL_gettablefield(L, "binio", "openfiles", "binio.readobject", 1) == LUA_TTABLE) {  /* 0.20.2 & 1.6.4, avoid segmentation faults */
      lua_pushinteger(L, hnd);
      lua_pushnil(L);
      lua_rawset(L, -3);
    } else
      luaL_error(L, "Error in " LUA_QS ": 'binio.openfiles' table not found.", "binio.readobject");  /* Agena 1.6.0 */
    agn_poptop(L);  /* delete "openfiles" */
  }
  if (message)
    luaL_error(L, "Error in " LUA_QS " with reading file #%d: I/O error.", "binio.readobject", hnd);
  else
    luaL_error(L, "Error in " LUA_QS ": wrong workspace file version.", "binio.readobject");
}


static void binio_readobjectutypename (lua_State *L, int hnd, int idx) {
  size_t size;
  ssize_t success;
  size = sec_readl(hnd, &success);  /* first read length of the string */
  if (success) {
    char data[size + 1];
    if (read(hnd, &data, size) == (ssize_t)size) {  /* now read string itself */
      data[size] = '\0';
      lua_pushlstring(L, data, size);
      agn_setutype(L, idx-1, -1);  /* set user-defined type to set */
      agn_poptop(L);            /* delete utype string */
      return;
    }  /* else issue an error */
  }
  binio_readobjecterror(L, hnd, 1);
}


static int binio_readobjectaux (lua_State *L, int hnd) {  /* May 03, 2010 */
  unsigned char data;
  int res;
  if (lua_checkstack(L, 2) == 0)  /* Agena 1.8.5 fix to prevent stack overflow = segmentation fault */
    luaL_error(L, "Error in " LUA_QS ": stack cannot be extended, structure too nested.", "binio.readobject");
  res = read(hnd, &data, CHARSIZE);
  if (res > 0) {
    switch (data - (data & 32)) {  /* get data type ID */
      case 0: {  /* read char */
        unsigned char num;
        if (sec_read(hnd, &num, sizeof(unsigned char)))
          lua_pushnumber(L, num);
        else
          binio_readobjecterror(L, hnd, 1);
        break;
      }
      case 1: {  /* read long */
        int32_t data;
        if (sec_read(hnd, &data, sizeof(int32_t))) {
#if BYTE_ORDER != BIG_ENDIAN
          tools_swapint32_t(&data);
#endif
          lua_pushnumber(L, data);
        } else
          binio_readobjecterror(L, hnd, 1);
        break;
      }
      case 2: {  /* read number */
        uint64_t num;
        if (sec_read(hnd, &num, sizeof(uint64_t))) {
#if BYTE_ORDER == BIG_ENDIAN
          lua_pushnumber(L, tools_unswapdouble(num));
#else
          double_cast x;  /* 2.16.3 */
          x.i = tools_unswapu64(num);  /* 2.17.1 change */
          lua_pushnumber(L, x.f);
#endif
        } else
          binio_readobjecterror(L, hnd, 1);
        break;
      }
      case 3: {  /* read string */
        size_t size;
        ssize_t success;
        size = sec_readl(hnd, &success);  /* first read length of the string */
        if (success) {
          char data[size + 1];
          if (read(hnd, &data, size) == (ssize_t)size) {  /* now read string itself */
            data[size] = '\0';
            lua_pushlstring(L, data, size);
          }
        } else
          binio_readobjecterror(L, hnd, 1);
        break;
      }
      case 4: {  /* read short string < 256 chars */
        unsigned char size;
        if (sec_read(hnd, &size, sizeof(unsigned char))) {
          char data[size + 1];
          if (read(hnd, &data, size) == size) {  /* now read string itself */
            data[size] = '\0';
            lua_pushlstring(L, data, size);
          }
        } else
          binio_readobjecterror(L, hnd, 1);
        break;
      }
      case 5: {  /* read table */
        lua_Number i, size;
        uint64_t num;
        if (sec_read(hnd, &num, sizeof(uint64_t))) {
#if BYTE_ORDER == BIG_ENDIAN
          size = tools_unswapdouble(num);
#else
          double_cast x;  /* 2.16.3 */
          x.i = tools_unswapu64(num);  /* 2.17.1 change */
          size = x.f;
#endif
          lua_newtable(L);
          if ((data & 32) == 32)  /* has a user-defined type been set ? */
            binio_readobjectutypename(L, hnd, -1);
          for (i=1; i <= size; i++) {
            binio_readobjectaux(L, hnd);
            binio_readobjectaux(L, hnd);
            lua_rawset(L, -3);
          }
        } else
          binio_readobjecterror(L, hnd, 1);
        break;
      }
      case 6: {  /* read set */
        lua_Number i, size;
        uint64_t num;
        if (sec_read(hnd, &num, sizeof(uint64_t))) {
#if BYTE_ORDER == BIG_ENDIAN
          size = tools_unswapdouble(num);
#else
          double_cast x;  /* 2.16.3 */
          x.i = tools_unswapu64(num);  /* 2.17.1 change */
          size = x.f;
#endif
          agn_createset(L, size);
          for (i=1; i <= size; i++) {
            binio_readobjectaux(L, hnd);
            lua_srawset(L, -2);
          }
        } else
          binio_readobjecterror(L, hnd, 1);
        break;
      }
      case 7: {  /* read sequence */
        lua_Number i, size;
        uint64_t num;
        if (sec_read(hnd, &num, sizeof(uint64_t))) {
#if BYTE_ORDER == BIG_ENDIAN
          size = tools_unswapdouble(num);
#else
          double_cast x;  /* 2.16.3 */
          x.i = tools_unswapu64(num);  /* 2.17.1 change */
          size = x.f;
#endif
          agn_createseq(L, size);
          if ((data & 32) == 32)  /* has a user-defined type been set ? */
            binio_readobjectutypename(L, hnd, -1);
          /* read contents of sequence */
          for (i=1; i <= size; i++) {
            binio_readobjectaux(L, hnd);
            lua_seqinsert(L, -2);
          }
        } else
          binio_readobjecterror(L, hnd, 1);
        break;
      }
      case 8: {  /* read pair */
        int typeset = ((data & 32) == 32);
        if (typeset) {  /* read user defined type */
          size_t size;
          ssize_t success;
          size = sec_readl(hnd, &success);  /* first read length of the string */
          if (success) {
            char data[size + 1];
            if (read(hnd, &data, size) == size) {  /* now read string itself */
              data[size] = '\0';
              lua_pushlstring(L, data, size);
            } else
              binio_readobjecterror(L, hnd, 1);
          } else
            binio_readobjecterror(L, hnd, 1);
        }
        /* read pair */
        binio_readobjectaux(L, hnd);
        binio_readobjectaux(L, hnd);
        agn_createpair(L, -2, -1);  /* assign it */
        lua_remove(L, -2);  /* remove left .. */
        lua_remove(L, -2);  /* .. and right hand-side */
        if (typeset) {  /* now assign user-defined type */
          agn_setutype(L, -1, -2);
          lua_remove(L, -2);  /* and remove utype string */
        }
        break;
      }
      case 9: {  /* read complex */
        lua_Number r, i;
        binio_readobjectaux(L, hnd);  /* read real part (long or double) */
        binio_readobjectaux(L, hnd);  /* read imaginary part (long or double) */
        r = agn_checknumber(L, -2);   /* Agena 1.4.3/1.5.0 */
        i = agn_checknumber(L, -1);   /* Agena 1.4.3/1.5.0 */
        agn_poptoptwo(L);
#ifndef PROPCMPLX
        agn_Complex z;
        __real__ z = r;
        __imag__ z = i;
        agn_createcomplex(L, z);
#else
        agn_createcomplex(L, r, i);
#endif
        break;
      }
      case 10: {  /* read boolean */
        unsigned char type;
        if (read(hnd, &type, CHARSIZE) < 1)
          binio_readobjecterror(L, hnd, 1);
        else {
          if (type == 2)
            lua_pushfail(L);
          else
            lua_pushboolean(L, type);
        }
        break;
      }
      case 11: {  /* read null */
        lua_pushnil(L);
        break;
      }
      case 12: {  /* read procedure, Agena 1.6.1 */
        size_t size;
        ssize_t success;
        size = sec_readl(hnd, &success);  /* first read length of the string */
        if (success) {
          char data[size + 1];
          if (read(hnd, &data, size) == (ssize_t)size) {  /* now read `strings.dump` string */
            data[size] = '\0';
            if (luaL_loadbuffer(L, data, size, data) != 0) {
              luaL_error(L, "Error in " LUA_QS ": %s.", "binio.readobject", agn_tostring(L, -1));
            }
          }
        } else
          binio_readobjecterror(L, hnd, 1);
        break;
      }
      case 13: {  /* read register, 2.3.0 RC 3 */
        lua_Number i, size;
        uint64_t num;
        if (sec_read(hnd, &num, sizeof(uint64_t))) {
#if BYTE_ORDER == BIG_ENDIAN
          size = tools_unswapdouble(num);
#else
          double_cast x;  /* 2.16.3 */
          x.i = tools_unswapu64(num);  /* 2.17.1 change */
          size = x.f;
#endif
          agn_createreg(L, size);
          /* read contents of register */
          for (i=1; i <= size; i++) {
            binio_readobjectaux(L, hnd);
            agn_regrawseti(L, -2, i);
          }
        } else
          binio_readobjecterror(L, hnd, 1);
        break;
      }
      default:
        luaL_error(L, "Error in " LUA_QS " with reading file #%d: unknown type.", "binio.readobject", hnd);
    }
  }
  else if (res == 0)
    lua_pushnil(L);
  else
    binio_readobjecterror(L, hnd, 1);
  return res;
}


#define FILESPEC "AGENAWORKSPACE"
#define FILESPECLEN 14
#define FILESPECVER 1
#define FILESPECSUB 0

static int binio_readobject (lua_State *L) {
  int hnd;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.readobject");  /* 2.37.5 */
  if (lseek64(hnd, 0L, SEEK_CUR) == -1)
    binio_readobjecterror(L, hnd, 1);
  else {
    char data[FILESPECLEN + 1];
    if (read(hnd, &data, FILESPECLEN) == FILESPECLEN) {
      unsigned char version, subversion;
      data[FILESPECLEN] = '\0';
      /* read file header */
      if (tools_strneq(data, FILESPEC))
        binio_readobjecterror(L, hnd, 0);
      /* read file version and subversion */
      if (sec_read(hnd, &version, sizeof(unsigned char)) && (version != FILESPECVER))
        binio_readobjecterror(L, hnd, 0);
      if (sec_read(hnd, &subversion, sizeof(unsigned char)) && (subversion != FILESPECSUB))  /* 2.16.6 change to prevent compiler warning */
        binio_readobjecterror(L, hnd, 0);
    } else
      binio_readobjecterror(L, hnd, 1);
    binio_readobjectaux(L, hnd);
  }
  /* for tables, sequences, registers, sets and functions, look for a metatable, read and assign; 4.10.4 */
  luaL_checkstack(L, 2, "not enough stack space");
  if (binio_readobjectaux(L, hnd) && (lua_istable(L, -1) && (lua_type(L, -2) >= LUA_TTABLE || lua_isfunction(L, -2))) ) {  /* anything read ? */
    lua_setmetatable(L, -2);
  } else {  /* pop whatever */
    agn_poptop(L);
  }
  /* with functions, look for a remember table, read and assign; 4.10.4 */
  /* first read rtable mode, a string */
  if (binio_readobjectaux(L, hnd) && lua_isstring(L, -1) && lua_isfunction(L, -2) ) {  /* rtable mode read ? */
    const char *mode = agn_tostring(L, -1);
    if (tools_streq(mode, "none")) {
      /* do nothing */
    } else if (tools_streq(mode, "rotable")) {
      agn_creatertable(L, -2, 0);  /* 0 = RETURN statement cannot update the rtable */
    } else if (tools_streq(mode, "rtable"))  {
      agn_creatertable(L, -2, 1);  /* 1 = RETURN statement can update the rtable */
    }
  }
  agn_poptop(L);  /* pop whatever */
  /* now read remember table and assign */
  if (binio_readobjectaux(L, hnd) && lua_istable(L, -1) && lua_isfunction(L, -2)) {  /* anything read ? */
    lua_pushnil(L);
    while (lua_next(L, -2)) {
      agn_setrtable(L, -4, -2, -1);
      agn_poptop(L);
    }
  }
  agn_poptop(L);  /* pop table or whatever */
  /* finally read store */
  if (binio_readobjectaux(L, hnd) && lua_istable(L, -1) && lua_isfunction(L, -2)) {  /* anything read ? */
    agn_setstorage(L, -2);  /* pops table */
  } else {
    agn_poptop(L);
  }
  return 1;
}


static int binio_lock (lua_State *L) {  /* 0.32.5 */
  int hnd;
  size_t nargs;
  off64_t start, size;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.lock");  /* 2.37.5 */
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1) {  /* lock entire file (in Windows lock 2^63 bytes only) */
    start = 0;
    size = 0;
  } else {
    /* lock from current file position */
    start = my_fpos(hnd);
    size = agnL_checknumber(L, 2);
    if (size < 0) luaL_error(L, "Error in " LUA_QS ": must lock at least zero or more bytes.", "binio.lock");  /* 1.12.2 */
  }
  lua_pushboolean(L, my_lock(hnd, start, size) == 0);
  return 1;
}


static int binio_unlock (lua_State *L) {  /* 0.32.5 */
  int hnd;
  size_t nargs;
  off64_t start, size;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.unlock");  /* 2.37.5 */
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1) {  /* lock entire file */
    start = 0;
    size = 0;
  } else {
    /* lock from current file position */
    start = my_fpos(hnd);
    size = agnL_checknumber(L, 2);
    if (size < 0) luaL_error(L, "Error in " LUA_QS ": must unlock at least one byte.", "binio.unlock");
  }
  lua_pushboolean(L, my_unlock(hnd, start, size) == 0);
  return 1;
}


/* 2.2.0, 4.3.0 fix */
static int binio_eof (lua_State *L) {
  FILE **p = biniotopnfile(L, 1);
  lua_pushboolean(L, tools_eof(*p));  /* returns fail if result == -1 */
  return 1;
}


int lines (lua_State *L) {
  int hnd, res, en, nts, flag;
  off64_t length, pos, bufferlen, i;
  unsigned char c;
  luaL_Buffer b;
  i = 0;
  hnd = lua_tointeger(L, lua_upvalueindex(1));
  pos = lua_tointeger(L, lua_upvalueindex(2));
  length = lua_tointeger(L, lua_upvalueindex(3));
  nts = lua_tointeger(L, lua_upvalueindex(4));  /* convert embedded zeros with white spaces */
  bufferlen = agn_getbuffersize(L);
  if (pos > length || pos == MAX_INT) {
    lua_pushnil(L);
    return 1;
  }
  luaL_buffinit(L, &b);
  if (bufferlen < 1) bufferlen = LUAL_BUFFERSIZE;
  unsigned char buffer[bufferlen];
  flag = 0;
  do {
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    res = read(hnd, buffer, bufferlen);
    en = errno;
    if (res > 0) {
      for (i=0; i < res; i++) {
        if (pos > length) { flag = 1; pos = MAX_INT; break; }
        c = buffer[i]; pos++;
        switch (c) {
          case '\0':   /* embedded zero ? */
            if (nts) {  /* convert zeros with spaces ? */
              luaL_addchar(&b, ' ');  /* and continue with next character in file */
              continue;
            }
            pos = MAX_INT;
            flag = 1;
            break;     /* bail out and return assembled line */
          case '\r':   /* carriage return ? */
            continue;  /* ignore it and continue */
          case '\n':
            flag = 1;
            break;     /* return line w/o newline */
          default:
            luaL_addchar(&b, c);  /* add to buffer and continue with next character in file */
            continue;
        }
        if (flag) break;
      }  /* end of for i */
      if (flag) break;
    } else if (res == 0)  /* eof reached ? */
      break;
    else {
      luaL_clearbuffer(&b);  /* 2.14.9 */
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.lines", hnd, my_ioerror(en));
    }
  } while (1);  /* end of do */
  lua_pushinteger(L, pos); lua_replace(L, lua_upvalueindex(2));
  luaL_pushresult(&b);  /* close buffer and return latest line */
  if (res == 0) {  /* eof ? */
    lua_pushnil(L);
    lua_replace(L, -2);   /* replace buffer with `null` */
  } else {
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (lseek64(hnd, i - res + 1, SEEK_CUR) == -1) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "binio.lines", hnd, my_ioerror(en));
    }
  }
  return 1;
}

/* Returns an iterator function that beginning from the current file position, with each call returns a new line
   from the file pointed to by the handle fd. By default, the function traverses the file up to its end. If the
   second argument n is a positive integer, it reads the next n characters from the current file position (default
   is `infinity` = end of file). The function generally ignores carriage returns (ASCII code 13) and does not
   return newlines.

   If the last argument is the Boolean value true, all embedded zeros are replaced with white spaces and the
   traversal of the file continues instead of being finished. By default, zeros are not ignored, so if one
   is encountered, the traversal stops.

   The iterator function returns a string, and null if the end of the file has been reached. It also returns
   null if the last argument is not true and an embedded zero has been found in the file. The iterator function
   does not close the file at the end of traversal, use `binio.close` to accomplish this. */
static int binio_lines (lua_State *L) {  /* 2.6.1 */
  off64_t length;
  int hnd, nts, nargs;
  nargs = lua_gettop(L);
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "binio.lines");  /* 2.37.5 */
  length = (agn_isnumber(L, 2)) ? lua_tointeger(L, 2) : MAX_INT;  /* default: go to the end of the file, see remark below on MAX_INT */
  if (length < 1)
    luaL_error(L, "Error in " LUA_QS " with file #%d: length must be positive.", "binio.lines", hnd);
  nts = lua_istrue(L, nargs);  /* 2.36.2 optimisation */
  luaL_checkstack(L, 4, "not enough stack space");  /* 3.18.4 fix */
  lua_pushinteger(L, hnd);  /* file descriptor */
  lua_pushinteger(L, 0);  /* pos */
  lua_pushinteger(L, length);  /* in Linux, no more than MAX_INT can be pushed'integer */
  lua_pushinteger(L, nts);
  lua_pushcclosure(L, &lines, 4);
  return 1;
}


/* Truncates an open file denoted by its handle fh at the current file position. The function returns true on
   success and issues an error otherwise. You may query and change the current file position by calling io.seek. */
static int binio_truncate (lua_State *L) {  /* 6.1.2 */
  int en;
  int64_t pos, eof;
  FILE **p = biniotopnfile(L, 1);
  pos = _ftelli64(*p);
  if (pos == -1)  /* there is no errno for ftell */
    luaL_error(L, "Error in " LUA_QS ": could not determine current file position.", "binio.truncate");
  if (_fseeki64(*p, 0, SEEK_END) != 0)
    luaL_error(L, "Error in " LUA_QS ": could not determine length of file.", "binio.truncate");
  eof = _ftelli64(*p);
  if (eof == -1)  /* there is no errno for ftell */
    luaL_error(L, "Error in " LUA_QS ": could not determine length of file.", "binio.truncate");
  if (eof == pos)
    lua_pushfalse(L);
  else {
    set_errno(0);  /* Windows 2000 seems susceptible to uncleared errno's */
    if (ftruncate(fileno(*p), pos) != 0) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS ": could not truncate file: %s.", "binio.truncate", my_ioerror(en));
    }
    else {
      if (_fseeki64(*p, pos, SEEK_SET) != 0)  /* reset file position */
        luaL_error(L, "Error in " LUA_QS ": could not reset file position.", "binio.truncate");
      lua_pushtrue(L);
    }
  }
  return 1;
}


/* Checks the error indicator for the file denoted by file handle `fh` and returns `true` if set or `false` if not set. */
static int binio_ferror (lua_State *L) {  /* 6.1.2 */
  FILE **p = biniotopnfile(L, 1);
  lua_pushboolean(L, ferror(*p) != 0);
  return 1;
}


/* Clears the end-of-file and error indicators for the file denoted by file handle `fh`. The function returns nothing. */
static int binio_clearerror (lua_State *L) {  /* 6.1.2 */
  FILE **p = biniotopnfile(L, 1);
  clearerr(*p);
  return 0;
}


/* Obtains the file position indicator of the file denoted by filehandle and returns an `fpos_t' userdata. The value is
   only meaningful as the input to `binio.fsetpos`.

   In case of an error, the function returns `null` plus the error message.

   See also: binio.filepos, binio.seek. 6.1.2 */

#define FPOS_T_UD   "fpos_t"
static int binio_fgetpos (lua_State *L) {
  int rc, en;
  fpos_t pos;
  FILE **p = biniotopnfile(L, 1);
  clearerr(*p);
  rc = fgetpos(*p, &pos);
  if (rc == 0) {
    fpos_t *posud = (fpos_t*)lua_newuserdata(L, sizeof(fpos_t));
    *posud = pos;  /* Copy the data from fpos_t to the userdata */
    lua_setmetatabletoobject(L, -1, FPOS_T_UD, 1);
  } else {
    en = errno;
    lua_pushnil(L);
    lua_pushstring(L, my_ioerror(en));
  }
  return 1 + (rc != 0);
}


/* Sets the file position indicator and the multibyte parsing state (if any) of the file denoted by filehandle
   according to the `fpos_t' userdata value obtained by `binio.fgetpos`.

   Besides establishing new parse state and position, a call to this function clears the end-of-file state, if
   it is set. 6.1.2 */
static int binio_fsetpos (lua_State *L) {
  int rc, en;
  FILE **p = biniotopnfile(L, 1);
  fpos_t *pos = luaL_checkudata(L, 2, FPOS_T_UD);
  clearerr(*p);
  rc = fsetpos(*p, pos);
  lua_pushboolean(L, rc == 0);
  if (rc != 0) {
    en = errno;
    lua_pushstring(L, my_ioerror(en));
  }
  return 1 + (rc != 0);
}


/* ----------------------------  Metamethods  ------------------------------- */

static int binio_tostring (lua_State *L) {  /* 0.24.2 */
  FILE *f = *topfile(L);
  if (f == NULL)
    lua_pushfail(L);
  else {  /* 2.15.5 fix: just returning an integer is quite deceiving as binio file handles are userdata */
    lua_pushstring(L, AGN_FILEHANDLE);
    lua_pushinteger(L, fileno(f));
    lua_concat(L, 2);
  }
  return 1;
}


static int binio_gc (lua_State *L) {
  FILE *f = *topfile(L);
  /* ignore closed files */
  if (f != NULL) fclose(f);
  return 0;
}


static int fpos_tostring (lua_State *L) {  /* at the console, the file object is formatted as follows: */
  if (luaL_isudata(L, 1, FPOS_T_UD))
    lua_pushfstring(L, FPOS_T_UD "(%p)", lua_topointer(L, 1));
  else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}

static int fpos_gc (lua_State *L) {
  luaL_checkudata(L, 1, FPOS_T_UD);
  lua_setmetatabletoobject(L, 1, NULL, 1);
  return 0;
}

static const struct luaL_Reg fpos_mt [] = {
  {"__gc",             fpos_gc},
  {"__tostring",       fpos_tostring},
  {NULL, NULL}
};


static const luaL_Reg biniolib[] = {
  {"clearerror",       binio_clearerror},
  {"close",            binio_close},
  {"eof",              binio_eof},
  {"ferror",           binio_ferror},
  {"fgetpos",          binio_fgetpos},  /* Ocotber 05, 2025 */
  {"filepos",          binio_filepos},
  {"fsetpos",          binio_fsetpos},  /* Ocotber 05, 2025 */
  {"isfdesc",          binio_isfdesc},
  {"length",           binio_length},
  {"lock",             binio_lock},
  {"readbytes",        binio_readbytes},
  {"readchar",         binio_readchar},
  {"readindex",        binio_readindex},
  {"readlong",         binio_readlong},
  {"readlongdouble",   binio_readlongdouble},
  {"readnumber",       binio_readnumber},
  {"readshortstring",  binio_readshortstring},
  {"readstring",       binio_readstring},
  {"rewind",           binio_rewind},
  {"seek",             binio_seek},
  {"sync",             binio_sync},
  {"toend",            binio_toend},
  {"truncate",         binio_truncate},
  {"unlock",           binio_unlock},
  {"writebytes",       binio_writebytes},
  {"writechar",        binio_writechar},
  {"writeindex",       binio_writeindex},
  {"writelong",        binio_writelong},
  {"writelongdouble",  binio_writelongdouble},
  {"writenumber",      binio_writenumber},
  {"writeshortstring", binio_writeshortstring},
  {"writestring",      binio_writestring},
  {"__gc",             binio_gc},
  {"__tostring",       binio_tostring},
  {NULL, NULL}
};


static const luaL_Reg binio[] = {
  {"clearerror",       binio_clearerror},
  {"close",            binio_close},             /* July 13, 2008 */
  {"eof",              binio_eof},               /* June 14, 2014 */
  {"ferror",           binio_ferror},
  {"fgetpos",          binio_fgetpos},           /* October 05, 2025 */
  {"filepos",          binio_filepos},           /* July 13, 2008 */
  {"fsetpos",          binio_fsetpos},           /* October 05, 2025 */
  {"isfdesc",          binio_isfdesc},           /* May 17, 2015 */
  {"length",           binio_length},            /* July 13, 2008 */
  {"lines",            binio_lines},             /* May 17, 2015 */
  {"lock",             binio_lock},              /* June 12, 2010 */
  {"open",             binio_open},              /* July 13, 2008 */
  {"readbytes",        binio_readbytes},         /* July 18, 2009 */
  {"readchar",         binio_readchar},          /* July 13, 2008 */
  {"readindex",        binio_readindex},         /* January 05, 2018 */
  {"readint64",        binio_readint64},         /* March 28, 2026 */
  {"readlong",         binio_readlong},          /* October 26, 2008 */
  {"readlongdouble",   binio_readlongdouble},    /* December 29, 2022 */
  {"readnumber",       binio_readnumber},        /* October 25, 2008 */
  {"readobject",       binio_readobject},        /* May 02, 2010 */
  {"readshortstring",  binio_readshortstring},   /* May 08, 2010 */
  {"readstring",       binio_readstring},        /* July 20, 2008 */
  {"readuint64",       binio_readuint64},        /* March 28, 2026 */
  {"rewind",           binio_rewind},            /* July 20, 2008 */
  {"seek",             binio_seek},              /* July 13, 2008 */
  {"sync",             binio_sync},              /* July 13, 2008 */
  {"toend",            binio_toend},             /* July 20, 2008 */
  {"truncate",         binio_truncate},          /* October 05, 2025 */
  {"unlock",           binio_unlock},            /* June 12, 2010 */
  {"writebytes",       binio_writebytes},        /* July 19, 2009 */
  {"writechar",        binio_writechar},         /* July 19, 2008 */
  {"writeindex",       binio_writeindex},        /* January 22, 2018 */
  {"writeint64",       binio_writeint64},        /* March 28, 2026 */
  {"writelong",        binio_writelong},         /* October 26, 2008 */
  {"writelongdouble",  binio_writelongdouble},   /* December 29, 2022 */
  {"writenumber",      binio_writenumber},       /* October 25, 2008 */
  {"writeshortstring", binio_writeshortstring},  /* May 08, 2010 */
  {"writestring",      binio_writestring},       /* July 20, 2008 */
  {"writeuint64",      binio_writeuint64},       /* March 28, 2026 */
  {NULL, NULL}
};


static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGN_FILEHANDLE);  /* create metatable for file handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, biniolib);  /* file methods */
}

/*
** Open binio library
*/
LUALIB_API int luaopen_binio (lua_State *L) {
  createmeta(L);
  luaL_register(L, AGENA_BINIOLIBNAME, binio);
  lua_newtable(L);
  lua_setfield(L, -2, "openfiles");  /* table for information on all open files */
  luaL_newmetatable(L, FPOS_T_UD);
  luaL_register(L, NULL, fpos_mt);
  return 1;
}

