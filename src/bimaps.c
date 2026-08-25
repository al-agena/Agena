/*
** $Id: bimaps.c,v 0.1 26.07.2012 $
** Bimaps Library
** See Copyright Notice in agena.h
*/

#include <stdio.h>

#define bimaps_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnxlib.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_BIMAPSLIBNAME "bimaps"
LUALIB_API int (luaopen_bimaps) (lua_State *L);
#endif


/* The function pushes the underlying mapping table onto the stack. REMOVE IT later ! */
static FORCE_INLINE void checkbimap (lua_State *L, int idx, const char *procname) {
  if (!(lua_istable(L, idx) &&
        agn_isutype(L, idx, "bimap")
       ) ||
    (!luaL_getmetafield(L, idx, "__index") || !lua_istable(L, -1)) ) {
    luaL_error(L, "Error in " LUA_QS ": structure is not a bimap or corrupt.", procname);
  }
}

/*** Metamethods ********************************************************************** */

static int mt_empty (lua_State *L) {
  int l;
  checkbimap(L, 1, "bimaps.__empty");
  l = agn_size(L, -1);
  agn_poptop(L);
  lua_pushboolean(L, l == 0);
  return 1;
}

static int mt_filled (lua_State *L) {
  int l;
  checkbimap(L, 1, "bimaps.__filled");
  l = agn_size(L, -1);
  agn_poptop(L);
  lua_pushboolean(L, l != 0);
  return 1;
}

static int mt_in (lua_State *L) {
  int rc = 1;
  checkbimap(L, 2, "bimaps.__in");
  lua_pushnil(L);
  while (rc && lua_next(L, -2) ) {
    if (lua_equal(L, 1, -2) || lua_equal(L, 1, -1)) {
      /* prepare premature exit */
      agn_poptop(L);
      rc = 0;
    }
    agn_poptop(L);
  }
  agn_poptop(L);  /* drop the map pushed by checkbimap */
  lua_pushboolean(L, !rc);
  return 1;
}

static int mt_notin (lua_State *L) {
  int rc = 1;
  checkbimap(L, 2, "bimaps.__notin");
  lua_pushnil(L);
  while (rc && lua_next(L, -2) ) {
    if (lua_equal(L, 1, -2) || lua_equal(L, 1, -1)) {
      /* prepare premature exit */
      agn_poptop(L);
      rc = 0;
    }
    agn_poptop(L);
  }
  agn_poptop(L);  /* drop the map pushed by checkbimap */
  lua_pushboolean(L, rc);
  return 1;
}


static int bimaps_getsize (lua_State *L) {
  int l;
  checkbimap(L, 1, "bimaps.getsize");
  l = agn_size(L, -1);
  agn_poptop(L);
  lua_pushnumber(L, l);
  return 1;
}

/* Returns the underlying table in bimap t, without invoking any metamethods, if no index k is given - or if k is given,
   returns entry t[k] without invoking any metamethods.

   bimaps.rawget := proc(t, k) is
      local tbl := t('raw');
      return if unassigned k then tbl else tbl[k] end
   end;
*/
static int bimaps_rawget (lua_State *L) {
  int nargs = lua_gettop(L);
  checkbimap(L, 1, "bimaps.rawget");
  if (nargs == 2) {  /* get an entry, else return whole underlying mapping table */
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    lua_remove(L, -2);
  }
  return 1;
}


/* Returns all indices in bimap t, without invoking any metamethod.
   bimaps.indices := proc(t) is
      return tables.indices(t('raw'))
   end;
*/
static int bimaps_indices (lua_State *L) {
  int i = 0;
  lua_createtable(L, 8, 0);
  checkbimap(L, 1, "bimaps.indices");
  lua_pushnil(L);
  while (lua_next(L, -2) ) {
    agn_poptop(L);          /* pop value */
    lua_pushvalue(L, -1);   /* duplicate key */
    lua_rawseti(L, -4, ++i);  /* drops duplicated key, leaves origonal one on stack */
  }
  agn_poptop(L);  /* drop the map pushed by checkbimap */
  return 1;  /* return table of indices */
}


/* Returns all entries in bimap t, without invoking any metamethod.
   bimaps.entries := proc(t) is
      return tables.entries(t('raw'))
   end;
*/
static int bimaps_entries (lua_State *L) {
  int i = 0;
  lua_createtable(L, 8, 0);
  checkbimap(L, 1, "bimaps.entries");
  lua_pushnil(L);
  while (lua_next(L, -2) ) {
    lua_rawseti(L, -4, ++i);  /* drops duplicated key, leaves origonal one on stack */
  }
  agn_poptop(L);  /* drop the map pushed by checkbimap */
  return 1;  /* return table of indices */
}


static const struct luaL_Reg bimaps_bimapslib [] = {  /* metamethods for bimaps */
  /* the Agena `size` implementation is 37 % faster */
  {"getsize",  bimaps_getsize},   /* January 07, 2025 */
  {"rawget",   bimaps_rawget},    /* January 07, 2025 */
  {"indices",  bimaps_indices},   /* January 07, 2025 */
  {"entries",  bimaps_entries},   /* January 07, 2025 */
  {"__empty",  mt_empty},         /* metamethod for `empty` operator */
  {"__filled", mt_filled},        /* metamethod for `filled` operator */
  {"__in",     mt_in},            /* metamethod for `in` operator */
  {"__notin",  mt_notin},         /* metamethod for `notin` operator */
  {NULL, NULL}
};

static const luaL_Reg bimapslib[] = {
  {"getsize",  bimaps_getsize},   /* January 07, 2025 */
  {"rawget",   bimaps_rawget},    /* January 07, 2025 */
  {"indices",  bimaps_indices},   /* January 07, 2025 */
  {"entries",  bimaps_entries},   /* January 07, 2025 */
  {NULL, NULL}
};


/*
** Open bimaps library
*/
LUALIB_API int luaopen_bimaps (lua_State *L) {
  /* metamethods */
  luaL_newmetatable(L, AGENA_BIMAPSLIBNAME);  /* create metatable */
  luaL_register(L, NULL, bimaps_bimapslib);  /* methods */
  /* register library */
  luaL_register(L, AGENA_BIMAPSLIBNAME, bimapslib);
  return 1;
}

