/*
** $Id: hashes.c, initiated August 12, 2007; Alexander Walz
** String and Number Hashes
** core function taken from Kernigham/Ritchie `Programming in C`, 2nd Ed., p. 138
** See Copyright Notice in agena.h
*/

#define hashes_c
#define LUA_LIB

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "agena.h"

#include "agenalib.h"
#include "agnconf.h"
#include "agnhlps.h"  /* for lua_hashnum */
#include "agnxlib.h"
#include "md5.h"
#include "prepdefs.h"  /* FORCE_INLINE */
#include "sunpro.h"

#define AGENA_LIBVERSION  "hashes 1.2.3 for Agena as of April 09, 2023\n"

typedef uint32_t ub4;  /* unsigned 4-byte quantities, 2.10.1 patch according to German Wikipedia article on MD5 */

/* Daniel J. Bernstein hash, taken from: http://www.cse.yorku.ca/~oz/hash.html,
   collisions: 1.2707446386376. The algorithm is equivalent to:

djb := proc(s :: string, n, sh) is
   local h;
   h :=  5381;
   n :=  n or 0;
   sh := sh or 5;
   for i in s do
      h := bytes.add32(bytes.shift32(h, -sh), h, abs i)
   od;
   return if n <> 0 then h % n else h fi
end; */
static int hashes_djb (lua_State *L) {
  ub4 n, h, lsh;
  const char *s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  lsh = agnL_optuint32_t(L, 3, 5);  /* 2.12.6 extension */
  h = agnL_optuint32_t(L, 4, 5381);  /* 2.14.8 extension, 2.12.0 RC 4 patch, no UL suffix ! */
  while (*s)
    h = (h << lsh) + h + (ub4)*s++;  /* 2.12.0 RC 4 patch: cast uniformly */
  lua_pushnumber(L, (n != 0) ? h % n : h);  /* 2.11.4 change, 2.12.0 RC 4 patch: use (n != 0) instead of just (n) */
  return 1;
}


/* Modified Daniel J. Bernstein hash,
   taken from: http://www.eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx,
   collisions: 1.270045594805. The algorithm used is equivalent to:

djb2 := proc(s :: string, n, f) is
   local h;
   h :=  5381;
   f :=  f or 33;
   n :=  n or 0;
   for i in s do
      h := bytes.xor32(bytes.mul32(f, h), abs i)
   od;
   return if n <> 0 then h % n else h fi
end; */
static int hashes_djb2 (lua_State *L) {
  ub4 n, h, f;
  const char *s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);   /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  f = agnL_optuint32_t(L, 3, 33);  /* 2.12.6 extension, 33 for 11 colls, 37 for 3 colls */
  h = agnL_optuint32_t(L, 4, 5381);  /* 2.14.8 extension, 2.12.0 RC 4 patch, no UL suffix ! */
  while (*s)
    h = f * h ^ (ub4)*s++;  /* = (f*h) ^ *s, 2.12.0 RC 4 patch: cast uniformly */
  lua_pushnumber(L, (n != 0) ? h % n : h);  /* 2.11.4 change, 2.12.0 RC 4 patch: use (n != 0) instead of just (n) */
  return 1;
}


/* Fowler/Noll/Vo hash, taken from: http://www.eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx,
   collisions: 1.2710700375654. This is equivalent to:

fnv := proc(s :: string, n) is
   local h;
   h := 2166136261;
   n := n or 0;
   for i in s do
      h := bytes.xor32(bytes.mul32(h, 16777619), abs i)
   od;
   return if n <> 0 then h % n else h fi
end; */
static int hashes_fnv (lua_State *L) {
  ub4 n, h;
  const char *s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  h = 2166136261UL;
  while (*s)
    h = (h * 16777619) ^ (ub4)*s++;  /* 2.12.0 RC 4 patch: cast uniformly */
  lua_pushnumber(L, (n != 0) ? h % n : h);  /* 2.11.4 change */
  return 1;
}


/* Bob Jenkins' hash (Robert Jenkins' 96 bit Mix Function), taken from:
   http://www.eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx,
   collisions: 1.2701495898179 */
/* taken from Lua 5.2.4, modified, 5.0.2 */
#define ALLONES    (~(((~(LUA_UINT32)0) << (LUA_NBITS - 1)) << 1U))
/* macro to trim extra bits */
#define trim(x)    ((x) & ALLONES)

#define mix(a,b,c) { \
  a -= b; a -= c; a ^= ( c >> 13UL ); \
  b -= c; b -= a; b ^= ( a << 8UL  ); \
  c -= a; c -= b; c ^= ( b >> 13UL ); \
  a -= b; a -= c; a ^= ( c >> 12UL ); \
  b -= c; b -= a; b ^= ( a << 16UL ); \
  c -= a; c -= b; c ^= ( b >> 5UL  ); \
  a -= b; a -= c; a ^= ( c >> 3UL  ); \
  b -= c; b -= a; b ^= ( a << 10UL ); \
  c -= a; c -= b; c ^= ( b >> 15UL ); \
}

static int hashes_jen (lua_State *L) {
  size_t l;
  ub4 a, b, c, len, length, n;
  const char *s = agn_checklstring(L, 1, &l);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  c = 16777551UL;  /* 2.27.0 change */
  len = length = l;
  a = b = 0x9e3779b9UL;
  while (len >= 12) {
    a += ((ub4)s[0] + ((ub4)s[1] << 8UL)  /* 2.27.0 fix */
      + ((ub4)s[2] << 16UL)
      + ((ub4)s[3] << 24UL));
    b += ((ub4)s[4] + ((ub4)s[5] << 8UL)  /* 2.27.0 fix */
      + ((ub4)s[6] << 16UL)
      + ((ub4)s[7] << 24UL));
    c += ((ub4)s[8] + ((ub4)s[9] << 8UL)  /* 2.27.0 fix */
      + ((ub4)s[10] << 16UL)
      + ((ub4)s[11] << 24UL));
    mix(a, b, c);
    s += 12UL;
    len -= 12UL;
  }
  c += length;
  switch (len) {
    case 11: c += ((ub4)s[10] << 24UL);
    case 10: c += ((ub4)s[9] << 16UL);
    case 9 : c += ((ub4)s[8] << 8UL);
    /* First byte of c reserved for length */
    case 8 : b += ((ub4)s[7] << 24UL);
    case 7 : b += ((ub4)s[6] << 16UL);
    case 6 : b += ((ub4)s[5] << 8UL);
    case 5 : b += (ub4)s[4];  /* 2.27.0 fix */
    case 4 : a += ((ub4)s[3] << 24UL);
    case 3 : a += ((ub4)s[2] << 16UL);
    case 2 : a += ((ub4)s[1] << 8UL);
    case 1 : a += (ub4)s[0];  /* 2.27.0 fix */
  }
  mix(a, b, c);
  lua_pushnumber(L, (n != 0) ? c % n : c);  /* 2.11.4 change, 2.12.0 RC 4 patch: use (n != 0) instead of just (n) */
  return 1;
}


/* One-at-a-Time Hash hash, taken from the website `A Hash Function for Hash Table Lookup`,
   http://www.burtleburtle.net/bob/hash/doobs.html */
static int hashes_oaat (lua_State *L) {
  ub4 n, h;
  const char *s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  h = agnL_optuint32_t(L, 3, 0);  /* 2.14.8 extension */
  while (*s) {
    h += (ub4)*s++;   /* 2.12.0 RC 4 patch: cast uniformly */
    h += (h << 10U);  /* 5.0.2 changed ... << 10UL to changed 10U, and the following */
    h ^= (h >> 6U);
  }
  h += (h << 3U);
  h ^= (h >> 11U);
  h += (h << 15U);
  lua_pushnumber(L, (n != 0) ? h & trim(n) : h);  /* 2.11.4 change, 2.12.0 RC 4 patch: use (n != 0) instead of just (n), changed 5.0.2 */
  return 1;
}


/* Like `hashes.oaat`, but uses bit rotation internally, instead of simple bit shifts. 2.27.0 */
static int hashes_roaat (lua_State *L) {
  ub4 n, h;
  const char *s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);
  h = agnL_optuint32_t(L, 3, 0);
  while (*s) {
    h += (ub4)*s++;  /* 5.0.2 changed ... << 10UL to changed 10U, and the following */
    h += tools_rotl32(h, 10U);  /* (h << 10) */
    h ^= tools_rotr32(h, 6U);   /* (h >> 6) */
  }
  h += tools_rotl32(h, 3U);     /* (h << 3) */
  h ^= tools_rotr32(h, 11U);    /* (h >> 11) */
  h += tools_rotl32(h, 15U);    /* (h << 15) */
  lua_pushnumber(L, (n != 0) ? h & trim(n) : h);  /* changed 5.0.2 */
  return 1;
}


/* taken from: http://stackoverflow.com/questions/628790/have-a-good-hash-function-for-a-c-hash-table,
   invented by from Paul Larson of Microsoft Research,
   collisions: 1.2681150349105
   With initial h=5381 and f=33, emulates `GNU hash`. */
static int hashes_pl (lua_State *L) {
  ub4 n, h, f;
  const char *s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);    /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  f = agnL_optuint32_t(L, 3, 101);  /* 2.12.6 extension, 379 & 694 are even better with German word list (2 colls each)  */
  h = agnL_optuint32_t(L, 4, 0);    /* 2.14.8 extension */
  while (*s)
    h = h * f + (ub4)*s++;  /* 2.12.0 RC 4 patch: cast uniformly */
  lua_pushnumber(L, (n != 0) ? h % n : h);  /* 2.11.4 change, 2.12.0 RC 4 patch: use (n != 0) instead of just (n) */
  return 1;
}


static int hashes_raw (lua_State *L) {
  ub4 n, h;
  const char *s = luaL_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  h = agnL_optuint32_t(L, 3, 0);  /* 2.14.8 extension */
  while (*s)
    h = 38*(h << 1UL) + (ub4)*s++ - 63;  /* 2.12.0 RC 4 patch: cast uniformly */
  lua_pushnumber(L, (n != 0) ? h % n : h);  /* 2.11.4 change, 2.12.0 RC 4 patch: use (n != 0) instead of just (n) */
  return 1;
}


/* Shift-Add-XOR hash (also called: `JS hash`),
   taken from: http://www.eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx,
   collisions: 1.2696167953224 */
static int hashes_sax (lua_State *L) {
  ub4 n, h;
  const char *s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  h = agnL_optuint32_t(L, 3, 5381);  /* 2.14.8 extension, 2.12.0 RC 4 patch, no UL suffix ! */
  while (*s)
    h ^= (h << 5UL) + (h >> 2UL) + (ub4)*s++;  /* 2.12.0 RC 4 patch: cast uniformly */
  lua_pushnumber(L, (n != 0) ? h % n : h);  /* 2.11.4 change, 2.12.0 RC 4 patch: use (n != 0) instead of just (n) */
  return 1;
}


/* public-domain re-implementation of ndbm database library hash, see: http://www.cse.yorku.ca/~oz/hash.html */
static int hashes_sdbm (lua_State *L) {
  ub4 n, h;
  const char *s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  h = agnL_optuint32_t(L, 3, 0);  /* 2.14.8 extension */
  while (*s)
    h = (ub4)*s++ + (h << 6UL) + (h << 16UL) - h;  /* 2.12.0 RC 4 patch: cast uniformly */
  lua_pushnumber(L, (n != 0) ? h % n : h);  /* 2.11.4 change, 2.12.0 RC 4 patch: use (n != 0) instead of just (n) */
  return 1;
}


/* written by sth,
   taken from http://stackoverflow.com/questions/628790/have-a-good-hash-function-for-a-c-hash-table,
   collisions: 1.2811778916885 */
static int hashes_sth (lua_State *L) {
  ub4 n, h;
  const char *s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  h = agnL_optuint32_t(L, 3, 0);  /* 2.14.8 extension */
  while (*s)
    h = (h << 6UL) ^ (h >> 26UL) ^ (ub4)*s++;  /* 2.12.0 RC 4 patch: cast uniformly */
  lua_pushnumber(L, (n != 0) ? h % n : h);  /* 2.11.4 change, 2.12.0 RC 4 patch: use (n != 0) instead of just (n) */
  return 1;
}


/* byte to hexadecimal conversion taken from Answer #9, written by kriss,
   at http://stackoverflow.com/questions/6357031/how-do-you-convert-buffer-byte-array-to-hex-string-in-c. */
static int hashes_md5 (lua_State *L) {
  unsigned char *pout, pin, str[32 + 1];  /* target buffer str should be large enough */
  const char *hex;
  char *message;
  unsigned char buffer[64];
  size_t len, i;
  MD5_CTX mdContext;
  for (i=0; i < 64*CHARSIZE; i++) buffer[i] = 0;  /* make sure buffer is zeroed for correct results ! */
  hex = "0123456789ABCDEF";
  message = (char *)luaL_checklstring(L, 1, &len);
  if (lua_gettop(L) > 1 && !agn_isnumber(L, 2)) {  /* file input, 2.11.3 */
    /* based on example available at http://www.efgh.com/software/md5.htm:
    "Although MD5Digest() can handle blocks of any size (even zero), it operates internally on blocks of size 64,
    so it is most efficient if given blocks that are exact multiples of this size." */
    FILE *fp;
    fp = fopen(message, "rb");  /* generally open in binary mode to read complete files */
    if (fp == NULL)
      luaL_error(L, "Error in " LUA_QS ": file could not be opened.", "hashes.md5");
    MD5Init(&mdContext);
    while ((len = fread(buffer, 1, sizeof(buffer), fp)) > 0)
      MD5Update(&mdContext, buffer, len);
    MD5Final(&mdContext);
    if (fclose(fp) != 0)
      luaL_error(L, "Error in " LUA_QS ": file could not be closed.", "hashes.md5");
  } else {  /* 3.9.4 fix */
    int bytes = agnL_optnonnegint(L, 2, 64);
    if (bytes == 0) bytes = len;
    MD5Init(&mdContext);
    /* results have been validated, among other engines, with https://onlinehashtools.com/calculate-md5-hash */
    while (len > 0) {
      if (bytes > len) bytes = len;
      memcpy(buffer, (char *)message, bytes);
      MD5Update(&mdContext, buffer, bytes);
      len -= bytes;
      message += bytes;
    }
    MD5Final(&mdContext);
  }
  /* convert to hexadecimal string */
  pout = str;
  for (i=0; i < 16; i++) {
    pin = mdContext.digest[i];
    *pout++ = hex[(pin >> 4) & 0xF];
    *pout++ = hex[(pin) & 0xF];
  }
  *pout = 0;
  lua_pushstring(L, (const char *)str);
  return 1;
}


#if BYTE_ORDER == BIG_ENDIAN
typedef union {
  double f;
  uint64_t i;
  struct {
    uint32_t msw;
    uint32_t lsw;
  } parts;
} ieee_double_shape_type;
#else
typedef union {
  double f;
  uint64_t i;
  struct {
    uint32_t lsw;
    uint32_t msw;
  } parts;
} double_32_64;
#endif


/* 64 bit Mix Function, by Thomas Wang, taken from gist.github/badboy/6267743, 2.10.0 */
static FORCE_INLINE uint64_t uint64hash (uint64_t key) {
  key = (~key) + (key << 21ULL);  /* key = (key << 21) - key - 1; */
  key =   key  ^ (key >> 24ULL);
  key = (key + (key << 3ULL)) + (key << 8ULL);  /* key * 265 */
  key =  key ^ (key >> 14ULL);
  key = (key + (key << 2ULL)) + (key << 4ULL);  /* key * 21 */
  key =  key ^ (key >> 28ULL);
  key =  key + (key << 31ULL);
  return key;
}

static int hashes_mix64 (lua_State *L) {
  uint64_t n;
  double_cast x;  /* 2.16.3 change */
  x.f = agn_checknumber(L, 1);
  n = agnL_optnonnegint(L, 2, 0);  /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  lua_pushinteger(L, (n != 0) ? uint64hash(x.i) % n : uint64hash(x.i));  /* 2.12.0 RC 4 patch: use (n != 0) instead of just (n) */
  return 1;
}


/* Squirrel Eiserloh's hash function, unsigned 64-bit version, 2.38.2, based on MIT-licenced:
   https://github.com/heckj/Squirrel3/blob/main/Sources/CSquirrel/squirrel.c */
static FORCE_INLINE uint64_t uint64squirrel3 (uint64_t key, uint64_t seed) {
  const uint64_t BIT_NOISE1 = 0xB5297A4DB5297A4DULL;  /* a prime, 2.39.1 OS/2 ULL const fix */
  const uint64_t BIT_NOISE2 = 0x68E31DA468E31DA4ULL;  /* a prime, ditto */
  const uint64_t BIT_NOISE3 = 0x1B56C4E91B56C4E9ULL;  /* a prime, ditto */
  key *= BIT_NOISE1;
  key += seed;
  key ^= (key >> 8);
  key += BIT_NOISE2;
  key ^= (key << 8);
  key *= BIT_NOISE3;
  key ^= (key >> 8);
  return key;
}

/* Similar to `bytes.numwords`, but applying Squirrel Eiserloh's hash function on its first argument key,
   any Agena number, and returning the higher and lower parts of the result as two unsigned 4-byte integers.
   You may optionally add a seed as a second argument, a non-negative integer, with 0 the default. */
static int hashes_squirrel64 (lua_State *L) {
  uint64_t seed;
  double_32_64 x;
  x.f = agn_checknumber(L, 1);
  seed = agnL_optnonnegint(L, 2, 0);
  x.i = uint64squirrel3(x.i, seed);
  luaL_checkstack(L, 2, "not enough stack space");  /* 7.1.9 fix */
  lua_pushnumber(L, x.parts.msw);
  lua_pushnumber(L, x.parts.lsw);
  return 2;
}


/* Taken from https://gist.github.com/serialhex/ddbaf64fa9942a6c41b29618974f5b3a, unknown licence as
   there is no information on the site. */
static FORCE_INLINE uint32_t uint32squirrel3 (int key, uint32_t seed) {
  const uint32_t BIT_NOISE1 = 0xb5297a4d;
  const uint32_t BIT_NOISE2 = 0x68e31da4;
  const uint32_t BIT_NOISE3 = 0x1b56c4e9;
  uint32_t mangled = key;
  mangled *= BIT_NOISE1;
  mangled += seed;
  mangled ^= (mangled >> 8);
  mangled += BIT_NOISE2;
  mangled ^= (mangled << 8);
  mangled *= BIT_NOISE3;
  mangled ^= (mangled >> 8);
  return mangled;
}

/* Takes any signed integer key and applies Squirrel Eiserloh's hash function on it. The return is a
   signed 4-byte integer. You may optionally add a seed as a second argument, a non-negative integer,
   with 0 the default. 2.38.2 */
static int hashes_squirrel32 (lua_State *L) {
  int key;
  uint32_t seed;
  key = agn_checkinteger(L, 1);
  seed = agnL_optnonnegint(L, 2, 0);
  lua_pushnumber(L, uint32squirrel3(key, seed));
  return 1;
}


/* 64 bit to 32 bit Mix Function, by Thomas Wang, taken from gist.github/badboy/6267743, 2.10.0 */
static FORCE_INLINE uint32_t uint64to32hash (uint64_t key) {
  key = (~key) + (key << 18ULL);  /* key = (key << 18) - key - 1; */
  key =   key  ^ (key >> 31ULL);
  key = key * 21ULL;  /* key = (key + (key << 2)) + (key << 4); */
  key = key ^ (key >> 11ULL);
  key = key + (key << 6ULL);
  key = key ^ (key >> 22ULL);
  return (uint32_t)key;
}

static FORCE_INLINE int hashes_mix64to32 (lua_State *L) {
  uint64_t n;
  double_cast x;  /* 2.16.3 change */
  x.f = agn_checknumber(L, 1);
  n = agnL_optnonnegint(L, 2, 0);  /* 2.10.0 fix, 2.11.4 change, 2.12.6 change */
  lua_pushinteger(L, (n != 0) ? uint64to32hash(x.i) % n : uint64to32hash(x.i));  /* 2.12.0 RC 4 patch: use (n != 0) instead of just (n) */
  return 1;
}


/* luhn.c - Luhn-10 algorithm, written by Bruce D. Lightner, modified to prevent compiler warnings,
   taken from: https://github.com/bdlightner/luhn-implementation-C-/blob/master/luhn.c */
static FORCE_INLINE int luhnchecknumber (char *number) {
  int i, sum, ch, num, twoup, len;
  len = tools_strlen(number);  /* 2.17.8 tweak */
  sum = 0;
  twoup = 0;
  for (i=len - 1; i >= 0; --i) {
    ch = number[i];
    num = (ch >= '0' && ch <= '9') ? ch - '0' : 0;
    if (twoup) {
      num += num;
      if (num > 9) num = (num % 10) + 1;
    }
    sum += num;
    twoup++;  /* to avoid compiler warnings */
    twoup = twoup & 1;
  }
  sum = 10 - (sum % 10);
  if (sum == 10) sum = 0;
  return (sum == 0) ? 1 : 0;
}

static FORCE_INLINE int luhncalcdigit (char *number) {
  int i, sum, ch, num, twoup, len;
  len = tools_strlen(number);  /* 2.17.8 tweak */
  sum = 0;
  twoup = 1;
  for (i=len - 1; i >= 0; --i) {
    ch = number[i];
    num = (ch >= '0' && ch <= '9') ? ch - '0' : 0;
    if (twoup) {
      num += num;
      if (num > 9) num = (num % 10) + 1;
    }
    sum += num;
    twoup++;  /* to avoid compiler warnings */
    twoup = twoup & 1;
  }
  sum = 10 - (sum % 10);
  if (sum == 10) sum = 0;
  return sum;
}

/* If passed no option, computes the checksum of its argument (an integer or string consisting of ciphers), and returns an integer
   in the range 0 .. 9 using the Luhn formula, which is used to validate credit card numbers, IMEIs or some social security numbers.

   If passed the Boolean option `true`, the function checks whether the `number` (an integer or string consisting of ciphers) includes
   the correct checksum digit at its end.

   If you pass an integer x and if |x| > math.lastcontint, then an error is issued, for x cannot be represented accurately on your system.
   Pass a string instead.

   The Luhn formula does not recognise the transposition 09 vs. 90, nor does it detect twin 22 vs. 55, 33 vs. 66, and 44 vs. 77.

   See also: hashess.damm, hashes.verhoeff. */
static int hashes_luhn (lua_State *L) {  /* 2.10.0 */
  int option;
  const char *n;
  option = agnL_optboolean(L, 2, 0);
  n = NULL;  /* just to prevent compiler warnings */
  /* DO NOT TOUCH: the following if/then/else is 5 percent faster than optimised code. */
  if (agn_isnumber(L, 1)) {
    if (fabs(agn_checkinteger(L, 1)) > AGN_LASTCONTINT)  /* integer cannot be represented with correct accuracy */
      luaL_error(L, "Error in " LUA_QS ": absolute number too large, pass a string instead.", "hashes.luhn");
    n = lua_tostring(L, 1);
  } else if (agn_isstring(L, 1)) {
    n = agn_tostring(L, 1);
  } else
    luaL_error(L, "Error in " LUA_QS ": number or string expected, got %s.",
      "hashes.luhn", luaL_typename(L, 1));
  if (!option)
    lua_pushinteger(L, luhncalcdigit((char *)n));
  else
    lua_pushboolean(L, luhnchecknumber((char *)n));
  return 1;
}

/* Verhoeff algorithm as in wikipedia (http://en.wikipedia.org/wiki/Verhoeff_algorithm),
   C version taken from: https://sites.google.com/site/abapexamples/c/verhoeff-algorithm
   Licence: `Welcome, this site is just a way, to allow me save my own work. If you came
   here, you welcome to use my code, but without warranty. Enjoy! ` */
static const int d[10][10] = {
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },
  { 1, 2, 3, 4, 0, 6, 7, 8, 9, 5 },
  { 2, 3, 4, 0, 1, 7, 8, 9, 5, 6 },
  { 3, 4, 0, 1, 2, 8, 9, 5, 6, 7 },
  { 4, 0, 1, 2, 3, 9, 5, 6, 7, 8 },
  { 5, 9, 8, 7, 6, 0, 4, 3, 2, 1 },
  { 6, 5, 9, 8, 7, 1, 0, 4, 3, 2 },
  { 7, 6, 5, 9, 8, 2, 1, 0, 4, 3 },
  { 8, 7, 6, 5, 9, 3, 2, 1, 0, 4 },
  { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }
};

static const int  p[8][10] = {
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },
  { 1, 5, 7, 6, 2, 8, 3, 0, 9, 4 },
  { 5, 8, 0, 3, 7, 9, 6, 1, 4, 2 },
  { 8, 9, 1, 6, 0, 4, 3, 5, 2, 7 },
  { 9, 4, 5, 3, 1, 2, 6, 8, 7, 0 },
  { 4, 2, 8, 6, 5, 7, 3, 9, 0, 1 },
  { 2, 7, 9, 3, 8, 0, 6, 4, 1, 5 },
  { 7, 0, 4, 6, 9, 1, 3, 2, 5, 8 }
};

static const int j[10] = { 0, 4, 3, 2, 1, 5, 6, 7, 8, 9 };

/* If passed no option, computes the checksum of its argument (an integer or string consisting of ciphers), and returns an integer
   in the range 0 .. 9 using the Verhoeff algorithm. Contrary to the Luhn algorithm, it detects all single-digit errors,
   and all accidental transposition involving two adjacent ciphers.

   If passed the Boolean option `true`, the function checks whether the `number` (an integer or string consisting of ciphers) includes
   the correct checksum digit at its end.

   If you pass an integer x and if |x| > math.lastcontint, then an error is issued, for x cannot be represented accurately on your system.
   Pass a string instead. The function also returns an error, if a non-digit is included in string x.

   See also: hashess.damm, hashes.luhn. */
static int hashes_verhoeff (lua_State *L) {  /* 2.10.0 */
  int c, i, option;
  size_t len;
  const char *str;
  option = agnL_optboolean(L, 2, 0);
  str = NULL;
  /* DO NOT TOUCH: the following if/then/else is 5 percent faster than optimised code. */
  if (agn_isnumber(L, 1)) {
    if (fabs(agn_checkinteger(L, 1)) > AGN_LASTCONTINT)
      /* integer cannot be represented with correct accuracy */
      luaL_error(L, "Error in " LUA_QS ": absolute number too large, pass a string instead.",
        "hashes.verhoeff");
    str = lua_tostring(L, 1);
  } else if (agn_isstring(L, 1)) {
    str = agn_tostring(L, 1);
  } else
    luaL_error(L, "Error in " LUA_QS ": number or string expected, got %s.",
      "hashes.verhoeff", luaL_typename(L, 1));
  c = i = 0;
  len = tools_strlen(str);  /* 2.17.8 tweak */
  while (len--)
    c = d[c][p[++i % 8][str[len] - '0']];  /* we start with 1 (++i), cause the check digit is the 0. */
  if (!option)
    lua_pushinteger(L, j[c]);
  else
    lua_pushboolean(L, c == 0);
  return 1;
}


/* taken from: https://en.wikibooks.org/wiki/Algorithm_Implementation/Checksums/Damm_Algorithm */
static const char dammTable[] =
  "0317598642" "7092154863" "4206871359" "1750983426" "6123045978"
  "3674209581" "5869720134" "8945362017" "9438617205" "2581436790";

static FORCE_INLINE char dammLookup (char* number) {
  char interim, *p;
  interim = '0';
  for (p=number; *p != '\0'; ++p) {
    if ((unsigned char)(*p - '0') > 9)
      interim = dammTable[((char)tools_reducerange(*p, '0', '9') - '0') + (interim - '0')*10];
    /* return '-';  // minus sign indicates an error: character is not a digit */
    interim = dammTable[(*p - '0') + (interim - '0')*10];
  }
  return interim;
}

/* If passed no option, computes the checksum of its argument (an integer or string consisting of ciphers), and returns an integer
   in the range 0 .. 9 using the Damm algorithm. Contrary to the Luhn algorithm, it detects all single-digit errors and all
   adjacent transposition errors.

   If passed the Boolean option `true`, the function checks whether the `number` (an integer or string consisting of ciphers) includes
   the correct checksum digit at its end.

   If you pass an integer x and if |x| > math.lastcontint, then an error is issued, for x cannot be represented accurately on your system.
   Pass a string instead.

   See also: hashess.luhn, hashes.verhoeff. */
static int hashes_damm (lua_State *L) {  /* 2.10.0 */
  int option;
  const char *n;
  option = agnL_optboolean(L, 2, 0);
  n = NULL;
  /* DO NOT TOUCH: the following if/then/else is 5 percent faster than optimised code. */
  if (agn_isnumber(L, 1)) {
    if (fabs(agn_checkinteger(L, 1)) > AGN_LASTCONTINT)
    /* integer cannot be represented with correct accuracy. */
      luaL_error(L, "Error in " LUA_QS ": absolute number too large, pass a string instead.", "hashes.damm");
    n = lua_tostring(L, 1);  /* 5 percent faster tham optimising */
  } else if (agn_isstring(L, 1)) {
    n = agn_tostring(L, 1);  /* 5 percent faster tham optimising */
  } else
    luaL_error(L, "Error in " LUA_QS ": number or string expected, got %s.",
      "hashes.damm", luaL_typename(L, 1));
  if (!option)
    lua_pushnumber(L, dammLookup((char *)n) - '0');
  else
    lua_pushboolean(L, dammLookup((char *)n) == '0');
  return 1;
}


/* Returns the same checksum as the UNIX cksum utility for the given string. The return is a
   non-negative integer. The function can be used to validate the integrity of a file but
   may not always detect hacker manipulation.

   Taken from: http://pubs.opengroup.org/onlinepubs/9699919799/utilities/cksum.html */
static int hashes_cksum (lua_State *L) {  /* 2.10.0 */
  size_t l;
  int chars;
  const char *str;
  str = agn_checklstring(L, 1, &l);
  chars = agnL_optposint(L, 2, l);
  if (chars < l) l = chars;
  /* do not use lua_pushinteger as it would return negative and positive values */
  lua_pushnumber(L, tools_cksum((const unsigned char *)str, l));
  return 1;
}

/* efficient 8-bit implementation of Fletcher's checksum using a 16-bit accumulator (only the first 20 chars are processed),
   see: https://en.wikipedia.org/wiki/Fletcher's_checksum */
static FORCE_INLINE uint16_t fletcher16 (const char *data, size_t bytes) {
  uint16_t sum1 = 0xff, sum2 = 0xff;
  size_t tlen;
  while (bytes) {
    tlen = ((bytes >= 20) ? 20 : bytes);
    bytes -= tlen;
    do {
      sum2 += sum1 += *data++;
      tlen--;
    } while (tlen);
    sum1 = (sum1 & 0xff) + (sum1 >> 8);
    sum2 = (sum2 & 0xff) + (sum2 >> 8);
  }
  /* Second reduction step to reduce sums to 8 bits */
  sum1 = (sum1 & 0xff) + (sum1 >> 8);
  sum2 = (sum2 & 0xff) + (sum2 >> 8);
  return (sum2 << 8) | sum1;
}

/* efficient 16-bit implementation of Fletcher's checksum using a 32-bit accumulator (only the first 359 chars are processed),
   see: https://en.wikipedia.org/wiki/Fletcher's_checksum */
static FORCE_INLINE uint32_t fletcher32 (const char *data, size_t bytes) {
  uint32_t sum1 = 0xffff, sum2 = 0xffff;
  size_t tlen;
  while (bytes) {
    tlen = ((bytes >= 359) ? 359 : bytes);
    bytes -= tlen;
    do {
      sum2 += sum1 += *data++;
      tlen--;
    } while (tlen);
    sum1 = (sum1 & 0xffff) + (sum1 >> 16UL);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16UL);
  }
  /* Second reduction step to reduce sums to 16 bits */
  sum1 = (sum1 & 0xffff) + (sum1 >> 16UL);
  sum2 = (sum2 & 0xffff) + (sum2 >> 16UL);
  return (sum2 << 16UL) | sum1;
}

/* If `mode` is not given or is `true`, returns the position-dependent 16-bit checksum of a string `str` according to
   Fletcher's algorithm using an internal 32-bit accumulator, and returns an integer in the range [x, y]. The 360th
   and all succeeding characters are ignored.

   If `mode` is `false`, returns the position-dependent 8-bit checksum using an internal 16-bit accumulator, and
   returns an integer in the range [257, 65535]. The 21st and all succeeding characters are ignored.

   If the option `len` is given, only the first len characters are processed. */
static int hashes_fletcher (lua_State *L) {  /* 2.10.0 */
  int mode, chars;
  size_t l;
  const char *data;
  data = agn_checklstring(L, 1, &l);
  mode = agnL_optboolean(L, 2, 1);
  chars = agnL_optposint(L, 3, l);
  if (chars < l) l = chars;
  /* do not use lua_pushinteger as it would return negative and positive values */
  lua_pushnumber(L, mode ? fletcher32(data, l) : fletcher16(data, l));
  return 1;
}


/* Returns the legacy BSD checksum for a string s and returns an integer in the range [0, 255]. The BSD checksum algorithm
   computes a checksum by segmenting the data and adding it to an accumulator that is circular right shifted between each
   summation. Patched 2.12.0 RC 4, return type formerly was uchar; extension to process embedded zeros 2.38.3 */
static uint32_t bsdchecksum (const char *data, size_t l, int mode) {
  if (mode) {
    uint16_t checksum = 0;
    while (l--) {  /* original BSD 16-bit checksum */
      /* if (checksum & 1) checksum |= 0x10000; */ /* rotate the accumulator */
      /* checksum = ((checksum >> 1) + (ub4)*data++) & 0xffff; */ /* add next chunk */
      /* See: https://en.wikipedia.org/wiki/BSD_checksum, 2.38.3 */
      checksum = (checksum >> 1) + ((checksum & 1) << 15);
      checksum += (uint16_t)*data++;
      checksum &= 0xffff;  /* keep it within bounds */
    }
    return (uint32_t)checksum;
  } else {
    /* 8-bit checksum (see: https://en.wikipedia.org/wiki/BSD_checksum), patched 2.12.0 RC 4 (uchar instead of char) */
    uint8_t checksum = 0;
    while (l--) {
      checksum = (((checksum & 0xff) >> 1)) + ((checksum & 0x1) << 7);  /* rotate the accumulator */
      checksum = (checksum + (uint8_t)*data++) & 0xff;  /* add next chunk */
    }
    return (uint32_t)checksum;
  }
}

static int hashes_bsd (lua_State *L) {  /* 2.10.0 */
  int option;
  size_t l;
  const char *str;
  str = agn_checklstring(L, 1, &l);
  option = agnL_optboolean(L, 2, 1);  /* default: use original 16-bit checksum */
  lua_pushnumber(L, bsdchecksum(str, l, option));  /* in the range [0, 255] */
  return 1;
}


/* The function mixes three non-negative 32-bit integers a, b, c, and returns an integer. */
static int hashes_mix (lua_State *L) {  /* 2.10.0 */
  ub4 a, b, c;
  a = agnL_optuint32_t(L, 1, 0);  /* changed 4.12.5 */
  b = agnL_optuint32_t(L, 2, 0);  /* changed 4.12.5 */
  c = agnL_optuint32_t(L, 3, 0);  /* changed 4.12.5 */
  mix(a, b, c);
  lua_pushnumber(L, c);
  return 1;
}


/* Performs 32-bit reversed cyclic redundancy check for string str, starting with initial CRC value n,
   which is 0 by default. The return is a nonnegative integer in the range 0 .. 2^32-1. */
static int hashes_crc32 (lua_State *L) {
  ub4 crc;
  size_t len;
  const char *buf = agn_checklstring(L, 1, &len);
  crc = agnL_optuint32_t(L, 2, 0);
  lua_pushnumber(L, tools_crc32(buf, len, crc));
  return 1;
}


/* Performs 8-bit reversed cyclic redundancy check for string str, starting with initial CRC value n,
   which is 0 by default. */
static int hashes_crc8 (lua_State *L) {  /* 2.16.4 */
  uint32_t crc;
  size_t len, i, j;
  const char *buf;
  buf = agn_checklstring(L, 1, &len);
  crc = agnL_optuint32_t(L, 2, 0) << 8;  /* 4.12.5 change */
  for (i=len; i; i--, buf++) {
    crc ^= (*buf << 8UL);
    for (j=8; j; j--) {
      if (crc & 0x8000)
        crc ^= (0x1070 << 3UL);
      crc <<= 1;
    }
  }
  lua_pushnumber(L, (uint8_t)(crc >> 8UL));
  return 1;
}


/* Returns the digital root and the additive persistence for the integer x and base b. By default b is 10.

   The digital root is the sum of its digits and the sum of the digits of this sum, and so forth, until the
   respective sum is less than b. The additive persistence is the number of summations it took to compute
   the single digit.

   See: https://rosettacode.org/wiki/Digital_root */
static FORCE_INLINE int rosetta_droot (int64_t x, int base, int *pers) {
  int d = 0;
  if (pers) {
    for (*pers=0; x >= base; x = d, (*pers)++) {
      for (d=0; x; d += x % base, x /= base);
    }
  } else if (x && !(d = x % (base - 1)))
    d = base - 1;
  return d;
}

static int hashes_droot (lua_State *L) {
  int d, base, p;
  int64_t x;
  x = agn_checknumber(L, 1);
  base = luaL_optint(L, 2, 10);
  if (base < 1)
    luaL_error(L, "Error in " LUA_QS ": base must be positive.", "hashes.droot");
  d = rosetta_droot(x, base, &p);
  luaL_checkstack(L, 2, "not enough stack space");  /* 7.1.9 fix */
  lua_pushinteger(L, d);
  lua_pushinteger(L, p);
  return 2;
}


/*********************************************************************
 *
 * Function:    reflect()
 *
 * Description: Reorder the bits of a binary sequence, by reflecting
 *        them about the middle position.
 *
 * Notes:    No checking is done that nBits <= 32.
 *
 * Returns:    The reflection of the original data.
 *
 * Copyright (c) 2000 by Michael Barr.  This software is placed into
 * the public domain and may be used for any purpose.  However, this
 * notice must not be changed or removed and no warranty is either
 * expressed or implied by its publication or distribution.
 * https://github.com/ARMmbed/DAPLink/blob/master/source/daplink/crc16.c
 *********************************************************************/
static FORCE_INLINE unsigned long aux_reflect (unsigned long data, unsigned char nBits) {
  unsigned long reflection = 0x00000000;
  unsigned char bit;
  /* Reflect the data about the center bit. */
  for (bit=0; bit < nBits; ++bit) {
    if (data & 0x01) {  /* If the least significant bit (LSB) bit is set, set the reflection of it. */
      reflection |= (1 << ((nBits - 1) - bit));
    }
    data = (data >> 1);
  }
  return reflection;
}

/* Source: Henry S. Warren, http://www.hackersdelight.org/hdcodetxt/reverse.c.txt
   You are free to use, copy, and distribute any of the code on this web site, whether modified by you or not.
   You need not give attribution. This includes the algorithms (some of which appear in Hacker's Delight),
   the Hacker's Assistant, and any code submitted by readers. Submitters implicitly agree to this. */
static FORCE_INLINE unsigned int aux_reverse32 (register unsigned int x) {
  x = ((x & 0xaaaaaaaa) >> 1)  | ((x & 0x55555555) << 1);
  x = ((x & 0xcccccccc) >> 2)  | ((x & 0x33333333) << 2);
  x = ((x & 0xf0f0f0f0) >> 4)  | ((x & 0x0f0f0f0f) << 4);
  x = ((x & 0xFF00FF00) >> 8)  | ((x & 0x00FF00FF) << 8);
  x = ((x & 0xFFFF0000) >> 16) | ((x & 0x0000FFFF) << 16);
  return x;
}

static FORCE_INLINE unsigned int aux_reverse8 (register unsigned int x) {
  x = ((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1);
  x = ((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2);
  x = ((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4);
  return x;
}

/* Reorders the bits of the n-bit integer x by reflecting them about the middle position. By default, n is 32,
   but may be any other integer in [1, 32]. The return is an integer. */
static int hashes_reflect (lua_State *L) {
  int bits;
  uint32_t x = agnL_optuint32_t(L, 1, 0);  /* 4.12.5 change */
  bits = luaL_optint(L, 2, 32);
  if (bits < 1 || bits > 32)
    luaL_error(L, "Error in " LUA_QS ": bits must be in [1, 32].", "hashes.reflect");
  switch (bits) {
    case  8: lua_pushnumber(L, aux_reverse8(x)); break;
    case 32: lua_pushnumber(L, aux_reverse32(x)); break;
    default: lua_pushnumber(L, aux_reflect(x, bits));
  }
  return 1;
}


/* CRC16 */
/* https://c64preservation.com/svn/nibtools/trunk/crc.h
 * Select the CRC standard from the list that follows.
 */
#define CRC16

#if defined(CRC_CCITT)

typedef unsigned short  crc;

#define CRC_NAME      "CRC-CCITT"
#define POLYNOMIAL      0x1021
#define INITIAL_REMAINDER  0xFFFF
#define FINAL_XOR_VALUE    0x0000
#define REFLECT_DATA    FALSE
#define REFLECT_REMAINDER  FALSE
#define CHECK_VALUE      0x29B1

#elif defined(CRC16)

typedef unsigned short crc;

#define CRC_NAME      "CRC-16"
#define POLYNOMIAL      0x8005
#define INITIAL_REMAINDER  0x0000
#define FINAL_XOR_VALUE    0x0000
#define REFLECT_DATA    TRUE
#define REFLECT_REMAINDER  TRUE
#define CHECK_VALUE      0xBB3D

#elif defined(CRC32)

typedef unsigned int crc;

#define CRC_NAME      "CRC-32"
#define POLYNOMIAL      0x04C11DB7
#define INITIAL_REMAINDER  0xFFFFFFFF
#define FINAL_XOR_VALUE    0xFFFFFFFF
#define REFLECT_DATA    TRUE
#define REFLECT_REMAINDER  TRUE
#define CHECK_VALUE      0xCBF43926

#else

#error "One of CRC_CCITT, CRC16, or CRC32 must be #define'd."

#endif

/*
 * Derive parameters from the standard-specific parameters in crc.h.
 * https://c64preservation.com/svn/nibtools/trunk/crc.c
 */
#define WIDTH    (8 * sizeof(crc))
#define TOPBIT   (1 << (WIDTH - 1))

#if (REFLECT_DATA == TRUE)
#undef  REFLECT_DATA
#define REFLECT_DATA(X)      ((unsigned char) aux_reflect((X), 8))
#else
#undef  REFLECT_DATA
#define REFLECT_DATA(X)      (X)
#endif

#if (REFLECT_REMAINDER == TRUE)
#undef  REFLECT_REMAINDER
#define REFLECT_REMAINDER(X)  ((crc) aux_reflect((X), WIDTH))
#else
#undef  REFLECT_REMAINDER
#define REFLECT_REMAINDER(X)  (X)
#endif

crc  crcTable[256];

/*********************************************************************
 *
 * Function:    crcInit()
 *
 * Description: Populate the partial CRC lookup table.
 *
 * Notes:    This function must be rerun any time the CRC standard
 *        is changed.  If desired, it can be run "offline" and
 *        the table results stored in an embedded system's ROM.
 *
 * Returns:    None defined.
 *
 *********************************************************************/
void aux_crcInit (void) {
  crc remainder;
  int dividend;
  unsigned char bit;
  /* Compute the remainder of each possible dividend. */
  for (dividend=0; dividend < 256; ++dividend) {
    /* Start with the dividend followed by zeros. */
    remainder = dividend << (WIDTH - 8);
    /* Perform modulo-2 division, a bit at a time. */
    for (bit=8; bit > 0; --bit) {
      /* Try to divide the current data bit. */
      if (remainder & TOPBIT) {
        remainder = (remainder << 1) ^ POLYNOMIAL;
      } else {
        remainder = (remainder << 1);
      }
    }
    /* Store the result into the table. */
    crcTable[dividend] = remainder;
  }
}


/*********************************************************************
 *
 * Function:    crcFast()
 *
 * Description: Compute the CRC of a given message.
 *
 * Notes:    crcInit() must be called first.
 *
 * Returns:    The CRC of the message.
 *
 *********************************************************************/
crc aux_crcFast (unsigned char const message[], int nBytes, crc remainder) {
  /* crc remainder = INITIAL_REMAINDER; */
  crc data;
  int byte;
  /* Divide the message by the polynomial, a byte at a time. */
  for (byte=0; byte < nBytes; ++byte) {
    data = REFLECT_DATA(message[byte]) ^ (remainder >> (WIDTH - 8));
    remainder = crcTable[data] ^ (remainder << 8);
  }
  /* The final remainder is the CRC. */
  return (REFLECT_REMAINDER(remainder) ^ FINAL_XOR_VALUE);
}

/* Returns the CRC16 value of a string with an initial CRC value which is 0 by default. */
static int hashes_crc16 (lua_State *L) {
  unsigned const char *str;
  size_t l;
  crc init;
  str = (unsigned const char*)agn_checklstring(L, 1, &l);
  init = agnL_optinteger(L, 2, 0);
  lua_pushinteger(L, aux_crcFast(str, l*CHARSIZE, init));
  return 1;
}


/* Returns a byte with even parity for the non-negative integer x, and returns
   an integer in the range [0, 255].
   Source: Henry S. Warren, http://hackersdelight.org/hdcodetxt/parity.c.txt
   You are free to use, copy, and distribute any of the code on this web site, whether modified by you or not.
   You need not give attribution. This includes the algorithms (some of which appear in Hacker's Delight),
   the Hacker's Assistant, and any code submitted by readers. Submitters implicitly agree to this. */
static int hashes_parity (lua_State *L) {
  uint32_t x, y;
  x = agnL_optuint32_t(L, 1, 0);  /* 4.12.5 change */
  y = (x * 0x10204081) & 0x888888FF;
  lua_pushnumber(L, (y % 1920) & 0xff);  /* return a byte with even parity */
  return 1;
}


/* Returns the hash, an integer, Lua/Agena internally computes for strings. Adaption of the Shift-Add-XOR hash. This variant
   chooses the length of the string as its seed, not a fixed value and scans from the right to the left. See also: hashes.sax. */
static int hashes_lua (lua_State *L) {  /* 2.11.4 */
  const char *str;
  size_t l, step, l1;
  unsigned int h;
  ub4 n;
  str = agn_checklstring(L, 1, &l);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.12.6 change */
  h = cast(unsigned int, l);  /* seed */
  step = (l >> 5) + 1;        /* if string is too long, don't hash all its chars */
  for (l1=l; l1 >= step; l1 -= step) {  /* compute hash */
    h = h ^ ((h << 5) + (h >> 2) + cast(unsigned char, str[l1 - 1]));
  }
  lua_pushnumber(L, (n != 0) ? h % n : h);  /* 2.12.0 RC 4 patch: use (n != 0) instead of just (n) */
  return 1;
}


/* Returns the integral hash for any number, used internally by Lua to refer to numeric keys in the hash parts of tables. The
   return is an unsigned 4-byte integer. Equals
      import bytes;
      hx, lx := bytes.numworda(n);
      return (hx + lx) % ((total - 1) || 1)

import hashes
for i from -10 to 10 by 0.5 do print(hashes.numlua(i)) od

   Compared to the other numeric hashing functions, `hashes.numlua` is best with integral numbers, with only half the collisions
   than with fractional numbers.

   Adapted from ltable.c/luaH_hashnum; 2.37.3 */

/* Hash for doubles proposed by Gemini AI in May 2026, put to the public domain */
#define hashnumints   cast_int(sizeof(lua_Number)/sizeof(int))

static unsigned int hashmurmurgemai (lua_Number n) {
  unsigned int a[hashnumints], h;
  int i;
  n += 0.0;  /* normalize number (avoid -0) */
  tools_memcpy(a, &n, sizeof(a));
  /* Initial fold */
  h = a[0];
  for (i=1; i < hashnumints; i++) h += a[i];
  /* MurmurHash3 32-bit mixer for numbers */
  h ^= h >> 16;
  h *= 0x85ebca6bU;
  h ^= h >> 13;
  h *= 0xc2b2ae35U;
  h ^= h >> 16;
  return h;
}

/* Hash for lua_Numbers in Luau 0.719, MIT License, taken from:
   https://github.com/luau-lang/luau/blob/master/VM/src/ltable.cpp */
static uint32_t hashmurmurluau (lua_Number n) {
  unsigned int i[2];
  n += 0.0;  /* normalize number (avoid -0) */
  memcpy(i, &n, sizeof(i));
  /* mask out sign bit to make sure -0 and 0 hash to the same value */
  uint32_t h1 = i[0];
  uint32_t h2 = i[1] & 0x7fffffff;
  /* finalizer from MurmurHash64B */
  const uint32_t m = 0x5bd1e995;
  h1 ^= h2 >> 18;
  h1 *= m;
  h2 ^= h1 >> 22;
  h2 *= m;
  h1 ^= h2 >> 17;
  h1 *= m;
  h2 ^= h1 >> 19;
  h2 *= m;
  return h2;
}

static int hashes_numlua (lua_State *L) {
  ub4 total;
  lua_Number n = agn_checknumber(L, 1);
  int nargs = lua_gettop(L);
  if (nargs > 1 && (lua_istrue(L, nargs) ||
      (agn_isstring(L, nargs) && tools_streq(agn_tostring(L, nargs), "Lua")) ) ) {  /* return hash used by Lua 5.1 for numbers */
    /* -1 here actually means 4294967295, that is (uint_32t)(-1). This is safe. */
    total = ((nargs == 2) ? (ub4)(-1) : agn_checkuint32_t(L, 2)) | 1;
    lua_pushnumber(L, lua_hashnum(n) % total);
    return 1;
  } else if (nargs > 1 && agn_isstring(L, nargs) && tools_streq(agn_tostring(L, nargs), "Luau")) {  /* 7.5.5 */
    total = ((nargs == 2) ? (ub4)(-1) : agn_checkuint32_t(L, 2)) | 1;
    lua_pushnumber(L, hashmurmurluau(n) % total);
  } else if (nargs > 1 && agn_isstring(L, nargs) && tools_streq(agn_tostring(L, nargs), "GemAI")) {  /* 7.5.5 */
    total = ((nargs == 2) ? (ub4)(-1) : agn_checkuint32_t(L, 2)) | 1;
    lua_pushnumber(L, hashmurmurgemai(n) % total);
  }
  luaL_checkstack(L, 2, "not enough stack space");  /* 7.1.9 fix */
  n += 0.0;  /* normalize number (avoid -0) */
  /* Don't know how the formula evolved, it has not been taken from Lua 5.1 and later */
  ub4 hx, lx;
  const ub4 mask = 0x5bd1e995;
  EXTRACT_WORDS(hx, lx, n);
  /* -1 | 1 = 4294967295 = 2^32 - 1 = UINT_MAX.
     Warning: this always ORs 1 to the modulus given by the user. */
  total = (agnL_optuint32_t(L, 2, 0) - 1) | 1;
  lua_pushnumber(L, (hx + lx) % total);
  lua_pushnumber(L, ((hx ^ mask) + (lx ^ mask)) % total);
  return 2;
}


/* Lua 5.5.0 hash for floating-point numbers, taken from Lua 5.5.0 source file ltable.c; 6.5.10
   The main computation should be just
      n = frexp(n, &i); return (n * INT_MAX) + i
   but there are some numerical subtleties.
   In a two-complement representation, INT_MAX may not have an exact representation as a float, but INT_MIN does;
   because the absolute value of 'frexp' is smaller than 1 (unless 'n' is inf/NaN), the absolute value of the
   product 'frexp * -INT_MIN' is smaller or equal to INT_MAX. Next, the use of 'unsigned int' avoids overflows
   when adding 'i'; the use of '~u' (instead of '-u') avoids problems with INT_MIN. */

static int hashes_hashfloat (lua_State *L) {
  int i;
  ub4 r, total;
  lua_Integer ni;
  lua_Number n = agn_checknumber(L, 1);
  total = (agnL_optuint32_t(L, 2, 0) - 1) | 1;  /* will always be non-zero */
  n = sun_frexp(n, &i) * -cast_num(INT_MIN);
  if (!lua_numbertointeger(n, &ni)) {  /* is 'n' inf/-inf/NaN? */
    r = 0;
  } else {  /* normal case */
    ub4 u = cast_uint(i) + cast_uint(ni);
    r = (u <= cast_uint(INT_MAX)) ? u : ~u;  /* u will be no less than 0 and no more than 2,147,483,647 */
  }
  lua_pushnumber(L, r % total);
  return 1;
}


/*-----------------------------------------------------------------------------
   MurmurHash1Aligned, by Austin Appleby

   Same algorithm as MurmurHash1, but with aligned reads if applicable - should
   be safer on certain platforms. Performance should be equal to or better than
   the simple version. 6.4.5 */
unsigned int murmurhash1 (const void * key, int len, unsigned int seed) {
  const unsigned int m = 0xc6a4a793;
  const int r = 16;
  const unsigned char *data = (const unsigned char *)key;
  unsigned int h = seed^(len*m);
  int align = (int)((uintptr_t)data & 3);
  if (align && (len >= 4)) {  /* pre-load the temp registers */
    unsigned int t = 0, d = 0;
    switch (align) {
      case 1: t |= data[2] << 16;
      case 2: t |= data[1] << 8;
      case 3: t |= data[0];
    }
    t <<= (8*align);
    data += 4-align;
    len -= 4-align;
    int sl = 8*(4 - align);
    int sr = 8*align;
    /* mix */
    while (len >= 4) {
      d = *(unsigned int *)data;
      t = (t >> sr) | (d << sl);
      h += t;
      h *= m;
      h ^= h >> r;
      t = d;
      data += 4;
      len -= 4;
    }
    /* handle leftover data in temp registers */
    int pack = len < align ? len : align;
    d = 0;
    switch (pack) {
      case 3: d |= data[2] << 16;
      case 2: d |= data[1] << 8;
      case 1: d |= data[0];
      case 0:
        h += (t >> sr) | (d << sl);
        h *= m;
        h ^= h >> r;
    }
    data += pack;
    len -= pack;
  } else {  /* data is not aligned */
    while (len >= 4) {
      h += *(unsigned int *)data;
      h *= m;
      h ^= h >> r;
      data += 4;
      len -= 4;
    }
  }
  /* Handle tail bytes */
  switch (len) {
    case 3: h += data[2] << 16;
    case 2: h += data[1] << 8;
    case 1: h += data[0];
      h *= m;
      h ^= h >> r;
  };
  h *= m;
  h ^= h >> 10;
  h *= m;
  h ^= h >> 17;
  return h;
}

/* Returns MurmurHash2, an integer, for the given string s. If n, a positive integer, is given,
   the computed hash is taken modulo n. Note that the function returns different values on little-
   endian and big-endian machines. */
static int hashes_murmur1 (lua_State *L) {  /* 6.4.5 */
  const char *str;
  size_t l;
  ub4 h, n, seed;
  str = agn_checklstring(L, 1, &l);
  n = agnL_optuint32_t(L, 2, 0);
  seed = agnL_optuint32_t(L, 3, 0x9747b28c);
  h = murmurhash1(str, l, seed);
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}

/* MurmurHash2, by Austin Appleby

   Note - This code makes a few assumptions about how your machine behaves -

   1. We can read a 4-byte value from any address without crashing
   2. sizeof(int) == 4

   And it has a few limitations -

   1. It will not work incrementally.
   2. It will not produce the same results on little-endian and big-endian machines.

Source: https://github.com/jvirkki/libbloom/blob/master/murmur2/MurmurHash2.c */

unsigned int murmurhash2 (const void *key, int len, const unsigned int seed) {
  /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well. */
  const unsigned char *data;
  const ub4 m = 0x5bd1e995;
  const int r = 24;
  /* initialise the hash to a 'random' value */
  ub4 h = seed ^ len;
  /* mix 4 bytes at a time into the hash */
  data = (const unsigned char *)key;
  while (len >= 4) {
    ub4 k = *(ub4 *)data;
    k *= m;
    k ^= k >> r;
    k *= m;
    h *= m;
    h ^= k;
    data += 4;
    len -= 4;
  }
  /* handle the last few bytes of the input array */
  switch (len) {
  case 3: h ^= data[2] << 16UL;
  case 2: h ^= data[1] << 8UL;
  case 1: h ^= data[0];
          h *= m;
  };
  /* do a few final mixes of the hash to ensure the last few bytes are well-incorporated. */
  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;
  return h;
}

/* Returns MurmurHash2, an integer, for the given string s. If n, a positive integer, is given,
   the computed hash is taken modulo n. Note that the function returns different values on little-
   endian and big-endian machines. */
static int hashes_murmur2 (lua_State *L) {  /* 2.12.0 RC 4 */
  const char *str;
  size_t l;
  ub4 h, n, seed;
  str = agn_checklstring(L, 1, &l);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.12.6 change */
  seed = agnL_optuint32_t(L, 3, 0x9747b28c);  /* 6.4.5 change */
  h = murmurhash2(str, l, seed);
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* https://github.com/PeterScott/murmur3/blob/master/murmur3.c, public domain */

/*-----------------------------------------------------------------------------
  MurmurHash3 was written by Austin Appleby, and is placed in the public
  domain. The author hereby disclaims copyright to this source code.

  Note - The x86 and x64 versions do _not_ produce the same results, as the
  algorithms are optimized for their respective platforms. You can still
  compile and run any of them on any platform, but your performance with the
  non-native version will be less than optimal. */

static FORCE_INLINE uint32_t rotl32 (uint32_t x, int8_t r) {
  return (x << r) | (x >> (32 - r));
}

static FORCE_INLINE uint64_t rotl64 (uint64_t x, int8_t r) {
  return (x << r) | (x >> (64 - r));
}

static FORCE_INLINE uint32_t rotr32 (uint32_t x, int8_t r) {  /* 2.16.8 */
  return (x >> r) | (x << (32 - r));
}

static FORCE_INLINE uint64_t rotr64 (uint64_t x, int8_t r) {  /* 2.16.8 */
  return (x >> r) | (x << (64 - r));
}

#define ROTL32(x,y)  rotl32(x,y)
#define ROTL64(x,y)  rotl64(x,y)

#define BIG_CONSTANT(x) (x##LLU)

/* Block read - if your platform needs to do endian-swapping or can only handle aligned reads, do the conversion here */

#define getblock(p, i) (p[i])

/* Finalisation mix - force all bits of a hash block to avalanche */

static FORCE_INLINE uint32_t fmix32 (uint32_t h) {
  h ^= h >> 16UL;
  h *= 0x85ebca6b;
  h ^= h >> 13UL;
  h *= 0xc2b2ae35;
  h ^= h >> 16UL;
  return h;
}

static FORCE_INLINE uint64_t fmix64 (uint64_t k) {
  k ^= k >> 33ULL;
  k *= BIG_CONSTANT(0xff51afd7ed558ccd);
  k ^= k >> 33ULL;
  k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
  k ^= k >> 33ULL;
  return k;
}

void MurmurHash3_x86_32 (const void *key, int len, uint32_t seed, void *out) {
  const uint8_t *data = (const uint8_t*)key;
  const int nblocks = div4(len);  /* 2.17.8 tweak */
  int i;
  uint32_t h1 = seed;
  uint32_t c1 = 0xcc9e2d51;
  uint32_t c2 = 0x1b873593;
  /* body */
  const uint32_t *blocks = (const uint32_t *)(data + mul4(nblocks));  /* 2.17.8 tweak */
  for (i = -nblocks; i; i++) {
    uint32_t k1 = getblock(blocks, i);
    k1 *= c1;
    k1 = ROTL32(k1, 15);
    k1 *= c2;
    h1 ^= k1;
    h1 = ROTL32(h1, 13);
    h1 = h1*5 + 0xe6546b64;
  }
  /* tail */
  const uint8_t * tail = (const uint8_t*)(data + mul4(nblocks));  /* 2.17.8 tweak */
  uint32_t k1 = 0;
  switch(len & 3) {
    case 3: k1 ^= tail[2] << 16;
    case 2: k1 ^= tail[1] << 8;
    case 1: k1 ^= tail[0];
            k1 *= c1; k1 = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
  };
  /* finalisation */
  h1 ^= len;
  h1 = fmix32(h1);
  *(uint32_t*)out = h1;
}

/* Computes MurmurHash3 using 32-bit unsigned integers internally, for the given string s and returns an integer.
   If n, a positive integer, is given, the computed hash is taken modulo n. Note that the function returns different
   values on little-endian and big-endian machines. */
static int hashes_murmur3 (lua_State *L) {  /* 2.12.0 RC 4 */
  const char *str;
  size_t l;
  ub4 h, n, seed;
  str = agn_checklstring(L, 1, &l);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.12.6 change */
  seed = agnL_optuint32_t(L, 3, 0x9747b28c);  /* 6.4.5 change */
  MurmurHash3_x86_32(str, l, seed, &h);
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


static FORCE_INLINE uint32_t getblock32 (const uint32_t * p, int i) {  /* new 6.4.5 */
  return p[i];
}

void MurmurHash3_x86_128 (const void * key, const int len, uint32_t seed, void * out) {
  int i;
  const uint8_t * data = (const uint8_t*)key;
  const int nblocks = len / 16;
  uint32_t h1 = seed;
  uint32_t h2 = seed;
  uint32_t h3 = seed;
  uint32_t h4 = seed;
  const uint32_t c1 = 0x239b961b;
  const uint32_t c2 = 0xab0e9789;
  const uint32_t c3 = 0x38b34ae5;
  const uint32_t c4 = 0xa1e38b93;
  /* body */
  const uint32_t * blocks = (const uint32_t *)(data + nblocks*16);
  for (i=-nblocks; i; i++) {
    uint32_t k1 = getblock32(blocks,i*4+0);
    uint32_t k2 = getblock32(blocks,i*4+1);
    uint32_t k3 = getblock32(blocks,i*4+2);
    uint32_t k4 = getblock32(blocks,i*4+3);
    k1 *= c1; k1  = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
    h1 = ROTL32(h1,19); h1 += h2; h1 = h1*5+0x561ccd1b;
    k2 *= c2; k2  = ROTL32(k2,16); k2 *= c3; h2 ^= k2;
    h2 = ROTL32(h2,17); h2 += h3; h2 = h2*5+0x0bcaa747;
    k3 *= c3; k3  = ROTL32(k3,17); k3 *= c4; h3 ^= k3;
    h3 = ROTL32(h3,15); h3 += h4; h3 = h3*5+0x96cd1c35;
    k4 *= c4; k4  = ROTL32(k4,18); k4 *= c1; h4 ^= k4;
    h4 = ROTL32(h4,13); h4 += h1; h4 = h4*5+0x32ac3b17;
  }
  /* tail */
  const uint8_t * tail = (const uint8_t*)(data + nblocks*16);
  uint32_t k1 = 0;
  uint32_t k2 = 0;
  uint32_t k3 = 0;
  uint32_t k4 = 0;
  switch (len & 15) {
    case 15: k4 ^= tail[14] << 16;
    case 14: k4 ^= tail[13] << 8;
    case 13: k4 ^= tail[12] << 0;
             k4 *= c4; k4  = ROTL32(k4,18); k4 *= c1; h4 ^= k4;
    case 12: k3 ^= tail[11] << 24;
    case 11: k3 ^= tail[10] << 16;
    case 10: k3 ^= tail[ 9] << 8;
    case  9: k3 ^= tail[ 8] << 0;
             k3 *= c3; k3  = ROTL32(k3,17); k3 *= c4; h3 ^= k3;
    case  8: k2 ^= tail[ 7] << 24;
    case  7: k2 ^= tail[ 6] << 16;
    case  6: k2 ^= tail[ 5] << 8;
    case  5: k2 ^= tail[ 4] << 0;
             k2 *= c2; k2  = ROTL32(k2,16); k2 *= c3; h2 ^= k2;
    case  4: k1 ^= tail[ 3] << 24;
    case  3: k1 ^= tail[ 2] << 16;
    case  2: k1 ^= tail[ 1] << 8;
    case  1: k1 ^= tail[ 0] << 0;
             k1 *= c1; k1  = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
  };
  /* finalization */
  h1 ^= len; h2 ^= len; h3 ^= len; h4 ^= len;
  h1 += h2; h1 += h3; h1 += h4;
  h2 += h1; h3 += h1; h4 += h1;
  h1 = fmix32(h1);
  h2 = fmix32(h2);
  h3 = fmix32(h3);
  h4 = fmix32(h4);
  h1 += h2; h1 += h3; h1 += h4;
  h2 += h1; h3 += h1; h4 += h1;
  ((uint32_t*)out)[0] = h1;
  ((uint32_t*)out)[1] = h2;
  ((uint32_t*)out)[2] = h3;
  ((uint32_t*)out)[3] = h4;
}

/* Computes MurmurHash3 using 128-bit unsigned integers internally, for the given string s and returns an integer.
   If n, a positive integer, is given, the computed hash is taken modulo n. The third optional argument depicts the
   seed, which is 0x9747b28c by default. 6.4.5 */
static int hashes_murmur3128x86 (lua_State *L) {
  const char *str;
  size_t l;
  ub4 i, n, seed;
  uint32_t h[4];
  str = agn_checklstring(L, 1, &l);
  n = agnL_optuint32_t(L, 2, 0);
  seed = agnL_optuint32_t(L, 3, 0x9747b28c);
  MurmurHash3_x86_128(str, l, seed, h);
  luaL_checkstack(L, 4, "not enough stack space");
  for (i=0; i < 4; i++)
    lua_pushnumber(L, (n != 0) ? h[i] % n : h[i]);  /* 2.14.10 change: return four unsigned 32-bit integers of just one sliced number */
  return 4;
}


void MurmurHash3_x64_128 (const void * key, const int len, const uint32_t seed, void * out) {  /* out must be an array of 4 uint32_t's */
  const uint8_t *data = (const uint8_t*)key;
  const int nblocks = len / 16;
  int i;
  uint64_t h1 = seed;
  uint64_t h2 = seed;
  uint64_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
  uint64_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);
  /* body */
  const uint64_t *blocks = (const uint64_t *)(data);
  for (i=0; i < nblocks; i++) {
    uint64_t k1 = getblock(blocks, mul2(i) + 0);  /* 2.17.8 tweak */
    uint64_t k2 = getblock(blocks, mul2(i) + 1);  /* 2.17.8 tweak */
    k1 *= c1; k1  = ROTL64(k1, 31); k1 *= c2; h1 ^= k1;
    h1 = ROTL64(h1, 27); h1 += h2; h1 = h1*5 + 0x52dce729;
    k2 *= c2; k2  = ROTL64(k2, 33); k2 *= c1; h2 ^= k2;
    h2 = ROTL64(h2,31); h2 += h1; h2 = h2*5 + 0x38495ab5;
  }
  /* tail */
  const uint8_t *tail = (const uint8_t*)(data + nblocks*16);
  uint64_t k1 = 0;
  uint64_t k2 = 0;
  switch(len & 15) {
    case 15: k2 ^= (uint64_t)(tail[14]) << 48;
    case 14: k2 ^= (uint64_t)(tail[13]) << 40;
    case 13: k2 ^= (uint64_t)(tail[12]) << 32;
    case 12: k2 ^= (uint64_t)(tail[11]) << 24;
    case 11: k2 ^= (uint64_t)(tail[10]) << 16;
    case 10: k2 ^= (uint64_t)(tail[ 9]) << 8;
    case  9: k2 ^= (uint64_t)(tail[ 8]) << 0;
             k2 *= c2; k2  = ROTL64(k2, 33); k2 *= c1; h2 ^= k2;
    case  8: k1 ^= (uint64_t)(tail[ 7]) << 56;
    case  7: k1 ^= (uint64_t)(tail[ 6]) << 48;
    case  6: k1 ^= (uint64_t)(tail[ 5]) << 40;
    case  5: k1 ^= (uint64_t)(tail[ 4]) << 32;
    case  4: k1 ^= (uint64_t)(tail[ 3]) << 24;
    case  3: k1 ^= (uint64_t)(tail[ 2]) << 16;
    case  2: k1 ^= (uint64_t)(tail[ 1]) << 8;
    case  1: k1 ^= (uint64_t)(tail[ 0]) << 0;
             k1 *= c1; k1  = ROTL64(k1, 31); k1 *= c2; h1 ^= k1;
  };
  /* finalisation */
  h1 ^= len; h2 ^= len;
  h1 += h2;
  h2 += h1;
  h1 = fmix64(h1);
  h2 = fmix64(h2);
  h1 += h2;
  h2 += h1;
  ((uint64_t*)out)[0] = h1;
  ((uint64_t*)out)[1] = h2;
}


/* Computes MurmurHash3 using 128-bit unsigned integers internally, for the given string s and returns an integer.
   If n, a positive integer, is given, the computed hash is taken modulo n. The third optional argument depicts the
   seed, which is 0x9747b28c by default. */
static int hashes_murmur3128 (lua_State *L) {  /* 2.12.0 RC 4 */
  const char *str;
  size_t l;
  ub4 i, n, seed;
  uint32_t h[4];  /* 2.14.10 fix; crashes otherwise */
  str = agn_checklstring(L, 1, &l);
  n = agnL_optuint32_t(L, 2, 0);  /* 2.12.6 change */
  seed = agnL_optuint32_t(L, 3, 0x9747b28c);  /* 2.14.10 change */
  MurmurHash3_x64_128(str, l, seed, h);
  luaL_checkstack(L, 4, "not enough stack space");  /* 4.7.1 fix */
  for (i=0; i < 4; i++)
    lua_pushnumber(L, (n != 0) ? h[i] % n : h[i]);  /* 2.14.10 change: return four unsigned 32-bit integers of just one sliced number */
  return 4;
}


/* Taken from musl-1.2.4 clib, MIT licence, https://www.musl-libc.org/src/crypt/crypt_sha256.c */

/*
 * public domain sha256 crypt implementation
 *
 * original sha crypt design: http://people.redhat.com/drepper/SHA-crypt.txt
 * in this implementation at least 32bit int is assumed,
 * key length is limited, the $5$ prefix is mandatory, '\n' and ':' is rejected
 * in the salt and rounds= setting must contain a valid iteration count,
 * on error "*" is returned.
 */

/* public domain sha256 implementation based on fips180-3 */

struct sha256 {
	uint64_t len;    /* processed message length */
	uint32_t h[8];   /* hash state */
	uint8_t buf[64]; /* message block buffer */
};

static uint32_t ror32 (uint32_t n, int k) { return (n >> k) | (n << (32-k)); }
#define Ch(x,y,z)  (z ^ (x & (y ^ z)))
#define Maj(x,y,z) ((x & y) | (z & (x | y)))
#define S0(x)      (ror32(x,2) ^ ror32(x,13) ^ ror32(x,22))
#define S1(x)      (ror32(x,6) ^ ror32(x,11) ^ ror32(x,25))
#define R0(x)      (ror32(x,7) ^ ror32(x,18) ^ (x>>3))
#define R1(x)      (ror32(x,17) ^ ror32(x,19) ^ (x>>10))

static const uint32_t KK[64] = {
0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void processblock256 (struct sha256 *s, const uint8_t *buf) {
	uint32_t W[64], t1, t2, a, b, c, d, e, f, g, h;
	int i;
	for (i = 0; i < 16; i++) {
		W[i] = (uint32_t)buf[4*i]<<24;
		W[i] |= (uint32_t)buf[4*i+1]<<16;
		W[i] |= (uint32_t)buf[4*i+2]<<8;
		W[i] |= buf[4*i+3];
	}
	for (; i < 64; i++)
		W[i] = R1(W[i-2]) + W[i-7] + R0(W[i-15]) + W[i-16];
	a = s->h[0];
	b = s->h[1];
	c = s->h[2];
	d = s->h[3];
	e = s->h[4];
	f = s->h[5];
	g = s->h[6];
	h = s->h[7];
	for (i = 0; i < 64; i++) {
		t1 = h + S1(e) + Ch(e,f,g) + KK[i] + W[i];
		t2 = S0(a) + Maj(a,b,c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}
	s->h[0] += a;
	s->h[1] += b;
	s->h[2] += c;
	s->h[3] += d;
	s->h[4] += e;
	s->h[5] += f;
	s->h[6] += g;
	s->h[7] += h;
}

static void pad256 (struct sha256 *s) {
	unsigned r = s->len % 64;
	s->buf[r++] = 0x80;
	if (r > 56) {
		tools_bzero(s->buf + r, 64 - r);
		r = 0;
		processblock256(s, s->buf);
	}
	tools_bzero(s->buf + r, 56 - r);
	s->len *= 8;
	s->buf[56] = s->len >> 56;
	s->buf[57] = s->len >> 48;
	s->buf[58] = s->len >> 40;
	s->buf[59] = s->len >> 32;
	s->buf[60] = s->len >> 24;
	s->buf[61] = s->len >> 16;
	s->buf[62] = s->len >> 8;
	s->buf[63] = s->len;
	processblock256(s, s->buf);
}

static FORCE_INLINE void sha256_init (struct sha256 *s) {
	s->len = 0;
	s->h[0] = 0x6a09e667;
	s->h[1] = 0xbb67ae85;
	s->h[2] = 0x3c6ef372;
	s->h[3] = 0xa54ff53a;
	s->h[4] = 0x510e527f;
	s->h[5] = 0x9b05688c;
	s->h[6] = 0x1f83d9ab;
	s->h[7] = 0x5be0cd19;
}

static FORCE_INLINE void sha256_sum (struct sha256 *s, uint8_t *md) {
	int i;
	pad256(s);
	for (i = 0; i < 8; i++) {
		md[4*i] = s->h[i] >> 24;
		md[4*i+1] = s->h[i] >> 16;
		md[4*i+2] = s->h[i] >> 8;
		md[4*i+3] = s->h[i];
	}
}

static void sha256_update (struct sha256 *s, const void *m, unsigned long len) {
	const uint8_t *p = m;
	unsigned r = s->len % 64;
	s->len += len;
	if (r) {
		if (len < 64 - r) {
			memcpy(s->buf + r, p, len);
			return;
		}
		memcpy(s->buf + r, p, 64 - r);
		len -= 64 - r;
		p += 64 - r;
		processblock256(s, s->buf);
	}
	for (; len >= 64; len -= 64, p += 64)
		processblock256(s, p);
	memcpy(s->buf, p, len);
}

static char *to64 (char *s, unsigned int u, int n) {
  while (--n >= 0) {
    *s++ = tools_b64[u % 64];
    u /= 64;
  }
  return s;
}

/* key limit is not part of the original design, added for DoS protection.
 * rounds limit has been lowered (versus the reference/spec), also for DoS
 * protection. runtime is O(klen^2 + klen*rounds) */

#define KEY_MAX 256
#define SALT_MAX 16
#define ROUNDS_DEFAULT 5000
#define ROUNDS_MIN 1000
#define ROUNDS_MAX 9999999

/* hash n bytes of the repeated md message digest */
static FORCE_INLINE void hashmd256 (struct sha256 *s, unsigned int n, const void *md) {
	unsigned int i;
	for (i = n; i > 32; i -= 32)
		sha256_update(s, md, 32);
	sha256_update(s, md, i);
}

static char *sha256crypt (const char *key, const char *setting, char *output) {
	struct sha256 ctx;
	unsigned char md[32], kmd[32], smd[32];
	unsigned int i, r, klen, slen;
	char rounds[20] = "";
	const char *salt;
	char *p;
	/* reject large keys */
	klen = tools_strnlen(key, KEY_MAX+1);
	if (klen > KEY_MAX)
		return 0;
	/* setting: $5$rounds=n$salt$ (rounds=n$ and closing $ are optional) */
	if (strncmp(setting, "$5$", 3) != 0)
		return 0;
	salt = setting + 3;
	r = ROUNDS_DEFAULT;
	if (strncmp(salt, "rounds=", sizeof "rounds=" - 1) == 0) {
		unsigned long u;
		char *end;
		/*
		 * this is a deviation from the reference:
		 * bad rounds setting is rejected if it is
		 * - empty
		 * - unterminated (missing '$')
		 * - begins with anything but a decimal digit
		 * the reference implementation treats these bad
		 * rounds as part of the salt or parse them with
		 * strtoul semantics which may cause problems
		 * including non-portable hashes that depend on
		 * the host's value of ULONG_MAX.
		 */
		salt += sizeof "rounds=" - 1;
		if (!isdigit(*salt))
			return 0;
		u = strtoul(salt, &end, 10);
		if (*end != '$')
			return 0;
		salt = end+1;
		if (u < ROUNDS_MIN)
			r = ROUNDS_MIN;
		else if (u > ROUNDS_MAX)
			return 0;
		else
			r = u;
		/* needed when rounds is zero prefixed or out of bounds */
		sprintf(rounds, "rounds=%u$", r);
	}
	for (i = 0; i < SALT_MAX && salt[i] && salt[i] != '$'; i++)
		/* reject characters that interfere with /etc/shadow parsing */
		if (salt[i] == '\n' || salt[i] == ':')
			return 0;
	slen = i;
	/* B = sha(key salt key) */
	sha256_init(&ctx);
	sha256_update(&ctx, key, klen);
	sha256_update(&ctx, salt, slen);
	sha256_update(&ctx, key, klen);
	sha256_sum(&ctx, md);
	/* A = sha(key salt repeat-B alternate-B-key) */
	sha256_init(&ctx);
	sha256_update(&ctx, key, klen);
	sha256_update(&ctx, salt, slen);
	hashmd256(&ctx, klen, md);
	for (i = klen; i > 0; i >>= 1)
		if (i & 1)
			sha256_update(&ctx, md, sizeof md);
		else
			sha256_update(&ctx, key, klen);
	sha256_sum(&ctx, md);
	/* DP = sha(repeat-key), this step takes O(klen^2) time */
	sha256_init(&ctx);
	for (i = 0; i < klen; i++)
		sha256_update(&ctx, key, klen);
	sha256_sum(&ctx, kmd);
	/* DS = sha(repeat-salt) */
	sha256_init(&ctx);
	for (i = 0; i < 16 + md[0]; i++)
		sha256_update(&ctx, salt, slen);
	sha256_sum(&ctx, smd);
	/* iterate A = f(A,DP,DS), this step takes O(rounds*klen) time */
	for (i = 0; i < r; i++) {
		sha256_init(&ctx);
		if (i % 2)
			hashmd256(&ctx, klen, kmd);
		else
			sha256_update(&ctx, md, sizeof md);
		if (i % 3)
			sha256_update(&ctx, smd, slen);
		if (i % 7)
			hashmd256(&ctx, klen, kmd);
		if (i % 2)
			sha256_update(&ctx, md, sizeof md);
		else
			hashmd256(&ctx, klen, kmd);
		sha256_sum(&ctx, md);
	}
	/* output is $5$rounds=n$salt$hash */
	p = output;
	p += sprintf(p, "$5$%s%.*s$", rounds, slen, salt);
	static const unsigned char perm[][3] = {
		{0,10,20},{21,1,11},{12,22,2},{3,13,23},{24,4,14},
		{15,25,5},{6,16,26},{27,7,17},{18,28,8},{9,19,29} };
	for (i=0; i<10; i++) p = to64(p,
		(md[perm[i][0]]<<16)|(md[perm[i][1]]<<8)|md[perm[i][2]], 4);
	p = to64(p, (md[31]<<8)|md[30], 3);
	*p = 0;
	return output;
}

/* Calculates a SHA256 cryptographic hash for string `str`, optionally using a `salt` of type string, and the optional number
   of `rounds` to be taken. `salt` by default is the empty string and `rounds` is 5000.

   The first return is the hash itself, and the second return includes the control parameters salt and
   rounds plus the first result. In case of errors, the function returns `fail`.

   Based on hashes_sha512. 3.7.8 */
static int hashes_sha256 (lua_State *L) {
  char *p, output[128], *control;
  const char *key, *salt;
  int rounds;
  size_t l;
  key = agn_checklstring(L, 1, &l);
  if (l < 1 || l > KEY_MAX)
    luaL_error(L, "Error in " LUA_QS ": first argument is the empty string or too long.", "hashes.sha256");
  salt = agnL_optstring(L, 2, "");
  rounds = agnL_optposint(L, 3, ROUNDS_DEFAULT);
  if (rounds < ROUNDS_MIN || rounds > ROUNDS_MAX)
    luaL_error(L, "Error in " LUA_QS ": number of rounds must be in %d .. %d.", "hashes.sha256", ROUNDS_MIN, ROUNDS_MAX);
  control = str_concat("$5$rounds=", tools_itoa(rounds, 10), "$", salt, "$", NULL);
  l = tools_strlen(control);  /* 2.17.8 tweak */
  p = sha256crypt(key, control, output);
  xfree(control);
  if (!p) {
    lua_pushfail(L);
    return 1;
  }
  /* produces the same output as the mkpasswd command,
     https://stackoverflow.com/questions/34463134/sha-512-crypt-output-written-with-python-code-is-different-from-mkpasswd */
  luaL_checkstack(L, 2, "not enough stack space"); /* 7.1.9 fix */
  lua_pushstring(L, p + l);
  lua_pushstring(L, p);
  return 2;
}


#undef Ch
#undef Maj
#undef S0
#undef S1
#undef R0
#undef R1


/* Taken from musl-1.1.19/src/misc/a64l.c, MIT licence, https://www.musl-libc.org/src/crypt/crypt_sha512.c */
/*
 * public domain sha512 crypt implementation based on fips180-3
 *
 * original sha crypt design: http://people.redhat.com/drepper/SHA-crypt.txt
 * in this implementation at least 32bit int is assumed,
 * key length is limited, the $6$ prefix is mandatory, '\n' and ':' is rejected
 * in the salt and rounds= setting must contain a valid iteration count,
 * on error "*" is returned.
 */
/* >=2^64 bits messages are not supported (about 2000 peta bytes) */

struct sha512 {
  uint64_t len;     /* processed message length */
  uint64_t h[8];    /* hash state */
  uint8_t buf[128]; /* message block buffer */
};

static uint64_t ror (uint64_t n, int k) { return (n >> k) | (n << (64 - k)); }
#define Ch(x,y,z)  (z ^ (x & (y ^ z)))
#define Maj(x,y,z) ((x & y) | (z & (x | y)))
#define S0(x)      (ror(x, 28) ^ ror(x, 34) ^ ror(x, 39))
#define S1(x)      (ror(x, 14) ^ ror(x, 18) ^ ror(x, 41))
#define R0(x)      (ror(x, 1)  ^ ror(x, 8)  ^ (x >> 7))
#define R1(x)      (ror(x, 19) ^ ror(x, 61) ^ (x >> 6))

static const uint64_t K[80] = {
0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static void processblock (struct sha512 *s, const uint8_t *buf) {
  uint64_t W[80], t1, t2, a, b, c, d, e, f, g, h;
  int i;
  for (i=0; i < 16; i++) {
    W[i] = (uint64_t)buf[8*i] << 56;
    W[i] |= (uint64_t)buf[8*i + 1] << 48;
    W[i] |= (uint64_t)buf[8*i + 2] << 40;
    W[i] |= (uint64_t)buf[8*i + 3] << 32;
    W[i] |= (uint64_t)buf[8*i + 4] << 24;
    W[i] |= (uint64_t)buf[8*i + 5] << 16;
    W[i] |= (uint64_t)buf[8*i + 6] << 8;
    W[i] |= buf[8*i + 7];
  }
  for (; i < 80; i++)
    W[i] = R1(W[i - 2]) + W[i - 7] + R0(W[i - 15]) + W[i - 16];
  a = s->h[0];
  b = s->h[1];
  c = s->h[2];
  d = s->h[3];
  e = s->h[4];
  f = s->h[5];
  g = s->h[6];
  h = s->h[7];
  for (i=0; i < 80; i++) {
    t1 = h + S1(e) + Ch(e, f, g) + K[i] + W[i];
    t2 = S0(a) + Maj(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  s->h[0] += a;
  s->h[1] += b;
  s->h[2] += c;
  s->h[3] += d;
  s->h[4] += e;
  s->h[5] += f;
  s->h[6] += g;
  s->h[7] += h;
}

static void pad (struct sha512 *s) {
  unsigned r = s->len % 128;
  s->buf[r++] = 0x80;
  if (r > 112) {
    tools_bzero(s->buf + r, 128 - r);
    r = 0;
    processblock(s, s->buf);
  }
  tools_bzero(s->buf + r, 120 - r);
  s->len *= 8;
  s->buf[120] = s->len >> 56;
  s->buf[121] = s->len >> 48;
  s->buf[122] = s->len >> 40;
  s->buf[123] = s->len >> 32;
  s->buf[124] = s->len >> 24;
  s->buf[125] = s->len >> 16;
  s->buf[126] = s->len >> 8;
  s->buf[127] = s->len;
  processblock(s, s->buf);
}

static FORCE_INLINE void sha512_init (struct sha512 *s) {
  s->len = 0;
  s->h[0] = 0x6a09e667f3bcc908ULL;
  s->h[1] = 0xbb67ae8584caa73bULL;
  s->h[2] = 0x3c6ef372fe94f82bULL;
  s->h[3] = 0xa54ff53a5f1d36f1ULL;
  s->h[4] = 0x510e527fade682d1ULL;
  s->h[5] = 0x9b05688c2b3e6c1fULL;
  s->h[6] = 0x1f83d9abfb41bd6bULL;
  s->h[7] = 0x5be0cd19137e2179ULL;
}

static FORCE_INLINE void sha512_sum (struct sha512 *s, uint8_t *md) {
  int i;
  pad(s);
  for (i=0; i < 8; i++) {
    md[8*i] = s->h[i] >> 56;
    md[8*i + 1] = s->h[i] >> 48;
    md[8*i + 2] = s->h[i] >> 40;
    md[8*i + 3] = s->h[i] >> 32;
    md[8*i + 4] = s->h[i] >> 24;
    md[8*i + 5] = s->h[i] >> 16;
    md[8*i + 6] = s->h[i] >> 8;
    md[8*i + 7] = s->h[i];
  }
}

static void sha512_update (struct sha512 *s, const void *m, unsigned long len) {
  const uint8_t *p = m;
  unsigned r = s->len % 128;
  s->len += len;
  if (r) {
    if (len < 128 - r) {
      tools_memcpy(s->buf + r, p, len);  /* 2.21.5 tweak */
      return;
    }
    tools_memcpy(s->buf + r, p, 128 - r);  /* 2.21.5 tweak */
    len -= 128 - r;
    p += 128 - r;
    processblock(s, s->buf);
  }
  for (; len >= 128; len -= 128, p += 128)
    processblock(s, p);
  tools_memcpy(s->buf, p, len);  /* 2.21.5 tweak */
}

/*
static char *to64 (char *s, unsigned int u, int n) {
  while (--n >= 0) {
    *s++ = tools_b64[u % 64];
    u /= 64;
  }
  return s;
}
*/

/* key limit is not part of the original design, added for DoS protection.
 * rounds limit has been lowered (versus the reference/spec), also for DoS
 * protection. runtime is O(klen^2 + klen*rounds)
#define KEY_MAX 256
#define SALT_MAX 16
#define ROUNDS_DEFAULT 5000
#define ROUNDS_MIN 1000
#define ROUNDS_MAX 9999999 */

/* hash n bytes of the repeated md message digest */
static FORCE_INLINE void hashmd (struct sha512 *s, unsigned int n, const void *md) {
  unsigned int i;
  for (i=n; i > 64; i -= 64)
    sha512_update(s, md, 64);
  sha512_update(s, md, i);
}

static char *sha512crypt (const char *key, const char *setting, char *output) {
  struct sha512 ctx;
  unsigned char md[64], kmd[64], smd[64];
  unsigned int i, r, klen, slen;
  char rounds[20] = "";
  const char *salt;
  char *p;
  /* reject large keys */
  for (i=0; i <= KEY_MAX && key[i]; i++);
  if (i > KEY_MAX)
    return 0;
  klen = i;
  /* setting: $6$rounds=n$salt$ (rounds=n$ and closing $ are optional) */
  if (tools_strncmp(setting, "$6$", 3) != 0)
    return 0;
  salt = setting + 3;
  r = ROUNDS_DEFAULT;
  if (tools_strncmp(salt, "rounds=", sizeof "rounds=" - 1) == 0) {
    unsigned long u;
    char *end;
    /*
     * this is a deviation from the reference:
     * bad rounds setting is rejected if it is
     * - empty
     * - unterminated (missing '$')
     * - begins with anything but a decimal digit
     * the reference implementation treats these bad
     * rounds as part of the salt or parse them with
     * strtoul semantics which may cause problems
     * including non-portable hashes that depend on
     * the host's value of ULONG_MAX.
    */
    salt += sizeof "rounds=" - 1;
    if (!isdigit(*salt))
      return 0;
    u = strtoul(salt, &end, 10);
    if (*end != '$')
      return 0;
    salt = end + 1;
    if (u < ROUNDS_MIN)
      r = ROUNDS_MIN;
    else if (u > ROUNDS_MAX)
      return 0;
    else
      r = u;
    /* needed when rounds is zero prefixed or out of bounds */
    sprintf(rounds, "rounds=%u$", r);
  }
  for (i=0; i < SALT_MAX && salt[i] && salt[i] != '$'; i++) {
    /* reject characters that interfere with /etc/shadow parsing */
    if (salt[i] == '\n' || salt[i] == ':')
      return 0;
  }
  slen = i;
  /* B = sha(key salt key) */
  sha512_init(&ctx);
  sha512_update(&ctx, key, klen);
  sha512_update(&ctx, salt, slen);
  sha512_update(&ctx, key, klen);
  sha512_sum(&ctx, md);
  /* A = sha(key salt repeat-B alternate-B-key) */
  sha512_init(&ctx);
  sha512_update(&ctx, key, klen);
  sha512_update(&ctx, salt, slen);
  hashmd(&ctx, klen, md);
  for (i=klen; i > 0; i >>= 1) {
    if (i & 1)
      sha512_update(&ctx, md, sizeof md);
    else
      sha512_update(&ctx, key, klen);
  }
  sha512_sum(&ctx, md);
  /* DP = sha(repeat-key), this step takes O(klen^2) time */
  sha512_init(&ctx);
  for (i=0; i < klen; i++)
    sha512_update(&ctx, key, klen);
  sha512_sum(&ctx, kmd);
  /* DS = sha(repeat-salt) */
  sha512_init(&ctx);
  for (i=0; i < 16 + md[0]; i++)
    sha512_update(&ctx, salt, slen);
  sha512_sum(&ctx, smd);
  /* iterate A = f(A,DP,DS), this step takes O(rounds*klen) time */
  for (i=0; i < r; i++) {
    sha512_init(&ctx);
    if (i % 2)
      hashmd(&ctx, klen, kmd);
    else
      sha512_update(&ctx, md, sizeof md);
    if (i % 3)
      sha512_update(&ctx, smd, slen);
    if (i % 7)
      hashmd(&ctx, klen, kmd);
    if (i % 2)
      sha512_update(&ctx, md, sizeof md);
    else
      hashmd(&ctx, klen, kmd);
    sha512_sum(&ctx, md);
  }
  /* output is $6$rounds=n$salt$hash */
  p = output;
  p += sprintf(p, "$6$%s%.*s$", rounds, slen, salt);
#if 0
  static const unsigned char perm[][3] = {
    0, 21, 42, 22, 43, 1, 44, 2, 23, 3, 24, 45, 25, 46, 4,
    47, 5, 26, 6, 27, 48, 28, 49, 7, 50, 8, 29, 9, 30, 51,
    31, 52, 10, 53, 11, 32, 12, 33, 54, 34, 55, 13, 56, 14, 35,
    15, 36, 57, 37, 58, 16, 59, 17, 38, 18, 39, 60, 40, 61, 19,
    62, 20, 41 };
  for (i=0; i < 21; i++)
    p = to64(p, (md[perm[i][0]] << 16) | (md[perm[i][1]] << 8) | md[perm[i][2]], 4);
#else
  p = to64(p, (md[0] << 16)  | (md[21] << 8) | md[42], 4);
  p = to64(p, (md[22] << 16) | (md[43] << 8) | md[1], 4);
  p = to64(p, (md[44] << 16) | (md[2] << 8)  | md[23], 4);
  p = to64(p, (md[3] << 16)  | (md[24] << 8) | md[45], 4);
  p = to64(p, (md[25] << 16) | (md[46] << 8) | md[4], 4);
  p = to64(p, (md[47] << 16) | (md[5] << 8)  | md[26], 4);
  p = to64(p, (md[6] << 16)  | (md[27] << 8) | md[48], 4);
  p = to64(p, (md[28] << 16) | (md[49] << 8) | md[7], 4);
  p = to64(p, (md[50] << 16) | (md[8] << 8)  | md[29], 4);
  p = to64(p, (md[9] << 16)  | (md[30] << 8) | md[51], 4);
  p = to64(p, (md[31] << 16) | (md[52] << 8) | md[10], 4);
  p = to64(p, (md[53] << 16) | (md[11] << 8) | md[32], 4);
  p = to64(p, (md[12] << 16) | (md[33] << 8) | md[54], 4);
  p = to64(p, (md[34] << 16) | (md[55] << 8) | md[13], 4);
  p = to64(p, (md[56] << 16) | (md[14] << 8) | md[35], 4);
  p = to64(p, (md[15] << 16) | (md[36] << 8) | md[57], 4);
  p = to64(p, (md[37] << 16) | (md[58] << 8) | md[16], 4);
  p = to64(p, (md[59] << 16) | (md[17] << 8) | md[38], 4);
  p = to64(p, (md[18] << 16) | (md[39] << 8) | md[60], 4);
  p = to64(p, (md[40] << 16) | (md[61] << 8) | md[19], 4);
  p = to64(p, (md[62] << 16) | (md[20] << 8) | md[41], 4);
#endif
  p = to64(p, md[63], 2);
  *p = 0;
  return output;
}

/* Calculates a SHA512 cryptographic hash for string `str`, optionally using a `salt` of type string, and the optional number
   of `rounds` to be taken. `salt` by default is the empty string and `rounds` is 5000.

   The first return is the hash itself, and the second return includes the control parameters salt and
   rounds plus the first result. Thus, the second return is the same as the output of the mkpasswd UNIX
   command. In case of errors, the function returns `fail`. */
static int hashes_sha512 (lua_State *L) {  /* 2.12.6 */
  char *p, output[128], *control;
  const char *key, *salt;
  int rounds;
  size_t l;
  key = agn_checklstring(L, 1, &l);
  if (l < 1 || l > KEY_MAX)
    luaL_error(L, "Error in " LUA_QS ": first argument is the empty string or too long.", "hashes.sha512");
  salt = agnL_optstring(L, 2, "");
  rounds = agnL_optposint(L, 3, ROUNDS_DEFAULT);
  if (rounds < ROUNDS_MIN || rounds > ROUNDS_MAX)
    luaL_error(L, "Error in " LUA_QS ": number of rounds must be in %d .. %d.", "hashes.sha512", ROUNDS_MIN, ROUNDS_MAX);
  control = str_concat("$6$rounds=", tools_itoa(rounds, 10), "$", salt, "$", NULL);
  l = tools_strlen(control);  /* 2.17.8 tweak */
  p = sha512crypt(key, control, output);
  xfree(control);
  if (!p) {
    lua_pushfail(L);
    return 1;
  }
  /* produces the same output as the mkpasswd command,
     https://stackoverflow.com/questions/34463134/sha-512-crypt-output-written-with-python-code-is-different-from-mkpasswd */
  luaL_checkstack(L, 2, "not enough stack space");  /* 7.1.9 fix */
  lua_pushstring(L, p + l);
  lua_pushstring(L, p);
  return 2;
}


/* Computes a variable-length integer hash for string or number x and the given keyword (`salt`). If the optional positive integer
   n is given, the computed hash is taken modulo n. Depending on the given keyword, the number of collisions might be zero, so this
   function is an alternative to hashes.md5.

   See: https://stackoverflow.com/questions/10334237/one-way-hash-with-variable-length-alphanumeric-output, answer #1 by johnnycrash */
static int hashes_varlen (lua_State *L) {  /* 2.14.6 */
  const char *str, *key;
  char *r;
  int salt;
  size_t l1, l2, i;
  ub4 n, h;
  str = NULL;  /* to avoid compiler warnings */
  if (agn_isstring(L, 1)) {
    str = agn_checklstring(L, 1, &l1);
  } else if (agn_isnumber(L, 1)) {
    str = lua_tolstring(L, 1, &l1);
    if (str == NULL)
      luaL_error(L, "Error in " LUA_QS ": first argument could not be converted to a string.", "hashes.varlen");
  } else
    luaL_error(L, "Error in " LUA_QS ": first argument must be a number or string.", "hashes.varlen");
  key = agn_checklstring(L, 2, &l2);
  n = agnL_optuint32_t(L, 3, 0);
  r = agn_stralloc(L, l1, "hashes.varlen", NULL);  /* 2.16.5 change */
  h = 0;
  for (i=0; i < l1; i++) {
    salt = key[i % l2];
    r[i] = str[i] ^ salt;  /* xor with a different char from the key */
    h += (((str[i] - '0') ^ salt) % 10) * luai_numipow(10, i);
  }
  luaL_checkstack(L, 2, "not enough stack space");  /* 7.1.9 fix */
  r[i] = '\0';
  lua_pushlstring(L, r, l1);
  xfree(r);
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 2;
}


/* Sums up all the ASCII values in string str and returns the result as a positive integer. The sum is expressed as an unsigned
32-bit integer, so keep overflows in mind.

Instead of of just adding the plain ASCII values, you might optionally apply function f to the first character in str, and
function g to the second character in str, then f again on the third character, g on the fourth character, and so forth.

An example to compute the so-called Internet checksum, not strictly compliant to RFC 1071, see:
https://stackoverflow.com/questions/4113890/how-to-calculate-the-internet-checksum-from-a-byte-in-java

import bytes;
sum := hashes.sumupchars("alexander", << x -> bytes.shift32(bytes.and32(x, 0xff), -8) >>, << x -> bytes.and32(x, 0xff) >>):
bytes.not32(sum + bytes.shift32(sum, 16)):

Note that the Internet checksum, even if strictly implemented according to RFC 1071, has by far more collisions than the
other hash functions available in this package. */
static int hashes_sumupchars (lua_State *L) {  /* 2.14.6 */
  int isfunc2, isfunc3, error;
  size_t l;
  const char *str;
  register ub4 sum;
  sum = 0;
  str = agn_checklstring(L, 1, &l);
  isfunc2 = lua_isfunction(L, 2);
  isfunc3 = lua_isfunction(L, 3);
  while (l > 0) {
    if (isfunc2) {
      lua_pushvalue(L, 2);
      lua_pushnumber(L, (unsigned char) *str++);
      sum += agn_ncall(L, 1, &error, 1);
    } else
      sum += (unsigned char) *str++;
    if ((--l) == 0) break;
    if (isfunc3) {
      lua_pushvalue(L, 3);
      lua_pushnumber(L, (unsigned char) *str++);
      sum += agn_ncall(L, 1, &error, 1);
    } else
      sum += (unsigned char) *str++;
    --l;
  }
  lua_pushnumber(L, sum);
  return 1;
}


static int hashes_djb2rot (lua_State *L) {  /* 2.14.7; like djb2, but using an additional left rotation-bit shift
  operation; good performance, few collisions. The algorithm used is equivalent to:

djb2rot := proc(s :: string, n, sh, f) is
   local h;
   h :=  5381;
   f :=  f or 33;
   sh := sh or 17;
   n :=  n or 0;
   for i in s do
      h := bytes.rotate32(h, -sh);
      h := bytes.xor32(bytes.mul32(f, h), abs i)
   od;
   return if n <> 0 then h % n else h fi
end; */
  const char *s;
  ub4 n, h, sh, f;
  s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);   /* default: no modulus operation at finish */
  sh = agnL_optposint(L, 3, 17);   /* shift factor, 17 = one collision with 309k German word dictionary */
  f = agnL_optposint(L, 4, 33);    /* factor, 33 = one collision with 309k German word dictionary */
  h = agnL_optuint32_t(L, 5, 5381);  /* default: no modulus operation at finish */
  while (*s) {
    h = (h << sh) | (h >> (LUA_NBITS - sh));  /* unsigned left rotation-bit shift */
    h = f * h ^ (ub4)*s++;
  }
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


static FORCE_INLINE ub4 strval (const char *s, uint32_t sh, register uint32_t h) {  /* 2.14.7 */
  while (*s) {
    h <<= sh;  /* 8 is the original shift */
    h += (ub4)*s++;
  }
  return h;
  /* words with the same ending have the same hash value */
}


/* Computes a hash with many collisions, useful to classify words with common endings since they have the same
   hash code The algorithm used is equivalent to:

strval := proc(s :: string, sh, h) is
   sh := n or 8;
   h := h or 0;
   for i in s do
      h := bytes.shift32(h, -sh);
      h := bytes.add32(h, abs i)
   od;
   return h
end; */
static int hashes_strval (lua_State *L) {
  lua_pushnumber(L, strval(agn_checkstring(L, 1), agnL_optposint(L, 2, 8), agnL_optnonnegint(L, 3, 0)) );
  return 1;
}


/* See: A. V. Aho, R. Sethi, J. D. Ullman, "Compilers: Principle, Techniques, and Tools", Addison-Wesley, 1988, p. 436,
   taken from raw.githubusercontent.com/pscedu/pfl/master/pfl/opt-misc.c; 2.14.8 */
static int hashes_asu (lua_State *L) {  /* 2.14.8 */
  const char *s;
  ub4 n, h, g;
  s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);  /* default: no modulus operation at finish */
  h = agnL_optuint32_t(L, 3, 0);
  while (*s) {
    h = (h << 4UL) + (ub4)*s++;
    g = h & 0xf0000000;
    if (g) {
      h = h ^ (g >> 24UL);
      h = h ^ g;
    }
  }
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* Similar to hashes.asu, tweaked for 32-bit processors; see http://www.partow.net/programming/hashfunctions */
static int hashes_elf (lua_State *L) {  /* 2.14.8 */
  const char *s;
  ub4 n, h, x;
  s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);  /* default: no modulus operation at finish */
  h = agnL_optuint32_t(L, 3, 0);
  while (*s) {
    h = (h << 4UL) + (ub4)*s++;
    x = h & 0xf0000000L;
    if (x) h ^= (x >> 24UL);
    h &= ~x;
  }
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* Algorithm proposed by Donald E. Knuth in The Art Of Computer Programming Volume 3, under
   the topic of sorting and search, see: http://www.partow.net/programming/hashfunctions */
static int hashes_dek (lua_State *L) {  /* 2.14.8 */
  const char *s;
  ub4 n, h;
  size_t l;  /* to prevent compiler warnings in DJGPP */
  s = agn_checklstring(L, 1, &l);
  n = agnL_optuint32_t(L, 2, 0);  /* default: no modulus operation at finish */
  h = agnL_optuint32_t(L, 3, l);  /* initial value chosen by Donald Knuth is string length */
  while (*s) {
    h = ((h << 5UL) ^ (h >> 27UL)) ^ (ub4)*s++;
  }
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* Algorithm proposed by Brian Kernigham and Dennis Ritchie, similar to hashes.djb. */
static int hashes_bkdr (lua_State *L) {  /* 2.14.8 */
  const char *s;
  ub4 n, h, seed;
  s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);  /* default: no modulus operation at finish */
  seed = agnL_optuint32_t(L, 3, 131);  /* 31 131 1313 13131 131313 etc. */
  h = agnL_optuint32_t(L, 4, 0);  /* 3.10.1 */
  while (*s) {
    h = (h * seed) + (ub4)*s++;
  }
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* Computes the System V hash to access libraries via dynamic symbol tables on UNIX. For GNU hash,
   see `hashes.pl`. */
static int hashes_sysv (lua_State *L) {  /* 2.15.0, found in Ulysses-libc/src/ldso/dynlink.c */
  ub4 n, h, f;
  const char *s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);
  f = agnL_optuint32_t(L, 3, 16);
  h = agnL_optuint32_t(L, 4, 0);
  while (*s) {
    h = f*h + (ub4)*s++;
    h ^= (h >> 24UL) & 0xf0;
  }
  h &= 0xfffffff;
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* Returns a hash value for the non-negative integer k and the given number of slots m (also a non-negative integer)
   that is more evenly distributed than just computing k % m. The function uses Fibonacci hashing, and returns a
   value that is equivalent to:

   fibmod := proc(k :: nonnegint, m :: nonnegint) is
      local p := k *(math.Phi - 1);
      return bytes.numto32(m * frac(p))
   end;

   The result is in the range 0 .. m - 1 if m is odd; if m is even, the result is always in the range 1 .. m - 1,
   unless k = 0 or m = 0 where the function returns 0.

   Note that with a < b, fibmod(a, m) is not necessarily less than fibmod(b, m).

   If you pass the optional third argument `true`, then the results are always the same across different platforms -
   due to performance reasons, the default is false. */
static int hashes_fibmod (lua_State *L) {  /* 2.14.8 */
  ub4 k, m;
  k = agnL_optuint32_t(L, 1, 0);  /* integer hash, 4.12.5 change */
  m = agnL_optuint32_t(L, 2, 0);  /* total number of slots in hash table */
  lua_pushnumber(L, tools_fibmod(k, m, agnL_optboolean(L, 3, 0)));  /* result is in the range 0 .. m - 1;
    "double p = k*_PHIM1; floor(m*(p - floor(p)))" is slower. */
  return 1;
}


/* Computes the Fibonacci modulus for an integer a over a set of 2^i slots. It is an alternative to the % operator,
   but returning a more evenly distributed integer in the range [0, 2^i - 1], with every number far removed from
   the previous and the next number. See also: hashes.fibmod, math.iapow2. 20 % faster than hashes.fibmod called
   with a power of 2. 2.38.2
   See also:
   https://probablydance.com/2018/06/16/fibonacci-hashing-the-optimization-that-the-world-forgot-or-a-better-alternative-to-integer-modulo/
   */
static int hashes_fibmod2 (lua_State *L) {
  lua_Integer hash, shift;
  hash = agn_checkinteger(L, 1);     /* number */
  shift = agn_checknonnegint(L, 2);  /* pow-2 exponent */
  uint64_t t = (2654435769ULL * (uint64_t)hash) & 0x00000000FFFFFFFFULL;
  shift &= 0x0000001F;  /* make sure shift is 31 or less */
  lua_pushnumber(L, (shift == 0) ? 0 : t >> (32 - shift));  /* returns a value in the uint32_t domain */
  return 1;
}


/* Computes the Wang hash for non-negative integer x. If n, a positive integer, is given, the computed hash
   is taken modulo n. 4.5.3. Written by Enno Rehling, taken from his file strings.c.

   Copyright © 2017 by Enno Rehling

   Permission to use, copy, modify, and/or distribute this software for
   any purpose with or without fee is hereby granted, provided that the
   above copyright notice and this permission notice appear in all
   copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
   REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL ISC BE LIABLE FOR ANY
   SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
   OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */
static int hashes_wang (lua_State *L) {
  ub4 h, n;
  h = agnL_optuint32_t(L, 1, 0);
  n = agnL_optuint32_t(L, 2, 0);
  h = ~h + (h << 15);
  h = h ^ (h >> 12);
  h = h + (h << 2);
  h = h ^ (h >> 4);
  h = h * 2057;
  h = h ^ (h >> 16);
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/*
import hashes;

fibmod := hashes.fibmod;
fibmod2 := hashes.fibmod2;

watch();
s := nseq( << x -> 2^x >>, 1, 10);
for i from 0 to 1m do
   for j in s do
       x := fibmod(i, j)
   od
od;
watch():

watch();
s := nseq(1, 10);
for i from 0 to 1m do
   for j in s do
       x := fibmod2(i, j)
   od
od;
watch():
*/

/* Hash found in General Purpose Hash Function Algorithms Library, by Arash Partow, 2002. MIT, 2.15.4, adapted.
   See: http://www.partow.net/programming/hashfunctions/index.html */
static int hashes_rs (lua_State *L) {
  const char *s;
  ub4 n, h, a, b;
  a = 63689UL;
  b = 378551UL;
  s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);
  h = agnL_optuint32_t(L, 3, 0);
  while (*s) {
    h = h*a + (ub4)*s++;
    a *= b;
  }
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* Hash found in General Purpose Hash Function Algorithms Library, by Arash Partow, 2002. MIT, 2.15.4, adapted.
   See: http://www.partow.net/programming/hashfunctions/index.html */
static int hashes_bp (lua_State *L) {
  const char *s;
  ub4 n, h;
  s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);
  h = agnL_optuint32_t(L, 3, 0);
  while (*s) {
    h = h << 7UL ^ (ub4)*s++;
  }
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* P. J. Weinberger Hash Function, found in General Purpose Hash Function Algorithms Library, by Arash Partow, 2002. MIT.
   See: http://www.partow.net/programming/hashfunctions/index.html, 2.15.4, adapted */
static int hashes_pjw (lua_State *L) {
  const char *s;
  ub4 n, h, test;
  const ub4 BitsInUnsignedInt = (ub4)(sizeof(ub4)*8);
  const ub4 ThreeQuarters     = (ub4)((BitsInUnsignedInt*3)/4);
  const ub4 OneEighth         = (ub4)(BitsInUnsignedInt/8);
  const ub4 HighBits          = (ub4)(0xFFFFFFFFUL) << (BitsInUnsignedInt - OneEighth);
  s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);
  h = agnL_optuint32_t(L, 3, 0);
  test = 0;
  while (*s) {
    h = (h << OneEighth) + (ub4)*s++;
    if ( (test = h & HighBits) != 0 )
      h = (h ^ (test >> ThreeQuarters)) & ~HighBits;
  }
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* Taken from: https://stackoverflow.com/questions/9335169/understanding-strange-java-hash-function */
/**
 * Applies a supplemental hash function to a given hashCode, which
 * defends against poor quality hash functions.  This is critical
 * because HashMap uses power-of-two length hash tables, that
 * otherwise encounter collisions for hashCodes that do not differ
 * in lower bits. Note: Null keys always map to hash 0, thus index 0.
 * This function ensures that hashCodes that differ only by
 * constant multiples at each bit position have a bounded
 * number of collisions (approximately 8 at default load factor). UNDOCUMENTED 2.16.6
 */
static FORCE_INLINE int hashmaphash (int32_t hx) {
  uint32_t h = (uint32_t)hx;
  h ^= (h >> 20UL) ^ (h >> 12UL);
  return h ^ (h >> 7UL) ^ (h >> 4UL);
}


static int hashes_hashmap (lua_State *L) {  /* 2.16.6 */
  int32_t h;
  ub4 n;
  h = luaL_checkint32_t(L, 1);
  n = agnL_optuint32_t(L, 2, 0);
  lua_pushnumber(L, (n != 0) ? hashmaphash(h) % n : hashmaphash(h));
  return 1;
}


/* Computes the Adler32 string hash.
   Taken from: https://github.com/SheetJS/js-adler32/blob/master/adler32.js */
static int hashes_adler32 (lua_State *L) {  /* 2.16.8 */
  const char *s;
  ub4 p, q, r, n, h;
  p = 0; q = 1;
  s = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);
  h = agnL_optuint32_t(L, 3, 65521);  /* default: largest prime that is less than 65536 */
  while (*s) {
    /* unsigned char c;
    c = *s++;
    if (c < 128) q += c;
    else {
      q += 192 | ((c >> 6) & 31); p += q;
      q += 128 | (c & 63);
    } */
    q += (ub4)*s++;
    p += q;
  q = 15*rotr32(q, 16) + (q & 65535);
  p = 15*rotr32(p, 16) + (p & 65535);
  }
  r = ((q % h) << 16UL) | (p % h);
  lua_pushnumber(L, (n != 0) ? r % n : r);
  return 1;
}


/* taken from https://gist.github.com/serialhex/ddbaf64fa9942a6c41b29618974f5b3a#file-derpyhash-c,
   no licence information given. 2.38.2
   "fails a few tests, but overall is fast and passes most tests"

   Takes a string and an optional seed and computes the Derpy hash, returned as two unsigned 4-byte integers, i.e. the
   higher and lower parts of the hash. The seed defaults to 0.

   Some Fibonacci constants for a number of bits b
   * floor(2^(b)/ ((1 + v5)/2))
   * 2^8 = 158
   * 2^16 = 40503
   * 2^32 = 2654435769
   * 2^64 = 11400714819323198485
   * 2^128 = 210306068529402873165736369884012333108 */
static uint64_t derpyhash (const char *key, int len, uint32_t seed) {
  int i;
  const uint64_t *data = (const uint64_t *)key;
  const uint64_t fib = 11400714819323198485ULL;
  /* 8 bytes in a 64-bit value, so take the last 7 bits */
  uint8_t m = len & 7;
  /* initialize */
  uint64_t h = (seed ^ len) * fib;
  for (i=0; i < len/8; i++) {
    h = (h + data[i]) * fib;
  }
  if (m) {
    uint8_t rem = len - m;
    const uint8_t *d = (const uint8_t *)key;
    uint64_t last = 0;
    switch (m) {
      case 7: last = d[rem++];
      case 6: last = (last << 8ULL) | d[rem++];
      case 5: last = (last << 8ULL) | d[rem++];
      case 4: last = (last << 8ULL) | d[rem++];
      case 3: last = (last << 8ULL) | d[rem++];
      case 2: last = (last << 8ULL) | d[rem++];
      case 1: last = (last << 8ULL) | d[rem++];
    }
    h = (h + last) * fib;
  }
  /* from pcg_output_rxs_m_xs_64_64: */
  uint64_t word = ((h >> ((h >> 59ULL) + 5ULL)) ^ h) * 12605985483714917081ULL;
  return (word >> 43ULL) ^ word;
}

static int hashes_derpy (lua_State *L) {  /* 2.38.2, changed 2.38.3 */
  size_t l;
  uint64_t h;
  int interweave;
  uint32_t seed, high, low;
  const char *s = agn_checklstring(L, 1, &l);
  seed = agnL_optuint32_t(L, 2, 0);
  interweave = agnL_optboolean(L, 3, 1);
  h = derpyhash(s, l, seed);
  high = (uint32_t)(h >> 32ULL);
  low = (uint32_t)h;
  luaL_checkstack(L, 2 - (interweave != 0), "not enough stack space");  /* 7.1.9 fix */
  if (interweave) {
    lua_pushnumber(L, high ^ low);
  } else {
    lua_pushnumber(L, high);
    lua_pushnumber(L, low);
  }
  return 2 - (interweave != 0);
}


/* Splits a number into its higher and lower unsigned 4-byte words hx and lx and applies one of the following binary operations on them:
   'or' (the default), 'and', 'xor'. See also: `bytes.numwords`. By passing a non-negative mask as the optional third argument,
   the mask is applied to the intermediate result, the default is 0xFFFFFFFF.
   If a fourth positive sh integer is given, the intermediate result is right-shifted sh bits; if sh is a negative integer, it is
   left-shifted sh bits. If sh is 0 (the default), there is no shift.
   If a fifth argument n is given, a positive integer, the intermediate result is taken modulus n. The default is 1.

   Thus, with hx, lx := bytes.numwords(x):
   option = 'or':  (((hx || lx) && mask) >>> sh) % n, if sh > 0,
   option = 'and': (((hx && lx) && mask) >>> sh) % n, if sh > 0,
   option = 'xor': (((hx ^^ lx) && mask) >>> sh) % n, if sh > 0,

   or

   option = 'or':  (((hx || lx) && mask) <<< |sh|) % n, if sh < 0,
   option = 'and': (((hx && lx) && mask) <<< |sh|) % n, if sh < 0,
   option = 'xor': (((hx ^^ lx) && mask) <<< |sh|) % n, if sh < 0.
   */
enum ACTION {AGN_AND, AGN_OR, AGN_XOR};

static int hashes_interweave (lua_State *L) {  /* 2.17.1 */
  uint32_t hx, lx, mask, sh, n;
  static const int action[] = {AGN_AND, AGN_OR, AGN_XOR};
  static const char *const actionnames[] = {"and", "or", "xor", NULL};
  EXTRACT_WORDS(hx, lx, agn_checknumber(L, 1));
  int op = luaL_checkoption(L, 2, "or", actionnames);
  mask = agnL_optuint32_t(L, 3, 0xFFFFFFFFUL);
  sh = agnL_optuint32_t(L, 4, 0);
  n = agnL_optuint32_t(L, 5, 0);
  switch (action[op]) {
    case AGN_AND: { hx &= lx; break; }
    case AGN_OR:  { hx |= lx; break; }
    case AGN_XOR: { hx ^= lx; break; }
    default:
      luaL_error(L, "Error in " LUA_QS ": unknown option.", "hashes.interweave");
  }
  hx = (mask == 0xFFFFFFFFUL) ? hx : hx & mask;
#ifdef DONOTCOMPILEME
  if (sh < 0)
    hx <<= sh;
  else 
#endif
  if (sh > 0)
    hx >>= sh;
  if (n > 0) hx %= n;
  lua_pushnumber(L, hx);
  return 1;
}


/* Hashes an unsigned 4-byte integer a (i.e. in the range 0 .. 2^32 - 1) to yet another integer in the same range, Julia-style.
   If a second argument n is given, a positive integer, the intermediate result is taken modulus n. The default for n is 1.

   The function takes care of +/-0, undefined and infinity, i.e. maps them to the same hash.

   Taken from Julia 1.5.0 RC 2, file base/hashing.jl, function hash_32_32; 2.21.7 */
static FORCE_INLINE uint32_t julia32to32 (uint32_t a) {
  a = (a + 0x7ed55d16) + (a << 12UL);
  a = (a ^ 0xc761c23c) ^ (a >> 19UL);
  a = (a + 0x165667b1) + (a << 5UL);
  a = (a + 0xd3a2646c) ^ (a << 9UL);
  a = (a + 0xfd7046c5) + (a << 3UL);
  a = (a ^ 0xb55a4f09) ^ (a >> 16UL);
  return a;
}

static int hashes_j32to32 (lua_State *L) {
  uint32_t a, n;
  a = luaL_checkuint32_t(L, 1);
  n = agnL_optposint(L, 2, 0);
  a = julia32to32(a);
  if (n > 0) a %= n;
  lua_pushnumber(L, a);
  return 1;
}


/* Value-based hashing of an unsigned 4-byte integer n, with seed h which by default is 4294967295 = 2^32 - 1,
   ported from the Julia language.

   The function takes care of +/-0, undefined and infinity, i.e. maps them to the same hash.

   See Julia 1.5.0 RC 2, file base/hashing2.jl, function hash_integer. 2.21.7 */

#define UINT32W 4294967295UL  /* 4294967296 is too long for uint32_t */

static int hashes_jinteger (lua_State *L) {
  ub4 n, h;
  n = agnL_optuint32_t(L, 1, 0);
  h = agnL_optuint32_t(L, 2, UINT32W);
  h ^= julia32to32((n % UINT32W) ^ h);
  n >>= 31;  /* = tools_uarshift32(sizeof(uint32_t), -3) - 1; in Julia, we need >> 32 */
  while (n != 0) {
    h ^= julia32to32((n % UINT32W) ^ h);
    n >>= 31;
  }
  lua_pushnumber(L, h);
  return 1;
}


/* Maps a number x to one or two unsigned 4 byte integers, Julia-style.
   By default, only one unsigned 4-byte integer is returned. If you pass `true` for `option` then the function splits x into its higher and lower
   unsigned 4-byte words and returns unsigned 4-byte integer hashes for each of them. In this case, the second return is equal to the result of
   `hashes.jnumber` when called without an option. See also: `hashes.jinteger`, `bytes.numwords`.
   Taken from julia-1.8.0-full.tar.gz/src/support/hashing.c, MIT licence. 2.31.1 */

static int hashes_jnumber (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  ub4 n = agnL_optuint32_t(L, 2, 0);
  int option = agnL_optboolean(L, 3, 0);
  uint64_t key = tools_doubletouint64(x);
  key = (~key) + (key << 18);  /* key = (key << 18) - key - 1; */
  key =   key  ^ (key >> 31);
  key = key * 21;              /* key = (key + (key << 2)) + (key << 4); */
  key = key ^ (key >> 11);
  key = key + (key << 6);
  key = key ^ (key >> 22);
  switch (option) {
    case 0: {
      lua_pushnumber(L, n == 0 ? (uint32_t)key : (uint32_t)key % n);
      break;
    }
    case 1: {
      uint32_t hx, lx;
      hx = tools_uint64touint32(key, &lx);
      lua_pushnumber(L, n == 0 ? hx : hx % n);
      lua_pushnumber(L, n == 0 ? lx : lx % n);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": unknown option.", "hashes.jnumber");
  }
  return 1 + option;
}


/* Computes the System V IPC (Inter Process Communications) key. inode, device and optional id are all 4-byte signed integers,
   with id defaulting to 0. The return is an integer. The return is equivalent to, in signed bits mode:
      ((inode && 0xffff) || ((device && 0xff) <<< 16) || ((id && 0xffu) <<< 24))
   See also: `os.ftok`. 2.35.5 */
static int hashes_ftok (lua_State *L) {
  int32_t inode, device, id, n, r;
  /* we do not explicitly set signed bits mode for the ftok macro implicitly processes signed integers */
  inode = luaL_checkint32_t(L, 1);
  device = luaL_checkint32_t(L, 2);
  id = luaL_optint32_t(L, 3, 0);
  n = luaL_optint32_t(L, 4, 0);
  r = tools_ftok(inode, device, id);
  lua_pushnumber(L, (n != 0) ? r % n : r);
  return 1;
}


/* Takes a string and applies the SuperFastHash algorithm on all its characters, returning an integer in the domain
   0 through 4294967295 decimal. If n, a positive integer, is given, the computed hash is taken modulo n. The optional
   argument h determines the initial value of the resulting hash code before the string is evaluated, and is 0 by default.

   Taken from https://github.com/LloydLabs/Windows-API-Hashing/tree/master
   No licence given there. 3.10.1 */
static int hashes_superfast (lua_State *L) {
  char *p;
  int32_t rem;
  size_t slen, i;
  uint32_t h, n, t;
  const char *s = agn_checklstring(L, 1, &slen);
  if (s == NULL || slen == 0) {
    lua_pushinteger(L, 0);
    return 1;
  }
  p = (char *)s;
  n = agnL_optuint32_t(L, 2, 0);
  h = agnL_optuint32_t(L, 3, 0);
  rem = (slen & 3);
  slen >>= 2;
  for (i = slen; i > 0; i--) {
    h += *(const uint16_t *)p;
    t = (*(const uint16_t *)(p + 2) << 11) ^ h;
    h = (h << 16) ^ t;
    p += (2*sizeof(uint16_t));
    h += h >> 11;
  }
  switch (rem) {
    case 1: {
      h += *p;
      h ^= h << 10;
      h += h >> 1;
      break;
    }
    case 2: {
      h += *(const uint16_t *)p;
      h ^= h << 11;
      h += h >> 17;
      break;
    }
    case 3: {
      h += *(const uint16_t *)p;
      h ^= h << 16;
      h ^= p[sizeof(uint16_t)] << 18;
      h += h >> 11;
      break;
    }
  }
  h ^= h << 3;
  h += h >> 5;
  h ^= h << 4;
  h += h >> 17;
  h ^= h << 25;
  h += h >> 6;
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


#define ISPELLSHIFT 5

/* Computes the ISpell hash for string s. If n, a positive integer, is given, the computed hash is taken modulo n.
   The optional argument h determines the initial value of the resulting hash code before the string is evaluated,
   and is 0 by default. */
static int hashes_ispell (lua_State *L) {  /* 3.10.1 */
  int	i;
  uint32_t h, n;
  const char *str = agn_checkstring(L, 1);
  n = agnL_optuint32_t(L, 2, 0);
  h = agnL_optuint32_t(L, 3, 0);
  for (i=4; i && *str; i--) {
    h = (h << 8) | uchar(*str++);
  }
  while (*str) {
    h = (h << ISPELLSHIFT) | ((h >> (32 - ISPELLSHIFT)) & ((1 << ISPELLSHIFT) - 1));
    h ^= uchar(*str++);
  }
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* Internet Checksum according to RFC 1077, 3.17.3; many, many collisions ! */
static int hashes_internet (lua_State *L) {
  uint32_t h, n;
  size_t l;
  const char *s = agn_checklstring(L, 1, &l);
  n = agnL_optuint32_t(L, 2, 0);
  h = tools_rfc1071((void *)s, l);
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2018, Erik Moqvist
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This software is part of the Simba project.
 *
 * The function conducts a 16-bits cyclic redundancy check using the
 * CCITT algorithm (polynomial `x^16+x^12+x^5+x^1`).
 *
 * @param[in] crc Initial crc. Should be 0xffff for CCITT/CRC ITU-T.
 * @param[in] buf_p Buffer to calculate crc of.
 * @param[in] size Size of the buffer.
 *
 * @return Calculated crc.
 *
 * Taken from: https://github.com/eerimoq/simba
 * See licence above.
 */
static const uint16_t ccitt_tab[256] = {
  0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
  0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
  0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
  0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
  0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
  0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
  0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
  0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
  0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
  0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
  0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
  0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
  0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
  0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
  0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
  0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
  0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
  0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
  0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
  0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
  0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
  0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
  0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
  0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
  0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
  0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
  0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
  0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
  0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
  0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
  0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
  0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

static int hashes_ccitt (lua_State *L) {
  size_t l;
  uint16_t crc, n;
  const char *s = agn_checklstring(L, 1, &l);
  n =   (uint16_t)agnL_optuint32_t(L, 2, 0);
  crc = (uint16_t)agnL_optnonnegint(L, 3, 0);
  while (l--) {
    crc = (crc << 8) ^ ccitt_tab[(crc >> 8) ^ uchar(*s++)];
  }
  lua_pushnumber(L, (n != 0) ? crc % n : crc);
  return 1;
}


/* CRC ITU-T V.41, 4.7.2, UNDOC */
static int hashes_ccitu (lua_State *L) {
  size_t l;
  uint16_t crc, n;
  const char *s = agn_checklstring(L, 1, &l);
  n =   (uint16_t)agnL_optuint32_t(L, 2, 0);
  crc = (uint16_t)agnL_optnonnegint(L, 3, 0);
  while (l--) {
    crc = (crc >> 8) ^ ccitt_tab[(crc ^ uchar(*s++)) & 0xff];
  }
  lua_pushnumber(L, (n != 0) ? crc % n : crc);
  return 1;
}


/**
 * Conducts an 8-bits cyclic redundancy check using the CRC-7 algorithm
 * (polynomial `x^7+x^3+1`).
 *
 * @param[in] buf_p Buffer to calculate crc of.
 * @param[in] size Size of the buffer.
 *
 * @return Calculated crc.
 *
 * Taken from: https://github.com/eerimoq/simba
 * See licence above.
 */
static int hashes_crc7 (lua_State *L) {
  size_t l;
  uint8_t crc, n, data;
  const char *s = agn_checklstring(L, 1, &l);
  n =   (uint8_t)agnL_optuint32_t(L, 2, 0);
  crc = (uint8_t)agnL_optnonnegint(L, 3, 0);
  while (l--) {
    data = uchar(*s++);
    data ^= (crc << 1);
    if (data & 0x80) {
      data ^= 9;
    }
    crc = (data ^ (crc & 0x78) ^ (crc << 4) ^ ((crc >> 3) & 0x0f));
  }
  crc = ((crc << 1) ^ (crc << 4) ^ (crc & 0x70) ^ ((crc >> 3) & 0x0f));
  crc |= 1;
  lua_pushnumber(L, (n != 0) ? crc % n : crc);
  return 1;
}


/* Taken from:
   https://stackoverflow.com/questions/2351087/what-is-the-best-32bit-hash-function-for-short-strings-tag-names
   Posted by user `Alexander` */
static const unsigned char maprime2c_tab[256] = {
  0xa3, 0xd7, 0x09, 0x83, 0xf8, 0x48, 0xf6, 0xf4,
  0xb3, 0x21, 0x15, 0x78, 0x99, 0xb1, 0xaf, 0xf9,
  0xe7, 0x2d, 0x4d, 0x8a, 0xce, 0x4c, 0xca, 0x2e,
  0x52, 0x95, 0xd9, 0x1e, 0x4e, 0x38, 0x44, 0x28,
  0x0a, 0xdf, 0x02, 0xa0, 0x17, 0xf1, 0x60, 0x68,
  0x12, 0xb7, 0x7a, 0xc3, 0xe9, 0xfa, 0x3d, 0x53,
  0x96, 0x84, 0x6b, 0xba, 0xf2, 0x63, 0x9a, 0x19,
  0x7c, 0xae, 0xe5, 0xf5, 0xf7, 0x16, 0x6a, 0xa2,
  0x39, 0xb6, 0x7b, 0x0f, 0xc1, 0x93, 0x81, 0x1b,
  0xee, 0xb4, 0x1a, 0xea, 0xd0, 0x91, 0x2f, 0xb8,
  0x55, 0xb9, 0xda, 0x85, 0x3f, 0x41, 0xbf, 0xe0,
  0x5a, 0x58, 0x80, 0x5f, 0x66, 0x0b, 0xd8, 0x90,
  0x35, 0xd5, 0xc0, 0xa7, 0x33, 0x06, 0x65, 0x69,
  0x45, 0x00, 0x94, 0x56, 0x6d, 0x98, 0x9b, 0x76,
  0x97, 0xfc, 0xb2, 0xc2, 0xb0, 0xfe, 0xdb, 0x20,
  0xe1, 0xeb, 0xd6, 0xe4, 0xdd, 0x47, 0x4a, 0x1d,
  0x42, 0xed, 0x9e, 0x6e, 0x49, 0x3c, 0xcd, 0x43,
  0x27, 0xd2, 0x07, 0xd4, 0xde, 0xc7, 0x67, 0x18,
  0x89, 0xcb, 0x30, 0x1f, 0x8d, 0xc6, 0x8f, 0xaa,
  0xc8, 0x74, 0xdc, 0xc9, 0x5d, 0x5c, 0x31, 0xa4,
  0x70, 0x88, 0x61, 0x2c, 0x9f, 0x0d, 0x2b, 0x87,
  0x50, 0x82, 0x54, 0x64, 0x26, 0x7d, 0x03, 0x40,
  0x34, 0x4b, 0x1c, 0x73, 0xd1, 0xc4, 0xfd, 0x3b,
  0xcc, 0xfb, 0x7f, 0xab, 0xe6, 0x3e, 0x5b, 0xa5,
  0xad, 0x04, 0x23, 0x9c, 0x14, 0x51, 0x22, 0xf0,
  0x29, 0x79, 0x71, 0x7e, 0xff, 0x8c, 0x0e, 0xe2,
  0x0c, 0xef, 0xbc, 0x72, 0x75, 0x6f, 0x37, 0xa1,
  0xec, 0xd3, 0x8e, 0x62, 0x8b, 0x86, 0x10, 0xe8,
  0x08, 0x77, 0x11, 0xbe, 0x92, 0x4f, 0x24, 0xc5,
  0x32, 0x36, 0x9d, 0xcf, 0xf3, 0xa6, 0xbb, 0xac,
  0x5e, 0x6c, 0xa9, 0x13, 0x57, 0x25, 0xb5, 0xe3,
  0xbd, 0xa8, 0x3a, 0x01, 0x05, 0x59, 0x2a, 0x46
};

#define PRIME_MULT 1717

static int hashes_maprime2c (lua_State *L) {
  size_t l, i;
  ub4 h, n;
  const char *s = agn_checklstring(L, 1, &l);
  n = agnL_optuint32_t(L, 2, 0);
  h = agnL_optnonnegint(L, 3, l);
  for (i=0; i != l; i++, s++) {
    h ^= maprime2c_tab[(uchar(*s) + i) & 0xff];
    h *= PRIME_MULT;
  }
  lua_pushnumber(L, (n != 0) ? h % n : h);
  return 1;
}


/* Takes two unsigned 32-bit integers, represented by standard Agena numbers, and returns the unsigned 32-bit
   and 16-bit integer fingerprints. 7.1.9 */
static int hashes_finger (lua_State *L) {
  uint64_t h, x;
  uint16_t fp16;
  ub4 hx, lx, n, fp32;
  hx = agnL_optuint32_t(L, 1, 0);
  lx = agnL_optuint32_t(L, 2, 0);
  n = agnL_optuint32_t(L, 3, 0);
  h = tools_twouint32touint64(hx, lx);
  x = h ^ (h >> 32);
  fp16 = (uint16_t)x;
  fp32 = (uint32_t)x;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushnumber(L, (n == 0) ? (lua_Number)fp32 : (lua_Number)(fp32 % n));
  lua_pushnumber(L, (n == 0) ? (lua_Number)fp16 : (lua_Number)(fp16 % n));
  return 2;
}


/* `hashes.wy` computes the WyHash, one of the fastest hashes in the world (used in the Go language runtime).
   It uses a "mum-line" (multiply and mix) strategy. It is exceptionally good at maintaining randomness even
   in the low-order bits.

   The function returns the higher word (hx) and the lower word (lx) of the WyHash plus the XORed combination
   hx ^^ lx, in this order.

   Created by Gemini AI, 7.1.9 */

/* Helper to perform 64x64 -> 128-bit multiply and return XORed results */
static uint64_t wy_mum (uint64_t a, uint64_t b) {
  uint64_t au, al, bu, bl, t, k, w1, w2, hi, lo;
  au = a >> 32; al = a & 0xFFFFFFFFU;
  bu = b >> 32; bl = b & 0xFFFFFFFFU;
  t = al*bl;
  k = t >> 32;
  t = au*bl + k;
  w1 = t & 0xFFFFFFFFU;
  w2 = t >> 32;
  t = al*bu + w1;
  k = t >> 32;
  /* High 64 bits and Low 64 bits */
  hi = au*bu + w2 + k;
  lo = a*b;
  return hi ^ lo;
}

static uint64_t wyhash64 (double val) {
  uint64_t key, secret;
  val += 0.0;
  memcpy(&key, &val, sizeof(key));
  /* WyHash v4 constants */
  secret = 0xa0761d6478bd642fULL;
  return wy_mum(key ^ secret, key ^ 0xe7037ed1a0b428dbULL);
}

static int hashes_wy (lua_State *L) {
  uint64_t h;
  ub4 n, hx, lx, cx;
  lua_Number x = agn_checknumber(L, 1);
  n = agnL_optuint32_t(L, 2, 0);
  h = wyhash64(x);
  hx = (uint32_t)(h >> 32);          /* high */
  lx = (uint32_t)(h & 0xFFFFFFFFU);  /* low */
  cx = hx ^ lx;  /* high and low combined, equal to (uint32_t)(h ^ (h >> 32)) */
  luaL_checkstack(L, 3, "not enough stack space");
  if (n == 0) {
    /* return raw 32-bit unsigned integers */
    lua_pushnumber(L, (lua_Number)hx);
    lua_pushnumber(L, (lua_Number)lx);
    lua_pushnumber(L, (lua_Number)cx);
  } else {
    /* return values constrained to [0, n-1] */
    lua_pushnumber(L, (lua_Number)(hx % n));
    lua_pushnumber(L, (lua_Number)(lx % n));
    lua_pushnumber(L, (lua_Number)(cx % n));
  }
  return 3;
}


static const luaL_Reg hashes[] = {
  {"adler32", hashes_adler32},    /* November 03, 2019 */
  {"asu", hashes_asu},            /* March 30, 2019 */
  {"bkdr", hashes_bkdr},          /* March 30, 2019 */
  {"bp", hashes_bp},              /* July 04, 2019 */
  {"bsd", hashes_bsd},            /* February 23, 2017 */
  {"ccitt", hashes_ccitt},        /* December 31, 2024 */
  {"ccitu", hashes_ccitu},        /* January 07, 2025 */
  {"cksum", hashes_cksum},        /* February 23, 2017 */
  {"crc7", hashes_crc7},          /* December 31, 2024 */
  {"crc8", hashes_crc8},          /* October 05, 2019 */
  {"crc16", hashes_crc16},        /* May 11, 2018 */
  {"crc32", hashes_crc32},        /* November 13, 2017 */
  {"damm", hashes_damm},          /* February 23, 2017 */
  {"dek", hashes_dek},            /* March 30, 2019 */
  {"derpy", hashes_derpy},        /* April 05, 2023 */
  {"droot", hashes_droot},        /* November 13, 2017 */
  {"djb", hashes_djb},            /* January 09, 2011 */
  {"djb2", hashes_djb2},          /* January 09, 2011 */
  {"djb2rot", hashes_djb2rot},    /* March 25, 2019 */
  {"elf", hashes_elf},            /* March 30, 2019 */
  {"fibmod", hashes_fibmod},      /* April 05, 2019 */
  {"fibmod2", hashes_fibmod2},    /* April 05, 2023 */
  {"finger", hashes_finger},      /* March 18, 2026 */
  {"fletcher", hashes_fletcher},  /* February 23, 2017 */
  {"fnv", hashes_fnv},            /* January 09, 2011 */
  {"ftok", hashes_ftok},          /* January 20, 2023 */
  {"hashfloat", hashes_hashfloat},/* January 04, 2026 */
  {"hashmap", hashes_hashmap},    /* October 18, 2019 */
  {"internet", hashes_internet},  /* June 19, 2024 */
  {"ispell", hashes_ispell},      /* January 29, 2024 */
  {"interweave", hashes_interweave},  /* February 14, 2020 */
  {"jen", hashes_jen},            /* January 09, 2011 */
  {"j32to32", hashes_j32to32},    /* July 29, 2020 */
  {"jinteger", hashes_jinteger},  /* July 29, 2020 */
  {"jnumber", hashes_jnumber},    /* August 20, 2022 */
  {"lua", hashes_lua},            /* December 04, 2017 */
  {"luhn", hashes_luhn},          /* February 22, 2017 */
  {"mix64", hashes_mix64},        /* February 14, 2017 */
  {"mix64to32", hashes_mix64to32},  /* February 14, 2017 */
  {"maprime2c", hashes_maprime2c},  /* January 07, 2025 */
  {"md5", hashes_md5},            /* October 10, 2014 */
  {"mix", hashes_mix},            /* March 05, 2017 */
  {"murmur1", hashes_murmur1},    /* November 21, 2025 */
  {"murmur2", hashes_murmur2},    /* June 13, 2018 */
  {"murmur3", hashes_murmur3},    /* June 16, 2018 */
  {"murmur3128", hashes_murmur3128},  /* June 16, 2018 */
  {"murmur3128x86", hashes_murmur3128x86},  /* November 21, 2025 */
  {"numlua", hashes_numlua},      /* February 23, 2023 */
  {"oaat", hashes_oaat},          /* October 26, 2014 */
  {"parity", hashes_parity},      /* November 15, 2017 */
  {"pjw", hashes_pjw},            /* July 04, 2019 */
  {"pl", hashes_pl},              /* January 09, 2011 */
  {"raw", hashes_raw},            /* January 06, 2008 */
  {"reflect", hashes_reflect},    /* November 13, 2017 */
  {"roaat", hashes_roaat},        /* April 05, 2022 */
  {"rs", hashes_rs},              /* July 04, 2019 */
  {"sax", hashes_sax},            /* January 09, 2011 */
  {"sdbm", hashes_sdbm},          /* October 26, 2014 */
  {"sha256", hashes_sha256},      /* December 31, 2023 */
  {"sha512", hashes_sha512},      /* August 04, 2018 */
  {"squirrel32", hashes_squirrel32},  /* April 04, 2023 */
  {"squirrel64", hashes_squirrel64},  /* April 04, 2023 */
  {"sth", hashes_sth},            /* January 09, 2011 */
  {"strval", hashes_strval},      /* March 26, 2019 */
  {"sumupchars", hashes_sumupchars},  /* February 23, 2019 */
  {"superfast", hashes_superfast},  /* January 29, 2024 */
  {"sysv", hashes_sysv},          /* May 09, 2019 */
  {"varlen", hashes_varlen},      /* February 22, 2019 */
  {"verhoeff", hashes_verhoeff},  /* February 23, 2017 */
  {"wang", hashes_wang},          /* August 14, 2025 */
  {"wy", hashes_wy},              /* March 19, 2026 */
  {NULL, NULL}
};


/*
** Open hashes library
*/
LUALIB_API int luaopen_hashes (lua_State *L) {
  aux_crcInit();
  luaL_register(L, AGENA_HASHESLIBNAME, hashes);
  lua_rawsetstringstring(L, -1, "initstring", AGENA_LIBVERSION);
  return 1;
}

/* ====================================================================== */

