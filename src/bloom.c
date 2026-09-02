/*
** $Id: bloom.c,v 0.1 15/06/2018 $
** Bloom filter
** See Copyright Notice in agena.h
*/

/*
  Implements a Bloom filter, a dictionary containing bit signatures of its individual strings (words).

  A Bloom filter is a memory-efficient mean to check whether a string _probably_ is part of a dictionary or whether it is _definitely
  not_ part of the dictionary, with acceptable query times. It consumes less memory than the original dictionary of strings and can
  be used to prevent unnecessary access to the file system on which the actual dictionary resides, for example in dBASE III+, binary
  or text files.

  With respect to this package, a dictionary does not depict an Agena table dictionary, but just a list of strings, e.g.: "Akatsuki",
  "Chandrayaan", "Chang'e", "Mars Express", "Venera", "Voyager".

  Depending on the size of the Bloom filter, the hash string function used, and the number of internal iterations - aka number of
  `salts` - when inserting or reading values, around 85 % of memory can be saved with only around 5 % of the words to be actually
  looked up in the original dictionary. Bloom filter look-up takes around a third more running time than searching Agena built-in
  data structures.

  Technically, the hash value of a string - see `hashes` package for a variety of string hash functions - is converted into a
  bit signature that is stored to slots in the Bloom filter. Internally, the Bloom filter implemented here uses four unsigned bytes
  for each slot (C type uint32_t). The string hash function used should produce the least number of collisions.

  You cannot delete values from a Bloom filter. Also, you cannot change the number of slots of the bloom filter or the number of
  salts.

  You may use the package as follows:

  1) Determine the number of entries s in your original dictionary d.

  2) Create a Bloom filter with n slots and the number of salts:

  b := bloom(s \ 4, 4);

  3) Insert all entries str of your dictionary into Bloom filter b using any string hash function, e.g.:

  for str in d do
     bloom.include(b, hashes.sdbm(str))
  od;

  4) Query the Bloom filter for any entry, using the same hash function:

  result := hashes.find(b, hashes.sdbm('Zond'));

  if result = false then
     print('entry really not included')
  else
     print('entry probably included, please search the original dictionary.')
  fi

  5) Query a Bloom filter slot, with an index counting from 1:

  b[1]:

  6) Check the state of the bloom filter:

  bloom.attrib(b): */

#define bloom_c
#define LUA_LIB

#include <stdlib.h>

#include "agena.h"

#include "agenalib.h"
#include "agnconf.h"
#include "agnxlib.h"

#define checkbloom(L, n) (Bloom *)luaL_checkudata(L, n, "bloom")

#define AGENA_LIBVERSION	"bloom 0.1.3 for Agena as of June 02, 2019\n"

typedef struct Bloom {
  size_t size;    /* number of slots */
  size_t nsalts;  /* number of hash functions to be applied (salts) */
  size_t wordsincluded;
  size_t collisions;
  uint32_t *c;    /* pointer to data */
} Bloom;


/* Creates a Bloom filter, of type userdata, consisting of `n` slots. The number of salts internally applied when inserting or searching
   the hash value of a string is given by `salts`, a positive integer in the range [1, 65]. If `salts` is 1, then no salt is applied,
   otherwise salts - 1 salts are applied.

   With a large list of surnames, for example, n should be at least a fourth of the number of words contained in the dictionary, and salts
   should be 4. */
static int bloom_new (lua_State *L) {
  Bloom *a;
  size_t n, nsalts;
  n = luaL_checkint(L, 1);
  luaL_argcheck(L, n > 0, 1, "invalid size");
  nsalts = agn_checknonnegint(L, 2);  /* 7.9.6 change */
  a = (Bloom *)lua_newuserdata(L, sizeof(Bloom));
  if (!a)  /* 7.3.4 fix */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "bloom.new");
  a->c = calloc(n, sizeof(uint32_t));
  if (!a->c) {  /* 7.3.4 fix */
    agn_poptop(L);  /* pop new userdata */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "bloom.new");
  }
  a->size = n;
  a->wordsincluded = 0;
  a->collisions = 0;
  a->nsalts = nsalts;
  lua_setmetatabletoobject(L, -1, "bloom", 1);
  /* agn_setutypestring(L, -1, "bloom"); */
  return 1;
}


/* bloom.get (a, i) = a[i]
   With a bloom filter a, returns the value stored at a[i], where i, the index, is an integer counting from 1. This is equivalent to the expression a[i]. */
static int bloom_get (lua_State *L) {  /* get a number from the given Lua/Agena index. */
  Bloom *a = checkbloom(L, 1);
  if (agn_isinteger(L, 2)) {
    int i = agn_tointeger(L, 2) - 1;  /* realign Lua/Agena index to C index, starting from 0 */
    if (i < 0 || i >= a->size)
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "bloom.get", i + 1);
    lua_pushnumber(L, a->c[i]);
    return 1;
  } else {  /* 4.4.1 call OOP method */
    return agn_initmethodcall(L, AGENA_BLOOMLIBNAME, sizeof(AGENA_BLOOMLIBNAME) - 1);
  }
}


/* ISC License

Copyright (c) 2005-2008, Simon Howard

https://github.com/fragglet/c-algorithms/blob/master/src/bloom-filter.c

Permission to use, copy, modify, and/or distribute this software
for any purpose with or without fee is hereby granted, provided
that the above copyright notice and this permission notice appear
in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

/* Salt values.  These salts are XORed with the output of the hash function to
 * give multiple unique hashes.
 *
 * These are "nothing up my sleeve" numbers: they are derived from the first
 * 256 numbers in the book "A Million Random Digits with 100,000 Normal
 * Deviates" published by the RAND corporation, ISBN 0-8330-3047-7.
 *
 * The numbers here were derived by taking each number from the book in turn,
 * then multiplying by 256 and dividing by 100,000 to give a byte range value.
 * Groups of four numbers were then combined to give 32-bit integers, most
 * significant byte first.
 */

static const unsigned int salts[] = {
  0x0,
  0x1953c322, 0x588ccf17, 0x64bf600c, 0xa6be3f3d,
  0x341a02ea, 0x15b03217, 0x3b062858, 0x5956fd06,
  0x18b5624f, 0xe3be0b46, 0x20ffcd5c, 0xa35dfd2b,
  0x1fc4a9bf, 0x57c45d5c, 0xa8661c4a, 0x4f1b74d2,
  0x5a6dde13, 0x3b18dac6, 0x05a8afbf, 0xbbda2fe2,
  0xa2520d78, 0xe7934849, 0xd541bc75, 0x09a55b57,
  0x9b345ae2, 0xfc2d26af, 0x38679cef, 0x81bd1e0d,
  0x654681ae, 0x4b3d87ad, 0xd5ff10fb, 0x23b32f67,
  0xafc7e366, 0xdd955ead, 0xe7c34b1c, 0xfeace0a6,
  0xeb16f09d, 0x3c57a72d, 0x2c8294c5, 0xba92662a,
  0xcd5b2d14, 0x743936c8, 0x2489beff, 0xc6c56e00,
  0x74a4f606, 0xb244a94a, 0x5edfc423, 0xf1901934,
  0x24af7691, 0xf6c98b25, 0xea25af46, 0x76d5f2e6,
  0x5e33cdf2, 0x445eb357, 0x88556bd2, 0x70d1da7a,
  0x54449368, 0x381020bc, 0x1c0520bf, 0xf7e44942,
  0xa27e2a58, 0x66866fc5, 0x12519ce7, 0x437a8456,
};

#define SIZESALTS      (sizeof(salts) / sizeof(*salts))
#define SIZEBLOOMFIELD (sizeof(uint32_t)*8)  /* 32 bits */
#define INDEXRSHIFT    (((SIZEBLOOMFIELD)/8 + 1))  /* 5 (sic !) */

static INLINE uint32_t aux_gethasharg (lua_State *L, int idx, const char *procname) {
  uint32_t h;
  if (agn_isstring(L, idx)) {
    size_t l;
    const char *str = lua_tolstring(L, idx, &l);
    h = tools_murmurhash3(str, l*CHARSIZE, 10301212);
  } else if (agn_isnumber(L, idx)) {
    h = lua_touint32_t(L, idx);
  } else {
    h = 0;
    luaL_error(L, "Error in " LUA_QS ": expected a number or string, got %s.", procname, luaL_typename(L, idx));
  }
  return h;
}

/* bloom.include(b, hash [, true]))
   Inserts the hash value of a string into the Bloom filter b, a userdata. By default, the function returns nothing.

   If a hash value has already been inserted, nothing happens.

   If the optional third argument is `true`, internal information is returned: the last internal subhash - an integer -
   computed before inserting the signature of the string into the Bloom filter, and a table with the keys representing
   the slot indices of the Bloom filter modified (an integer starting from 1) and the respective bit position set to 1
   (counting from 0, from the right of the bitfield).

   Example: bloom.include(b, hashes.pl('Soyuz')).

   See also: `bloom.find`. */
static int bloom_include (lua_State *L) {  /* 2.12.0 RC 4 */
  unsigned int hash, subhash, index, i, nsalts, seqsize, internal;
  /* unsigned char b, *table; */
  uint32_t b, flag;
  Bloom *a = checkbloom(L, 1);
  seqsize = a->size;
  hash = aux_gethasharg(L, 2, "bloom.include");
  internal = agnL_optboolean(L, 3, 0);
  nsalts = a->nsalts;
  subhash = -1;
  flag = 0;
  if (nsalts > SIZESALTS)
    luaL_error(L, "Error in " LUA_QS ": third argument too large, must be %d or less.",
    "bloom.include", SIZESALTS);
  if (internal)
    lua_createtable(L, 0, nsalts);
  for (i=1; i <= nsalts; i++) {
    /* generate a unique hash */
    subhash = hash ^ salts[i - 1];
    /* find the index into the table */
    index = subhash % (seqsize * SIZEBLOOMFIELD);  /* bit index */
    /* insert into the table; index / 8 finds the byte index of the table,
       index % 8 gives the bit index within that byte to set. */
    b = 1 << (index & (SIZEBLOOMFIELD - 1));  /* mask, = 1 << (index % SIZEBLOOMFIELD) */
    index >>= INDEXRSHIFT;  /* = index /= SIZEBLOOMFIELD */
    if ( (flag = a->c[index] != b ) )
      a->c[index] |= b;
    if (internal) {
      lua_pushinteger(L, index % SIZEBLOOMFIELD);  /* bit number index % SIZEBLOOMFIELD (counting from 0) */
      lua_rawseti(L, -2, index + 1);               /* has been set in slot index + 1 */
    }
  }
  if (flag)
    a->wordsincluded++;
  else
    a->collisions++;
  if (internal) {
    lua_pushnumber(L, (subhash == -1) ? AGN_NAN : subhash);
    lua_pushvalue(L, -2);
    lua_remove(L, -3);
    return 2;
  } else
    return 0;
}


/* bloom.find(b, hash)
   Checks whether a string converted to a hash value is part of a dictionary of strings represented by Bloom userdata b. The function returns
   `true` or `false`, where `false` means that the string is definitely not included in the original dictionary and `true` means it is probably
   part of the original dictionary. Example: bloom.find(b, hashes.pl('Soyuz')).

   See also: `bloom.include`. */

static int aux_find (lua_State *L, int idxbloom, int idxstring, const char *procname) {
  unsigned int hash, subhash, index, i, nsalts, seqsize;
  uint32_t b;
  int bit;
  Bloom *a = checkbloom(L, idxbloom);
  seqsize = a->size;
  hash = aux_gethasharg(L, idxstring, procname);
  nsalts = a->nsalts;
  subhash = -1;
  if (nsalts > SIZESALTS)
    luaL_error(L, "Error in " LUA_QS ": third argument too large, must be %d or less.",
    procname, SIZESALTS);
  for (i=1; i <= nsalts; i++) {
    /* generate a unique hash */
    subhash = hash ^ salts[i - 1];
    /* find the index into the table to test */
    index = subhash % (seqsize * SIZEBLOOMFIELD);
    /* the byte at index / 8 holds the value to test */
    b = a->c[index >> INDEXRSHIFT];  /* = table[index / SIZEBLOOMFIELD] */
    bit = 1 << (index & (SIZEBLOOMFIELD - 1));  /* = 1 << (index % SIZEBLOOMFIELD) */
    /* test if the particular bit is set; if it is not set, this value cannot have been inserted. */
    if ((b & bit) == 0) return 0;
  }
  /* All necessary bits were set. This may indicate that the value was inserted, or the values could
     have been set through other insertions. */
  return 1;
}

static int bloom_find (lua_State *L) {  /* 2.12.0 RC 4 */
  lua_pushboolean(L, aux_find(L, 1, 2, "bloom.find"));
  return 1;
}


static int mt_in (lua_State *L) {  /* 5.5.11 */
  lua_pushboolean(L, aux_find(L, 2, 1, "(in operator)"));
  return 1;
}


static int mt_notin (lua_State *L) {  /* 5.5.11 */
  lua_pushboolean(L, !aux_find(L, 2, 1, "(notin operator)"));
  return 1;
}


static int mt_getsize (lua_State *L) {  /* returns the number of slots in the Bloom filter */
  Bloom *a = checkbloom(L, 1);
  lua_pushinteger(L, a->wordsincluded);
  return 1;
}


static int mt_empty (lua_State *L) {  /* 5.5.11 */
  Bloom *a = checkbloom(L, 1);
  lua_pushboolean(L, a->wordsincluded == 0);
  return 1;
}


static int mt_filled (lua_State *L) {  /* 5.5.11 */
  Bloom *a = checkbloom(L, 1);
  lua_pushboolean(L, a->wordsincluded != 0);
  return 1;
}


static int mt_bloom2string (lua_State *L) {  /* at the console, the filter is formatted as follows: */
  Bloom *a;
  a = checkbloom(L, 1);
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%u, %u)", a->size, a->nsalts);
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid type.", "bloom.__tostring");
  return 1;
}


static int mt_bloomgc (lua_State *L) {  /* please do not forget to garbage collect deleted userdata */
  (void)L;
  Bloom *a = checkbloom(L, 1);
  xfree(a->c);
  /* do not free(a) ! */
  lua_setmetatabletoobject(L, 1, NULL, 1);
  return 0;
}


/* bloom.toseq (a)
   Receives a Bloom userdata structure a and converts its internal slots into a sequence of integers, the return. */
static int bloom_toseq (lua_State *L) {
  size_t i;
  Bloom *a = checkbloom(L, 1);
  agn_createseq(L, a->size);
  for (i=0; i < a->size; i++)
    agn_seqsetinumber(L, -1, i + 1, a->c[i]);
  return 1;
}


/* bloom.attrib (a)
   Returns various information on the Bloom filter:
   - key 'size': number of internal slots used by the bloom filter (first argument to `bloom.new`).
   - key 'salts': number of internal hash functions (salts) applied to a word when computing the signature (second argument to `bloom.new`).
   - key 'wordsincluded': number of words included into the filter. If the signature of a word is already included,
     it is not counted.
   - key 'collisions': number of collisions detected when trying to include a word into the filter, for its signature is already present.
     If a word has already been included in the filter, its collision is counted nevertheless.
   - key 'bytes': size of the whole Bloom filter userdata in bytes. */
static int bloom_attrib (lua_State *L) {
  Bloom *a = checkbloom(L, 1);
  lua_createtable(L, 0, 4);
  lua_pushstring(L, "wordsincluded");
  lua_pushnumber(L, a->wordsincluded);
  lua_settable(L, -3);
  lua_pushstring(L, "collisions");
  lua_pushnumber(L, a->collisions);
  lua_settable(L, -3);
  lua_pushstring(L, "size");
  lua_pushnumber(L, a->size);
  lua_settable(L, -3);
  lua_pushstring(L, "salts");
  lua_pushnumber(L, a->nsalts);
  lua_settable(L, -3);
  lua_pushstring(L, "bytes");
  lua_pushnumber(L, sizeof(Bloom) + a->size * sizeof(uint32_t));
  lua_settable(L, -3);
  return 1;
}


static const struct luaL_Reg bloom_arraylib [] = {  /* metamethods for numeric userdata `n` */
  {"attrib", bloom_attrib},
  {"find", bloom_find},
  {"get", bloom_get},
  {"include", bloom_include},
  {"toseq", bloom_toseq},
  {"__in", mt_in},
  {"__notin", mt_notin},
  {"__empty", mt_empty},            /* metamethod for `empty` operator */
  {"__filled", mt_filled},          /* metamethod for `filled` operator */
  {"__tostring", mt_bloom2string},  /* for output at the console, e.g. print(n) */
  {"__size", mt_getsize},
  {"__gc", mt_bloomgc},             /* please do not forget garbage collection */
  {NULL, NULL}
};


static const luaL_Reg bloomlib[] = {
  {"attrib", bloom_attrib},
  {"find", bloom_find},
  {"get", bloom_get},
  {"include", bloom_include},
  {"new", bloom_new},
  {"toseq", bloom_toseq},
  {NULL, NULL}
};


/*
** Open bloom library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_BLOOMLIBNAME);  /* create metatable for rbtree */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, bloom_arraylib);  /* methods */
}

LUALIB_API int luaopen_bloom (lua_State *L) {
  /* metamethods */
  createmeta(L);
  /* register library */
  luaL_register(L, AGENA_BLOOMLIBNAME, bloomlib);
  lua_rawsetstringstring(L, -1, "initstring", AGENA_LIBVERSION);
  return 1;
}

