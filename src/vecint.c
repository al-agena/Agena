#include <stdio.h>

#define vecint_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnconf.h"  /* for uchar */
#include "agnhlps.h"
#include "agnxlib.h"
#include "intvec.h"
#include "vecint.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_VECINTLIBNAME "vecint"
LUALIB_API int (luaopen_vecint) (lua_State *L);
#endif


static int mt_getsize (lua_State *L) {
  lua_pushnumber(L, _intvec_size(checkvecint(L, 1)));
  return 1;
}

static int mt_zero (lua_State *L) {
  int i;
  intvec *v = checkvecint(L, 1);
  for (i=0; i < v->size; i++) {
    if (_intvec_get(v, i)) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}

static int mt_tostring (lua_State *L) {  /* at the console, the array is formatted as follows: */
  intvec *v = checkvecint(L, 1);
  lua_pushfstring(L, "vecint(%d)", v->size);
  return 1;
}

static int mt_gc (lua_State *L) {
  intvec *v = checkvecint(L, 1);
  lua_setmetatabletoobject(L, 1, NULL, 1);
  _intvec_free(v);
  return 0;
}


static int vecint_new (lua_State *L) {
  size_t initslots;
  intvec *v;
  luaL_checkstack(L, lua_gettop(L), "too many arguments");
  initslots = agn_checkposint(L, 1);
  v = (intvec *)lua_newuserdata(L, sizeof(intvec));
  if (v)
    _intvec_init(v, initslots);
  else
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "vecint.new");
  lua_setmetatabletoobject(L, -1, "vecint", 1);
  return 1;
}


static int vecint_append (lua_State *L) {
  intvec *v = checkvecint(L, 1);
  _intvec_append(v, agn_checkinteger(L, 2));
  return 0;
}


static int vecint_getitem (lua_State *L) {
  intvec *v = checkvecint(L, 1);
  int pos = agn_checkposint(L, 2);
  if (pos > v->size)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "vecint.getitem", pos);
  lua_pushinteger(L, _intvec_get(v, pos - 1));
  return 1;
}


static int vecint_setitem (lua_State *L) {
  intvec *v = checkvecint(L, 1);
  int pos = agn_checkposint(L, 2);
  int val = agn_checkposint(L, 3);
  if (_intvec_set(v, pos - 1, val))
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "vecint.setitem", pos);
  return 1;
}


static int vecint_include (lua_State *L) {
  intvec *v = checkvecint(L, 1);
  int pos = agn_checkposint(L, 2);
  if (_intvec_insert(v, pos - 1, agn_checkinteger(L, 3)))
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "vecint.include", pos);
  return 1;
}


static int vecint_purge (lua_State *L) {
  int pos, olditem;
  intvec *v = checkvecint(L, 1);
  pos = agn_checkposint(L, 2);
  if (pos > v->size)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "vecint.purge", pos);
  olditem = _intvec_get(v, pos - 1);
  if (_intvec_delete(v, pos - 1)) {
    luaL_error(L, "Error in " LUA_QS ": error while deleting item.", "vecint.purge", pos);
  } else {
    luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
    lua_pushinteger(L, olditem);  /* item just deleted */
  }
  lua_pushinteger(L, v->size);
  lua_pushinteger(L, v->capacity);
  return 3;
}


static int vecint_attribs (lua_State *L) {
  intvec *v = checkvecint(L, 1);
  lua_pushinteger(L, v->size);
  lua_pushinteger(L, v->capacity);
  return 2;
}


static int vecint_newsize (lua_State *L) {
  lua_pushinteger(L, intvec_newsize(agn_checknonnegint(L, 1)));
  return 1;
}


static const struct luaL_Reg vecint_fieldlib [] = {  /* metamethods for field `n' */
  {"__index",      vecint_getitem},     /* n[p], with p the index, counting from 1 */
  {"__writeindex", vecint_setitem},     /* n[p] := value, with p the index, counting from 1 */
  {"__tostring",   mt_tostring},        /* for output at the console, e.g. print(n) */
  {"__size",       mt_getsize},         /* metamethod for `size` operator */
  {"__zero",       mt_zero},            /* metamethod for `zero` operator */
  {"__gc",         mt_gc},              /* please do not forget garbage collection */
  {NULL, NULL}
};


static const luaL_Reg vecintlib[] = {
  {"append",       vecint_append},
  {"attribs",      vecint_attribs},
  {"getitem",      vecint_getitem},
  {"include",      vecint_include},
  {"new",          vecint_new},
  {"newsize",      vecint_newsize},
  {"purge",        vecint_purge},
  {"setitem",      vecint_setitem},
  {NULL, NULL}
};

/*
** Open bitfield library
*/
LUALIB_API int luaopen_vecint (lua_State *L) {
  /* metamethods */
  luaL_newmetatable(L, "vecint");
  luaL_register(L, NULL, vecint_fieldlib);
  /* register library */
  luaL_register(L, AGENA_VECINTLIBNAME, vecintlib);
  return 1;
}


