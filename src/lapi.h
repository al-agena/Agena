/*
** $Id: lapi.h,v 2.2 2005/04/25 19:24:10 roberto Exp $
** Auxiliary functions from Lua/Agena API
** See Copyright Notice in agena.h
*/

#ifndef lapi_h
#define lapi_h

#include "lobject.h"  /* for TValue, etc. definitions */

LUA_API void luaA_pushobject (lua_State *L, const TValue *o);  /* used by `debug` package */
LUA_API void luaA_pushgcobject (lua_State *L, GCObject *o);    /* used by `environ` package */
LUA_API TValue *index2adr (lua_State *L, int idx);

/* from Lua 5.4.0, unused */
#define codeNresults(n)          (-(n) - 3)
#define hastocloseCfunc(n)       ((n) < LUA_MULTRET)

#endif
