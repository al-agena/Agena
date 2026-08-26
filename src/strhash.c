/* strhash Library, implements hash sets for Agena strings.
   created with the help of Gemini AI on July 07, 2026 and put to the public domain.
   Agena C package functions by a_walz.

   This is a working edition, successfully checked with Valgrind.

   It stores k~v pairs with k a string and v a double.

   Usage:

   import strhash

   h := strhash.new()
   strhash.include(h, "apple")
   strhash.include(h, "banana")
   strhash.getitem(h, "banana"):
   strhash.getitem(h, "anana"):
   strhash.purge(h, "banana");
   strhash.getitem(h, "banana"):

   strhash.attrib(h):

   strhash.totable(h):

   strhash.resize(h, 16):
   true    16

   strhash.toseq(h):
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define strhash_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnhlps.h"  /* include before khash.h */
#include "agnxlib.h"

#include "khash.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_STRHASHLIBNAME "strhash"
LUALIB_API int (luaopen_strhash) (lua_State *L);
#endif


#define checkstrhash(L,n) ((kh_strhash_t **)luaL_checkudata(L, n, AGENA_STRHASHLIBNAME))

#define isstrhash(L,n)    (luaL_isudata(L, n, AGENA_STRHASHLIBNAME) && agn_isutypeset(L, n))
#define checkkeylength(L,len,pn) { \
  if (len > 1073741823) { \
    luaL_error(L, "Error in " LUA_QS ": string key is too large.", pn); \
  } \
}


/*** Agena C Functions ****************************************************************/

KHASH_SET_INIT_STR(strhash)

static int strhash_new (lua_State *L) {
  khash_t(strhash) **ht;
  size_t size = (size_t)tools_nextpow2u32(agnL_optposint(L, 1, 8));
  ht = (khash_t(strhash) **)lua_newuserdata(L, sizeof(khash_t(strhash) *));
  *ht = kh_init(strhash);
  if (*ht == NULL) {
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strhash.new");
  }
  kh_resize(strhash, *ht, size);
  lua_setmetatabletoobject(L, -1, AGENA_STRHASHLIBNAME, 1);
  lua_createtable(L, 0, 0);
  lua_setfenv(L, -2);
  return 1;
}


static int strhash_getitem (lua_State *L) {
  int nargs = lua_gettop(L);
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strhash.getitem");
  }
  if (nargs == 1) {
    lua_getfenv(L, 1);
  } else {
    if (agn_isstring(L, 2)) {
      size_t l;
      const char *key = lua_tolstring(L, 2, &l);
      checkkeylength(L, l, "strhash.getitem");
      khint_t k = kh_get(strhash, h, key);
      lua_pushboolean(L, k != kh_end(h));
    } else {
      luaL_error(L, "Error in " LUA_QS ": invalid second argument given.", "strhash.getitem");
    }
  }
  return 1;
}


static int strhash_has (lua_State *L) {  /* new 7.7.11 */
  size_t l;
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strhash.has");
  }
  const char *key = agn_checklstring(L, 2, &l);
  checkkeylength(L, l, "strhash.getitem");
  khint_t k = kh_get(strhash, h, key);
  lua_pushboolean(L, k != kh_end(h));
  return 1;
}


static int strhash_include (lua_State *L) {
  int rc;
  size_t l;
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  const char *key = agn_checklstring(L, 2, &l);
  checkkeylength(L, l, "strhash.include");
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strhash.include");
  }
  khint_t k = kh_get(strhash, h, key);
  if (k != kh_end(h)) {
    lua_pushboolean(L, 0);
    return 1;
  }
  char *newkey = strdup(key);
  if (!newkey) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strhash.include");  /* 7.7.7 fix */
  k = kh_put(strhash, h, newkey, &rc);
  if (rc == -1) {
    free(newkey);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strhash.include");
  }
  if (rc == 0) { free(newkey); }
  lua_pushboolean(L, rc);
  return 0;
}


static int strhash_purge (lua_State *L) {
  int rc = 0;
  size_t l;
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strhash.purge");
  }
  const char *key = agn_checklstring(L, 2, &l);
  checkkeylength(L, l, "strhash.purge");
  int shrink = agnL_optboolean(L, 3, 0);
  khint_t k = kh_get(strhash, h, key);
  if (k != kh_end(h)) {
    free((void *)kh_key(h, k));
    kh_del(strhash, h, k);
    rc = 1;
  }
  if (rc && shrink) {
    kh_resize(strhash, h, kh_size(h));
  }
  lua_pushboolean(L, rc);
  return 1;
}


static int strhash_cleanse (lua_State *L) {
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strhash.cleanse");
  } else {
    khint_t k;
    for (k = kh_begin(h); k != kh_end(h); ++k) {
      if (kh_exist(h, k)) {
        free((void *)kh_key(h, k));
      }
    }
    kh_clear(strhash, h);
  }
  return 0;
}


static int strhash_resize (lua_State *L) {
  int rc = 1;
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strhash.resize");
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
      kh_resize(strhash, h, actual_target);
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


static int iterate (lua_State *L) {
  kh_strhash_t **ht = lua_touserdata(L, lua_upvalueindex(1));
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  khint_t k = (khint_t)lua_tointeger(L, lua_upvalueindex(2));
  if (h->n_buckets == 0 || k >= kh_end(h) ) {
    lua_pushnil(L);
    return 1;
  }
  luaL_checkstack(L, 2, "not enough stack space");
  for (; k != kh_end(h); ++k) {
    if (kh_exist(h, k)) {
      lua_pushstring(L, (const char *)kh_key(h, k));
      lua_pushinteger(L, (int)k + 1);
      lua_replace(L, lua_upvalueindex(2));
      return 1;
    }
  }
  lua_pushnil(L);
  return 1;
}

static int strmap_iterate (lua_State *L) {  /* 7.7.3 */
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushvalue(L, 1);
  (void)h;  /* silences the unused variable warning */
  lua_pushinteger(L, (int)kh_begin(h));
  lua_pushcclosure(L, &iterate, 2);
  return 1;
}


static int strhash_attrib (lua_State *L) {
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strhash.attrib");
  }
  size_t n_occupied = h ? kh_size(h) : 0;
  size_t n_capacity = h ? h->n_buckets : 0;
  size_t n_empty = 0;
  if (h) {
    khint_t k = kh_begin(h);
    khint_t end = kh_end(h);
    for (; k != end; ++k) {
      if (!kh_exist(h, k)) {
        n_empty++;
      }
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


static int strhash_totable (lua_State *L) {
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  int idx = 1;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strset.totable");
  }
  lua_createtable(L, (int)kh_size(h), 0);
  khint_t k;
  for (k=kh_begin(h); k != kh_end(h); ++k) {
    if (kh_exist(h, k)) {
      const char *s = kh_key(h, k);
      lua_rawsetilstring(L, -1, idx++, s, strlen(s));
    }
  }
  return 1;
}


static int strhash_toseq (lua_State *L) {
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  int idx = 1;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strset.toseq");
  }
  agn_createseq(L, (int)kh_size(h));
  khint_t k;
  for (k=kh_begin(h); k != kh_end(h); ++k) {
    if (kh_exist(h, k)) {
      const char *s = kh_key(h, k);
      lua_seqrawsetilstring(L, -1, idx++, s, strlen(s));
    }
  }
  return 1;
}


/*** Metamethods **********************************************************************/

static int mt_index (lua_State *L) {
  return strhash_getitem(L);
}


static int mt_getsize (lua_State *L) {
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strhash.__getsize");
  }
  lua_pushinteger(L, h ? (lua_Integer)kh_size(h) : 0);
  return 1;
}

static int mt_empty (lua_State *L) {
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strhash.__empty");
  }
  lua_pushboolean(L, h ? kh_size(h) == 0 : 1);
  return 1;
}


static int mt_filled (lua_State *L) {
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strhash.__filled");
  }
  lua_pushboolean(L, h ? kh_size(h) != 0 : 0);
  return 1;
}


static int aux_in (lua_State *L, const char *opname) {
  kh_strhash_t **ht = checkstrhash(L, 2);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", opname);
  }
  if (agn_isstring(L, 1)) {
    size_t l;
    const char *key = lua_tolstring(L, 1, &l);
    checkkeylength(L, l, opname);
    khint_t k = kh_get(strhash, h, key);
    return (k != kh_end(h));
  }
  luaL_error(L, "Error in " LUA_QS ": invalid first operand given.", opname);
  return 0;
}

static int mt_in (lua_State *L) {
  lua_pushboolean(L, aux_in(L, "strhash.__in"));
  return 1;
}

static int mt_notin (lua_State *L) {
  lua_pushboolean(L, !aux_in(L, "strhash.__notin"));
  return 1;
}


static int mt_u2string (lua_State *L) {  /* at the console, the hash table is formatted as follows: */
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (!h) {
    luaL_error(L, "Error in " LUA_QS ": uninitialized hash set.", "strhash.__tostring");
  }
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%u)", h ? kh_size(h) : 0);
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid string hash table.", "strhash.__tostring");
  return 1;
}


static int mt_gc (lua_State *L) {
  kh_strhash_t **ht = checkstrhash(L, 1);
  kh_strhash_t *h = (ht && *ht) ? *ht : NULL;
  if (h) {
    khint_t k;
    for (k=kh_begin(h); k != kh_end(h); ++k) {
      if (kh_exist(h, k)) {
        free((void *)kh_key(h, k));
      }
    }
    *ht = NULL;  /* 7.7.10 fix */
    kh_destroy(strhash, h);
  }
  agn_udfreeze(L, 1, 0);
  lua_setmetatabletoobject(L, 1, NULL, 1);
  lua_pushnil(L);
  lua_setfenv(L, 1);
  return 0;
}


static const struct luaL_Reg strhash_lib [] = {  /* metamethods for numeric userdata `n` */
  {"__index",     mt_index},     /* n[p], with p the index, counting from 1 */
  {"__writeindex", strhash_include},
  {"__size",      mt_getsize},   /* metamethod for `size` operator */
  {"__empty",     mt_empty},     /* metamethod for `empty` operator */
  {"__filled",    mt_filled},    /* metamethod for `filled` operator */
  {"__in",        mt_in},        /* metamethod for `in` operator */
  {"__notin",     mt_notin},     /* metamethod for `notin` operator */
  {"__tostring",  mt_u2string},  /* for output at the console, e.g. print(n) */
  {"__gc",        mt_gc},        /* please do not forget garbage collection */
  {NULL, NULL}
};


static const luaL_Reg strhashlib[] = {
  {"attrib",      strhash_attrib},
  {"cleanse",     strhash_cleanse},
  {"getitem",     strhash_getitem},
  {"has",         strhash_has},
  {"include",     strhash_include},
  {"iterate",     strmap_iterate},
  {"new",         strhash_new},
  {"purge",       strhash_purge},
  {"resize",      strhash_resize},
  {"toseq",       strhash_toseq},
  {"totable",     strhash_totable},
  {NULL, NULL}
};


/*
** Open strhash library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_STRHASHLIBNAME);  /* create metatable */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, strhash_lib);  /* methods */
}

LUALIB_API int luaopen_strhash (lua_State *L) {
  createmeta(L);
  luaL_register(L, AGENA_STRHASHLIBNAME, strhashlib);
  return 1;
}

