/*
** $Id: bags.c,v 0.1 26.07.2012 $
** Multiset Library
** See Copyright Notice in agena.h
*/

#include <stdio.h>

#define bags_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnxlib.h"

static int bags_new (lua_State *L) {
  int nargs, i;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  lua_createtable(L, 0, nargs);  /* 1.7.10 */
  agn_setutypestring(L, -1, "bag");
  lua_setmetatabletoobject(L, -1, "bags", 0);  /* assign the metatable defined below, simplified 5.2.5 */
  for (i=0; i < nargs; i++) {
    lua_pushvalue(L, i + 1);
    if (lua_isnil(L, -1)) {  /* 2.10.0 */
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": `null` cannot be added.", "bags.new");
    }
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {  /* element not yet in bag ? */
      agn_poptop(L);  /* pop null */
      lua_pushvalue(L, i + 1);
      lua_pushnumber(L, 1);
      lua_settable(L, -3);
    } else {
      if (!agn_isnumber(L, -1)) {
        agn_poptop(L);  /* pop anything */
        luaL_error(L, "Error in " LUA_QS ": invalid bag.", "bags.new");
      } else {
        lua_pushvalue(L, i + 1);
        lua_pushnumber(L, agn_tonumber(L, -2) + 1);
        lua_settable(L, -4);
        agn_poptop(L);  /* drop number */
      }
    }
  }
  return 1;
}


static int bags_include (lua_State *L) {
  int nargs, i;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": expected at least two arguments (a bag and an element).", "bags.include");
  else if (!(lua_istable(L, 1) && agn_isutype(L, 1, "bag"))) {  /* 2.2.6 */
    luaL_error(L, "Error in " LUA_QS ": structure is not a bag.", "bags.include");
  }
  for (i=2; i <= nargs; i++) {
    lua_pushvalue(L, i);
    if (lua_isnil(L, -1)) {  /* 2.10.0 */
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": `null` cannot be added.", "bags.include");
    }
    lua_gettable(L, 1);
    if (lua_isnil(L, -1)) {  /* element not yet in bag ? */
      agn_poptop(L);  /* pop null */
      lua_pushvalue(L, i);
      lua_pushinteger(L, 1);
      lua_settable(L, 1);
    } else {
      if (agn_isnumber(L, -1)) {
        lua_pushvalue(L, i);
        lua_pushinteger(L, agn_tointeger(L, -2) + 1);
        lua_settable(L, 1);
        agn_poptop(L);  /* drop number */
      } else {
        agn_poptop(L);  /* pop anything */
        luaL_error(L, "Error in " LUA_QS ": invalid bag.", "bags.include");
      }
    }
  }
  return 0;
}


static int bags_minclude (lua_State *L) {  /* based on bags.include, extended 3.13.1 */
  int nargs, i;
  nargs = lua_gettop(L);
  if (nargs != 2)
    luaL_error(L, "Error in " LUA_QS ": expected at least two arguments (a bag and a sequence).", "bags.minclude");
  else if (!(lua_istable(L, 1) && agn_isutype(L, 1, "bag"))) {  /* 2.2.6 */
    luaL_error(L, "Error in " LUA_QS ": first argument is not a bag.", "bags.minclude");
  }
  luaL_checkstack(L, 3, "not enough stack space");
  switch (lua_type(L, 2)) {
    case LUA_TTABLE: {
      lua_pushnil(L);
      while (lua_next(L, 2)) {
        lua_pushvalue(L, -1);  /* duplicate value */
        lua_gettable(L, 1);    /* drops duplicated value */
        if (lua_isnil(L, -1)) {  /* element not yet in bag ? */
          agn_poptop(L);  /* pop null */
          lua_pushinteger(L, 1);
          lua_settable(L, 1);  /* drops value and counter, leaves key for next iteration */
        } else if (agn_isnumber(L, -1)) {
          int c = agn_tointeger(L, -1);
          agn_poptop(L);  /* drop number */
          lua_pushinteger(L, c + 1);
          lua_settable(L, 1);  /* pops value and counter, leaves key for next iteration */
        } else {
          lua_pop(L, 3);  /* clear the stack */
          luaL_error(L, "Error in " LUA_QS ": invalid bag.", "bags.minclude");
        }
      }
      break;
    }
    case LUA_TSEQ: {
      for (i=1; i <= agn_seqsize(L, 2); i++) {
        lua_seqrawgeti(L, 2, i);
        lua_gettable(L, 1);
        if (lua_isnil(L, -1)) {  /* element not yet in bag ? */
          agn_poptop(L);  /* pop null */
          lua_seqrawgeti(L, 2, i);
          lua_pushinteger(L, 1);
          lua_settable(L, 1);
        } else if (agn_isnumber(L, -1)) {
          lua_seqrawgeti(L, 2, i);
          lua_pushinteger(L, agn_tointeger(L, -2) + 1);
          lua_settable(L, 1);
          agn_poptop(L);  /* drop number */
        } else {
          agn_poptop(L);  /* pop anything */
          luaL_error(L, "Error in " LUA_QS ": invalid bag.", "bags.minclude");
        }
      }
      break;
    }
    case LUA_TREG: {
      for (i=1; i <= agn_regsize(L, 2); i++) {
        agn_regrawgeti(L, 2, i);
        lua_gettable(L, 1);
        if (lua_isnil(L, -1)) {  /* element not yet in bag ? */
          agn_poptop(L);  /* pop null */
          agn_regrawgeti(L, 2, i);
          lua_pushinteger(L, 1);
          lua_settable(L, 1);
        } else if (agn_isnumber(L, -1)) {
          agn_regrawgeti(L, 2, i);
          lua_pushinteger(L, agn_tointeger(L, -2) + 1);
          lua_settable(L, 1);
          agn_poptop(L);  /* drop number */
        } else {
          agn_poptop(L);  /* pop anything */
          luaL_error(L, "Error in " LUA_QS ": invalid bag.", "bags.minclude");
        }
      }
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected for argument #2, got %s.", "bags.minclude",
        luaL_typename(L, 2));
  }
  return 0;
}


static int bags_remove (lua_State *L) {
  int nargs, i;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": expected at least two arguments (a bag and an element).", "bags.remove");
  else if (!(lua_istable(L, 1) && agn_isutype(L, 1, "bag"))) {  /* 2.2.6 */
    luaL_error(L, "Error in " LUA_QS ": structure is not a bag.", "bags.remove");
  }
  for (i=2; i <= nargs; i++) {  /* 2.2.6 fix */
    lua_pushvalue(L, i);
    if (lua_isnil(L, -1)) {  /* 2.10.0 */
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": `null` cannot be removed.", "bags.remove");
    }
    lua_gettable(L, 1);  /* pops the element */
    if (lua_isnil(L, -1)) {  /* element not in bag ? */
      agn_poptop(L);  /* pop null and ignore it */
    } else if (agn_isnumber(L, -1)) {
      int c = agn_tointeger(L, -1);
      agn_poptop(L);  /* pop number */
      lua_pushvalue(L, i);  /* push key (element) */
      /* now push number of occurrences or delete element */
      if (c == 1)
        lua_pushnil(L);
      else
        lua_pushinteger(L, c - 1);
      lua_settable(L, 1);
    } else {
      agn_poptop(L);  /* pop anything */
      luaL_error(L, "Error in " LUA_QS ": invalid bag.", "bags.remove");
    }
  }
  return 0;
}


static int bags_bagtoset (lua_State *L) {
  if (!(lua_istable(L, 1) && agn_isutype(L, 1, "bag"))) {
    luaL_error(L, "Error in " LUA_QS ": structure is not a bag.", "bags.bagtoset");
  }
  agn_createset(L, agn_size(L, 1));
  lua_pushnil(L);
  while (lua_next(L, 1)) {
    lua_pushvalue(L, -2);  /* put table key at top of the stack */
    lua_srawset(L, -4);    /* put key into set and pop it */
    agn_poptop(L);         /* remove value */
  }
  return 1;
}


static int bags_attrib (lua_State *L) {
  size_t c = 0;
  if (!(lua_istable(L, 1) && agn_isutype(L, 1, "bag"))) {  /* 2.2.6 */
    luaL_error(L, "Error in " LUA_QS ": structure is not a bag.", "bags.attrib");
  }
  lua_pushinteger(L, agn_size(L, 1));  /* number of unique elements */
  lua_pushnil(L);
  while (lua_next(L, 1)) {
    c += luaL_checknumber(L, -1);
    agn_poptop(L);  /* remove value */
  }
  lua_pushinteger(L, c);  /* accumulated number of all occurrences of the elements in a bag */
  return 2;
}


static int bags_getsize (lua_State *L) {  /* 2.18.0 */
  if (!(lua_istable(L, 1) && agn_isutype(L, 1, "bag"))) {
    luaL_error(L, "Error in " LUA_QS ": structure is not a bag.", "bags.getsize");
  }
  lua_pushnumber(L, agn_size(L, 1));  /* number of unique elements */
  return 1;
}


static int mt_empty (lua_State *L) {  /* 2.18.0 */
  if (!(lua_istable(L, 1) && agn_isutype(L, 1, "bag"))) {
    luaL_error(L, "Error in " LUA_QS ": structure is not a bag.", "bags.__empty");
  }
  lua_pushboolean(L, agn_size(L, 1) == 0);  /* 4.7.2 fix */
  return 1;
}


static int mt_filled (lua_State *L) {  /* 2.18.0 */
  if (!(lua_istable(L, 1) && agn_isutype(L, 1, "bag"))) {
    luaL_error(L, "Error in " LUA_QS ": structure is not a bag.", "bags.__filled");
  }
  lua_pushboolean(L, agn_size(L, 1) != 0);  /* 4.7.2 fix */
  return 1;
}


static const struct luaL_Reg bags_bagslib [] = {  /* metamethods for bags */
  {"attrib",   bags_attrib},
  {"bagtoset", bags_bagtoset},
  {"getsize",  bags_getsize},
  {"include",  bags_include},
  {"minclude", bags_minclude},
  {"remove",   bags_remove},
  {"__size",   bags_getsize},     /* retrieve the number of unique elements */
  {"__empty",  mt_empty},         /* metamethod for `empty` operator */
  {"__filled", mt_filled},        /* metamethod for `filled` operator */
  {NULL, NULL}
};


static const luaL_Reg bagslib[] = {
  {"attrib",   bags_attrib},          /* August 14, 2012 */
  {"bagtoset", bags_bagtoset},        /* July 27, 2012 */
  {"getsize",  bags_getsize},         /* March 27, 2020 */
  {"include",  bags_include},         /* July 26, 2012 */
  {"minclude", bags_minclude},        /* September 03, 2013 */
  {"new",      bags_new},             /* July 26, 2012 */
  {"remove",   bags_remove},          /* July 27, 2012 */
  {NULL, NULL}
};


/*
** Open bags library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_BAGSLIBNAME);  /* create metatable */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, bags_bagslib);  /* methods */
}

LUALIB_API int luaopen_bags (lua_State *L) {
  /* metamethods */
  createmeta(L);
  /* register library */
  luaL_register(L, AGENA_BAGSLIBNAME, bagslib);
  return 1;
}

