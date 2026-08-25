/*
** $Id: lrtable.c,v 1.00 17.12.2010 alex Exp $
** library for remember table administration
** See Copyright Notice in agena.h
*/


#include <stdlib.h>

#define lrtable_c
#define LUA_LIB

#include "agena.h"

#include "agnxlib.h"
#include "agenalib.h"


/* initialises an rtable with an empty table */
static int rtable_init (lua_State *L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  agn_creatertable(L, 1, 1);  /* 1 = RETURN statement can update the rtable */
  lua_pushnil(L);
  return 1;
}


/* 0.22.0: initialises a defaults rtable with an empty table */
static int rtable_roinit (lua_State *L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  agn_creatertable(L, 1, 0);  /* 0 = RETURN statement canNOT update the rtable */
  lua_pushnil(L);
  return 1;
}


/* returns the remember table */
static int rtable_get (lua_State *L) {
  int nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  if (!agn_getrtable(L, 1)) {
    lua_pushfail(L);  /* no rtable */
  } else if (nargs == 1) {
    agn_copy(L, -1, 3);  /* make a true copy of the rtable so that the user cannot destroy it */
    lua_remove(L, -2);  /* remove original rtable */
    lua_newtable(L);
    lua_pushnil(L);
    while (lua_next(L, -3) != 0) {  /* traverse rtable */
      lua_pushnil(L);
      while (lua_next(L, -2) != 0) {  /* get arguments and results */
        lua_rawset2(L, -5);
      }
      agn_poptop(L);
    }
    lua_remove(L, -2);  /* remove rtable copy */
  } else {
    agn_copy(L, -1, 3);  /* make a true copy of the rtable so that the user cannot destroy it */
    lua_remove(L, -2);  /* remove original rtable */
  }
  return 1;
}


/* Empties the contents of the remember table of procedure f, but does not delete the table so that it will continue collecting
   results with the next call to f. Read-only remember tables cannot be emptied. The memory previously occupied by cached function
  arguments and results can be reused for other purposes. 2.33.2 */
static int rtable_forget (lua_State *L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  if (agn_getrtablewritemode(L, 1) == 0)
    luaL_error(L, "Error in " LUA_QS ": cannot empty read-only remember table.", "rtable.forget");
  if (!agn_getrtable(L, 1)) {  /* no rtable */
    lua_pushfail(L);
  } else {
    agn_cleanse(L, -1, 1);
    agn_poptop(L);  /* pop rtable from stack */
    lua_pushtrue(L);
  }
  return 1;
}


/* sets a remember table with initial values */
static int rtable_put (lua_State *L) {
  agn_setrtable(L, 1, 2, 3);
  lua_pushnil(L);
  return 1;
}


/* deletes an rtable */
static int rtable_purge (lua_State *L) {  /* extended 2.33.2 */
  luaL_checktype(L, 1, LUA_TFUNCTION);
  if (!agn_getrtable(L, 1)) {  /* no rtable */
    lua_pushfail(L);
  } else {
    agn_cleanse(L, -1, 0);
    agn_poptop(L);  /* drop rtable */
    agn_deletertable(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_pushnil(L);
  }
  return 1;
}


static int rtable_mode (lua_State *L) {  /* Agena 1.1.0 */
  int result;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  result = agn_getrtablewritemode(L, 1);
  if (result == 0)
    lua_pushstring(L, "rotable");
  else if (result == 1)
    lua_pushstring(L, "rtable");
  else
    lua_pushstring(L, "none");
  return 1;
}


static const luaL_Reg rtablelib[] = {
  {"purge", rtable_purge},                  /* added February 02, 2008 */
  {"forget", rtable_forget},                /* added October 20, 2022 */
  {"get", rtable_get},                      /* added January 28, 2008 */
  {"init", rtable_init},                    /* added January 28, 2008 */
  {"roinit", rtable_roinit},                /* added May 23, 2009 */
  {"mode", rtable_mode},                    /* added December 04, 2010 */
  {"put", rtable_put},                      /* added January 28, 2008 */
  {NULL, NULL}
};


/*
** Open rtable library
*/
LUALIB_API int luaopen_rtable (lua_State *L) {
  luaL_register(L, AGENA_RTABLELIBNAME, rtablelib);
  return 1;
}


