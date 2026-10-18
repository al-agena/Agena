/*
** $Id: gmp.c, initiated September 22, 2019 $
** GNU Multiple Precision Arithmetic Library (GMP) binding
** See Copyright Notice in agena.h
** Supports signed and unsigned integers, only.
*/

#include <limits.h>
#include <stdio.h>

#include <gmp.h>

#define gmp_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnxlib.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_GMPLIBNAME "gmp"
LUALIB_API int (luaopen_gmp) (lua_State *L);
#endif


typedef unsigned long int ui;

typedef struct Gmp {
  mpz_t ui;
  int base;
} Gmp;

#ifndef gmp_bitcnt_t
#define gmp_bitcnt_t size_t
#endif

#define creategmpui(x) { \
  x = (Gmp *)lua_newuserdata(L, sizeof(Gmp)); \
  lua_setmetatabletoobject(L, -1, "gmp", 1); \
  mpz_init(x->ui); \
  x->base = 0; \
}

/* #define checkgmp(L, n) (Gmp *)luaL_checkudata(L, n, "gmp") */

static Gmp *checkgmp (lua_State *L, int i) {  /* 6.3.9 */
  Gmp *a;
  switch (lua_type(L, i)) {
    case LUA_TNUMBER: {
      ui x = agn_checknonnegint(L, i);
      creategmpui(a);
      mpz_set_ui(a->ui, x);
      a->base = 10;
      lua_replace(L, i);
      break;
    }
    case LUA_TSTRING: {
      int r;
      const char *str = agn_tostring(L, i);
      creategmpui(a);
      r = mpz_set_str(a->ui, str, 16);
      if (r == -1) {
        lua_pop(L, 1);
        luaL_error(L, "Error: input string malformed or memory allocation failed.");
      }
      a->base = 10;
      lua_replace(L, i);
      break;
    }
  }
  return (Gmp *)luaL_checkudata(L, i, "gmp");
}

#define isgmp(L,n) (luaL_isudata(L, n, "gmp"))

/* Creates an unsigned integer object (mpz_t GMP userdata object) from an unsigned integer n, or a string str representing an unsigned integer.
   If you pass a string you may indicate whether it is in decimal format by passing the optional second argument 10, and if its is in hexadecimal
   encoding, pass 16, which is the default. See also: `mp.setstring`. */
static int gmp_uint (lua_State *L) {
  Gmp *a;
  if (agn_isnumber(L, 1)) {
    creategmpui(a);
    mpz_set_ui(a->ui, (ui)agn_checknonnegint(L, 1));
    a->base = 10;
  } else if (agn_isstring(L, 1)) {
    int r, base;
    const char *str = agn_tostring(L, 1);
    base = agnL_optnonnegint(L, 2, 16);
    if (base == 1 || base > 62 || (base & 1) == 1)  /* first bit reserved for gmp.setstring flag */
      luaL_error(L, "Error in " LUA_QS ": base is invalid.", "gmp.uint");
    creategmpui(a);
    r = mpz_set_str(a->ui, str, base);
    if (r == -1) {
      lua_pop(L, 1);
      luaL_error(L, "Error in " LUA_QS ": input string malformed or allocation failed.", "gmp.uint");
    }
    a->base = 10;  /* 6.3.9 fix */
  } else if (isgmp(L, 1)) {  /* 6.5.5 extension */
    lua_settop(L, 1);
    return 1;
  } else
    luaL_error(L, "Error in " LUA_QS ": expected a number or string, got %s.", "gmp.uint", luaL_typename(L, 1));
  return 1;  /* leave userdata on the top of the stack */
}


static int gmp_sint (lua_State *L) {  /* Like gmp.uint, but creates a signed integer mpz_t GMP userdata object. */
  Gmp *a;
  if (agn_isnumber(L, 1)) {
    creategmpui(a);
    mpz_set_si(a->ui, agn_checknumber(L, 1));
    a->base = 10;
  } else if (agn_isstring(L, 1)) {
    int r, base;
    const char *str = agn_tostring(L, 1);
    base = agnL_optnonnegint(L, 2, 16);
    if (base == 1 || base > 62 || (base & 1) == 1)
      luaL_error(L, "Error in " LUA_QS ": base is invalid.", "gmp.sint");
    creategmpui(a);
    r = mpz_set_str(a->ui, str, base);
    if (r == -1) {
      lua_pop(L, 1);
      luaL_error(L, "Error in " LUA_QS ": allocation failed.", "gmp.sint");
    }
    a->base = 10;  /* 6.3.9 fix */
  } else if (isgmp(L, 1)) {  /* 6.5.5 extension */
    lua_settop(L, 1);
    return 1;
  } else
    luaL_error(L, "Error in " LUA_QS ": expected a number or string, got %s.", "gmp.sint", luaL_typename(L, 1));
  return 1;  /* leave userdata on the top of the stack */
}


/*****************************************************************************************************

	Macro templates

******************************************************************************************************/

/* take two mpint arguments a, b, apply procname to them and push the result onto the stack in new mpint r. */
#define template_2args_1ret(procname) { \
  Gmp *a, *b, *r; \
  a = checkgmp(L, 1); \
  b = checkgmp(L, 2); \
  creategmpui(r); \
  procname(r->ui, a->ui, b->ui); \
  r->base = a->base; \
}

/* take three mpint arguments r, a, b, apply procname to them; the result is implicitly stored to mpint r. */
#define template_3args_composite(procname) { \
  Gmp *a, *b, *r; \
  r = checkgmp(L, 1); \
  a = checkgmp(L, 2); \
  b = checkgmp(L, 3); \
  lua_settop(L, 1); \
  procname(r->ui, a->ui, b->ui); \
}

/* take two mpint arguments a, b, apply procname to them and push the result - a Lua/Agena number - onto the stack  */
#define template_2args_1num(procname) { \
  Gmp *a, *b; \
  a = checkgmp(L, 1); \
  b = checkgmp(L, 2); \
  lua_pushnumber(L, procname(a->ui, b->ui)); \
}

/* take a non-negative Agena integer, apply procname to it and push the result onto the stack in new mpint r. */
#define template_1int_1ret(procname,pn) { \
  ui n; \
  Gmp *r; \
  if (agn_isnonnegint(L, 1)) { \
    n = agn_tointeger(L, 1); \
  } else { \
    Gmp *a = checkgmp(L, 1); \
    if (mpz_sgn(a->ui) < 0) { \
      luaL_error(L, "Error in " LUA_QS ": second argument must be non-negative.", pn); \
    } \
    if (mpz_fits_ulong_p(a->ui) == 0 && mpz_sgn(a->ui) > 0) \
      luaL_error(L, "Error in " LUA_QS ": value is too large.", pn); \
    n = mpz_get_ui(a->ui); \
  } \
  creategmpui(r); \
  procname(r->ui, n); \
  r->base = 10; \
}


/*****************************************************************************************************

	Signed and Unsigned Integer Arithmetic

******************************************************************************************************/

/* gmp.add(a, b): Adds two mpints a, b, and returns a new mpint. Used by __add metamethod, i.e.: gmp.add(a, b) = a + b. */
static int gmp_add (lua_State *L) {  /* a + b */
  template_2args_1ret(mpz_add);
  return 1;
}


/* gmp.subtract(a, b): Subtracts two mpints a, b and returns a new mpint. Used by __sub metamethod, i.e.: gmp.subtract(a, b) = a - b. */
static int gmp_subtract (lua_State *L) {  /* a - b  */
  template_2args_1ret(mpz_sub);
  return 1;
}


/* gmp.multiply(a, b): Multiplies two mpints a, b and returns a new mpint. Used by __mul metamethod, i.e.: gmp.multiply(a, b) = a * b. */
static int gmp_multiply (lua_State *L) {  /* a * b */
  template_2args_1ret(mpz_mul);
  return 1;
}


/* gmp.divide(a, b): Divides two mpints a, b and returns a new mpint. Used by __div metamethod, i.e.: gmp.divide(a, b) = a / b. */
static int gmp_divide (lua_State *L) {
  Gmp *a, *b, *r;
  a = checkgmp(L, 1);
  b = checkgmp(L, 2);
  if (mpz_sgn(b->ui) == 0) {  /* 6.3.8 fix */
    lua_pushundefined(L);
  } else {
    creategmpui(r);
    mpz_div(r->ui, a->ui, b->ui);
    r->base = a->base;
  }
  return 1;
}


/* gmp.addmul(r, a, b): Multiplies two mpints a, b, adds the result to r and returns the updated value of r: i.e.: gmp.addmul(r, a, b) <=> r +:= a * b. */
static int gmp_addmul (lua_State *L) {  /* r +:= a*b */
  template_3args_composite(mpz_addmul);
  return 1;
}


/* gmp.submul(r, a, b): Multiplies two mpints a, b, subtracts the result from r and returns the updated value of r: i.e.: gmp.submul(r, a, b) <=> r -:= a * b. */
static int gmp_submul (lua_State *L) {  /* r -:= a*b */
  template_3args_composite(mpz_submul);
  return 1;
}


/* gmp.modulus(a, b): Computes the modulus of two mpints a, b and returns a new mpint. Used by __mod metamethod, i.e.: gmp.subtract(a, b) = a % b. */
static int gmp_modulus (lua_State *L) {  /* r := a % b */
  Gmp *a, *b, *r;
  a = checkgmp(L, 1);
  b = checkgmp(L, 2);
  if (mpz_sgn(b->ui) == 0) {  /* 6.3.8 fix */
    lua_pushundefined(L);
  } else {
    creategmpui(r);
    mpz_mod(r->ui, a->ui, b->ui);
    r->base = a->base;
  }
  return 1;
}


static int gmp_power (lua_State *L) {  /* r := a ^ b, 6.3.8 */
  Gmp *a, *r;
  unsigned long int e = 0;
  a = checkgmp(L, 1);
  if (agn_isnonnegint(L, 2)) {
    e = agn_tointeger(L, 2);
  } else {
    Gmp *b = checkgmp(L, 2);
    if (mpz_cmp_ui(b->ui, 0) < 0) {  /* 6.4.0 fix: return 0 with negative exponents */
      creategmpui(r);
      mpz_set_ui(r->ui, 0);
      return 1;
    } else {
      if (mpz_fits_ulong_p(b->ui) == 0 && mpz_sgn(b->ui) > 0)  /* 6.4.2 check */
        luaL_error(L, "Error in " LUA_QS ": value is too large.", "gmp.power");
      e = mpz_get_ui(b->ui);
    }
  }
  if (e == 0 && mpz_sgn(a->ui) == 0) {
    lua_pushundefined(L);
  } else {
    creategmpui(r);
    mpz_pow_ui(r->ui, a->ui, e);
    r->base = a->base;
  }
  return 1;
}


static int mt_antilog2 (lua_State *L) {  /* r := 2^a, 6.4.0 */
  Gmp *a, *r;
  mpz_t two;
  unsigned long int e = 0;
  a = checkgmp(L, 1);
  creategmpui(r);
  if (mpz_cmp_ui(a->ui, 0) < 0) {
    mpz_set_ui(r->ui, 0);
  } else {
    if (mpz_fits_ulong_p(a->ui) == 0 && mpz_sgn(a->ui) > 0) {  /* 6.4.2 check */
      lua_pop(L, 1);
      luaL_error(L, "Error in " LUA_QS ": value is too large.", "antilog2");
    }
    e = mpz_get_ui(a->ui);
    mpz_init(two);
    mpz_set_ui(two, 2);
    mpz_pow_ui(r->ui, two, e);
    mpz_clear(two);
  }
  r->base = a->base;
  return 1;
}


static int mt_antilog10 (lua_State *L) {  /* r := 10^a, 6.4.0 */
  Gmp *a, *r;
  mpz_t ten;
  unsigned long int e = 0;
  a = checkgmp(L, 1);
  creategmpui(r);
  if (mpz_cmp_ui(a->ui, 0) < 0) {
    mpz_set_ui(r->ui, 0);
  } else {
    if (mpz_fits_ulong_p(a->ui) == 0 && mpz_sgn(a->ui) > 0) {  /* 6.4.2 check */
      lua_pop(L, 1);
      luaL_error(L, "Error in " LUA_QS ": value is too large.", "antilog10");
    }
    e = mpz_get_ui(a->ui);
    mpz_init(ten);
    mpz_set_ui(ten, 10);
    mpz_pow_ui(r->ui, ten, e);
    mpz_clear(ten);
  }
  r->base = a->base;
  return 1;
}


/* gmp.root(a, n): Computes the truncated integer part of the n-th root of a, with a an mpint, n a number,
   and returns the result as a new mpint. */
static int gmp_root (lua_State *L) {  /* integer root, 6.3.9 */
  Gmp *a, *r;
  ui e = 0;
  a = checkgmp(L, 1);
  if (agn_isnonnegint(L, 2)) {
    e = agn_tointeger(L, 2);
  } else {
    Gmp *b = checkgmp(L, 2);
    if (mpz_fits_ulong_p(b->ui) == 0 && mpz_sgn(b->ui) > 0) {  /* 6.4.2 check */
      luaL_error(L, "Error in " LUA_QS ": second argument is too large.", "gmp.root");
    }
    e = mpz_get_ui(b->ui);
  }
  if (e == 0) {
    lua_pushundefined(L);
  } else {
    creategmpui(r);
    mpz_root(r->ui, a->ui, e);
    r->base = a->base;
  }
  return 1;
}


/* Set root to the truncated integer part of the nth root of u. Set rem to the remainder, u-root**n. 6.3.9 */
static int gmp_rootrem (lua_State *L) {
  Gmp *a, *r, *rem;
  unsigned long int e = 0;
  a = checkgmp(L, 1);
  if (agn_isnonnegint(L, 2)) {
    e = agn_tointeger(L, 2);
  } else {
    Gmp *b = checkgmp(L, 2);
    if (mpz_fits_ulong_p(b->ui) == 0 && mpz_sgn(b->ui) > 0) {  /* 6.4.2 check */
      luaL_error(L, "Error in " LUA_QS ": second argument is too large.", "gmp.rootrem");
    }
    e = mpz_get_ui(b->ui);
  }
  luaL_checkstack(L, 2, "not enough stack space");
  if (e == 0 && mpz_sgn(a->ui) == 0) {
    lua_pushundefined(L);
    lua_pushundefined(L);
  } else {
    creategmpui(r);
    creategmpui(rem);
    mpz_rootrem(r->ui, rem->ui, a->ui, e);
    r->base = a->base;
    rem->base = a->base;
  }
  return 2;
}


static int mt_sqrt (lua_State *L) {
  Gmp *a, *r;
  a = checkgmp(L, 1);
  creategmpui(r);
  mpz_sqrt(r->ui, a->ui);
  r->base = a->base;
  return 1;
}


/* gmp.neg(a): Returns -a, with a a mpint, as a new mpint. Used by __unm metamethod, i.e. gmp.neg(a) <=> -a.
  The operation is NOT in-place. */
static int gmp_neg (lua_State *L) {
  Gmp *a, *r;
  a = checkgmp(L, 1);
  creategmpui(r);
  mpz_neg(r->ui, a->ui);
  r->base = a->base;  /* 6.3.8 fix */
  return 1;
}


/* gmp.mul2exp(a, b): Computes a * 2^b, with a, b mpints, and returns the result in a new mpint. The operation is equivalent to a left shift by b bits. */
static int gmp_mul2exp (lua_State *L) {
  Gmp *a, *b, *r;
  ui e;
  a = checkgmp(L, 1);
  b = checkgmp(L, 2);  /* 6.4.1 extension */
  if (mpz_sgn(b->ui) < 0)
    luaL_error(L, "Error in " LUA_QS ": second argument must be non-negative.", "gmp.mul2exp");
  if (mpz_fits_ulong_p(b->ui) == 0 && mpz_sgn(b->ui) > 0) {  /* 6.4.2 check */
    luaL_error(L, "Error in " LUA_QS ": second argument is too large.", "gmp.mul2exp");
  }
  e = mpz_get_ui(b->ui);
  creategmpui(r);
  mpz_mul_2exp(r->ui, a->ui, e);
  r->base = a->base;
  return 1;
}


/* Similar to `frexp`, converting mpint a to a `mantissa` d, returned as an Agena number, and an `exponent` e,
   an Agena number by default, such that a = d * 2^e, with |d| in the range [0.5, 1). If the exponent e is too
   big, the function issues an error by default.

   If you pass `false` as the second argument, the exponent will be returned as an mpint instead of a number,
   not issuing an error with very large exponents.

   See also: `mp.mul2exp`, `mp.tonumber`. 6.4.2. */
static int gmp_getd2exp (lua_State *L) {
  long int e;
  int asnum;
  Gmp *a, *r;
  a = checkgmp(L, 1);
  asnum = agnL_optboolean(L, 2, 1);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushnumber(L, mpz_get_d_2exp(&e, a->ui));
  if (asnum) {
    if (e > AGN_LASTCONTINT) {
      lua_pop(L, 1);
      luaL_error(L, "Error in " LUA_QS ": exponent too big.", "gmp.getd2exp");
    }
    lua_pushnumber(L, e);
  } else {
    creategmpui(r);
    mpz_set_ui(r->ui, e);
    r->base = a->base;
  }
  return 2;
}


/* gmp.tdiv(a, b): Returns both quotient and remainder of a / b, with a, b units, both rounded towards zero. */
static int gmp_tdiv (lua_State *L) {
  Gmp *a, *b, *q, *r;
  a = checkgmp(L, 1);
  b = checkgmp(L, 2);
  luaL_checkstack(L, 2, "not enough stack space");  /* 6.3.8 fix */
  if (mpz_sgn(b->ui) == 0) {  /* 6.3.8 fix */
    lua_pushundefined(L);
    lua_pushundefined(L);
  } else {
    creategmpui(q);
    creategmpui(r);
    mpz_tdiv_qr(q->ui, r->ui, a->ui, b->ui);
    r->base = a->base;
    q->base = a->base;
  }
  return 2;
}


/* gmp.tdivq(a, b): Divides two mpints a, b and returns the resulting quotient as a new mpint, rounded towards zero. */
static int gmp_tdivq (lua_State *L) {
  Gmp *a, *b, *r;
  a = checkgmp(L, 1);
  b = checkgmp(L, 2);
  if (mpz_sgn(b->ui) == 0) {  /* 6.3.8 fix */
    lua_pushundefined(L);
  } else {
    creategmpui(r);
    mpz_tdiv_q(r->ui, a->ui, b->ui);
    r->base = a->base;
  }
  return 1;
}


/* gmp.tdivr(a, b): Divides two mpints a, b and returns the resulting remainder as a new mpint, rounded towards zero. */
static int gmp_tdivr (lua_State *L) {
  Gmp *a, *b, *r;
  a = checkgmp(L, 1);
  b = checkgmp(L, 2);
  if (mpz_sgn(b->ui) == 0) {  /* 6.3.8 fix */
    lua_pushundefined(L);
  } else {
    creategmpui(r);
    mpz_tdiv_r(r->ui, a->ui, b->ui);
    r->base = a->base;
  }
  return 1;
}


/* gmp.powm(a, b, c): With three uints, computes (a ^ b) % c and returns the result as a new mpint. */
static int gmp_powm (lua_State *L) {
  Gmp *r, *base, *expo, *mod;
  base = checkgmp(L, 1);
  expo = checkgmp(L, 2);
  mod  = checkgmp(L, 3);
  if (mpz_sgn(mod->ui) == 0) {  /* 6.3.8 fix */
    lua_pushundefined(L);
  } else {
    int secure = agnL_optboolean(L, 4, 0);  /* 6.3.8 extension */
    if (secure && (mpz_odd_p(mod->ui) == 0 || mpz_cmp_ui(expo->ui, 0) <= 0)) {
      lua_pushundefined(L);
    } else {
      creategmpui(r);
      if (secure) { /* intended for cryptographic purposes, where resilience to side-channel attacks is desired */
        mpz_powm_sec(r->ui, base->ui, expo->ui, mod->ui);
      } else
        mpz_powm(r->ui, base->ui, expo->ui, mod->ui);
      r->base = base->base;
    }
  }
  return 1;
}


/* gmp.mulm(a, b, c): With three uints, computes (a * b) % c and returns the result as a new mpint. 6.3.8 */
static int gmp_mulm (lua_State *L) {
  Gmp *r, *a, *b, *mod;
  a = checkgmp(L, 1);
  b = checkgmp(L, 2);
  mod  = checkgmp(L, 3);
  if (mpz_sgn(mod->ui) == 0) {
    lua_pushundefined(L);
  } else {
    mpz_t t;
    creategmpui(r);
    mpz_init(t);
    mpz_mul(t, a->ui, b->ui);
    mpz_mod(r->ui, t, mod->ui);
    mpz_clear(t);
    r->base = a->base;
  }
  return 1;
}


/* modular multiplicative inverse, 6.3.8. Replaces gmp.invert */
static int gmp_invm (lua_State *L) {
  Gmp *r, *base, *mod;
  base = checkgmp(L, 1);
  mod  = checkgmp(L, 2);
  if (mpz_sgn(mod->ui) == 0) {
    lua_pushundefined(L);
  } else {
    int rc;
    creategmpui(r);
    rc = mpz_invert(r->ui, base->ui, mod->ui);
    if (!rc) {
      agn_poptop(L);
      lua_pushundefined(L);
    } else {
      r->base = base->base;
    }
  }
  return 1;
}


/* gmp.log2(a): Like `ilog2`, but for mpint a. Rewritten and patched 6.3.8 */
static int gmp_log2 (lua_State *L) {
  Gmp *a = checkgmp(L, 1);
  int retmp = agnL_optboolean(L, 2, 0);
  if (mpz_sgn(a->ui) <= 0) {
    lua_pushundefined(L);
  } else if (retmp) {
    creategmpui(a);
    mpz_set_ui(a->ui, (ui)mpz_sizeinbase(a->ui, 2) - 1);
    a->base = 10;
  } else {
    lua_pushnumber(L, mpz_sizeinbase(a->ui, 2) - 1);
  }
  return 1;
}


/* gmp.log2(a): Like `ilog10`, but for mpint a. 6.3.8 */
static int gmp_log10 (lua_State *L) {
  Gmp *a = checkgmp(L, 1);
  int retmp = agnL_optboolean(L, 2, 0);
  if (mpz_sgn(a->ui) <= 0) {
    lua_pushundefined(L);
  } else if (retmp) {
    creategmpui(a);
    mpz_set_ui(a->ui, (ui)mpz_sizeinbase(a->ui, 10) - 1);
    a->base = 10;
  } else {
    lua_pushnumber(L, mpz_sizeinbase(a->ui, 10) - 1);
  }
  return 1;
}


/*****************************************************************************************************

	Number Theoretic Functions

******************************************************************************************************/


/* gmp.testprime: Checks whether mpint a is a prime and returns:
   - 0 if a is no prime,
   - 1 if a is probably prime,
   - 2 if a is definitely prime.
   The accuracy of the result can be controlled by the optional second parameter `reps` which by default is 15,
   with reasonable values between 15 and 50. */
static int gmp_testprime (lua_State *L) {
  int reps;
  Gmp *a = checkgmp(L, 1);
  reps = agnL_optposint(L, 2, 15);
  lua_pushnumber(L, mpz_probab_prime_p(a->ui, reps));
  /* 0 = no prime, 1 = probably prime, 2 = definitely prime */
  return 1;
}


/* gmp.nextprime(a): Returns the next prime to mpint a, as a new mpint. */
static int gmp_nextprime (lua_State *L) {
  Gmp *a, *r;
  a = checkgmp(L, 1);
  creategmpui(r);
  mpz_nextprime(r->ui, a->ui);
  r->base = a->base;
  return 1;
}


/* For whetever reason, even if you have GMP 6.3.0 installed, mpz_prevprime cannot be found ... */
#define PREVPRIME (defined(__DJGPP__) || defined(__OS2__) || defined(DEBIAN))

#ifdef PREVPRIME
/* Generated by Gemini AI, November 08, 2025
 * Custom function to find the greatest prime less than op.
 * Note: This implementation is for versions without mpz_prevprime.
 * The primality test is probabilistic (Miller-Rabin).
 *
 * rop: The mpz_t to store the result (the previous prime).
 * op: The mpz_t to start the search from.
 * reps: The number of Miller-Rabin repetitions (e.g., 25 for strong probability).
 *
 * Returns: 1 if a probable prime is found, 0 otherwise (e.g., if op <= 3). */
static int aux_gmpz_prevprime (mpz_t rop, const mpz_t op, int reps) {
  /* 1. Check for small numbers where no previous prime exists or is complex */
  if (mpz_cmp_ui(op, 2) <= 0) {
    /* No prime exists before 3 (primes are 2, 3, 5, ...).
       If op is 3, the previous prime is 2, which we handle next.
       We can simply return 0 or set rop to 0 if preferred, but for op<=3, let's return 0. */
    return 0;
  }
  /* 2. Start the search at op - 1 */
  mpz_sub_ui(rop, op, 1);
  /* Check if the starting candidate is 2 (e.g., if op was 3) */
  if (mpz_cmp_ui(rop, 2) == 0) return 1;  /* rop is already set to 2 */
  /* Optimization: Skip even numbers (start by making rop odd if it isn't)
     The only even prime is 2, which is handled above. */
  if (mpz_even_p(rop)) {
    mpz_sub_ui(rop, rop, 1); /* Decrement to the largest odd number less than op */
  }
  /* 3. Loop: Check primality and decrement by 2 */
  while (mpz_cmp_ui(rop, 3) >= 0) {
    int is_prime = mpz_probab_prime_p(rop, reps);
    if (is_prime > 0) return 1;  // Found a probable prime
    /* Go to the next smaller odd number (decrement by 2) */
    mpz_sub_ui(rop, rop, 2);
  }
  /* 4. Fallback: No prime found (should only happen if op <= 3 originally) */
  return 0;
}
#endif

/* gmp.prevprime(a): Returns the greatest prime less than mpint a, as a new mpint. 6.3.9 */
static int gmp_prevprime (lua_State *L) {
  int rc;
  Gmp *a, *r;
  a = checkgmp(L, 1);
  creategmpui(r);
#ifdef PREVPRIME
  rc = aux_gmpz_prevprime(r->ui, a->ui, 25);
#else
  rc = mpz_prevprime(r->ui, a->ui);
#endif
  if (rc == 0) {
    mpz_set_ui(r->ui, 0);
  }
  r->base = a->base;
  return 1;
}


/* gmp.gcd(a, b): Returns the greatest common divisor of mpints a and b, as a new mpint. */
static int gmp_gcd (lua_State *L) {
  template_2args_1ret(mpz_gcd);
  return 1;
}


/* gmp.gcdext(s, t, a, b):
   Returns the greatest common divisor of mpint a and mpint b and returns the result. In addition the function sets mpints s and t to coefficients
   satisfying a*s + b*t = (the return). */
static int gmp_gcdext (lua_State *L) {
  Gmp *r, *a, *b, *c, *d;
  a = checkgmp(L, 1);
  b = checkgmp(L, 2);
  c = checkgmp(L, 3);
  d = checkgmp(L, 4);
  creategmpui(r);
  mpz_gcdext(r->ui, a->ui, b->ui, c->ui, d->ui);
  r->base = a->base;
  return 1;
}


/* gmp.lcm(a, b): Returns the least common multiple of mpints a and b, as a new mpint. */
static int gmp_lcm (lua_State *L) {
  template_2args_1ret(mpz_lcm);
  return 1;
}


/* gmp.jacobi(a, b): Computes the Jacobi symbol (a/b) of the mpints a, b, which is defined only if b is odd. The return is a new mpint. */
static int gmp_jacobi (lua_State *L) {
  template_2args_1num(mpz_jacobi);
  return 1;
}


/* gmp.legendre(a, b): Computes the Legendre symbol (a/p) of the mpints a, b, which is defined only if p is an odd positive prime. The return is a new mpint. */
static int gmp_legendre (lua_State *L) {
  template_2args_1num(mpz_legendre);
  return 1;
}


/* gmp.kronecker(a, b): Computes the Jacobi symbol (a/b) for mpints a, b, with the Kronecker extension (a/2)=(2/a) with odd a, and (a/2)=0 with even a.
   The return is a new mpint. */
static int gmp_kronecker (lua_State *L) {
  template_2args_1num(mpz_kronecker);
  return 1;
}


/* gmp.remove(a, b): Removes all occurrences of the factor a from b returns the result as a new mpint. */
static int gmp_remove (lua_State *L) {
  template_2args_1ret(mpz_remove);
  return 1;
}


/* gmp.factorial(n): Returns the factorial of number n (_not_ mpint) as a new mpint. */
static int gmp_factorial (lua_State *L) {
  template_1int_1ret(mpz_fac_ui, "gmp.factorial");
  return 1;
}


/* gmp.fib(n): Returns the n-th Fibonacci number, with n a number (_not_ mpint), and returns a new mpint. */
static int gmp_fib (lua_State *L) {
  template_1int_1ret(mpz_fib_ui, "gmp.fib");
  return 1;
}


/* gmp.lucas(n): Sets the n-th Lucas number, with n a number (_not_ mpint), and returns a new mpint. */
static int gmp_lucas (lua_State *L) {
  template_1int_1ret(mpz_lucnum_ui, "gmp.lucas");
  return 1;
}


/* gmp.primorial(n): Returns the primorial of number n (_not_ mpint), i.e. the product of all positive
   prime numbers <= n. The return is a new mpint. */
#if (!(defined(__POWERPC__) || defined(__ppc__) || defined(__powerpc) || defined(__APPLE__) ))
static int gmp_primorial (lua_State *L) {
  template_1int_1ret(mpz_primorial_ui, "gmp.primorial");
  return 1;
}
#endif

/* gmp.binomial(n, k): Computes the binomial coefficient n (a mpint) over k (a number) and returns the result as a new mpint. */
static int gmp_binomial (lua_State *L) {
  ui n;
  Gmp *a, *b, *r;
  a = checkgmp(L, 1);
  b = checkgmp(L, 2);  /* 6.4.1 extension */
  if (mpz_sgn(b->ui) < 0)
    luaL_error(L, "Error in " LUA_QS ": second argument must be non-negative.", "gmp.binomial");
  if (mpz_fits_ulong_p(b->ui) == 0 && mpz_sgn(b->ui) > 0) {  /* 6.4.2 check */
    luaL_error(L, "Error in " LUA_QS ": second argument is too large.", "gmp.binomial");
  }
  n = mpz_get_ui(b->ui);
  creategmpui(r);
  mpz_bin_ui(r->ui, a->ui, n);
  r->base = a->base;
  return 1;
}


/*****************************************************************************************************

	Bitwise operations

******************************************************************************************************/


/* gmp.andint: Conducts a bitwise-and, `a and b`, with a, b mpints, and returns the result as a new mpint. */
static int gmp_band (lua_State *L) {
  template_2args_1ret(mpz_and);
  return 1;
}


/* gmp.orint: Conducts a bitwise-or, `a or b`, with a, b mpints, and returns the result as a new mpint. */
static int gmp_bor (lua_State *L) {
  template_2args_1ret(mpz_ior);
  return 1;
}


/* gmp.xorint: Conducts a bitwise-xor, `a xor b`, with a, b mpints, and returns the result as a new mpint. */
static int gmp_bxor (lua_State *L) {
  template_2args_1ret(mpz_xor);
  return 1;
}


/* gmp.com(a): Computes the one’s complement of mpint a and returns the result as a new mpint. */
static int gmp_com (lua_State *L) {
  Gmp *a, *r;
  a = checkgmp(L, 1);
  creategmpui(r);
  mpz_com(r->ui, a->ui);
  r->base = a->base;
  return 1;
}


/* Scan mpint a, starting from bit n (a number, not an mpint), towards more significant bits, until the first 0 or 1 bit
   (respectively) is found. The functions return the index of the found bit. Bit positions start from 0. */
static int gmp_scan0 (lua_State *L) {
  ui n;
  Gmp *a = checkgmp(L, 1);
  n = agn_checknonnegint(L, 2);
  lua_pushnumber(L, mpz_scan0(a->ui, n));
  return 1;
}


static int gmp_scan1 (lua_State *L) {
  ui n;
  Gmp *a = checkgmp(L, 1);
  n = agn_checknonnegint(L, 2);
  lua_pushnumber(L, mpz_scan1(a->ui, n));
  return 1;
}


/* gmp.popcount: Computes the population count of a (number of 1 bits in the binary representation) and returns the result a number. */
static int gmp_popcount (lua_State *L) {
  Gmp *a = checkgmp(L, 1);
  lua_pushnumber(L, mpz_popcount(a->ui));
  return 1;
}


/* gmp.mostsigbit(a): Returns the position of the most significant bit (msb) as a number, counting from bit position number 0. If all bits are cleared,
   i.e. zero, returns -1. */
static int gmp_mostsigbit (lua_State *L) {
  Gmp *a;
  ui i = 0;
  a = checkgmp(L, 1);
  if (mpz_sgn(a->ui) == 0) {
    lua_pushnumber(L, -1);
  } else {  /* search for the most significant bit */
    while (mpz_scan1(a->ui, i) != ULONG_MAX) i++;
    lua_pushnumber(L, (mpz_tstbit(a->ui, --i) == 1) ? i : AGN_NAN);  /* better sure than sorry */
  }
  return 1;
}


/* gmp.leastsigbit(a): Returns the position of the least significant bit (lsb) as a number, counting from bit position number 0. If all bits are cleared,
   i.e. zero, returns -1. */
static int gmp_leastsigbit (lua_State *L) {
  Gmp *a;
  ui i = 0;
  a = checkgmp(L, 1);
  if (mpz_sgn(a->ui) == 0) {
    lua_pushnumber(L, -1);
  } else {  /* search for the most significant bit */
    while (mpz_scan1(a->ui, i) != ULONG_MAX && mpz_tstbit(a->ui, i) == 0) i++;
    lua_pushnumber(L, (mpz_tstbit(a->ui, i) == 1) ? i : AGN_NAN);  /* better sure than sorry */
  }
  return 1;
}


/* gmp.hamdist(a, b): Computes the hamming distance between mpint a and mpint b, i.e. the number of positions a and b have different bit values.
   The count is returned as a number. */
static int gmp_hamdist (lua_State *L) {
  template_2args_1num(mpz_hamdist);
  return 1;
}


/* Sets bit n in mpint r. The function returns nothing. n is an integer position counting from 0. */
static int gmp_setbit (lua_State *L) {
  ui n;
  Gmp *a = checkgmp(L, 1);
  n = agn_checknonnegint(L, 2);
  mpz_setbit(a->ui, n);
  return 0;
}


/* Returns bit n in mpint r. The function returns a number. n is an integer position counting from 0. */
static int gmp_getbit (lua_State *L) {
  ui n;
  Gmp *a = checkgmp(L, 1);
  n = agn_checknonnegint(L, 2);
  lua_pushnumber(L, mpz_tstbit(a->ui, n));
  return 1;
}


/* Clears bit n in mpint r. The function returns nothing. n is an integer position counting from 0. */
static int gmp_clrbit (lua_State *L) {  /* clear bit */
  ui n;
  Gmp *a = checkgmp(L, 1);
  n = agn_checknonnegint(L, 2);
  mpz_clrbit(a->ui, n);
  return 0;
}


/* Conducts a bitwise complement (`not` operation) on bit number n in uint r. n is an integer position counting from 0. The function returns nothing. */
static int gmp_combit (lua_State *L) {
  ui n;
  Gmp *a = checkgmp(L, 1);
  n = agn_checknonnegint(L, 2);
  mpz_combit(a->ui, n);
  return 0;
}


/*****************************************************************************************************

	Miscellaneous

******************************************************************************************************/

/* gmp.tonumber(a): Returns the numeric value in mpint a as a number. */
static int gmp_tonumber (lua_State *L) {
  Gmp *a = checkgmp(L, 1);
  lua_pushnumber(L, mpz_get_d(a->ui));
  return 1;
}


/* gmp.swap(a, b): Swaps the values in mpints a and b. The function returns nothing. */
static int gmp_swap (lua_State *L) {
  Gmp *a, *b;
  a = checkgmp(L, 1);
  b = checkgmp(L, 2);
  mpz_swap(a->ui, b->ui);
  return 0;
}


/* gmp.cmp(a, b): Compares a and b and returns a positive number if a > b, 0 if a = b, and a negative number if a < b. */
static int gmp_cmp (lua_State *L) {
  template_2args_1num(mpz_cmp);
  return 1;
}


/* gmp.cmpabs(a, b): Compares the absolute values of a and b and returns a positive number if a > b, 0 if a = b, and a negative number if a < b. */
static int gmp_cmpabs (lua_State *L) {
  template_2args_1num(mpz_cmpabs);
  return 1;
}


/* gmp.iseven(a): Checks whether mpint a represents an even integer and returns `true` or `false`. */
static int gmp_iseven (lua_State *L) {
  Gmp *a = checkgmp(L, 1);
  lua_pushboolean(L, mpz_even_p(a->ui) != 0);
  return 1;
}


/* gmp.isodd(a): Checks whether mpint a represents an odd integer and returns `true` or `false`. */
static int gmp_isodd (lua_State *L) {
  Gmp *a = checkgmp(L, 1);
  lua_pushboolean(L, mpz_odd_p(a->ui) != 0);
  return 1;
}


/* gmp.ispow2(a): Checks whether mpint a is a power of two and returns `true` or `false`. 6.3.8 */
static int gmp_ispow2 (lua_State *L) {
  Gmp *a = checkgmp(L, 1);
  lua_pushboolean(L, (mpz_cmp_ui(a->ui, 0) <= 0) ? 0 : mpz_popcount(a->ui) == 1);
  return 1;
}


static int gmp_perfectpower (lua_State *L) {  /* 6.3.9 */
  Gmp *a = checkgmp(L, 1);
  lua_pushboolean(L, mpz_perfect_power_p(a->ui) != 0);
  return 1;
}


static int gmp_perfectsquare (lua_State *L) {  /* 6.3.9 */
  Gmp *a = checkgmp(L, 1);
  lua_pushboolean(L, mpz_perfect_square_p(a->ui) != 0);
  return 1;
}


/* gmp.sizeinbase(a, base): Returns the size of `a` measured in number of digits in the given `base`, an integer.
   `base` can vary from 2 to 62. */
static int gmp_sizeinbase (lua_State *L) {
  ui n;
  Gmp *a = checkgmp(L, 1);
  n = agn_checknonnegint(L, 2);
  if (n < 2 || n > 62)
    luaL_error(L, "Error in " LUA_QS ": invalid base.", "gmp.sizeinbase");
  lua_pushnumber(L, mpz_sizeinbase(a->ui, n));
  return 1;
}


static int gmp_attrib (lua_State *L) {
  char *buffer = NULL;
  Gmp *a = checkgmp(L, 1);
  lua_createtable(L, 0, 3);
  lua_rawsetstringnumber(L, -1, "sizeinbase10", mpz_sizeinbase(a->ui, 10));  /* size of op measured in number of digits in base 10 */
  lua_rawsetstringnumber(L, -1, "limbs", mpz_size(a->ui));  /* size measured in number of limbs */
  lua_rawsetstringnumber(L, -1, "base", a->base);  /* base */
  buffer = mpz_get_str(NULL, (a->base == 0) ? 16 : a->base & 0x1E, a->ui);
  if (!buffer) {
    lua_rawsetstringstring(L, -1, "value", buffer);
  }
  xfree(buffer);
  if ((a->base & 0x01)) {
    char *buffer = NULL;
    buffer = (char *)mpz_export(buffer, NULL, 1, 1, 0, 0, a->ui);
    lua_rawsetstringstring(L, -1, "string", buffer);
    xfree(buffer);
  }
  return 1;
}


static int gmp_min (lua_State *L) {  /* 6.4.2 */
  lua_pushvalue(L, 2 - (mpz_cmp(checkgmp(L, 1)->ui, checkgmp(L, 2)->ui) < 0));
  return 1;
}


static int gmp_max (lua_State *L) {  /* 6.4.2 */
  lua_pushvalue(L, 2 - (mpz_cmp(checkgmp(L, 1)->ui, checkgmp(L, 2)->ui) > 0));
  return 1;
}


static int gmp_minmax (lua_State *L) {  /* 6.4.2 */
  Gmp *a = checkgmp(L, 1);
  Gmp *b = checkgmp(L, 2);
  int lt = mpz_cmp(a->ui, b->ui) < 0;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushvalue(L, 2 - lt);
  lua_pushvalue(L, 1 + lt);
  return 2;
}


/*****************************************************************************************************

	Metamethods

******************************************************************************************************/

/* abs methamethod and gmp.argument: determimes the absolute value */
static int mt_abs (lua_State *L) {
  Gmp *a, *r;
  a = checkgmp(L, 1);
  creategmpui(r);
  mpz_abs(r->ui, a->ui);
  r->base = a->base;  /* 6.3.8 fix */
  return 1;
}


static int mt_pow (lua_State *L) {  /* r := a * b, 6.3.8 */
  Gmp *a, *b, *r;
  a = checkgmp(L, 1);
  b = checkgmp(L, 2);
  if (mpz_sgn(a->ui) == 0 && mpz_sgn(b->ui) == 0) {
    lua_pushundefined(L);
  } else {
    creategmpui(r);
    if (mpz_cmp_ui(b->ui, 0) < 0) {  /* 6.4.0 fix: return 0 with negative exponents */
      mpz_set_ui(r->ui, 0);
    } else if (mpz_fits_ulong_p(b->ui) == 0 && mpz_sgn(b->ui) > 0) {  /* 6.4.2 check */
      lua_pop(L, 1);
      luaL_error(L, "Error in " LUA_QS ": second argument is too large.", "^");
    } else {
      mpz_pow_ui(r->ui, a->ui, mpz_get_ui(b->ui));
    }
    r->base = a->base;
  }
  return 1;
}

static int mt_square (lua_State *L) {  /* r := a * a, 6.3.9 */
  Gmp *a, *r;
  a = checkgmp(L, 1);
  creategmpui(r);
  mpz_pow_ui(r->ui, a->ui, 2);
  r->base = a->base;
  return 1;
}

static int mt_cube (lua_State *L) {  /* r := a * a * a, 6.3.9 */
  Gmp *a, *r;
  a = checkgmp(L, 1);
  creategmpui(r);
  mpz_pow_ui(r->ui, a->ui, 3);
  r->base = a->base;
  return 1;
}

static int mt_sign (lua_State *L) {
  Gmp *a = checkgmp(L, 1);
  lua_pushnumber(L, mpz_sgn(a->ui));
  return 1;
}

static int mt_zero (lua_State *L) {  /* 6.3.9 */
  Gmp *a = checkgmp(L, 1);
  lua_pushboolean(L, mpz_sgn(a->ui) == 0);
  return 1;
}

static int mt_nonzero (lua_State *L) {  /* 6.3.9 */
  Gmp *a = checkgmp(L, 1);
  lua_pushboolean(L, mpz_sgn(a->ui) != 0);
  return 1;
}

static int mt_size (lua_State *L) {
  Gmp *a = checkgmp(L, 1);
  lua_pushnumber(L, mpz_sizeinbase(a->ui, a->base));  /* 6.3.9 change */
  return 1;
}

#define template_gmp(cond) { \
  Gmp *a, *b; \
  a = checkgmp(L, 1); \
  b = checkgmp(L, 2); \
  lua_pushboolean(L, (cond)); \
  return 1; \
}

static int mt_isequal (lua_State *L) {
  template_gmp(0 == mpz_cmp(a->ui, b->ui));
}

static int mt_islessthan (lua_State *L) {
  template_gmp(0 > mpz_cmp(a->ui, b->ui));
}

static int mt_islessorequal (lua_State *L) {
  template_gmp(0 >= mpz_cmp(a->ui, b->ui));
}

static int mt_tostring (lua_State *L) {  /*  */
  Gmp *a;
  a = checkgmp(L, 1);  /* push userdata on stack */
  lua_settop(L, 1);   /* don't let the stack be polluted by futile additional arguments */
  if (agn_getutype(L, 1)) {
    char *buffer = mpz_get_str(NULL, (a->base == 0) ? 16 : a->base & 0x1E, a->ui);
    if (!buffer)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "gmp.__tostring");
    lua_pushfstring(L, "(%s)", buffer);  /* always return hex */
    lua_concat(L, 2);
    xfree(buffer);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid mp object.", "gmp.__tostring");
  return 1;
}

static int mt_gmpgc (lua_State *L) {  /* __gc method */
  Gmp *a;
  (void)L;
  a = checkgmp(L, 1);
  mpz_clear(a->ui);
  lua_setmetatabletoobject(L, 1, NULL, 0);  /* 2.21.9 */
  return 0;
}

static const struct luaL_Reg mt_gmplib [] = {
  {"__tostring",  mt_tostring},  /* for output at the console, e.g. print(n) */
  {"__unm",       gmp_neg},
  {"__add",       gmp_add},
  {"__sub",       gmp_subtract},
  {"__mul",       gmp_multiply},
  {"__div",       gmp_divide},
  {"__intdiv",    gmp_divide},
  {"__mod",       gmp_modulus},
  {"__pow",       mt_pow},
  {"__ipow",      mt_pow},
  {"__square",    mt_square},
  {"__cube",      mt_cube},
  {"__antilog2",  mt_antilog2},
  {"__antilog10", mt_antilog10},
  {"__sqrt",      mt_sqrt},
  {"__abs",       mt_abs},
  {"__sign",      mt_sign},
  {"__even",      gmp_iseven},
  {"__odd",       gmp_isodd},
  {"__zero",      mt_zero},
  {"__nonzero",   mt_nonzero},
  {"__size",      mt_size},
  {"__eq",        mt_isequal},
  {"__lt",        mt_islessthan},
  {"__le",        mt_islessorequal},
  {"__gc",        mt_gmpgc},
  {NULL, NULL}
};


static const luaL_Reg gmplib[] = {
  {"add", gmp_add},                         /* added on September 22, 2019 */
  {"addmul", gmp_addmul},                   /* added on September 22, 2019 */
  {"band", gmp_band},                       /* added on September 22, 2019 */
  {"argument", mt_abs},                     /* added on September 22, 2019 */
  {"attrib", gmp_attrib},                   /* added on September 22, 2019 */
  {"binomial", gmp_binomial},               /* added on September 22, 2019 */
  {"clrbit", gmp_clrbit},                   /* added on September 22, 2019 */
  {"cmp", gmp_cmp},                         /* added on September 22, 2019 */
  {"cmpabs", gmp_cmpabs},                   /* added on September 30, 2019 */
  {"combit", gmp_combit},                   /* added on September 22, 2019 */
  {"com", gmp_com},                         /* added on September 22, 2019 */
  {"divide", gmp_divide},                   /* added on September 22, 2019 */
  {"factorial", gmp_factorial},             /* added on September 22, 2019 */
  {"fib", gmp_fib},                         /* added on September 22, 2019 */
  {"gcd", gmp_gcd},                         /* added on September 22, 2019 */
  {"gcdext", gmp_gcdext},                   /* added on September 22, 2019 */
  {"getbit", gmp_getbit},                   /* added on September 22, 2019 */
  {"getd2exp", gmp_getd2exp},               /* added on November 13, 2025 */
  {"hamdist", gmp_hamdist},                 /* added on September 22, 2019 */
  {"invert", gmp_invm},                     /* added on September 22, 2019 */
  {"leastsigbit", gmp_leastsigbit},         /* added on September 26, 2019 */
  {"lucas", gmp_lucas},                     /* added on September 30, 2019 */
  {"invm", gmp_invm},                       /* added on November 07, 2025 */
  {"iseven", gmp_iseven},                   /* added on September 22, 2019 */
  {"isodd", gmp_isodd},                     /* added on September 22, 2019 */
  {"ispow2", gmp_ispow2},                   /* added on November 07, 2025 */
  {"jacobi", gmp_jacobi},                   /* added on September 22, 2019 */
  {"kronecker", gmp_kronecker},             /* added on September 22, 2019 */
  {"lcm", gmp_lcm},                         /* added on September 22, 2019 */
  {"legendre", gmp_legendre},               /* added on September 22, 2019 */
  {"log2", gmp_log2},                       /* added on September 25, 2019 */
  {"log10", gmp_log10},                     /* added on November 07, 2025 */
  {"max", gmp_max},                         /* added on November 15, 2025 */
  {"min", gmp_min},                         /* added on November 15, 2025 */
  {"minmax", gmp_minmax},                   /* added on November 15, 2025 */
  {"modulus", gmp_modulus},                 /* added on September 22, 2019 */
  {"mostsigbit", gmp_mostsigbit},           /* added on September 26, 2019 */
  {"mul2exp", gmp_mul2exp},                 /* added on September 22, 2019 */
  {"multiply", gmp_multiply},               /* added on September 22, 2019 */
  {"mulm", gmp_mulm},                       /* added on November 07, 2025 */
  {"neg", gmp_neg},                         /* added on September 25, 2019 */
  {"nextprime", gmp_nextprime},             /* added on September 22, 2019 */
  {"bor", gmp_bor},                         /* added on September 22, 2019 */
  {"perfectpower", gmp_perfectpower},       /* added on November 08, 2025 */
  {"perfectsquare", gmp_perfectsquare},     /* added on November 08, 2025 */
  {"popcount", gmp_popcount},               /* added on September 22, 2019 */
  {"power", gmp_power},                     /* added on November 07, 2025 */
  {"powm", gmp_powm},                       /* added on September 22, 2019 */
  {"prevprime", gmp_prevprime},             /* added on November 08, 2025 */
#if (!(defined(__POWERPC__) || defined(__ppc__) || defined(__powerpc) || defined(__APPLE__)))
  {"primorial", gmp_primorial},             /* added on September 22, 2019 */
#endif
  {"remove", gmp_remove},                   /* added on September 22, 2019 */
  {"root", gmp_root},                       /* added on September 25, 2019 */
  {"rootrem", gmp_rootrem},                 /* added on November 08, 2025 */
  {"scan0", gmp_scan0},                     /* added on September 25, 2019 */
  {"scan1", gmp_scan1},                     /* added on September 25, 2019 */
  {"setbit", gmp_setbit},                   /* added on September 22, 2019 */
  {"sint", gmp_sint},                       /* added on September 27, 2019 */
  {"sizeinbase", gmp_sizeinbase},           /* added on September 22, 2019 */
  {"submul", gmp_submul},                   /* added on September 22, 2019 */
  {"subtract", gmp_subtract},               /* added on September 22, 2019 */
  {"swap", gmp_swap},                       /* added on September 22, 2019 */
  {"tdiv", gmp_tdiv},                       /* added on September 22, 2019 */
  {"tdivq", gmp_tdivq},                     /* added on September 22, 2019 */
  {"tdivr", gmp_tdivr},                     /* added on September 22, 2019 */
  {"testprime", gmp_testprime},             /* added on September 22, 2019 */
  {"tonumber", gmp_tonumber},               /* added on September 22, 2019 */
  {"tostring", mt_tostring},                /* added on September 22, 2019 */
  {"uint", gmp_uint},                       /* added on September 22, 2019 */
  {"bxor", gmp_bxor},                       /* added on September 22, 2019 */
  {NULL, NULL}
};


/*
** Open gmp library
*/

LUALIB_API int luaopen_gmp (lua_State *L) {
  luaL_newmetatable(L, "gmp");
  luaL_register(L, NULL, mt_gmplib);
  lua_pop(L, 1);  /* 7.11.2 fix */
  luaL_register(L, AGENA_GMPLIBNAME, gmplib);
  return 1;
}

