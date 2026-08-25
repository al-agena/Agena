/*
** $Id: cuckoo.c,v 0.1 15/06/2018 $
** Cuckoo filter
** See Copyright Notice in agena.h
*/

/* The following functions have all been written by Jonah H. Harris, published at:

   https://github.com/jonahharris/libcuckoofilter/tree/master

   From Mr. Harris' Github pages:

   "Similar to a Bloom filter, a Cuckoo filter provides a space-efficient data structure
   designed to answer approximate set-membership queries (e.g. "is item x contained in this
   set?") Unlike standard Bloom filters, however, Cuckoo filters support deletion. Likewise,
   Cuckoo filters are more optimal than Bloom variants which support deletion, such as
   counting Bloom filters, in both space and time.

   Cuckoo filters are based on cuckoo hashing. A Cuckoo filter is essentially a cuckoo
   hash table which stores each key's fingerprint. As Cuckoo hash tables are highly compact,
   a cuckoo filter often requires less space than conventional Bloom filters for applications
   that require low false positive rates (< 3%).

   This library was designed to provide a target false positive probability of ~P(0.001) and
   was hard-coded to use sixteen bits per item and four nests per bucket. As it uses two hashes,
   it's a (2, 4)-cuckoo filter."

   Licence:

   The MIT License

   Copyright (c) 2015 Jonah H. Harris

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

   Adaptions to make the code compile in GCC and Agena port by a. walz. */

#define cuckoo_c
#define LUA_LIB

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "agena.h"
#include "agenalib.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "prepdefs.h"

#define checkcuckoo(L, n) (Cuckoo *)luaL_checkudata(L, n, "cuckoo")

#define AGENA_LIBVERSION	"cuckoo 0.0.2 for Agena as of October 14, 2024\n"

#define CUCKOO_NESTS_PER_BUCKET     4

static inline uint32_t hash (const void *, uint32_t, uint32_t, uint32_t, uint32_t);

typedef enum {
  CUCKOO_FILTER_OK = 0,
  CUCKOO_FILTER_NOT_FOUND,
  CUCKOO_FILTER_FULL,
  CUCKOO_FILTER_ALLOCATION_FAILED,
} CUCKOO_FILTER_RETURN;

typedef struct cuckoo_nest_t {
  uint16_t fingerprint;
} __attribute__((packed)) cuckoo_nest_t;

typedef struct cuckoo_item_t {
  uint32_t fingerprint;
  uint32_t h1;
  uint32_t h2;
  uint32_t padding;
} __attribute__((packed)) cuckoo_item_t;

typedef struct cuckoo_result_t {
  int was_found;
  cuckoo_item_t item;
} cuckoo_result_t;

typedef struct cuckoo_filter_t {
  uint32_t       bucket_count;
  uint32_t       nests_per_bucket;
  uint32_t       mask;
  uint32_t       max_kick_attempts;
  uint32_t       seed;
  uint32_t       padding;
  size_t         allocation_in_bytes;
  uint32_t       wordsincluded;
  cuckoo_item_t  victim;
  cuckoo_item_t *last_victim;
  cuckoo_nest_t  bucket[1];
} __attribute__((packed)) cuckoo_filter_t;

typedef struct Cuckoo {
  struct cuckoo_filter_t *filter;    /* pointer to data */
  size_t size;    /* number of slots */
} Cuckoo;

/* ------------------------------------------------------------------------- */

static inline size_t next_power_of_two (size_t x) {
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
#if defined(IS64BIT)
  x |= x >> 32;
#endif
  return ++x;
}

/* ------------------------------------------------------------------------- */

static inline int add_fingerprint_to_bucket (struct cuckoo_filter_t *filter, uint32_t fp, uint32_t h) {
  size_t ii;
  for (ii = 0; ii < filter->nests_per_bucket; ++ii) {
    cuckoo_nest_t *nest = &filter->bucket[(h * filter->nests_per_bucket) + ii];
    if (0 == nest->fingerprint) {
      nest->fingerprint = fp;
      filter->wordsincluded++;
      return CUCKOO_FILTER_OK;
    }
  }
  return CUCKOO_FILTER_FULL;
}

/* ------------------------------------------------------------------------- */

static inline int remove_fingerprint_from_bucket (struct cuckoo_filter_t *filter, uint32_t fp, uint32_t h) {
  size_t ii;
  for (ii = 0; ii < filter->nests_per_bucket; ++ii) {
    cuckoo_nest_t *nest = &filter->bucket[(h * filter->nests_per_bucket) + ii];
    if (fp == nest->fingerprint) {
      nest->fingerprint = 0;
      filter->wordsincluded--;
      return CUCKOO_FILTER_OK;
    }
  }
  return CUCKOO_FILTER_NOT_FOUND;
}

/* ------------------------------------------------------------------------- */

static inline int cuckoo_filter_move (struct cuckoo_filter_t *filter, uint32_t fingerprint, uint32_t h1, int depth) {
  uint32_t h2 = ((h1 ^ hash(&fingerprint, sizeof(fingerprint),
    filter->bucket_count, 900, filter->seed)) % filter->bucket_count);
  if (CUCKOO_FILTER_OK == add_fingerprint_to_bucket(filter, fingerprint, h1)) return CUCKOO_FILTER_OK;
  if (CUCKOO_FILTER_OK == add_fingerprint_to_bucket(filter, fingerprint, h2)) return CUCKOO_FILTER_OK;
  /* printf("depth = %u\n", depth); */
  if (filter->max_kick_attempts == depth) return CUCKOO_FILTER_FULL;
  size_t row = (0 == (rand() % 2) ? h1 : h2);
  size_t col = (rand() % filter->nests_per_bucket);
  size_t elem = filter->bucket[(row * filter->nests_per_bucket) + col].fingerprint;
  filter->bucket[(row * filter->nests_per_bucket) + col].fingerprint = fingerprint;
  return cuckoo_filter_move(filter, elem, row, (depth + 1));
}

/* ------------------------------------------------------------------------- */

/* This version does not ignore user-defined seed arguments. */

int cuckoo_filter_new (struct cuckoo_filter_t **filter, size_t max_key_count, size_t max_kick_attempts, uint32_t seed) {
  struct cuckoo_filter_t *new_filter;
  size_t bucket_count = next_power_of_two(max_key_count / CUCKOO_NESTS_PER_BUCKET);
  if (0.96 < (double) max_key_count / bucket_count / CUCKOO_NESTS_PER_BUCKET) {
    bucket_count <<= 1;
  }
  /* FIXME: Should check for integer overflows here */
  size_t allocation_in_bytes = (sizeof(cuckoo_filter_t)
    + (bucket_count * CUCKOO_NESTS_PER_BUCKET * sizeof(cuckoo_nest_t)));
  new_filter = calloc(allocation_in_bytes, 1);
  if (!new_filter) return CUCKOO_FILTER_ALLOCATION_FAILED;
  new_filter->last_victim = NULL;
  memset(&new_filter->victim, 0, sizeof(new_filter)->victim);
  new_filter->bucket_count = bucket_count;
  new_filter->nests_per_bucket = CUCKOO_NESTS_PER_BUCKET;
  new_filter->max_kick_attempts = max_kick_attempts;
  new_filter->seed = (size_t)seed;  /* (size_t)time(NULL); */
  /* new_filter->seed = (size_t)10301212; */
  new_filter->mask = (uint32_t)((1U << (sizeof(cuckoo_nest_t) * 8)) - 1);
  new_filter->allocation_in_bytes = allocation_in_bytes;
  *filter = new_filter;
  (*filter)->wordsincluded = 0;
  return CUCKOO_FILTER_OK;
}

/* ------------------------------------------------------------------------- */

static int cuckoo_filter_free (struct cuckoo_filter_t **filter) {
  free(*filter);
  *filter = NULL;
  return CUCKOO_FILTER_OK;
}

/* ------------------------------------------------------------------------- */

static inline int cuckoo_filter_lookup (struct cuckoo_filter_t *filter, cuckoo_result_t *result, void *key, size_t key_length_in_bytes) {
  uint32_t fingerprint = hash(key, key_length_in_bytes, filter->bucket_count, 1000, filter->seed);
  uint32_t h1 = hash(key, key_length_in_bytes, filter->bucket_count, 0, filter->seed);
  fingerprint &= filter->mask; fingerprint += !fingerprint;
  uint32_t h2 = ((h1 ^ hash(&fingerprint, sizeof(fingerprint), filter->bucket_count, 900, filter->seed)) % filter->bucket_count);
  result->was_found = 0;
  result->item.fingerprint = 0;
  result->item.h1 = 0;
  result->item.h2 = 0;
  {
    size_t ii;
    for (ii = 0; ii < filter->nests_per_bucket; ++ii) {
      cuckoo_nest_t *n1 = &filter->bucket[(h1 * filter->nests_per_bucket) + ii];
      if (fingerprint == n1->fingerprint) {
        result->was_found = 1;
        break;
      }
      cuckoo_nest_t *n2 = &filter->bucket[(h2 * filter->nests_per_bucket) + ii];
      if (fingerprint == n2->fingerprint) {
        result->was_found = 1;
        break;
      }
    }
  }
  result->item.fingerprint = fingerprint;
  result->item.h1 = h1;
  result->item.h2 = h2;
  return (1 == result->was_found) ? CUCKOO_FILTER_OK : CUCKOO_FILTER_NOT_FOUND;
}


/* Get hash values used with a given Cuckoo filter. Based on cuckoo_filter_lookup, added by a. walz */
static uint32_t cuckoo_filter_fingerprint (struct cuckoo_filter_t *filter, void *key, size_t key_length_in_bytes, uint32_t *fingerprint) {
  *fingerprint = hash(key, key_length_in_bytes, filter->bucket_count, 1000, filter->seed);
  uint32_t fingerprint2 = *fingerprint & filter->mask;
  fingerprint2 += !fingerprint2;
  return fingerprint2;
}

/* ------------------------------------------------------------------------- */

static int cuckoo_filter_add (struct cuckoo_filter_t *filter, void *key, size_t key_length_in_bytes) {
  cuckoo_result_t result;
  cuckoo_filter_lookup(filter, &result, key, key_length_in_bytes);
  if (1 == result.was_found) return CUCKOO_FILTER_OK;
  if (NULL != filter->last_victim) return CUCKOO_FILTER_FULL;
  return cuckoo_filter_move(filter, result.item.fingerprint, result.item.h1, 0);
}

/* ------------------------------------------------------------------------- */

static int cuckoo_filter_remove (struct cuckoo_filter_t *filter, void *key, size_t key_length_in_bytes) {
  cuckoo_result_t result;
  int was_deleted = 0;
  cuckoo_filter_lookup(filter, &result, key, key_length_in_bytes);
  if (0 == result.was_found) return CUCKOO_FILTER_NOT_FOUND;
  if (CUCKOO_FILTER_OK == remove_fingerprint_from_bucket(filter, result.item.fingerprint, result.item.h1)) {
    was_deleted = 1;
  } else if (CUCKOO_FILTER_OK == remove_fingerprint_from_bucket(filter, result.item.fingerprint, result.item.h2)) {
    was_deleted = 1;
  }
  if (0 && ((1 == was_deleted) & (NULL != filter->last_victim))) {  /* deleted and filter is full ? */
    /* ??? empty statement in original code ??? */
  }
  return (was_deleted == 1) ? CUCKOO_FILTER_OK : CUCKOO_FILTER_NOT_FOUND;
}

/* ------------------------------------------------------------------------- */

static int cuckoo_filter_contains (struct cuckoo_filter_t *filter, void *key, size_t key_length_in_bytes) {
  cuckoo_result_t result;
  return cuckoo_filter_lookup(filter, &result, key, key_length_in_bytes);
}

/* ------------------------------------------------------------------------- */

static inline uint32_t hash (const void *key, uint32_t key_length_in_bytes, uint32_t size, uint32_t n, uint32_t seed) {
  uint32_t h1 = tools_murmurhash3(key, key_length_in_bytes, seed);
  uint32_t h2 = tools_murmurhash3(key, key_length_in_bytes, h1);
  return ((h1 + (n * h2)) % size);
}

/* end of Jonah H. Harris' code ***************************************/


/* cuckoo.new([m [, n [, s]])

   Creates a Cuckoo filter, of type userdata. m depicts the maximum number of slots and by default is 500,000.
   n is the maximum number of kicks, defaulting to 100, and s is an optional seed which by default is the number
   of seconds elapsed since the given epoch. */
static int cuckoo_new (lua_State *L) {
  Cuckoo *a;
  size_t max_key_count, max_kick_attempts;
  uint32_t seed;
  int rc;
  max_key_count = agnL_optposint(L, 1, 500000);
  max_kick_attempts = agnL_optposint(L, 2, 100);
  seed = (uint32_t)agnL_optposint(L, 3, (uint32_t)(time(NULL) & 0xffffffff));  /* or use (size_t)10301212 */
  a = (Cuckoo *)lua_newuserdata(L, sizeof(Cuckoo));
  rc = cuckoo_filter_new(&a->filter, max_key_count, max_kick_attempts, seed);
  if (rc != CUCKOO_FILTER_OK) {
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "cuckoo.new");
  }
  lua_setmetatabletoobject(L, -1, "cuckoo", 1);
  /* agn_setutypestring(L, -1, "cuckoo"); */
  a->size = max_key_count;
  return 1;
}


/* cuckoo.find(c, s);

   Tries to find string s in Cuckoo filter c. It returns `false` if s is definitely not in the filter, and `true`
   if it is likely that s is part of the filter. */
static int cuckoo_find (lua_State *L) {
  size_t l;
  const char *str;
  Cuckoo *a = checkcuckoo(L, 1);
  str = agn_checklstring(L, 2, &l);
  lua_pushboolean(L, cuckoo_filter_contains(a->filter, (char *)str, l) == CUCKOO_FILTER_OK);
  return 1;
}


static int cuckoo_hash (lua_State *L) {
  size_t l;
  uint32_t fingerprint, fingerprint2;
  const char *str;
  Cuckoo *a = checkcuckoo(L, 1);
  str = agn_checklstring(L, 2, &l);
  fingerprint2 = cuckoo_filter_fingerprint(a->filter, (char *)str, l, &fingerprint);
  lua_pushnumber(L, fingerprint);
  lua_pushnumber(L, fingerprint2);
  return 2;
}


/* cuckoo.include(c, s);

   Inserts string s into Cuckoo filter c. It returns `true` on success and `false` otherwise. If the maximum number of
   entries has been exceeded, the function deliberately issues an error. */
static int cuckoo_include (lua_State *L) {
  int rc;
  size_t l;
  const char *str;
  Cuckoo *a = checkcuckoo(L, 1);
  str = agn_checklstring(L, 2, &l);
  rc = cuckoo_filter_add(a->filter, (char *)str, l);
  if (rc == CUCKOO_FILTER_FULL)
    luaL_error(L, "Error in " LUA_QS ": the filter is full.", "cuckoo.include");
  lua_pushboolean(L, rc == CUCKOO_FILTER_OK);
  return 1;
}


/* cuckoo.remove(c, s);

   Deletes string s from Cuckoo filter c. It returns `true` on success and `false` if s has not been found. In all
   other cases it returns `fail`. */
static int cuckoo_remove (lua_State *L) {
  int rc;
  size_t l;
  const char *str;
  Cuckoo *a = checkcuckoo(L, 1);
  str = agn_checklstring(L, 2, &l);
  rc = cuckoo_filter_remove(a->filter, (char *)str, l);
  switch (rc) {
    case CUCKOO_FILTER_OK:
      lua_pushtrue(L);
      break;
    case CUCKOO_FILTER_NOT_FOUND:
      lua_pushfalse(L);
      break;
    default:
      lua_pushfail(L);
  }
  return 1;
}


static int mt_in (lua_State *L) {  /* 5.5.11 */
  size_t l;
  const char *str;
  Cuckoo *a = checkcuckoo(L, 2);
  str = agn_checklstring(L, 1, &l);
  lua_pushboolean(L, cuckoo_filter_contains(a->filter, (char *)str, l) == CUCKOO_FILTER_OK);
  return 1;
}


static int mt_notin (lua_State *L) {  /* 5.5.11 */
  size_t l;
  const char *str;
  Cuckoo *a = checkcuckoo(L, 2);
  str = agn_checklstring(L, 1, &l);
  lua_pushboolean(L, !cuckoo_filter_contains(a->filter, (char *)str, l) == CUCKOO_FILTER_OK);
  return 1;
}


static int mt_getsize (lua_State *L) {  /* returns the number of slots in the Cuckoo filter */
  Cuckoo *a = checkcuckoo(L, 1);
  lua_pushinteger(L, a->filter->wordsincluded);
  return 1;
}


static int mt_empty (lua_State *L) {  /* 5.5.11 */
  Cuckoo *a = checkcuckoo(L, 1);
  lua_pushboolean(L, a->filter->wordsincluded == 0);
  return 1;
}


static int mt_filled (lua_State *L) {  /* 5.5.11 */
  Cuckoo *a = checkcuckoo(L, 1);
  lua_pushboolean(L, a->filter->wordsincluded != 0);
  return 1;
}


static int mt_cuckoo2string (lua_State *L) {  /* at the console, the filter is formatted as follows: */
  Cuckoo *a;
  a = checkcuckoo(L, 1);
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%u)", a->size);
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid type.", "cuckoo.__tostring");
  return 1;
}

static int mt_cuckoogc (lua_State *L) {  /* please do not forget to garbage collect deleted userdata */
  Cuckoo *a = checkcuckoo(L, 1);
  cuckoo_filter_free(&a->filter);
  /* do not free(a) ! */
  lua_setmetatabletoobject(L, 1, NULL, 1);
  return 0;
}


static int cuckoo_attrib (lua_State *L) {
  Cuckoo *a = checkcuckoo(L, 1);
  lua_createtable(L, 0, 9);
  lua_pushstring(L, "wordsincluded");
  lua_pushnumber(L, a->filter->wordsincluded);
  lua_settable(L, -3);
  lua_pushstring(L, "bucketcount");
  lua_pushnumber(L, a->filter->bucket_count);
  lua_settable(L, -3);
  lua_pushstring(L, "nestsperbucket");
  lua_pushnumber(L, a->filter->nests_per_bucket);
  lua_settable(L, -3);
  lua_pushstring(L, "mask");
  lua_pushnumber(L, a->filter->mask);
  lua_settable(L, -3);
  lua_pushstring(L, "maxkickattempts");
  lua_pushnumber(L, a->filter->max_kick_attempts);
  lua_settable(L, -3);
  lua_pushstring(L, "seed");
  lua_pushnumber(L, a->filter->seed);
  lua_settable(L, -3);
  lua_pushstring(L, "padding");
  lua_pushnumber(L, a->filter->padding);
  lua_settable(L, -3);
  lua_pushstring(L, "size");
  lua_pushnumber(L, a->size);
  lua_settable(L, -3);
  lua_pushstring(L, "bytes");
  lua_pushnumber(L, a->filter->allocation_in_bytes);
  lua_settable(L, -3);
  return 1;
}


static const struct luaL_Reg cuckoo_arraylib [] = {  /* metamethods for numeric userdata `n` */
  {"attrib", cuckoo_attrib},
  {"find", cuckoo_find},
  {"hash", cuckoo_hash},
  {"include", cuckoo_include},
  {"remove", cuckoo_remove},
  {"__in", mt_in},
  {"__notin", mt_notin},
  {"__empty", mt_empty},             /* metamethod for `empty` operator */
  {"__filled", mt_filled},           /* metamethod for `filled` operator */
  {"__tostring", mt_cuckoo2string},  /* for output at the console, e.g. print(n) */
  {"__size", mt_getsize},
  {"__gc", mt_cuckoogc},             /* please do not forget garbage collection */
  {NULL, NULL}
};


static const luaL_Reg cuckoolib[] = {
  {"attrib", cuckoo_attrib},
  {"find", cuckoo_find},
  {"hash", cuckoo_hash},
  {"include", cuckoo_include},
  {"new", cuckoo_new},
  {"remove", cuckoo_remove},
  {NULL, NULL}
};


/*
** Open cuckoo library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_CUCKOOLIBNAME);  /* create metatable for rbtree */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, cuckoo_arraylib);  /* methods */
}

LUALIB_API int luaopen_cuckoo (lua_State *L) {
  /* metamethods */
  createmeta(L);
  /* register library */
  luaL_register(L, AGENA_CUCKOOLIBNAME, cuckoolib);
  lua_rawsetstringstring(L, -1, "initstring", AGENA_LIBVERSION);
  return 1;
}

