/*
** $Id: lmathlib.c,v 1.67 2005/08/26 17:36:32 roberto Exp $
** Standard mathematical library
** See Copyright Notice in agena.h
*/

#include <math.h>
#include <stdint.h>  /* for UINT32_MAX */
#include <stdlib.h>
#include <string.h>

#define lmathlib_c
#define LUA_LIB
#define LUA_CORE  /* just to import agnconf.h's luai_num* macros */

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"  /* for trunc and isfinite, and FLT_EVAL_METHOD constant, and off64* types */
#include "agnconf.h"
#include "agnxlib.h"
#include "cephes.h"
#include "charbuf.h"
#include "dual.h"
#include "lcomplex.h"
#include "llimits.h"
#include "numtheory.h"  /* for nt_powermodint */

/* For unknown reasons I am too tired to clarify, we have to import the *GET_*, *SET_*, *INSERT*, *EXTRACT*,
   etc. Sun Microsystems macros from the following header file instead of agnhlps.h: */
#include "sunpro.h"


static int math_min (lua_State *L) {  /* changed 2.14.4; with just two arguments, fmin is slower */
  int i, n;
  lua_Number d, dmin;
  n = lua_gettop(L);  /* number of arguments */
  dmin = agn_checknumber(L, 1);
  for (i=2; i <= n; i++) {
    d = agn_checknumber(L, i);
    if (d < dmin) dmin = d;
  }
  lua_pushnumber(L, dmin);
  return 1;
}


static int math_max (lua_State *L) {  /* changed 2.14.4; with just two arguments, fmax is slower */
  int i, n;
  lua_Number d, dmax;
  n = lua_gettop(L);  /* number of arguments */
  dmax = agn_checknumber(L, 1);
  for (i=2; i <= n; i++) {
    d = agn_checknumber(L, i);
    if (d > dmax) dmax = d;
  }
  lua_pushnumber(L, dmax);
  return 1;
}


static int math_randoms (lua_State *L) {
  /* RNG algorithm taken from https://en.wikipedia.org/w/index.php?title=Random_number_generation&diff=778577551&oldid=612091085,
     written by George Marsaglia. 0.32.2, 22.05.2010 */
  int nargs, mode;
  nargs = lua_gettop(L);
  if (nargs == 3 && lua_isnoneornil(L, 3)) nargs--;
  mode = (nargs > 0 && lua_type(L, nargs) == LUA_TBOOLEAN);  /* 2.8.3 fix; 2.27.4 for uuid */
  switch (nargs - mode) {  /* check number of arguments */
    case 0: {  /* no arguments */
      lua_pushnumber(L, tools_random(mode));  /* Number between 0 and 1 */
      break;
    }
    case 1: {  /* only upper limit given */
      lua_Number u;
      u = agn_checknumber(L, 1);  /* 2.10.1 fix */
      if (u < 1)  /* 2.10.1 improvement */
        luaL_error(L, "Error in " LUA_QS ": border must be positive.", "math.random");
      lua_pushnumber(L, tools_randomrange(1, u, mode));  /* 2.14.3 fix, avoid low frequency */
      break;
    }
    case 2: {  /* lower and upper limits given */
      lua_Number l, u;
      l = agn_checknumber(L, 1);  /* 2.10.1 fix */
      u = agn_checknumber(L, 2);  /* 2.10.1 fix */
      if (u < l)  /* 2.10.1 improvement */
        luaL_error(L, "Error in " LUA_QS ": upper border is less than lower border.", "math.random");
      lua_pushnumber(L, tools_randomrange(l, u, mode));  /* 2.14.3 fix, avoid low frequency */
      break;
    }
    default:
      return luaL_error(L, "Error in " LUA_QS ": wrong arguments given.", "math.random");  /* 2.10.1, 2.14.6 changed */
  }
  return 1;
}


LUAI_UINT32 m_w = AGN_RANDOM_MW; /* must not be zero, nor 0x464fffff; for definition, see agnconf.h */
LUAI_UINT32 m_z = AGN_RANDOM_MZ; /* must not be zero, nor 0x9068ffff; for definition, see agnconf.h */

static int math_randomseeds (lua_State *L) {  /* changed 0.32.2, 23.05.2010, extended 2.10.1 */
  if (lua_gettop(L) != 0) {
    lua_Number w, z;
    w = agn_checkposint(L, 1);
    z = agn_checkposint(L, 2);
    /* do not check for w = 0x464fffff & z = 0x9068ffff, as being warned by George Marsaglia;
       set seeds to global C domain ... */
    m_w = (LUAI_UINT32)w;
    m_z = (LUAI_UINT32)z;
    /* ... and return them */
  }
  /* if you change the number of results, CHANGE environ_restart, as well ! */
  lua_pushnumber(L, m_w);  /* do NOT use lua_pushinteger as there might be overflows, 2.15.1 fix */
  lua_pushnumber(L, m_z);  /* do NOT use lua_pushinteger as there might be overflows, 2.15.1 fix */
  return 2;
}


/* constant found in Cephes Math Library Release 2.8, June, 2000 by Stephen L. Moshier
   file sin.c (and fixed it) */
#define P64800   (4.8481368110953599358991410e-6)  /* = (Pi/180)/3600 */

/* Takes a number representing decimal degrees and splits it into the DMS parts hour, minute, second plus the sign. hour, minute, second
   will always be nonnegative, so you might check sign after calling this auxiliary function. 3.16.1 */
static FORCE_INLINE void aux_tosgesim (lua_Number x, long long int *hrs, long long int *min, long long int *sec, long long int *sign) {
  if (x < 0) {
    x = -x;
    *sign = -1;
  } else
    *sign = 1;
  *sec = sun_floor(3600.0*x + 0.5);
  *hrs = *sec/3600;
  *sec = fabs(fmod(*sec, 3600.0));
  *min = *sec/60;
  *sec = fmod(*sec, 60);
}

/* converts degrees to radians */
static int math_toradians (lua_State *L) {
  lua_Number h, m, s, sign;
  if (lua_istable(L, 1)) {  /* 5.5.3 extension */
    int rc;
    h = agn_rawgetinumber(L, 1, 1, &rc);  /* defaults to 0 */
    m = agn_rawgetinumber(L, 1, 2, &rc);  /* ditto */
    s = agn_rawgetinumber(L, 1, 3, &rc);  /* ditto */
  } else {
    h = agn_checknumber(L, 1);
    m = agnL_optnumber(L, 2, 0);
    s = agnL_optnumber(L, 3, 0);
  }
  if (tools_isfrac(h)) {  /* 5.5.4 */
    long long int hrs, min, sec, sign;
    aux_tosgesim(h, &hrs, &min, &sec, &sign);
    h = (lua_Number)(sign*hrs);
    m = (lua_Number)(sign*min);
    s = (lua_Number)(sign*sec);
  }
  sign = 1.0;
  if (h < 0.0) {  /* 3.16.1 fix, streamline minutes and seconds with a negative hour */
    sign = -1.0;
    h = -h;
    m = fabs(m);
    s = fabs(s);
  }
  lua_pushnumber(L, sign*((h*60.0 + m)*60.0 + s)*P64800);
  return 1;
}


/* converts radians to degrees, 3.16.1 */
static int math_todegrees (lua_State *L) {
  int nargs = lua_gettop(L);
  lua_Number r = agn_checknumber(L, 1)*RAD2DEG;
  if (nargs > 1) {  /* 5.5.4 extension */
    long long int hrs, min, sec, sign;
    aux_tosgesim(r, &hrs, &min, &sec, &sign);
    lua_pushnumber(L, sign*hrs);
    lua_pushnumber(L, sign*min);
    lua_pushnumber(L, sign*sec);
  } else {
    lua_pushnumber(L, r);
  }
  return 1 + 2*(nargs > 1);
}


/* 2.8.4, see http://stackoverflow.com/questions/13975745/the-fastest-way-to-get-current-quadrant-of-an-angle,
   solution #2 posted by Vincent Mimoun-Prat.
   The function returns the quadrant of an angle given in radians and returns an integer in [1, 4]. */
static int math_quadrant (lua_State *L) {
  lua_Number angle = fmod(agn_checknumber(L, 1)/RADIANS_PER_DEGREE, 360.0);  /* [0..360) if angle is positive, (-360..0] if negative */
  angle += (angle < 0)*360.0;  /* back to [0..360) */
  lua_pushnumber(L, fmod(sun_trunc(angle/90), 4) + 1);  /* the quadrant, 2.11.0 tuning */
  return 1;
}


/* converts sexagesimal, passed as a triple, to decimal */
static int math_todecimal (lua_State *L) {
  lua_Number h, m, s, sign;
  if (lua_istable(L, 1)) {  /* 5.5.3 extension */
    int rc;
    h = agn_rawgetinumber(L, 1, 1, &rc);  /* defaults to 0 */
    m = agn_rawgetinumber(L, 1, 2, &rc);  /* ditto */
    s = agn_rawgetinumber(L, 1, 3, &rc);  /* ditto */
  } else {
    h = agn_checknumber(L, 1);
    m = agnL_optnumber(L, 2, 0);
    s = agnL_optnumber(L, 3, 0);
  }
  if (h < 0) {  /* 2.9.8 improvement */
    sign = -1;   /* 2.2.0 patch */
    h = -h;
    m = fabs(m);  /* 3.16.1 fix, streamline minutes and seconds with a negative hour */
    s = fabs(s);
  } else
    sign = 1;
  lua_pushnumber(L, sign*(h + m/60.0 + s/3600.0));
  return 1;
}


/* Converts a number representing a decimal number x into its sexagesimal representation consisting of three numbers:
   degrees, minutes, and seconds. For example: 10.5125 returns 10, 30, 45. */
static int math_tosgesim (lua_State *L) {   /* based on math.dms, 2.9.8, replacing the former - inprecise - Agena 1.12.7 version */
  long long int hrs, min, sec, sign;  /* do not use int or long int due to possible overflows */
  int nargs = lua_gettop(L), nrets = 3;
  lua_Number x = agn_checknumber(L, 1);
  aux_tosgesim(x, &hrs, &min, &sec, &sign);
  if (nargs == 1) {
    luaL_checkstack(L, 3, "not enough stack space");
    lua_pushnumber(L, sign*hrs);
    lua_pushnumber(L, sign*min);
    lua_pushnumber(L, sign*sec);
  } else {  /* 5.5.3 extension */
    lua_createtable(L, 3, 0);
    lua_rawsetinumber(L, -1, 1, sign*hrs);
    lua_rawsetinumber(L, -1, 2, sign*min);
    lua_rawsetinumber(L, -1, 3, sign*sec);
    nrets = 1;
  }
  return nrets;
}


/* Converts a number representing a decimal number x into its TI-30 sexagesimal DMS representation and returns a number.
   For example: 10.5125 returns 10.3045, representing 10°30'45''.

   Based on http://stackoverflow.com/questions/6862684/converting-from-decimal-degrees-to-degrees-minutes-seconds-tenths,
   solution #4 proposed by user807566.

   Do not use the formula math.dms := proc(x :: number) is local f := frac(x)*60; return int(x) + int(f)/100 + frac(f)/100 end;
   or any version also detecting the sign of x for it is prone to slight rounding errors in loops with fractional step sizes. */
static int math_dms (lua_State *L) {   /* rewritten in C, 2.8.3, 25 percent faster than the former - inprecise - Agena version */
  long long int hrs, min, sec, sign;  /* do not use int or long int due to possible overflows */
  lua_Number x = agn_checknumber(L, 1);
  aux_tosgesim(x, &hrs, &min, &sec, &sign);
  lua_pushnumber(L, sign*((lua_Number)hrs + (lua_Number)min/100.0 + (lua_Number)sec/10000.0));
  return 1;
}


/* Converts a number in DMS notation to its decimal representation; 10.3045, representing 10°30'45'', returns 10.5125; 2.34.5 */
static int math_dd (lua_State *L) {
  long long int deg, min, sec, sign;  /* do not use int or long int due to possible overflows */
  lua_Number x = agn_checknumber(L, 1);
  if (x < 0) {
    x = -x;
    sign = -1;
  } else
    sign = 1;
  sec = sun_floor(10000.0*x + 0.5);
  deg = sec/10000.0;
  sec = fabs(fmod(sec, 10000.0));  /* fmod computes the remainder of numerator divided by denominator */
  min = sec/100.0;
  sec = fmod(sec, 100.0);
  lua_pushnumber(L, sign*((lua_Number)deg + (lua_Number)min/60.0 + (lua_Number)sec/3600.0));
  return 1;
}


static int math_splitdms (lua_State *L) {  /* 3.16.1 */
  long long int deg, min, sec, sign;  /* do not use int or long int due to possible overflows */
  int nargs = lua_gettop(L), nret = 3;
  lua_Number x = agn_checknumber(L, 1);
  if (x < 0) {
    x = -x;
    sign = -1;
  } else
    sign = 1;
  sec = sun_floor(10000.0*x + 0.5);
  deg = sec/10000.0;
  sec = fabs(fmod(sec, 10000.0));  /* fmod computes the remainder of numerator divided by denominator */
  min = sec/100.0;
  sec = fmod(sec, 100.0);
  if (nargs == 1) {
    luaL_checkstack(L, 3, "not enough stack space");
    lua_pushnumber(L, sign*deg);
    lua_pushnumber(L, sign*min);
    lua_pushnumber(L, sign*sec);
  } else {  /* 5.5.3 extension */
    lua_createtable(L, 3, 0);
    lua_rawsetinumber(L, -1, 1, sign*deg);
    lua_rawsetinumber(L, -1, 2, sign*min);
    lua_rawsetinumber(L, -1, 3, sign*sec);
    nret = 1;
  }
  return nret;
}


/* Converts a non-negative integer in the range [0, 255] to its hexadecimal representation, returned as a 2-character string. */
static int math_tohex (lua_State *L) {  /* 2.11.3, extended 4.7.4, hcnaged 6.7.8 */
  agnL_pushhex(L, 1, agnL_optboolean(L, 2, 0));
  return 1;
}


#define testconstprefix(obj,l,sign) { \
  if (l > 0 && obj[0] == '-') { \
    l--; obj++; sign = -1; \
  } else if (l > 0 && obj[0] == '+') { \
    l--; obj++; \
  } \
  if (l > 1 && obj[0] == '0' && (obj[1] == 'o' || obj[1] == 'O')) { \
    l -= 2; obj += 2; \
  } \
  if (l > 0 && obj[0] == '-') { \
    l--; obj++; sign *= -1; \
  } else if (l > 0 && obj[0] == '+') { \
    l--; obj++; \
  } \
}

/* Converts a string s representing a hexadecimal value to a decimal number. The string may or may not start with `0x`, `0X`, and also may
   contain a sign, a fractional or a `p exponent` part. See also `tonumber`, `math.tohex`, `math.convertbase`. */
#define HEXALPHA     "-.0123456789abcdefABCDEFpP"
#define HEXALPHALEN  26
static int math_hextodec (lua_State *L) {  /* 2.39.3 */
  size_t l;
  char *hex, *endptr;
  int overflow;
  double sign, r;
  sign = 1;
  hex = (char *)agn_checklstring(L, 1, &l);
  testconstprefix(hex, l, sign);
  if (l == 0)
    luaL_error(L, "Error in " LUA_QS ": input is invalid or the empty string.", "math.hextodec");
  if (!tools_strinalphabet(hex, HEXALPHA, l, HEXALPHALEN))
    luaL_error(L, "Error in " LUA_QS ": input is invalid.", "math.hextodec");
  r = lua_strany2number(hex, &endptr, 16, 'x', 'X', &lishdigit, 0, &overflow);
  lua_pushnumber(L, overflow ? AGN_NAN : sign*r);
  return 1;
}


#define OCTALPHA     ".01234567pB"
#define OCTALPHALEN  11

static int math_octtodec (lua_State *L) {  /* 2.39.3 */
  size_t l;
  char *oct, *endptr;
  double sign, r;
  int overflow;
  sign = 1;
  oct = (char *)agn_checklstring(L, 1, &l);
  testconstprefix(oct, l, sign);
  if (l == 0)
    luaL_error(L, "Error in " LUA_QS ": input is invalid or the empty string.", "math.octtodec");
  if (!tools_strinalphabet(oct, OCTALPHA, l, OCTALPHALEN))
    luaL_error(L, "Error in " LUA_QS ": input is invalid.", "math.octtodec");
  /* 2.39.4 extension for range, fractions, p-exponent */
  r = lua_strany2number(oct, &endptr, 8, 'o', 'O', &lisodigit, 0, &overflow);
  lua_pushnumber(L, overflow ? AGN_NAN : sign*r);
  return 1;
}


#define BINALPHA     ".01pB"
#define BINALPHALEN  5
static int math_bintodec (lua_State *L) {  /* 2.39.4 */
  size_t l;
  char *bin, *endptr;
  double sign, r;
  int overflow;
  sign = 1;
  bin = (char *)agn_checklstring(L, 1, &l);
  testconstprefix(bin, l, sign);
  if (l == 0)
    luaL_error(L, "Error in " LUA_QS ": input is invalid or the empty string.", "math.bintodec");
  if (!tools_strinalphabet(bin, BINALPHA, l, BINALPHALEN))
    luaL_error(L, "Error in " LUA_QS ": input is invalid.", "math.bintodec");
  r = lua_strany2number(bin, &endptr, 2, 'b', 'B', &lisbdigit, 0, &overflow);
  lua_pushnumber(L, overflow ? AGN_NAN : sign*r);
  return 1;
}


static int math_nextafter (lua_State *L) {
  lua_Number dir;  /* 2.10.0 improvement */
  dir = agnL_optnumber(L, 2, HUGE_VAL);
  lua_pushnumber(L, sun_nextafter(agn_checknumber(L, 1), dir));
  return 1;
}


/* Returns x rounded to the nearest integer, returns the same result as round(x, 0) does but is implemented differently and 5 % faster.
   The function has been included for C math library compatibility reasons only. 2.35.1, extended 4.12.0
   In the second form, if any second positive number eps is given and if non-integral x is very close to its
   nearest integer n, returns this integer n if |x - n| <= eps, and its argument x otherwise. */
static int math_nearbyint (lua_State *L) {
  if (lua_gettop(L) == 1) {
    lua_pushnumber(L, rint(agn_checknumber(L, 1)));
  } else {
    /* adapt := << (x :: number) with n := int(x) -> math.zeroin(sign(x)*(x |- n), 10*DoubleEps) + n >>; 4.12.0;
       C version almost twice as fast as the Agena pendant. */
    lua_Number x, intx, eps;
    x = agn_checknumber(L, 1);
    intx = rint(x);  /* round to nearest integer, not towards zero. */
    eps = agn_checkpositive(L, 2);
    lua_pushnumber(L, (fabs(x - intx) <= eps) ? intx : x);
  }
  return 1;
}


/* Converts the number x in the scale [a1, a2] to one in the scale [b1, b2]. The second and third arguments
   must be pairs of numbers. If the third argument is missing, then x is converted to a number in [0, 1].
   The return is a number. */
static int math_norm (lua_State *L) {
  lua_Number x, a1, a2, b1, b2;
  int t3;
  x = agn_checknumber(L, 1);
  agnL_pairgetinumbers(L, "math.norm", 2, &a1, &a2);  /* 2.9.8; 2.10.1 does not delete pair from arg list */
  if (x < a1 || x > a2)
    luaL_error(L, "Error in " LUA_QS ": first argument out of range.", "math.norm");
  if (a1 > a2)  /* 2.2.1 */
    luaL_error(L, "Error in " LUA_QS ": left border greater than right border in second argument.", "math.norm");
  t3 = lua_type(L, 3);
  if (t3 == LUA_TNONE) {
    b1 = 0; b2 = 1;
  }
  else {  /* 2.10.1 */
    agnL_pairgetinumbers(L, "math.norm", 3, &b1, &b2);  /* 2.9.8 */
    if (b1 > b2)  /* 2.2.1 */
      luaL_error(L, "Error in " LUA_QS ": left border greater than right border in third argument.", "math.norm");
  }
  lua_pushnumber(L, b1 + (x - a1) * (b2 - b1)/(a2 - a1));  /* 2.2.1 fix */
  return 1;
}


/* Returns the largest power of two less than or equal to x, where x is a non-negative integer. If x <= 1, the result is x;
   if x < 0, undefined is returned. See also: math.ceilpow2. */
static int math_floorpow2 (lua_State *L) {  /* 2.16.0 */
  lua_Number x = agn_checknumber(L, 1);  /* 2.16.0 */
  if (l_unlikely(x < 0))  /* 2.16.0 optimisation */
    lua_pushundefined(L);
  else if (l_unlikely(sun_exponent(x) > 30))  /* return fail with x >= 2^31, 2.16.0 optimisation */
    lua_pushfail(L);
  else {
    unsigned int v = x;
    v -= (v & (v - 1)) != 0;  /* if v is not a power of 2, decrement v by 1 */
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v &= ~(v >> 1);
    lua_pushinteger(L, v);
  }
  return 1;
}


/* Returns smallest exponent to 2 equals or greater than x, i.e. ilog2(x - 1) + 1 */
static int math_ceillog2 (lua_State *L) {  /* 2.3.1 */
  int x = agn_checkinteger(L, 1);
  if (l_unlikely(x < 1))  /* 2.3.3 improvement, 2.16.0 optimisation */
    lua_pushundefined(L);
  else
    lua_pushinteger(L, lua_log2(L, x - 1) + 1);
  return 1;
}


/* Returns the integral part of the logarithm of x to the base 2, but unlike `ilog2` working in unsigned 32-bit integer mode.
   It gives a direct interface to an underlying system-internal base-2 logarithm function that is used in memory management. */
static int math_ilog2 (lua_State *L) {  /* 2.29.0 UNDOC; implicit overflow guard by internal 4-byte cast */
  lua_pushinteger(L, lua_log2(L, agn_checknonnegint(L, 1)));  /* returns -1 if argument is zero */
  return 1;
}


/* The iterated logarithm of n, log*(n) (for "log star") returns the number of times the logarithm function to a given base b
   must be iteratively applied on n until the result reaches or drops below 1. If x <= 1, returns 0.

   The algorithm is equivalent to:

   > logs := proc(x, b) is
   >    for i from 0 while x > 1 do
   >        x := log(x, b)
   >    od;
   >    return i
   > end;  2.30.5 */

static FORCE_INLINE void aux_abslog (lua_Number *a, lua_Number b) {
  lua_Number ta, tb, td, la, lb;
  ta = sun_atan2(a[1], a[0]);
  tb = sun_atan2(0, b);
  la = sun_pytha(a[0], a[1]);
  la = sun_log(la);
  lb = 2.0*sun_log(b);
  td = lb*lb + 4*tb*tb;
  a[0] = (la*lb + 4*ta*tb)/td; a[1] = -2*((la*tb - ta*lb)/td);
}


#define LOGSMAXITER 1024
static int math_logs (lua_State *L) {
  int i;
  lua_Number b = agn_checknumber(L, 2);
  if (agn_isnumber(L, 1)) {
    lua_Number x = agn_tonumber(L, 1);
    if (b == 2) {
      for (i=0; x > 1; i++) x = sun_log2(x);
    } else if (b == 10) {
      for (i=0; x > 1; i++) x = sun_log10(x);
    } else if (b == EXP1) {
      for (i=0; x > 1; i++) x = sun_log(x);
    } else {
      for (i=0; x > 1; i++) x = tools_logbase(x, b);
    }
    lua_pushinteger(L, tools_isnan(x) ? 0 : i);  /* 4.3.2 change */
  } else if (lua_iscomplex(L, 1)) {  /* 4.5.2, the following code is very prone to round-off errors */
    int i, c, maxiter;
    lua_Number x[2], y[2], eps;
    x[0] = agn_complexreal(L, 1); x[1] = agn_compleximag(L, 1);
    y[0] = y[1] = 0.0;
    eps = agnL_optpositive(L, 3, agn_getdblepsilon(L));
    maxiter = agnL_optposint(L, 4, LOGSMAXITER);  /* 4.5.6 */
    aux_abslog(x, b);
    for (i=0, c=0; (c < maxiter) && (fabs(x[0] - y[0]) > eps || fabs(x[1] - y[1]) > eps); i++, c++) {
      y[0] = x[0]; y[1] = x[1];
      aux_abslog(x, b);
    }
    lua_pushinteger(L, i);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected a (complex) number for argument #1, got %s.", "math.logs", luaL_typename(L, 1));
  }
  return 1;
}


/* Finds the smallest power of two greater than or equal to x, taken from:
   http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2 */
static int math_ceilpow2 (lua_State *L) {  /* 2.3.3 */
  lua_Number x;
  x = agn_checknumber(L, 1);  /* 2.16.0 */
  if (l_unlikely(x < 0))  /* 2.16.0 optimisation */
    lua_pushundefined(L);
  else if (l_unlikely(sun_exponent(x) > 30))  /* return fail with x >= 2^31, 2.16.0 optimisation */
    lua_pushfail(L);
  else {
    unsigned int v = x;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    v += (v == 0);
    lua_pushnumber(L, v);
  }
  return 1;
}


/* Taken from: http://graphics.stanford.edu/~seander/bithacks.html#InterleaveTableLookup

Interleaves the bits of integers x and y, so that all of the bits of x are in the even
positions and y in the odd; the function can be used to linearising 2D integer coordinates,
combining x and y into a single integer that can be compared easily. The result has the property
that a number is usually close to another if their x and y values are close. */
static const unsigned short MortonTable256[256] = {
  0x0000, 0x0001, 0x0004, 0x0005, 0x0010, 0x0011, 0x0014, 0x0015,
  0x0040, 0x0041, 0x0044, 0x0045, 0x0050, 0x0051, 0x0054, 0x0055,
  0x0100, 0x0101, 0x0104, 0x0105, 0x0110, 0x0111, 0x0114, 0x0115,
  0x0140, 0x0141, 0x0144, 0x0145, 0x0150, 0x0151, 0x0154, 0x0155,
  0x0400, 0x0401, 0x0404, 0x0405, 0x0410, 0x0411, 0x0414, 0x0415,
  0x0440, 0x0441, 0x0444, 0x0445, 0x0450, 0x0451, 0x0454, 0x0455,
  0x0500, 0x0501, 0x0504, 0x0505, 0x0510, 0x0511, 0x0514, 0x0515,
  0x0540, 0x0541, 0x0544, 0x0545, 0x0550, 0x0551, 0x0554, 0x0555,
  0x1000, 0x1001, 0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015,
  0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 0x1054, 0x1055,
  0x1100, 0x1101, 0x1104, 0x1105, 0x1110, 0x1111, 0x1114, 0x1115,
  0x1140, 0x1141, 0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155,
  0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411, 0x1414, 0x1415,
  0x1440, 0x1441, 0x1444, 0x1445, 0x1450, 0x1451, 0x1454, 0x1455,
  0x1500, 0x1501, 0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515,
  0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551, 0x1554, 0x1555,
  0x4000, 0x4001, 0x4004, 0x4005, 0x4010, 0x4011, 0x4014, 0x4015,
  0x4040, 0x4041, 0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055,
  0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 0x4114, 0x4115,
  0x4140, 0x4141, 0x4144, 0x4145, 0x4150, 0x4151, 0x4154, 0x4155,
  0x4400, 0x4401, 0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415,
  0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 0x4454, 0x4455,
  0x4500, 0x4501, 0x4504, 0x4505, 0x4510, 0x4511, 0x4514, 0x4515,
  0x4540, 0x4541, 0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555,
  0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011, 0x5014, 0x5015,
  0x5040, 0x5041, 0x5044, 0x5045, 0x5050, 0x5051, 0x5054, 0x5055,
  0x5100, 0x5101, 0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115,
  0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 0x5154, 0x5155,
  0x5400, 0x5401, 0x5404, 0x5405, 0x5410, 0x5411, 0x5414, 0x5415,
  0x5440, 0x5441, 0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455,
  0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 0x5514, 0x5515,
  0x5540, 0x5541, 0x5544, 0x5545, 0x5550, 0x5551, 0x5554, 0x5555
};

static int math_morton (lua_State *L) {  /* 2.3.3 */
  unsigned short x, y;   /* Interleave bits of x and y, so that all of the
    bits of x are in the even positions and y in the odd; unsigned int z = 0;
    z gets the resulting Morton Number. */
  unsigned int z;
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  z = MortonTable256[y >> 8] << 17 |
    MortonTable256[x >> 8]   << 16 |
    MortonTable256[y & 0xFF] <<  1 |
    MortonTable256[x & 0xFF];
  lua_pushnumber(L, z);
  return 1;
}


/* Returns a value equivalent to exp(x) - 1, with x a number. It is computed in a way that is accurate even
   if x is near 0, since exp(~0) and 1 are nearly equal. See also: math.lnp1.

From: http://en.cppreference.com/w/cpp/numeric/math/expm1

   Parameters
arg   -   value of floating-point or Integral type
Return value

If no errors occur earg
-1 is returned.

If a range error due to overflow occurs, +HUGE_VAL, +HUGE_VALF, or +HUGE_VALL is returned.

If a range error occurs due to underflow, the correct result (after rounding) is returned.
Error handling

Errors are reported as specified in math_errhandling

If the implementation supports IEEE floating-point arithmetic (IEC 60559),

    If the argument is ±0, it is returned, unmodified
    If the argument is -?, -1 is returned
    If the argument is +?, +? is returned
    If the argument is NaN, NaN is returned

Notes

The functions std::expm1 and std::log1p are useful for financial calculations, for example, when calculating small daily interest rates: (1+x)n
-1 can be expressed as std::expm1(n * std::log1p(x)). These functions also simplify writing accurate inverse hyperbolic functions.

For IEEE-compatible type double, overflow is guaranteed if 709.8 < arg */
static int math_expm1 (lua_State *L) {  /* 2.3.3 */
  if (agn_isnumber(L, 1))
    lua_pushnumber(L, sun_expm1(agn_tonumber(L, 1)));  /* 2.11.1 tuning */
  else {
    trydualfix(L, "expm1", 1);
    luaL_nonumordual(L, 1, "expm1");
  }
  return 1;
}


/* Returns a value equivalent to ln(1 + x), with x a number. It is computed in a way that is accurate even
   if x is near zero.

   Example: ln(1.0000000000000001) -> 0, math.lnp1(0.0000000000000001)
   -> 1e-016. See also: math.expm1.

From: http://en.cppreference.com/w/cpp/numeric/math/log1p

If no errors occur ln(1+arg) is returned.

If a domain error occurs, an implementation-defined value is returned (NaN where supported)

If a pole error occurs, -HUGE_VAL, -HUGE_VALF, or -HUGE_VALL is returned.

If a range error occurs due to underflow, the correct result (after rounding) is returned.

Error handling

Errors are reported as specified in math_errhandling

Domain error occurs if arg is less than -1.

Pole error may occur if arg is -1.

If the implementation supports IEEE floating-point arithmetic (IEC 60559),

    If the argument is ±0, it is returned unmodified
    If the argument is -1, -infinity is returned and FE_DIVBYZERO is raised.
    If the argument is less than -1, NaN is returned and FE_INVALID is raised.
    If the argument is +infinity, +infinity is returned
    If the argument is NaN, NaN is returned

Some Notes:

The functions std::expm1 and std::log1p are useful for financial calculations, for example, when calculating small daily interest rates: (1+x)n
-1 can be expressed as std::expm1(n * std::log1p(x)). These functions also simplify writing accurate inverse hyperbolic functions. */
static int math_lnp1 (lua_State *L) {  /* 2.3.3 */
  if (agn_isnumber(L, 1)) {
    lua_Number x = agn_tonumber(L, 1);
    lua_pushnumber(L, (x <= -1) ? AGN_NAN : log1p(x));
  } else {
    trydualfix(L, "lnp1", 1);
    luaL_nonumordual(L, 1, "lnp1");
  }
  return 1;
}


/* Computes x - ln(1 + x) in a way that is accurate even if x is near zero. See also: `math.lnp1`. The algorithm is ten percent faster than simply
   returning rlog1(x) = x - log1p(x). */
static int math_xlnp1 (lua_State *L) {  /* 2.17.1 */
  lua_Number h, r, d, e, x, y;
  x = agn_checknumber(L, 1);
  if (x < -0.39 || x > 0.57) {
    lua_pushnumber(L, x - sun_log(x + 1));
    return 1;
  }
  if (x < -0.18) {
    h = x + 0.3;
    h /= 0.7;
    y = 0.566749439387324e-01 - h*0.3;
  } else if (x > 0.18) {
    h = 0.75*x - 0.25;
    y = 0.456512608815524e-01 + h/3;
  } else {  /* -0.18 <= x <= 0.18 */
    h = x;
    y = 0;
  }
  r = h/(h + 2);
  d = r*r;
  e = ((0.620886815375787e-02*d - 0.224696413112536)*d + 0.333333333333333)/((0.354508718369557*d - 0.127408923933623e+01)*d + 1);
  lua_pushnumber(L, 2*d*(1/(1 - r) - r*e) + y);
  return 1;
}


/* Returns `false` if at least one of its arguments is `undefined`, and `true` otherwise. */

static int math_isordered (lua_State *L) {  /* 2.3.3 */
  lua_pushboolean(L, !isunordered(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


/* Returns a number with the magnitude of x and the sign of y. It is a plain binding to
   C's copysign function and does not postprocess its result. */
static int math_copysign (lua_State *L) {  /* 2.3.3 */
  lua_pushnumber(L, copysign(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int math_signbit (lua_State *L) {  /* 2.4.2 */
  lua_pushboolean(L, signbit(agn_checknumber(L, 1)) != 0);
  return 1;
}


/* The function adds x and y using Kahan-Ozawa round-off error prevention and returns two numbers: the sum of x and y
   plus the updated value of the correction variable. The correction variable q should be 0 at first invocation.
   A typical usage should look like:

   s, q -> 0;
   to 10 do
      s, q := math.koadd(s, 0.1!0, q)
   od;
   print(s, q); */
#define aux_koadd(s,sold,x,q,u,v,w) { \
  v[0] = x[0] - q[0]; \
  sold[0] = s[0]; \
  s[0] += v[0]; \
  if (fabs(x[0]) < fabs(q[0])) { \
    t[0] = x[0]; \
    x[0] = -q[0]; \
    q[0] = t[0]; \
  } \
  u[0] = (v[0] - x[0]) + q[0]; \
  if (fabs(sold[0]) < fabs(v[0])) { \
    t[0] = sold[0]; \
    sold[0] = v[0]; \
    v[0] = t[0]; \
  } \
  w[0] = (s[0] - sold[0]) - v[0]; \
  q[0] = u[0] + w[0]; \
  v[1] = x[1] - q[1]; \
  sold[1] = s[1]; \
  s[1] += v[1]; \
  if (fabs(x[1]) < fabs(q[1])) { \
    t[1] = x[1]; \
    x[1] = -q[1]; \
    q[1] = t[1]; \
  } \
  u[1] = (v[1] - x[1]) + q[1]; \
  if (fabs(sold[1]) < fabs(v[1])) { \
    t[1] = sold[1]; \
    sold[1] = v[1]; \
    v[1] = t[1]; \
  } \
  w[1] = (s[1] - sold[1]) - v[1]; \
  q[1] = u[1] + w[1]; \
}

#define aux_kadd_getopt(L,idx,q,procname) { \
  if (lua_gettop(L) <= idx - 1) { \
    q[0] = q[1] = 0; \
  } else if (agn_isnumber(L, idx)) { \
    q[0] = agn_tonumber(L, idx); q[1] = 0; \
  } else if (lua_iscomplex(L, idx)) { \
    q[0] = agn_complexreal(L, idx); q[1] = agn_compleximag(L, idx); \
  } else { \
    luaL_error(L, "Error in " LUA_QS ": expected a (complex) number for arg #3, got %s.", procname, luaL_typename(L, idx)); \
  } \
}

#define aux_koadd_finalise(L,s,q) { \
  agn_pushcomplex(L, s[0], s[1]); \
  agn_pushcomplex(L, q[0], q[1]); \
}

static int math_koadd (lua_State *L) {  /* 2.4.1, Kahan-Ozawa summation, gets the accumulator s, an argument x to be added and a
  correction value q, returns updated s and q; extended to complex numbers 4.5.1 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      switch (lua_type(L, 2)) {
        case LUA_TNUMBER: {
          volatile lua_Number q, s, sold, u, v, w, x, t;
          s = agn_checknumber(L, 1);    /* cumulative sum */
          x = agn_checknumber(L, 2);    /* value to be added to sum */
          q = agnL_optnumber(L, 3, 0);  /* correction value */
          v = x - q;
          sold = s;
          s += v;
          if (fabs(x) < fabs(q)) {
            t = x;
            x = -q;
            q = t;
          }
          u = (v - x) + q;
          if (fabs(sold) < fabs(v)) {
            t = sold;
            sold = v;
            v = t;
          }
          w = (s - sold) - v;
          q = u + w;
          lua_pushnumber(L, s);  /* corrected sum */
          lua_pushnumber(L, q);
          break;
        }
        case LUA_TCOMPLEX: {
          volatile lua_Number q[2], s[2], sold[2], u[2], v[2], w[2], x[2], t[2];
          s[0] = agn_tonumber(L, 1); s[1] = 0;
          x[0] = agn_complexreal(L, 2); x[1] = agn_compleximag(L, 2);
          aux_kadd_getopt(L, 3, q, "math.koadd");
          aux_koadd(s, sold, x, q, u, v, w);
          aux_koadd_finalise(L, s, q);  /* corrected sum */
          break;
        }
        default:
          luaL_error(L, "Error in " LUA_QS ": expected a (complex) number for arg #2, got %s.", "math.koadd", luaL_typename(L, 2));
      }
      break;
    }
    case LUA_TCOMPLEX: {
      volatile lua_Number q[2], s[2], sold[2], u[2], v[2], w[2], x[2], t[2];
      s[0] = agn_complexreal(L, 1); s[1] = agn_compleximag(L, 1);
      switch (lua_type(L, 2)) {
        case LUA_TNUMBER:
          x[0] = agn_tonumber(L, 2); x[1] = 0;
          break;
        case LUA_TCOMPLEX:
          x[0] = agn_complexreal(L, 2); x[1] = agn_compleximag(L, 2);
          break;
        default:
          luaL_error(L, "Error in " LUA_QS ": expected a (complex) number for arg #2, got %s.", "math.koadd", luaL_typename(L, 2));
      }
      aux_kadd_getopt(L, 3, q, "math.koadd");
      aux_koadd(s, sold, x, q, u, v, w);
      aux_koadd_finalise(L, s, q);  /* corrected sum */
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": invalid arguments.", "math.koadd");
  }
  return 2;
}


#define aux_kbadd(s,x,cs,ccs,c,cc,t) { \
  t[0] = s[0] + x[0]; \
  c[0] = (fabs(s[0]) >= fabs(x[0])) ? (s[0] - t[0]) + x[0] : (x[0] - t[0]) + s[0]; \
  s[0] = t[0]; \
  t[0] = cs[0] + c[0]; \
  cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
  cs[0] = t[0]; \
  ccs[0] += cc[0]; \
  t[1] = s[1] + x[1]; \
  c[1] = (fabs(s[1]) >= fabs(x[1])) ? (s[1] - t[1]) + x[1] : (x[1] - t[1]) + s[1]; \
  s[1] = t[1]; \
  t[1] = cs[1] + c[1]; \
  cc[1] = (fabs(cs[1]) >= fabs(c[1])) ? (cs[1] - t[1]) + c[1] : (c[1] - t[1]) + cs[1]; \
  cs[1] = t[1]; \
  ccs[1] += cc[1]; \
}

#define aux_kbadd_finalise(L,s,cs,ccs) { \
  agn_pushcomplex(L, s[0], s[1]); \
  agn_pushcomplex(L, cs[0], cs[1]); \
  agn_pushcomplex(L, ccs[0], ccs[1]); \
}

static int math_kbadd (lua_State *L) {  /* 3.7.2, Kahan-Babuska summation, gets the accumulator s, an argument x to be added and
  two correction values cs, ccs, returns updates accumulator s (uncorrected) and cs, ccs. Extended to complex numbers 4.5.1.
  A typical usage should look like:
   s, cs, ccs -> 0;
   to 10 do
      s, cs, ccs := math.kbadd(s, 0.1, cs, ccs)
   od;
   print(s, cs, ccs); */
  luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      switch (lua_type(L, 2)) {
        case LUA_TNUMBER: {
          volatile lua_Number s, x, cs, ccs, t, c, cc;
          s  = agn_tonumber(L, 1);        /* cumulative sum */
          x  = agn_tonumber(L, 2);        /* value to be added to sum */
          cs = agnL_optnumber(L, 3, 0);   /* correction value */
          ccs = agnL_optnumber(L, 4, 0);  /* correction value */
          t = s + x;
          c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
          s = t;
          t = cs + c;
          cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
          cs = t;
          ccs += cc;
          lua_pushnumber(L, s);  /* contains _un_corrected sum s + x */
          lua_pushnumber(L, cs);
          lua_pushnumber(L, ccs);
          break;
        }
        case LUA_TCOMPLEX: {
          volatile lua_Number s[2], x[2], cs[2], ccs[2], t[2], c[2], cc[2];
          s[0] = agn_tonumber(L, 1); s[1] = 0;
          x[0] = agn_complexreal(L, 2); x[1] = agn_compleximag(L, 2);
          aux_kadd_getopt(L, 3, cs, "math.kbadd");
          aux_kadd_getopt(L, 4, ccs, "math.kbadd");
          aux_kbadd(s, x, cs, ccs, c, cc, t);
          aux_kbadd_finalise(L, s, cs, ccs);  /* corrected sum */
          break;
        }
        default:
          luaL_error(L, "Error in " LUA_QS ": expected a (complex) number for arg #2, got %s.", "math.kbadd", luaL_typename(L, 2));
      }
      break;
    }
    case LUA_TCOMPLEX: {
      volatile lua_Number s[2], x[2], cs[2], ccs[2], t[2], c[2], cc[2];
      s[0] = agn_complexreal(L, 1); s[1] = agn_compleximag(L, 1);
      switch (lua_type(L, 2)) {
        case LUA_TNUMBER:
          x[0] = agn_tonumber(L, 2); x[1] = 0;
          break;
        case LUA_TCOMPLEX:
          x[0] = agn_complexreal(L, 2); x[1] = agn_compleximag(L, 2);
          break;
        default:
          luaL_error(L, "Error in " LUA_QS ": expected a (complex) number for arg #2, got %s.", "math.kbadd", luaL_typename(L, 2));
      }
      aux_kadd_getopt(L, 3, cs, "math.kbadd");
      aux_kadd_getopt(L, 4, ccs, "math.kbadd");
      aux_kbadd(s, x, cs, ccs, c, cc, t);
      aux_kbadd_finalise(L, s, cs, ccs);  /* corrected sum */
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": invalid arguments.", "math.kbadd");
  }
  return 3;
}


static int math_examul (lua_State *L) {  /* exact multiplication, 3.11.2 */
  lua_Number t = 0;
  lua_pushnumber(L, tools_examul(agn_checknumber(L, 1), agn_checknumber(L, 2), &t));
  lua_pushnumber(L, t);
  return 2;
}


/* Emulates Matlab's `eps` function and returns the relative spacing between |x| and its next larger
   number in the machine’s floating point system. On x86 machines and with C doubles,
   eps() and eps(1) return 2.2204460492503e-016 = 2 ^(-52), and eps(2) returns 4.4408920985006e-016
   = 2 ^(-51). 2.4.2, extended 2.9.7 to compute a mathematical epsilon that takes into account the
   magnitude of its first argument. */
static int math_eps (lua_State *L) {  /* 2.4.2, five times faster than an Agena implementation */
  lua_Number x;
  if (lua_iscomplex(L, 1) && agn_compleximag(L, 1) == 0.0) {  /* 4.12.0 extension */
    lua_pushnumber(L, agn_complexreal(L, 1));
    lua_replace(L, 1);
  }
  x = fabs(luaL_optnumber(L, 1, 1));
  lua_pushnumber(L, (lua_gettop(L) == 1) ? sun_nextafter(x, HUGE_VAL) - x : tools_matheps(x));  /* 2.9.7 */
  return 1;
}


/* The following math_precision function is much simpler than math_eps/sun_nextafter, but is not much faster:

   Computes the double precision of a number x away from zero, and returns a positive number. The function is
   similar to `math.eps`, but uses a much simpler algorithm. If no argument is given, then x is 1.

   The function uses code taken from:
   - https://randomascii.wordpress.com/2012/02/13/dont-store-that-in-a-float/
     originally written by Bruce Dawson, and
   - http://stackoverflow.com/questions/15685181/how-to-get-the-sign-mantissa-and-exponent-of-a-floating-point-number
     answered by eran (parts struct) */
static int math_precision (lua_State *L) {
  double x;
  double_cast d;
  x = luaL_optnumber(L, 1, 1);
  if (sizeof(double) != 8)
    luaL_error(L, "Error in " LUA_QS ": system does not support 64-bit C doubles.", "math.precision");
  d.f = x;
  /* lua_pushinteger(L, d.i & ((1LL << 52) - 1));  // mantissa
  lua_pushinteger(L, (d.i >> 52) & 0x7FF);         // exponent
  lua_pushinteger(L, (d.i >> 63) != 0);            // is argument negative ?
  lua_pushinteger(L, d.parts.mantissa);
  lua_pushinteger(L, d.parts.exponent);
  lua_pushinteger(L, d.parts.sign); */
  d.i += 1;
  lua_pushnumber(L, fabs(d.f - x));  /* precision */
  return 1;
}


/* Returns the number of integer digits (without decimal places) in the number x to the base b. By default, b is 10.
   Formerly implemented in Agena as: math.ndigits := << n :: number -> entier(ln(n)/ln(10) + 1) >>;
   See also: math.decompose, math.nthdigit. */
static int math_ndigits (lua_State *L) {  /* 2.7.0, extended 2.11.0 RC1 */
  lua_Number x, b;
  int overflow = 0;
  x = agn_checknumber(L, 1);
  b = agnL_optinteger(L, 2, 10);
  if (b == -10) {  /* 2.31.0 */
    lua_pushnumber(L, tools_ndigplaces(x, &overflow));
    lua_pushboolean(L, overflow);
    return 2;
  } else if (b <= 0)
    luaL_error(L, "Error in " LUA_QS ": base %d is non-positive.", "math.ndigits", b);
  if (x < 0) x = -x;
  /* 3.7.2 fix, do not use `1 + (x > 0)*sun_floor(tools_logbase(x, b))` as this correctly computes `undefined` with x = 0 */
  lua_pushnumber(L, 1 + ((x < 1) ? 0 : sun_floor(tools_logbase(x, b))));  /* 3.18.1 fix */
  return 1;
}


static int math_length (lua_State *L) {  /* tries to emulate Maple's `length' function; 3.18.1, based on math_ndigits */
  int sizeint, sizefrac, overflow;
  lua_Number x;
  overflow = 0;
  x = agn_checknumber(L, 1);
  if (x == 0.0) {
    lua_pushinteger(L, 0);
    return 1;
  }
  if (x < 0) x = -x;
  sizeint = 1 + ((x < 1) ? -1 : sun_floor(tools_logbase(x, 10)));
  sizefrac = tools_isfrac(x)*(4 + tools_ndigplaces(x, &overflow));
  if (overflow)
    luaL_error(L, "Error in " LUA_QS ": %f overflowed.", "math.ndigits", x);  /* 6.6.11 fix */
  lua_pushinteger(L, sizeint + sizefrac);
  return 1;
}


/* Splits an integer x to the base b into its digits and returns them in a sequence, with the highest-order digit
   as the first element and the lowest-order digit as the last element. Any sign of x is ignored. By default, the base
   is 10, but you may choose any other positive base.

   Example: b := 256; > math.decompose(15 * b^2 + 7 * b + 1, 256): -> seq(15, 7, 1) */
static int math_decompose (lua_State *L) {  /* 2.7.0 */
  lua_Integer x, base;
  size_t c, i;
  x = agn_checkinteger(L, 1);
  base = agnL_optposint(L, 2, 10);  /* 2.10.0 modificattion */
  agn_createseq(L, 0);  /* we chose a sequence instead of a register for the number of digits in non-base10 numbers
                           cannot always be correctly be determined with C's log function due to round-off errors. */
  c = 0;
  if (x < 0) x = -x;
  if (x == 0) {
    agn_seqsetinumber(L, -1, ++c, 0);
  } else {
    while (x > 0) {
      agn_seqsetinumber(L, -1, ++c, x % base);
      x /= base;
    }
  }
  /* reverse order of sequence */
  for (i=1; i <= c/2; i++) {
    lua_seqrawgeti(L, -1, i);
    lua_seqrawgeti(L, -2, c - i + 1);
    /* swap */
    lua_seqrawseti(L, -3, i);  /* and pop value */
    lua_seqrawseti(L, -2, c - i + 1);  /* and pop value */
  }
  return 1;
}


/* Takes a table, sequence or register of coefficients and a base and returns the composed number. In the coefficient
   list, the highest-order digit as the first element and the lowest-order digit as the last element. By default, the
   base is 10. The function does not take care of potential overflows. It is the complement to `math.decompose`. 2.27.5 */
static int math_compose (lua_State *L) {
  lua_Number *a, base;
  volatile double r, cs, ccs;
  size_t n, i;
  a = agnL_tonumarray(L, 1, &n, "math.compose", 0, 1);
  base = agnL_optposint(L, 2, 10);
  r = cs = ccs = 0;
  for (i=0; i < n; i++) {
    r = tools_kbadd(r, a[i]*tools_intpow(base, n - 1 - i), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
  }
  lua_pushnumber(L, n == 0 ? 0 : r + cs + ccs);
  if (!lua_isuserdata(L, 1)) { xfree(a); }  /* 7.3.5 fix */
  return 1;
}


/* math.wrap(x, a, b)
   math.wrap(x, [a])

Clamping function: Conducts a range reduction of the number x to the interval [a, b) and returns a number.
In the second form, if a is not given, a is set to -Pi and b to +Pi. If a is given, a is set to -a and b to +a,
so a should be positive.

See also: math.norm, zx.range. */

#define P1 (4 * 7.8539812564849853515625e-01)
#define P2 (4 * 3.7748947079307981766760e-08)
#define P3 (4 * 2.6951514290790594840552e-15)

static int math_wrap (lua_State *L) {
  lua_Number theta, min, max;
  size_t nargs;
  theta = agn_checknumber(L, 1);
  nargs = lua_gettop(L);
  if (nargs == 1) {  /* 2.10.0: Same as math.wrap(theta, -Pi, Pi), but 35 percent faster, taken from Julia 0.5.0 */
    lua_Number y, r;
    int isneg, sign;
    theta = agn_checknumber(L, 1);
    isneg = signbit(theta);
    sign = isneg ? -1 : 1;
    if (isneg) theta = -theta;
    y = 2*luai_numint(theta/PI2);  /* 2.21.8 change */
    r = ((theta - y*P1) - y*P2) - y*P3;
    if (r > PI) r -= PI2;  /* 2.21.8 change */
    lua_pushnumber(L, sign*r);
  } else {
    min = agnL_optnumber(L, 2, -PI);
    max = agnL_optnumber(L, 3, PI);
    if (nargs == 2) {
      min = -min; max = -min;
    }
    if (min >= max)
      luaL_error(L, "Error in " LUA_QS ": minimum >= maximum.", "math.wrap");
    lua_pushnumber(L, tools_reducerange(theta, min, max));
  }
  return 1;
}


/* Subtracts the nearest integer multiple of Pi from its argument. See also math.wrap. 2.40.0 */
static int math_redupi (lua_State *L) {  /* 2.40.0 */
  lua_Number x = agn_checknumber(L, 1);
  lua_pushnumber(L, tools_ismultiple(x, PI, 0, agn_getdblepsilon(L)) ? 0.0 : tools_redupi(x));  /* 2.41.2 improvement */
  return 1;
}


static int math_fdim (lua_State *L) {
  if (lua_gettop(L) == 2)
    /* fdim is 7 % faster than sun_fdim */
    lua_pushnumber(L, fdim(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  else {  /* 2.13.0 unification with former math.fdima; this is faster than calling sun_fdim: */
    lua_Number x, y, a;
    x = agn_checknumber(L, 1);
    y = agn_checknumber(L, 2);
    a = agn_checknumber(L, 3);
    if (tools_isnan(x) || tools_isnan(y))
      lua_pushundefined(L);
    else
      lua_pushnumber(L, x >= y ? x - y : a);
  }
  return 1;
}


static int math_isminuszero (lua_State *L) {  /* 2.8.4 */
  lua_pushboolean(L, luai_numisminuszero(agn_checknumber(L, 1)));
  return 1;
}


/* Rounds a float to an integer according to the current rounding method which you can query and set with environ.kernel/rounding.
   2.8.6, extended 2.30.3 */
static int math_rint (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, rint(agn_tonumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {  /* added 2.30.3 */
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      agn_pushcomplex(L, rint(re), rint(im));
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.rint");
  }
  return 1;
}


/* 2.9.0, returns -1 if its numeric argument x is -infinity, +1 if its numeric argument x is +infinity,
  or 0 if neither.

  There seems to be a bug in MingW/GCC's version of isinf: it also returns +1 for -infinity, so an alternative
  has been implemented which is 3 percent faster than copysign(1, x) * fabs(isinf(x));
  (fpclassify(x) == FP_INFINITE) * copysign(1, x) is not faster;

  lua_Number x = agn_checknumber(L, 1);
  volatile lua_Number t;
  if ((t == x) && ((t - x) != 0.0))  # C89 compliant version taken from: http://www.devx.com/tips/Tip/42853
    lua_pushnumber(L, x < 0.0 ? -1 : 1);
  else
    lua_pushnumber(L, 0);
  is slower ! */
static int math_isinfinity (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  lua_pushnumber(L, tools_isinf(x) * copysign(1, x));  /* 3.7.4 change */
  return 1;
}


/* Returns x with its sign flipped if y is negative. For example abs(x) = flipsign(x, x).

   This is a port of the corresponding function of the same name available in Julia 0.5. 2.10.0 */
static int math_flipsign (lua_State *L) {
  lua_Number x, y;
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  lua_pushnumber(L, (signbit(y)) ? -x : x);
  return 1;
}


/* The function returns the largest integer less than or equal to x/y.

   This is a port of the corresponding function of the same name available in Julia 0.5. 2.10.0 */
static int math_fld (lua_State *L) {
  lua_Number x, y;
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  lua_pushnumber(L, luai_numentier(x/y));
  return 1;
}


/* The function returns the smallest integer larger than or equal to x/y.

   This is a port of the corresponding function of the same name available in Julia 0.5. 2.10.0 */
static int math_cld (lua_State *L) {
  lua_Number x, y;
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  lua_pushnumber(L, luai_numentier(x/y) + 1);
  return 1;
}


/* Returns the integer square root: the largest integer m such that m*m <= x.

   This is a port of the corresponding function of the same name in Julia 0.5. 2.10.0;
   externalised 4.11.0 */
static int math_isqrt (lua_State *L) {
  lua_pushnumber(L, tools_isqrt(agn_checknumber(L, 1)));
  return 1;
}


/* Checks whether nonnegatvive x is a power of base 2 and returns `true` or `false`. With negative x or too large values,
   returns `fail`. See: https://github.com/OpsLabJPL/MarsImagesIOS/blob/master/Mars-Images/Math.m,
   https://www.exploringbinary.com/the-powers-of-two/ */
static int math_ispow2 (lua_State *L) {  /* 2.10.0, extended 2.32.3 */
  int rc = tools_ispow2(agn_checknumber(L, 1));
  if (rc == -1) lua_pushfail(L);
  else lua_pushboolean(L, rc);
  return 1;
}


/* Returns the mantissa m of a number x such that m * 2^math.exponent(x) equals x.
   The result is identical to the one returned by `frexp`. The function is around
   20 percent faster but returns correct results only if your system supports IEEE 754
   floating-point numbers, whereas `frexp` always works regardless of the internal
   representation. See also: frexp, math.exponent.

   We use Sun's `frexp` implementation because it also treats subnormal values, 0,
   undefined and infinity values and is only slightly slower than a bitfield/bitmask-
   based implementation. */
static int math_mantissa (lua_State *L) {
  lua_pushnumber(L, sun_frexp_man(agn_checknumber(L, 1)));
  return 1;
}


/* Taken from: newlib-3.0.0.20180831/newlib/libm/math/s_signif.c
 *
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/* Returns the mantissa of number x in normalised form, ranging between [1, 2), i.e. math.significand(x) =
   2*math.mantissa(x) = ldexp(x, -ilog2(x)). If x is 0, the return is 0.

   The function just mimics C's significand function.

   See also: `math.uexponent`. */
static int math_significand (lua_State *L) {  /* 2.14.13 */
  lua_Number x = agn_checknumber(L, 1);
  lua_pushnumber(L, sun_ldexp(x, (double)-sun_ilogb(x)));
  return 1;
}


/* Returns the exponent e of a number x such that math.mantissa(x) * 2^e equals x.
   The result is identical to the one returned by `frexp`. The function is around
   20 percent faster but returns correct results only if your system supports IEEE 754
   floating-point numbers, whereas `frexp` always works regardless of the internal
   representation. See also: frexp, math.mantissa.

   We use Sun's `frexp` implementation because it also treats subnormal values, 0,
   undefined and infinity values and is only slightly slower than a bitfield/bitmask-
   based implementation. */
static int math_exponent (lua_State *L) {  /* 2.10.0, changed and tuned 2.11.0 RC1 */
  lua_pushinteger(L, sun_frexp_exp(agn_checknumber(L, 1)));
  return 1;
}


/* With no option, returns the unbiased exponent of number x, i.e. returns math.exponent(x) - 1;
   except for x = 0 where the result is -1023.

   With any option, returns sign(x)*sun_exponent(x). If x is nan, returns 0x401 = 1025; also returns 1024
   for x = +infinity, and -1024 for x = -infinity. For 0 and subnormal numbers, returns 0.*/
static int math_uexponent (lua_State *L) {  /* 2.13.0, documented with 2.16.12 */
  double x = agn_checknumber(L, 1);
  if (lua_gettop(L) == 1) {
    int32_t hx, ux;
    x = agn_checknumber(L, 1);
    GET_HIGH_WORD(hx, x);
    ux = ((((uint32_t)hx) >> 20) & 0x7ff) - 0x3ff;
    lua_pushnumber(L, ux);
  } else
    lua_pushnumber(L, sun_uexponent(x));  /* new 2.31.2 */
  return 1;
}


/* The function is a plain binding to the C `%` modulus operator. Both its arguments must
   be integers. The return is an integer. */
static int math_modulus (lua_State *L) {  /* 2.10.0 */
  int y = agn_checkinteger(L, 2);
  /* if the second operand is zero, Agena CRASHES, so check it. */
  lua_pushnumber(L, (y == 0) ? AGN_NAN : agn_checkinteger(L, 1) % y);  /* undefined as in Maple */
  return 1;
}


static int math_nearmod (lua_State *L) {  /* Returns the closest value to the given number x divisible by the given modulus m,
  equivalent to round(x/m) * m. 2.14.8, idea taken from VidCoder source file MathUtilities.cs */
  lua_Integer x, m;
  x = agn_checkinteger(L, 1);  /* number */
  m = agn_checkinteger(L, 2);  /* modulus */
  lua_pushnumber(L, (m == 0) ? AGN_NAN : sun_round((lua_Number)x/m) * m);
  return 1;
}


/* Returns the relative spacing between |x| and its next larger number on the machine’s
   floating point system, taking into account the magnitude of its argument. The function
   works like `math.eps` with the `true` option but is 20 percent faster.

   If |x| < 1, you may choose a constant epsilon value yourself, e.g. Eps. */
static int math_epsilon (lua_State *L) {  /* 2.10.0, code optimised 2.10.1, extended 2.14.2 */
  lua_Number x, eps, ulp;
  int nres = 0;
  if (lua_iscomplex(L, 1) && agn_compleximag(L, 1) == 0.0) {  /* 4.12.0 extension */
    lua_pushnumber(L, agn_complexreal(L, 1));
    lua_replace(L, 1);
  }
  if (agn_isnumber(L, 1)) {
    int method;
    x = fabs(agn_tonumber(L, 1));
    method = agnL_optnonnegint(L, 2, 0);
    ulp = sun_nextafter(x, HUGE_VAL) - x;  /* machine epsilon at x */
    switch (method) {
      case 0: {  /* x * sqrt(ulp), for backward/forward differences; AGN_EPSILON = 1.4901161193847656e-08 */
        lua_pushnumber(L, (fabs(x) < 1) ? AGN_EPSILON : x * sqrt(ulp));  /* 3.11.1/4.2.3/4.2.5 fix */
        break;
      }
      case 1: {  /* x * cbrt(ulp), for central differences */
        lua_pushnumber(L, (fabs(x) < 0.0123927159) ? AGN_EPSILON : x * sun_cbrt(ulp));  /* 3.11.1/4.2.3/4.2.5 fix */
        break;
      }
      case 2: {  /* sqrt(ulp) * (|x| + sqrt(ulp)), for backward/forward differences  */
        eps = sqrt(ulp);
        lua_pushnumber(L, eps*(x + eps));
        break;
      }
      case 3: {  /* cbrt(ulp) * (|x| + cbrt(ulp)), for central differences */
        eps = sun_cbrt(ulp);
        lua_pushnumber(L, eps*(x + eps));
        break;
      }
      default:
        luaL_error(L, "Error in " LUA_QS ": invalid option %d.", "math.epsilon", method);
    }
    nres = 1;
  } else if (lua_isfunction(L, 1)) {
    lua_Number abserr, origh, h;
    int nargs, n;
    x = agn_checknumber(L, 2);
    nargs = lua_gettop(L);
    n = 4;  /* iterations */
    if (lua_ispair(L, nargs)) {
      agn_pairgeti(L, nargs, 2);
      if (!agn_isnumber(L, -1)) {
        agn_poptop(L);
        luaL_error(L, "Error in " LUA_QS ": right-hand side of option must be a number.", "math.epsilon");
      }
      n = agn_tonumber(L, -1);
      agn_poptop(L);
      if (tools_isfrac(n) || n < 1)
        luaL_error(L, "Error in " LUA_QS ": right-hand side of option must be a positive integer.", "math.epsilon");
      nargs--;
    }
    h = agnL_fneps(L, 1, x, n, 3, nargs, &origh, &abserr);
    luaL_checkstack(L, 3, "not enough stack space");  /* 3.15.3 fix */
    lua_pushnumber(L, origh);
    lua_pushnumber(L, h);
    lua_pushnumber(L, abserr);
    nres = 3;
  } else
     luaL_error(L, "Error in " LUA_QS ": wrong argument of type %s.", "math.epsilon", luaL_typename(L, 1));  /* 4.12.0 extension */
  return nres;
}


/* Returns true if the number x is +0, -0, +infinity, -infinity or `undefined`. With complex number argument x
   returns true if the real or the imaginary part is +0, -0, +infinity, -infinity or `undefined`. 6.7.8 */
static int math_isexceptional (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushboolean(L, sun_isexceptional(agn_tonumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      lua_pushboolean(L, sun_isexceptional(re) || sun_isexceptional(im));
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.isnormal");
  }
  return 1;
}


/* Checks whether the number x is subnormal, i.e. whether internally the leading digit of its mantissa is 0.
   The function returns `true` or `false`. Subnormal numbers are very close to zero, have reduced
   precision and lead to excessive CPU usage, they are in the range [-2.2250738585072009e-308, -4.9406564584124654e-324] and
   [4.9406564584124654e-324, 2.2250738585072009e-308]. 0, undefined and +/-infinity are not subnormal.
   fpclassify(x) == FP_SUBNORMAL does not work with MinGW GCC */
static int math_issubnormal (lua_State *L) {  /* 2.10.0 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushboolean(L, sun_issubnormal(agn_tonumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {  /* added 2.30.3 */
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      lua_pushboolean(L, sun_issubnormal(re) || sun_issubnormal(im));
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.issubnormal");
  }
  return 1;
}


/* Returns true if the number x is neither +0, -0, +infinity, -infinity, undefined nor subnormal. The result is equal to th expression
   math.fpclassify(x) = math.fp_normal. */
static int math_isnormal (lua_State *L) {  /* 2.27.0 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushboolean(L, sun_isnormal(agn_tonumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {  /* added 2.30.3 */
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      lua_pushboolean(L, sun_isnormal(re) && sun_isnormal(im));
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.isnormal");
  }
  return 1;
}


/* Checks whether its numeric argument x is subnormal and in this case returns -0 or 0, depending on the sign of x; otherwise
   returns its argument x.
   The function is useful to prevent excessive CPU usage in case of arguments very close to zero.
   For more information, see `math.issubnormal`. */
static int math_zerosubnormal (lua_State *L) {  /* 2.10.0 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, tools_chop(agn_tonumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {  /* added 2.30.3 */
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      agn_pushcomplex(L, tools_chop(re), tools_chop(im));
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.zerosubnormal");
  }
  return 1;
}


/* Checks whether its numeric argument x is subnormal and in this case normalises it, i.e. returns a non-zero normalised value
   that is close to x; otherwise returns its argument x unaltered.
   It is useful to prevent excessive CPU usage in case of arguments very close to zero.
   For more information, see `math.issubnormal`. 2.10.0 */
static int math_normalise (lua_State *L) {
  uint32_t high;
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      uint32_t option = lua_gettop(L) > 1;
      lua_pushnumber(L, sun_normalise(agn_tonumber(L, 1), &high));  /* 2.18.1 */
      if (option) lua_pushnumber(L, high);  /* 2.18.1 */
      return 1 + option;
    }
    case LUA_TCOMPLEX: {  /* added 2.30.3 */
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      agn_pushcomplex(L, sun_normalise(re, &high), sun_normalise(im, &high));
      return 1;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.normalise");
  }
  return 0;  /* should not happen */
}


/* Returns the sign s, the mantissa m and the exponent e of the number x, in this order, such that
   s*m*2^e = x. The sign is -1 if x is negative (including -0) and 1 otherwise. The mantissa is a
   non-negative float, the exponent a negative or positive integer or zero.

   If any option is given, then instead of the sign the signbit s is returned: 1 if x is negative or
   -0, and 0 otherwise. In this case x = signum(-s)*m*2^e. */
static int math_frexp (lua_State *L) {  /* 2.10.0 */
  lua_Number x, m;
  int e, s, anyoption;
  x = agn_checknumber(L, 1);
  anyoption = lua_gettop(L) > 1;
  m = sun_xfrexp(x, &e, &s);
  if (!anyoption) s = (s == 0) ? 1 : -1;
  luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
  lua_pushnumber(L, s);
  lua_pushnumber(L, m);
  lua_pushnumber(L, e);
  return 3;
}


/* Checks whether a number cannot be represented exactly on your system.
   See: https://en.wikipedia.org/wiki/Double-precision_floating-point_format#IEEE_754_double-precision_binary_floating-point_format:_binary64 */
static int math_isirregular (lua_State *L) {  /* 2.10.0 */
  LUAI_INT32 hx, lx;
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      EXTRACT_WORDS(hx, lx, x);
      hx &= 0x7FFFFFFF;     /* absolute value */
      if (hx < 0x43300000)  /* x < 2^52; a number with decimal places can internally be represented as a number with decimal places, but not */
        lua_pushfalse(L);   /* necessarily itself. With n < 52, the spacing between two subsequent representable numbers is the _fraction_ 2^(n-52). */
      else if (hx < 0x43400000)  /* 2^52 <= x < 2^53, representable numbers are exactly the integers; spacing between representable */
        lua_pushfail(L);         /* numbers is exactly 1 */
      else if (hx == 0x43400000 && lx == 0)  /* 2^53 <= x <= 2^53 + 1, representable numbers are exactly the integers; spacing between representable */
        lua_pushfail(L);    /* numbers is exactly 1, 2.16.0 fix */
      else                  /* x > 2^53 (+ 1), an integer mostly cannot be exactly represented; with n > 52, spacing is the _integer_ 2^(n-52) */
        lua_pushtrue(L);
      return 1;
    }
    case LUA_TCOMPLEX: {  /* added 2.30.3 */
      LUAI_INT32 hy, ly;
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      EXTRACT_WORDS(hx, lx, re);
      EXTRACT_WORDS(hy, ly, im);
      hx &= 0x7FFFFFFF;
      hy &= 0x7FFFFFFF;
      if (hx < 0x43300000 && hy < 0x43300000) {
        lua_pushfalse(L);
      } else if ( ((hx >= 0x43300000) && ((hx < 0x43400000) || (hx == 0x43400000 && lx == 0) )) ||
                  ((hy >= 0x43300000) && ((hy < 0x43400000) || (hy == 0x43400000 && ly == 0) ))) {
        lua_pushfail(L);
      } else
        lua_pushtrue(L);
      return 1;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.isirregular");
  }
  return 0;  /* should not happen */
}


/* Returns its argument x - a number - if x is non-negative, otherwise returns 0. By passing any
   non-negative optional number d (the `direction`), the return is the same.

   By passing any non-positive optional number d, returns x if it is negative, otherwise returns 0.

   If x should be `undefined`, you can return any other number by passing the optional argument subs,
   which is `undefined` by default. */
static int math_branch (lua_State *L) {  /* 2.10.0, see also (unused) tools_branch */
  lua_Number x, d, r;
  x = agn_checknumber(L, 1);
  d = agnL_optnumber(L, 2, HUGE_VAL);  /* the `direction` or interval: if d > 0 -> return in [0, infinity], */
  if (tools_isnan(x)) x = agnL_optnumber(L, 3, AGN_NAN);  /* optionally convert x = undefined to any other number, default is `itself` */
  r = signbit(x) == 0;                 /* if d <= 0 -> return in [-infinity, 0] */
  r = (d > 0) ? r*x : (!r)*x;
  if (luai_numisminuszero(r)) r = 0;   /* convert -0 to 0 */
  lua_pushnumber(L, r);
  return 1;
}


/* logarithmic factorial, written by John D. Cook and release to the public domain, modification by a walz;
   taken from: http://www.johndcook.com/blog/cpp_log_factorial; 2.10.0, core moved to agnhlps.c 2.41.2
   For an approximation, which is slower, see https://www.johndcook.com/blog/csharp_log_factorial/
   lua_Integer x = n + 1;
   lua_pushnumber(L, (x - 0.5)*sun_log(x) - x + 0.5*sun_log(2*PI) + 1.0/(12.0*x));
   Extended 3.16.2 */
static int math_lnfact (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      lua_pushnumber(L, tools_isint(x) ? tools_lnfactorial(x) : luai_numlngamma(x + 1));
      break;
    }
    case LUA_TCOMPLEX: {  /* 3.16.2 extension */
#ifndef PROPCMPLX
      agn_Complex z, r, t;
      /* we cannot use cephes_clgam for there are issues with the imaginary unit of the result sometimes missing +/- PI2*I */
      z = agn_tocomplex(L, 1);
      t = z + 1.0+0.0*I;
      r = cephes_cgamma(t);
      r = tools_clog(r);
      if (cimag(z) == 0 && cimag(r) != 0) r += PI2*I;
      agn_createcomplex(L, r);
#else
      lua_Number z[2], re, im, im0, t1, si, co;
      agn_getcmplxparts(L, 1, &re, &im);
      im0 = im;  /* im will be overwritten with the next operation */
      cephes_clgam(agn_complexreal(L, 1) + 1, agn_compleximag(L, 1), &re, &im);
      t1 = sun_exp(re);
      sun_sincos(im, &si, &co);
      agnCmplx_create(z, t1*co, t1*si);
      agnCmplx_log(z, z[0], z[1]);
      if (im0 == 0 && !(tools_approx(z[1], 0, agn_getepsilon(L)))) z[1] += PI2;
      agn_createcomplex(L, z[0], z[1]);
#endif
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.lnfact");
  }
  return 1;
}


static int math_lnbinomial (lua_State *L) {  /* 3.7.3 */
  lua_pushnumber(L, tools_lnbinomiall((long double)agn_checkinteger(L, 1), (long double)agn_checkinteger(L, 2)));
  return 1;
}


static int math_lnbeta (lua_State *L) {  /* 4.3.1 */
  lua_pushnumber(L, tools_lnbeta(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


/* The falling factorial function computes x*(x - 1)*(x - 2)* ... *(x - n + 1), with x a number and n an integer.
   If n is negative, the rising factorial function is computed. See also: fact. */
static int math_fall (lua_State *L) {  /* 2.10.2 */
  int i, n;
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      long double r, x;  /* 3.16.2 change */
      r = 1.0;
      x = agn_checknumber(L, 1);
      n = agn_checkinteger(L, 2);
      if (n > 0) {  /* falling factorial, FallingFactorial:= (x, n)-> pochhammer(x - n + 1, n); see
        https://www.mapleprimes.com/questions/219924-Falling-Factorial */
        for (i=1; i <= n; i++) {
          r *= x;
          x -= 1.0;
        }
      } else if (n < 0) {  /* rising factorial = Pochhammer function */
        for (i=-1; n <= i; i--) {
          r *= x;
          x += 1.0;
        }
      }
      lua_pushnumber(L, (lua_Number)r);
      break;
    }
    case LUA_TCOMPLEX: {  /* 3.16.2 */
      lua_Number r[2], z[2], re, im;
      agnCmplx_create(r, 1, 0);
      agn_getcmplxparts(L, 1, &re, &im);
      agnCmplx_create(z, re, im);
      n = agn_checkinteger(L, 2);
      if (n > 0) {  /* falling factorial */
        for (i=1; i <= n; i++) {
          agnCmplx_mul(r, r[0], r[1], z[0], z[1]);
          agnCmplx_sub(z, z[0], z[1], 1.0, 0);
        }
      } else if (n < 0) {  /* rising factorial = Pochhammer function */
        for (i=-1; n <= i; i--) {
          agnCmplx_mul(r, r[0], r[1], z[0], z[1]);
          agnCmplx_add(z, z[0], z[1], 1.0, 0);
        }
      }
      agn_pushcomplex(L, r[0], r[1]);
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.lnfact");
  }
  return 1;
}


/* Table taken from: http://www.mymathlib.com/functions/combinatorial.html#doublefactorial,
   Copyright © 2004 RLH. All rights reserved. */
static long double const double_factorials[] = {
  1.000000000000000000000e+0L,     /*   0!! */
  1.000000000000000000000e+0L,     /*   1!! */
  2.000000000000000000000e+0L,     /*   2!! */
  3.000000000000000000000e+0L,     /*   3!! */
  8.000000000000000000000e+0L,     /*   4!! */
  1.500000000000000000000e+1L,     /*   5!! */
  4.800000000000000000000e+1L,     /*   6!! */
  1.050000000000000000000e+2L,     /*   7!! */
  3.840000000000000000000e+2L,     /*   8!! */
  9.450000000000000000000e+2L,     /*   9!! */
  3.840000000000000000000e+3L,     /*  10!! */
  1.039500000000000000000e+4L,     /*  11!! */
  4.608000000000000000000e+4L,     /*  12!! */
  1.351350000000000000000e+5L,     /*  13!! */
  6.451200000000000000000e+5L,     /*  14!! */
  2.027025000000000000000e+6L,     /*  15!! */
  1.032192000000000000000e+7L,     /*  16!! */
  3.445942500000000000000e+7L,     /*  17!! */
  1.857945600000000000000e+8L,     /*  18!! */
  6.547290750000000000000e+8L,     /*  19!! */
  3.715891200000000000000e+9L,     /*  20!! */
  1.374931057500000000000e+10L,    /*  21!! */
  8.174960640000000000000e+10L,    /*  22!! */
  3.162341432250000000000e+11L,    /*  23!! */
  1.961990553600000000000e+12L,    /*  24!! */
  7.905853580625000000000e+12L,    /*  25!! */
  5.101175439360000000000e+13L,    /*  26!! */
  2.134580466768750000000e+14L,    /*  27!! */
  1.428329123020800000000e+15L,    /*  28!! */
  6.190283353629375000000e+15L,    /*  29!! */
  4.284987369062400000000e+16L,    /*  30!! */
  1.918987839625106250000e+17L,    /*  31!! */
  1.371195958099968000000e+18L,    /*  32!! */
  6.332659870762850625000e+18L,    /*  33!! */
  4.662066257539891200000e+19L,    /*  34!! */
  2.216430954766997718750e+20L,    /*  35!! */
  1.678343852714360832000e+21L,    /*  36!! */
  8.200794532637891559375e+21L,    /*  37!! */
  6.377706640314571161600e+22L,    /*  38!! */
  3.198309867728777708156e+23L,    /*  39!! */
  2.551082656125828464640e+24L,    /*  40!! */
  1.311307045768798860344e+25L,    /*  41!! */
  1.071454715572847955149e+26L,    /*  42!! */
  5.638620296805835099479e+26L,    /*  43!! */
  4.714400748520531002655e+27L,    /*  44!! */
  2.537379133562625794766e+28L,    /*  45!! */
  2.168624344319444261221e+29L,    /*  46!! */
  1.192568192774434123540e+30L,    /*  47!! */
  1.040939685273333245386e+31L,    /*  48!! */
  5.843584144594727205346e+31L,    /*  49!! */
  5.204698426366666226931e+32L,    /*  50!! */
  2.980227913743310874726e+33L,    /*  51!! */
  2.706443181710666438004e+34L,    /*  52!! */
  1.579520794283954763605e+35L,    /*  53!! */
  1.461479318123759876522e+36L,    /*  54!! */
  8.687364368561751199827e+36L,    /*  55!! */
  8.184284181493055308524e+37L,    /*  56!! */
  4.951797690080198183901e+38L,    /*  57!! */
  4.746884825265972078944e+39L,    /*  58!! */
  2.921560637147316928502e+40L,    /*  59!! */
  2.848130895159583247366e+41L,    /*  60!! */
  1.782151988659863326386e+42L,    /*  61!! */
  1.765841154998941613367e+43L,    /*  62!! */
  1.122755752855713895623e+44L,    /*  63!! */
  1.130138339199322632555e+45L,    /*  64!! */
  7.297912393562140321551e+45L,    /*  65!! */
  7.458913038715529374863e+46L,    /*  66!! */
  4.889601303686634015439e+47L,    /*  67!! */
  5.072060866326559974907e+48L,    /*  68!! */
  3.373824899543777470653e+49L,    /*  69!! */
  3.550442606428591982435e+50L,    /*  70!! */
  2.395415678676082004164e+51L,    /*  71!! */
  2.556318676628586227353e+52L,    /*  72!! */
  1.748653445433539863039e+53L,    /*  73!! */
  1.891675820705153808241e+54L,    /*  74!! */
  1.311490084075154897280e+55L,    /*  75!! */
  1.437673623735916894263e+56L,    /*  76!! */
  1.009847364737869270905e+57L,    /*  77!! */
  1.121385426514015177525e+58L,    /*  78!! */
  7.977794181429167240152e+58L,    /*  79!! */
  8.971083412112121420203e+59L,    /*  80!! */
  6.462013286957625464523e+60L,    /*  81!! */
  7.356288397931939564567e+61L,    /*  82!! */
  5.363471028174829135554e+62L,    /*  83!! */
  6.179282254262829234236e+63L,    /*  84!! */
  4.558950373948604765221e+64L,    /*  85!! */
  5.314182738666033141443e+65L,    /*  86!! */
  3.966286825335286145742e+66L,    /*  87!! */
  4.676480810026109164470e+67L,    /*  88!! */
  3.529995274548404669711e+68L,    /*  89!! */
  4.208832729023498248023e+69L,    /*  90!! */
  3.212295699839048249437e+70L,    /*  91!! */
  3.872126110701618388181e+71L,    /*  92!! */
  2.987435000850314871976e+72L,    /*  93!! */
  3.639798544059521284890e+73L,    /*  94!! */
  2.838063250807799128377e+74L,    /*  95!! */
  3.494206602297140433495e+75L,    /*  96!! */
  2.752921353283565154526e+76L,    /*  97!! */
  3.424322470251197624825e+77L,    /*  98!! */
  2.725392139750729502981e+78L,    /*  99!! */
  3.424322470251197624825e+79L,    /* 100!! */
  2.752646061148236798011e+80L,    /* 101!! */
  3.492808919656221577321e+81L,    /* 102!! */
  2.835225442982683901951e+82L,    /* 103!! */
  3.632521276442470440414e+83L,    /* 104!! */
  2.976986715131818097048e+84L,    /* 105!! */
  3.850472553029018666839e+85L,    /* 106!! */
  3.185375785191045363842e+86L,    /* 107!! */
  4.158510357271340160186e+87L,    /* 108!! */
  3.472059605858239446588e+88L,    /* 109!! */
  4.574361392998474176205e+89L,    /* 110!! */
  3.853986162502645785712e+90L,    /* 111!! */
  5.123284760158291077349e+91L,    /* 112!! */
  4.355004363627989737855e+92L,    /* 113!! */
  5.840544626580451828178e+93L,    /* 114!! */
  5.008255018172188198533e+94L,    /* 115!! */
  6.775031766833324120686e+95L,    /* 116!! */
  5.859658371261460192284e+96L,    /* 117!! */
  7.994537484863322462410e+97L,    /* 118!! */
  6.972993461801137628817e+98L,    /* 119!! */
  9.593444981835986954892e+99L,    /* 120!! */
  8.437322088779376530869e+100L,   /* 121!! */
  1.170400287783990408497e+102L,   /* 122!! */
  1.037790616919863313297e+103L,   /* 123!! */
  1.451296356852148106536e+104L,   /* 124!! */
  1.297238271149829141621e+105L,   /* 125!! */
  1.828633409633706614235e+106L,   /* 126!! */
  1.647492604360283009859e+107L,   /* 127!! */
  2.340650764331144466221e+108L,   /* 128!! */
  2.125265459624765082718e+109L,   /* 129!! */
  3.042845993630487806088e+110L,   /* 130!! */
  2.784097752108442258360e+111L,   /* 131!! */
  4.016556711592243904036e+112L,   /* 132!! */
  3.702850010304228203619e+113L,   /* 133!! */
  5.382185993533606831408e+114L,   /* 134!! */
  4.998847513910708074886e+115L,   /* 135!! */
  7.319772951205705290715e+116L,   /* 136!! */
  6.848421094057670062594e+117L,   /* 137!! */
  1.010128667266387330119e+119L,   /* 138!! */
  9.519305320740161387006e+119L,   /* 139!! */
  1.414180134172942262166e+121L,   /* 140!! */
  1.342222050224362755568e+122L,   /* 141!! */
  2.008135790525578012276e+123L,   /* 142!! */
  1.919377531820838740462e+124L,   /* 143!! */
  2.891715538356832337677e+125L,   /* 144!! */
  2.783097421140216173670e+126L,   /* 145!! */
  4.221904686000975213009e+127L,   /* 146!! */
  4.091153209076117775295e+128L,   /* 147!! */
  6.248418935281443315253e+129L,   /* 148!! */
  6.095818281523415485189e+130L,   /* 149!! */
  9.372628402922164972880e+131L,   /* 150!! */
  9.204685605100357382635e+132L,   /* 151!! */
  1.424639517244169075878e+134L,   /* 152!! */
  1.408316897580354679543e+135L,   /* 153!! */
  2.193944856556020376852e+136L,   /* 154!! */
  2.182891191249549753292e+137L,   /* 155!! */
  3.422553976227391787889e+138L,   /* 156!! */
  3.427139170261793112668e+139L,   /* 157!! */
  5.407635282439279024864e+140L,   /* 158!! */
  5.449151280716251049143e+141L,   /* 159!! */
  8.652216451902846439782e+142L,   /* 160!! */
  8.773133561953164189120e+143L,   /* 161!! */
  1.401659065208261123245e+145L,   /* 162!! */
  1.430020770598365762827e+146L,   /* 163!! */
  2.298720866941548242121e+147L,   /* 164!! */
  2.359534271487303508664e+148L,   /* 165!! */
  3.815876639122970081921e+149L,   /* 166!! */
  3.940422233383796859469e+150L,   /* 167!! */
  6.410672753726589737628e+151L,   /* 168!! */
  6.659313574418616692502e+152L,   /* 169!! */
  1.089814368133520255397e+154L,   /* 170!! */
  1.138742621225583454418e+155L,   /* 171!! */
  1.874480713189654839282e+156L,   /* 172!! */
  1.970024734720259376143e+157L,   /* 173!! */
  3.261596440949999420351e+158L,   /* 174!! */
  3.447543285760453908250e+159L,   /* 175!! */
  5.740409736071998979819e+160L,   /* 176!! */
  6.102151615796003417602e+161L,   /* 177!! */
  1.021792933020815818408e+163L,   /* 178!! */
  1.092285139227484611751e+164L,   /* 179!! */
  1.839227279437468473134e+165L,   /* 180!! */
  1.977036102001747147269e+166L,   /* 181!! */
  3.347393648576192621104e+167L,   /* 182!! */
  3.617976066663197279502e+168L,   /* 183!! */
  6.159204313380194422831e+169L,   /* 184!! */
  6.693255723326914967079e+170L,   /* 185!! */
  1.145612002288716162647e+172L,   /* 186!! */
  1.251638820262133098844e+173L,   /* 187!! */
  2.153750564302786385775e+174L,   /* 188!! */
  2.365597370295431556815e+175L,   /* 189!! */
  4.092126072175294132973e+176L,   /* 190!! */
  4.518290977264274273516e+177L,   /* 191!! */
  7.856882058576564735309e+178L,   /* 192!! */
  8.720301586120049347886e+179L,   /* 193!! */
  1.524235119363853558650e+181L,   /* 194!! */
  1.700458809293409622838e+182L,   /* 195!! */
  2.987500833953152974954e+183L,   /* 196!! */
  3.349903854308016956991e+184L,   /* 197!! */
  5.915251651227242890409e+185L,   /* 198!! */
  6.666308670072953744411e+186L,   /* 199!! */
  1.183050330245448578082e+188L,   /* 200!! */
  1.339928042684663702627e+189L,   /* 201!! */
  2.389761667095806127725e+190L,   /* 202!! */
  2.720053926649867316332e+191L,   /* 203!! */
  4.875113800875444500559e+192L,   /* 204!! */
  5.576110549632227998481e+193L,   /* 205!! */
  1.004273442980341567115e+195L,   /* 206!! */
  1.154254883773871195686e+196L,   /* 207!! */
  2.088888761399110459600e+197L,   /* 208!! */
  2.412392707087390798983e+198L,   /* 209!! */
  4.386666398938131965159e+199L,   /* 210!! */
  5.090148611954394585854e+200L,   /* 211!! */
  9.299732765748839766137e+201L,   /* 212!! */
  1.084201654346286046787e+203L,   /* 213!! */
  1.990142811870251709953e+204L,   /* 214!! */
  2.331033556844515000592e+205L,   /* 215!! */
  4.298708473639743693499e+206L,   /* 216!! */
  5.058342818352597551284e+207L,   /* 217!! */
  9.371184472534641251828e+208L,   /* 218!! */
  1.107777077219218863731e+210L,   /* 219!! */
  2.061660583957621075402e+211L,   /* 220!! */
  2.448187340654473688846e+212L,   /* 221!! */
  4.576886496385918787393e+213L,   /* 222!! */
  5.459457769659476326126e+214L,   /* 223!! */
  1.025222575190445808376e+216L,   /* 224!! */
  1.228377998173382173378e+217L,   /* 225!! */
  2.317003019930407526930e+218L,   /* 226!! */
  2.788418055853577533569e+219L,   /* 227!! */
  5.282766885441329161400e+220L,   /* 228!! */
  6.385477347904692551873e+221L,   /* 229!! */
  1.215036383651505707122e+223L,   /* 230!! */
  1.475045267365983979483e+224L,   /* 231!! */
  2.818884410071493240523e+225L,   /* 232!! */
  3.436855472962742672195e+226L,   /* 233!! */
  6.596189519567294182824e+227L,   /* 234!! */
  8.076610361462445279657e+228L,   /* 235!! */
  1.556700726617881427146e+230L,   /* 236!! */
  1.914156655666599531279e+231L,   /* 237!! */
  3.704947729350557796609e+232L,   /* 238!! */
  4.574834407043172879756e+233L,   /* 239!! */
  8.891874550441338711861e+234L,   /* 240!! */
  1.102535092097404664021e+236L,   /* 241!! */
  2.151833641206803968270e+237L,   /* 242!! */
  2.679160273796693333572e+238L,   /* 243!! */
  5.250474084544601682579e+239L,   /* 244!! */
  6.563942670801898667251e+240L,   /* 245!! */
  1.291616624797972013915e+242L,   /* 246!! */
  1.621293839688068970811e+243L,   /* 247!! */
  3.203209229498970594508e+244L,   /* 248!! */
  4.037021660823291737319e+245L,   /* 249!! */
  8.008023073747426486270e+246L,   /* 250!! */
  1.013292436866646226067e+248L,   /* 251!! */
  2.018021814584351474540e+249L,   /* 252!! */
  2.563629865272614951950e+250L,   /* 253!! */
  5.125775409044252745332e+251L,   /* 254!! */
  6.537256156445168127472e+252L,   /* 255!! */
  1.312198504715328702805e+254L,   /* 256!! */
  1.680074832206408208760e+255L,   /* 257!! */
  3.385472142165548053237e+256L,   /* 258!! */
  4.351393815414597260689e+257L,   /* 259!! */
  8.802227569630424938416e+258L,   /* 260!! */
  1.135713785823209885040e+260L,   /* 261!! */
  2.306183623243171333865e+261L,   /* 262!! */
  2.986927256715041997655e+262L,   /* 263!! */
  6.088324765361972321403e+263L,   /* 264!! */
  7.915357230294861293785e+264L,   /* 265!! */
  1.619494387586284637493e+266L,   /* 266!! */
  2.113400380488727965441e+267L,   /* 267!! */
  4.340244958731242828482e+268L,   /* 268!! */
  5.685047023514678227036e+269L,   /* 269!! */
  1.171866138857435563690e+271L,   /* 270!! */
  1.540647743372477799527e+272L,   /* 271!! */
  3.187475897692224733237e+273L,   /* 272!! */
  4.205968339406864392708e+274L,   /* 273!! */
  8.733683959676695769070e+275L,   /* 274!! */
  1.156641293336887707995e+277L,   /* 275!! */
  2.410496772870768032263e+278L,   /* 276!! */
  3.203896382543178951145e+279L,   /* 277!! */
  6.701181028580735129692e+280L,   /* 278!! */
  8.938870907295469273695e+281L,   /* 279!! */
  1.876330688002605836314e+283L,   /* 280!! */
  2.511822724950026865908e+284L,   /* 281!! */
  5.291252540167348458405e+285L,   /* 282!! */
  7.108458311608576030520e+286L,   /* 283!! */
  1.502715721407526962187e+288L,   /* 284!! */
  2.025910618808444168698e+289L,   /* 285!! */
  4.297766963225527111855e+290L,   /* 286!! */
  5.814363475980234764164e+291L,   /* 287!! */
  1.237756885408951808214e+293L,   /* 288!! */
  1.680351044558287846843e+294L,   /* 289!! */
  3.589494967685960243821e+295L,   /* 290!! */
  4.889821539664617634314e+296L,   /* 291!! */
  1.048132530564300391196e+298L,   /* 292!! */
  1.432717711121732966854e+299L,   /* 293!! */
  3.081509639859043150115e+300L,   /* 294!! */
  4.226517247809112252220e+301L,   /* 295!! */
  9.121268533982767724342e+302L,   /* 296!! */
  1.255275622599306338909e+304L,   /* 297!! */
  2.718138023126864781854e+305L,   /* 298!! */
  3.753274111571925953339e+306L,   /* 299!! */
  8.154414069380594345561e+307L }; /* 300!! */

static int math_dblfact (lua_State *L) {  /* 2.40.2 */
  int n = agn_checknonnegint(L, 1);
  lua_pushnumber(L, (n < 301) ? double_factorials[n] : HUGE_VAL);
  return 1;
}


static long double const triple_factorials[] = {
  1.000000000000000000000e+0L,     /*   0!!! */
  1.000000000000000000000e+0L,     /*   1!!! */
  2.000000000000000000000e+0L,     /*   2!!! */
  3.000000000000000000000e+0L,     /*   3!!! */
  4.000000000000000000000e+0L,     /*   4!!! */
  1.000000000000000000000e+1L,     /*   5!!! */
  1.800000000000000000000e+1L,     /*   6!!! */
  2.800000000000000000000e+1L,     /*   7!!! */
  8.000000000000000000000e+1L,     /*   8!!! */
  1.620000000000000000000e+2L,     /*   9!!! */
  2.800000000000000000000e+2L,     /*  10!!! */
  8.800000000000000000000e+2L,     /*  11!!! */
  1.944000000000000000000e+3L,     /*  12!!! */
  3.640000000000000000000e+3L,     /*  13!!! */
  1.232000000000000000000e+4L,     /*  14!!! */
  2.916000000000000000000e+4L,     /*  15!!! */
  5.824000000000000000000e+4L,     /*  16!!! */
  2.094400000000000000000e+5L,     /*  17!!! */
  5.248800000000000000000e+5L,     /*  18!!! */
  1.106560000000000000000e+6L,     /*  19!!! */
  4.188800000000000000000e+6L,     /*  20!!! */
  1.102248000000000000000e+7L,     /*  21!!! */
  2.434432000000000000000e+7L,     /*  22!!! */
  9.634240000000000000000e+7L,     /*  23!!! */
  2.645395200000000000000e+8L,     /*  24!!! */
  6.086080000000000000000e+8L,     /*  25!!! */
  2.504902400000000000000e+9L,     /*  26!!! */
  7.142567040000000000000e+9L,     /*  27!!! */
  1.704102400000000000000e+10L,    /*  28!!! */
  7.264216960000000000000e+10L,    /*  29!!! */
  2.142770112000000000000e+11L,    /*  30!!! */
  5.282717440000000000000e+11L,    /*  31!!! */
  2.324549427200000000000e+12L,    /*  32!!! */
  7.071141369600000000000e+12L,    /*  33!!! */
  1.796123929600000000000e+13L,    /*  34!!! */
  8.135922995200000000000e+13L,    /*  35!!! */
  2.545610893056000000000e+14L,    /*  36!!! */
  6.645658539520000000000e+14L,    /*  37!!! */
  3.091650738176000000000e+15L,    /*  38!!! */
  9.927882482918400000000e+15L,    /*  39!!! */
  2.658263415808000000000e+16L,    /*  40!!! */
  1.267576802652160000000e+17L,    /*  41!!! */
  4.169710642825728000000e+17L,    /*  42!!! */
  1.143053268797440000000e+18L,    /*  43!!! */
  5.577337931669504000000e+18L,    /*  44!!! */
  1.876369789271577600000e+19L,    /*  45!!! */
  5.258045036468224000000e+19L,    /*  46!!! */
  2.621348827884666880000e+20L,    /*  47!!! */
  9.006574988503572480000e+20L,    /*  48!!! */
  2.576442067869429760000e+21L,    /*  49!!! */
  1.310674413942333440000e+22L,    /*  50!!! */
  4.593353244136821964800e+22L,    /*  51!!! */
  1.339749875292103475200e+23L,    /*  52!!! */
  6.946574393894367232000e+23L,    /*  53!!! */
  2.480410751833883860992e+24L,    /*  54!!! */
  7.368624314106569113600e+24L,    /*  55!!! */
  3.890081660580845649920e+25L,    /*  56!!! */
  1.413834128545313800765e+26L,    /*  57!!! */
  4.273802102181810085888e+26L,    /*  58!!! */
  2.295148179742698933453e+27L,    /*  59!!! */
  8.483004771271882804593e+27L,    /*  60!!! */
  2.607019282330904152392e+28L,    /*  61!!! */
  1.422991871440473338741e+29L,    /*  62!!! */
  5.344293005901286166893e+29L,    /*  63!!! */
  1.668492340691778657531e+30L,    /*  64!!! */
  9.249447164363076701815e+30L,    /*  65!!! */
  3.527233383894848870150e+31L,    /*  66!!! */
  1.117889868263491700546e+32L,    /*  67!!! */
  6.289624071766892157234e+32L,    /*  68!!! */
  2.433791034887445720403e+33L,    /*  69!!! */
  7.825229077844441903819e+33L,    /*  70!!! */
  4.465633090954493431636e+34L,    /*  71!!! */
  1.752329545118960918690e+35L,    /*  72!!! */
  5.712417226826442589788e+35L,    /*  73!!! */
  3.304568487306325139411e+36L,    /*  74!!! */
  1.314247158839220689018e+37L,    /*  75!!! */
  4.341437092388096368239e+37L,    /*  76!!! */
  2.544517735225870357346e+38L,    /*  77!!! */
  1.025112783894592137434e+39L,    /*  78!!! */
  3.429735302986596130909e+39L,    /*  79!!! */
  2.035614188180696285877e+40L,    /*  80!!! */
  8.303413549546196313214e+40L,    /*  81!!! */
  2.812382948449008827345e+41L,    /*  82!!! */
  1.689559776189977917278e+42L,    /*  83!!! */
  6.974867381618804903100e+42L,    /*  84!!! */
  2.390525506181657503243e+43L,    /*  85!!! */
  1.453021407523381008859e+44L,    /*  86!!! */
  6.068134622008360265697e+44L,    /*  87!!! */
  2.103662445439858602854e+45L,    /*  88!!! */
  1.293189052695809097885e+46L,    /*  89!!! */
  5.461321159807524239127e+46L,    /*  90!!! */
  1.914332825350271328597e+47L,    /*  91!!! */
  1.189733928480144370054e+48L,    /*  92!!! */
  5.079028678620997542388e+48L,    /*  93!!! */
  1.799472855829255048881e+49L,    /*  94!!! */
  1.130247232056137151551e+50L,    /*  95!!! */
  4.875867531476157640693e+50L,    /*  96!!! */
  1.745488670154377397415e+51L,    /*  97!!! */
  1.107642287415014408520e+52L,    /*  98!!! */
  4.827108856161396064286e+52L,    /*  99!!! */
  1.745488670154377397415e+53L,    /* 100!!! */
  1.118718710289164552605e+54L,    /* 101!!! */
  4.923651033284623985572e+54L,    /* 102!!! */
  1.797853330259008719337e+55L,    /* 103!!! */
  1.163467458700731134709e+56L,    /* 104!!! */
  5.169833584948855184850e+56L,    /* 105!!! */
  1.905724530074549242498e+57L,    /* 106!!! */
  1.244910180809782314139e+58L,    /* 107!!! */
  5.583420271744763599638e+58L,    /* 108!!! */
  2.077239737781258674322e+59L,    /* 109!!! */
  1.369401198890760545553e+60L,    /* 110!!! */
  6.197596501636687595598e+60L,    /* 111!!! */
  2.326508506315009715241e+61L,    /* 112!!! */
  1.547423354746559416475e+62L,    /* 113!!! */
  7.065260011865823858982e+62L,    /* 114!!! */
  2.675484782262261172527e+63L,    /* 115!!! */
  1.795011091506008923111e+64L,    /* 116!!! */
  8.266354213883013915009e+64L,    /* 117!!! */
  3.157072043069468183582e+65L,    /* 118!!! */
  2.136063198892150618502e+66L,    /* 119!!! */
  9.919625056659616698011e+66L,    /* 120!!! */
  3.820057172114056502134e+67L,    /* 121!!! */
  2.605997102648423754572e+68L,    /* 122!!! */
  1.220113881969132853855e+69L,    /* 123!!! */
  4.736870893421430062647e+69L,    /* 124!!! */
  3.257496378310529693216e+70L,    /* 125!!! */
  1.537343491281107395858e+71L,    /* 126!!! */
  6.015826034645216179561e+71L,    /* 127!!! */
  4.169595364237478007316e+72L,    /* 128!!! */
  1.983173103752628540656e+73L,    /* 129!!! */
  7.820573845038781033430e+73L,    /* 130!!! */
  5.462169927151096189584e+74L,    /* 131!!! */
  2.617788496953469673667e+75L,    /* 132!!! */
  1.040136321390157877446e+76L,    /* 133!!! */
  7.319307702382468894042e+76L,    /* 134!!! */
  3.534014470887184059450e+77L,    /* 135!!! */
  1.414585397090614713327e+78L,    /* 136!!! */
  1.002745155226398238484e+79L,    /* 137!!! */
  4.876939969824314002041e+79L,    /* 138!!! */
  1.966273701955954451524e+80L,    /* 139!!! */
  1.403843217316957533877e+81L,    /* 140!!! */
  6.876485357452282742877e+81L,    /* 141!!! */
  2.792108656777455321164e+82L,    /* 142!!! */
  2.007495800763249273445e+83L,    /* 143!!! */
  9.902138914731287149744e+83L,    /* 144!!! */
  4.048557552327310215688e+84L,    /* 145!!! */
  2.930943869114343939229e+85L,    /* 146!!! */
  1.455614420465499211012e+86L,    /* 147!!! */
  5.991865177444419119219e+86L,    /* 148!!! */
  4.367106364980372469451e+87L,    /* 149!!! */
  2.183421630698248816518e+88L,    /* 150!!! */
  9.047716417941072870020e+88L,    /* 151!!! */
  6.638001674770166153566e+89L,    /* 152!!! */
  3.340635094968320689273e+90L,    /* 153!!! */
  1.393348328362925221983e+91L,    /* 154!!! */
  1.028890259589375753803e+92L,    /* 155!!! */
  5.211390748150580275266e+92L,    /* 156!!! */
  2.187556875529792598514e+93L,    /* 157!!! */
  1.625646610151213691008e+94L,    /* 158!!! */
  8.286111289559422637673e+94L,    /* 159!!! */
  3.500091000847668157622e+95L,    /* 160!!! */
  2.617291042343454042523e+96L,    /* 161!!! */
  1.342350028908626467303e+97L,    /* 162!!! */
  5.705148331381699096923e+97L,    /* 163!!! */
  4.292357309443264629738e+98L,    /* 164!!! */
  2.214877547699233671050e+99L,    /* 165!!! */
  9.470546230093620500893e+99L,    /* 166!!! */
  7.168236706770251931663e+100L,   /* 167!!! */
  3.720994280134712567364e+101L,   /* 168!!! */
  1.600522312885821864651e+102L,   /* 169!!! */
  1.218600240150942828383e+103L,   /* 170!!! */
  6.362900219030358490193e+103L,   /* 171!!! */
  2.752898378163613607199e+104L,   /* 172!!! */
  2.108178415461131093102e+105L,   /* 173!!! */
  1.107144638111282377294e+106L,   /* 174!!! */
  4.817572161786323812599e+106L,   /* 175!!! */
  3.710394011211590723860e+107L,   /* 176!!! */
  1.959646009456969807810e+108L,   /* 177!!! */
  8.575278447979656386426e+108L,   /* 178!!! */
  6.641605280068747395709e+109L,   /* 179!!! */
  3.527362817022545654057e+110L,   /* 180!!! */
  1.552125399084317805943e+111L,   /* 181!!! */
  1.208772160972512026019e+112L,   /* 182!!! */
  6.455073955151258546925e+112L,   /* 183!!! */
  2.855910734315144762935e+113L,   /* 184!!! */
  2.236228497799147248135e+114L,   /* 185!!! */
  1.200643755658134089728e+115L,   /* 186!!! */
  5.340553073169320706689e+115L,   /* 187!!! */
  4.204109575862396826494e+116L,   /* 188!!! */
  2.269216698193873429586e+117L,   /* 189!!! */
  1.014705083902170934271e+118L,   /* 190!!! */
  8.029849289897177938604e+118L,   /* 191!!! */
  4.356896060532236984805e+119L,   /* 192!!! */
  1.958380811931189903143e+120L,   /* 193!!! */
  1.557790762240052520089e+121L,   /* 194!!! */
  8.495947318037862120370e+121L,   /* 195!!! */
  3.838426391385132210160e+122L,   /* 196!!! */
  3.068847801612903464576e+123L,   /* 197!!! */
  1.682197568971496699833e+124L,   /* 198!!! */
  7.638468518856413098219e+124L,   /* 199!!! */
  6.137695603225806929151e+125L,   /* 200!!! */
  3.381217113632708366665e+126L,   /* 201!!! */
  1.542970640808995445840e+127L,   /* 202!!! */
  1.245952207454838806618e+128L,   /* 203!!! */
  6.897682911810725067996e+128L,   /* 204!!! */
  3.163089813658440663972e+129L,   /* 205!!! */
  2.566661547356967941632e+130L,   /* 206!!! */
  1.427820362744820089075e+131L,   /* 207!!! */
  6.579226812409556581063e+131L,   /* 208!!! */
  5.364322633976062998012e+132L,   /* 209!!! */
  2.998422761764122187058e+133L,   /* 210!!! */
  1.388216857418416438604e+134L,   /* 211!!! */
  1.137236398402925355579e+135L,   /* 212!!! */
  6.386640482557580258433e+135L,   /* 213!!! */
  2.970784074875411178613e+136L,   /* 214!!! */
  2.445058256566289514494e+137L,   /* 215!!! */
  1.379514344232437335822e+138L,   /* 216!!! */
  6.446601442479642257590e+138L,   /* 217!!! */
  5.330226999314511141596e+139L,   /* 218!!! */
  3.021136413869037765449e+140L,   /* 219!!! */
  1.418252317345521296670e+141L,   /* 220!!! */
  1.177980166848506962293e+142L,   /* 221!!! */
  6.706922838789263839297e+142L,   /* 222!!! */
  3.162702667680512491574e+143L,   /* 223!!! */
  2.638675573740655595536e+144L,   /* 224!!! */
  1.509057638727584363842e+145L,   /* 225!!! */
  7.147708028957958230957e+145L,   /* 226!!! */
  5.989793552391288201867e+146L,   /* 227!!! */
  3.440651416298892349560e+147L,   /* 228!!! */
  1.636825138631372434889e+148L,   /* 229!!! */
  1.377652517049996286429e+149L,   /* 230!!! */
  7.947904771650441327482e+149L,   /* 231!!! */
  3.797434321624784048943e+150L,   /* 232!!! */
  3.209930364726491347380e+151L,   /* 233!!! */
  1.859809716566203270631e+152L,   /* 234!!! */
  8.923970655818242515015e+152L,   /* 235!!! */
  7.575435660754519579817e+153L,   /* 236!!! */
  4.407749028261901751395e+154L,   /* 237!!! */
  2.123905016084741718574e+155L,   /* 238!!! */
  1.810529122920330179576e+156L,   /* 239!!! */
  1.057859766782856420335e+157L,   /* 240!!! */
  5.118611088764227541762e+157L,   /* 241!!! */
  4.381480477467199034575e+158L,   /* 242!!! */
  2.570599233282341101414e+159L,   /* 243!!! */
  1.248941105658471520190e+160L,   /* 244!!! */
  1.073462716979463763471e+161L,   /* 245!!! */
  6.323674113874559109478e+161L,   /* 246!!! */
  3.084884530976424654869e+162L,   /* 247!!! */
  2.662187538109070133408e+163L,   /* 248!!! */
  1.574594854354765218260e+164L,   /* 249!!! */
  7.712211327441061637173e+164L,   /* 250!!! */
  6.682090720653766034853e+165L,   /* 251!!! */
  3.967979032974008350015e+166L,   /* 252!!! */
  1.951189465842588594205e+167L,   /* 253!!! */
  1.697251043046056572853e+168L,   /* 254!!! */
  1.011834653408372129254e+169L,   /* 255!!! */
  4.995045032557026801164e+169L,   /* 256!!! */
  4.361935180628365392231e+170L,   /* 257!!! */
  2.610533405793600093475e+171L,   /* 258!!! */
  1.293716663432269941502e+172L,   /* 259!!! */
  1.134103146963375001980e+173L,   /* 260!!! */
  6.813492189121296243970e+173L,   /* 261!!! */
  3.389537658192547246734e+174L,   /* 262!!! */
  2.982691276513676255208e+175L,   /* 263!!! */
  1.798761937928022208408e+176L,   /* 264!!! */
  8.982274794210250203846e+176L,   /* 265!!! */
  7.933958795526378838853e+177L,   /* 266!!! */
  4.802694374267819296449e+178L,   /* 267!!! */
  2.407249644848347054631e+179L,   /* 268!!! */
  2.134234915996595907651e+180L,   /* 269!!! */
  1.296727481052311210041e+181L,   /* 270!!! */
  6.523646537539020518049e+181L,   /* 271!!! */
  5.805118971510740868812e+182L,   /* 272!!! */
  3.540066023272809603413e+183L,   /* 273!!! */
  1.787479151285691621945e+184L,   /* 274!!! */
  1.596407717165453738923e+185L,   /* 275!!! */
  9.770582224232954505419e+185L,   /* 276!!! */
  4.951317249061365792789e+186L,   /* 277!!! */
  4.438013453719961394207e+187L,   /* 278!!! */
  2.725992440560994307012e+188L,   /* 279!!! */
  1.386368829737182421981e+189L,   /* 280!!! */
  1.247081780495309151772e+190L,   /* 281!!! */
  7.687298682382003945774e+190L,   /* 282!!! */
  3.923423788156226254206e+191L,   /* 283!!! */
  3.541712256606677991033e+192L,   /* 284!!! */
  2.190880124478871124545e+193L,   /* 285!!! */
  1.122099203412680708703e+194L,   /* 286!!! */
  1.016471417646116583426e+195L,   /* 287!!! */
  6.309734758499148838691e+195L,   /* 288!!! */
  3.242866697862647248151e+196L,   /* 289!!! */
  2.947767111173738091937e+197L,   /* 290!!! */
  1.836132814723252312059e+198L,   /* 291!!! */
  9.469170757758929964602e+198L,   /* 292!!! */
  8.636957635739052609374e+199L,   /* 293!!! */
  5.398230475286361797454e+200L,   /* 294!!! */
  2.793405373538884339558e+201L,   /* 295!!! */
  2.556539460178759572375e+202L,   /* 296!!! */
  1.603274451160049453844e+203L,   /* 297!!! */
  8.324348013145875331882e+203L,   /* 298!!! */
  7.644052985934491121400e+204L,   /* 299!!! */
  4.809823353480148361531e+205L,   /* 300!!! */
  2.505628751956908474896e+206L,   /* 301!!! */
  2.308504001752216318663e+207L,   /* 302!!! */
  1.457376476104484953544e+208L,   /* 303!!! */
  7.617111405949001763685e+208L,   /* 304!!! */
  7.040937205344259771922e+209L,   /* 305!!! */
  4.459572016879723957845e+210L,   /* 306!!! */
  2.338453201626343541451e+211L,   /* 307!!! */
  2.168608659246032009752e+212L,   /* 308!!! */
  1.378007753215834702974e+213L,   /* 309!!! */
  7.249204925041664978499e+213L,   /* 310!!! */
  6.744372930255159550329e+214L,   /* 311!!! */
  4.299384190033404273279e+215L,   /* 312!!! */
  2.269001141538041138270e+216L,   /* 313!!! */
  2.117733100100120098803e+217L,   /* 314!!! */
  1.354306019860522346083e+218L,   /* 315!!! */
  7.170043607260209996934e+218L,   /* 316!!! */
  6.713213927317380713206e+219L,   /* 317!!! */
  4.306693143156461060543e+220L,   /* 318!!! */
  2.287243910716006989022e+221L,   /* 319!!! */
  2.148228456741561828226e+222L,   /* 320!!! */
  1.382448498953224000434e+223L,   /* 321!!! */
  7.364925392505542504650e+223L,   /* 322!!! */
  6.938777915275244705170e+224L,   /* 323!!! */
  4.479133136608445761408e+225L,   /* 324!!! */
  2.393600752564301314011e+226L,   /* 325!!! */
  2.262041600379729773885e+227L,   /* 326!!! */
  1.464676535670961763980e+228L,   /* 327!!! */
  7.851010468410908309957e+228L,   /* 328!!! */
  7.442116865249310956083e+229L,   /* 329!!! */
  4.833432567714173821135e+230L,   /* 330!!! */
  2.598684465044010650596e+231L,   /* 331!!! */
  2.470782799262771237420e+232L,   /* 332!!! */
  1.609533045048819882438e+233L,   /* 333!!! */
  8.679606113246995572990e+233L,   /* 334!!! */
  8.277122377530283645355e+234L,   /* 335!!! */
  5.408031031364034804991e+235L,   /* 336!!! */
  2.925027260164237508098e+236L,   /* 337!!! */
  2.797667363605235872130e+237L,   /* 338!!! */
  1.833322519632407798892e+238L,   /* 339!!! */
  9.945092684558407527532e+238L,   /* 340!!! */
  9.540045709893854323964e+239L,   /* 341!!! */
  6.269963017142834672211e+240L,   /* 342!!! */
  3.411166790803533781944e+241L,   /* 343!!! */
  3.281775724203485887443e+242L,   /* 344!!! */
  2.163137240914277961913e+243L,   /* 345!!! */
  1.180263709618022688552e+244L,   /* 346!!! */
  1.138776176298609602943e+245L,   /* 347!!! */
  7.527717598381687307456e+245L,   /* 348!!! */
  4.119120346566899183048e+246L,   /* 349!!! */
  3.985716617045133610300e+247L,   /* 350!!! */
  2.642228877031972244917e+248L,   /* 351!!! */
  1.449930361991548512433e+249L,   /* 352!!! */
  1.406957965816932164436e+250L,   /* 353!!! */
  9.353490224693181747007e+250L,   /* 354!!! */
  5.147252785069997219137e+251L,   /* 355!!! */
  5.008770358308278505392e+252L,   /* 356!!! */
  3.339196010215465883681e+253L,   /* 357!!! */
  1.842716497055059004451e+254L,   /* 358!!! */
  1.798148558632671983436e+255L,   /* 359!!! */
  1.202110563677567718125e+256L,   /* 360!!! */
  6.652206554368763006068e+256L,   /* 361!!! */
  6.509297782250272580037e+257L,   /* 362!!! */
  4.363661346149570816795e+258L,   /* 363!!! */
  2.421403185790229734209e+259L,   /* 364!!! */
  2.375893690521349491714e+260L,   /* 365!!! */
  1.597100052690742918947e+261L,   /* 366!!! */
  8.886549691850143124546e+261L,   /* 367!!! */
  8.743288781118566129506e+262L,   /* 368!!! */
  5.893299194428841370914e+263L,   /* 369!!! */
  3.288023385984552956082e+264L,   /* 370!!! */
  3.243760137794988034047e+265L,   /* 371!!! */
  2.192307300327528989980e+266L,   /* 372!!! */
  1.226432722972238252619e+267L,   /* 373!!! */
  1.213166291535325524733e+268L,   /* 374!!! */
  8.221152376228233712425e+268L,   /* 375!!! */
  4.611387038375615829846e+269L,   /* 376!!! */
  4.573636919088177228245e+270L,   /* 377!!! */
  3.107595598214272343297e+271L,   /* 378!!! */
  1.747715687544358399512e+272L,   /* 379!!! */
  1.737982029253507346733e+273L,   /* 380!!! */
  1.183993922919637762796e+274L,   /* 381!!! */
  6.676273926419449086134e+274L,   /* 382!!! */
  6.656471172040933137988e+275L,   /* 383!!! */
  4.546536664011409009137e+276L,   /* 384!!! */
  2.570365461671487898162e+277L,   /* 385!!! */
  2.569397872407800191263e+278L,   /* 386!!! */
  1.759509688972415286536e+279L,   /* 387!!! */
  9.973017991285373044868e+279L,   /* 388!!! */
  9.994957723666342744015e+280L,   /* 389!!! */
  6.862087786992419617490e+281L,   /* 390!!! */
  3.899450034592580860543e+282L,   /* 391!!! */
  3.918023427677206355654e+283L,   /* 392!!! */
  2.696800500288020909674e+284L,   /* 393!!! */
  1.536383313629476859054e+285L,   /* 394!!! */
  1.547619253932496510483e+286L,   /* 395!!! */
  1.067932998114056280231e+287L,   /* 396!!! */
  6.099441755109023130445e+287L,   /* 397!!! */
  6.159524630651336111723e+288L,   /* 398!!! */
  4.261052662475084558121e+289L,   /* 399!!! */
  2.439776702043609252178e+290L,   /* 400!!! */
  2.469969376891185780801e+291L,   /* 401!!! */
  1.712943170314983992365e+292L,   /* 402!!! */
  9.832300109235745286277e+292L,   /* 403!!! */
  9.978676282640390554436e+293L,   /* 404!!! */
  6.937419839775685169077e+294L,   /* 405!!! */
  3.991913844349712586228e+295L,   /* 406!!! */
  4.061321247034638955656e+296L,   /* 407!!! */
  2.830467294628479548983e+297L,   /* 408!!! */
  1.632692762339032447767e+298L,   /* 409!!! */
  1.665141711284201971819e+299L,   /* 410!!! */
  1.163322058092305094632e+300L,   /* 411!!! */
  6.726694180836813684802e+300L,   /* 412!!! */
  6.877035267603754143612e+301L,   /* 413!!! */
  4.816153320502143091777e+302L,   /* 414!!! */
  2.791578085047277679193e+303L,   /* 415!!! */
  2.860846671323161723742e+304L,   /* 416!!! */
  2.008335934649393669271e+305L,   /* 417!!! */
  1.166879639549762069903e+306L,   /* 418!!! */
  1.198694755284404762248e+307L};  /* 419!!! */

static int math_trifact (lua_State *L) {  /* 2.40.2 */
  int n = agn_checknonnegint(L, 1);
  lua_pushnumber(L, (n < 420) ? triple_factorials[n] : HUGE_VAL);
  return 1;
}


#define diff(x)         fabs((x) - sun_round((x)))
static double aux_pochhammer (lua_Number x, lua_Number n, lua_Number limit) {
  int isnegintn, ismintx;
  lua_Number xplusn = x + n;
  isnegintn = tools_isnegint(n);
  if (x == 0 && !isnegintn)
    return (n == 0) ? 1 : 0;
  else if (x != 0.0 && n == 0.0)  /* like in Maple V R 4 */
    return 1;
  /* for !(...) condition see http://functions.wolfram.com/introductions/PDF/Pochhammer.pdf: */
  ismintx = tools_isint(-x);
  if ((xplusn == 0 || tools_isnegint(xplusn) || isnegintn || ((xplusn <= 0) && diff(xplusn) <= tools_matheps(xplusn)) ) &&
            !(ismintx && -x >= 0 && tools_isint(n) && n <= -x))
    return AGN_NAN;
  else if (n < 0 && (xplusn == 0 || tools_isnegint(xplusn)))
    return 1/aux_pochhammer(xplusn, -n, limit);
  else if (isnegintn)  /* put this before next condition, don't put it into conditions above */
    return 1/aux_pochhammer(xplusn, -n, limit);
  else if (tools_isnegint(x) && tools_isfrac(n))
    return 0;
  else if (tools_isposint(n) && n < limit) {  /* use Maple V R4's definition */
    lua_Number r;
    int i;
    r = 1.0;
    for (i=1; i <= n; i++) {
      r *= x;
      x += 1.0;  /* r is the result */
    }
    return r;
  }
  else if (xplusn >= limit && x > 0 && n > 0)  /* avoid overflow */
    return sun_exp(sun_lgamma(xplusn) - sun_lgamma(x));  /* 2.17.4 tweak */
  else
    return tools_gamma(xplusn)/tools_gamma(x);  /* 2.17.4 tweak */
}


/* Computes the Pochhammer function (rising factorial), where both x and n can be real numbers. It returns a number. Test the function independently with poch2.tst, put test cases into math2.tst ! */
static int math_pochhammer (lua_State *L) {  /* 2.13.0 */
  lua_Number n, limit;
  n = agn_checknumber(L, 2);
  limit = agnL_optnumber(L, 3, 33);  /* with n <= 32, simple MVR4 multiplication is faster, else (l)gamma versions */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      /* with x + n > 174, we are in infinity */
      lua_pushnumber(L, aux_pochhammer(x, n, limit));
      break;
    }
    case LUA_TCOMPLEX: {  /* 3.16.2 */
      int i;
      lua_Number r[2], z[2], re, im;
      if (tools_isfrac(n)) {
        luaL_error(L, "Error in " LUA_QS ": second argument must be integral.", "math.pochhammer");
      }
      agnCmplx_create(r, 1, 0);
      agn_getcmplxparts(L, 1, &re, &im);
      agnCmplx_create(z, re, im);
      for (i=1; i <= n; i++) {
        agnCmplx_mul(r, r[0], r[1], z[0], z[1]);
        agnCmplx_add(z, z[0], z[1], 1.0, 0);
      }
      agn_pushcomplex(L, r[0], r[1]);
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.pochhammer");
  }
  return 1;
}


/* Returns ln(abs(x)) for numeric or complex x. With complex numbers, takes care of underflows. 2.16.13 */
static int math_lnabs (lua_State *L) {
  if (lua_isnumber(L, 1)) {
    lua_pushnumber(L, sun_log(fabs(agn_tonumber(L, 1))));  /* sun_log takes care of x <= 0 and returns `undefined` in his case */
  } else if (lua_iscomplex(L, 1)) {
    lua_Number a, b, max, q;
    agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
    if (a >= b) {
      max = a;
      q = luai_numdiv(b, a);
    } else {
      max = b;
      q = luai_numdiv(a, b);
    }
    if (tools_isnan(q)) q = 0;
    lua_pushnumber(L, sun_log(max) + 0.5*log1p(q*q));  /* handles underflows if q is close to 0 */
  } else
    luaL_nonumorcmplx(L, 1, "math.lnabs");
  return 1;
}


/* Returns ln sqrt(a*a + b*b) = ln hypot(a, b), avoiding undue underflow with |a*a + b*b| ~ 0; 3.16.4; returns 0 with a = b = 0.
   ln sqrt(a^2+b^2) = ln(a*sqrt(1+b^2/a^2)) = ln a + 0.5*ln(1+b^2/a^2)  */
static int math_lnhypot (lua_State *L) {
  int plusone;
  lua_Number re, im, max, q;
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      re = agn_tonumber(L, 1);
      im = agn_checknumber(L, 2);
      plusone = lua_gettop(L) == 3;
      break;
    }
    case LUA_TCOMPLEX: {
      agn_getcmplxparts(L, 1, &re, &im);
      plusone = lua_gettop(L) == 2;
      break;
    } \
    default: {
      re = im = AGN_NAN;
      plusone = 0;
      luaL_nonumorcmplx(L, 1, "math.lnhypot");
    }
  }
  if (!plusone) {
    re = fabs(re); im = fabs(im);
    if (re >= im) {
      max = re;
      q = luai_numdiv(im, re);
    } else {
      max = im;
      q = luai_numdiv(re, im);
    }
    lua_pushnumber(L, re == 0 && im == 0 ? 0 : sun_log(max) + 0.5*log1p(q*q));  /* ln sqrt(a^2 + b^2) */
  } else {  /* the result is always non-negative, 4.5.6 */
    lua_pushnumber(L, log1p(sun_hypot(re, im)));  /* ln(1 + sqrt(a^2 + b^2)) */
  }
  return 1;
}


/* returns ln(a*a + b*b) or ln(a*a + b*b + 1), avoiding undue underflow with |a*a + b*b| ~ 0; 3.16.4; returns 0 with a = b = 0. */
static int math_lnpytha (lua_State *L) {
  int plusone;
  lua_Number re, im, max, q, t1, t2, t3;
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      re = agn_tonumber(L, 1);
      im = agn_checknumber(L, 2);
      plusone = lua_gettop(L) == 3;
      break;
    }
    case LUA_TCOMPLEX: {
      agn_getcmplxparts(L, 1, &re, &im);
      plusone = lua_gettop(L) == 2;
      break;
    }
    default: {
      re = im = AGN_NAN;
      plusone = 0;
      luaL_nonumorcmplx(L, 1, "lnhypot");
    }
  }
  re = fabs(re); im = fabs(im);
  if (re >= im) {
    max = re;
    q = luai_numdiv(im, re);
  } else {
    max = im;
    q = luai_numdiv(re, im);
  }
  t1 = 2*sun_log(max);
  t2 = q*q;  /* t2 <= 1 */
  if (!plusone) {
    lua_pushnumber(L, re == 0 && im == 0 ? 0 : t1 + log1p(t2));  /* ln(a^2 + b^2) */
  } else {  /* the result is always non-negative, 4.5.6 */
    t3 = 1.0/max;
    lua_pushnumber(L, re == 0 && im == 0 ? 0 : t1 + log1p(t2 + t3*t3));  /* ln(1 + a^2 + b^2) */
  }
  return 1;
}


/* Computes the rectangular pulse function for number x. a represents the rising edge, and b the falling edge
   of the rectangular pulse function. By default, a = -0.5 and b = +0.5.

   It returns 0 if x < a or x > b; 0.5 if (x = a or x = b) and a <> b, and 1 otherwise.

   See also: heaviside.

   See: https://de.mathworks.com/help/symbolic/rectangularpulse.html?requestedDomain=www.mathworks.com,
        https://en.wikipedia.org/wiki/Rectangular_function */
static int math_rectangular (lua_State *L) {  /* 2.4.2 / 2.11.0 RC1 */
  lua_Number x, a, b, c;
  int pi;
  c = 0.5;
  x = agn_checknumber(L, 1);
  if (lua_gettop(L) == 2 && lua_isboolean(L, 2)) {
    a = -c; b = c; pi = lua_toboolean(L, 2);
  } else {
    a = agnL_optnumber(L, 2, -c);
    b = agnL_optnumber(L, 3, c);
    pi = agnL_optboolean(L, 4, 0);
  }
  if (a > b)
    luaL_error(L, "Error in " LUA_QS ": second argument larger than third one.", "math.rectangular");
  if (x < a || x > b)  /* 2.11.0 RC2 patch */
    lua_pushnumber(L, 0);
  else if ((x == a || x == b) && a != b) {
    if (!pi)  /* 2.11.0 RC2 */
      lua_pushnumber(L, c);
    else  /* box distribution Pi(x): 1 if |x| < 1/2, 0 if |x| > 1/2, undefined if |x| = 0.5 */
      lua_pushnumber(L, AGN_NAN);
  } else
    lua_pushnumber(L, 1);
  return 1;
}


/* Computes the triangular function for the given number x. By default, the function returns 0 if |x| < 0.5,
   and |x| otherwise.

   By passing a left and a right border a, b, the function returns non-zero values in this range, and 0 otherwise,
   so the general formula used by the function is:

   math.triangular(x, a, b) := max(0, 1 - |2*(x - offset)/d|), where d := |b - a| and offset := a + d/2. */
static int math_triangular (lua_State *L) {  /* 2.11.0 RC2 */
  lua_Number x, a, b, c, d, offset;
  c = 0.5;
  x = agn_checknumber(L, 1);
  a = agnL_optnumber(L, 2, -c);
  b = agnL_optnumber(L, 3, c);
  if (a > b)
    luaL_error(L, "Error in " LUA_QS ": second argument larger than third one.", "math.triangular");
  d = fabs(b - a);
  offset = a + 0.5*d;  /* shift on abscissa with respect to zero */
  x -= offset;  /* shift point to new centre */
  /* for formula and alternatives, see: https://en.wikipedia.org/wiki/Triangular_function */
  lua_pushnumber(L, fMax(0, 1 - fabs(x*2/d)));
  return 1;
}


/* Returns 0 if its number argument x is zero or close to zero, and 1 otherwise:
   0 if |x| <= eps
   1 if |x| > eps

   With complex numbers a + I*b, returns
   0 if |a| <= eps and |b| <= eps
   1 if |a| > eps and |b| > eps

   By default, eps is set to the constant Eps.

   See also: heaviside, math.rectangular, math.zeroin. */
static int math_unitise (lua_State *L) {  /* 2.11.0 RC1 */
  int type;
  lua_Number eps;
  type = lua_type(L, 1);
  eps = agnL_optnonnegative(L, 2, agn_getepsilon(L));  /* 2.13.0 change */
  if (type == LUA_TNUMBER) {
    lua_Number x = fabs(agn_tonumber(L, 1));
    lua_pushinteger(L, x > eps);
  } else if (type == LUA_TCOMPLEX) {
    lua_Number a, b;
    agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
    lua_pushinteger(L, fabs(a) > eps && fabs(b) > eps);
  } else
    luaL_nonumorcmplx(L, 1, "math.unitise");
  return 1;
}


/* Sets a number or complex number x to 0 if |x| <= DoubleEps. With a complex number x, sets its respective parts to zero
   if there absolute values are less or equal to DoubleEps or if its magnitude |x| <= DoubleEps. */
static int math_zeroin (lua_State *L) {  /* reintroduced 2.30.3 */
  int type;
  lua_Number eps;
  type = lua_type(L, 1);
  eps = agnL_optnonnegative(L, 2, agn_getdblepsilon(L));
  if (type == LUA_TNUMBER) {
    lua_Number x = agn_tonumber(L, 1);
    lua_pushnumber(L, fabs(x) <= eps ? 0 : x); /* 2.41.2 fix */
  } else if (type == LUA_TCOMPLEX) {
    lua_Number a, b;
    agn_getcmplxparts(L, 1, &a, &b);
    if (sun_hypot(a, b) <= eps) {
      agn_pushcomplex(L, 0, 0);
    } else {
      agn_pushcomplex(L, fabs(a) <= eps ? 0 : a, fabs(b) <= eps ? 0 : b);
    }
  } else
    luaL_nonumorcmplx(L, 1, "math.zeroin");
  return 1;
}



/* Returns x clipped to be between a and b. The return is x if a <= x <= b, a if x < a, and b if x > b.
   By default a = -1, b = +1.
   The function is a port of Mathematica's Clip function.
   See http://reference.wolfram.com/language/ref/Clip.html */

static int math_clip (lua_State *L) {  /* 2.11.0 RC2, extended 2.13.0 */
  lua_Number x, a, b;
  x = agn_checknumber(L, 1);
  a = agnL_optnumber(L, 2, -1);
  b = agnL_optnumber(L, 3, 1);
  if (lua_gettop(L) == 2) {  /* ex-math.symtrunc emulation */
    if (a > 0) a = -a;
    b = -a;
  }
  if (a > b)
    luaL_error(L, "Error in " LUA_QS ": second argument larger than third one.", "math.clip");
  if (!lua_isfunction(L, 4)) {
    x = tools_clip(x, a, b);  /* 4.12.4 change */
  } else {  /* 2.13.0, we have a function */
    if (x < a || x > b) {    /* x not part of [a, b] ? */
      lua_pushvalue(L, 4);   /* push function */
      lua_pushnumber(L, x);  /* push argument */
      lua_call(L, 1, 1);     /* leaves result on stack */
      if (!agn_isnumber(L, -1)) {
        agn_poptop(L);
        luaL_error(L, "Error in " LUA_QS ": function must return a number.", "math.clip");
      }
      x = agn_tonumber(L, -1);
      agn_poptop(L);  /* level the stack */
    }  /* else x in [a, b] as just return x unchanged */
  }  /* stack is leveled */
  lua_pushnumber(L, x);
  return 1;
}


/* The Dirac delta function, also known as the impulse function, returns 0 for all numbers x other than 0, and `undefined`
   if x = 0, iff eps is set to zero which is the default.

   If eps is set to any positive value x, returns 1/(2*eps)*exp(-|x|/eps) even if x = 0.

   For formula(s), see: https://www.maplesoft.com/support/help/maple/view.aspx?path=Dirac */

static void aux_checknumoptions (lua_State *L, int pos, int *nargs, int *classic, const char *procname) {
  int checkoptions;
  *classic = 0;      /* 1 = `math.dirac`: return infinity instead of undefined at the origin */
  /* check for options, here `map in-place` */
  checkoptions = 1;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "classic")) {
        *classic = agn_checkboolean(L, -1);
        /* we delete the option so that the agnL_optnumber() check in `math.dirac` doesn't issue an error. */
        luaL_checkstack(L, 1, "not enough stack space");
        lua_pushnil(L);
        lua_replace(L, *nargs);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
}

static int math_dirac (lua_State *L) {  /* 2.11.0 RC2 */
  int nargs, classic;
  lua_Number x, eps, reps;
  nargs = lua_gettop(L);
  x = agn_checknumber(L, 1);
  if (tools_isnan(x)) {  /* 2.14.13 fix */
    lua_pushundefined(L);
    return 1;
  }
  aux_checknumoptions(L, 2, &nargs, &classic, "math.dirac");
  eps = agnL_optnumber(L, 2, 0);
  if (eps == 0) {  /* ideal case, the default: 0 if x <> 0 and `undefined` if x = 0 */
    lua_pushnumber(L, (x == 0) ? classic ? HUGE_VAL : AGN_NAN : 0);  /* 6.7.8 change from inf to either inf or nan */
  } else {
    reps = 1/eps;
    lua_pushnumber(L, 0.5*reps * sun_exp(-fabs(x)*reps));  /* 0.638 secs, best convergence, max. value is 33554432 */
  }
  /* piecewise(And(-eps/2 < x, x < eps/2), 1/eps, fabs(x) > eps/2, 0); */
  /* lua_pushnumber(L, 1/M_PI * eps/(x*x + eps*eps));   0.197 secs, slow convergence to infinity, max. value is 21361414.861763 */
  /* lua_pushnumber(L, 1/M_PI * sin(x/eps)/x);          0.369 secs, slow convergence to infinity, max. value is 21361414.861763 */
  return 1;
}


/* For the given number x, returns
   - 0 if x is zero (= constant `math.fp_zero`),
   - 1 if x is infinite, i.e. +/-infinity (= constant `math.fp_infinite`),
   - 2 if x is undefined (= constant `math.fp_nan`),
   - 3 if x is normal (= constant `math.fp_normal`),
   - 4 is x is subnormal (= constant `math.fp_subnormal`; fpclassify might not return correct results in this case on some systems).
   The function returns `fail` if it could not determine the type of floating-point number (of C type double). It is a port to C's
   fpclassify.

   See also: ...

   The function has originally been written by Luiz Henrique de Figueiredo, `C99 math functions for Lua` library file
   lmathx.c, on 04 Apr 2010 22:53:32: `This code is hereby placed in the public domain.` Mapping to integers changed 2.25.4. */
static int math_fpclassify (lua_State *L) {  /* 2.11.0 RC2 */
  switch (tools_fpclassify(agn_checknumber(L, 1))) {  /* call portable version of fpclassify */
    case FP_NAN:
      lua_pushinteger(L, 0); break;
    case FP_INFINITE:
      lua_pushinteger(L, 1); break;
    case FP_SUBNORMAL:
      lua_pushinteger(L, 2); break;
    case FP_ZERO:
      lua_pushinteger(L, 3); break;
    case FP_NORMAL:
      lua_pushinteger(L, 4); break;
    default:
      lua_pushfail(L);
  }
  return 1;
}


/* Shrinks a number x more or less near zero to exactly zero, using one of many methods.

   The default for eps is Eps. The standard method is 0 for hard shrinking.

   In the first form performs hard shrinking by returning
   - 0 if x < eps,
   - and x otherwise.
   In the second form performs soft shrinking by returning:
   - 0 if |x| <= eps,
   - sign(x) * (abs(x) - eps) if |x| > eps.
   eps is Eps by default.
   The function is a port of Mathematica's Chop function.
   See http://reference.wolfram.com/language/ref/Chop.html &
       https://reference.wolfram.com/legacy/v8/ref/Threshold.html */

static lua_Number aux_chop (lua_State *L, lua_Number x, lua_Number dx, int n, int kind) {  /* 2.11.1 */
  switch (kind) {
    case 0:  /* "Hard" */
      return (fabs(x) >= dx)*x;
    case 1:  /* "Soft" */
      return tools_signum(x)*(fMax(0, fabs(x) - dx));  /* 2.17.1 optimisation */
    case 2:  /* "Firm" */
      luaL_error(L, "Error in " LUA_QS ": `firm` method not yet supported.", "math.chop");
      return 0;
    case 3:  /* "PiecewiseGarrote" */
      return (fabs(x) >= dx) * (x - dx*dx/x);
    case 4:  /* "SmoothGarrote" */
      return tools_intpow(x, 2*n + 1)/(tools_intpow(x, 2*n) + tools_intpow(dx, 2*n));
    case 5:  /* "Hyperbola" */
      return (fabs(x) >= dx) * tools_signum(x)*tools_mhypot(x, dx);  /* 2.17.1 optimisation */
    default:
      luaL_error(L, "Error in " LUA_QS ": wrong option %ls.", "math.chop", kind);
      return 0;
  }
}

static int math_chop (lua_State *L) {  /* 2.11.0 RC2 */
  lua_Number dx;
  int kind, n;
  dx = agnL_optnumber(L, 2, agn_getepsilon(L));
  n = agnL_optinteger(L, 4, 1);
  kind = 0;
  if (lua_gettop(L) == 3) {
    if (lua_isboolean(L, 3))
      kind = lua_toboolean(L, 3);
    else if (agn_isnumber(L, 3))
      kind = lua_tointeger(L, 3);
    else
      luaL_error(L, "Error in " LUA_QS ": third argument must be a number or boolean.", "math.chop");
  }
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_checknumber(L, 1);
      lua_pushnumber(L, aux_chop(L, x, dx, n, kind));
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
      a = aux_chop(L, a, dx, n, kind);
      b = aux_chop(L, b, dx, n, kind);
      agn_pushcomplex(L, a, b);  /* 2.17.6 */
     break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.chop");
  }
  return 1;
}


/* static const double shuge = 1.0e307; */

static int math_sincos (lua_State *L) {  /* 2.11.0, 15 percent faster than calling Agena's sin and cos operators */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x, s, c;
      x = agn_checknumber(L, 1);
      sun_sincos(x, &s, &c);
      lua_pushnumber(L, s);
      lua_pushnumber(L, c);
      break;
    }
    case LUA_TCOMPLEX: {  /* 2.30.3 */
      lua_Number a, b, i, si, co, sih, coh;
      agn_getcmplxparts(L, 1, &a, &b);
      sun_sincos(a, &si, &co);
      sun_sinhcosh(b, &sih, &coh);  /* 4.5.7 overflow tweak */
      i = co*sih;
      if (i == -0) i = 0;
      agn_pushcomplex(L, si*coh, i);
      i = -si*sih;
      if (i == -0) i = 0;
      agn_pushcomplex(L, co*coh, i);
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.sincos");
  }
  return 2;
}


static int math_sinhcosh (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number z, sih, coh;
      z = agn_checknumber(L, 1);
      sun_sinhcosh(z, &sih, &coh);  /* 4.5.7 overflow tweak */
      lua_pushnumber(L, sih);
      lua_pushnumber(L, coh);
      break;
    }
    case LUA_TCOMPLEX: {  /* 2.30.3 */
      lua_Number a, b, i, si, co, sih, coh;
      agn_getcmplxparts(L, 1, &a, &b);
      sun_sinhcosh(a, &sih, &coh);  /* 4.5.7 overflow tweak */
      sun_sincos(b, &si, &co);
      i = coh*si;
      if (i == -0) i = 0;
      agn_pushcomplex(L, sih*co, i);
      i = sih*si;
      if (i == -0) i = 0;
      agn_pushcomplex(L, coh*co, i);
     break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "math.sinhcosh");
  }
  return 2;
}


/* Returns both sin(Pi*x) and cos(Pi*x), plus tan(Pi*x) if 2nd arg is true; 2.35.0 */
static int math_sincospi (lua_State *L) {
  int gettangent = agnL_optboolean(L, 2, 0);
  if (agn_isnumber(L, 1)) {
    lua_Number sp, cp, tp;
    tools_sincospi(agn_tonumber(L, 1), &sp, &cp, &tp, gettangent);
    lua_pushnumber(L, sp);
    lua_pushnumber(L, cp);
    if (gettangent) lua_pushnumber(L, tp);
  } else if (lua_iscomplex(L, 1)) {  /* 4.5.3 extension */
    double a, b;
    long double t1, t3, t4, t5, si, co;
    gettangent = 0;
    agn_getcmplxparts(L, 1, &a, &b);
    t1 = M_PIld*(long double)a;
    t3 = M_PIld*(long double)b;
    sun_sincosl(t1, &si, &co);
    /* t9 = sin(t1)*cosh(t3)+sqrt(-1.0)*cos(t1)*sinh(t3) */
    t4 = tools_coshl(t3);
    t5 = tools_sinhl(t3);
    agn_pushcomplex(L, (lua_Number)(si*t4), (lua_Number)(co*t5));
    agn_pushcomplex(L, (lua_Number)(co*t4), (lua_Number)(-si*t5));
  } else {
    luaL_nonumorcmplx(L, 1, "math.sinpi");
  }
  return 2 + gettangent;
}


static int math_sinpi (lua_State *L) {  /* 2.35.0 */
  if (agn_isnumber(L, 1)) {
    lua_Number sp, cp, tp;
    tools_sincospi(agn_tonumber(L, 1), &sp, &cp, &tp, 0);
    lua_pushnumber(L, sp);
  } else if (lua_iscomplex(L, 1)) {  /* 4.5.3 extension */
    double a, b;
    long double t1, t3, si, co;
    agn_getcmplxparts(L, 1, &a, &b);
    t1 = M_PIld*(long double)a;
    t3 = M_PIld*(long double)b;
    sun_sincosl(t1, &si, &co);
    /* t9 = sin(t1)*cosh(t3)+sqrt(-1.0)*cos(t1)*sinh(t3) */
    agn_pushcomplex(L, (lua_Number)(si*tools_coshl(t3)), (lua_Number)(co*tools_sinhl(t3)));
  } else {
    luaL_nonumorcmplx(L, 1, "math.sinpi");
  }
  return 1;
}


static int math_cospi (lua_State *L) {  /* 2.35.0 */
  if (agn_isnumber(L, 1)) {
    lua_Number sp, cp, tp;
    tools_sincospi(agn_checknumber(L, 1), &sp, &cp, &tp, 0);
    lua_pushnumber(L, cp);
  } else if (lua_iscomplex(L, 1)) {  /* 4.5.3 extension */
    double a, b;
    long double t1, t3, si, co;
    agn_getcmplxparts(L, 1, &a, &b);
    t1 = M_PIld*(long double)a;
    t3 = M_PIld*(long double)b;
    sun_sincosl(t1, &si, &co);
    /* t9 = cos(t1)*cosh(t3)-sqrt(-1.0)*sin(t1)*sinh(t3) */
    agn_pushcomplex(L, (lua_Number)(co*tools_coshl(t3)), (lua_Number)(-si*tools_sinhl(t3)));
  } else {
    luaL_nonumorcmplx(L, 1, "math.cospi");
  }
  return 1;
}


static int math_tanpi (lua_State *L) {  /* 2.35.0 */
  if (agn_isnumber(L, 1)) {
    lua_Number sp, cp, tp;
    tools_sincospi(agn_checknumber(L, 1), &sp, &cp, &tp, 1);
    lua_pushnumber(L, tp);
  } else if (lua_iscomplex(L, 1)) {  /* 4.5.3 extension */
    double a, b;
    long double t1, t3, t5, t6, t10, t11, t12, si;
    agn_getcmplxparts(L, 1, &a, &b);
    t1 = M_PIld*(long double)a;
    t5 = M_PIld*(long double)b;
    sun_sincosl(t1, &si, &t3);
    t6 = tools_sinhl(t5);
    t10 = t3*t3;
    t11 = t6*t6;
    t12 = t10 + t11;
    if (t12 == 0.0) {
      agn_pushcomplexnan(L);
    } else {
      agn_pushcomplex(L, (lua_Number)((si*t3)/t12), (lua_Number)((t6*tools_coshl(t5))/t12));
    }
  } else {
    luaL_nonumorcmplx(L, 1, "math.tanpi");
  }
  return 1;
}


/* Returns sin(Pi*x)/(Pi*x) with number or complex number x, preserving accuracy especially with larger x and with x around the origin.
   With x = 0, returns 1. The type of return depends on the type of input. 4.5.3 */
static int math_sincpi (lua_State *L) {
  if (agn_isnumber(L, 1)) {
    lua_Number x = agn_tonumber(L, 1);
    if (fabs(x) < 0.025) {  /* avoid catastrophic cancellation, max. absolute error is -.88e-23 */
      lua_Number z = x*x;
      lua_pushnumber(L,
         1.0 + (
        -1.6449340668482264364724150 + (
         0.8117424252833536436370024 + (
        -0.1907518241220842136964720 + (
         0.2614784781765480050465324e-1 -
         0.2346081035455823637508933e-2*z)*z)*z)*z)*z
      );
    } else {
      lua_Number sp, cp, tp;
      tools_sincospi(x, &sp, &cp, &tp, 0);
      lua_pushnumber(L, x == 0.0 ? 1 : sp/(M_PI*x));
    }
  } else if (lua_iscomplex(L, 1)) {
    /* Maple V Release 4:
       > restart; Digits := 25:
       > assume(a, real, b, real); z := a + I*b:
       > e := sin(Pi*z)/(Pi*z):
       > f := evalc(e):
       > readlib(C)(f, optimized); */
    lua_Number a, b;
    long double sp, cp, shp, chp, t1, t5, t8, t9, t11, t12, t16, t18;
    agn_getcmplxparts(L, 1, &a, &b);
    if (a == 0.0 && b == 0.0) {
      agn_pushcomplex(L, 1, 0);
    } else {
      sun_sincosl(M_PIld*(long double)a, &sp, &cp);
      t1 = M_PIld*(long double)b;
      shp = tools_sinhl(t1);
      chp = tools_coshl(t1);
      t5 = sp*chp;
      t8 = a*a;
      t9 = b*b;
      t11 = ONEOPIld/(t8 + t9);
      t12 = a*t11;
      t16 = cp*shp;
      t18 = b*t11;
      agn_pushcomplex(L, t5*t12 + t16*t18, t16*t12 - t5*t18);
    }
  } else {
    luaL_nonumorcmplx(L, 1, "math.sincpi");
  }
  return 1;
}


/* Returns tan(Pi*x)/(Pi*x) with number or complex number x, preserving accuracy especially with larger x and with x around the origin.
   With x = 0, returns 1. The type of return depends on the type of input. 4.5.3 */
static int math_tancpi (lua_State *L) {  /* 4.5.4 */
  if (agn_isnumber(L, 1)) {
    lua_Number x = agn_tonumber(L, 1);
    if (fabs(x) < 0.025) {  /* avoid catastrophic cancellation, max. absolute error is -.1240e-20 */
      /* convert(series(tan(Pi*x)/(Pi*x), x=0, 16), polynom); */
      lua_Number z = x*x;
      lua_pushnumber(L,
        1.0 + (
        3.289868133696452872944829 + (
        12.98787880453365829819204 + (
        51.88449616120690612544037 + (
        207.5093202809084968049281 + (
        830.0247016959867563615605 + (
        3320.093240290012164600171 +
        13280.37049096654835984896*z)*z)*z)*z)*z)*z)*z
      );
    } else {
      lua_Number sp, cp, tp;
      tools_sincospi(x, &sp, &cp, &tp, 1);
      lua_pushnumber(L, tp/(M_PI*x));
    }
  } else if (lua_iscomplex(L, 1)) {
    lua_Number a, b;
    long double sp, t3, t4, t6, t7, t10, t15, t16, t19, t21;
    agn_getcmplxparts(L, 1, &a, &b);
    if (a == 0.0 && b == 0.0) {
      agn_pushcomplex(L, 1, 0);
    } else {  /* fixed 4.5.5 */
      sun_sincosl(M_PIld*(long double)a, &sp, &t3);
      t4 = sp*t3;
      t6 = M_PIld*b;
      t7 = tools_sinhl(t6);
      t10 = 1/tools_pythal(t3, t7);
      t15 = 1/tools_pythal(a, b);
      t16 = t10*a*t15;
      t19 = t7*tools_coshl(t6);
      t21 = t10*b*t15;
      agn_pushcomplex(L, (t4*t16 + t19*t21)*ONEOPIld, (t19*t16 - t4*t21)*ONEOPIld);
    }
  } else {
    luaL_nonumorcmplx(L, 1, "math.tancpi");
  }
  return 1;
}


/* With only x given, returns cos(Pi*x)/(Pi*x) with number or complex number x, preserving accuracy especially with larger x.
   With x = 0, returns `undefined`.

   With any second option, returns the first derivative of `math.sincpi` at real or complex x:

            d  ( sin(Pi x) )   cos(Pi x) Pi x - sin(Pi x)
           --- ( --------- ) = --------------------------
            dx (   Pi x    )           Pi x^2

   With x = 0, returns 0.

   With both forms, the type of return depends on the type of input. 4.5.3 */
static int math_coscpi (lua_State *L) {  /* 4.5.3 */
  if (lua_gettop(L) == 1) {
    /* cos(Pi*x)/(Pi*x) in the real and complex domain */
    if (agn_isnumber(L, 1)) {
      lua_Number x, sp, cp, tp;
      x = agn_tonumber(L, 1);
      tools_sincospi(x, &sp, &cp, &tp, 0);
      lua_pushnumber(L, x == 0.0 ? AGN_NAN : cp/(M_PI*x));
    } else if (lua_iscomplex(L, 1)) {
      /* assume(a, real, b, real); z := a + I*b; factor(cos(Pi*z)/(Pi*z)); */
      lua_Number a, b;
      long double si, co, t1, t3, t4, t5, t11, t12, t16, t18;
      agn_getcmplxparts(L, 1, &a, &b);
      if (a == 0.0 && b == 0.0) {
        agn_pushcomplexnan(L);
      } else {
        sun_sincosl(M_PIld*(long double)a, &si, &co);
        t1 = M_PIld*(long double)b;
        t3 = tools_sinhl(t1);
        t4 = tools_coshl(t1);
        t5 = co*t4;
        t11 = ONEOPIld/tools_pythal(a, b);
        t12 = a*t11;
        t16 = si*t3;
        t18 = b*t11;
        agn_pushcomplex(L, t5*t12 - t16*t18, -t16*t12 - t5*t18);
      }
    } else {
      luaL_nonumorcmplx(L, 1, "math.coscpi");
    }
  } else {
    /* > factor(diff(sin(Pi*x)/(Pi*x), x));
                        cos(Pi x) Pi x - sin(Pi x)
                        --------------------------
                                  Pi x^2          */
    if (agn_isnumber(L, 1)) {
      lua_Number x = agn_tonumber(L, 1);
      if (fabs(x) < 0.025) {  /* avoid catastrophic cancellation, max. absolute error is -.424945e-20 */
        lua_Number z = x*x;
        lua_pushnumber(L,
          (-3.289868133696452872944829 + (
            3.246969701133414574548009 + (
           -1.144510944732505282178832 + (
            0.2091827825412384040372259 -
            0.2346081035455823637508933e-1*z)*z)*z)*z)*x
        );
      } else {
        lua_Number sp, cp, tp;
        tools_sincospi(x, &sp, &cp, &tp, 0);
        lua_pushnumber(L, sp == 0 ? 0 : (cp*M_PI*x - sp)/(M_PI*(x*x)));
      }
    } else if (lua_iscomplex(L, 1)) {
      /* Maple's C() unfortunately does not return a true real solution */
#ifndef PROPCMPLX
      agn_Complex t1, t2, t3, t6, t9, t13, t14, t15, t16;
      /* t1 = a+(sqrt(-1.0))*b; */
      t1 = agn_tocomplex(L, 1);
      if (t1 == 0.0 + I*0.0) {
        t16 = AGN_NAN + 0.0*I;
      } else {
        /* t2 = 0.3141592653589793E1*t1; */
        t2 = (M_PI + 0.0*I)*t1;
        /* t3 = cos(t2); */
        t3 = tools_ccos(t2);
        /* t6 = sin(t2); */
        t6 = tools_csin(t2);
        /* t9 = t1*t1; */
        t9 = t1*t1;
        /* t12 = t3/t1-t6/0.3141592653589793E1/t9; */
        t13 = t3/t1;
        t14 = (ONEOPI + 0.0*I)*t6;
        t15 = t14/t9;
        t16 = t13 - t15;
      }
      agn_createcomplex(L, t16);
#else
      lua_Number t1[2], t2[2], t3[2], t6[2], t9[2], t13[2], t14[2], t15[2], t16[2];
      agn_getcmplxparts(L, 1, &t1[0], &t1[1]);
      if (t1[0] == 0.0 && t1[1] == 0.0) {
        t16[0] = AGN_NAN; t16[1] = AGN_NAN;
      } else {
        agnc_mul(t2, M_PI, 0.0, t1[0], t1[1]);
        agnc_cos(t3, t2[0], t2[1]);
        agnc_sin(t6, t2[0], t2[1]);
        agnc_mul(t9, t1[0], t1[1], t1[0], t1[1]);
        agnc_div(t13, t3[0], t3[1], t1[0], t1[1]);
        agnc_mul(t14, ONEOPI, 0.0, t6[0], t6[1]);
        agnc_div(t15, t14[0], t14[1], t9[0], t9[1]);
        agnc_sub(t16, t13[0], t13[1], t15[0], t15[1]);
      }
      agn_createcomplex(L, t16[0], t16[1]);  /* 6.7.0 DJGPP fix */
#endif

    } else {
      luaL_nonumorcmplx(L, 1, "math.coscpi");
    }
  }
  return 1;
}


/* Conducts an argument reduction of x into the range |y| < Pi/2 and returns y = x - N*Pi/2. If any option is given, then the function also
   returns N, or actually the last three digits of N. The number of operations conducted are independent of the exponent of the input.

   The function is 60 percent faster than `math.wrap`, but returns a result different from x if its argument |x| is already in the range Pi/4 .. Pi/2.

   This function is just a port to the underlying C function rem_pio2 which is used to compute sines, cosines and tangents. 2.11.0 */
static int math_rempio2 (lua_State *L) {
  lua_Number x, y[2];
  int32_t n, isopt;
  isopt = lua_gettop(L) != 1;
  x = agn_checknumber(L, 1);
  n = sun_rem_pio2(x, y);
  lua_pushnumber(L, y[0] + y[1]);
  if (isopt) lua_pushnumber(L, n);
  return 1 + isopt;
}


/* Returns a function that gets a number with each call, adds it to an internal accumulator using Kahan-Ozawa (KO) summation, and
   returns the accumulated sum. The function can be used if high precision numeric addition is needed. The initial value of the
   accumulator is 0. The function automatically takes care of storing and processing the KO correction value - so the user does
   not have to worry about this. See also: math.koadd. */

static int accu_kahan (lua_State *L) {  /* 2.12.0 RC 3, based on math_koadd */
  volatile lua_Number q[2], s[2], v[2], x[2], t[2];
  int type, currtype;
  type = agn_tointeger(L, lua_upvalueindex(5));
  currtype = lua_type(L, 1);
  switch (currtype) {
    case LUA_TNUMBER: {
      x[0] = agn_tonumber(L, 1);
      x[1] = 0;
      break;
    }
    case LUA_TCOMPLEX: {
      x[0] = agn_complexreal(L, 1);
      x[1] = agn_compleximag(L, 1);
      if (type == LUA_TNUMBER) {
        type = LUA_TCOMPLEX;
        lua_pushinteger(L, type);
        lua_replace(L, lua_upvalueindex(5));  /* update type accumulator */
      }
      break;
    }
    default: {
      x[0] = x[1] = 0.0;
    }
  }
  s[0] = agn_tonumber(L, lua_upvalueindex(1));  /* accumulator, real part */
  q[0] = agn_tonumber(L, lua_upvalueindex(3));  /* correction value */
  v[0] = x[0] - q[0];
  t[0] = s[0] + v[0];
  q[0] = (t[0] - s[0]) - v[0];
  s[0] = t[0];
  lua_pushnumber(L, s[0]);
  lua_replace(L, lua_upvalueindex(1));  /* update accumulator, real part */
  lua_pushnumber(L, q[0]);
  lua_replace(L, lua_upvalueindex(3));  /* update q[0], real part */
  if (type == LUA_TCOMPLEX) {
    s[1] = agn_tonumber(L, lua_upvalueindex(2));  /* accumulator, imaginary part */
    q[1] = agn_tonumber(L, lua_upvalueindex(4));  /* correction value */
    v[1] = x[1] - q[1];
    t[1] = s[1] + v[1];
    q[1] = (t[1] - s[1]) - v[1];
    s[1] = t[1];
    lua_pushnumber(L, s[1]);
    lua_replace(L, lua_upvalueindex(2));  /* update accumulator, imaginary part */
    lua_pushnumber(L, q[1]);
    lua_replace(L, lua_upvalueindex(4));  /* update q[1], imaginary part */
    agn_pushcomplex(L,
      (fabs(s[0]) < AGN_HEPSILON) ? 0 : s[0],
      (fabs(s[1]) < AGN_HEPSILON) ? 0 : s[1]);
  } else {
    lua_pushnumber(L, (fabs(s[0]) < AGN_HEPSILON) ? 0 : s[0]);
  }
  return 1;
}

static int accu_kahanozawa (lua_State *L) {  /* 2.12.0 RC 3, based on math_koadd */
  volatile lua_Number q[2], s[2], sold[2], u[2], v[2], w[2], x[2], t[2];
  int type, currtype;
  type = agn_tointeger(L, lua_upvalueindex(5));
  currtype = lua_type(L, 1);
  switch (currtype) {
    case LUA_TNUMBER: {
      x[0] = agn_tonumber(L, 1);
      x[1] = 0;
      break;
    }
    case LUA_TCOMPLEX: {
      x[0] = agn_complexreal(L, 1);
      x[1] = agn_compleximag(L, 1);
      if (type == LUA_TNUMBER) {
        type = LUA_TCOMPLEX;
        lua_pushinteger(L, type);
        lua_replace(L, lua_upvalueindex(5));  /* update type accumulator */
      }
      break;
    }
    default: {
      x[0] = x[1] = 0.0;
    }
  }
  s[0] = agn_tonumber(L, lua_upvalueindex(1));  /* accumulator, real part */
  q[0] = agn_tonumber(L, lua_upvalueindex(3));  /* correction value, real part */
  v[0] = x[0] - q[0];
  sold[0] = s[0];
  s[0] += v[0];
  if (fabs(x[0]) < fabs(q[0])) {
    t[0] = x[0];
    x[0] = -q[0];
    q[0] = t[0];
  }
  u[0] = (v[0] - x[0]) + q[0];
  if (fabs(sold[0]) < fabs(v[0])) {
    t[0] = sold[0];
    sold[0] = v[0];
    v[0] = t[0];
  }
  w[0] = (s[0] - sold[0]) - v[0];
  q[0] = u[0] + w[0];
  lua_pushnumber(L, s[0]);
  lua_replace(L, lua_upvalueindex(1));  /* update accumulator, real part */
  lua_pushnumber(L, q[0]);
  lua_replace(L, lua_upvalueindex(3));  /* update q[0] */
  if (type == LUA_TCOMPLEX) {
    s[1] = agn_tonumber(L, lua_upvalueindex(2));  /* accumulator, imaginary part */
    q[1] = agn_tonumber(L, lua_upvalueindex(4));  /* correction value, imaginary part */
    v[1] = x[1] - q[1];
    sold[1] = s[1];
    s[1] += v[1];
    if (fabs(x[1]) < fabs(q[1])) {
      t[1] = x[1];
      x[1] = -q[1];
      q[1] = t[1];
    }
    u[1] = (v[1] - x[1]) + q[1];
    if (fabs(sold[1]) < fabs(v[1])) {
      t[1] = sold[1];
      sold[1] = v[1];
      v[1] = t[1];
    }
    w[1] = (s[1] - sold[1]) - v[1];
    q[1] = u[1] + w[1];
    lua_pushnumber(L, s[1]);
    lua_replace(L, lua_upvalueindex(2));  /* update accumulator, imaginary part */
    lua_pushnumber(L, q[1]);
    lua_replace(L, lua_upvalueindex(4));  /* update q[1] */
    agn_pushcomplex(L,
      (fabs(s[0]) < AGN_HEPSILON) ? 0 : s[0],
      (fabs(s[1]) < AGN_HEPSILON) ? 0 : s[1]);
  } else {
    lua_pushnumber(L, (fabs(s[0]) < AGN_HEPSILON) ? 0 : s[0]);
  }
  return 1;
}

static int accu_kahanbabuska (lua_State *L) {  /* 2.12.0 RC 3, based on stats_KahanBabuskaMean */
  volatile lua_Number x[2], s[2], s0[2], c[2], cc[2], cs[2], ccs[2], t[2];
  int type, currtype;
  type = agn_tointeger(L, lua_upvalueindex(5));
  currtype = lua_type(L, 1);
  switch (currtype) {
    case LUA_TNUMBER: {
      x[0] = agn_tonumber(L, 1);
      x[1] = 0;
      break;
    }
    case LUA_TCOMPLEX: {
      x[0] = agn_complexreal(L, 1);
      x[1] = agn_compleximag(L, 1);
      if (type == LUA_TNUMBER) {
        type = LUA_TCOMPLEX;
        lua_pushinteger(L, type);
        lua_replace(L, lua_upvalueindex(5));  /* update type accumulator */
      }
      break;
    }
    default: {
      x[0] = x[1] = 0.0;
    }
  }
  s[0]   = agn_tonumber(L, lua_upvalueindex(1));  /* accumulator, real part */
  cs[0]  = agn_tonumber(L, lua_upvalueindex(3));  /* first correction value, real part */
  ccs[0] = agn_tonumber(L, lua_upvalueindex(6));  /* second correction value, real part */
  t[0] = s[0] + x[0];
  c[0] = (fabs(s[0]) >= fabs(x[0])) ? (s[0] - t[0]) + x[0] : (x[0] - t[0]) + s[0];
  s[0] = t[0];
  t[0] = cs[0] + c[0];
  cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0];
  cs[0] = t[0];
  ccs[0] += cc[0];
  lua_pushnumber(L, s[0]);
  lua_replace(L, lua_upvalueindex(1));  /* update accumulator, real part */
  lua_pushnumber(L, cs[0]);
  lua_replace(L, lua_upvalueindex(3));  /* update cs, real part */
  lua_pushnumber(L, ccs[0]);
  lua_replace(L, lua_upvalueindex(6));  /* update ccs, real part */
  /* the correction values are added to the final sum, not the intermediate sum. */
  s0[0] = s[0] + cs[0] + ccs[0];
  if (type == LUA_TCOMPLEX) {
    s[1] = agn_tonumber(L, lua_upvalueindex(2));    /* accumulator, imaginary part */
    cs[1]  = agn_tonumber(L, lua_upvalueindex(4));  /* first correction value, imaginary part */
    ccs[1] = agn_tonumber(L, lua_upvalueindex(7));  /* second correction value, imaginary part */
    t[1] = s[1] + x[1];
    c[1] = (fabs(s[1]) >= fabs(x[1])) ? (s[1] - t[1]) + x[1] : (x[1] - t[1]) + s[1];
    s[1] = t[1];
    t[1] = cs[1] + c[1];
    cc[1] = (fabs(cs[1]) >= fabs(c[1])) ? (cs[1] - t[1]) + c[1] : (c[1] - t[1]) + cs[1];
    cs[1] = t[1];
    ccs[1] += cc[1];
    lua_pushnumber(L, s[1]);
    lua_replace(L, lua_upvalueindex(2));  /* update accumulator, imaginary part */
    lua_pushnumber(L, cs[1]);
    lua_replace(L, lua_upvalueindex(4));  /* update cs, imaginary part */
    lua_pushnumber(L, ccs[1]);
    lua_replace(L, lua_upvalueindex(7));  /* update ccs, imaginary part */
    s0[1] = s[1] + cs[1] + ccs[1];
    agn_pushcomplex(L,
      (fabs(s0[0]) < AGN_HEPSILON) ? 0 : s0[0],
      (fabs(s0[1]) < AGN_HEPSILON) ? 0 : s0[1]);
  } else {
    lua_pushnumber(L, (fabs(s0[0]) < AGN_HEPSILON) ? 0 : s0[0]);
  }
  return 1;
}

/* See: https://en.wikipedia.org/wiki/Kahan_summation_algorithm */
static int accu_neumaier (lua_State *L) {  /* 2.12.0 RC 3, based on stats_KahanBabuskaMean */
  volatile lua_Number s[2], c[2], t[2], x[2], s0[2];
  int type, currtype;
  type = agn_tointeger(L, lua_upvalueindex(5));
  currtype = lua_type(L, 1);
  switch (currtype) {
    case LUA_TNUMBER: {
      x[0] = agn_tonumber(L, 1);
      x[1] = 0;
      break;
    }
    case LUA_TCOMPLEX: {
      x[0] = agn_complexreal(L, 1);
      x[1] = agn_compleximag(L, 1);
      if (type == LUA_TNUMBER) {
        type = LUA_TCOMPLEX;
        lua_pushinteger(L, type);
        lua_replace(L, lua_upvalueindex(5));  /* update type accumulator */
      }
      break;
    }
    default: {
      x[0] = x[1] = 0.0;
    }
  }
  s[0] = agn_tonumber(L, lua_upvalueindex(1));  /* accumulator, real part */
  c[0] = agn_tonumber(L, lua_upvalueindex(3));  /* first correction value, real part */
  t[0] = s[0] + x[0];
  c[0] += (fabs(s[0]) >= fabs(x[0])) ? (s[0] - t[0]) + x[0] : (x[0] - t[0]) + s[0];
  s[0] = t[0];
  lua_pushnumber(L, s[0]);
  lua_replace(L, lua_upvalueindex(1));  /* update accumulator, real part */
  lua_pushnumber(L, c[0]);
  lua_replace(L, lua_upvalueindex(3));  /* update c, real part */
  /* the correction values are added to the final sum, not the intermediate sum. */
  s0[0] = s[0] + c[0];
  if (type == LUA_TCOMPLEX) {
    s[1] = agn_tonumber(L, lua_upvalueindex(2));  /* accumulator, imaginary part */
    c[1] = agn_tonumber(L, lua_upvalueindex(4));  /* correction value, imaginary part */
    t[1] = s[1] + x[1];
    c[1] += (fabs(s[1]) >= fabs(x[1])) ? (s[1] - t[1]) + x[1] : (x[1] - t[1]) + s[1];
    s[1] = t[1];
    lua_pushnumber(L, s[1]);
    lua_replace(L, lua_upvalueindex(2));  /* update accumulator, imaginary part */
    lua_pushnumber(L, c[1]);
    lua_replace(L, lua_upvalueindex(4));  /* update c, imaginary part */
    s0[1] = s[1] + c[1];
    agn_pushcomplex(L,
      (fabs(s0[0]) < AGN_HEPSILON) ? 0 : s0[0],
      (fabs(s0[1]) < AGN_HEPSILON) ? 0 : s0[1]);
  } else {
    lua_pushnumber(L, (fabs(s0[0]) < AGN_HEPSILON) ? 0 : s0[0]);
  }
  return 1;
}

/* See: https://github.com/JuliaLang/julia/blob/d386e40c17d43b79fc89d3e579fc04547241787c/base/reduce.jl#L366-L371 */
static int accu_kbn (lua_State *L) {  /* 2.12.0 RC 3, based on accu_neumaier; patched 2.30.5 */
  volatile lua_Number s[2], c[2], t[2], x[2], s0[2];
  int type, currtype;
  type = agn_tointeger(L, lua_upvalueindex(5));
  currtype = lua_type(L, 1);
  switch (currtype) {
    case LUA_TNUMBER: {
      x[0] = agn_tonumber(L, 1);
      x[1] = 0;
      break;
    }
    case LUA_TCOMPLEX: {
      x[0] = agn_complexreal(L, 1);
      x[1] = agn_compleximag(L, 1);
      if (type == LUA_TNUMBER) {
        type = LUA_TCOMPLEX;
        lua_pushinteger(L, type);
        lua_replace(L, lua_upvalueindex(5));  /* update type accumulator */
      }
      break;
    }
    default: {
      x[0] = x[1] = 0.0;
    }
  }
  s[0] = agn_tonumber(L, lua_upvalueindex(1));  /* accumulator, real part */
  c[0] = agn_tonumber(L, lua_upvalueindex(3));  /* first correction value, real part */
  t[0] = s[0] + x[0];
  c[0] -= (fabs(s[0]) >= fabs(x[0])) ? (s[0] - t[0]) + x[0] : (x[0] - t[0]) + s[0];
  s[0] = t[0];
  lua_pushnumber(L, s[0]);
  lua_replace(L, lua_upvalueindex(1));  /* update accumulator, real part */
  lua_pushnumber(L, c[0]);
  lua_replace(L, lua_upvalueindex(3));  /* update c, real part */
  /* the correction values are added to the final sum, not the intermediate sum. */
  s0[0] = s[0] - c[0];
  if (type == LUA_TCOMPLEX) {
    s[1] = agn_tonumber(L, lua_upvalueindex(2));  /* accumulator, imaginary part */
    c[1] = agn_tonumber(L, lua_upvalueindex(4));  /* correction value, imaginary part */
    t[1] = s[1] + x[1];
    c[1] -= (fabs(s[1]) >= fabs(x[1])) ? (s[1] - t[1]) + x[1] : (x[1] - t[1]) + s[1];
    s[1] = t[1];
    lua_pushnumber(L, s[1]);
    lua_replace(L, lua_upvalueindex(2));  /* update accumulator, imaginary part */
    lua_pushnumber(L, c[1]);
    lua_replace(L, lua_upvalueindex(4));  /* update c, imaginary part */
    s0[1] = s[1] - c[1];
    agn_pushcomplex(L,
      (fabs(s0[0]) < AGN_HEPSILON) ? 0 : s0[0],
      (fabs(s0[1]) < AGN_HEPSILON) ? 0 : s0[1]);
  } else {
    lua_pushnumber(L, (fabs(s0[0]) < AGN_HEPSILON) ? 0 : s0[0]);
  }
  return 1;
}

static int accu_raw (lua_State *L) {  /* no correction; 2.30.5, based on accu_neumaier */
  volatile lua_Number s[2], x[2];
  int type, currtype;
  type = agn_tointeger(L, lua_upvalueindex(5));
  currtype = lua_type(L, 1);
  switch (currtype) {
    case LUA_TNUMBER: {
      x[0] = agn_tonumber(L, 1);
      x[1] = 0;
      break;
    }
    case LUA_TCOMPLEX: {
      x[0] = agn_complexreal(L, 1);
      x[1] = agn_compleximag(L, 1);
      if (type == LUA_TNUMBER) {
        type = LUA_TCOMPLEX;
        lua_pushinteger(L, type);
        lua_replace(L, lua_upvalueindex(5));  /* update type accumulator */
      }
      break;
    }
    default: {
      x[0] = x[1] = 0.0;
    }
  }
  s[0] = agn_tonumber(L, lua_upvalueindex(1)) + x[0];   /* accumulator, real part */
  lua_pushnumber(L, s[0]);
  lua_replace(L, lua_upvalueindex(1));  /* update accumulator */
  if (type == LUA_TCOMPLEX) {
    s[1] = agn_tonumber(L, lua_upvalueindex(2)) + x[1];  /* accumulator, imaginary part */
    lua_pushnumber(L, s[1]);
    lua_replace(L, lua_upvalueindex(2));  /* update accumulator */
    agn_pushcomplex(L, s[0], s[1]);
  } else {
    lua_pushnumber(L, s[0]);
  }
  return 1;
}

static int math_accu (lua_State *L) {  /* 2.12.0 RC 3, around 25 percent faster than an Agena implementation */
  int nargs, type;
  lua_Number startre, startim;
  const char *method;
  nargs = lua_gettop(L);
  if (lua_isnoneornil(L, 1)) {
    startre = startim = 0.0; type = LUA_TNUMBER;
  } else if (agn_isnumber(L, 1)) {
    startre = agn_tonumber(L, 1); startim = 0.0; type = LUA_TNUMBER;
  } else if (lua_iscomplex(L, 1)) {
    startre = agn_complexreal(L, 1); startim = agn_compleximag(L, 1); type = LUA_TCOMPLEX;
  } else {
    startre = startim = 0.0; type = LUA_TNUMBER;
    if (!agn_isstring(L, 1))
      luaL_error(L, "Error in " LUA_QS ": expected a (complex) number as initialiser, got %s.", "math.accu", luaL_typename(L, 1));
  }
  method = (agn_isstring(L, nargs)) ? agn_tostring(L, nargs) : "neumaier";
  luaL_checkstack(L, 5 + 2*tools_streq(method, "babuska"), "not enough stack space");  /* 3.18.4 fix */
  lua_pushnumber(L, startre);  /* initial value, real part */
  lua_pushnumber(L, startim);  /* initial value, imaginary part */
  lua_pushnumber(L, 0.0);      /* correction value cs, real part */
  lua_pushnumber(L, 0.0);      /* correction value cs, imaginary part */
  lua_pushinteger(L, type);
  if (tools_streq(method, "kahan")) {
    lua_pushcclosure(L, &accu_kahan, 5);
  } else if (tools_streq(method, "ozawa")) {
    lua_pushcclosure(L, &accu_kahanozawa, 5);
  } else if (tools_streq(method, "neumaier")) {
    lua_pushcclosure(L, &accu_neumaier, 5);
  } else if (tools_streq(method, "babuska")) {
    lua_pushnumber(L, 0);  /* ccs correction value, real part */
    lua_pushnumber(L, 0);  /* ccs correction value, imaginary part */
    lua_pushcclosure(L, &accu_kahanbabuska, 7);
  } else if (tools_streq(method, "kbn")) {
    lua_pushcclosure(L, &accu_kbn, 5);
  } else if (tools_streq(method, "raw")) {  /* 2.30.5, raw mode with no correction */
    lua_pushcclosure(L, &accu_raw, 5);
  } else
    luaL_error(L, "Error in " LUA_QS ": unknown method " LUA_QS ".", "math.accu", method);
  return 1;
}


/* Computes the unit of least precision (ULP), the spacing between floating-point numbers, as a measure
   of accuracy in numeric calculations. Equals, seehttp://web.mit.edu/hyperbook/Patrikalakis-Maekawa-Cho/node48.html:
   double ulp;
   int exp;
   frexp(x, &exp);
   ulp = ldexp(0.5, exp - 52); */
static int math_ulp (lua_State *L) {  /* 2.12.3 */
  int option;
  int64_t dist;
  double_cast p, q;
  lua_Number x, eps, r;
  dist = 0;
  x = agn_checknumber(L, 1);
  eps = agnL_optpositive(L, 2, 0);
  r = sun_nextafter(x, HUGE_VAL) - x;
  option = lua_gettop(L) > 1;
  if (option) {
    /* Adapted from: https://autohotkey.com/board/topic/24203-floating-point-comparison/
       Mixed with code from tools_sinfast, 2.18.2 */
    p.f = x;
    q.f = x + eps;
    p.i &= 0x7FFFFFFFFFFFFFFFULL;  /* absolute value, only ULL hex will work with uint64_t */
    q.i &= 0x7FFFFFFFFFFFFFFFULL;  /* ditto */
    dist = q.i - p.i;  /* ULP distance = ULPs between values becomes; diff = p.i - q.i */
  }
  lua_pushnumber(L, r);
  if (option) lua_pushnumber(L, dist);
  return 1 + option;
}


static int math_beta (lua_State *L) {  /* 2.17.4, DO NOT DELETE as it is used by `beta`; rewritten and tuned 7 times 2.41.0 */
  lua_pushnumber(L, tools_beta(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


/* For number x, gives x if x > 0 and 0 otherwise. 25% faster than an Agena implementation. Idea taken from:
   https://reference.wolfram.com/language/ref/Ramp.html */
static int math_ramp (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  lua_pushnumber(L, (x > 0) * x);
  return 1;
}


/* For number x, gives 0 for x < 0 and 1 otherwise. See also: heaviside. Idea taken from:
   https://reference.wolfram.com/language/ref/UnitStep.html */
static int math_unitstep (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  lua_pushnumber(L, x >= 0);
  return 1;
}


/* chi, the piecewise indicator function, is defined as follows:
   - chi(x, a, b) simplifies to 1 if a < x < b; to 0 if x < a < b or if a < b < x; and to -chi(b, x, a) if b < a.
     Otherwise chi(x, a, b) simplifies to sign(x-a)/2 - sign(x-b)/2;
   - chi(x) simplifies to chi(x, 0, 1);
   - chi(a, x) simplifies to chi(x, a, 1). 3.5.3 */
static int math_chi (lua_State *L) {
  lua_Number x, a, b, sign;
  x = agn_checknumber(L, 1);
  a = agnL_optnumber(L, 2, 0);
  b = agnL_optnumber(L, 3, 1);
  sign = 1.0;
labelchi:
  if (a < x && x < b)
    lua_pushnumber(L, sign);
  else if ((x < a && a < b) || (a < b && b < x))
    lua_pushnumber(L, 0);
  else if (b < a) {
    lua_Number t;
    SWAP(a, b, t);
    sign = -1.0;
    goto labelchi;
  } else {
    lua_pushnumber(L, sign*(0.5*tools_sign(x - a) - 0.5*tools_sign(x - b)));
  }
  return 1;
}


/* Multiplies, not copies, its first argument with the sign of its second, and returns x * signum(y). See also: math.copysign. 2.18.1. */
static int math_mulsign (lua_State *L) {
  lua_pushnumber(L, tools_mulsign(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


/* Computes the relative error |b - a|/|a|, handling case of `undefined` and `infinity`. */
static int math_relerror (lua_State *L) {  /* 2.21.6 */
  lua_pushnumber(L, tools_relerr(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


/* Returns the next multiple towards +`infinity` of an integer to the given integer base. If base is negative, the result is the next
   multiple towards -infinity; 2.22.1, changed 2.22.2, optimised 5.4.1
   Taken from: https://stackoverflow.com/questions/2403631/how-do-i-find-the-next-multiple-of-10-of-any-integer */
static int math_nextmultiple (lua_State *L) {
  int isoption = lua_gettop(L) > 2;
  int x = agn_checkinteger(L, 1);
  int base = agn_checkinteger(L, 2);
  lua_pushnumber(L, tools_nextmultiple(x, base));
  if (isoption) {  /* UNDOC: if n is already a multiple, return n. 5.4.1 extended to the negative domain */
    int isneg, r;
    isneg = (x < 0);
    if (isneg) x = -x;
    r = tools_adjustmultiple(x, base);
    lua_pushnumber(L, isneg ? -r + isneg*(x % base != 0)*base : r);
  }
  return 1 + isoption;
}


/* Returns the previous multiple of an integer n to the given integer base. If base is negative, it is considered positive.
   The result is always less than n. 5.4.1 */
static int math_prevmultiple (lua_State *L) {
  int isoption = lua_gettop(L) > 2;
  int n = agn_checkinteger(L, 1);
  int base = agn_checkinteger(L, 2);
  lua_pushnumber(L, tools_prevmultiple(n, base, 0));
  if (isoption) {  /* UNDOC: if n is already a multiple, return n. */
    lua_pushnumber(L, tools_prevmultiple(n, base, 1));
  }
  return 1 + isoption;
}


/* By default returns the smallest power of 2 greater than x. If the third argument is true, then the smallest power of base
   greater than _or_ equal to x is returned. 2.26.3 */
static int math_nextpower (lua_State *L) {
  lua_pushnumber(L, tools_nextpower(agn_checknumber(L, 1), agn_checkposint(L, 2), agnL_optboolean(L, 3, 0)));
  return 1;
}


/* Returns the smallest power of base greater than non-negative number x. With x = 0, returns 1. 5.4.1 */
static int math_nextpow2 (lua_State *L) {
  lua_pushnumber(L, tools_nextpower(agn_checknonnegative(L, 1), 2, 0));
  return 1;
}


/* By default returns the smallest power of base less than x. If the third argument is true, then the smallest power of base
   less than _or_ equal to x is returned. 3.19.3 */
static int math_prevpower (lua_State *L) {
  lua_pushnumber(L, tools_prevpower(agn_checknumber(L, 1), agn_checkposint(L, 2), agnL_optboolean(L, 3, 0)));
  return 1;
}


/* Returns the smallest power of 2 less than non-negative number x. Returns 0 if x is zero. 5.4.1 */
static int math_prevpow2 (lua_State *L) {
  lua_pushnumber(L, tools_prevpow2(agn_checknonnegative(L, 1)));
  return 1;
}


/* If x is close to y, with y preferably positive, defined by `eps` which is `Eps` by default, returns sign(x)*y. If optional
  `onesided` is -1, then the function adjusts x only if x < -y or x > +y for positive y. If `onesided` is +1, the function adjusts
  x only if x > -y or x < +y for positive y. 2.32.5, UNDOC */
static int math_adjust (lua_State *L) {
  int onesided;
  lua_Number x, y, eps;
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  onesided = agnL_optinteger(L, 3, 0);  /* -1, 0 or 1 */
  eps = agnL_optpositive(L, 4, agn_getepsilon(L));
  tools_adjust(x, y, eps, onesided);
  lua_pushnumber(L, x);
  return 1;
}


static int math_add (lua_State *L) {  /* 2.32.0 */
  lua_pushnumber(L, luai_numadd(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int math_subtract (lua_State *L) {  /* 2.32.0 */
  lua_pushnumber(L, luai_numsub(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int math_multiply (lua_State *L) {  /* 2.32.0 */
  lua_pushnumber(L, luai_nummul(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int math_divide (lua_State *L) {  /* 2.32.0 */
  lua_pushnumber(L, luai_numdiv(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int math_intdivide (lua_State *L) {  /* 2.32.0 */
  lua_pushnumber(L, luai_numintdiv(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int math_modulo (lua_State *L) {  /* 2.32.0 */
  lua_pushnumber(L, luai_nummod(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int math_remainder (lua_State *L) {  /* 2.38.1 UNDOC ! */
  if (agn_isinteger(L, 1) && agn_isinteger(L, 1)) {
    /* see: https://shadyf.com/blog/notes/2016-07-16-modulo-for-negative-numbers/ */
    lua_Integer k, n;
    k = agn_tointeger(L, 1);
    n = agn_tointeger(L, 2);
    k = nt_powermodint(k, 1, n);
    if (k < 0) k += n;
    lua_pushnumber(L, k);
    return 1;
  }
  lua_pushnumber(L, sun_remainder(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


/*
** {===========================================================================
** Pseudo-Random Number Generator based on 'xoshiro256**'. Taken from Lua 5.4.4
** ============================================================================
*/

/* number of binary digits in the mantissa of a float */
#define FIGS  l_floatatt(MANT_DIG)

#if FIGS > 64
/* there are only 64 random bits; use them all */
#undef FIGS
#define FIGS  64
#endif

/*
** LUA_RAND32 forces the use of 32-bit integers in the implementation
** of the PRN generator (mainly for testing).
*/
#if !defined(LUA_RAND32) && !defined(Rand64)

/* try to find an integer type with at least 64 bits */

#if (ULONG_MAX >> 31 >> 31) >= 3

/* 'long' has at least 64 bits */
#define Rand64    unsigned long

#elif !defined(LUA_USE_C89) && defined(LLONG_MAX)

/* there is a 'long long' type (which must have at least 64 bits) */
#define Rand64    unsigned long long

#elif (LUA_MAXUNSIGNED >> 31 >> 31) >= 3

/* 'lua_Integer' has at least 64 bits */
#define Rand64    lua_Unsigned

#endif
#endif

#if defined(Rand64)  /* { */

/*
** Standard implementation, using 64-bit integers.
** If 'Rand64' has more than 64 bits, the extra bits do not interfere
** with the 64 initial bits, except in a right shift. Moreover, the
** final result has to discard the extra bits.
*/

/* avoid using extra bits when needed; ...ull, otherwise GCC for OS/2 will issue a warning */
#define trim64(x)  ((x) & 0xffffffffffffffffull)


/* rotate left 'x' by 'n' bits */
static Rand64 rotl (Rand64 x, int n) {
  return (x << n) | (trim64(x) >> (64 - n));
}

static Rand64 nextrand (Rand64 *state) {
  Rand64 state0 = state[0];
  Rand64 state1 = state[1];
  Rand64 state2 = state[2] ^ state0;
  Rand64 state3 = state[3] ^ state1;
  Rand64 res = rotl(state1 * 5, 7) * 9;
  state[0] = state0 ^ state3;
  state[1] = state1 ^ state2;
  state[2] = state2 ^ (state1 << 17);
  state[3] = rotl(state3, 45);
  return res;
}

/* must take care to not shift stuff by more than 63 slots */

/*
** Convert bits from a random integer into a float in the
** interval [0,1), getting the higher FIG bits from the
** random unsigned integer and converting that to a float.
*/

/* must throw out the extra (64 - FIGS) bits */
#define shift64_FIG  (64 - FIGS)

/* to scale to [0, 1), multiply by scaleFIG = 2^(-FIGS) */
#define scaleFIG  (l_mathop(0.5) / ((Rand64)1 << (FIGS - 1)))

static lua_Number I2d (Rand64 x) {
  return (lua_Number)(trim64(x) >> shift64_FIG) * scaleFIG;
}

/* convert a 'Rand64' to a 'lua_Unsigned' */
#define I2UInt(x)  ((lua_Unsigned)trim64(x))

/* convert a 'lua_Unsigned' to a 'Rand64' */
#define Int2I(x)  ((Rand64)(x))

#else  /* no 'Rand64'   }{ */

/*
** Use two 32-bit integers to represent a 64-bit quantity.
*/
typedef struct Rand64 {
  lu_int32 h;  /* higher half */
  lu_int32 l;  /* lower half */
} Rand64;

/*
** If 'lu_int32' has more than 32 bits, the extra bits do not interfere
** with the 32 initial bits, except in a right shift and comparisons.
** Moreover, the final result has to discard the extra bits.
*/

/* avoid using extra bits when needed */
#define trim32(x)  ((x) & 0xffffffffu)

/*
** basic operations on 'Rand64' values
*/

/* build a new Rand64 value */
static Rand64 packI (lu_int32 h, lu_int32 l) {
  Rand64 result;
  result.h = h;
  result.l = l;
  return result;
}

/* return i << n */
static Rand64 Ishl (Rand64 i, int n) {
  lua_assert(n > 0 && n < 32);
  return packI((i.h << n) | (trim32(i.l) >> (32 - n)), i.l << n);
}

/* i1 ^= i2 */
static void Ixor (Rand64 *i1, Rand64 i2) {
  i1->h ^= i2.h;
  i1->l ^= i2.l;
}

/* return i1 + i2 */
static Rand64 Iadd (Rand64 i1, Rand64 i2) {
  Rand64 result = packI(i1.h + i2.h, i1.l + i2.l);
  if (trim32(result.l) < trim32(i1.l))  /* carry? */
    result.h++;
  return result;
}

/* return i * 5 */
static Rand64 times5 (Rand64 i) {
  return Iadd(Ishl(i, 2), i);  /* i * 5 == (i << 2) + i */
}

/* return i * 9 */
static Rand64 times9 (Rand64 i) {
  return Iadd(Ishl(i, 3), i);  /* i * 9 == (i << 3) + i */
}

/* return 'i' rotated left 'n' bits */
static Rand64 rotl (Rand64 i, int n) {
  lua_assert(n > 0 && n < 32);
  return packI((i.h << n) | (trim32(i.l) >> (32 - n)),
               (trim32(i.h) >> (32 - n)) | (i.l << n));
}

/* for offsets larger than 32, rotate right by 64 - offset */
static Rand64 rotl1 (Rand64 i, int n) {
  lua_assert(n > 32 && n < 64);
  n = 64 - n;
  return packI((trim32(i.h) >> n) | (i.l << (32 - n)),
               (i.h << (32 - n)) | (trim32(i.l) >> n));
}

/*
** implementation of 'xoshiro256**' algorithm on 'Rand64' values
*/
static Rand64 nextrand (Rand64 *state) {
  Rand64 res = times9(rotl(times5(state[1]), 7));
  Rand64 t = Ishl(state[1], 17);
  Ixor(&state[2], state[0]);
  Ixor(&state[3], state[1]);
  Ixor(&state[1], state[2]);
  Ixor(&state[0], state[3]);
  Ixor(&state[2], t);
  state[3] = rotl1(state[3], 45);
  return res;
}

/*
** Converts a 'Rand64' into a float.
*/

/* an unsigned 1 with proper type */
#define UONE    ((lu_int32)1)

#if FIGS <= 32
/* 2^(-FIGS) */
#define scaleFIG       (l_mathop(0.5) / (UONE << (FIGS - 1)))
/*
** get up to 32 bits from higher half, shifting right to
** throw out the extra bits.
*/
static lua_Number I2d (Rand64 x) {
  lua_Number h = (lua_Number)(trim32(x.h) >> (32 - FIGS));
  return h * scaleFIG;
}
#else  /* 32 < FIGS <= 64 */
/* must take care to not shift stuff by more than 31 slots */
/* 2^(-FIGS) = 1.0 / 2^30 / 2^3 / 2^(FIGS-33) */
#define scaleFIG  \
  ((lua_Number)1.0 / (UONE << 30) / 8.0 / (UONE << (FIGS - 33)))  /* change by a_walz */
/*
** use FIGS - 32 bits from lower half, throwing out the other
** (32 - (FIGS - 32)) = (64 - FIGS) bits
*/
#define shiftLOW  (64 - FIGS)
/*
** higher 32 bits go after those (FIGS - 32) bits: shiftHI = 2^(FIGS - 32)
*/
#define shiftHI    ((lua_Number)(UONE << (FIGS - 33)) * 2.0)  /* change by a_walz */
static lua_Number I2d (Rand64 x) {
  lua_Number h = (lua_Number)trim32(x.h) * shiftHI;
  lua_Number l = (lua_Number)(trim32(x.l) >> shiftLOW);
  return (h + l) * scaleFIG;
}
#endif

/* convert a 'Rand64' to a 'lua_Unsigned' */
static lua_Unsigned I2UInt (Rand64 x) {
  return ((lua_Unsigned)trim32(x.h) << 31 << 1) | (lua_Unsigned)trim32(x.l);
}

/* convert a 'lua_Unsigned' to a 'Rand64' */
static Rand64 Int2I (lua_Unsigned n) {
  return packI((lu_int32)(n >> 31 >> 1), (lu_int32)n);
}
#endif  /* } */

/*
** A state uses four 'Rand64' values.
*/
typedef struct {
  Rand64 s[4];
} RanState;


/*
** Project the random integer 'ran' into the interval [0, n].
** Because 'ran' has 2^B possible values, the projection can only be
** uniform when the size of the interval is a power of 2 (exact
** division). Otherwise, to get a uniform projection into [0, n], we
** first compute 'lim', the smallest Mersenne number not smaller than
** 'n'. We then project 'ran' into the interval [0, lim].  If the result
** is inside [0, n], we are done. Otherwise, we try with another 'ran',
** until we have a result inside the interval.
*/
static lua_Unsigned project (lua_Unsigned ran, lua_Unsigned n,
                             RanState *state) {
  if ((n & (n + 1)) == 0)  /* is 'n + 1' a power of 2? */
    return ran & n;  /* no bias */
  else {
    lua_Unsigned lim = n;
    /* compute the smallest (2^b - 1) not smaller than 'n' */
    lim |= (lim >> 1);
    lim |= (lim >> 2);
    lim |= (lim >> 4);
    lim |= (lim >> 8);
    lim |= (lim >> 16);
#if (LUA_MAXUNSIGNED >> 31) >= 3
    lim |= (lim >> 32);  /* integer type has more than 32 bits */
#endif
    lua_assert((lim & (lim + 1)) == 0  /* 'lim + 1' is a power of 2, */
      && lim >= n  /* not smaller than 'n', */
      && (lim >> 1) < n);  /* and it is the smallest one */
    while ((ran &= lim) > n)  /* project 'ran' into [0..lim] */
      ran = I2UInt(nextrand(state->s));  /* not inside [0..n]? try again */
    return ran;
  }
}


/* math.random ([m [, n]])
   When called without arguments, returns a pseudo-random float with uniform distribution in the range [0,1). When called with two integers m and n,
   `math.random` returns a pseudo-random integer with uniform distribution in the range [m, n]. The call `math.random(n)`, for a positive n, is
   equivalent to `math.random(1, n)`. The call `math.random(0)` produces an integer with all bits (pseudo)random.

   This function uses the xoshiro256** algorithm to produce pseudo-random 64-bit integers, which are the results of calls with argument 0. Other
   results (ranges and floats) are unbiased extracted from these integers.

   Agena initializes its pseudo-random generator with the equivalent of a call to `math.randomseed` with no arguments, so that `math.random` should
   generate different sequences of results each time the program runs.

   The function has been taken from Lua 5.4.4, 2.27.10 */

static int genseed = 1;

static int math_random (lua_State *L) {
  lua_Integer low, up;
  lua_Unsigned p;
  RanState *state;
  Rand64 rv;
  int nargs = lua_gettop(L);
  if (nargs < 3) {
    state = (RanState *)lua_touserdata(L, lua_upvalueindex(1));
    rv = nextrand(state->s);  /* next pseudo-random value */
  } else {
    state = NULL;
#if defined(Rand64)
    rv = 0;
#else
    rv.h = rv.l = 0;
#endif
  }
  switch (nargs) {  /* check number of arguments */
    case 0: {  /* no arguments */
      lua_pushnumber(L, I2d(rv));  /* float between 0 and 1 */
      return 1;
    }
    case 1: {  /* only upper limit */
      low = 1;
      up = luaL_checkinteger(L, 1);
      if (up == 0) {  /* single 0 as argument? */
        lua_pushinteger(L, I2UInt(rv));  /* full random integer */
        return 1;
      }
      break;
    }
    case 2: {  /* lower and upper limits, use xoshiro256** algorithm */
      low = luaL_checkinteger(L, 1);
      up = luaL_checkinteger(L, 2);
      break;
    }
    case 3: {  /* not faster than xoshiro256** algorithm */
      low = luaL_checkinteger(L, 1);
      up = luaL_checkinteger(L, 2);
      lua_pushnumber(L, tools_randint(low, up, genseed));
      genseed = 0;
      return 1;
    }
    default: return luaL_error(L, "wrong number of arguments");
  }
  /* random integer in the interval [low, up] */
  luaL_argcheck(L, low <= up, 1, "interval is empty");
  /* project random integer into the interval [0, up - low] */
  p = project(I2UInt(rv), (lua_Unsigned)up - (lua_Unsigned)low, state);
  lua_pushinteger(L, p + (lua_Unsigned)low);
  return 1;
}


static void setseed (lua_State *L, Rand64 *state,
                     lua_Unsigned n1, lua_Unsigned n2) {
  int i;
  state[0] = Int2I(n1);
  state[1] = Int2I(0xff);  /* avoid a zero state */
  state[2] = Int2I(n2);
  state[3] = Int2I(0);
  for (i = 0; i < 16; i++)
    nextrand(state);  /* discard initial values to "spread" seed */
  lua_pushinteger(L, n1);
  lua_pushinteger(L, n2);
}


/*
** Set a "random" seed. To get some randomness, use the current time
** and the address of 'L' (in case the machine does address space layout
** randomization).
*/
static void randseed (lua_State *L, RanState *state) {
  lua_Unsigned seed1 = (lua_Unsigned)time(NULL);
  lua_Unsigned seed2 = (lua_Unsigned)(size_t)L;
  setseed(L, state->s, seed1, seed2);
}


/* math.randomseed ([x [, y]])

   When called with at least one argument, the integer parameters x and y are joined into a 128-bit seed that is used to reinitialize the pseudo-random
   generator; equal seeds produce equal sequences of numbers. The default for y is zero.

   When called with no arguments, Lua generates a seed with a weak attempt for randomness.

   This function returns the two seed components that were effectively used, so that setting them again repeats the sequence.

   To ensure a required level of randomness to the initial state (or contrarily, to have a deterministic sequence, for instance when debugging a program),
   you should call math.randomseed with explicit arguments.

   The function has been taken from Lua 5.4.4, 2.27.10 */
static int math_randomseed (lua_State *L) {
  RanState *state = (RanState *)lua_touserdata(L, lua_upvalueindex(1));
  if (lua_isnone(L, 1)) {
    randseed(L, state);
  }
  else {
    lua_Integer n1 = luaL_checkinteger(L, 1);
    lua_Integer n2 = luaL_optinteger(L, 2, 0);
    setseed(L, state->s, n1, n2);
  }
  return 2;  /* return seeds */
}

static const luaL_Reg randfuncs[] = {
  {"random", math_random},
  {"randomseed", math_randomseed},
  {NULL, NULL}
};


/* Registers the random functions and initialize their state. See luaopen_math(). */
static void setrandfunc (lua_State *L) {
  RanState *state = (RanState *)lua_newuserdatauv(L, sizeof(RanState), 1);  /* instead of 0 */
  randseed(L, state);  /* initialize with a "random" seed */
  lua_pop(L, 2);  /* remove pushed seeds */
  luaL_setfuncs(L, randfuncs, 1);
}


/* Returns x rounded to the nearest integer towards zero, returns the same result as int(x).
   The function has been included for C math library compatibility reasons only. 2.29.2 */
static int math_trunc (lua_State *L) {
  lua_pushnumber(L, sun_trunc(agn_checknumber(L, 1)));
  return 1;
}


/* returns the sign of the gamma function, i.e. -1 if x < 0 and odd(entier(x)), and 1 otherwise. 3.7.5 */
static int math_gammasign (lua_State *L) {
  int i, s, nargs;
  nargs = lua_gettop(L);
  s = 1;
  for (i=1; i <= nargs; i++) {
    s *= tools_gammasign(agn_checknumber(L, i));
  }
  lua_pushinteger(L, s);
  return 1;
}


/* Computes the Hamming distance of two integers considered as binary values, that is, as sequences of bits. 3.10.0
   See https://en.wikipedia.org/wiki/Hamming_distance */
static int math_hamming (lua_State *L) {
#if !(defined(__DJGPP__) || defined(__OS2__))
  uint64_t x = (uint64_t)agn_checknumber(L, 1);
  uint64_t y = (uint64_t)agn_checknumber(L, 1);
  lua_pushinteger(L, __builtin_popcountll(x ^ y));
#else
  uint32_t x = (uint32_t)agn_checknumber(L, 1);
  uint32_t y = (uint32_t)agn_checknumber(L, 1);
  lua_pushinteger(L, tools_onebits32(x ^ y));
#endif
  return 1;
}


/* Approximates the arithmetic–geometric mean of two real numbers a, g, that is the limit of the sequence:

   a[0] := a; g[0] := g;
   a[n+1] := 0.5*(a[n] + g[n]); g[n+1] := sqrt(a[n]*g[n]);

   The return is a number.

   The ALGOL 68 version implemented here, see
      https://rosettacode.org/wiki/Arithmetic-geometric_mean#ALGOL_68
   in C might be a little bit slower than the one using a fixed epsilon value, but at least it converges
   when compiled with MinGW/GCC 9.2.0, contrary to
      https://rosettacode.org/wiki/Arithmetic-geometric_mean#C
   which hangs with math.atm(1026, 1027) for example. 3.10.7 */
static int math_agm (lua_State *L) {
  if (agn_isnumber(L, 1) && agn_isnumber(L, 2)) {
    lua_Number a, g;
    a = agn_checknumber(L, 1);
    g = agn_checknumber(L, 2);
    if (isnan(a) || isnan(g) || a*g < 0.0) {  /* 3.13.1 fix */
      lua_pushundefined(L);
    } else if (a + g == 0.0) {
      lua_pushnumber(L, 0.0);
    } else {
      lua_Number epsilon, next_epsilon, next_a, next_g;
      epsilon = a + g;
      next_a = 0.5*epsilon;
      next_g = sqrt(a*g);
      next_epsilon = fabs(a - g);
      while (next_epsilon < epsilon) {
        epsilon = next_epsilon;
        a = next_a;
        g = next_g;
        next_a = 0.5*(a + g);
        next_g = sqrt(a*g);
        next_epsilon = fabs(a - g);
      }
      lua_pushnumber(L, a);
    }
  } else {  /* 3.10.8 extension */
    lua_Number ar, ai, gr, gi;
    if (agn_isnumber(L, 1) && lua_iscomplex(L, 2)) {
      ar = agn_tonumber(L, 1);
      ai = 0;
      agn_getcmplxparts(L, 2, &gr, &gi);
    } else if (lua_iscomplex(L, 1) && agn_isnumber(L, 2)) {
      agn_getcmplxparts(L, 1, &ar, &ai);
      gr = agn_tonumber(L, 2);
      gi = 0;
    } else if (lua_iscomplex(L, 1) && lua_iscomplex(L, 2)) {
      agn_getcmplxparts(L, 1, &ar, &ai);
      agn_getcmplxparts(L, 2, &gr, &gi);
    } else {
      luaL_nonumorcmplx(L, 1, "math.agm");
    }
    if (isnan(ar) || isnan(ai) || isnan(gr) || isnan(gi)) {  /* 3.13.1 fix */
      lua_pushundefined(L);
      return 1;
    }
    if (ai == 0.0 && gi == 0.0) {
      if (ar*gr < 0.0) {
        lua_pushundefined(L);
        return 1;
      } else if (ar + gr == 0.0) {
        agn_pushcomplex(L, 0.0, 0.0);
        return 1;
      }
    }
#ifdef PROPCMPLX
    {  /* 50 % faster than an Agena implementation */
      lua_Number epsilon[2], next_epsilon, next_a[2], next_g[2], aplusg[2], aminusg[2], atimesg[2];
      agnCmplx_add(epsilon, ar, ai, gr, gi);
      agnCmplx_mul(next_a, 0.5, 0, epsilon[0], epsilon[1]);
      agnCmplx_mul(atimesg, ar, ai, gr, gi);
      agnCmplx_sqrt(next_g, atimesg[0], atimesg[1]);
      agnCmplx_sub(aminusg, ar, ai, gr, gi);
      next_epsilon = sun_hypot(aminusg[0], aminusg[1]);
      while (next_epsilon < sun_hypot(epsilon[0], epsilon[1])) {
        epsilon[0] = next_epsilon;
        epsilon[1] = 0;
        ar = next_a[0]; ai = next_a[1];
        gr = next_g[0]; gi = next_g[1];
        agnCmplx_add(aplusg, ar, ai, gr, gi);
        agnCmplx_mul(next_a, 0.5, 0, aplusg[0], aplusg[1]);
        agnCmplx_mul(atimesg, ar, ai, gr, gi);
        agnCmplx_sqrt(next_g, atimesg[0], atimesg[1]);
        agnCmplx_sub(aminusg, ar, ai, gr, gi);
        next_epsilon = sun_hypot(aminusg[0], aminusg[1]);
      }
      agn_pushcomplex(L, ar, ai);
    }
#else
    {  /* twice as fast as an Agena implementation */
      agn_Complex a, g, next_a, next_g;
      lua_Number epsilon, next_epsilon;
      a = ar + I*ai;
      g = gr + I*gi;
      epsilon = a + g;
      next_a = 0.5*epsilon;
      next_g = tools_csqrt(a*g);
      next_epsilon = tools_cabs(a - g);
      while (next_epsilon < tools_cabs(epsilon)) {
        epsilon = next_epsilon;
        a = next_a;
        g = next_g;
        next_a = 0.5*(a + g);
        next_g = tools_csqrt(a*g);
        next_epsilon = tools_cabs(a - g);
      }
      agn_createcomplex(L, a);
    }
#endif
  }
  return 1;
}


static int math_tocomplex (lua_State *L) {
  if (agn_isnumber(L, 1)) {
    agn_pushcomplex(L, agn_tonumber(L, 1), 0);
  } else if (lua_iscomplex(L, 1)) {
    lua_pushvalue(L, 1);
  } else {
    luaL_nonumorcmplx(L, 1, "math.tocomplex");
  }
  return 1;
}


static int math_sind (lua_State *L) {
  lua_pushnumber(L, sindg(agn_checknumber(L, 1)));
  return 1;
}


static int math_cosd (lua_State *L) {
  lua_pushnumber(L, cosdg(agn_checknumber(L, 1)));
  return 1;
}


static int math_tand (lua_State *L) {
  lua_pushnumber(L, tandg(agn_checknumber(L, 1)));
  return 1;
}


static int math_cotd (lua_State *L) {
  lua_pushnumber(L, cotdg(agn_checknumber(L, 1)));
  return 1;
}


static int math_cscd (lua_State *L) {  /* 3.11.1 */
  lua_pushnumber(L, cscdg(agn_checknumber(L, 1)));
  return 1;
}


static int math_secd (lua_State *L) {  /* 3.11.1 */
  lua_pushnumber(L, secdg(agn_checknumber(L, 1)));
  return 1;
}


/* Taken from:
   https://github.com/luau-lang/luau
   file: VM/src/lmathlib.cpp

   Licence: MIT.

   Copyright (c) 2019-2024 Roblox Corporation
   Copyright (c) 1994–2019 Lua.org, PUC-Rio. */

static const unsigned char kPerlinHash[257] = {
  151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,
  190, 6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,
  125, 136, 171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,
  41,  55,  46,  245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130,
  116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207,
  206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,
  172, 9,   129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144,
  12,  191, 179, 162, 241, 81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157, 184, 84,  204, 176, 115, 121, 50,  45,
  127, 4,   150, 254, 138, 236, 205, 93,  222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180, 151
};

static const lua_Number kPerlinGrad[16][3] = {
  {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0},
  {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1},
  {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1},
  {1, 1, 0}, {0, -1, 1}, {-1, 1, 0}, {0, -1, -1}
};

#define perlin_fade(t) ((t)*(t)*(t)*((t)*(t*6 - 15) + 10))

#define perlin_lerp(t,a,b) ((a) + (t)*((b) - (a)))

static inline lua_Number perlin_grad (int hash, lua_Number x, lua_Number y, lua_Number z) {
  const lua_Number *g = kPerlinGrad[hash & 15];
  return g[0]*x + g[1]*y + g[2]*z;
}

static lua_Number perlin (lua_Number x, lua_Number y, lua_Number z) {
  int xi, yi, zi, a, aa, ab, b, ba, bb;
  lua_Number xflr, yflr, zflr, xf, yf, zf, u, v, w, la, lb, la1, lb1;
  const unsigned char *p;
  xflr = sun_floor(x);
  yflr = sun_floor(y);
  zflr = sun_floor(z);
  xi = (int)(xflr) & 255;
  yi = (int)(yflr) & 255;
  zi = (int)(zflr) & 255;
  xf = x - xflr;
  yf = y - yflr;
  zf = z - zflr;
  u = perlin_fade(xf);
  v = perlin_fade(yf);
  w = perlin_fade(zf);
  p = kPerlinHash;
  a =  (p[xi] + yi) & 255;
  aa = (p[a] + zi) & 255;
  ab = (p[a + 1] + zi) & 255;
  b =  (p[xi + 1] + yi) & 255;
  ba = (p[b] + zi) & 255;
  bb = (p[b + 1] + zi) & 255;
  la = perlin_lerp(u, perlin_grad(p[aa], xf, yf, zf), perlin_grad(p[ba], xf - 1, yf, zf));
  lb = perlin_lerp(u, perlin_grad(p[ab], xf, yf - 1, zf), perlin_grad(p[bb], xf - 1, yf - 1, zf));
  la1 = perlin_lerp(u, perlin_grad(p[aa + 1], xf, yf, zf - 1), perlin_grad(p[ba + 1], xf - 1, yf, zf - 1));
  lb1 = perlin_lerp(u, perlin_grad(p[ab + 1], xf, yf - 1, zf - 1), perlin_grad(p[bb + 1], xf - 1, yf - 1, zf - 1));
  return perlin_lerp(w, perlin_lerp(v, la, lb), perlin_lerp(v, la1, lb1));
}

/* With numbers x, y and z computes gradient Perlin noise which can be used in applying pseudo-random changes to a variable,
   procedurally generating terrain, and assisting in the creation of image textures. If not given y and z default to zero.
   By default, the return is a number in the range [-1, 1]. By passing the last argument `true`, the return is a number in
   [0, 1]. All input values should be in [-1, 1]. 4.9.0 */
static int math_noise (lua_State* L) {
  lua_Number r = perlin(agn_checknumber(L, 1), agnL_optnumber(L, 2, 0), agnL_optnumber(L, 3, 0));
  lua_pushnumber(L, (lua_istrue(L, lua_gettop(L))) ? (r + 1.0)/2.0 : r);
  return 1;
}


/* Returns the linear interpolation between a and b based on factor t. By default, the function uses the formula a + (b - a)*t =
   fma(t, b - a, a), which has monotonic behaviour. t is typically between 0 and 1 but values outside this range are acceptable.
   With any fourth argument given, the function returns a*(1 - t) + t*b = fma(1 - t, a, t*b). This variant is monotonic only
   if a*b < 0. With t = 0 returns a; with t = 1 returns b. With a = b and any t returns a.
   Example: To compute the number that is 35% between 56 and 132, issue:
   > math.lerp(56, 132, 0.35):
   82.6
   See also: `math.invlerp`.
   The function is twice as fast as the Agena implementation
      lerp := << a :: number, b :: number, t :: number, opt -> if opt then fma(1 - t, a, t*b) else fma(t, b - a, a) fi >>;
   Idea taken from luau, but using built-in hardware fused multiply-add. 4.12.4. See also:
   https://en.wikipedia.org/wiki/Linear_interpolation
   https://www.trysmudford.com/blog/linear-interpolation-functions
   https://luau.org/ */
static int math_lerp (lua_State* L) {
  lua_Number a, b, t;
  a = agn_checknumber(L, 1);
  b = agn_checknumber(L, 2);
  t = agn_checknumber(L, 3);
  lua_pushnumber(L, lua_gettop(L) == 3 ? fma(t, b - a, a) : fma(1 - t, a, t*b));
  return 1;
}


/* The function works in the opposite way to `math.lerp`. Instead of passing a factor t, you pass any value, and the function
   will return the corresponding factor, wherever it falls on that spectrum. With a = b, returns `undefined`.
   For example, to find the value half-way between 50 and 100, enter:
   > math.lerp(50, 100, .5):
   75
   > math.invlerp(50, 100, 75):
   0.5
   The function has been inspired by Trys Mudford, see his blog at
   https://www.trysmudford.com/blog/linear-interpolation-functions/ */
static int math_invlerp (lua_State* L) {
  lua_Number a, b, x;
  a = agn_checknumber(L, 1);
  b = agn_checknumber(L, 2);
  x = agn_checknumber(L, 3);
  lua_pushnumber(L, b == a ? AGN_NAN : tools_clip((x - a)/(b - a), 0, 1));
  return 1;
}


/* In default mode, works exactly as the `symmod` operator but implemented as a function. If given any option,
   then the SunPro version will be called internally instead of the standard GCC implementation. 4.12.5. */
static int math_fmod (lua_State *L) {
  lua_Number x, y;
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  lua_pushnumber(L, lua_gettop(L) == 2 ? fmod(x, y) : sun_fmod(x, y));
  return 1;
}


static int math_onepinv (lua_State *L) {  /* = 1 + 1/x, UNDOC, 6.7.5, 50 % slower than genuine 1+1/x in the real domain */
  if (lua_isnumber(L, 1)) {
    lua_Number x = agn_tonumber(L, 1);
    lua_pushnumber(L, x == 0.0 ? AGN_NAN: 1.0 + 1.0/x);
  } else if (lua_iscomplex(L, 1)) {
    double a, b, t1, t2, t3, t4;
    agn_getcmplxparts(L, 1, &a, &b);
    t1 = a*a;
    t2 = b*b;
    t3 = t1 + t2;
    if (t3 == 0.0) {
      lua_pushundefined(L);
    } else {
      t4 = 1/t3;
      agn_pushcomplex(L, 1.0 + a*t4, b*t4);
    }
  } else if (agn_isdual(L, 1)) {
    trydualfix(L, "onepinv", 1);
    return 1;
  } else
    luaL_nonumorcmplxordual(L, 1, "onepinv");
  return 1;
}


static int math_pow32 (lua_State *L) {  /* x*sqrt(x) = x^(3/2), 6.7.5 */
  if (lua_isnumber(L, 1)) {
    lua_Number x = agn_tonumber(L, 1);
    lua_pushnumber(L, x*sqrt(x));
  } else if (lua_iscomplex(L, 1)) {
    double re, im;
    long double a, b, t1, t2, t4, t6, t12, t15;
    agn_getcmplxparts(L, 1, &re, &im);
    a = (long double)re; b = (long double)im;
    t1 = a*a;
    t2 = b*b;
    t4 = sqrtl(t1 + t2);
    t6 = sqrtl(2.0L*t4 + 2.0L*a);
    t12 = tools_csgnl(b, -a);
    t15 = sqrtl(2.0L*t4 - 2.0L*a);
    agn_pushcomplex(L, 0.5L*t6*a + (-0.5L*t4 + 0.5L*a)*t6, (0.5L*t4 + 0.5L*a)*t12*t15 + 0.5L*t12*t15*a);
  } else if (agn_isdual(L, 1)) {
    trydualfix(L, "pow32", 1);
    return 1;
  } else
    luaL_nonumorcmplxordual(L, 1, "pow32");
  return 1;
}


static int math_pow52 (lua_State *L) {  /* x*x*sqrt(x) = x^(5/2), 6.7.5 */
  if (lua_isnumber(L, 1)) {
    lua_Number x = agn_tonumber(L, 1);
    lua_pushnumber(L, x*x*sqrt(x));
  } else if (lua_iscomplex(L, 1)) {
    double re, im;
    long double a, b, t1, t2, t3, t4, t6, t10, t18, t19, t21, t22, t29;
    agn_getcmplxparts(L, 1, &re, &im);
    a = (long double)re; b = (long double)im;
    t1 = a*a;
    t2 = b*b;
    t3 = t1 + t2;
    t4 = sqrtl(t3);
    t6 = sqrtl(2.0L*t4 + 2.0L*a);
    t10 = (-0.5L*t4 + 0.5L*a)*t6;
    t18 = tools_csgnl(b, -a);
    t19 = (0.5L*t4 + 0.5L*a)*t18;
    t21 = sqrtl(2.0L*t4 - 2.0L*a);
    t22 = t21*t4;
    t29 = t18*t21;
    agn_pushcomplex(L, -0.5L*t6*t3 + t6*t1 + t10*t4 + 2.0L*t10*a + (0.5L*t4 - 0.5L*a)*t6*t4,
      t19*t22 + 2.0L*t19*t21*a + (-0.5L*t4 - 0.5L*a)*t18*t22 - 0.5L*t29*t3 + t29*t1);
  } else if (agn_isdual(L, 1)) {
    trydualfix(L, "pow52", 1);
    return 1;
  } else
    luaL_nonumorcmplxordual(L, 1, "pow52");
  return 1;
}


static int math_pow72 (lua_State *L) {  /* x*x*x*sqrt(x) = x^(7/2), 6.7.7 */
  if (lua_isnumber(L, 1)) {
    lua_Number sqrtx, x2, x = agn_tonumber(L, 1);
    /* protect against fast overflow */
    sqrtx = sqrt(x);
    x2 = x*x;
    lua_pushnumber(L, (x*sqrtx)*x2);  /* 3 times faster than sun_pow(x, 3.5, 0 or 1) */
  } else if (lua_iscomplex(L, 1)) {
    double re, im;
    long double a, b, t1, t2, t3, t4, t6, t11, t18, t21, t28, t30, t31, t37, t42, t49;
    agn_getcmplxparts(L, 1, &re, &im);
    a = (long double)re; b = (long double)im;
    t1 = a*a;
    t2 = b*b;
    t3 = t1 + t2;
    t4 = sqrtl(t3);
    t6 = sqrtl(2.0L*t4 + 2.0L*a);
    t11 = t4*a;
    t18 = t1*a;
    t21 = (-0.75L*t4 + 0.75L*a)*t6;
    t28 = tools_csgnl(b, -a);
    t30 = sqrtl(2.0L*t4 - 2.0L*a);
    t31 = t28*t30;
    t37 = t30*t2;
    t42 = (0.75L*t4 + 0.75L*a)*t28;
    t49 = t30*t4*a;
    agn_pushcomplex(L, -t6*t3*a + (0.75L*t4 - 0.75L*a)*t6*t11 + 2.0L*(-0.375L*t4 + 0.375L*a)*
      t6*t2 - 0.5*t6*a*t2 + 1.5L*t6*t18 + 2.0L*t21*t1 + t21*t11 + (1.25L*t4 - 1.25L*a)*t6*t2,
      1.5L*t31*t18 - t31*t3*a + 2.0L*(0.375L*t4 + 0.375L*a)*t28*t37 - t31*t2*0.5L*a +
      2.0L*t42*t30*t1 + (-1.25L*t4 - 1.25L*a)*t28*t37 + t42*t49 + (-0.75L*t4 - 0.75L*a)*t28*t49);
  } else if (agn_isdual(L, 1)) {
    trydualfix(L, "pow72", 1);
    return 1;
  } else
    luaL_nonumorcmplxordual(L, 1, "pow72");
  return 1;
}


static int math_powe (lua_State *L) {  /* x^E, 6.7.7 */
  if (lua_isnumber(L, 1)) {
    lua_Number x = agn_tonumber(L, 1);
    lua_pushnumber(L, tools_powl(x, M_Eld));
  } else if (lua_iscomplex(L, 1)) {
    double re, im;
    long double a, b, t1, t2, t3, t7, t9, si, co;
    agn_getcmplxparts(L, 1, &re, &im);
    a = (long double)re; b = (long double)im;
    t1 = M_Eld;
    t2 = a*a;
    t3 = b*b;
    t7 = tools_expl(0.5L*t1*tools_logl(t2 + t3));
    t9 = t1*sun_atan2l(b, a);
    sun_sincosl(t9, &si, &co);
    agn_pushcomplex(L, t7*co, t7*si);
  } else if (agn_isdual(L, 1)) {
    trydualfix(L, "powe", 1);
    return 1;
  } else
    luaL_nonumorcmplxordual(L, 1, "powe");
  return 1;
}


/* Checks whether the number x is equal to the number y and returns `true` or `false`. 5.5.1 */
static int math_eq (lua_State *L) {
  lua_pushboolean(L, agn_checknumber(L, 1) == agn_checknumber(L, 2));
  return 1;
}


/* Checks whether the number x is not equal to the number y and returns `true` or `false`. 5.5.1 */
static int math_neq (lua_State *L) {
  lua_pushboolean(L, agn_checknumber(L, 1) != agn_checknumber(L, 2));
  return 1;
}


/* Checks whether the number x is less than the number y and returns `true` or `false`. 5.5.1 */
static int math_lt (lua_State *L) {
  lua_pushboolean(L, agn_checknumber(L, 1) < agn_checknumber(L, 2));
  return 1;
}


/* Checks whether the number x is greater than the number y and returns `true` or `false`. 5.5.1 */
static int math_gt (lua_State *L) {
  lua_pushboolean(L, agn_checknumber(L, 1) > agn_checknumber(L, 2));
  return 1;
}


/* Checks whether the number x is less than or equal to the number y and returns `true` or `false`. 5.5.1 */
static int math_le (lua_State *L) {
  lua_pushboolean(L, agn_checknumber(L, 1) <= agn_checknumber(L, 2));
  return 1;
}


/* Checks whether the number x is greater than or equal to the number y and returns `true` or `false`. 5.5.1 */
static int math_ge (lua_State *L) {
  lua_pushboolean(L, agn_checknumber(L, 1) >= agn_checknumber(L, 2));
  return 1;
}


static const luaL_Reg mathlib[] = {
  {"accu", math_accu},                    /* added on May 26, 2018 */
  {"add", math_add},                      /* added on September 24, 2022 */
  {"adjust", math_adjust},                /* added on October 08, 2022 */
  {"agm", math_agm},                      /* added on February 27, 2024 */
  {"beta", math_beta},                    /* added on March 08, 2020 */
  {"bintodec", math_bintodec},            /* added on May 18, 2023 */
  {"branch", math_branch},                /* added on September 25, 2016 */
  {"ceillog2", math_ceillog2},            /* added on November 21, 2014 */
  {"ceilpow2", math_ceilpow2},            /* added on November 27, 2014 */
  {"chi", math_chi},                      /* added on December 05, 2023 */
  {"chop", math_chop},                    /* added on September 17, 2017 */
  {"cld", math_cld},                      /* added on October 19, 2016 */
  {"clip", math_clip},                    /* added on September 04, 2017 */
  {"compose", math_compose},              /* added on April 24, 2022 */
  {"copysign", math_copysign},            /* added on December 04, 2014 */
  {"coscpi", math_coscpi},                /* added on November 07, 2024 */
  {"cosd", math_cosd},                    /* added on March 03, 2024 */
  {"cospi", math_cospi},                  /* added on December 29, 2022 */
  {"cotd", math_cotd},                    /* added on March 03, 2024 */
  {"cscd", math_cscd},                    /* added on March 05, 2024 */
  {"dblfact", math_dblfact},              /* added June 23, 2023 */
  {"dd", math_dd},                        /* added on November 28, 2022 */
  {"decompose", math_decompose},          /* added on June 05, 2015 */
  {"dirac", math_dirac},                  /* added on September 04, 2017 */
  {"divide", math_divide},                /* added on September 23, 2022 */
  {"dms", math_dms},                      /* added on July 27, 2015 */
  {"eps", math_eps},                      /* added on February 09, 2015 */
  {"eq", math_eq},                        /* added on August 23, 2025 */
  {"epsilon", math_epsilon},              /* added January 30, 2017 */
  {"examul", math_examul},                /* added on March 10, 2024 */
  {"expm1", math_expm1},                  /* added on December 02, 2014 */
  {"exponent", math_exponent},            /* added on January 24, 2017 */
  {"fall", math_fall},                    /* added on June 11, 2017 */
  {"fdim", math_fdim},                    /* added on August 03, 2015 */
  {"fld", math_fld},                      /* added on October 19, 2016 */
  {"flipsign", math_flipsign},            /* added on November 21, 2014 */
  {"floorpow2", math_floorpow2},          /* added on August 01, 2019 */
  {"fmod", math_fmod},                    /* added on May 20, 2025 */
  {"fpclassify", math_fpclassify},        /* added on September 09, 2017 */
  {"frexp", math_frexp},                  /* added on February 08, 2017 */
  {"gammasign", math_gammasign},          /* added on December 19, 2023 */
  {"ge", math_ge},                        /* added on August 23, 2025 */
  {"gt", math_gt},                        /* added on August 23, 2025 */
  {"hamming", math_hamming},              /* added on January 24, 2024 */
  {"hextodec", math_hextodec},            /* added on May 16, 2023 */
  {"ilog2", math_ilog2},                  /* added on June 24, 2022 */
  {"intdivide", math_intdivide},          /* added on September 24, 2022 */
  {"invlerp", math_invlerp},              /* added on May 18, 2025 */
  {"isexceptional", math_isexceptional},  /* added on February 12, 2026 */
  {"isinfinity", math_isinfinity},        /* added on October 19, 2015 */
  {"isirregular", math_isirregular},      /* added on March 05, 2017 */
  {"isminuszero", math_isminuszero},      /* added on August 12, 2015 */
  {"isnormal", math_isnormal},            /* added on April 08, 2022 */
  {"isordered", math_isordered},          /* added on December 02, 2014 */
  {"ispow2", math_ispow2},                /* added on January 19, 2017 */
  {"isqrt", math_isqrt},                  /* added on October 19, 2016 */
  {"issubnormal", math_issubnormal},      /* added on January 31, 2017 */
  {"kbadd", math_kbadd},                  /* added on December 12, 2023 */
  {"koadd", math_koadd},                  /* added on February 01, 2015 */
  {"le", math_le},                        /* added on August 23, 2025 */
  {"length", math_length},                /* added on July 02, 2024 */
  {"lerp", math_lerp},                    /* added on May 18, 2025 */
  {"lnabs", math_lnabs},                  /* added on January 22, 2020 */
  {"lnbeta", math_lnbeta},                /* added on October 07, 2024 */
  {"lnbinomial", math_lnbinomial},        /* added on December 14, 2023 */
  {"lnfact", math_lnfact},                /* added on October 14, 2016 */
  {"lnhypot", math_lnhypot},              /* added May 21, 2024 */
  {"lnp1", math_lnp1},                    /* added on December 02, 2014 */
  {"lnpytha", math_lnpytha},              /* added on November 15, 2024 */
  {"logs", math_logs},                    /* added on August 14, 2022 */
  {"lt", math_lt},                        /* added on August 23, 2025 */
  {"mantissa", math_mantissa},            /* added on January 24, 2017 */
  {"max", math_max},
  {"min", math_min},
  {"modulo", math_modulo},                /* added on September 24, 2022 */
  {"modulus", math_modulus},              /* added on January 29, 2017 */
  {"morton", math_morton},                /* added on November 27, 2014 */
  {"mulsign", math_mulsign},              /* added on April 01, 2020 */
  {"multiply", math_multiply},            /* added on September 24, 2022 */
  {"ndigits", math_ndigits},              /* added on August 12, 2017 */
  {"nearbyint", math_nearbyint},          /* added on January 02, 2023 */
  {"nearmod", math_nearmod},              /* added on March 30, 2019 */
  {"neq", math_neq},                      /* added on August 23, 2025 */
  {"nextafter", math_nextafter},          /* added on August 17, 2009 */
  {"nextmultiple", math_nextmultiple},    /* added on October 15, 2020 */
  {"nextpow2", math_nextpow2},            /* added on August 11, 2025 */
  {"nextpower", math_nextpower},          /* added on March 29, 2022 */
  {"noise", math_noise},                  /* added on February 7, 2025 */
  {"norm", math_norm},                    /* added on May 22, 2010 */
  {"normalise", math_normalise},          /* added on March 14, 2017 */
  {"octtodec", math_octtodec},            /* added on May 17, 2023 */
  {"onepinv", math_onepinv},              /* added February 09, 2026 */
  {"pochhammer", math_pochhammer},        /* added on August 14, 2018 */
  {"pow32", math_pow32},                  /* added on February 09, 2026 */
  {"pow52", math_pow52},                  /* added on February 09, 2026 */
  {"pow72", math_pow72},                  /* added on February 11, 2026 */
  {"powe", math_powe},                    /* added on February 11, 2026 */
  {"precision", math_precision},          /* added on March 05, 2017 */
  {"prevmultiple", math_prevmultiple},    /* added on August 11, 2025 */
  {"prevpow2", math_prevpow2},            /* added on August 11, 2025 */
  {"prevpower", math_prevpower},          /* added on August 06, 2024 */
  {"quadrant", math_quadrant},            /* added on August 02, 2015 */
  {"ramp", math_ramp},                    /* added on March 26, 2020 */
  {"randoms", math_randoms},
  {"randomseeds", math_randomseeds},
  {"rectangular", math_rectangular},      /* added on February 08, 2015/August 06, 2017 */
  {"redupi", math_redupi},                /* added on June 17, 2023 */
  {"relerror", math_relerror},            /* added on July 21, 2020 */
  {"remainder", math_remainder},          /* added on March 28, 2023 */
  {"rempio2", math_rempio2},              /* added on October 22, 2017 */
  {"rint", math_rint},                    /* added on September 28, 2015 */
  {"secd", math_secd},                    /* added on March 05, 2024 */
  {"signbit", math_signbit},              /* added on February 08, 2015 */
  {"significand", math_significand},      /* added on May 04, 2019 */
  {"sincos", math_sincos},                /* added on October 17, 2017 */
  {"sincospi", math_sincospi},            /* added on December 27, 2022 */
  {"sincpi", math_sincpi},                /* added on November 09, 2024 */
  {"sind", math_sind},                    /* added on March 03, 2024 */
  {"sinpi", math_sinpi},                  /* added on December 29, 2022 */
  {"sinhcosh", math_sinhcosh},            /* added on November 05, 2017 */
  {"splitdms", math_splitdms},            /* added on May 10, 2024 */
  {"subtract", math_subtract},            /* added on September 24, 2022 */
  {"tancpi", math_tancpi},                /* added on November 11, 2024 */
  {"tand", math_tand},                    /* added on March 03, 2024 */
  {"tanpi", math_tanpi},                  /* added on December 29, 2022 */
  {"tocomplex", math_tocomplex},          /* added on March 02, 2024 */
  {"todecimal", math_todecimal},          /* added on August 25, 2013 */
  {"todegrees", math_todegrees},          /* added on May 10, 2024 */
  {"tohex",  math_tohex},                 /* added on November 17, 2017 */
  {"toradians", math_toradians},          /* added on January 21, 2008 */
  {"tosgesim",  math_tosgesim},           /* added on July 26, 2016 */
  {"triangular",  math_triangular},       /* added on September 09, 2017 */
  {"trifact", math_trifact},              /* added June 23, 2023 */
  {"trunc", math_trunc},                  /* added on July 09, 2022 */
  {"ulp", math_ulp},                      /* added on July 21, 2018 */
  {"uexponent", math_uexponent},          /* added on September 09, 2018 */
  {"unitise",  math_unitise},             /* added on August 30, 2017 */
  {"unitstep",  math_unitstep},           /* added on March 26, 2020 */
  {"xlnp1", math_xlnp1},                  /* added on February 08, 2020 */
  {"wrap", math_wrap},                    /* added on November 11, 2015 */
  {"zerosubnormal", math_zerosubnormal},  /* added on February 01, 2017 */
  {"zeroin", math_zeroin},                /* added on August 07, 2022 */
  {NULL, NULL}
};


/*
** Open math library
*/
LUALIB_API int luaopen_math (lua_State *L) {
  luaL_register(L, LUA_MATHLIBNAME, mathlib);
  lua_pushnumber(L, AGN_LASTCONTINT);      /* added 2.10.0; 2^53 = 9007199254740992 */
  lua_setfield(L, -2, "lastcontint");      /* Largest integer i so that i-1 <> i but i = i+1; needed by math.convertbase */
  lua_pushnumber(L, sun_pow(2, -1022, 0)); /* added 2.10.0 */
  lua_setfield(L, -2, "smallestnormal");   /* smallest normal number 2.2250738585072009e-308 = 2^-1022 on x86 CPUs */
  lua_pushnumber(L, TWO54);                /* added 2.27.0 */
  lua_setfield(L, -2, "two54");            /* constant with which subnormal numbers can be multiplied to get normal */
  /* for math.smallest see library.agn */
  /* the following fp_* constants have been reordered in 2.25.4, see math.fpclassify: */
  lua_pushinteger(L, 0);                   /* added 2.11.0 RC2 */
  lua_setfield(L, -2, "fp_nan");           /* for FP_NAN */
  lua_pushinteger(L, 1);                   /* added 2.11.0 RC2 */
  lua_setfield(L, -2, "fp_infinite");      /* for FP_INFINITE */
  lua_pushinteger(L, 2);                   /* added 2.11.0 RC2 */
  lua_setfield(L, -2, "fp_subnormal");     /* for FP_SUBNORMAL */
  lua_pushinteger(L, 3);                   /* added 2.11.0 RC2 */
  lua_setfield(L, -2, "fp_zero");          /* for FP_ZERO */
  lua_pushinteger(L, 4);                   /* added 2.11.0 RC2 */
  lua_setfield(L, -2, "fp_normal");        /* for FP_NORMAL */
  setrandfunc(L);                          /* math.xrandom/seed; added on May 25, 2022 */
  return 1;
}

