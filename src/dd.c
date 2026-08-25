/* Double-Double Arithmetic library.

   Generated with the help of Gemini AI, April 09, 2026. The implementation is based on the TwoSum and TwoProd algorithms.

   History: The TwoSum algorithm was discovered by Ole Møller in 1965 and later popularized and formally
   proven by Donald Knuth in "The Art of Computer Programming" (Volume 2). The TwoProd splitting technique
   is attributed to Veltkamp and Dekker (1971).

   IMPORTANT: To get the best results, use the compiler switches:

   set gccfileopts=-O3 -mfma -fno-unsafe-math-optimizations -DLUA_BUILD_AS_DLL -fgnu89-inline -Wno-attributes
   set gccfileopts=-O3 -fno-builtin -ffloat-store -fno-unsafe-math-optimizations -DLUA_BUILD_AS_DLL -fgnu89-inline -Wno-attributes
*/

#define dd_c
#define LUA_LIB

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "agena.h"

#include "agenalib.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "ddmath.h"

/*
import dd;

one := dd.new(1.0)
four := dd.new(4.0)
five := dd.new(5.0)

one / five:

d1_5 := one / five
d1_239 := one / dd.new(239)

# pi = 4 * (4 * atan(1/5) - atan(1/239))
my_pi := four * (four * arctan(d1_5) - arctan(d1_239))

print("DD Pi:     " & tostring(my_pi))
print("Standard:  3.141592653589793238462643383279")
# Standard double stops being accurate after 15 decimal places.

# DD Pi:     3.141592653589793|||55077723972352687279e+00
# Maple:     3.141592653589793|||238462643383279502884197
*/

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_DDLIBNAME "dd"  /* change linalg.c if you change that name */
LUALIB_API int (luaopen_dd) (lua_State *L);
#endif


#define checkdd(L,n) (dd_pair *)luaL_checkudata(L, n, AGENA_DDLIBNAME)
#define isdd(L,n)    (luaL_isudata(L, n, AGENA_DDLIBNAME) && agn_isutypeset(L, n))

static INLINE int aux_pushdd (lua_State *L, dd_pair val) {
  dd_pair *userdata = (dd_pair *)lua_newuserdata(L, sizeof(dd_pair));
  *userdata = val;
  lua_setmetatabletoobject(L, -1, AGENA_DDLIBNAME, 1);
  return 1;
}

static INLINE dd_pair getdd (lua_State *L, int idx) {
  if (agn_isnumber(L, idx)) {
    return (dd_pair){agn_tonumber(L, idx), 0.0};
  }
  dd_pair *ud = (dd_pair *)luaL_checkudata(L, idx, AGENA_DDLIBNAME);
  return *ud;
}

/* Applies a function 'f' (which takes and returns a dd_pair) to the value at stack idx 1 */
static int dd_apply1 (lua_State *L, dd_pair (*f)(dd_pair)) {
  /* 1. Get the input from the stack using your helper */
  dd_pair a = getdd(L, 1);
  /* 2. Execute the function and get the result */
  dd_pair result = f(a);
  /* 3. Push the result back to the Lua stack as a new userdata */
  return aux_pushdd(L, result);
}

static int dd_apply2 (lua_State *L, dd_pair (*f)(dd_pair, dd_pair)) {
  /* 1. Extract both arguments. getdd handles number vs userdata. */
  dd_pair a = getdd(L, 1);
  dd_pair b = getdd(L, 2);
  /* 2. Execute and push. */
  return aux_pushdd(L, f(a, b));
}

static int dd_apply1bool (lua_State *L, int (*f)(dd_pair)) {
  dd_pair a = getdd(L, 1);
  lua_pushboolean(L, f(a));
  return 1;
}

static int dd_apply2bool (lua_State *L, int (*f)(dd_pair, dd_pair)) {
  dd_pair a = getdd(L, 1);
  dd_pair b = getdd(L, 2);
  lua_pushboolean(L, f(a, b));
  return 1;
}

static int dd_apply1int (lua_State *L, int (*f)(dd_pair)) {
  dd_pair a = getdd(L, 1);
  lua_pushinteger(L, f(a));
  return 1;
}


static int ddf_new (lua_State *L) {
  dd_pair val;
  if (isdd(L, 1)) {  /* 7.4.1 extension */
    lua_settop(L, 1);
  } else {
    if (lua_gettop(L) >= 2) {
      val.hi = luaL_checknumber(L, 1);
      val.lo = luaL_checknumber(L, 2);
    } else {
      double d = luaL_checknumber(L, 1);
      val.hi = d;
      val.lo = 0.0;
    }
    aux_pushdd(L, val);
  }
  return 1;
}


static int mt_add (lua_State *L) { return dd_apply2(L, dd_add); }
static int mt_sub (lua_State *L) { return dd_apply2(L, dd_sub); }
static int mt_mul (lua_State *L) { return dd_apply2(L, dd_mul); }
static int mt_div (lua_State *L) { return dd_apply2(L, dd_div); }
static int mt_recip (lua_State *L) { return dd_apply1(L, dd_inv); }
static int mt_pow (lua_State *L) { return dd_apply2(L, dd_pow); }

static int mt_ipow (lua_State *L) {
  dd_pair a = getdd(L, 1);
  int b = agn_checkposint(L, 2);
  return aux_pushdd(L, dd_pow_n(a, b));
}

static int mt_abs (lua_State *L)    { return dd_apply1(L, dd_abs); }
static int mt_absdiff (lua_State *L) { return dd_apply2(L, dd_absdiff); }
static int mt_unm (lua_State *L)    { return dd_apply1(L, dd_unm); }
static int mt_sign (lua_State *L)   { return dd_apply1int(L, dd_sign); }

static int mt_sqrt (lua_State *L)   { return dd_apply1(L, dd_sqrt); }
static int mt_square (lua_State *L) { return dd_apply1(L, dd_square); }
static int mt_cube (lua_State *L)   { return dd_apply1(L, dd_cube); }

static int mt_ln (lua_State *L)     { return dd_apply1(L, dd_log); }
static int mt_exp (lua_State *L)    { return dd_apply1(L, dd_exp); }
static int mt_antilog2 (lua_State *L)  { return dd_apply1(L, dd_exp2); }
static int mt_antilog10 (lua_State *L) { return dd_apply1(L, dd_exp10); }

static int mt_lngamma (lua_State *L) { return dd_apply1(L, dd_lgamma); }

static int mt_sin (lua_State *L)    { return dd_apply1(L, dd_sin); }
static int mt_cos (lua_State *L)    { return dd_apply1(L, dd_cos); }
static int mt_tan (lua_State *L)    { return dd_apply1(L, dd_tan); }
static int mt_sinc (lua_State *L)   { return dd_apply1(L, dd_sinc); }
static int mt_sinh (lua_State *L)   { return dd_apply1(L, dd_sinh); }
static int mt_cosh (lua_State *L)   { return dd_apply1(L, dd_cosh); }
static int mt_tanh (lua_State *L)   { return dd_apply1(L, dd_tanh); }
static int mt_arcsin (lua_State *L) { return dd_apply1(L, dd_asin); }
static int mt_arccos (lua_State *L) { return dd_apply1(L, dd_acos); }
static int mt_arctan (lua_State *L) { return dd_apply1(L, dd_atan); }
static int mt_arcsec (lua_State *L) { return dd_apply1(L, dd_asec); }

static int mt_eq (lua_State *L)     { return dd_apply2bool(L, dd_eq); }
static int mt_lt (lua_State *L)     { return dd_apply2bool(L, dd_lt); }
static int mt_le (lua_State *L)     { return dd_apply2bool(L, dd_le); }

static int mt_aeq (lua_State *L) {
  dd_pair a = getdd(L, 1);
  dd_pair b = getdd(L, 2);
  lua_pushboolean(L, dd_approx(a, b, DD_APPROX_EPS));
  return 1;
}

static int mt_zero (lua_State *L)     { return dd_apply1bool(L, dd_zero); }
static int mt_nonzero (lua_State *L)  { return dd_apply1bool(L, dd_nonzero); }
static int mt_nan (lua_State *L)      { return dd_apply1bool(L, dd_isnan); }
static int mt_infinite (lua_State *L) { return dd_apply1bool(L, dd_isinf); }
static int mt_finite (lua_State *L)   { return dd_apply1bool(L, dd_isfinite); }
static int mt_entier (lua_State *L)   { return dd_apply1(L, dd_floor); }
static int mt_int (lua_State *L)      { return dd_apply1(L, dd_trunc); }
static int mt_frac (lua_State *L)     { return dd_apply1(L, dd_frac); }

static int mt_left (lua_State *L) {
  dd_pair a = getdd(L, 1);
  lua_pushnumber(L, a.hi);
  return 1;
}

static int mt_right (lua_State *L) {
  dd_pair a = getdd(L, 1);
  lua_pushnumber(L, a.lo);
  return 1;
}

static int mt_even (lua_State *L) {
  dd_pair a = getdd(L, 1);
  lua_pushboolean(L, tools_isevenorodd(a.hi) == 2);
  return 1;
}

static int mt_odd (lua_State *L) {
  dd_pair a = getdd(L, 1);
  lua_pushboolean(L, tools_isevenorodd(a.hi) == 1);
  return 1;
}

static int mt_index (lua_State *L) {
  if (isdd(L, 1) || agn_isnumber(L, 1)) {
    if (lua_gettop(L) == 2 && agn_isstring(L, 2)) {
      /* sizeof(<string constant>) == strlen(<string constant>) + 1 */
      return agn_initmethodcall(L, AGENA_DDLIBNAME, sizeof(AGENA_DDLIBNAME) - 1);
    }
  }
  luaL_error(L, "Error in " LUA_QS " package: illegal call.", AGENA_DDLIBNAME);
  return 0;
}

static int mt_tostring (lua_State *L) {
  dd_pair a = getdd(L, 1);
  lua_Number x = dd2double(a);
  if (isnan(x)) {
    lua_pushstring(L, "undefined (unknown)");
  } else if (isinf(x)) {
    lua_pushstring(L, (x >= 0) ? "infinity (unknown)" : "-infinity (unknown)");
  } else {
    luaL_checkstack(L, 4, "not enough stack space");
    if (agnL_gettablefield(L, "strings", "format", "strings.format", 1) != LUA_TFUNCTION) {
      return 1;
    }
    lua_pushstring(L, "%0.19g (%0.19g)");  /* use %g, not %f as this will zero very small values */
    lua_pushnumber(L, x);
    lua_pushnumber(L, a.lo);
    lua_call(L, 3, 1);
  }
  /* lua_pushstring(L, dd_to_str(a)); */
  return 1;
}

static int ddf_tostring (lua_State *L) {
  dd_pair a = getdd(L, 1);
  lua_pushstring(L, dd_to_str(a));
  return 1;
}

static int mt_gc (lua_State *L) {
  (void)checkdd(L, 1);
  lua_setmetatabletoobject(L, 1, NULL, 1);
  return 0;
}


static int ddf_arctan2 (lua_State *L)  { return dd_apply2(L, dd_atan2); }
static int ddf_cbrt (lua_State *L)     { return dd_apply1(L, dd_cbrt); }
static int ddf_copysign (lua_State *L) { return dd_apply2(L, dd_copysign); }
static int ddf_signbit (lua_State *L)  { return dd_apply1int(L, dd_signbit); }
static int ddf_floor (lua_State *L)    { return dd_apply1(L, dd_floor); }
static int ddf_ceil (lua_State *L)     { return dd_apply1(L, dd_ceil); }
static int ddf_trunc (lua_State *L)    { return dd_apply1(L, dd_trunc); }  /* trunc toward zero */
static int ddf_round (lua_State *L)    { return dd_apply1(L, dd_round); }  /* round to nearest integer */
static int ddf_fact (lua_State *L)     { return dd_apply1(L, dd_fac); }
static int ddf_sec (lua_State *L)      { return dd_apply1(L, dd_sec); }
static int ddf_csc (lua_State *L)      { return dd_apply1(L, dd_csc); }
static int ddf_cot (lua_State *L)      { return dd_apply1(L, dd_cot); }
static int ddf_sech (lua_State *L)     { return dd_apply1(L, dd_sech); }
static int ddf_csch (lua_State *L)     { return dd_apply1(L, dd_csch); }
static int ddf_coth (lua_State *L)     { return dd_apply1(L, dd_coth); }
static int ddf_arccsc (lua_State *L)   { return dd_apply1(L, dd_acsc); }
static int ddf_arccot (lua_State *L)   { return dd_apply1(L, dd_acot); }
static int ddf_hypot (lua_State *L)    { return dd_apply2(L, dd_hypot); }
static int ddf_hypot4 (lua_State *L)   { return dd_apply2(L, dd_hypot4); }
static int ddf_erf (lua_State *L)      { return dd_apply1(L, dd_erf); }
static int ddf_erfc (lua_State *L)     { return dd_apply1(L, dd_erfc); }
static int ddf_log2 (lua_State *L)     { return dd_apply1(L, dd_log2); }   /* 4.7.1 */
static int ddf_log10 (lua_State *L)    { return dd_apply1(L, dd_log10); }  /* 4.7.1 */

static int ddf_fma (lua_State *L) {
  dd_pair a = getdd(L, 1);
  dd_pair b = getdd(L, 2);
  dd_pair c = getdd(L, 3);
  aux_pushdd(L, dd_fma(a, b, c));
  return 1;
}

static int ddf_modf (lua_State *L) {
  dd_pair a = getdd(L, 1);
  dd_pair i_part;
  dd_pair f_part = dd_modf(a, &i_part);
  luaL_checkstack(L, 2, "not enough stack space");
  aux_pushdd(L, i_part); /* integral part */
  aux_pushdd(L, f_part); /* fractional part */
  return 2;
}

static int ddf_root (lua_State *L) {
  dd_pair a = getdd(L, 1);
  int b = agn_checkposint(L, 2);
  return aux_pushdd(L, dd_root_n(a, b));
}

static int ddf_sincos (lua_State *L) {
  dd_pair a = getdd(L, 1);
  dd_pair si, co;
  luaL_checkstack(L, 2, "not enough stack space");
  dd_sincos(a, &si, &co);
  aux_pushdd(L, si);
  aux_pushdd(L, co);
  return 2;
}

/* renorm (Internal Precision Rounding): Sometimes "rounding" refers to the internal
   process of ensuring the hi and lo parts are as "separated" as possible.
   While your two_sum and two_prod functions handle this automatically, a dedicated
   renormalization ensures that. */

static int ddf_renorm (lua_State *L) {
  double hi, lo;
  int nargs = lua_gettop(L);
  if (nargs == 2 && lua_istrue(L, nargs)) {  /* 7.5.1 extension */
    dd_pair z, a;
    a = getdd(L, 1);
    z.hi = dd2double(a);
    z.lo = 0.0;
    return aux_pushdd(L, z);
  }
  if (nargs == 2) {  /* 7.5.1 fix */
    hi = agn_checknumber(L, 1);
    lo = agn_checknumber(L, 2);
  } else {
    dd_pair a = getdd(L, 1);
    hi = a.hi;
    lo = a.lo;
  }
  return aux_pushdd(L, dd_renorm(hi, lo));
}


static int ddf_ldexp (lua_State *L) {
  dd_pair a = getdd(L, 1);
  int b = agn_checkinteger(L, 2);
  return aux_pushdd(L, dd_ldexp(a, b));
  return 1;
}


static int ddf_frexp (lua_State *L) {
  int e;
  dd_pair a = getdd(L, 1);
  dd_pair r = dd_frexp(a, &e);
  luaL_checkstack(L, 2, "not enough stack space");
  aux_pushdd(L, r);
  lua_pushinteger(L, e);
  return 2;
}


static int ddf_tonumber (lua_State *L) {
  dd_pair a = getdd(L, 1);
  lua_pushnumber(L, dd2double(a));
  return 1;
}


static int ddf_get (lua_State *L) {  /* 7.4.1 */
  int nargs = lua_gettop(L);
  dd_pair a = getdd(L, 1);
  if (nargs == 1) {
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushnumber(L, a.hi);
    lua_pushnumber(L, a.lo);
  } else {
    int i = agn_checkposint(L, 2);
    if (i > 2)
      luaL_error(L, "Error in " LUA_QS " package: index is out of range.", "dd.get");
    if (i == 1)
      lua_pushnumber(L, a.hi);
    else
      lua_pushnumber(L, a.lo);
  }
  return 1 + (nargs == 1);
}


/* A hard-coded C test to verify DD precision.
   This bypasses the script interpreter and the getdd() helper.
   Logistic Logistic map iteration: x = 4 * x * (1 - x)
*/
static int ddf_test1 (lua_State *L) {
  int i;
  /* 1. Use high-precision initialization for 0.1 (1/10) */
  dd_pair x = dd_div((dd_pair){1.0, 0.0}, (dd_pair){10.0, 0.0});
  dd_pair four = {4.0, 0.0};
  dd_pair one = {1.0, 0.0};
  printf("Starting test with x0 = %.20f (lo: %.20e)\n", x.hi, x.lo);
  for (i=0; i < 100; i++) {
    /* Manual breakdown of: x = 4 * x * (1 - x) */
    dd_pair t1 = dd_add(one, (dd_pair){-x.hi, -x.lo}); /* 1 - x */
    dd_pair t2 = dd_mul(x, t1);                        /* x * (1-x) */
    x = dd_mul(four, t2);                             /* 4 * x * (1-x) */
    if (i < 5 || i > 95) {
      printf("Iter %d: hi=%.20f lo=%.20e\n", i + 1, x.hi, x.lo);
    }
  }
  /* Return the final result to the script */
  return aux_pushdd(L, x);
}

static int ddf_test2 (lua_State *L) {
  int i;
  dd_pair x = DD_NOUGHT;
  dd_pair hundredth = dd_div(DD_ONE, (dd_pair){100.0, 0.0});
  printf("Starting test with x0 = %.20f (lo: %.20e)\n", x.hi, x.lo);
  for (i=0; i < 10000; i++) {
    x = dd_add(x, hundredth);
    if (i < 5 || i > 9995) {
      printf("Iter %d: hi=%.20f lo=%.20e\n", i + 1, x.hi, x.lo);
    }
  }
  /* Return the final result to the script */
  return aux_pushdd(L, x);
}


static int ddf_test3 (lua_State *L) {
  /* This is the hex-exact representation of 1/3 in Double-Double */
  dd_pair third;
  /* Copy these EXACTLY into your test3 function */
  third.hi = 0.333333333333333314829616256247;
  third.lo = 1.850371707708594e-17;
  /* If your printer is working, this will print many 3s */
  printf("C-Side Print: %s\n", dd_to_str(third));
  return aux_pushdd(L, third);
}


static int ddf_test4 (lua_State *L) {
  dd_pair third;
  /* Use the exact 64-bit bit-pattern for 1/3 */
  third.hi = 1.0 / 3.0;
  /* The exact remainder to make it a Double-Double */
  /* This is (1/3 - (double)(1/3)) */
  third.lo = (1.0 - (third.hi * 3.0)) / 3.0;
  printf("C-Side Print: %s\n", dd_to_str(third));
  return aux_pushdd(L, third);
}


static int ddf_isdd (lua_State *L) {  /* 7.4.1 */
  int i, r = 1, nargs = lua_gettop(L);
  for (i=0; i < nargs && r; i++) {
    r = isdd(L, i + 1);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int ddf_checkdd (lua_State *L) {  /* 7.4.1 */
  int i, nargs = lua_gettop(L);
  for (i=0; i < nargs; i++) {
    if (!isdd(L, i + 1)) {
      luaL_error(L, "Error: value #%d must be a dd number, got %s.", i + 1, luaL_typename(L, i + 1));
    }
  }
  return 0;
}


static const struct luaL_Reg dd_lib [] = {  /* metamethods for complex mapm numbers */
  {"__abs",         mt_abs},
  {"__absdiff",     mt_absdiff},
  {"__sign",        mt_sign},
  {"__add",         mt_add},
  {"__sub",         mt_sub},
  {"__mul",         mt_mul},
  {"__div",         mt_div},
  {"__recip",       mt_recip},
  {"__pow",         mt_pow},
  {"__ipow",        mt_ipow},
  {"__square",      mt_square},
  {"__cube",        mt_cube},
  {"__sqrt",        mt_sqrt},
  {"__ln",          mt_ln},
  {"__exp",         mt_exp},
  {"__antilog2",    mt_antilog2},
  {"__antilog10",   mt_antilog10},
  {"__lngamma",     mt_lngamma},
  {"__sin",         mt_sin},
  {"__cos",         mt_cos},
  {"__tan",         mt_tan},
  {"__sinh",        mt_sinh},
  {"__cosh",        mt_cosh},
  {"__tanh",        mt_tanh},
  {"__arcsin",      mt_arcsin},
  {"__arccos",      mt_arccos},
  {"__arctan",      mt_arctan},
  {"__arcsec",      mt_arcsec},
  {"__sinc",        mt_sinc},
  {"__left",        mt_left},
  {"__right",       mt_right},
  {"__unm",         mt_unm},
  {"__eq",          mt_eq},
  {"__lt",          mt_lt},
  {"__le",          mt_le},
  {"__eeq",         mt_eq},
  {"__aeq",         mt_aeq},
  {"__zero",        mt_zero},
  {"__nonzero",     mt_nonzero},
  {"__entier",      mt_entier},
  {"__int",         mt_int},
  {"__frac",        mt_frac},
  {"__even",        mt_even},
  {"__odd",         mt_odd},
  {"__nan",         mt_nan},
  {"__finite",      mt_finite},
  {"__infinite",    mt_infinite},
  {"__gc",          mt_gc},
  {"__tostring",    mt_tostring},
  {"__index",       mt_index},  /* OOP-style calls */
  {NULL, NULL}
};

static const luaL_Reg ddlib[] = {
  {"arccsc",       ddf_arccsc},
  {"arccot",       ddf_arccot},
  {"arctan2",      ddf_arctan2},
  {"cbrt",         ddf_cbrt},
  {"ceil",         ddf_ceil},
  {"checkdd",      ddf_checkdd},
  {"cot",          ddf_cot},
  {"coth",         ddf_coth},
  {"csc",          ddf_csc},
  {"csch",         ddf_csch},
  {"copysign",     ddf_copysign},
  {"erf",          ddf_erf},
  {"erfc",         ddf_erfc},
  {"fact",         ddf_fact},
  {"floor",        ddf_floor},
  {"fma",          ddf_fma},
  {"frexp",        ddf_frexp},
  {"get",          ddf_get},
  {"hypot",        ddf_hypot},
  {"hypot4",       ddf_hypot4},
  {"isdd",         ddf_isdd},
  {"ldexp",        ddf_ldexp},
  {"log2",         ddf_log2},
  {"log10",        ddf_log10},
  {"modf",         ddf_modf},
  {"new",          ddf_new},
  {"renorm",       ddf_renorm},
  {"root",         ddf_root},
  {"round",        ddf_round},
  {"sec",          ddf_sec},
  {"sech",         ddf_sech},
  {"signbit",      ddf_signbit},
  {"sincos",       ddf_sincos},
  {"tonumber",     ddf_tonumber},
  {"tostring",     ddf_tostring},
  {"trunc",        ddf_trunc},
  {"test1",        ddf_test1},
  {"test2",        ddf_test2},
  {"test3",        ddf_test3},
  {"test4",        ddf_test4},
  {NULL, NULL}
};


/*
** Open dd library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_DDLIBNAME);  /* create metatable */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, dd_lib);  /* methods */
}

LUALIB_API int luaopen_dd (lua_State *L) {
  createmeta(L);
  luaL_register(L, AGENA_DDLIBNAME, ddlib);
  aux_pushdd(L, DD_NOUGHT);
  lua_setfield(L, -2, "naught");
  aux_pushdd(L, DD_NOUGHT);
  lua_setfield(L, -2, "nought");
  aux_pushdd(L, DD_ONE);
  lua_setfield(L, -2, "one");
  aux_pushdd(L, DD_TWO);
  lua_setfield(L, -2, "two");
  aux_pushdd(L, DD_THREE);
  lua_setfield(L, -2, "three");
  aux_pushdd(L, DD_FOUR);
  lua_setfield(L, -2, "four");
  aux_pushdd(L, DD_FIVE);
  lua_setfield(L, -2, "five");
  aux_pushdd(L, DD_SIX);
  lua_setfield(L, -2, "six");
  aux_pushdd(L, DD_SEVEN);
  lua_setfield(L, -2, "seven");
  aux_pushdd(L, DD_EIGHT);
  lua_setfield(L, -2, "eight");
  aux_pushdd(L, DD_NINE);
  lua_setfield(L, -2, "nine");
  aux_pushdd(L, DD_TEN);
  lua_setfield(L, -2, "ten");
  aux_pushdd(L, DD_ELEVEN);
  lua_setfield(L, -2, "eleven");
  aux_pushdd(L, DD_TWELVE);
  lua_setfield(L, -2, "twelve");
  aux_pushdd(L, (dd_pair){16.0, 0.0});
  lua_setfield(L, -2, "sixteen");  /* 7.4.1 */
  aux_pushdd(L, (dd_pair){100.0, 0.0});
  lua_setfield(L, -2, "hundred");
  aux_pushdd(L, (dd_pair){1000.0, 0.0});
  lua_setfield(L, -2, "thousand");
  aux_pushdd(L, dd_div_d_d(1.0, 2.0));
  lua_setfield(L, -2, "half");
  aux_pushdd(L, dd_div_d_d(1.0, 3.0));
  lua_setfield(L, -2, "third");
  aux_pushdd(L, dd_div_d_d(1.0, 4.0));
  lua_setfield(L, -2, "quarter");
  aux_pushdd(L, dd_div_d_d(3.0, 4.0));
  lua_setfield(L, -2, "threequarter");
  aux_pushdd(L, dd_div_d_d(1.0, 5.0));
  lua_setfield(L, -2, "fifth");
  aux_pushdd(L, dd_div_d_d(1.0, 6.0));
  lua_setfield(L, -2, "sixth");
  aux_pushdd(L, dd_div_d_d(1.0, 8.0));
  lua_setfield(L, -2, "eighth");
  aux_pushdd(L, dd_div_d_d(1.0, 12.0));
  lua_setfield(L, -2, "twelfth");
  aux_pushdd(L, dd_div_d_d(1.0, 16.0));
  lua_setfield(L, -2, "sixteenth");
  aux_pushdd(L, dd_div_d_d(1.0, 10.0));
  lua_setfield(L, -2, "tenth");
  aux_pushdd(L, dd_div_d_d(1.0, 12.0));
  lua_setfield(L, -2, "twelfth");
  aux_pushdd(L, dd_div_d_d(1.0, 16.0));
  lua_setfield(L, -2, "sixteenth");
  aux_pushdd(L, dd_div_d_d(1.0, 100.0));
  lua_setfield(L, -2, "hundredth");
  aux_pushdd(L, dd_div_d_d(1.0, 1000.0));
  lua_setfield(L, -2, "thousandth");
  aux_pushdd(L, DD_PI);
  lua_setfield(L, -2, "Pi");
  aux_pushdd(L, DD_2PI);
  lua_setfield(L, -2, "Pi2");
  aux_pushdd(L, DD_PI_2);
  lua_setfield(L, -2, "PiO2");
  aux_pushdd(L, DD_PI_4);
  lua_setfield(L, -2, "PiO4");
  aux_pushdd(L, DD_PI_180);
  lua_setfield(L, -2, "PiO180");
  aux_pushdd(L, DD_E);
  lua_setfield(L, -2, "E");
  aux_pushdd(L, DD_LN2);
  lua_setfield(L, -2, "Ln2");
  aux_pushdd(L, DD_INV_LN2);
  lua_setfield(L, -2, "Invln2");
  aux_pushdd(L, DD_ZETA2);
  lua_setfield(L, -2, "Zeta2");
  aux_pushdd(L, DD_EULER_GAMMA);
  lua_setfield(L, -2, "Euler");
  aux_pushdd(L, DD_SQRT2);
  lua_setfield(L, -2, "sqrt2");
  aux_pushdd(L, DD_SQRT3);
  lua_setfield(L, -2, "sqrt3");
  aux_pushdd(L, DD_MAX);
  lua_setfield(L, -2, "ddmax");
  aux_pushdd(L, DD_MIN);
  lua_setfield(L, -2, "ddmin");
  aux_pushdd(L, DD_NAN);
  lua_setfield(L, -2, "undefined");
  aux_pushdd(L, DD_INF);
  lua_setfield(L, -2, "infinity");
  aux_pushdd(L, DD_APPROX_EPS);
  lua_setfield(L, -2, "Eps");
  return 1;
}


/*
1. Extract the pointer from Lua
dd_pair *p = (dd_pair *)luaL_checkudata(L, 1, "Lua.dd_pair");

2. Dereference it to use your library functions
dd_pair result = dd_some_function(*p);

3. Push the result back to Lua
dd_push(L, result);
*/


