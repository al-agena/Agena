#define iconv_c
#define LUA_LIB

/*
 *  Lua-iconv 7 - Performs character set conversions in Lua
 * (c) 2005-11 Alexandre Erwin Ittner <alexandre@ittner.com.br>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * If you use this package in a product, an acknowledgment in the product
 * documentation would be greatly appreciated (but it is not required).
 *
 * `iconv` is an abbreviation for `internationalization conversion`.
 * Agena port by alex walz
 */

#include <errno.h>
#include <iconv.h>
#include <stdlib.h>
#include <string.h>  /* for strdup */

#include "agena.h"

#include "agenalib.h"
#include "agnxlib.h"

#define LIB_NAME        "iconv_t"
#define LIB_VERSION     LIB_NAME " 7"
#define ICONV_TYPENAME  "iconv"  /* do not change */

#define BOXPTR(L, p)   (*(void**)(lua_newuserdata(L, sizeof(void*))) = (p))
#define UNBOXPTR(L, i) (*(void**)(lua_touserdata(L, i)))

/* Set a integer constant. Assumes a table is on the stack top */
#define TBL_SET_INT_CONST(L, c) {   \
    lua_pushliteral(L, #c);         \
    lua_pushnumber(L, c);           \
    lua_settable(L, -3);            \
}

#define ERROR_NO_MEMORY     1
#define ERROR_INVALID       2
#define ERROR_INCOMPLETE    3
#define ERROR_UNKNOWN       4
#define ERROR_FINALIZED     5

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_ICONVLIBNAME ICONV_TYPENAME
LUALIB_API int (luaopen_iconv) (lua_State *L);
#endif


static void push_iconv_t (lua_State *L, iconv_t cd) {
  BOXPTR(L, cd);
  luaL_getmetatable(L, ICONV_TYPENAME);
  lua_setmetatable(L, -2);
  agn_setutypestring(L, -1, ICONV_TYPENAME);
}


static iconv_t get_iconv_t (lua_State *L, int i) {
  int rc;
  luaL_getudata(L, i, ICONV_TYPENAME, &rc);  /* 4.4.1 */
  if (rc) {
    iconv_t cd = UNBOXPTR(L, i);
    return cd;  /* May be NULL. This must be checked by the caller. */
  }
  if (lua_isuserdata(L, i)) {
    luaL_argerror(L, i, lua_pushfstring(L, ICONV_TYPENAME " handle is invalid and may have been closed"));
  } else {
    luaL_argerror(L, i, lua_pushfstring(L, ICONV_TYPENAME " expected, got %s", luaL_typename(L, i)));
  }
  return NULL;
}


static int Liconv_open (lua_State *L) {
  /* order of arguments changed in Agena to `from` -> `to` */
  const char *fromcode, *tocode;
  char *fromstr, *tostr;
  size_t i, l_from, l_to;
  fromcode = luaL_checklstring(L, 1, &l_from);
  tocode = luaL_checklstring(L, 2, &l_to);
  if (fromcode == NULL || tocode == NULL) {
    luaL_error(L, "Error in `iconv.open`: empty string not allowed.");
  }
  fromstr = tools_strndup(fromcode, l_from);  /* 2.27.0 change */
  tostr = tools_strndup(tocode, l_to);  /* 2.27.0 change */
  if (fromstr == NULL || tostr == NULL) {
    /* It is safe to free a null pointer. The C Standard specifies that free(NULL) has no effect: */
    xfree(fromstr);
    xfree(tostr);
    luaL_error(L, "Error in `iconv.open`: memory allocation failed.");
  }
  /* capitalise arguments, new 2.26.1 */
  for (i=0; i < l_from; i++) fromstr[i] = toupper(fromstr[i]);
  for (i=0; i < l_to; i++) tostr[i] = toupper(tostr[i]);
  iconv_t cd = iconv_open(tostr, fromstr);
  xfree(fromstr); xfree(tostr);
  if (cd != (iconv_t)(-1))
    push_iconv_t(L, cd);  /* ok */
  else
    luaL_error(L, "Error in `iconv.open`: invalid arguments `%s` or `%s`.`", fromcode, tocode);  /* error */
  return 1;
}

/* Use a fixed-size buffer in the stack to avoid a lot of small mallocs and prevent memory fragmentation.
 * This should not be a problem in any contemporary general purpose system but, if you are running in a very
 * limited stack system you may use a smaller buffer, but the luaL_Buffer will compensate this with more
 * reallocs and memcpys.
 */
#define CONV_BUF_SIZE 256

static int Liconv_convert (lua_State *L) {
  luaL_Buffer b;
  size_t ibleft;
  iconv_t cd = get_iconv_t(L, 1);
#if defined(__SOLARIS) || defined(__OS2__)  /* 2.26.2 fix for OS/2 */
  const char *inbuf = luaL_checklstring(L, 2, &ibleft);
#else
  char *inbuf = (char*)luaL_checklstring(L, 2, &ibleft);
#endif
  char outbufs[CONV_BUF_SIZE];
  char *outbuf = outbufs;
  size_t obleft = CONV_BUF_SIZE;
  size_t ret = -1;
  if (cd == NULL) {
    lua_pushnil(L);  /* changed 2.26.1, lua_pushstring(L, ""); */
    lua_pushnumber(L, ERROR_FINALIZED);
    return 2;
  }
  luaL_buffinit(L, &b);
  do {
    ret = iconv(cd, &inbuf, &ibleft, &outbuf, &obleft);
    if (ret == (size_t)(-1)) {
      luaL_addlstring(&b, outbufs, CONV_BUF_SIZE - obleft);
      if (errno == EILSEQ) {
        luaL_pushresult(&b);
        lua_pushnumber(L, ERROR_INVALID);
        return 2;   /* Invalid character sequence */
      } else if (errno == EINVAL) {
        luaL_pushresult(&b);
        lua_pushnumber(L, ERROR_INCOMPLETE);
        return 2;   /* Incomplete character sequence */
      } else if (errno == E2BIG) {
        obleft = CONV_BUF_SIZE;
        outbuf = outbufs;
      } else {
        luaL_pushresult(&b);
        lua_pushnumber(L, ERROR_UNKNOWN);
        return 2; /* Unknown error */
      }
    }
  } while (ret == (size_t)(-1));
  luaL_addlstring(&b, outbufs, CONV_BUF_SIZE - obleft);
  luaL_pushresult(&b);
  return 1;   /* Done */
}


/* a GNU extension */
static int push_one (unsigned int cnt, const char * const *names, void *data) {
  int i, n;
  lua_State *L = (lua_State*) data;
  n = (int)lua_tonumber(L, -1);
  /* Stack: <tbl> n */
  agn_poptop(L);
  for (i = 0; i < cnt; i++) {
    /* Stack> <tbl> */
    lua_pushnumber(L, n++);
    lua_pushstring(L, names[i]);
    /* Stack: <tbl> n <str> */
    lua_settable(L, -3);
  }
  lua_pushnumber(L, n);
  /* Stack: <tbl> n */
  return 0;
}

static int Liconv_list (lua_State *L) {
  luaL_checkstack(L, 3, "");  /* 2.31.6/7 fix */
  lua_newtable(L);
  lua_pushnumber(L, 1);
  /* Stack:   <tbl> 1 */
  iconvlist(push_one, (void*)L);
  /* Stack:   <tbl> n */
  agn_poptop(L);
  /* Agena extension */
  lua_getglobal(L, "sort");
  if (!lua_isfunction(L, -1))
    luaL_error(L, "Error in " LUA_QS ": could not fetch internal sorting function.", "iconv.list");
  lua_pushvalue(L, -2);
  lua_call(L, 1, 0);
  return 1;  /* return table */
}
/* of GNU extension */


static int Liconv_close (lua_State *L) {
  int i, n;
  n = lua_gettop(L);
  luaL_checkstack(L, n, "too many arguments");
  for (i=1; i <= n; i++) {
    iconv_t cd = get_iconv_t(L, i);
    if (cd != NULL && iconv_close(cd) == 0) {
      /* Mark the pointer as freed, preventing interpreter crashes if the user forces __gc to be called twice. */
      void **ptr = lua_touserdata(L, i);
      lua_setmetatabletoobject(L, i, NULL, 1);  /* 2.31.7 fix */
      *ptr = NULL;
      lua_pushtrue(L);    /* ok */
    } else
      lua_pushfalse(L);   /* error */
  }
  return n;
}


static int mt_iconvgc (lua_State *L) {  /* 2.31.7: new gc method, do not just call Liconv_close */
  iconv_t cd = get_iconv_t(L, 1);
  if (cd != NULL && iconv_close(cd) == 0) {
    /* Mark the pointer as freed, preventing interpreter crashes if the user forces __gc to be called twice. */
    void **ptr = lua_touserdata(L, 1);
    lua_setmetatabletoobject(L, 1, NULL, 1);  /* 2.31.7 fix */
    *ptr = NULL;
  }
  return 0;
}


static int mt_tostring (lua_State *L) {  /* 2.31.7, at the console, the handle is formatted as follows: */
  iconv_t cd = get_iconv_t(L, 1);
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%p)", cd);
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid iconv handle.", "iconv.__tostring");
  return 1;
}


static const struct luaL_Reg mt_iconvlib[] = {
  {"close",      Liconv_close},
  {"convert",    Liconv_convert},
  {"__gc",       mt_iconvgc},
  {"__tostring", mt_tostring},
  {NULL, NULL}
};


static const luaL_Reg iconvlib[] = {
  {"open",    Liconv_open},
  {"close",   Liconv_close},
  {"convert", Liconv_convert},
  {"list",    Liconv_list},
  { NULL, NULL }
};


/*
** Open iconv library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, ICONV_TYPENAME);  /* create metatable */
  lua_pushvalue(L, -1);                  /* push metatable */
  lua_setfield(L, -2, "__index");        /* metatable.__index = metatable */
  luaL_register(L, NULL, mt_iconvlib);   /* methods */
}

int luaopen_iconv (lua_State *L) {
  /* metamethods */
  createmeta(L);
  lua_pop(L, 1);  /* 7.11.2 fix */
  /* register library */
  luaL_register(L, AGENA_ICONVLIBNAME, iconvlib);
  TBL_SET_INT_CONST(L, ERROR_NO_MEMORY);
  TBL_SET_INT_CONST(L, ERROR_INVALID);
  TBL_SET_INT_CONST(L, ERROR_INCOMPLETE);
  TBL_SET_INT_CONST(L, ERROR_FINALIZED);
  TBL_SET_INT_CONST(L, ERROR_UNKNOWN);
  lua_pushliteral(L, "VERSION");
  lua_pushstring(L, LIB_VERSION);
  lua_settable(L, -3);
  return 1;
}

