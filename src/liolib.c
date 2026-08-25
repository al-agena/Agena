/*
** $Id: liolib.c,v 2.73 2006/05/08 20:14:16 roberto Exp $
** Standard I/O (and system) library
** See Copyright Notice in agena.h
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* for ftruncate */
#include <fcntl.h>

#if defined(__linux__) || defined(__SOLARIS) || defined(__APPLE__)
#include <sys/time.h>      /* for g/setrlimit */
#include <sys/resource.h>  /* ditto */
#endif

#if defined(_WIN32) || defined(__OS2__) || defined(__DJGPP)
#include <conio.h>   /* getch; a UNIX version is included in agnhlps.c */
#endif

#if defined(_WIN32)   /* 2.30 RC 2 eCS adaption */
#include <windows.h>  /* for io_putclip */
#include <tchar.h>    /* for GetFileNameFromHandle */
#include <psapi.h>    /* ditto */
#include <io.h>       /* isatty */
#endif

#if defined(__OS2__)
#include <os2.h>
#endif

#if defined(__DJGPP__)
#include <io.h>   /* for _flush_disk_cache  */
#endif


#define liolib_c
#define LUA_LIB

#include "agena.h"

#include "agnxlib.h"
#include "agenalib.h"

#include "agnhlps.h"  /* for getch() */
#include "agnconf.h"  /* for uchar */
#include "agncmpt.h"  /* for *fseek*, *ftell* defines, 2.3.0 RC 2 */
#include "lstrlib.h"  /* for SPECIALS */
#include "intvec.h"

#define IO_INPUT   1
#define IO_OUTPUT  2

static const char *const fnames[] = {"input", "output"};

/* mapping table to correctly read in diacritics from text files in Windows */

static unsigned char f2c[256] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
  10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
  20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
  30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
  50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
  60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
  70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
  80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
  90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
  100, 101, 102, 103, 104, 105, 106, 107, 108, 109,  /* 100 .. 109 */
  110, 111, 112, 113, 114, 115, 116, 117, 118, 119,  /* 110 .. 119 */
  120, 121, 122, 123, 124, 125, 126, 127, 128, 129,  /* 120 .. 129 */
  130, 131, 132, 133, 134, 135, 136, 137, 138, 139,  /* 130 .. 139 */
  153, 141, 142, 143, 144, 145, 146, 147, 148, 149,  /* 140 .. 149 */
  150, 151, 152, 153, 154, 155, 148, 157, 158, 159,  /* 150 .. 159 */
  160, 161, 162, 163, 164, 165, 166, 245, 168, 169,  /* 160 .. 169 */
  170, 171, 172, 173, 174, 175, 176, 177, 178, 179,  /* 170 .. 179 */
  180, 181, 182, 183, 184, 185, 186, 187, 188, 189,  /* 180 .. 189 */
  190, 168, 183, 181, 182, 199, 142, 143, 146, 128,  /* 190 .. 199 */
  212, 144, 210, 211, 222, 214, 215, 216, 209, 165,  /* 200 .. 209 */
  227, 224, 226, 229, 153, 215, 157, 235, 233, 234,  /* 210 .. 219 */
  154, 237, 232, 225, 133, 160, 131, 198, 132, 134,  /* 220 .. 229 */
  145, 135, 138, 130, 136, 137, 141, 161, 140, 139,  /* 230 .. 239 */
  208, 164, 149, 162, 147, 228, 148, 247, 155, 151,  /* 240 .. 249 */
  163, 150, 129, 236, 231, 152                       /* 250 .. 255 */
};


static int pushresult (lua_State *L, int i, const char *filename) {
  int en = errno;  /* calls to Lua API may change this value */
  if (i) {
    lua_pushboolean(L, 1);
    return 1;
  }
  else {
    luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
    lua_pushnil(L);
    if (filename)
      lua_pushfstring(L, "%s: %s", filename, my_ioerror(en));  /* changed 2.10.4 */
    else
      lua_pushfstring(L, "%s", my_ioerror(en));  /* changed 2.10.4 */
    lua_pushinteger(L, en);
    return 3;
  }
}


static int issueioerror (lua_State *L, const char *filename, const char *fn) {
  int en = errno;  /* calls to Lua API may change this value */
  if (filename)
    luaL_error(L, "Error in " LUA_QS ": '%s': %s.", fn, filename, my_ioerror(en));  /* changed 2.10.4 */
  else
    luaL_error(L, "Error in " LUA_QS ": %s.", fn, my_ioerror(en));  /* changed 2.10.4 */
  return 2;
}


static void fileerror (lua_State *L, int arg, const char *filename) {
  lua_pushfstring(L, "%s: %s", filename, my_ioerror(errno));  /* changed 2.10.4 */
  luaL_argerror(L, arg, lua_tostring(L, -1));
}


static FILE *tofile (lua_State *L) {
  FILE **f = iotopfile(L);
  if (*f == NULL)
    luaL_error(L, "Error in " LUA_QS " package: attempt to use a closed file.", "io");
  return *f;
}


/*
** When creating file handles, always creates a `closed' file handle
** before opening the actual file; so, if there is a memory error, the
** file is not left opened.
*/
static FILE **newfile (lua_State *L) {
  FILE **pf = (FILE **)lua_newuserdata(L, sizeof(FILE *));
  *pf = NULL;  /* file handle is currently `closed' */
  luaL_getmetatable(L, LUA_FILEHANDLE);
  lua_setmetatable(L, -2);
  return pf;
}


static FILE* io_aux_checkfile (lua_State *L, const char *procname, int checklength) {
  FILE *f = tofile(L);
  if (f == NULL) { /* but a closed file ? */
    if (procname)
      luaL_error(L, "Error in " LUA_QS ", attempt to use a closed file.", procname);
    else
      luaL_error(L, "Error, attempt to use a closed file.");
  }
  if (checklength &&
      !(f == stdin || f == stdout || f == stderr) &&  /* 5.6.1 fix */
      (_fseeki64(f, 0, SEEK_CUR) != 0 || _ftelli64(f) == -1))  /* 2.2.1 fix, 2.3.0 RC 2 extension */
    luaL_error(L, "Error in " LUA_QS ": could not determine length.", procname);
  return f;
}


/*
** this function has a separated environment, which defines the
** correct __close for 'popen' files
*/
static int io_pclose (lua_State *L) {
  FILE **p = iotopfile(L);
  int ok = lua_pclose(L, *p);
  *p = NULL;
  errno = 0;
  return luaL_execresult(L, ok);  /* 2.12.12, taken from Lua 5.4.0 */
}


static int io_fclose (lua_State *L) {  /* 1.0.2, extended Agena 1.1.0, 1.6.0 */
  int i, nargs;
  nargs = lua_gettop(L) - 2;
  for (i=0; i < nargs; i++) {
    FILE **p = iotopnfile(L, i + 1);
    int ok = (fclose(*p) == 0);
    if (ok) {
      /* 0.20.2, avoid segmentation faults; 1.6.4, it is not possible to issue an error, obviously io has its own stack ??? ! */
      if (agnL_gettablefield(L, "io", "openfiles", NULL, 0) == LUA_TTABLE) {
        /* DO NOT use lua_istable since it checks a value on the stack and not the return value of agn_gettablefield (a nonneg int) */
        lua_pushfstring(L, "file(%p)", *p);
        lua_pushnil(L);
        lua_rawset(L, -3);
      }
      agn_poptop(L);  /* delete "openfiles" */
    }
    *p = NULL;
    pushresult(L, ok, NULL);
  }
  return nargs;
}


/* Checks the error indicator for the file denoted by file handle `fh` and returns `true` if set or `false` if not set. */
static int io_ferror (lua_State *L) {  /* 2.22.0 */
  FILE *f = io_aux_checkfile(L, "io.ferror", 0);
  lua_pushboolean(L, ferror(f) != 0);
  return 1;
}


/* Clears the end-of-file and error indicators for the file denoted by file handle `fh`. The function returns nothing. */
static int io_clearerror (lua_State *L) {  /* 2.22.0 */
  FILE *f = io_aux_checkfile(L, "io.clearerror", 0);
  clearerr(f);
  return 0;
}


static int aux_close (lua_State *L) {
  lua_getfenv(L, 1);
  lua_getfield(L, -1, "__close");
  return (lua_tocfunction(L, -1))(L);
}


static int io_close (lua_State *L) {
  if (lua_isnone(L, 1))
    lua_rawgeti(L, LUA_ENVIRONINDEX, IO_OUTPUT);
  tofile(L);  /* make sure argument is a file */
  return aux_close(L);
}


static int io_gc (lua_State *L) {  /* 0.24.2 */
  FILE *f = *iotopfile(L);
  /* ignore closed files */
  if (f != NULL)
    aux_close(L);
  return 0;
}


static int io_tostring (lua_State *L) {  /* 0.24.2 */
  FILE *f = *iotopfile(L);
  if (f == NULL)
    lua_pushliteral(L, "file(closed)");
  else
    lua_pushfstring(L, "file(%p)", f);
  return 1;
}


static int io_open (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  agn_checkvalidpath(L, filename, 0, "io.open");  /* 7.6.3 */
  /* 2.22.1 change, 2.36.1 patch for missing `+` and `b` options */
  static const char *const mode[] =
    {"r", "w", "a", "r",    "w",     "a",      "r+", "r+", "w+", "a+", "r+",    "w+",     "a+",      "rb", "wb", "ab", "r+b", "w+b", "a+b", NULL};
  static const char *const modenames[] =
    {"r", "w", "a", "read", "write", "append", "r+", "rw", "w+", "a+", "read+", "write+", "append+", "rb", "wb", "ab", "r+b", "w+b", "a+b", NULL};
  int op = luaL_checkoption(L, 2, "r", modenames);
  FILE **pf = newfile(L);
  *pf = fopen64(filename, mode[op]);  /* 2.2.1 */
  if (*pf == NULL) issueioerror(L, filename, "io.open");
  /* enter new open file to global io.openfiles table
     0.20.2, avoid segmentation faults; 1.6.4, it is not possible to issue an error, obviously io has its own stack ??? ! */
  if (agnL_gettablefield(L, "io", "openfiles", NULL, 0) == LUA_TTABLE) {
    /* DO NOT use lua_istable since it checks a value on the stack and not the return value of agn_gettablefield (a nonneg int) */
    lua_pushfstring(L, "file(%p)", *pf);
    lua_createtable(L, 2, 0);
    lua_pushstring(L, filename);
    lua_rawseti(L, -2, 1);
    lua_pushstring(L, mode[op]);
    lua_rawseti(L, -2, 2);
    lua_rawset(L, -3);
  }
  agn_poptop(L);  /* delete "openfiles" */
  return 1;
}


static int io_popen (lua_State *L) {  /* changed 2.22.0 */
  const char *filename = luaL_checkstring(L, 1);
  static const char *const mode[] =      {"r", "w", "r",    "w",     NULL};  /* 2.22.1 change */
  static const char *const modenames[] = {"r", "w", "read", "write", NULL};
  int op = luaL_checkoption(L, 2, "r", modenames);
  FILE **pf = newfile(L);
  *pf = lua_popen(L, filename, mode[op]);
  return (*pf == NULL) ? luaL_fileresult(L, 0, filename) : 1;  /* taken from Lua 5.4.0 */
}


static int io_tmpfile (lua_State *L) {
  FILE **pf = newfile(L);
  *pf = tmpfile();
  return (*pf == NULL) ? pushresult(L, 0, NULL) : 1;
}


static FILE *getiofile (lua_State *L, int findex) {
  FILE *f;
  lua_rawgeti(L, LUA_ENVIRONINDEX, findex);
  f = *(FILE **)lua_touserdata(L, -1);
  if (f == NULL)
    luaL_error(L, "Error in " LUA_QS " package: standard %s file is closed.", "io", fnames[findex - 1]);
  return f;
}


static int g_iofile (lua_State *L, int f, const char *mode) {
  if (!lua_isnoneornil(L, 1)) {
    const char *filename = lua_tostring(L, 1);
    if (filename) {
      FILE **pf = newfile(L);
      *pf = fopen64(filename, mode);
      if (*pf == NULL)
        fileerror(L, 1, filename);
    }
    else {
      tofile(L);  /* check that it's a valid file handle */
      lua_pushvalue(L, 1);
    }
    lua_rawseti(L, LUA_ENVIRONINDEX, f);
  }
  /* return current value */
  lua_rawgeti(L, LUA_ENVIRONINDEX, f);
  return 1;
}


static int io_input (lua_State *L) {
  return g_iofile(L, IO_INPUT, "r");
}


static int io_output (lua_State *L) {
  return g_iofile(L, IO_OUTPUT, "w");
}


static int io_auxreadline (lua_State *L);
static int io_auxreadcsvline (lua_State *L);

static void aux_lines (lua_State *L, int idx, int toclose) {
  luaL_checkstack(L, 3, "not enough stack space");  /* 3.18.4/4.3.0 fix */
  lua_pushvalue(L, idx);  /* push FILE userdata object */
  lua_pushboolean(L, toclose);  /* close/not close file when finished */
  lua_pushcclosure(L, io_auxreadline, 2);
}


static int vecint_gc (lua_State *L) {  /* 3.10.3 */
  if (luaL_isudata(L, 1, "io_vecint")) {
    intvec *v = lua_touserdata(L, 1);
    lua_setmetatabletoobject(L, 1, NULL, 1);
    _intvec_free(v);
  }
  return 0;
}

static const struct luaL_Reg vecint_arraylib [] = {  /* 3.10.3 */
  {"__gc", vecint_gc},
  {NULL, NULL}
};

static void aux_csvlines (lua_State *L, int idx, int toclose) {  /* 3.10.3 */
  intvec *v;
  char *unwrap, *sep;
  int isseq, tonumber, match, nargs, bailout, init, header, ignorefnidx, skipfaultylines, noindex;
  size_t i, n, ls;
  lua_Integer index;
  unwrap = sep = NULL;
  init = n = tonumber = noindex = skipfaultylines = 0;  /* position of token in string */
  header = ignorefnidx = 0;
  nargs = lua_gettop(L) - 1;
  sep = tools_strdup(";");
  if (!sep) luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
  aux_splitcheckoptions(L, idx, &nargs, 2, &init, &tonumber, &match, &bailout, &unwrap, &sep,
    &header, &ignorefnidx, &skipfaultylines, "io.lines");
  if (lua_istrue(L, nargs)) { nargs--; tonumber = 1; }
  if (nargs > 1 && agn_isstring(L, nargs)) {
    xfree(sep);
    sep = tools_strdup(agn_tostring(L, nargs--));
    luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
  }
  /* start the processing */
  ls = tools_strlen(sep);
  if (ls == 0) {
    xfreeall(sep, unwrap);  /* 5.5.1 fix */
    luaL_error(L, "Error in " LUA_QS ": delimiter is the empty string.", "io.lines");
    return;
  }
  isseq = lua_isseq(L, 2);
  nargs = (isseq) ? agn_seqsize(L, 2) : nargs - 1;  /* nargs now has the number of fields to be retrieved */
  noindex = nargs == 0;  /* no index given ? -> return all fields */
  /* push values for iterator */
  luaL_checkstack(L, 10, "not enough stack space");
  lua_pushvalue(L, idx);        /* (1) push FILE userdata object */
  lua_pushboolean(L, toclose);  /* (2) close/not close file when finished */
  v = (intvec *)lua_newuserdata(L, sizeof(intvec));  /* (3) array with the field numbers */
  if (!(v && !_intvec_init(v, nargs))) {
    xfreeall(sep, unwrap);  /* 5.5.1 fix */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "io.lines");
  }
  luaL_getmetatable(L, "io_vecint");
  lua_setmetatable(L, -2);
  for (i=1; i <= nargs; i++) {
    index = (noindex) ? i : ((isseq) ? lua_seqrawgetinumber(L, 2, i) : agn_checkinteger(L, i + 1));
    _intvec_append(v, index);
  }
  lua_pushlstring(L, sep, ls);  /* (4) push delimiter */
  lua_pushstring(L, unwrap);    /* (5) push `unwrapper' (or `null`) */
  lua_pushboolean(L, tonumber); /* (6) convert-to-number flag */
  lua_pushinteger(L, 0);        /* (7) line counter */
  lua_pushboolean(L, header);   /* (8) header flag */
  lua_pushboolean(L, skipfaultylines);  /* (9) skipfaulty(lines) flag */
  if (ignorefnidx) {            /* (10) ignore function */
    if (!(lua_isfunction(L, ignorefnidx) || lua_isstring(L, ignorefnidx))) {  /* better sure than sorry */
      xfreeall(sep, unwrap);  /* 5.5.1 fix */
      luaL_error(L, "Error in " LUA_QS ": invalid right-hand side for " LUA_QS " option.", "io.lines", "ignore");
    }
    lua_pushvalue(L, ignorefnidx);
  } else {
    lua_pushnil(L);
  }
  lua_pushcclosure(L, io_auxreadcsvline, 10);
  xfreeall(sep, unwrap);
}

static int io_lines (lua_State *L) {
  int nargs = lua_gettop(L);
  if (lua_isnoneornil(L, 1)) {  /* no arguments? */
    /* will iterate over default input */
    lua_rawgeti(L, LUA_ENVIRONINDEX, IO_INPUT);
    tofile(L);  /* check that it's a valid file handle */
    aux_lines(L, 1, 0);
  }
  else if (lua_type(L, 1) == LUA_TSTRING) {
    const char *filename = agn_checkstring(L, 1);
    FILE **pf = newfile(L);
    *pf = fopen64(filename, "r");
    if (*pf == NULL)
      issueioerror(L, filename, "io.lines");  /* 2.16.10 */
    if (nargs == 1)
      aux_lines(L, lua_gettop(L), 1);  /* the FILE userdata is at the top of the stack */
    else
      aux_csvlines(L, lua_gettop(L), 1);
  } else {
    int result;
    FILE **f = (FILE**)luaL_getudata(L, 1, LUA_FILEHANDLE, &result);
    if (result) {  /* first argument is a file handle ? */
      if (*f == NULL)  /* but a closed file ? */
        luaL_error(L, "Error in " LUA_QS ": attempt to use a closed file.", "io.lines");
      else {  /* read line */
        if (nargs == 1)
          aux_lines(L, 1, 0);  /* but do not close it when done */
        else
          aux_csvlines(L, lua_gettop(L), 0);
      }
    }
    else
      luaL_error(L, "Error in " LUA_QS ": file handle or name expected.", "io.lines");
  }
  return 1;
}


static int f_lines (lua_State *L) {  /* 4.3.0 */
  tofile(L);  /* check that it's a valid file handle */
  aux_lines(L, 1, 0);
  return 1;
}


/*
** {======================================================
** READ
** =======================================================
*/


/* maximum length of a numeral */
#if !defined (L_MAXLENNUM)
#define L_MAXLENNUM     200
#endif


/* auxiliary structure used by 'read_number' */
typedef struct {
  FILE *f;  /* file being read */
  int c;  /* current character (look ahead) */
  int n;  /* number of elements in buffer 'buff' */
  char buff[L_MAXLENNUM + 1];  /* +1 for ending '\0' */
} RN;


/*
** Add current char to buffer (if not out of space) and read next one
*/
static int nextc (RN *rn) {
  if (rn->n >= L_MAXLENNUM) {  /* buffer overflow? */
    rn->buff[0] = '\0';  /* invalidate result */
    return 0;  /* fail */
  }
  else {
    rn->buff[rn->n++] = rn->c;  /* save current char */
    rn->c = l_getc(rn->f);  /* read next one */
    return 1;
  }
}


/*
** Accept current char if it is in 'set' (of size 2)
*/
static int test2 (RN *rn, const char *set) {
  if (rn->c == set[0] || rn->c == set[1])
    return nextc(rn);
  else return 0;
}


/*
** Read a sequence of (hex)digits
*/
static int readdigits (RN *rn, int hex) {
  int count = 0;
  while ((hex ? isxdigit(rn->c) : isdigit(rn->c)) && nextc(rn))
    count++;
  return count;
}


/*
** Read a number: first reads a valid prefix of a numeral into a buffer.
** Then it calls 'lua_stringtonumber' to check whether the format is
** correct and to convert it to a Lua number.
*/
static int read_number (lua_State *L, FILE *f) {
  RN rn;
  int count = 0;
  int hex = 0;
  char decp[2];
  rn.f = f; rn.n = 0;
  decp[0] = lua_getlocaledecpoint();  /* get decimal point from locale */
  decp[1] = '.';  /* always accept a dot */
  l_lockfile(rn.f);
  do { rn.c = l_getc(rn.f); } while (isspace(rn.c));  /* skip spaces */
  test2(&rn, "-+");  /* optional sign */
  if (test2(&rn, "00")) {
    if (test2(&rn, "xX")) hex = 1;  /* numeral is hexadecimal */
    else count = 1;  /* count initial '0' as a valid digit */
  }
  count += readdigits(&rn, hex);  /* integral part */
  if (test2(&rn, decp))  /* decimal point? */
    count += readdigits(&rn, hex);  /* fractional part */
  if (count > 0 && test2(&rn, (hex ? "pP" : "eE"))) {  /* exponent mark? */
    test2(&rn, "-+");  /* exponent sign */
    readdigits(&rn, 0);  /* exponent digits */
  }
  ungetc(rn.c, rn.f);  /* unread look-ahead char */
  l_unlockfile(rn.f);
  rn.buff[rn.n] = '\0';  /* finish string */
  if (lua_stringtonumber(L, rn.buff))  /* is this a valid number? */
    return 1;  /* ok */
  else {  /* invalid format */
   lua_pushnil(L);  /* "result" to be removed */
   return 0;  /* read fails */
  }
}


static int test_eof (lua_State *L, FILE *f) {  /* this is the original Lua 5.1/5.4 implementation; if you rewind later, you'll get errors */
  int c = getc(f);
  ungetc(c, f);            /* no-op when c == EOF */
  lua_pushliteral(L, "");  /* changed 2.22.0, taken from Lua 5.4.0 */
  return (c != EOF);
}


static int read_line (lua_State *L, FILE *f, int chop) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (;;) {
    size_t l;
    char *p = luaL_prepbuffer(&b);
    if (fgets(p, LUAL_BUFFERSIZE, f) == NULL) {  /* eof? */
      luaL_pushresult(&b);  /* close buffer */
      return (lua_strlen(L, -1) > 0);  /* check whether read something */
    }
    l = tools_strlen(p);  /* 2.17.8 tweak */
    if (l == 0 || p[l - 1] != '\n')
      luaL_addsize(&b, l);
    else {
      if (l > 1 && p[l - 2] == '\r')  /* Agena 1.2.1 patch; added in 0.5.4 to correctly read DOS files on UNIX platforms */
        luaL_addsize(&b, l - 2*chop);  /* do not include \r\n `eol', changed 2.22.0 */
      else
        luaL_addsize(&b, l - chop);  /* do or do not include \n `eol', changed 2.22.0 */
      luaL_pushresult(&b);  /* close buffer */
      return 1;  /* read at least an `eol' */
    }
  }
}


static void read_all (lua_State *L, FILE *f) {  /* taken from Lua 5.4.0, at least on Windows, it is not faster then read_chars */
  size_t nr;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  do {  /* read file in chunks of LUAL_BUFFERSIZE bytes */
    char *p = luaL_prepbuffer(&b);
    nr = fread(p, CHARSIZE, LUAL_BUFFERSIZE, f);
    luaL_addsize(&b, nr);
  } while (nr == LUAL_BUFFERSIZE);
  luaL_pushresult(&b);  /* close buffer */
}


static int read_chars (lua_State *L, FILE *f, size_t n) {
  size_t rlen;  /* how much to read */
  size_t nr;  /* number of chars actually read */
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  rlen = LUAL_BUFFERSIZE;  /* try to read that much each time */
  do {
    char *p = luaL_prepbuffer(&b);
    if (rlen > n) rlen = n;  /* cannot read more than asked */
    nr = fread(p, CHARSIZE, rlen, f);
    luaL_addsize(&b, nr);
    n -= nr;  /* still have to read `n' chars */
  } while (n > 0 && nr == rlen);  /* until end of count or eof */
  luaL_pushresult(&b);  /* close buffer */
  return (n == 0 || lua_strlen(L, -1) > 0);
}


static int g_read (lua_State *L, FILE *f, int first) {
  int nargs, success, n;
  nargs = lua_gettop(L) - 1;
  clearerr(f);
  if (nargs == 0) {  /* no arguments? */
    success = read_line(L, f, 1);
    n = first + 1;  /* to return 1 result */
  }
  else {  /* ensure stack space for all results and for auxlib's buffer */
    luaL_checkstack(L, nargs + LUA_MINSTACK, "too many arguments");
    success = 1;
    for (n = first; nargs-- && success; n++) {
      if (lua_type(L, n) == LUA_TNUMBER) {
        size_t l = (size_t)lua_tointeger(L, n);
        success = (l == 0) ? test_eof(L, f) : read_chars(L, f, l);
      }
      else {
        const char *p = lua_tostring(L, n);
        luaL_argcheck(L, p && p[0] == '*', n, "invalid option");
        switch (p[1]) {
          case 'n':  /* number */
            success = read_number(L, f);
            break;
          case 'l':  /* line */
            success = read_line(L, f, 1);
            break;
          case 'L':  /* line with end-of-line */
            success = read_line(L, f, 0);
            break;
          case 'a':  /* file */
            /* read_chars(L, f, ~((size_t)0)); */ /* read MAX_SIZE_T chars */
            read_all(L, f);  /* read entire file, taken from Lua 5.4.0 */
            success = 1; /* always success */
            break;
          default:
            return luaL_argerror(L, n, "invalid format");
        }
      }
    }
  }
  if (ferror(f)) {
    return pushresult(L, 0, NULL);
  }
  if (!success) {
    agn_poptop(L);  /* remove last result */
    lua_pushnil(L);  /* push nil instead */
  }
  return n - first;
}


static int io_read (lua_State *L) {
  if (lua_isnoneornil(L, 1)) {  /* no arguments? */
    /* will iterate over default input */
    if (agn_getgui(L))
      luaL_error(L, "Error in " LUA_QS ": keyboard input not supported by AgenaEdit.", "io.read");
    return g_read(L, getiofile(L, IO_INPUT), 1);
  }
  else {
    int result;
    FILE **pf = (FILE **)luaL_getudata(L, 1, LUA_FILEHANDLE, &result);
    if (result) {  /* first argument is a file handle ? */
      if (*pf == NULL)  /* but a closed file ? */
        luaL_error(L, "Error in " LUA_QS " package: attempt to use a closed file.", "io");
      else  /* read line */
        return g_read(L, *pf, 2);
    }
    else
      luaL_error(L, "Error in " LUA_QS " package: file handle or name expected.", "io");
  }
  return 1;
}


static int f_read (lua_State *L) {  /* 4.3.0 */
  return g_read(L, tofile(L), 2);
}


static FILE *aux_gethandle (lua_State *L, const char **filename, const char *mode, const char *procname) {
  FILE *pf;
  if (lua_isstring(L, 1)) {
    *filename = agn_checkstring(L, 1);
    pf = fopen64(*filename, mode);
    if (pf == NULL)
      luaL_error(L, "Error in " LUA_QS ": file %s could not be opened.", procname, *filename);  /* bugfix 2.3.0 RC 3 */
  } else {  /* 2.2.0 */
    pf = tofile(L);
    if (pf == NULL)  /* but a closed file ? */
      luaL_error(L, "Error in " LUA_QS ": attempt to use a closed file.", procname);
  }
  return pf;
}


/* readfile: 1.10.2, 1 % faster on Windows than the Agena version, 8 % faster on Mac OS X;
   1.11.2 extended to return file contents without line breaks */
static int io_readfile (lua_State *L) {
  const char *filename, *pattern;
  int r, flag, removenls;
  size_t plen;
  FILE *pf;
  r = 1;
  filename = NULL;
  pf = aux_gethandle(L, &filename, "rb", "io.readfile");  /* 2.2.0 */
  removenls = agnL_optboolean(L, 2, 0);  /* remove carriage returns and/or newlines */
  pattern = luaL_optlstring(L, 3, NULL, &plen);
  flag = agnL_optboolean(L, 4, 1);  /* return only matching file contents, default = true */
  if (removenls)
    agnL_readlines(L, pf, "io.readfile", 0);  /* 2.16.10 */
  else
    r = read_chars(L, pf, ~((size_t)0));  /* read MAX_SIZE_T chars */
  if (pattern != NULL && r == 1) {  /* 1.11.6 */
    size_t l;
    const char *str = lua_tolstring(L, -1, &l);
    char *lookup = agnL_strmatch(L, str, l, pattern, plen);
    if (lookup == NULL) {  /* specific substring in file not found ? */
      if (flag == 1) {  /* return only matching file contents ? */
        agn_poptop(L);  /* pop string and return `null` */
        lua_pushnil(L);
      }  /* else leave file contents on stack */
    } else {  /* pattern found ? */
      if (flag == 1) {  /* return only matching file contents ? */
        lua_pushnumber(L, lookup - str + 1);  /* leave string on stack and also push position where the match starts */
        r = 2;
      } else {
        agn_poptop(L);  /* pop string and return `null` */
        lua_pushnil(L);
      }
    }
  }
  if (ferror(pf)) {
    luaL_error(L, "Error in " LUA_QS ": file %s could possibly not be read.",
      "io.readfile", filename == NULL ? "\b" : filename);
  }
  if (filename != NULL && fclose(pf) != 0)
    luaL_error(L, "Error in " LUA_QS ": file %s could not be closed.", "io.readfile", filename);
  return r;
}


static int io_infile (lua_State *L) {  /* 1.11.6 */
  const char *filename, *pattern;
  char *line, buf[agn_getbuffersize(L)];  /* 2.34.9 adaption */
  int r, found, hasspec;
  size_t linelength, plength, bufsize, maxbufsize;
  ptrdiff_t start, end;
  FILE *fp;
  bufsize = agn_getbuffersize(L);
  found = 0;
  maxbufsize = 0;
  line = NULL;
  filename = NULL;
  fp = aux_gethandle(L, &filename, "rb", "io.infile");  /* 2.2.0 */
  pattern = agn_checklstring(L, 2, &plength);
  if (plength == 0)
    luaL_error(L, "Error in " LUA_QS ": pattern must not be the empty string.", "io.infile");
  if (!agnL_isvalidpattern(L, pattern, plength, &hasspec, 1)) {  /* 6.1.1 fix, invalid pattern given */
    luaL_error(L, "Error in " LUA_QS ": %s", "io.infile", agn_tostring(L, -1));
  }
  while ( (line = tools_getline(fp, buf, line, &linelength, bufsize, &maxbufsize, &r)) ) {  /* 2.14.12, 2.21.5 tuning, 2.34.9 change */
    if (r > 0)
      luaL_error(L, "Error in " LUA_QS ": buffer allocation error.", "io.infile");
    else if (r == -1) break;  /* EOF reached and last line empty ? */
    if (hasspec) {  /* 2.36.1 */
      found = (agn_strmatch(L, line, linelength, pattern, 1, &start, &end) != NULL);
    } else {
      found = (tools_lmemfind(line, linelength, pattern, plength) != NULL);  /* 2.21.5 tuning by 5 % */
    }
    if (found) break;  /* found ? */
  }
  xfree(line);
  if (ferror(fp)) {  /* 2.2.0 */
    luaL_error(L, "Error in " LUA_QS ": file %s could possibly not be read.",
      "io.infile", filename == NULL ? "\b" : filename);
  }
  if (filename != NULL && fclose(fp) != 0)  /* 2.2.0 */
    luaL_error(L, "Error in " LUA_QS ": file %s could not be closed.", "io.infile", filename);
  lua_pushboolean(L, found);
  return 1;
}


static int io_auxreadline (lua_State *L) {
  FILE *f = *(FILE **)lua_touserdata(L, lua_upvalueindex(1));
  int success;
  if (f == NULL)  /* file is already closed? */
    luaL_error(L, "Error in " LUA_QS ": file is already closed.", "io.lines");
  success = read_line(L, f, 1);
  if (ferror(f))
    return luaL_error(L, "%s.", my_ioerror(errno));  /* changed 2.10.4 */
  if (success) return 1;
  else {  /* EOF */
    if (lua_toboolean(L, lua_upvalueindex(2))) {  /* generator created file? */
      lua_settop(L, 0);
      lua_pushvalue(L, lua_upvalueindex(1));
      aux_close(L);  /* close it */
    }
    return 0;
  }
}

static void aux_closecsvfile(lua_State *L) {  /* 3.10.4 */
  if (lua_toboolean(L, lua_upvalueindex(2))) {  /* generator created file? */
    lua_settop(L, 0);
    lua_pushvalue(L, lua_upvalueindex(1));
    aux_close(L);  /* close it */
  }
}

static int io_auxreadcsvline (lua_State *L) {  /* 3.10.3 */
  int success;
  FILE *f = *(FILE **)lua_touserdata(L, lua_upvalueindex(1));
  if (f == NULL)  /* file is already closed? */
    luaL_error(L, "Error in " LUA_QS ": file is already closed.", "io.lines");
jumpback:
  success = read_line(L, f, 1);
  if (ferror(f))
    return luaL_error(L, "%s.", my_ioerror(errno));
  if (success) {
    intvec *v;
    size_t l1, l2;
    int i, n, c, index, init, tonumber, lc, lu, reset, header, skipfaultylines, fetchnextline, getline;
    const char *e, *s, *s1, *sep, *unwrap;
    sep = unwrap = NULL;
    getline = 1;
    v = (intvec *)lua_touserdata(L, lua_upvalueindex(3));  /* (3) array with the field numbers */
    sep = lua_tolstring(L, lua_upvalueindex(4), &l2);
    if (lua_isstring(L, lua_upvalueindex(5)))
      unwrap = lua_tostring(L, lua_upvalueindex(5));
    tonumber = lua_toboolean(L, lua_upvalueindex(6));
    lc = lua_tointeger(L, lua_upvalueindex(7));
    header = lua_toboolean(L, lua_upvalueindex(8));
    skipfaultylines = lua_toboolean(L, lua_upvalueindex(9));
    if (lc == 0 && header) {  /* we are at the beginning of the file, 3.10.4 */
      lua_pushinteger(L, 1);
      lua_replace(L, lua_upvalueindex(7));
      agn_poptop(L);  /* drop header */
      goto jumpback;  /* fetch next line */
    } else if (lua_isfunction(L, lua_upvalueindex(10))) {  /* 3.10.4 */
      luaL_checkstack(L, 2, "not enough stack space");
      lua_pushvalue(L, lua_upvalueindex(10));
      lua_pushvalue(L, -2);
      lua_call(L, 1, 1);
      if (!lua_isboolean(L, -1)) {
        agn_poptoptwo(L);  /* remove boolean and line */
        luaL_error(L, "Error in " LUA_QS ": ignore function does not evaluate to a boolean.", "io.lines");
      }
      fetchnextline = agn_istrue(L, -1);
      agn_poptop(L);  /* remove boolean */
      if (fetchnextline) {
        agn_poptop(L);  /* drop line */
        goto jumpback;  /* fetch next line */
      }
    } else if (lua_isstring(L, lua_upvalueindex(10))) {  /* 5.4.3, ignore lines starting with the string */
      int pos;
      size_t p_len;
      const char *p = lua_tolstring(L, lua_upvalueindex(10), &p_len);  /* the pattern */
      s = lua_tolstring(L, -1, &l1);  /* the line */
      if (p_len == 0 || l1 == 0) {
        /* bail out if string or pattern is empty or if pattern is longer or equal in size */
        fetchnextline = 0;
      } else if (!tools_hasstrchr(p, SPECIALS, p_len)) {  /* or no special characters? Agena 1.3.2, 2.16.5/2.16.12/2.26.3 optimisation */
        pos = tools_strncmp(s, p, p_len);
        fetchnextline = (pos == 0 && p_len < l1);
      } else {
        MatchState ms;
        initMatchState(L, &ms, 0, s, l1);
        fetchnextline = (match(&ms, s, p, 1) != NULL);
      }
      if (fetchnextline) {
        agn_poptop(L);  /* drop line */
        goto jumpback;  /* fetch next line */
      }
      getline = 0;
    }
    if (getline) s = lua_tolstring(L, -1, &l1);  /* we might have already read the line */
    if (skipfaultylines && l1 == 0) {  /* empty line and skipfaultylines option set to true ? */
      agn_poptop(L);  /* pop line */
      goto jumpback;  /* and try to read the next one, 3.10.7 */
    }
    agn_createseq(L, v->size);
    for (i=0; i < v->size; i++) {
      lua_pushstring(L, "");  /* fill sequence with empty strings for it might later be filled in non-ascending field order */
      lua_seqrawseti(L, -2, i + 1);
    }
    c = n = init = lu = reset = 0;
    do {  /* determine the number of words in the string */
      s1 = tools_lmemfind(s + init, l1 - init, sep, l2);
      if (s1) {
        init += (s1 - (s + init)) + l2;
        n++;
      }
    } while (s1);
    n++;  /* now we have the number of fields, not separators */
    if (v->size == 0) {
      for (i=1; i <= n; i++) _intvec_append(v, i);
      reset = 1;
    } else {
      for (i=1; i <= v->size; i++) {
        index = _intvec_get(v, i - 1);
        if (index < 0) {
          index += n + 1;
          _intvec_set(v, i - 1, index);
        }
        if (index <= 0 || index > n) {
          agn_poptoptwo(L);  /* pop sequence and original line */
          if (skipfaultylines) goto jumpback;  /* 3.10.4 */
          lua_pushfail(L);
          return 1;
        }
      }
    }
    if (unwrap) lu = tools_strlen(unwrap);
    /* collect all matching tokens into a sequence */
    while ((e=strstr(s, sep)) != NULL) {
      c++;
      for (i=0; i < v->size; i++) {
        if (_intvec_get(v, i) == c) {  /* the user might pass the field numbers in arbitrary order */
          lua_pushlstring(L, s, e - s);
          if (unwrap) agnL_strunwrap(L, -1, unwrap, lu);
          if (tonumber) {
            if (!agnL_strtonumber(L, -1)) agnL_strtocomplex(L, -1);  /* complex value must be in the form a+I*b. */
          }
          lua_seqrawseti(L, -2, i + 1);
        }
      }
      s = e + l2;
    }
    /* process last token */
    if (tools_strlen(s) != 0) {  /* not the empty string ? */
      c++;
      for (i=0; i < v->size; i++) {
        if (_intvec_get(v, i) == c) {  /* the user might pass the field numbers in arbitrary order */
          lua_pushstring(L, s);
          if (unwrap) agnL_strunwrap(L, -1, unwrap, lu);
          if (tonumber) {  /* 3.10.3 fix for complex numbers */
            if (!agnL_strtonumber(L, -1)) agnL_strtocomplex(L, -1);
          }
          lua_seqrawseti(L, -2, i + 1);
        }
      }
    }
    lua_remove(L, -2);  /* remove original line pushed onto the stack */
    /* if no field numbers have been explicitly given, clean up array for next line which might
       have a different number of fields */
    if (reset) {
      for (i=v->size; i > 0; i--) _intvec_delete(v, i - 1);
    }
    return 1;
  } else {  /* EOF */
    aux_closecsvfile(L);
    return 0;
  }
}


/* }====================================================== */


static int g_write (lua_State *L, FILE *f, int arg, int flag, char *delim) {
  int nargs, status;
  nargs = lua_gettop(L) - 1 - 1*(delim != NULL);  /* 0.30.4 */
  /* subtract 1 because if an option is given, it is still on the stack */
  status = 1;
  for (; nargs--; arg++) {  /* 0.26.0 simplification */
    size_t l;
    const char *s;
    if (lua_isnil(L, arg)) {  /* 2.21.0 improvement */
      s = "null"; l = 4;
    } else
      s = luaL_checklstring(L, arg, &l);  /* also converts Booleans */
    status = status && (fwrite(s, CHARSIZE, l, f) == l);
    if (delim != NULL && nargs != 0)  /* do not print delim after last argument */
      fwrite(delim, CHARSIZE, tools_strlen(delim), f);  /* 2.17.8 tweak */
  }
  if (flag) fwrite("\n", CHARSIZE, 1, f);
  if (delim != NULL) { xfree(delim); }
  return pushresult(L, status, NULL);
}


/* extended November 22, 2008 - 0.12.2; patched 11.07.2009 (0.24.3), patched 31.01.2010 (0.30.4), patched
   14.11.2011 (1.5.1)
   newline = 0 -> no newline, 1 -> insert a newline */
static int io_writeaux (lua_State *L, int newline) {
  int result, nargs;
  char *delim;
  FILE **f = (FILE **)luaL_getudata(L, 1, LUA_FILEHANDLE, &result);
  delim = NULL;  /* to avoid compiler warnings */
  nargs = lua_gettop(L);
  if (nargs != 0 && lua_ispair(L, nargs)) {  /* 0.24.3 patch */
    luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.2 fix */
    agn_pairgeti(L, nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, nargs, 2);  /* get right value, set to stack index  -1 */
    if (tools_strneq("delim", agn_checkstring(L, -2)))  /* 2.16.12 tweak */
      luaL_error(L, "Error in " LUA_QS ": unknown option.", (newline == 0) ? "write" : "writeline");  /* 1.5.1 */
    else {
      size_t l;
      const char *str = agn_checklstring(L, -1, &l);
      delim = tools_strndup(str, l);  /* free() see g_write; `Agena 1.0.4, 1.5.1, 2.27.0 optimisation */
    }
    agn_poptoptwo(L);  /* 0.30.4 */
  }
  if (result) {  /* first argument is a file handle ? */
    if (*f == NULL) {  /* but a closed file ? */
      luaL_error(L, "Error in " LUA_QS ": attempt to use a closed file.", (newline == 0) ? "io.write" : "io.writeline");  /* 1.5.1 */
      return 0;  /* just to prevent compiler warnings */
    } else  /* write to file */
      return g_write(L, *f, 2, newline, delim);
  }
  else
    return g_write(L, getiofile(L, IO_OUTPUT), 1, newline, delim);
}


static int io_write (lua_State *L) {
  io_writeaux(L, 0);
  return 1;
}


static int f_write (lua_State *L) {  /* 4.3.0 */
  return g_write(L, tofile(L), 2, 0, NULL);
}


static int io_writeline (lua_State *L) {
  io_writeaux(L, 1);
  return 1;
}


static int f_writeline (lua_State *L) {  /* 4.3.0 */
  return g_write(L, tofile(L), 2, 1, NULL);
}


static int io_seek (lua_State *L) {
  static const int mode[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  FILE *f = tofile(L);
  int64_t offset = 0;
  int op = luaL_checkoption(L, 2, "cur", modenames);
  offset = (int64_t)luaL_optnumber(L, 3, 0);
  op = _fseeki64(f, offset, mode[op]);  /* 2.2.1 fix */
  if (op != 0)  /* 1.9.3 */
    return pushresult(L, 0, NULL);  /* error */
  else {
    lua_Number offset = _ftelli64(f);  /* 2.3.0 RC 2 */
    lua_pushnumber(L, (offset == -1) ? AGN_NAN : offset);
    return 1;
  }
}


static int io_filepos (lua_State *L) {  /* same as binio_filepos, 4.3.0 */
  int en, nargs;
  off64_t fpos;  /* off64_t is signed */
  nargs = lua_gettop(L);
  FILE *f = tofile(L);
  set_errno(0);  /* better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  if (nargs == 2 && agn_isnumber(L, 2)) {  /* 7.3.3 extension: set file position to offset, relative to
    the beginning of the file. */
    fpos = (off64_t)agn_tonumber(L, 2);
    if (fpos < 0)
      luaL_error(L, "Error in " LUA_QS ": offset must be nonnegative.", "io.filepos");
    fpos = _fseeki64(f, fpos, SEEK_SET);
    if (fpos >= 0) {
      lua_pushnumber(L, fpos);
    } else {
      return pushresult(L, 0, NULL);  /* error */
    }
  } else {
    fpos = _ftelli64(f);
    en = errno;
    if (fpos == -1)
      luaL_error(L, "Error in " LUA_QS ": %s.", "io.filepos", my_ioerror(en));
    else
      lua_pushnumber(L, (lua_Number)fpos);
  }
  return 1;
}


static int io_setvbuf (lua_State *L) {
  static const int mode[] = {_IONBF, _IOFBF, _IOLBF};
  static const char *const modenames[] = {"no", "full", "line", NULL};
  FILE *f = tofile(L);
  int op = luaL_checkoption(L, 2, NULL, modenames);
  lua_Integer sz = agnL_optinteger(L, 3, LUAL_BUFFERSIZE);
  int res = setvbuf(f, NULL, mode[op], sz);
  return pushresult(L, res == 0, NULL);
}


static int io_sync (lua_State *L) {
  if (lua_isnoneornil(L, 1))  /* no arguments ? -> default output */
    return pushresult(L, fflush(getiofile(L, IO_OUTPUT)) == 0, NULL);
  else {
    int rc1, rc2;
    rc1 = fflush(tofile(L)) == 0;  /* flush buffers */
    rc2 = tools_fsync(agn_tofileno(L, 1, 1)) == 0;
    return pushresult(L, rc1 && rc2, NULL);
  }
}


static int f_flush (lua_State *L) {  /* 4.3.0 */
  int rc1, rc2;
  rc1 = fflush(tofile(L)) == 0;  /* flush buffers */
  rc2 = tools_fsync(agn_tofileno(L, 1, 1)) == 0;
  return pushresult(L, rc1 && rc2, NULL);
}


/* read in an entire file and return its line in a table; taken from own C code (clean.c);
   0.10.0, April 27, 2008; extended 0.12.2, November 08, 2008;
   patched 0.27.2, October 13, 2009 to fix the long-known `cannot read more than 2048
   chars per line` bug.
   extended 0.27.2, October 14, 2009 to accept file handles. */
static int io_readlines (lua_State *L) {
  int hasoption, hasfunction, r;
  size_t i, linelength, bufsize, maxbufsize;
  unsigned int c, convert;
  char *line, buf[agn_getbuffersize(L)];  /* 2.34.9 adaption */
  const char *skip, *filename;
  FILE *fp;
  bufsize = agn_getbuffersize(L);
  skip = "";
  maxbufsize = convert = hasoption = hasfunction = c = 0;
  for (i=2; i <= lua_gettop(L); i++) {
    if (agn_isstring(L, i)) {
      skip = agn_tostring(L, i);
      hasoption = 1;
    } else if (lua_isboolean(L, i)) {  /* Agena 1.6.0 */
      convert = lua_toboolean(L, i);
    } else if (lua_isfunction(L, i))
      hasfunction = i;
  }
  line = NULL;
  filename = NULL;
  fp = aux_gethandle(L, &filename, "rb", "io.readlines");  /* 2.2.0 */
  lua_newtable(L);
  while ( (line = tools_getline(fp, buf, line, &linelength, bufsize, &maxbufsize, &r)) ) {  /* 2.14.12, 2.21.5 tuning, 2.34.9 change */
    if (r > 0) luaL_error(L, "Error in " LUA_QS ": buffer allocation error.", "io.readlines");
    else if (r == -1) break;  /* EOF reached and last line empty ? */
    /* A conversion is only needed in Windows; in your UNIX terminal, you may have to set the character set properly. */
    if (convert) {
      for (i=0; i < linelength; i++) {  /* 1.6.11 fix */
        line[i] = f2c[uchar(line[i])];
      }
    }
    /* delete \r in DOS files */
    if (0 && linelength > 0 && line[linelength - 1] == '\r')  /* 1.6.11 Valgrind, changed 2.14.12 */
      line[linelength - 1] = '\0';
    if (!hasoption || (strstr(line, skip) != line)) {
      /* if line begins with the optional second string skip it */
      if (!hasfunction) {
        lua_rawsetilstring(L, -1, ++c, line, linelength);
      } else {  /* apply a function before table insertion, 2.14.8 */
        lua_pushvalue(L, hasfunction);
        lua_pushlstring(L, line, linelength);
        lua_call(L, 1, 1);
        /* do not check whether result is a string, insert function result regardless of its type */
        lua_rawseti(L, -2, ++c);
      }
    }
  }
  xfree(line);
  if (filename != NULL && fclose(fp) != 0)  /* 1.11.6 */
    luaL_error(L, "Error in " LUA_QS ": file %s could not be closed.", "io.readlines", filename);
  return 1;
}


static int io_nlines (lua_State *L) {
  int r;
  size_t linelength, bufsize, maxbufsize;
  uint32_t c;
  FILE *fp;
  char *line, buf[agn_getbuffersize(L)];  /* 2.34.9 adaption */
  const char *filename = NULL;
  line = NULL;
  bufsize = agn_getbuffersize(L);
  maxbufsize = 0;
  c = 0;
  fp = aux_gethandle(L, &filename, "rb", "io.nlines");  /* 2.2.0, 2.14.12 */
  while ( (line = tools_getline(fp, buf, line, &linelength, bufsize, &maxbufsize, &r)) != NULL ) {  /* 2.14.12, 2.21.5 tuning, 2.34.9 change */
    if (r > 0)
      luaL_error(L, "Error in " LUA_QS ": buffer allocation error.", "io.nlines");
    else if (r == -1) break;  /* EOF reached and last line empty ? */
    c++;
  }
  xfree(line);
  if (filename != NULL && fclose(fp) != 0)  /* 1.11.6 */
    luaL_error(L, "Error in " LUA_QS ": file %s could not be closed.", "io.nlines", filename);
  lua_pushnumber(L, c);
  return 1;
}

/* Skips the given number of lines and sets the file position to the beginning of the line
   that follows the last line skipped.

   If f is a file name, then with each call to io.skiplines the search always starts at the
   very first line in the file. If you use a file handle, then lines can be skipped multiple
   times, always relative to the current file position.

   The second argument n may be any nonnegative number. If n is 0, then the function does
   nothing and does not change the file position.

   The function returns two values: the number of lines actually skipped and the number of
   characters skipped in this process, including newlines.

   Based on io_nlines; Agena 1.10.1, 12.03.2013 */

static int io_skiplines (lua_State *L) {
  size_t c, linelength, nchars, nlines, bufsize, maxbufsize;
  int r;
  char *line, buf[agn_getbuffersize(L)];  /* 2.34.9 adaption */
  const char *filename;
  FILE *fp;
  bufsize = agn_getbuffersize(L);
  c = 0;
  nchars = 0;
  maxbufsize = 0;
  line = NULL;
  filename = NULL;
  fp = aux_gethandle(L, &filename, "rb", "io.skiplines");  /* 2.2.0 */
  nlines = agn_checknonnegint(L, 2);
  if (nlines == 0) {  /* do noting just return */
    if (filename != NULL && fclose(fp) != 0)
      luaL_error(L, "Error in " LUA_QS ": file could not be closed.", "io.skiplines");
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 2;
  }
  while ( (line = tools_getline(fp, buf, line, &linelength, bufsize, &maxbufsize, &r)) ) {  /* 2.14.12, 2.21.5 tuning, 2.34.9 change */
    if (r > 0) luaL_error(L, "Error in " LUA_QS ": buffer allocation error.", "io.skiplines");
    else if (r == -1) break;  /* EOF reached and last line empty ? */
    nchars += linelength;
    if (++c == nlines) break;  /* nlines to skip or EOF reached ? */
  }
  xfree(line);
  if (filename != NULL && fclose(fp) != 0)
    luaL_error(L, "Error in " LUA_QS ": file could not be closed.", "io.skiplines");
  lua_pushnumber(L, c);
  lua_pushnumber(L, nchars + c);  /* include newlines */
  return 2;
}


/* Reads the remainder of a file starting from a specific marker character or until a validator function returns `true`.

   You can either pass a filename or a file handle for first argument h.

   If h is a file handle, the search starts from the current file position, if h is a filename, the search starts from
   the beginning of the file.

   In the first form, `chr` denotes the marker character which should be a string of length 1. In the second form,
   `f` is a function returning a Boolean.

   The function returns a string, either the contents of the file from the marker to the end of the file, or an
   empty string if the marker is not found.

   This function scans the file from its current position until it encounters the first occurrence of the specified char `chr`
   or `f` returns `true`. It consumes the marker, positions the file cursor immediately after it, and reads the rest of the
   file into memory.

   The returned string includes `chr` at the very beginning or the character where `f` evaluates to `true`. If `chr` is not
   found or `f` always returns `false` before reaching the end of the file (EOF), the function returns an empty string.

   With a file handle h, the function does not close the file when returning, with a filename h it does.

   Based on a proposal by Gemini AI, 7.8.11 */
static int io_readfrom (lua_State *L) {
  int ch, bufsize, targetchar, includemarker;
  size_t l, nr;
  luaL_Buffer b;
  const char *filename, *str;
  FILE *fp;
  targetchar = ch = 0;
  bufsize = agn_getbuffersize(L);
  filename = NULL;
  fp = aux_gethandle(L, &filename, "rb", "io.readfrom");
  includemarker = agnL_optboolean(L, 3, 0);  /* 7.8.12 change */
  if (agn_isstring(L, 2)) {
    str = lua_tolstring(L, 2, &l);
    if (l == 0)
      luaL_error(L, "Error in " LUA_QS ": target character is the empty string.", "io.readfrom");
    targetchar = str[0];
    while ((ch = fgetc(fp)) != EOF) {  /* advance until targetchar is consumed */
      if (ch == targetchar) break;
    }
  } else if (lua_isfunction(L, 2)) {
    int rc;
    while ((ch = fgetc(fp)) != EOF) {  /* advance until targetchar is consumed */
      lua_pushvalue(L, 2);
      lua_pushchar(L, ch);
      lua_call(L, 1, 1);
      rc = lua_istrue(L, -1);
      agn_poptop(L);
      if (rc) {
        targetchar = ch;
        break;
      }
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": second argument is not a character or function.", "io.readfrom");
  }
  if (ch == EOF) {  /* target was never found */
    if (filename != NULL && fclose(fp) != 0)
      luaL_error(L, "Error in " LUA_QS ": file could not be closed.", "io.readfrom");
    lua_pushliteral(L, "");
    return 1;
  }
  luaL_buffinit(L, &b);
  if (includemarker) {  /* include the target character in the result, changed 7.8.12 */
    luaL_addchar(&b, targetchar);
  }
  do {
    char *p = luaL_prepbuffer(&b);
    nr = fread(p, 1, bufsize, fp);
    luaL_addsize(&b, nr);
  } while (nr == bufsize);
  luaL_pushresult(&b);
  if (filename != NULL && fclose(fp) != 0)
    luaL_error(L, "Error in " LUA_QS ": file could not be closed.", "io.readfrom");
  return 1;
}


static int io_readto (lua_State *L) {  /* Based on io_readfrom, 7.8.12 */
  int ch, targetchar, includemarker;
  size_t l;
  luaL_Buffer b;
  const char *filename, *str;
  FILE *fp;
  targetchar = ch = 0;
  filename = NULL;
  fp = aux_gethandle(L, &filename, "rb", "io.readto");
  includemarker = agnL_optboolean(L, 3, 0);
  luaL_buffinit(L, &b);
  if (agn_isstring(L, 2)) {
    str = lua_tolstring(L, 2, &l);
    if (l == 0)
      luaL_error(L, "Error in " LUA_QS ": target character is the empty string.", "io.readto");
    targetchar = str[0];
    while ((ch = fgetc(fp)) != EOF) {  /* insert until targetchar is consumed */
      if (ch == targetchar) {
        if (includemarker) {  /* include the target character in the result */
          luaL_addchar(&b, targetchar);
        }
        break;
      }
      luaL_addchar(&b, ch);
    }
  } else if (lua_isfunction(L, 2)) {
    int rc, flag = 0;
    while ((ch = fgetc(fp)) != EOF) {  /* insert until targetchar is consumed */
      lua_pushvalue(L, 2);
      lua_pushchar(L, ch);
      lua_call(L, 1, 1);
      rc = lua_isfalse(L, -1);
      agn_poptop(L);
      if (rc) {
        luaL_addchar(&b, ch); flag = 1;
      } else {
        if (flag && includemarker) {  /* include the target character in the result */
          luaL_addchar(&b, ch);
        }
        break;
      }
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": second argument is not a character or function.", "io.readto");
  }
  luaL_pushresult(&b);
  if (filename != NULL && fclose(fp) != 0)
    luaL_error(L, "Error in " LUA_QS ": file could not be closed.", "io.readto");
  return 1;
}


/* get a single keystroke; June 29, 2007; does not work in Haiku */

#if defined(_WIN32) || defined(__unix__) || defined(__OS2__) || defined(__APPLE__)
static int io_getkey (lua_State *L) {
  if (agn_getgui(L))
    luaL_error(L, "Error in " LUA_QS ": keyboard input not supported by AgenaEdit.", "io.getkey");
  if (lua_gettop(L) == 0) {  /* 2.16.1 */
    lua_pushinteger(L, getch());
    return 1;
  } else
    (void)getch();
  return 0;
}
#endif


static int io_anykey (lua_State *L) {
  if (agn_getgui(L))
    luaL_error(L, "Error in " LUA_QS ": keyboard input not supported by AgenaEdit.", "io.anykey");
#if defined(_WIN32)
  int i = kbhit();
  lua_pushboolean(L, i);
  if (i) { getch(); }  /* 0.31.4, `clear the buffer` */
#elif defined(__OS2__) || defined(__unix__) || defined(__APPLE__)
  int i;
  i = kbhit();
  if (i == -1)
    luaL_error(L, "Error in " LUA_QS ": encountered terminal IO failure.", "io.anykey");
  lua_pushboolean(L, i);
#else
  lua_pushfail(L);
#endif
  return 1;
}


void *io_gethandle (lua_State *L) {  /* 0.32.5 */
  void *ud;
  luaL_checkany(L, 1);
  ud = lua_touserdata(L, 1);
  if (ud == NULL || (*((FILE **)ud)) == NULL) return NULL;
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_FILEHANDLE);
  if (!lua_getmetatable(L, 1)) {
    agn_poptop(L);  /* pop field */
    return NULL;
  }
  if (!lua_rawequal(L, -2, -1)) ud = NULL;
  agn_poptoptwo(L);  /* pop metatable and field */
  return ud;
}


/* determine whether the argument is a file descriptor pointing to an open file, 0.12.2,
   patched 0.32.5 for proper cleaning of stack before leaving */
static int io_isfdesc (lua_State *L) {
  void *ud = io_gethandle(L);
  lua_pushboolean(L, ud != NULL);
  return 1;
}


static int io_isopen (lua_State *L) {  /* 1.12.1 */
  void *ud = io_gethandle(L);
  if (ud == NULL) {
    lua_pushfalse(L);
  } else {
    FILE *f = *((FILE **)ud);
    lua_pushboolean(L, !(_fseeki64(f, 0, SEEK_CUR) != 0 || _ftelli64(f) == -1));
  }
  return 1;
}


static int io_lock (lua_State *L) {  /* 0.32.5 */
  size_t nargs;
  void *ud;
  int hnd;
  off64_t start, size;
  hnd = 0;
  ud = io_gethandle(L);
  if (ud == NULL)
    luaL_error(L, "Error in " LUA_QS ": invalid file handle.", "io.lock");
  hnd = fileno((*((FILE **)ud)));
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1) {  /* lock entire file (in Windows lock 2^63 bytes only) */
    start = 0;
    size = 0;
  } else {
    /* lock from current file position */
    start = my_fpos(hnd);
    size = agnL_checknumber(L, 2);
    if (size < 0) luaL_error(L, "Error in " LUA_QS ": must lock at least one byte.", "io.lock");
  }
  lua_pushboolean(L, my_lock(hnd, start, size) == 0);
  return 1;
}


static int io_unlock (lua_State *L) {  /* 0.32.5 */
  size_t nargs;
  void *ud;
  int hnd;
  off64_t start, size;
  hnd = 0;
  ud = io_gethandle(L);
  if (ud == NULL)
    luaL_error(L, "Error in " LUA_QS ": invalid file handle.", "io.unlock");
  hnd = fileno((*((FILE **)ud)));
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1) {  /* lock entire file (in Windows lock 2^63 bytes only) */
    start = 0;
    size = 0;
  } else {
    /* unlock from current file position */
    start = my_fpos(hnd);
    size = agnL_checknumber(L, 2);
    if (size < 0) luaL_error(L, "Error in " LUA_QS ": must unlock at least one byte.", "io.unlock");
  }
  lua_pushboolean(L, my_unlock(hnd, start, size) == 0);
  return 1;
}


static int io_fileno (lua_State *L) {  /* 1.9.3 */
  int filenr;
  FILE *f = tofile(L);
  filenr = fileno(f);
  if (filenr == -1)
    lua_pushfail(L);
  else
    lua_pushinteger(L, filenr);
  return 1;
}


/* creates a new file denoted by its first argument (a string) and writes all of the given strings or numbers
   starting with the second argument in binary mode to it. To write other values, use `tostring` or
   `strings.format`. After writing all data, the function automatically closes the new file.

   By default, no character is inserted between neighbouring strings. This may be changed by passing the
   option 'delim':<str> (i.e. a pair, e.g. 'delim':'|') as the last argument to the function with <str>
   being a string of any length.

   The function returns the total number of bytes written, and issues an error otherwise.
   */
static int io_writefile (lua_State *L) {  /* 1.11.3 */
  int nargs, status, arg;
  size_t bytes, sizedelim;
  FILE *pf;
  char *delim;
  const char *filename;
  bytes = 0;  /* chars to be written */
  sizedelim = 0;  /* size of the delimiter */
  status = 1;
  arg = 2;  /* start with writing argument #2 */
  filename = NULL;
  pf = aux_gethandle(L, &filename, "wb", "io.writefile");  /* 2.2.0 */
  delim = NULL;  /* to avoid compiler warnings */
  nargs = lua_gettop(L);
  if (nargs < 2) luaL_error(L, "Error in " LUA_QS ": must get at least two arguments.", "io.writefile");
  if (nargs > 2 && lua_ispair(L, nargs)) {
    luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.2 fix */
    agn_pairgeti(L, nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, nargs, 2);  /* get right value, set to stack index  -1 */
    if (tools_strneq("delim", agn_checkstring(L, -2)))  /* 2.16.12 tweak */
      luaL_error(L, "Error in " LUA_QS ": unknown option.", "io.writefile");
    else {
      const char *str = agn_checklstring(L, -1, &sizedelim);
      delim = tools_strndup(str, sizedelim);  /* 2.27.0 optimisation */
      /* sizedelim = tools_strlen(delim); */ /* 2.17.8 tweak */
    }
    agn_poptoptwo(L);
  }
  clearerr(pf);
  nargs = nargs - 1 - (delim != NULL);  /* subtract 1 because if an option is given, it is still on the stack */
  for (; nargs--; arg++) {
    size_t l;
    const char *s = luaL_checklstring(L, arg, &l);
    status = status && (fwrite(s, CHARSIZE, l, pf) == l);
    bytes += l*CHARSIZE;
    if (delim != NULL && nargs != 0) {  /* do not print delim after last argument */
      fwrite(delim, CHARSIZE, sizedelim, pf);
      bytes += sizedelim*CHARSIZE;
    }
  }
  if (delim != NULL) xfree(delim);
  if (ferror(pf) || status == 0) {
    luaL_error(L, "Error in " LUA_QS ": file %s could possibly not be written.",
      "io.writefile", filename == NULL ? "\b" : filename);
  }
  if (filename != NULL && fclose(pf) != 0)
    luaL_error(L, "Error in " LUA_QS ": file %s could not be closed.", "io.writefile", filename);
  lua_pushnumber(L, bytes);
  return 1;
}


#ifdef _WIN32
/* 1.19.2: The following is based on code written by banders7, published at
   http://www.daniweb.com/software-development/c/code/217173/transfer-data-tofrom-the-windows-clipboard

   Copy input to Windows clipboard
   Data lines must be terminated by the CR LF pair (0xD,0xA)
   data in: line1CRLFline2CRLFline3CRLF --- Caller must format
   "this is a line\n" is not acceptable (alex: this does not seem to bother Windows 2000 SP4,
   "this is a line\r\n" is acceptable.
   If clipboard data shows square empty boxes at line ends in Windows,
   it is because lines are terminated by \n only.
   7.5.15/17 fixes by Gemini AI to resolve a use-after-free vulnerability and a heap corruption issue.
   Partial rollback to 7.5.14 because of issues with the empty string, but no stable solution, yet. */

static void aux_checkclip (lua_State *L, int pos, int *nargs, int *maxretries, int *waitms, const char *procname) {
  int checkoptions;
  *maxretries = 5;  /* max number of attempts to successfully get access to the clipboard */
  *waitms = 25;     /* wait time in milliseconds between retries */
  /* check for options, here `map in-place` */
  checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 fix */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "retries")) {
        *maxretries = agn_checknonnegint(L, -1);
      } else if (tools_streq(option, "wait")) {
        *waitms = agn_checknonnegint(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
}

static int io_putclip (lua_State *L) {
  char *toclipdata;
  size_t bytes;
  HGLOBAL clipbuffer;
  char *buffer;
  int en, maxretries, waitms, retries = 0, nargs = lua_gettop(L);
  aux_checkclip(L, 2, &nargs, &maxretries, &waitms, "io.putclip");  /* 7.6.2 */
  toclipdata = (char *)agn_checklstring(L, 1, &bytes);
  /* Retry loop to handle clipboard contentio, 7.6.2 */
  while (!OpenClipboard(NULL)) {
    if (++retries > maxretries) {
      lua_pushfail(L);
      lua_pushstring(L, "could not open clipboard");
      return 2;
    }
    Sleep(waitms);  /* wait 50ms before retrying */
  }
  EmptyClipboard();
  /* allocate memory for the string + null terminator */
  clipbuffer = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, bytes + 1);
  if (clipbuffer == NULL) {
    en = GetLastError();
    CloseClipboard();
    lua_pushfail(L);
    lua_pushstring(L, my_ioerror(en));
    return 2;
  }
  buffer = (char *)GlobalLock(clipbuffer);
  if (buffer == NULL) {
    en = GetLastError();
    GlobalFree(clipbuffer);
    CloseClipboard();
    lua_pushfail(L);
    lua_pushstring(L, my_ioerror(en));
    return 2;
  }
  /* Copy string including null terminator */
  memcpy(buffer, toclipdata, bytes + 1);
  GlobalUnlock(clipbuffer);
  /* Transfer ownership to the clipboard */
  if (SetClipboardData(CF_TEXT, clipbuffer) == NULL) {
    en = GetLastError();
    GlobalFree(clipbuffer);
    CloseClipboard();
    lua_pushfail(L);
    lua_pushstring(L, my_ioerror(en));
    return 2;
  }
  /* DO NOT call GlobalFree here; the system owns clipbuffer now */
  CloseClipboard();
  agn_pushboolean(L, 1);
  return 1;
}


static int io_getclip (lua_State *L) {  /* tweaked 2.3.0 RC 2 */
  char *buffer, *data;
  HANDLE hData;
  size_t l;
  int en, maxretries, waitms, retries = 0, nargs = lua_gettop(L);
  /* open the clipboard */
  aux_checkclip(L, 1, &nargs, &maxretries, &waitms, "io.getclip");  /* 7.6.2 */
  while (!OpenClipboard(NULL)) {
    if (++retries > maxretries) {
      en = GetLastError();
      lua_pushfail(L);
      lua_pushstring(L, my_ioerror(en));
      return 2;
    }
    Sleep(waitms);  /* wait 50ms before retrying */
  }
  hData = GetClipboardData(CF_TEXT);
  en = GetLastError();
  if (hData == NULL) {
    CloseClipboard();  /* 7.6.2 fix */
    lua_pushfail(L);
    lua_pushstring(L, my_ioerror(en));
    return 2;
  }
  buffer = (char*)GlobalLock(hData);
  if (buffer == NULL) {   /* return an error message */
    CloseClipboard();     /* 7.6.2 fix */
    GlobalUnlock(hData);  /* 7.6.2 fix */
    lua_pushfail(L);
    lua_pushstring(L, my_ioerror(en));  /* changed 2.10.4 */
    return 2;
  }
  /* return pointer to retrieved data */
  l = tools_strlen(buffer);
  data = tools_stralloc(l); // (char *)malloc((l + 1)*sizeof(char));  //tools_stralloc(l);  /* 2.16.5 change, 2.17.8 tweak */
  if (data == NULL) {
    CloseClipboard();     /* 7.6.2 fix */
    GlobalUnlock(hData);  /* 7.6.2 fix */
    buffer = NULL;
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "io.getclip");
  }
  memcpy(data, buffer, l + 1);  /* 7.6.2 change */
  GlobalUnlock(hData);  /* 7.6.2 fix */
  CloseClipboard();     /* 7.6.2 fix */
  lua_pushstring(L, data);
  xfree(data);
  return 1;
}
#endif


/* Truncates an open file denoted by its handle fh at the current file position. The function returns true on
   success and issues an error otherwise. You may query and change the current file position by calling io.seek. */
static int io_truncate (lua_State *L) {
  int en;
  int64_t pos, eof;  /* 2.2.1 fix */
  FILE *f = io_aux_checkfile(L, "io.truncate", 0);
  pos = _ftelli64(f);  /* 2.2.1 fix */
  if (pos == -1)  /* there is no errno for ftell */
    luaL_error(L, "Error in " LUA_QS ": could not determine current file position.", "io.truncate");
  if (_fseeki64(f, 0, SEEK_END) != 0)  /* 2.3.0 RC 2 extension */
    luaL_error(L, "Error in " LUA_QS ": could not determine length of file.", "io.truncate");
  eof = _ftelli64(f);
  if (eof == -1)  /* there is no errno for ftell */
    luaL_error(L, "Error in " LUA_QS ": could not determine length of file.", "io.truncate");
  if (eof == pos)
    lua_pushfalse(L);
  else {
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (ftruncate(fileno(f), pos) != 0) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS ": could not truncate file: %s.", "io.truncate", my_ioerror(en));  /* changed 2.10.4 */
    }
    else {
      if (_fseeki64(f, pos, SEEK_SET) != 0)  /* reset file position */
        luaL_error(L, "Error in " LUA_QS ": could not reset file position.", "io.truncate");
      lua_pushtrue(L);
    }
  }
  return 1;
}


/* Returns the size of an open file denoted by its file handle fd and returns the number of bytes as a non-negative integer */

static int io_filesize (lua_State *L) {  /* 60 % faster than using os.fstat; patched 2.37.5 for large files */
  int hnd = agn_tofileno(L, 1, 1);
  if (hnd == -1) {
    set_errno(0);  /* reset, better be sure than sorry, 4.3.0 */
    luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "io.filesize");  /* 2.37.5 */
  } else {
#ifdef _WIN32
    lua_pushnumber(L, _filelengthi64(hnd));  /* 2.37.5 fix */
    set_errno(0);  /* reset, better be sure than sorry, 4.3.0 */
#else
    int en, result;
    off64_t size, oldpos;
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    oldpos = lseek64(hnd, 0L, SEEK_CUR);  /* fstat* is slower */
    en = errno;
    if (oldpos == -1) {
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "io.filesize", hnd, my_ioerror(en));
    }
    size = lseek64(hnd, 0L, SEEK_END);
    en = errno;
    if (size == -1) {
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "io.filesize", hnd, my_ioerror(en));
    }
    result = lseek64(hnd, oldpos, SEEK_SET);  /* reset cursor to original file position */
    en = errno;
    if (result == -1) {
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s", "io.filesize", hnd, my_ioerror(en));
    } else
      lua_pushnumber(L, size);
#endif
  }
  return 1;
}


/* Moves the current file position of the open file denoted by its handle fh either to the left or the right.
   If n is a positive integer, then the file position is moved n characters to the right, if it is a
   negative integer, it is moved n characters to the left. If n is zero, the position is not changed at all.
   The function returns true on success and false otherwise. */

static int io_move (lua_State *L) {
  FILE *f = io_aux_checkfile(L, "io.move", 0);
  lua_pushboolean(L, _fseeki64(f, (int64_t)agnL_checknumber(L, 2), SEEK_CUR) == 0);  /* 2.2.1 fix, 2.3.0 RC 2 extension */
  set_errno(0);  /* reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's, 4.3.0 */
  return 1;
}


static int io_eof (lua_State *L) {  /* 2.2.0 u1, tuned by 25 percent 2.22.0 */
  FILE *f = io_aux_checkfile(L, "io.eof", 0);  /* 2.22.0: no length check any longer */
  lua_pushboolean(L, tools_eof(f));  /* returns fail if result == -1 */
  return 1;
}


static int io_rewind (lua_State *L) {  /* 2.2.0 u1 */
  FILE *f = io_aux_checkfile(L, "io.rewind", 1);
  int64_t op = _fseeki64(f, 0, SEEK_SET);  /* 2.3.0 RC 1 */
  if (op != 0) { /* 1.9.3 */
    int rc = pushresult(L, 0, NULL);  /* error */
    set_errno(0);  /* reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's, 4.3.0 */
    return rc;
  } else {
    lua_pushnumber(L, (lua_Number)_ftelli64(f));  /* 2.3.0 RC 2 */
    return 1;
  }
}


static int io_toend (lua_State *L) {  /* 2.2.0 u1 */
  FILE *f = io_aux_checkfile(L, "io.toend", 1);
  int op = _fseeki64(f, 0, SEEK_END);  /* 2.3.0 RC 2 */
  if (op != 0) {  /* 1.9.3 */
    int rc = pushresult(L, 0, NULL);  /* error */
    set_errno(0);  /* reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's, 4.3.0 */
    return rc;
  } else {
    lua_pushnumber(L, (lua_Number)_ftelli64(f));
    return 1;
  }
}


/* 2.8.6, taken from lua_sys package written by Nodir Temirkhodjaev, <nodir.temir@gmail.com>, modified by Alexander Walz
   checks whether the file denoted by its handle is associated with an open terminal device. Documented 5.5.15. */
static int io_isatty (lua_State *L) {
  FILE *f = io_aux_checkfile(L, "io.isatty", 1);
  /* alternatively in Windows: GetConsoleMode(f, &is_con), with LPDWORD is_con */
  lua_pushboolean(L, isatty(fileno(f)));
  return 1;
}


/* Source: https://batchloaf.wordpress.com/2012/04/17/simulating-a-keystroke-in-win32-c-or-c-using-sendinput/ */

static int io_keystroke (lua_State *L) {  /* 2.16.1 */
  int nrets;
  lua_Integer inkey;
  inkey = agn_checknonnegint(L, 1);  /* using agn_checkstring does not work, so get integral ASCII code instead */
  if (inkey < 1 || inkey > 254 || inkey == '\n' || inkey == '\r' || inkey == 26)  /* newline, CR, 26 = CTRL + Z to avoid abuse*/
    luaL_error(L, "Error in " LUA_QS ": unsupported ASCII code %d.", "io.keystroke", (int)inkey);
#if defined(_WIN32)
  INPUT ip;
  nrets = 0;
  ip.type = INPUT_KEYBOARD;
  ip.ki.wVk = 0;  /* Important: must be 0 for Unicode */
  ip.ki.wScan = (WORD)inkey;
  ip.ki.dwFlags = KEYEVENTF_UNICODE;
  ip.ki.time = 0;
  ip.ki.dwExtraInfo = 0;
  if (SendInput(1, &ip, sizeof(INPUT)) == 0) {  /* send press */
    lua_pushfail(L);
    lua_pushstring(L, "SendInput failed");
    return 2;
  }
  /* Send Release */
  ip.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
  SendInput(1, &ip, sizeof(INPUT));
  nrets = 0;
/* DO NOT DELETE: Does not work but demonstrates some OS/2 C API features:
#elif defined(__OS2__)
  APIRET rc;
  PVOID pMem;
  PKBDKEYINFO s_key;
  rc = DosAllocMem(&pMem, sizeof(KBDKEYINFO), PAG_COMMIT|OBJ_TILE|PAG_WRITE); // allocate memory
  if (rc != NO_ERROR)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "io.keystroke");
  s_key = (PKBDKEYINFO)pMem;
  memset(s_key, 0, sizeof(KBDKEYINFO));
  HKBD hd;
  KbdOpen(&hd);  // open virtual keyboard
  KbdGetFocus(IO_NOWAIT, hd);  // activate it
  rc = KbdCharIn(s_key, IO_NOWAIT, hd);
  KbdFreeFocus(hd);
  KbdClose(hd);  // close virtual keyboard
  lua_pushchar(L, s_key->chChar);
  lua_pushnumber(L, s_key->chChar);
  DosFreeMem(s_key);  // free memory
  nrets = 2; */
#else
  nrets = 1;
  lua_pushfail(L);
#endif
  return nrets;
}


/* OS/2 only: Get status information about the keyboard. The function returns a table with the
   contents of the KBDINFO structure after the call to the C API function KbdGetStatus.
   See http:://www.edm2.com/index.php/KbdSetStatus_(FAPI) for the meaning of the results. */
#if defined(__OS2__)
static int io_kbdgetstatus (lua_State *L) {  /* 2.16.1 */
  APIRET rc;
  HKBD hd;
  KBDINFO state;
  KbdOpen(&hd);  /* open virtual keyboard */
  state.cb = sizeof(KBDINFO);
  /* better sure than sorry: zero the following fields */
  state.fsMask = 0;
  state.chTurnAround = 0;
  state.fsInterim = 0;
  state.fsState = 0;
  /* see: www.edm2.com/index.php/KbdSetStatus_(FAPI) */
  KbdGetFocus(IO_NOWAIT, hd);  /* otherwise KbdGetStatus returns an error */
  rc = KbdGetStatus(&state, hd);
  if (rc != NO_ERROR)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "io.kbdgetstatus");
  KbdClose(hd);
  lua_createtable(L, 0, 5);
  lua_rawsetstringnumber(L, -1, "cb", (lua_Number)state.cb);
  lua_rawsetstringnumber(L, -1, "fsMask", (lua_Number)state.fsMask);
  lua_rawsetstringnumber(L, -1, "chTurnAround", (lua_Number)state.chTurnAround);
  lua_rawsetstringnumber(L, -1, "fsInterim", (lua_Number)state.fsInterim);
  lua_rawsetstringnumber(L, -1, "fsState", (lua_Number)state.fsState);
  return 1;
}
#endif


/* Returns the maximum number of open files, minus 2 for stdin & stdout, if no argument is given, or sets the maximum
   number of files that are allowed to be opened simultaneously if an integer is given. The function is not available
   on other platforms.
   See: https://stackoverflow.com/questions/3184345/fopen-problem-too-many-open-files?rq=1, by tommyk*/

/* Arguments: [number_of_files (number)]
 * Returns: number_of_files (number) */


/* sys_limit_nfiles taken from package "Lua System (LuaSys v1.8): System library for Lua", written by: Nodir Temirkhodjaev.
   There are no copyright remarks in Mr. Temirkhodjaev source files, accompanying files or his GIT site.
   See: https://github.com/tnodir/luasys
   UNIX, non-Windows version, adapted by awalz */
#if defined(__linux__) || defined(__SOLARIS) || defined(__APPLE__)
static int sys_limit_nfiles (int nargs) {
  struct rlimit rlim;
  if (nargs == 0) {  /* at least in Solaris, reading rlim_max returns a wrong result, use rlim_cur instead */
    rlim.rlim_cur = 0;
    return (getrlimit(RLIMIT_NOFILE, &rlim) == 0) ? rlim.rlim_cur : -1;
  }
  rlim.rlim_cur = rlim.rlim_max = nargs;
  return (setrlimit(RLIMIT_NOFILE, &rlim) == 0) ? nargs : -1;
}
#endif


static int io_maxopenfiles (lua_State *L) {  /* 2.16.10 */
  int nargs = lua_gettop(L);
  if (nargs == 0) {
#ifdef _WIN32
    lua_pushnumber(L, _getmaxstdio());
  } else {
    /* if (_setmaxstdio(agn_checkposint(L, 1)) == -1)
      luaL_error(L, "Error in " LUA_QS ": could not set default.", "io.maxopenfiles"); */
    int new_limit = (int)agn_checkposint(L, 1);
    int rc = _setmaxstdio(new_limit);
    if (rc == -1)
      luaL_error(L, "Error in " LUA_QS ": could not set default.", "io.maxopenfiles");
    lua_pushnumber(L, rc);
    return 1;
  }
#elif defined(__linux__) || defined(__SOLARIS) || defined(__APPLE__)
    int rc = sys_limit_nfiles(nargs);
    lua_pushnumber(L, (rc == -1) ? AGN_NAN : rc);
  } else {
    int rc = sys_limit_nfiles(agn_checkposint(L, 1));
    if (rc == -1)
      luaL_error(L, "Error in " LUA_QS ": could not change number of open files.", "io.maxopenfiles");
    lua_pushnumber(L, rc);
    return 1;
  }
#elif defined(__DJGPP__) || defined(__OS2__)
    int i;
    char *str;
    const char *const envnames[] = {"FILES", "FILESHIGH", NULL};
    for (i=0; envnames[i]; i++) {
      str = tools_getenv(envnames[i]);  /* 2.16.11 */
      if (str) {
        lua_Number r;
        lua_pushstring(L, str);
        r = lua_tonumber(L, -1);
        agn_poptop(L);
        lua_pushnumber(L, (r == 0) ? AGN_NAN : r);
        xfree(str);
        return 1;
      }
    }
    lua_pushundefined(L);
  } else
    luaL_error(L, "Error in " LUA_QS ": cannot change number of open files on this platform.", "io.maxopenfiles");
#else
  }  /* do nothing */
  lua_pushundefined(L);
  nargs = 0;
#endif
  return (nargs == 0);
}


/* The function creates a unique temporary filename from the given `template` which must always end with six `X`'s,
   and returns it as a string. It also creates a file of the same name, bit does not open it. You should prefer this function
   over `os.tmpname`. */
static int io_mkstemp (lua_State *L) {  /* 2.14.3, fixed 2.14.6 */
  int fd;
  size_t l;
  const char *str = agn_checklstring(L, 1, &l);
  if (l == 0 || l > 32)
    luaL_error(L, "Error in " LUA_QS ": argument must not be the empty string or is too long.", "io.mkstemp");
  char template[32 + 1];
  strcpy(template, str);  /* 2.14.6 fix */  /* for (i=0; i < l; i++) template[i] = str[i]; template[i] = '\0'; */
  fd = mkstemp(template);
  if (fd == -1)
    luaL_error(L, "Error in " LUA_QS ": unable to generate filename or template must end with `XXXXXX`.", "io.mkstemp");
  close(fd);
  lua_pushstring(L, template);
  return 1;
}


/* Retrieves the file status flags and returns an integer which you can mask with the flags listed below. The function
   is a port to C's `fcntl` function. 4.3.0; see also: https://perldoc.perl.org/Fcntl

  io.O_RDONLY  - file is in read-only mode
  io.O_WRONLY  - file is in write-only mode
  io.O_RDWR    - file is open for reading and writing
  io.O_ACCMODE - Bit mask for extracting the file access mode (read-only, write-only, or read/write) from the other flags
  io.O_APPEND  - file is in append mode (writing to the end of a file)
  io.O_TEXT    - file is in text mode
  io.O_BINARY  - file is binary mode

  The function is available on UNIX platforms only, including Mac OS X. */
#if defined(__unix__) || defined(__APPLE__) || defined(__OS2__)
static int io_fcntl (lua_State *L) {
  FILE *f = tofile(L);
  int fd = fileno(f);
  lua_pushinteger(L, fcntl(fd, F_GETFL));
  return 1;
}
#endif


static const luaL_Reg flib[] = {
  {"close",      io_close},       /* 4.3.0 */
  {"eof",        io_eof},         /* 4.3.0 */
  {"ferror",     io_ferror},      /* 6.1.2 */
  {"fileno",     io_fileno},      /* 6.1.2 */
  {"filepos",    io_filepos},     /* 4.3.0 */
  {"filesize",   io_filesize},    /* 6.1.2 */
  {"isatty",     io_isatty},      /* 6.1.2 */
  {"isfdesc",    io_isfdesc},     /* 6.1.2 */
  {"isopen",     io_isopen},      /* 6.1.2 */
  {"lines",      f_lines},        /* 4.3.0 */
  {"lock",       io_lock},        /* 4.3.0 */
  {"move",       io_move},        /* 4.3.2 */
  {"nlines",     io_nlines},      /* 6.1.2 */
  {"pclose",     io_pclose},      /* 6.1.2 */
  {"read",       f_read},         /* 4.3.0 */
  {"readfile",   io_readfile},    /* 6.1.2 */
  {"readfrom",   io_readfrom},    /* 7.8.11 */
  {"readlines",  io_readlines},   /* 6.1.2 */
  {"readto",     io_readto},      /* 7.8.12 */
  {"rewind",     io_rewind},      /* 4.3.0 */
  {"seek",       io_seek},        /* 4.3.0 */
  {"setvbuf",    io_setvbuf},     /* 4.3.0 */
  {"skiplines",  io_skiplines},   /* 4.3.0 */
  {"sync",       f_flush},        /* 4.3.0 */
  {"toend",      io_toend},       /* 4.3.0 */
  {"truncate",   io_truncate},    /* 6.1.2 */
  {"unlock",     io_unlock},      /* 4.3.0 */
  {"write",      f_write},        /* 4.3.0 */
  {"writefile",  io_writefile},   /* 6.1.2 */
  {"writeline",  f_writeline},    /* 4.3.0 */
  {"__gc",       io_gc},
  {"__tostring", io_tostring},
  {NULL, NULL}
};

static const luaL_Reg iolib[] = {
  {"anykey", io_anykey},
  {"clearerror", io_clearerror}, /* 2.22.0, September 30, 2020 */
  {"close", io_close},
  {"eof", io_eof},               /* 2.2.0 RC 5, June 14, 2014 */
#if defined(__unix__) || defined(__APPLE__) || defined(__OS2__)
  {"fcntl", io_fcntl},           /* 4.3.0, October 02, 2024 */
#endif
  {"ferror", io_ferror},         /* 2.22.0, September 30, 2020 */
  {"fileno", io_fileno},         /* 1.9.3, February 19, 2013 */
  {"filepos", io_filepos},       /* 2.2.1, June 20, 2014 */
  {"filesize", io_filesize},     /* 2.0.0, November 29, 2013 */
#ifdef _WIN32
  {"getclip", io_getclip},       /* 1.12.9, November 01, 2013 */
#endif
#if defined(_WIN32) || defined(__unix__) || defined(__OS2__) || defined(__APPLE__)
  {"getkey", io_getkey},         /* added on June 29, 2007 */
#endif
  {"infile", io_infile},         /* 1.11.6, June 02, 2013 */
  {"input", io_input},
  {"isatty", io_isatty},         /* 2.8.6, September 23, 2015 */
  {"isfdesc", io_isfdesc},       /* 0.32.5, June 12, 2010 */
  {"isopen", io_isopen},         /* 1.12.1, June 14, 2013 */
#if defined(__OS2__)
  {"kbdgetstatus", io_kbdgetstatus},  /* added 2.16.1, August 26, 2019 */
#endif
  {"keystroke", io_keystroke},        /* added 2.16.1, August 22, 2019 */
  {"lines", io_lines},
  {"lock", io_lock},             /* 0.32.5, June 12, 2010 */
  {"maxopenfiles", io_maxopenfiles},  /* 2.16.10, November 25, 2019 */
  {"mkstemp", io_mkstemp},       /* added December 22, 2018 */
  {"move", io_move},             /* 2.0.0, November 29, 2013 */
  {"nlines", io_nlines},         /* 0.31.7, April 22, 2010 */
  {"open", io_open},
  {"output", io_output},
  {"pclose", io_pclose},         /* 3.17.3 */
  {"popen", io_popen},
#ifdef _WIN32
  {"putclip", io_putclip},       /* 1.12.9, November 01, 2013 */
#endif
  {"read", io_read},
  {"readfile", io_readfile},     /* 1.10.2, March 19, 2013 */
  {"readfrom", io_readfrom},     /* 7.8.11, August 21, 2026 */
  {"readlines", io_readlines},   /* 0.10.0, April 27, 2008 */
  {"readto", io_readto},         /* 7.8.12, August 22, 2026 */
  {"rewind", io_rewind},         /* 2.2.1, June 19, 2014 */
  {"seek", io_seek},
  {"setvbuf", io_setvbuf},
  {"skiplines", io_skiplines},   /* 1.10.1, March 12, 2013 */
  {"sync", io_sync},
  {"tmpfile", io_tmpfile},
  {"toend", io_toend},           /* 2.2.1, June 19, 2014 */
  {"truncate", io_truncate},     /* 2.0.0 RC 5 */
  {"unlock", io_unlock},         /* 0.32.5, June 12, 2010 */
  {"write", io_write},
  {"writefile", io_writefile},   /* 1.11.3, May 08, 2013 */
  {"writeline", io_writeline},   /* May 08, 2007 - added 0.5.1 */
  {NULL, NULL}
};


static void createmeta (lua_State *L) {
  luaL_newmetatable(L, LUA_FILEHANDLE);  /* create metatable for file handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, flib);  /* file methods */
}

static void createstdfile (lua_State *L, FILE *f, int k, const char *fname) {
  *newfile(L) = f;
  if (k > 0) {
    lua_pushvalue(L, -1);
    lua_rawseti(L, LUA_ENVIRONINDEX, k);
  }
  lua_setfield(L, -2, fname);
}

LUALIB_API int luaopen_io (lua_State *L) {
  luaL_newmetatable(L, "io_vecint");
  luaL_register(L, NULL, vecint_arraylib);  /* associate __gc method */
  createmeta(L);
  /* create (private) environment (with fields IO_INPUT, IO_OUTPUT, __close) */
  lua_createtable(L, 2, 1);
  lua_replace(L, LUA_ENVIRONINDEX);
  /* open library */
  luaL_register(L, LUA_IOLIBNAME, iolib);
  /* table for information on all open files */
  lua_newtable(L);
  lua_setfield(L, -2, "openfiles");  /* table for information on all open files */
  /* create (and set) default files */
  createstdfile(L, stdin, IO_INPUT, "stdin");
  createstdfile(L, stdout, IO_OUTPUT, "stdout");
  createstdfile(L, stderr, 0, "stderr");
  /* create environment for 'popen' */
  lua_getfield(L, -1, "popen");
  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, io_pclose);
  lua_setfield(L, -2, "__close");
  lua_setfenv(L, -2);
  agn_poptop(L);  /* pop 'popen' */
  /* set default close function */
  lua_pushcfunction(L, io_fclose);
  lua_setfield(L, LUA_ENVIRONINDEX, "__close");
  /* fnctl flags, 4.3.0 */
  lua_pushinteger(L, O_RDONLY);
  lua_setfield(L, -2, "O_RDONLY");
  lua_pushinteger(L, O_WRONLY);
  lua_setfield(L, -2, "O_WRONLY");
  lua_pushinteger(L, O_RDWR);
  lua_setfield(L, -2, "O_RDWR");
  lua_pushinteger(L, O_ACCMODE);
  lua_setfield(L, -2, "O_ACCMODE");
  lua_pushinteger(L, O_APPEND);
  lua_setfield(L, -2, "O_APPEND");
#ifdef _WIN32
  lua_pushinteger(L, O_TEXT);
  lua_setfield(L, -2, "O_TEXT");
  lua_pushinteger(L, O_BINARY);
  lua_setfield(L, -2, "O_BINARY");
#endif
  return 1;
}

