/*
** $Id: skycrane.c, initiated January 31, 2013 $
** Skycrane utilities library
** See Copyright Notice in agena.h
*/


#include <stdlib.h>

#define skycrane_c
#define LUA_LIB

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"
#include "agnhlps.h"
#include "agnconf.h"
#include "agncmpt.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_SKYCRANELIBNAME "skycrane"
LUALIB_API int (luaopen_skycrane) (lua_State *L);
#endif


static int skycrane_tocomma (lua_State *L) {  /* rewritten 2.37.1, slightly faster */
  char *newstr;
  const char *str;
  size_t l;
  int t = lua_type(L, 1);
  luaL_argcheck(L, t == LUA_TNUMBER || t == LUA_TSTRING, 1, "number or string expected");
  str = lua_tolstring(L, 1, &l);
  if (t == LUA_TSTRING && !tools_isnumericstring(str, 0))
    luaL_error(L, "Error in " LUA_QS ": expected a numeric string.", "skycrane.tocomma");
  newstr = tools_strndup(str, l);
  if (!newstr)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "skycrane.tocomma");
  str_charreplace(newstr, '.', ',', 0);
  lua_pushlstring(L, newstr, l);
  xfree(newstr);
  return 1;
}


static int skycrane_trimpath (lua_State *L) {
  char *newstr;
  size_t l;
  const char *str = agn_checklstring(L, 1, &l);
  newstr = tools_strndup(str, l);  /* 2.37.1, patched for the former implementation changed the original string */
  if (!newstr)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "skycrane.trimpath");
  str_charreplace(newstr, '\\', '/', 1);
  lua_pushstring(L, newstr);
  xfree(newstr);
  return 1;
}


/* Returns math.branch(ceil(a * log10(x + 1) - 1)), a maximum tolerance value especially suited for
   comparing similar strings and estimating similarities. A good value for a might be a number greater than 3. */
static int skycrane_tolerance (lua_State *L) {  /* 2.10.0, added on September 25, 2016 */
  lua_Number x, a;
  x = agn_checknumber(L, 1);
  a = agn_checknumber(L, 2);
  lua_pushnumber(L, tools_branch(sun_ceil(a*(sun_log10(x + 1) - 1)), 1));
  return 1;
}


/* Takes a table, sequence or register obj and counts the number of occurrences of each of its values.
   The return is a table with the key the respective value in obj and the value the associated count. Example:

   > import skycrane

   1 is included 4 times, 2 is included twice, 3 only once, etc.:

   > skycrane.obcount([1, 1, 1, 2, 2, 1, 3, 8, 9, 8, 9]):
   [1 ~ 4, 2 ~ 2, 3 ~ 1, 8 ~ 2, 9 ~ 2] */
static int skycrane_obcount (lua_State *L) {  /* based on bags.minclude, 3.13.2 */
  int i;
  luaL_checkstack(L, 4, "not enough stack space");
  lua_createtable(L, 0, 0);
  switch (lua_type(L, 1)) {
    case LUA_TTABLE: {
      lua_pushnil(L);
      while (lua_next(L, 1)) {
        lua_pushvalue(L, -1);  /* duplicate value */
        lua_gettable(L, -4);    /* drops duplicated value */
        if (lua_isnil(L, -1)) {  /* element not yet in bag ? */
          agn_poptop(L);  /* pop null */
          lua_pushinteger(L, 1);
          lua_settable(L, -4);  /* drops value and counter, leaves key for next iteration */
        } else {
          int c = agn_tointeger(L, -1);
          agn_poptop(L);  /* drop number */
          lua_pushinteger(L, c + 1);
          lua_settable(L, -4);  /* pops value and counter, leaves key for next iteration */
        }
      }
      break;
    }
    case LUA_TSEQ: {
      for (i=1; i <= agn_seqsize(L, 1); i++) {
        lua_seqrawgeti(L, 1, i);
        lua_gettable(L, -2);
        if (lua_isnil(L, -1)) {  /* element not yet in bag ? */
          agn_poptop(L);  /* pop null */
          lua_seqrawgeti(L, 1, i);
          lua_pushinteger(L, 1);
          lua_settable(L, -3);
        } else {
          lua_seqrawgeti(L, 1, i);
          lua_pushinteger(L, agn_tointeger(L, -2) + 1);
          lua_settable(L, -4);
          agn_poptop(L);  /* drop number */
        }
      }
      break;
    }
    case LUA_TREG: {
      for (i=1; i <= agn_regsize(L, 1); i++) {
        agn_regrawgeti(L, 1, i);
        lua_gettable(L, -2);
        if (lua_isnil(L, -1)) {  /* element not yet in bag ? */
          agn_poptop(L);  /* pop null */
          agn_regrawgeti(L, 1, i);
          lua_pushinteger(L, 1);
          lua_settable(L, -3);
        } else {
          agn_regrawgeti(L, 1, i);
          lua_pushinteger(L, agn_tointeger(L, -2) + 1);
          lua_settable(L, -4);
          agn_poptop(L);  /* drop number */
        }
      }
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.", "skycrane.obcount",
        luaL_typename(L, 1));
  }
  return 1;
}


static const luaL_Reg skycranelib[] = {
  {"obcount", skycrane_obcount},       /* added on April 12, 2024 */
  {"tocomma", skycrane_tocomma},       /* added on January 31, 2013 */
  {"tolerance", skycrane_tolerance},   /* added on September 25, 2016 */
  {"trimpath", skycrane_trimpath},     /* added on February 01, 2013 */
  {NULL, NULL}
};


/*
** Open skycrane library
*/
LUALIB_API int luaopen_skycrane (lua_State *L) {
  luaL_register(L, AGENA_SKYCRANELIBNAME, skycranelib);
  return 1;
}

