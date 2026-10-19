/* intmap Library, implements integer-to-integer hash maps.
   Created with the help of Gemini AI on July 07, 2026 and put to the public domain.
   Agena C package functions by a_walz.

   This is a working edition, successfully checked with Valgrind.

   The package implements an integer-to-integer hash map, with Agena numbers both the keys and
   associated values. Specifically, the integers are signed 4-byte integers.

Usage:

import intmap

h := intmap.new()
intmap.include(h, 10, 100)
intmap.include(h, -10, -100)
intmap.getitem(h, 10):
intmap.getitem(h, -10):
intmap.getitem(h, 1):
intmap.purge(h, -100);
intmap.getitem(h, -100):

intmap.attrib(h):
intmap.totable(h):

100 in h:
100 notin h:

f := intmap.iterate(h);
f():

*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define intmap_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnxlib.h"
#include "agnhlps.h"  /* include before khash.h */

#include "khash.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_INTMAPLIBNAME "intmap"
LUALIB_API int (luaopen_intmap) (lua_State *L);
#endif


KHASH_INIT(intmap, int32_t, int32_t, 1, kh_int_hash_func, kh_int_hash_equal)

typedef struct {
  kh_intmap_t *hash;
  int issigned;
} IntMap;

#define checkintmap(L,n) ((IntMap *)luaL_checkudata(L, n, AGENA_INTMAPLIBNAME))

#define isintmap(L,n)    (luaL_isudata(L, n, AGENA_INTMAPLIBNAME) && agn_isutypeset(L, n))

/*** Agena C Functions ****************************************************************/

static int32_t aux_checkinteger (lua_State *L, int idx, int issigned, const char *procname)  {  /* 7.7.11 */
  int32_t s_val;  /* khint_t is natively a uint32_t !!! */
  lua_Number x = lua_checkinteger(L, idx);
  if (!issigned) {
    /* unsigned to signed: maps from 0.0 .. 4294967295.0 (2^31-1) to -2147483648.0 (-2^31) .. 2147483647.0 (2^31-1)*/
    if (x < 0.0 || x > 4294967295.0)
      luaL_error(L, "Error in " LUA_QS ": %f is out of range.", procname, (lua_Number)x);
    /* Step 1: Force Lua input into a clean 32-bit unsigned integer */
    uint32_t u_val = (uint32_t)x;
    /* Step 2: Shift the range. 0 becomes 0x80000000 (INT_MIN) */
    s_val = (int32_t)(u_val ^ 0x80000000U);
  } else {
    /* signed: just check range and cast to int32_t */
    if (x < -2147483648.0 || x > 2147483647.0) {
      luaL_error(L, "Error in " LUA_QS ": %f is out of range.", procname, (lua_Number)x);
    }
    s_val = (int32_t)x;  /* khint_t is unsigned actually ! */
  }
  return s_val;
}

static lua_Number aux_tonumber (int32_t x, int issigned) {  /* 7.7.11 */
  if (!issigned) {
    uint32_t u_val = (uint32_t)x ^ 0x80000000U;
    return (lua_Number)u_val;
  } else {
    /* int32_t s_val = (int32_t)x;
    return (lua_Number)s_val; */
    return (lua_Number)x;
  }
}

static void aux_checkoptions (lua_State *L, int pos, int *nargs, int *issigned, const char *procname) {
  int checkoptions;
  *issigned = 1;
  /* check for options */
  checkoptions = 1;  /* check n options; CHANGE THIS if you add/delete options */
  /* check for properly allocated slots will be done by agn_pairgetiall() */
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "signed")) {
        *issigned = agn_checkboolean(L, -1);
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

static int intmap_new (lua_State *L) {
  int issigned, nargs = lua_gettop(L);
  IntMap *map;
  aux_checkoptions(L, 1, &nargs, &issigned, "intmap.new");
  if (nargs == 0) lua_settop(L, 0);
  size_t size = (size_t)tools_nextpow2u32(agnL_optposint(L, 1, 8));
  map = (IntMap *)lua_newuserdata(L, sizeof(IntMap));
  map->hash = kh_init(intmap);
  if (map->hash == NULL) {
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "intmap.new");
  }
  map->issigned = issigned;
  kh_resize(intmap, map->hash, size);
  lua_setmetatabletoobject(L, -1, AGENA_INTMAPLIBNAME, 1);
  lua_createtable(L, 0, 0);
  lua_setfenv(L, -2);
  return 1;
}


static int intmap_getitem (lua_State *L)  {
  int nargs = lua_gettop(L);
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "intmap.getitem");
  }
  if (nargs == 1)  {
    lua_getfenv(L, 1);
  } else {
    luaL_checkstack(L, 2, "not enough stack space");
    if (agn_isinteger(L, 2))  {
      int32_t key = aux_checkinteger(L, 2, map->issigned, "intmap.getitem");
      khint_t k = kh_get(intmap, h, key);
      if (k != kh_end(h))  {
        lua_pushnumber(L, aux_tonumber(kh_val(h, k), map->issigned));
      } else  {
        lua_pushnil(L);
      }
    } else  {
      luaL_error(L, "Error in " LUA_QS ": invalid second argument given.", "intmap.getitem");
    }
  }
  return 1;
}


static int intmap_include (lua_State *L) {
  int rc;
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  int32_t key = aux_checkinteger(L, 2, map->issigned, "intmap.include");
  int32_t val = aux_checkinteger(L, 3, map->issigned, "intmap.include");
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "intmap.include");
  }
  khint_t k = kh_put(intmap, h, key, &rc);
  if (rc == -1) {
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "intmap.include");
  }
  kh_val(h, k) = val;
  lua_pushboolean(L, rc);
  return 1;  /* 7.7.9 fix */
}


static int intmap_purge (lua_State *L) {
  int rc = 0;
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  int32_t key = aux_checkinteger(L, 2, map->issigned, "intmap.purge");
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "intmap.purge");
  }
  int shrink = agnL_optboolean(L, 3, 0);
  khint_t k = kh_get(intmap, h, key);
  if (k != kh_end(h)) {
    kh_del(intmap, h, k);
    rc = 1;
  }
  if (rc && shrink) {
    kh_resize(intmap, h, kh_size(h));
  }
  lua_pushboolean(L, rc);
  return 1;
}


static int aux_has (khash_t(intmap) *h, int32_t x) {
  int rc = 0;
  if (h) {
    khint_t k;
    for (k = kh_begin(h); k != kh_end(h) && !rc; ++k) {
      if (kh_exist(h, k)) {
        khint_t d = kh_val(h, k);
        if (x == d) {
          rc = 1;
        }
      }
    }
  }
  return rc;
}

static int intmap_has (lua_State *L) {
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  int32_t x = aux_checkinteger(L, 2, map->issigned, "intmap.has");
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "intmap.has");
  }
  lua_pushboolean(L, aux_has(h, x));
  return 1;
}


static int intmap_resize (lua_State *L) {
  int rc = 1;
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "intmap.resize");
  } else {
    khint_t old_buckets = h->n_buckets;
    khint_t target_buckets = (khint_t)agn_checknonnegint(L, 2);
    khint_t actual_target = 1;
    while (actual_target < target_buckets) {
      actual_target <<= 1;
    }
    if (actual_target < old_buckets) {
      actual_target = old_buckets;
    }
    if (actual_target != old_buckets) {
      kh_resize(intmap, h, actual_target);
      rc = (h->n_buckets != old_buckets);
    } else {
      rc = 0;
    }
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushboolean(L, rc);
  lua_pushnumber(L, (lua_Number)h->n_buckets);
  return 2;
}


static int intmap_cleanse (lua_State *L) {
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "intmap.cleanse");
  } else {
    kh_clear(intmap, h);
  }
  return 0;
}


static int iterate (lua_State *L) {
  IntMap *map = (IntMap *)lua_touserdata(L, lua_upvalueindex(1));
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  khint_t k = (khint_t)lua_tointeger(L, lua_upvalueindex(2));
  if (!h || h->n_buckets == 0 || k >= kh_end(h) ) {
    lua_pushnil(L);
    return 1;
  }
  luaL_checkstack(L, 3, "not enough stack space");
  for (; k != kh_end(h); ++k) {
    if (kh_exist(h, k)) {
      lua_pushnumber(L, aux_tonumber(kh_key(h, k), map->issigned));
      lua_pushnumber(L, aux_tonumber(kh_val(h, k), map->issigned));
      lua_pushinteger(L, (lua_Integer)k + 1);
      lua_replace(L, lua_upvalueindex(2));
      return 2;
    }
  }
  lua_pushnil(L);
  return 1;
}

static int intmap_iterate (lua_State *L) {  /* 7.7.3 */
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "intmap.iterate");
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushvalue(L, 1);
  lua_pushinteger(L, (lua_Integer)kh_begin(h));
  lua_pushcclosure(L, &iterate, 2);
  return 1;
}


/* Counts how many times khash.h has to resolve collisions by moving to the next slot (probing).
   Created by Gemini AI, 7.7.11 */
static uint32_t khash_collisions (kh_intmap_t *h) {
  uint32_t collision_moves = 0;
  khint_t k;
  /* kh_end(h) is the total number of buckets currently allocated in the table array */
  khint_t mask = h->n_buckets - 1;
  /* Loop through every physical bucket slot in the hash table */
  for (k = kh_begin(h); k < kh_end(h); ++k) {
    /* Check if the current slot actually contains a valid inserted element */
    if (kh_exist(h, k)) {
      int32_t key = (int32_t)kh_key(h, k);
      /* 1. Calculate the raw hash value using your hash function */
      khint_t raw_hash = kh_int_hash_func(key);
      /* 2. Calculate the "ideal" starting bucket slot khash intended to use */
      khint_t ideal_slot = raw_hash & mask;
      /* 3. If its current slot (k) is not its ideal slot, it was moved! */
      if (k != ideal_slot) collision_moves++;
    }
  }
  return collision_moves;
}

static int intmap_attrib (lua_State *L) {
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  size_t n_occupied = h ? kh_size(h) : 0;
  size_t n_capacity = h ? h->n_buckets : 0;
  size_t n_empty = 0;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "intmap.attrib");
  }
  khint_t k;
  for (k = kh_begin(h); k != kh_end(h); ++k) {
    if (!kh_exist(h, k)) n_empty++;
  }
  size_t n_void = (n_capacity > n_occupied + n_empty) ? (n_capacity - n_occupied - n_empty) : 0;
  lua_createtable(L, 0, 6);
  lua_rawsetstringnumber(L, -1, "count", (double)n_occupied);
  lua_rawsetstringnumber(L, -1, "capacity", (double)n_capacity);
  lua_rawsetstringnumber(L, -1, "empty", (double)n_empty);
  lua_rawsetstringnumber(L, -1, "void", (double)n_void);
  double cap = (double)n_capacity;
  lua_rawsetstringnumber(L, -1, "loadfactor", cap == 0.0 ? 0.0 : (double)n_occupied / cap);
  lua_rawsetstringnumber(L, -1, "utilization", cap == 0.0 ? 0.0 : (double)(n_occupied + n_empty) / cap);
  lua_rawsetstringboolean(L, -1, "signed", map->issigned);
  lua_rawsetstringnumber(L, -1, "collisions", (double)khash_collisions(h));
  return 1;
}


static int intmap_totable (lua_State *L)  {
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "intmap.totable");
  }
  lua_createtable(L, 0, (int)kh_size(h));
  khint_t k;
  for (k=kh_begin(h); k != kh_end(h); ++k)  {
    if (kh_exist(h, k))  {
      lua_pushnumber(L, aux_tonumber((int32_t)kh_key(h, k), map->issigned));
      lua_pushnumber(L, aux_tonumber(kh_val(h, k), map->issigned));
      lua_rawset(L, -3);
    }
  }
  return 1;
}


static int intmap_remap (lua_State *L)  {  /* 7.7.11 */
  lua_Number x = lua_checkinteger(L, 1);
  if (agnL_optboolean(L, 2, 1)) {
    /* unsigned to signed: maps from 0.0 .. 4294967295.0 (2^31-1) to -2147483648.0 (-2^31) .. 2147483647.0 (2^31-1)*/
    /* Step 1: Force Lua input into a clean 32-bit unsigned integer */
    if (x < 0.0 || x > 4294967295.0)
      luaL_error(L, "Error in " LUA_QS ": %f is out of range.", "intmap.remap", (lua_Number)x);
    uint32_t u_val = (uint32_t)x;
    /* Step 2: Shift the range. 0 becomes 0x80000000 (INT_MIN) */
    uint32_t shifted = u_val ^ 0x80000000U;
    /* Step 3: Cast to signed and push back to Lua */
    int32_t s_val = (int32_t)shifted;
    lua_pushnumber(L, (lua_Number)s_val);
  } else {
    /* signed to unsigned: maps from -2147483648.0 (-2^31) .. 2147483647.0 (2^31-1) to 0.0 .. 4294967295.0 (2^31-1) */
    if (x < -2147483648.0 || x > 2147483647.0) {
      luaL_error(L, "Error in " LUA_QS ": %f is out of range.", "intmap.remap", (lua_Number)x);
    }
    /* Step 1: Force Lua input into a clean 32-bit signed integer */
    int32_t s_val = (int32_t)x;
    /* Step 2: Reverse the shift using the exact same XOR operation */
    uint32_t u_val = (uint32_t)s_val ^ 0x80000000U;
    /* Step 3: Push back to Lua as an unsigned integer */
    lua_pushnumber(L, (lua_Number)u_val);
  }
  return 1;
}


/*** Metamethods **********************************************************************/

static int mt_index (lua_State *L) {
  if (isintmap(L, 1) || agn_isnumber(L, 1)) {  /* 7.7.9 extension for OOP-style calls */
    if (lua_gettop(L) == 2 && agn_isstring(L, 2)) {
      /* sizeof(<string constant>) == strlen(<string constant>) + 1 */
      return agn_initmethodcall(L, AGENA_INTMAPLIBNAME, sizeof(AGENA_INTMAPLIBNAME) - 1);
    }
  }
  return intmap_getitem(L);
}


static int mt_getsize (lua_State *L)  {
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  lua_pushinteger(L, h ? (lua_Integer)kh_size(h) : 0);
  return 1;
}

static int mt_empty (lua_State *L)  {
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  lua_pushboolean(L, h ? kh_size(h) == 0 : 1);
  return 1;
}

static int mt_filled (lua_State *L)  {
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  lua_pushboolean(L, h ? kh_size(h) != 0 : 0);
  return 1;
}


static int mt_in (lua_State *L)  {
  IntMap *map = checkintmap(L, 2);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  khint_t val = aux_checkinteger(L, 1, map->issigned, "intmap.__in");
  lua_pushboolean(L, aux_has(h, val));
  return 1;
}


static int mt_notin (lua_State *L)  {
  IntMap *map = checkintmap(L, 2);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  khint_t val = aux_checkinteger(L, 1, map->issigned, "intmap.__notin");
  lua_pushboolean(L, !aux_has(h, val));
  return 1;
}


static int mt_u2string (lua_State *L) {  /* at the console, the hash table is formatted as follows: */
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%u)", h ? kh_size(h) : 0);
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid string hash table.", "intmap.__tostring");
  return 1;
}


static int mt_gc (lua_State *L)  {
  IntMap *map = checkintmap(L, 1);
  kh_intmap_t *h = (map && map->hash) ? map->hash : NULL;
  if (h)  {
    map->hash = NULL;  /* 7.7.10 fix */
    kh_destroy(intmap, h);
  }
  agn_udfreeze(L, 1, 0);
  lua_setmetatabletoobject(L, 1, NULL, 1);
  lua_pushnil(L);
  lua_setfenv(L, 1);
  return 0;
}


static const struct luaL_Reg intmap_lib [] = {  /* metamethods for numeric userdata `n` */
  {"attrib",      intmap_attrib},
  {"cleanse",     intmap_cleanse},
  {"has",         intmap_has},
  {"include",     intmap_include},
  {"iterate",     intmap_iterate},
  {"purge",       intmap_purge},
  {"resize",      intmap_resize},
  {"totable",     intmap_totable},
  {"__index",      mt_index},     /* n[p], with p the index, counting from 1, OOP-style calls */
  {"__writeindex", intmap_include},
  {"__size",       mt_getsize},   /* metamethod for `size` operator */
  {"__empty",      mt_empty},     /* metamethod for `empty` operator */
  {"__filled",     mt_filled},    /* metamethod for `filled` operator */
  {"__in",         mt_in},        /* metamethod for `in` operator */
  {"__notin",      mt_notin},     /* metamethod for `notin` operator */
  {"__tostring",   mt_u2string},  /* for output at the console, e.g. print(n) */
  {"__gc",         mt_gc},        /* please do not forget garbage collection */
  {NULL, NULL}
};


static const luaL_Reg intmaplib[] = {
  {"attrib",      intmap_attrib},
  {"cleanse",     intmap_cleanse},
  {"getitem",     intmap_getitem},
  {"has",         intmap_has},
  {"include",     intmap_include},
  {"iterate",     intmap_iterate},
  {"new",         intmap_new},
  {"purge",       intmap_purge},
  {"remap",       intmap_remap},
  {"resize",      intmap_resize},
  {"totable",     intmap_totable},
  {NULL, NULL}
};


/*
** Open intmap library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_INTMAPLIBNAME);  /* create metatable */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, intmap_lib);  /* methods */
}

LUALIB_API int luaopen_intmap (lua_State *L) {
  createmeta(L);
  lua_pop(L, 1);  /* 7.11.2 fix */
  luaL_register(L, AGENA_INTMAPLIBNAME, intmaplib);
  return 1;
}

