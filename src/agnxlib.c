/*
** $Id: agnxlib.c, 15.03.2009 12:37:08 $
** Auxiliary functions for building Agena libraries
** Revision 08.08.2009 22:14:11
** See Copyright Notice in agena.h
*/

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__OS2__) || defined(__APPLE__) || defined(__HAIKU__)
#include <unistd.h>   /* for access */
#endif

#if defined(__OS2__)
#define INCL_BASE  1
#include <os2emx.h>
#endif

#if defined(_WIN32)
#include <io.h>       /* for access */
#include <windows.h>
#endif

/* This file uses only the official API of Lua.
** Any function declared here could be written as an application function.
*/

#define agnxlib_c
#define LUA_LIB

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"
#include "agnhlps.h"
#include "charbuf.h"
#include "long.h"
#include "lstrlib.h"  /* for SPECIALS, match() */
#include "numarray.h"

#define FREELIST_REF  0  /* free list of references */

/* convert a stack index to positive */
#define abs_index(L, i)    ((i) > 0 || (i) <= LUA_REGISTRYINDEX ? (i) : \
          lua_gettop(L) + (i) + 1)

/*
** {======================================================
** Error-report functions
** =======================================================
*/


LUALIB_API int luaL_argerror (lua_State *L, int narg, const char *extramsg) {
  lua_Debug ar;  /* 2.20.2, changed back to original meaning since type indication does not fit here */
  if (!lua_getstack(L, 0, &ar))  /* no stack frame? */
    return luaL_error(L, "Wrong argument #%d: %s.", narg, extramsg);
  lua_getinfo(L, "n", &ar);
  if (tools_streq(ar.namewhat, "method")) {  /* 2.16.12 tweak */
    narg--;  /* do not count `self' */
    if (narg == 0)  /* error is in the self argument itself? */
      return luaL_error(L, "Calling " LUA_QS " on bad self (%s).", ar.name, extramsg);
  }
  if (ar.name == NULL) ar.name = "?";
  return luaL_error(L, "Wrong argument #%d to " LUA_QS ": %s.", narg, ar.name, extramsg);
}


LUALIB_API int luaL_typerror (lua_State *L, int narg, const char *tname) {
  const char *msg = lua_pushfstring(L, "%s expected, got %s",
                                    tname, luaL_typename(L, narg));
  return luaL_argerror(L, narg, msg);
}


LUALIB_API int luaL_typeerror (lua_State *L, int narg, const char *extramsg, int type) {
  lua_Debug ar;
  if (!lua_getstack(L, 0, &ar))  /* no stack frame? */
    return luaL_error(L, "Wrong argument #%d: %s.", narg, extramsg);
  lua_getinfo(L, "n", &ar);
  if (tools_streq(ar.namewhat, "method")) {  /* 2.16.12 tweak */
    narg--;  /* do not count `self' */
    if (narg == 0)  /* error is in the self argument itself? */
      return luaL_error(L, "Calling " LUA_QS " on bad self (%s).",
                           ar.name, extramsg);
  }
  if (ar.name == NULL) ar.name = "?";
  return luaL_error(L, "Wrong argument #%d to " LUA_QS ": %s, got %s.",
                        narg, ar.name, extramsg, lua_typename(L, type));
}


static int typeerror (lua_State *L, int narg, const char *tname) {  /* changed 2.3.0 RC 3 */
  const char *msg = lua_pushfstring(L, "%s expected, got %s", tname, luaL_typename(L, narg));
  agn_poptop(L);  /* 2.3.0 RC 4 fix */
  return luaL_argerror(L, narg, msg);
}


LUALIB_API void tag_error (lua_State *L, int narg, int tag) {
  typeerror(L, narg, lua_typename(L, tag));
}


LUALIB_API void tag_error2 (lua_State *L, int narg, const char *what) {  /* 4.3.0 */
  typeerror(L, narg, what);
}


LUALIB_API void luaL_where (lua_State *L, int level) {
  lua_Debug ar;
  if (lua_getstack(L, level, &ar)) {  /* check function at level */
    lua_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      /* lua_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline); */
      lua_pushfstring(L, "", ar.currentline);
      return;
    }
  }
  lua_pushliteral(L, "");  /* else, no information available... */
}


LUALIB_API int luaL_error (lua_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  luaL_where(L, 1);
  lua_pushvfstring(L, fmt, argp);
  va_end(argp);
  lua_concat(L, 2);
  return lua_error(L);
}


/* Takes one or more strings and pushes their concatenation onto the top of the stack The resulting string does not automatically
   have a newline at its end, so you may have to explicitly add it if you need it. The last argument must always be NULL !
   Not the fastest implementation, but quite neat. 3.4.4 */
LUALIB_API int agnL_pushvstring (lua_State *L, const char *str, ...) {
  va_list args;
  const char *s;
  int c = 1;
  luaL_checkstack(L, 1, "not enough stack space");
  lua_pushstring(L, str);
  va_start(args, str);
  while ((s = va_arg(args, char *))) {  /* starts with the first argument AFTER str ! */
    luaL_checkstack(L, 1, "not enough stack space");
    lua_pushstring(L, s);
    c++;
  }
  va_end(args);
  lua_concat(L, c);
  return 1;
}


/* }====================================================== */


LUALIB_API int luaL_checkoption (lua_State *L, int narg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? luaL_optstring(L, narg, def) :
                             luaL_checkstring(L, narg);
  int i;
  for (i=0; lst[i]; i++)
    if (tools_streq(lst[i], name))  /* 2.16.12 tweak */
      return i;
  return luaL_argerror(L, narg,
                       lua_pushfstring(L, "invalid option " LUA_QS, name));
}


LUALIB_API int luaL_checksetting (lua_State *L, int narg, const char *const lst[], const char *errmsg) {  /* 2.11.1 RC 2 */
  const char *name = luaL_checkstring(L, narg);
  int i;
  for (i=0; lst[i]; i++)
    if (tools_streq(lst[i], name))  /* 2.16.12 tweak */
      return i;
  return luaL_argerror(L, narg,
                       lua_pushfstring(L, "%s " LUA_QS, errmsg, name));
}


/* Like luaL_checkoption, but returns an error if a given option at index idx is not a string.
   Returns the index of the option found in lst, or issues an error if the option at idx is not part of lst[].
   def must not be NULL. If ignorecase is 1, the function compares option names case-insensitively,
   and case-sensitively if it is 0.

   Usage:
      static const char *const datatypes[] = {"uchar", "double", "int32", NULL};
      whichone = agnL_checkoption(L, 2, "double", datatypes, 0); */
LUALIB_API int agnL_checkoption (lua_State *L, int narg, const char *def,
                                 const char *const lst[], int ignorecase) {  /* 2.9.0 */
  int i;
  const char *name = agnL_optstring(L, narg, def);
  if (ignorecase) {
    for (i=0; lst[i]; i++) {
      if (tools_stricmp(lst[i], name) == 0)
        return i;
    }
  } else {
    for (i=0; lst[i]; i++) {
      if (tools_streq(lst[i], name))  /* 2.16.12 tweak */
        return i;
    }
  }
  return luaL_argerror(L, narg,
                       lua_pushfstring(L, "invalid option " LUA_QS, name));
}


/* For a given string in `modenames` at stack index `idx`, returns the respective integral representation
   in `mode`. If the string passed cannot be found in `modenames`, issues an error.

   Example:

   static const int mode[] = {OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_INTDIV, OP_MOD, OP_POW, OP_IPOW};
   static const char *const modenames[] = {"+", "-", "*", "/", "\\", "%", "^", "**", NULL};
   int op = agnL_getnumoption(L, 1, modenames, mode, "zip");

   2.21.1 */
LUALIB_API int agnL_getsetting (lua_State *L, int idx,
  const char *const *modenames, const int *mode, const char *procname) {
  int op = luaL_checksetting(L, idx, modenames, "invalid setting");
  return mode[op];
}


LUALIB_API int luaL_newmetatable (lua_State *L, const char *tname) {
  lua_getfield(L, LUA_REGISTRYINDEX, tname);  /* get registry.name */
  if (!lua_isnil(L, -1))  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  agn_poptop(L);
  lua_newtable(L);  /* create metatable */
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


/* sets the metatable and the user-defined type of the structure at index idx (positive) to the structure at the top
   of the stack. The function does not change the stack and pushes nothing. */
LUALIB_API void luaL_setmetatype (lua_State *L, int idx) {  /* 2.29.0/3.15.5 */
  /* set type of new structure to the one of the original one */
  if (agn_getutype(L, idx)) {
    agn_setutype(L, -2, -1);
    agn_poptop(L);  /* pop type (string) */
  }
  /* copy metatable */
  if (lua_getmetatable(L, idx))
    lua_setmetatable(L, -2);
}


LUALIB_API void *luaL_checkudata (lua_State *L, int ud, const char *tname) {
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    /* remove read-only status as otherwise the metatable cannot be read, causing crashes. 5.1.6 fix */
    int rostatus = agn_udfreeze(L, ud, -1);
    if (rostatus) agn_udfreeze(L, ud, 0);
    if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
      lua_getfield(L, LUA_REGISTRYINDEX, tname);  /* get correct metatable */
      if (rostatus) agn_udfreeze(L, ud, 1);  /* reset read-only status */
      if (lua_rawequal(L, -1, -2)) {  /* does it have the correct mt? */
        agn_poptoptwo(L);  /* remove both metatables */
        return p;
      }
    }
  }
  typeerror(L, ud, tname);  /* else error */
  return NULL;  /* to avoid warnings */
}


LUALIB_API void *luaL_getudata (lua_State *L, int ud, const char *tname, int *result) {  /* 0.12.2 */
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    /* remove read-only status as otherwise the metatable cannot be read, causing crashes. 5.1.6 fix */
    int rostatus = agn_udfreeze(L, ud, -1);
    if (rostatus) agn_udfreeze(L, ud, 0);
    if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
      lua_getfield(L, LUA_REGISTRYINDEX, tname);  /* get correct metatable */
      if (rostatus) agn_udfreeze(L, ud, 1);  /* reset read-only status */
      if (lua_rawequal(L, -1, -2)) {  /* does it have the correct mt? */
        agn_poptoptwo(L);  /* remove both metatables */
        *result = 1;
        return p;
      }
    }
  }
  *result = 0;
  return NULL;
}


/* Checks whether the object at stack position ud is a userdata object. If ud depicts the stack position of a userdata,
   the function also checks whether the metatable - if available - attached to it complies with metatable `tname` which
   the programmer originally intended to be used by the specific userdata.

   The function returns 1 if all checks have been successful, and 0 otherwise.

   See also: luaL_getudata. */

LUALIB_API int luaL_isudata (lua_State *L, int ud, const char *tname) {  /* 2.14.6 */
  int rc = 0;  /* 5.3.0 fix */
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    /* remove read-only status as otherwise the metatable cannot be read, causing crashes. 5.1.6 fix */
    int rostatus = agn_udfreeze(L, ud, -1);
    if (rostatus) agn_udfreeze(L, ud, 0);
    luaL_checkstack(L, 2, "not enough stack space");  /* 5.3.0 fix */
    if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
      lua_getfield(L, LUA_REGISTRYINDEX, tname);  /* get correct metatable */
      rc = lua_rawequal(L, -1, -2);  /* does it have the correct mt? */
      agn_poptoptwo(L);  /* pop any two values */
    }
    if (rostatus) agn_udfreeze(L, ud, 1);  /* reset read-only status to original state */
  }
  return rc;
}


LUALIB_API int agnL_calludata (lua_State *L, int nargs, int regidx, const char *mt) {  /* externalised 5.4.0 */
  if (regidx == LUA_NOREF) {
    lua_getfenv(L, 1);  /* push internal status table onto the stack */
  } else {
    lua_rawgeti(L, LUA_REGISTRYINDEX, regidx);  /* push registry structure onto the stack */
  }
  switch (nargs) {
    case 1:
      break;
    case 2:
      lua_replace(L, 1);  /* replace argument #1 with registry table */
      lua_settop(L, 2);   /* discard any surplus arguments */
      lua_rawget(L, 1);   /* for key argument #2 fetch value from registry table */
      break;
    case 3:
      lua_replace(L, 1);  /* replace argument #1 with registry table */
      lua_settop(L, 3);   /* discard any surplus arguments */
      lua_rawset(L, 1);   /* set key ~ value pair (argument #2 and #3) into registry table */
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": wrong number of arguments.", "(%s call)", mt);
  }
  /* return reference to internal table or table entry read, otherwise return nothing */
  return nargs < 3;
}


LUALIB_API void luaL_checkstack (lua_State *L, int space, const char *mes) {
  if (!lua_checkstack(L, space))
    luaL_error(L, "stack overflow (%s)", mes);
}


LUALIB_API void agnL_initvaluecache (lua_State *L, const char *procname) {  /* 5.3.4 fix to avoid segfaults at start-up */
  L->C = luaL_newstate();
  if (!L->C) {
    if (procname) lua_writestringerror("%s: ", procname);
    lua_writestringerror("%s\n", "Error, cannot create cache stack.");
  }
  /* 5.4.0 extension, initialise like lhf did in lapi.c/ae_open(), but do not create a slot for an error message */
  lua_settop(L->C, 0);
}


/* Reserves a given number of slots in a cache stack and issues an error if it could not, usually if there are already
   LUAI_MAXCSTACK slots assigned. Usage: luaL_checkcache(L->C, 16, "not enough cache space"); i.e. do not pass L, pass L->C ! */
LUALIB_API void luaL_checkcache (lua_State *L, int space, const char *procname) {  /* 2.37.0 */
  if (!lua_checkstack(L, space))
    luaL_error(L, "Error in " LUA_QS ", cannot extend cache size beyond %d slots.", procname, LUAI_MAXCSTACK);
}


LUALIB_API void luaL_checktype (lua_State *L, int narg, int t) {
  if (lua_type(L, narg) != t)
    tag_error(L, narg, t);
}


LUALIB_API void luaL_checkany (lua_State *L, int narg) {
  if (lua_type(L, narg) == LUA_TNONE)
    luaL_argerror(L, narg, "value expected");
}


LUALIB_API const char *luaL_checklstring (lua_State *L, int narg, size_t *len) {
  const char *s = lua_tolstring(L, narg, len);
  if (!s) tag_error(L, narg, LUA_TSTRING);
  return s;
}


LUALIB_API const char *luaL_checklstringornil (lua_State *L, int narg, size_t *len) {  /* 2.38.0 */
  const char *s;
  if (lua_isnil(L, narg)) {
    s = ""; *len = 0;
  } else if (lua_isstring(L, narg)) {
    s = lua_tolstring(L, narg, len);
    if (!s) tag_error(L, narg, LUA_TSTRING);
  } else {
    s = ""; *len = 0;
    luaL_error(L, "value is not a number, string or null, got %s", luaL_typename(L, narg));
  }
  return s;
}


LUALIB_API const char *luaL_optlstring (lua_State *L, int narg,
                                        const char *def, size_t *len) {
  if (lua_isnoneornil(L, narg)) {
    if (len)
      *len = (def ? tools_strlen(def) : 0);  /* 2.17.8 tweak */
    return def;
  }
  else return luaL_checklstring(L, narg, len);
}


/* Similar to luaL_optstring, but returns an error if a given option is not a string. The length of the optional string is not determined. */
LUALIB_API const char *agnL_optstring (lua_State *L, int narg, const char *def) {  /* 2.9.0 */
  if (lua_isnoneornil(L, narg))
    return def;
  else
    return agn_checkstring(L, narg);
}


/* Checks for an optional argument and returns it if given, or a default, in both cases cast to uint32_t. Faster than a macro,
   see agnxlib.h `agnL_optuint32_t`, 2.16.8 */
LUALIB_API LUA_UINT32 agnL_optuint32_t (lua_State *L, int narg, uint32_t def) {
  if (lua_isnoneornil(L, narg))
    return def;
  else if (lua_isnumber(L, narg)) {  /* 4.12.5 patch for all platforms, see above */
    lua_Number n = agn_tonumber(L, narg);
    if (n < 0.0) {
      luaL_error(L, "Wrong argument #%d: expected a non-negative number.", narg);
    }
    return (uint32_t)tools_reducerange(n, 0, 4294967296.0);
  } else
    return lua_touint32_t(L, narg);
}


LUALIB_API lua_Number luaL_checknumber (lua_State *L, int narg) {
  lua_Number d = lua_tonumber(L, narg);
  if (d == 0 && !lua_isnumber(L, narg))  /* avoid extra test when d is not 0 */
    tag_error(L, narg, LUA_TNUMBER);
  return d;
}


LUALIB_API lua_Number agnL_checknumber (lua_State *L, int narg) {  /* Agena 1.4.3/1.5.0 */
  if (!agn_isnumber(L, narg))  /* 0.5.3 */
    tag_error(L, narg, LUA_TNUMBER);
  return agn_tonumber(L, narg);
}


LUALIB_API int agnL_checkboolean (lua_State *L, int narg) {  /* Agena 1.6.0 */
  if (!lua_isboolean(L, narg))
    tag_error(L, narg, LUA_TBOOLEAN);
  return lua_toboolean(L, narg);  /* -1 for fail, 0 for false, 1 for true */
}


LUALIB_API lua_Number luaL_optnumber (lua_State *L, int narg, lua_Number def) {
  return luaL_opt(L, luaL_checknumber, narg, def);
}


/* If the value at stack index narg is a number, returns this number. If this argument is absent or is NULL,
   returns def. Otherwise, raises an error. The function does not try to convert a string into a number,
   see luaL_optnumber instead. */
LUALIB_API lua_Number agnL_optnumber (lua_State *L, int narg, lua_Number def) {  /* Agena 1.4.3/1.5.0 */
  return luaL_opt(L, agnL_checknumber, narg, def);
}


/* If the value at stack index narg is a Boolean, returns this Booelan as an integer: -1 for fail, 0 for false, and 1 for true.
   If there is no value at index narg or is NULL, returns def. Otherwise, raises an error. */
LUALIB_API int agnL_optboolean (lua_State *L, int narg, int def) {  /* Agena 1.6.0 */
  return luaL_opt(L, agnL_checkboolean, narg, def);
}


LUALIB_API lua_Integer luaL_checkinteger (lua_State *L, int narg) {
  lua_Integer d = lua_tointeger(L, narg);
  if (d == 0 && !lua_isnumber(L, narg))  /* avoid extra test when d is not 0 */
    tag_error(L, narg, LUA_TNUMBER);
  return d;
}


LUALIB_API lua_Integer agnL_checkinteger (lua_State *L, int narg) {  /* Agena 1.4.3/1.5.0 */
  if (!agn_isinteger(L, narg))  /* 0.5.3/4.3.0 */
    tag_error2(L, narg, "integer");
  return lua_tointeger(L, narg);
}


LUALIB_API int32_t luaL_checkint32_t (lua_State *L, int narg) {
  if (!agn_isinteger(L, narg))  /* avoid extra test when d is not 0 */  /* 0.5.3/4.3.0 */
    tag_error2(L, narg, "integer");
  return lua_toint32_t(L, narg);
}


LUALIB_API uint32_t luaL_checkuint32_t (lua_State *L, int narg) {  /* 2.14.5 */
  if (!agn_isnonnegint(L, narg))  /* avoid extra test when d is not 0 */  /* 0.5.3/4.3.0 */
    tag_error2(L, narg, "non-negative integer");
  if (agn_tonumber(L, narg) < 0)  /* 2.16.4 extension */
    luaL_argerror(L, narg, "non-negative value expected");
  return lua_touint32_t(L, narg);
}


LUALIB_API off64_t luaL_checkoff64_t (lua_State *L, int narg) {
  if (!agn_isnumber(L, narg))  /* avoid extra test when d is not 0 */  /* 0.5.3 */
    tag_error(L, narg, LUA_TNUMBER);
  return lua_tooff64_t(L, narg);
}


LUALIB_API lua_Integer luaL_optinteger (lua_State *L, int narg,
                                                      lua_Integer def) {
  return luaL_opt(L, luaL_checkinteger, narg, def);
}


LUALIB_API lua_Integer agnL_optinteger (lua_State *L, int narg,  /* Agena 1.4.3/1.5.0 */
                                                      lua_Integer def) {
  return luaL_opt(L, agnL_checkinteger, narg, def);
}


LUALIB_API lua_Integer agnL_optposint (lua_State *L, int narg,  /* Agena 2.9.4 */
                                                      lua_Integer def) {
  return luaL_opt(L, agn_checkposint, narg, def);
}


LUALIB_API lua_Integer agnL_optnonnegint (lua_State *L, int narg,  /* Agena 2.10.0 */
                                                      lua_Integer def) {
  return luaL_opt(L, agn_checknonnegint, narg, def);
}


LUALIB_API lua_Integer agnL_optnonzeroint (lua_State *L, int narg,  /* 4.11.0 */
                                                      lua_Integer def) {
  return luaL_opt(L, agn_checknonzeroint, narg, def);
}


LUALIB_API lua_Number agnL_optpositive (lua_State *L, int narg, lua_Number def) {  /* Agena 2.12.5, 2.13.0 patch */
  return luaL_opt(L, agn_checkpositive, narg, def);
}


LUALIB_API lua_Number agnL_optnonnegative (lua_State *L, int narg, lua_Number def) {  /* Agena 2.12.5, 2.13.0 patch */
  return luaL_opt(L, agn_checknonnegative, narg, def);
}


LUALIB_API int32_t luaL_optint32_t (lua_State *L, int narg, int32_t def) {
  return luaL_opt(L, luaL_checkint32_t, narg, def);
}


LUALIB_API off64_t luaL_optoff64_t (lua_State *L, int narg, off64_t def) {
  return luaL_opt(L, luaL_checkoff64_t, narg, def);
}


LUALIB_API int luaL_getmetafield (lua_State *L, int obj, const char *event) {
  if (!lua_getmetatable(L, obj))  /* no metatable? */
    return 0;
  lua_pushstring(L, event);
  lua_rawget(L, -2);
  if (lua_isnil(L, -1)) {
    agn_poptoptwo(L);  /* remove metatable and metafield */
    return 0;
  }
  else {
    lua_remove(L, -2);  /* remove only metatable */
    return 1;
  }
}


LUALIB_API int agnL_getmetafield (lua_State *L, int obj, const char *event) {  /* 2.19.1 */
  if (!lua_getmetatable(L, obj))  /* no metatable? */
    return 0;
  lua_pushstring(L, event);
  lua_rawget(L, -2);
  if (!lua_isfunction(L, -1)) {
    agn_poptoptwo(L);  /* remove metatable and metafield */
    return 0;
  }
  else {
    lua_remove(L, -2);  /* remove only metatable */
    return 1;
  }
}


LUALIB_API int luaL_callmeta (lua_State *L, int obj, const char *event) {
  obj = abs_index(L, obj);
  luaL_checkstack(L, 2, "not enough stack space");  /* 2.31.7 fix */
  if (!luaL_getmetafield(L, obj, event))  /* no metafield? */
    return 0;
  lua_pushvalue(L, obj);
  lua_call(L, 1, 1);
  return 1;
}


LUALIB_API void (luaL_register) (lua_State *L, const char *libname,
                                const luaL_Reg *l) {
  luaI_openlib(L, libname, l, 0);
}


LUALIB_API int luaI_libsize (const luaL_Reg *l) {
  int size = 0;
  for (; l->name; l++) size++;
  return size;
}


LUALIB_API void luaI_openlib (lua_State *L, const char *libname,
                              const luaL_Reg *l, int nup) {
  if (libname) {
    int size = luaI_libsize(l);
    /* check whether lib already exists */
    luaL_checkstack(L, 2, "not enough stack space");  /* 7.0.2 */
    luaL_findtable(L, LUA_REGISTRYINDEX, "_LOADED", 1);  /* Lua 5.1.4 version */
    lua_getfield(L, -1, libname);  /* get _LOADED[libname] */
    if (!lua_istable(L, -1)) {  /* not found? */
      agn_poptop(L);  /* remove previous result */
      /* try global variable (and create one if it does not exist) */
      if (luaL_findtable(L, LUA_GLOBALSINDEX, libname, size) != NULL) {
        luaL_error(L, "Error, there is a name conflict for module " LUA_QS ".", libname);
      }
      lua_pushvalue(L, -1);
      lua_setfield(L, -3, libname);  /* _LOADED[libname] = new table */
    }
    lua_remove(L, -2);  /* remove _LOADED table */
    lua_insert(L, -(nup + 1));  /* move library table to below upvalues */
  }
  for (; l->name; l++) {
    int i;
    luaL_checkstack(L, nup + 1, "not enough stack space");  /* 7.0.2 */
    for (i=0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -nup);
    lua_pushcclosure(L, l->func, nup);
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);  /* remove upvalues */
}


/* }====================================================== */


LUALIB_API const char *luaL_gsub (lua_State *L, const char *s, const char *p,
                                                               const char *r) {
  const char *wild;
  size_t l = tools_strlen(p);  /* 2.17.8 tweak */
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while ((wild = strstr(s, p)) != NULL) {
    luaL_addlstring(&b, s, wild - s);  /* push prefix */
    luaL_addstring(&b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after `p' */
  }
  luaL_addstring(&b, s);  /* push last suffix */
  luaL_pushresult(&b);
  return lua_tostring(L, -1);
}


/* szhint is the size of the table to be created, not the length of fname */
LUALIB_API const char *luaL_findtable (lua_State *L, int idx,
                                       const char *fname, int szhint) {
  const char *e;
  lua_pushvalue(L, idx);
  do {
    e = strchr(fname, '.');
    if (e == NULL) e = fname + tools_strlen(fname);  /* 2.17.8 tweak */
    lua_pushlstring(L, fname, e - fname);  /* push index = packagename */
    lua_rawget(L, -2);                     /* try to retrieve corresponding entry */
    if (lua_isnil(L, -1)) {  /* no such field?  Create one and leave it on stack. */
      agn_poptop(L);  /* remove this nil */
      lua_createtable(L, 0, (*e == '.' ? 1 : szhint));  /* new table for field */
      lua_pushlstring(L, fname, e - fname);
      lua_pushvalue(L, -2);
      lua_settable(L, -4);  /* set new table into field */
    }
    else if (!lua_istable(L, -1)) {  /* field has a non-table value? */
      agn_poptoptwo(L);  /* remove table and value */
      return fname;  /* return problematic part of the name */
    }
    lua_remove(L, -2);  /* remove previous table */
    fname = e + 1;
  } while (*e == '.');
  return NULL;
}


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

#define bufflen(B)  ((B)->p - (B)->buffer)

#define bufffree(B)  ((size_t)(LUAL_BUFFERSIZE - bufflen(B)))

#define LIMIT  (LUA_MINSTACK/2)


int emptybuffer (luaL_Buffer *B) {
  size_t l = bufflen(B);
  if (l == 0) return 0;  /* put nothing on stack */
  else {
    lua_pushlstring(B->L, B->buffer, l);
    B->p = B->buffer;
    B->lvl++;
    return 1;
  }
}


static void adjuststack (luaL_Buffer *B) {
  if (B->lvl > 1) {
    lua_State *L = B->L;
    int toget = 1;  /* number of levels to concat */
    size_t toplen = lua_strlen(L, -1);
    do {
      size_t l = lua_strlen(L, -(toget + 1));
      if (B->lvl - toget + 1 >= LIMIT || toplen > l) {
        toplen += l;
        toget++;
      }
      else break;
    } while (toget < B->lvl);
    lua_concat(L, toget);
    B->lvl = B->lvl - toget + 1;
  }
}


LUALIB_API char *luaL_prepbuffer (luaL_Buffer *B) {
  if (emptybuffer(B))
    adjuststack(B);
  return B->buffer;
}


LUALIB_API char *luaL_prepbuffsize (luaL_Buffer *B, size_t sz) {
  return luaL_prepbuffer(B);
}


LUALIB_API void luaL_addlstring (luaL_Buffer *B, const char *s, size_t l) {
  while (l--)
    luaL_addchar(B, *s++);
}


LUALIB_API void luaL_addstring (luaL_Buffer *B, const char *s) {
  luaL_addlstring(B, s, tools_strlen(s));  /* 2.17.8 tweak */
}


/* In string s, replaces any occurrence of the string p with the string r and adds the resulting string to buffer B.
   Search patterns are ignored. Taken from Lua 5.4.0 RC 4, 2.21.2 */
LUALIB_API void luaL_addgsub (luaL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = tools_strlen(p);  /* 2.25.1 tweak */
  while ((wild = strstr(s, p)) != NULL) {
    luaL_addlstring(b, s, wild - s);  /* push prefix */
    luaL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  luaL_addstring(b, s);  /* push last suffix */
}


LUALIB_API void luaL_pushresult (luaL_Buffer *B) {
  emptybuffer(B);
  lua_concat(B->L, B->lvl);
  B->lvl = 1;
}


/* Clears a luaL_Buffer and resets it, does not leave anything on the stack */
LUALIB_API void luaL_clearbuffer (luaL_Buffer *B) {  /* 2.14.9, better sure than sorry */
  emptybuffer(B);
  lua_concat(B->L, B->lvl);
  agn_poptop(B->L);  /* delete pushed string */
  B->lvl = 1;
}


LUALIB_API void luaL_addvalue (luaL_Buffer *B) {
  lua_State *L = B->L;
  size_t vl;
  const char *s = lua_tolstring(L, -1, &vl);
  if (vl <= bufffree(B)) {  /* fit into buffer? */
    tools_memcpy(B->p, s, vl);  /* put it there; 2.25.1 tweak */
    B->p += vl;
    agn_poptop(L);  /* remove from stack */
  }
  else {
    if (emptybuffer(B))
      lua_insert(L, -2);  /* put buffer before new value */
    B->lvl++;  /* add new value into B stack */
    adjuststack(B);
  }
}


LUALIB_API void luaL_buffinit (lua_State *L, luaL_Buffer *B) {
  B->L = L;
  B->p = B->buffer;
  B->lvl = 0;
}


/* }====================================================== */

#ifdef LUA_COMPAT_5_1
LUALIB_API int luaL_ref (lua_State *L, int t) {
  int ref;
  t = abs_index(L, t);
  if (lua_isnil(L, -1)) {
    agn_poptop(L);  /* remove from stack */
    return LUA_REFNIL;  /* `nil' has a unique fixed reference */
  }
  lua_rawgeti(L, t, FREELIST_REF);  /* get first free element */
  ref = (int)lua_tointeger(L, -1);  /* ref = t[FREELIST_REF] */
  agn_poptop(L);  /* remove it from stack */
  if (ref != 0) {  /* any free element? */
    lua_rawgeti(L, t, ref);  /* remove it from list */
    lua_rawseti(L, t, FREELIST_REF);  /* (t[FREELIST_REF] = t[ref]) */
  }
  else {  /* no free elements */
    ref = (int)lua_objlen(L, t);
    ref++;  /* create new reference */
  }
  lua_rawseti(L, t, ref);
  return ref;
}


LUALIB_API void luaL_unref (lua_State *L, int t, int ref) {
  if (ref >= 0) {
    t = abs_index(L, t);
    lua_rawgeti(L, t, FREELIST_REF);
    lua_rawseti(L, t, ref);  /* t[ref] = t[FREELIST_REF] */
    lua_pushinteger(L, ref);
    lua_rawseti(L, t, FREELIST_REF);  /* t[FREELIST_REF] = ref */
  }
}
#else
/*
** The previously freed references form a linked list: t[1] is the index
** of a first free index, t[t[1]] is the index of the second element,
** etc. A zero signals the end of the list.
** Taken from the Lua 5.5.1 April 1, 2026 snapshot. 7.3.9
*/
LUALIB_API int luaL_ref (lua_State *L, int t) {
  int ref;
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);  /* remove from stack */
    return LUA_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = lua_absindex(L, t);
  if (lua_rawgeti(L, t, 1) == LUA_TNUMBER)  /* already initialized? */
    ref = (int)lua_tointeger(L, -1);  /* ref = t[1] */
  else {  /* first access */
    lua_assert(!lua_toboolean(L, -1));  /* must be nil or false */
    ref = 0;  /* list is empty */
    lua_pushinteger(L, 0);  /* initialize as an empty list */
    lua_rawseti(L, t, 1);  /* ref = t[1] = 0 */
  }
  lua_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    lua_rawgeti(L, t, ref);  /* remove it from list */
    lua_rawseti(L, t, 1);  /* (t[1] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)lua_rawlen(L, t) + 1;  /* get a new reference */
  lua_rawseti(L, t, ref);
  return ref;
}


LUALIB_API void luaL_unref (lua_State *L, int t, int ref) {
  if (ref >= 0) {
    t = lua_absindex(L, t);
    lua_rawgeti(L, t, 1);
    lua_assert(lua_isinteger(L, -1));
    lua_rawseti(L, t, ref);  /* t[ref] = t[1] */
    lua_pushinteger(L, ref);
    lua_rawseti(L, t, 1);  /* t[1] = ref */
  }
}
#endif

/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
  int extraline;
  FILE *f;
  char buff[LUAL_BUFFERSIZE];
} LoadF;


/* 0.13.3 */
LUALIB_API const char *getF (lua_State *L, void *ud, size_t *size) {  /* made public 2.21.2 */
  LoadF *lf = (LoadF *)ud;
  (void)L;
  if (lf->extraline) {
    lf->extraline = 0;
    *size = 1;
    return "\n";
  }
  if (feof(lf->f)) return NULL;
  *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);
  return (*size > 0) ? lf->buff : NULL;
}


static int errfile (lua_State *L, const char *what, int fnameindex) {
  const char *serr = my_ioerror(errno);  /* 2.9.4, strerror(errno); */
  const char *filename = lua_tostring(L, fnameindex) + 1;
  lua_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  lua_remove(L, fnameindex);
  return LUA_ERRFILE;
}


/* 0.13.3 */
LUALIB_API int luaL_loadfile (lua_State *L, const char *filename) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = lua_gettop(L) + 1;  /* index of filename on the stack */
  lf.extraline = 0;
  if (filename == NULL) {
    lua_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    lua_pushfstring(L, "@%s", filename);
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  c = getc(lf.f);
  if (c == '#') {  /* Unix exec. file? */
    lf.extraline = 1;
    while ((c = getc(lf.f)) != EOF && c != '\n') ;  /* skip first line */
    if (c == '\n') c = getc(lf.f);
  }
  if (c == LUA_SIGNATURE[0] && filename) {  /* binary file? */
    lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
    if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
    /* skip eventual `#!...' */
    while ((c = getc(lf.f)) != EOF && c != LUA_SIGNATURE[0]) ;
    lf.extraline = 0;
  }
  ungetc(c, lf.f);
  status = lua_load(L, getF, &lf, lua_tostring(L, -1));
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    lua_settop(L, fnameindex);  /* ignore results from `lua_load' */
    return errfile(L, "read", fnameindex);
  }
  lua_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


LUALIB_API const char *getS (lua_State *L, void *ud, size_t *size) {  /* made public 2.21.2 */
  LoadS *ls = (LoadS *)ud;
  (void)L;
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


LUALIB_API int luaL_loadbuffer (lua_State *L, const char *buff, size_t size,
                                const char *name) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return lua_load(L, getS, &ls, name);
}


LUALIB_API int (luaL_loadstring) (lua_State *L, const char *s) {
  return luaL_loadbuffer(L, s, tools_strlen(s), s);  /* 2.17.8 tweak */
}



/* }====================================================== */


static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}


static int panic (lua_State *L) {
  (void)L;  /* to avoid warnings */
  fprintf(stderr, "PANIC: unprotected error in call to Agena API (%s)\n",
                   lua_tostring(L, -1));
  return 0;
}


/*
** Emit a warning. '*warnstate' means:
** 0 - warning system is off;
** 1 - ready to start a new message;
** 2 - previous message is to be continued.
** Taken from Lua 5.4.0 RC 4
*/
static void warnf (void *ud, const char *message, int tocont) {
  int *warnstate = (int *)ud;
  if (*warnstate != 2 && !tocont && *message == '@') {  /* control message? */
    if (tools_streq(message, "@off"))  /* 2.25.1 tweak */
      *warnstate = 0;
    else if (tools_streq(message, "@on"))  /* 2.25.1 tweak */
      *warnstate = 1;
    return;
  }
  else if (*warnstate == 0)  /* warnings off? */
    return;
  if (*warnstate == 1)  /* previous message was the last? */
    lua_writestringerror("%s", "Agena warning: ");  /* start a new warning */
  lua_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    *warnstate = 2;  /* to be continued */
  else {  /* last part */
    lua_writestringerror("%s", "\n");  /* finish message with end-of-line */
    *warnstate = 1;  /* ready to start a new message */
  }
}


LUALIB_API lua_State *luaL_newstate (void) {
  lua_State *L = lua_newstate(l_alloc, NULL);
  if (L) {  /* changed 2.21.1 */
    int *warnstate;  /* space for warning state */
    lua_atpanic(L, &panic);
    warnstate = (int *)lua_newuserdata(L, sizeof(int));
    luaL_ref(L, LUA_REGISTRYINDEX);  /* make sure it won't be collected */
    *warnstate = 0;  /* default is warnings off */
    lua_setwarnf(L, warnf, warnstate);
  }
  return L;
}

/* Agena initialisation (loading library.agn & agena.ini), 0.24.0 */

void printwarning (int i, const char *path) {
  if (i)
    fprintf(stderr, "\nWarning, main Agena library not found in:\n\n   %s\n\n", path);  /* 2.34.10 optimisation */
  else {  /* Agena 1.3.1 fix */
    fprintf(stderr, "Warning, unsuccessful initialisation. Could not determine the path to the main\n"
                    "Agena library directory");
    if (path != NULL)
       fprintf(stderr, " in\n\n   %s.\n\n", path);
    else
       fprintf(stderr, ".\n\n");
  }
  fprintf(stderr, "Some functions will not work. Please set or reset libname manually.\n\n");
  fprintf(stderr, "You should permanently set the operating system variable AGENAPATH to the path\n"
                  "of the main Agena library folder by adding the following line to your profile\n"
                  "file:\n\n"
#if (!defined(__DJGPP__)) && (defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__))  /* 2.34.9 fix for DOS */
                  "   export AGENAPATH=/usr/agena/lib\n\n"
                  "in UNIX.\n\n"
#else
                  "   set AGENAPATH=<drive letter>:/agena/lib\n\n"
                  "(or similar, depending on where you have Agena installed) in OS/2, DOS, Windows.\n\n"  /* 2.34.9 improvement */
#endif
                  "Alternatively, you may change into the Agena `bin` folder and run Agena from there.\n");
#ifdef __OS2__
  fprintf(stderr, "\nIf you run Agena the first time in OS/2, just reboot your computer for the\n"
                  "addition of AGENAPATH to PATH in CONFIG.SYS to take effect.\n");
#endif
  if (!i) fprintf(stderr, "\n");
  fflush(stderr);
}


LUALIB_API const char *luaL_pushnexttemplate (lua_State *L, const char *path) {
  const char *l;
  while (*path == *LUA_PATHSEP) path++;  /* skip separators */
  if (*path == '\0') return NULL;  /* no more templates */
  l = strchr(path, *LUA_PATHSEP);  /* find next separator */
  if (l == NULL) l = path + tools_strlen(path);  /* 2.17.8 tweak */
  lua_pushlstring(L, path, l - path);  /* template */
  return l;
}


/* changed Agena 1.0.2 */
static int agnL_initialise_auxloop (lua_State *L, const char *path, char *what, int warning, int debug) {
  const char *fn;
  int result1, result2;
  result1 = result2 = 0;
  while ((path = luaL_pushnexttemplate(L, path)) != NULL) {  /* pushes a path onto the stack */
    agnL_debuginfo2(debug,  "      Searching ", luaL_checkstring(L, -1));
    agnL_debuginfo2(debug,  what, " ");
    luaL_checkstack(L, 1, "not enough stack space");  /* 7.0.2 fix */
    lua_pushstring(L, what);
    lua_concat(L, 2);
    fn = luaL_checkstring(L, -1);
    result1 = !tools_exists(fn);  /* 2.16.11 */
    agnL_debuginfo(debug, (result1) ? "failed.\n" : "succeeded.\n");
    result2 = 0;
    if (!result1) {  /* file found and readable */
      agnL_debuginfo3(debug, "      Executing: ", fn, " ");
      result2 = luaL_dofile(L, fn);  /* execute it; 0 = no errors, 1 = errors;
      a string is pushed on the stack in case of errors */
      agnL_debuginfo(debug, (result2) ? "failed.\n" : "succeeded.\n");
    }
    if (result2) {  /* print syntax error that occurred in file */
      warning = 1;
      fprintf(stderr, "%s\n", lua_tostring(L, -1));
    }
    lua_pop(L, 1 + result2);  /* pop template and possible error message */
    if (result1 + result2 == 0) break;
  }
  if (warning == 0)  /* changed Agena 1.0.2 */
    return 0;
  else if (result2 == 1)
    return 2;  /* syntax error in file */
  else
    return result1;  /* file found (0) or file not found (1) */
}


char *getuserhome () {
  char *userhome = NULL;
#ifdef _WIN32
  userhome = getenv("UserProfile");
#elif (!defined(__DJGPP__)) && (defined(__unix__) || defined(__APPLE__))  /* 2.34.9 DOS fix */
  userhome = getenv("HOME");
#elif defined(__HAIKU__)  /* 0.33.1 */
  userhome = getenv("HOME");
  if (userhome == NULL)
    userhome = "/boot/home";
#elif defined(__OS2__) || defined(__DJGPP__)
  userhome = getenv("HOME");
  if (userhome == NULL)
    userhome = getenv("USER");
#endif
  return userhome;
}

/* IMPORTANT NOTE: Since values popped may be gc'ed, the strings put on the stack will only be popped
   by this procedure when no longer needed. Do not put an agn_poptop() statement right after the
   str = agn_tostring(L, n) assignment !!! */
LUALIB_API void agnL_initialise (lua_State *L, int skipinis, int debug, int skipmainlib, int skipagenapath) {
  char *path;
  char *userhome, *newuserhome;
  int result1, result2, result3, result2u, result3u;
  newuserhome = NULL;  /* to prevent compiler warnings */
  result1 = result2 = result3 = result2u = result3u = 0;
  if (skipinis)
    agnL_debuginfo2(debug, "\nIgnoring initialisation file(s)", skipmainlib ? "" : ".\n");
  if (skipmainlib) {
    agnL_debuginfo2(debug, "%s main library file lib/library.agn.\n", skipinis ? " and" : "\nIgnoring");
  }
  agnL_debuginfo(debug, "\nCalling agnL_initialise:\n");
  lua_getglobal(L, "libname");
  if (!agn_isstring(L, -1)) {  /* 0.20.2 */
    int isnil = lua_isnoneornil(L, -1);  /* 2.34.9 improvement */
    agn_poptop(L);  /* remove `something` */
    fprintf(stderr, "Error: " LUA_QS
      " is %s, must be a path to the main Agena library\nfolder.\n", "libname", isnil ? "unassigned" : "not a string");
    fflush(stderr);
    return;
  }
  /* better be sure than sorry, don't rely on Agena not GC'ing the string pushed onto the stack; 7.3.2 */
  path = tools_strdup(agn_tostring(L, -1));
  if (!path) luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
  agn_poptop(L);
  agnL_debuginfo(debug, "   Getting libname: "); agnL_debuginfo2(debug, path, "\n");
  result1 = 0;
  /* load library.agn file */
  if (!skipmainlib) {
    agnL_debuginfo(debug, "   Trying to read library.agn:\n");
    result1 = agnL_initialise_auxloop(L, path, "/library.agn", 1, debug);
    if (result1 == 1) {
      if (!debug)
        printwarning(1, path);  /* library.agn not found, extended in 0.31.5 */
      else
        agnL_debuginfo(debug, "   Hint: path should include the substring 'agena'.\n");
    }
  }
  /* run primary agena.ini file */
  if (!skipinis) {
    agnL_debuginfo(debug, "   Trying to read global initialisation file(s):\n");
#if (!defined(__DJGPP__)) && (defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__))  /* 2.34.9 DOS fix */
    result2 = agnL_initialise_auxloop(L, path, "/.agenainit", 0, debug);
    result2u = agnL_initialise_auxloop(L, path, "/agena.ini", 0, debug);  /* 2.14.1 extension */
#else
    result2 = agnL_initialise_auxloop(L, path, "/agena.ini", 0, debug);
#endif
  }
  xfree(path);
  agnL_debuginfo(debug, "   Getting user home: ");
  userhome = getuserhome();  /* 0.31.6 */
  if (userhome != NULL) {  /* 0.31.6, 1.0.4 patch */
    newuserhome = tools_strdup(userhome);  /* 2.5.0 patch */
    if (!newuserhome) luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
    agnL_debuginfo2(debug, newuserhome, ".\n");
  } else {  /* 2.34.9 improvement */
    agnL_debuginfo(debug, "failed.\n");
  }
  if (userhome != NULL && !skipinis) {
    /* run user's agena initialisation file, 0.29.4 */
    agnL_debuginfo(debug, "   Trying to read personal initialisation file:\n");
#if (!defined(__DJGPP__)) && (defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__))  /* 2.34.9 DOS fix */
    result3 = agnL_initialise_auxloop(L, newuserhome, "/.agenainit", 0, debug);
    result3u = agnL_initialise_auxloop(L, newuserhome, "/agena.ini", 0, debug);  /* 2.14.1 extension */
#else
    result3 = agnL_initialise_auxloop(L, newuserhome, "/agena.ini", 0, debug);
#endif
  }
  if (result1 || result2 || result3 || result2u || result3u) {  /* error message printed ? 2.14.1 extension */
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  /* assign `environ.homedir` environment variable */
  if (userhome != NULL) {
    agnL_debuginfo(debug, "   environ.homedir set to ");
    lua_getglobal(L, "environ");
    if (!lua_istable(L, -1)) {
      agn_poptop(L);  /* pop null */
      /* Do not call luaL_error as this might cause a segfault, 7.0.2 fix and change */
      agnL_debuginfo(debug, "nothing as table `environ` not found.\n");
    } else {
      str_charreplace(newuserhome, '\\', '/', 1);
      lua_pushstring(L, newuserhome);
      lua_setfield(L, -2, "homedir");
      agn_poptop(L);  /* pop environ table */
      agnL_debuginfo2(debug, newuserhome, ".\n\n");
    }
    xfree(newuserhome);  /* 2.5.0 patch */
  }
  agn_setdebug(L, debug);  /* 1.9.4 */
  agn_setnoini(L, skipinis);  /* set whether ini has been read to lua_State, 2.8.6 */
  agn_setnomainlib(L, skipmainlib);  /* same with library.agn, 2.8.6 */
  agn_setskipagenapath(L, skipagenapath);  /* respect or ignore AGENAPATH environment variable, 2.35.4 */
  /* The userdata pushed by main() in agena.c is still on the top of the stack */
}

/* end of Agena initialisation */

/* This helper function introduced in 7.3.2 no longer pokes into system-managed memory by writing a
   finalising '\0' into the memory area administering environment variables. Advised by Gemini AI. */
static int tryassignagenapath (lua_State *L, const char *incoming_path, int debug) {
  char *copy, *worker, *newpath;
  int success = 0;
  size_t in_len;
  if (incoming_path == NULL) return 0;
  in_len = strlen(incoming_path);
  /* Allocate with extra padding (8 bytes) and ZERO the memory */
  /* This prevents 'size 4' over-reads from hitting uninitialized memory */
  copy = (char *)calloc(in_len + 8, sizeof(char));
  if (copy == NULL) return 0;
  strcpy(copy, incoming_path);
  /* Normalize */
  str_charreplace(copy, '\\', '/', 1);
  worker = strstr(copy, "/agena");
  if (worker != NULL) {
    /* Move to the start of the part we might want to clip */
    char *searcher = worker + 6;
    /* Find the next slash or the end */
    while (*searcher != '\0') {
      if (*searcher == '/') break;
      searcher++;
    }
    /* Truncate */
    *searcher = '\0';
    newpath = str_concat(copy, "/lib", NULL);
    if (newpath != NULL) {
      if (tools_exists(newpath)) {
        lua_pushstring(L, newpath);
        lua_setglobal(L, "libname");
        success = 1;
      }
      xfree(newpath); /* Use your specific free */
    }
  }
  xfree(copy); /* Use standard free since we used calloc */
  return success;
}

LUALIB_API void agnL_setLibname (lua_State *L, int warning, int debug, int skipagenapath) {  /* changed Agena 1.0.2, 1.0.3,
  patched and improved 2.1.6 */
  char *path, *apath;
  int result;
  path = NULL;  /* to prevent compiler warnings */
  agnL_debuginfo(debug, "Calling agnL_setLibname:\n");
  lua_getglobal(L, "libname");
  if (!lua_isnil(L, -1)) {  /* libname already set via command line option ? */
    agn_poptop(L);
    agnL_debuginfo(debug, "   libname already set, exiting agnL_setLibname.\n");
    return;
  }
  agn_poptop(L);  /* pop null */
  /* try to assign libname from AGENAPATH variable set in the operating system */
  if (skipagenapath) {
    agnL_debuginfo(debug, "   Ignoring AGENAPATH due to given -a switch.\n");
    goto labelA;  /* 2.35.4 */
  }
  agnL_debuginfo(debug, "   Trying to get environment variable AGENAPATH: ");
  path = tools_getenv("AGENAPATH");  /* 2.37.1 change */
  agnL_debuginfo(debug, (path == NULL) ? "unassigned.\n" : "found.\n");
  if (path != NULL) {  /* 2.37.4 fix */
    agnL_debuginfo(debug, "   Setting libname to AGENAPATH: ");
    str_charreplace(path, '\\', '/', 1);
    lua_pushstring(L, path);  /* 0.28.2 patch */
    lua_setglobal(L, "libname");
    agnL_debuginfo2(debug, path, ".\n");
    xfree(path);
    return;
  }
#ifdef _WIN32
  else {  /* 2.1.6, avoid setting AGENAPATH to the environment in Windows 2000 or later */
    int winversion;
    struct WinVer winver;
    winversion = getWindowsVersion(&winver);
    if (winversion >= MS_WIN2K || winversion == MS_WINNT4) {  /* NT 4.0, 2000 or later ? 2.17.2 extension */
      result = 0;
      agnL_debuginfo(debug, "   Trying to get AGENAPATH by calling GetModuleFileName: ");
      apath = agn_stralloc(L, MAX_PATH, "agnL_setLibname", NULL);  /* 2.16.5 */
      if (GetModuleFileName(NULL, apath, MAX_PATH + 1)) {
        int j;  /* 2.1.8 patch */
        for (j=0; j < tools_strlen(apath); j++) apath[j] = tolower(apath[j]);  /* 2.17.8 tweak */
        result = tryassignagenapath(L, apath, debug);
      } else
        agnL_debuginfo(debug, "failed.\n");
      xfree(apath);  /* path is NULL, so no need to free it */
      if (result) return;
    }
  }
#endif
labelA:
  /* try to build path from current working directory */
  agnL_debuginfo(debug, "   Trying path to current working directory");
  apath = agn_stralloc(L, PATH_MAX, "agnL_setLibname", NULL);  /* 2.16.5 */
  /* Agena 1.0.4 */
  if (tools_cwd(apath) != 0) {
    xfree(apath); /* 2.35.4 patch; path is NULL, so no need to free it */
    luaL_error(L, "memory failure during initialisation, in " LUA_QL("agnL_setLibname") ".");
  }
  result = tryassignagenapath(L, apath, debug);
  if (result) {
    xfree(apath);  /* path is NULL, so no need to free it */
    return;
   } else {  /* 2.35.4 */
    str_charreplace(apath, '\\', '/', 1);
    char *altpath = str_concat(apath, "/../lib/library.agn", NULL);
    char *newpath = str_concat(apath, "/../lib", NULL);
    agnL_debuginfo2(debug, "   failed. Trying\n      ", newpath);
    if (tools_exists(altpath)) {
      lua_pushstring(L, newpath);
      lua_setglobal(L, "libname");
      agnL_debuginfo3(debug, ".\n   Success, libname set to ", newpath, ".\n");
      xfree(newpath); xfree(altpath); xfree(apath);  /* path is NULL, so no need to free it */
      return;
    }
    agnL_debuginfo(debug, "\n   did not succeed.\n");
    xfree(apath); xfree(altpath); xfree(newpath);
  }
#if (!defined(__DJGPP__)) && (defined(__unix__) || defined(__APPLE__))  /* 2.34.9 DOS fix */
  /* set default path if path could not be determined before */
  if (tools_exists("/usr/agena/lib")) {  /* 2.16.11 change */
    lua_pushstring(L, "/usr/agena/lib");
    lua_setglobal(L, "libname");
    agnL_debuginfo(debug, "   libname set to default path /usr/agena/lib.\n");
    return;
  }
#elif defined(__HAIKU__)
  /* set default path if path could not be determined before */
  if (tools_exists("/boot/common/share/agena/lib")) {  /* 2.16.11 change */
    lua_pushstring(L, "/boot/common/share/agena/lib");
    lua_setglobal(L, "libname");
    agnL_debuginfo(debug, "   libname set to default path /boot/common/share/agena/lib.\n");
    return;
  }
#elif defined(_WIN32)
  /* try to find Agena installation in the standard Windows programme subdirectory such as `C:\Program Files`
     and set libname accordingly; 0.24.0 - June 21, 2009 */
  path = tools_getenv("ProgramFiles");  /* Agena 1.0.4 patch / 2.16.11 change; path has been NULL before */
  if (path != NULL) {  /* "ProgramFiles" is set ? */
    str_charreplace(path, '\\', '/', 1);
    lua_pushstring(L, path);
    lua_pushstring(L, "/agena/lib");
    lua_concat(L, 2);  /* string `/c/programmes/agena/lib` is now on stack */
    if (tools_exists(agn_tostring(L, -1))) {  /* Agena programme folder found and readable, 2.16.11 change */
      agnL_debuginfo3(debug, "   libname set to '", agn_tostring(L, -1), "/agena/lib'.\n");
      lua_setglobal(L, "libname");  /* pops string */
      xfree(path);  /* 2.16.11 patch */
      return;
    }
    agn_poptop(L);  /* pop string `/c/programmes/agena/lib` */
    xfree(path);
  }
#endif
  if (warning) {
    printwarning(0, path);
  }
}


/* assumes that i is negative ! */
LUALIB_API void agnL_printnonstruct (lua_State *L, int i) {
  const char *r;
  size_t l;
  luaL_checkstack(L, 2, "not enough stack space");  /* 2.31.7 fix */
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, i - 1);
  lua_call(L, 1, 1);  /* this will also call the __string metamethod if one exists; if tostring is
                         unassigned, lua_call issues an error */
  r = lua_tolstring(L, -1, &l);  /* checks for non-strings, too */
  if (r == NULL)  /* 1.10.5 patch */
    luaL_error(L, "value is not a number, boolean or string, got %s", luaL_typename(L, -1));
  if (l == tools_strlen(r))  /* 2.16.1 patch for embedded zeros */ /* 2.17.8 tweak */
    fprintf(stdout, "%s", r);
  else {  /* there are embedded \0's in the string */
    const char *oldr;
    ptrdiff_t n = 0;
    oldr = r;
    while (n <= l) {
      fprintf(stdout, "%s", r);
      r += tools_strlen(r) + 1;  /* +1: skip embedded zero, 2.17.8 tweak */
      n += r - oldr;
    }
  }
  agn_poptop(L);  /* pops string printed by fprintf */
}


LUALIB_API int agnL_gettablefield (lua_State *L, const char *table, const char *field, const char *procname, int issueerror) {  /* 1.6.4 */
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 security fix */
  lua_getglobal(L, table);
  if (!lua_istable(L, -1)) {
    if (issueerror) {
      agn_poptop(L);  /* in case of an error, remove object on the top */
      luaL_error(L, "Error in " LUA_QS ": table " LUA_QS " does not exist.", procname, table);
    }
    return LUA_TNONE - 1;  /* = -3 */
  }
  lua_getfield(L, -1, field);
  lua_remove(L, -2);  /* remove table */
  return (lua_type(L, -1));
}


LUALIB_API lua_Unsigned luaL_checkunsigned (lua_State *L, int narg) {  /* 2.5.4, taken from Lua 5.2.4 */
  int isnum;
  lua_Unsigned d = lua_tounsignedx(L, narg, &isnum);
  if (!isnum)
    tag_error(L, narg, LUA_TNUMBER);
  return d;
}


LUALIB_API lua_Unsigned luaL_optunsigned (lua_State *L, int narg,
                                                        lua_Unsigned def) {  /* 2.5.4, taken from Lua 5.2.4 */
  return luaL_opt(L, luaL_checkunsigned, narg, def);
}


LUALIB_API int agnL_gettop (lua_State *L, const char *message, const char *procname) {  /* 2.9.8 */
  int nargs = lua_gettop(L);
  if (nargs == 0) {
    luaL_error(L, "Error in " LUA_QS ": %s.", procname, message);
  }
  return nargs;
}


/* ******************************************************************************************************************** */
/* NEW OR MOVED FROM lapi.c IN 2.10.4 */
/* ******************************************************************************************************************** */

/* Pushes the mathematical function at index idx and number x onto the stack, optionally pushes the numbers
   at stack positions optstart through optstop (including) and then calls the function with the values pushed.
   The function at idx should return one number, otherwise an error is issued. If the function at idx is not
   multivariate, then pass values for optstart and optstop such that optstart > optstop. The function does not
   change the stack. */
LUALIB_API lua_Number agnL_fncall (lua_State *L, int idx, lua_Number x, int optstart, int optstop) {  /* 2.10.4 */
  int i, error, slots;
  lua_Number r;
  slots = 2 + (optstart <= optstop ? optstop - optstart + 1 : 0);  /* 2.31.7 fix */
  luaL_checkstack(L, slots--, "too many arguments");  /* 2.31.6/7 fix */
  lua_pushvalue(L, idx);
  lua_pushnumber(L, x);
  for (i=optstart; i <= optstop; i++) lua_pushvalue(L, i);
  r = agn_ncall(L, slots, &error, 1);
  return r;
}


LUALIB_API lua_Number agnL_fncallx (lua_State *L, int idx, lua_Number x,
  int optstart, int optstop, int *error, int quit) {  /* 6.6.11 */
  int i, slots;
  slots = 2 + (optstart <= optstop ? optstop - optstart + 1 : 0);  /* 2.31.7 fix */
  luaL_checkstack(L, slots--, "too many arguments");  /* 2.31.6/7 fix */
  lua_pushvalue(L, idx);
  lua_pushnumber(L, x);
  for (i=optstart; i <= optstop; i++) lua_pushvalue(L, i);
  /* catch error with agn_npcall() and set error so that memory may be freed appropriately
     in the calling function, 7.3.4 */
  return agn_npcall(L, slots, error, quit);
}


LUALIB_API int agnL_fillarray (lua_State *L, int idx, int type, lua_Number *a, size_t *ii,
                            int left, int right, int reverse) {  /* 2.10.4 */
  int i;
  size_t iii;
  lua_Number x;
  iii = 0;
  if (reverse) {
    switch (type) {
      case LUA_TTABLE: {
        for (i=right; i >= left; i--) {
          x = agn_getinumber(L, idx, i);
          if (isnan(x)) return 1;  /* 2.10.1, changed to isnan instead of tools_isnan */
          a[++iii - 1] = x;
        }
        break;
      }
      case LUA_TSEQ: {
        for (i=right; i >= left; i--) {
          x = lua_seqrawgetinumber(L, idx, i);
          if (isnan(x)) return 1;  /* 2.10.1, changed to isnan instead of tools_isnan */
          a[++iii - 1] = x;
        }
        break;
      }
      case LUA_TREG: {  /* 3.18.8 */
        int rc;
        for (i=right; i >= left; i--) {
          x = agn_regrawgetinumber(L, idx, i, &rc);
          if (!rc) return 1;  /* 4.4.7 change */
          a[++iii - 1] = x;
        }
        break;
      }
      default: lua_assert(0);  /* should not happen */
    }
  } else {
    switch (type) {
      case LUA_TTABLE: {
        for (i=left; i <= right; i++) {
          x = agn_getinumber(L, idx, i);
          if (isnan(x)) return 1;  /* 2.10.1, changed to isnan instead of tools_isnan */
          a[++iii - 1] = x;
        }
        break;
      }
      case LUA_TSEQ: {
        for (i=left; i <= right; i++) {
          x = lua_seqrawgetinumber(L, idx, i);
          if (isnan(x)) return 1;  /* 2.10.1, changed to isnan instead of tools_isnan */
          a[++iii - 1] = x;
        }
        break;
      }
      case LUA_TREG: {  /* 3.18.8 */
        int rc;
        for (i=left; i <= right; i++) {
          x = agn_regrawgetinumber(L, idx, i, &rc);
          if (!rc) return 1;  /* 4.4.7 change */
          a[++iii - 1] = x;
        }
        break;
      }
      default: lua_assert(0);  /* should not happen */
    }
  }
  *ii = iii;
  return 0;
}


/* Tests if a value at stack index idx is a linalg.vector and returns 1 if true and 0 otherwise. It also stores
   the dimension of the vector in dim. 2.3.0 RC 4 */
LUALIB_API int agnL_islinalgvector (lua_State *L, int idx, size_t *dim) {
  int r = agn_istableutype(L, idx, "vector");
  *dim = 0;
  if (r) {
    lua_getfield(L, idx, "dim");
    *dim = agn_checknumber(L, -1);
    agn_poptop(L);
  }
  return r && (agn_asize(L, idx) == *dim);  /* check whether vector is malformed */
}


LUALIB_API void agnL_onexit (lua_State *L, int restart) {  /* 2.7.0, 2.37.0 */
  lua_lock(L);
  if (agnL_gettablefield(L, "environ", "onexit", NULL, 0) != LUA_TFUNCTION)
    agn_poptop(L);  /* remove "infolevel" (or "environ" if table environ does not exist) */
    /* do nothing */
  else
    lua_call(L, 0, 0);
  if (!restart && L) {
    lua_gc(L, LUA_GCCOLLECT, 0);  /* added 6.1.3, patched 7.6.6 for srglue/sragena. */
    if (L->C) lua_close(L->C);  /* 2.37.0, close the cache stack at final exit, not at restart.
      7.6.6 patch to prevent Valgrind warnings and segfaults with srglue/sragena. */
  }
  lua_unlock(L);
}


/* Determines an epsilon value by taking the function value f(x) into account, using a divided difference table. Also returns
   original epsilon estimate before correction in parameter origh, and the absolute error in parameter abserr.
   The function must be at index position idx.
   n - number of iterations, must be positive
   p - first index of further arguments to f
   q - last index of further arguments to f
   if p > q, no further arguments are evaluated */
LUALIB_API lua_Number agnL_fneps (lua_State *L, int fidx, lua_Number x, int n, int p, int q, lua_Number *origh, lua_Number *abserr) {  /* 2.14.3, based on math.epsilon */
  lua_Number h, qu, *a, *b, ulp, eps, r;
  int i, j, error, slots;
  a = agn_malloc(L, n * sizeof(lua_Number), "(C API function agnL_fneps)", NULL);  /* 4.11.5 fix */
  b = agn_malloc(L, n * sizeof(lua_Number), "(C API function agnL_fneps)", a, NULL);  /* 4.11.5 fix */
  ulp = sun_nextafter(x, HUGE_VAL) - x;  /* machine epsilon at x */
  eps = (fabs(x) < 7.62e-6) ? DBL_EPSILON : x * sun_cbrt(ulp);  /* 4.2.3 fix: don't return `undefined` */
  qu = 0;
  h = eps;
  slots = 2 + (p >= q)*(q - p + 1);  /* 3.15.3 fix */
  for (i=0; i < n; i++) {  /* determine appropriate epsilon step size */
    luaL_checkstack(L, slots, "not enough stack space");  /* 3.15.3 fix */
    a[i] = x + (i - (int)(n/2))*h;  /* patched 2.10.1 */
    lua_pushvalue(L, fidx);  /* changed 2.14.3 */
    lua_pushnumber(L, a[i]);
    for (j=p; j <= q; j++) lua_pushvalue(L, j);
    r = agn_ncall(L, slots - 1, &error, 0);
    if (error) {  /* 2.14.3 fix, avoid memory leaks */
      xfree(a); xfree(b);
      luaL_error(L, "Error: return of function call is not a number.");
    }
    b[i] = r;
  }
  for (j=0; j < n; j++) {  /* compute divided difference table */
    for (i=0; i < n - 1 - j; i++) {
      b[i] = (b[i + 1] - b[i])/(a[i + j + 1] - a[i]);
    }
  }
  for (i=0; i < n; i++) qu += b[i];
  qu = fabs(qu);
  if (qu < 100*eps) qu = 100*eps;  /* for second and higher derivative, results would be imprecise */
  h = sun_cbrt(eps/(2*qu));
  *origh = h;
  if (h > 100*eps) h = 100*eps;    /* for second and higher derivative, results would be imprecise */
  *abserr = fabs(h*h*qu);          /* absolute error, previously: fabs(h*h*q*100) */
  xfree(a); xfree(b);
  return h;
}


/* Reads in the file or pipe depicted by its handle f and pushes its entire contents or output as a string onto the top of the stack. The string
  `procname` indicates the name of the function from which it is called. The function also removes any carriage returns ('\r') from the output.
   If you read from a pipe, pass 1 for ispipe, and 0 otherwise.
   Note that you have to open the file or pipe before, and that the function does not close the file or pipe automatically. */
LUALIB_API void agnL_readlines (lua_State *L, FILE *fp, const char *procname, int ispipe) {  /* 2.16.10 */
  int r;
  size_t linelength, maxbufsize;
  char *line, buf[LUAL_BUFFERSIZE];  /* 2.34.9 adaption */
  luaL_Buffer b;
  if (fp == NULL)
    luaL_error(L, "Error in " LUA_QS ": internal %s error.", procname, ispipe ? "popen" : "fopen*");
  maxbufsize = 0;
  line = NULL;
  luaL_buffinit(L, &b);
  while ( (line = tools_getline(fp, buf, line, &linelength, LUAL_BUFFERSIZE, &maxbufsize, &r)) ) {  /* 2.21.5 change, 2.34.9 change */
    if (r > 0) {
      luaL_clearbuffer(&b);  /* 2.14.9 */
      luaL_error(L, "Error in " LUA_QS ": buffer allocation error.", procname);
    }
    else if (r == -1) break;  /* EOF reached and last line empty ? */
    /* delete carriage return (in DOS files) */
    if (linelength > 0 && line[linelength - 1] == '\r') {
      linelength--;
    }
    luaL_addlstring(&b, line, linelength);
    if (r == -1) break;  /* EOF reached ? */
  }
  xfree(line);
  luaL_pushresult(&b);
}


/* Executes the operating system command represented by string `str` and puts the output - a string - onto the top of the stack.
   The string `procname` indicates the name of the function from which it is called. The function also removes any carriage returns
   ('\r') from the output.
   See:  https://c-for-dummies.com/blog/?p=1418, `The Marvelous popen() Function` */
LUALIB_API int agnL_pexecute (lua_State *L, const char *str, const char *procname) {  /* 2.16.10 */
  FILE *p = popen(str, "r");
  agnL_readlines(L, p, procname, 1);
  pclose(p);
  return 1;
}


/* agnL_pairgetnumbers

For the given Agena procedure `procname`, checks whether the value at stack index `idx` is a pair. It then checks whether the
left-hand and right-hand side are numbers and returns these numbers in x and y.

If the value at `idx` is not a pair, or if at least one of its operands is not a number, it issues an error. */

LUALIB_API void agnL_pairgetinumbers (lua_State *L, const char *procname, int idx, lua_Number *x, lua_Number *y) {  /* 1.8.7, renamed 2.10.1 */
  if (!lua_ispair(L, idx)) {
    luaL_error(L, "Error in " LUA_QS ": pair expected, got %s.", procname, luaL_typename(L, idx));
  }
  agn_pairgeti(L, idx, 1);  /* push left-hand side */
  if (lua_isnumber(L, -1)) {  /* changed 2.31.5 to allow numeric strings in lhs */
    *x = lua_tonumber(L, -1);
    agn_poptop(L);  /* pop number */
  } else {
    int typeop = lua_type(L, -1);
    agn_poptop(L);  /* pop non-number */
    luaL_error(L, "Error in " LUA_QS ": number expected in left operand of pair, got %s.", procname, lua_typename(L, typeop));
  }
  agn_pairgeti(L, idx, 2);  /* push right-hand side */
  if (lua_isnumber(L, -1)) {  /* changed 2.31.5 to allow numeric strings in lhs */
    *y = agn_tonumber(L, -1);
    agn_poptop(L);  /* pop number */
  } else {
    int typeop = lua_type(L, -1);
    agn_poptop(L);  /* pop non-number */
    luaL_error(L, "Error in " LUA_QS ": number expected in right operand of pair, got %s.", procname, lua_typename(L, typeop));
  }
  if (idx < 0) lua_remove(L, idx);  /* remove pair at idx, 2.10.1 fix */
}


LUALIB_API void agnL_pairgetilongnumbers (lua_State *L, const char *procname, int idx, long double *x, long double *y) {  /* 2.34.10 */
  if (!lua_ispair(L, idx)) {
    luaL_error(L, "Error in " LUA_QS ": pair expected, got %s.", procname, luaL_typename(L, idx));
  }
  agn_pairgeti(L, idx, 1);  /* push left-hand side */
  *x = checkandgetdlongnum(L, -1);
  agn_poptop(L);  /* pop number */
  agn_pairgeti(L, idx, 2);  /* push right-hand side */
  *y = checkandgetdlongnum(L, -1);
  agn_poptop(L);  /* pop number */
  if (idx < 0) lua_remove(L, idx);  /* remove pair at idx, 2.10.1 fix */
}


LUALIB_API void agnL_pairgeticomplex (lua_State *L, const char *procname, int idx,
  lua_Number *xreal, lua_Number *ximag, lua_Number *yreal, lua_Number *yimag) {  /* 4.9.4 */
  int rc;
#ifndef PROPCMPLX
  agn_Complex z;
#else
  lua_Number z[2];
#endif
  if (!lua_ispair(L, idx)) {
    luaL_error(L, "Error in " LUA_QS ": pair expected, got %s.", procname, luaL_typename(L, idx));
  }
  agn_pairgeti(L, idx, 1);  /* push left-hand side */
#ifndef PROPCMPLX
  z = agn_tocomplexx(L, -1, &rc);
  *xreal = creal(z); *ximag = cimag(z);
#else
  agn_tocomplexx(L, -1, &rc, z);
  *xreal = z[0]; *ximag = z[1];
#endif
  agn_poptop(L);  /* pop number */
  agn_pairgeti(L, idx, 2);  /* push right-hand side */
#ifndef PROPCMPLX
  z = agn_tocomplexx(L, -1, &rc);
  *yreal = creal(z); *yimag = cimag(z);
#else
  agn_tocomplexx(L, -1, &rc, z);
  *yreal = z[0]; *yimag = z[1];
#endif
  agn_poptop(L);  /* pop number */
  if (idx < 0) lua_remove(L, idx);  /* remove pair at idx, 2.10.1 fix */
}


/* Checks for a pair of integers at stack index idx for procedure procname and returns them in x and y. If the object at idx is not
   a pair or the pair does not consist of integers, the function issues an error. If validrange is set to 1, then the function also
   checks whether x <= y and throws an error otherwise. Regardless of success or failure, the function does not change the stack.
   3.18.5 */
LUALIB_API void agnL_pairgetiintegers (lua_State *L, const char *procname, int idx, int validrange, int *x, int *y) {
  if (!lua_ispair(L, idx)) {
    luaL_error(L, "Error in " LUA_QS ": pair expected, got %s.", procname, luaL_typename(L, idx));
  }
  agn_pairgeti(L, idx, 1);  /* push left-hand side */
  if (agn_isinteger(L, -1)) {
    *x = lua_tointeger(L, -1);
    agn_poptop(L);  /* pop number */
  } else {
    int typeop = lua_type(L, -1);
    agn_poptop(L);  /* pop non-number */
    luaL_error(L, "Error in " LUA_QS ": integer expected in left operand of pair, got %s.", procname, lua_typename(L, typeop));
  }
  agn_pairgeti(L, idx, 2);  /* push right-hand side */
  if (agn_isinteger(L, -1)) {
    *y = lua_tointeger(L, -1);
    agn_poptop(L);  /* pop number */
  } else {
    int typeop = lua_type(L, -1);
    agn_poptop(L);  /* pop non-number */
    luaL_error(L, "Error in " LUA_QS ": integer expected in right operand of pair, got %s.", procname, lua_typename(L, typeop));
  }
  if (idx < 0) lua_remove(L, idx);  /* remove pair at idx */
  if (validrange && *x > *y)
    luaL_error(L, "Error in " LUA_QS ": left-hand side integer %d > right-hand side integer %d.", procname, *x, *y);
}


/* Checks for a pair of positive integers at stack index idx for procedure procname and returns them in x and y. If the object at idx
   is not a pair or the pair does not consist of positive integers, the function issues an error. If validrange is set to 1, then the
   function also checks whether x <= y and throws an error otherwise. Regardless of success or failure, the function does not change
   the stack. 3.18.5 */
LUALIB_API void agnL_pairgetiposints (lua_State *L, const char *procname, int idx, int validrange, int *x, int *y) {  /* 3.18.5 */
  if (!lua_ispair(L, idx)) {
    luaL_error(L, "Error in " LUA_QS ": pair expected, got %s.", procname, luaL_typename(L, idx));
  }
  agn_pairgeti(L, idx, 1);  /* push left-hand side */
  if (agn_isposint(L, -1)) {
    *x = lua_tointeger(L, -1);
    agn_poptop(L);  /* pop number */
  } else {
    int typeop = lua_type(L, -1);
    agn_poptop(L);  /* pop non-number */
    luaL_error(L, "Error in " LUA_QS ": positive integer expected in left operand of pair, got %s.", procname, lua_typename(L, typeop));
  }
  agn_pairgeti(L, idx, 2);  /* push right-hand side */
  if (agn_isposint(L, -1)) {
    *y = lua_tointeger(L, -1);
    agn_poptop(L);  /* pop number */
  } else {
    int typeop = lua_type(L, -1);
    agn_poptop(L);  /* pop non-number */
    luaL_error(L, "Error in " LUA_QS ": positive integer expected in right operand of pair, got %s.", procname, lua_typename(L, typeop));
  }
  if (idx < 0) lua_remove(L, idx);  /* remove pair at idx */
  if (validrange && *x > *y)
    luaL_error(L, "Error in " LUA_QS ": left-hand side integer %d > right-hand side integer %d.", procname, *x, *y);
}


/* Checks for a pair of non-negative integers at stack index idx for procedure procname and returns them in x and y. If the object at idx
   is not a pair or the pair does not consist of non-negative integers, the function issues an error. If validrange is set to 1, then the
   function also checks whether x <= y and throws an error otherwise. Regardless of success or failure, the function does not change the
   stack. 3.18.5 */
LUALIB_API void agnL_pairgetinonnegints (lua_State *L, const char *procname, int idx, int validrange, int *x, int *y) {  /* 3.18.5 */
  if (!lua_ispair(L, idx)) {
    luaL_error(L, "Error in " LUA_QS ": pair expected, got %s.", procname, luaL_typename(L, idx));
  }
  agn_pairgeti(L, idx, 1);  /* push left-hand side */
  if (agn_isnonnegint(L, -1)) {
    *x = lua_tointeger(L, -1);
    agn_poptop(L);  /* pop number */
  } else {
    int typeop = lua_type(L, -1);
    agn_poptop(L);  /* pop non-number */
    luaL_error(L, "Error in " LUA_QS ": non-negative integer expected in left operand of pair, got %s.", procname, lua_typename(L, typeop));
  }
  agn_pairgeti(L, idx, 2);  /* push right-hand side */
  if (agn_isnonnegint(L, -1)) {
    *y = lua_tointeger(L, -1);
    agn_poptop(L);  /* pop number */
  } else {
    int typeop = lua_type(L, -1);
    agn_poptop(L);  /* pop non-number */
    luaL_error(L, "Error in " LUA_QS ": non-negative integer expected in right operand of pair, got %s.", procname, lua_typename(L, typeop));
  }
  if (idx < 0) lua_remove(L, idx);  /* remove pair at idx */
  if (validrange && *x > *y)
    luaL_error(L, "Error in " LUA_QS ": left-hand side integer %d > right-hand side integer %d.", procname, *x, *y);
}


/* Returns the k'th entry in a table array, register, sequence, pair, string or numarray at stack index idx and
   pushes it onto the top of the stack. See also: agnL_gettablefield, 2.20.2, extended 2.20.3 */

LUALIB_API int agnL_geti (lua_State *L, int idx, int k) {
  switch (lua_type(L, idx)) {
    case LUA_TTABLE: {
      lua_rawgeti(L, idx, k);
      break;
    }
    case LUA_TSEQ: {
      lua_seqrawgeti(L, idx, k);
      break;
    }
    case LUA_TREG: {  /* changed 2.20.3, get top, not maxsize */
      agn_regrawgeti(L, idx, k);
      break;
    }
    case LUA_TPAIR: {  /* added 2.20.3 */
      agn_pairgeti(L, idx, k);
      break;
    }
    case LUA_TSTRING: {  /* added 2.20.3 */
      const char *str = agn_tostring(L, idx);
      lua_pushlstring(L, str + k - 1, 1);
      break;
    }
    case LUA_TUSERDATA: {
      NumArray *a = checknumarray(L, idx);
      switch (a->datatype) {
        case NAUCHAR:  lua_pushinteger(L, a->data.c[k - 1]);  break;
        case NADOUBLE: lua_pushnumber(L,  a->data.n[k - 1]);  break;
        case NAINT32:  lua_pushnumber(L,  a->data.i[k - 1]);  break;
        case NAUINT16: lua_pushnumber(L,  a->data.us[k - 1]); break;
      }
      break;
    }
    default: {
      luaL_error(L, "Error in wrong type of argument, got %s.", luaL_typename(L, idx));
    }
  }
  return 1;
}


/* takes
   - either a table, register, or a sequence of date time values of the form [yy, mm, dd, hh, mm, ss, msecs] or
   - seven numbers yy, mm, dd, hh, mm, ss, msecs
   and returns the number of seconds elapsed since the begin of the epoch (usually January 01, 1970)
   as a Time64_t value; idx must be a _positive_ index number;
   err = 1 -> issue an error, err = 0 -> just return a `boolean`: 1 = given date is valid, 0 = it is not */

static lua_Number aux_getinumber (lua_State *L, int idx, int k, const char *procname) {
  int e;
  lua_Number r;
  agnL_geti(L, idx, k);  /* gets an entry index by k from the table, sequence, register at stack index idx */
  r = agn_tonumberx(L, -1, &e);
  agn_poptop(L);
  if (e)
    luaL_error(L, "Error in " LUA_QS ": structure must contain numbers only, item %d is not.", procname, k);
  return r;
}

LUALIB_API Time64_T agnL_datetosecs (lua_State *L, int idx, const char *procname, int err, int withLSDbug, int *msecs) {  /* Agena 2.9.8/2.10.0 */
  Time64_T t;
  size_t nops;
  int year, month, day, hour, minute, second;
  year = month = day = hour = minute = second = *msecs = 0;  /* to prevent compiler warnings */
  switch (lua_type(L, idx)) {
    case LUA_TTABLE: case LUA_TSEQ: case LUA_TREG: {
      nops = agn_nops(L, idx);
      if (nops < 3)
        luaL_error(L, "Error in " LUA_QS ": structure must contain at least three numbers.", procname);
      year =   aux_getinumber(L, idx, 1, procname);  /* 5.5.4 fix */
      month =  aux_getinumber(L, idx, 2, procname);
      day =    aux_getinumber(L, idx, 3, procname);
      hour =   (nops > 3) ? aux_getinumber(L, idx, 4, procname) : 0;
      minute = (nops > 4) ? aux_getinumber(L, idx, 5, procname) : 0;
      second = (nops > 5) ? aux_getinumber(L, idx, 6, procname) : 0;
      *msecs = (nops > 6) ? aux_getinumber(L, idx, 7, procname) : 0;  /* 2.10.0 */
      break;
    }
    case LUA_TNUMBER: {  /* 5.5.7 simplification */
      nops = lua_gettop(L);
      if (nops < 3)
        luaL_error(L, "Error in " LUA_QS ": expected at least three arguments of type number.", procname);
      year =    agn_checknumber(L, idx);
      month =   agn_checknumber(L, idx + 1);
      day =     agn_checknumber(L, idx + 2);
      hour =    (nops > 3) ? agn_checknumber(L, idx + 3) : 0;
      minute =  (nops > 4) ? agn_checknumber(L, idx + 4) : 0;
      second =  (nops > 5) ? agn_checknumber(L, idx + 5) : 0;
      *msecs =  (nops > 6) ? agn_checknumber(L, idx + 6) : 0;  /* 2.10.0 */
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": expected a table, sequence, register or at least three numbers.", procname);
  }
  if (err) {
    int rc;
    t = tools_maketime(year, month, day, hour, minute, second, &rc);  /* 2.9.8 */
    if (rc == -2) {  /* invalid date */
      if (withLSDbug && year == 1900 && month == 2 && day == 29)
        return (Time64_T)(60.0 + (hour*3600.0 + minute*60.0 + second + *msecs/1000.0)/86400.0);  /* simulate LSD/ESD 1900/2/29 = 60 bug */
      else
        luaL_error(L, "Error in " LUA_QS ": time component out of range.", procname);
    }
    return t;  /* seconds are in UTC, 1970/1/1 01:00:00 CET returns 0 seoonds in UTC */
  } else
    return tools_checkdatetime(year, month, day, hour, minute, second, *msecs);  /* returns 0 or 1 */
}


LUALIB_API const char *agnL_tolstringx (lua_State *L, int idx, size_t *len, const char *procname) {  /* 2.26.1 */
  switch (lua_type(L, idx)) {
    case LUA_TNUMBER: case LUA_TSTRING: {
      size_t l;
      const char *r = lua_tolstring(L, idx, &l);
      *len = l;
      return r;
    }
    case LUA_TNIL: {
      *len = 4;
      return "null";
    }
    case LUA_TBOOLEAN: {
      int booltype = lua_toboolean(L, idx);
      if (booltype == LUA_TFAIL) {
        *len = 4;
        return "fail";
      }
      else {
        *len = (booltype) ? 4 : 5;
        return (booltype) ? "true" : "false";
      }
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": invalid argument #%d, got %s.", procname, idx, lua_typename(L, lua_type(L, idx)));
  }
  return NULL;  /* cannot happen */
}


/* Tries to convert the string at stack position idx (idx < 0) to a number and if successful replaces the string with that number,
   otherwise leaves the stack unchanged. Returns 1 if it could convert the string, and 0 otherwise. */
LUALIB_API int agnL_strtonumber (lua_State *L, int idx) {  /* 2.12.0 RC 4/2.39.1 */
  int rc, overflow;
  lua_Number num;
  rc = 0;
  if (agn_isstring(L, idx) && luaO_str2d(agn_tostring(L, idx), &num, &overflow)) {
    lua_pushnumber(L, num);
    lua_replace(L, idx - 1);
    rc = 1;
  }  /* else leave value untouched */
  return rc;
}


/* Tries to convert the string at stack position idx (idx < 0) to a complex number and if successful replaces the string with that
   complex number, otherwise leaves the stack unchanged. The complex value must be in the form a+I*b. Returns 1 if it could convert
   the string, and 0 otherwise. 3.4.9 */
LUALIB_API int agnL_strtocomplex (lua_State *L, int idx) {
  int rc;
#ifndef PROPCMPLX
  agn_Complex num;
#else
  lua_Number num[2];
#endif
  rc = 0;
#ifndef PROPCMPLX
  if (agn_isstring(L, idx) && luaO_str2c(agn_tostring(L, idx), &num)) {
    agn_createcomplex(L, num);
#else
  if (agn_isstring(L, idx) && luaO_str2c(agn_tostring(L, idx), num)) {
    agn_createcomplex(L, num[0], num[1]);
#endif
    lua_replace(L, idx - 1);
    rc = 1;
  }  /* else leave value untouched */
  return rc;
}


/* Removes the wrapping substring delim of length delimlen from the string at stack position idx and replaces it with the resulting string.
   idx must be negative. If the string could be unwrapped, returns 1 and 0 otherwise, where 0 means there was no string at idx with the
   enclosing substring delim. 2.39.x */
LUALIB_API int agnL_strunwrap (lua_State *L, int idx, const char *delim, size_t delimlen) {
  size_t l, i;
  const char *str;
  str = lua_tolstring(L, idx, &l);
  for (i=0; l > 1 && i < delimlen; i++) {
    if (str[0] == delim[i] && str[l - 1] == delim[i]) {
      lua_pushlstring(L, ++str, l - 2);
      lua_replace(L, idx - 1);
      return 1;
    }
  }
  return 0;  /* could not unwrap, do nothing */
}


LUALIB_API void agnL_issuememfileerror (lua_State *L, size_t wrongnewsize, const char *procname) {  /* 2.26.3 */
  if (wrongnewsize == 0)
    luaL_error(L, "Error in " LUA_QS ": new size is zero.", procname);
  else
    luaL_error(L, "Error in " LUA_QS ": cannot resize buffer.", procname);
}


/* Creates a C double array and puts all numbers in the table, sequence or register at index idx into it. If a
   non-number shall included in the structure, an error is issued. See also agnL_fillarray. Free the returned array
   after usage. 2.27.5 */
#define checkfornumber(L,idx,rc,a,procname) { \
  if (rc == 0) { \
    xfree(a); \
    luaL_error(L, "Error in " LUA_QS ": %s contains a non-number.", procname, luaL_typename(L, idx)); \
  } \
}

LUALIB_API lua_Number *agnL_tonumarray (lua_State *L, int idx, size_t *size, const char *procname, int duplicate, int throwerr) {
  lua_Number *a, x2;
  size_t n, i, ii;
  int rc;
  ii = 0;
  a = NULL;  /* to prevent compiler warnings */
  switch (lua_type(L, idx)) {
    case LUA_TTABLE: {
      n = agn_asize(L, idx);
      agn_createarray(a, n, procname);
      if (a == NULL) { *size = 0; return NULL; }
      if (throwerr) {
        for (i=1; i <= n; i++) {
          x2 = agn_rawgetinumber(L, idx, i, &rc);  /* returns 0 if a value is non-numeric */
          checkfornumber(L, idx, rc, a, procname);
          a[i - 1] = x2;
        }
        ii = n;
      } else {
        for (i=1; i <= n; i++) {
          x2 = agn_getinumber(L, idx, i);  /* returns 0 if a value is non-numeric */
          if (!tools_isnan(x2)) a[++ii - 1] = x2;  /* 2.5.2 patch */
        }
      }
      break;
    }
    case LUA_TSEQ: {
      n = agn_seqsize(L, idx);
      agn_createarray(a, n, procname);
      if (a == NULL) { *size = 0; return NULL; }
      if (throwerr) {
        for (i=1; i <= n; i++) {
          x2 = agn_seqrawgetinumber(L, idx, i, &rc);  /* returns 0 if a value is non-numeric */
          checkfornumber(L, idx, rc, a, procname);
          a[i - 1] = x2;
        }
        ii = n;
      } else {
        for (i=1; i <= n; i++) {
          x2 = lua_seqrawgetinumber(L, idx, i);
          if (!tools_isnan(x2)) a[++ii - 1] = x2;
        }
      }
      break;
    }
    case LUA_TREG: {
      n = agn_regsize(L, idx);
      agn_createarray(a, n, procname);
      if (a == NULL) { *size = 0; return NULL; }
      if (throwerr) {
        for (i=1; i <= n; i++) {
          x2 = agn_regrawgetinumber(L, idx, i, &rc);  /* returns 0 if a value is non-numeric */
          checkfornumber(L, idx, rc, a, procname);
          a[i - 1] = x2;
        }
        ii = n;
      } else {
        for (i=1; i <= n; i++) {
          x2 = agn_reggetinumber(L, idx, i);
          if (!tools_isnan(x2)) a[++ii - 1] = x2;
        }
      }
      break;
    }
    case LUA_TUSERDATA: {
      NumArray *b = checknumarray(L, 1);
      if (b->datatype != NADOUBLE)
        luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", procname);
      if (duplicate) {  /* duplicate array, 2.18.1 extension */
        size_t i;
        *size = b->size;
        lua_Number *c = malloc(*size * sizeof(lua_Number));
        if (c == NULL)
          luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
        for (i=0; i < *size; i++) c[i] = b->data.n[i];
        return c;  /* explicitly return the new array here and FREE it later */
      }
      a = b->data.n;
      ii = b->size;
      break;
    }
    default: {
      luaL_error(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.", procname, luaL_typename(L, idx));
    }
  }
  *size = ii;
  return a;  /* FREE it in calling function */
}


/* for sequences.new (formerly nseq), registers.new (formerly nreg), tables.new, moved from lbaselib.c 2.28.1 */
LUALIB_API void luaL_aux_nstructure (lua_State *L, const char *fn, size_t *nargs, size_t *offset,
    lua_Number *a, lua_Number *b, volatile lua_Number *step, lua_Number *eps, size_t *total,
    int *isfunc, int *isdefault, int *isint) {
  int matlab, hasstep;
  *nargs = lua_gettop(L);
  matlab = lua_isboolean(L, 1);  /* 2.4.1 change */
  *isfunc = lua_isfunction(L, 1 + matlab);  /* 2.4.1 change */
  *offset = matlab + *isfunc;  /* 2.4.1 change */
  *isdefault = agn_isinteger(L, 1) && lua_ispair(L, 2);
  if (*isdefault) {
    *a = 1; *b = agn_tointeger(L, 1);
    hasstep = 0;
    *step = 1;
  } else {
    *a = agn_checknumber(L, 1 + *offset);
    *b = agn_checknumber(L, 2 + *offset);
    *step = agnL_optnumber(L, 3 + *offset, 1);
    if (*step <= 0)  /* 2.28.1 fix */
      luaL_error(L, "Error in " LUA_QS ": %s must be positive.", fn, (matlab) ? "number of elements" : "step size");
    hasstep = agn_isnumber(L, 3 + *offset) && (*step != 1.0);  /* 2.28.1 fix */
  }
  *eps = agn_gethepsilon(L);  /* 2.4.1 change, 2.35.0 change to hEps */
  *isint = tools_isint(*step) && tools_isint(*a) && tools_isint(*b);  /* 2.12.2, 2.14.13 */
  if (matlab) {  /* 2.4.1 change */
    if (*a >= *b)
      luaL_error(L, "Error in " LUA_QS ": left border greater or equals right border.", fn);
    if (!(matlab) && (*step < 2 || tools_isfrac(*step)))
      luaL_error(L, "Error in " LUA_QS ": expected a positive integer > 1 for argument #%d.", fn, 3 + *offset);
    *step = (hasstep) ? fabs(*b - *a)/(*step - 1) : 1;
  } else if (*a > *b) {
    luaL_error(L, "Error in " LUA_QS ": start value greater than stop value.", fn);
  }
  *total = tools_numiters(*a, *b, *step);  /* total number of iterations, do not use floor, (int), etc ! 2.12.2 & 2.28.1 & 2.35.0 fix */
}


LUALIB_API char *agnL_strmatch (lua_State *L, const char *data, int datalen, const char *pattern, int patternlen) {  /* 2.37.7 */
  char *lookup;
  if (!tools_hasstrchr(pattern, SPECIALS, patternlen)) {
    lookup = strstr(data, pattern);
  } else {
    ptrdiff_t start, end;
    lookup = (char *)agn_strmatch(L, data, datalen, pattern, 1, &start, &end);
  }
  return lookup;
}


/* Checks whether a string represents a valid pattern and return 1 if so and 0 with a malformed pattern.
   The function has been introduced to check user-defined patterns in functions that malloc memory
   and where calls to match() might throw errors later, causing memory leaks if not properly freed,
   which sometimes is only very hard to accomplish, if at all. 6.1.2 */
static int auxL_testpattern (lua_State *L) {
  size_t l;
  const char *p = luaL_checklstring(L, 1, &l);
  MatchState ms;
  initMatchState(L, &ms, 0, "", 0);
  ms.level = 0;
  match(&ms, "", p, 1);
  return 0;
}

LUALIB_API int agnL_isvalidpattern (lua_State *L, const char *p, size_t l, int *hasspecials, int pusherror) {
  int status;
  if (!tools_hasstrchr(p, SPECIALS, l)) {
    *hasspecials = 0;
    return 1;
  }
  *hasspecials = 1;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushcfunction(L, auxL_testpattern);
  lua_pushstring(L, p);
  status = lua_pcall(L, 1, 0, 0);
  if (status != 0 && !pusherror) {  /* there is an error message on the stack, remove it */
    lua_pop(L, 1);
  }
  return status == 0;
}


/* Checks whether the object at the given acceptable stack index is either a function or a structure with a '__call' metamethod. It
   returns 1 if so and 0 otherwise. The function does not change the stack. */
LUALIB_API int agnL_iscallable (lua_State *L, int idx) {  /* 3.1.0 */
  int rc = 1;
  if (!lua_isfunction(L, idx)) {
    rc = agnL_getmetafield(L, idx, "__call");  /* pushes '__call' metatable if present */
    if (rc) agn_poptop(L);  /* if found, pop '__call' metatable */
  }
  return rc;
}


LUALIB_API lua_Number luaL_str2d (lua_State *L, const char *s, int *overflow) {  /* 3.5.1, 3.6.0 added L param due to whatever
  missing entry point problem sometimes observed in Windows 11. */
  lua_Number r;
  (void)L;
  luaO_str2d(s, &r, overflow);
  return r;
}


LUALIB_API int agnL_newldblarray (lua_State *L, size_t n, const char *procname) {  /* creates numeric C array userdata object */
  LongDoubleArray *a;
  if (n < 1)  /* 2.35.1 fix */
    luaL_error(L, "Error in " LUA_QS ": structure is empty.", procname);
  a = (LongDoubleArray *)lua_newuserdata(L, sizeof(LongDoubleArray));  /* push userdata on stack */
  a->v = calloc(n, sizeof(long double));
  if (!a->v)  /* 2.35.1 fix */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
  a->size = n;
  /* attach proper metatable with __gc method to userdata; name of mt name must be different from the one of
     the numarray package to avoid crashes during gc. */
  luaL_getmetatable(L, "calc");
  lua_setmetatable(L, -2);
  return 1;  /* leave userdata on the top of the stack */
}

#define aux_pushud(L,a,nops,procname) { \
  if (nops > 256) \
    luaL_error(L, "Error in " LUA_QS ": received too many coefficients.", procname); \
  agnL_newldblarray(L, nops, procname); \
  a = (LongDoubleArray *)lua_touserdata(L, -1); \
}

/* 6.6.5: Takes a structure of numbers or long double userdata residing at stack index 1 and pushes onto the stack
   a userdata LongDoubleArray. Instead of a structure, numbers or long double userdata might be given as separarate arguments. */

LUALIB_API void agnL_pushcoeffs (lua_State *L, LongDoubleArray *a, size_t nops, const char *procname) {
  int i;  /* 7.9.5 change from size_t to i to avoid compiler warnings */
  switch (lua_type(L, 1)) {
    case LUA_TTABLE : {
      nops = agn_asize(L, 1);
      aux_pushud(L, a, nops, procname);
      for (i=0; i < nops; i++) {
        lua_rawgeti(L, 1, i + 1);
        setdblarray(a, i, agnL_checkdlongnum(L, -1, procname));
        agn_poptop(L);
      }
      break;
    }
    case LUA_TSEQ: {
      nops = agn_seqsize(L, 1);
      aux_pushud(L, a, nops, procname);
      for (i=0; i < nops; i++) {
        lua_seqrawgeti(L, 1, i + 1);
        setdblarray(a, i, agnL_checkdlongnum(L, -1, procname));
        agn_poptop(L);
      }
      break;
    }
    case LUA_TREG: {
      nops = agn_regsize(L, 1);
      aux_pushud(L, a, nops, procname);
      for (i=0; i < nops; i++) {
        agn_regrawgeti(L, 1, i + 1);
        setdblarray(a, i, agnL_checkdlongnum(L, -1, procname));
        agn_poptop(L);
      }
      break;
    }
    default: {
      aux_pushud(L, a, nops, procname);
      for (i=0; i < nops; i++) {  /* register coefficients in *a */
        setdblarray(a, i, agnL_checkdlongnum(L, i + 1, procname));
      }
    }
  }
}


/* Takes the integer residing at stack index idx and pushes the hexadecimal representation of this number as a string
   onto the stack. The integer at idx will be removed if idx is negative. 6.7.8 */
LUALIB_API void agnL_pushhex (lua_State *L, int idx, int classic) {
  int n, h, t, lt10;
  Charbuf buf;
  n = 0;
  h = agn_checknonnegint(L, idx);
  if (idx < 0) { lua_remove(L, idx); }
  charbuf_init(L, &buf, 0, 6, 1);
  if (h == 0)
    charbuf_appendchar(L, &buf, '0');
  while (h != 0) {
    t = h % 16;
    lt10 = t < 10;
    t += lt10*48 + (!lt10)*55;
    charbuf_appendchar(L, &buf, classic ? t : ltolower(t));
    h /= 16;
    n++;
  }
  if ((n & 1) || n == 0) charbuf_appendchar(L, &buf, '0');
  charbuf_reverse(L, &buf, buf.size);
  if (!classic) charbuf_prepend(L, &buf, "0x", 2);
  charbuf_finish(&buf);  /* better sure than sorry */
  lua_pushlstring(L, buf.data, buf.size);
  charbuf_free(&buf);
}


/* *** Compatibility with Lua 5.4.0 ********************************************************************************************* */

/* Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack (below optional upvalues, see next).

   When nup is not zero, all functions are created with nup upvalues, initialized with copies of the nup values previously pushed
   on the stack on top of the library table. These values are popped from the stack after the registration.

   Taken from Lua 5.4.0 RC 2, 2.21.2

** Set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/

LUALIB_API void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* place holder? */
      lua_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        lua_pushvalue(L, -nup);
      lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);  /* remove upvalues */
}


/* Ensures that the value t[fname], where t is the value at index idx, is a table, and pushes that table onto the stack.
   otherwise creates a new table, assigned to t[fname], and pushes it onto the top of the stack. Returns true if it finds
   a previous table and false if it creates a new table.

** ensure that stack[idx][fname] has a table and push that table
** into the stack. Taken from Lua 5.4.0 RC 2, 2.21.2
*/

LUALIB_API int luaL_getsubtable (lua_State *L, int idx, const char *fname) {
  if (lua_getfield(L, idx, fname) == LUA_TTABLE)
    return 1;  /* table already there */
  else {
    lua_pop(L, 1);  /* remove previous result */
    idx = lua_absindex(L, idx);
    lua_newtable(L);
    lua_pushvalue(L, -1);  /* copy to be left at top */
    lua_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


LUALIB_API int luaL_fileresult (lua_State *L, int stat, const char *fname) {  /* taken from Lua 5.4.0 */
  int en = errno;  /* calls to Lua API may change this value */
  if (stat) {
    lua_pushboolean(L, 1);
    return 1;
  }
  else {
    lua_pushnil(L);
    if (fname)
      lua_pushfstring(L, "%s: %s", fname, strerror(en));
    else
      lua_pushstring(L, strerror(en));
    lua_pushinteger(L, en);
    return 3;
  }
}


#if !defined(inspectstat)   /* { */
#if defined(LUA_USE_POSIX)
#include <sys/wait.h>
/* use appropriate macros to interpret 'pclose' return status */
#define inspectstat(stat,what) \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }
#else
#define inspectstat(stat,what)  /* no op */
#endif
#endif                     /* } */

LUALIB_API int luaL_execresult (lua_State *L, int stat) {
  const char *what = "exit";  /* type of termination */
  if (stat == -1)  /* error? */
    return luaL_fileresult(L, 0, NULL);
  else {
    inspectstat(stat, what);  /* interpret result */
    /* successful termination? changed 2.22.0 */
    lua_pushboolean(L, (*what == 'e' && stat != -1));
    /* lua_pushstring(L, what); */
    lua_pushinteger(L, stat);
    return 2;  /* return true/nil,what,code */
  }
}

/* end of compatibility with Lua 5.4.0 */

/* *** Compatibility with Lua 5.2 *********************************************************************************************** */

/* taken from lcurses-9.0.0.1 library, files /ext/include/compat-5.2.*; Agena 2.27.1 */

LUALIB_API const char *luaL_tolstring (lua_State *L, int idx, size_t *len) {
  if (!luaL_callmeta(L, idx, "__tostring")) {
    int t = lua_type(L, idx);
    switch (t) {
      case LUA_TNIL:
        lua_pushliteral(L, "null");
        break;
      case LUA_TSTRING:
      case LUA_TNUMBER:
        lua_pushvalue(L, idx);
        break;
      case LUA_TBOOLEAN:
        if (lua_toboolean(L, idx))
          lua_pushliteral(L, "true");
        else
          lua_pushliteral(L, "false");
        break;
      default:
        lua_pushfstring(L, "%s: %p", lua_typename(L, t),
                                     lua_topointer(L, idx));
        break;
    }
  }
  return lua_tolstring(L, -1, len);
}


LUALIB_API int luaL_len (lua_State *L, int i) {
  int res = 0, isnum = 0;
  luaL_checkstack(L, 1, "not enough stack space");
  lua_len(L, i);
  res = (int)lua_tointegerx(L, -1, &isnum);
  lua_pop(L, 1);
  if (!isnum)
    luaL_error(L, "object length is not a number");
  return res;
}


LUALIB_API void luaL_setmetatable (lua_State *L, const char *tname) {
  luaL_checkstack(L, 1, "not enough stack space");
  luaL_getmetatable(L, tname);
  lua_setmetatable(L, -2);
}


LUALIB_API void *luaL_testudata (lua_State *L, int i, const char *tname) {
  void *p = lua_touserdata(L, i);
  luaL_checkstack(L, 2, "not enough stack space");
  if (p == NULL || !lua_getmetatable(L, i))
    p = NULL;
  else {
    int rostatus, res = 0;
    rostatus = agn_udfreeze(L, i, -1);  /* 5.3.0 frozen fix */
    if (rostatus) agn_udfreeze(L, i, 0);
    luaL_getmetatable(L, tname);
    res = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    if (!res)
      p = NULL;
    if (rostatus) agn_udfreeze(L, i, 1);  /* reset read-only status to its original state */
  }
  return p;
}


static int countlevels (lua_State *L) {
  lua_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (lua_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (lua_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}

static int findfield (lua_State *L, int objidx, int level) {
  if (level == 0 || !lua_istable(L, -1))
    return 0;  /* not found */
  lua_pushnil(L);  /* start 'next' loop */
  while (lua_next(L, -2)) {  /* for each pair in table */
    if (lua_type(L, -2) == LUA_TSTRING) {  /* ignore non-string keys */
      if (lua_rawequal(L, objidx, -1)) {  /* found object? */
        lua_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        lua_remove(L, -2);  /* remove table (but keep name) */
        lua_pushliteral(L, ".");
        lua_insert(L, -2);  /* place '.' between the two names */
        lua_concat(L, 3);
        return 1;
      }
    }
    lua_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}

static int pushglobalfuncname (lua_State *L, lua_Debug *ar) {
  int top = lua_gettop(L);
  lua_getinfo(L, "f", ar);  /* push function */
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  if (findfield(L, top + 1, 2)) {
    lua_copy(L, -1, top + 1);  /* move name to proper place */
    lua_pop(L, 2);  /* remove pushed values */
    return 1;
  }
  else {
    lua_settop(L, top);  /* remove function and global table */
    return 0;
  }
}

static void pushfuncname (lua_State *L, lua_Debug *ar) {
  if (*ar->namewhat != '\0')  /* is there a name? */
    lua_pushfstring(L, "function " LUA_QS, ar->name);
  else if (*ar->what == 'm')  /* main? */
      lua_pushliteral(L, "main chunk");
  else if (*ar->what == 'C') {
    if (pushglobalfuncname(L, ar)) {
      lua_pushfstring(L, "function " LUA_QS, lua_tostring(L, -1));
      lua_remove(L, -2);  /* remove name */
    }
    else
      lua_pushliteral(L, "?");
  }
  else
    lua_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
}

#define LEVELS1 12  /* size of the first part of the stack */
#define LEVELS2 10  /* size of the second part of the stack */

LUALIB_API void luaL_traceback (lua_State *L, lua_State *L1,
                                const char *msg, int level) {
  lua_Debug ar;
  int top = lua_gettop(L);
  int numlevels = countlevels(L1);
  int mark = (numlevels > LEVELS1 + LEVELS2) ? LEVELS1 : 0;
  if (msg) lua_pushfstring(L, "%s\n", msg);
  lua_pushliteral(L, "stack traceback:");
  while (lua_getstack(L1, level++, &ar)) {
    if (level == mark) {  /* too many levels? */
      lua_pushliteral(L, "\n\t...");  /* add a '...' */
      level = numlevels - LEVELS2;  /* and skip to last ones */
    }
    else {
      lua_getinfo(L1, "Slnt", &ar);
      lua_pushfstring(L, "\n\t%s:", ar.short_src);
      if (ar.currentline > 0)
        lua_pushfstring(L, "%d:", ar.currentline);
      lua_pushliteral(L, " in ");
      pushfuncname(L, &ar);
      lua_concat(L, lua_gettop(L) - top);
    }
  }
  lua_concat(L, lua_gettop(L) - top);
}


LUALIB_API void luaL_requiref (lua_State *L, char const* modname,
                    lua_CFunction openf, int glb) {
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushcfunction(L, openf);
  lua_pushstring(L, modname);
  lua_call(L, 1, 1);
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "loaded");
  lua_replace(L, -2);
  lua_pushvalue(L, -2);
  lua_setfield(L, -2, modname);
  lua_pop(L, 1);
  if (glb) {
    lua_pushvalue(L, -1);
    lua_setglobal(L, modname);
  }
}


/*********************************************************************
* This file contains source code from 5.2 and Lua 5.4:
*
* Copyright (C) 1994-2025 Lua.org, PUC-Rio.
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
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*********************************************************************/

/* *** Compatibility for luaposix integration *********************************************************************************** */

/* POSIX library for Lua 5.1, 5.2, 5.3 & 5.4.
 * Copyright (C) 2013-2022 Gary V. Vaughan
 * Copyright (C) 2010-2013 Reuben Thomas <rrt@sc3d.org>
 * Copyright (C) 2008-2010 Natanael Copa <natanael.copa@gmail.com>
 * Clean up and bug fixes by Leo Razoumov <slonik.az@gmail.com> 2006-10-11
 * Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br> 07 Apr 2006 23:17:49
 * Based on original by Claudio Terra for Lua 3.x.
 * With contributions by Roberto Ierusalimschy.
 * With documentation from Steve Donovan 2012
 */

LUALIB_API void luaL_checknargs (lua_State *L, int maxargs) {  /* 2.28.2 */
  int nargs = lua_gettop(L);
  lua_pushfstring(L, "no more than %d argument%s expected, got %d",
    maxargs, maxargs == 1 ? "" : "s", nargs);
  luaL_argcheck(L, nargs <= maxargs, maxargs + 1, lua_tostring(L, -1));
  lua_pop(L, 1);
}

static int argtypeerror(lua_State *L, int narg, const char *expected) {  /* 2.28.2 */
  const char *got = luaL_typename(L, narg);
  return luaL_argerror(L, narg,
    lua_pushfstring(L, "%s expected, got %s", expected, got));
}

LUALIB_API void luaL_checktypex (lua_State *L, int narg, int t, const char *expected) {  /* 2.28.2 */
  if (lua_type(L, narg) != t)
    argtypeerror(L, narg, expected);
}

LUALIB_API int luaL_pusherror (lua_State *L, const char *info) {  /* 2.28.2 */
  lua_pushnil(L);
  if (info == NULL)
    lua_pushstring(L, strerror(errno));
  else
    lua_pushfstring(L, "%s: %s", info, strerror(errno));
  lua_pushinteger(L, errno);
  return 3;
}


/* Taken from http://lua-users.org/files/wiki_insecure/users/rici/luaref.c, written by Rici Lake for Lua 5.x; 2.36.1 */

/* Creates a new C reference to the object at stack index idx. */
LUALIB_API luaRef *luaL_newref (lua_State *L, int idx) {
  void *self = malloc(1);
  lua_pushlightuserdata(L, self);
  lua_pushvalue(L, idx);
  lua_settable(L, LUA_REGISTRYINDEX);
  return (luaRef*)self;
}


/* Pushes a referenced object onto the stack. */
LUALIB_API void luaL_pushref (lua_State *L, luaRef *r) {
  lua_pushlightuserdata(L, (void*)r);
  lua_gettable(L, LUA_REGISTRYINDEX);
}


/* Frees a reference. */
LUALIB_API void luaL_freeref (lua_State *L, luaRef *r) {
  lua_pushlightuserdata(L, (void*)r);
  lua_pushnil(L);
  lua_settable(L, LUA_REGISTRYINDEX);
  xfree(r);
}

