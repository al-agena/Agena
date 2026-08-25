/*
** $Id: lsetlib.c,v 1.38 2005/10/23 17:38:15 roberto Exp $
** Library for UltraSet Manipulation
** See Copyright Notice in agena.h
*/

#include <stddef.h>

#define lsetlib_c
#define LUA_LIB

#include "agena.h"

#include "agnxlib.h"
#include "agenalib.h"


/* Creates a new set. If the first argument n is a positive integer, it pre-allocates n slots for future insertion, the default is 4. If the last
   argument is `true` the number of pre-allocated slots is adjusted to an optimum of the smallest power of 2 greater than or equal n. 2.33.4 */
static int set_newset (lua_State *L) {
  int total = agnL_optposint(L, 1, 4);
  if (agnL_optboolean(L, 2, 0)) total = tools_nextpower(total, 2, 1);
  agn_createset(L, total);
  return 1;
}


static int set_new (lua_State *L) {
  /* use `volatile` so that the compiler does not render the Kahan code effectless */
  /* volatile lua_Number idx, step, c, y, t; 4.6.0, based on seq_new */
  int isfunc, isint, isdefault;
  size_t i, nargs, total, offset;
  lua_Number a, b, eps;
  volatile lua_Number s, c, cs, ccs, cc, t, idx, step, x;
  luaL_aux_nstructure(L, "sets.new", &nargs, &offset, &a, &b, &step, &eps, &total, &isfunc, &isdefault, &isint);
  luaL_checkstack(L, 1 + isdefault, "not enough stack space");
  agn_createset(L, total);
  cs = ccs = 0;
  s = idx = a;
  if (isdefault) {  /* create a sequence of n slots and fill it with one and the same default of any type */
    agn_pairgeti(L, 2, 2);
  }
  if (isfunc) {  /* function passed ? */
    int slots = 2 + (nargs >= 4 + offset)*(nargs - 4 - (int)offset + 1);
    while (idx <= b || tools_approx(idx, b, eps)) {
      luaL_checkstack(L, slots, "not enough stack space");
      lua_pushvalue(L, offset);  /* push function */
      lua_pushnumber(L, (fabs(idx) < AGN_HEPSILON) ? 0 : idx);  /* quite dirty hack to avoid roundoff errors with 0 */
      for (i=4 + offset; i <= nargs; i++) lua_pushvalue(L, i);
      lua_call(L, slots - 1, 1);
      lua_srawset(L, -2);
      if (isint) {
        idx += step;
      } else {
        x = step;  /* Kahan-Babuska */
        t = s + x;
        c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
        s = t;
        t = cs + c;
        cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
        cs = t;
        ccs += cc;
        idx = s + cs + ccs;
      }
    }
  } else {
    while (idx <= b || tools_approx(idx, b, eps)) {
      if (isdefault) {  /* fill with default value  */
        lua_pushvalue(L, -1);
        lua_srawset(L, -3);
      } else {
        lua_pushnumber(L, (fabs(idx) < AGN_HEPSILON) ? 0 : idx);
        lua_srawset(L, -2);
      }
      if (isint) {  /* 2.12.2 */
        idx += step;
      } else {
        x = step;  /* Kahan-Babuska */
        t = s + x;
        c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
        s = t;
        t = cs + c;
        cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
        cs = t;
        ccs += cc;
        idx = s + cs + ccs;
      }
    }
  }
  if (isdefault) agn_poptop(L);
  return 1;
}


/* Resizes set `s` to store at least `n` elements. If the last argument is `true` the number of pre-allocated slots is adjusted to an optimum of the
   smallest power of 2 greater than or equal n.

   If only `s` is given, the number of pre-allocated slots will be changed to the smallest power of 2 greater than or equal the current size, usually
   freeing formerly occupied space.

   If n < size(s) or the number of pre-allocated slots would not change, the function does nothing and returns without modifying the set. */
static int set_resize (lua_State *L) {  /* 2.33.4 */
  size_t a[4];
  luaL_checktype(L, 1, LUA_TSET);
  agn_sstate(L, 1, a);
  if (lua_gettop(L) == 1) {  /* resize to optimum size, but do not drop elements. */
    agn_setresize(L, 1, tools_nextpower(a[0], 2, 1), 1);  /* only shrink if possible, never enlarge */
  } else {
    int newsize = agn_checkposint(L, 2);
    if (agnL_optboolean(L, 3, 0)) newsize = tools_nextpower(newsize, 2, 1);
    agn_setresize(L, 1, newsize, 2);  /* only resize if size < newsize and allocated size != newsize, shrink or enlarge */
  }
  agn_sstate(L, 1, a);  /* read status once again */
  lua_pushinteger(L, a[0]);  /* us->size */
  lua_pushinteger(L, (a[0] == 1 && a[1] == 0) ? 1 : a[1]);  /* actnodesize; lsizenode is set to 0 even if there is exactly one element in a set */
  return 2;
}


/* Removes all elements from a set and returns nothing. Contrary to `cleanse`, the function does not resize the set,
   call sets.resize afterwards if needed. 5.1.2 */
static int set_cleanse (lua_State *L) {
  luaL_checktype(L, 1, LUA_TSET);
  lua_pushvalue(L, 1);
  lua_pushnil(L);
  while (lua_usnext(L, -2) != 0) {
    lua_sdelete(L, -3); /* delete value in the set and pop it */
    agn_poptop(L); /* pop the key from stack, too, since it cannot be used for next iteration
                      as it has been purged from the set */
    lua_pushnil(L); /* restart iteration */
  }
  agn_poptop(L);
  lua_gc(L, LUA_GCCOLLECT, 0);
  return 0;
}


static int set_numunion (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TSET, 1, "argument is not a set");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TSET, 2, "argument is not a set");
  lua_pushinteger(L, agn_numunion(L, 1, 2));
  return 1;
}


static int set_numintersect (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TSET, 1, "argument is not a set");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TSET, 2, "argument is not a set");
  lua_pushinteger(L, agn_numintersect(L, 1, 2));
  return 1;
}


static int set_numminus (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TSET, 1, "argument is not a set");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TSET, 2, "argument is not a set");
  lua_pushinteger(L, agn_numminus(L, 1, 2));
  return 1;
}


/* Checks whether all elements sequence s are of type x, a string, and returns true or false. Eligible values for x are
   "number", "integer" (numbers that are all integral), "complex", "string", "boolean", etc. 3.10.2 */
static int set_isall (lua_State *L) {
  agn_usisall(L, 1, "sets.isall");
  return 1;
}


/* }====================================================== */

static const luaL_Reg set_funcs[] = {
  {"cleanse", set_cleanse},            /* added July 11, 2025 */
  {"isall", set_isall},                /* added February 04, 2024 */
  {"new", set_new},                    /* added November 25, 2024 */
  {"newset", set_newset},              /* added October 25, 2022 */
  {"numunion", set_numunion},          /* added January 26, 2024 */
  {"numintersect", set_numintersect},  /* added January 26, 2024 */
  {"numminus", set_numminus},          /* added January 26, 2024 */
  {"resize", set_resize},              /* added October 25, 2022 */
  {NULL, NULL}
};


LUALIB_API int luaopen_sets (lua_State *L) {
  luaL_register(L, AGENA_SETSLIBNAME, set_funcs);
  return 1;
}

