/*
** $Id: utils.c, v 1.0.1 by Alexander Walz - Initiated: October 17, 2006
   Revision 08.08.2009 22:13:15
** See Copyright Notice in agena.h
*/

#include <assert.h>  /* assert() */
#include <limits.h>  /* CHAR_BIT */
#include <stddef.h>  /* size_t */
#include <stdlib.h>

/* instead of time.h: */
#include "agnt64.h"

/* the following package ini declarations must be included after `#include <` and before `include #` ! */

#define utils_c
#define LUA_LIB

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"
#include "agnconf.h"  /* for uchar */

#include "lseq.h"     /* for agnSeq_bestsize */
#include "rfc3339.h"
#include "utils.h"    /* typedefs and checkarray macro */

static int utils_singlesubs (lua_State *L) {  /* Agena 1.5.0 */
  int rc;
  size_t i, l;
  unsigned char token;
  luaL_Buffer b;
  const char *s = luaL_checklstring(L, 1, &l);
  luaL_buffinit(L, &b);
  if (lua_istable(L, 2)) {  /* new 6.1.0 */
    for (i=0; i < l; i++) {
      lua_rawgeti(L, 2, uchar(s[i]));
      if (agn_isinteger(L, -1)) {
        luaL_addchar(&b, agn_tointeger(L, -1));
      } else if (agn_isstring(L, -1)) {
        size_t len;
        const char *str = lua_tolstring(L, -1, &len);
        luaL_addlstring(&b, str, len);
      } else {
        luaL_addchar(&b, uchar(s[i]));
      }
      agn_poptop(L);
    }
  } else if (lua_isseq(L, 2)) {
    for (i=0; i < l; i++) {
      token = agn_seqrawgetiinteger(L, 2, uchar(s[i]) + 1, &rc);
      luaL_addchar(&b, rc ? token : uchar(s[i]));
    }
  } else if (lua_isreg(L, 2)) {  /* new 6.1.0 */
    for (i=0; i < l; i++) {
      token = agn_regrawgetiinteger(L, 2, uchar(s[i]) + 1, &rc);
      luaL_addchar(&b, rc ? token : uchar(s[i]));
    }
  } else
    luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.", "utils.singlesubs",
                  luaL_typename(L, 2));
  luaL_pushresult(&b);
  return 1;
}

/*
----------------------------------------------------------------------------
  Internal utilities
----------------------------------------------------------------------------
*/

static void setfield (lua_State *L, const char *key, int value) {
  lua_pushinteger(L, value);
  agn_rawsetfield(L, -2, key);
}


static void setboolfield (lua_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  lua_pushboolean(L, value);
  agn_rawsetfield(L, -2, key);
}

/*
----------------------------------------------------------------------------
  Other functions
----------------------------------------------------------------------------
*/

static int utils_calendar (lua_State *L) {
  Time64_T seconds = luaL_opt(L, (time_t)agn_checknumber, 1, time(NULL));
  struct TM *stm;
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
  lua_createtable(L, 0, 9);  /* 9 = number of fields */
  if (seconds < 0) {  /* 0.31.3 patch */
    agn_poptop(L);
    lua_pushfail(L);
    return 1;
  }
  stm = localtime64(&seconds);
  if (stm != NULL) {
    setfield(L, "sec", stm->tm_sec);
    setfield(L, "min", stm->tm_min);
    setfield(L, "hour", stm->tm_hour);
    setfield(L, "day", stm->tm_mday);
    setfield(L, "month", stm->tm_mon + 1);
    setfield(L, "year", stm->tm_year + 1900);
    setfield(L, "wday", stm->tm_wday + 1);
    setfield(L, "yday", stm->tm_yday + 1);
    setboolfield(L, "DST", stm->tm_isdst);
  } else
    lua_pushfail(L);
  return 1;
}


/* ******************************************************************
LICENCE:        Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.

                Permission is hereby granted, free of charge, to any person
                obtaining a copy of this software and associated
                documentation files (the "Software"), to deal in the
                Software without restriction, including without limitation
                the rights to use, copy, modify, merge, publish, distribute,
                sublicense, and/or sell copies of the Software, and to
                permit persons to whom the Software is furnished to do so,
                subject to the following conditions:

                The above copyright notice and this permission notice shall
                be included in all copies or substantial portions of the
                Software.

                THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
                KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
                WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
                PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
                OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
                OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
                OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
                SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

VERSION HISTORY:
                Bob Trower 08/04/01 -- Create Version 0.00.00B

******************************************************************** */

/*
** Translation Table as described in RFC1113, see: https://en.wikipedia.org/wiki/Base64
*/
/* static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; */

/*
** Translation Table to decode (created by author)
*/
static const char cd64[] = "|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

/*
** encodeblock
**
** encode 3 8-bit binary bytes as 4 '6-bit' characters
*/
void encodeblock (unsigned char in[3], unsigned char out[4], int len) {
  out[0] = tools_cb64[ in[0] >> 2 ];
  out[1] = tools_cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
  out[2] = (unsigned char) (len > 1 ? tools_cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
  out[3] = (unsigned char) (len > 2 ? tools_cb64[ in[2] & 0x3f ] : '=');
}


/*
** encode
**
** base64 encode a stream adding padding and line breaks as per spec.
*/

static int utils_encodeb64 (lua_State *L) {
  luaL_Buffer b;
  unsigned char in[3], out[4];
  size_t linesize;
  int i, len, blocksout = 0;
  const char *str = agn_checkstring(L, 1);
  linesize = agnL_optnumber(L, 2, 72);
  luaL_buffinit(L, &b);
  while (*str) {
    len = 0;
    for (i = 0; i < 3; i++) {
      in[i] = (unsigned char) *str;
      if (*str)
        len++;
      else {
        in[i] = 0;
        break;
      }
      str++;
    }
    if (len) {
      encodeblock(in, out, len);
      for (i = 0; i < 4; i++) {
        luaL_addchar(&b, out[i]);
      }
      blocksout++;
    }
    if (blocksout >= (linesize/4) || *str == '\0') {
      if (blocksout) {
        /* luaL_addchar(&b, '\r'); */  /* Linux base64 only works in ignore mode when encountering CRs */
        luaL_addchar(&b, '\n');
      }
      blocksout = 0;
    }
  }
  luaL_pushresult(&b);
  return 1;
}

/*
** decodeblock
**
** decode 4 '6-bit' characters into 3 8-bit binary bytes
*/
void decodeblock (unsigned char in[4], unsigned char out[3]) {
  out[0] = uchar(in[0] << 2 | in[1] >> 4);
  out[1] = uchar(in[1] << 4 | in[2] >> 2);
  out[2] = uchar(((in[2] << 6) & 0xc0) | in[3]);
}

/*
** decode
**
** decode a base64 encoded stream discarding padding, line breaks and noise
*/
static int utils_decodeb64 (lua_State *L) {
  luaL_Buffer b;
  unsigned char in[4], out[3], v;
  int i, len;
  const char *str = agn_checkstring(L, 1);
  luaL_buffinit(L, &b);
  while (*str) {
    for (len=0, i = 0; i < 4 && *str; i++) {
      v = 0;
      while (*str && v == 0) {
        v = uchar(*str++);
        v = uchar((v < 43 || v > 122) ? 0 : cd64[v - 43]);
        if (v) {
          v = uchar((v == '$') ? 0 : v - 61);
        }
      }
      if (*str) {
        len++;
        if (v) {
          in[i] = uchar(v - 1);
        }
      }
      else {
        in[i] = 0;
        break;
      }
    }
    if (len) {
      decodeblock(in, out);
      for (i = 0; i < len - 1; i++) {
        luaL_addchar(&b, out[i]);
      }
    }
  }
  luaL_pushresult(&b);
  return 1;
}


/* Base85 implementation.
   Code written by Rafa García, Copyright (c) 2016-2018, MIT licence, taken from:
   https://github.com/rafagafe/base85

   To check the result, use https://dencode.com/en/string/ascii85.

   Adapted to Base 85 Z85 by A. Walz */

/* Lookup table to convert a binary number in a base 85 digit. */
static char const bintodigit[] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '!', '#', '$', '%', '&', '(', ')', '*', '+', '-', ';',
  '<', '=', '>', '?', '@', '^', '_', '`', '{', '|', '}', '~'
};

static char const bintodigitz85[] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '.', '-', ':', '+', '=', '^', '!', '/', '*', '?', '&',
  '<', '>', '(', ')', '[', ']', '{', '}', '@', '%', '$', '#'
};

/* Escape values. */
enum escape {
  notadigit = 85u  /* Return value when a non-digit-base-85 is found. */
};

/** Lookup table to convert a base 85 digit in a binary number. */
static unsigned char const digittobin[] = {
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 62, 85, 63, 64, 65, 66, 85, 67, 68, 69, 70, 85, 71, 85, 85,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 85, 72, 73, 74, 75, 76,
  77, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
  25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 85, 85, 85, 78, 79,
  80, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
  51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 81, 82, 83, 84, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
};

/*
bintodigit := [
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '.', '-', ':', '+', '=', '^', '!', '/', '*', '?', '&',
  '<', '>', '(', ')', '[', ']', '{', '}', '@', '%', '$', '#'
];

scope
printf("  ")
for i from 1 to 256 do
   if char(i - 1) notin bintodigit then
      printf("%2d, ", 85)
   else
      pos := (char(i - 1) in join(bintodigit)) - 1
      printf("%2d, ", pos)
   end;
   if i % 16 = 0 then
      printf("\n  ")
   fi
end;
epocs
*/

static unsigned char const digittobinz85[] = {
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 68, 85, 84, 83, 82, 72, 85, 75, 76, 70, 65, 85, 63, 62, 69,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 64, 85, 73, 66, 74, 71,
  81, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
  51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 77, 85, 78, 67, 85,
  85, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
  25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 79, 85, 80, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
  85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85
};

/* Some powers of 85. */
#define p850 1ul            /* < 85^0 */
#define p851 85ul           /* < 85^1 */
#define p852 (p851 * p851)  /* < 85^2 */
#define p853 (p851 * p852)  /* < 85^3 */
#define p854 (p851 * p853)  /* < 85^4 */

/** Powers of 85 list. */
static unsigned long const pow85[] = { p854, p853, p852, p851, p850 };

/** Converts a integer of 4 bytes in 5 digits of base 85.
  * @param dest Memory block where to put the 5 digits of base 85.
  * @param value Value of the integer of 4 bytes. */
static void ultob85 (char* dest, unsigned int long value, int z85) {
  unsigned int digitsQty, i, bin;
  digitsQty = sizeof(pow85)/sizeof(*pow85);
  for (i=0; i < digitsQty; i++) {
    bin = value/pow85[i];
    dest[i] = z85 ? bintodigitz85[bin] : bintodigit[bin];
    value -= bin*pow85[i];
  }
}

/* Convert a big-endian array of  bytes in a unsigned long.
 * @param src Pointer to array of bytes.
 * @param sz Size in bytes of array from 0 until 4.
 * @return  The unsigned long value. */
static unsigned long betoul (void const *src, size_t sz) {
  size_t i, j;
  unsigned long value = 0;
  char* d = (char*)&value;
  char const* const s = (char const*)src;
  for (i = 0; i < sz; ++i) {
#if BYTE_ORDER == BIG_ENDIAN
    d[i] = s[i];
#else
    d[3 - i] = s[i];
#endif
    for (j=sz; j < 4; ++j) {
#if BYTE_ORDER == BIG_ENDIAN
      d[j] = 0;
#else
      d[3 - j] = 0;
#endif
    }
  }
  return value;
}

static char *bintob85 (char *dest, void const *src, size_t size, int z85) {
  size_t i, remainder;
  char *rslt;
  size_t quartets = size/4;
  char const *s = (char*)src + 4*quartets;
  dest += 5*quartets;
  remainder = size % 4;
  if (remainder)
    ultob85(dest, betoul(s, remainder), z85);
  rslt = dest + 5*(remainder != 0);
  *rslt = '\0';
  for (i=0; i < quartets; i++)
    ultob85(dest -= 5, betoul(s -= 4, 4), z85);
  return rslt;
}

/** Copy a unsigned long in a big-endian array of 4 bytes.
  * @param dest Destination memory block.
  * @param value Value to copy.
  * @return  dest + 4 */
static void *ultobe (void *dest, unsigned long value) {
  int i;
  char* const d = (char*)dest;
  char const* const s = (char*)&value;
  for (i=0; i < 4; ++i)
#if BYTE_ORDER == BIG_ENDIAN
    d[i] = s[i];
#else
    d[i] = s[3 - i];
#endif
  return d + 4;
}

/* Convert a base85 string to binary format. src must be non-empty ! */
static void *b85tobin (void *dest, char const *src, int z85) {
  int i;
  unsigned char const *s;
  for (s = (unsigned char const*)src; ; ) {
    unsigned long value = 0;
    for (i=0; i < sizeof(pow85)/sizeof(*pow85); ++i, ++s) {
      unsigned int const bin = z85 ? digittobinz85[uchar(*s)] : digittobin[uchar(*s)];
      if (bin == notadigit) {  /* detects invalid input characters, as well */
        return i == 0 ? dest : NULL;  /* patch: return NULL instead of 0 to avoid crashes */
      }
      value += bin*pow85[i];
    }
    dest = ultobe(dest, value);
  }
}

#define aux_bintob85size(l) (5*(l/4) + 5*(l % 4 != 0))
#define err_encode(z85) ((z85) ? "utils.encodez85" : "utils.encodeb85")
static int aux_encodeb85 (lua_State *L, int z85) {  /* 2.27.2 */
  size_t l, b85l;
  char *dest, *end;
  const char *src = agn_checklstring(L, 1, &l);
  if (l == 0) {  /* 5.5.8 change */
    lua_pushliteral(L, "");
    return 1;
  }
  b85l = aux_bintob85size(l);
  dest = (char *)tools_malloc(b85l*CHARSIZE, 1);
  if (!dest)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", err_encode(z85));
  end = bintob85(dest, src, l, z85);
  if (end == NULL || end - dest != b85l) {  /* 5.5.8 fix */
    xfree(dest);
    luaL_error(L, "Error in " LUA_QS ": conversion failed.", err_encode(z85));
  }
  while (*end == '\0') end--;
  lua_pushlstring(L, dest, end - dest + 1);
  xfree(dest);
  return 1;
}

#define err_decode(z85) ((z85) ? "utils.decodez85" : "utils.decodeb85")
static int aux_decodeb85 (lua_State *L, int z85) {  /* 2.27.2 */
  int checkinput;
  size_t i, l, binl;
  char *dest, *end;
  const char *src = agn_checklstring(L, 1, &l);
  if (l == 0) {  /* 5.5.8 change */
    lua_pushliteral(L, "");
    return 1;
  }
  checkinput = agnL_optboolean(L, 2, 0);
  for (i=0; checkinput && i < l; i++) {
    unsigned char c = uchar(src[i]);  /* 5.5.8 change */
    /* 0-9, a-z, A-Z, .-:+=^!/, *?&<>()[]{}@%, and $# */
    unsigned int const bin = z85 ? digittobinz85[c] : digittobin[c];
    if (bin == notadigit) {
      unsigned char chr[2];
      chr[0] = c;
      chr[1] = '\0';
      luaL_error(L, "Error in " LUA_QS ": input contains invalid character " LUA_QS ".",
        err_decode(z85), chr);
    }
  }
  binl = (4*(l - 1))/5 + 1;
  dest = (char *)tools_malloc(binl*CHARSIZE, 1);
  if (!dest)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", err_decode(z85));
  end = b85tobin(dest, src, z85);  /* b85tobin does not allocate memory */
  if (end == NULL) {
    xfree(dest);
    if (l % 5)
      luaL_error(L, "Error in " LUA_QS ": Base85 string size not a multiple of 5.", err_decode(z85));
    else
      luaL_error(L, "Error in " LUA_QS ": Base85 string is malformed.", err_decode(z85));
  }
  if (end - dest != binl) {
    xfree(dest);
    luaL_error(L, "Error in " LUA_QS ": invalid input, recall with `true` option for a clue.",
      err_decode(z85));
  }
  while (*end == 0) end--;
  lua_pushlstring(L, dest, end - dest + 1);
  xfree(dest);
  return 1;
}

static int utils_decodeb85 (lua_State *L) {  /* 5.5.8 */
  return aux_decodeb85(L, 0);
}

static int utils_encodeb85 (lua_State *L) {  /* 5.5.8 */
  return aux_encodeb85(L, 0);
}

static int utils_decodez85 (lua_State *L) {  /* 5.5.8 */
  return aux_decodeb85(L, 1);
}

static int utils_encodez85 (lua_State *L) {  /* 5.5.8 */
  return aux_encodeb85(L, 1);
}


/*
* lascii85.c
* ascii85 encoding and decoding for Lua 5.1
* Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br>
* 23 Mar 2010 22:25:18
* This code is hereby placed in the public domain.
*/

#define uint uint32_t

static void encode (luaL_Buffer *b, uint c1, uint c2, uint c3, uint c4, int n) {
  unsigned long tuple = c4 + 256UL*(c3 + 256UL*(c2 + 256UL*c1));
  if (tuple == 0 && n == 4)
    luaL_addlstring(b, "z", 1);
  else {
    int i;
    char s[5];
    for (i=0; i < 5; i++) {
      s[4 - i] = '!' + (tuple % 85);
      tuple /= 85;
    }
    luaL_addlstring(b, s, n + 1);
  }
}

static int utils_encodea85 (lua_State *L) {
  size_t l;
  const unsigned char *s = (const unsigned char*)luaL_checklstring(L, 1, &l);
  luaL_Buffer b;
  int n;
  luaL_buffinit(L, &b);
  luaL_addlstring(&b, "<~", 2);
  for (n=l/4; n--; s += 4) encode(&b, s[0], s[1], s[2], s[3], 4);
  switch (l % 4) {
    case 1: encode(&b, s[0], 0, 0, 0, 1);       break;
    case 2: encode(&b, s[0], s[1], 0, 0, 2);    break;
    case 3: encode(&b, s[0], s[1], s[2], 0, 3); break;
  }
  luaL_addlstring(&b, "~>", 2);
  luaL_pushresult(&b);
 return 1;
}

static void decode (luaL_Buffer *b, int c1, int c2, int c3, int c4, int c5, int n) {
  unsigned long tuple = c5 + 85L*(c4 + 85L*(c3 + 85L*(c2 + 85L*c1)));
  /* equals:
     n := 0;
     for i to 5 do
        n := n + c[i] * base**(5 - i)
     od;
     where c[1] is the highest-order coefficient */
  char s[4];
  switch (--n) {
    case 4: s[3] = tuple;
    case 3: s[2] = tuple >> 8;
    case 2: s[1] = tuple >> 16;
    case 1: s[0] = tuple >> 24;
   }
  luaL_addlstring(b, s, n);
}

static int utils_decodea85 (lua_State *L) {
  size_t l, r_len;
  const char *s;
  luaL_Buffer b;
  int n = 0;
  char t[5];
  s = luaL_checklstring(L, 1, &l);
  s = (const char *)tools_between(s, l, "<~", 2, "~>", 2, &r_len);
  if (!r_len) {
    lua_pushstring(L, "");
    return 1;
  }
  luaL_buffinit(L, &b);
  while (r_len--) {
    int c = *s++;
    switch (c) {
      case 'z':
        luaL_addlstring(&b, "\0\0\0\0", 4);
        break;
      case '\n': case '\r': case '\t': case ' ':
      case '\0': case '\f': case '\b': case 0177:
        break;
      default: {
        if (c < '!' || c > 'u') {
          luaL_error(L, "Error in " LUA_QS ": %c is not allowed in ASCII85 strings.", "utils.decodea85", c);
          return 0;
        }
        t[n++] = c - '!';
        if (n == 5) {
          decode(&b, t[0], t[1], t[2], t[3], t[4], 5);
          n = 0;
        }
        break;
      }
    }
  }
  switch (n) {
    case 1: decode(&b, t[0], 85, 0, 0, 0, 1);          break;
    case 2: decode(&b, t[0], t[1], 85, 0, 0, 2);       break;
    case 3: decode(&b, t[0], t[1], t[2], 85, 0, 3);    break;
    case 4: decode(&b, t[0], t[1], t[2], t[3], 85, 4); break;
  }
  luaL_pushresult(&b);
  return 1;
}


/* Converts a string str to its hexadecimal representation and returns a new string where each character in str is replaced
   by a two-digit hexadecimal value. The resulting string is twice as long as str. See also: `utils.unhexlify`. */
static int utils_hexlify (lua_State *L) {  /* 2.16.3 */
  char *buffer, *b;
  const char *str;
  size_t l, i;
  int byte;
  str = agn_checklstring(L, 1, &l);
  if (l == 0)
    luaL_error(L, "Error in " LUA_QS ": string must be nonempty.", "utils.hexlify");
  buffer = agn_stralloc(L, 2*l, "utils.hexlify", NULL);  /* 2.16.5 change */
  b = buffer;
  for (i=0; i < l; i++) {
    byte = uchar((str[i] >> 4) & 0x0f);
    *buffer++ = uchar(byte + (byte > 9 ? 'a' - 10 : '0'));
    byte = uchar(str[i] & 0x0f);
    *buffer++ = uchar(byte + (byte > 9 ? 'a' - 10 : '0'));
  }
  buffer = 0;
  lua_pushlstring(L, b, 2*l);  /* do not use pushstring */
  xfree(b);  /* buffer points to end of string, so do not use it ! */
  return 1;
}


/**
 * base32 (de)coder implementation as specified by RFC4648.
 *
 * Copyright (c) 2010 Adrien Kunysz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **/

/**
 * Returns the length of the output buffer required to encode len bytes of
 * data into base32. This is a macro to allow users to define buffer size at
 * compilation time.
 */
#define BASE32_LEN(len)  (((len)/5)*8 + ((len) % 5 ? 8 : 0))

/**
 * Returns the length of the output buffer required to decode a base32 string
 * of len characters. Please note that len must be a multiple of 8 as per
 * definition of a base32 string. This is a macro to allow users to define
 * buffer size at compilation time.
 */
#define UNBASE32_LEN(len)  (((len)/8)*5)

/**
 * Encode the data pointed to by plain into base32 and store the
 * result at the address pointed to by coded. The "coded" argument
 * must point to a location that has enough available space
 * to store the whole coded string. The resulting string will only
 * contain characters from the [A-Z2-7=] set. The "len" arguments
 * define how many bytes will be read from the "plain" buffer.
 **/
/* void base32_encode (const unsigned char *plain, size_t len, unsigned char *coded); */

/**
 * Decode the null terminated string pointed to by coded and write
 * the decoded data into the location pointed to by plain. The
 * "plain" argument must point to a location that has enough available
 * space to store the whole decoded string.
 * Returns the length of the decoded string. This may be less than
 * expected due to padding. If an invalid base32 character is found
 * in the coded string, decoding will stop at that point.
 **/
/* size_t base32_decode (const unsigned char *coded, unsigned char *plain); */

/**
 * Let this be a sequence of plain data before encoding:
 *
 *  01234567 01234567 01234567 01234567 01234567
 * +--------+--------+--------+--------+--------+
 * |< 0 >< 1| >< 2 ><|.3 >< 4.|>< 5 ><.|6 >< 7 >|
 * +--------+--------+--------+--------+--------+
 *
 * There are 5 octets of 8 bits each in each sequence.
 * There are 8 blocks of 5 bits each in each sequence.
 *
 * You probably want to refer to that graph when reading the algorithms in this
 * file. We use "octet" instead of "byte" intentionnaly as we really work with
 * 8 bits quantities. This implementation will probably not work properly on
 * systems that don't have exactly 8 bits per (unsigned) char.
 **/

#define MIN(x,y)  (x < y ? x : y)

static const unsigned char PADDING_CHAR = '=';

/* Pad the given buffer with len padding characters. */
static void pad (unsigned char *buf, int len) {
  int i;
  for (i=0; i < len; i++) buf[i] = PADDING_CHAR;
}

/* This convert a 5 bits value into a base32 character.
 * Only the 5 least significant bits are used. */
static unsigned char encode_char (unsigned char c) {
  static unsigned char base32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  return base32[c & 0x1F];  /* 0001 1111 */
}

/* Decode given character into a 5 bits value.
 * Returns -1 iff the argument given was an invalid base32 character
 * or a padding character. */
static int decode_char (unsigned char c) {
  int retval = -1;
  if (c >= 'A' && c <= 'Z')
    retval = c - 'A';
  if (c >= '2' && c <= '7')
    retval = c - '2' + 26;
  assert(retval == -1 || ((retval & 0x1F) == retval));
  return retval;
}

/**
 * Given a block id between 0 and 7 inclusive, this will return the index of
 * the octet in which this block starts. For example, given 3 it will return 1
 * because block 3 starts in octet 1:
 *
 * +--------+--------+
 * | ......<|.3 >....|
 * +--------+--------+
 *  octet 1 | octet 2
 */
#define GET_OCTET(block)  (((block)*5)/8)

/**
 * Given a block id between 0 and 7 inclusive, this will return how many bits
 * we can drop at the end of the octet in which this block starts.
 * For example, given block 0 it will return 3 because there are 3 bits
 * we don't care about at the end:
 *
 *  +--------+-
 *  |< 0 >...|
 *  +--------+-
 *
 * Given block 1, it will return -2 because there
 * are actually two bits missing to have a complete block:
 *
 *  +--------+-
 *  |.....< 1|..
 *  +--------+-
 **/
#define GET_OFFSET(block)   (8 - 5 - (5*(block)) % 8)

/**
 * Like "b >> offset" but it will do the right thing with negative offset.
 * We need this as bitwise shifting by a negative offset is undefined
 * behavior.
 */
static unsigned char shift_right (unsigned char byte, signed char offset) {
  return (offset > 0) ? byte >> offset : byte << -offset;
}

#define SHIFT_LEFT(byte,offset)   (shift_right((byte), -(offset)))

/**
 * Encode a sequence. A sequence is no longer than 5 octets by definition.
 * Thus passing a length greater than 5 to this function is an error. Encoding
 * sequences shorter than 5 octets is supported and padding will be added to the
 * output as per the specification.
 */
static void encode_sequence (const unsigned char *plain, int len, unsigned char *coded) {
  int block;
  assert(CHAR_BIT == 8);  /* not sure this would work otherwise */
  assert(len >= 0 && len <= 5);
  for (block=0; block < 8; block++) {
    int octet = GET_OCTET(block);  /* figure out which octet this block starts in */
    int junk = GET_OFFSET(block);  /* how many bits do we drop from this octet? */
    if (octet >= len) { /* we hit the end of the buffer */
      pad(&coded[block], 8 - block);
      return;
    }
    unsigned char c = shift_right(plain[octet], junk);  /* first part */
    if (junk < 0  /* is there a second part? */
      &&  octet < len - 1) {  /* is there still something to read? */
      c |= shift_right(plain[octet + 1], 8 + junk);
    }
    coded[block] = encode_char(c);
  }
}

static void base32_encode (const unsigned char *plain, size_t len, unsigned char *coded) {
  size_t i, j;
  /* All the hard work is done in encode_sequence(),
     here we just need to feed it the data sequence by sequence. */
  for (i=0, j = 0; i < len; i += 5, j += 8) {
    encode_sequence(&plain[i], MIN(len - i, 5), &coded[j]);
  }
}

static int decode_sequence (const unsigned char *coded, unsigned char *plain) {
  int block;
  assert(CHAR_BIT == 8);
  assert(coded && plain);
  plain[0] = 0;
  for (block=0; block < 8; block++) {
    int offset = GET_OFFSET(block);
    int octet = GET_OCTET(block);
    int c = decode_char(coded[block]);
    if (c < 0) return octet;  /* invalid char, stop here */
    plain[octet] |= SHIFT_LEFT(c, offset);
    if (offset < 0) {  /* does this block overflows to next octet? */
      assert(octet < 4);
      plain[octet + 1] = SHIFT_LEFT(c, 8 + offset);
    }
  }
  return 5;
}

static size_t base32_decode (const unsigned char *coded, unsigned char *plain) {
  size_t i, j, written;
  written = 0;
  for (i=0, j = 0; ; i += 8, j += 5) {
    int n = decode_sequence(&coded[i], &plain[j]);
    written += n;
    if (n < 5) return written;
  }
}

static int utils_encodeb32 (lua_State *L) {  /* 3.10.3 */
  size_t l;
  const char *str = agn_checklstring(L, 1, &l);
  unsigned char *coded =
    (unsigned char *)agn_malloc(L, BASE32_LEN(l)*sizeof(unsigned char), "utils.encodeb32", NULL);  /* 4.11.5 fix */
  base32_encode((unsigned char *)str, l, coded);
  lua_pushlstring(L, (const char *)coded, BASE32_LEN(l));
  xfree(coded);
  return 1;
}


static int utils_decodeb32 (lua_State *L) {  /* 3.10.3 */
  size_t i, l;
  int checkinput;
  const char *str = agn_checklstring(L, 1, &l);
  checkinput = agnL_optboolean(L, 2, 0);  /* 5.5.8 */
  for (i=0; checkinput && i < l; i++) {
    char c = str[i];
    char check = (c >= '2' && c <= '7') ||
                 (c >= 'a' && c <= 'z') ||
                 (c >= 'A' && c <= 'Z') ||
                 (c == '=');
    if (!check) {
      char chr[2];
      chr[0] = c;
      chr[1] = '\0';
      luaL_error(L, "Error in " LUA_QS ": input contains invalid character " LUA_QS ".", "utils.decodeb32", chr);
    }
  }
  l = tools_nextmultiple(l, 8);
  unsigned char *uncoded =
    (unsigned char *)agn_malloc(L, UNBASE32_LEN(l)*sizeof(unsigned char), "utils.decodeb32", NULL);  /* 4.11.5 fix */
  size_t outl = base32_decode((unsigned char *)str, uncoded);
  lua_pushlstring(L, (const char *)uncoded, outl);
  xfree(uncoded);
  return 1;
}


/* Taken from: https://stackoverflow.com/questions/10156409/convert-hex-string-char-to-int, answer by radhoo. */
static int aux_unhexlify (unsigned char byte) {
  if (byte >= '0' && byte <= '9')     byte -= '0';
  else if (byte >= 'a' && byte <='f') byte -= 'a' - 10;  /* 2.17.1. patch */
  else if (byte >= 'A' && byte <='F') byte -= 'A' - 10;  /* 2.17.1. patch */
  else return -1;
  return byte;
}

#define aux_unhexlifyerror(L, val) { \
  if ((val) == -1) { \
    xfree(buffer); \
    luaL_error(L, "Error in " LUA_QS ": string contains an invalid character.", "utils.unhexlify"); \
  } \
}

/* Conducts the opposite operation of `utils.hexilify`. */
static int utils_unhexlify (lua_State *L) {  /* 2.16.3 */
  char *buffer, *b;
  const char *str;
  size_t l;
  uint32_t val, val1;
  val = 0;
  str = agn_checklstring(L, 1, &l);
  if ((l & 1) == 1)
    luaL_error(L, "Error in " LUA_QS ": string must have an even number of characters.", "utils.unhexlify");
  if (l == 0)
    luaL_error(L, "Error in " LUA_QS ": string must be nonempty.", "utils.unhexlify");
  buffer = agn_stralloc(L, l/2, "utils.unhexlify", NULL);  /* 2.16.5 change */
  b = buffer;
  while (*str) {
    val = aux_unhexlify(uchar(*str++));
    aux_unhexlifyerror(L, val);
    val *= 16;
    val1 = aux_unhexlify(uchar(*str++));
    aux_unhexlifyerror(L, val1);
    val += val1;
    *buffer++ = uchar(val);
  }
  buffer = 0;
  lua_pushlstring(L, b, l/2);  /* do not use pushstring */
  xfree(b);  /* buffer now is pointing to end of string, so do not pass buffer to xfree ! */
  return 1;
}


static int utils_isdate (lua_State *L) {  /* rewritten 2.9.9 */
  int ms;
  agn_pushboolean(L, agnL_datetosecs(L, 1, "utils.isdate", 0, 0, &ms) != 0);
  return 1;
}


/* Receives a positive integer n, a function and any optional arguments, executes the function
   n times. The function returns the execution time in seconds. */
static int utils_speed (lua_State *L) {  /* 2.14.0 */
  int nargs;
  uint32_t i, j, n;
  lua_Number t0, t1;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": must get at least iterations and function.", "utils.speed");
  n = agn_checkposint(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  t0 = (lua_Number)tools_time();
  for (i=0; i < n; i++) {
    luaL_checkstack(L, nargs - 1, "not enough stack space");  /* 3.5.5 fix */
    for (j=2; j <= nargs; j++)
      lua_pushvalue(L, j);
    lua_call(L, nargs - 2, 0);
  }
  t1 = (lua_Number)tools_time();  /* explicitly assign to t1 to avoid negative results ! */
  lua_pushnumber(L, fabs(t1 - t0));  /* XXX 2.14.0: due to unknown reasons, the difference sometimes is negative, so apply fabs. */
  return 1;
}


/* Returns the number of iterations in the interval [a, b], with a <= b, with an optional positive step size, which is
   one by default. a, b, step may all be integral or fractional. 3.6.3 */
static int utils_numiters (lua_State *L) {
  lua_Number a, b, step;
  a = agn_checknumber(L, 1);
  b = agn_checknumber(L, 2);
  if (a > b)
    luaL_error(L, "Error in " LUA_QS ": start value > stop value.", "utils.numiters");
  step = agnL_optpositive(L, 3, 1);
  lua_pushnumber(L, tools_numiters(a, b, step));  /* rounds to the nearest integer towards zero */
  return 1;
}


#define setfield(L, key, value)       lua_rawsetstringinteger(L, -1, key, value)

static int utils_rfc3339 (lua_State *L) {
  int rc, lsd;
  rfc3339time t = { { 0 } };
  const char *str = agn_checkstring(L, 1);
  rc = rfc3339time_parse(str, &t);
  lsd = agnL_optboolean(L, 2, 0);
  if (rc) {
    struct TM tt = t.datetime;
    double fracsecs = (double)t.secfrac_us/1000000.0;
    if (lsd) {
      int rc;
      Time64_T t;
      struct TM *stm;
      t = tools_maketime(1900 + tt.tm_year, tt.tm_mon + 1, tt.tm_mday, tt.tm_hour, tt.tm_min, tt.tm_sec, &rc);
      if (rc != 0)
        luaL_error(L, "Error in " LUA_QS ": time could not be converted.", "utils.rfc3339");
      stm = localtime64(&t);
      if (stm == NULL)
        luaL_error(L, "Error in " LUA_QS ": time could not be converted.", "utils.rfc3339");
      lua_pushnumber(L,
        tools_esd(stm->tm_year + 1900, stm->tm_mon + 1, stm->tm_mday, stm->tm_hour, stm->tm_min, stm->tm_sec) + fracsecs);
    } else {
      luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
      lua_createtable(L, 0, 8);
      setfield(L, "year",   1900 + tt.tm_year);
      setfield(L, "month",  tt.tm_mon + 1);
      setfield(L, "day",    tt.tm_mday);
      setfield(L, "hour",   tt.tm_hour);
      setfield(L, "min",    tt.tm_min);
      setfield(L, "sec",    tt.tm_sec);
      setfield(L, "msec",   t.secfrac_us/1000L);
      setfield(L, "gmtoff", t.tm_gmtoff);
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": could not convert timestamp.", "utils.rfc3339");
  }
  return 1;
}


/* With non-negative integer x returns entier(log2(x)) for x > 1 and x otherwise. The result
   thus is suited primarily for an estimate of the required memory space to be reserved in advance
   for new tables, sequences, registers, numarrays, etc. 3.20.1 */
static int utils_ilog2 (lua_State *L) {  /* sun_uexponent covers a broader number range */
  lua_pushinteger(L, agnO_log2(agn_checknonnegint(L, 1)));
  return 1;
}


/* Returns three suggestions for the optimum size of a structure if at least x slots are needed. The growth pattern
   of the first result, is: 0, 4, 8, 12, 16, 24, 32, 40, 48, 60, 72, 84, 100, 116, 136, etc.

   The Agena equivalent is:

   newsize := << x -> (x + (x >>> 3) + 6) && ~~3 >>
   x := 0
   while x < 1000 do
      y := newsize(x);
      print(x, y, utils.newsize(x))
      x := y
   od;

   In general, the first result is around 113 percent of x (median), rounded up to the next multiple of four.

   Idea taken from: https://github.com/python/cpython/blob/main/Objects/listobject.c

   The second result is the requested number of slots x, rounded up to the next power of two if x is not already
   a power of two.

   The third result is x rounded up to the next multiple of four if x is not already a multiple of four.

   formerly:
   #define newsize(x)    (x + (x >> 3) + (x < 9 ? 3 : 6)) but returning odd results, too.
   Taken from: https://news.ycombinator.com/item?id=1996857; */
static int utils_newsize (lua_State *L) {
  size_t x = (size_t)agn_checknonnegint(L, 1);
  luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
  lua_pushinteger(L, agn_newsize(NULL, x));
  lua_pushinteger(L, agnSeq_bestsize(x));
  lua_pushinteger(L, ((x + (size_t)3)) & ~(size_t)3);
  return 3;
}

/*
multidim := proc(dims :: listing, idx :: integer, offset) is
   local len, m, q, r;
   if not isall(dims, posint) then
      error('Error in `multidim`: second argument must have positive integers only.')
   fi;
   offset := offset or 1;  # 1 if array indices start from 1, 0 if they start from 0
   if offset :- nonnegint then
      error('Error in `multidim`: offset must be non-negative.')
   fi;
   dims := copy(dims);  # we modify dims below, so we need to duplicate
   len := size dims;
   m := mulup(dims);  # get size of flattened array
   idx := utils.posrelat(idx, m) - offset;
   if idx < 0 or idx >= m then
      error('Error in `multidim`: index ' & idx + offset & ' is out-of-bounds.')
   fi;
   for i from len downto 1 do
      q, r := iqr(idx, dims[i]);
      dims[i] := r + offset;
      idx := q
   od;
   return dims
end;
*/


typedef struct IntDimsArray {
  int size;
  int *v;  /* the dimensions (input) */
  int *w;  /* the coordinates (output) */
  int m;   /* the product of all dimensions in v */
} IntDimsArray;

#define getintarrayv(x, n)     ((x)->v[(n)])
#define setintarrayv(x, n, y)  (x)->v[(n)] = (y)
#define getintarrayw(x, n)     ((x)->w[(n)])
#define setintarrayw(x, n, y)  (x)->w[(n)] = (y)

static int aux_newintarray (lua_State *L, size_t n, const char *procname) {  /* creates numeric C array userdata object */
  IntDimsArray *a;
  if (n < 1)
    luaL_error(L, "Error in " LUA_QS ": structure is empty.", procname);
  a = (IntDimsArray *)lua_newuserdata(L, sizeof(IntDimsArray));  /* push userdata on stack */
  a->v = calloc(n, sizeof(int));
  if (!a->v)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
  a->w = calloc(n, sizeof(int));
  if (!a->w) {
    xfree(a->v);  /* 7.3.5 fix */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
  }
  a->size = n;
  a->m = 1;
  luaL_getmetatable(L, "utils");
  lua_setmetatable(L, -2);
  return 1;  /* leave userdata on the top of the stack */
}

#define aux_pushud(L,a,nops,procname) { \
  if (nops < 1 || nops > 256) \
    luaL_error(L, "Error in " LUA_QS ": received too many coordinates or none at all.", procname); \
  aux_newintarray(L, nops, procname); \
  a = (IntDimsArray *)lua_touserdata(L, -1); \
}

#define checkintarray(L, n) (IntDimsArray *)luaL_checkudata(L, n, "utils")   /* for gc */

static void aux_pushintdims (lua_State *L, int idx, const char *procname) {
  int i, n, x, rc;
  IntDimsArray *a;
  luaL_argcheck(L, lua_type(L, idx) == LUA_TTABLE, idx, "argument is not a table");
  n = agn_size(L, idx);
  luaL_checkstack(L, 1, "not enough stack space");
  aux_pushud(L, a, n, procname);  /* input dimensions a->v, resulting coordinates a->w, push as ud */
  a->m = 1;
  for (i=0; i < n; i++) {
    x = (int)agn_rawgetiinteger(L, idx, i + 1, &rc);
    if (!rc || x < 1) {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": dimension must be a positive integer.", procname);
    }
    setintarrayv(a, i, x);  /* a->w is already zeroed */
    a->m *= x;
  }
}

static int multidim (lua_State *L) {
  int idx, i, n;
  lua_Number quo, rem;
  IntDimsArray *a;
  idx = agn_checkinteger(L, 1);
  a = (IntDimsArray *)lua_touserdata(L, lua_upvalueindex(1));
  i = n = a->size;
  idx = tools_posrelat(idx, a->m) - 1;
  if (idx < 0 || idx >= a->m) {
    luaL_error(L, "Error in " LUA_QS ": index %d is out-of-bounds.", "utils.multidim", idx + 1);
  }
  while (i--) {
    quo = sun_iqr(idx, getintarrayv(a, i), &rem);
    /* we have to write the output to a->w so that the input dimensions are not overwritten for the next call */
    setintarrayw(a, i, rem + 1);
    idx = quo;
  }
  /* we avoid creating a hash part by writing from `left` to `right` */
  lua_createtable(L, n, 0);
  for (i=0; i < n; i++) {
    agn_setinumber(L, -1, i + 1, (lua_Number)getintarrayw(a, i));  /* lua_rawsetiinteger pushes a number onto the stack before inserting */
  }
  return 1;
}

/* utils.multidim(dims);
   utils.multidim(idx, dims [, 0]);

   The function returns the multi-dimensional indices for a given one-dimensional index.

   In the first form, the function generates a factory that each time it is called with a one-dimensional index returns the
   corresponding multi-dimensional indices. To represents array a with two rows and three columns with indices starting from 1, issue:

   > f := utils.multidim([2, 3]):
   procedure(01E06548)

   > f(4):
   [2, 1]

   The factory variant is the fastest way to compute the indices.

   In the second form, the one-dimensional index is represented by `idx`, and `dims` is a table, sequence or register with the
   dimensions:

   > utils.multidim(4, [2, 3]):
   [2, 1]

   By default, the indices start from 1. You can change this to zero by passing the third optional argument 0.

   See also: `utils.onedim`. */

static int utils_multidim (lua_State *L) {  /* 4.12.7, around 3 times faster than the Agena implementation */
  int64_t idx, i, m, n, offset, t, x, *a;
  int rc;
  lua_Number quo, rem;
  if (lua_gettop(L) == 1) {
    /* closure variant: just pass the dimensions and the factory will produce a procedure that takes a 1-dim coord */
    aux_pushintdims(L, 1, "utils.multidim");
    lua_pushcclosure(L, &multidim, 1);
    return 1;
  }
  idx = agn_checkinteger(L, 1);
  offset = agnL_optnonnegint(L, 3, 1);
  if (offset < 0 || offset > 1)
    luaL_error(L, "Error in " LUA_QS ": base must be either one or zero.", "utils.onedim");
  t = lua_type(L, 2);
  luaL_typecheck(L, t == LUA_TTABLE || t == LUA_TSEQ || t == LUA_TREG, 2, "table, sequence or register expected", t);
  m = 1;
  n = agn_nops(L, 2);
  a = agn_malloc(L, n*sizeof(int64_t), "utils.multidim", NULL);  /* dimensions */
  switch (t) {
    case LUA_TTABLE: {
      for (i=1; i <= n; i++) {  /* traverse the dimensions */
        x = (int64_t)agn_rawgetiinteger(L, 2, i, &rc);
        if (!rc || x < 1) {
          xfree(a);
          luaL_error(L, "Error in " LUA_QS ": dimension must be a positive integer.", "utils.multidim");
        }
        m *= x;
        a[i - 1] = x;
      }
      idx = tools_posrelat(idx, m) - offset;
      if (idx < 0 || idx >= m) {
        xfree(a);
        luaL_error(L, "Error in " LUA_QS ": index %d is out-of-bounds.", "utils.multidim", idx + offset);
      }
      for (i=n; i > 0; i--) {
        quo = sun_iqr(idx, a[i - 1], &rem);
        a[i - 1] = rem + offset;
        idx = quo;
      }
      /* we avoid creating a hash part */
      lua_createtable(L, n, 0);
      for (i=0; i < n; i++) {
        agn_setinumber(L, -1, i + 1, (lua_Number)a[i]);  /* lua_rawsetiinteger pushes a number onto the stack before inserting */
      }
      break;
    }
    case LUA_TSEQ: {
      for (i=1; i <= n; i++) {
        x = (int64_t)agn_seqrawgetiinteger(L, 2, i, &rc);
        if (!rc || x < 1) {
          xfree(a);
          luaL_error(L, "Error in " LUA_QS ": dimension must be a positive integer.", "utils.multidim");
        }
        m *= x;
        a[i - 1] = x;
      }
      idx = tools_posrelat(idx, m) - offset;
      if (idx < 0 || idx >= m) {
        xfree(a);
        luaL_error(L, "Error in " LUA_QS ": index %d is out-of-bounds.", "utils.multidim", idx + offset);
      }
      for (i=n; i > 0; i--) {
        quo = sun_iqr(idx, a[i - 1], &rem);
        a[i - 1] = rem + offset;
        idx = quo;
      }
      /* we cannot fill from right to left */
      agn_createseq(L, n);
      for (i=0; i < n; i++) {
        agn_seqsetinumber(L, -1, i + 1, (lua_Number)a[i]);
      }
      break;
    }
    case LUA_TREG: {
      for (i=1; i <= n; i++) {
        x = (int64_t)agn_regrawgetiinteger(L, 2, i, &rc);
        if (!rc || x < 1) {
          xfree(a);
          luaL_error(L, "Error in " LUA_QS ": dimension must be a positive integer.", "utils.multidim");
        }
        m *= x;
        a[i - 1] = x;
      }
      idx = tools_posrelat(idx, m) - offset;
      if (idx < 0 || idx >= m) {
        xfree(a);
        luaL_error(L, "Error in " LUA_QS ": index %d is out-of-bounds.", "utils.multidim", idx + offset);
      }
      for (i=n; i > 0; i--) {
        quo = sun_iqr(idx, a[i - 1], &rem);
        a[i - 1] = rem + offset;
        idx = quo;
      }
      agn_createreg(L, n);
      for (i=0; i < n; i++) {
        agn_regsetinumber(L, -1, i + 1, (lua_Number)a[i]);
      }
      break;
    }
    default: {
      xfree(a);
      luaL_error(L, "this should not happen.", "utils.multidim");
    }
  }
  xfree(a);
  return 1;
}


static int onedim (lua_State *L) {
  int64_t idx, size, i, n, x;
  int rc;
  IntDimsArray *a;
  idx = 0;
  size = 1;
  a = (IntDimsArray *)lua_touserdata(L, lua_upvalueindex(1));  /* the dimensions */
  if (lua_istable(L, 1)) {  /* the coordinates are in a table */
    i = n = luaL_getn(L, 1);  /* 4.12.9 change */
    if (n != a->size)
      luaL_error(L, "Error in " LUA_QS ": need %d table entries, got %d.", "utils.onedim", a->size, n);
    while (i--) {
      x = (int64_t)tools_posrelat(agn_rawgetiinteger(L, 1, i + 1, &rc), getintarrayv(a, i)) - 1;
      if (!rc || x < 0 || x >= getintarrayv(a, i)) {  /* 4.12.9 fix */
        xfree(a);
        luaL_error(L, "Error in " LUA_QS ": entry #%d is not an integer, is zero or out-of-range.", "utils.onedim", i);
      }
      idx += x*size;
      if (i == 0) break;
      size *= getintarrayv(a, i);
    }
  } else {  /* the arguments are the coordinates */
    i = n = lua_gettop(L);
    if (n != a->size) {
      if (lua_isseq(L, 1) || lua_isreg(L, 1))
        luaL_error(L, "Error in " LUA_QS ": need a table of %d integers.", "utils.onedim", a->size);
      else
        luaL_error(L, "Error in " LUA_QS ": need %d integer arguments, got %d.", "utils.onedim", a->size, n);
    }
    while (i--) {
      x = (int64_t)tools_posrelat(agn_checkinteger(L, i + 1), getintarrayv(a, i)) - 1;
      if (x < 0 || x >= getintarrayv(a, i)) {  /* 4.12.9 fix */
        xfree(a);
        luaL_error(L, "Error in " LUA_QS ": entry #%d is zero or out-of-range.", "utils.onedim", i);
      }
      idx += x*size;
      if (i == 0) break;
      size *= getintarrayv(a, i);
    }
  }
  lua_pushinteger(L, idx + 1);
  return 1;
}

/* Computes the one-dimensional index equivalent for multidimensional indices of any structure.

   Pass the dimensions of the structure in a table, followed by one or more indices of interest.

   By default, the function assumes that all indices start from one. You may append a final zero
   to the argument list to indicate that all the indices start from zero instead of one.

   Imagine a two dimensional table array A with two rows and three columns, with all the indices
   starting from one:

   A| 1 2 3
   -|------
   1| 1 2 3
    |
   2| 4 5 6

   This array has dimensions 2, 3.

   You can internally represent this two-dimensional array as a one-dimensional one, B, with dimension 6:

   B| 1 2 3 4 5 6
   -|------------
   1| 1 2 3 4 5 6

   `utils.onedim easily allows to convert array indices into just one. So, for example, A[2, 1] and
   B[4] are identical:

   > utils.onedim([2, 3], 2, 1):
   4

   If the indices start from zero, we have:

   A| 0 1 2
   -|------
   0| 0 1 2
    |
   1| 3 4 5

   B| 0 1 2 3 4 5
   -|------------
   0| 0 1 2 3 4 5

   The dimensions, of course, still are the same.

   utils.onedim([2, 3], 1, 0, 0):
   3

   Based on numarray.c/aux_getcoord, 4.10.9 */

static int utils_onedim (lua_State *L) {  /* 4.12.7 */
  int64_t idx, i, n1, n2, t1, t2, base, *a, x, size, nargs;
  int rc;
  nargs = lua_gettop(L);
  if (nargs == 1) {
    /* closure variant: just pass the dimensions and the factory will produce a procedure that takes a 1-dim coord */
    aux_pushintdims(L, 1, "utils.onedim");
    lua_pushcclosure(L, &onedim, 1);
    return 1;
  }
  t1 = lua_type(L, 1);
  t2 = lua_type(L, 2);
  if (t1 == LUA_TTABLE) {  /* arg 1: table with dimensions, arg2 .. nargs: coordinates and optionally offset (numbers) */
    int flag = 1;
    for (i=2; i <= nargs; i++) {
      if (lua_type(L, i) != LUA_TNUMBER) { flag = 0; break; }
    }
    if (flag) {
      int64_t j, n, dim;
      idx = 0LL; size = 1LL; base = 1LL;
      luaL_checktype(L, 1, LUA_TTABLE);
      n = luaL_getn(L, 1);
      if (n + 2 == nargs) {
        base = agn_checkinteger(L, nargs);
        if (base < 0 || base > 1)
          luaL_error(L, "Error in " LUA_QS ": base must be either one or zero.", "utils.onedim");
        nargs--;
      }
      if (nargs - 1 != n)
        luaL_error(L, "Error in " LUA_QS ": dimensions mismatch.", "utils.onedim");
      /* we read the indices from right to left */
      for (i=nargs; i > 1; i--) {
        dim = (int64_t)agn_rawgetiinteger(L, 1, i - 1, &rc);
        j = tools_posrelat(agn_checkinteger(L, i), dim);
        if (j < base || j > dim - 1 + base)
          luaL_error(L, "Error in " LUA_QS ": argument #%d is out-of-range.", "utils.onedim", i);
        idx += (j - base)*size;
        size *= dim;
      }
      lua_pushnumber(L, idx + (base == 1));
      return 1;
    }
  }
  luaL_typecheck(L, t1 == LUA_TTABLE || t1 == LUA_TSEQ || t1 == LUA_TREG, 1, "table, sequence or register expected", t1);  /* the coordinates */
  luaL_typecheck(L, t2 == LUA_TTABLE || t2 == LUA_TSEQ || t2 == LUA_TREG, 2, "table, sequence or register expected", t2);  /* the dimensions */
  if (t1 != t2) {
    luaL_error(L, "Error in " LUA_QS ": first and second argument must be of the same type.", "utils.onedim");
  }
  base = agnL_optnonnegint(L, 3, 1);
  n2 = agn_nops(L, 2);
  a = agn_malloc(L, n2*sizeof(int64_t), "utils.onedim", NULL);  /* dimensions */
  idx = 0;
  size = 1;
  switch (t2) {
    case LUA_TTABLE: {
      for (i=1; i <= n2; i++) {  /* traverse the dimensions */
        x = (int64_t)agn_rawgetiinteger(L, 2, i, &rc);
        if (!rc || x < 1) {
          xfree(a);
          luaL_error(L, "Error in " LUA_QS ": dimension must be a positive integer.", "utils.onedim");
        }
        a[i - 1] = x;
      }
      i = n1 = agn_size(L, 1);
      if (n1 != n2) {
        xfree(a);
        luaL_error(L, "Error in " LUA_QS ": number of indices must be %d.", "utils.onedim", n1);
      }
      while (i--) {
        x = (int64_t)tools_posrelat(agn_rawgetiinteger(L, 1, i + 1, &rc), a[i]);
        if (x < base || x > a[i] - 1 + base) {
          xfree(a);
          luaL_error(L, "Error in " LUA_QS ": entry #%d is zero or out-of-range.", "utils.onedim", i);
        }
        idx += (x - base)*size;
        if (i == 0) break;
        size *= a[i];
      }
      break;
    }
    case LUA_TSEQ: {
      for (i=1; i <= n2; i++) {
        x = (int64_t)agn_seqrawgetiinteger(L, 2, i, &rc);
        if (!rc || x < 1) {
          xfree(a);
          luaL_error(L, "Error in " LUA_QS ": dimension must be a positive integer.", "utils.onedim");
        }
        a[i - 1] = x;
      }
      i = n1 = agn_seqsize(L, 1);
      if (n1 != n2) {
        xfree(a);
        luaL_error(L, "Error in " LUA_QS ": number of indices must be %d.", "utils.onedim", n1);
      }
      while (i--) {
        x = (int64_t)tools_posrelat(agn_seqrawgetiinteger(L, 1, i + 1, &rc), a[i]);
        if (x < base || x > a[i] - 1 + base) {
          xfree(a);
          luaL_error(L, "Error in " LUA_QS ": entry #%d is zero or out-of-range.", "utils.onedim", i);
        }
        idx += (x - base)*size;
        if (i == 0) break;
        size *= a[i];
      }
      break;
    }
    case LUA_TREG: {
      for (i=1; i <= n2; i++) {
        x = (int64_t)agn_regrawgetiinteger(L, 2, i, &rc);
        if (!rc || x < 1) {
          xfree(a);
          luaL_error(L, "Error in " LUA_QS ": dimension must be a positive integer.", "utils.onedim");
        }
        a[i - 1] = x;
      }
      i = n1 = agn_regsize(L, 1);
      if (n1 != n2) {
        xfree(a);
        luaL_error(L, "Error in " LUA_QS ": number of indices must be %d.", "utils.onedim", n1);
      }
      while (i--) {
        x = (int64_t)tools_posrelat(agn_regrawgetiinteger(L, 1, i + 1, &rc), a[i]);
        if (x < base || x > a[i] - 1 + base) {
          xfree(a);
          luaL_error(L, "Error in " LUA_QS ": entry #%d is zero or out-of-range.", "utils.onedim", i);
        }
        idx += (x - base)*size;
        if (i == 0) break;
        size *= a[i];
      }
      break;
    }
    default: {
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "utils.onedim");
    }
  }
  xfree(a);
  lua_pushnumber(L, idx + base);
  return 1;
}


static int arraygc (lua_State *L) {  /* __gc method */
  IntDimsArray *a = checkintarray(L, 1);
  xfreeall(a->v, a->w);
  lua_setmetatabletoobject(L, 1, NULL, 0);
  return 0;
}

static const struct luaL_Reg utils_arraylib [] = {
  {"__gc", arraygc},
  {NULL, NULL}
};


typedef struct Userdata {
  int  registry;  /* registry, for attribute information; 5.1.6 */
  char *name;     /* the name of the user data, to be freed ! */
  int  namelen;   /* length of the name */
} Userdata;

#define checkudata(L,n) (Userdata *)luaL_checkudata(L, n, "udata")
#define isudata(L,n)    (luaL_isudata(L, n, "udata") && agn_isutypeset(L, n))
#define aux_pushstatusdata(L,u) { \
  if (u->registry == LUA_NOREF) { \
    lua_getfenv(L, 1); \
  } else { \
    lua_rawgeti(L, LUA_REGISTRYINDEX, u->registry); \
  } \
}

static int utils_udata (lua_State *L) {
  Userdata *u;
  if (agn_isstring(L, 1)) {  /* set up new udata object */
    size_t l;
    const char *name = lua_tolstring(L, 1, &l);
    int t = lua_type(L, 2);
    u = (Userdata *)lua_newuserdata(L, sizeof(Userdata));
    agn_setutypestring(L, -1, name);
    lua_setmetatabletoobject(L, -1, "udata", 0);
    switch (t) {  /* 5.5.15 */
      case LUA_TNIL: case LUA_TTABLE: case LUA_TSET: case LUA_TSEQ: case LUA_TREG: case LUA_TPAIR: case LUA_TUSERDATA:
        lua_pushvalue(L, 2);  /* use it as accompanying data container */
        break;
      default:  /* any other data or none at all (just one argument given): ... */
        lua_createtable(L, 0, 0);  /* ... create accompanying table. */
    }
    if (lua_istable(L, -1)) {  /* can only set tables to userdata environment */
      lua_setfenv(L, -2);
      u->registry = LUA_NOREF;
    } else {
      u->registry = luaL_ref(L, LUA_REGISTRYINDEX);  /* ... and put it into the registry */
    }
    char *temp = tools_strdup(name);
    if (!temp) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "utils.udata");
    u->name = temp;
    u->namelen = l;
  } else {  /* return the accompanying data container, 5.5.15 fix */
    u = checkudata(L, 1);
    aux_pushstatusdata(L, u);
  }
  return 1;
}

static int ulist_udatatostring (lua_State *L) {
  void *p;
  checkudata(L, 1);
  if (agn_getutype(L, 1)) {
    if (agn_isstring(L, -1)) {  /* return utype string */
      return 1;
    }
    agn_poptop(L);  /* drop whatever */
  }
  /* better be sure than sorry */
  p = lua_touserdata(L, 1);
  lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  return 1;
}

static int utils_udataoftype (lua_State *L) {
  lua_pushboolean(L, isudata(L, 1));
  return 1;
}

static int utils_udatagetitem (lua_State *L) {  /* 5.1.6 */
  int nargs = lua_gettop(L);
  Userdata *u = checkudata(L, 1);
  if (nargs == 2 && u->name != NULL && lua_isstring(L, 2)) {  /* OOP method call, 5.5.14 */
    int t = agnL_gettablefield(L, u->name, agn_tostring(L, 2), NULL, 0);
    agn_poptop(L);
    if (t == LUA_TFUNCTION) {
      return agn_initmethodcall(L, u->name, u->namelen);
    }
  }
  aux_pushstatusdata(L, u);
  lua_replace(L, 1);  /* replace argument #1 with registry table */
  lua_settop(L, 2);   /* discard any surplus arguments */
  lua_rawget(L, 1);   /* for key argument #2 fetch value from registry table */
  return 1;
}

static int utils_udatasetitem (lua_State *L) {  /* 5.1.6 */
  Userdata *u = checkudata(L, 1);
  agnL_checkudfreezed(L, 1, "udata", "(call metamethod)");
  /* push registry table onto the stack */
  aux_pushstatusdata(L, u);
  lua_replace(L, 1);  /* replace argument #1 with registry table */
  lua_settop(L, 3);   /* discard any surplus arguments */
  lua_rawset(L, 1);   /* set key ~ value pair (argument #2 and #3) into registry table */
  return 0;
}

static int utils_udatacall (lua_State *L) {  /* 5.1.6 */
  Userdata *u = checkudata(L, 1);
  return agnL_calludata(L, lua_gettop(L), u->registry, "udata");  /* 5.4.0 change */
}

static int utils_udatagc (lua_State *L) {  /* __gc method */
  Userdata *u = checkudata(L, 1);
  /* remove read-only mode if set */
  agn_udfreeze(L, 1, 0);
  /* delete metatable and user-defined type */
  lua_setmetatabletoobject(L, 1, NULL, 1);
  /* delete registry table, 5.1.6 change */
  if (u->registry == LUA_NOREF) {
    lua_pushnil(L);  /* destroy environment table */
    lua_setfenv(L, 1);
  } else {
    luaL_unref(L, LUA_REGISTRYINDEX, u->registry);
  }
  xfree(u->name);
  return 0;
}

static int utils_left (lua_State *L) {  /* 5.5.15 */
  Userdata *u = checkudata(L, 1);
  aux_pushstatusdata(L, u);
  lua_replace(L, 1);  /* replace argument #1 with registry table */
  lua_settop(L, 1);   /* discard any surplus arguments */
  luaL_argcheck(L, lua_ispair(L, 1), 1, "pair expected");
  agn_pairgeti(L, 1, 1);
  return 1;
}

static int utils_right (lua_State *L) {  /* 5.5.15 */
  Userdata *u = checkudata(L, 1);
  aux_pushstatusdata(L, u);
  lua_replace(L, 1);  /* replace argument #1 with registry table */
  lua_settop(L, 1);   /* discard any surplus arguments */
  luaL_argcheck(L, lua_ispair(L, 1), 1, "pair expected");
  agn_pairgeti(L, 1, 2);
  return 1;
}

static const struct luaL_Reg utils_udatalib [] = {
  {"__call",       utils_udatacall},
  {"__index",      utils_udatagetitem},
  {"__writeindex", utils_udatasetitem},
  {"__oftype",     utils_udataoftype},
  {"__left",       utils_left},
  {"__right",      utils_right},
  {"__tostring",   ulist_udatatostring},
  {"__gc",         utils_udatagc},
  {NULL, NULL}
};


static const luaL_Reg utils[] = {
  {"calendar",   utils_calendar},          /* added on April 07, 2007 */
  {"decodeb32",  utils_decodeb32},         /* added on February 04, 2024 */
  {"decodea85",  utils_decodea85},         /* added on April 22, 2022 */
  {"decodeb64",  utils_decodeb64},         /* added on May 27, 2012 */
  {"decodeb85",  utils_decodeb85},         /* added on April 16, 2022 */
  {"decodez85",  utils_decodez85},         /* added on August 02, 2025 */
  {"encodea85",  utils_encodea85},         /* added on April 22, 2022 */
  {"encodeb32",  utils_encodeb32},         /* added on February 04, 2024 */
  {"encodeb64",  utils_encodeb64},         /* added on May 27, 2012 */
  {"encodeb85",  utils_encodeb85},         /* added on April 16, 2022 */
  {"encodez85",  utils_encodez85},         /* added on September 02, 2025 */
  {"hexlify",    utils_hexlify},           /* added on September 25, 2019 */
  {"ilog2",      utils_ilog2},             /* added on August 13, 2024 */
  {"isdate",     utils_isdate},            /* added on January 31, 2013 */
  {"multidim",   utils_multidim},          /* added May 27, 2025 */
  {"numiters",   utils_numiters},          /* added on November 30, 2023 */
  {"onedim",     utils_onedim},            /* added on April 04, 2025/May 27, 2025 */
  {"newsize",    utils_newsize},           /* added on September 16, 2024 */
  {"rfc3339",    utils_rfc3339},           /* added on June 17, 2024 */
  {"singlesubs", utils_singlesubs},        /* added on September 07, 2011 - added on August 05, 2007 */
  {"speed",      utils_speed},             /* added on September 22, 2018 */
  {"udata",      utils_udata},             /* added on July 15, 2025 */
  {"unhexlify",  utils_unhexlify},         /* added on September 25, 2019 */
  {NULL, NULL}
};


/*
** Open utils library
*/
LUALIB_API int luaopen_utils (lua_State *L) {
  luaL_newmetatable(L, "utils");
  luaL_register(L, NULL, utils_arraylib);  /* associate __gc method */
  luaL_newmetatable(L, "udata");
  luaL_register(L, NULL, utils_udatalib);
  luaL_register(L, AGENA_UTILSLIBNAME, utils);
  return 1;
}

