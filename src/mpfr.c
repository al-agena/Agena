/*
** $Id: mpfr.c, initiated August 21, 2020 $
** GNU Multiple Precision Floating-Point Reliable Library (MPFR) binding
** See Copyright Notice in agena.h
**
** ATTENTION:
** 1) Always use MPFR_RNDN in intermediate MPFR function calls.
** 2) Use MPFR_ROUNDING in the finalising call MPFR function call, so that the user's rounding setting is regarded.
*/

#define AGENA_LIBVERSION	"mpfr 2.1.1 for Agena as of April 19, 2026\n"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <mpfr.h>
#include <gmp.h>

#define mpflib_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnxlib.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_MPFRLIBNAME "mpfr"  /* change linalg.c if you change that name */
LUALIB_API int (luaopen_mpf) (lua_State *L);
#endif


/* Default precision, can be changed by calling `mpfr.precision`. */
static int MPFR_PRECISION = 128;
/* Extra precision, recommended by Gemini AI, 8 instead of 10, to make up for round-off errors,
   in composite functions and loops: */
#define EXTRABITS   ((mpfr_prec_t)8)
/* Round to nearest, with ties to even, 6.6.2 change */
static mpfr_rnd_t MPFR_ROUNDING = MPFR_RNDN;

gmp_randstate_t randomstate;

#define MPFR_NULL                 ((mpfr_ptr)0)

#define Mpfr_t                    mpfr_t
#define Mpfr_init                 mpfr_init
#define Mpfr_set(mr,mx)           mpfr_set(mr, mx, MPFR_ROUNDING)
#define Mpfr_set_d(mx,d)          mpfr_set_d(mx, d, MPFR_ROUNDING)
#define Mpfr_set_str(mx,s)        mpfr_set_str(mx, s, 10, MPFR_ROUNDING)
#define Mpfr_init_set_d(mx,x)     mpfr_init_set_d(mx, x, MPFR_ROUNDING)
#define Mpfr_init_set_str(mx,x)   mpfr_init_set_str(mx, x, 10, MPFR_ROUNDING)
#define Mpfr_init2_d(mr,x,prec) { \
  mpfr_init2(mr, prec); \
  Mpfr_set_d(mr, x); \
}
#define Mpfr_get_d(mx)            mpfr_get_d(mx, MPFR_ROUNDING)
#define Mpfr_clear(mx)            mpfr_clear(mx)
#define Mpfr_clears               mpfr_clears
#define Mpfr_cmp                  mpfr_cmp
#define Mpfr_set_default_prec     mpfr_set_default_prec(MPFR_PRECISION)
#define Mpfr_iszero(mx)           (mpfr_zero_p(mx) != 0)
#define Mpfr_isnan                MPFR_IS_NAN
#define Mpfr_isneg                MPFR_IS_NEG
#define Mpfr_issingular           MPFR_IS_SINGULAR  /* NaN, Infinity, Zero */

#define Mpfr_add(mr, mx, my)      mpfr_add(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_sub(mr, mx, my)      mpfr_sub(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_mul(mr, mx, my)      mpfr_mul(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_div(mr, mx, my)      mpfr_div(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_recip(mr, mx)        mpfr_ui_div(mr, 1, mx, MPFR_ROUNDING)
#define mpfr_recip(mr, mx, rnd)   mpfr_ui_div(mr, 1, mx, rnd)
#define Mpfr_pow(mr, mx, my)      mpfr_pow(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_sqr(mr, mx)          mpfr_sqr(mr, mx, MPFR_ROUNDING)
#define Mpfr_copysign(mr, mx, my) mpfr_copysign(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_fma(mr, mx, my, mz)  mpfr_fma(mr, mx, my, mz, MPFR_ROUNDING)
#define Mpfr_fms(mr, mx, my, mz)  mpfr_fms(mr, mx, my, mz, MPFR_ROUNDING)

#define Mpfr_neg(mr, mx)          mpfr_neg(mr, mx, MPFR_ROUNDING)
#define Mpfr_abs(mr, mx)          mpfr_abs(mr, mx, MPFR_ROUNDING)

#define Mpfr_sqrt(mr, mx)         mpfr_sqrt(mr, mx, MPFR_ROUNDING)
#define Mpfr_rec_sqrt(mr,mx)      mpfr_rec_sqrt(mr, mx, MPFR_ROUNDING)
#define Mpfr_cbrt(mr, mx)         mpfr_cbrt(mr, mx, MPFR_ROUNDING)
#define Mpfr_log(mr, mx)          mpfr_log(mr, mx, MPFR_ROUNDING)
#define Mpfr_log2(mr, mx)         mpfr_log2(mr, mx, MPFR_ROUNDING)
#define Mpfr_log10(mr, mx)        mpfr_log10(mr, mx, MPFR_ROUNDING)
#define Mpfr_exp(mr, mx)          mpfr_exp(mr, mx, MPFR_ROUNDING)
#define Mpfr_exp2(mr, mx)         mpfr_exp2(mr, mx, MPFR_ROUNDING)
#define Mpfr_exp10(mr, mx)        mpfr_exp10(mr, mx, MPFR_ROUNDING)

#define Mpfr_sin(mr, mx)          mpfr_sin(mr, mx, MPFR_ROUNDING)
#define Mpfr_cos(mr, mx)          mpfr_cos(mr, mx, MPFR_ROUNDING)
#define Mpfr_sincos(ms, mc, mx)   mpfr_sin_cos(ms, mc, mx, MPFR_ROUNDING)
#define Mpfr_tan(mr, mx)          mpfr_tan(mr, mx, MPFR_ROUNDING)
#define Mpfr_sec(mr, mx)          mpfr_sec(mr, mx, MPFR_ROUNDING)
#define Mpfr_csc(mr, mx)          mpfr_csc(mr, mx, MPFR_ROUNDING)
#define Mpfr_cot(mr, mx)          mpfr_cot(mr, mx, MPFR_ROUNDING)
#define Mpfr_asin(mr, mx)         mpfr_asin(mr, mx, MPFR_ROUNDING)
#define Mpfr_acos(mr, mx)         mpfr_acos(mr, mx, MPFR_ROUNDING)
#define Mpfr_atan(mr, mx)         mpfr_atan(mr, mx, MPFR_ROUNDING)
#define Mpfr_atan2(mr, mx, my)    mpfr_atan2(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_dim(mr, mx, my)      mpfr_dim(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_hypot(mr, mx, my)    mpfr_hypot(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_modf(mr, mx, my)     mpfr_modf(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_fmod(mr, mx, my)     mpfr_fmod(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_sinh(mr, mx)         mpfr_sinh(mr, mx, MPFR_ROUNDING)
#define Mpfr_cosh(mr, mx)         mpfr_cosh(mr, mx, MPFR_ROUNDING)
#define Mpfr_tanh(mr, mx)         mpfr_tanh(mr, mx, MPFR_ROUNDING)
#define Mpfr_sech(mr, mx)         mpfr_sech(mr, mx, MPFR_ROUNDING)
#define Mpfr_csch(mr, mx)         mpfr_csch(mr, mx, MPFR_ROUNDING)
#define Mpfr_coth(mr, mx)         mpfr_coth(mr, mx, MPFR_ROUNDING)
#define Mpfr_acosh(mr, mx)        mpfr_acosh (mr, mx, MPFR_ROUNDING)
#define Mpfr_asinh(mr, mx)        mpfr_asinh (mr, mx, MPFR_ROUNDING)
#define Mpfr_atanh(mr, mx)        mpfr_atanh (mr, mx, MPFR_ROUNDING)

#define Mpfr_ceil(mr, mx)         mpfr_ceil(mr, mx)
#define Mpfr_trunc(mr, mx)        mpfr_trunc(mr, mx)
#define Mpfr_floor(mr, mx)        mpfr_floor(mr, mx)
#define Mpfr_round(mr, mx)        mpfr_round(mr, mx)

#define Mpfr_min(mr,mx,my)        mpfr_min(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_max(mr,mx,my)        mpfr_max(mr, mx, my, MPFR_ROUNDING)
#define Mpfr_reldiff(mr,mx,my)    mpfr_reldiff(mr, mx, my, MPFR_ROUNDING)

#define Mpfr_eint(mr,mx)          mpfr_eint(mr,mx,MPFR_ROUNDING)
#define Mpfr_li2(mr,mx)           mpfr_li2(mr,mx,MPFR_ROUNDING)
#define Mpfr_gamma(mr,mx)         mpfr_gamma(mr,mx,MPFR_ROUNDING)
#define Mpfr_lngamma(mr,mx)       mpfr_lngamma(mr,mx,MPFR_ROUNDING)
#define Mpfr_digamma(mr,mx)       mpfr_digamma(mr,mx,MPFR_ROUNDING)
#define Mpfr_beta(mr,mx,my)       mpfr_beta(mr,mx,my,MPFR_ROUNDING)
#define Mpfr_zeta(mr,mx)          mpfr_zeta(mr,mx,MPFR_ROUNDING)
#define Mpfr_erf(mr,mx)           mpfr_erf(mr,mx,MPFR_ROUNDING)
#define Mpfr_erfc(mr,mx)          mpfr_erfc(mr,mx,MPFR_ROUNDING)
#define Mpfr_j0(mr,mx)            mpfr_j0(mr,mx,MPFR_ROUNDING)
#define Mpfr_j1(mr,mx)            mpfr_j1(mr,mx,MPFR_ROUNDING)
#define Mpfr_jn(mr,mx,n)          mpfr_jn(mr,n,mx,MPFR_ROUNDING)
#define Mpfr_y0(mr,mx)            mpfr_y0(mr,mx,MPFR_ROUNDING)
#define Mpfr_y1(mr,mx)            mpfr_y1(mr,mx,MPFR_ROUNDING)
#define Mpfr_yn(mr,mx,n)          mpfr_yn(mr,n,mx,MPFR_ROUNDING)
#define Mpfr_agm(mr,mx,my)        mpfr_agm(mr,mx,my,MPFR_ROUNDING)
#define Mpfr_ai(mr,mx)            mpfr_ai(mr,mx,MPFR_ROUNDING)

#define Mpfr_const_log2(mr)       mpfr_const_log2(mr, MPFR_ROUNDING)
#define Mpfr_const_pi(mr)         mpfr_const_pi(mr, MPFR_ROUNDING)
#define Mpfr_const_euler(mr)      mpfr_const_euler(mr, MPFR_ROUNDING)
#define Mpfr_const_catalan(mr)    mpfr_const_catalan(mr, MPFR_ROUNDING)

#define Mpfr_set_nan(mr)          mpfr_set_nan(mr)
#define Mpfr_setnan               MPFR_SET_NAN
#define Mpfr_setinf               MPFR_SET_INF
#define Mpfr_set_inf(mr,s)        mpfr_set_inf(mr,s)
#define Mpfr_setzero              MPFR_SET_ZERO
#define Mpfr_set_zero(mr,s)       mpfr_set_zero(mr,s)
#define Mpfr_set_one(mr)          mpfr_set_ui(mr, 1, MPFR_ROUNDING)
#define Mpfr_swap(mx, my)         mpfr_swap(mx, my)
#define Mpfr_get_str              mpfr_get_str

#define Mpfr_cfinalise(z) { \
  mpfr_set(z->real, z->real, MPFR_ROUNDING); \
  mpfr_set(z->imag, z->imag, MPFR_ROUNDING); \
}

#ifdef LUA
#define agn_isnumber              lua_isnumber
#define agn_tonumber              lua_tonumber
#define agn_isstring              lua_isstring
#define agn_tostring              lua_tostring
#define agn_checkinteger          luaL_checkinteger
#define agn_checknumber           luaL_checknumber
#endif

typedef struct Mpfr {
  Mpfr_t val;
} Mpfr;

typedef struct CMpfr {
  Mpfr_t real;
  Mpfr_t imag;
} CMpfr;

#define creatempf(x) { \
  x = (Mpfr *)lua_newuserdata(L, sizeof(Mpfr)); \
  lua_setmetatabletoobject(L, -1, "mpfr", 1); \
  Mpfr_init(x->val); \
}

#define createcmpf(x) { \
  x = (CMpfr *)lua_newuserdata(L, sizeof(CMpfr)); \
  lua_setmetatabletoobject(L, -1, "cmpfr", 1); \
  Mpfr_init(x->real); \
  Mpfr_init(x->imag); \
}

/* #define checkmpfr(L, n)            (Mpfr *)luaL_checkudata(L, n, "mpfr") */

Mpfr INLINE *checkmpfr (lua_State *L, int n) {
  if (agn_isnumber(L, n)) {
    Mpfr *mx;
    creatempf(mx);
    Mpfr_set_d(mx->val, agn_tonumber(L, n));
    lua_replace(L, n);  /* move (and pop) */
  }
  return (Mpfr *)luaL_checkudata(L, n, "mpfr");
}

#define checkcmpfr(L, n)            (CMpfr *)luaL_checkudata(L, n, "cmpfr")

#define ismpf(L,idx)  (luaL_isudata(L, idx, "mpfr"))
#define iscmpf(L,idx) (luaL_isudata(L, idx, "cmpfr"))

static void mpfr_csgn (mpfr_t rr, mpfr_t a, mpfr_t b);
static void mpfr_carcsin (mpfr_t rx, mpfr_t ry, mpfr_t a, mpfr_t b, mpfr_prec_t prec);
static void mpfr_carcsinh (mpfr_t rx, mpfr_t ry, mpfr_t a, mpfr_t b, mpfr_prec_t prec);
static void mpfr_carctan (mpfr_t rx, mpfr_t ry, mpfr_t a, mpfr_t b, mpfr_prec_t prec);

/* Creates an MPFR floating-point object (mpfr_t MPFR userdata object) from a number, or a string str representing a number.
   See also: `mp.setstring`. */
static int Mpf_new (lua_State *L) {
  int nargs = lua_gettop(L);
  if (nargs == 1) {
    Mpfr *mr;
    if (agn_isnumber(L, 1)) {
      lua_Number x = agn_tonumber(L, 1);
      creatempf(mr);
      if (isnan(x)) {  /* 3.3.7 fix */
        Mpfr_set_nan(mr->val);
      } else if (isinf(x)) {
        Mpfr_set_inf(mr->val, tools_sign(x));
      } else {
        Mpfr_set_d(mr->val, x);
      }
    } else if (agn_isstring(L, 1)) {  /* mpfr_init_set_* crashes in Windows with mpfr-3.1.5 */
      const char *str = agn_tostring(L, 1);
      creatempf(mr);  /* first scan args, then push userdata */
      if (Mpfr_set_str(mr->val, str) == -1)
        luaL_error(L, "Error in " LUA_QS ": allocation failed, probably due to a non-numeric string.", "mpfr.new");
    } else if (ismpf(L, 1) || iscmpf(L, 1)) {  /* 6.5.5/15 extension */
      lua_settop(L, 1);
      /* return 1; */
    } else {
      luaL_error(L, "Error in " LUA_QS ": expected a number, string or MPFR value, got %s.", "mpfr.new", luaL_typename(L, 1));
    }
  } else if (nargs == 2) {
    CMpfr *c;
    if (agn_isnumber(L, 1) && agn_isnumber(L, 2)) {  /* 6.5.15 extension */
      lua_Number re = agn_tonumber(L, 1);
      lua_Number im = agn_tonumber(L, 2);
      createcmpf(c);
      if (isnan(re)) {
        Mpfr_set_nan(c->real);
      } else if (isinf(re)) {
        Mpfr_set_inf(c->real, tools_sign(re));
      } else {
        Mpfr_set_d(c->real, re);
      }
      if (isnan(im)) {
        Mpfr_set_nan(c->imag);
      } else if (isinf(im)) {
        Mpfr_set_inf(c->imag, tools_sign(im));
      } else {
        Mpfr_set_d(c->imag, im);
      }
    } else if (agn_isstring(L, 1) && agn_isstring(L, 2)) {
      const char *strre = agn_tostring(L, 1);
      const char *strim = agn_tostring(L, 2);
      createcmpf(c);  /* first scan args, then push userdata */
      if (Mpfr_set_str(c->real, strre) == -1)
        luaL_error(L, "Error in " LUA_QS ": allocation failed, probably due to a non-numeric string.", "mpfr.new");
      if (Mpfr_set_str(c->imag, strim) == -1)
        luaL_error(L, "Error in " LUA_QS ": allocation failed, probably due to a non-numeric string.", "mpfr.new");
    } else if (ismpf(L, 1) && ismpf(L, 2)) {
      CMpfr *c;
      Mpfr *re = lua_touserdata(L, 1);
      Mpfr *im = lua_touserdata(L, 2);
      createcmpf(c);
      mpfr_set_prec(c->real, mpfr_get_prec(re->val));  /* copy precision */
      mpfr_set(c->real, re->val, MPFR_ROUNDING);
      mpfr_set_prec(c->imag, mpfr_get_prec(im->val));
      mpfr_set(c->imag, im->val, MPFR_ROUNDING);
    } else {
      luaL_error(L, "Error in " LUA_QS ": expected two numbers strings or MPFR values.", "mpfr.new");
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": need one or two arguments.", "mpfr.new");
  }
  return 1;  /* leave userdata on the top of the stack */
}


/* Clones an MPFR value and returns it. The rounding mode of the MPFR value returned will be the current one, not necessarily
   the one with which the value to be duplicated has been created. Extended 7.5.1 to use a different precision. */
static int Mpf_clone (lua_State *L) {
  int nargs = lua_gettop(L);
  if (ismpf(L, 1)) {
    Mpfr *a, *r;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(r);
    if (nargs == 1) {
      mpfr_set_prec(r->val, mpfr_get_prec(a->val));  /* copy precision */
    } else {
      mpfr_prec_t prec = agn_checkposint(L, 2);
      if (prec < 2) prec = 2;
      mpfr_set_prec(r->val, prec);
    }
    mpfr_set(r->val, a->val, MPFR_ROUNDING);
  } else if (iscmpf(L, 1)) {  /* 6.5.16 extension */
    CMpfr *a, *r;
    a = (CMpfr *)lua_touserdata(L, 1);
    createcmpf(r);
    if (nargs == 1) {
      mpfr_set_prec(r->real, mpfr_get_prec(a->real));  /* copy precision */
      mpfr_set_prec(r->imag, mpfr_get_prec(a->imag));  /* copy precision */
    } else {
      mpfr_prec_t prec = agn_checkposint(L, 2);
      if (prec < 2) prec = 2;
      mpfr_set_prec(r->real, prec);
      mpfr_set_prec(r->imag, prec);
    }
    mpfr_set(r->real, a->real, MPFR_ROUNDING);
    mpfr_set(r->imag, a->imag, MPFR_ROUNDING);
    if (nargs == 1) {
      mpfr_set_prec(r->real, mpfr_get_prec(a->real));  /* copy precision */
      mpfr_set_prec(r->imag, mpfr_get_prec(a->imag));  /* copy precision */
    } else {

    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid mpf object.", "mpfr.clone");
  }
  return 1;
}

/* Generated by Gemini AI, patched, put into the public domain
 * Checks if an mpfr_t variable representing an integer is odd.
 * Assumes 'x' holds an exact integer value.
 * * @param x The mpfr_t number to check.
 * @return 1 if odd, 0 if even, -1 for non-finite or non-integer input.
 */
static int mpfr_isodd (mpfr_t x) {
  int isodd;
  /* 1. Check for zero as mpfr_regular_p(0) return 0. Handle the zero case (0 is even) */
  if (Mpfr_iszero(x)) return 0;  /* Even */
  /* 2. Check for valid integer input */
  if (mpfr_regular_p(x) == 0 || mpfr_integer_p(x) == 0) return -1;  /* Not a finite, exact integer */
  /* 3. Setup temporary variables for calculation */
  mpfr_t two, remainder;
  /* Initialize variables with the same precision as x */
  mpfr_init2(two, mpfr_get_prec(x));
  mpfr_init2(remainder, mpfr_get_prec(x));
  /* Set the divisor to 2 */
  mpfr_set_si(two, 2, MPFR_ROUNDING);
  /* 4. Calculate the remainder (r = x mod 2)
     We use mpfr_fmod with MPFR_RNDZ for truncated (C-style) modulo result. */
  mpfr_fmod(remainder, x, two, MPFR_RNDZ);
  /* 5. Check the remainder for odd/even
     An odd number will have a remainder of 1 or -1 (depending on x's sign).
     Check if remainder is 1 OR remainder is -1 */
  if (mpfr_cmp_si(remainder, 1) == 0 || mpfr_cmp_si(remainder, -1) == 0) {
    isodd = 1;
  } else { /* Remainder must be 0 for a finite integer */
    isodd = 0;
  }
  /* 6. Clean up temporary variables */
  mpfr_clear(two);
  mpfr_clear(remainder);
  return isodd;
}

static int mpfr_iseven (mpfr_t x) {
  int rc = mpfr_isodd(x);
  return (rc == -1) ? -1 : !rc;
}


/*****************************************************************************************************

	Macro templates

******************************************************************************************************/

/* take two mpf num arguments a, b, apply procname to them and push the result onto the stack in new mpf num r. */
#define template_2args_1ret(procname) { \
  Mpfr *mx, *my, *mr; \
  mx = checkmpfr(L, 1); \
  my = checkmpfr(L, 2); \
  creatempf(mr); \
  procname(mr->val, mx->val, my->val); \
}

/* take three mpf num arguments a, b, c, apply procname to them and push the result onto the stack in new mpf num r. */
#define template_3args_1ret(procname) { \
  Mpfr *mx, *my, *mz, *mr; \
  mx = checkmpfr(L, 1); \
  my = checkmpfr(L, 2); \
  mz = checkmpfr(L, 3); \
  creatempf(mr); \
  procname(mr->val, mx->val, my->val, mz->val); \
}

/* take three mpf num arguments r, a, b, apply procname to them; the result is implicitly stored to mpf num r. */
#define template_3args_composite(procname) { \
  Mpfr *mx, *my, *mr; \
  mr = checkmpfr(L, 1); \
  mx = checkmpfr(L, 2); \
  my = checkmpfr(L, 3); \
  lua_settop(L, 1); \
  procname(mr->val, mx->val, my->val); \
}

/* take two mpf num arguments a, b, apply procname to them and push the result - a Lua/Agena number - onto the stack  */
#define template_2args_1num(procname) { \
  Mpfr *mx, *my; \
  mx = checkmpfr(L, 1); \
  my = checkmpfr(L, 2); \
  lua_pushnumber(L, procname(mx->val, my->val)); \
}

/* take a mpf num, apply procname to it and push the result onto the stack as a new mpf num r. */
#define template_1arg_1ret(procname) { \
  Mpfr *mx, *mr; \
  mx = checkmpfr(L, 1); \
  creatempf(mr); \
  procname(mr->val, mx->val); \
}

/* Create a constant: log2, pi, euler, catalan, inf, nan */
#define template_create_constant(procname) { \
  Mpfr *mr; \
  creatempf(mr); \
  procname(mr->val); \
}

/* Depending on the sign of its argument, an integer: if it is non-negative, returns +infinity as an MPFR object, and
   -infinity as an MPFR object, otherwise. 2.9.10 */
#define template_InfZero(procname) { \
  Mpfr *mr; \
  int sgn; \
  sgn = (int)luaL_checkint32_t(L, 1); \
  creatempf(mr); \
  procname(mr->val, sgn); \
}

static int Mpf_Inf (lua_State *L) {
  template_InfZero(Mpfr_set_inf);
  return 1;
}


/* Depending on the sign of its argument, an integer: if it is non-negative, returns 0 as an MPFR object, and -0 as an
   MPFR object, otherwise. 2.9.10 */
static int Mpf_Zero (lua_State *L) {
  template_InfZero(Mpfr_set_zero);
  return 1;
}

static int Mpf_Pi (lua_State *L) {
  template_create_constant(Mpfr_const_pi);
  return 1;
}

static int Mpf_Log2 (lua_State *L) {
  template_create_constant(Mpfr_const_log2);
  return 1;
}

static int Mpf_Euler (lua_State *L) {
  template_create_constant(Mpfr_const_euler);
  return 1;
}

static int Mpf_Catalan (lua_State *L) {
  template_create_constant(Mpfr_const_catalan);
  return 1;
}

static int Mpf_Nan (lua_State *L) {
  template_create_constant(Mpfr_set_nan);
  return 1;
}


/*****************************************************************************************************

 Real Arithmetic

******************************************************************************************************/

/* mpfr.add(a, b): Adds two mpf num a, b, and returns a new mpf num. Used by __add metamethod, i.e.:
   mpfr.add(a, b) = a + b. */
static int Mpf_add (lua_State *L) {  /* a + b */
  template_2args_1ret(Mpfr_add);
  return 1;
}


/* mpfr.subtract(a, b): Subtracts two mpf num a, b and returns a new mpf num. Used by __sub metamethod, i.e.:
   mpfr.subtract(a, b) = a - b. */
static int Mpf_subtract (lua_State *L) {  /* a - b  */
  template_2args_1ret(Mpfr_sub);
  return 1;
}


/* mp.multiply(a, b): Multiplies two mpints a, b and returns a new mpint. Used by __mul metamethod, i.e.:
   mp.multiply(a, b) = a * b. */
static int Mpf_multiply (lua_State *L) {  /* a * b */
  template_2args_1ret(Mpfr_mul);
  return 1;
}


/* mp.divide(a, b): Divides two mpints a, b and returns a new mpint. Used by __div metamethod, i.e.:
   mp.divide(a, b) = a / b. */

#include <mpfr-impl.h>  /* must be included here, not in the header */
static int Mpf_divide (lua_State *L) {  /* extended 3.4.2 explicit check for zero denominator */
  Mpfr *mx, *my, *mr;
  mx = checkmpfr(L, 1);
  my = checkmpfr(L, 2);
  creatempf(mr);
  if (Mpfr_iszero(my->val)) {
    Mpfr_setnan(mr->val);  /* same as template_create_constant(Mpfr_set_nan) */
    /* cannot use MPFR_RET_NAN as it cannot be linked */
  } else {
    Mpfr_div(mr->val, mx->val, my->val);
  }
  return 1;
}


/* __pow metamethod + mpfr.pow(): Exponentiation a^b: raises mpf num a to the power of mpf num b and returns
   a new mpf num. Used by __pow metamethod. */
static int Mpf_pow (lua_State *L) {
  template_2args_1ret(Mpfr_pow);
  return 1;
}


/* fast multiply-add */
static int Mpf_fma (lua_State *L) {
  if (ismpf(L, 1) || agn_isnumber(L, 1)) {
    Mpfr *a, *b, *c, *r;
    a = checkmpfr(L, 1);
    b = checkmpfr(L, 2);
    c = checkmpfr(L, 3);
    creatempf(r);
    Mpfr_fma(r->val, a->val, b->val, c->val);
  } else if (iscmpf(L, 1)) {
    CMpfr *a, *b, *c, *r;
    a = (CMpfr *)lua_touserdata(L, 1);
    b = checkcmpfr(L, 2);
    c = checkcmpfr(L, 3);
    createcmpf(r);
    /* re = areal*breal-[aimag*bimag-creal] */
    mpfr_fms(r->real, a->imag, b->imag, c->real, MPFR_RNDN);
    mpfr_fms(r->real, a->real, b->real, r->real, MPFR_RNDN);
    /* im = areal*bimag+[aimag*breal+cimag] */
    mpfr_fma(r->imag, a->imag, b->real, c->imag, MPFR_RNDN);
    mpfr_fma(r->imag, a->real, b->imag, r->imag, MPFR_ROUNDING);
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr.fma");
  }
  return 1;
}


/* fast multiply-subtract */
static int Mpf_fms (lua_State *L) {
  template_3args_1ret(Mpfr_fms);
  return 1;
}


/* dim: returns a-b if a > b, 0 if a <= b, or `undefined` of a or b is `undefined`. */
static int Mpf_dim (lua_State *L) {
  template_2args_1ret(Mpfr_dim);
  return 1;
}


/*****************************************************************************************************

	Miscellaneous

******************************************************************************************************/

/* mpfr.tonumber(a): Returns the numeric value in mpf num a as a number. */
static int Mpf_tonumber (lua_State *L) {
  int nrets = 0;
  if (ismpf(L, 1)) {
    Mpfr *a = (Mpfr *)lua_touserdata(L, 1);
    lua_pushnumber(L, Mpfr_get_d(a->val));
    nrets = 1;
  } else if (iscmpf(L, 1)) {  /* 6.5.15 extension */
    CMpfr *a = (CMpfr *)lua_touserdata(L, 1);
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushnumber(L, Mpfr_get_d(a->real));
    lua_pushnumber(L, Mpfr_get_d(a->imag));
    nrets = 2;
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr.tonumber");
  }
  return nrets;
}


/* Gets or sets the overall precision, in bits. If a number in the range 2 .. 2,147,483,647 is being passed, the function
   sets the precision for all values subsequently allocated. If no argument is given, the current setting is returned.
   The default precision at invocation of the package is 128. */
static int Mpf_precision (lua_State *L) {
  int nargs = lua_gettop(L);
  if (nargs != 0) {
    if (agn_isinteger(L, 1)) {
      int precision = agn_tointeger(L, 1);
      if (precision < MPFR_PREC_MIN || precision > MPFR_PREC_MAX)
        luaL_error(L, "Error in " LUA_QS ": precision out of range %d .. %d.", "mpfr.precision", MPFR_PREC_MIN, MPFR_PREC_MAX);
      MPFR_PRECISION = precision;
      mpfr_set_default_prec(precision);
    } else if (ismpf(L, 1)) {  /* set precision of a specific real MPFR value, 7.5.1 */
      Mpfr *a = (Mpfr *)lua_touserdata(L, 1);
      if (nargs == 2) {
        mpfr_prec_t prec = agn_checkposint(L, 2);
        if (prec < 2) prec = 2;
        mpfr_prec_round(a->val, prec, MPFR_ROUNDING);
      }
      lua_pushnumber(L, mpfr_get_prec(a->val));
      return 1;  /* do not return the current global precision, but the precision of the MPFR value */
    } else if (iscmpf(L, 1)) {  /* set precision of a specific complex MPFR value, 7.5.1 */
      CMpfr *a = (CMpfr *)lua_touserdata(L, 1);
      if (nargs == 2) {
        mpfr_prec_t prec = agn_checkposint(L, 2);
        if (prec < 2) prec = 2;
        mpfr_prec_round(a->real, prec, MPFR_ROUNDING);
        mpfr_prec_round(a->imag, prec, MPFR_ROUNDING);
      }
      lua_pushnumber(L, mpfr_get_prec(a->real));
      return 1;  /* do not return the current global precision, but the precision of the MPFR value */
    } else {
      luaL_error(L, "Error in " LUA_QS ": need an integer or real MPFR value, got %s.", "mpfr.precision",
        luaL_typename(L, 1));
    }
  }
  lua_pushinteger(L, MPFR_PRECISION);
  return 1;
}


/* Gets or sets the current rounding mode. If a string rmode is passed, the function sets the rounding mode for all values
   subsequently allocated. Valid settings for rmode are:
     rndn = MPFR_RNDN=0,  round to nearest, with ties to even
     rndz = MPFR_RNDZ,    round toward zero
     rndu = MPFR_RNDU,    round toward +Inf
     rndd = MPFR_RNDD,    round toward -Inf
   If no argument is given, the current rounding mode is returned. The default rounding mode at invocation of the package
   is 'rndn', i.e. rounding to nearest. */
static int Mpf_rounding (lua_State *L) {
  static const char *const opts[] = {"rndn", "rndz", "rndu", "rndd", NULL};
  static const char *const desc[] = {
    "round to nearest, with ties to even",
    "round toward zero",
    "round toward +infinity",
    "round toward -infinity",
    NULL
  };
  if (lua_gettop(L) != 0) {
    int roundingmode = agnL_checkoption(L, 1, "rndn", opts, 0);
    MPFR_ROUNDING = roundingmode;
  }
  luaL_checkstack(L, 2, "not enough stack space");  /* 6.5.1 fix */
  lua_pushstring(L, opts[MPFR_ROUNDING]);
  lua_pushstring(L, desc[MPFR_ROUNDING]);
  return 2;
}


/* mpfr.swap(a,b): swaps the values of a and b efficiently. Returns nothing. 2.9.10 */
static int Mpf_swap (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *mx, *my;
    mx = (Mpfr *)lua_touserdata(L, 1);
    my = (Mpfr *)checkmpfr(L, 2);
    Mpfr_swap(mx->val, my->val);
  } else if (iscmpf(L, 1)) {  /* 6.5.15 */
    CMpfr *mx = lua_touserdata(L, 1);
    CMpfr *my = checkcmpfr(L, 2);
    Mpfr_swap(mx->real, my->real);
    Mpfr_swap(my->imag, my->imag);
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr.swap");
  }
  return 0;
}


/*****************************************************************************************************

	Metamethods

******************************************************************************************************/

static int aux_tostring (lua_State *L, Mpfr_t x, int flag) {
  int deciloca;
  char *r, *r0;
  mpfr_exp_t mpfrDeciloca;
  char *buf = Mpfr_get_str(NULL, &mpfrDeciloca, 10, 0, x, MPFR_ROUNDING);
  r = NULL; r0 = NULL;
  if (!buf)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "mpfr.__tostring");
  /* see: https://machinecognitis.github.io/Math.Mpfr.Native/html/2408892b-5aea-259c-00be-b1a44fa53c1f.htm */
  if (tools_streq(buf, "@NaN@")) {
    mpfr_free_str(buf);
    lua_pushstring(L, "undefined");
    return 1;
  } else if (tools_streq(buf, "-@Inf@")) {
    mpfr_free_str(buf);
    lua_pushstring(L, "-infinity");
    return 1;
  } else if (tools_streq(buf, "@Inf@")) {
    mpfr_free_str(buf);
    lua_pushstring(L, "infinity");
    return 1;
  }
  deciloca = mpfrDeciloca;
  if (deciloca < 0) {
    r0 = str_insert(buf, ".", 1 + (buf[0] == '-'));
    r = str_concat(r0, "e", tools_itoa(--deciloca, 10), NULL);
    if (!r) {
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "__tostring metamethod");
    }
  } else {
    if (deciloca == 0 && (buf[0] == '0' || (buf[0] == '-' && buf[1] == '0'))) deciloca = 1;
    r = str_insert(buf, ".", deciloca + (buf[0] == '-'));
  }
  lua_pushfstring(L, flag ? "(%s)" : "%s", r);
  if (flag) { lua_concat(L, 2); }
  mpfr_free_str(buf);
  if (r0) { xfree(r0); }  /* 6.5.15, better sure than sorry */
  xfree(r);
  return 1;
}

static int mt_tostring (lua_State *L) {
  Mpfr *a;
  a = checkmpfr(L, 1);  /* push userdata on stack */
  lua_settop(L, 1);    /* don't let the stack be polluted by futile additional arguments */
  if (agn_getutype(L, 1)) {
    aux_tostring(L, a->val, 1);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid mpf object.", "mpfr.__tostring");
  return 1;
}


static int mtc_tostring (lua_State *L) {  /* 6.5.15 */
  CMpfr *a = checkcmpfr(L, 1);  /* push userdata on stack */
  luaL_checkstack(L, 6, "not enough stack space");
  lua_settop(L, 1);  /* don't let the stack be polluted by futile additional arguments */
  if (agn_getutype(L, 1)) {
    lua_pushliteral(L, "(");
    aux_tostring(L, a->real, 0);
    lua_pushliteral(L, ", ");
    aux_tostring(L, a->imag, 0);
    lua_pushliteral(L, ")");
    lua_concat(L, 6);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid mpf object.", "mpfr.__tostring/cmplx");
  return 1;
}


static int Mpf_tostring (lua_State *L) {  /* 6.5.15 */
  int rc = 0;
  if (ismpf(L, 1)) {
    Mpfr *a = (Mpfr *)lua_touserdata(L, 1);
    aux_tostring(L, a->val, 0);
    rc = 1;
  } else if (iscmpf(L, 1)) {
    CMpfr *a = (CMpfr *)lua_touserdata(L, 1);
    luaL_checkstack(L, 2, "not enough stack space");
    aux_tostring(L, a->real, 0);
    aux_tostring(L, a->imag, 0);
    rc = 2;
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr.tostring");
  }
  return rc;
}


#define template_cmp(cond) { \
  Mpfr *a, *b; \
  a = checkmpfr(L, 1); \
  b = checkmpfr(L, 2); \
  lua_pushboolean(L, (cond)); \
  return 1; \
}

static int mt_eq (lua_State *L) {
  template_cmp(0 == Mpfr_cmp(a->val, b->val));
}

static int mt_lt (lua_State *L) {
  template_cmp(0 > Mpfr_cmp(a->val, b->val));
}

static int mt_le (lua_State *L) {
  template_cmp(0 >= Mpfr_cmp(a->val, b->val));
}


static int Mpf_cmpd (lua_State *L) {  /* 3.4.2 */
  Mpfr *mx = checkmpfr(L, 1);
  lua_pushinteger(L, mpfr_cmp_d(mx->val, agn_checknumber(L, 2)));
  return 1;
}


static int Mpf_cmp (lua_State *L) {  /* 6.5.15 */
  Mpfr *mx = checkmpfr(L, 1);
  Mpfr *my = checkmpfr(L, 2);
  lua_pushinteger(L, mpfr_cmp(mx->val, my->val));
  return 1;
}


/* mp.neg(a): Returns -a, with a a mpf num, as a new mpf num. Used by __unm metamethod, i.e. mp.neg(a) <=> -a. */
static int mt_neg (lua_State *L) {
  template_1arg_1ret(Mpfr_neg);
  return 1;
}


/* __abs methamethod and mpfr.argument: determimes the absolute value */
static int mt_abs (lua_State *L) {
  template_1arg_1ret(Mpfr_abs);
  return 1;
}


static int mt_absdiff (lua_State *L) {  /* 7.5.8 */
  if (ismpf(L, 1)) {
    Mpfr *a, *b, *r;
    a = (Mpfr *)lua_touserdata(L, 1);
    b = checkmpfr(L, 2);
    creatempf(r);
    Mpfr_sub(r->val, a->val, b->val);
    Mpfr_abs(r->val, r->val);
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr/absdiff mt");
  }
  return 1;
}


/* __sign methamethod: determimes sign and returns a number: -1, 0 or +1 */
static int mt_sign (lua_State *L) {
  Mpfr *a = checkmpfr(L, 1);
  lua_pushinteger(L, mpfr_sgn(a->val));
  return 1;
}


/* __square metamethod, 3.3.7 */
static int mt_square (lua_State *L) {
  Mpfr *mx, *mr;
  mx = checkmpfr(L, 1);
  creatempf(mr);
  Mpfr_sqr(mr->val, mx->val);
  return 1;
}


static int mt_squareadd (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *a, *c, *r;
    a = (Mpfr *)lua_touserdata(L, 1);
    c = checkmpfr(L, 2);
    creatempf(r);
    Mpfr_fma(r->val, a->val, a->val, c->val);
  } else if (iscmpf(L, 1)) {
    CMpfr *a, *c, *r;
    a = (CMpfr *)lua_touserdata(L, 1);
    c = checkcmpfr(L, 2);
    createcmpf(r);
    /* re = areal*breal-[aimag*bimag-creal] */
    mpfr_fms(r->real, a->imag, a->imag, c->real, MPFR_RNDN);
    mpfr_fms(r->real, a->real, a->real, r->real, MPFR_RNDN);
    /* im = areal*bimag+[aimag*breal+cimag] */
    mpfr_fma(r->imag, a->imag, a->real, c->imag, MPFR_RNDN);
    mpfr_fma(r->imag, a->real, a->imag, r->imag, MPFR_ROUNDING);
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr/squareadd mt");
  }
  return 1;
}


/* __cube metamethod, 3.3.7 */
static int mt_cube (lua_State *L) {
  Mpfr *mx, *mr;
  mpfr_t my;
  mx = checkmpfr(L, 1);
  creatempf(mr);
  mpfr_init2(my, MPFR_PREC(mx->val));
  mpfr_set_d(my, 3.0, MPFR_RNDN);
  mpfr_pow(mr->val, mx->val, my, MPFR_RNDN);
  mpfr_clear(my);
  return 1;
}


/* __recip metamethod, 3.3.7 */
static int mt_recip (lua_State *L) {
  Mpfr *mx, *mr;
  mx = checkmpfr(L, 1);
  creatempf(mr);
  if (Mpfr_iszero(mx->val)) {
    Mpfr_setnan(mr->val);  /* same as template_create_constant(Mpfr_set_nan) */
    /* cannot use MPFR_RET_NAN as it cannot be linked */
  } else {
    mpfr_ui_div(mr->val, 1, mx->val, MPFR_ROUNDING);
  }
  return 1;
}


static int mt_gc (lua_State *L) {  /* __gc method */
  lua_lock(L);
  if (ismpf(L, 1)) {  /* avoid panics/crashes during restart, 3.4.6 */
    Mpfr *a = lua_touserdata(L, 1);
    lua_setmetatabletoobject(L, 1, NULL, 1);
    Mpfr_clear(a->val);
  }
  lua_unlock(L);
  return 0;
}


static int mtc_gc (lua_State *L) {  /* __gc method */
  lua_lock(L);
  if (iscmpf(L, 1)) {  /* avoid panics/crashes during restart */
    CMpfr *a = lua_touserdata(L, 1);
    lua_setmetatabletoobject(L, 1, NULL, 1);
    Mpfr_clear(a->real);
    Mpfr_clear(a->imag);
  }
  lua_unlock(L);
  return 0;
}


static void mpfcleanup (void) {  /* cleanup is not called when pressing CTRL+C */
  mpfr_free_cache();
  mpfr_clear_underflow();
  mpfr_clear_overflow();
  mpfr_clear_flags();
#ifndef DEBIAN
  mpfr_mp_memory_cleanup();
#endif
  gmp_randclear(randomstate);
}


static void mpfsigcleanup (int sig) {  /* for CTRL+C */
  mpfr_free_cache();
  mpfr_clear_underflow();
  mpfr_clear_overflow();
  mpfr_clear_flags();
#ifndef DEBIAN
  mpfr_mp_memory_cleanup();
#endif
  gmp_randclear(randomstate);
}


static int mtc_real (lua_State *L) {
  Mpfr *real;
  CMpfr *c = checkcmpfr(L, 1);
  creatempf(real);
  mpfr_set_prec(real->val, mpfr_get_prec(c->real));
  Mpfr_set(real->val, c->real);
  return 1;
}


static int mtc_imag (lua_State *L) {
  Mpfr *imag;
  CMpfr *c = checkcmpfr(L, 1);
  creatempf(imag);
  mpfr_set_prec(imag->val, mpfr_get_prec(c->imag));
  Mpfr_set(imag->val, c->imag);
  return 1;
}


/*****************************************************************************************************

	Transcendental Functions

******************************************************************************************************/

/* __sqrt methamethod: determimes square root */
static int mt_sqrt (lua_State *L) {
  template_1arg_1ret(Mpfr_sqrt);
  return 1;
}


/* determimes inverse square root, 2.21.10 */
static int Mpf_recsqrt (lua_State *L) {
  template_1arg_1ret(Mpfr_rec_sqrt);
  return 1;
}


static int Mpf_root (lua_State *L) {  /* 3.4.2 */
  Mpfr *mx, *mr;
  uint32_t k;
  mx = checkmpfr(L, 1);
  k = agn_checkuint32_t(L, 2);
  creatempf(mr);
  if (k == 0) {
    Mpfr_setnan(mr->val);
  } else {
#if (0 && defined(DEBIAN) && defined(IS32BIT))
    /* for reasons unknown, mpfr_rootn_ui() is somehow not available in Debian. This fix has been suggested by Gemini AI. 6.3.8 */
    mpfr_root(mr->val, mx->val, k, MPFR_PRECISION);
#else
    mpfr_rootn_ui(mr->val, mx->val, k, MPFR_PRECISION);
#endif
  }
  return 1;
}


/* __ln methamethod: determimes natural logarithm */
static int mt_ln (lua_State *L) {
  template_1arg_1ret(Mpfr_log);
  return 1;
}


/* __exp methamethod: determines exponential function to the base E = 2.71828... */
static int mt_exp (lua_State *L) {
  template_1arg_1ret(Mpfr_exp);
  return 1;
}


/* __sin methamethod: determimes sine */
static int mt_sin (lua_State *L) {
  template_1arg_1ret(Mpfr_sin);
  return 1;
}


/* __sin methamethod: determimes cosine */
static int mt_cos (lua_State *L) {
  template_1arg_1ret(Mpfr_cos);
  return 1;
}


/* __tan methamethod: determimes tangent */
static int mt_tan (lua_State *L) {
  template_1arg_1ret(Mpfr_tan);
  return 1;
}


/* __arcsin methamethod: determimes arcsine */
static int mt_arcsin (lua_State *L) {
  template_1arg_1ret(Mpfr_asin);
  return 1;
}


/* __arccos methamethod: determimes arccosine */
static int mt_arccos (lua_State *L) {
  template_1arg_1ret(Mpfr_acos);
  return 1;
}


/* __arctan methamethod: determimes arctangent */
static int mt_arctan (lua_State *L) {
  template_1arg_1ret(Mpfr_atan);
  return 1;
}


/* mpfr.arctan2(): arc-tangent2 of y and x  */
static int Mpf_arctan2 (lua_State *L) {
  template_2args_1ret(Mpfr_atan2);
  return 1;
}


/* mpfr.hypot(): hypotenuse */
static int Mpf_hypot (lua_State *L) {
  template_2args_1ret(Mpfr_hypot);
  return 1;
}


static int Mpf_cathet (lua_State *L) {  /* 3.4.2 */
  Mpfr *mx, *my, *mr;
  mx = checkmpfr(L, 1);
  my = checkmpfr(L, 2);
  creatempf(mr);
  if (mpfr_cmp(mx->val, my->val) < 0) {
    Mpfr_setnan(mr->val);
  } else {
    mpfr_t mxsq, mysq;
    mpfr_prec_t prec = MPFR_PREC(mx->val) + 2;
    mpfr_inits2(prec, mxsq, mysq, MPFR_NULL);
    mpfr_sqr(mxsq, mx->val, MPFR_RNDN);
    mpfr_sqr(mysq, my->val, MPFR_RNDN);
    mpfr_sub(mxsq, mxsq, mysq, MPFR_RNDN);
    mpfr_sqrt(mr->val, mxsq, MPFR_ROUNDING);
    mpfr_clears(mxsq, mysq, MPFR_NULL);
  }
  return 1;
}


static int Mpf_pytha (lua_State *L) {  /* 3.4.2 */
  Mpfr *mx, *my, *mr;
  mx = checkmpfr(L, 1);
  my = checkmpfr(L, 2);
  creatempf(mr);
  mpfr_t mxsq, mysq;
  mpfr_prec_t prec = MPFR_PREC(mx->val) + 2;
  mpfr_inits2(prec, mxsq, mysq, MPFR_NULL);
  mpfr_sqr(mxsq, mx->val, MPFR_RNDN);
  mpfr_sqr(mysq, my->val, MPFR_RNDN);
  mpfr_add(mr->val, mxsq, mysq, MPFR_ROUNDING);
  mpfr_clears(mxsq, mysq, MPFR_NULL);
  return 1;
}


static int Mpf_pytha4 (lua_State *L) {  /* 3.4.2 */
  Mpfr *mx, *my, *mr;
  mx = checkmpfr(L, 1);
  my = checkmpfr(L, 2);
  creatempf(mr);
  mpfr_t mxsq, mysq;
  mpfr_prec_t prec = MPFR_PREC(mx->val) + 2;
  mpfr_inits2(prec, mxsq, mysq, MPFR_NULL);
  mpfr_sqr(mxsq, mx->val, MPFR_RNDN);
  mpfr_sqr(mysq, my->val, MPFR_RNDN);
  mpfr_sub(mr->val, mxsq, mysq, MPFR_ROUNDING);
  mpfr_clears(mxsq, mysq, MPFR_NULL);
  return 1;
}


static int Mpf_relerror (lua_State *L) {  /* 3.4.2 */
  template_2args_1ret(Mpfr_reldiff);
  return 1;
}


/* __sinh methamethod: determimes hyperbolic sine */
static int mt_sinh (lua_State *L) {
  template_1arg_1ret(Mpfr_sinh);
  return 1;
}


/* __cosh methamethod: determimes hyperbolic cosine */
static int mt_cosh (lua_State *L) {
  template_1arg_1ret(Mpfr_cosh);
  return 1;
}


/* __tanh methamethod: determimes hyperbolic tangent */
static int mt_tanh (lua_State *L) {
  template_1arg_1ret(Mpfr_tanh);
  return 1;
}


static int Carccosh (lua_State *L);
static int Carctanh (lua_State *L);

#define aux_turntompfandcallagain(L,n,fn) { \
  Mpfr *mx; \
  creatempf(mx); \
  Mpfr_set_d(mx->val, agn_tonumber(L, n)); \
  lua_replace(L, n); \
  return fn(L); \
}

/* arcsinh: determimes inverse hyperbolic sine, 3.3.7 */
static int Mpf_arcsinh (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *mx, *mr;
    mx = (Mpfr *)lua_touserdata(L, 1);
    creatempf(mr);
    Mpfr_asinh(mr->val, mx->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_arcsinh);
  } else {  /* 6.5.15 extension; 100 % boost by Gemini AI, 6.6.1, see asbove */
    mpfr_prec_t prec;
    CMpfr *a, *z;
    a = checkcmpfr(L, 1);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    mpfr_carcsinh(z->real, z->imag, a->real, a->imag, prec);
  }
  return 1;
}


static int Mpf_arccosh (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *mx, *mr;
    mx = (Mpfr *)lua_touserdata(L, 1);
    creatempf(mr);
    Mpfr_acosh(mr->val, mx->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_arccosh);
  } else {  /* 6.5.15 extension */
    Carccosh(L);
  }
  return 1;
}


/* arcsinh: determimes inverse hyperbolic tangent, 3.3.7 */
static int Mpf_arctanh (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *mx, *mr;
    mx = (Mpfr *)lua_touserdata(L, 1);
    creatempf(mr);
    Mpfr_atanh(mr->val, mx->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_arctanh);
  } else {  /* 6.5.15 extension */
    Carctanh(L);
  }
  return 1;
}


/* Generated by Gemini AI, 6.6.1, put to the public domain */
#undef mpfr_const_pi  /* leave that here, otherwise the package will not compile */
static int Mpf_arccot (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *mx = (Mpfr *)lua_touserdata(L, 1);
    Mpfr *mr;
    creatempf(mr);
    /* acot(x) = atan(1/x) */
    if (mpfr_zero_p(mx->val)) {
      /* Limit as x->0 is pi/2 */
      mpfr_const_pi(mr->val, MPFR_RNDN);
      mpfr_div_2ui(mr->val, mr->val, 1, MPFR_RNDN);
    } else {
      mpfr_ui_div(mr->val, 1, mx->val, MPFR_RNDN);
      mpfr_atan(mr->val, mr->val, MPFR_ROUNDING);
    }
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_arccot);
  } else {
    /* Complex Case: acot(z) = atan(1/z) */
    CMpfr *a = checkcmpfr(L, 1);
    CMpfr *z;
    createcmpf(z);
    mpfr_prec_t prec = mpfr_get_prec(a->real) + 4;
    if (mpfr_zero_p(a->real) && mpfr_zero_p(a->imag)) {
      /* acot(0) = pi/2 */
      mpfr_const_pi(z->real, MPFR_RNDN);
      mpfr_div_2ui(z->real, z->real, 1, MPFR_RNDN);
      mpfr_set_zero(z->imag, 1);
      return 1;
    }
    mpfr_set_prec(z->real, prec);
    mpfr_set_prec(z->imag, prec);
    mpfr_t re, im, den;
    mpfr_inits2(prec, re, im, den, MPFR_NULL);
    /* 1. Complex Reciprocal: 1 / (a + bi) */
    mpfr_sqr(re, a->real, MPFR_RNDN);
    mpfr_sqr(im, a->imag, MPFR_RNDN);
    mpfr_add(den, re, im, MPFR_RNDN);
    mpfr_div(re, a->real, den, MPFR_RNDN);
    mpfr_div(im, a->imag, den, MPFR_RNDN);
    mpfr_neg(im, im, MPFR_RNDN);
    /* 2. Call the internal atan */
    mpfr_carctan(z->real, z->imag, re, im, prec);
    mpfr_set(z->real, z->real, MPFR_ROUNDING);
    mpfr_set(z->imag, z->imag, MPFR_ROUNDING);
    mpfr_clears(re, im, den, MPFR_NULL);
  }
  return 1;
}


/* 3.4.2; extended to the complex domain 6.6.1 */
static int Mpf_arccoth (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *mx, *mr;
    int isundef;
    mx = checkmpfr(L, 1);
    isundef = mpfr_cmp_d(mx->val, 1.0) < 1;  /* x <= 1 ? */
    creatempf(mr);
    if (isundef || Mpfr_issingular(mx->val)) {
      if (isundef || Mpfr_isnan(mx->val)) {
        Mpfr_setnan(mr->val);  /* same as template_create_constant(Mpfr_set_nan) */
      } else if (Mpfr_iszero(mx->val)) {
        Mpfr_setzero(mr->val);
      } else {  /* inf */
        Mpfr_setinf(mr->val);
      }
    } else {
      mpfr_prec_t prec = MPFR_PREC(mx->val) + 2;
      mpfr_t mxm1, mxp1;
      mpfr_init2(mxm1, prec);
      mpfr_init2(mxp1, prec);
      /* 0.5*(ln(x + 1) - ln(x - 1)) */
      mpfr_add_d(mxp1, mx->val, 1.0, MPFR_RNDN);
      mpfr_log(mxp1, mxp1, MPFR_RNDN);
      mpfr_sub_d(mxm1, mx->val, 1.0, MPFR_RNDN);
      mpfr_log(mxm1, mxm1, MPFR_RNDN);
      mpfr_sub(mr->val, mxp1, mxm1, MPFR_RNDN);
      mpfr_div_d(mr->val, mr->val, 2.0, MPFR_ROUNDING);
      mpfr_clears(mxm1, mxp1, MPFR_NULL);
    }
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_arccoth);
  } else {
    /* Complex Case: acoth(z) = atanh(1/z) */
    mpfr_t z1_re, z1_im, z2_re, z2_im, t1, t2;
    CMpfr *a, *z;
    a = checkcmpfr(L, 1);
    createcmpf(z);
    mpfr_prec_t prec = mpfr_get_prec(a->real) + 4;
    mpfr_set_prec(z->real, prec);
    mpfr_set_prec(z->imag, prec);
    /* Identity: acoth(z) = 0.5 * (ln(z + 1) - ln(z - 1)) */
    mpfr_inits2(prec, z1_re, z1_im, z2_re, z2_im, t1, t2, MPFR_NULL);
    /* 1. ln(z + 1) -> z1 */
    mpfr_add_ui(t1, a->real, 1, MPFR_RNDN); /* re+1 */
    /* Real part: 0.5 * ln(re^2 + im^2) */
    mpfr_sqr(z1_re, t1, MPFR_RNDN);
    mpfr_sqr(z1_im, a->imag, MPFR_RNDN);
    mpfr_add(z1_re, z1_re, z1_im, MPFR_RNDN);
    mpfr_log(z1_re, z1_re, MPFR_RNDN);
    mpfr_div_2ui(z1_re, z1_re, 1, MPFR_RNDN);
    /* Imag part: atan2(im, re) */
    mpfr_atan2(z1_im, a->imag, t1, MPFR_RNDN);
    /* 2. ln(z - 1) -> z2 */
    mpfr_sub_ui(t1, a->real, 1, MPFR_RNDN); /* re-1 */
    /* Real part */
    mpfr_sqr(z2_re, t1, MPFR_RNDN);
    mpfr_sqr(z2_im, a->imag, MPFR_RNDN);
    mpfr_add(z2_re, z2_re, z2_im, MPFR_RNDN);
    mpfr_log(z2_re, z2_re, MPFR_RNDN);
    mpfr_div_2ui(z2_re, z2_re, 1, MPFR_RNDN);
    /* Imag part */
    mpfr_atan2(z2_im, a->imag, t1, MPFR_RNDN);
    /* 3. Result = 0.5 * (z1 - z2) */
    mpfr_sub(z->real, z1_re, z2_re, MPFR_RNDN);
    mpfr_div_2ui(z->real, z->real, 1, MPFR_RNDN);
    mpfr_sub(z->imag, z1_im, z2_im, MPFR_RNDN);
    mpfr_div_2ui(z->imag, z->imag, 1, MPFR_RNDN);
    /* 4. Branch Correction for Maple V4 Compatibility */
    /* If the imaginary part is negative for positive 'b', shift by PI */
    if (mpfr_sgn(a->imag) > 0 && mpfr_sgn(z->imag) < 0) {
      mpfr_t pi;
      mpfr_init2(pi, prec);
      mpfr_const_pi(pi, MPFR_RNDN);
      mpfr_add(z->imag, z->imag, pi, MPFR_RNDN);
      mpfr_clear(pi);
    }
    mpfr_set(z->real, z->real, MPFR_ROUNDING);
    mpfr_set(z->imag, z->imag, MPFR_ROUNDING);
    mpfr_clears(z1_re, z1_im, z2_re, z2_im, t1, t2, MPFR_NULL);
  }
  return 1;
}


static int mt_arcsec (lua_State *L) {   /* arcsec(x), 6.5.15 */
  mpfr_t Mpfr_One;
  mpfr_prec_t prec;
  Mpfr *mx, *mr;
  mx = checkmpfr(L, 1);
  prec = mpfr_get_prec(mx->val) + 2;
  Mpfr_init2_d(Mpfr_One, 1.0, prec);
  if (Mpfr_cmp(mx->val, Mpfr_One) < 0) {
    lua_pushundefined(L);
  } else {
    creatempf(mr);
    mpfr_ui_div(mr->val, 1, mx->val, MPFR_RNDN);
    mpfr_acos(mr->val, mr->val, MPFR_ROUNDING);
  }
  mpfr_clear(Mpfr_One);
  return 1;
}


/* exponential integral */
static int Mpf_eint (lua_State *L) {
  template_1arg_1ret(Mpfr_eint);
  return 1;
}


/* real part of the dilogarithm of its argument */
static int Mpf_li2 (lua_State *L) {
  template_1arg_1ret(Mpfr_li2);
  return 1;
}


/* Gamma function */
static int Mpf_gamma (lua_State *L) {
  template_1arg_1ret(Mpfr_gamma);
  return 1;
}


/* logarithm of the Gamma function */
static int Mpf_lngamma (lua_State *L) {
  template_1arg_1ret(Mpfr_lngamma);
  return 1;
}


/* Digamma (Psi) function */
static int Mpf_digamma (lua_State *L) {
  template_1arg_1ret(Mpfr_digamma);
  return 1;
}


/* Riemann Zeta function */
static int Mpf_zeta (lua_State *L) {
  template_1arg_1ret(Mpfr_zeta);
  return 1;
}


/* first kind Bessel function of order 0 */
static int Mpf_j0 (lua_State *L) {
  template_1arg_1ret(Mpfr_j0);
  return 1;
}


/* first kind Bessel function of order 1 */
static int Mpf_j1 (lua_State *L) {
  template_1arg_1ret(Mpfr_j1);
  return 1;
}


/* second kind Bessel function of order 0 */
static int Mpf_y0 (lua_State *L) {
  template_1arg_1ret(Mpfr_y0);
  return 1;
}


/* second kind Bessel function of order 1 */
static int Mpf_y1 (lua_State *L) {
  template_1arg_1ret(Mpfr_y1);
  return 1;
}


/* Airy function */
static int Mpf_ai (lua_State *L) {
  template_1arg_1ret(Mpfr_ai);
  return 1;
}


/* Beta function */
static int Mpf_beta (lua_State *L) {
  template_2args_1ret(Mpfr_beta);
  return 1;
}


/* arithmetic-geometric mean function */
static int Mpf_agm (lua_State *L) {
  template_2args_1ret(Mpfr_agm);
  return 1;
}


/* take two mpf num arguments a, b, apply procname to them and push the result - a Lua/Agena number - onto the stack  */
#define template_Bessel(procname) { \
  Mpfr *mr, *mx; \
  long int n; \
  mx = checkmpfr(L, 1); \
  n = (long int)luaL_checkint32_t(L, 2); \
  creatempf(mr); \
  procname(mr->val, mx->val, n); \
}


/* Bessel function of the first kind with order n */
static int Mpf_jn (lua_State *L) {
  template_Bessel(Mpfr_jn);
  return 1;
}


/* Bessel function of the second kind with order n */
static int Mpf_yn (lua_State *L) {
  template_Bessel(Mpfr_yn);
  return 1;
}


/*****************************************************************************************************

	Miscellaneous Numeric Functions

******************************************************************************************************/

/* determimes if mpf num is `undefined` */
static int Mpf_isundefined (lua_State *L) {
  if (ismpf(L, 1) || agn_isnumber(L, 1)) {
    Mpfr *a = checkmpfr(L, 1);
    lua_pushboolean(L, mpfr_nan_p(a->val) != 0);
  } else if (iscmpf(L, 1)) {
    CMpfr *a = (CMpfr *)lua_touserdata(L, 1);
    lua_pushboolean(L, mpfr_nan_p(a->real) != 0 || mpfr_nan_p(a->imag) != 0);
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr.isundefined");
  }
  return 1;
}


static int Mpf_isinfinite (lua_State *L) {
  if (ismpf(L, 1) || agn_isnumber(L, 1)) {
    Mpfr *a = checkmpfr(L, 1);
    lua_pushboolean(L, mpfr_inf_p(a->val) != 0);
  } else if (iscmpf(L, 1)) {
    CMpfr *a = (CMpfr *)lua_touserdata(L, 1);
    lua_pushboolean(L, mpfr_inf_p(a->real) != 0 || mpfr_inf_p(a->imag) != 0);
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr.isinfinite");
  }
  return 1;
}


/* determimes if mpf num is finite, i.e. neither `undefined` or `infinity` */
static int Mpf_isfinite (lua_State *L) {
  if (ismpf(L, 1) || agn_isnumber(L, 1)) {
    Mpfr *a = checkmpfr(L, 1);
    lua_pushboolean(L, mpfr_number_p(a->val) != 0);
  } else if (iscmpf(L, 1)) {
    CMpfr *a = (CMpfr *)lua_touserdata(L, 1);
    lua_pushboolean(L, mpfr_number_p(a->real) != 0 && mpfr_number_p(a->imag) != 0);
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr.isfinite");
  }
  return 1;
}


/* __zero metamethod + mpfr.iszero: determimes if mpf num is zero */
static int Mpf_iszero (lua_State *L) {
  if (ismpf(L, 1) || agn_isnumber(L, 1)) {
    Mpfr *a = checkmpfr(L, 1);
    lua_pushboolean(L, Mpfr_iszero(a->val));
  } else if (iscmpf(L, 1)) {
    CMpfr *a = (CMpfr *)lua_touserdata(L, 1);
    lua_pushboolean(L, Mpfr_iszero(a->real) && Mpfr_iszero(a->imag));
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr.iszero");
  }
  return 1;
}


/* __nonzero metamethod + mpfr.isnonzero: determimes if mpf num is non-zero */
static int Mpf_isnonzero (lua_State *L) {
  if (ismpf(L, 1) || agn_isnumber(L, 1)) {
    Mpfr *a = checkmpfr(L, 1);
    lua_pushboolean(L, Mpfr_iszero(a->val) == 0);
  } else if (iscmpf(L, 1)) {
    CMpfr *a = (CMpfr *)lua_touserdata(L, 1);
    lua_pushboolean(L, !(Mpfr_iszero(a->real) && Mpfr_iszero(a->imag)));
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr.isnonzero");
  }
  return 1;
}


/* mpfr.modf() */
static int Mpf_modf (lua_State *L) {
  template_2args_1ret(Mpfr_modf);
  return 1;
}


/* mpfr.fmod() */
static int Mpf_fmod (lua_State *L) {
  template_2args_1ret(Mpfr_fmod);
  return 1;
}


static int Mpf_remquo (lua_State *L) {  /* 6.4.4 */
  long q;
  Mpfr *mx, *my, *mr, *mq;
  mx = checkmpfr(L, 1);
  my = checkmpfr(L, 2);
  luaL_checkstack(L, 2, "not enough stack space");
  creatempf(mr);
  creatempf(mq);
  mpfr_remquo(mr->val, &q, mx->val, my->val, MPFR_RNDN);
  mpfr_set_si(mq->val, q, MPFR_ROUNDING);
  return 2;  /* remainder and quotient, in this order */
}


static int mt_mod (lua_State *L) {  /* 6.4.4 */
  Mpfr *a, *b, *r;
  a = checkmpfr(L, 1);
  b = checkmpfr(L, 2);
  creatempf(r);
  /* r = a - floor(a/b)*b */
  mpfr_div(r->val, a->val, b->val, MPFR_RNDN);
  mpfr_floor(r->val, r->val);
  mpfr_mul(r->val, r->val, b->val, MPFR_RNDN);
  mpfr_sub(r->val, a->val, r->val, MPFR_ROUNDING);
  return 1;
}


/* Like `math.nextafter`, but for MPFR values. Note that this function does _not_ change the argument you are passing. */
static int Mpf_nexttoward (lua_State *L) {
  mpfr_t r;
  Mpfr *mr, *mx, *my;
  mx = checkmpfr(L, 1);
  my = checkmpfr(L, 2);
  mpfr_init(r);
  mpfr_set_prec(r, mpfr_get_prec(mx->val));  /* copy precision */
  Mpfr_set(r, mx->val);
  mpfr_nexttoward(r, my->val);
  creatempf(mr);
  mpfr_set_prec(mr->val, mpfr_get_prec(mx->val));  /* copy precision */
  Mpfr_set(mr->val, r);
  mpfr_clear(r);
  return 1;
}


/* Returns a uniformly distributed random float on the interval [0, 1]. 2.21.10 */
static int Mpf_random (lua_State *L) {
  Mpfr *mr;
  creatempf(mr);
  mpfr_urandom(mr->val, randomstate, MPFR_ROUNDING);
  return 1;
}


/* Resets the random nuumber generator. 2.21.10 */
static int Mpf_randinit (lua_State *L) {
  gmp_randinit_default(randomstate);
  return 0;
}


static int Mpfc_getparts (lua_State *L) {
  Mpfr *real, *imag;
  CMpfr *c = checkcmpfr(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");
  creatempf(real);
  mpfr_set_prec(real->val, mpfr_get_prec(c->real));  /* copy precision */
  Mpfr_set(real->val, c->real);
  creatempf(imag);
  mpfr_set_prec(imag->val, mpfr_get_prec(c->imag));
  Mpfr_set(imag->val, c->imag);
  return 2;
}


/*****************************************************************************************************

	Rounding Functions

******************************************************************************************************/

/* mpfr.ceil rounds up to to the next higher or equal integer */
static int Mpf_ceil (lua_State *L) {
  template_1arg_1ret(Mpfr_ceil);
  return 1;
}


/* mpfr.trunc rounds to the next integer toward zero */
static int Mpf_trunc (lua_State *L) {
  template_1arg_1ret(Mpfr_trunc);
  return 1;
}


/* mpfr.floor rounds to the next lower or equal integer */
static int Mpf_floor (lua_State *L) {
  template_1arg_1ret(Mpfr_floor);
  return 1;
}


/* mpfr.round rounds to the nearest integer, rounding halfway cases away from zero */
static int Mpf_round (lua_State *L) {
  template_1arg_1ret(Mpfr_round);
  return 1;
}


static int mt_int (lua_State *L) {  /* 6.4.4 */
  Mpfr *mx, *mr;
  mx = checkmpfr(L, 1);
  creatempf(mr);
  Mpfr_trunc(mr->val, mx->val);
  return 1;
}


static int mt_frac (lua_State *L) {  /* 6.4.4 */
  Mpfr *mx, *mr;
  mx = checkmpfr(L, 1);
  creatempf(mr);
  mpfr_trunc(mr->val, mx->val);
  mpfr_sub(mr->val, mx->val, mr->val, MPFR_ROUNDING);
  return 1;
}


static int Mpf_regular (lua_State *L) {  /* 6.4.4, UNDOC */
  lua_pushboolean(L, mpfr_regular_p(checkmpfr(L, 1)->val) != 0);
  return 1;
}


static int mt_integral (lua_State *L) {  /* 6.4.4 */
  Mpfr *x = checkmpfr(L, 1);
  lua_pushboolean(L, (mpfr_zero_p(x->val) != 0) || !(mpfr_regular_p(x->val) == 0 || mpfr_integer_p(x->val) == 0));
  return 1;
}


static int Mpfr_isfractional (Mpfr *mx) {  /* 6.4.4 */
  return !(mpfr_regular_p(mx->val) == 0 || mpfr_integer_p(mx->val) != 0);
}


static int mt_fractional (lua_State *L) {  /* 6.4.4 */
  Mpfr *mx;
  mx = checkmpfr(L, 1);
  lua_pushboolean(L, Mpfr_isfractional(mx));
  return 1;
}


static int mt_odd (lua_State *L) {  /* 6.4.4 */
  Mpfr *mx;
  mx = checkmpfr(L, 1);
  lua_pushboolean(L, mpfr_isodd(mx->val) == 1);
  return 1;
}


static int mt_even (lua_State *L) {  /* 6.4.4 */
  Mpfr *mx;
  mx = checkmpfr(L, 1);
  lua_pushboolean(L, mpfr_iseven(mx->val) == 1);
  return 1;
}

/* Returns the mantissa m and the exponent e, in this order, of the MPFR float x such that x = m*2^e. The return values
   m and e are both MPFR floats, with e representing an integer, and m a fractional value in the range [0.5, 1)
   (or zero when x is zero).
   The third result is an Agena number with the following meaning:
   zero: The computed mantissa () is exactly equal to the infinitely precise result.
   A positive value: The computed mantissa is greater than the infinitely precise result, so the result was rounded up.
   A negative value: The computed mantissa is less than the infinitely precise result, so the result was rounded down. 6.4.2 */
static int Mpf_frexp (lua_State *L) {
  int rc;
  Mpfr *exponent, *mantissa;
  mpfr_exp_t expo;
  luaL_checkstack(L, 3, "not enough stack space");
  creatempf(mantissa);
  rc = mpfr_frexp(&expo, mantissa->val, checkmpfr(L, 1)->val, MPFR_ROUNDING);
  creatempf(exponent);
  mpfr_set_si(exponent->val, expo, MPFR_ROUNDING);
  lua_pushnumber(L, rc);
  return 3;
}


/* Computes m*2^e, 6.4.2 */
static int Mpf_mul2exp (lua_State *L) {
  Mpfr *mr, *mx, *my;
  creatempf(mr);
  mx = checkmpfr(L, 1);
  my = checkmpfr(L, 2);
  mpfr_mul_2exp(mr->val, mx->val, mpfr_get_ui(my->val, MPFR_ROUNDING), MPFR_ROUNDING);
  return 1;
}


/* Computes m/2^e, 6.4.2 */
static int Mpf_div2exp (lua_State *L) {
  Mpfr *mr, *mx, *my;
  creatempf(mr);
  mx = checkmpfr(L, 1);
  my = checkmpfr(L, 2);
  mpfr_div_2exp(mr->val, mx->val, mpfr_get_ui(my->val, MPFR_ROUNDING), MPFR_ROUNDING);
  return 1;
}


/* minimum of a and b, 2.21.10, rewritten 6.4.2 */
static int Mpf_min (lua_State *L) {
  lua_pushvalue(L, 2 - (Mpfr_cmp(checkmpfr(L, 1)->val, checkmpfr(L, 2)->val) < 0));
  return 1;
}


/* maximum of a and b, 2.21.10, rewritten 6.4.2 */
static int Mpf_max (lua_State *L) {
  lua_pushvalue(L, 2 - (Mpfr_cmp(checkmpfr(L, 1)->val, checkmpfr(L, 2)->val) > 0));
  return 1;
}


static int Mpf_minmax (lua_State *L) {  /* 6.4.2 */
  Mpfr *a = checkmpfr(L, 1);
  Mpfr *b = checkmpfr(L, 2);
  int lt = Mpfr_cmp(a->val, b->val) < 0;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushvalue(L, 2 - lt);
  lua_pushvalue(L, 1 + lt);
  return 2;
}


/* mpfr.copysign(): like math.copysign. 2.21.10 */
static int Mpf_copysign (lua_State *L) {
  template_2args_1ret(Mpfr_copysign);
  return 1;
}


/* mpfr.signbit(): like math.signbit, i.e. checks the sign bit and returns `true`
  (value is negative) or `false`; 2.21.10 */
static int Mpf_signbit (lua_State *L) {
  Mpfr *mx;
  mx = checkmpfr(L, 1);
  lua_pushboolean(L, mpfr_signbit(mx->val));
  return 1;
}


static int mpfr_approx (mpfr_t a, mpfr_t b, mpfr_t eps) {  /* 6.5.16 */
  mpfr_t dist;
  mpfr_prec_t prec;
  int rc;
  prec = MPFR_PREC(eps) + 2;
  mpfr_init2(dist, prec);
  Mpfr_sub(dist, a, b);
  Mpfr_abs(dist, dist);
  if (mpfr_cmp(dist, eps) < 0) {  /* dist < eps ? */
    rc = 1;
  } else {  /* dist <= (eps * fMax(|a|, |b|)) */
    mpfr_t x, y, p;
    mpfr_init2(x, prec);
    mpfr_init2(y, prec);
    mpfr_init2(p, prec);
    mpfr_abs(x, a, MPFR_RNDN);
    mpfr_abs(y, b, MPFR_RNDN);
    mpfr_mul(p, (mpfr_cmp(x, y) > 0) ? x : y, eps, MPFR_ROUNDING);
    rc = mpfr_cmp(dist, p) <= 0;
    mpfr_clears(x, y, p, MPFR_NULL);
  }
  mpfr_clear(dist);
  return rc;
}


static int Mpf_approx (lua_State *L) {  /* 6.5.6 */
  Mpfr *eps;
  if (lua_gettop(L) == 2) {
    Mpfr *t;
    /* first increase stack top, then replace, otherwise it won't work
       without calling the function once again */
    lua_settop(L, 3);
    creatempf(t);
    Mpfr_set_d(t->val, agn_getepsilon(L));
    lua_replace(L, 3);
  }
  eps = checkmpfr(L, 3);
  if ((ismpf(L, 1) || agn_isnumber(L, 1)) && (ismpf(L, 2) || agn_isnumber(L, 2))) {
    Mpfr *a, *b;
    a = (Mpfr *)checkmpfr(L, 1);
    b = (Mpfr *)checkmpfr(L, 2);
    lua_pushboolean(L, mpfr_approx(a->val, b->val, eps->val));
  } else if (iscmpf(L, 1) && iscmpf(L, 2)) {  /* 6.5.16 extension */
    CMpfr *a, *b;
    a = (CMpfr *)lua_touserdata(L, 1);
    b = (CMpfr *)lua_touserdata(L, 2);
    lua_pushboolean(L,
      mpfr_approx(a->real, b->real, eps->val) &&
      mpfr_approx(a->imag, b->imag, eps->val)
    );
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr.approx");
  }
  return 1;
}


static int mt_aeq (lua_State *L) {  /* 7.5.2 */
  mpfr_t eps;
  Mpfr_init(eps);
  Mpfr_set_d(eps, agn_getepsilon(L));
  if ((ismpf(L, 1) || agn_isnumber(L, 1)) && (ismpf(L, 2) || agn_isnumber(L, 2))) {
    Mpfr *a, *b;
    a = (Mpfr *)checkmpfr(L, 1);
    b = (Mpfr *)checkmpfr(L, 2);
    lua_pushboolean(L, mpfr_approx(a->val, b->val, eps));
  } else if (iscmpf(L, 1) && iscmpf(L, 2)) {
    CMpfr *a, *b;
    a = (CMpfr *)lua_touserdata(L, 1);
    b = (CMpfr *)lua_touserdata(L, 2);
    lua_pushboolean(L,
      mpfr_approx(a->real, b->real, eps) &&
      mpfr_approx(a->imag, b->imag, eps)
    );
  } else {
    mpfr_clear(eps);
    luaL_error(L, "Error in " LUA_QS ": invalid (c)mpf object.", "mpfr/aeq mt");
  }
  mpfr_clear(eps);
  return 1;
}


/* ****************************************************************************************************

  Complex Arithmetic

* *****************************************************************************************************/

/* *****************************************************************************************************
   Complex helper functions
*  ****************************************************************************************************/

/* AUX: proposed by Gemini AI, 6.6.0 */
static void mpfr_csgn (mpfr_t rr, mpfr_t a, mpfr_t b) {
  mpfr_t Mpfr_Zero;
  mpfr_prec_t prec = mpfr_get_prec(a) + 2;
  mpfr_init2(Mpfr_Zero, prec);
  Mpfr_set_d(Mpfr_Zero, 0.0);
  int acomp = Mpfr_cmp(a, Mpfr_Zero);
  int bcomp = Mpfr_cmp(b, Mpfr_Zero);
  if (acomp > 0 || (acomp == 0 && bcomp > 0))
    Mpfr_set_d(rr, 1.0);
  else if (acomp < 0 || (acomp == 0 && bcomp < 0))
    Mpfr_set_d(rr, -1.0);
  else
    Mpfr_set_d(rr, 0.0);
  mpfr_clear(Mpfr_Zero);
}

static int mpfr_tools_csgn (mpfr_srcptr a, mpfr_srcptr b) {
  if (mpfr_sgn(a) > 0 || (mpfr_sgn(a) == 0 && mpfr_sgn(b) > 0))
    return 1;
  if (mpfr_sgn(a) < 0 || (mpfr_sgn(a) == 0 && mpfr_sgn(b) < 0))
    return -1;
  return 0;
}

/* AUX: Created by Gemini AI, 6.6.1 */
static void mpfr_carcsin (mpfr_t rx, mpfr_t ry, mpfr_t a, mpfr_t b, mpfr_prec_t prec) {
  if (mpfr_zero_p(b)) {
    mpfr_asin(rx, a, MPFR_RNDN);
    mpfr_set_zero(ry, 1);
    return;
  }
  mpfr_t t1, t2, am, bm, alpha, beta;
  mpfr_inits2(prec, t1, t2, am, bm, alpha, beta, MPFR_NULL);
  mpfr_abs(am, a, MPFR_RNDN);
  mpfr_abs(bm, b, MPFR_RNDN);
  /* Foci for arcsin are at +/- 1 on the REAL axis
     t1 = sqrt((am + 1)^2 + bm^2) */
  mpfr_add_ui(t1, am, 1, MPFR_RNDN);
  mpfr_hypot(t1, t1, bm, MPFR_RNDN);
  /* t2 = sqrt((am - 1)^2 + bm^2) */
  mpfr_sub_ui(t2, am, 1, MPFR_RNDN);
  mpfr_hypot(t2, t2, bm, MPFR_RNDN);
  /* alpha = 0.5 * (t1 + t2), beta = 0.5 * (t1 - t2) */
  mpfr_add(alpha, t1, t2, MPFR_RNDN);
  mpfr_div_2ui(alpha, alpha, 1, MPFR_RNDN);
  mpfr_sub(beta, t1, t2, MPFR_RNDN);
  mpfr_div_2ui(beta, beta, 1, MPFR_RNDN);
  /* REAL PART: rx = sign(a) * asin(beta) */
  mpfr_asin(rx, beta, MPFR_RNDN);
  if (mpfr_sgn(a) < 0) mpfr_neg(rx, rx, MPFR_RNDN);
  /* IMAGINARY PART: ry = sign(b) * acosh(alpha) */
  mpfr_acosh(ry, alpha, MPFR_RNDN);
  if (mpfr_sgn(b) < 0) mpfr_neg(ry, ry, MPFR_RNDN);
  mpfr_set(rx, rx, MPFR_ROUNDING);
  mpfr_set(ry, ry, MPFR_ROUNDING);
  mpfr_clears(t1, t2, am, bm, alpha, beta, MPFR_NULL);
}


/* AUX: -I*arcsin(I*z), 100 % boost by Gemini AI, 6.6.1, Hull's Method */
static void mpfr_carcsinh (mpfr_t rx, mpfr_t ry, mpfr_t a, mpfr_t b, mpfr_prec_t prec) {
  if (mpfr_zero_p(b)) {
    mpfr_asinh(rx, a, MPFR_RNDN);
    mpfr_set_zero(ry, 1);
    return;
  }
  mpfr_t t1, t2, am, bm, alpha, beta;
  mpfr_inits2(prec, t1, t2, am, bm, alpha, beta, MPFR_NULL);
  mpfr_abs(am, a, MPFR_RNDN);  /* |x| */
  mpfr_abs(bm, b, MPFR_RNDN);  /* |y| */
  /* For asinh, the foci are at +/- i.
     We use the identity based on: sqrt(x^2 + (y+1)^2) and sqrt(x^2 + (y-1)^2)
     t1 = sqrt(am^2 + (bm + 1)^2) */
  mpfr_add_ui(t1, bm, 1, MPFR_RNDN);
  mpfr_hypot(t1, am, t1, MPFR_RNDN);
  /* t2 = sqrt(am^2 + (bm - 1)^2) */
  mpfr_sub_ui(t2, bm, 1, MPFR_RNDN);
  mpfr_hypot(t2, am, t2, MPFR_RNDN);
  /* alpha = 0.5 * (t1 + t2) */
  mpfr_add(alpha, t1, t2, MPFR_RNDN);
  mpfr_div_2ui(alpha, alpha, 1, MPFR_RNDN);
  /* beta = 0.5 * (t1 - t2) */
  mpfr_sub(beta, t1, t2, MPFR_RNDN);
  mpfr_div_2ui(beta, beta, 1, MPFR_RNDN);
  /* REAL PART: rx = sign(x) * log(alpha + sqrt(alpha^2 - 1)) */
  mpfr_acosh(rx, alpha, MPFR_RNDN);
  if (mpfr_sgn(a) < 0) mpfr_neg(rx, rx, MPFR_RNDN);
  /* IMAGINARY PART: ry = sign(y) * asin(beta)
     For asinh, we use the sign of y. */
  mpfr_asin(ry, beta, MPFR_RNDN);
  if (mpfr_sgn(b) < 0) mpfr_neg(ry, ry, MPFR_RNDN);
  mpfr_set(rx, rx, MPFR_ROUNDING);
  mpfr_set(ry, ry, MPFR_ROUNDING);
  mpfr_clears(t1, t2, am, bm, alpha, beta, MPFR_NULL);
}


/* AUX: Generated by Gemini AI, 6.6.1, put to the public domain; extended by a_walz 6.6.2 */
static void mpfr_carctan (mpfr_t rx, mpfr_t ry, mpfr_t a, mpfr_t b, mpfr_prec_t prec) {
  mpfr_t t1, u, v, w;
  /* 1. Fast Path for Real Numbers (b == 0) */
  if (mpfr_zero_p(b)) {
    mpfr_atan(rx, a, MPFR_RNDN);
    mpfr_set_zero(ry, 1);  /* Imaginary part is exactly zero */
    return;
  }
  mpfr_inits2(prec, t1, u, v, w, MPFR_NULL);
  /* Purely Imaginary Path (a == 0) */
  if (mpfr_zero_p(a)) {
    mpfr_abs(t1, b, MPFR_RNDN);
    /* 2a. Singularity Check (a == 0 and |b| == 1) */
    if (mpfr_cmp_ui(t1, 1) == 0) {
      mpfr_set_nan(rx);
      mpfr_set_nan(ry);
    } else {
      /* 2b. else: (a == 0) and |b| <> 1 */
      mpfr_set_zero(rx, 1);
      /* ry = 0.5 * ln(|(b+1)/(b-1)|) */
      mpfr_add_ui(u, b, 1, MPFR_RNDN);
      mpfr_sub_ui(v, b, 1, MPFR_RNDN);
      mpfr_div(u, u, v, MPFR_RNDN);
      mpfr_abs(u, u, MPFR_RNDN);
      mpfr_log(u, u, MPFR_RNDN);
      mpfr_div_2ui(ry, u, 1, MPFR_ROUNDING);
    }
    goto cleanup;
  }
  /* 3. Precompute a^2 */
  mpfr_sqr(t1, a, MPFR_RNDN);
  /* --- Real Part (Maple/C99 Branch Cut Logic) --- */
  mpfr_ui_sub(u, 1, b, MPFR_RNDN);
  mpfr_atan2(v, a, u, MPFR_RNDN);
  mpfr_add_ui(u, b, 1, MPFR_RNDN);
  mpfr_neg(w, a, MPFR_RNDN);
  mpfr_atan2(w, w, u, MPFR_RNDN);
  mpfr_sub(rx, v, w, MPFR_RNDN);
  mpfr_div_2ui(rx, rx, 1, MPFR_RNDN);
  /* --- Imaginary Part --- */
  /* num = a^2 + (b+1)^2 */
  mpfr_add_ui(u, b, 1, MPFR_RNDN);
  mpfr_sqr(u, u, MPFR_RNDN);
  mpfr_add(u, u, t1, MPFR_RNDN);
  /* den = a^2 + (b-1)^2 */
  mpfr_sub_ui(v, b, 1, MPFR_RNDN);
  mpfr_sqr(v, v, MPFR_RNDN);
  mpfr_add(v, v, t1, MPFR_RNDN);
  mpfr_div(u, u, v, MPFR_RNDN);
  mpfr_log(u, u, MPFR_RNDN);
  mpfr_div_2ui(ry, u, 2, MPFR_ROUNDING);
cleanup:
  mpfr_clears(t1, u, v, w, MPFR_NULL);
}


/* Computes erf(z) for small/medium |z| using power series,
   created by Gemini AI, put into the public domain. 6.6.2 */
static void mpfr_cerf_series (mpfr_t res_re, mpfr_t res_im, mpfr_t z_re, mpfr_t z_im, mpfr_prec_t prec) {
  unsigned long n;
  mpfr_t term_re, term_im, sum_re, sum_im, z2_re, z2_im, next_re, next_im, tmp, const_factor;
  /* Initialize variables */
  mpfr_inits2(prec, term_re, term_im, sum_re, sum_im, z2_re, z2_im, next_re, next_im, tmp, const_factor, MPFR_NULL);
  /* z^2 = (z_re^2 - z_im^2) + i(2 * z_re * z_im) */
  mpfr_mul(tmp, z_re, z_re, MPFR_RNDN);      /* re^2 */
  mpfr_mul(z2_im, z_im, z_im, MPFR_RNDN);    /* im^2 */
  mpfr_sub(z2_re, tmp, z2_im, MPFR_RNDN);    /* re^2 - im^2 */
  mpfr_mul(z2_im, z_re, z_im, MPFR_RNDN);    /* re * im */
  mpfr_mul_ui(z2_im, z2_im, 2, MPFR_RNDN);  /* 2 * re * im */
  /* Initial term (n=0): term = z */
  mpfr_set(term_re, z_re, MPFR_RNDN);
  mpfr_set(term_im, z_im, MPFR_RNDN);
  mpfr_set(sum_re, term_re, MPFR_RNDN);
  mpfr_set(sum_im, term_im, MPFR_RNDN);
  /* Iteration loop */
  for (n=1; n < 10000; n++) {  /* Safety limit */
    /* term = term * (-z^2) / n
       Complex mul: (a+bi)(c+di) = (ac-bd) + i(ad+bc)
       Here: (term_re + i*term_im) * (-z2_re - i*z2_im) */
    mpfr_neg(next_re, z2_re, MPFR_RNDN);
    mpfr_neg(next_im, z2_im, MPFR_RNDN);
    /* Multiply term by -z^2 */
    mpfr_mul(tmp, term_re, next_re, MPFR_RNDN);
    mpfr_mul(next_re, term_im, next_im, MPFR_RNDN);
    mpfr_sub(next_re, tmp, next_re, MPFR_RNDN); /* New Real */
    mpfr_mul(tmp, term_re, z2_im, MPFR_RNDN);   /* Use original z2_im for ad+bc logic */
    mpfr_neg(tmp, tmp, MPFR_RNDN);
    mpfr_mul(next_im, term_im, z2_re, MPFR_RNDN);
    mpfr_neg(next_im, next_im, MPFR_RNDN);
    mpfr_add(next_im, tmp, next_im, MPFR_RNDN);  /* New Imag */
    mpfr_div_ui(term_re, next_re, n, MPFR_RNDN);
    mpfr_div_ui(term_im, next_im, n, MPFR_RNDN);
    /* Add to sum: term / (2n + 1) */
    mpfr_div_ui(next_re, term_re, 2 * n + 1, MPFR_RNDN);
    mpfr_div_ui(next_im, term_im, 2 * n + 1, MPFR_RNDN);
    mpfr_add(sum_re, sum_re, next_re, MPFR_RNDN);
    mpfr_add(sum_im, sum_im, next_im, MPFR_RNDN);
    /* Convergence check: if term is small enough, break */
    if (mpfr_get_exp(next_re) < -prec && mpfr_get_exp(next_im) < -prec) break;
  }
  /* Multiply sum by 2/sqrt(pi) */
  mpfr_const_pi(tmp, MPFR_RNDN);
  mpfr_sqrt(tmp, tmp, MPFR_RNDN);
  mpfr_ui_div(const_factor, 2, tmp, MPFR_RNDN);
  mpfr_mul(res_re, sum_re, const_factor, MPFR_RNDN);
  mpfr_mul(res_im, sum_im, const_factor, MPFR_ROUNDING);
  mpfr_clears(term_re, term_im, sum_re, sum_im, z2_re, z2_im, next_re, next_im, tmp, const_factor, MPFR_NULL);
}

#undef mpfr_const_log2  /* leave that here, otherwise the package will not compile */
/* Generated by Gemini AI, 6.6.2, put to the public domain */
static void mpfr_cexp2 (mpfr_t res_re, mpfr_t res_im, mpfr_t z_re, mpfr_t z_im, mpfr_prec_t prec) {
  mpfr_t ln2, exponent_im, magnitude, sin_val, cos_val;
  mpfr_inits2(prec, ln2, exponent_im, magnitude, sin_val, cos_val, MPFR_NULL);
  /* 1. Get ln(2) */
  mpfr_const_log2(ln2, MPFR_RNDN);
  /* 2. Calculate the real magnitude: 2^x = exp(x * ln(2))
     Optimization: MPFR has a native mpfr_exp2 for the real part! */
  mpfr_exp2(magnitude, z_re, MPFR_RNDN);
  /* 3. Calculate the phase: y * ln(2) */
  mpfr_mul(exponent_im, z_im, ln2, MPFR_RNDN);
  /* 4. Calculate sin and cos of the phase simultaneously */
  mpfr_sin_cos(sin_val, cos_val, exponent_im, MPFR_RNDN);
  /* 5. Finalize:
     Real part = 2^x * cos(y * ln 2)
     Imag part = 2^x * sin(y * ln 2) */
  mpfr_mul(res_re, magnitude, cos_val, MPFR_RNDN);
  mpfr_mul(res_im, magnitude, sin_val, MPFR_ROUNDING);
  mpfr_clears(ln2, exponent_im, magnitude, sin_val, cos_val, MPFR_NULL);
}

/* Based on mpfr_cexp2, 6.6.2 */
static void mpfr_cexp10 (mpfr_t res_re, mpfr_t res_im, mpfr_t z_re, mpfr_t z_im, mpfr_prec_t prec) {
  mpfr_t ln10, exponent_im, magnitude, sin_val, cos_val;
  mpfr_inits2(prec, ln10, exponent_im, magnitude, sin_val, cos_val, MPFR_NULL);
  /* 1. Get ln(10) */
  mpfr_log_ui(ln10, 10, MPFR_RNDN);
  /* 2. Calculate the real magnitude: 2^x = exp(x * ln(10))
     Optimization: MPFR has a native mpfr_exp10 for the real part! */
  mpfr_exp10(magnitude, z_re, MPFR_RNDN);
  /* 3. Calculate the phase: y * ln(10) */
  mpfr_mul(exponent_im, z_im, ln10, MPFR_RNDN);
  /* 4. Calculate sin and cos of the phase simultaneously */
  mpfr_sin_cos(sin_val, cos_val, exponent_im, MPFR_RNDN);
  /* 5. Finalize:
     Real part = 2^x * cos(y * ln 2)
     Imag part = 2^x * sin(y * ln 2) */
  mpfr_mul(res_re, magnitude, cos_val, MPFR_RNDN);
  mpfr_mul(res_im, magnitude, sin_val, MPFR_ROUNDING);
  mpfr_clears(ln10, exponent_im, magnitude, sin_val, cos_val, MPFR_NULL);
}

/* Generated by Gemini AI, 6.6.2, put to the public domain */
static void mpfr_clog2 (mpfr_t res_re, mpfr_t res_im, mpfr_t z_re, mpfr_t z_im, mpfr_prec_t prec) {
  mpfr_t r2, t_im2, ln2, angle;
  /* Initialize temps */
  mpfr_inits2(prec, r2, t_im2, ln2, angle, MPFR_NULL);
  /* 1. Get ln(2) constant */
  mpfr_const_log2(ln2, MPFR_RNDN);
  /* 2. Calculate Real Part: log2(|z|)
     Strategy: log2(sqrt(x^2 + y^2)) = 0.5 * log2(x^2 + y^2) */
  mpfr_sqr(r2, z_re, MPFR_RNDN);  /* x^2 */
  mpfr_sqr(t_im2, z_im, MPFR_RNDN);  /* y^2 */
  mpfr_add(r2, r2, t_im2, MPFR_RNDN);  /* r2 = x^2 + y^2 */
  mpfr_log2(res_re, r2, MPFR_RNDN);  /* log2(x^2 + y^2) */
  mpfr_mul_d(res_re, res_re, 0.5, MPFR_RNDN);  /* 0.5 * log2(r2) */
  /* 3. Calculate Imaginary Part: atan2(y, x) / ln(2) */
  mpfr_atan2(angle, z_im, z_re, MPFR_RNDN);
  mpfr_div(res_im, angle, ln2, MPFR_ROUNDING);
  /* Cleanup */
  mpfr_clears(r2, ln2, angle, t_im2, MPFR_NULL);
}


static void mpfr_clog10 (mpfr_t res_re, mpfr_t res_im, mpfr_t z_re, mpfr_t z_im, mpfr_prec_t prec) {
  mpfr_t r2, t_im10, ln10, angle;
  mpfr_inits2(prec, r2, t_im10, ln10, angle, MPFR_NULL);
  mpfr_log_ui(ln10, 10, MPFR_RNDN);
  mpfr_sqr(r2, z_re, MPFR_RNDN);
  mpfr_sqr(t_im10, z_im, MPFR_RNDN);
  mpfr_add(r2, r2, t_im10, MPFR_RNDN);
  mpfr_log10(res_re, r2, MPFR_RNDN);
  mpfr_mul_d(res_re, res_re, 0.5, MPFR_RNDN);
  mpfr_atan2(angle, z_im, z_re, MPFR_RNDN);
  mpfr_div(res_im, angle, ln10, MPFR_ROUNDING);
  mpfr_clears(r2, ln10, angle, t_im10, MPFR_NULL);
}

/* Generated by Gemini AI, 6.6.2, put to the public domain */
static void mpfr_ccbrt (mpfr_t res_re, mpfr_t res_im, mpfr_t z_re, mpfr_t z_im, mpfr_prec_t prec) {
  mpfr_t r, theta, s, c, t_im2;
  mpfr_inits2(prec, r, theta, s, c, t_im2, MPFR_NULL);
  /* 1. Calculate magnitude r = sqrt(x^2 + y^2) */
  mpfr_mul(r, z_re, z_re, MPFR_RNDN);
  mpfr_mul(t_im2, z_im, z_im, MPFR_RNDN);
  mpfr_add(r, r, t_im2, MPFR_RNDN);
  mpfr_sqrt(r, r, MPFR_RNDN);
  /* 2. Calculate the cubic root of the magnitude: r^(1/3) */
  mpfr_cbrt(r, r, MPFR_RNDN);
  /* 3. Calculate the angle theta = atan2(y, x) */
  mpfr_atan2(theta, z_im, z_re, MPFR_RNDN);
  /* 4. Divide the angle by 3 */
  mpfr_div_ui(theta, theta, 3, MPFR_RNDN);
  /* 5. Simultaneous sine and cosine of (theta/3) */
  mpfr_sin_cos(s, c, theta, MPFR_RNDN);
  /* 6. Finalize: res = r_cbrt * (cos + i*sin) */
  mpfr_mul(res_re, r, c, MPFR_RNDN);
  mpfr_mul(res_im, r, s, MPFR_ROUNDING);
  mpfr_clears(r, theta, s, c, t_im2, MPFR_NULL);
}

/* ***************************************************************************************************************/
/* Complex implementations                                                                                       */
/* ***************************************************************************************************************/

static int Carccosh (lua_State *L) {  /* not optimisable */
  mpfr_t Mpfr_One;
  mpfr_prec_t prec;
  CMpfr *a, *z;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  Mpfr_init2_d(Mpfr_One, 1.0, prec);
  if (mpfr_sgn(a->imag) && Mpfr_cmp(a->real, Mpfr_One) >= 0) {
    mpfr_acosh(z->real, a->real, MPFR_RNDN);
    Mpfr_setzero(z->imag);
  } else {
    mpfr_t u, v, t2, t6, t7, t8, t9, t11, t13, t15, t9h, t11h, t16, Mpfr_Two;
    mpfr_inits2(prec, u, v, t2, t6, t7, t8, t9, t11, t13, t15, t9h, t11h, t16, MPFR_NULL);
    Mpfr_init2_d(Mpfr_Two, 2.0, prec);
    /* t2 = tools_csgn(b, 1 - a); */
    mpfr_sub(u, Mpfr_One, a->real, MPFR_RNDN);
    mpfr_csgn(t2, a->imag, u);
    /* t6 = a*a; */
    mpfr_mul(t6, a->real, a->real, MPFR_RNDN);
    /* t7 = b*b; */
    mpfr_mul(t7, a->imag, a->imag, MPFR_RNDN);
    /* t8 = t6 + t7 + 1.0; */
    mpfr_add(u, t6, t7, MPFR_RNDN);
    mpfr_add(t8, u, Mpfr_One, MPFR_RNDN);
    /* t9 = sqrt(2.0*a + t8); */
    mpfr_mul(u, Mpfr_Two, a->real, MPFR_RNDN);
    mpfr_add(v, u, t8, MPFR_RNDN);
    mpfr_sqrt(t9, v, MPFR_RNDN);
    /* t11 = sqrt(-2.0*a + t8); */
    mpfr_neg(v, u, MPFR_RNDN);
    mpfr_add(u, v, t8, MPFR_RNDN);
    mpfr_sqrt(t11, u, MPFR_RNDN);
    /* t13 = sun_pow(0.5*(t9 + t11), 2.0, 1); */
    mpfr_add(u, t9, t11, MPFR_RNDN);
    mpfr_div(v, u, Mpfr_Two, MPFR_RNDN);
    mpfr_sqr(t13, v, MPFR_RNDN);
    /* t15 = sqrt(t13 - 1.0); */
    mpfr_sub(u, t13, Mpfr_One, MPFR_RNDN);
    mpfr_sqrt(t15, u, MPFR_RNDN);
    /* t9h = 0.5*t9; */
    mpfr_div(t9h, t9, Mpfr_Two, MPFR_RNDN);
    /* t11h = 0.5*t11; */
    mpfr_div(t11h, t11, Mpfr_Two, MPFR_RNDN);
    /* t16 = t9h - t11h; */
    mpfr_sub(t16, t9h, t11h, MPFR_RNDN);
    /* real -t2*tools_csgn(-b, a)*sun_log(t9h + t11h + t15) */
    mpfr_neg(u, a->imag, MPFR_RNDN);
    mpfr_csgn(v, u, a->real);
    mpfr_mul(t7, t2, v, MPFR_RNDN);
    mpfr_neg(t6, t7, MPFR_RNDN);  /* t6 = -t2*tools_csgn(-b, a) */
    /* sun_log(t9h + t11h + t15) */
    mpfr_add(u, t9h, t11h, MPFR_RNDN);
    mpfr_add(v, u, t15, MPFR_RNDN);
    mpfr_log(t7, v, MPFR_RNDN);
    mpfr_mul(z->real, t6, t7, MPFR_RNDN);
    /* imag: t2*sun_acos(t16)); */
    mpfr_acos(u, t16, MPFR_RNDN);
    mpfr_mul(z->imag, t2, u, MPFR_RNDN);
    mpfr_clears(u, v, t2, t6, t7, t8, t9, t11, t13, t15, t9h, t11h, t16, Mpfr_Two, MPFR_NULL);
  }
  Mpfr_cfinalise(z);
  mpfr_clear(Mpfr_One);
  return 1;
}


static int Carctanh (lua_State *L) {  /* not optimisable */
  mpfr_t absa, Mpfr_One;
  mpfr_prec_t prec;
  CMpfr *a, *z;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  mpfr_init2(absa, prec);
  Mpfr_init2_d(Mpfr_One, 1.0, prec);
  Mpfr_abs(absa, a->real);
  if (mpfr_sgn(a->imag) && Mpfr_cmp(absa, Mpfr_One) <= 0) {
    mpfr_atanh(z->real, a->real, MPFR_RNDN);
    Mpfr_setzero(z->imag);
  } else {
    mpfr_t t1, t2, t3, t4, t6, u, v, w, Mpfr_Two, Mpfr_Four;
    mpfr_inits2(prec, t1, t2, t3, t4, t6, u, v, w, MPFR_NULL);
    Mpfr_init2_d(Mpfr_Two, 2.0, prec);
    Mpfr_init2_d(Mpfr_Four, 4.0, prec);
    /* t1 = a + 1.0; */
    mpfr_add(t1, a->real, Mpfr_One, MPFR_RNDN);
    /* t2 = t1*t1; */
    mpfr_mul(t2, t1, t1, MPFR_RNDN);
    /* t3 = b*b; */
    mpfr_mul(t3, a->imag, a->imag, MPFR_RNDN);
    /* t4 = a - 1.0; */
    mpfr_sub(t4, a->real, Mpfr_One, MPFR_RNDN);
    /* t6 = t4*t4; */
    mpfr_mul(t6, t4, t4, MPFR_RNDN);
    /* im = 0.5*(sun_atan2(b, t1) - sun_atan2(-b, 1.0 - a)); */
    /* u = sun_atan2(b, t1) */
    mpfr_atan2(u, a->imag, t1, MPFR_RNDN);
    /* v = sun_atan2(-b, 1.0 - a) */
    mpfr_neg(t1, a->imag, MPFR_RNDN);
    mpfr_sub(w, Mpfr_One, a->real, MPFR_RNDN);
    mpfr_atan2(v, t1, w, MPFR_RNDN);
    mpfr_sub(w, u, v, MPFR_RNDN);
    mpfr_div(u, w, Mpfr_Two, MPFR_RNDN);
    if (mpfr_sgn(a->real) > 0 && Mpfr_iszero(a->imag)) {
      mpfr_neg(v, u, MPFR_RNDN);
      mpfr_set(z->imag, v, MPFR_RNDN);
    } else
      mpfr_set(z->imag, u, MPFR_RNDN);
    /* 0.25*sun_log((t2 + t3)/(t6 + t3)) */
    mpfr_add(u, t2, t3, MPFR_RNDN);
    mpfr_add(v, t6, t3, MPFR_RNDN);
    mpfr_div(w, u, v, MPFR_RNDN);
    mpfr_log(u, w, MPFR_RNDN);
    mpfr_div(z->real, u, Mpfr_Four, MPFR_RNDN);
    mpfr_clears(t1, t2, t3, t4, t6, u, v, w, Mpfr_Two, Mpfr_Four, MPFR_NULL);
  }
  Mpfr_cfinalise(z);
  mpfr_clears(absa, Mpfr_One, MPFR_NULL);
  return 1;
}


static int Csinc (lua_State *L) {  /* 25% optimized by Gemini AI, 6.6.1 */
  CMpfr *a = checkcmpfr(L, 1);
  CMpfr *z;
  createcmpf(z);
  if (mpfr_zero_p(a->real) && mpfr_zero_p(a->imag)) {
    mpfr_set_ui(z->real, 1, MPFR_RNDN);
    mpfr_set_zero(z->imag, 1);
    return 1;
  }
  mpfr_prec_t prec = mpfr_get_prec(a->real) + 4;
  mpfr_set_prec(z->real, prec);
  mpfr_set_prec(z->imag, prec);
  /* We reuse variables aggressively: si/co for sin/cos and later for final Re/Im */
  mpfr_t si, co, sih, coh, denom;
  mpfr_inits2(prec, si, co, sih, coh, denom, MPFR_NULL);
  /* 1. Numerator: sin(x+iy) = sin(x)cosh(y) + i cos(x)sinh(y) */
  mpfr_sin_cos(si, co, a->real, MPFR_RNDN);
  mpfr_sinh_cosh(sih, coh, a->imag, MPFR_RNDN);
  mpfr_mul(sih, sih, co, MPFR_RNDN);  /* sih = Im(sin z) = cos(x)sinh(y) */
  mpfr_mul(si, si, coh, MPFR_RNDN);   /* si  = Re(sin z) = sin(x)cosh(y) */
  /* 2. Denominator: |z|^2 = x^2 + y^2 */
  mpfr_sqr(coh, a->real, MPFR_RNDN);
  mpfr_sqr(denom, a->imag, MPFR_RNDN);
  mpfr_add(denom, denom, coh, MPFR_RNDN);
  /* 3. Complex Division: (Re_sin + i Im_sin) / (x + iy)
     Formula: [(Re_sin * x + Im_sin * y) / denom] + i [(Im_sin * x - Re_sin * y) / denom]
     Real Part */
  mpfr_mul(coh, si, a->real, MPFR_RNDN);  /* Re_sin * x */
  mpfr_fma(z->real, sih, a->imag, coh, MPFR_RNDN);  /* + Im_sin * y */
  mpfr_div(z->real, z->real, denom, MPFR_RNDN);
  /* Imaginary Part */
  mpfr_mul(coh, sih, a->real, MPFR_RNDN);  /* Im_sin * x */
  mpfr_neg(si, si, MPFR_RNDN);             /* -Re_sin */
  mpfr_fma(z->imag, si, a->imag, coh, MPFR_RNDN); /* + (-Re_sin * y) */
  mpfr_div(z->imag, z->imag, denom, MPFR_RNDN);
  Mpfr_cfinalise(z);
  mpfr_clears(si, co, sih, coh, denom, MPFR_NULL);
  return 1;
}


static int Mpf_cosc (lua_State *L) {  /* 25% optimized by Gemini AI, 6.6.1 */
  if (ismpf(L, 1)) {
    Mpfr *a = (Mpfr *)lua_touserdata(L, 1);
    if (Mpfr_iszero(a->val)) {
      lua_pushundefined(L);  /* 6.3.9 */
    } else {
      Mpfr *mr;
      mpfr_t co;
      mpfr_init2(co, mpfr_get_prec(a->val) + EXTRABITS);
      creatempf(mr);
      mpfr_cos(co, a->val, MPFR_RNDN);
      mpfr_div(mr->val, co, a->val, MPFR_ROUNDING);
      mpfr_clear(co);
    }
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_cosc);
  } else {
    /* Complex Case */
    CMpfr *a = checkcmpfr(L, 1);
    CMpfr *z;
    createcmpf(z);
    if (mpfr_zero_p(a->real) && mpfr_zero_p(a->imag)) {
      /* cos(0)/0 is undefined/infinite. Returning NaN as per your undefined logic. */
      mpfr_set_nan(z->real);
      mpfr_set_nan(z->imag);
      return 1;
    }
    mpfr_prec_t prec = mpfr_get_prec(a->real) + 4;
    mpfr_set_prec(z->real, prec);
    mpfr_set_prec(z->imag, prec);
    mpfr_t si, co, sih, coh, denom;
    mpfr_inits2(prec, si, co, sih, coh, denom, MPFR_NULL);
    /* 1. Numerator: cos(x + iy) = cos(x)cosh(y) - i sin(x)sinh(y) */
    mpfr_sin_cos(si, co, a->real, MPFR_RNDN);
    mpfr_sinh_cosh(sih, coh, a->imag, MPFR_RNDN);
    /* Re_cos = cos(x)cosh(y)
       Im_cos = -sin(x)sinh(y) */
    mpfr_mul(co, co, coh, MPFR_RNDN);
    mpfr_mul(si, si, sih, MPFR_RNDN);
    mpfr_neg(si, si, MPFR_RNDN);
    /* 2. Denominator: |z|^2 = x^2 + y^2 */
    mpfr_sqr(coh, a->real, MPFR_RNDN);
    mpfr_sqr(denom, a->imag, MPFR_RNDN);
    mpfr_add(denom, denom, coh, MPFR_RNDN);
    /* 3. Complex Division: (Re_cos + i Im_cos) / (x + iy)
       Formula: [(Re_cos * x + Im_cos * y) / denom] + i [(Im_cos * x - Re_cos * y) / denom]
       Real Part: (Re_cos * x + Im_cos * y) / denom */
    mpfr_mul(coh, co, a->real, MPFR_RNDN);
    mpfr_fma(z->real, si, a->imag, coh, MPFR_RNDN);
    mpfr_div(z->real, z->real, denom, MPFR_RNDN);
    /* Imaginary Part: (Im_cos * x - Re_cos * y) / denom */
    mpfr_mul(coh, si, a->real, MPFR_RNDN);
    mpfr_neg(sih, co, MPFR_RNDN); /* -Re_cos */
    mpfr_fma(z->imag, sih, a->imag, coh, MPFR_RNDN);
    mpfr_div(z->imag, z->imag, denom, MPFR_RNDN);
    Mpfr_cfinalise(z);
    mpfr_clears(si, co, sih, coh, denom, MPFR_NULL);
  }
  return 1;
}


static int Mpf_tanc (lua_State *L) {  /* 30% optimized by Gemini AI, 6.6.1 */
  if (ismpf(L, 1)) {
    Mpfr *a = (Mpfr *)lua_touserdata(L, 1);
    if (Mpfr_iszero(a->val)) {
      lua_pushundefined(L);  /* 6.3.9 */
    } else {
      Mpfr *mr;
      mpfr_t ta;
      mpfr_init2(ta, mpfr_get_prec(a->val) + EXTRABITS);
      creatempf(mr);
      mpfr_tan(ta, a->val, MPFR_RNDN);
      mpfr_div(mr->val, ta, a->val, MPFR_ROUNDING);
      mpfr_clear(ta);
    }
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_tanc);
  } else {
    CMpfr *a, *z;
    mpfr_prec_t prec;
    a = checkcmpfr(L, 1);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    /* if ((a) == 0 && (b) == 0) */
    if (Mpfr_iszero(a->imag)) {
      if (Mpfr_iszero(a->real)) {
        Mpfr_set_one(z->real);
      } else {
        mpfr_t u;
        mpfr_init2(u, prec);
        mpfr_tan(u, a->real, MPFR_RNDN);
        mpfr_div(z->real, u, a->real, MPFR_RNDN);
        mpfr_clear(u);
      }
      Mpfr_setzero(z->imag);
    } else {
      /* Using 8 variables to ensure we don't overwrite sin/cos results */
      mpfr_t x2, y2, s2x, c2x, sh2y, ch2y, den_tan, den_div;
      mpfr_inits2(prec, x2, y2, s2x, c2x, sh2y, ch2y, den_tan, den_div, MPFR_NULL);
      /* 1. Prepare double angles */
      mpfr_mul_2ui(x2, a->real, 1, MPFR_RNDN);
      mpfr_mul_2ui(y2, a->imag, 1, MPFR_RNDN);
      /* 2. Trigonometric components */
      mpfr_sin_cos(s2x, c2x, x2, MPFR_RNDN);      /* sin(2x), cos(2x) */
      mpfr_sinh_cosh(sh2y, ch2y, y2, MPFR_RNDN);  /* sinh(2y), cosh(2y) */
      /* 3. tan(z) denominator: cos(2x) + cosh(2y) */
      mpfr_add(den_tan, c2x, ch2y, MPFR_RNDN);
      /* 4. Combined scalar denominator: (x^2 + y^2) * den_tan */
      mpfr_sqr(x2, a->real, MPFR_RNDN);
      mpfr_sqr(y2, a->imag, MPFR_RNDN);
      mpfr_add(den_div, x2, y2, MPFR_RNDN);
      mpfr_mul(den_div, den_div, den_tan, MPFR_RNDN);
      /* 5. Complex Division Numerators via FMA
         Re = (sin(2x)*x + sinh(2y)*y) / den_div */
      mpfr_mul(x2, s2x, a->real, MPFR_RNDN);           /* sin(2x)*x */
      mpfr_fma(z->real, sh2y, a->imag, x2, MPFR_RNDN); /* + sinh(2y)*y */
      mpfr_div(z->real, z->real, den_div, MPFR_RNDN);
      /* Im = (sinh(2y)*x - sin(2x)*y) / den_div */
      mpfr_mul(y2, sh2y, a->real, MPFR_RNDN);          /* sinh(2y)*x */
      mpfr_neg(s2x, s2x, MPFR_RNDN);                   /* -sin(2x) */
      mpfr_fma(z->imag, s2x, a->imag, y2, MPFR_RNDN);  /* - sin(2x)*y */
      mpfr_div(z->imag, z->imag, den_div, MPFR_RNDN);
      mpfr_clears(x2, y2, s2x, c2x, sh2y, ch2y, den_tan, den_div, MPFR_NULL);
    }
    Mpfr_cfinalise(z);
  }
  return 1;
}


static int Mpf_csc (lua_State *L) {  /* 6.5.15, cannot be optimised */
  if (ismpf(L, 1)) {
    Mpfr *mr, *a;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(mr);
    Mpfr_csc(mr->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_csc);
  } else {
    CMpfr *a, *z;
    mpfr_prec_t prec;
    a = checkcmpfr(L, 1);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    /* if (b == 0) */
    if (Mpfr_iszero(a->imag)) {
      /* agn_pushcomplex(L, 1/sun_sin(x), 0); */
      mpfr_t si;
      mpfr_init2(si, prec);
      mpfr_sin(si, a->real, MPFR_RNDN);
      if (Mpfr_iszero(si)) {
        Mpfr_setnan(z->real);
        Mpfr_setnan(z->imag);
      } else {
        mpfr_recip(z->real, si, MPFR_RNDN);
        Mpfr_setzero(z->imag);
      }
      mpfr_clear(si);
    } else {
      mpfr_t t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w;
      mpfr_inits2(prec, t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w, MPFR_NULL);
      /* sun_sincos(a, &t1, &t7); */
      mpfr_sin_cos(t1, t7, a->real, MPFR_RNDN);
      /* luai_numsinhcosh(b, &t9, &t2); */
      mpfr_sinh(t9, a->imag, MPFR_RNDN);
      mpfr_cosh(t2, a->imag, MPFR_RNDN);
      /* t4 = t1*t1; */
      mpfr_mul(t4, t1, t1, MPFR_RNDN);
      /* t5 = t2*t2; */
      mpfr_mul(t5, t2, t2, MPFR_RNDN);
      /* t8 = t7*t7; */
      mpfr_mul(t8, t7, t7, MPFR_RNDN);
      /* t10 = t9*t9; */
      mpfr_mul(t10, t9, t9, MPFR_RNDN);
      /* t13 = 1/(t4*t5 + t8*t10); */
      mpfr_mul(u, t4, t5, MPFR_RNDN);
      mpfr_mul(v, t8, t10, MPFR_RNDN);
      mpfr_add(w, u, v, MPFR_RNDN);
      mpfr_recip(t13, w, MPFR_RNDN);
      /* re = t1*t2*t13 */
      mpfr_mul(u, t1, t2, MPFR_RNDN);
      mpfr_mul(z->real, u, t13, MPFR_RNDN);
      /* im = -t7*t9*t13 */
      mpfr_mul(u, t7, t9, MPFR_RNDN);
      mpfr_mul(v, u, t13, MPFR_RNDN);
      mpfr_neg(z->imag, v, MPFR_RNDN);
      mpfr_clears(t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w, MPFR_NULL);
    }
    Mpfr_cfinalise(z);
  }
  return 1;
}


static int Mpf_sec (lua_State *L) {  /* 3.5.2 */
  if (ismpf(L, 1)) {
    Mpfr *mr, *a;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(mr);
    Mpfr_sec(mr->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_sec);
  } else {
    CMpfr *a, *z;
    mpfr_prec_t prec;
    a = checkcmpfr(L, 1);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    /* if (b == 0) */
    if (Mpfr_iszero(a->imag)) {
      /* agn_pushcomplex(L, 1/sun_sin(x), 0); */
      mpfr_t co;
      mpfr_init2(co, prec);
      Mpfr_cos(co, a->real);
      if (Mpfr_iszero(co)) {
        Mpfr_setnan(z->real);
        Mpfr_setnan(z->imag);
      } else {
        mpfr_recip(z->real, co, MPFR_RNDN);
        Mpfr_setzero(z->imag);
      }
      mpfr_clear(co);
    } else {
      mpfr_t t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w;
      mpfr_inits2(prec, t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w, MPFR_NULL);
      /* sun_sincos(a, &t7, &t1); */
      mpfr_sin_cos(t7, t1, a->real, MPFR_RNDN);
      /* luai_numsinhcosh(b, &t9, &t2); */
      mpfr_sinh(t9, a->imag, MPFR_RNDN);
      mpfr_cosh(t2, a->imag, MPFR_RNDN);
      /* t4 = t1*t1; */
      mpfr_mul(t4, t1, t1, MPFR_RNDN);
      /* t5 = t2*t2; */
      mpfr_mul(t5, t2, t2, MPFR_RNDN);
      /* t8 = t7*t7; */
      mpfr_mul(t8, t7, t7, MPFR_RNDN);
      /* t10 = t9*t9; */
      mpfr_mul(t10, t9, t9, MPFR_RNDN);
      /* t13 = 1/(t4*t5 + t8*t10); */
      mpfr_mul(u, t4, t5, MPFR_RNDN);
      mpfr_mul(v, t8, t10, MPFR_RNDN);
      mpfr_add(w, u, v, MPFR_RNDN);
      mpfr_recip(t13, w, MPFR_RNDN);
      /* re = t1*t2*t13 */
      mpfr_mul(u, t1, t2, MPFR_RNDN);
      mpfr_mul(z->real, u, t13, MPFR_RNDN);
      /* im = t7*t9*t13 */
      mpfr_mul(u, t7, t9, MPFR_RNDN);
      mpfr_mul(z->imag, u, t13, MPFR_RNDN);
      mpfr_clears(t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w, MPFR_NULL);
    }
    Mpfr_cfinalise(z);
  }
  return 1;
}


static int Mpf_cot (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *mr, *a;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(mr);
    Mpfr_cot(mr->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_cot);
  } else {
    CMpfr *a, *z;
    mpfr_prec_t prec;
    a = checkcmpfr(L, 1);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    /* if (b == 0) */
    if (Mpfr_iszero(a->imag)) {
      /* agn_pushcomplex(L, -sun_tan(PIO2 + x), 0); */
      mpfr_t u, v, Mpfr_Two, pi, Mpfr_HALF_PI;
      mpfr_init2(u, prec);
      mpfr_init2(v, prec);
      Mpfr_init2_d(Mpfr_Two, 2.0, prec);
      mpfr_init2(pi, prec);
      mpfr_init2(Mpfr_HALF_PI, prec);
      mpfr_const_pi(pi, MPFR_RNDN);
      mpfr_div(Mpfr_HALF_PI, pi, Mpfr_Two, MPFR_RNDN);
      mpfr_add(u, Mpfr_HALF_PI, a->real, MPFR_RNDN);
      mpfr_tan(v, u, MPFR_RNDN);
      mpfr_neg(z->real, v, MPFR_RNDN);
      Mpfr_setzero(z->imag);
      mpfr_clears(u, v, Mpfr_Two, pi, Mpfr_HALF_PI, MPFR_NULL);
    } else {
      mpfr_t t1, t4, t5, t6, t8, co, coh, u, v;
      mpfr_inits2(prec, t1, t4, t5, t6, t8, co, coh, u, v, MPFR_NULL);
      /* sun_sincos(a, &t1, &co); */
      mpfr_sin_cos(t1, co, a->real, MPFR_RNDN);
      /* luai_numsinhcosh(b, &t5, &coh); */
      mpfr_sinh(t5, a->imag, MPFR_RNDN);
      mpfr_cosh(coh, a->imag, MPFR_RNDN);
      /* t4 = t1*t1; */
      mpfr_mul(t4, t1, t1, MPFR_RNDN);
      /* t6 = t5*t5; */
      mpfr_mul(t6, t5, t5, MPFR_RNDN);
      /* t8 = 1/(t4 + t6); */
      mpfr_add(u, t4, t6, MPFR_RNDN);
      mpfr_recip(t8, u, MPFR_RNDN);
      /* re = t1*co*t8 */
      mpfr_mul(u, t1, co, MPFR_RNDN);
      mpfr_mul(z->real, u, t8, MPFR_RNDN);
      /* im = -t5*coh*t8 */
      mpfr_mul(u, t5, coh, MPFR_RNDN);
      mpfr_mul(v, u, t8, MPFR_RNDN);
      mpfr_neg(z->imag, v, MPFR_RNDN);
      mpfr_clears(t1, t4, t5, t6, t8, co, coh, u, v, MPFR_NULL);
    }
    Mpfr_cfinalise(z);
  }
  return 1;
}


static int Mpf_coth (lua_State *L) {  /* 33% optimized by Gemini AI, 6.6.1 */
  if (ismpf(L, 1)) {
    Mpfr *mr, *a;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(mr);
    Mpfr_coth(mr->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_coth);
  } else {
    CMpfr *a, *z;
    mpfr_prec_t prec;
    a = checkcmpfr(L, 1);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    /* if (b == 0) */
    if (Mpfr_iszero(a->imag)) {
      /* agn_pushcomplex(L, 1/luai_numtanh(x), 0); */
      mpfr_t u;
      mpfr_init2(u, prec);
      Mpfr_tanh(u, a->real);
      if (Mpfr_iszero(u)) {
        Mpfr_setnan(z->real);
        Mpfr_setnan(z->imag);
      } else {
        Mpfr_recip(z->real, u);
        Mpfr_setzero(z->imag);
      }
      mpfr_clear(u);
    } else {
      mpfr_t x2, y2, si, co, sih, coh, den;
      mpfr_inits2(prec, x2, y2, si, co, sih, coh, den, MPFR_NULL);
      /* 1. Prepare double angles: 2x and 2y */
      mpfr_mul_2ui(x2, a->real, 1, MPFR_RNDN);
      mpfr_mul_2ui(y2, a->imag, 1, MPFR_RNDN);
      /* 2. Compute components */
      mpfr_sin_cos(si, co, y2, MPFR_RNDN);     /* si = sin(2y), co = cos(2y) */
      mpfr_sinh_cosh(sih, coh, x2, MPFR_RNDN); /* sih = sinh(2x), coh = cosh(2x) */
      /* 3. Denominator: cosh(2x) - cos(2y) */
      mpfr_sub(den, coh, co, MPFR_RNDN);
      /* 4. Result: (sinh(2x) / den) - i(sin(2y) / den) */
      /* Check for division by zero (happens at 0, pi*i, etc.) */
      if (mpfr_zero_p(den)) {
        mpfr_set_nan(z->real);
        mpfr_set_nan(z->imag);
      } else {
        mpfr_div(z->real, sih, den, MPFR_RNDN);
        mpfr_div(z->imag, si, den, MPFR_RNDN);
        mpfr_neg(z->imag, z->imag, MPFR_RNDN);
      }
      mpfr_clears(x2, y2, si, co, sih, coh, den, MPFR_NULL);
    }
    Mpfr_cfinalise(z);
  }
  return 1;
}


static int Mpf_csch (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *mr, *a;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(mr);
    Mpfr_csch(mr->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_csch);
  } else {
    CMpfr *a, *z;
    mpfr_prec_t prec;
    a = checkcmpfr(L, 1);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    /* if (b == 0) */
    if (Mpfr_iszero(a->imag)) {
      mpfr_t u;
      mpfr_init2(u, prec);
      Mpfr_sinh(u, a->real);
      if (Mpfr_iszero(u)) {
        Mpfr_setnan(z->real);
        Mpfr_setnan(z->imag);
      } else {
        Mpfr_recip(z->real, u);
        Mpfr_setzero(z->imag);
      }
      mpfr_clear(u);
    } else {
      mpfr_t t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w;
      mpfr_inits2(prec, t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w, MPFR_NULL);
      /* t1 = sinh(a); t2 = cos(b); t7 = cosh(a); t9 = sin(b); */
      mpfr_sin_cos(t9, t2, a->imag, MPFR_RNDN);
      mpfr_sinh(t1, a->real, MPFR_RNDN);
      mpfr_cosh(t7, a->real, MPFR_RNDN);
      /* t4 = t1*t1; */
      mpfr_mul(t4, t1, t1, MPFR_RNDN);
      /* t5 = t2*t2; */
      mpfr_mul(t5, t2, t2, MPFR_RNDN);
      /* t8 = t7*t7; */
      mpfr_mul(t8, t7, t7, MPFR_RNDN);
      /* t10 = t9*t9; */
      mpfr_mul(t10, t9, t9, MPFR_RNDN);
      /* t13 = 1/(t4*t5+t8*t10); */
      mpfr_mul(u, t4, t5, MPFR_RNDN);
      mpfr_mul(v, t8, t10, MPFR_RNDN);
      mpfr_add(w, u, v, MPFR_RNDN);
      mpfr_recip(t13, w, MPFR_RNDN);
      /* re = t1*t2*t13 */
      mpfr_mul(u, t1, t2, MPFR_RNDN);
      mpfr_mul(z->real, u, t13, MPFR_RNDN);
      /* im = -t7*t9*t13; */
      mpfr_mul(u, t7, t9, MPFR_RNDN);
      mpfr_mul(v, u, t13, MPFR_RNDN);
      mpfr_neg(z->imag, v, MPFR_RNDN);
      mpfr_clears(t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w, MPFR_NULL);
    }
    Mpfr_cfinalise(z);
  }
  return 1;
}


static int Mpf_sech (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *mr, *a;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(mr);
    Mpfr_sech(mr->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_sech);
  } else {
    CMpfr *a, *z;
    mpfr_prec_t prec;
    a = checkcmpfr(L, 1);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    /* if (b == 0) */
    if (Mpfr_iszero(a->imag)) {
      mpfr_t u;
      mpfr_init2(u, prec);
      Mpfr_cosh(u, a->real);
      if (Mpfr_iszero(u)) {
        Mpfr_setnan(z->real);
        Mpfr_setnan(z->imag);
      } else {
        Mpfr_recip(z->real, u);
        Mpfr_setzero(z->imag);
      }
      mpfr_clear(u);
    } else {
      mpfr_t t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w;
      mpfr_inits2(prec, t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w, MPFR_NULL);
      /* t1 = cosh(a); t2 = cos(b); t7 = sinh(a); t9 = sin(b); */
      mpfr_sin_cos(t9, t2, a->imag, MPFR_RNDN);
      mpfr_sinh(t7, a->real, MPFR_RNDN);
      mpfr_cosh(t1, a->real, MPFR_RNDN);
      /* t4 = t1*t1; */
      mpfr_mul(t4, t1, t1, MPFR_RNDN);
      /* t5 = t2*t2; */
      mpfr_mul(t5, t2, t2, MPFR_RNDN);
      /* t8 = t7*t7; */
      mpfr_mul(t8, t7, t7, MPFR_RNDN);
      /* t10 = t9*t9; */
      mpfr_mul(t10, t9, t9, MPFR_RNDN);
      /* t13 = 1/(t4*t5+t8*t10); */
      mpfr_mul(u, t4, t5, MPFR_RNDN);
      mpfr_mul(v, t8, t10, MPFR_RNDN);
      mpfr_add(w, u, v, MPFR_RNDN);
      mpfr_recip(t13, w, MPFR_RNDN);
      /* re = t1*t2*t13 */
      mpfr_mul(u, t1, t2, MPFR_RNDN);
      mpfr_mul(z->real, u, t13, MPFR_RNDN);
      /* im = -t7*t9*t13; */
      mpfr_mul(u, t7, t9, MPFR_RNDN);
      mpfr_mul(v, u, t13, MPFR_RNDN);
      mpfr_neg(z->imag, v, MPFR_RNDN);
      mpfr_clears(t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w, MPFR_NULL);
    }
    Mpfr_cfinalise(z);
  }
  return 1;
}


/* Generated by Gemini AI, put to the public domain, 6.6.1 */
static int Mpf_arccsc (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *mx, *mr;
    mx = (Mpfr *)lua_touserdata(L, 1);
    creatempf(mr);
    if (mpfr_zero_p(mx->val)) {
      mpfr_set_nan(mr->val);
    } else {
      /* acsc(x) = asin(1/x) */
      mpfr_ui_div(mr->val, 1, mx->val, MPFR_RNDN);
      Mpfr_asin(mr->val, mr->val);
    }
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_arccsc);
  } else {
    CMpfr *a, *z;
    mpfr_prec_t prec;
    a = checkcmpfr(L, 1);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    if (mpfr_zero_p(a->real) && mpfr_zero_p(a->imag)) {
      mpfr_set_nan(z->real);
      mpfr_set_nan(z->imag);
      return 1;
    }
    mpfr_t re, im, den;
    mpfr_inits2(prec, re, im, den, MPFR_NULL);
    /* 1. Complex Reciprocal: 1 / (a + bi) */
    mpfr_sqr(re, a->real, MPFR_RNDN);
    mpfr_sqr(im, a->imag, MPFR_RNDN);
    mpfr_add(den, re, im, MPFR_RNDN);
    mpfr_div(re, a->real, den, MPFR_RNDN);  /* re = a/den */
    mpfr_div(im, a->imag, den, MPFR_RNDN);
    mpfr_neg(im, im, MPFR_RNDN);            /* im = -b/den */
    /* 2. Call the internal arcsin */
    mpfr_carcsin(z->real, z->imag, re, im, prec);
    mpfr_clears(re, im, den, MPFR_NULL);
    Mpfr_cfinalise(z);
  }
  return 1;
}


static int Mpf_arccsch (lua_State *L) {
  if (ismpf(L, 1)) {
    Mpfr *mx, *mr;
    mx = (Mpfr *)lua_touserdata(L, 1);
    creatempf(mr);
    if (Mpfr_iszero(mx->val)) {
      Mpfr_setnan(mr->val);  /* same as template_create_constant(Mpfr_set_nan) */
      /* cannot use MPFR_RET_NAN as it cannot be linked */
    } else {
      Mpfr_set_d(mr->val, 1.0);
      mpfr_div(mr->val, mr->val, mx->val, MPFR_RNDN);
      mpfr_asinh(mr->val, mr->val, MPFR_ROUNDING);
    }
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_arccsch);
  } else {  /* 6.6.1 extension, 13 % Gemini AI-optimised */
    CMpfr *a, *z;
    mpfr_prec_t prec;
    a = checkcmpfr(L, 1);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    mpfr_t re, im, den;
    mpfr_inits2(prec, re, im, den, MPFR_NULL);
    /* 1. Complex Reciprocal: 1 / (a + bi)
       den = a^2 + b^2 */
    mpfr_sqr(re, a->real, MPFR_RNDN);
    mpfr_sqr(im, a->imag, MPFR_RNDN);
    mpfr_add(den, re, im, MPFR_RNDN);
    /* re = a / den, im = -b / den */
    mpfr_div(re, a->real, den, MPFR_RNDN);
    mpfr_div(im, a->imag, den, MPFR_RNDN);
    mpfr_neg(im, im, MPFR_RNDN);
    /* 2. Call the optimized internal asinh
       Use the optimized logic we built earlier */
    mpfr_carcsinh(z->real, z->imag, re, im, prec);
    mpfr_clears(re, im, den, MPFR_NULL);
    Mpfr_cfinalise(z);
  }
  return 1;
}


static int Mpf_arcsech (lua_State *L) {  /* 3.4.2 */
  if (ismpf(L, 1)) {
    Mpfr *mx, *mr;
    mx = checkmpfr(L, 1);
    creatempf(mr);
    if (Mpfr_isneg(mx->val) || Mpfr_issingular(mx->val)) {
      if (Mpfr_iszero(mx->val) || Mpfr_isneg(mx->val) || Mpfr_isnan(mx->val)) {
        Mpfr_setnan(mr->val);  /* same as template_create_constant(Mpfr_set_nan) */
      } else {  /* inf */
        Mpfr_setinf(mr->val);
      }
    } else {
      mpfr_d_div(mr->val, 1.0, mx->val, MPFR_RNDN);
      Mpfr_acosh(mr->val, mr->val);
    }
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_arcsech);
  } else {  /* 6.6.1 extension, generated by Gemini AI, put into the public domain */
    mpfr_prec_t prec;
    CMpfr *a, *z;
    a = checkcmpfr(L, 1);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    mpfr_t t1, t2, t8, t10, t12, t13, t15, t17, t19, t21, t23, t25, t30, t31, tmp, tmp2;
    double t4; /* t4 is just a sign scalar (-1, 0, 1) */
    /* Initialize all variables */
    mpfr_inits2(prec, t1, t2, t8, t10, t12, t13, t15, t17, t19, t21, t23, t25, t30, t31, tmp, tmp2, MPFR_NULL);
    /* t1 = a*a; t2 = b*b; */
    mpfr_mul(t1, a->real, a->real, MPFR_RNDN);
    mpfr_mul(t2, a->imag, a->imag, MPFR_RNDN);
    /* t4 = tools_csgn(-b, -a + t1 + t2); */
    mpfr_neg(tmp, a->imag, MPFR_RNDN);  /* tmp = -b */
    mpfr_sub(tmp2, t1, a->real, MPFR_RNDN);
    mpfr_add(tmp2, tmp2, t2, MPFR_RNDN);  /* tmp2 = -a + t1 + t2 */
    t4 = (double)mpfr_tools_csgn(tmp, tmp2);
    /* t8 = t1 + t2; */
    mpfr_add(t8, t1, t2, MPFR_RNDN);
    /* t10 = a/t8; */
    mpfr_div(t10, a->real, t8, MPFR_RNDN);
    /* t12 = tools_square(t10 + 1.0); */
    mpfr_add_ui(tmp, t10, 1, MPFR_RNDN);
    mpfr_sqr(t12, tmp, MPFR_RNDN);
    /* t13 = t8*t8; t15 = t2/t13; */
    mpfr_mul(t13, t8, t8, MPFR_RNDN);
    mpfr_div(t15, t2, t13, MPFR_RNDN);
    /* t17 = sqrt(t12 + t15); */
    mpfr_add(tmp, t12, t15, MPFR_RNDN);
    mpfr_sqrt(t17, tmp, MPFR_RNDN);
    /* t19 = tools_square(t10 - 1.0); */
    mpfr_sub_ui(tmp, t10, 1, MPFR_RNDN);
    mpfr_mul(t19, tmp, tmp, MPFR_RNDN);
    /* t21 = sqrt(t19 + t15); */
    mpfr_add(tmp, t19, t15, MPFR_RNDN);
    mpfr_sqrt(t21, tmp, MPFR_RNDN);
    /* t23 = tools_square(0.5*(t17 + t21)); */
    mpfr_add(tmp, t17, t21, MPFR_RNDN);
    mpfr_mul_d(tmp, tmp, 0.5, MPFR_RNDN);
    mpfr_sqr(t23, tmp, MPFR_RNDN);
    /* Note: tools_adjust logic here. */
    /* If t23 < 1.0, we clamp to 1.0 for the sqrt(t23 - 1.0) */
    if (mpfr_cmp_ui(t23, 1) < 0) mpfr_set_ui(t23, 1, MPFR_RNDN);
    /* t25 = sqrt(t23 - 1.0); */
    mpfr_sub_ui(t25, t23, 1, MPFR_RNDN);
    mpfr_sqrt(t25, t25, MPFR_RNDN);
    /* t30 = 0.5*(t17 + t21) + t25; */
    mpfr_add(tmp, t17, t21, MPFR_RNDN);
    mpfr_mul_d(tmp, tmp, 0.5, MPFR_RNDN);
    mpfr_add(t30, tmp, t25, MPFR_RNDN);
    /* t31 = 0.5*(t17 - t21);  */
    mpfr_sub(t31, t17, t21, MPFR_RNDN);
    mpfr_mul_d(t31, t31, 0.5, MPFR_RNDN);
    /* Note: tools_adjust logic for acos: clamp t31 between -1 and 1 */
    if (mpfr_cmp_si(t31, 1) > 0) mpfr_set_si(t31, 1, MPFR_RNDN);
    if (mpfr_cmp_si(t31, -1) < 0) mpfr_set_si(t31, -1, MPFR_RNDN);
    /* Final Assembly: */
    /* Real Part: -t4 * tools_csgn(b, a) * log(t30) */
    int csgn_final = mpfr_tools_csgn(a->imag, a->real);
    mpfr_log(z->real, t30, MPFR_RNDN);
    mpfr_mul_d(z->real, z->real, -t4 * (double)csgn_final, MPFR_RNDN);
    /* Imaginary Part: t4 * acos(t31) */
    mpfr_acos(z->imag, t31, MPFR_RNDN);
    mpfr_mul_d(z->imag, z->imag, t4, MPFR_RNDN);
    Mpfr_cfinalise(z);
    mpfr_clears(t1, t2, t8, t10, t12, t13, t15, t17, t19, t21, t23, t25, t30, t31, tmp, tmp2, MPFR_NULL);
  }
  return 1;
}


/* --- The Robust Complex ERF, generated by Gemini AI, 6.6.2 --- */
static void mpfr_cerf (mpfr_t res_re, mpfr_t res_im, mpfr_t z_re, mpfr_t z_im, mpfr_prec_t prec) {
  mpfr_t mag;
  mpfr_init2(mag, prec);
  /* Check magnitude for Hybrid Switch */
  Mpfr_mul(mag, z_re, z_re);
  mpfr_t t_im2; mpfr_init2(t_im2, prec);
  mpfr_mul(t_im2, z_im, z_im, MPFR_RNDN);
  mpfr_add(mag, mag, t_im2, MPFR_RNDN);
  mpfr_sqrt(mag, mag, MPFR_RNDN);
  if (mpfr_cmp_d(mag, 4.0) < 0) {
    /* Use the Power Series loop you implemented earlier, mpfr_cerf_series(...) */
    mpfr_cerf_series(res_re, res_im, z_re, z_im, prec);
  } else {
    /* --- Monolithic Continued Fraction + Finalizer --- */
    unsigned long j;
    int sign = 1;
    if (mpfr_sgn(z_re) < 0) {
      sign = -1;
      mpfr_neg(z_re, z_re, MPFR_RNDN);
    }
    /* 1. Declare and Initialize ALL local variables immediately */
    mpfr_t f_re, f_im, C_re, C_im, D_re, D_im, delta_re, delta_im;
    mpfr_t tr, ti, den, next_re, a_re, exp_mag, pi_sqrt;
    mpfr_inits2(prec, f_re, f_im, C_re, C_im, D_re, D_im, delta_re, delta_im,
        tr, ti, den, next_re, a_re, exp_mag, pi_sqrt, MPFR_NULL);
    /* 2. Lentz's Method Loop */
    mpfr_set(f_re, z_re, MPFR_RNDN);
    mpfr_set(f_im, z_im, MPFR_RNDN);
    if (mpfr_zero_p(f_re) && mpfr_zero_p(f_im)) mpfr_set_d(f_re, 1e-100, MPFR_RNDN);
    mpfr_set(C_re, f_re, MPFR_RNDN);
    mpfr_set(C_im, f_im, MPFR_RNDN);
    mpfr_set_ui(D_re, 0, MPFR_RNDN);
    mpfr_set_ui(D_im, 0, MPFR_RNDN);
    for (j=1; j < 500; j++) {
      mpfr_set_d(a_re, j * 0.5, MPFR_RNDN);
      /* D = b + a/D */
      mpfr_mul(tr, D_re, D_re, MPFR_RNDN);
      mpfr_mul(ti, D_im, D_im, MPFR_RNDN);
      mpfr_add(den, tr, ti, MPFR_RNDN);
      if (mpfr_zero_p(den)) mpfr_set_d(den, 1e-100, MPFR_RNDN);
      mpfr_mul(tr, a_re, D_re, MPFR_RNDN);
      mpfr_div(tr, tr, den, MPFR_RNDN);
      mpfr_neg(ti, a_re, MPFR_RNDN);
      mpfr_mul(ti, ti, D_im, MPFR_RNDN);
      mpfr_div(ti, ti, den, MPFR_RNDN);
      mpfr_add(D_re, z_re, tr, MPFR_RNDN);
      mpfr_add(D_im, z_im, ti, MPFR_RNDN);
      if (mpfr_zero_p(D_re) && mpfr_zero_p(D_im)) mpfr_set_d(D_re, 1e-100, MPFR_RNDN);
      /* C = b + a/C */
      mpfr_mul(tr, C_re, C_re, MPFR_RNDN);
      mpfr_mul(ti, C_im, C_im, MPFR_RNDN);
      mpfr_add(den, tr, ti, MPFR_RNDN);
      if (mpfr_zero_p(den)) mpfr_set_d(den, 1e-100, MPFR_RNDN);
      mpfr_mul(tr, a_re, C_re, MPFR_RNDN);
      mpfr_div(tr, tr, den, MPFR_RNDN);
      mpfr_neg(ti, a_re, MPFR_RNDN);
      mpfr_mul(ti, ti, C_im, MPFR_RNDN);
      mpfr_div(ti, ti, den, MPFR_RNDN);
      mpfr_add(C_re, z_re, tr, MPFR_RNDN);
      mpfr_add(C_im, z_im, ti, MPFR_RNDN);
      if (mpfr_zero_p(C_re) && mpfr_zero_p(C_im)) mpfr_set_d(C_re, 1e-100, MPFR_RNDN);
      /* delta = (1/D) * C */
      mpfr_mul(tr, D_re, D_re, MPFR_RNDN);
      mpfr_mul(ti, D_im, D_im, MPFR_RNDN);
      mpfr_add(den, tr, ti, MPFR_RNDN);
      mpfr_div(tr, D_re, den, MPFR_RNDN); /* 1/D real */
      mpfr_neg(ti, D_im, MPFR_RNDN);
      mpfr_div(ti, ti, den, MPFR_RNDN); /* 1/D imag */
      mpfr_mul(den, tr, C_re, MPFR_RNDN);
      mpfr_mul(next_re, ti, C_im, MPFR_RNDN);
      mpfr_sub(delta_re, den, next_re, MPFR_RNDN);
      mpfr_mul(den, tr, C_im, MPFR_RNDN);
      mpfr_mul(next_re, ti, C_re, MPFR_RNDN);
      mpfr_add(delta_im, den, next_re, MPFR_RNDN);
      /* f = f * delta */
      mpfr_mul(tr, f_re, delta_re, MPFR_RNDN);
      mpfr_mul(ti, f_im, delta_im, MPFR_RNDN);
      mpfr_sub(next_re, tr, ti, MPFR_RNDN);
      mpfr_mul(tr, f_re, delta_im, MPFR_RNDN);
      mpfr_mul(ti, f_im, delta_re, MPFR_RNDN);
      mpfr_add(f_im, tr, ti, MPFR_RNDN);
      mpfr_set(f_re, next_re, MPFR_RNDN);
      mpfr_sub_ui(tr, delta_re, 1, MPFR_RNDN);
      if (mpfr_get_exp(tr) < -prec && mpfr_get_exp(delta_im) < -prec) break;
    }
    /* 3. Integrated Finalizer */
    mpfr_mul(tr, z_re, z_re, MPFR_RNDN);
    mpfr_mul(ti, z_im, z_im, MPFR_RNDN);
    mpfr_sub(tr, ti, tr, MPFR_RNDN);     /* y^2 - x^2 */
    mpfr_exp(exp_mag, tr, MPFR_RNDN);    /* exp(-z^2) */
    mpfr_mul(ti, z_re, z_im, MPFR_RNDN);
    mpfr_mul_si(ti, ti, -2, MPFR_RNDN);  /* angle */
    mpfr_sin_cos(ti, tr, ti, MPFR_RNDN);  /* tr=cos, ti=sin */
    mpfr_const_pi(pi_sqrt, MPFR_RNDN);
    mpfr_sqrt(pi_sqrt, pi_sqrt, MPFR_RNDN);
    mpfr_mul(tr, tr, exp_mag, MPFR_RNDN);
    mpfr_div(tr, tr, pi_sqrt, MPFR_RNDN);  /* Num Real */
    mpfr_mul(ti, ti, exp_mag, MPFR_RNDN);
    mpfr_div(ti, ti, pi_sqrt, MPFR_RNDN);  /* Num Imag */
    mpfr_mul(den, f_re, f_re, MPFR_RNDN);
    mpfr_mul(next_re, f_im, f_im, MPFR_RNDN);
    mpfr_add(den, den, next_re, MPFR_RNDN);  /* Denominator */
    /* (tr + i*ti) / (f_re + i*f_im) */
    mpfr_mul(exp_mag, tr, f_re, MPFR_RNDN);
    mpfr_mul(next_re, ti, f_im, MPFR_RNDN);
    mpfr_add(res_re, exp_mag, next_re, MPFR_RNDN);
    mpfr_div(res_re, res_re, den, MPFR_RNDN);  /* Result erfc Real */
    mpfr_mul(exp_mag, ti, f_re, MPFR_RNDN);
    mpfr_mul(next_re, tr, f_im, MPFR_RNDN);
    mpfr_sub(res_im, exp_mag, next_re, MPFR_RNDN);
    mpfr_div(res_im, res_im, den, MPFR_RNDN);  /* Result erfc Imag */
    /* 4. Convert erfc -> erf */
    mpfr_ui_sub(res_re, 1, res_re, MPFR_RNDN);
    mpfr_neg(res_im, res_im, MPFR_RNDN);
    /* 5. Cleanup and Symmetry */
    if (sign == -1) {
      mpfr_neg(res_re, res_re, MPFR_RNDN);
      mpfr_neg(res_im, res_im, MPFR_RNDN);
      mpfr_neg(z_re, z_re, MPFR_RNDN);
    }
    mpfr_set(res_re, res_re, MPFR_ROUNDING);
    mpfr_set(res_im, res_im, MPFR_ROUNDING);
    mpfr_clears(f_re, f_im, C_re, C_im, D_re, D_im, delta_re, delta_im,
                tr, ti, den, next_re, a_re, exp_mag, pi_sqrt, MPFR_NULL);
  }
  mpfr_clears(mag, t_im2, MPFR_NULL);
}


static int Mpf_cbrt (lua_State *L) {  /* extended to the complex domain 6.6.2 */
  if (ismpf(L, 1)) {
    Mpfr *a, *r;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(r);
    Mpfr_cbrt(r->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_cbrt);
  } else {
    mpfr_prec_t prec;
    CMpfr *a, *z;
    a = checkcmpfr(L, 1);
    createcmpf(z);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    mpfr_ccbrt(z->real, z->imag, a->real, a->imag, prec);
  }
  return 1;
}


static int Mpf_exp2 (lua_State *L) {  /* extended to the complex domain 6.6.2 */
  if (ismpf(L, 1)) {
    Mpfr *a, *r;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(r);
    Mpfr_exp2(r->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_exp2);
  } else {
    mpfr_prec_t prec;
    CMpfr *a, *z;
    a = checkcmpfr(L, 1);
    createcmpf(z);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    mpfr_cexp2(z->real, z->imag, a->real, a->imag, prec);
  }
  return 1;
}


static int Mpf_exp10 (lua_State *L) {  /* extended to the complex domain 6.6.2 */
  if (ismpf(L, 1)) {
    Mpfr *a, *r;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(r);
    Mpfr_exp10(r->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_exp10);
  } else {
    mpfr_prec_t prec;
    CMpfr *a, *z;
    a = checkcmpfr(L, 1);
    createcmpf(z);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    mpfr_cexp10(z->real, z->imag, a->real, a->imag, prec);
  }
  return 1;
}


static int mtc_antilog2 (lua_State *L) {  /* 7.5.8 */
  mpfr_prec_t prec;
  CMpfr *a, *z;
  a = checkcmpfr(L, 1);
  createcmpf(z);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  mpfr_cexp2(z->real, z->imag, a->real, a->imag, prec);
  return 1;
}


static int mtc_antilog10 (lua_State *L) {  /* 7.5.8 */
  mpfr_prec_t prec;
  CMpfr *a, *z;
  a = checkcmpfr(L, 1);
  createcmpf(z);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  mpfr_cexp10(z->real, z->imag, a->real, a->imag, prec);
  return 1;
}


static int Mpf_log2 (lua_State *L) {  /* extended to the complex domain 6.6.2 */
  if (ismpf(L, 1)) {
    Mpfr *a, *r;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(r);
    Mpfr_log2(r->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_log2);
  } else {
    mpfr_prec_t prec;
    CMpfr *a, *z;
    a = checkcmpfr(L, 1);
    createcmpf(z);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    mpfr_clog2(z->real, z->imag, a->real, a->imag, prec);
  }
  return 1;
}


static int Mpf_log10 (lua_State *L) {  /* extended to the complex domain 6.6.2 */
  if (ismpf(L, 1)) {
    Mpfr *a, *r;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(r);
    Mpfr_log10(r->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_log10);
  } else {
    mpfr_prec_t prec;
    CMpfr *a, *z;
    a = checkcmpfr(L, 1);
    createcmpf(z);
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    mpfr_clog10(z->real, z->imag, a->real, a->imag, prec);
  }
  return 1;
}


static int Mpf_erf (lua_State *L) {  /* extended to the complex domain 6.6.2 */
  if (ismpf(L, 1)) {
    Mpfr *a, *r;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(r);
    Mpfr_erf(r->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_erf);
  } else {
    mpfr_prec_t prec;
    CMpfr *a, *z;
    a = checkcmpfr(L, 1);
    createcmpf(z);
    /* extended precision for the loop in mpfr_cerf, as I unfortunately mixed rounding modes all over the package */
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    mpfr_cerf(z->real, z->imag, a->real, a->imag, prec);
  }
  return 1;
}


static int Mpf_erfc (lua_State *L) {  /* extended to the complex domain 6.6.2 */
  if (ismpf(L, 1)) {
    Mpfr *a, *r;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(r);
    Mpfr_erfc(r->val, a->val);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_erfc);
  } else {
    mpfr_t Mpfr_One;
    mpfr_prec_t prec;
    CMpfr *a, *z;
    a = checkcmpfr(L, 1);
    /* extended precision for the loop in mpfr_cerf, as I unfortunately mixed rounding modes all over the package */
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    createcmpf(z);
    Mpfr_init2_d(Mpfr_One, 1.0, prec);
    mpfr_cerf(z->real, z->imag, a->real, a->imag, prec);
    /* erfc(x) = 1 - erf(x) */
    mpfr_sub(z->real, Mpfr_One, z->real, MPFR_RNDN);
    mpfr_neg(z->imag, z->imag, MPFR_ROUNDING);
    mpfr_clear(Mpfr_One);
  }
  return 1;
}


static int Mpf_argument (lua_State *L) {  /* 6.6.3 */
  if (ismpf(L, 1)) {
    mpfr_t Mpfr_Zero;
    Mpfr *a, *r;
    a = (Mpfr *)lua_touserdata(L, 1);
    creatempf(r);
    mpfr_init2(Mpfr_Zero, mpfr_get_prec(a->val));  /* 6.6.6 fix */
    Mpfr_set_d(Mpfr_Zero, 0.0);
    Mpfr_atan2(r->val, Mpfr_Zero, a->val);
    mpfr_clear(Mpfr_Zero);
  } else if (agn_isnumber(L, 1)) {
    aux_turntompfandcallagain(L, 1, Mpf_erfc);
  } else {
    CMpfr *a;
    Mpfr *r;
    a = checkcmpfr(L, 1);
    creatempf(r);
    Mpfr_atan2(r->val, a->imag, a->real);
  }
  return 1;
}


static int polygen_complexgenerator (lua_State *L) {
  CMpfr *x, *c, *r, *first;
  mpfr_t nextreal, nextimag, tmp;
  size_t i, nops;
  int tbl = lua_upvalueindex(1);
  nops = agn_tointeger(L, lua_upvalueindex(2));
  x = checkcmpfr(L, 1);
  /* 1. Guard precision */
  mpfr_prec_t prec = mpfr_get_prec(x->real) + 12;
  mpfr_inits2(prec, nextreal, nextimag, tmp, MPFR_NULL);
  /* 2. Initialize accumulator with leading coefficient (a_n) */
  luaL_checkstack(L, 2, "not enough stack space");
  createcmpf(r);
  lua_geti(L, tbl, 1);
  first = checkcmpfr(L, -1);
  mpfr_set(r->real, first->real, MPFR_RNDN);
  mpfr_set(r->imag, first->imag, MPFR_RNDN);
  lua_pop(L, 1);
  for (i=2; i <= nops; i++) {
    lua_geti(L, tbl, i);
    c = checkcmpfr(L, -1);
    /* Goal: next_acc = (acc * z) + coeff[i]
       Real Part: nextreal = R*x - I*y + coeffs_r[i]
       Step A: tmp = R*x + coeffs_r[i] */
    mpfr_fma(tmp, r->real, x->real, c->real, MPFR_RNDN);
    /* Step B: nextreal = -I*y + tmp */
    mpfr_neg(nextimag, r->imag, MPFR_RNDN); /* Temporarily negate I for subtraction */
    mpfr_fma(nextreal, nextimag, x->imag, tmp, MPFR_RNDN);
    /* Imaginary Part: nextimag = R*y + I*x + coeffs_i[i]
       Step A: tmp = R*y + coeffs_i[i] */
    mpfr_fma(tmp, r->real, x->imag, c->imag, MPFR_RNDN);
    /* Step B: nextimag = I*x + tmp */
    mpfr_fma(nextimag, r->imag, x->real, tmp, MPFR_RNDN);
    /* Update accumulator */
    mpfr_set(r->real, nextreal, MPFR_RNDN);
    mpfr_set(r->imag, nextimag, MPFR_RNDN);
    lua_pop(L, 1);
  }
  /* 4. Final step: Apply user's rounding and return ternary value
     We return a combined status (0 if both exact, else non-zero) */
  Mpfr_cfinalise(r);
  mpfr_clears(nextreal, nextimag, tmp, MPFR_NULL);
  return 1;
}

static int polygen_realgenerator (lua_State *L) {
  Mpfr *x, *c, *r;
  mpfr_t accu;
  size_t i, nops;
  int tbl = lua_upvalueindex(1);
  nops = agn_tointeger(L, lua_upvalueindex(2));
  x = checkmpfr(L, 1);
  mpfr_init2(accu, mpfr_get_prec(x->val) + 10);
  lua_geti(L, tbl, 1);
  mpfr_set(accu, checkmpfr(L, -1)->val, MPFR_RNDN);
  lua_pop(L, 1);
  creatempf(r);
  for (i=2; i <= nops; i++) {
    lua_geti(L, tbl, i);
    c = checkmpfr(L, -1);
    mpfr_fma(accu, accu, x->val, c->val, MPFR_RNDN);
    lua_pop(L, 1);
  }
  mpfr_set(r->val, accu, MPFR_ROUNDING);
  mpfr_clear(accu);
  return 1;
}

static int Mpf_polygen (lua_State *L) {
  size_t i, nops;
  int isnumormpf, iscmpf, issuecmplxerr, t;
  luaL_checktype(L, 1, LUA_TTABLE);
  nops = agn_asize(L, 1);  /* get number of coefficients in the table */
  if (nops == 0) {
    luaL_error(L, "Error in " LUA_QS ": table of coefficients is empty.", "mpfr.polygen");
  }
  iscmpf = 0; issuecmplxerr = 0;
  for (i=1; i <= nops; i++) {
    t = lua_geti(L, 1, i);
    isnumormpf = agn_isnumber(L, -1) || ismpf(L, -1);
    if (i == 1 && iscmpf(L, -1)) iscmpf = 1;
    if (iscmpf && i > 1 && !iscmpf(L, -1)) issuecmplxerr = 1;
    lua_pop(L, 1);
    if (issuecmplxerr) {
      luaL_error(L, "Error in " LUA_QS ": can process complex MPFR values only, got %s.", "mpfr.polygen",
        lua_typename(L, t));
    }
    if (!iscmpf && !isnumormpf) {
      luaL_error(L, "Error in " LUA_QS ": can process numbers or real MPFR values only, got %s.", "mpfr.polygen",
        lua_typename(L, t));
    }
  }
  lua_pushvalue(L, 1);
  lua_pushinteger(L, nops);
  lua_pushcclosure(L, (iscmpf) ? &polygen_complexgenerator : &polygen_realgenerator, 2);
  return 1;
}


static int Mpf_ismpfr (lua_State *L) {  /* 6.6.5 */
  int i, r = 1, nargs = lua_gettop(L);
  for (i=0; i < nargs && r; i++) {
    r = ismpf(L, i + 1);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int Mpf_iscmpfr (lua_State *L) {  /* 6.6.5 */
  int i, r = 1, nargs = lua_gettop(L);
  for (i=0; i < nargs && r; i++) {
    r = iscmpf(L, i + 1);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int Mpf_checkmpfr (lua_State *L) {  /* 6.6.5 */
  int i, nargs = lua_gettop(L);
  for (i=0; i < nargs; i++) {
    if (!ismpf(L, i + 1)) {
      luaL_error(L, "Error: value #%d must be a real MPFR number, got %s.", i + 1, luaL_typename(L, i + 1));
    }
  }
  return 0;
}


static int Mpf_checkcmpfr (lua_State *L) {  /* 6.6.5 */
  int i, nargs = lua_gettop(L);
  for (i=0; i < nargs; i++) {
    if (!iscmpf(L, i + 1)) {
      luaL_error(L, "Error: value #%d must be a complex MPFR number, got %s.", i + 1, luaL_typename(L, i + 1));
    }
  }
  return 0;
}


/* Complex Metamethods */

static int mtc_eq (lua_State *L) {
  CMpfr *a, *b;
  a = checkcmpfr(L, 1);
  b = checkcmpfr(L, 2);
  lua_pushboolean(L, Mpfr_cmp(a->real, b->real) == 0 && Mpfr_cmp(a->imag, b->imag) == 0);
  return 1;
}


static int mtc_neg (lua_State *L) {  /* -a */
  CMpfr *a, *c;
  a = checkcmpfr(L, 1);
  createcmpf(c);
  Mpfr_neg(c->real, a->real);
  Mpfr_neg(c->imag, a->imag);
  return 1;
}


static int mtc_sign (lua_State *L) {
  mpfr_t r;
  CMpfr *a = checkcmpfr(L, 1);
  mpfr_prec_t prec = mpfr_get_prec(a->real);  /* 6.6.0 patch */
  mpfr_init2(r, prec);
  mpfr_csgn(r, a->real, a->imag);
  lua_pushnumber(L, Mpfr_get_d(r));
  mpfr_clear(r);
  return 1;
}


static int mtc_abs (lua_State *L) {
  Mpfr *r;
  CMpfr *a = checkcmpfr(L, 1);
  creatempf(r);
  Mpfr_hypot(r->val, a->real, a->imag);
  return 1;
}


static int mtc_add (lua_State *L) {  /* a + b, 20 % optimized by Gemini AI, 6.6.1 */
  CMpfr *a, *b, *c;
  a = checkcmpfr(L, 1);
  b = checkcmpfr(L, 2);
  createcmpf(c);
  Mpfr_add(c->real, a->real, b->real);
  Mpfr_add(c->imag, a->imag, b->imag);
  return 1;
}


static int mtc_subtract (lua_State *L) {  /* a - b, 20 % optimized by Gemini AI, 6.6.1 */
  CMpfr *a, *b, *c;
  a = checkcmpfr(L, 1);
  b = checkcmpfr(L, 2);
  createcmpf(c);
  Mpfr_sub(c->real, a->real, b->real);
  Mpfr_sub(c->imag, a->imag, b->imag);
  return 1;
}


static int mtc_multiply (lua_State *L) {  /* 12 % optimized by Gemini AI, 6.6.1 */
  CMpfr *a = checkcmpfr(L, 1);
  CMpfr *b = checkcmpfr(L, 2);
  CMpfr *z;
  createcmpf(z);
  mpfr_prec_t prec = mpfr_get_prec(a->real) + 2;
  mpfr_set_prec(z->real, prec);
  mpfr_set_prec(z->imag, prec);
  /* Re = a->re * b->re - a->im * b->im */
  mpfr_mul(z->real, a->real, b->real, MPFR_RNDN);
  mpfr_fms(z->real, a->imag, b->imag, z->real, MPFR_RNDN);
  mpfr_neg(z->real, z->real, MPFR_RNDN);
  /* Im = a->re * b->im + a->im * b->re */
  mpfr_mul(z->imag, a->real, b->imag, MPFR_RNDN);
  mpfr_fma(z->imag, a->imag, b->real, z->imag, MPFR_RNDN);
  Mpfr_cfinalise(z);
  return 1;
}


static int mtc_divide (lua_State *L) {  /* 8% optimized by Gemini AI, 6.6.1 */
  CMpfr *a = checkcmpfr(L, 1);
  CMpfr *b = checkcmpfr(L, 2);
  CMpfr *z;
  createcmpf(z);
  if (mpfr_zero_p(b->real) && mpfr_zero_p(b->imag)) {
    mpfr_set_nan(z->real);
    mpfr_set_nan(z->imag);
  } else {
    mpfr_prec_t prec = mpfr_get_prec(a->real) + 4;
    mpfr_set_prec(z->real, prec);
    mpfr_set_prec(z->imag, prec);
    mpfr_t denom;
    mpfr_init2(denom, prec);
    /* 1. Calculate denominator: denom = b_re^2 + b_im^2 */
    mpfr_mul(denom, b->real, b->real, MPFR_RNDN);
    mpfr_fma(denom, b->imag, b->imag, denom, MPFR_RNDN);
    /* 2. Real part: (a_re * b_re + a_im * b_im) / denom */
    mpfr_mul(z->real, a->real, b->real, MPFR_RNDN);
    mpfr_fma(z->real, a->imag, b->imag, z->real, MPFR_RNDN);
    mpfr_div(z->real, z->real, denom, MPFR_RNDN);
    /* 3. Imag part: (a_im * b_re - a_re * b_im) / denom */
    mpfr_mul(z->imag, a->imag, b->real, MPFR_RNDN);
    mpfr_fms(z->imag, a->real, b->imag, z->imag, MPFR_RNDN);
    mpfr_neg(z->imag, z->imag, MPFR_RNDN);  /* Corrects fms order: -(ad - bc) = bc - ad */
    mpfr_div(z->imag, z->imag, denom, MPFR_RNDN);
    Mpfr_cfinalise(z);
    mpfr_clear(denom);
  }
  return 1;
}


static int mtc_recip (lua_State *L) {  /* 8% optimized by Gemini AI, 6.6.1 */
  CMpfr *a, *z;
  a = checkcmpfr(L, 1);
  createcmpf(z);
  if (Mpfr_iszero(a->real) && Mpfr_iszero(a->imag)) {
    Mpfr_set_nan(z->real);
    Mpfr_set_nan(z->imag);
  } else {  /* 6.6.1 optimised by Gemini AI */
    mpfr_prec_t prec = mpfr_get_prec(a->real) + 4;
    mpfr_set_prec(z->real, prec);
    mpfr_set_prec(z->imag, prec);
    mpfr_t denom;
    mpfr_init2(denom, prec);
    /* 1. denom = a_re^2 + a_im^2 */
    mpfr_mul(denom, a->real, a->real, MPFR_RNDN);
    mpfr_fma(denom, a->imag, a->imag, denom, MPFR_RNDN);
    /* 2. Real part: a_re / (a_re^2 + a_im^2) */
    mpfr_div(z->real, a->real, denom, MPFR_RNDN);
    /* 3. Imag part: -a_im / (a_re^2 + a_im^2) */
    mpfr_neg(z->imag, a->imag, MPFR_RNDN);
    mpfr_div(z->imag, z->imag, denom, MPFR_RNDN);
    Mpfr_cfinalise(z);
    mpfr_clear(denom);
  }
  return 1;
}


static int mtc_pow (lua_State *L) {  /* 6.6.1 optimised by Gemini AI, but no increase in speed */
  CMpfr *a = checkcmpfr(L, 1);
  CMpfr *b = checkcmpfr(L, 2);
  CMpfr *z;
  createcmpf(z);
  mpfr_prec_t prec = mpfr_get_prec(a->real) + 4;
  /* Handle 0^y cases */
  if (mpfr_zero_p(a->real) && mpfr_zero_p(a->imag)) {
    if (mpfr_sgn(b->real) <= 0) {
      mpfr_set_nan(z->real); mpfr_set_nan(z->imag);
    } else {
      mpfr_set_zero(z->real, 1); mpfr_set_zero(z->imag, 1);
    }
    return 1;
  }
  mpfr_t r, theta, tmp, absa, arga;
  mpfr_inits2(prec, r, theta, tmp, absa, arga, MPFR_NULL);
  /* 1. Get Polar coordinates of base 'a' */
  mpfr_hypot(absa, a->real, a->imag, MPFR_RNDN); /* |a| */
  mpfr_atan2(arga, a->imag, a->real, MPFR_RNDN); /* arg(a) */
  /* 2. Calculate Magnitude: r = |a|^b_re * exp(-b_im * arg(a)) */
  mpfr_pow(r, absa, b->real, MPFR_RNDN);
  if (!mpfr_zero_p(b->imag)) {
    mpfr_mul(tmp, b->imag, arga, MPFR_RNDN);
    mpfr_neg(tmp, tmp, MPFR_RNDN);
    mpfr_exp(tmp, tmp, MPFR_RNDN);
    mpfr_mul(r, r, tmp, MPFR_RNDN);
  }
  /* 3. Calculate Phase: theta = b_re * arg(a) + b_im * log(|a|) */
  mpfr_mul(theta, b->real, arga, MPFR_RNDN);
  if (!mpfr_zero_p(b->imag)) {
    mpfr_log(tmp, absa, MPFR_RNDN);
    mpfr_fma(theta, b->imag, tmp, theta, MPFR_RNDN); /* theta = (b_im * log|a|) + theta */
  }
  /* 4. Convert back to Rectangular: z = r * (cos(theta) + i*sin(theta)) */
  mpfr_sin_cos(z->imag, z->real, theta, MPFR_RNDN);
  mpfr_mul(z->real, z->real, r, MPFR_RNDN);
  mpfr_mul(z->imag, z->imag, r, MPFR_RNDN);
  Mpfr_cfinalise(z);
  mpfr_clears(r, theta, tmp, absa, arga, MPFR_NULL);
  return 1;
}


static int mtc_ipow (lua_State *L) {
  mpfr_t x, y, n, t1, t2, t5, t6, t7, t8, t9, Mpfr_Two;
  CMpfr *a, *z;
  mpfr_prec_t prec;
  a = checkcmpfr(L, 1);
  createcmpf(z);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  mpfr_inits2(prec, x, y, n, t1, t2, t5, t6, t7, t8, t9, MPFR_NULL);
  Mpfr_init2_d(Mpfr_Two, 2.0, prec);
  mpfr_mul(t1, a->real, a->real, MPFR_RNDN);
  mpfr_mul(t2, a->imag, a->imag, MPFR_RNDN);
  mpfr_add(x, t1, t2, MPFR_RNDN);
  Mpfr_set_d(n, agn_checknumber(L, 2));
  mpfr_div(y, n, Mpfr_Two, MPFR_RNDN);
  mpfr_pow(t5, x, y, MPFR_RNDN);
  mpfr_atan2(t6, a->imag, a->real, MPFR_RNDN);
  mpfr_mul(t7, t6, n, MPFR_RNDN);
  mpfr_sin_cos(t9, t8, t7, MPFR_RNDN);
  mpfr_mul(z->real, t5, t8, MPFR_RNDN);
  mpfr_mul(z->imag, t5, t9, MPFR_RNDN);
  Mpfr_cfinalise(z);
  mpfr_clears(x, y, n, t1, t2, t5, t6, t7, t8, t9, Mpfr_Two, MPFR_NULL);
  return 1;
}


static int mtc_square (lua_State *L) {  /* 4% optimized by Gemini AI, 6.6.1 */
  CMpfr *a, *z;
  a = checkcmpfr(L, 1);
  createcmpf(z);
  mpfr_prec_t prec = mpfr_get_prec(a->real) + 4;
  mpfr_set_prec(z->real, prec);
  mpfr_set_prec(z->imag, prec);
  /* One temporary is enough to hold b^2 */
  mpfr_t t;
  mpfr_init2(t, prec);
  /* 1. Real part: z_re = a_re^2 - a_im^2 */
  mpfr_sqr(z->real, a->real, MPFR_RNDN); /* Optimized squaring */
  mpfr_sqr(t, a->imag, MPFR_RNDN);      /* Optimized squaring */
  mpfr_sub(z->real, z->real, t, MPFR_RNDN);
  /* 2. Imaginary part: z_im = 2 * a_re * a_im
     Reusing t to avoid another init */
  mpfr_mul(t, a->real, a->imag, MPFR_RNDN);
  mpfr_mul_2ui(z->imag, t, 1, MPFR_RNDN);  /* Shift-based multiplication (fastest way to *2) */
  Mpfr_cfinalise(z);
  mpfr_clear(t);
  return 1;
}


static int mtc_cube (lua_State *L) {  /* 6% optimized by Gemini AI, 6.6.1 */
  CMpfr *a, *z;
  a = checkcmpfr(L, 1);
  createcmpf(z);
  mpfr_prec_t prec = mpfr_get_prec(a->real) + 4;
  mpfr_set_prec(z->real, prec);
  mpfr_set_prec(z->imag, prec);
  mpfr_t ta2, tb2, tmp;
  mpfr_inits2(prec, ta2, tb2, tmp, MPFR_NULL);
  /* 1. Precompute squares */
  mpfr_sqr(ta2, a->real, MPFR_RNDN);    /* ta2 = a^2 */
  mpfr_sqr(tb2, a->imag, MPFR_RNDN);    /* tb2 = b^2 */
  /* 2. Real part: a * (a^2 - 3*b^2) */
  mpfr_mul_ui(tmp, tb2, 3, MPFR_RNDN);  /* 3*b^2 */
  mpfr_sub(tmp, ta2, tmp, MPFR_RNDN);   /* a^2 - 3*b^2 */
  mpfr_mul(z->real, a->real, tmp, MPFR_RNDN);
  /* 3. Imaginary part: b * (3*a^2 - b^2) */
  mpfr_mul_ui(tmp, ta2, 3, MPFR_RNDN);  /* 3*a^2 */
  mpfr_sub(tmp, tmp, tb2, MPFR_RNDN);   /* 3*a^2 - b^2 */
  mpfr_mul(z->imag, a->imag, tmp, MPFR_RNDN);
  Mpfr_cfinalise(z);
  mpfr_clears(ta2, tb2, tmp, MPFR_NULL);
  return 1;
}


static int mtc_sqrt (lua_State *L) {  /* 25% optimized by Gemini AI, 6.6.1 */
  CMpfr *a = checkcmpfr(L, 1);
  CMpfr *z;
  createcmpf(z);
  mpfr_prec_t prec = mpfr_get_prec(a->real) + 4;
  mpfr_set_prec(z->real, prec);
  mpfr_set_prec(z->imag, prec);
  /* Pure real case (Imaginary is zero) */
  if (mpfr_zero_p(a->imag)) {
    if (mpfr_sgn(a->real) < 0) {
      mpfr_set_ui(z->real, 0, MPFR_RNDN);
      mpfr_neg(z->imag, a->real, MPFR_RNDN);
      mpfr_sqrt(z->imag, z->imag, MPFR_RNDN);
    } else {
      mpfr_sqrt(z->real, a->real, MPFR_RNDN);
      mpfr_set_ui(z->imag, 0, MPFR_RNDN);
    }
  } else {
    mpfr_t t, w;
    mpfr_inits2(prec, t, w, MPFR_NULL);
    /* 1. Calculate w = sqrt((|a_re| + hypot(a_re, a_im)) / 2) */
    mpfr_hypot(t, a->real, a->imag, MPFR_RNDN); /* t = magnitude */
    mpfr_abs(w, a->real, MPFR_RNDN);
    mpfr_add(w, w, t, MPFR_RNDN);
    mpfr_div_ui(w, w, 2, MPFR_RNDN);
    mpfr_sqrt(w, w, MPFR_RNDN);  /* w is our primary calculation block */
    if (mpfr_sgn(a->real) >= 0) {
      /* Case Re(z) >= 0
         result_re = w
         result_im = a_im / (2 * w) */
      mpfr_set(z->real, w, MPFR_RNDN);
      mpfr_mul_2ui(t, w, 1, MPFR_RNDN);  /* t = 2 * w */
      mpfr_div(z->imag, a->imag, t, MPFR_RNDN);
    } else {
      /* Case Re(z) < 0
         result_re = |a_im| / (2 * w)
         result_im = sgn(a_im) * w  */
      mpfr_abs(t, a->imag, MPFR_RNDN);
      mpfr_mul_2ui(z->real, w, 1, MPFR_RNDN);  /* Use z->real as temp for 2*w */
      Mpfr_div(z->real, t, z->real);
      if (mpfr_sgn(a->imag) >= 0)
        mpfr_set(z->imag, w, MPFR_RNDN);
      else
        mpfr_neg(z->imag, w, MPFR_RNDN);
    }
    mpfr_clears(t, w, MPFR_NULL);
  }
  Mpfr_cfinalise(z);
  return 1;
}


static int mtc_invsqrt (lua_State *L) {  /* 7.5.8 */
  mtc_sqrt(L);
  lua_replace(L, 1);
  mtc_recip(L);  /* checks for division by zero, too */
  return 1;
}


static int mtc_ln (lua_State *L) {  /* could not be optimised by Gemini AI */
  CMpfr *a, *z;
  a = checkcmpfr(L, 1);
  createcmpf(z);
  if (Mpfr_iszero(a->real) && Mpfr_iszero(a->imag)) {
    Mpfr_set_nan(z->real);
    Mpfr_set_nan(z->imag);
  } else {
    mpfr_t t, t0, t1, Mpfr_Two;
    mpfr_prec_t prec;
    prec = mpfr_get_prec(a->real) + EXTRABITS;
    mpfr_inits2(prec, t, t0, t1, MPFR_NULL);
    Mpfr_init2_d(Mpfr_Two, 2.0, prec);
    mpfr_mul(t0, a->real, a->real, MPFR_RNDN);
    mpfr_mul(t1, a->imag, a->imag, MPFR_RNDN);
    mpfr_add(t, t0, t1, MPFR_RNDN);
    mpfr_log(t, t, MPFR_RNDN);
    mpfr_div(z->real, t, Mpfr_Two, MPFR_RNDN);
    mpfr_atan2(z->imag, a->imag, a->real, MPFR_RNDN);
    Mpfr_cfinalise(z);
    mpfr_clears(t, t0, t1, Mpfr_Two, MPFR_NULL);
  }
  return 1;
}


static int mtc_exp (lua_State *L) {
  mpfr_t si, co, t;
  CMpfr *a, *z;
  mpfr_prec_t prec;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  mpfr_inits2(prec, si, co, t, MPFR_NULL);
  mpfr_exp(t, a->real, MPFR_RNDN);
  mpfr_sin_cos(si, co, a->imag, MPFR_RNDN);
  mpfr_mul(z->real, t, co, MPFR_RNDN);
  mpfr_mul(z->imag, t, si, MPFR_RNDN);
  Mpfr_cfinalise(z);
  mpfr_clears(si, co, t, MPFR_NULL);
  return 1;
}


static int mtc_sin (lua_State *L) {
  mpfr_t si, co, sih, coh;
  CMpfr *a, *z;
  mpfr_prec_t prec;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  mpfr_inits2(prec, si, co, sih, coh, MPFR_NULL);
  mpfr_sin_cos(si, co, a->real, MPFR_RNDN);
  mpfr_sinh(sih, a->imag, MPFR_RNDN);
  mpfr_cosh(coh, a->imag, MPFR_RNDN);
  mpfr_mul(z->real, si, coh, MPFR_RNDN);
  mpfr_mul(z->imag, co, sih, MPFR_RNDN);
  Mpfr_cfinalise(z);
  mpfr_clears(si, co, sih, coh, MPFR_NULL);
  return 1;
}


static int mtc_cos (lua_State *L) {
  mpfr_t si, co, sih, coh, sir;
  CMpfr *a, *z;
  mpfr_prec_t prec;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  mpfr_inits2(prec, si, co, sih, coh, sir, MPFR_NULL);
  mpfr_sin_cos(si, co, a->real, MPFR_RNDN);
  mpfr_sinh(sih, a->imag, MPFR_RNDN);
  mpfr_cosh(coh, a->imag, MPFR_RNDN);
  mpfr_mul(z->real, co, coh, MPFR_RNDN);
  mpfr_neg(sir, si, MPFR_RNDN);
  mpfr_mul(z->imag, sir, sih, MPFR_RNDN);
  Mpfr_cfinalise(z);
  mpfr_clears(si, co, sih, coh, sir, MPFR_NULL);
  return 1;
}


static int mtc_tan (lua_State *L) {
  mpfr_t re, im, si, co, sih, coh, den, Mpfr_Two;
  CMpfr *a, *z;
  mpfr_prec_t prec;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  mpfr_inits2(prec, re, im, si, co, sih, coh, den, MPFR_NULL);
  Mpfr_init2_d(Mpfr_Two, 2.0, prec);
  mpfr_mul(re, a->real, Mpfr_Two, MPFR_RNDN);
  mpfr_mul(im, a->imag, Mpfr_Two, MPFR_RNDN);
  mpfr_sin_cos(si, co, re, MPFR_RNDN);
  mpfr_sinh(sih, im, MPFR_RNDN);
  mpfr_cosh(coh, im, MPFR_RNDN);
  mpfr_add(den, co, coh, MPFR_RNDN);
  if (mpfr_sgn(den) == 0) {
    Mpfr_set_nan(z->real);
    Mpfr_set_nan(z->imag);
  } else {
    Mpfr_div(z->real, si, den);
    Mpfr_div(z->imag, sih, den);
  }
  Mpfr_cfinalise(z);
  mpfr_clears(re, im, si, co, sih, coh, den, Mpfr_Two, MPFR_NULL);
  return 1;
}


static int mtc_sinh (lua_State *L) {
  mpfr_t si, co, sih, coh;
  CMpfr *a, *z;
  mpfr_prec_t prec;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  mpfr_inits2(prec, si, co, sih, coh, MPFR_NULL);
  mpfr_sin_cos(si, co, a->imag, MPFR_RNDN);
  mpfr_sinh(sih, a->real, MPFR_RNDN);
  mpfr_cosh(coh, a->real, MPFR_RNDN);
  mpfr_mul(z->real, sih, co, MPFR_RNDN);
  mpfr_mul(z->imag, coh, si, MPFR_RNDN);
  Mpfr_cfinalise(z);
  mpfr_clears(si, co, sih, coh, MPFR_NULL);
  return 1;
}


static int mtc_cosh (lua_State *L) {
  mpfr_t si, co, sih, coh;
  CMpfr *a, *z;
  mpfr_prec_t prec;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  mpfr_inits2(prec, si, co, sih, coh, MPFR_NULL);
  mpfr_sin_cos(si, co, a->imag, MPFR_RNDN);
  mpfr_sinh(sih, a->real, MPFR_RNDN);
  mpfr_cosh(coh, a->real, MPFR_RNDN);
  mpfr_mul(z->real, coh, co, MPFR_RNDN);
  mpfr_mul(z->imag, sih, si, MPFR_RNDN);
  Mpfr_cfinalise(z);
  mpfr_clears(si, co, sih, coh, MPFR_NULL);
  return 1;
}


static int mtc_tanh (lua_State *L) {  /* 25% optimized by Gemini AI, 6.6.1 */
  CMpfr *a, *z;
  a = checkcmpfr(L, 1);
  createcmpf(z);
  mpfr_prec_t prec = mpfr_get_prec(a->real) + 4;
  mpfr_set_prec(z->real, prec);
  mpfr_set_prec(z->imag, prec);
  mpfr_t x2, y2, denom, si, co, sih, coh;
  mpfr_inits2(prec, x2, y2, denom, si, co, sih, coh, MPFR_NULL);
  /* 1. Prepare double angles: x2 = 2*x, y2 = 2*y */
  mpfr_mul_2ui(x2, a->real, 1, MPFR_RNDN);
  mpfr_mul_2ui(y2, a->imag, 1, MPFR_RNDN);
  /* 2. Get the components */
  mpfr_sin_cos(si, co, y2, MPFR_RNDN);    /* sin(2y), cos(2y) */
  mpfr_sinh_cosh(sih, coh, x2, MPFR_RNDN); /* sinh(2x), cosh(2x) */
  /* 3. Denominator: denom = cosh(2x) + cos(2y) */
  mpfr_add(denom, coh, co, MPFR_RNDN);
  /* 4. Final results */
  mpfr_div(z->real, sih, denom, MPFR_RNDN);
  mpfr_div(z->imag, si, denom, MPFR_RNDN);
  Mpfr_cfinalise(z);
  mpfr_clears(x2, y2, denom, si, co, sih, coh, MPFR_NULL);
  return 1;
}


static int mtc_arcsin (lua_State *L) {
  mpfr_t x, y, Mpfr_One;
  CMpfr *a, *z;
  mpfr_prec_t prec;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  mpfr_init2(x, prec);
  mpfr_init2(y, prec);
  Mpfr_init2_d(Mpfr_One, 1.0, prec);
  mpfr_carcsinh(x, y, a->imag, a->real, prec);
  if (mpfr_sgn(a->imag) == 0 && Mpfr_cmp(a->real, Mpfr_One) >= 0) {
    mpfr_t t;
    mpfr_init2(t, prec);
    Mpfr_neg(t, x);
    Mpfr_set(x, t);
    mpfr_clear(t);
  }
  Mpfr_set(z->real, y);
  Mpfr_set(z->imag, x);
  Mpfr_cfinalise(z);
  mpfr_clears(x, y, Mpfr_One, MPFR_NULL);
  return 1;
}


static int mtc_arccos (lua_State *L) {
  mpfr_t t, x, y, Mpfr_One, Mpfr_Two, pi, Mpfr_HALF_PI;
  CMpfr *a, *z;
  mpfr_prec_t prec;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  mpfr_inits2(prec, t, x, y, pi, Mpfr_HALF_PI, MPFR_NULL);
  Mpfr_init2_d(Mpfr_One, 1.0, prec);
  Mpfr_init2_d(Mpfr_Two, 2.0, prec);
  mpfr_const_pi(pi, MPFR_RNDN);
  mpfr_div(Mpfr_HALF_PI, pi, Mpfr_Two, MPFR_RNDN);
  mpfr_carcsinh(x, y, a->imag, a->real, prec);
  if (mpfr_sgn(a->imag) != 0 || Mpfr_cmp(a->real, Mpfr_One) <= 0) {
    mpfr_neg(t, x, MPFR_RNDN);
    Mpfr_set(x, t);
  }
  /* y = PIO2 - y; */
  mpfr_sub(z->real, Mpfr_HALF_PI, y, MPFR_RNDN);
  mpfr_set(z->imag, x, MPFR_RNDN);
  Mpfr_cfinalise(z);
  mpfr_clears(t, x, y, Mpfr_One, Mpfr_Two, pi, Mpfr_HALF_PI, MPFR_NULL);
  return 1;
}


static int mtc_arctan (lua_State *L) {  /* not optimisable */
  CMpfr *a, *z;
  mpfr_prec_t prec;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  mpfr_carctan(z->real, z->imag, a->real, a->imag, prec) ;
  return 1;
}


static int mtc_arcsec (lua_State *L) {
  mpfr_t e, t, u, v, x, y, pi, Mpfr_HALF_PI, Mpfr_One, Mpfr_Two;
  CMpfr *a, *z;
  mpfr_prec_t prec;
  a = checkcmpfr(L, 1);
  prec = mpfr_get_prec(a->real) + EXTRABITS;
  createcmpf(z);
  mpfr_inits2(prec, x, y, e, t, u, v, pi, Mpfr_HALF_PI, MPFR_NULL);
  Mpfr_init2_d(Mpfr_One, 1.0, prec);
  Mpfr_init2_d(Mpfr_Two, 2.0, prec);
  mpfr_const_pi(pi, MPFR_RNDN);
  mpfr_div(Mpfr_HALF_PI, pi, Mpfr_Two, MPFR_RNDN);
  /* e = x*x + y*y; */
  mpfr_mul(t, a->real, a->real, MPFR_RNDN);
  mpfr_mul(u, a->imag, a->imag, MPFR_RNDN);
  mpfr_add(e, t, u, MPFR_RNDN);
  /* t = a/e; u = (-b)/e; */
  mpfr_div(t, a->real, e, MPFR_RNDN);
  mpfr_div(u, a->imag, e, MPFR_RNDN);
  mpfr_neg(v, u, MPFR_RNDN);
  mpfr_set(u, v, MPFR_RNDN);
  /* tools_casinh(u, t, &x, &y); */
  mpfr_carcsinh(x, y, u, t, prec);
  if (mpfr_sgn(a->real) != 0 || Mpfr_cmp(t, Mpfr_One) > 0) {
    mpfr_neg(t, x, MPFR_RNDN);
    Mpfr_set(x, t);
  }
  /* y = PIO2 - y; */
  mpfr_sub(z->real, Mpfr_HALF_PI, y, MPFR_RNDN);
  Mpfr_set(z->imag, x);
  Mpfr_cfinalise(z);
  mpfr_clears(x, y, e, t, u, v, pi, Mpfr_One, Mpfr_Two, Mpfr_HALF_PI, MPFR_NULL);
  return 1;
}


static int mtc_sinc (lua_State *L) {
  Csinc(L);
  return 1;
}


/*****************************************************************************************************/

static const struct luaL_Reg mt_mpfrlib [] = {
  {"__tostring",   mt_tostring},  /* for output at the console, e.g. print(n) */
  {"__unm",        mt_neg},
  {"__add",        Mpf_add},
  {"__sub",        Mpf_subtract},
  {"__mul",        Mpf_multiply},
  {"__div",        Mpf_divide},
  {"__mod",        mt_mod},
  {"__recip",      mt_recip},
  {"__abs",        mt_abs},
  {"__absdiff",    mt_absdiff},
  {"__sign",       mt_sign},
  {"__pow",        Mpf_pow},
  {"__sqrt",       mt_sqrt},
  {"__invsqrt",    Mpf_recsqrt},
  {"__square",     mt_square},
  {"__squareadd",  mt_squareadd},
  {"__cube",       mt_cube},
  {"__exp",        mt_exp},
  {"__antilog10",  Mpf_exp10},
  {"__antilog2",   Mpf_exp2},
  {"__ln",         mt_ln},
  {"__sin",        mt_sin},
  {"__cos",        mt_cos},
  {"__tan",        mt_tan},
  {"__sinh",       mt_sinh},
  {"__cosh",       mt_cosh},
  {"__tanh",       mt_tanh},
  {"__arcsin",     mt_arcsin},
  {"__arccos",     mt_arccos},
  {"__arctan",     mt_arctan},
  {"__arcsec",     mt_arcsec},
  {"__nonzero",    Mpf_isnonzero},
  {"__zero",       Mpf_iszero},
  {"__even",       mt_even},
  {"__odd",        mt_odd},
  {"__int",        mt_int},
  {"__frac",       mt_frac},
  {"__integral",   mt_integral},
  {"__fractional", mt_fractional},
  {"__finite",     Mpf_isfinite},
  {"__infinite",   Mpf_isinfinite},
  {"__nan",        Mpf_isundefined},
  {"__eq",         mt_eq},
  {"__aeq",        mt_aeq},
  {"__lt",         mt_lt},
  {"__le",         mt_le},
  {"__gc",         mt_gc},
  {NULL, NULL}
};


static const struct luaL_Reg mtc_mpfrlib [] = {
  {"__unm",        mtc_neg},
  {"__abs",        mtc_abs},
  {"__sign",       mtc_sign},
  {"__add",        mtc_add},
  {"__sub",        mtc_subtract},
  {"__mul",        mtc_multiply},
  {"__div",        mtc_divide},
  {"__recip",      mtc_recip},
  {"__pow",        mtc_pow},
  {"__ipow",       mtc_ipow},
  {"__sqrt",       mtc_sqrt},
  {"__invsqrt",    mtc_invsqrt},
  {"__square",     mtc_square},
  {"__squareadd",  mt_squareadd},
  {"__cube",       mtc_cube},
  {"__antilog2",   mtc_antilog2},
  {"__antilog10",  mtc_antilog10},
  {"__exp",        mtc_exp},
  {"__ln",         mtc_ln},
  {"__sin",        mtc_sin},
  {"__cos",        mtc_cos},
  {"__tan",        mtc_tan},
  {"__sinh",       mtc_sinh},
  {"__cosh",       mtc_cosh},
  {"__tanh",       mtc_tanh},
  {"__arcsin",     mtc_arcsin},
  {"__arccos",     mtc_arccos},
  {"__arctan",     mtc_arctan},
  {"__arcsec",     mtc_arcsec},
  {"__sinc",       mtc_sinc},
  {"__nonzero",    Mpf_isnonzero},
  {"__zero",       Mpf_iszero},
  {"__imag",       mtc_imag},
  {"__real",       mtc_real},
  {"__infinite",   Mpf_isinfinite},
  {"__nan",        Mpf_isundefined},
  {"__eq",         mtc_eq},
  {"__aeq",        mt_aeq},
  {"__tostring",   mtc_tostring},  /* for output at the console, e.g. print(n) */
  {"__gc",         mtc_gc},
  {NULL, NULL}
};


static const luaL_Reg mpfrlib[] = {
  {"add",         Mpf_add},              /* added on August 21, 2020 */
  {"agm",         Mpf_agm},              /* added on August 25, 2020 */
  {"ai",          Mpf_ai},               /* added on August 25, 2020 */
  {"approx",      Mpf_approx},           /* added on December 28, 2025 */
  {"arccosh",     Mpf_arccosh},          /* added on August 23, 2023 */
  {"arccot",      Mpf_arccot},           /* added on January 14, 2026 */
  {"arccoth",     Mpf_arccoth},          /* added on August 28, 2023 */
  {"arccsc",      Mpf_arccsc},           /* added on January 14, 2026 */
  {"arccsch",     Mpf_arccsch},          /* added on August 28, 2023 */
  {"arcsech",     Mpf_arcsech},          /* added on August 28, 2023 */
  {"arcsinh",     Mpf_arcsinh},          /* added on August 23, 2023 */
  {"arctanh",     Mpf_arctanh},          /* added on August 23, 2023 */
  {"arctan2",     Mpf_arctan2},          /* added on August 21, 2020 */
  {"argument",    Mpf_argument},         /* added on January 16, 2026 */
  {"beta",        Mpf_beta},             /* added on August 25, 2020 */
  {"cathet",      Mpf_cathet},           /* added on August 28, 2023 */
  {"cbrt",        Mpf_cbrt},             /* added on August 21, 2020 */
  {"ceil",        Mpf_ceil},             /* added on August 21, 2020 */
  {"clone",       Mpf_clone},            /* added on August 30, 2020 */
  {"checkcmpfr",  Mpf_checkcmpfr},        /* added on January 21, 2026 */
  {"checkmpfr",   Mpf_checkmpfr},         /* added on January 21, 2026 */
  {"cmp",         Mpf_cmp},              /* added on January 12, 2026 */
  {"cmpd",        Mpf_cmpd},             /* added on August 28, 2023 */
  {"copysign",    Mpf_copysign},         /* added on August 30, 2020 */
  {"cosc",        Mpf_cosc},             /* added on January 12, 2026 */
  {"cot",         Mpf_cot},              /* added on August 21, 2020 */
  {"coth",        Mpf_coth},             /* added on August 23, 2023 */
  {"csc",         Mpf_csc},              /* added on August 21, 2020 */
  {"csch",        Mpf_csch},             /* added on August 23, 2023 */
  {"digamma",     Mpf_digamma},          /* added on August 25, 2020 */
  {"dim",         Mpf_dim},              /* added on August 21, 2020 */
  {"divide",      Mpf_divide},           /* added on August 21, 2020 */
  {"div2exp",     Mpf_div2exp},          /* added on November 15, 2025 */
  {"eint",        Mpf_eint},             /* added on August 25, 2020 */
  {"erf",         Mpf_erf},              /* added on August 25, 2020 */
  {"erfc",        Mpf_erfc},             /* added on August 25, 2020 */
  {"exp10",       Mpf_exp10},            /* added on August 21, 2020 */
  {"exp2",        Mpf_exp2},             /* added on August 21, 2020 */
  {"floor",       Mpf_floor},            /* added on August 21, 2020 */
  {"fma",         Mpf_fma},              /* added on August 21, 2020 */
  {"fmod",        Mpf_fmod},             /* added on August 21, 2020 */
  {"fms",         Mpf_fms},              /* added on August 21, 2020 */
  {"fraction",    mt_frac},              /* added on November 17, 2025 */
  {"frexp",       Mpf_frexp},            /* added on November 15, 2025 */
  {"gamma",       Mpf_gamma},            /* added on August 25, 2020 */
  {"getparts",    Mpfc_getparts},        /* added on January 11, 2026 */
  {"hypot",       Mpf_hypot},            /* added on August 21, 2020 */
  {"Inf",         Mpf_Inf},              /* added on August 27, 2020 */
  {"iscmpfr",     Mpf_iscmpfr},          /* added on January 20, 2026 */
  {"iseven",      mt_even},              /* added on November 17, 2025 */
  {"isfinite",    Mpf_isfinite},         /* added on August 21, 2020 */
  {"isfractional", mt_fractional},       /* added on November 17, 2025 */
  {"isinfinite",  Mpf_isinfinite},       /* added on August 21, 2020 */
  {"isintegral",  mt_integral},          /* added on November 17, 2025 */
  {"ismpfr",      Mpf_ismpfr},           /* added on January 20, 2026 */
  {"isnonzero",   Mpf_isnonzero},        /* added on August 21, 2020 */
  {"isodd",       mt_odd},               /* added on November 17, 2025 */
  {"isundefined", Mpf_isundefined},      /* added on August 21, 2020 */
  {"iszero",      Mpf_iszero},           /* added on August 21, 2020 */
  {"j0",          Mpf_j0},               /* added on August 25, 2020 */
  {"j1",          Mpf_j1},               /* added on August 25, 2020 */
  {"jn",          Mpf_jn},               /* added on August 25, 2020 */
  {"li2",         Mpf_li2},              /* added on August 25, 2020 */
  {"lgamma",      Mpf_lngamma},          /* added on August 25, 2020 */
  {"log10",       Mpf_log10},            /* added on August 21, 2020 */
  {"log2",        Mpf_log2},             /* added on August 21, 2020 */
  {"max",         Mpf_max},              /* added on August 27, 2020 */
  {"min",         Mpf_min},              /* added on August 27, 2020 */
  {"minmax",      Mpf_minmax},           /* added on November 15, 2025 */
  {"modf",        Mpf_modf},             /* added on August 21, 2020 */
  {"mul2exp",     Mpf_mul2exp},          /* added on November 15, 2025 */
  {"multiply",    Mpf_multiply},         /* added on August 21, 2020 */
  {"Nan",         Mpf_Nan},              /* added on August 27, 2020 */
  {"new",         Mpf_new},              /* added on August 21, 2020 */
  {"nexttoward",  Mpf_nexttoward},       /* added on August 30, 2020 */
  {"polygen",     Mpf_polygen},          /* added on January 16, 2026 */
  {"pow",         Mpf_pow},              /* added on August 21, 2020 */
  {"precision",   Mpf_precision},        /* added on August 21, 2020 */
  {"pytha",       Mpf_pytha},            /* added on August 28, 2023 */
  {"pytha4",      Mpf_pytha4},           /* added on August 28, 2023 */
  {"randinit",    Mpf_randinit},         /* added on August 30, 2020 */
  {"random",      Mpf_random},           /* added on August 30, 2020 */
  {"recsqrt",     Mpf_recsqrt},          /* added on August 27, 2020 */
  {"relerror",    Mpf_relerror},         /* added on August 28, 2023 */
  {"remquo",      Mpf_remquo},           /* added on November 17, 2025 */
  {"invsqrt",     Mpf_recsqrt},          /* added on August 27, 2020 */
  {"regular",     Mpf_regular},          /* added on November 17, 2025 */
  {"root",        Mpf_root},             /* added on August 28, 2023 */
  {"round",       Mpf_round},            /* added on August 21, 2020 */
  {"rounding",    Mpf_rounding},         /* added on August 25, 2020 */
  {"sec",         Mpf_sec},              /* added on August 21, 2020 */
  {"sech",        Mpf_sech},             /* added on August 23, 2023 */
  {"signbit",     Mpf_signbit},          /* added on August 30, 2020 */
  {"subtract",    Mpf_subtract},         /* added on August 21, 2020 */
  {"swap",        Mpf_swap},             /* added on August 27, 2020 */
  {"tanc",        Mpf_tanc},             /* added on January 12, 2026 */
  {"tonumber",    Mpf_tonumber},         /* added on August 21, 2020 */
  {"tostring",    Mpf_tostring},         /* added on August 21, 2020 */
  {"trunc",       Mpf_trunc},            /* added on August 21, 2020 */
  {"y0",          Mpf_y0},               /* added on August 25, 2020 */
  {"y1",          Mpf_y1},               /* added on August 25, 2020 */
  {"yn",          Mpf_yn},               /* added on August 25, 2020 */
  {"zeta",        Mpf_zeta},             /* added on August 25, 2020 */
  {"Zero",        Mpf_Zero},             /* added on August 27, 2020 */
  {NULL, NULL}
};


static int nopened = 0;

/*
** Open mpf library
*/

LUALIB_API int luaopen_mpfr (lua_State *L) {
  /* #if defined(_WIN32) && !defined(WINLEGACY)
  struct WinVer winversion;
  if (getWindowsVersion(&winversion) < MS_WINS2008) {
    luaL_error(L, "Error in " LUA_QS " package: need at least Windows 7 or 2008 Server or later.\nYou may download and install the Agena Legacy version instead.", "mpfr");
  }
  #endif */
  luaL_checkstack(L, 3, "not enough stack space");  /* 3.18.4 fix */
  nopened++;
  Mpfr_set_default_prec;
  luaL_newmetatable(L, "mpfr");
  luaL_register(L, NULL, mt_mpfrlib);  /* associate __gc method */
  luaL_newmetatable(L, "cmpfr");
  luaL_register(L, NULL, mtc_mpfrlib);  /* associate __gc method */
  luaL_register(L, AGENA_MPFRLIBNAME, mpfrlib);
  lua_rawsetstringstring(L, -1, "initstring", AGENA_LIBVERSION);
  /* constants */
  Mpf_Log2(L);
  lua_setfield(L, -2, "Ln2");
  Mpf_Pi(L);
  lua_setfield(L, -2, "Pi");
  Mpf_Euler(L);
  lua_setfield(L, -2, "Euler");
  Mpf_Catalan(L);
  lua_setfield(L, -2, "Catalan");
  /* clean-up at exit */
  if (nopened == 1) {
    gmp_randinit_default(randomstate);  /* 3.4.2 fix, initialise only once or space will not be cleaned up */
    atexit(mpfcleanup);
    signal(SIGTERM, mpfsigcleanup);  /* for CTRL+c */
  }
  return 1;
}

