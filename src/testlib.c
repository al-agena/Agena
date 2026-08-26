/*
** $Id: testlib.c,v 1.67 2005/08/26 17:36:32 roberto Exp $
** Approximations
** See Copyright Notice in agena.h
*/

/*
import testlib, stats
s := seq();
for x from -100 to 100 by 0.001 do
   insert testlib.dexp10(x) |- math.exp10(x) into s
od;
stats.median(stats.sorted(s)):
stats.amean(s):

import testlib

gcc := testlib.sunpow
fn := testlib.dpow

watch();
for i from -10 to 10 by 0.001 do
   x := fn(i)
od;
watch():

watch();
for i from -10 to 10 by 0.001 do
   y := gcc(i)
od;
watch():
*/

#include <stdlib.h>
#include <stdint.h>  /* for UINT32_MAX */
#include <math.h>
#include <string.h>

#define testlib_c
#define LUA_LIB

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"
#include "agnconf.h"
#include "agncmpt.h"  /* for fmaf, trunc and isfinite, and FLT_EVAL_METHOD constant, and off64* types */

/* For unknown reasons I am too tired to clarify, we import the *GET_*, *SET_*, *INSERT*, *EXTRACT*,
   etc. Sun Microsystems macros from the following header file instead of agnhlps.h: */
#include "sunpro.h"

#define AGENA_LIBVERSION	"testlib 0.0.3 for Agena as of June 10, 2026\n"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_TESTLIBLIBNAME "testlib"
LUALIB_API int (luaopen_testlib) (lua_State *L);
#endif

/* ***************************************************************************************************** */
/* For reference tests only:                                                                             */
/* ***************************************************************************************************** */

static int testlib_sunsin (lua_State *L) {
  lua_pushnumber(L, sun_sin(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccsin (lua_State *L) {
  lua_pushnumber(L, sin(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_suncos (lua_State *L) {
  lua_pushnumber(L, sun_cos(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gcccos (lua_State *L) {
  lua_pushnumber(L, cos(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunsincos (lua_State *L) {
  lua_Number si, co;
  sun_sincos(agn_checknumber(L, 1), &si, &co);
  lua_pushnumber(L, si);
  lua_pushnumber(L, co);
  return 2;
}

static int testlib_sunsinhcosh (lua_State *L) {
  lua_Number si, co;
  sun_sinhcosh(agn_checknumber(L, 1), &si, &co);
  lua_pushnumber(L, si);
  lua_pushnumber(L, co);
  return 2;
}


static int testlib_suntan (lua_State *L) {
  lua_pushnumber(L, sun_tan(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gcctan (lua_State *L) {
  lua_pushnumber(L, tan(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccsinh (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, sinh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gcccosh (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, cosh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gcctanh (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, tanh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunsqrt (lua_State *L) {  /* 4.5.6 */
  lua_pushnumber(L, sun_sqrt(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunlog (lua_State *L) {
  lua_pushnumber(L, sun_log(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gcclog (lua_State *L) {
  lua_pushnumber(L, log(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccilogb (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, ilogb(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunexp (lua_State *L) {
  lua_pushnumber(L, sun_exp(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunexp2 (lua_State *L) {
  lua_pushnumber(L, sun_exp2(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_exp2_9 (lua_State *L) {
  lua_pushnumber(L, tools_exp2_9(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunexp10 (lua_State *L) {
  lua_pushnumber(L, sun_exp10(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_exp10_9 (lua_State *L) {
  lua_pushnumber(L, tools_exp10_9(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccexp (lua_State *L) {
  lua_pushnumber(L, exp(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccexp2 (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, exp2(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccexpm1 (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, expm1(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunlog2 (lua_State *L) {
  lua_pushnumber(L, sun_log2(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunlog10 (lua_State *L) {
  lua_pushnumber(L, sun_log10(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunlog1p (lua_State *L) {
  lua_pushnumber(L, sun_log1p(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gcclog2 (lua_State *L) {
  lua_pushnumber(L, log2(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gcclog10 (lua_State *L) {
  lua_pushnumber(L, log10(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gcclog1p (lua_State *L) {
  lua_pushnumber(L, log1p(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccsqrt (lua_State *L) {
  lua_pushnumber(L, sqrt(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccrsqrt (lua_State *L) {
  lua_pushnumber(L, 1.0/sqrt(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gcccbrt (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, cbrt(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gcchypot (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, hypot(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}

static int testlib_gccasinh (lua_State *L) {
  lua_pushnumber(L, asinh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccacosh (lua_State *L) {
  lua_pushnumber(L, acosh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccasin (lua_State *L) {
  lua_pushnumber(L, asin(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccacos (lua_State *L) {
  lua_pushnumber(L, acos(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccatan (lua_State *L) {
  lua_pushnumber(L, atan(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunpow (lua_State *L) {
  lua_pushnumber(L, sun_pow(agn_checknumber(L, 1), agn_checknumber(L, 2), 1));
  return 1;
}

static int testlib_gccpow (lua_State *L) {  /* 2.29.5 UNDOC, DO NOT DELETE: needed for regression tests !!! */
  lua_Number x, y;
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  if (x == 0 && y <= 0)
    lua_pushundefined(L);
  else
    lua_pushnumber(L, pow(x, y));
  return 1;
}

static int testlib_gccatanh (lua_State *L) {
  lua_pushnumber(L, atanh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccrecip (lua_State *L) {
  lua_pushnumber(L, 1/agn_checknumber(L, 1));
  return 1;
}

static int testlib_gcctrunc (lua_State *L) {
#ifndef __ARMCPU  /* 2.37.1 */
  lua_pushnumber(L, trunc(agn_checknumber(L, 1)));
#else
  luaL_error(L, "Error in " LUA_QS ": the function is not supported on ARM platforms.", "testlib.gcctrunc");
#endif
  return 1;
}

static int testlib_gccfloor (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, floor(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccceil (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, ceil(agn_checknumber(L, 1)));
  return 1;
}

/* In OpenSUSE, calls sun_round instead of round which is not available there. */
static int testlib_gccround (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, round(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccerf (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, erf(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccerfc (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, erfc(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gcclgamma (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, lgamma(agn_checknumber(L, 1)));
  return 1;
}

/* In DOS & OS/2, calls gamma instead of tgamma */
static int testlib_gcctgamma (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, tgamma(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunfrac (lua_State *L) {
  lua_pushnumber(L, sun_frac(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunsinh (lua_State *L) {
  lua_pushnumber(L, sun_sinh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_suncosh (lua_State *L) {
  lua_pushnumber(L, sun_cosh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_suntanh (lua_State *L) {
  lua_pushnumber(L, sun_tanh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunsech (lua_State *L) {
  lua_pushnumber(L, 1/sun_cosh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_suncsch (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  lua_pushnumber(L, x == 0 ? AGN_NAN : 1/sun_sinh(x));
  return 1;
}

static int testlib_suncoth (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  lua_pushnumber(L, x == 0 ? AGN_NAN : sun_cosh(x)/sun_sinh(x));
  return 1;
}

static int testlib_sunasinh (lua_State *L) {
  lua_pushnumber(L, sun_asinh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunacosh (lua_State *L) {
  lua_pushnumber(L, sun_acosh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunatanh (lua_State *L) {
  lua_pushnumber(L, sun_atanh(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunerf (lua_State *L) {
  lua_pushnumber(L, sun_erf(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunerfc (lua_State *L) {
  lua_pushnumber(L, sun_erfc(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_sunfmod (lua_State *L) {
  lua_pushnumber(L, sun_fmod(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}

static int testlib_isinf (lua_State *L) {  /* 3.7.4, covers +/-infinity */
  lua_pushboolean(L, tools_isinf(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_isnan (lua_State *L) {
  lua_pushboolean(L, tools_isnan(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_isnanorinf (lua_State *L) {
  lua_pushboolean(L, tools_isnanorinf(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_isfinite (lua_State *L) {
  lua_pushboolean(L, tools_isfinite(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_isnonposint (lua_State *L) {
  lua_pushboolean(L, tools_isnonposint(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_isint (lua_State *L) {
  lua_pushboolean(L, tools_isint(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_isfrac (lua_State *L) {
  lua_pushboolean(L, tools_isfrac(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_isnonnegint (lua_State *L) {
  lua_pushboolean(L, tools_isnonnegint(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_gccldexp (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, ldexp(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}

static int testlib_gccfrexp (lua_State *L) {  /* 3.15.1 */
  int e;
  lua_pushnumber(L, frexp(agn_checknumber(L, 1), &e));
  lua_pushnumber(L, e);
  return 2;
}

static int testlib_gccmodf (lua_State *L) {  /* 3.15.1 */
  lua_Number e;
  lua_pushnumber(L, modf(agn_checknumber(L, 1), &e));
  lua_pushnumber(L, e);
  return 2;
}

static int testlib_gccfmod (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, fmod(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}

static int testlib_gccremainder (lua_State *L) {  /* 3.15.1 */
  lua_pushnumber(L, remainder(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}

/* In DOS & OpenSUSE, calls sun_remquo instead of remquo which is not available there. */
static int testlib_gccremquo (lua_State *L) {  /* 3.15.1 */
  int q;
  lua_pushnumber(L, remquo(agn_checknumber(L, 1), agn_checknumber(L, 2), &q));
  lua_pushnumber(L, q);
  return 2;
}


/* testlib_examul and testlib_dekkermul do not seem to give better results than multiplying values primitively */

static int testlib_examul (lua_State *L) {  /* exact multiplication, 3.11.2 */
  lua_Number t = 0;
  lua_pushnumber(L, tools_examul(agn_checknumber(L, 1), agn_checknumber(L, 2), &t));
  lua_pushnumber(L, t);
  return 2;
}


/* The following has been taken from:
   Double it Like Dekker: A Remarkable Technique to Double Floating Point Precision (Part 1):
      https://www.youtube.com/watch?v=6OuqnaHHUG8
   Double it Like Dekker 2: Dekker Multiplication and Division
      https://www.youtube.com/watch?v=5IL1LJ5noww&t=690s
   by Creel.
   Code modified to prevent GCC from optimising it. */

struct DD {
  double upper, lower;
};

static const double SCALE = 134217729;

static struct DD Split (double a) {
  volatile struct DD R;
  volatile double x, p;
  x = a;
  p = a*SCALE;
  R.upper = (x - p) + p;
  R.lower = x - R.upper;
  return R;
}

static struct DD DekkerMul12 (double a, double b) {
  volatile struct DD A, B, R;
  volatile double p, q;
  A = Split(a);
  B = Split(b);
  p = A.upper*B.upper;
  q = A.upper*B.lower + A.lower*B.upper;
  R.upper = p + q;
  R.lower = p - R.upper + q + A.lower*B.lower;
  return R;
}

static struct DD DekkerMul (double a, double b) {
  volatile struct DD A, B, R, T;
  volatile double c;
  A = Split(a);
  B = Split(b);
  T = DekkerMul12(A.upper, B.upper);
  c = A.upper*B.lower + A.lower*B.upper + T.lower;
  R.upper = T.upper + c;
  R.lower = T.upper - R.upper + c;
  return R;
}

static int testlib_dekkermul (lua_State *L) {  /* exact multiplication, 3.11.3 */
  struct DD R;
  R = DekkerMul(luaL_checknumber(L, 1), luaL_checknumber(L, 2));
  lua_pushnumber(L, R.upper);
  lua_pushnumber(L, R.lower);
  return 2;
}


/* @(#)z_sine.c 1.0 98/08/13 */
/* See: https://github.com/eblot/newlib/blob/master/newlib/libm/mathfp/s_sine.c */
/******************************************************************
 * The following routines are coded directly from the algorithms
 * and coefficients given in "Software Manual for the Elementary
 * Functions" by William J. Cody, Jr. and William Waite, Prentice
 * Hall, 1980.
 ******************************************************************/

/*
FUNCTION
        <<sin>>, <<cos>>, <<sine>>, <<sinf>>, <<cosf>>, <<sinef>>---sine or cosine
INDEX
sin
INDEX
cos
SYNOPSIS
        #include <math.h>
        double sin(double <[x]>);
        double cos(double <[x]>);

DESCRIPTION
        <<sin>> and <<cos>> compute (respectively) the sine and cosine
        of the argument <[x]>.  Angles are specified in radians.
RETURNS
        The sine or cosine of <[x]> is returned.

PORTABILITY
        <<sin>> and <<cos>> are ANSI C.

QUICKREF
        sin ansi pure

COMMENT
        This implementation has an error around n*Pi that is 1/3 larger than
        the Sun implementations.
*/

/******************************************************************
 * sine
 *
 * Input:
 *   x - floating point value
 *   cosine - indicates cosine value
 *
 * Output:
 *   Sine of x.
 *
 * Description:
 *   This routine calculates sines and cosines.
 *
 *****************************************************************/

static const double r[] = {
  -0.16666666666666665052,
   0.83333333333331650314e-02,
  -0.19841269841201840457e-03,
   0.27557319210152756119e-05,
  -0.25052106798274584544e-07,
   0.16058936490371589114e-09,
  -0.76429178068910467734e-12,
   0.27204790957888846175e-14
};

static const double z_rooteps = 7.4505859692e-9;

static FORCE_INLINE double sine (double x, int cosine) {
  int sgn, N;
  double y, XN, g, R, res;
  double YMAX = 210828714.0;
  if (isinf(x) || isnan(x)) return x;
  /* Use sin and cos properties to ease computations. */
  if (cosine) {
    sgn = 1;
    y = fabs(x) + PIO2;
  } else {
    if (x < 0.0) {
      sgn = -1;
      y = -x;
    } else {
      sgn = 1;
      y = x;
    }
  }
  /* Check for values of y that will overflow here. */
  if (y > YMAX) return x;
  /* Calculate the exponent. */
  if (y < 0.0)
    N = (int)(y*ONEOPI - 0.5);
  else
    N = (int)(y*ONEOPI + 0.5);
  XN = (double)N;
  if (N & 1) sgn = -sgn;
  if (cosine) XN -= 0.5;
  y = fabs(x) - XN*PI;
  if (-z_rooteps < y && y < z_rooteps)
    res = y;
  else {
    g = y*y;
    /* Calculate the Taylor series. */
    R = ((((((( (r[7])*g + r[6] * g + r[5]) * g + r[4]) * g + r[3]) * g + r[2]) * g + r[1]) * g + r[0]) * g);
    /* Finally, compute the result. */
    res = y + y * R;
  }
  res *= sgn;
  return (res);
}

static int testlib_sine (lua_State *L) {
  lua_pushnumber(L, sine(agn_checknumber(L, 1), 0));
  return 1;
}


static int testlib_cosine (lua_State *L) {
  lua_pushnumber(L, sine(agn_checknumber(L, 1), 1));
  return 1;
}


static int testlib_pythal (lua_State *L) {  /* 4.5.5 */
  lua_pushnumber(L, tools_pythal(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int testlib_mpytha (lua_State *L) {  /* 4.5.6 */
  lua_pushnumber(L, tools_mpytha(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int testlib_sunpytha (lua_State *L) {  /* 4.5.6 */
  lua_pushnumber(L, sun_pytha(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int testlib_invpytha (lua_State *L) {  /* 4.5.6 */
  lua_pushnumber(L, tools_invpytha(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int testlib_toolssignum (lua_State *L) {
  lua_pushnumber(L, tools_signum(agn_checknumber(L, 1)));
  return 1;
}

static int testlib_toolssign (lua_State *L) {
  lua_pushnumber(L, tools_sign(agn_checknumber(L, 1)));
  return 1;
}


static int testlib_isirregular (lua_State *L) {
  lua_pushboolean(L, sun_isirregular(agn_checknumber(L, 1)));
  return 1;
}


#ifdef __i386__

/* 2.8 % slower than sun_sincos on i7-9700K CPU @ 3.60GHz */
static int testlib_x87sincos (lua_State *L) {
  register double co, si, x;
  x = agn_checknumber(L, 1);
  __asm __volatile__
    ("fsincos"
     : "=t" (co), "=u" (si) : "0" (x));
  lua_pushnumber(L, si);
  lua_pushnumber(L, co);
  return 2;
}

/* 3 % slower than sun_sin on i7-9700K CPU @ 3.60GHz */
static int testlib_x87sin (lua_State *L) {
  register double x, r;
  x = agn_checknumber(L, 1);
  __asm __volatile__
    ("fsin"
     : "=t" (r) : "0" (x));
  lua_pushnumber(L, r);
  return 1;
}

#endif


/* String reference functions ****************************************************************/

static int str_strrepl (lua_State *L) {  /* 2.36.1 UNDOC, for testing */
  const char *s, *p, *q;
  char *t;
  size_t s_len;
  s = agn_checklstring(L, 1, &s_len);
  p = agn_checkstring(L, 2);
  q = agn_checkstring(L, 3);
  t = tools_strndup(s, s_len);
  str_charreplace(t, p[0], q[0], 0);
  lua_pushlstring(L, t, s_len);
  xfree(t);  /* 2.39.10 */
  return 1;
}


static int str_strrev (lua_State *L) {  /* 5.4.2 UNDOC, for testing */
  const char *s;
  char *t;
  size_t l;
  s = agn_checklstring(L, 1, &l);
  t = str_reverse(s, l);
  lua_pushlstring(L, t, l);
  xfree(t);
  return 1;
}


static int str_gccmemset (lua_State *L) {  /* 2.37.8 UNDOC, for testing */
  int val = agnL_optnonnegint(L, 1, 0);
  size_t n = agnL_optnonnegint(L, 2, 64);
  char *p = malloc(n * CHARSIZE);
  if (!p) luaL_error(L, "memalloc error");
	memset(p, val, n);
	lua_pushlstring(L, p, n);
  xfree(p);
  return 1;
}


static int str_memset (lua_State *L) {  /* 2.36.1 UNDOC, for testing */
  int val = agnL_optnonnegint(L, 1, 0);
  size_t n = agnL_optnonnegint(L, 2, 64);
  char *p = malloc(n * CHARSIZE);
  if (!p) luaL_error(L, "memalloc error");
	memset(p, val, n);
	lua_pushlstring(L, p, n);
  xfree(p);
  return 1;
}


#if defined(__GNUC__) && defined(__INTEL)
static int str_asmset (lua_State *L) {  /* 2.36.1 UNDOC, for testing */
  int val = agnL_optnonnegint(L, 1, 0);
  size_t n = agnL_optnonnegint(L, 2, 64);
  char *p = malloc(n * CHARSIZE);
  if (!p) luaL_error(L, "memalloc error");
	asm_memset(p, val, n);
	lua_pushlstring(L, p, n);
  xfree(p);
  return 1;
}
#endif


static int str_gccmemcmp (lua_State *L) {  /* 2.36.1 UNDOC, for testing */
  const char *s, *p;
  size_t l, n;
  s = agn_checklstring(L, 1, &l);
  p = agn_checkstring(L, 2);
  n = agnL_optnonnegint(L, 3, l);
  if (n > l) n = l;
	lua_pushinteger(L, memcmp(s, p, n));
  return 1;
}


static int str_memcmp (lua_State *L) {  /* 2.36.1 UNDOC, for testing */
  const char *s, *p;
  size_t l, n;
  s = agn_checklstring(L, 1, &l);
  p = agn_checkstring(L, 2);
  n = agnL_optnonnegint(L, 3, l);
  if (n > l) n = l;
	lua_pushinteger(L, tools_memcmp(s, p, n));
  return 1;
}


static int str_hasstrchr (lua_State *L) {  /* 2.39.11 UNDOC */
  size_t l, n;
  const char *s = agn_checklstring(L, 1, &l);
  const char *p = agn_checkstring(L, 2);
  n = agnL_optposint(L, 3, l);
  lua_pushboolean(L, tools_hasstrchr(s, p, n));
  return 1;
}


static int str_strsep (lua_State *L) {  /* 2.39.12 UNDOC; not faster than strings.fields with 1-char delimiters */
  size_t l, n, c;
  char *token;
  const char *s = agn_checklstring(L, 1, &l);
  const char *delim = luaL_optstring(L, 2, " ");
  char *str = tools_strndup(s, l);  /* the string will be modified, so duplicate it */
  agn_createseq(L, 0);
  c = 0;
  while ( ((token = tools_strsep(&str, delim, &n)) != NULL)) {
    lua_pushlstring(L, token, n);
    lua_seqrawseti(L, -2, ++c);
  }
  if (c == 0) {
    agn_poptop(L);
    lua_pushnil(L);
  }
  xfree(str);
  return 1;
}


static int str_gccstrlen (lua_State *L) {  /* 5.5.9 UNDOC */
  lua_pushinteger(L, strlen(agn_checkstring(L, 1)));
  return 1;
}


/* This is as fast as GNU's strlen() */
#define STRLEN_ALIGN (sizeof(size_t))
#define STRLEN_ONES ((size_t)-1/UCHAR_MAX)
#define STRLEN_HIGHS ((STRLEN_ONES) * (UCHAR_MAX/2+1))
#define STRLEN_HASZERO(x) ((x)-(STRLEN_ONES & ~(x) & STRLEN_HIGHS))

size_t ulysses_strlen (const char *s) {
	const char *a = s;
	const size_t *w;
	for (; (uintptr_t)s % STRLEN_ALIGN; s++) if (!*s) return s - a;
	for (w = (const void *)s; !STRLEN_HASZERO(*w); w++);
	for (s = (const void *)w; *s; s++);
	return s - a;
}


static int str_ustrlen (lua_State *L) {  /* 5.5.9 UNDOC */
  lua_pushinteger(L, ulysses_strlen(agn_checkstring(L, 1)));
  return 1;
}


static int str_cstrcmp (lua_State *L) {  /* 2.18.6 UNDOC */
  const char *s, *t;
  s = agn_checkstring(L, 1);
  t = agn_checkstring(L, 2);
  if (agnL_optboolean(L, 3, 1))  /* true ? */
    lua_pushinteger(L, tools_strcmp(s, t));
  else  /* false */
    lua_pushinteger(L, strcmp(s, t));
  return 1;
}


static int str_strmatch (lua_State *L) {  /* 2.36.1 UNDOC, for testing */
  const char *s, *p;
  ptrdiff_t init, start, end;
  size_t s_len;
  s = agn_checklstring(L, 1, &s_len);
  p = agn_checkstring(L, 2);
  init = agnL_optposint(L, 3, 1);
  lua_pushstring(L, agn_strmatch(L, s, s_len, p, init, &start, &end));
  lua_pushinteger(L, start);
  lua_pushinteger(L, end);
  return 3;
}

static char *tools_twobyte_memmem (const unsigned char *h, size_t k, const unsigned char *n) {
  uint16_t nw = n[0] << 8 | n[1], hw = h[0] << 8 | h[1];
  for (h++, k--; k; k--, hw = hw << 8 | *++h)
    if (hw == nw) return (char *)h - 1;
  return 0;
}

static char *tools_threebyte_memmem (const unsigned char *h, size_t k, const unsigned char *n) {
  uint32_t nw = n[0] << 24 | n[1] << 16 | n[2] << 8;
  uint32_t hw = h[0] << 24 | h[1] << 16 | h[2] << 8;
  for (h+=2, k-=2; k; k--, hw = (hw | *++h) << 8)
    if (hw == nw) return (char *)h - 2;
  return 0;
}

static char *tools_fourbyte_memmem (const unsigned char *h, size_t k, const unsigned char *n) {
  uint32_t nw = n[0] << 24 | n[1] << 16 | n[2] << 8 | n[3];
  uint32_t hw = h[0] << 24 | h[1] << 16 | h[2] << 8 | h[3];
  for (h+=3, k-=3; k; k--, hw = hw << 8 | *++h)
    if (hw == nw) return (char *)h - 3;
  return 0;
}

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define BITOP(a,b,op) \
 ((a)[(size_t)(b)/(8*sizeof *(a))] op (size_t)1<<((size_t)(b)%(8*sizeof *(a))))

static char *tools_twoway_memmem (const unsigned char *h, const unsigned char *z, const unsigned char *n, size_t l) {
  size_t i, ip, jp, k, p, ms, p0, mem, mem0;
  size_t byteset[32/sizeof(size_t)] = { 0 };
  size_t shift[256];
  /* Computing length of needle and fill shift table */
  for (i=0; i < l; i++)
    BITOP(byteset, n[i], |=), shift[n[i]] = i + 1;
  /* Compute maximal suffix */
  ip = -1; jp = 0; k = p = 1;
  while (jp + k < l) {
    if (n[ip + k] == n[jp + k]) {
      if (k == p) {
        jp += p;
        k = 1;
      } else k++;
    } else if (n[ip + k] > n[jp + k]) {
      jp += k;
      k = 1;
      p = jp - ip;
    } else {
      ip = jp++;
      k = p = 1;
    }
  }
  ms = ip;
  p0 = p;
  /* And with the opposite comparison */
  ip = -1; jp = 0; k = p = 1;
  while (jp + k < l) {
    if (n[ip + k] == n[jp + k]) {
      if (k == p) {
        jp += p;
        k = 1;
      } else k++;
    } else if (n[ip + k] < n[jp + k]) {
      jp += k;
      k = 1;
      p = jp - ip;
    } else {
      ip = jp++;
      k = p = 1;
    }
  }
  if (ip + 1 > ms + 1) ms = ip;
  else p = p0;
  /* Periodic needle? */
  if (memcmp(n, n + p, ms + 1)) {
    mem0 = 0;
    p = MAX(ms, l - ms - 1) + 1;
  } else mem0 = l - p;
  mem = 0;
  /* Search loop */
  for (;;) {
    /* If remainder of haystack is shorter than needle, done */
    if (z - h < l) return 0;
    /* Check last byte first; advance by shift on mismatch */
    if (BITOP(byteset, h[l - 1], &)) {
      k = l - shift[h[l - 1]];
      if (k) {
        if (mem0 && mem && k < p) k = l - p;
        h += k;
        mem = 0;
        continue;
      }
    } else {
      h += l;
      mem = 0;
      continue;
    }
    /* Compare right half */
    for (k=MAX(ms + 1, mem); n[k] && n[k] == h[k]; k++);
    if (n[k]) {
      h += k - ms;
      mem = 0;
      continue;
    }
    /* Compare left half */
    for (k=ms + 1; k > mem && n[k - 1] == h[k - 1]; k--);
    if (k == mem) return (char *)h;
    h += p;
    mem = mem0;
  }
}

void *tools_memmem (const void *h0, size_t k, const void *n0, size_t l) {
  const unsigned char *h = h0, *n = n0;
  /* Return immediately on empty needle */
  if (!l) return (void *)h;
  /* Return immediately when needle is longer than haystack */
  if (k < l) return 0;
  /* Use faster algorithms for short needles */
  h = tools_memchr(h0, *n, k);
  if (!h || l == 1) return (void *)h;
  if (l == 2) return tools_twobyte_memmem(h, k, n);
  if (l == 3) return tools_threebyte_memmem(h, k, n);
  if (l == 4) return tools_fourbyte_memmem(h, k, n);
  return tools_twoway_memmem(h, h + k, n, l);
}


static int str_issubstring (lua_State *L) {  /* 5.5.9 UNDOC */
  const char *s, *t;
  size_t l1, l2;
  s = agn_checklstring(L, 1, &l1);
  t = agn_checklstring(L, 2, &l2);
  if (agnL_optboolean(L, 3, 1))  /* true ? */
    lua_pushboolean(L, tools_lmemfind(s, l1, t, l2) != NULL);
  else
    lua_pushboolean(L, tools_memmem(s, l1, t, l2) != NULL);
  return 1;
}


/* SplitMix64 based hash */
static uint64_t dcf_hash (double val) {
  uint64_t u;
  val += 0.0;
  memcpy(&u, &val, sizeof(u));
  u = (u^(u >> 30))*0xbf58476d1ce4e5b9ULL;
  u = (u^(u >> 27))*0x94d049bb133111ebULL;
  return u^(u >> 31);
}

static uint64_t dcf_hash_alt (double val) {
  uint64_t u;
  /* Normalize -0.0 to 0.0 */
  val += 0.0;
  union {
    double d;
    uint64_t u;
  } pun;
  pun.d = val;
  u = pun.u;
  /* SplitMix64 */
  u = (u ^ (u >> 30)) * 0xbf58476d1ce4e5b9ULL;
  u = (u ^ (u >> 27)) * 0x94d049bb133111ebULL;
  return u ^ (u >> 31);
}


static uint32_t ddhash_hash (uint64_t h, uint32_t *lx) {
  uint32_t hx;
  hx = (uint32_t)(h >> 32);          /* high */
  *lx = (uint32_t)(h & 0xFFFFFFFFU);  /* low */
  return hx;
}


static int testlib_punning (lua_State *L) {
  uint64_t h;
  uint32_t hx, lx;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_Number x = agn_checknumber(L, 1);
  int old = agnL_optboolean(L, 2, 1);
  h = (old) ? dcf_hash(x) : dcf_hash_alt(x);
  hx = ddhash_hash(h, &lx);
  lua_pushinteger(L, hx);
  lua_pushinteger(L, lx);
  return 2;
}


static int testlib_settop (lua_State *L) {
  int top = agn_checkinteger(L, 1);
  lua_settop(L, top);
  return top;
}


static const luaL_Reg testlib[] = {
  {"punning", testlib_punning},
  /* Math reference functions */
  {"dekkermul", testlib_dekkermul},
  {"examul",   testlib_examul},
  {"exp10_9",  testlib_exp10_9},
  {"exp2_9",   testlib_exp2_9},
  {"gccacos",  testlib_gccacos},
  {"gccacosh", testlib_gccacosh},
  {"gccasin",  testlib_gccasin},
  {"gccasinh", testlib_gccasinh},
  {"gccatan",  testlib_gccatan},
  {"gccatanh", testlib_gccatanh},
  {"gcccbrt",  testlib_gcccbrt},
  {"gccceil",  testlib_gccceil},
  {"gcccos",   testlib_gcccos},
  {"gcccosh",  testlib_gcccosh},
  {"gccerf",   testlib_gccerf},
  {"gccerfc",  testlib_gccerfc},
  {"gccexp",   testlib_gccexp},
  {"gccexp2",  testlib_gccexp2},
  {"gccexpm1", testlib_gccexpm1},
  {"gccfloor", testlib_gccfloor},
  {"gccfmod",  testlib_gccfmod},
  {"gccfrexp", testlib_gccfrexp},
  {"gcchypot", testlib_gcchypot},
  {"gccilogb", testlib_gccilogb},
  {"gccldexp", testlib_gccldexp},
  {"gcclgamma",testlib_gcclgamma},
  {"gcclog",   testlib_gcclog},
  {"gcclog1p", testlib_gcclog1p},
  {"gcclog2",  testlib_gcclog2},
  {"gcclog10", testlib_gcclog10},
  {"gccmodf",  testlib_gccmodf},
  {"gccpow",   testlib_gccpow},
  {"gccrecip", testlib_gccrecip},
  {"gccremainder", testlib_gccremainder},
  {"gccremquo", testlib_gccremquo},
  {"gccround", testlib_gccround},
  {"gccrsqrt", testlib_gccrsqrt},
  {"gccrsqrt", testlib_gccrsqrt},
  {"gccsin",   testlib_gccsin},
  {"gccsinh",  testlib_gccsinh},
  {"gccsqrt",  testlib_gccsqrt},
  {"gcctan",   testlib_gcctan},
  {"gcctanh",  testlib_gcctanh},
  {"gccltamma",testlib_gcctgamma},
  {"gcctrunc", testlib_gcctrunc},
  {"isint",    testlib_isint},
  {"sunacosh", testlib_sunacosh},
  {"sunasinh", testlib_sunasinh},
  {"sunatanh", testlib_sunatanh},
  {"suncos",   testlib_suncos},
  {"suncosh",  testlib_suncosh},
  {"suncoth",  testlib_suncoth},
  {"suncsch",  testlib_suncsch},
  {"sunerf",   testlib_sunerf},
  {"sunerfc",  testlib_sunerfc},
  {"sunexp",   testlib_sunexp},
  {"sunexp10", testlib_sunexp10},
  {"sunexp2",  testlib_sunexp2},
  {"sunfmod",  testlib_sunfmod},
  {"sunfrac",  testlib_sunfrac},
  {"sunlog",   testlib_sunlog},
  {"sunlog10", testlib_sunlog10},
  {"sunlog1p", testlib_sunlog1p},
  {"sunlog2",  testlib_sunlog2},
  {"sunpow",   testlib_sunpow},
  {"sunsech",  testlib_sunsech},
  {"sunsin",   testlib_sunsin},
  {"sunsincos", testlib_sunsincos},
  {"sunsinhcosh", testlib_sunsinhcosh},
  {"sunsinh",  testlib_sunsinh},
  {"sunsqrt",  testlib_sunsqrt},
  {"suntan",   testlib_suntan},
  {"suntanh",  testlib_suntanh},
  {"isfrac",   testlib_isfrac},
  {"isinf",    testlib_isinf},
  {"isint",    testlib_isint},
  {"isnan",    testlib_isnan},
  {"isnanorinf", testlib_isnanorinf},
  {"isnonnegint", testlib_isnonnegint},
  {"isnonposint", testlib_isnonposint},
  {"isint", testlib_isint},
  {"isfinite", testlib_isfinite},
  {"isirregular", testlib_isirregular},
  {"sine", testlib_sine},                 /* added on April 29, 2024 */
  {"cosine", testlib_cosine},             /* added on April 29, 2024 */
  {"mpytha", testlib_mpytha},
  {"pythal", testlib_pythal},
  {"sunpytha", testlib_sunpytha},
  {"invpytha", testlib_invpytha},
  {"toolssignum", testlib_toolssignum},
  {"toolssign", testlib_toolssign},
#ifdef __i386__
  {"x87sincos", testlib_x87sincos},
  {"x87sin",   testlib_x87sin},
#endif
  /* String reference functions */
#if defined(__GNUC__) && defined(__INTEL)
  {"asmset", str_asmset},
#endif
  {"cstrcmp", str_cstrcmp},               /* added on June 15, 2022 */
  {"hasstrchr", str_hasstrchr},           /* added on June 05, 202e */
  {"issubstring", str_issubstring},       /* added on September 03, 2025 */
  {"gccmemcmp", str_gccmemcmp},
  {"gccmemset", str_gccmemset},
  {"gccstrlen", str_gccstrlen},           /* added on September 03, 2025 */
  {"memcmp", str_memcmp},
  {"memset", str_memset},
  {"settop", testlib_settop},
  {"strmatch", str_strmatch},             /* added on January 28, 2023 */
  {"strrepl", str_strrepl},
  {"strsep", str_strsep},                 /* added on June 08, 2023 */
  {"strrev", str_strrev},                 /* added on June 08, 2023 */
  {"ustrlen", str_ustrlen},               /* added on September 03, 2025 */
  {NULL, NULL}
};


/*
** Open testlib library
*/
LUALIB_API int luaopen_testlib (lua_State *L) {
  luaL_register(L, AGENA_TESTLIBLIBNAME, testlib);
  return 1;
}

