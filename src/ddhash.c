/* Double-to-Double Hash Library, based on C functions (strhash.c) created by Gemini AI and put to the public domain.

   This is a working edition, successfully checked with Valgrind. Started May 19, 2026.

   It stores k~v pairs with k an Agena number (C double) and v an Agena number (C double), too.

   Performance:

import ddhash
include := ddhash.include
get := ddhash.getitem

t := [];
watch()
for key to 100k do
   t[key] := sqrt(key);
   x := t[key]
od;
watch():
#0.01800012588501

s := ddhash.new(196k);
watch();
for key to 100k do
   s[key] := sqrt(key);
   y := s[key]
od;
watch():
#0.047000169754028 (linear step size: 0.055000066757202)

s := ddhash.new(196k);
watch();
for key to 100k do
   include(s, key, sqrt(key))
   z := get(s, key)
od;
watch():
#0.046999931335449 (linear step size: 0.051000118255615)

a := ddhash.new()
a[Pi] := E
a[E] := Pi

for i := 0, i < ddhash.attrib(a).capacity do
   x, y, z := ddhash.getbyhash(a, i++);
   if x then print(i - 1, x, y, z) fi
od;
*/

#define ddhash_c
#define LUA_LIB

#define checkddhash(L,n) (DdHashTable *)luaL_checkudata(L, n, AGENA_DDHASHLIBNAME)
#define isddhash(L,n)    (luaL_isudata(L, n, AGENA_DDHASHLIBNAME) && agn_isutypeset(L, n))

#include <inttypes.h>  /* for PRId64 format specifier */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "agena.h"

#include "agenalib.h"
#include "agnhlps.h"  /* for UNIX cksum hash, etc. */
#include "agnxlib.h"
#include "ddhash.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_DDHASHLIBNAME "ddhash"
LUALIB_API int (luaopen_ddhash) (lua_State *L);
#endif


static FORCE_INLINE unsigned int hash (lua_Number n) {  /* 2 % lookup tweak */
  uint64_t bits;
  uint32_t h;
  n += 0.0;  /* normalise -0.0 to 0.0 */
  tools_memcpy(&bits, &n, sizeof(bits));
  /* Fold 64 bits down to 32 bits using an initial XOR fold */
  h = (uint32_t)(bits ^ (bits >> 32));
  /* MurmurHash3 32-bit avalanche mixer (fmix32) */
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

/* Internal helper: Fast-track insertion for rehashing */
static void upsert_rehash (DdHashTable *ht, DdHashTableEntry *old_entry) {
  size_t i, idx;
  size_t slot = (size_t)hash(old_entry->key) & (ht->capacity - 1);
  for (i=0; i < ht->capacity; i++) {
    idx = (slot + ((i * i + i) >> 1)) & (ht->capacity - 1);
    if (ht->entries[idx].status != DDHASH_OCCUPIED) {
      ht->entries[idx] = *old_entry;
      ht->count++;
      return;
    }
  }
}

/* Internal helper: Resize logic using the power-of-two growth rule */
static int ddht_resize (DdHashTable *ht) {
  size_t i, new_size = ht->capacity * 2;
  DdHashTableEntry *new_entries = calloc(new_size, sizeof(DdHashTableEntry));
  if (!new_entries) return 0;
  DdHashTableEntry *old_entries = ht->entries;
  size_t old_size = ht->capacity;
  ht->entries = new_entries;
  ht->capacity = new_size;
  ht->count = 0;
  for (i=0; i < old_size; i++) {
    if (old_entries[i].status == DDHASH_OCCUPIED) {
      upsert_rehash(ht, &old_entries[i]);
    }
  }
  xfree(old_entries);
  return 1;
}

/* Create a new hash table ensuring initial capacity is a power of 2 */
DdHashTable *ddht_create (size_t size) {
  DdHashTable *ht = malloc(sizeof(DdHashTable));
  if (!ht) return NULL;
  size_t p2_size = 16;
  while (p2_size < size) p2_size <<= 1;
  ht->capacity = p2_size;
  ht->count = 0;
  ht->entries = calloc(p2_size, sizeof(DdHashTableEntry));
  if (!ht->entries) { free(ht); return NULL; }
  return ht;
}

/* Free all keys and the table structure itself */
void ddht_destroy (DdHashTable *ht, int all) {
  if (!ht) return;
  xfree(ht->entries);
  if (all) { xfree(ht); }
}

/* Internal upsert: handles both new inserts and updates */
static int upsert (DdHashTable *ht, double key, double value) {
  size_t i, idx, slot, first_free_idx, target;
  if (ht->count + 1 > ht->capacity * 0.50) {
    if (!ddht_resize(ht)) return -1;
  }
  /* Optimization: modulo by a power of 2 can use bitwise AND instead of % */
  slot = (size_t)hash(key) & (ht->capacity - 1);
  first_free_idx = (size_t)-1;
  for (i=0; i < ht->capacity; i++) {
    /* Triangular progression: (i*i + i) / 2, 7.5.11 */
    idx = (slot + ((i * i + i) >> 1)) & (ht->capacity - 1);
    DdHashTableEntry *e = &ht->entries[idx];
    if (e->status == DDHASH_EMPTY) {
      target = (first_free_idx != (size_t)-1) ? first_free_idx : idx;
      DdHashTableEntry *dest = &ht->entries[target];
      dest->key = key;
      dest->value = value;
      dest->status = DDHASH_OCCUPIED;
      ht->count++;
      return 1;
    }
    if (e->status == DDHASH_DELETED) {
      if (first_free_idx == (size_t)-1) first_free_idx = idx;
      continue;
    }
    if (e->status == DDHASH_OCCUPIED && e->key == key) {
      e->value = value;
      return 1;
    }
  }
  return 0;
}

/* User-facing API */
int ddht_insert (DdHashTable *ht, double key, double value) {
  return upsert(ht, key, value);
}

/* Retrieve entry based on key and length */
DdHashTableEntry *ddht_get (DdHashTable *ht, double key) {
  if (ht->capacity == 0) return NULL;
  size_t i, slot = (size_t)hash(key) & (ht->capacity - 1);
  for (i=0; i < ht->capacity; i++) {
    size_t idx = (slot + ((i * i + i) >> 1)) & (ht->capacity - 1);
    DdHashTableEntry *e = &ht->entries[idx];
    if (e->status == DDHASH_EMPTY) break;
    if (e->status == DDHASH_OCCUPIED && e->key == key) return e;
  }
  return NULL;
}

double ddht_getentry (DdHashTable *ht, double key, int *rc) {
  if (ht->capacity == 0) { *rc = 0; return AGN_NAN; }
  size_t i, slot = (size_t)hash(key) & (ht->capacity - 1);
  for (i=0; i < ht->capacity; i++) {
    size_t idx = (slot + ((i * i + i) >> 1)) & (ht->capacity - 1);
    DdHashTableEntry *e = &ht->entries[idx];
    if (e->status == DDHASH_EMPTY) break;
    if (e->status == DDHASH_OCCUPIED && e->key == key) {
      *rc = 1;
      return e->value;
    }
  }
  *rc = 0;
  return AGN_NAN;
}

/* Explore what is stored given a precomputed hash */
DdHashTableEntry *ddht_getbyhash (DdHashTable *ht, unsigned int hashval, size_t *idx) {
  *idx = 0;
  if (ht->capacity == 0 || !ht->entries) return NULL;
  /* Calculate the starting point based on the hash */
  size_t i, slot = (size_t)hashval & (ht->capacity - 1);
  /* Search for the first occupied slot starting from the hash index */
  for (i=0; i < ht->capacity; i++) {
    *idx = (slot + ((i * i + i) >> 1)) & (ht->capacity - 1);
    DdHashTableEntry *e = &ht->entries[*idx];
    /* If we hit an empty slot, the sequence for this hash chain ends */
    if (e->status == DDHASH_EMPTY) break;
    /* Return the first thing we find that is actually a value */
    if (e->status == DDHASH_OCCUPIED) return e;
  }
  return NULL;
}

/* Delete entry and mark slot as a tombstone */
int ddht_delete (DdHashTable *ht, double key, double eps) {
  if (ht->capacity == 0) return 0;
  size_t i, slot = (size_t)hash(key) & (ht->capacity - 1);
  for (i=0; i < ht->capacity; i++) {
    size_t idx = (slot + ((i * i + i) >> 1)) & (ht->capacity - 1);
    DdHashTableEntry *e = &ht->entries[idx];
    if (e->status == DDHASH_EMPTY) return 0;
    if (e->status == DDHASH_OCCUPIED && (eps != 0.0 ? tools_approx(e->key, key, eps) : e->key == key)) {
      e->status = DDHASH_DELETED;
      ht->count--;
      return 1;
    }
  }
  return 0;
}

int ddht_shrink (DdHashTable *ht, size_t requested_size) {
  if (requested_size < ht->count) return 0;
  size_t p2_size = 16;
  while (p2_size < requested_size) p2_size <<= 1;
  DdHashTableEntry *new_entries = calloc(p2_size, sizeof(DdHashTableEntry));
  if (!new_entries) return -1;
  DdHashTableEntry *old_entries = ht->entries;
  size_t i, old_size = ht->capacity;
  ht->entries = new_entries;
  ht->capacity = p2_size;
  ht->count = 0;
  for (i=0; i < old_size; i++) {
    if (old_entries[i].status == DDHASH_OCCUPIED) {
      upsert_rehash(ht, &old_entries[i]);
    }
  }
  xfree(old_entries);
  return 1;
}

/*** Agena C Functions ****************************************************************/

static int ddhash_new (lua_State *L) {
  DdHashTable *ht;
  size_t size = (size_t)tools_nextpow2u32(agnL_optposint(L, 1, 8));  /* smallest power of 2 greater than _OR_ equal x */
  ht = (DdHashTable *)lua_newuserdata(L, sizeof(DdHashTable));
  ht->entries = calloc(size, sizeof(DdHashTableEntry));
  if (ht->entries == NULL) {
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ddhash.new");
  }
  ht->capacity = size;
  ht->count = 0;
  lua_setmetatabletoobject(L, -1, AGENA_DDHASHLIBNAME, 1);
  lua_createtable(L, 0, 0);  /* create accompanying status table */
  lua_setfenv(L, -2);
  return 1;
}


static int ddhash_getitem (lua_State *L) {
  int nargs = lua_gettop(L);
  DdHashTable *ht = checkddhash(L, 1);
  if (nargs == 1) {  /* get internal environment/admin table */
    lua_getfenv(L, 1);
    return 1;
  }
  double key = (double)agn_checknumber(L, 2);
  DdHashTableEntry *entry = ddht_get(ht, key);
  if (entry && entry->status == DDHASH_OCCUPIED) {
    lua_pushnumber(L, entry->value);
  } else {
    lua_pushnil(L);
  }
  return 1;
}



static int ddhash_getbyhash (lua_State *L) {
  size_t idx;
  DdHashTable *ht = checkddhash(L, 1);
  unsigned int hashval = (unsigned int)agn_checknonnegint(L, 2);
  DdHashTableEntry *entry = ddht_getbyhash(ht, hashval, &idx);
  luaL_checkstack(L, 3, "not enough stack space");
  if (entry && entry->status == DDHASH_OCCUPIED) {
    lua_pushnumber(L, idx);
    lua_pushnumber(L, entry->key);
    lua_pushnumber(L, entry->value);
  } else {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushnil(L);
  }
  return 3;
}


static int ddhash_include (lua_State *L) {
  int rc;
  DdHashTable *ht = checkddhash(L, 1);
  double key = (double)agn_checknumber(L, 2);
  double value = (double)agn_checknumber(L, 3);
  rc = ddht_insert(ht, key, value);
  if (rc == -1)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ddhash.include");
  lua_pushboolean(L, rc);
  return 0;
}


static int ddhash_purge (lua_State *L) {
  DdHashTable *ht = checkddhash(L, 1);
  double key = (double)agn_checknumber(L, 2);
  double eps = agnL_optnonnegative(L, 3, agn_getepsilon(L));
  int shrink = agnL_optboolean(L, 4, 0);
  int rc = ddht_delete(ht, key, eps);
  /* Only shrink if the delete actually found and removed an item */
  if (rc && shrink) {
    if (ddht_shrink(ht, agn_newsize(NULL, ht->count)) == -1) {
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed during shrink.", "ddhash.purge");
    }
  }
  lua_pushboolean(L, rc);
  return 1;
}


static int aux_has (DdHashTable *ht, lua_Number x, lua_Number eps) {
  int rc = 0;
  if (ht->entries) {
    size_t i;
    for (i=0; i < ht->capacity && !rc; i++) {
      DdHashTableEntry *e = &ht->entries[i];
      rc = (e->status == DDHASH_OCCUPIED) && (eps != 0.0 ? tools_approx(e->value, x, eps) : e->value == x);
    }
  }
  return rc;
}

static int ddhash_has (lua_State *L) {
  DdHashTable *ht = checkddhash(L, 1);
  lua_Number x = agn_checknumber(L, 2);
  lua_Number eps = agnL_optnonnegative(L, 3, agn_getepsilon(L));
  lua_pushboolean(L, aux_has(ht, x, eps));
  return 1;
}


static int ddhash_attrib (lua_State *L) {
  DdHashTable *ht = checkddhash(L, 1);
  size_t i, deleted_count = 0;
  /* 1. Safety Check: Ensure entries exist before looping */
  if (ht->entries) {
    for (i=0; i < ht->capacity; i++) {
      /* Access status inside the entry struct */
      if (ht->entries[i].status == DDHASH_DELETED) {
        deleted_count++;
      }
    }
  }
  lua_createtable(L, 0, 6);
  /* Use the correct pointer name 'ht' */
  lua_rawsetstringnumber(L, -1, "count",     (double)ht->count);
  lua_rawsetstringnumber(L, -1, "capacity",  (double)ht->capacity);
  lua_rawsetstringnumber(L, -1, "deleted",   (double)deleted_count);
  lua_rawsetstringnumber(L, -1, "void",      (double)(ht->capacity - ht->count - deleted_count));  /* we have the reserved `empty` keyword in Agena */
  /* 2. Guard against Divide-by-Zero */
  double cap = (double)ht->capacity;
  lua_rawsetstringnumber(L, -1, "loadfactor",  cap == 0.0 ? 0.0: (double)ht->count/cap);
  lua_rawsetstringnumber(L, -1, "utilization", cap == 0.0 ? 0.0: (double)(ht->count + deleted_count)/cap);
  return 1;
}


/* The function rehashes the table to remove all slots marked deleted and optimizes the memory footprint
   based on the current number of elements. It aims for a ~50% load factor, with a minimum of eight
   preallocated slots. The function returns nothing. See also: ddhash.attrib, ddhash.purge.
   Written by Gemini AI, January 06, 2026. 6.5.12
   We deliberately use the name `compact` as it works differently from `lifo.shrink`, etc. */
static int ddhash_compact (lua_State *L) {
  DdHashTable *ht = checkddhash(L, 1);
  /* aim for a ~50% load factor. If count is 0, we still need a small minimum capacity. */
  size_t target_count = (ht->count < 8) ? 8 : ht->count * 2;
  size_t new_cap = tools_nextpow2u32(target_count);
  /* Even if new_cap == u->capacity, the next call will still rebuild the table and clear all tombstones. */
  if (ddht_shrink(ht, new_cap) == -1) {
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ddhash.compact");
  }
  return 0;
}


/* Returns the high and lower unsigned 32-bit words of the UNIX cksum hash for number x.
   Depending on the hash used, only lx might be non-zero.

   If a hash function returns a 32-bit integer, you cannot reassamble the hash with bytes.setnumwords.
   Instead the hash value simply is lx. */
static int ddhash_hash (lua_State *L) {
  uint64_t h;
  uint32_t n, hx, lx, cx;
  double x = (double)agn_checknumber(L, 1);
  n = agnL_optuint32_t(L, 2, 0);
  h = hash(x);
  luaL_checkstack(L, 3, "not enough stack space");
  hx = (uint32_t)(h >> 32);          /* high */
  lx = (uint32_t)(h & 0xFFFFFFFFU);  /* low */
  cx = hx ^ lx;  /* high and low combined, equal to (uint32_t)(h ^ (h >> 32)) */
  if (n == 0) {
    /* return raw 32-bit unsigned integers */
    lua_pushnumber(L, (lua_Number)(unsigned int)hx);
    lua_pushnumber(L, (lua_Number)(unsigned int)lx);
    lua_pushnumber(L, (lua_Number)(unsigned int)cx);
  } else {
    /* return values constrained to [0, n-1] */
    lua_pushnumber(L, (lua_Number)(hx % n));
    lua_pushnumber(L, (lua_Number)(lx % n));
    lua_pushnumber(L, (lua_Number)(cx % n));
  }
  return 3;
}


/*** Metamethods **********************************************************************/

static int mt_index (lua_State *L) {
  /* if (!agn_isinteger(L, 2))  // OOP method
    return agn_initmethodcall(L, AGENA_DDHASHLIBNAME, sizeof(AGENA_DDHASHLIBNAME) - 1); */
  return ddhash_getitem(L);
}


static int mt_getsize (lua_State *L) {
  DdHashTable *ht = checkddhash(L, 1);
  lua_pushinteger(L, ht->count);
  return 1;
}


static int mt_empty (lua_State *L) {
  DdHashTable *ht = checkddhash(L, 1);
  lua_pushboolean(L, ht->count == 0);
  return 1;
}


static int mt_filled (lua_State *L) {
  DdHashTable *ht = checkddhash(L, 1);
  lua_pushboolean(L, ht->count != 0);
  return 1;
}


static int mt_in (lua_State *L) {
  DdHashTable *ht = checkddhash(L, 2);
  lua_Number x = agn_checknumber(L, 1);
  lua_pushboolean(L, aux_has(ht, x, agn_getepsilon(L)));
  return 1;
}


static int mt_notin (lua_State *L) {
  DdHashTable *ht = checkddhash(L, 2);
  lua_Number x = agn_checknumber(L, 1);
  lua_pushboolean(L, !aux_has(ht, x, agn_getepsilon(L)));
  return 1;
}


static int mt_u2string (lua_State *L) {  /* at the console, the hash table is formatted as follows: */
  DdHashTable *ht = checkddhash(L, 1);
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%u)", (unsigned int)ht->count);
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid dd hash table.", "ddhash.__tostring");
  return 1;
}


static int mt_gc (lua_State *L) {  /* __gc method */
  DdHashTable *ht = checkddhash(L, 1);
  /* remove read-only mode if set */
  agn_udfreeze(L, 1, 0);
  /* delete metatable and user-defined type */
  lua_setmetatabletoobject(L, 1, NULL, 1);
  /* delete registry table */
  lua_pushnil(L);  /* destroy environment table */
  lua_setfenv(L, 1);
  ddht_destroy(ht, 0);
  return 0;
}


static const struct luaL_Reg ddhash_lib [] = {  /* metamethods for numeric userdata `n` */
  {"__index",     mt_index},     /* n[p], with p the index, counting from 1 */
  {"__writeindex", ddhash_include},
  {"__size",      mt_getsize},   /* metamethod for `size` operator */
  {"__empty",     mt_empty},     /* metamethod for `empty` operator */
  {"__filled",    mt_filled},    /* metamethod for `filled` operator */
  {"__in",        mt_in},        /* metamethod for `in` operator */
  {"__notin",     mt_notin},     /* metamethod for `notin` operator */
  {"__tostring",  mt_u2string},  /* for output at the console, e.g. print(n) */
  {"__gc",        mt_gc},        /* please do not forget garbage collection */
  {NULL, NULL}
};


static const luaL_Reg ddhashlib[] = {
  {"attrib",      ddhash_attrib},
  {"compact",     ddhash_compact},
  {"getitem",     ddhash_getitem},
  {"getbyhash",   ddhash_getbyhash},
  {"has",         ddhash_has},
  {"hash",        ddhash_hash},
  {"include",     ddhash_include},
  {"new",         ddhash_new},
  {"purge",       ddhash_purge},
  {NULL, NULL}
};


/*
** Open ddhash library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_DDHASHLIBNAME);  /* create metatable */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, ddhash_lib);  /* methods */
}

LUALIB_API int luaopen_ddhash (lua_State *L) {
  createmeta(L);
  luaL_register(L, AGENA_DDHASHLIBNAME, ddhashlib);
  return 1;
}

