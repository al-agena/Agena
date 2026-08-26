#ifndef REGEX_H
#define REGEX_H

/*
  License of Lrexlib release
  --------------------------

  Copyright (C) Reuben Thomas 2000-2020
  Copyright (C) Shmuel Zeigerman 2004-2020

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
  documentation files (the "Software"), to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
  Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
  WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
  OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
  OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  This code has been taken from: lrexlib-posix-2.9.1-1.src.rock
  available at https://luarocks.org/modules/rrt/lrexlib-pcre2

  Ported to Agena by awalz, started 18/04/2023

  Example:

  # !!! Look out for escaping backslashes in the regex pattern, and substitute one backslash with two backslashes !!!
  # Furthermore, if the regex pattern contains single or double quotes, better escape them with one backslash each -
  # the string will not be accepted otherwise; something like "abc\"xyz" or 'abc\'xyz' will work, something like
  # 'abc'xyz' will not.

import regex
pattern :=  "(?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|\"(?:[\\x01-\\x08\\x0b\\x0c\\x0e-\\x1f\\x21\\x23-\\x5b\\x5d-\\x7f]|\\\\[\\x01-\\x09\\x0b\\x0c\\x0e-\\x7f])*\")@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?|\\[(?:(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9]))\\.){3}(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9])|[a-z0-9-]*[a-z0-9]:(?:[\\x01-\\x08\\x0b\\x0c\\x0e-\\x1f\\x21-\\x5a\\x53-\\x7f]|\\\\[\\x01-\\x09\\x0b\\x0c\\x0e-\\x7f])+)\\])"
print(regex.find('abc@xyz.il', pattern));
p := regex.new(pattern);
print(regex.find('abc@xyz.il', p));
*/

/* Common structs and functions */

#undef REX_VERSION
#define REX_VERSION "2.9.2"

#ifndef REX_LIBNAME
#define REX_LIBNAME "regex"
#endif

#ifndef REX_OPENLIB
#define REX_OPENLIB luaopen_regex
#endif

#ifndef PCRE2_CODE_UNIT_WIDTH
#define PCRE2_CODE_UNIT_WIDTH 8
#endif

#include <pcre2.h>

#include "agena.h"

typedef struct {
  pcre2_code *pr;
  pcre2_compile_context *ccontext;
  pcre2_match_data *match_data;
  PCRE2_SIZE *ovector;
  int ncapt;
  const unsigned char *tables;
  int freed;
} TPcre2;

typedef struct {  /* compile arguments */
  const char  *pattern;
  size_t       patlen;
  void        *ud;
  int          cflags;
  const char  *locale;             /* PCRE, Oniguruma */
  const unsigned char *tables;     /* PCRE */
  int          tablespos;          /* PCRE */
  void        *syntax;             /* Oniguruma */
  const unsigned char *translate;  /* GNU */
  int          gnusyn;             /* GNU */
} TArgComp;

typedef struct {  /* exec arguments */
  const char  *text;
  size_t       textlen;
  int          startoffset;
  int          eflags;
  int          funcpos;
  int          maxmatch;
  int          funcpos2;           /* used with gsub */
  int          reptype;            /* used with gsub */
  size_t       ovecsize;           /* PCRE: dfa_exec */
  size_t       wscount;            /* PCRE: dfa_exec */
} TArgExec;

#define TUserdata TPcre2

#define ALG_NOMATCH(res)   ((res) == PCRE2_ERROR_NOMATCH)
#define ALG_ISMATCH(res)   ((res) >= 0)
#define ALG_SUBBEG(ud,n)   ((int)(ud)->ovector[(n)+(n)])
#define ALG_SUBEND(ud,n)   ((int)(ud)->ovector[(n)+(n)+1])
#define ALG_SUBLEN(ud,n)   (ALG_SUBEND((ud),(n)) - ALG_SUBBEG((ud),(n)))
#define ALG_SUBVALID(ud,n) (0 == pcre2_substring_length_bynumber((ud)->match_data, (n), NULL))
#define ALG_NSUB(ud)       ((int)(ud)->ncapt)
#define ALG_PUSHSUB(L,ud,text,n) \
  lua_pushlstring(L, (text) + ALG_SUBBEG((ud),(n)), ALG_SUBLEN((ud),(n)))
#define ALG_PUSHSUB_OR_FALSE(L,ud,text,n) \
  (ALG_SUBVALID(ud,n) ? (void) ALG_PUSHSUB(L,ud,text,n) : lua_pushboolean(L,0))
#define ALG_PUSHSTART(L,ud,offs,n)   lua_pushinteger(L, (offs) + ALG_SUBBEG(ud,n) + 1)
#define ALG_PUSHEND(L,ud,offs,n)     lua_pushinteger(L, (offs) + ALG_SUBEND(ud,n))
#define ALG_PUSHOFFSETS(L,ud,offs,n) \
  (ALG_PUSHSTART(L,ud,offs,n), ALG_PUSHEND(L,ud,offs,n))

#define ALG_BASE(st)  0
#define ALG_PULL
#define ALG_CHARSIZE 1

#define ALG_CFLAGS_DFLT 0
#define ALG_EFLAGS_DFLT 0

#define ALG_GETCARGS(a,b,c)   checkarg_compile(a,b,c)
#define ALG_GETCFLAGS(L,pos)  getcflags(L, pos)

int getcflags (lua_State *L, int pos);
void checkarg_compile (lua_State *L, int pos, TArgComp *argC);
void do_named_subpatterns (lua_State *L, TPcre2 *ud, const char *text);  /* obviously not used */

#endif

