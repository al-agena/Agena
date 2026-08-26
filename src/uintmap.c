/* uintmap Library, created with the help of Gemini AI on July 07/21, 2026 and put to the public domain.
   Agena C package functions by a_walz.

   This is a working edition, successfully checked with Valgrind.

   The package implements an unsigned-4-byte-integer-to-string hash map, with non-negative Agena integers
   the keys and the strings the associated values.

Usage:

h := uintmap.new()
uintmap.include(h, 10, 'ten')
uintmap.include(h, 20, 'twenty')
uintmap.getitem(h, 10):
uintmap.getitem(h, 20):
uintmap.getitem(h, 1):
uintmap.purge(h, 20);
uintmap.getitem(h, 20):

uintmap.attrib(h):
uintmap.totable(h):

'ten' in h:
'ten' notin h:

f := uintmap.iterate(h);
f():

OOP-style usage:

h := uintmap.new()
h@@include(10, 'ten')
h@@include(20, 'twenty')
h[10], h[20]:
h@@purge(20);
h[20]:
f := h@@iterate();
f():
f():
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define uintmap_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnhlps.h"  /* include before khash.h */
#include "agnxlib.h"

#include "khash.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_UINTMAPLIBNAME "uintmap"
LUALIB_API int (luaopen_uintmap) (lua_State *L);
#endif

#define checkuintmap(L,n) ((kh_uintmap_t **)luaL_checkudata(L, n, AGENA_UINTMAPLIBNAME))

#define isuintmap(L,n)    (luaL_isudata(L, n, AGENA_UINTMAPLIBNAME) && agn_isutypeset(L, n))

#include <stdint.h>
#include <string.h>

KHASH_MAP_INIT_INT(uintmap, char *)


/*** Agena C Functions ****************************************************************/

static int uintmap_new (lua_State *L) {
  khash_t(uintmap) **ht;
  size_t size = (size_t)tools_nextpow2u32(agnL_optposint(L, 1, 8));
  ht = (khash_t(uintmap) **)lua_newuserdata(L, sizeof(khash_t(uintmap) *));
  *ht = kh_init(uintmap);
  if (*ht == NULL) {
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "uintmap.new");
  }
  kh_resize(uintmap, *ht, size);
  lua_setmetatabletoobject(L, -1, AGENA_UINTMAPLIBNAME, 1);
  lua_createtable(L, 0, 0);
  lua_setfenv(L, -2);
  return 1;
}


static int uintmap_getitem (lua_State *L)  {
  int nargs = lua_gettop(L);
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "uintmap.getitem");
  }
  if (nargs == 1) {
    lua_getfenv(L, 1);
  } else {
    luaL_checkstack(L, 2, "not enough stack space");
    khint_t key = agn_checkuint32_t(L, 2);
    khint_t k = kh_get(uintmap, h, key);
    if (k != kh_end(h)) {
      lua_pushstring(L, kh_val(h, k));
    } else  {
      lua_pushnil(L);
    }
  }
  return 1;
}


static int uintmap_include (lua_State *L) {
  int rc;
  khint_t key;
  const char *val;
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "uintmap.include");
  }
  if (lua_gettop(L) == 2) {
    size_t l;
    val = agn_checklstring(L, 2, &l);
    key = tools_crc32(val, l, 0);  /* 7.7.10 change, formerly: __ac_X31_hash_string(val); */
  } else {
    key = agn_checkuint32_t(L, 2);
    val = agn_checkstring(L, 3);
  }
  char *newval = strdup(val);
  if (!newval)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "uintmap.include");
  khint_t k = kh_put(uintmap, h, key, &rc);
  if (rc == -1) {
    free(newval);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "uintmap.include");
  }
  if (rc == 0) { free(kh_val(h, k)); }
  kh_val(h, k) = newval;
  lua_pushboolean(L, rc);
  return 1;
}


static int uintmap_purge (lua_State *L) {
  int shrink, rc = 0;
  khint_t key, k;
  char *val = NULL;
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "uintmap.purge");
  }
  if (agn_isstring(L, 2)) {
    size_t l;
    val = (char *)agn_checklstring(L, 2, &l);
    key = tools_crc32(val, l, 0);  /* 7.7.10 change, formerly: __ac_X31_hash_string(val); */
  } else {
    key = agn_checkuint32_t(L, 2);
  }
  shrink = agnL_optboolean(L, 3, 0);
  k = kh_get(uintmap, h, key);
  if (k != kh_end(h)) {
    if (val) {
      rc = tools_streq(kh_val(h, k), val);
    } else {
      rc = 1;
    }
    if (rc) {
      free((void *)kh_val(h, k));
      kh_del(uintmap, h, k);
    }
  }
  if (rc && shrink) {
    kh_resize(uintmap, h, kh_size(h));
  }
  lua_pushboolean(L, rc);
  return 1;
}


static int aux_has (khash_t(uintmap) *h, const char *x) {
  int rc = 0;
  if (h) {
    khint_t k;
    for (k = kh_begin(h); k != kh_end(h) && !rc; ++k) {
      if (kh_exist(h, k)) {
        const char *d = kh_val(h, k);
        if (tools_streq(x, d)) {
          rc = 1;
        }
      }
    }
  }
  return rc;
}

static int uintmap_has (lua_State *L) {
  size_t l;
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  const char *x = agn_checklstring(L, 2, &l);
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "uintmap.has");
  }
  khint_t key = tools_crc32(x, l, 0);  /* 7.7.10 change, formerly: __ac_X31_hash_string(val); */
  khint_t k = kh_get(uintmap, h, key);
  if (k != kh_end(h)) {
    const char *d = kh_val(h, k);
    if (tools_streq(x, d)) {  /* found via fast path */
      lua_pushtrue(L);
      return 1;
    }
  }
  lua_pushboolean(L, aux_has(h, x));
  return 1;
}


static int uintmap_resize (lua_State *L) {
  int rc = 1;
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "uintmap.resize");
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
      kh_resize(uintmap, h, actual_target);
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


static int uintmap_cleanse (lua_State *L) {
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "uintmap.cleanse");
  } else {
    khint_t k;
    for (k = kh_begin(h); k != kh_end(h); ++k) {
      if (kh_exist(h, k)) {
        free((void *)kh_val(h, k));
      }
    }
    kh_clear(uintmap, h);
  }
  return 0;
}


static int iterate (lua_State *L) {
  kh_uintmap_t **ht = lua_touserdata(L, lua_upvalueindex(1));
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  khint_t k = (khint_t)lua_tointeger(L, lua_upvalueindex(2));
  if (h->n_buckets == 0 || k >= kh_end(h) ) {
    lua_pushnil(L);
    return 1;
  }
  luaL_checkstack(L, 3, "not enough stack space");
  for (; k != kh_end(h); ++k) {
    if (kh_exist(h, k)) {
      lua_pushnumber(L, (lua_Number)kh_key(h, k));
      lua_pushstring(L, (const char *)kh_val(h, k));
      lua_pushinteger(L, (int)k + 1);
      lua_replace(L, lua_upvalueindex(2));
      return 2;
    }
  }
  lua_pushnil(L);
  return 1;
}

static int uintmap_iterate (lua_State *L) {  /* 7.7.3 */
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  (void)h;  /* silences the unused variable warning */
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushvalue(L, 1);
  lua_pushinteger(L, (int)kh_begin(h));  /* evaluates to constant 0, actually */
  lua_pushcclosure(L, &iterate, 2);
  return 1;
}


static int uintmap_attrib (lua_State *L) {
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  size_t n_occupied = h ? kh_size(h) : 0;
  size_t n_capacity = h ? h->n_buckets : 0;
  size_t n_empty = 0;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "uintmap.attrib");
  }
  khint_t k;
  for (k = kh_begin(h); k != kh_end(h); ++k) {
    if (!kh_exist(h, k)) {
      n_empty++;
    }
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
  return 1;
}


static int uintmap_totable (lua_State *L) {
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "uintmap.totable");
  }
  lua_createtable(L, (int)kh_size(h), 0);
  khint_t k;
  for (k=kh_begin(h); k != kh_end(h); ++k)  {
    if (kh_exist(h, k))  {
      lua_pushnumber(L, (lua_Number)kh_key(h, k));
      lua_pushstring(L, (const char *)kh_val(h, k));
      lua_rawset(L, -3);
    }
  }
  return 1;
}


static int uintmap_hash (lua_State *L) {
  const char *buf = agn_checkstring(L, 1);
  uint32_t n = agnL_optuint32_t(L, 2, 0);
  uint32_t crc = (uint32_t)__ac_X31_hash_string(buf);
  lua_pushnumber(L, (n != 0) ? crc % n : crc);
  return 1;
}


/*** Metamethods **********************************************************************/

static int mt_index (lua_State *L) {
  if (isuintmap(L, 1) || agn_isnumber(L, 1)) {
    if (lua_gettop(L) == 2 && agn_isstring(L, 2)) {
      /* sizeof(<string constant>) == strlen(<string constant>) + 1 */
      return agn_initmethodcall(L, AGENA_UINTMAPLIBNAME, sizeof(AGENA_UINTMAPLIBNAME) - 1);
    }
  }
  return uintmap_getitem(L);
}


static int mt_getsize (lua_State *L)  {
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  lua_pushinteger(L, h ? (lua_Integer)kh_size(h) : 0);
  return 1;
}

static int mt_empty (lua_State *L)  {
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  lua_pushboolean(L, h ? kh_size(h) == 0 : 1);
  return 1;
}


static int mt_filled (lua_State *L)  {
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  lua_pushboolean(L, h ? kh_size(h) != 0 : 0);
  return 1;
}


static int mt_in (lua_State *L)  {
  kh_uintmap_t **ht = checkuintmap(L, 2);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  const char *val = agn_checkstring(L, 1);
  khint_t key = __ac_X31_hash_string(val);
  khint_t k = kh_get(uintmap, h, key);
  if (k != kh_end(h)) {
    const char *d = kh_val(h, k);
    if (tools_streq(val, d)) {  /* found via fast path */
      lua_pushtrue(L);
      return 1;
    }
  }
  lua_pushboolean(L, aux_has(h, val));
  return 1;
}


static int mt_notin (lua_State *L)  {
  kh_uintmap_t **ht = checkuintmap(L, 2);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  const char *val = agn_checkstring(L, 1);
  khint_t key = __ac_X31_hash_string(val);
  khint_t k = kh_get(uintmap, h, key);
  if (k != kh_end(h)) {
    const char *d = kh_val(h, k);
    if (tools_streq(val, d)) {  /* found via fast path */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushboolean(L, !aux_has(h, val));
  return 1;
}


static int mt_u2string (lua_State *L) {  /* at the console, the hash table is formatted as follows: */
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%u)", h ? kh_size(h) : 0);
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid string hash table.", "uintmap.__tostring");
  return 1;
}


static int mt_gc (lua_State *L)  {
  kh_uintmap_t **ht = checkuintmap(L, 1);
  kh_uintmap_t *h = (ht && *ht) ? *ht : NULL;
  if (h)  {
    khint_t k;
    for (k=kh_begin(h); k != kh_end(h); ++k)  {
      if (kh_exist(h, k))  {
        free((void *)kh_val(h, k));
      }
    }
    *ht = NULL;  /* 7.7.10 fix */
    kh_destroy(uintmap, h);
  }
  agn_udfreeze(L, 1, 0);
  lua_setmetatabletoobject(L, 1, NULL, 1);
  lua_pushnil(L);
  lua_setfenv(L, 1);
  return 0;
}


static const struct luaL_Reg uintmap_lib [] = {  /* metamethods for numeric userdata `n` */
  {"attrib",       uintmap_attrib},
  {"cleanse",      uintmap_cleanse},
  {"has",          uintmap_has},
  {"include",      uintmap_include},
  {"iterate",      uintmap_iterate},
  {"purge",        uintmap_purge},
  {"resize",       uintmap_resize},
  {"totable",      uintmap_totable},
  {"__index",      mt_index},     /* n[p], with p the index, counting from 1, OOP-style calls */
  {"__writeindex", uintmap_include},
  {"__size",       mt_getsize},   /* metamethod for `size` operator */
  {"__empty",      mt_empty},     /* metamethod for `empty` operator */
  {"__filled",     mt_filled},    /* metamethod for `filled` operator */
  {"__in",         mt_in},        /* metamethod for `in` operator */
  {"__notin",      mt_notin},     /* metamethod for `notin` operator */
  {"__tostring",   mt_u2string},  /* for output at the console, e.g. print(n) */
  {"__gc",         mt_gc},        /* please do not forget garbage collection */
  {NULL, NULL}
};


static const luaL_Reg uintmaplib[] = {
  {"attrib",       uintmap_attrib},
  {"cleanse",      uintmap_cleanse},
  {"getitem",      uintmap_getitem},
  {"has",          uintmap_has},
  {"hash",         uintmap_hash},
  {"include",      uintmap_include},
  {"iterate",      uintmap_iterate},
  {"new",          uintmap_new},
  {"purge",        uintmap_purge},
  {"resize",       uintmap_resize},
  {"totable",      uintmap_totable},
  {NULL, NULL}
};


/*
** Open uintmap library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_UINTMAPLIBNAME);  /* create metatable */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, uintmap_lib);  /* methods */
}

LUALIB_API int luaopen_uintmap (lua_State *L) {
  createmeta(L);
  luaL_register(L, AGENA_UINTMAPLIBNAME, uintmaplib);
  return 1;
}

