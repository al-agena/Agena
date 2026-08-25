/*
** $Id: lobject.c,v 2.22 2006/02/10 17:43:52 roberto Exp $
** Some generic functions over Lua & Agena objects
** See Copyright Notice in agena.h
*/

/* #define __USE_MINGW_ANSI_STDIO 1 */

#include <ctype.h>
#include <errno.h>  /* errno */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define lobject_c
#define LUA_CORE

#include "agena.h"
#include "agncmpt.h"
#include "agnhlps.h"

#include "ldebug.h"
#include "ldo.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "lvm.h"


const TValue luaO_nilobject_ = {{NULL}, LUA_TNIL};

/*
** converts an integer to a "floating point byte", represented as
** (eeeeexxx), where the real value is (1xxx) * 2^(eeeee - 1) if
** eeeee != 0 and (xxx) otherwise.
*/
int luaO_int2fb (unsigned int x) {
  int e = 0;  /* exponent */
  while (x >= 16) {
    x = (x + 1) >> 1;
    e++;
  }
  if (x < 8) return x;
  else return ((e + 1) << 3) | (cast_int(x) - 8);
}


/* converts back */
int luaO_fb2int (int x) {
  int e = (x >> 3) & 31;
  if (e == 0) return x;
  else return ((x & 7) + 8) << (e - 1);
}


/*
** Computes ceil(log2(x))
*/
static const lu_byte log_2[256] = {  /* log_2[i] = ceil(log2(i - 1)) */
  0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
};

int luaO_log2 (unsigned int x) {  /* sun_uexponent covers a broader number range */
  int l = -1;
  while (x >= 256) { l += 8; x >>= 8; }
  return l + log_2[x];
}


int agnO_log2 (unsigned int x) {  /* like luaO_log2 but returns x for x < 2. */
  int l = -1*(x > 1);
  while (x >= 256) { l += 8; x >>= 8; }
  return l + log_2[x];
}


int luaO_rawequalObj (const TValue *t1, const TValue *t2) {
  if (ttype(t1) != ttype(t2)) return 0;
  else switch (ttype(t1)) {
    case LUA_TNIL:
      return 1;
    case LUA_TNUMBER:
      return luai_numeq(nvalue(t1), nvalue(t2));
    case LUA_TCOMPLEX: {  /* 6.8.1 fix */
#ifndef PROPCMPLX
      agn_Complex z1 = cvalue(t1);
      agn_Complex z2 = cvalue(t2);
      return luai_numeq(creal(z1), creal(z2)) &&
             luai_numeq(cimag(z1), cimag(z2));
#else
      return luai_numeq((cvalue(t1))[0], (cvalue(t2))[0]) &&
             luai_numeq((cvalue(t1))[1], (cvalue(t2))[1]);
#endif
    }
    case LUA_TBOOLEAN:
      return bvalue(t1) == bvalue(t2);  /* boolean true must be 1 !! */
    case LUA_TLIGHTUSERDATA:
      return pvalue(t1) == pvalue(t2);
    case LUA_TFAIL:
      return 1;
    default:
      lua_assert(iscollectable(t1));
      return gcvalue(t1) == gcvalue(t2);
  }
}


int lisbdigit (unsigned char x) { return x >= '0' && x < '2'; }
int lisodigit (unsigned char x) { return x >= '0' && x < '8'; }
int lishdigit (unsigned char x) { return lisxdigit((unsigned char)x); }

int luaO_str2d (const char *s, lua_Number *result, int *overflow) {  /* modified 1.6.4 to avoid compiler warnings */
  int en;
  char *endptr;
  set_errno(0);  /* reset, 2.39.5 fix */
  *overflow = 0;
  *result = lua_str2number(s, &endptr);  /* GREP "STRTOD" ERANGE error thrown by strtod */
  en = errno;
  if (en == ERANGE) *overflow = 1;
  if (endptr == s) return 0;  /* conversion failed */
  switch (*endptr) {  /* patched 0.12.2 */
    case 'x':
    case 'X': /* maybe an hexadecimal constant? */
      *result = lua_strany2number(s, &endptr, 16, 'x', 'X', &lishdigit, 1, overflow);
      break;
    case 'b':
    case 'B': /* maybe a binary constant? 0.31.2; range extension and fractions 2.34.9  */
      *result = lua_strany2number(s, &endptr, 2, 'b', 'B', &lisbdigit, 1, overflow);
      break;
    case 'o':
    case 'O': /* maybe an octal constant? 0.31.2; range extension to 2^64 - 1 and fractions 2.34.9 */
      *result = lua_strany2number(s, &endptr, 8, 'o', 'O', &lisodigit, 1, overflow);
      break;
    case 'k':
      *result *= 1000.0;
      ++endptr;
      break;
    case 'K':  /* kilobyte */
      *result *= 1024.0;
      ++endptr;
      break;
    case 'm':  /* million */
      *result *= 1000000.0;
      ++endptr;
      break;
    case 'M':  /* megabyte */
      *result *= 1048576.0;
      ++endptr;
      break;
    case 'g':  /* billion, 1.5.1 */
      *result *= 1000000000.0;
      ++endptr;
      break;
    case 'G':  /* gigabyte */
      *result *= 1073741824.0;
      ++endptr;
      break;
    case 't':  /* trillion, 1.5.1 */
      *result *= 1000000000000.0;
      ++endptr;
      break;
    case 'T':  /* terabyte, 1.5.1 */
      *result *= 1099511627776.0;
      ++endptr;
      break;
    /* overflow with (Tera+)bytes, so not queried here */
    case 'p':  /* percentage, 2.10.0 */
      *result /= 100.0;
      ++endptr;
      break;
    case 'd': { /* 3.16.1 change: indicates a number in degrees and converts it to radians */
      long double r = (long double)(*result);
      *result = r*(M_PIld/180.0L);
      ++endptr;
      break;
    } case 'D':  /* 2.10.0, dozen */
      *result *= 12;
      ++endptr;
      break;
    case 'r': { /* 3.16.1 change: indicates a number in radians and converts it to degrees */
      long double r = (long double)(*result);
      *result = r*(180.0L/M_PIld);
      ++endptr;
      break;
    }
  }
  if (*endptr == '\0') return 1;  /* most common case */
  while (isspace(cast(unsigned char, *endptr))) endptr++;
  if (*endptr != '\0') return 0;  /* invalid trailing characters? */
  return 1;
}


#ifndef PROPCMPLX
int luaO_str2c (const char *s, agn_Complex *result) {  /* 0.26.1 */
#else
int luaO_str2c (const char *s, lua_Number *result) {  /* 0.26.1 */
#endif
  char *endptr;
  lua_Number real, imag, sign;
  sign = 1;
  real = lua_str2number(s, &endptr);
  imag = 0;
  if (endptr == s) {  /* only `-I`, `+I` passed ? */
    while (isspace(cast(unsigned char, *endptr))) endptr++;
    if (*endptr == '+' || *endptr == '-') {
      if (*endptr == '-') sign = -1;
      ++endptr;
    }
    if (*endptr == 'I') {
#ifndef PROPCMPLX
      *result = 0 + sign*I;
#else
      result[0] = 0; result[1] = sign;
#endif
      ++endptr;
      goto end;
    } else {  /* conversion failed */
#ifndef PROPCMPLX
      *result = real;
#else
      result[0] = real; result[1] = real;
#endif
      return 0;
    }
  }
  /* as yet, a number has been recognized */
  while (isspace(cast(unsigned char, *endptr))) endptr++;
  if (*endptr == '*') {  /* only imaginary part passed ? */
    ++endptr;
    while (isspace(cast(unsigned char, *endptr))) endptr++;
    if (*endptr == 'I') {
#ifndef PROPCMPLX
      *result = 0 + real*I;
#else
      result[0] = 0; result[1] = real;
#endif
      ++endptr;
    } else {  /* conversion failed */
#ifndef PROPCMPLX
      *result = real;
#else
      result[0] = real; result[1] = real;
#endif
      return 0;
    }
  } else if (*endptr == '+' || *endptr == '-') {  /* real part passed, try imaginary part */
    const char *newpos;
    if (*endptr == '-') sign = -1;
    ++endptr;
    while (isspace(cast(unsigned char, *endptr))) endptr++;
    if (*endptr == 'I') {  /* only imaginary unit passed ? */
#ifndef PROPCMPLX
      *result = real + sign*I;
#else
      result[0] = real; result[1] = sign;
#endif
      ++endptr;
      goto end;
    }
    newpos = endptr;
    /* read number of imaginary part */
    imag = lua_str2number(newpos, &endptr);
    if (endptr == newpos) {  /* conversion failed */
#ifndef PROPCMPLX
      *result = imag;
#else
      result[0] = imag; result[1] = imag;
#endif
      return 0;
    }
    while (isspace(cast(unsigned char, *endptr))) endptr++;
    if (*endptr == '*') {
      ++endptr;
      while (isspace(cast(unsigned char, *endptr))) endptr++;
      if (*endptr == 'I') {
#ifndef PROPCMPLX
        *result = real + sign*imag*I;
#else
        result[0] = real; result[1] = sign*imag;
#endif
        ++endptr;
      } else {  /* conversion failed */
#ifndef PROPCMPLX
        *result = imag;
#else
        result[0] = imag; result[1] = imag;
#endif
        return 0;
      }
    } else {  /* conversion failed */
#ifndef PROPCMPLX
      *result = imag;
#else
      result[0] = imag; result[1] = imag;
#endif
      return 0;
    }
  }
end:
  if (*endptr == '\0') return 1;  /* most common case */
  while (isspace(cast(unsigned char, *endptr))) endptr++;
  if (*endptr != '\0') return 0;  /* invalid trailing characters? */
  return 1;
}


/* maximum length of a numeral */
#if !defined (L_MAXLENNUM)
#define L_MAXLENNUM  200
#endif

static const char *l_str2dloc (const char *s, lua_Number *result, int mode) {
  char *endptr;
  int overflow;
  *result = (mode == 'x') ? lua_strany2number(s, &endptr, 16, 'x', 'X', &lishdigit, 1, &overflow)  /* try to convert */
                          : lua_str2number(s, &endptr);
  if (endptr == s) return NULL;  /* nothing recognized? */
  while (lisspace(cast_uchar(*endptr))) endptr++;  /* skip trailing spaces */
  return (*endptr == '\0') ? endptr : NULL;  /* OK if no trailing characters */
}


/* === Taken from Lua 5.4.0 RC 4 =======================================================*/

int luaO_hexavalue (int c) {  /* takes a character */
  if (lisdigit(c)) return c - '0';
  else return (ltolower(c) - 'a') + 10;
}

static int isneg (const char **s) {
  if (**s == '-') { (*s)++; return 1; }
  else if (**s == '+') (*s)++;
  return 0;
}

/*
** Convert string 's' to a Lua number (put in 'result'). Return NULL
** on fail or the address of the ending '\0' on success.
** 'pmode' points to (and 'mode' contains) special things in the string:
** - 'x'/'X' means a hexadecimal numeral
** - 'n'/'N' means 'inf' or 'nan' (which should be rejected)
** - '.' just optimizes the search for the common case (nothing special)
** This function accepts both the current locale or a dot as the radix
** mark. If the conversion fails, it may mean number has a dot but
** locale accepts something else. In that case, the code copies 's'
** to a buffer (because 's' is read-only), changes the dot to the
** current locale radix mark, and tries to convert again.
*/
static const char *l_str2d (const char *s, lua_Number *result) {
  const char *endptr;
  const char *pmode = strpbrk(s, ".xXnN");
  int mode = pmode ? ltolower(cast_uchar(*pmode)) : 0;
  if (mode == 'n')  /* reject 'inf' and 'nan' */
    return NULL;
  endptr = l_str2dloc(s, result, mode);  /* try to convert */
  if (endptr == NULL) {  /* failed? may be a different locale */
    char buff[L_MAXLENNUM + 1];
    const char *pdot = strchr(s, '.');
    if (tools_strlen(s) > L_MAXLENNUM || pdot == NULL)  /* 2.25.1 tweak */
      return NULL;  /* string too long or no dot; fail */
    strcpy(buff, s);  /* copy string to buffer */
    buff[pdot - s] = lua_getlocaledecpoint();  /* correct decimal point */
    endptr = l_str2dloc(buff, result, mode);  /* try again */
    if (endptr != NULL)
      endptr = s + (endptr - buff);  /* make relative to 's' */
  }
  return endptr;
}

#define MAXBY10    cast(lua_Unsigned, LUA_MAXINTEGER / 10)
#define MAXLASTD   cast_int(LUA_MAXINTEGER % 10)

static const char *l_str2int (const char *s, ptrdiff_t *result) {
  ptrdiff_t a = 0;  /* 2.26.0 fix */
  int empty = 1;
  int neg;
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);
  if (s[0] == '0' &&
     (s[1] == 'x' || s[1] == 'X')) {  /* hex? */
    s += 2;  /* skip '0x' */
    for (; lisxdigit(cast_uchar(*s)); s++) {
      a = a * 16 + luaO_hexavalue(*s);
      empty = 0;
    }
  }
  else {  /* decimal */
    for (; lisdigit(cast_uchar(*s)); s++) {
      int d = *s - '0';
      if (a >= MAXBY10 && (a > MAXBY10 || d > MAXLASTD + neg))  /* overflow? */
        return NULL;  /* do not accept it (as integer) */
      a = a * 10 + d;
      empty = 0;
    }
  }
  while (lisspace(cast_uchar(*s))) s++;  /* skip trailing spaces */
  if (empty || *s != '\0') return NULL;  /* something wrong in the numeral */
  else {
    *result = l_castU2S((neg) ? 0u - a : a);
    return s;
  }
}


const char *luaO_str2uint64 (const char *s, uint64_t *result) {  /* 6.2.0 */
  uint64_t olda = 0ULL, a = 0ULL;
  int empty = 1;
  int neg;
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);
  if (s[0] == '0' &&
     (s[1] == 'x' || s[1] == 'X')) {  /* hex? */
    int c = 1;
    s += 2;  /* skip '0x' */
    for (; lisxdigit(cast_uchar(*s)); s++, c++) {
      olda = a;
      a = a * 16ULL + luaO_hexavalue(*s);
      if (olda > a || c > 16) return NULL;  /* overflow, extended 6.2.1 */
      empty = 0;
    }
  }
  else {  /* decimal */
    for (; lisdigit(cast_uchar(*s)); s++) {
      int d = *s - '0';
      if (a >= MAXBY10 && (a > MAXBY10 || d > MAXLASTD + neg))  /* overflow? */
        return NULL;  /* do not accept it (as integer) */
      a = a * 10 + d;
      empty = 0;
    }
  }
  while (lisspace(cast_uchar(*s))) s++;  /* skip trailing spaces */
  if (empty || *s != '\0') return NULL;  /* something wrong in the numeral */
  else {
    *result = (neg) ? 0ULL - a : a;
    return s;
  }
}


const char *luaO_str2int64 (const char *s, int64_t *result) {  /* 6.2.0 */
  int64_t a = 0LL;
  int empty = 1;
  int neg;
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);
  if (s[0] == '0' &&
     (s[1] == 'x' || s[1] == 'X')) {  /* hex? */
    s += 2;  /* skip '0x' */
    uint64_t b = 0ULL;
    for (; lisxdigit(cast_uchar(*s)); s++) {
      b = b * 16ULL + luaO_hexavalue(*s);
      if (!neg && (b > 0x7FFFFFFFFFFFFFFFULL)) { *result = 1; return NULL; }  /* overflow, extended 6.2.1 */
      if ( neg && (b > 0x8000000000000000ULL)) { *result = 1; return NULL; }  /* underflow, extended 6.2.1 */
      empty = 0;
    }
    a = (int64_t)b;
  }
  else {  /* decimal */
    for (; lisdigit(cast_uchar(*s)); s++) {
      int d = *s - '0';
      if (a >= MAXBY10 && (a > MAXBY10 || d > MAXLASTD + neg)) { /* overflow? */
        *result = -1; return NULL;  /* do not accept it (as integer) */
      }
      a = a * 10 + d;
      empty = 0;
    }
  }
  while (lisspace(cast_uchar(*s))) s++;  /* skip trailing spaces */
  if (empty || *s != '\0') { *result = 3; return NULL; } /* something wrong in the numeral */
  else {
    *result = (neg) ? 0LL - a : a;
    return s;
  }
}


/* On PowerPC and ARM l_str2int does seem to not work correctly and returns wrong results; OS/2, DOS, Windows, x86 Linux &
   x86 Mac OS X are fine, however. This bug affects Agena only, in Lua 5.4.0 RC 5 and 6 it works just fine at least on
   ARM Raspbian. Tip from the Lua community: check LUA_INTEGER. XXX Fix me. */
#if (defined(__i386__) || defined(__x86_64__))
#define ACTIVATE   1
#else
#define ACTIVATE   0
#endif

size_t luaO_str2num (const char *s, TValue *o) {
  ptrdiff_t i; lua_Number n;
  const char *e;
  if (ACTIVATE && ((e = l_str2int(s, &i)) != NULL)) {  /* try as an integer */
    setnvalue(o, i);
  }
  else if ((e = l_str2d(s, &n)) != NULL) {  /* else try as a float */
    setnvalue(o, n);
  }
  else
    return 0;  /* conversion failed */
  return ((unsigned char*)e - (unsigned char*)s) + 1;  /* success; return string size */
}


static lua_Number numarith (int op, lua_Number v1, lua_Number v2) {
  switch (op) {
    case OP_ADD:    return luai_numadd(v1, v2);
    case OP_SUB:    return luai_numsub(v1, v2);
    case OP_MUL:    return luai_nummul(v1, v2);
    case OP_DIV:    return luai_numdiv(v1, v2);
    case OP_POW:    return luai_numpow(v1, v2);
    case OP_INTDIV: return luai_numintdiv(v1, v2);
    case OP_UNM:    return luai_numunm(v1);
    case OP_MOD:    return luai_nummod(v1, v2);
    default:        lua_assert(0); return 0;
  }
}

int luaO_rawarith (lua_State *L, int op, const TValue *p1, const TValue *p2,
                   TValue *res) {
  switch (op) {
    case OP_DIV: case OP_POW: {  /* operate only on floats */
      lua_Number n1; lua_Number n2;
      if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(op, n1, n2));
        return 1;
      }
      else return 0;  /* fail */
    }
    default: {  /* other operations */
      lua_Number n1; lua_Number n2;
      if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(op, n1, n2));
        return 1;
      }
      else return 0;  /* fail */
    }
  }
}

void luaO_arith (lua_State *L, int op, const TValue *p1, const TValue *p2,
                 StkId res) {
  if (!luaO_rawarith(L, op, p1, p2, s2v(res))) {
    /* could not perform raw operation; try metamethod */
    if (!call_binTM(L, p1, p2, res, cast(TMS, (op - LUA_OPADD) + TM_ADD)))
      luaG_typeerror(L, p1, "perform arithmetic on");
  }
}

int luaO_utf8esc (char *buff, unsigned long x) {
  int n = 1;  /* number of bytes put in buffer (backwards) */
  lua_assert(x <= 0x7FFFFFFFu);
  if (x < 0x80)  /* ascii? */
    buff[UTF8BUFFSZ - 1] = cast_char(x);
  else {  /* need continuation bytes */
    unsigned int mfb = 0x3f;  /* maximum that fits in first byte */
    do {  /* add continuation bytes */
      buff[UTF8BUFFSZ - (n++)] = cast_char(0x80 | (x & 0x3f));
      x >>= 6;  /* remove added bits */
      mfb >>= 1;  /* now there is one less bit available in first byte */
    } while (x > mfb);  /* still needs continuation byte? */
    buff[UTF8BUFFSZ - n] = cast_char((~mfb << 1) | x);  /* add first byte */
  }
  return n;
}

/* === end Lua 5.4.0 =========================================================================== */


static void pushstr (lua_State *L, const char *str) {
  setsvalue2s(L, L->top, luaS_new(L, str));
  incr_top(L);
}


/* the function pushes and returns a format string, not the formatted value, it handles only `%d', `%c', %f, %lf, %p, and `%s' formats */
const char *luaO_pushvfstring (lua_State *L, const char *fmt, va_list argp) {
  int n = 1;
  pushstr(L, "");
  for (;;) {
    const char *e = strchr(fmt, '%');
    if (e == NULL) break;
    setsvalue2s(L, L->top, luaS_newlstr(L, fmt, e - fmt));
    incr_top(L);
    switch (*(e + 1)) {
      case 's': {
        const char *s = va_arg(argp, char *);
        if (s == NULL) s = "(null)";
        pushstr(L, s);
        break;
      }
      case 'c': {
        char buff[2];
        buff[0] = cast(char, va_arg(argp, int));
        buff[1] = '\0';
        pushstr(L, buff);
        break;
      }
      case 'd': {
        setnvalue(L->top, cast_num(va_arg(argp, int)));
        incr_top(L);
        break;
      }
      case 'u': {  /* 2.12.0 */
        setnvalue(L->top, cast_num(va_arg(argp, unsigned int)));
        incr_top(L);
        break;
      }
      case 'f': {
        setnvalue(L->top, cast_num(va_arg(argp, l_uacNumber)));
        incr_top(L);
        break;
      }
      case 'l': {  /* 2.32.7 */
        if (*(e + 2) == 'f') {
          char buf[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
          /* This tip by Kai Tietz saved my day (prepend __mingw_): https://mingw-w64-public.narkive.com/iAkoBOXk/missing-long-double-format-in-printf
             On Intel, long doubles do not have 128 bits, but only 80, so the max number of decimal digits is ~19.2; see:
             https://stackoverflow.com/questions/33076474/strtold-parsing-lower-number-than-given-number */
          xsprintf(buf, "%.19LF", va_arg(argp, long double));
          e++;
          pushstr(L, buf);
        } else if (*(e + 2) == 'e') {  /* 2.41.2 */
          char buf[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
          xsprintf(buf, "%.19Le", va_arg(argp, long double));
          e++;
          pushstr(L, buf);
        } else {
          luaG_runerror(L, "unknown specifier l%c", *(e + 2));
        }
        break;
      }
      case 'p': {
        char buff[4*sizeof(void *) + 8]; /* should be enough space for a `%p' */
        sprintf(buff, "%p", va_arg(argp, void *));
        pushstr(L, buff);
        break;
      }
      case '%': {
        pushstr(L, "%");
        break;
      }
      default: {
        char buff[3];
        buff[0] = '%';
        buff[1] = *(e + 1);
        buff[2] = '\0';
        pushstr(L, buff);
        break;
      }
    }
    n += 2;
    fmt = e + 2;
  }
  pushstr(L, fmt);
  luaV_concat(L, n + 1, cast_int(L->top - L->base) - 1);
  L->top -= n;
  return svalue(L->top - 1);
}


const char *luaO_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  return msg;
}


void luaO_chunkid (char *out, const char *source, size_t bufflen) {
  if (*source == '=') {
    strncpy(out, source + 1, bufflen);  /* remove first char */
    out[bufflen - 1] = '\0';  /* ensures null termination */
  }
  else {  /* out = "source", or "...source" */
    if (*source == '@') {
      size_t l;
      source++;  /* skip the `@' */
      bufflen -= sizeof(" '...' ");
      l = tools_strlen(source);  /* 2.17.8 tweak */
      strcpy(out, "");
      if (l > bufflen) {
        source += (l - bufflen);  /* get last part of file name */
        strcat(out, "...");
      }
      strcat(out, source);
    }
    else {  /* out = [string "string"] */
      size_t len = strcspn(source, "\n\r");  /* stop at first newline */
      bufflen -= sizeof(" [string \"...\"] ");
      if (len > bufflen) len = bufflen;
      strcpy(out, "[string \"");
      if (source[len] != '\0') {  /* must truncate? */
        strncat(out, source, len);
        strcat(out, "...");
      }
      else
        strcat(out, source);
      strcat(out, "\"]");
    }
  }
}

/*
** {============================================================================================
** Lua's implementation for 'lua_strx2number', taken from Lua 5.4.4, 2.27.10, generalised 2.39.4
** =============================================================================================
** Convert a hexadecimal, octal or binary numeric string to a number, following C99 specification for 'strtod';
** input including fractions checked with: https://www.binaryhexconverter.com/octal-to-decimal-converter.
*/

/* maximum number of significant digits to read (to avoid overflows
   even with single floats) */
#define MAXSIGDIG  30

static int getmaxsigdig (int base) {
  switch (base) {
     case  2: return MAXSIGDIG*3;
     case  8: return MAXSIGDIG*2;
     default: return MAXSIGDIG;  /* especially for hexadecimals */
  }
}

lua_Number lua_strany2number (const char *s, char **endptr, int base,
                              char tbase, char Tbase, int (*f)(unsigned char),
                              int check, int *overflow) {
  int dot = lua_getlocaledecpoint();
  lua_Number r = l_mathop(0.0);  /* result (accumulator) */
  int sigdig = 0;  /* number of significant digits */
  int nosigdig = 0;  /* number of non-significant digits */
  int e = 0;  /* exponent correction */
  int neg;  /* 1 if number is negative */
  int hasdot = 0;  /* true after seen a dot */
  int maxsigdig = getmaxsigdig(base);
  *endptr = cast_charp(s);  /* nothing is valid yet */
  *overflow = 0;
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);  /* check sign */
  if (check && !(*s == '0' && (*(s + 1) == tbase || *(s + 1) == Tbase)))  /* check '0<basetoken>' */
    return l_mathop(0);  /* invalid format (no '0<basetoken>') */
  for (s += check*2; ; s++) {  /* skip '0<basetoken>' and read numeral */
    if (*s == dot) {
      if (hasdot) break;  /* second dot? stop loop */
      else hasdot = 1;
    }
    else if ((*f)(cast_uchar(*s))) {
      if (sigdig == 0 && *s == '0')  /* non-significant digit (zero)? */
        nosigdig++;
      else if (++sigdig <= maxsigdig)  /* can read it without overflow? */
          r = (r * l_mathop(base)) + luaO_hexavalue(*s);
      else {
        e++; /* too many digits; ignore, but still count for exponent */
        *overflow = 1;
      }
      if (hasdot) e--;  /* decimal digit? correct exponent */
    }
    else break;  /* neither a dot nor a digit */
  }
  if (nosigdig + sigdig == 0)  /* no digits? */
    return l_mathop(0.0);  /* invalid format */
  *endptr = cast_charp(s);  /* valid up to here */
  e *= luaO_log2(base);  /* each digit multiplies/divides value by 2^3 (octal), 2^4 (hex), ... */
  if (*s == 'p' || *s == 'P') {  /* exponent part? */
    int exp1 = 0;  /* exponent value */
    int neg1;  /* exponent sign */
    s++;  /* skip 'p' */
    neg1 = isneg(&s);  /* sign */
    if (!lisdigit(cast_uchar(*s))) {
      return l_mathop(0.0);  /* invalid; must have at least one digit */
    }
    while (lisdigit(cast_uchar(*s)))  /* read exponent */
      exp1 = exp1*10 + *(s++) - '0';
    if (neg1) exp1 = -exp1;
    e += exp1;
    *endptr = cast_charp(s);  /* valid up to here */
  }
  if (neg) r = -r;
  return sun_ldexp(r, e);  /* 2.27.10 `fix' */
}

/* }====================================================== */

