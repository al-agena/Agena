/* strmap Library, implements string-to-number hash maps.
   Created with the help of Gemini AI on July 07, 2026 and put to the public domain.
   Agena C package functions by a_walz.

   This is a working edition, successfully checked with Valgrind.

   The package implements a string-to-number hash map, with the strings the keys and Agena numbers
   the associated values.

   Usage:

   import strmap

   h := strmap.new()
   strmap.include(h, "apple", 10)
   strmap.include(h, "banana", 20)
   strmap.getitem(h, "banana"):
   strmap.getitem(h, "banana"):
   strmap.getitem(h, "anana"):
   strmap.purge(h, "banana");
   strmap.getitem(h, "banana"):

   10 in h:
   10 notin h:

   strmap.attrib(h):
   strmap.totable(h):
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define strmap_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnhlps.h"  /* include before khash.h */
#include "agnxlib.h"

#include "khash.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_STRMAPLIBNAME "strmap"
LUALIB_API int (luaopen_strmap) (lua_State *L);
#endif

#define checkstrmap(L,n) ((kh_strmap_t **)luaL_checkudata(L, n, AGENA_STRMAPLIBNAME))

#define isstrmap(L,n)    (luaL_isudata(L, n, AGENA_STRMAPLIBNAME) && agn_isutypeset(L, n))
#define checkkeylength(L,len,pn) { \
  if (len > 1073741823) { \
    luaL_error(L, "Error in " LUA_QS ": string key is too large.", pn); \
  } \
}


/*** Agena C Functions ****************************************************************/

KHASH_MAP_INIT_STR(strmap, double)

static int strmap_new (lua_State *L) {
  khash_t(strmap) **ht;
  size_t size = (size_t)tools_nextpow2u32(agnL_optposint(L, 1, 8));
  ht = (khash_t(strmap) **)lua_newuserdata(L, sizeof(khash_t(strmap) *));
  *ht = kh_init(strmap);
  if (*ht == NULL) {
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strmap.new");
  }
  kh_resize(strmap, *ht, size);
  lua_setmetatabletoobject(L, -1, AGENA_STRMAPLIBNAME, 1);
  lua_createtable(L, 0, 0);
  lua_setfenv(L, -2);
  return 1;
}


static int strmap_getitem (lua_State *L)  {
  int nargs = lua_gettop(L);
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "strmap.getitem");
  }
  if (nargs == 1)  {
    lua_getfenv(L, 1);
  } else {
    luaL_checkstack(L, 2, "not enough stack space");
    if (agn_isstring(L, 2))  {
      size_t l;
      const char *key = lua_tolstring(L, 2, &l);
      checkkeylength(L, l, "strmap.getitem");
      khint_t k = kh_get(strmap, h, key);
      if (k != kh_end(h))  {
        lua_pushnumber(L, kh_val(h, k));
      } else  {
        lua_pushnil(L);
      }
    } else  {
      luaL_error(L, "Error in " LUA_QS ": invalid second argument given.", "strmap.getitem");
    }
  }
  return 1;
}


static int strmap_include (lua_State *L) {
  int rc;
  size_t l;
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  const char *key = agn_checklstring(L, 2, &l);
  double value = (double)agn_checknumber(L, 3);
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "strmap.include");
  }
  checkkeylength(L, l, "strmap.include");
  char *newkey = strdup(key);
  if (!newkey)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strmap.include");  /* 7.7.7 fix */
  khint_t k = kh_put(strmap, h, newkey, &rc);
  if (rc == -1) {
    free(newkey);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strmap.include");
  }
  if (rc == 0) { free(newkey); }
  kh_val(h, k) = value;
  lua_pushboolean(L, rc);
  return 1;  /* 7.7.9 fix */
}


static int strmap_purge (lua_State *L) {
  int rc = 0;
  size_t l;
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  const char *key = agn_checklstring(L, 2, &l);
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "strmap.purge");
  }
  checkkeylength(L, l, "strmap.purge");
  int shrink = agnL_optboolean(L, 3, 0);
  khint_t k = kh_get(strmap, h, key);
  if (k != kh_end(h)) {
    free((void *)kh_key(h, k));
    kh_del(strmap, h, k);
    rc = 1;
  }
  if (rc && shrink) {
    kh_resize(strmap, h, kh_size(h));
  }
  lua_pushboolean(L, rc);
  return 1;
}


static int aux_has (khash_t(strmap) *h, lua_Number x, lua_Number eps) {
  int rc = 0;
  if (h) {
    khint_t k;
    for (k = kh_begin(h); k != kh_end(h) && !rc; ++k) {
      if (kh_exist(h, k)) {
        double d = kh_val(h, k);
        if (eps != 0.0 ? tools_approx(d, x, eps) : d == x) {
          rc = 1;
        }
      }
    }
  }
  return rc;
}

static int strmap_has (lua_State *L) {
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  lua_Number x = agn_checknumber(L, 2);
  lua_Number eps = agnL_optnonnegative(L, 3, agn_getepsilon(L));
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "strmap.has");
  }
  lua_pushboolean(L, aux_has(h, x, eps));
  return 1;
}


static int strmap_resize (lua_State *L) {
  int rc = 1;
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "strmap.resize");
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
      kh_resize(strmap, h, actual_target);
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


static int strmap_cleanse (lua_State *L) {
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "strmap.cleanse");
  } else {
    khint_t k;
    for (k = kh_begin(h); k != kh_end(h); ++k) {
      if (kh_exist(h, k)) {
        free((void *)kh_key(h, k));
      }
    }
    kh_clear(strmap, h);
  }
  return 0;
}


static int iterate (lua_State *L) {
  kh_strmap_t **ht = lua_touserdata(L, lua_upvalueindex(1));
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  khint_t k = (khint_t)lua_tointeger(L, lua_upvalueindex(2));
  if (h->n_buckets == 0 || k >= kh_end(h) ) {
    lua_pushnil(L);
    return 1;
  }
  luaL_checkstack(L, 3, "not enough stack space");
  for (; k != kh_end(h); ++k) {
    if (kh_exist(h, k)) {
      lua_pushstring(L, (const char *)kh_key(h, k));
      lua_pushnumber(L, (lua_Number)kh_val(h, k));
      lua_pushinteger(L, (int)k + 1);
      lua_replace(L, lua_upvalueindex(2));
      return 2;
    }
  }
  lua_pushnil(L);
  return 1;
}

static int strmap_iterate (lua_State *L) {  /* 7.7.3 */
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushvalue(L, 1);
  (void)h;  /* silences the unused variable warning */
  lua_pushinteger(L, (int)kh_begin(h));
  lua_pushcclosure(L, &iterate, 2);
  return 1;
}


static int strmap_attrib (lua_State *L) {
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  size_t n_occupied = h ? kh_size(h) : 0;
  size_t n_capacity = h ? h->n_buckets : 0;
  size_t n_empty = 0;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "strmap.attrib");
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


static int strmap_totable (lua_State *L)  {
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  if (!h)  {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash map.", "strmap.totable");
  }
  lua_createtable(L, (int)kh_size(h), 0);
  khint_t k;
  for (k=kh_begin(h); k != kh_end(h); ++k)  {
    if (kh_exist(h, k))  {
      lua_rawsetstringnumber(L, -1, (const char *)kh_key(h, k), (lua_Number)kh_val(h, k));
    }
  }
  return 1;
}


/*** Metamethods **********************************************************************/

static int mt_index (lua_State *L) {
  return strmap_getitem(L);
}


static int mt_getsize (lua_State *L)  {
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  lua_pushinteger(L, h ? (lua_Integer)kh_size(h) : 0);
  return 1;
}

static int mt_empty (lua_State *L)  {
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  lua_pushboolean(L, h ? kh_size(h) == 0 : 1);
  return 1;
}


static int mt_filled (lua_State *L)  {
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  lua_pushboolean(L, h ? kh_size(h) != 0 : 0);
  return 1;
}


static int mt_in (lua_State *L)  {
  kh_strmap_t **ht = checkstrmap(L, 2);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  lua_Number val = agn_checknumber(L, 1);
  lua_pushboolean(L, aux_has(h, val, agn_getepsilon(L)));
  return 1;
}


static int mt_notin (lua_State *L)  {
  kh_strmap_t **ht = checkstrmap(L, 2);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  lua_Number val = agn_checknumber(L, 1);
  lua_pushboolean(L, !aux_has(h, val, agn_getepsilon(L)));
  return 1;
}


static int mt_u2string (lua_State *L) {  /* at the console, the hash table is formatted as follows: */
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%u)", h ? kh_size(h) : 0);
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid string hash table.", "strmap.__tostring");
  return 1;
}


static int mt_gc (lua_State *L)  {
  kh_strmap_t **ht = checkstrmap(L, 1);
  kh_strmap_t *h = (ht && *ht) ? *ht : NULL;
  if (h)  {
    khint_t k;
    for (k=kh_begin(h); k != kh_end(h); ++k)  {
      if (kh_exist(h, k))  {
        free((void *)kh_key(h, k));
      }
    }
    *ht = NULL;  /* 7.7.10 fix */
    kh_destroy(strmap, h);
  }
  agn_udfreeze(L, 1, 0);
  lua_setmetatabletoobject(L, 1, NULL, 1);
  lua_pushnil(L);
  lua_setfenv(L, 1);
  return 0;
}


static const struct luaL_Reg strmap_lib [] = {  /* metamethods for numeric userdata `n` */
  {"__index",      mt_index},     /* n[p], with p the index, counting from 1 */
  {"__writeindex", strmap_include},
  {"__size",       mt_getsize},   /* metamethod for `size` operator */
  {"__empty",      mt_empty},     /* metamethod for `empty` operator */
  {"__filled",     mt_filled},    /* metamethod for `filled` operator */
  {"__in",         mt_in},        /* metamethod for `in` operator */
  {"__notin",      mt_notin},     /* metamethod for `notin` operator */
  {"__tostring",   mt_u2string},  /* for output at the console, e.g. print(n) */
  {"__gc",         mt_gc},        /* please do not forget garbage collection */
  {NULL, NULL}
};


static const luaL_Reg strmaplib[] = {
  {"attrib",      strmap_attrib},
  {"cleanse",     strmap_cleanse},
  {"getitem",     strmap_getitem},
  {"has",         strmap_has},
  {"include",     strmap_include},
  {"iterate",     strmap_iterate},
  {"new",         strmap_new},
  {"purge",       strmap_purge},
  {"resize",      strmap_resize},
  {"totable",     strmap_totable},
  {NULL, NULL}
};


/*
** Open strmap library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_STRMAPLIBNAME);  /* create metatable */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, strmap_lib);  /* methods */
}

LUALIB_API int luaopen_strmap (lua_State *L) {
  createmeta(L);
  luaL_register(L, AGENA_STRMAPLIBNAME, strmaplib);
  return 1;
}

