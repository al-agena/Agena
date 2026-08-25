/*
** $Id: calc.c, initiated June 20, 2008 $
** Calculus library
** See Copyright Notice in agena.h
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define calc_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "calc.h"
#include "cephes.h"
#include "ddmath.h"  /* for savgol() */
#include "dual.h"
#include "interp.h"
#include "jbmath.h"
#include "long.h"
#include "numarray.h"
#include "prepdefs.h"

#define checkldblarray(L, n) (LongDoubleArray *)luaL_checkudata(L, n, "calc")

static int aux_newldblarray64 (lua_State *L, size_t n, const char *procname) {  /* creates numeric C array userdata object */
  DoubleArray *a;
  if (n < 1)
    luaL_error(L, "Error in " LUA_QS ": structure is empty.", procname);
  a = (DoubleArray *)lua_newuserdata(L, sizeof(DoubleArray));  /* push userdata on stack */
  a->v = calloc(n, sizeof(double));
  if (!a->v)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
  a->size = n;
  /* attach proper metatable with __gc method to userdata; name of mt name must be different from the one of
     the numarray package to avoid crashes during gc. */
  luaL_getmetatable(L, "calc");
  lua_setmetatable(L, -2);
  return 1;  /* leave userdata on the top of the stack */
}

/* 2.8.2, there allegedly is a bug in Lua when more than 256 arguments are pushed into the userdata stack; aux_newldblarray
   pushes userdata a. */
#define aux_pushud(L,a,nops,procname) { \
  if (nops > 256) \
    luaL_error(L, "Error in " LUA_QS ": received too many coefficients.", procname); \
  agnL_newldblarray(L, nops, procname); \
  a = (LongDoubleArray *)lua_touserdata(L, -1); \
}

#define aux_pushud64(L,a,nops,procname) { \
  if (nops > 256) \
    luaL_error(L, "Error in " LUA_QS ": received too many coefficients.", procname); \
  aux_newldblarray64(L, nops, procname); \
  a = (DoubleArray *)lua_touserdata(L, -1); \
}


static int arraygc (lua_State *L) {  /* __gc method */
  LongDoubleArray *a = checkldblarray(L, 1);  /* 2.9.0 */
  xfree(a->v);
  lua_setmetatabletoobject(L, 1, NULL, 0);
  return 0;
}

static const struct luaL_Reg numarray_arraylib [] = {
  {"__gc", arraygc},
  {NULL, NULL}
};


static int aux_checkepsandstep (lua_State *L, int pos, int *nargs, lua_Number *eps, lua_Number *step, int *iters, const char *procname) {
  int rc = 0;
  /* we do not set eps here since the functions using this aux function need different values */
  *step = 0.1;
  int checkoptions = 3;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streqx(option, "eps", "epsilon", NULL)) {
        *eps = agn_checkpositive(L, -1);
        rc = 1;
      } else if (tools_streq(option, "step")) {
        *step = agn_checkpositive(L, -1);
      } else if (tools_streq(option, "iters")) {
        *iters = agn_checkposint(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
  return rc;
}

/* Check options for various functions; pos counting from 1 */
static void aux_fcheckoptionslong (lua_State *L, int pos, int *nargs,
  long double *eps, long double *omega, int *samples, long double *length,
  int *adjust, long double *delta, int *iters, int *use64, const char *procname) {
  int checkoptions = 8;  /* check n options; CHANGE THIS if you add/delete options */
  *use64 = 0;
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streqx(option, "eps", "epsilon", NULL)) {
        *eps = agn_checkpositive(L, -1);
      } else if (tools_streq("omega", option)) {
        *omega = agn_checknumber(L, -1);
      } else if (tools_streq("samples", option)) {
        *samples = agn_checkposint(L, -1);
      } else if (tools_streq("iters", option)) {
        *iters = agn_checkposint(L, -1);
      } else if (tools_streqx(option, "length", "step", NULL)) {
        *length = agn_checkpositive(L, -1);
      } else if (tools_streq("adjust", option)) {
        *adjust = agn_checkboolean(L, -1);
      } else if (tools_streq("delta", option)) {
        *delta = agn_checkpositive(L, -1);
      } else if (tools_streq("use", option)) {
        *use64 = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
  /* lhf, May 29, 2011 at 12:20 at
     https://stackoverflow.com/questions/6167555/how-can-i-safely-iterate-a-lua-table-while-keys-are-being-removed
     "You can safely remove entries while traversing a table but you cannot create new entries, that is, new keys.
      You can modify the values of existing entries, though. (Removing an entry being a special case of that rule.)" */
}

/* Like aux_fcheckoptionslong but for doubles. */
static void aux_fcheckoptions (lua_State *L, int pos, int *nargs,
  double *eps, double *omega, int *samples, double *length,
  int *adjust, double *delta, int *iters, const char *procname) {
  int checkoptions = 7;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streqx(option, "eps", "epsilon", NULL)) {
        *eps = agn_checkpositive(L, -1);
      } else if (tools_streq("omega", option)) {
        *omega = agn_checknumber(L, -1);
      } else if (tools_streq("samples", option)) {
        *samples = agn_checkposint(L, -1);
      } else if (tools_streq("iters", option)) {
        *iters = agn_checkposint(L, -1);
      } else if (tools_streqx(option, "length", "step", NULL)) {
        *length = agn_checkpositive(L, -1);
      } else if (tools_streq("adjust", option)) {
        *adjust = agn_checkboolean(L, -1);
      } else if (tools_streq("delta", option)) {
        *delta = agn_checkpositive(L, -1);
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

static void aux_geteps (lua_State *L, lua_Number x, int pos, int *nargs, lua_Number *eps, lua_Number *delta, lua_Number *step, int *sided, int *deriv,
  int lenient, int minderiv, int maxderiv, int derivdefault, const char *procname) {
  int checkoptions = 5;  /* check n options; CHANGE THIS if you add/delete options */
  *eps = 0;
  *delta = 0;
  *sided = 0;
  *deriv = derivdefault;
  *step = 0.1;
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streqx(option, "eps", "epsilon", NULL)) {
        *eps = agn_checkpositive(L, -1);
      } else if (tools_streq(option, "delta")) {
        *delta = agn_checkpositive(L, -1);
      } else if (tools_streq(option, "deriv")) {
        *deriv = agn_checkinteger(L, -1);
        if (*deriv < minderiv || *deriv > maxderiv) {
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": deriv must be in [%d to %d].", procname, minderiv, maxderiv);
        }
      } else if (tools_streq(option, "side")) {
        option = agn_checkstring(L, -1);
        if (tools_streq(option, "left"))       *sided = 1;
        else if (tools_streq(option, "right")) *sided = 2;
        else if (tools_streq(option, "both"))  *sided = 3;
        else if (tools_streq(option, "all"))   *sided = 4;
        else {
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": unknown right-hand side value for side option.", procname);
        }
      } else if (tools_streq(option, "step")) {
        *step = agn_checkpositive(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
  if (*eps == 0) {
    *eps = (lenient) ? agn_getepsilon(L) :
                       ((*deriv < 3) ? tools_matheps(x) :
                                       tools_cbrteps(x));  /* 4.2.5 improvement for third derivative */
  }
  if (*delta == 0) {
    *delta = (*deriv < 3) ? tools_matheps(x) :
                            tools_cbrteps(x);  /* 4.2.5 improvement for third derivative */
  }
}


/* DE-Quadrature, Numerical Automatic Integrator for Improper Integral
      method    : Double Exponential (DE) Transformation
      dimension : one
      table     : use
   functions
      intde  : integrator of f(x) over (a,b).

   written by Takuya Ooura in C and available at http://www.kurims.kyoto-u.ac.jp/~ooura/intde.html.
   See intde2.c file.
   For an explanation of the algorithm used, see: http://en.wikipedia.org/wiki/Tanh-sinh_quadrature

   Copyright(C) 1996 Takuya OOURA.
   You may use, copy, modify this code for any purpose and without fee.
   You may distribute this ORIGINAL package. */

#define NSLOTS 8000

#define INTEG_EPS   1.0e-15L
#define INTEG_TINY  1.0e-307L

#ifndef __ARMCPU
static long double intde_w[NSLOTS] = { 0 };

static void intdeini (int lenaw, long double tiny, long double eps, long double *aw) {
  long double efs = 0.1L, hoff = 8.5L;  /* adjustable parameters */
  int noff, nk, k, j;
  long double tinyln, epsln, h0, ehp, ehm, h, t, ep, em, xw, wg;
  tinyln = -tools_logl(tiny);
  epsln = 1.0L - tools_logl(efs*eps);
  h0 = hoff/epsln;
  ehp = tools_expl(h0);
  ehm = 1.0L/ehp;
  aw[2] = eps;
  aw[3] = tools_expl(-ehm*epsln);
  aw[4] = tools_sqrtl(efs*eps);
  noff = 5;
  aw[noff] = 0.5L;
  aw[noff + 1] = h0;
  aw[noff + 2] = M_PIO2ld*h0*0.5L;
  h = 2.0L;
  nk = 0;
  k = noff + 3;
  do {
    t = h*0.5L;
    do {
      em = tools_expl(h0*t);
      ep = M_PIO2ld*em;
      em = M_PIO2ld/em;
      j = k;
      do {
        if (j >= NSLOTS - 2) break;  /* 2.34.10 invalid array access patch */
        xw = 1/(1 + tools_expl(ep - em));
        wg = xw*(1.0L - xw)*h0;
        aw[j] = xw;
        aw[j + 1] = wg*4.0L;
        aw[j + 2] = wg*(ep + em);
        ep *= ehp;
        em *= ehm;
        j += 3;
      } while (ep < tinyln && j <= lenaw - 3);
      t += h;
      k += nk;
    } while (t < 1);
    h *= 0.5L;
    if (nk == 0) {
      if (j > lenaw - 6) j -= 3;
      nk = j - noff;
      k += nk;
      aw[1] = nk;
    }
  } while (2*k - noff - 3 <= lenaw);
  aw[0] = k - 3;
}

static long double lastIntdeEps = INTEG_EPS;
#endif

static int calc_intde64 (lua_State *L);

static int calc_intde (lua_State *L) {
#if defined(__ARMCPU)
  return calc_intde64(L);
#else
  int noff, lenawm, nk, k, j, jtmp, jm, m, klim, nargs, samples, adjust, iters, use64;
  long double r, a, b, omega, length, eps, delta, err, epsh, ba, ir, xa, fa, fb, errt, errh, errd, h, iback, irback;
  /* "If there are fewer initializers in a brace-enclosed list than there are elements or members of an aggregate,
      or fewer characters in a string literal used to initialize an array of known size than there are elements
      in the array, the remainder of the aggregate shall be initialized implicitly the same as objects that have
      static storage duration." https://stackoverflow.com/questions/5636070/zero-an-array-in-c-code
      2.34.10 invalid array access patch */
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)iters; (void)use64;
  long double aw[NSLOTS] = { 0 };
  fa = fb = 0;  /* to prevent compiler warnings on Mac OS X and other UNIX flavours, 3.7.6/3.13.1 */
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  b = agnL_checknumber(L, 3);
  eps = INTEG_EPS;  /* 3.12.0 change for `eps` option */
  aux_fcheckoptionslong(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.intde");  /* 3.11.5 change */
  if (tools_isinf(a) || tools_isinf(b))  /* 4.2.4 fix */
    luaL_error(L, "Error in " LUA_QS ": at least one boundary is +/-infinity.", "calc.intde");
  else if (a > b)  /* 3.3.5 / 4.2.4 change: if a = b, then return zero below */
    luaL_error(L, "Error in " LUA_QS ": left boundary > right boundary.", "calc.intde");
  if (eps == lastIntdeEps)  /* 3.3.5/6 40 % boost */
    for (j=0; j < NSLOTS; j++) aw[j] = intde_w[j];
  else {
    lastIntdeEps = eps;
    intdeini(NSLOTS, INTEG_TINY, eps, aw);
  }
  noff = 5;
  lenawm = (int)(aw[0] + 0.5);
  nk = (int)(aw[1] + 0.5);
  epsh = aw[4];
  ba = b - a;
  r = agnL_fncall(L, 1, (a + b)*aw[noff], 4, nargs);  /* 3.11.5 change for multivariate support */
  ir = r*aw[noff + 1];
  r *= aw[noff + 2];
  err = fabsl(r);
  k = nk + noff;
  j = noff;
  do {
    j += 3;
    if (j >= NSLOTS - 2) break;  /* 2.34.10 fix against invalid array access when run in Valgrind */
    xa = ba*aw[j];
    fa = agnL_fncall(L, 1, a + xa, 4, nargs);  /* 3.11.5 change for multivariate support */
    fb = agnL_fncall(L, 1, b - xa, 4, nargs);  /* 3.11.5 change for multivariate support */
    ir = fmal(fa + fb, aw[j + 1], ir);  /* 6.7.7 change */
    fa *= aw[j + 2];
    fb *= aw[j + 2];
    r += fa + fb;
    err += fabsl(fa) + fabsl(fb);
  } while (aw[j] > epsh && j < k);
  errt = err*aw[3];
  errh = err*epsh;
  errd = 1.0L + 2.0L*errh;
  jtmp = j;
  while (fabsl(fa) > errt && j < k) {  /* 2.34.10 fix against invalid array access when run in Valgrind */
    j += 3;
    if (j >= NSLOTS - 2) break;  /* 2.34.10 fix against invalid array access when run in Valgrind */
    fa = agnL_fncall(L, 1, fmal(ba, aw[j], a), 4, nargs);  /* 3.11.5 change for multivariate support */
    ir = fmal(fa, aw[j + 1], ir);
    fa *= aw[j + 2];
    r += fa;
  }
  jm = j;
  j = jtmp;
  while (fabsl(fb) > errt && j < k) {  /* 2.34.10 fix against invalid array access when run in Valgrind */
    j += 3;
    if (j >= NSLOTS - 2) break;  /* 2.34.10 fix against invalid array access when run in Valgrind */
    fb = agnL_fncall(L, 1, fmal(-ba, aw[j], b), 4, nargs);  /* 3.11.5 change for multivariate support */
    ir = fmal(fb, aw[j + 1], ir);
    fb *= aw[j + 2];
    r += fb;
  }
  if (j < jm) jm = j;
  jm -= noff + 3;
  h = 1.0L;
  m = 1;
  klim = k + nk;
  while (errd > errh && klim <= lenawm) {
    iback = r;
    irback = ir;
    do {
      jtmp = k + jm;
      for (j=k + 3; j < NSLOTS - 2 && j <= jtmp; j += 3) {  /* 2.34.10 invalid array access patch */
        xa = ba*aw[j];
        fa = agnL_fncall(L, 1, a + xa, 4, nargs);  /* 3.11.5 change for multivariate support */
        fb = agnL_fncall(L, 1, b - xa, 4, nargs);  /* 3.11.5 change for multivariate support */
        ir = fmal(fa + fb, aw[j + 1], ir);
        r = fmal(fa + fb, aw[j + 2], r);
      }
      k += nk;
      j = jtmp;
      do {
        j += 3;
        if (j >= NSLOTS - 2) break;  /* 2.34.10 invalid array access patch */
        fa = agnL_fncall(L, 1, fmal(ba, aw[j], a), 4, nargs);  /* 3.11.5 change for multivariate support */
        ir = fmal(fa, aw[j + 1], ir);
        fa *= aw[j + 2];
        r += fa;
      } while (fabsl(fa) > errt && j < k);
      j = jtmp;
      do {
        j += 3;
        if (j >= NSLOTS - 2) break;  /* 2.34.10 invalid array access patch */
        fb = agnL_fncall(L, 1, b - ba*aw[j], 4, nargs);  /* 3.11.5 change for multivariate support */
        ir = fmal(fb, aw[j + 1], ir);
        fb *= aw[j + 2];
        r += fb;
      } while (fabsl(fb) > errt && j < k);
    } while (k < klim);
    errd = h*(fabsl(r - 2.0L*iback) + fabsl(ir - 2.0L*irback));
    h *= 0.5L;
    m *= 2;
    klim = 2*klim - noff;
  }
  r *= h*ba;
  err = (errd > errh) ? -errd*(m*fabsl(ba)) : err*aw[2]*(m*fabsl(ba));
  lua_pushnumber(L, (err < 0) ? AGN_NAN : r);  /* 6.7.9 change */
  lua_pushnumber(L, err);
  return 2;
#endif
}


static double intde_w64[NSLOTS] = { 0 };

static void intdeini64 (int lenaw, double tiny, double eps, double *aw) {
  double efs = 0.1L, hoff = 8.5;  /* adjustable parameters */
  int noff, nk, k, j;
  double tinyln, epsln, h0, ehp, ehm, h, t, tcs, tccs, ep, em, xw, wg;
  tinyln = -sun_log(tiny);
  epsln = 1.0 - sun_log(efs*eps);
  h0 = hoff/epsln;
  ehp = sun_exp(h0);
  ehm = 1.0/ehp;
  aw[2] = eps;
  aw[3] = sun_exp(-ehm*epsln);
  aw[4] = sqrt(efs*eps);
  noff = 5;
  aw[noff] = 0.5;
  aw[noff + 1] = h0;
  aw[noff + 2] = M_PIO2*h0*0.5;
  h = 2.0;
  nk = 0;
  k = noff + 3;
  do {
    tcs = tccs = 0.0;
    t = h*0.5;
    do {
      em = sun_exp(h0*t);
      ep = M_PIO2*em;
      em = M_PIO2/em;
      j = k;
      do {
        if (j >= NSLOTS - 2) break;  /* 2.34.10 invalid array access patch */
        xw = 1/(1 + sun_exp(ep - em));
        wg = xw*(1 - xw)*h0;
        aw[j] = xw;
        aw[j + 1] = wg*4.0;
        aw[j + 2] = wg*(ep + em);
        ep *= ehp;
        em *= ehm;
        j += 3;
      } while (ep < tinyln && j <= lenaw - 3);
      t = tools_kbadd(t, h, &tcs, &tccs);
      k += nk;
      t += tcs + tccs;
    } while (t < 1);
    h *= 0.5;
    if (nk == 0) {
      if (j > lenaw - 6) j -= 3;
      nk = j - noff;
      k += nk;
      aw[1] = nk;
    }
  } while (2*k - noff - 3 <= lenaw);
  aw[0] = k - 3;
}

#define INTEG_EPS64   1.0e-15
#define INTEG_TINY64  1.0e-307
static double lastIntdeEps64 = INTEG_EPS64;

static int calc_intde64 (lua_State *L) {
  int noff, lenawm, nk, k, j, jtmp, jm, m, klim, nargs, samples, adjust, iters;
  double r, a, b, omega, length, eps, delta, err, epsh, ba, ir, xa, fa, fb, errt, errh, errd, h, iback, irback;
  double rcs, rccs, ircs, irccs, errcs, errccs;
  /* "If there are fewer initializers in a brace-enclosed list than there are elements or members of an aggregate,
      or fewer characters in a string literal used to initialize an array of known size than there are elements
      in the array, the remainder of the aggregate shall be initialized implicitly the same as objects that have
      static storage duration." https://stackoverflow.com/questions/5636070/zero-an-array-in-c-code
      2.34.10 invalid array access patch */
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)iters;
  double aw[NSLOTS] = { 0 };
  fa = fb = rcs = rccs = ircs = irccs = errcs = errccs = 0.0;  /* fa, fb to prevent compiler warnings on Mac OS X and other UNIX flavours */
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  b = agnL_checknumber(L, 3);
  eps = INTEG_EPS64;
  aux_fcheckoptions(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, "calc.intde64");
  if (tools_isinf(a) || tools_isinf(b))
    luaL_error(L, "Error in " LUA_QS ": at least one boundary is +/-infinity.", "calc.intde64");
  else if (a > b)
    luaL_error(L, "Error in " LUA_QS ": left boundary > right boundary.", "calc.intde64");
  if (eps == lastIntdeEps64)
    for (j=0; j < NSLOTS; j++) aw[j] = intde_w64[j];
  else {
    lastIntdeEps64 = eps;
    intdeini64(NSLOTS, INTEG_TINY64, eps, aw);
  }
  noff = 5;
  lenawm = (int)(aw[0] + 0.5);
  nk = (int)(aw[1] + 0.5);
  epsh = aw[4];
  ba = b - a;
  r = agnL_fncall(L, 1, (a + b)*aw[noff], 4, nargs);
  ir = r*aw[noff + 1];
  r *= aw[noff + 2];
  err = fabs(r);
  k = nk + noff;
  j = noff;
  do {
    j += 3;
    if (j >= NSLOTS - 2) break;
    xa = ba*aw[j];
    fa = agnL_fncall(L, 1, a + xa, 4, nargs);
    fb = agnL_fncall(L, 1, b - xa, 4, nargs);
    ir = tools_kbadd(ir, (fa + fb)*aw[j + 1], &ircs, &irccs);
    fa *= aw[j + 2];
    fb *= aw[j + 2];
    r = tools_kbadd(r, fa + fb, &rcs, &rccs);
    err = tools_kbadd(err, fabs(fa) + fabs(fb), &errcs, &errccs);
  } while (aw[j] > epsh && j < k);
  err += errcs + errccs;
  errt = err*aw[3];
  errh = err*epsh;
  errd = 1 + 2*errh;
  jtmp = j;
  while (fabs(fa) > errt && j < k) {
    j += 3;
    if (j >= NSLOTS - 2) break;
    fa = agnL_fncall(L, 1, a + ba*aw[j], 4, nargs);
    ir = tools_kbadd(ir, fa*aw[j + 1], &ircs, &irccs);
    fa *= aw[j + 2];
    r = tools_kbadd(r, fa, &rcs, &rccs);
  }
  jm = j;
  j = jtmp;
  while (fabs(fb) > errt && j < k) {
    j += 3;
    if (j >= NSLOTS - 2) break;
    fb = agnL_fncall(L, 1, b - ba*aw[j], 4, nargs);
    ir = tools_kbadd(ir, fb*aw[j + 1], &ircs, &irccs);
    fb *= aw[j + 2];
    r = tools_kbadd(r, fb, &rcs, &rccs);
  }
  if (j < jm) jm = j;
  jm -= noff + 3;
  h = 1;
  m = 1;
  klim = k + nk;
  r += rcs + rccs;
  ir += ircs + irccs;
  while (errd > errh && klim <= lenawm) {
    iback = r;
    irback = ir;
    do {
      jtmp = k + jm;
      for (j=k + 3; j < NSLOTS - 2 && j <= jtmp; j += 3) {
        xa = ba*aw[j];
        fa = agnL_fncall(L, 1, a + xa, 4, nargs);
        fb = agnL_fncall(L, 1, b - xa, 4, nargs);
        ir = tools_kbadd(ir, (fa + fb)*aw[j + 1], &ircs, &irccs);
        r  = tools_kbadd(r, (fa + fb)*aw[j + 2], &rcs, &rccs);
      }
      k += nk;
      j = jtmp;
      do {
        j += 3;
        if (j >= NSLOTS - 2) break;
        fa = agnL_fncall(L, 1, a + ba*aw[j], 4, nargs);
        ir = tools_kbadd(ir, fa*aw[j + 1], &ircs, &irccs);
        fa *= aw[j + 2];
        r = tools_kbadd(r, fa, &rcs, &rccs);
      } while (fabs(fa) > errt && j < k);
      j = jtmp;
      do {
        j += 3;
        if (j >= NSLOTS - 2) break;
        fb = agnL_fncall(L, 1, b - ba*aw[j], 4, nargs);
        ir = tools_kbadd(ir, fb*aw[j + 1], &ircs, &irccs);
        fb *= aw[j + 2];
        r = tools_kbadd(r, fb, &rcs, &rccs);
      } while (fabs(fb) > errt && j < k);
    } while (k < klim);
    r += rcs + rccs;
    ir += ircs + irccs;
    errd = h*(fabs(r - 2*iback) + fabs(ir - 2*irback));
    h *= 0.5;
    m *= 2;
    klim = 2*klim - noff;
  }
  r += rcs + rccs;
  r *= h*ba;
  err = (errd > errh) ? -errd*(m*fabs(ba)) : err*aw[2]*(m*fabs(ba));
  lua_pushnumber(L, (err < 0) ? AGN_NAN : r);  /* 6.7.9 change */
  lua_pushnumber(L, err);
  return 2;
}


#ifndef __ARMCPU
static void intdeiini (int lenaw, long double tiny, long double eps, long double *aw) {
  /* ---- adjustable parameter ---- */
  long double efs = 0.1, hoff = 11.0;
  /* ------------------------------ */
  int noff, nk, k, j;
  long double tinyln, epsln, h0, ehp, ehm, h, t, ep, em, xp, xm, wp, wm;
  tinyln = -tools_logl(tiny);
  epsln = 1.0L - tools_logl(efs*eps);
  h0 = hoff/epsln;
  ehp = tools_expl(h0);
  ehm = 1.0L/ehp;
  aw[2] = eps;
  aw[3] = tools_expl(-ehm*epsln);
  aw[4] = tools_sqrtl(efs*eps);
  noff = 5;
  aw[noff] = 1;
  aw[noff + 1] = 4.0L*h0;
  aw[noff + 2] = 2.0L*M_PIO4ld*h0;
  h = 2;
  nk = 0;
  k = noff + 6;
  do {
    t = h*0.5L;
    do {
      em = tools_expl(h0*t);
      ep = M_PIO4ld*em;
      em = M_PIO4ld/em;
      j = k;
      do {
        if (j >= NSLOTS - 5) break;  /* 2.34.10 invalid array access patch */
        xp = tools_expl(ep - em);
        xm = 1.0L/xp;
        wp = xp*((ep + em)*h0);
        wm = xm*((ep + em)*h0);
        aw[j] = xm;
        aw[j + 1] = xp;
        aw[j + 2] = xm*(4.0L*h0);
        aw[j + 3] = xp*(4.0L*h0);
        aw[j + 4] = wm;
        aw[j + 5] = wp;
        ep *= ehp;
        em *= ehm;
        j += 6;
      } while (ep < tinyln && j <= lenaw - 6);
      t += h;
      k += nk;
    } while (t < 1);
    h *= 0.5L;
    if (nk == 0) {
      if (j > lenaw - 12) j -= 6;
      nk = j - noff;
      k += nk;
      aw[1] = nk;
    }
  } while (2*k - noff - 6 <= lenaw);
  aw[0] = k - 6;
}

static long double lastIntdeiEps = INTEG_EPS;
static long double intdei_w[NSLOTS] = { 0 };
#endif

static int calc_intdei64 (lua_State *L);

static int calc_intdei (lua_State *L) {
#if defined(__ARMCPU)
  return calc_intdei64(L);
#else
  int noff, lenawm, nk, k, j, jtmp, jm, m, klim, nargs, samples, adjust, iters, use64;
  long double err, omega, length, eps, delta, r, a, epsh, ir, fp, fm, errt, errh, errd, h, iback, irback;
  /* "If there are fewer initializers in a brace-enclosed list than there are elements or members of an aggregate,
      or fewer characters in a string literal used to initialize an array of known size than there are elements
      in the array, the remainder of the aggregate shall be initialized implicitly the same as objects that have
      static storage duration." https://stackoverflow.com/questions/5636070/zero-an-array-in-c-code
      2.34.10 invalid array access patch */
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)iters; (void)use64;
  long double aw[NSLOTS] = { 0 };
  fp = fm = 0;  /* to prevent compiler warnings on Mac OS X and other UNIX flavours, 3.7.6/3.13.1 */
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  eps = INTEG_EPS;  /* 3.12.0 change for `eps` option */
  aux_fcheckoptionslong(L, 3, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.intdei");  /* 3.11.5 change */
  if (eps == lastIntdeiEps)  /* x % boost, 3.11.5 */
    for (j=0; j < NSLOTS; j++) aw[j] = intdei_w[j];
  else {
    lastIntdeiEps = eps;
    intdeiini(NSLOTS, INTEG_TINY, eps, aw);
  }
  noff = 5;
  lenawm = (int)(aw[0] + 0.5);
  nk = (int)(aw[1] + 0.5);
  epsh = aw[4];
  r = agnL_fncall(L, 1, a + aw[noff], 3, nargs);  /* 3.11.5/3.12.0 change */
  ir = r*aw[noff + 1];
  r *= aw[noff + 2];
  err = fabsl(r);
  k = nk + noff;
  j = noff;
  do {
    j += 6;
    if (j >= NSLOTS - 5) break;  /* 2.34.10 invalid array access patch */
    fm = agnL_fncall(L, 1, a + aw[j], 3, nargs);  /* 3.11.5 change for multivariate support */
    fp = agnL_fncall(L, 1, a + aw[j + 1], 3, nargs);  /* 3.11.5 change for multivariate support */
    ir = ir + fmal(fm, aw[j + 2], fp*aw[j + 3]);
    fm *= aw[j + 4];
    fp *= aw[j + 5];
    r += fm + fp;
    err += fabsl(fm) + fabsl(fp);
  } while (aw[j] > epsh && j < k);
  errt = err*aw[3];
  errh = err*epsh;
  errd = 1.0L + 2.0L*errh;
  jtmp = j;
  while (fabsl(fm) > errt && j < k) {
    j += 6;
    if (j >= NSLOTS - 4) break;  /* 2.34.10 invalid array access patch */
    fm = agnL_fncall(L, 1, a + aw[j], 3, nargs);  /* 3.11.5 change for multivariate support */
    ir = fmal(fm, aw[j + 2], ir);
    fm *= aw[j + 4];
    r += fm;
  }
  jm = j;
  j = jtmp;
  while (fabsl(fp) > errt && j < k) {
    j += 6;
    if (j >= NSLOTS - 5) break;  /* 2.34.10 invalid array access patch */
    fp = agnL_fncall(L, 1, a + aw[j + 1], 3, nargs);  /* 3.11.5 change for multivariate support */
    ir = fmal(fp, aw[j + 3], ir);
    fp *= aw[j + 5];
    r += fp;
  }
  if (j < jm) jm = j;
  jm -= noff + 6;
  h = 1.0L;
  m = 1;
  klim = k + nk;
  while (errd > errh && klim <= lenawm) {
    iback = r;
    irback = ir;
    do {
      jtmp = k + jm;
      for (j=k + 6; j <= jtmp; j += 6) {
        if (j >= NSLOTS - 5) break;  /* 2.34.10 invalid array access patch */
        fm = agnL_fncall(L, 1, a + aw[j], 3, nargs);  /* 3.11.5 change for multivariate support */
        fp = agnL_fncall(L, 1, a + aw[j + 1], 3, nargs);  /* 3.11.5 change for multivariate support */
        ir = fmal(fm, aw[j + 2], fmal(fp, aw[j + 3], ir));
        r += fmal(fm, aw[j + 4], fp*aw[j + 5]);
      }
      k += nk;
      j = jtmp;
      do {
        j += 6;
        if (j >= NSLOTS - 4) break;  /* 2.34.10 invalid array access patch */
        fm = agnL_fncall(L, 1, a + aw[j], 3, nargs);  /* 3.11.5 change for multivariate support */
        ir = fmal(fm, aw[j + 2], ir);
        fm *= aw[j + 4];
        r += fm;
      } while (fabsl(fm) > errt && j < k);
      j = jtmp;
      do {
        j += 6;
        if (j >= NSLOTS - 5) break;  /* 2.34.10 invalid array access patch */
        fp = agnL_fncall(L, 1, a + aw[j + 1], 3, nargs);  /* 3.11.5 change for multivariate support */
        ir = fmal(fp, aw[j + 3], ir);
        fp *= aw[j + 5];
        r += fp;
      } while (fabsl(fp) > errt && j < k);
    } while (k < klim);
    errd = h*(fabsl(r - 2*iback) + fabsl(ir - 2*irback));
    h *= 0.5L;
    m *= 2;
    klim = 2*klim - noff;
  }
  r *= h;
  err = (errd > errh) ? -errd*m : aw[2]*m;
  lua_pushnumber(L, (err < 0) ? AGN_NAN : r);  /* 6.7.9 change */
  lua_pushnumber(L, err);
  return 2;
#endif
}

static void intdeiini64 (int lenaw, double tiny, double eps, double *aw) {
  /* ---- adjustable parameter ---- */
  double efs = 0.1, hoff = 11.0;
  /* ------------------------------ */
  int noff, nk, k, j;
  double tinyln, epsln, h0, ehp, ehm, h, t, ep, em, xp, xm, wp, wm;
  tinyln = -sun_log(tiny);
  epsln = 1 - sun_log(efs*eps);
  h0 = hoff/epsln;
  ehp = sun_exp(h0);
  ehm = 1/ehp;
  aw[2] = eps;
  aw[3] = sun_exp(-ehm*epsln);
  aw[4] = sqrt(efs*eps);
  noff = 5;
  aw[noff] = 1;
  aw[noff + 1] = 4*h0;
  aw[noff + 2] = 2*M_PIO4*h0;
  h = 2;
  nk = 0;
  k = noff + 6;
  do {
    t = h*0.5;
    do {
      em = sun_exp(h0*t);
      ep = M_PIO4*em;
      em = M_PIO4/em;
      j = k;
      do {
        if (j >= NSLOTS - 5) break;
        xp = sun_exp(ep - em);
        xm = 1/xp;
        wp = xp*((ep + em)*h0);
        wm = xm*((ep + em)*h0);
        aw[j] = xm;
        aw[j + 1] = xp;
        aw[j + 2] = xm*(4*h0);
        aw[j + 3] = xp*(4*h0);
        aw[j + 4] = wm;
        aw[j + 5] = wp;
        ep *= ehp;
        em *= ehm;
        j += 6;
      } while (ep < tinyln && j <= lenaw - 6);
      t += h;
      k += nk;
    } while (t < 1);
    h *= 0.5;
    if (nk == 0) {
      if (j > lenaw - 12) j -= 6;
      nk = j - noff;
      k += nk;
      aw[1] = nk;
    }
  } while (2*k - noff - 6 <= lenaw);
  aw[0] = k - 6;
}

static double lastIntdeiEps64 = (double)INTEG_EPS64;
static double intdei_w64[NSLOTS] = { 0 };

static int calc_intdei64 (lua_State *L) {
  int noff, lenawm, nk, k, j, jtmp, jm, m, klim, nargs, samples, adjust, iters;
  double err, omega, length, eps, delta, r, a, epsh, ir, fp, fm, errt, errh, errd, h, iback, irback;
  double rcs, rccs, ircs, irccs, errcs, errccs;
  /* "If there are fewer initializers in a brace-enclosed list than there are elements or members of an aggregate,
      or fewer characters in a string literal used to initialize an array of known size than there are elements
      in the array, the remainder of the aggregate shall be initialized implicitly the same as objects that have
      static storage duration." https://stackoverflow.com/questions/5636070/zero-an-array-in-c-code
      2.34.10 invalid array access patch */
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)iters;
  double aw[NSLOTS] = { 0 };
  fp = fm = rcs = rccs = ircs = irccs = errcs = errccs = 0.0;  /* to prevent compiler warnings on Mac OS X and other UNIX flavours, 5.2.0 fix */
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  eps = INTEG_EPS64;
  aux_fcheckoptions(L, 3, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, "calc.intdei64");
  if (eps == lastIntdeiEps64)
    for (j=0; j < NSLOTS; j++) aw[j] = intdei_w64[j];
  else {
    lastIntdeiEps64 = eps;
    intdeiini64(NSLOTS, INTEG_TINY64, eps, aw);
  }
  noff = 5;
  lenawm = (int)(aw[0] + 0.5);
  nk = (int)(aw[1] + 0.5);
  epsh = aw[4];
  r = agnL_fncall(L, 1, a + aw[noff], 3, nargs);
  ir = r*aw[noff + 1];
  r *= aw[noff + 2];
  err = fabs(r);
  k = nk + noff;
  j = noff;
  do {
    j += 6;
    if (j >= NSLOTS - 5) break;
    fm = agnL_fncall(L, 1, a + aw[j], 3, nargs);
    fp = agnL_fncall(L, 1, a + aw[j + 1], 3, nargs);
    ir = tools_kbadd(ir, fm*aw[j + 2] + fp*aw[j + 3], &ircs, &irccs);
    fm *= aw[j + 4];
    fp *= aw[j + 5];
    r = tools_kbadd(r, fm + fp, &rcs, &rccs);
    err = tools_kbadd(err, fabs(fm) + fabs(fp), &errcs, &errccs);
  } while (aw[j] > epsh && j < k);
  err += errcs + errccs;
  errt = err*aw[3];
  errh = err*epsh;
  errd = 1 + 2*errh;
  jtmp = j;
  while (fabs(fm) > errt && j < k) {
    j += 6;
    if (j >= NSLOTS - 4) break;
    fm = agnL_fncall(L, 1, a + aw[j], 3, nargs);
    ir = tools_kbadd(ir, fm*aw[j + 2], &ircs, &irccs);
    fm *= aw[j + 4];
    r = tools_kbadd(r, fm, &rcs, &rccs);
  }
  jm = j;
  j = jtmp;
  while (fabs(fp) > errt && j < k) {
    j += 6;
    if (j >= NSLOTS - 5) break;
    fp = agnL_fncall(L, 1, a + aw[j + 1], 3, nargs);
    ir = tools_kbadd(ir, fp*aw[j + 3], &ircs, &irccs);
    fp *= aw[j + 5];
    r = tools_kbadd(r, fp, &rcs, &rccs);
  }
  if (j < jm) jm = j;
  jm -= noff + 6;
  h = 1;
  m = 1;
  klim = k + nk;
  r += rcs + rccs;
  ir += ircs + irccs;
  while (errd > errh && klim <= lenawm) {
    iback = r;
    irback = ir;
    do {
      jtmp = k + jm;
      for (j=k + 6; j <= jtmp; j += 6) {
        if (j >= NSLOTS - 5) break;
        fm = agnL_fncall(L, 1, a + aw[j], 3, nargs);
        fp = agnL_fncall(L, 1, a + aw[j + 1], 3, nargs);
        ir = tools_kbadd(ir, fm*aw[j + 2] + fp*aw[j + 3], &ircs, &irccs);
        r = tools_kbadd(r, fm*aw[j + 4] + fp*aw[j + 5], &rcs, &rccs);
      }
      k += nk;
      j = jtmp;
      do {
        j += 6;
        if (j >= NSLOTS - 4) break;
        fm = agnL_fncall(L, 1, a + aw[j], 3, nargs);
        ir = tools_kbadd(ir, fm*aw[j + 2], &ircs, &irccs);
        fm *= aw[j + 4];
        r = tools_kbadd(r, fm, &rcs, &rccs);
      } while (fabs(fm) > errt && j < k);
      j = jtmp;
      do {
        j += 6;
        if (j >= NSLOTS - 5) break;
        fp = agnL_fncall(L, 1, a + aw[j + 1], 3, nargs);
        ir = tools_kbadd(ir, fp*aw[j + 3], &ircs, &irccs);
        fp *= aw[j + 5];
        r = tools_kbadd(r, fp, &rcs, &rccs);
      } while (fabs(fp) > errt && j < k);
    } while (k < klim);
    r += rcs + rccs;
    ir += ircs + irccs;
    errd = h*(fabs(r- 2*iback) + fabs(ir - 2*irback));
    h *= 0.5;
    m *= 2;
    klim = 2*klim - noff;
  }
  r += rcs + rccs;
  r *= h;
  err = (errd > errh) ? -errd*m : aw[2]*m;
  lua_pushnumber(L, (err < 0) ? AGN_NAN : r);  /* 6.7.9 change */
  lua_pushnumber(L, err);
  return 2;
}


/* Taken from INTDE1.C, 4.2.4 */
void intdeieasy (lua_State *L, int nargs, long double a, long double eps, long double *i, long double *err) {
  /* ---- adjustable parameter ---- */
  int mmax = 256;
  long double efs = 0.1, hoff = 11.0;
  /* ------------------------------ */
  int m;
  long double epsln, epsh, h0, ehp, ehm, epst, ir, h, iback, irback, t, ep, em, xp, xm, fp, fm, errt, errh, errd;
  epsln = 1.0L - tools_logl(efs*eps);
  epsh = sqrtl(efs*eps);
  h0 = hoff/epsln;
  ehp = tools_expl(h0);
  ehm = 1.0L/ehp;
  epst = tools_expl(-ehm*epsln);
  ir = agnL_fncall(L, 1, a + 1, 3, nargs);  /* ir = (*f)(a + 1);  */
  *i = ir*(2.0L*M_PIO4ld);
  *err = fabsl(*i)*epst;
  h = 2.0L*h0;
  m = 1;
  errh = 0.0L;
  do {
    iback = *i;
    irback = ir;
    t = h*0.5L;
    do {
      em = expl(t);
      ep = M_PIO4ld*em;
      em = M_PIO4ld/em;
      do {
        xp = tools_expl(ep - em);
        xm = 1.0L/xp;
        fp = agnL_fncall(L, 1, a + xp, 3, nargs)*xp;  /* (*f)(a + xp)*xp; */
        fm = agnL_fncall(L, 1, a + xm, 3, nargs)*xm;  /* (*f)(a + xm)*xm; */
        ir += fp + fm;
        *i = fmal(fp + fm, ep + em, *i);
        errt = (fabsl(fp) + fabsl(fm))*(ep + em);
        if (m == 1) *err += errt*epst;
        ep *= ehp;
        em *= ehm;
      } while (errt > *err || xm > epsh);
      t += h;
    } while (t < h0);
    if (m == 1) {
      errh = (*err/epst)*epsh*h0;
      errd = 1.0L + 2.0L*errh;
    } else {
      errd = h*(fabsl(*i - 2.0L*iback) + 4.0L*fabsl(ir - 2.0L*irback));
    }
    h *= 0.5L;
    m *= 2;
  } while (errd > errh && m < mmax);
  *i *= h;
  *err = (errd > errh) ? -errd*m : errh*epsh*m/(2.0L*efs);
}

static int calc_intdei2 (lua_State *L) {  /* 4.2.4 UNDOC, not better than calc.intdei */
  int nargs, samples, adjust, iters, use64;
  long double a, i, err, omega, length, eps, delta;
  (void)omega; (void)length; (void)samples; (void)adjust; (void)length; (void)delta; (void)iters; (void)use64;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  eps = INTEG_EPS;
  aux_fcheckoptionslong(L, 3, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.intdei2");
  intdeieasy(L, nargs, a, eps, &i, &err);
  lua_pushnumber(L, (lua_Number)i);
  lua_pushnumber(L, (lua_Number)err);
  return 2;
}


#define ONEoPI          (0.31830988618379067153)   /* = 1/Pi */
#define TWOoPI          (0.63661977236758134306)   /* = 2/Pi */
#define ONESIXTH        (0.16666666666666666667)
#define PI2SQUARED      (39.47841760435743447535)  /* = (2*Pi)^2 */

#define ONEoPIld        (0.318309886183790671537767526745L)  /* = 1/Pi */
#define TWOoPIld        (0.636619772367581343075535053490L)  /* = 2/Pi */

#ifndef __ARMCPU
static void intdeoini (int lenaw, long double tiny, long double eps, long double *aw) {
  /* ---- adjustable parameter ---- */
  int lmax = 5;
  long double efs = 0.1, enoff = 0.40, pqoff = 2.9, ppoff = -0.72;
  /* ------------------------------ */
  int noff0, nk0, noff, k, nk, j, q;
  long double tinyln, epsln, frq4, per2, pp, pq, ehp, ehm, h, t, ep, em, tk, xw, wg, xa;
  tinyln = -tools_logl(tiny);
  epsln = 1 - tools_logl(efs*eps);
  frq4 = TWOoPIld;
  per2 = M_PIld;
  pq = pqoff/epsln;
  pp = ppoff - tools_logl(pq*pq*frq4);
  ehp = tools_expl(2.0L*pq);
  ehm = 1.0L/ehp;
  aw[3] = lmax;
  aw[4] = eps;
  aw[5] = tools_sqrtl(efs*eps);
  noff0 = 6;
  nk0 = 1 + (int)(enoff*epsln);
  aw[1] = nk0;
  noff = 2*nk0 + noff0;
  wg = 0;
  xw = 1;
  for (k=1; k <= nk0; k++) {
    wg += xw;
    q = noff - 2*k;
    if (q >= NSLOTS - 1) break;  /* 2.34.10 invalid array access patch */
    aw[q] = wg;
    aw[q + 1] = xw;
    xw = xw*(nk0 - k)/k;
  }
  wg = per2/wg;
  for (k=noff0; k < NSLOTS - 1 && k <= noff - 2; k += 2) {  /* 2.34.10 invalid array access patch */
    aw[k] *= wg;
    aw[k + 1] *= wg;
  }
  xw = tools_expl(pp - 2.0L*PIO4);
  aw[noff] = tools_sqrtl(xw*(per2*0.5L));
  aw[noff + 1] = xw*pq;
  aw[noff + 2] = per2*0.5L;
  h = 2;
  nk = 0;
  k = noff + 3;
  do {
    t = h*0.5L;
    do {
      em = tools_expl(2.0L*pq*t);
      ep = M_PIO4ld*em;
      em = M_PIO4ld/em;
      tk = t;
      j = k;
      do {
        if (j >= NSLOTS - 2) break;  /* 2.34.10 invalid array access patch */
        xw = tools_expl(pp - ep - em);
        wg = tools_sqrtl(frq4*xw + tk*tk);
        xa = xw/(tk + wg);
        wg = (pq*xw*(ep - em) + xa)/wg;
        aw[j] = xa;
        aw[j + 1] = xw*pq;
        aw[j + 2] = wg;
        ep *= ehp;
        em *= ehm;
        tk += 1;
        j += 3;
      } while (ep < tinyln && j <= lenaw - 3);
      t += h;
      k += nk;
    } while (t < 1);
    h *= 0.5L;
    if (nk == 0) {
      if (j > lenaw - 6) j -= 3;
      nk = j - noff;
      k += nk;
      aw[2] = nk;
    }
  } while (2*k - noff - 3 <= lenaw);
  aw[0] = k - 3;
}

static long double lastIntdeoEps = INTEG_EPS;
static long double intdeo_w[NSLOTS] = { 0 };
#endif


static int calc_intdeo64 (lua_State *L);

static int calc_intdeo (lua_State *L) {
#if defined(__ARMCPU)
  return calc_intdeo64(L);
#else
  int lenawm, nk0, noff0, nk, noff, lmax, m, k, j, jm, l, nargs, samples, adjust, iters, use64;
  long double r, a, omega, length, delta, err, eps, per, perw, w02, ir, h, iback, irback, t, tk, xa, fm, fp, errh, s0, s1, s2, errd;
  /* "If there are fewer initializers in a brace-enclosed list than there are elements or members of an aggregate,
      or fewer characters in a string literal used to initialize an array of known size than there are elements
      in the array, the remainder of the aggregate shall be initialized implicitly the same as objects that have
      static storage duration." https://stackoverflow.com/questions/5636070/zero-an-array-in-c-code
      2.34.10 invalid array access patch */
  (void)samples; (void)adjust; (void)delta; (void)iters; (void)use64;
  long double aw[NSLOTS] = { 0 };
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  omega = length = 1.0L;
  eps = INTEG_EPS;  /* 3.12.0 change */
  aux_fcheckoptionslong(L, 3, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.intdeo");  /* 3.11.5 change */
  if (omega == 0)  /* 3.11.5 fix */
    luaL_error(L, "Error in " LUA_QS ": omega value must not be zero.", "calc.intdeo");
  if (eps == lastIntdeoEps)  /* x % boost, 3.11.5 */
    for (j=0; j < NSLOTS; j++) aw[j] = intdeo_w[j];
  else {
    lastIntdeoEps = eps;
    intdeoini(NSLOTS, INTEG_TINY, eps, aw);
  }
  lenawm = (int)(aw[0] + 0.5);
  nk0 = (int)(aw[1] + 0.5);
  noff0 = 6;
  nk = (int)(aw[2] + 0.5);
  noff = 2*nk0 + noff0;
  lmax = (int)(aw[3] + 0.5);
  eps = aw[4];
  per = 1.0L/fabsl(omega);
  w02 = 2.0L*aw[noff + 2];
  perw = per*w02;
  r = agnL_fncall(L, 1, a + aw[noff]*per, 3, nargs);  /* 3.11.5 change for multivariate support */
  ir = r*aw[noff + 1];
  r *= aw[noff + 2];
  err = fabsl(r);
  h = 2.0L;
  m = 1;
  k = noff;
  jm = fm = fp = errh = 0;  /* to prevent compiler warnings */
  do {
    iback = r;
    irback = ir;
    t = h*0.5L;
    do {
      if (k == noff) {
        tk = 1;
        k += nk;
        j = noff;
        do {
          j += 3;
          if (j >= NSLOTS - 2) break;  /* 2.34.10 invalid array access patch */
          xa = per*aw[j];
          fm = agnL_fncall(L, 1, a + xa, 3, nargs);  /* 3.11.5 change for multivariate support */
          fp = agnL_fncall(L, 1, fmal(perw, tk, a) + xa, 3, nargs);  /* 3.11.5 change for multivariate support */
          ir = fmal(fm + fp, aw[j + 1], ir);
          fm *= aw[j + 2];
          fp *= w02 - aw[j + 2];
          r += fm + fp;
          err += fabsl(fm) + fabsl(fp);
          tk += 1;
        } while (aw[j] > eps && j < k);
        errh = err*aw[5];
        err *= eps;
        jm = j - noff;
      } else {
        tk = t;
        for (j=k + 3; j < NSLOTS - 2 && j <= k + jm; j += 3) {  /* 2.34.10 invalid array access patch */
          xa = per*aw[j];
          fm = agnL_fncall(L, 1, a + xa, 3, nargs);  /* 3.11.5 change for multivariate support */
          fp = agnL_fncall(L, 1, fmal(perw, tk, a + xa), 3, nargs);  /* 3.11.5 change for multivariate support */
          ir = fmal(fm + fp, aw[j + 1], ir);
          fm *= aw[j + 2];
          fp *= w02 - aw[j + 2];
          r += fm + fp;
          tk += 1;
        }
        j = k + jm;
        k += nk;
      }
      while (fabs(fm) > err && j < k) {
        j += 3;
        if (j >= NSLOTS - 2) break;
        fm = agnL_fncall(L, 1, a + per*aw[j], 3, nargs);  /* 3.11.5 change for multivariate support */
        ir = fmal(fm, aw[j + 1], ir);
        fm *= aw[j + 2];
        r += fm;
      }
      fm = agnL_fncall(L, 1, a + perw*tk, 3, nargs);  /* 3.11.5 change for multivariate support */
      s2 = w02*fm;
      r += s2;
      if (fabsl(fp) > err || fabsl(s2) > err) {
        l = 0;
        for (;;) {
          l++;
          s0 = 0.0L;
          s1 = 0.0L;
          s2 = fm*aw[noff0 + 1];
          for (j=noff0 + 2; j < NSLOTS - 1 && j <= noff - 2; j += 2) {  /* 2.34.10 invalid array access patch */
            tk += 1;
            fm = agnL_fncall(L, 1, fmal(perw, tk, a), 3, nargs);  /* 3.11.5 change for multivariate support */
            s0 += fm;
            s1 = fmal(fm, aw[j], s1);
            s2 = fmal(fm, aw[j + 1], s2);
          }
          if (s2 <= err || l >= lmax) break;
          r = fmal(w02, s0, r);
        }
        r += s1;
        if (s2 > err) err = s2;
      }
      t += h;
    } while (t < 1);
    if (m == 1) {
      errd = 1.0L + 2.0L*errh;
    } else {
      errd = h*(fabsl(r - 2.0L*iback) + fabsl(ir - 2.0L*irback));
    }
    h *= 0.5L;
    m *= 2;
  } while (errd > errh && 2*k - noff <= lenawm);
  r *= h*per;
  if (errd > errh) {
    err = -errd*per;
  } else {
    err *= per*m*0.5L;
  }
  lua_pushnumber(L, (err < 0) ? AGN_NAN : r);  /* 6.7.9 change */
  lua_pushnumber(L, err);
  return 2;
#endif
}

static void intdeoini64 (int lenaw, double tiny, double eps, double *aw) {
  /* ---- adjustable parameter ---- */
  int lmax = 5;
  double efs = 0.1, enoff = 0.40, pqoff = 2.9, ppoff = -0.72;
  /* ------------------------------ */
  int noff0, nk0, noff, k, nk, j, q;
  double tinyln, epsln, frq4, per2, pp, pq, ehp, ehm, h, t, ep, em, tk, xw, wg, xa;
  tinyln = -sun_log(tiny);
  epsln = 1 - sun_log(efs*eps);
  frq4 = TWOoPI;
  per2 = PI;
  pq = pqoff/epsln;
  pp = ppoff - sun_log(pq*pq*frq4);
  ehp = sun_exp(2*pq);
  ehm = 1/ehp;
  aw[3] = lmax;
  aw[4] = eps;
  aw[5] = sqrt(efs*eps);
  noff0 = 6;
  nk0 = 1 + (int)(enoff*epsln);
  aw[1] = nk0;
  noff = 2*nk0 + noff0;
  wg = 0;
  xw = 1;
  for (k=1; k <= nk0; k++) {
    wg += xw;
    q = noff - 2*k;
    if (q >= NSLOTS - 1) break;
    aw[q] = wg;
    aw[q + 1] = xw;
    xw = xw*(nk0 - k)/k;
  }
  wg = per2/wg;
  for (k=noff0; k < NSLOTS - 1 && k <= noff - 2; k += 2) {
    aw[k] *= wg;
    aw[k + 1] *= wg;
  }
  xw = sun_exp(pp - 2*PIO4);
  aw[noff] = sqrt(xw*(per2*0.5));
  aw[noff + 1] = xw*pq;
  aw[noff + 2] = per2*0.5;
  h = 2;
  nk = 0;
  k = noff + 3;
  do {
    t = h*0.5;
    do {
      em = sun_exp(2*pq*t);
      ep = M_PIO4*em;
      em = M_PIO4/em;
      tk = t;
      j = k;
      do {
        if (j >= NSLOTS - 2) break;
        xw = sun_exp(pp - ep - em);
        wg = sqrt(fma(frq4, xw, tk*tk));
        xa = xw/(tk + wg);
        wg = fma(pq*xw, ep - em, xa)/wg;
        aw[j] = xa;
        aw[j + 1] = xw*pq;
        aw[j + 2] = wg;
        ep *= ehp;
        em *= ehm;
        tk += 1;
        j += 3;
      } while (ep < tinyln && j <= lenaw - 3);
      t += h;
      k += nk;
    } while (t < 1);
    h *= 0.5;
    if (nk == 0) {
      if (j > lenaw - 6) j -= 3;
      nk = j - noff;
      k += nk;
      aw[2] = nk;
    }
  } while (2*k - noff - 3 <= lenaw);
  aw[0] = k - 3;
}


static double lastIntdeoEps64 = INTEG_EPS64;
static double intdeo_w64[NSLOTS] = { 0 };

static int calc_intdeo64 (lua_State *L) {
  int lenawm, nk0, noff0, nk, noff, lmax, m, k, j, jm, l, nargs, samples, adjust, iters;
  double r, a, omega, length, delta, err, eps, per, perw, w02, ir, h, iback, irback, t, tk, xa, fm, fp, errh, s0, s1, s2, errd;
  double rcs, rccs, ircs, irccs, errcs, errccs;
  /* "If there are fewer initializers in a brace-enclosed list than there are elements or members of an aggregate,
      or fewer characters in a string literal used to initialize an array of known size than there are elements
      in the array, the remainder of the aggregate shall be initialized implicitly the same as objects that have
      static storage duration." https://stackoverflow.com/questions/5636070/zero-an-array-in-c-code
      2.34.10 invalid array access patch */
  (void)samples; (void)adjust; (void)delta; (void)iters;
  double aw[NSLOTS] = { 0 };
  rcs = rccs = ircs = irccs = errcs = errccs = 0.0;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  omega = length = 1.0L;
  eps = INTEG_EPS;  /* 3.12.0 change */
  aux_fcheckoptions(L, 3, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, "calc.intdeo64");
  if (omega == 0)  /* 3.11.5 fix */
    luaL_error(L, "Error in " LUA_QS ": omega value must not be zero.", "calc.intdeo64");
  if (eps == lastIntdeoEps64)
    for (j=0; j < NSLOTS; j++) aw[j] = intdeo_w64[j];
  else {
    lastIntdeoEps64 = eps;
    intdeoini64(NSLOTS, INTEG_TINY64, eps, aw);
  }
  lenawm = (int)(aw[0] + 0.5);
  nk0 = (int)(aw[1] + 0.5);
  noff0 = 6;
  nk = (int)(aw[2] + 0.5);
  noff = 2*nk0 + noff0;
  lmax = (int)(aw[3] + 0.5);
  eps = aw[4];
  per = 1/fabs(omega);
  w02 = 2*aw[noff + 2];
  perw = per*w02;
  r = agnL_fncall(L, 1, a + aw[noff]*per, 3, nargs);
  ir = r*aw[noff + 1];
  r *= aw[noff + 2];
  err = fabs(r);
  h = 2;
  m = 1;
  k = noff;
  jm = fm = fp = errh = 0;  /* to prevent compiler warnings */
  do {
    iback = r;
    irback = ir;
    t = h*0.5;
    do {
      if (k == noff) {
        tk = 1;
        k += nk;
        j = noff;
        do {
          j += 3;
          if (j >= NSLOTS - 2) break;
          xa = per*aw[j];
          fm = agnL_fncall(L, 1, a + xa, 3, nargs);
          fp = agnL_fncall(L, 1, a + xa + perw*tk, 3, nargs);
          ir = tools_kbadd(ir, (fm + fp)*aw[j + 1], &ircs, &irccs);
          fm *= aw[j + 2];
          fp *= w02 - aw[j + 2];
          r = tools_kbadd(r, fm + fp, &rcs, &rccs);
          err = tools_kbadd(err, fabs(fm) + fabs(fp), &errcs, &errccs);
          tk += 1;
        } while (aw[j] > eps && j < k);
        err += errcs + errccs;
        errh = err*aw[5];
        err *= eps;
        jm = j - noff;
      } else {
        tk = t;
        for (j=k + 3; j < NSLOTS - 2 && j <= k + jm; j += 3) {
          xa = per*aw[j];
          fm = agnL_fncall(L, 1, a + xa, 3, nargs);
          fp = agnL_fncall(L, 1, a + xa + perw*tk, 3, nargs);
          ir = tools_kbadd(ir, (fm + fp)*aw[j + 1], &ircs, &irccs);
          fm *= aw[j + 2];
          fp *= w02 - aw[j + 2];
          r = tools_kbadd(r, fm + fp, &rcs, &rccs);
          tk += 1;
        }
        j = k + jm;
        k += nk;
      }
      while (fabs(fm) > err && j < k) {
        j += 3;
        if (j >= NSLOTS - 2) break;
        fm = agnL_fncall(L, 1, a + per*aw[j], 3, nargs);
        ir = tools_kbadd(ir, fm*aw[j + 1], &ircs, &irccs);
        fm *= aw[j + 2];
        r = tools_kbadd(r, fm, &rcs, &rccs);
      }
      fm = agnL_fncall(L, 1, a + perw*tk, 3, nargs);
      s2 = w02*fm;
      r += s2;
      if (fabs(fp) > err || fabs(s2) > err) {
        double s0cs, s1cs, s2cs, s0ccs, s1ccs, s2ccs;
        l = 0;
        for (;;) {
          l++;
          s0cs = s1cs = s2cs = s0ccs = s1ccs = s2ccs = 0.0;
          s0 = 0.0;
          s1 = 0.0;
          s2 = fm*aw[noff0 + 1];
          for (j=noff0 + 2; j < NSLOTS - 1 && j <= noff - 2; j += 2) {
            tk += 1;
            fm = agnL_fncall(L, 1, a + perw*tk, 3, nargs);
            s0 = tools_kbadd(s0, fm, &s0cs, &s0ccs);
            s1 = tools_kbadd(s1, fm*aw[j], &s1cs, &s1ccs);
            s2 = tools_kbadd(s2, fm*aw[j + 1], &s2cs, &s2ccs);
          }
          if (s2 + s2cs + s2ccs <= err || l >= lmax) break;
          s0 += s0cs + s0ccs;
          r = tools_kbadd(r, w02*s0, &rcs, &rccs);
        }
        s1 += s1cs + s1ccs;
        s2 += s2cs + s2ccs;
        r = tools_kbadd(r, s1, &rcs, &rccs);
        if (s2 > err) err = s2;
      }
      t += h;
    } while (t < 1);
    if (m == 1) {
      errd = 1 + 2*errh;
    } else {
      errd = h*(fabs(r + rcs + rccs - 2*iback) + fabs(ir + ircs + irccs - 2*irback));
    }
    h *= 0.5;
    m *= 2;
  } while (errd > errh && 2*k - noff <= lenawm);
  r += rcs + rccs;
  r *= h*per;
  if (errd > errh) {
    err = -errd*per;
  } else {
    err *= per*m*0.5;
  }
  lua_pushnumber(L, (err < 0) ? AGN_NAN : r);  /* 6.7.9 change */
  lua_pushnumber(L, err);
  return 2;
}


/*
Copyright(C) 1996 Takuya OOURA (email: ooura@mmm.t.u-tokyo.ac.jp).
You may use, copy, modify this code for any purpose and
without fee. You may distribute this ORIGINAL package.

Written by Takuya Ooura in C and available at http://www.kurims.kyoto-u.ac.jp/~ooura/intcc.html.

Clenshaw-Curtis-Quadrature
Numerical Automatic Integrator, fast version
    method    : Chebyshev Series Expansion
    dimension : one
    table     : use
function
    intcc  : integrator of f(x) over [a,b].
necessary package
    fft2f.c  : FFT package

intcc
    [description]
        I = integral of f(x) over [a,b]
    [declaration]
        void intccini(int lenw, double *w);
        void intcc(double (*f)(double), double a, double b, double eps,
            int lenw, double *w, double *i, double *err);
    [usage]
        intccini(lenw, w);  // initialization of w
        ...
        intcc(f, a, b, eps, lenw, w, &i, &err);
    [parameters]
        lenw      : (length of w[]) - 1 (int)
        w         : work area and weights of the quadrature
                    formula, w[0...lenw] (double *)
        f         : integrand f(x) (double (*f)(double))
        a         : lower limit of integration (double)
        b         : upper limit of integration (double)
        eps       : relative error requested (double)
        i         : approximation to the integral (double *)
        err       : estimate of the absolute error (double *)
    [remarks]
        initial parameters
            lenw >= 14 and
            lenw > (maximum number of f(x) evaluations) * 3 / 2
            example :
                lenc = 3200;
        function
            f(x) needs to be analytic over [a,b].
        relative error
            eps is relative error requested excluding
            cancellation of significant digits.
            i.e. eps means : (absolute error) /
                             (integral_a^b |f(x)| dx).
            eps does not mean : (absolute error) / I.
        error message
            err >= 0 : normal termination.
            err < 0  : abnormal termination (n > nmax).
                       i.e. convergent error is detected :
                           1. f(x) or (d/dx)^n f(x) has
                              discontinuous points or sharp
                              peaks over [a,b].
                              you must use other routine.
                           2. relative error of f(x) is
                              greater than eps.
                           3. f(x) has oscillatory factor
                              and frequency of the oscillation
                              is very high.
*/
#ifndef __ARMCPU
static long double intcc_w[NSLOTS + 1] = { 0 };

static FORCE_INLINE void bitrv (int n, long double *a) {
  int j, k, l, m, m2, n1;
  long double xr;
  if (n > 2) {
    m = n >> 2;
    m2 = m << 1;
    n1 = n - 1;
    k = 0;
    for (j=0; j <= m2 - 2; j += 2) {
      if (j < k) {
        xr = a[j];
        a[j] = a[k];
        a[k] = xr;
      } else if (j > k) {
        xr = a[n1 - j];
        a[n1 - j] = a[n1 - k];
        a[n1 - k] = xr;
      }
      xr = a[j + 1];
      a[j + 1] = a[m2 + k];
      a[m2 + k] = xr;
      l = m;
      while (k >= l) {
        k -= l;
        l >>= 1;
      }
      k += l;
    }
  }
}

static FORCE_INLINE void bitrv2 (int n, long double *a) {
  int j, j1, k, k1, l, m, m2, n2;
  long double xr, xi;
  m = n >> 2;
  m2 = m << 1;
  n2 = n - 2;
  k = 0;
  for (j=0; j <= m2 - 4; j += 4) {
    if (j < k) {
      xr = a[j];
      xi = a[j + 1];
      a[j] = a[k];
      a[j + 1] = a[k + 1];
      a[k] = xr;
      a[k + 1] = xi;
    } else if (j > k) {
      j1 = n2 - j;
      k1 = n2 - k;
      xr = a[j1];
      xi = a[j1 + 1];
      a[j1] = a[k1];
      a[j1 + 1] = a[k1 + 1];
      a[k1] = xr;
      a[k1 + 1] = xi;
    }
    k1 = m2 + k;
    xr = a[j + 2];
    xi = a[j + 3];
    a[j + 2] = a[k1];
    a[j + 3] = a[k1 + 1];
    a[k1] = xr;
    a[k1 + 1] = xi;
    l = m;
    while (k >= l) {
      k -= l;
      l >>= 1;
    }
    k += l;
  }
}

static FORCE_INLINE void cdft (int n, long double wr, long double wi, long double *a) {
  void bitrv2(int n, long double *a);
  int i, j, k, l, m;
  long double wkr, wki, wdr, wdi, ss, xr, xi;
  m = n;
  while (m > 4) {
    l = m >> 1;
    wkr = 1;
    wki = 0;
    wdr = 1 - 2.0L*wi*wi;
    wdi = 2.0L*wi*wr;
    ss = 2.0L*wdi;
    wr = wdr;
    wi = wdi;
    for (j=0; j <= n - m; j += m) {
      i = j + l;
      xr = a[j] - a[i];
      xi = a[j + 1] - a[i + 1];
      a[j] += a[i];
      a[j + 1] += a[i + 1];
      a[i] = xr;
      a[i + 1] = xi;
      xr = a[j + 2] - a[i + 2];
      xi = a[j + 3] - a[i + 3];
      a[j + 2] += a[i + 2];
      a[j + 3] += a[i + 3];
      a[i + 2] = fmal(wdr, xr, -wdi*xi);
      a[i + 3] = fmal(wdr, xi, wdi*xr);
    }
    for (k=4; k <= l - 4; k += 4) {
      wkr -= ss*wdi;
      wki += ss*wdr;
      wdr -= ss*wki;
      wdi += ss*wkr;
      for (j=k; j <= n - m + k; j += m) {
        i = j + l;
        xr = a[j] - a[i];
        xi = a[j + 1] - a[i + 1];
        a[j] += a[i];
        a[j + 1] += a[i + 1];
        a[i] = fmal(wkr, xr, -wki*xi);
        a[i + 1] = fmal(wkr, xi, wki*xr);
        xr = a[j + 2] - a[i + 2];
        xi = a[j + 3] - a[i + 3];
        a[j + 2] += a[i + 2];
        a[j + 3] += a[i + 3];
        a[i + 2] = fmal(wdr, xr, -wdi*xi);
        a[i + 3] = fmal(wdr, xi, wdi*xr);
      }
    }
    m = l;
  }
  if (m > 2) {
    for (j=0; j <= n - 4; j += 4) {
      xr = a[j] - a[j + 2];
      xi = a[j + 1] - a[j + 3];
      a[j] += a[j + 2];
      a[j + 1] += a[j + 3];
      a[j + 2] = xr;
      a[j + 3] = xi;
    }
  }
  if (n > 4) bitrv2(n, a);
}

static FORCE_INLINE void rdft (int n, long double wr, long double wi, long double *a) {
  void cdft(int n, long double wr, long double wi, long double *a);
  int j, k;
  long double wkr, wki, wdr, wdi, ss, xr, xi, yr, yi;
  if (n > 4) {
    wkr = 0.0L;
    wki = 0.0L;
    wdr = wi*wi;
    wdi = wi*wr;
    ss = 4.0L*wdi;
    wr = 1.0L - 2.0L*wdr;
    wi = 2.0L*wdi;
    if (wi >= 0.0L) {
      cdft(n, wr, wi, a);
      xi = a[0] - a[1];
      a[0] += a[1];
      a[1] = xi;
    }
    for (k=(n >> 1) - 4; k >= 4; k -= 4) {
      j = n - k;
      xr = a[k + 2] - a[j - 2];
      xi = a[k + 3] + a[j - 1];
      yr = fmal(wdr, xr, -wdi*xi);
      yi = fmal(wdr, xi, wdi*xr);
      a[k + 2] -= yr;
      a[k + 3] -= yi;
      a[j - 2] += yr;
      a[j - 1] -= yi;
      wkr = fmal(ss, wdi, wkr);
      wki = fmal(ss, 0.5L - wdr, wki);
      xr = a[k] - a[j];
      xi = a[k + 1] + a[j + 1];
      yr = fmal(wkr, xr, -wki*xi);
      yi = fmal(wkr, xi, wki*xr);
      a[k] -= yr;
      a[k + 1] -= yi;
      a[j] += yr;
      a[j + 1] -= yi;
      wdr = fmal(ss, wki, wdr);
      wdi = fmal(ss, 0.5L - wkr, wdi);
    }
    j = n - 2;
    xr = a[2] - a[j];
    xi = a[3] + a[j + 1];
    yr = fmal(wdr, xr, -wdi*xi);
    yi = fmal(wdr, xi, wdi*xr);
    a[2] -= yr;
    a[3] -= yi;
    a[j] += yr;
    a[j + 1] -= yi;
    if (wi < 0.0L) {
      a[1] = 0.5L*(a[0] - a[1]);
      a[0] -= a[1];
      cdft(n, wr, wi, a);
    }
  } else {
    if (wi < 0.0L) {
      a[1] = 0.5L*(a[0] - a[1]);
      a[0] -= a[1];
    }
    if (n > 2) {
      xr = a[0] - a[2];
      xi = a[1] - a[3];
      a[0] += a[2];
      a[1] += a[3];
      a[2] = xr;
      a[3] = xi;
    }
    if (wi >= 0.0L) {
      xi = a[0] - a[1];
      a[0] += a[1];
      a[1] = xi;
    }
  }
}

static FORCE_INLINE void ddct (int n, long double wr, long double wi, long double *a) {
  void rdft(int n, long double wr, long double wi, long double *a);
  int j, k, m;
  long double wkr, wki, wdr, wdi, ss, xr;
  if (n > 2) {
    wkr = 0.5L;
    wki = 0.5L;
    wdr = 0.5L*(wr - wi);
    wdi = 0.5L*(wr + wi);
    ss = 2.0L*wi;
    if (wi < 0.0L) {
      xr = a[n - 1];
      for (k=n - 2; k >= 2; k -= 2) {
        a[k + 1] = a[k] - a[k - 1];
        a[k] += a[k - 1];
      }
      a[1] = 2.0L*xr;
      a[0] *= 2.0L;
      rdft(n, 1 - ss*wi, ss*wr, a);
      xr = wdr;
      wdr = wdi;
      wdi = xr;
      ss = -ss;
    }
    m = n >> 1;
    for (k=1; k <= m - 3; k += 2) {
      j = n - k;
      xr = fmal(wdi, a[k], -wdr*a[j]);
      a[k] = fmal(wdr, a[k], wdi*a[j]);
      a[j] = xr;
      wkr = fmal(-ss, wdi, wkr);
      wki = fmal(ss, wdr, wki);
      xr = fmal(wki, a[k + 1], -wkr*a[j - 1]);
      a[k + 1] = fmal(wkr, a[k + 1], wki*a[j - 1]);
      a[j - 1] = xr;
      wdr = fmal(-ss, wki, wdr);
      wdi = fmal(ss, wkr, wdi);
    }
    k = m - 1;
    j = n - k;
    xr = fmal(wdi, a[k], -wdr*a[j]);
    a[k] = fmal(wdr, a[k], wdi*a[j]);
    a[j] = xr;
    a[m] *= fmal(ss, wdr, wki);
    if (wi >= 0.0L) {
      rdft(n, 1.0L - ss*wi, ss*wr, a);
      xr = a[1];
      for (k=2; k <= n - 2; k += 2) {
        a[k - 1] = a[k] - a[k + 1];
        a[k] += a[k + 1];
      }
      a[n - 1] = xr;
    }
  } else {
    if (wi >= 0.0L) {
      xr = 0.5L*(wr + wi)*a[1];
      a[1] = a[0] - xr;
      a[0] += xr;
    } else {
      xr = a[0] - a[1];
      a[0] += a[1];
      a[1] = 0.5L*(wr - wi)*xr;
    }
  }
}

static void dfct (int n, long double wr, long double wi, long double *a) {
  void ddct(int n, long double wr, long double wi, long double *a);
  void bitrv(int n, long double *a);
  int j, k, m, mh;
  long double xr, xi, an;
  m = n >> 1;
  for (j=0; j <= m - 1; j++) {
    k = n - j;
    xr = a[j] + a[k];
    a[j] -= a[k];
    a[k] = xr;
  }
  an = a[n];
  while (m >= 2) {
    ddct(m, wr, wi, a);
    xr = fmal(-2.0L*wi, wi, 1.0L);
    wi *= 2.0L*wr;
    wr = xr;
    bitrv(m, a);
    mh = m >> 1;
    xi = a[m];
    a[m] = a[0];
    a[0] = an - xi;
    an += xi;
    for (j=1; j <= mh - 1; j++) {
      k = m - j;
      xr = a[m + k];
      xi = a[m + j];
      a[m + j] = a[j];
      a[m + k] = a[k];
      a[j] = xr - xi;
      a[k] = xr + xi;
    }
    xr = a[mh];
    a[mh] = a[m + mh];
    a[m + mh] = xr;
    m = mh;
  }
  xi = a[1];
  a[1] = a[0];
  a[0] = an + xi;
  a[n] = an - xi;
  bitrv(n, a);
}

static void intccini (int lenw, long double *w) {
  void dfct(int, long double, long double, long double *);
  int j, k, l, m;
  long double cos2, sin1, sin2, hl;
  cos2 = 0.0L;
  sin1 = 1.0L;
  sin2 = 1.0L;
  hl = 0.5L;
  k = lenw;
  l = 2.0L;
  while (l < k - l - 1) {
    w[0] = 0.5L*hl;
    for (j=1; j <= l; j++) {
      w[j] = hl/(1.0L - 4.0L*j*j);
    }
    w[l] *= 0.5L;
    dfct(l, 0.5L*cos2, sin1, w);
    cos2 = sqrtl(2.0L + cos2);
    sin1 /= cos2;
    sin2 /= 2.0L + cos2;
    w[k] = sin2;
    w[k - 1] = w[0];
    w[k - 2] = w[l];
    k -= 3;
    m = l;
    while (m > 1) {
      m >>= 1;
      for (j=m; j <= l - m; j += (m << 1)) {
        w[k] = w[j];
        k--;
      }
    }
    hl *= 0.5L;
    l *= 2;
  }
}

static void intcc (lua_State *L, long double a, long double b, long double eps,
  int lenw, long double *w, long double *i, long double *err, int nargs) {
  int j, k, l;
  long double esf, eref, erefh, hh, ir, iback, irback, ba, ss, x, y, fx, errir;
  esf = 10;
  ba = 0.5*(b - a);
  ss = 2*w[lenw];
  x = ba*w[lenw];
  w[0] = 0.5*agnL_fncall(L, 1, a, 4, nargs);   /* 3.11.5 change for multivariate support */
  w[3] = 0.5*agnL_fncall(L, 1, b, 4, nargs);   /* ditto */
  w[2] = agnL_fncall(L, 1, a + x, 4, nargs);   /* ditto */
  w[4] = agnL_fncall(L, 1, b - x, 4, nargs);   /* ditto */
  w[1] = agnL_fncall(L, 1, a + ba, 4, nargs);  /* ditto */
  eref = 0.5*(fabsl(w[0]) + fabsl(w[1]) + fabsl(w[2]) + fabsl(w[3]) + fabsl(w[4]));
  w[0] += w[3];
  w[2] += w[4];
  ir = w[0] + w[1] + w[2];
  *i = w[0]*w[lenw - 1] + w[1]*w[lenw - 2] + w[2]*w[lenw - 3];
  erefh = eref*sqrtl(eps);
  eref *= eps;
  hh = 0.25L;
  l = 2;
  k = lenw - 5;
  do {
    iback = *i;
    irback = ir;
    x = ba*w[k + 1];
    y = 0;
    *i = w[0]*w[k];
    for (j=1; j <= l; j++) {
      x += y;
      y = fmal(ss, ba - x, y);
      fx = agnL_fncall(L, 1, a + x, 4, nargs) + agnL_fncall(L, 1, b - x, 4, nargs);  /* 3.11.5 change for multivariate support */
      ir += fx;
      *i = fmal(w[j], w[k - j], fx*w[k - j - l] + *i);
      w[j + l] = fx;
    }
    ss = 2.0L*w[k + 1];
    *err = esf*l*fabsl(*i - iback);
    hh *= 0.25L;
    errir = hh*fabsl(ir - 2*irback);
    l *= 2;
    k -= l + 2;
  } while ((*err > erefh || errir > eref) && k > 4*l);
  *i *= b - a;
  if (*err > erefh || errir > eref) {
    *err *= -fabsl(b - a);
  } else {
    *err = eref*fabsl(b - a);
  }
}
#endif

static int calc_intcc64 (lua_State *L);

static int calc_intcc (lua_State *L) {  /* 3.3.5 */
#if defined(__ARMCPU)
  return calc_intcc64(L);
#else
  int j, nargs, samples, adjust, iters, use64;
  long double a, b, omega, length, eps, delta, i, err, w[NSLOTS + 1];
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)iters; (void)use64;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  b = agnL_checknumber(L, 3);
  if (a > b)  /* 3.3.5 */
    luaL_error(L, "Error in " LUA_QS ": left boundary > right boundary.", "calc.intcc");
  else if (a == b) {  /* 4.2.4: contrary to calc.intde, intcc would return undefined, so: */
    i = 0.0;
    err = 0.0L;
  } else {
    eps = INTEG_EPS;  /* 3.12.0 change */
    aux_fcheckoptionslong(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.intcc");  /* 3.11.5 change */
    for (j=0; j < NSLOTS + 1; j++) w[j] = intcc_w[j];
    intcc(L, a, b, eps, NSLOTS, w, &i, &err, nargs);
  }
  lua_pushnumber(L, (err < 0) ? AGN_NAN : i);  /* 6.7.9 change */
  lua_pushnumber(L, err);
  return 2;
#endif
}

static double intcc_w64[NSLOTS + 1] = { 0 };

static FORCE_INLINE void bitrv64 (int n, double *a) {
  int j, k, l, m, m2, n1;
  double xr;
  if (n > 2) {
    m = n >> 2;
    m2 = m << 1;
    n1 = n - 1;
    k = 0;
    for (j=0; j <= m2 - 2; j += 2) {
      if (j < k) {
        xr = a[j];
        a[j] = a[k];
        a[k] = xr;
      } else if (j > k) {
        xr = a[n1 - j];
        a[n1 - j] = a[n1 - k];
        a[n1 - k] = xr;
      }
      xr = a[j + 1];
      a[j + 1] = a[m2 + k];
      a[m2 + k] = xr;
      l = m;
      while (k >= l) {
        k -= l;
        l >>= 1;
      }
      k += l;
    }
  }
}

static FORCE_INLINE void bitrv264 (int n, double *a) {
  int j, j1, k, k1, l, m, m2, n2;
  double xr, xi;
  m = n >> 2;
  m2 = m << 1;
  n2 = n - 2;
  k = 0;
  for (j=0; j <= m2 - 4; j += 4) {
    if (j < k) {
      xr = a[j];
      xi = a[j + 1];
      a[j] = a[k];
      a[j + 1] = a[k + 1];
      a[k] = xr;
      a[k + 1] = xi;
    } else if (j > k) {
      j1 = n2 - j;
      k1 = n2 - k;
      xr = a[j1];
      xi = a[j1 + 1];
      a[j1] = a[k1];
      a[j1 + 1] = a[k1 + 1];
      a[k1] = xr;
      a[k1 + 1] = xi;
    }
    k1 = m2 + k;
    xr = a[j + 2];
    xi = a[j + 3];
    a[j + 2] = a[k1];
    a[j + 3] = a[k1 + 1];
    a[k1] = xr;
    a[k1 + 1] = xi;
    l = m;
    while (k >= l) {
      k -= l;
      l >>= 1;
    }
    k += l;
  }
}

static FORCE_INLINE void cdft64 (int n, double wr, double wi, double *a) {
  void bitrv264(int n, double *a);
  int i, j, k, l, m;
  double wkr, wki, wdr, wdi, ss, xr, xi;
  m = n;
  while (m > 4) {
    l = m >> 1;
    wkr = 1;
    wki = 0;
    wdr = 1 - 2*wi*wi;
    wdi = 2.0*wi*wr;
    ss = 2.0*wdi;
    wr = wdr;
    wi = wdi;
    for (j=0; j <= n - m; j += m) {
      i = j + l;
      xr = a[j] - a[i];
      xi = a[j + 1] - a[i + 1];
      a[j] += a[i];
      a[j + 1] += a[i + 1];
      a[i] = xr;
      a[i + 1] = xi;
      xr = a[j + 2] - a[i + 2];
      xi = a[j + 3] - a[i + 3];
      a[j + 2] += a[i + 2];
      a[j + 3] += a[i + 3];
      a[i + 2] = fma(wdr, xr, -wdi*xi);
      a[i + 3] = fma(wdr, xi, wdi*xr);
    }
    for (k=4; k <= l - 4; k += 4) {
      wkr -= ss*wdi;
      wki += ss*wdr;
      wdr -= ss*wki;
      wdi += ss*wkr;
      for (j=k; j <= n - m + k; j += m) {
        i = j + l;
        xr = a[j] - a[i];
        xi = a[j + 1] - a[i + 1];
        a[j] += a[i];
        a[j + 1] += a[i + 1];
        a[i] = fma(wkr, xr, -wki*xi);
        a[i + 1] = fma(wkr, xi, wki*xr);
        xr = a[j + 2] - a[i + 2];
        xi = a[j + 3] - a[i + 3];
        a[j + 2] += a[i + 2];
        a[j + 3] += a[i + 3];
        a[i + 2] = fma(wdr, xr, -wdi*xi);
        a[i + 3] = fma(wdr, xi, wdi*xr);
      }
    }
    m = l;
  }
  if (m > 2) {
    for (j=0; j <= n - 4; j += 4) {
      xr = a[j] - a[j + 2];
      xi = a[j + 1] - a[j + 3];
      a[j] += a[j + 2];
      a[j + 1] += a[j + 3];
      a[j + 2] = xr;
      a[j + 3] = xi;
    }
  }
  if (n > 4) bitrv264(n, a);
}

static FORCE_INLINE void rdft64 (int n, double wr, double wi, double *a) {
  void cdft64(int n, double wr, double wi, double *a);
  int j, k;
  double wkr, wki, wdr, wdi, ss, xr, xi, yr, yi;
  if (n > 4) {
    wkr = 0.0;
    wki = 0.0;
    wdr = wi*wi;
    wdi = wi*wr;
    ss = 4.0*wdi;
    wr = 1.0 - 2.0*wdr;
    wi = 2.0*wdi;
    if (wi >= 0.0) {
      cdft64(n, wr, wi, a);
      xi = a[0] - a[1];
      a[0] += a[1];
      a[1] = xi;
    }
    for (k=(n >> 1) - 4; k >= 4; k -= 4) {
      j = n - k;
      xr = a[k + 2] - a[j - 2];
      xi = a[k + 3] + a[j - 1];
      yr = fma(wdr, xr, -wdi*xi);
      yi = fma(wdr, xi, wdi*xr);
      a[k + 2] -= yr;
      a[k + 3] -= yi;
      a[j - 2] += yr;
      a[j - 1] -= yi;
      wkr = fma(ss, wdi, wkr);
      wki = fma(ss, 0.5 - wdr, wki);
      xr = a[k] - a[j];
      xi = a[k + 1] + a[j + 1];
      yr = fma(wkr, xr, -wki*xi);
      yi = fma(wkr, xi, wki*xr);
      a[k] -= yr;
      a[k + 1] -= yi;
      a[j] += yr;
      a[j + 1] -= yi;
      wdr = fma(ss, wki, wdr);
      wdi = fma(ss, 0.5 - wkr, wdi);
    }
    j = n - 2;
    xr = a[2] - a[j];
    xi = a[3] + a[j + 1];
    yr = fma(wdr, xr, -wdi*xi);
    yi = fma(wdr, xi, wdi*xr);
    a[2] -= yr;
    a[3] -= yi;
    a[j] += yr;
    a[j + 1] -= yi;
    if (wi < 0.0) {
      a[1] = 0.5*(a[0] - a[1]);
      a[0] -= a[1];
      cdft64(n, wr, wi, a);
    }
  } else {
    if (wi < 0.0) {
      a[1] = 0.5*(a[0] - a[1]);
      a[0] -= a[1];
    }
    if (n > 2) {
      xr = a[0] - a[2];
      xi = a[1] - a[3];
      a[0] += a[2];
      a[1] += a[3];
      a[2] = xr;
      a[3] = xi;
    }
    if (wi >= 0.0) {
      xi = a[0] - a[1];
      a[0] += a[1];
      a[1] = xi;
    }
  }
}

static FORCE_INLINE void ddct64 (int n, double wr, double wi, double *a) {
  void rdft64(int n, double wr, double wi, double *a);
  int j, k, m;
  double wkr, wki, wdr, wdi, ss, xr;
  if (n > 2) {
    wkr = 0.5;
    wki = 0.5;
    wdr = 0.5*(wr - wi);
    wdi = 0.5*(wr + wi);
    ss = 2.0*wi;
    if (wi < 0.0) {
      xr = a[n - 1];
      for (k=n - 2; k >= 2; k -= 2) {
        a[k + 1] = a[k] - a[k - 1];
        a[k] += a[k - 1];
      }
      a[1] = 2.0*xr;
      a[0] *= 2.0;
      rdft64(n, 1 - ss*wi, ss*wr, a);
      xr = wdr;
      wdr = wdi;
      wdi = xr;
      ss = -ss;
    }
    m = n >> 1;
    for (k=1; k <= m - 3; k += 2) {
      j = n - k;
      xr = wdi*a[k] - wdr*a[j];
      a[k] = wdr*a[k] + wdi*a[j];
      a[j] = xr;
      wkr = fma(-ss, wdi, wkr);
      wki = fma(ss, wdr, wki);
      xr = fma(wki, a[k + 1], -wkr*a[j - 1]);
      a[k + 1] = fma(wkr, a[k + 1], wki*a[j - 1]);
      a[j - 1] = xr;
      wdr = fma(-ss, wki, wdr);
      wdi = fma(ss, wkr, wdi);
    }
    k = m - 1;
    j = n - k;
    xr = fma(wdi, a[k], -wdr*a[j]);
    a[k] = fma(wdr, a[k], wdi*a[j]);
    a[j] = xr;
    a[m] *= fma(ss, wdr, wki);
    if (wi >= 0.0) {
      rdft64(n, fma(-ss, wi, 1.0), ss*wr, a);
      xr = a[1];
      for (k=2; k <= n - 2; k += 2) {
        a[k - 1] = a[k] - a[k + 1];
        a[k] += a[k + 1];
      }
      a[n - 1] = xr;
    }
  } else {
    if (wi >= 0.0) {
      xr = 0.5*(wr + wi)*a[1];
      a[1] = a[0] - xr;
      a[0] += xr;
    } else {
      xr = a[0] - a[1];
      a[0] += a[1];
      a[1] = 0.5*(wr - wi)*xr;
    }
  }
}

static void dfct64 (int n, double wr, double wi, double *a) {
  void ddct64(int n, double wr, double wi, double *a);
  void bitrv64(int n, double *a);
  int j, k, m, mh;
  double xr, xi, an;
  m = n >> 1;
  for (j=0; j <= m - 1; j++) {
    k = n - j;
    xr = a[j] + a[k];
    a[j] -= a[k];
    a[k] = xr;
  }
  an = a[n];
  while (m >= 2) {
    ddct64(m, wr, wi, a);
    xr = fma(-2.0*wi, wi, 1.0);
    wi *= 2.0*wr;
    wr = xr;
    bitrv64(m, a);
    mh = m >> 1;
    xi = a[m];
    a[m] = a[0];
    a[0] = an - xi;
    an += xi;
    for (j=1; j <= mh - 1; j++) {
      k = m - j;
      xr = a[m + k];
      xi = a[m + j];
      a[m + j] = a[j];
      a[m + k] = a[k];
      a[j] = xr - xi;
      a[k] = xr + xi;
    }
    xr = a[mh];
    a[mh] = a[m + mh];
    a[m + mh] = xr;
    m = mh;
  }
  xi = a[1];
  a[1] = a[0];
  a[0] = an + xi;
  a[n] = an - xi;
  bitrv64(n, a);
}

static void intccini64 (int lenw, double *w) {
  void dfct64(int, double, double, double *);
  int j, k, l, m;
  double cos2, sin1, sin2, hl;
  cos2 = 0.0;
  sin1 = 1.0;
  sin2 = 1.0;
  hl = 0.5;
  k = lenw;
  l = 2.0;
  while (l < k - l - 1) {
    w[0] = 0.5*hl;
    for (j=1; j <= l; j++) {
      w[j] = hl/(1.0 - 4.0*j*j);
    }
    w[l] *= 0.5;
    dfct64(l, 0.5*cos2, sin1, w);
    cos2 = sqrt(2.0 + cos2);
    sin1 /= cos2;
    sin2 /= 2.0 + cos2;
    w[k] = sin2;
    w[k - 1] = w[0];
    w[k - 2] = w[l];
    k -= 3;
    m = l;
    while (m > 1) {
      m >>= 1;
      for (j=m; j <= l - m; j += (m << 1)) {
        w[k] = w[j];
        k--;
      }
    }
    hl *= 0.5;
    l *= 2;
  }
}

static void intcc64 (lua_State *L, double a, double b, double eps,
  int lenw, double *w, double *i, double *err, int nargs) {
  int j, k, l;
  double esf, eref, erefh, hh, ir, iback, irback, ba, ss, x, y, fx, errir;
  double ics, iccs, ircs, irccs;
  esf = 10;
  ba = 0.5*(b - a);
  ss = 2*w[lenw];
  x = ba*w[lenw];
  w[0] = 0.5*agnL_fncall(L, 1, a, 4, nargs);
  w[3] = 0.5*agnL_fncall(L, 1, b, 4, nargs);
  w[2] = agnL_fncall(L, 1, a + x, 4, nargs);
  w[4] = agnL_fncall(L, 1, b - x, 4, nargs);
  w[1] = agnL_fncall(L, 1, a + ba, 4, nargs);
  eref = 0.5*(fabs(w[0]) + fabs(w[1]) + fabs(w[2]) + fabs(w[3]) + fabs(w[4]));
  w[0] += w[3];
  w[2] += w[4];
  ir = w[0] + w[1] + w[2];
  *i = fma(w[0], w[lenw - 1], fma(w[1], w[lenw - 2], w[2]*w[lenw - 3]));
  erefh = eref*sqrt(eps);
  eref *= eps;
  hh = 0.25;
  l = 2;
  k = lenw - 5;
  ics = iccs = ircs = irccs = 0.0;
  do {
    iback = *i;
    irback = ir;
    x = ba*w[k + 1];
    y = 0;
    *i = w[0]*w[k];
    for (j=1; j <= l; j++) {
      x += y;
      y = fma(ss, ba - x, y);
      fx = agnL_fncall(L, 1, a + x, 4, nargs) + agnL_fncall(L, 1, b - x, 4, nargs);
      ir = tools_kbadd(ir, fx, &ircs, &irccs);
      *i = tools_kbadd(*i, fma(w[j], w[k - j], fx*w[k - j - l]), &ics, &iccs);
      w[j + l] = fx;
    }
    ir += ircs + irccs;
    *i += ics + iccs;
    ics = iccs = ircs = irccs = 0.0;  /* we have to reset the correction variables here */
    ss = 2*w[k + 1];
    *err = esf*l*fabs(*i - iback);
    hh *= 0.25;
    errir = hh*fabs(ir - 2*irback);
    l *= 2;
    k -= l + 2;
  } while ((*err > erefh || errir > eref) && k > 4*l);
  *i *= b - a;
  if (*err > erefh || errir > eref) {
    *err *= -fabs(b - a);
  } else {
    *err = eref*fabs(b - a);
  }
}

static int calc_intcc64 (lua_State *L) {
  int j, nargs, samples, adjust, iters;
  double a, b, omega, length, eps, delta, i, err, w[NSLOTS + 1];
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)iters;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  b = agnL_checknumber(L, 3);
  i = err = 0.0;
  if (a > b)
    luaL_error(L, "Error in " LUA_QS ": left boundary > right boundary.", "calc.intcc64");
  else if (a != b) {
    eps = INTEG_EPS64;
    aux_fcheckoptions(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, "calc.intcc64");
    for (j=0; j < NSLOTS + 1; j++) w[j] = intcc_w64[j];
    intcc64(L, a, b, eps, NSLOTS, w, &i, &err, nargs);
  }
  lua_pushnumber(L, (err < 0) ? AGN_NAN : i);  /* 6.7.9 change */
  lua_pushnumber(L, err);
  return 2;
}


/* Computes the mean of a function, that is the average value of the function over an interval [a, b].
   The result is 1/(b - a) * calc.intcc(f, a, b). For the options supported, see `calc.intcc`.
   See also: calc.integ, calc.intde. 4.2.4 */

static int calc_mean64 (lua_State *L);

static int calc_mean (lua_State *L) {
#ifdef __ARMCPU
  return calc_mean64(L);
#else
  int j, nargs, samples, adjust, iters, use64;
  long double a, b, omega, length, eps, delta, i, err, w[NSLOTS + 1];
  (void)omega; (void)length; (void)samples; (void)adjust, (void)delta; (void)iters;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  b = agnL_checknumber(L, 3);
  err = i = 0.0L;
  if (a > b) {  /* 3.3.5 */
    luaL_error(L, "Error in " LUA_QS ": left boundary > right boundary.", "calc.mean");
  } else if (a == b) {
    lua_pushundefined(L);
    return 1;
  }
  eps = INTEG_EPS;
  aux_fcheckoptionslong(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.mean");
  for (j=0; j < NSLOTS + 1; j++) w[j] = intcc_w[j];
  intcc(L, a, b, eps, NSLOTS, w, &i, &err, nargs);
  if (err < 0)
    lua_pushfail(L);
  else
    lua_pushnumber(L, i/(b - a));
  return 1;
#endif
}


static int calc_mean64 (lua_State *L) {
  int j, nargs, samples, adjust, iters;
  double a, b, omega, length, eps, delta, i, err, w[NSLOTS + 1];
  (void)omega; (void)length; (void)samples; (void)adjust, (void)delta; (void)iters;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  b = agnL_checknumber(L, 3);
  err = i = 0.0L;
  if (a > b) {  /* 3.3.5 */
    luaL_error(L, "Error in " LUA_QS ": left boundary > right boundary.", "calc.mean");
  } else if (a == b) {
    lua_pushundefined(L);
    return 1;
  }
  eps = INTEG_EPS64;
  aux_fcheckoptions(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, "calc.mean");
  for (j=0; j < NSLOTS + 1; j++) w[j] = intcc_w64[j];
  intcc64(L, a, b, eps, NSLOTS, w, &i, &err, nargs);
  if (err < 0)
    lua_pushfail(L);
  else
    lua_pushnumber(L, i/(b - a));
  return 1;
}


/* riesum is based on the C code written by Joshua Corpuz, Manila, Philippines, and published at:
   https://github.com/ZyWick/NumericalIntegrationCalculator/blob/main/NIM.c

   Actual Riemann sum method: The loop essentially divides the interval [a, b] into n subintervals, gets the x-coordinate of a
   point for each subinterval, evaluates the height at each point, and sums up these evaluations then multiplied by the width to
   approximate the integral. */
static void aux_checkriesum (lua_State *L, int pos, int *nargs, int *absolute, int *step, int *rule, const char *procname) {
  *absolute = 0;  /* 1 = absolute value will be summed up */
  *step = 10;     /* number of subinmtervals */
  *rule = 2;      /* mid rule */
  int checkoptions = 3;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "absolute")) {
        *absolute = agn_checkboolean(L, -1);
      } else if (tools_streq(option, "step")) {
        *step = agn_checkposint(L, -1);
      } else if (tools_streq(option, "rule")) {
        const char *rhs = agn_checkstring(L, -1);
        if      (tools_streq(rhs, "left"))   *rule = 0;
        else if (tools_streq(rhs, "right"))  *rule = 1;
        else if (tools_streq(rhs, "mid"))    *rule = 2;
        else if (tools_streq(rhs, "random")) *rule = 3;
        else {
          luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, rhs);
        }
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

#ifndef __ARMCPU
static double riesum (lua_State *L, long double a, long double b, int n, int what, int absolute, int nargs) {
  int i;
  long double dx, sum, offset, f, x;
  /* calculate delta(x) by dividing the difference between the upper limit b and lower limit a by the  number of subintervals n.
     dx represents the step size in the reference axis. */
  dx = (b - a)/n;
  /* This initializes a variable sum to store the running total of the function evaluations. */
  sum = offset = 0.0L;
  /* The choice of the user modifies the type of Riemann sum to be used. */
  switch (what) {
    case 0:  /* calculates with left endpoints starting from a */
      offset = 0.0L;
      break;
    case 1:  /* calculates with right endpoints starting from a + dx */
      offset = dx;
      break;
    case 2:  /* calculates with midpoints as we get the half of delta x or step size */
      offset = dx/2.0L;
      break;
    case 3:  /* random value between [0, dx) */
      offset = tools_reducerange(tools_random(0), 0.0, dx);
      break;
  }
  /* the loop runs until i < n, where n is the number of subintervals specified by the user since we are looking for the summation
     of the area of each subdivision. The loop will calculate the height of each subinterval as we increment it by 1. */
  for (i=0; i < n; i++) {
    /* gets the x-coordinate of a point of a subinterval. xi is calculated by adding i * dx to the starting point a, which moves xi
       from a point in the subinterval to a point in the next subinterval as the loop progresses. */
    x = a + offset + i*dx;
    /* evaluates the height of a subinterval which is added to the running total sum. The loop repeats this process for each
       subinterval, evaluating the given function at a point for each rectangle and accumulating the results in sum. */
    f = agnL_fncall(L, 1, b - x, 4, nargs);
    sum += absolute ? fabsl(f) : f;
  }
  /* the total accumulated sum of function  evaluations is multiplied by dx. The multiplication by dx scales the sum to account
     for the width of each subinterval, making the result an approximation  of the integral over the given interval [a, b]. */
  return dx*sum;
}
#endif

/* Implements numerical integration using the Riemann sum method for the univariate real function f over the
   interval [a, b] with n subintervals where n defaults to 10.

   You can choose the rule to be used: either 'left' for left-hand rule, 'right' for right-hand rule, 'random' for a random-point rule
   and 'mid' (the default) for midpoint rule.

   If you pass the `absolute=true' option as the last argument then absolute instead of signed intermediate function values will
   be summed up.

   f should be continuous over [a, b] and the interval should be carefully chosen.

   The function internally uses 80-bit precision floats.

   See also: calc.integ, calc.intde, calc.intcc. 4.2.4; extended 4.12.3 */

static int calc_riesum64 (lua_State *L);

static int calc_riesum (lua_State *L) {
#ifdef __ARMCPU
  return calc_riesum64(L);
#else
  int nargs, n, op, absolute;
  long double a, b;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  b = agnL_checknumber(L, 3);
  aux_checkriesum(L, 4, &nargs, &absolute, &n, &op, "calc.riesum");
  if (a > b) {
    luaL_error(L, "Error in " LUA_QS ": left boundary > right boundary.", "calc.riesum");
  } else if (a == b) {
    lua_pushnumber(L, 0.0);
  } else {
    lua_pushnumber(L, riesum(L, a, b, n, op, absolute, nargs));
  }
  return 1;
#endif
}


static double riesum64 (lua_State *L, double a, double b, int n, int what, int absolute, int nargs) {
  int i;
  double dx, sum, offset, f, x, cs, ccs;
  /* calculate delta(x) by dividing the difference between the upper limit b and lower limit a by the  number of subintervals n.
     dx represents the step size in the reference axis. */
  dx = (b - a)/n;
  /* This initializes a variable sum to store the running total of the function evaluations. */
  sum = offset = cs = ccs = 0.0;
  /* The choice of the user modifies the type of Riemann sum to be used. */
  switch (what) {
    case 0:  /* calculates with left endpoints starting from a */
      offset = 0.0;
      break;
    case 1:  /* calculates with right endpoints starting from a + dx */
      offset = dx;
      break;
    case 2:  /* calculates with midpoints as we get the half of delta x or step size */
      offset = dx/2.0;
      break;
    case 3:  /* random value between [0, dx) */
      offset = tools_reducerange(tools_random(0), 0.0, dx);
      break;
  }
  /* the loop runs until i < n, where n is the number of subintervals specified by the user since we are looking for the summation
     of the area of each subdivision. The loop will calculate the height of each subinterval as we increment it by 1. */
  for (i=0; i < n; i++) {
    /* gets the x-coordinate of a point of a subinterval. xi is calculated by adding i * dx to the starting point a, which moves xi
       from a point in the subinterval to a point in the next subinterval as the loop progresses. */
    x = fma(i, dx, a + offset);
    /* evaluates the height of a subinterval which is added to the running total sum. The loop repeats this process for each
       subinterval, evaluating the given function at a point for each rectangle and accumulating the results in sum. */
    f = agnL_fncall(L, 1, b - x, 4, nargs);
    sum = tools_kbadd(sum, absolute ? fabsl(f) : f, &cs, &ccs);
  }
  sum += cs + ccs;
  /* the total accumulated sum of function  evaluations is multiplied by dx. The multiplication by dx scales the sum to account
     for the width of each subinterval, making the result an approximation  of the integral over the given interval [a, b]. */
  return dx*sum;
}

static int calc_riesum64 (lua_State *L) {
  int nargs, n, op, absolute;
  double a, b;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agnL_checknumber(L, 2);
  b = agnL_checknumber(L, 3);
  aux_checkriesum(L, 4, &nargs, &absolute, &n, &op, "calc.riesum64");
  if (a > b) {
    luaL_error(L, "Error in " LUA_QS ": left boundary > right boundary.", "calc.riesum64");
  } else if (a == b) {
    lua_pushnumber(L, 0.0);
  } else {
    lua_pushnumber(L, riesum64(L, a, b, n, op, absolute, nargs));
  }
  return 1;
}


static int calc_Si (lua_State *L) {  /* 0.32.2, Sine Integral */
  lua_Number x, si, ci;
  x = agn_checknumber(L, 1);
  if (x == HUGE_VAL)
    lua_pushnumber(L, 0.5*PI);
  else if (-x == HUGE_VAL)
    lua_pushnumber(L, -0.5*PI);
  else {
    sici(x, &si, &ci);
    lua_pushnumber(L, si);
  }
  return 1;
}


static int calc_Ci (lua_State *L) {  /* 0.32.2, Cosine Integral */
  lua_Number x, si, ci;
  x = agn_checknumber(L, 1);
  if (x == HUGE_VAL)
    lua_pushnumber(L, 0);
  else if (-x == HUGE_VAL)
    lua_pushfail(L);
  else if (x < 0)
    lua_pushundefined(L);
  else {
    sici(x, &si, &ci);  /* 4.11.7 fix */
    lua_pushnumber(L, ci);
  }
  return 1;
}


static int calc_SiCi (lua_State *L) {  /* 4.11.7 */
  lua_Number x, si, ci;
  x = agn_checknumber(L, 1);
  if (x == HUGE_VAL) {
    lua_pushnumber(L, 0.5*PI);
    lua_pushnumber(L, 0);
  } else if (-x == HUGE_VAL) {
    lua_pushnumber(L, -0.5*PI);
    lua_pushfail(L);
  } else {
    sici(x, &si, &ci);
    lua_pushnumber(L, si);
    lua_pushnumber(L, (x < 0) ? AGN_NAN : ci);
  }
  return 2;
}


/* Cin - Entire Cosine Integral **********************************************************************
   Taken from: http://www.mymathlib.com/functions/sin_cos_integrals.html#entire_cos_integral
   Copyright © 2004 RLH. All rights reserved. */

static FORCE_INLINE long double fi_rational_polynomial (long double x, long double a[], long double b[], int n ) {
  long double xx = x*x;
  long double numerator = xx + a[n - 1];
  long double denominator = xx + b[n - 1];
  int i;
  for (i=n - 2; i >= 0; i--) {
    numerator *= xx;
    numerator += a[i];
    denominator *= xx;
    denominator += b[i];
  }
  return (numerator/denominator)/x;
}

static FORCE_INLINE long double gi_rational_polynomial (long double x, long double a[], long double b[], int n ) {
  long double xx = x*x;
  long double numerator = xx + a[n - 1];
  long double denominator = xx + b[n - 1];
  int i;
  for (i=n - 2; i >= 0; i--) {
    numerator *= xx;
    numerator += a[i];
    denominator *= xx;
    denominator += b[i];
  }
  return (numerator/denominator)/xx;
}

static long double a_x_ge_1_le_4_fi[] = {
  +3.131622691136541251894e+6L,  +5.865887504115410010938e+8L,
  +1.634852375578508416146e+10L, +1.592481384106901732624e+11L,
  +7.184770514348595264787e+11L, +1.726730020205455640781e+12L,
  +2.397017133822436251930e+12L, +2.020697105077248035167e+12L,
  +1.067232555649863576986e+12L, +3.595836616885923865165e+11L,
  +7.789746108788072914678e+10L, +1.083563302486680874140e+10L,
  +9.574882063563057212637e+8L,  +5.257964657853357906628e+7L,
  +1.727886704287183044067e+6L,  +3.186889399585378551937e+4L,
  +2.926771594419498165548e+2L
};

static long double b_x_ge_1_le_4_fi[] = {
  +4.436542812456388065099e+7L,  +3.071881739597743437918e+9L,
  +5.510695064187223810111e+10L, +4.064528338807937104680e+11L,
  +1.502383531521515631047e+12L, +3.100277228892702060035e+12L,
  +3.813360580503500561372e+12L, +2.914078460895404297552e+12L,
  +1.419825394894026616675e+12L, +4.475808471509418423489e+11L,
  +9.178793064550390753770e+10L, +1.220855825313365329368e+10L,
  +1.040609666863148007442e+9L,  +5.554659381265650120714e+7L,
  +1.786400496083247945938e+6L,  +3.243425514601407346662e+4L,
  +2.946771482142805033246e+2L
};

static long double a_x_ge_1_le_4_gi[] = {
  +9.011634207324336137169e+5L,  +7.479818286024998460948e+8L,
  +4.151156375407831323555e+10L, +7.527803170191763096250e+11L,
  +6.273399733237371076085e+12L, +2.814715541899249302011e+13L,
  +7.442080767131902041599e+13L, +1.227172725914716222093e+14L,
  +1.309767511841246149009e+14L, +9.271225348708999857908e+13L,
  +4.418956912530285701879e+13L, +1.429482655697021907140e+13L,
  +3.143569573598121793475e+12L, +4.678982847861465840256e+11L,
  +4.663335634051774987907e+10L, +3.055838078958224702739e+9L,
  +1.280057381534594891504e+8L,  +3.283789466836908440869e+6L,
  +4.818060733773778820102e+4L,  +3.575079810165216346615e+2L
};

static long double b_x_ge_1_le_4_gi[] = {
  +3.473778902563924058876e+8L,  +2.845671273312673204906e+10L,
  +6.887224173494194811858e+11L, +7.375036329278632360411e+12L,
  +4.176080452260044111884e+13L, +1.381922611468670308990e+14L,
  +2.845010797960102251342e+14L, +3.797756529707299562974e+14L,
  +3.379834764627141276920e+14L, +2.042485720392467096358e+14L,
  +8.475284361332246080070e+13L, +2.427267535696371015657e+13L,
  +4.796526275835169465639e+12L, +6.502922666518397649596e+11L,
  +5.978699373743563855764e+10L, +3.657882344026889055127e+9L,
  +1.447319540468370039281e+8L,  +3.546640511226990055118e+6L,
  +5.024169863961865278657e+4L,  +3.635079182389876878272e+2L
};

static long double a_x_ge_4_le_12_fi[] = {
  +8.629036659345232923178e+15L, +9.470743102805298529462e+16L,
  +1.568021122342358329530e+17L, +9.015832733196613551192e+16L,
  +2.373367953145819143578e+16L, +3.275410521405716571530e+15L,
  +2.556227076494300926751e+14L, +1.177702886070105437976e+13L,
  +3.270951405687038350516e+11L, +5.490274976211303931784e+9L,
  +5.472393083052247561960e+7L,  +3.092021722264748314966e+5L,
  +8.929706311321410431845e+2L
};

static long double b_x_ge_4_le_12_fi[] = {
  +3.688863305339062824609e+16L, +1.876827085370834659310e+17L,
  +2.346441340788672968041e+17L, +1.163521165422882838284e+17L,
  +2.802875478319020095488e+16L, +3.655276206330722751898e+15L,
  +2.748086189268929173963e+14L, +1.234713082649844595139e+13L,
  +3.371469088301994839064e+11L, +5.594066018240082151795e+9L,
  +5.532510775982731500507e+7L,  +3.109681134906426986992e+5L,
  +8.949706311313908973230e+2L
};

static long double a_x_ge_4_le_12_gi[] = {
  +9.760124389962086158256e+17L, +2.768135717060729724771e+19L,
  +7.269925460678163397319e+19L, +6.335403079477117544205e+19L,
  +2.521611356160483301958e+19L, +5.326725622049037767865e+18L,
  +6.500059887901948040470e+17L, +4.822924381737713175777e+16L,
  +2.243854020350856804468e+15L, +6.651665288514689504327e+13L,
  +1.260766706261790080221e+12L, +1.513530299892650289088e+10L,
  +1.121138426325906850959e+8L,  +4.860629732996342070790e+5L,
  +1.106668096706748177652e+3L
};

static long double b_x_ge_4_le_12_gi[] = {
  +2.816012818637797223215e+19L, +1.453987140070137119268e+20L,
  +2.126102863700101349915e+20L, +1.330695747874759471888e+20L,
  +4.272711621417996420464e+19L, +7.778583943891982632436e+18L,
  +8.532286887837346444555e+17L, +5.856829003446583158094e+16L,
  +2.573169752130006183553e+15L, +7.312517856321160958464e+13L,
  +1.343719194435306279692e+12L, +1.577108016388663217206e+10L,
  +1.149410763356329587152e+8L,  +4.926189818912136539829e+5L,
  +1.112668096702659763721e+3L
};

static long double a_x_ge_12_le_48_fi[] = {
  +8.190718946165709238422e+17L, +1.209912798380869069939e+18L,
  +2.685711451753038556686e+17L, +2.031432644806673394287e+16L,
  +6.849516346373244528380e+14L, +1.167908359237227948685e+13L,
  +1.071365422608890062545e+11L, +5.395836264116777645374e+8L,
  +1.462073394608352079917e+6L,  +1.959326763594685895502e+3L
};

static long double b_x_ge_12_le_48_fi[] = {
  +1.759376483182613052616e+18L, +1.549737809630230245083e+18L,
  +3.002314821022841548975e+17L, +2.150253471166368305136e+16L,
  +7.064600781175281798566e+14L, +1.188341971751225609460e+13L,
  +1.081876692043348699994e+11L, +5.424692186656225562683e+8L,
  +1.465972048135541454369e+6L,  +1.961326763594685895323e+3L
};

static long double a_x_ge_12_le_48_gi[] = {
  +5.524091612614961621464e+19L, +1.284075904576105184520e+20L,
  +3.447334407523257944528e+19L, +3.121715037272484722094e+18L,
  +1.282539019600256176592e+17L, +2.740263968387649522824e+15L,
  +3.267265290103262920765e+13L, +2.245923126260050126684e+11L,
  +8.923806059854096302378e+8L,  +1.985082566703293127903e+6L,
  +2.254025115381787893881e+3L
};

static long double b_x_ge_12_le_48_gi[] = {
  +3.247999301164088453284e+20L, +2.442688918303073183435e+20L,
  +4.767807497134760332700e+19L, +3.740845893032137972381e+18L,
  +1.425986072860589430641e+17L, +2.920317933370183472849e+15L,
  +3.395218149102856121458e+13L, +2.297881510221565965240e+11L,
  +9.041055792759368518992e+8L,  +1.998522717395583928785e+6L,
  +2.260025115381787888363e+3L
};

static int n_x_ge_1_le_4_fi = sizeof(a_x_ge_1_le_4_fi)/sizeof(long double);
static int n_x_ge_1_le_4_gi = sizeof(a_x_ge_1_le_4_gi)/sizeof(long double);
static int n_x_ge_4_le_12_fi = sizeof(a_x_ge_4_le_12_fi)/sizeof(long double);
static int n_x_ge_4_le_12_gi = sizeof(a_x_ge_4_le_12_gi)/sizeof(long double);
static int n_x_ge_12_le_48_fi = sizeof(a_x_ge_12_le_48_fi)/sizeof(long double);
static int n_x_ge_12_le_48_gi = sizeof(a_x_ge_12_le_48_gi)/sizeof(long double);

static const double auxiliary_asymptotic_cutoff = 48.0;

static FORCE_INLINE long double xPower_Series_Cin (long double x) {
  long double sum;
  long double xx = -x*x;
  int n = (int)(2.41*xx + 7.15*fabsl(x) + 7.00);
  int k = n + n;
  /* If the argument is sufficiently small use the approximation
     Cin(x) = x^2 exp(-x^2/24) / 4 */
  if (fabsl(x) < 0.00025L) return - (tools_expl(xx/24.0L)*xx)/4.0L;
  /* Otherwise evaluate the power series expansion for Cin(x). */
  sum = xx/(k*k*(k - 1)) + 1.0L/(k - 2);
  for (k -= 2; k > 2; k -= 2) {
    sum *= xx/(k*(k - 1));
    sum += 1.0L/(k - 2);
  }
  return -0.5L*xx*sum;
}

static FORCE_INLINE long double xPower_Series_Si (long double x) {
  long double sum;
  long double xx = -x*x;
  int n = (int)(3.42*xx + 7.46*fabsl(x) + 6.95);
  int k = n + n;
  /* If the argument is sufficiently small use the approximation
     Si(x) = x exp(-x^2/18) */
  if (fabsl(x) <= 0.0003L) return x*tools_expl(xx/18.0L);
  /* Otherwise evaluate the power series expansion for Si(x). */
  sum = xx/((k + 1)*(k + 1)*k) + 1.0L/(k - 1);
  for (k--; k >= 3; k -=2) {
    sum *= xx/(k*(k - 1));
    sum += 1.0L/(k - 2);
  }
  return x*sum;
}

static FORCE_INLINE void xAuxiliary_Sin_Cos_Integrals_fi_gi (long double x, long double *fi, long double *gi);

static FORCE_INLINE long double Asymptotic_Series_Ci (long double x){
  long double fi, gi;
  xAuxiliary_Sin_Cos_Integrals_fi_gi(fabsl(x), &fi, &gi);
  return sun_sinl(fabsl(x))*fi - sun_cosl(x)*gi;
}

static long double xCos_Integral_Ci (long double x) {  /* GCC says not `inlinable` */
  if (x == 0.0L) return -LDBL_MAX;
  return (fabsl(x) <= 1.0L) ? tools_logl(fabsl(x)) + EULERGAMMA - xPower_Series_Cin(x) : Asymptotic_Series_Ci(x);
}

static FORCE_INLINE long double Asymptotic_Series_fi (long double x) {
  long double term = 1.0L;
  long double xx = - x*x;
  long double xn = 1.0L;
  long double factorial = 1.0L;
  long double fi = 0.0L;
  long double old_term = 0.0L;
  int j = 2;
  do {
    fi += term;
    old_term = term;
    factorial *= (long double)(j*(j - 1));
    xn *= xx;
    term = factorial/xn;
    j += 2;
  } while (fabsl(term) < fabsl(old_term));
  return fi/x;
}

static FORCE_INLINE long double Asymptotic_Series_gi (long double x) {
  long double term = 1.0L;
  long double xx = - x*x;
  long double xn = 1.0L;
  long double factorial = 1.0L;
  long double gi = 0.0L;
  long double old_term = 0.0L;
  int j = 3;
  do {
    gi += term;
    old_term = term;
    factorial *= (long double)(j*(j - 1));
    xn *= xx;
    term = factorial/xn;
    j += 2;
  } while (fabsl(term) < fabsl(old_term));
  return -gi/xx;
}

static FORCE_INLINE void xAuxiliary_Sin_Cos_Integrals_fi_gi (long double x, long double *fi, long double *gi) {
  long double si, ci, sx, cx;
  if (x == 0.0L) {
    *fi = PI2;
    *gi = LDBL_MAX;
  } else if (x <= 1.0L) {
    si = xPower_Series_Si(x);
    ci = xCos_Integral_Ci(x);
    sx = sun_sinl(x);
    cx = sun_cosl(x);
    *fi = sx*ci + cx*(PI2 - si);
    *gi = sx*(PI2 - si) - cx*ci;
  } else if (x <= 4.0) {
    *fi = fi_rational_polynomial(x, a_x_ge_1_le_4_fi, b_x_ge_1_le_4_fi, n_x_ge_1_le_4_fi);
    *gi = gi_rational_polynomial(x, a_x_ge_1_le_4_gi, b_x_ge_1_le_4_gi, n_x_ge_1_le_4_gi);
  } else if (x <= 12.0L) {
    *fi = fi_rational_polynomial(x, a_x_ge_4_le_12_fi, b_x_ge_4_le_12_fi, n_x_ge_4_le_12_fi);
    *gi = gi_rational_polynomial(x, a_x_ge_4_le_12_gi, b_x_ge_4_le_12_gi, n_x_ge_4_le_12_gi);
  } else if (x < auxiliary_asymptotic_cutoff) {
    *fi = fi_rational_polynomial(x, a_x_ge_12_le_48_fi, b_x_ge_12_le_48_fi, n_x_ge_12_le_48_fi);
    *gi = gi_rational_polynomial(x, a_x_ge_12_le_48_gi, b_x_ge_12_le_48_gi, n_x_ge_12_le_48_gi);
  } else {
    *fi = Asymptotic_Series_fi(x);
    *gi = Asymptotic_Series_gi(x);
  }
  return;
}

static int calc_Cin (lua_State *L) {  /* 2.41.0 */
  lua_Number x = agn_checknumber(L, 1);
  lua_pushnumber(L, (fabsl(x) <= 1.0L) ? xPower_Series_Cin(x) :
                                         tools_logl(fabsl(x)) + EULERGAMMA - Asymptotic_Series_Ci(x));
  return 1;
}


/* auxSiCi - Auxiliary Sine & Cosine Integrals  ******************************************************
   Taken from: http://www.mymathlib.com/functions/sin_cos_integrals.html#entire_cos_integral
   Copyright © 2004 RLH. All rights reserved. */

FORCE_INLINE long double xAuxiliary_Sin_Integral_fi (long double x) {
  long double si, ci;
  if (x == 0.0L) return PI2;
  if (x <= 1.0L) {
    si = xPower_Series_Si(x);
    ci = xCos_Integral_Ci(x);
    return sun_sinl(x)*ci + sun_cosl(x)*(PI2 - si);
  }
  if (x <= 4.0L)
    return fi_rational_polynomial(x, a_x_ge_1_le_4_fi, b_x_ge_1_le_4_fi, n_x_ge_1_le_4_fi);
  if (x <= 12.0L)
    return fi_rational_polynomial(x, a_x_ge_4_le_12_fi, b_x_ge_4_le_12_fi, n_x_ge_4_le_12_fi);
  if (x < auxiliary_asymptotic_cutoff)
    return fi_rational_polynomial(x, a_x_ge_12_le_48_fi, b_x_ge_12_le_48_fi, n_x_ge_12_le_48_fi);
  return Asymptotic_Series_fi( x );
}

long double xAuxiliary_Cos_Integral_gi (long double x) {
  long double si, ci;
  if (x == 0.0L) return LDBL_MAX;
  if (x <= 1.0L) {
    si = xPower_Series_Si(x);
    ci = xCos_Integral_Ci(x);
    return sun_sinl(x)*(PI2 - si) - sun_cosl(x)*ci;
  }
  if (x <= 4.0L)
    return gi_rational_polynomial(x, a_x_ge_1_le_4_gi, b_x_ge_1_le_4_gi, n_x_ge_1_le_4_gi);
  if (x <= 12.0L)
    return gi_rational_polynomial(x, a_x_ge_4_le_12_gi, b_x_ge_4_le_12_gi, n_x_ge_4_le_12_gi);
  if (x < auxiliary_asymptotic_cutoff)
    return gi_rational_polynomial(x, a_x_ge_12_le_48_gi, b_x_ge_12_le_48_gi, n_x_ge_12_le_48_gi);
  return Asymptotic_Series_gi( x );
}

static int calc_auxSiCi (lua_State *L) {  /* 2.41.0 */
  long double xfi, xgi;
  lua_Number x = agn_checknumber(L, 1);
  xAuxiliary_Sin_Cos_Integrals_fi_gi((long double) x, &xfi, &xgi);
  lua_pushnumber(L, (double)xfi);
  lua_pushnumber(L, (xgi >= DBL_MAX) ? DBL_MAX : (double) xgi);
  return 2;
}


static int calc_Ssi (lua_State *L) {  /* 0.32.2 */
  lua_Number x, si, ci;
  x = agn_checknumber(L, 1);
  if (x == HUGE_VAL)
    lua_pushnumber(L, 0);
  else if (-x == HUGE_VAL)
    lua_pushnumber(L, -PI);
  else {
    sici(x, &si, &ci);
    lua_pushnumber(L, si - 0.5*PI);
  }
  return 1;
}


static int calc_Shi (lua_State *L) {  /* 0.32.2 */
  lua_Number x, shi, chi;
  x = agn_checknumber(L, 1);
  if (-x == HUGE_VAL)
    lua_pushnumber(L, x);
  else {
    shichi(x, &shi, &chi);
    lua_pushnumber(L, shi);
  }
  return 1;
}


static int calc_Chi (lua_State *L) {  /* 0.32.2 */
  lua_Number x, shi, chi;
  x = agn_checknumber(L, 1);
  if (-x == HUGE_VAL)
    lua_pushfail(L);
  else if (x < 0)
    lua_pushundefined(L);
  else {
    shichi(agn_checknumber(L, 1), &shi, &chi);
    lua_pushnumber(L, chi);
  }
  return 1;
}


static int calc_dawson (lua_State *L) {  /* 0.32.2 */
  lua_pushnumber(L, dawsn(agn_checknumber(L, 1)));
  return 1;
}


/* Scaled Dawson Integral w_im(x) = 2*calc.dawson(x)/sqrt(Pi), 2.21.6 */
static int calc_scaleddawson (lua_State *L) {
  lua_pushnumber(L, tools_w_im(agn_checknumber(L, 1)));
  return 1;
}


static int calc_Ei (lua_State *L) {  /* exponential integral, 0.32.2, extended 2.1.5 for negative arguments */
  if (lua_gettop(L) == 1) {
    lua_pushnumber(L, tools_ei(agn_checknumber(L, 1)));
  } else {
    lua_pushnumber(L, tools_ei_taylor(agn_checknonnegint(L, 1), agn_checknumber(L, 2)));
  }
  return 1;
}


/* Entire Exponential Integral, 2.41.0, see:
   http://www.mymathlib.com/functions/exponential_integrals.html */
static int calc_Ein (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  lua_pushnumber(L, (x == 0.0) ? 0 : (EULERGAMMA + sun_log(fabs(x)) - tools_ei(-x)));
  return 1;
}


/* Computes the logarithmic integral calc.Li(x) = calc.Ei(ln(x)). It provides an approximation to the number
   of primes less than x > 1. 6.4.5 */
static int calc_Li (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  if (x <= 1) {  /* just calling tools_ei(sun_log(x)) for x <= 1 returns surprising results ... */
    lua_pushundefined(L);
  } else {
    lua_pushnumber(L, tools_ei(sun_log(x)));
  }
  return 1;
}


/* post-6.4.5, UNDOC */
static int calc_digamma (lua_State *L) {
  lua_pushnumber(L, r8_digamma(agn_checknumber(L, 1)));
  return 1;
}


/* Computes the elementary integral

                       infinity
                      /
                     |                     1
     RC(x, y) = 1/2  |          ----------------------- dt
                     |          sqrt((t + x)*(t + y)^2)
                    /
                      0

   where x is nonnegative and y is positive.

   By default, with eps = 0, the function uses non-iterative hard-coded formulas to compute the result.
   Where available, the function uses 80-bit precision floating-point arithmetic internally.

   With any positive eps, calls an iterative algorithm which is around three times slower (with eps =
   DoubleEps). 6.4.6 */
static int calc_rc (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  lua_Number y = agn_checknumber(L, 2);
  lua_Number eps = agnL_optnonnegative(L, 3, 0);
  if (eps != 0.0) {
    lua_pushnumber(L, r8_rc(x, y, eps));
  } else {
    long double r, sx, sxmy;
    if (x < 0.0L || y <= 0.0L) {
      r = AGN_NAN;
    } else {
      sx = sqrtl(x);
      if (x == y) {
        r = 1.0L/sx;
      } else if (x > y) {
        sxmy = sqrtl(x - y);
        r = tools_logl((sx + sxmy)/(sx - sxmy))/sqrtl(y);
      } else {
        r = sun_acosl(sqrtl(x/y))/sqrtl(y - x);
      }
    }
    lua_pushnumber(L, r);
  }
  return 1;
}


static int calc_harmonic (lua_State *L) {  /* 3.13.1 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      lua_pushnumber(L, (x == 0.0) ? AGN_NAN : psi(x + 1) + EULERGAMMA);  /* 6.4.5 fix */
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number a, b, re, im;
      agn_getcmplxparts(L, 1, &a, &b);
      tools_cpsi(a + 1, b, &re, &im);
      agn_pushcomplex(L, re + EULERGAMMA, im);
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "calc.harmonic");
  }
  return 1;
}


static int calc_dilog (lua_State *L) {  /* 0.32.2 */
  lua_pushnumber(L, spence(agn_checknumber(L, 1)));
  return 1;
}


static int calc_fresnelc (lua_State *L) {  /* 0.32.3 */
  lua_Number x, s, c;
  x = agn_checknumber(L, 1);
  fresnl(x, &s, &c);
  lua_pushnumber(L, c);
  return 1;
}


static int calc_fresnels (lua_State *L) {  /* 0.32.3 */
  lua_Number x, s, c;
  x = agn_checknumber(L, 1);
  fresnl(x, &s, &c);
  lua_pushnumber(L, s);
  return 1;
}


/* Computes the upper (default) or lower incomplete gamma function for argument x and non-negative parameter a. 3.1.4
   Maple:
   u := (a, x) -> int(t^(a-1) * exp(-t), t=x..infinity ); # upper incomplete gamma function
   l := (a, x) -> int(t^(a-1) * exp(-t), t=0..x); # lower incomplete gamma function */
static int calc_gammainc (lua_State *L) {
  lua_Number a, x;
  int isupper = (lua_gettop(L) == 2);
  a = agn_checknonnegative(L, 1);
  x = agn_checknonnegative(L, 2);
  lua_pushnumber(L, isupper ? tools_upperincompletegamma(a, x) : tools_lowerincompletegamma(a, x));
  return 1;
}


/* Implements the exponential sum function e_n(x), sometimes also denoted exp_n(x), 3.1.4
   See: https://mathworld.wolfram.com/ExponentialSumFunction.html */
static int calc_expn (lua_State *L) {  /* 3.1.4 */
  lua_Number n, x, gam, u;
  n = agn_checknumber(L, 1);
  x = agn_checknumber(L, 2);
  u = tools_upperincompletegamma(n, x);
  gam = tools_gamma(n + 1);
  lua_pushnumber(L, tools_isnan(gam) ? gam : u*sun_exp(x)/gam);  /* gim = upper incomplete gamma function */
  return 1;
}


/* calc.sections (f, a, b {, ..., step = val])

   returns all intervals where a function has a change in sign. f must be a function, a the left border of the
   main interval, b its right border, and step the step size. The return is a sequence of pairs denoting the
   found subintervals.

   Maple versions: 1997, 1998; tuned Agena version January 21, 2008;
   C Version of Agena version 29.05.2010, 0.32.3 */

static int calc_sections (lua_State *L) {
  double xleft, xright, step, eps, fl, fr, total;
  volatile double s, t, c, i;
  size_t seqcounter, counter;
  int nargs, iters;
  (void)iters;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  xleft = agn_checknumber(L, 2);
  xright = agn_checknumber(L, 3) + agn_gethepsilon(L);
  eps = agn_gethepsilon(L);  /* `eps` option, 3.12.0 */
  aux_checkepsandstep(L, 4, &nargs, &eps, &step, &iters, "calc.sections");
  if (step <= eps)
    luaL_error(L, "Error in " LUA_QS ": step size |%f| <= %f threshold.", "calc.sections", (lua_Number)step, (lua_Number)eps);
  luaL_checkstack(L, 1, "too many arguments");  /* 3.12.2 fix against stack corruption */
  agn_createseq(L, 4);  /* result */
  fl = agnL_fncall(L, 1, xleft, 4, nargs);  /* 3.12.0 change for multivariate functions */
  i = s = xleft;
  seqcounter = counter = 0;
  total = tools_numiters(xleft, xright, step);  /* total number of iterations, do not use floor, (int), etc ! */
  c = 0.0;
  while (i <= xright - step || total > counter) {  /* 3.19.3 fix to exclude ranges that are not in the bracketing interval */
    fr = agnL_fncall(L, 1, i + step, 4, nargs);    /* 3.12.0 change for multivariate functions */
    if ((fl*fr <= 0.0 ||
        (tools_isnan(fl) && !tools_isnan(fr)) ||  /* 4.12.3 improvement */
        (!tools_isnan(fl) && tools_isnan(fr)) ) && i + step <= xright) {      /* 3.19.3 fix to exclude ranges that are not in the bracketing interval */
      if (fabs(i) < agn_getdblepsilon(L)) i = 0.0;
      agn_createpairnumbers(L, i, fabs(i + step) < agn_getdblepsilon(L) ? 0.0 : i + step);  /* 2.34.1/4.12.3 optimisation */
      lua_seqrawseti(L, -2, ++seqcounter);
    }
    counter++;
    /* since right border in the current iteration is equal to the left border in the next iteration, avoid calling f twice. */
    fl = fr;
    /* apply Adapted Neumaier summation to avoid roundoff errors; in the context of a running iterator it is better than
       Kahan-Babuška. 2.34.0 */
    t = s + step;
    c += (fabs(s) >= step) ? (s - t) + step : (step - t) + s;
    s = t;
    i = s + c;
    /* Neumaier and Kx summation are inaccuarate over -step - 1 .. step + 1, so we have to adjust */
    if (fabs(i) < L->hEps) i = 0.0;  /* 4.11.9/4.12.1 fixes, taken from lvm.c */
    /* internal roundoff prevention variable c and uncorrected accumulator s will be updated with the next iteration. */
  }
  return 1;  /* return result */
}


/* Modified Regula Falsi method; Maple versions: 1996, 1997; Agena port January 20, 2008/July 02, 2008
   This algorithm, taken from an unknown FORTRAN book in the 1980s, is the most accurate and the
   fastest one, applicable to a large range of function types. All other methods tested
   including checking for a change of sign are slower and more inaccurate. */
#define ZEROS_MAXITER 125

static int calc_regulafalsi (lua_State *L) {  /* switched to long doubles 2.34.10 */
  int c, samples, adjust, nargs, iters, use64;
  long double x1, x2, u, v, z, a, b, eps, delta, omega, length;
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)iters; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  eps = agn_getepsilon(L);  /* default: Agena's current Eps setting (L->Eps); 3.12.0 change for `eps` option */
  iters = ZEROS_MAXITER;
  aux_fcheckoptionslong(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.regulafalsi");  /* 3.12.0 change */
  x1 = a;
  x2 = b;
  z = AGN_NAN;
  u = agnL_fncall(L, 1, x1, 4, nargs);  /* 3.12.0 change for multivariate support */
  v = agnL_fncall(L, 1, x2, 4, nargs);  /* ditto */
  if (fabsl(u) < eps) {
    lua_pushnumber(L, x1);
    return 1;
  }
  if (fabsl(v) < eps) {
    lua_pushnumber(L, x2);
    return 1;
  }
  c = 0;
  while (fabsl(u - v) > eps && c++ < iters) {
    z = x2 - v*(x2 - x1)/(v - u);
    u = v;
    x1 = x2;
    x2 = z;
    v = agnL_fncall(L, 1, x2, 4, nargs);  /* 3.12.0 change for multivariate support */
  }
  if (c != iters && z >= a && z <= b)
    lua_pushnumber(L, fabs(z) < agn_getdblepsilon(L) ? 0 : z);  /* 4.11.2 extension */
  else
    lua_pushnil(L);
  return 1;
}


/* Determines the root of the univariate function f in the borders a and b and returns a number if successful,
   and null otherwise.

   In general, the function will even return accurate results where `calc.regulafalsi` does not - or even cannot
   find a root at all*) -, but the runtime behaviour compared to `calc.regulafalsi` depends on the following conditions:
   a) the interval should not be too far from the origin,
   b) the width of the interval should not be too small.
   If both conditions are met, then `calc.zeroin` can be faster than `calc.regulafalsi`.

   The algorithm uses bisection combined with linear or quadric inverse interpolation, followed by applying
   Regula Falsi to the estimate done by the previous actions.

   Algorithm designed by G. Forsythe, M. Malcolm, C. Moler, Computer methods for mathematical computations.
   M., Mir, 1980, p.180 of the Russian edition, extended by awalz.

   *) Try exp(x)*sin(x) over [20*Pi -2, 20*Pi + 2].

   Source based upon http://www.bvds.be/bvds/mat350/zeroin.c. 2.24.3 */

/*
f := << x -> exp(x)*sin(x) >>

st := 2
maxiter := 100k;
border := 20

watch();
to maxiter do
   for i from -border to border do
      x := calc.regulafalsi(f, i*Pi - st, i*Pi + st, Eps)
   od
od;

watch():

watch();
to maxiter do
   for i from -border to border do
      y := calc.zeroin(f, i*Pi - st, i*Pi + st, Eps)
   od
od;
watch():
*/

#define MAXITERS 25  /* ten interpolations suffice */
static int calc_zeroin (lua_State *L) {
  long double a, b, origa, origb, c, fa, fb, fc, p, q, previous, next, dbleps, cureps, u, v, z, eps, delta, omega, length;
  int cnt, nargs, adjust, samples, use64;
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  c = a; origa = a; origb = b;
  eps = agn_getepsilon(L);  /* default: Agena's current Eps setting (L->Eps); 3.12.0 change for eps option */
  dbleps = agn_getdblepsilon(L);
  cnt = MAXITERS;
  aux_fcheckoptionslong(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &cnt, &use64, "calc.zeroin");  /* 3.12.0 change */
  fa = agnL_fncall(L, 1, a, 4, nargs);
  fb = agnL_fncall(L, 1, b, 4, nargs);
  fc = fa;
  if (fabsl(fa) < eps) {  /* root at endpoints ? */
    lua_pushnumber(L, a);
    return 1;
  }
  if (fabsl(fb) < eps) {
    lua_pushnumber(L, b);
    return 1;
  }
  while (cnt--) {
    previous = b - a;
    if (fabsl(fc) < fabsl(fb)) {
      a = b; b = c; c = a;
      fa = fb; fb = fc; fc = fa;
    }
    cureps = 2.0L*DBL_EPSILON*fabsl(b) + 0.5L*eps;
    next = 0.5L*(c - b);
    if (fabsl(next) <= cureps || tools_approxl(fb, 0.0L, dbleps))
      break;  /* 4.12.4 change from eps to dbleps to avoid problems with 64-bit Linux around the origin, e.g. with sinc. */
    if (fabsl(previous) >= cureps && fabsl(fa) > fabsl(fb)) {  /* can interpolation be done ? */
      register long double cmb, ux, uy, uxm1, ptwice;
      cmb = c - b;
      if (a == c) {  /* do linear interpolation */
        ux = fb/fa;
        p = cmb*ux;
        q = 1.0L - ux;
      } else {       /* do quadric inverse interpolation*/
        q = fa/fc;
        ux = fb/fc;
        uy = fb/fa;
        uxm1 = ux - 1.0L;
        p = uy*(cmb*q*(q - ux) - (b - a)*uxm1);
        q = (q - 1.0L)*uxm1*(uy - 1.0L);
      }
      if (p > 0.0L) q = -q;
      else p = -p;
      ptwice = 2.0L*p;
      if (ptwice < (1.5L*cmb*q - fabsl(cureps*q)) && ptwice < fabsl(previous*q))
        next = p/q;
    }
    if (fabsl(next) < cureps) {
      next = (next > 0.0L) ? cureps : -cureps;
    }
    a = b; fa = fb;
    b += next;
    fb = agnL_fncall(L, 1, b, 4, nargs);
    if ((fb > 0.0L && fc > 0.0L) || (fb < 0.0L && fc < 0.0L)) {  /* do not replace by fb*fc > 0 */
      c = a; fc = fa;
    }
  }
  /* now refine the estimate using regula falsi if the estimate is still too coarse */
  cnt = 2*MAXITERS;  /* 4.12.1 emergency brake fix */
  z = AGN_NAN;
  u = fa; v = fb;
  while (cnt-- && fabsl(u - v) > dbleps) {  /* just comparing against eps produces inaccurate results */
    z = b - v*(b - a)/(v - u);
    u = v;
    a = b;
    b = z;
    v = agnL_fncall(L, 1, b, 4, nargs);
  }
  if (z != z && tools_approxl(fb, 0.0L, eps))  /* regula falsi loop has not been run */
    lua_pushnumber(L, b);
  else if (z >= origa && z <= origb)
    lua_pushnumber(L, fabsl(z) < dbleps ? 0 : z);  /* 4.11.2 extension; z, not b */
  else
    lua_pushnil(L);
  return 1;
}


/* Implements the Anderson-Björck root-finding algorithm; it is a modification of the Illinois algorithm that weighs based
   on ordinate values when one side is not updating, and exhibits superlinear convergence. According to Galdino (2011), this
   is considered to be the state-of-the-art algorithm.
   Taken from: https://tio.run/##7VpbbyI5Fn6vX3FE1BogQAOtfmjU9Gp2plfb0s7OaGb2YZXJIkO5gjeuy9pVoVCL3957fClXFRQQEpKWeoKigMv2ufl8n28lsvn6y8X0YZ8L7wJ@i0MKcyLZAkQcp/2ART6LbiCk6TL2JRBBIRHxHfOpD3PK49UAAFTPH3gsKaRkLmEdZ@DH0XcprEiUQhqDpFS12/2ong/7qJ5/p8SnAibwr4jdUSEJh0UcyRS1ysHBnvYzOV3nD7FPdc9PYcJpSKOUpAyVQhxAuqSNYRvUdX6Kkiw1sSRSZiGGEmM0p3BHOPMH57T2b2iNjtDvVKZHbDyXzp@zFP3Dnj8ymXCydlqpzPjegcGeD85bT6aYBwKmMBwMi88IZZbl0StQeeET4QM2jYWHpi2o1H3eVe0AuYxX8A5sPQlUAJX9Pl2wkHBP5/gUWn@kLejCuOxIwjjDfFfukvkcR1UmZKFCzCL1hFPphSSXKU1U//GwqhQrWJiFYGrnNIgxO1BzyCKdX54XwLT/AXL4nHe7b6APbze2a5BFC9UEVtQAjugBRjs8jAq7K/q9QWux73iDfXQFCr6jxeg4KagZ/klXKQJY2uTwct6DPFOx6sGbqtksYikzsPOZxkEPVkuKtgftnHe6@D/rwHsYevks0kKV59AkApM/o43aVU6hiTgKmMSwIJLKCT6sBmQ0xIgMB6ONd1HzGZ9rp99hRc0JLFYMqkr7iaTLAc2Tdt5BmbokWWRKb7flNzRexBJL99fXzvvjDlrZzi/fdLblj7s5XMLooLSRCtDHHPPVpAqGTUpyQz3NMi2DfBXXAvEqOzQXBMrsKeT/0fkEK5Yu68PRlh24@JzzDSB01K9sA@04UVoI79iHBnw4BMPNKwQ5N2mlQdb6MmeS6ryamcHUjgXoinanBwa2Fr@fPcAKZREfpPEswKJkN9FMuR1cae/zTNdcwwcYwl9ghDTTH2E7nwWBrkZPco4P8M/47yyYIAhvEdEq2wlOFxgiFYQ5gtY3AZI4n8EcuQtRSPy1ympsd0OjBe1BlvjoGYZvHmOYbC96RyO@blll7Y7TmvMWDpymikssiVopq5bQ8ZaxV5ug8Y@EMRoMCrLwFFCM2FcXnw0vbYI/0nsVWvAKrnS0OWpTYXo9rhR6JnKvMf3K@s5A2dYFHNJrrVz/Mw1V9rpHqs/UVLBADRG/VIVr7GsHTkHfNbdtrmE6tY8LvwpSVNmEvzatWq2ZOXQ15mJRJ2iaiciMNgDlsmrC6/HjtFghu8psRZPOM2jco89oi/wylHNBya0KaDF8yBTbA/heocsrOnqHbGqwxwnTDZw97rG3wTmJcElnSSzZKRAPuMFzBekqzYIGmFft1sqgUDaBTKrpGdeWCz3pJbguxTnUUOCcpitKI411C1VcYmkI08rDHk4p9TrbWDEBx5VLhH/6m2JC@8gCBSW4JsxMn41WMInmIqjbnGAX1Qanycrc2wGstAKsYr3Axj4oebHEFXWE8ZMJrjwIL5VlkbHW10bhjKhDoVXVDJBnIiYcG2HGRn0hU3QDzbNZN8BZAJNPF/F3hS1O4qt@yVJCJc11nTME8gmq@ODApdJKp4tqHogCjLSoRkE6yarVBQhqLDveZllB72bay1w8h@ONRC0Mbtpt1NovTFLq8H8jMzfGyDGElaMWYqWIghvuEc0dQXyvoL1xr00A4lFEKfZNAI30uD@Ij2JHUSNFUXLhyeud08gwMKkJJnyFrcP9NIlAriyANE@4slqp43q/3p5JtXF3i52C@c7EJKsl49Ta/X4KNezlYqaGSWWO@@FyqXcaCB0ijKgKLM4HT7vWNDm1jcticC7V@tw8sF91RskwPkbG@Ho3jZ/J2Ccg2icFvDgE@F1PS4qqjcm5gM84Z1HM5FdD/CdrwASWhN/ZHY5ecghY2331C9QfPxNbq/fMxPsR34ishqWF0YGu4gJeOyu0s1gqNFSgt90f@2WvxzZYwgZLP8CS628h80Qkc58wfVNcs9fhJ6acMMY9GKP@7Dm5xxn3k9UOJfHYvVi4XYNURFK0PaFqu4I7mCMMVXCQ2jpFcUlOT3HCMtxa@z8PKe0jA14erWzjPKtVvUD4eSF8LsySCFEocWMw/28sFrfPuVoYVR343toBf9V2HMPuirKbJfabE4l1sTlSsZiVzwLal4XEkYVEqIZYT/QoMMhqz4eDt6pb6A5iDx9qdMNqdLDUuPKoK@QPUdgNt4ZDP3AD8sJz38pSJaE3RGZfb3P0i9H/QnPfBM1pxlFeXaptzVkJzYrmR0W/UNefhLrMDftMkGQpdw509S0OjshskQnRdI@tK1Qg9Y/iPtvpNi859H81wh09pSS6oTsXSWkM@JSK4vYqonkKeeX2KiCcq2/HUZU3feB/GVvc8vX@y6tmpafcXqG8hALyqFOKvVHXGmSIpjVRpA5LjRhnyq0HXI2bji7UCoxX5uc1Jos2uCg/CI8GgnagjTINQ6O2b9XqxNSP9qPxyjR4FHi0hB0A6aeHQXTY3BJLNqYufa3kB6GqNLZuKKLL3Jhurwvs9LYXVoGZtDQlFz/Nj3JRUJheZEEdhLU7ZWPE4btkC6V0FetrJBZnEvLvZCMozwbJR98nH0FkTwEJlxyLpX6hCliKK441OqRefskWt6qaAI/jpBG7KhB17J6C5gpk20ZWNyigayR1A0f2bVdlnz3ujnkrwSwgq4lfbxAUC656KpVEEBQNr0pZx66Zv4L7u3zWFIg/Aa@dYXjPS4VfLLpMF/0etV@8lTvpn/xpeVaet30XfeU2XtfVFzAr98P2dJZFKRW49QFKkB@UUwMntPGVn23JF7D9sk4Tn8ltqUcM3pYKOxfaKhdVEyv32MvK1ctx87KzE51QgQgOpX0ZZ7BzzdcYTBRZnok3umx2m@Up@H0tLd/H3jo2R5dZaq/ykL31TZ59O2iw/6KgZv0FuBP9Q1bf11S3sZ7Arx9/@fj97x9//Me/dw/@vfu9UA4V1wufsyhlXLmNDsuKt/WzhuYRQonuSODACMlbutLT@OljNDHTvRbjx7S8ZKWDfafAO4leHNL2i0PaY6aemkfOVuPiIZMNL/2c0OgRrFTlpcZ91VV1T2Va6NQ0@6RJ8x5FOpm11eSekUdpv9n1XmM4rbAv/wc
   and ported from Ruby to C and modified by a_walz; 3.19.3 */
/*
f := << x -> sin(x^3 - 5) >>;

zeroin := calc.zeroin
zeroab := calc.zeroab
zeros := calc.zeros

watch();
to 100k do
   x := zeroin(f, 1, 2, eps = DoubleEps)
od;
watch():

watch();
to 100k do
   y := zeroab(f, 1, 2, eps = DoubleEps)
od;
watch():

x, y:

watch();
to 100k do
   z := zeros(f, 1, 2, eps = DoubleEps)
od;
watch():

x, y, z:
*/
#define ABMAXITERS 40
static int calc_zeroab (lua_State *L) {
  int nargs, cnt;
  lua_Number epsd, step;
  long double xl, xu, xr, xr_prev, fr_prev, fl, fr, fu, eps, m, d, dbleps, z, origxl, origxr, bailout;
  (void)step;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  xl = origxl = agn_checknumber(L, 2);
  xu = origxr = agn_checknumber(L, 3);
  dbleps = agn_getdblepsilon(L);
  cnt = ABMAXITERS;
  if (aux_checkepsandstep(L, 4, &nargs, &epsd, &step, &cnt, "calc.zeroab")) {  /* user has given explicit `eps' option ? */
    eps = (long double)epsd;
  } else {
    eps = (long double)agn_getepsilon(L);  /* default: Agena's current Eps setting (L->Eps) */
  }
  fl = agnL_fncall(L, 1, xl, 4, nargs);
  fu = agnL_fncall(L, 1, xu, 4, nargs);
  if (fabsl(fl) < eps) {  /* root at endpoints ? 3.19.4 */
    lua_pushnumber(L, xl);
    return 1;
  }
  if (fabsl(fu) < eps) {
    lua_pushnumber(L, xu);
    return 1;
  }
  fr = xr = 0.0L;
  bailout = 1.0L;
  while (cnt-- && bailout > eps) {
    xr_prev = xr;
    fr_prev = fr;
    d = fu - fl;
    if (d == 0.0L) { xr = AGN_NAN; break; }
    xr = (xl*fu - xu*fl)/d;
    fr = agnL_fncall(L, 1, xr, 4, nargs);
    if (fr*fr_prev > 0.0L) {
      cnt--;
      if (fr*fu > 0.0L) {
        m = 1 - fr/fu;
        /* if (m < 0.0L) m = 0.5L; // this sometimes moves the alg. out of the bracketing interval */
        d = fu - fl*m;
        if (d == 0.0L) { xr = AGN_NAN; break; }
        xr = (xl*fu - xu*fl*m)/d;
      } else {
        m = 1 - fr/fl;
        /* if (m < 0.0L) m = 0.5L; // this sometimes moves the alg. out of the bracketing interval */
        d = fu*m - fl;
        if (d == 0.0L) { xr = AGN_NAN; break; }
        xr = (xl*fu*m - xu*fl)/d;
      }
      fr = agnL_fncall(L, 1, xr, 4, nargs);
    }
    if (fr*fu > 0.0L) {
      xu = xr; fu = fr;
    } else {
      xl = xr; fl = fr;
    }
    bailout = fabsl((xr - xr_prev)/xr);
  }
  /* now refine the estimate using regula falsi */
  cnt = ABMAXITERS;  /* 4.11.2 fix */
  z = AGN_NAN;
  while (cnt-- && fabsl(fl - fr) > dbleps) {  /* just comparing against eps produces inaccurate results */
    z = xr - fr*(xr - xl)/(fr - fl);
    fl = fr;
    xl = xr;
    xr = z;
    fr = agnL_fncall(L, 1, xr, 4, nargs);
  }
  if ((z != z && tools_approxl(agnL_fncall(L, 1, xr, 4, nargs), 0.0L, eps)) ||  /* regula falsi loop has not been run OR ... */
    (z >= origxl && z <= origxr))
    lua_pushnumber(L, fabs(xr) < agn_getdblepsilon(L) ? 0 : xr);  /* 4.11.2 extension */
  else
    lua_pushnil(L);
  return 1;
}


/* Purpose: The function seeks a zero of a univariate or multivariate function f over the interval [a, b] using
   Prof. Chandrupatla's algorithm. With multivariate f put its second, third etc. argument right after b.
   There are two options with which you can control accuracy: `eps` and `delta` which are both set to hEps by default.
   To use different values, pass `eps=<positive number>' and/or `delta=<positive number>' as the last arguments.

   Licensing: This code is distributed under the MIT license.
   Author:
     Original QBASIC version by Prof. Tirupathi R. Chandrupatla.
     C version by John Burkardt (18 March 2024), Agena adaption A. Walz 4.11.8
  Reference:
     Tirupathi Chandrupatla, A new hybrid quadratic/bisection algorithm for finding the zero
     of a nonlinear function without using derivatives, Advances in Engineering Software,
     Volume 28, Number 3, pages 145-149, 1997
  Further Reading:
     https://math.stackexchange.com/questions/1520077/explanation-of-chandrupatlas-algorithm-for-root-finding */
static int calc_chandrupatla (lua_State *L) {
  int samples, adjust, nargs, count, iters, use64;
  long double al, a, b, c, d, f0, f1, f2, f3, fh, fl, ph, t, tl, tol, x0, x3, xi, x1, x2, xm, fm, eps,
              xmold, omega, delta, length, xmdiff, xmdiffold;
  (void)omega; (void)length; (void)samples; (void)adjust; (void)iters; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  x1 = agn_checknumber(L, 2);
  x2 = agn_checknumber(L, 3);
  eps = (long double)agn_gethepsilon(L);  /* default: Agena's current hEps setting (L->hEps) */
  delta = eps;  /* 0.00001L; is too coarse */
  iters = ZEROS_MAXITER;
  aux_fcheckoptionslong(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.chandrupatla");
  f1 = agnL_fncall(L, 1, x1, 4, nargs);
  f2 = agnL_fncall(L, 1, x2, 4, nargs);
  if (f1 == 0.0L) {  /* added; if b is a root, the algorithm below will return it. */
    lua_pushnumber(L, x1);
    return 1;
  }
  t = 0.5L;
  count = 0;
  xm = xmold = x1 - 1.0L;
  xmdiff = 1.0L;
  xmdiffold = 0.0L;
  fm = (fabsl(f2) < fabsl(f1)) ? f2 : f1;  /* to avoid GCC warning */
  while (count++ < iters) {  /* added emergency break, may be obsolete */
    x0 = x1 + t*(x2 - x1);
    f0 = agnL_fncall(L, 1, x0, 4, nargs);
    /* arrange 2-1-3: 2-1 Interval, 1 Middle, 3 Discarded point. */
    if ((f0 > 0.0L) == (f1 > 0.0L)) {
      x3 = x1; f3 = f1; x1 = x0; f1 = f0;
    } else {
      x3 = x2; f3 = f2; x2 = x1; f2 = f1; x1 = x0; f1 = f0;
    }
    /* identify the one that approximates zero */
    xmold = xm;
    if (fabsl(f2) < fabsl(f1)) {
      xm = x2; fm = f2;
    } else {
      xm = x1; fm = f1;
    }
    xmdiffold = xmdiff;
    xmdiff = xm - xmold;
    tol = 2.0L*eps*fabsl(xm) + 0.5L*delta;
    tl = tol/fabsl(x2 - x1);
    if (tl > 0.5L || fm == 0.0L || xmdiff == xmdiffold) break;  /* check for non-convergence added */
    /* if inverse quadratic interpolation holds, use it */
    xi = (x1 - x2)/(x3 - x2);
    ph = (f1 - f2)/(f3 - f2);
    fl = 1.0L - sqrtl(1.0L - xi);
    fh = sqrtl(xi);
    if (fl < ph && ph < fh) {
      al = (x3 - x1)/( x2 - x1);
      a = f1/(f2 - f1);
      b = f3/(f2 - f3);
      c = f1/(f3 - f1);
      d = f2/(f3 - f2);
      t = a*b + c*d*al;
    } else {
      t = 0.5L;
    }
    /* adjust T away from the interval boundary */
    t = fMax(t, tl);
    t = fMin(t, 1.0L - tl);
  }
  if (tools_approxl(fm, 0.0L, 2.0L*L->Eps))
    lua_pushnumber(L, fabs(xm) < agn_getdblepsilon(L) ? 0 : xm);  /* 4.11.2 extension */
  else if (count == iters + 1 || xm == xmold)
    lua_pushfail(L);
  else
    lua_pushnil(L);
  return 1;
}


/* Check options for various functions; pos counting from 1 */
static void aux_itpcheckoptions (lua_State *L, int pos, int *nargs,
  long double *eps, long double *k1, long double *k2, int *n0, int *iters, const char *procname) {
  int checkoptions = 5;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streqx(option, "eps", "epsilon", NULL)) {
        *eps = agn_checkpositive(L, -1);
      } else if (tools_streq("k1", option)) {
        *k1 = agn_checknumber(L, -1);
      } else if (tools_streq("k2", option)) {
        *k2 = agn_checknumber(L, -1);
      } else if (tools_streq("n0", option)) {
        *n0 = agn_checknonnegint(L, -1);
      } else if (tools_streq("iters", option)) {
        *iters = agn_checkposint(L, -1);
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

/* Purpose: The function seeks a zero or a pole of a univariate or multivariate function f over the interval [a, b] using
   using the ITP algorithm. With multivariate f put its second, third etc. argument right after b.
   There are two options with which you can control accuracy:
     `eps=<positive number>' : error tolerance between exact and computed roots, set to DoubleEps by default,
     `k1`: a parameter, with suggested value 0.2/(b - a) and set to this formula if not given,
     `k2`: a parameter, typically set to 2,
     `n0`: a parameter that can be set to 0 for difficult problems, but is usually set to 1, to take more advantage of
           the secant method.
   Note that you have to check separately whether the return is a zero or a pole. The function returns `null` if
   sign(f(a, ...)) = sign(f(b, ...)).
   From Wikipedia: "The ITP method, short for Interpolate Truncate and Project, is the first root-finding algorithm that
   achieves the superlinear convergence of the secant method while retaining the optimal worst-case performance of the
   bisection method. It is also the first method with guaranteed average performance strictly better than the bisection
   method under any continuous distribution."
   Licensing: This code is distributed under the MIT license.
   Author: John Burkardt (02 March 2024), extensions by A. Walz 4.11.8 */
static int calc_itp (lua_State *L) {
  long double a, b, c, olda, oldb, epsi, delta, r, s, sigma, xf, xh, xitp, xt, ya, yb, yitp, k1, k2, z;
  int iters, nargs, nh, nmax, n0, cnt;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = olda = agn_checknumber(L, 2);
  b = oldb = agn_checknumber(L, 3);
  if (b < a) {
    c = a; a = b; b = c;
  }
#ifndef __ARMCPU
  epsi = agn_getdblepsilon(L);  /* default: Agena's current DoubleEps setting */
#else
  epsi = agn_gethepsilon(L);  /* algorithm won't finish otherwise on ARM CPUs */
#endif
  k1 = 0.2L/(b - a);
  k2 = 2.0L;
  n0 = 1; /* use secant method */
  iters = ZEROS_MAXITER;
  aux_itpcheckoptions(L, 4, &nargs, &epsi, &k1, &k2, &n0, &iters, "calc.itp");
  ya = agnL_fncall(L, 1, a, 4, nargs);
  yb = agnL_fncall(L, 1, b, 4, nargs);
  if (ya == 0.0L || tools_isnan(ya)) {  /* added; if the right border b is a root, the algorithm below will return it. 4.12.1 fix */
    lua_pushnumber(L, a);
    return 1;
  } else if (yb == 0.0L || tools_isnan(yb)) {  /* 4.12.1 extension */
    lua_pushnumber(L, b);
    return 1;
  }
  if (ya*yb > 0.0L) {  /* no change of sign */
    lua_pushnil(L);
    return 1;
  }
  /* modify f(x) so that y(a) < 0, 0 < y(b) */
  if (0.0 < ya) {
    s = -1.0L;
    ya = - ya;
    yb = - yb;
  } else {
    s = +1.0L;
  }
  nh = sun_ceill(tools_log2l((b - a)/2.0L/epsi));
  nmax = nh + n0;
  cnt = 0;
  while (2.0L*epsi < (b - a) && cnt < iters) {
    /* calculate parameters */
    xh = 0.5L*(a + b);
    r = epsi*tools_intpowl(2.0L, nmax - cnt++) - 0.5L*(b - a);
    delta = k1*tools_powl(b - a, k2);
    /* interpolation */
    xf = (yb*a - ya*b)/(yb - ya);
    /* truncation */
    sigma = (0.0L <= xh - xf) ? +1.0L : -1.0L;
    xt = (delta < fabsl(xh - xf)) ? xf + sigma*delta : xh;
    /* projection */
    xitp = (fabsl(xt - xh) <= r) ? xt : xh - sigma*r;
    /* update the interval */
    yitp = s*agnL_fncall(L, 1, xitp, 4, nargs);
    if (yitp > 0.0L) {
      b = xitp; yb = yitp;
    } else if (yitp < 0.0L) {
      a = xitp; ya = yitp;
    } else {  /* yitp can now also be nan */
      a = xitp;
      b = xitp;
      break;
    }
  }
  z = 0.5L*(a + b);
  if (tools_approxl(z, oldb, L->Eps) && agnL_fncall(L, 1, oldb, 4, nargs) == 0.0L)  /* added */
    lua_pushnumber(L, (lua_Number)oldb);
  else if (olda <= z && z <= oldb)
    lua_pushnumber(L, fabs(z) < agn_getdblepsilon(L) ? 0 : z);  /* 4.11.2 extension */
  else  /* just in case ..., 4.12.1 */
    lua_pushnil(L);
  return 1;
}


/* Purpose: `calc.brent` seeks the root of a function f(x) in an interval [a, b].
   Discussion:
     The interval [a, b] must be a change of sign interval for f. That is, f(a) and f(b) must be of opposite signs.
     Then assuming that F is continuous implies the existence of at least one value C between A and B for which
     f(c) = 0.
     The location of the zero is determined to within an accuracy of 4*EPSILON*abs(c) + 2*t.
     Thanks to Thomas Secretin for pointing out a transcription error in the setting of the value of P, 11 February 2013.
   Licensing: This code is distributed under the MIT license.
   Author: Original FORTRAN77 version by Richard Brent. This version by John Burkardt (27 March 2024).
   Remark: The algorithm actually is a variant of the algorithm designed by G. Forsythe, M. Malcolm and C. Moler, implemented
     with `calc.zeroin`.
   Reference:
     Richard Brent, Algorithms for Minimization Without Derivatives,
     Dover, 2002, ISBN: 0-486-41998-3, LC: QA402.5.B74.
   Input:
     double a, b: the endpoints of the change of sign interval.
     double t: a positive error tolerance.
     double f(double x); a function whose zero is being sought.
   Output:
     double zero_brent: the estimated zero of the function F. */
static int calc_brent (lua_State *L) {
  int nargs, n0, iters;
  long double a, b, c, d, e, t, fa, fb, fc, m, p, q, r, s, sa, sb, tol, k1, k2;
  (void)k1; (void)k2; (void)n0;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (b < a) {
    c = a; a = b; b = c;
  }
#ifndef __ARMCPU
  t = agn_getdblepsilon(L);  /* default: Agena's current DoubleEps setting */
#else
  t = agn_gethepsilon(L);  /* algorithm won't finish otherwise on ARM CPUs */
#endif
  iters = ZEROS_MAXITER;
  aux_itpcheckoptions(L, 4, &nargs, &t, &k1, &k2, &n0, &iters, "calc.brent");
  /* make local copies of A and B */
  sa = a;
  fa = agnL_fncall(L, 1, sa, 4, nargs);
  if (fa == 0.0L) {  /* added; if the right border b is a root, the algorithm below will return it. */
    lua_pushnumber(L, a);
    return 1;
  }
  sb = b;
  fb = agnL_fncall(L, 1, sb, 4, nargs);
  if (fa*fb > 0.0L) {
    lua_pushnil(L);
    return 1;
  }
  c = sa;
  fc = fa;
  e = sb - sa;
  d = e;
  while (iters--) {
    if (fabsl(fc) < fabsl(fb)) {
      sa = sb; sb = c;  c  = sa;
      fa = fb; fb = fc; fc = fa;
    }
    tol = 2.0L*DBL_EPSILON*fabsl(sb) + t;
    m = 0.5L*(c - sb);
    if (fabsl(m) <= tol || fb == 0.0L) break;
    if (fabsl(e) < tol || fabsl(fa) <= fabsl(fb)) {
      e = m;
      d = e;
    } else {
      s = fb/fa;
      if (sa == c) {
        p = 2.0L*m*s;
        q = 1.0L - s;
      } else {
        q = fa/fc;
        r = fb/fc;
        p = s*(2.0L*m*q*(q - r) - (sb - sa)*(r - 1.0L));
        q = (q - 1.0L)*(r - 1.0L)*(s - 1.0L);
      }
      if (0.0L < p) {
        q = -q;
      } else {
        p = -p;
      }
      s = e;
      e = d;
      if (2.0L*p < 3.0L*m*q - fabsl(tol*q) && p < fabsl(0.5L*s*q)) {
        d = p/q;
      } else {
        e = m;
        d = e;
      }
    }
    sa = sb;
    fa = fb;
    if (tol < fabsl(d)) {
      sb = sb + d;
    } else if (0.0L < m) {
      sb = sb + tol;
    } else {
      sb = sb - tol;
    }
    fb = agnL_fncall(L, 1, sb, 4, nargs);
    /* if ((0.0L < fb && 0.0L < fc) || (fb <= 0.0L && fc <= 0.0L)) { */
    if (fb*fc >= 0.0L) {
      c = sa;
      fc = fa;
      e = sb - sa;
      d = e;
    }
  }
  lua_pushnumber(L, fabs(sb) < agn_getdblepsilon(L) ? 0 : sb);  /* 4.11.2 extension */
  return 1;
}


/* Compute the symmetric derivative of a function f at a point x.
   The algorithm is based on Conte and de Boor's `Coefficients of Newton form of polynomial of degree 3`;
   written June 09, 2008, 0.11.2. The function must be at index 1.

   DO NOT MIGRATE TO LONG DOUBLES due to results less precise than with doubles (division by h) ! */
#define DIVDIFFTBLITERS 4
lua_Number aux_diff (lua_State *L, lua_Number x, lua_Number eps, int deriv, int nargs, lua_Number *abserr) {  /* 2.10.2 */
  lua_Number h, q, fph, fmh, a[4], b[4], f2ph, f2mh, f3ph, f3mh, f4ph, f4mh, h2;
  int i, j;
  /* luaL_checkstack is called by agnL_fncall */
  if (deriv == 0) {  /* 2.14.1 */
    *abserr = 0;
    return agnL_fncall(L, 1, x, 3, nargs);
  }
  q = 0;
  for (i=0; i < 4; i++) {  /* determine appropriate epsilon step size */
    a[i] = x + (i - 2)*eps;  /* patched 2.10.1 */
    b[i] = agnL_fncall(L, 1, a[i], 3, nargs);  /* 2.10.1 */
  }
  for (j=0; j < 4; j++) {  /* compute divided difference table */
    for (i=0; i < 3 - j; i++) {
      b[i] = (b[i + 1] - b[i])/(a[i + j + 1] - a[i]);
    }
  }
  for (i=0; i < 4; i++) q += b[i];
  q = fabs(q);
  if (deriv < 2 && q < 100*eps) q = 100*eps;  /* for second and higher derivative, results would be imprecise */
  h = sun_cbrt(eps/(2*q));  /* 3.3.4 change */
  if (deriv < 2 && h > 100*eps) h = 100*eps;  /* for second and higher derivative, results would be imprecise */
  *abserr = fabs(h*h*q);  /* absolute error, previously: fabs(h*h*q*100) */
  fph = agnL_fncall(L, 1, x + h, 3, nargs);  /* 2.10.1 */
  fmh = agnL_fncall(L, 1, x - h, 3, nargs);  /* 2.10.1 */
  /* see: https://en.wikipedia.org/wiki/Finite_difference_coefficient */
  if (deriv == 1) {
    return (fph - fmh)/(2*h);  /* symmetric difference quotient, accuracy is much better than with 8-point diffquot */
  }
  f2ph = agnL_fncall(L, 1, x + 2*h, 3, nargs);
  f2mh = agnL_fncall(L, 1, x - 2*h, 3, nargs);
  f3ph = agnL_fncall(L, 1, x + 3*h, 3, nargs);
  f3mh = agnL_fncall(L, 1, x - 3*h, 3, nargs);
  if (deriv == 2) {  /* second derivative, new 2.10.2; improved 4-fold 3.3.4 */
    lua_Number f = agnL_fncall(L, 1, x, 3, nargs);
    return (f3mh - 13.5*f2mh + 135*fmh - 245*f + 135*fph - 13.5*f2ph + f3ph)/(90*h*h);   /* new 2.10.2 */
    /* 8-points has lower quality:
       return (-f4mh + 8*560/315*f3mh - 560/5*f2mh + 896*fmh - 205*560/72*f + 896*fph - 560/5*f2ph + 8*560/315*f3ph - f4ph)/(560*h*h); */
  }
  f4ph = agnL_fncall(L, 1, x + 4*h, 3, nargs);
  f4mh = agnL_fncall(L, 1, x - 4*h, 3, nargs);
  h2 = h*h;
  if (deriv == 3) {  /* third derivative, new 2.10.2, slightly improved 3.3.4 */
    /* return (f3mh - 8*f2mh + 13*fmh - 13*fph + 8*f2ph - f3ph)/(8*h2*h); */
    return (-7*f4mh + 72*f3mh - 338*f2mh + 488*fmh - 488*fph + 338*f2ph - 72*f3ph + 7*f4ph)/(240*h2*h);
  } else if (deriv == 4) {  /* fourth derivative, new 3.3.4 */
    lua_Number f = agnL_fncall(L, 1, x, 3, nargs);
    return (f2mh - 4*fmh + 6*f - 4*fph + f2ph)/(h2*h2);
    /* return (-f3mh + 12*f2mh - 39*fmh + 56*f - 39*fph + 12*f2ph -f3ph)/(6*h2*h2);
    return (7*f4mh - 96*f3mh + 676*f2mh - 1952*fmh + 2730*f - 1952*fph + 676*f2ph - 96*f3ph + 7*f4ph)/(240*h2*h2); */
  }
  /* f5ph = agnL_fncall(L, 1, x + 5*h, 3, nargs);
     f5mh = agnL_fncall(L, 1, x - 5*h, 3, nargs); */
  /* fifth derivative, new 3.3.4 */
  return (-f3mh + 4*f2mh - 5*fmh + 5*fph - 4*f2ph + f3ph)/(2*h2*h2*h);
    /* return (f4mh - 9*f3mh + 26*f2mh - 29*fmh + 29*fph - 26*f2ph + 9*f3ph - f4ph)/(6*h2*h2*h);
       return (-13*f5mh + 152*f4mh - 783*f3mh + 1872*f2mh - 1938*fmh + 1938*fph - 1872*f2ph + 783*f3ph - 152*f4ph + 13*f5ph)/(288*h2*h2*h); */
  /* } else {  sixth derivative - way too imprecise
    lua_Number f = agnL_fncall(L, 1, x, 3, nargs);
    return (f3mh - 6*f2mh + 15*fmh - 20*f + 15*fph - 6*f2ph + f3ph)/(h2*h2*h2);
    return (13*f5mh - 190*f4mh + 1305*f3mh - 4680*f2mh + 9690*fmh - 12276*f + 9690*fph - 4680*f2ph + 1305*f3ph - 190*f4ph + 13*f5ph)/(240*h2*h2*h2);
  } */
}

static int calc_diff (lua_State *L) {
  lua_Number x, eps, delta, step, abserr;
  int sided, deriv, nargs;
  (void)sided; (void)step;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  x = agn_checknumber(L, 2);
  nargs = lua_gettop(L);
  aux_geteps(L, x, 3, &nargs, &eps, &delta, &step, &sided, &deriv, 0, 0, 5, 1, "calc.diff");
  lua_pushnumber(L, aux_diff(L, x, eps, deriv, nargs, &abserr));  /* externalised 2.10.2 */
  lua_pushnumber(L, abserr);  /* absolute error */
  return 2;
}


/*
Differentiation using Richardson`s extrapolation, taken from http://netlib.org/textbook/mathews/chap6.f

NUMERICAL METHODS: FORTRAN Programs, (c) John H. Mathews 1994, used by kind permission.

Author of the original FORTRAN routine:
Prof. John  H.  Mathews
Department of Mathematics
California State University Fullerton
Fullerton, CA  92634

The FORTRAN routine accompanies the book `NUMERICAL METHODS for Mathematics, Science and Engineering`,
2nd Ed, 1992, Prentice Hall, Englewood Cliffs, New Jersey, 07632, U.S.A.

Algorithm 6.2 (Differentiation Using Extrapolation).
Section 6.1, Approximating the Derivative, Page 327
Agena port January 01, 2010, 0.30.1

DO NOT MIGRATE TO LONG DOUBLES due to results less precise than with doubles (division by h) ! */

#define MAX_N 2*15  /* original value has been 15 */
static int calc_xpdiff (lua_State *L) {  /* 0.32.3 */
  lua_Number fx, fph, fmh, f2ph, f2mh, f3ph, f3mh, x, d[MAX_N + 1][MAX_N + 1],
    eps, delta, err, h, relerr, firstrelerr, small, abserr, step;
  int j, k, n, sided, deriv, nargs, stencil7;
  (void)sided; (void)step;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  /* luaL_checkstack is called by agnL_fncall */
  x = agnL_checknumber(L, 2);
  nargs = lua_gettop(L);
  aux_geteps(L, x, 3, &nargs, &eps, &delta, &step, &sided, &deriv, 0, 0, 3, 1, "calc.xpdiff");
  fx = agnL_fncall(L, 1, x, 3, nargs);
  if (deriv == 0) {  /* 2.14.1 */
    lua_pushnumber(L, fx);
    return 1;
  } else if (!tools_isfinite(fx)) {  /* infinity or undefined ? */
    lua_pushundefined(L);
    return 1;
  }
  stencil7 = 1;  /* default method for third derivative */
  small = 1e-7;
  h = 1.0;
  n = 0;
  err = 1.0;
  relerr = 1.0;
  for (j=0; j < 10; j++) {  /* 2.10.2, avoid `undefined` */
    fph = agnL_fncall(L, 1, x + h, 3, nargs);  /* 2.10.1; h and not eps */
    fmh = agnL_fncall(L, 1, x - h, 3, nargs);  /* 2.10.1; h and not eps */
    if (tools_isfinite(fph) && tools_isfinite(fmh)) break;
    h /= 2.0;
  }
  if ( (deriv != 3) && (!tools_isfinite(fph) || !tools_isfinite(fmh)) ) {  /* changes 2.16.1 due to GCC warning */
    /* fallback, as call to calc.diff often computes a reliable result */
    lua_pushnumber(L, aux_diff(L, x, eps, deriv, nargs, &abserr));
    lua_pushnumber(L, abserr);
    return 2;
  }
  if (deriv == 1)
    d[0][0] = 0.5*(fph - fmh)/h;
  else if (deriv == 2)
    d[0][0] = (fph - 2.0*fx + fmh)/(h*h);  /* new 2.10.2, formula taken from Wikipedia, 'Symmetric derivative' */
  else {  /* third derivative */
    f2ph = agnL_fncall(L, 1, x + 2.0*h, 3, nargs);
    f2mh = agnL_fncall(L, 1, x - 2.0*h, 3, nargs);
    f3ph = agnL_fncall(L, 1, x + 3.0*h, 3, nargs);
    f3mh = agnL_fncall(L, 1, x - 3.0*h, 3, nargs);
    if (tools_isfinite(f2ph) && tools_isfinite(f2mh) && (!tools_isfinite(f3ph) || !tools_isfinite(f3mh)))
      stencil7 = 0;
    else if (!tools_isfinite(f2ph) || !tools_isfinite(f2mh) || !tools_isfinite(f3ph) || !tools_isfinite(f3mh)) {
      /* fallback, as call to calc.diff often computes a reliable result */
      lua_pushnumber(L, aux_diff(L, x, eps, deriv, nargs, &abserr));
      lua_pushnumber(L, abserr);
      return 2;
    }
    /* new 2.10.2, 7-stencil, www.holoborodko.com/pavel/numerical-methods/numerical-derivative/central-differences/
       by Pavel Holoborodko */
    if (stencil7)
      d[0][0] = (f3mh - 8.0*f2mh + 13.0*fmh - 13.0*fph + 8.0*f2ph - f3ph)/(8.0*h*h*h);
    else  /* use 5-stencil method, also published on Pavel Holoborodko's site */
      d[0][0] = (-f2mh + 2.0*fmh - 2.0*fph + 2.0*f2ph)/(2.0*h*h*h);
  }
  firstrelerr = relerr;
  for (j=1; (j <= MAX_N) && (relerr > eps) && (err > delta); j++) {
    h /= 2.0;
    fph = agnL_fncall(L, 1, x + h, 3, nargs);  /* 2.10.1 */
    fmh = agnL_fncall(L, 1, x - h, 3, nargs);  /* 2.10.1 */
    if (deriv == 1)
      d[j][0] = 0.5*(fph - fmh)/h;
    else if (deriv == 2)
      d[j][0] = (fph - 2.0*fx + fmh)/(h*h);  /* new 2.10.2 */
    else {  /* third derivative, new 2.10.2 */
      f2ph = agnL_fncall(L, 1, x + 2.0*h, 3, nargs);
      f2mh = agnL_fncall(L, 1, x - 2.0*h, 3, nargs);
      if (stencil7) {
        f3ph = agnL_fncall(L, 1, x + 3.0*h, 3, nargs);
        f3mh = agnL_fncall(L, 1, x - 3.0*h, 3, nargs);
        d[j][0] = (f3mh - 8.0*f2mh + 13.0*fmh - 13.0*fph + 8.0*f2ph - f3ph)/(8.0*h*h*h);
      } else {
        d[j][0] = (-f2mh + 2.0*fmh - 2.0*fph + 2.0*f2ph)/(2.0*h*h*h);
      }
    }
    for (k=1; k <= j; k++) {
      d[j][k] = d[j][k - 1] + (d[j][k - 1] - d[j - 1][k - 1])/(luai_numipow(4.0, k) - 1);  /* 2.29.5 tuning */
    }
    err = fabs(d[j][j] - d[j - 1][j - 1]);
    relerr = 2*err/(fabs(d[j][j]) + fabs(d[j - 1][j - 1]) + small);
    if (j == 1) firstrelerr = relerr;
    n = j;
  }
  if (deriv > 2 && relerr > 0.5*firstrelerr) {  /* 4.2.5 */
    /* something may have gone wrong numerically with the third derivative, revert to calc.diff */
    lua_pushnumber(L, aux_diff(L, x, eps, deriv, nargs, &abserr));
    lua_pushnumber(L, abserr);
  } else {
    lua_pushnumber(L, d[n][n]);
    lua_pushnumber(L, err);
  }
  return 2;
}


/* Computes a mathematical epsilon value that takes into account the magnitude of the value of function
   f at point x. By default, r = 3 points around (and including) the centre x are used to calculate
   the result. You may choose any other positive integer for r with r < 100.

   The optional fourth argument `adjust` - a Boolean - controls whether the function shall adjust the result if the
   internal epsilon value computed has become too large to be probably of any use. By default no adjustment takes place.

   The second return, a Boolean, indicates whether the function adjusted the result (only if `adjust` has been set
   to true).

   The function is useful to analyse functions whose values are very close to zero and where floating point arithmetic
   may lead to fatal round-off errors as it returns an epsilon value that is larger than the one returned by
   `math.eps`.

   DO NOT MIGRATE TO LONG DOUBLES due to results less precise than with doubles (division by h) ! */

static int calc_eps (lua_State *L) {  /* 2.9.8 */
  lua_Number eps, h, x, a[100], b[100];
  volatile double q, cs, ccs;
  long double epsilon, omega, delta, length;
  int nargs, i, j, n, adjust, adjusted, samples, iters, use64;
  (void)omega; (void)epsilon; (void)delta; (void)iters; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  x = agn_checknumber(L, 2);
  samples = 3;  /* 3.12.0 `samples` option */
  adjust = 0;   /* 3.12.0 `adjust` option */
  aux_fcheckoptionslong(L, 3, &nargs, &epsilon, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.eps");  /* 3.12.0 change */
  if (samples > 99)
    luaL_error(L, "Error in " LUA_QS ": third argument %d too big.", "calc.eps", samples);
  eps = tools_matheps(x);  /* get mathematical epsilon */
  q = cs = ccs = 0;
  n = samples + 1;
  for (i=0; i < n; i++) {
    a[i] = x + (i - (int)(samples/2))*eps;  /* compute n points around the centre */
    b[i] = agnL_fncall(L, 1, a[i], 3, nargs);  /* 3.12.0 change for multivariate support */
  }
  for (j=0; j < n; j++) {
    for (i=0; i < samples - j; i++) {
      b[i] = (b[i + 1] - b[i])/(a[i + j + 1] - a[i]);
    }
  }
  for (i=0; i < n; i++) q = tools_kbadd(q, b[i], &cs, &ccs);
  q = fabs(q + cs + ccs);
  if ((adjusted = adjust && q < 100*eps)) q = 100*eps;
  h = sun_pow(eps/((samples - 1)*q), 1.0/samples, 0);
  if ((adjusted = adjust && h > 100*eps)) h = 100*eps;
  lua_pushnumber(L, h);  /* `ordinate` epsilon */
  lua_pushboolean(L, adjusted);
  return 2;
}


/* Estimates the minimum location of a univariate function through one-dimensional search over a given range [a, b] using "Golden section"
   combined with parabolic interpolation. Returns the _abscissa_ (x-axis) value where a minimum has been found. The algorithm has been
   described in:

   G.Forsythe, M.Malcolm, C.Moler, `Computer Methods for Mathematical Computations`. M., Mir, 1980, p.202 of the
   Russian edition.

   f:  function under investigation
   a:  left border of the range the min is seeked
   b:    right border of the range the min is seeked
   tol:  (optional) acceptable tolerance */

/* Computes both minima and maxima, controlled by parameter `sign`:
   minima: sign = +1,
   maxima: sign = -1
   3.12.1 */
static int aux_minmaxbr (lua_State *L, int nargs, lua_Number sign, lua_Number a, lua_Number b, lua_Number tol, const char *procname) {
  long double x, v, w, fv, fx, fw, r, eps;
  r = PHIINVSQld;  /* = (3 - sqrt(5))/2; Golden section ratio, 2.17.7 tweak; changed from PHI-1 back to (3 - sqrt(5))/2 2.21.4 */
  if (!(tol > 0 && b > a))
    luaL_error(L, "Error in " LUA_QS ": tolerance <= 0 or a <= b.", procname);
  v = a + r*(b - a);
  fv = sign*agnL_fncall(L, 1, v, 4, nargs);  /* first step - always gold section, 3.12.0 change for multivariate support */
  x = v; w = v;
  fx = fv; fw = fv;
  /* 2.1.3 optimisation, a and b might be changed in this loop, 2.21.8 change, 2.21.9 change; changing this to eps = tol
     does not improve the result. */
  eps = agn_getepsilon(L);
  while (1) {  /* main iteration loop */
    long double range, middle_range, tol_act, new_step, t, fa, fb, ft;
    range = b - a;           /* range over which the minimum is seeked for */
    middle_range = 0.5L*(a + b);  /* 2.17.7 tweak */
    tol_act = fmal(eps, fabsl(x), tol/3.0L);  /* actual tolerance; new_step: step at this iteration; 4.12.6 change */
    if (fmal(0.5L, range, fabsl(x - middle_range)) <= 2.0L*tol_act) {  /* 2.17.7/4.12.6 tweak */
      fa = sign*agnL_fncall(L, 1, a, 4, nargs);  /* 3.12.0 change for multivariate support */
      fb = sign*agnL_fncall(L, 1, b, 4, nargs);
      fx = sign*agnL_fncall(L, 1, x, 4, nargs);
      if (fa < fx) {  /* the function is not that good if the minimum is exactly at the left or right border */
        lua_pushnumber(L, a);   /* 0.29.4 patch, 28.12.2009 */
      } else if (fb < fx) {
        lua_pushnumber(L, b);
      } else {
        lua_pushnumber(L, x);  /* acceptable approximation is found */
      }
      return 1;
    }
    /* obtain the gold section step */
    new_step = r*(x < middle_range ? b - x : a - x);
    /* decide if the interpolation can be tried */
    if (fabsl(x - w) >= tol_act) {  /* if x and w are distinct interpolatiom may be tried */
      long double p, q, t;       /* interpolation step is calculated as p/q; division operation is delayed */
                                 /* until last moment */
      t = (x - w)*(fx - fv);
      q = (x - v)*(fx - fw);
      p = (x - v)*q - (x - w)*t;
      q = 2.0L*(q - t);
      if (q > 0.0L)     /* q was calculated with the op- */
        p = -p;         /* posite sign; make q positive */
      else              /* and assign possible minus to */
        q = -q;         /* p */
      if (fabsl(p) < fabsl(new_step*q) &&  /* If x+p/q falls in [a,b] */
        p > q*(a - x + 2.0L*tol_act) &&    /* not too close to a and */
        p < q*(b - x - 2.0L*tol_act)) {    /* b, and isn't too large */
        new_step = p/q;                    /* it is accepted */
      }
      /* If p/q is too large then the gold section procedure can reduce [a,b] range to more extent  */
    }
    if (fabsl(new_step) < tol_act) {  /* adjust the step to be not less */
      if (new_step > 0.0L)            /* than tolerance */
        new_step = tol_act;
      else
        new_step = -tol_act;
    }
    /* obtain the next approximation to min and reduce the enveloping range */
    t = x + new_step;  /* tentative point for the min */
    ft = sign*agnL_fncall(L, 1, t, 4, nargs);  /* 3.12.0 change for multivariate support */
    if (ft <= fx) {    /* t is a better approximation */
      if (t < x)       /* Reduce the range so that */
        b = x;         /* t would fall within it */
      else
        a = x;
      v = w; w = x; x = t;  /* assign the best approx to x */
      fv = fw; fw = fx; fx = ft;
    } else {      /* x remains the better approx */
      if (t < x)  /* reduce the range enclosing x */
        a = t;
      else
        b = t;
      if (ft <= fw || w == x) {
        v = w; w = t;
        fv = fw; fw = ft;
      } else if (ft <= fv || v == x || v == w) {
        v = t;
        fv = ft;
      }
    }
  }
}

static int calc_fminbr (lua_State *L) {
  long double a, b, tol, omega, delta, length;  /* Abscissae, descr. see above, f(x), f(v), f(w), r */
  int samples, adjust, nargs, iters, use64;
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)iters; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  tol = agn_getepsilon(L);  /* 3.12.1 change from AGN_EPSILON to agn_getepsilon */
  aux_fcheckoptionslong(L, 4, &nargs, &tol, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.fminbr");  /* 3.12.0 change */
  return aux_minmaxbr(L, nargs, 1.0, a, b, tol, "calc.fminbr");
}

static int calc_fmaxbr (lua_State *L) {
  long double a, b, tol, omega, delta, length;  /* Abscissae, descr. see above, f(x), f(v), f(w), r */
  int samples, adjust, nargs, iters, use64;
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)iters; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  tol = agn_getepsilon(L);  /* 3.12.1 change from AGN_EPSILON to agn_getepsilon */
  aux_fcheckoptionslong(L, 4, &nargs, &tol, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.fmaxbr");
  return aux_minmaxbr(L, nargs, -1.0, a, b, tol, "calc.fmaxbr");
}


/* Performs Golden section search for the minimum of a univariate function.

   Given a function f with a single local minimum in the interval [a, b], with a, b numbers, returns the _abscissa_
   value (x-axis) where a minimum has been found.

   Algorithm taken from https://en.wikipedia.org/wiki/Golden-section_search */
static int aux_minmaxgs (lua_State *L, int nargs, lua_Number sign, lua_Number a, lua_Number b, const char *procname) {
  int i, n;
  long double c, d, yc, yd, eps, h;
  eps = luai_numipow(agn_getepsilon(L), 2);
  h = b - a;
  if (h <= eps) {
    lua_pushnumber(L, (sign*agnL_fncall(L, 1, a, 4, nargs) < sign*agnL_fncall(L, 1, b, 4, nargs)) ? a : b);
    return 1;
  }
  /* Required steps to achieve tolerance */
  n = sun_ceill(tools_logl(eps/h)/tools_logl(PHIINVld));
  c = a + h*PHIINVSQld;
  d = a + h*PHIINVld;
  yc = sign*agnL_fncall(L, 1, c, 4, nargs);  /* 3.12.0 change for multivariate support */
  yd = sign*agnL_fncall(L, 1, d, 4, nargs);
  for (i=0; i < n; i++) {
    if (yc < yd) {
      b = d;
      d = c;
      yd = yc;
      h = PHIINVld*h;
      c = a + PHIINVSQld*h;
      yc = sign*agnL_fncall(L, 1, c, 4, nargs);  /* 3.12.0 change for multivariate support */
    } else {
      a = c;
      c = d;
      yc = yd;
      h = PHIINVld*h;
      d = a + PHIINVld*h;
      yd = sign*agnL_fncall(L, 1, d, 4, nargs);  /* 3.12.0 change for multivariate support */
    }
  }
  if (yc < yd) {
    lua_pushnumber(L, (sign*agnL_fncall(L, 1, a, 4, nargs) <= sign*agnL_fncall(L, 1, d, 4, nargs)) ? a : d);
  } else {
    lua_pushnumber(L, (sign*agnL_fncall(L, 1, b, 4, nargs) <= sign*agnL_fncall(L, 1, c, 4, nargs)) ? b : c);
  }
  return 1;
}

static int calc_fmings (lua_State *L) {
  long double a, b, omega, length, delta, tol;
  int samples, adjust, nargs, iters, use64;
  (void)omega; (void)length; (void)tol; (void)samples; (void)adjust; (void)delta; (void)iters; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (b <= a)
    luaL_error(L, "Error in " LUA_QS ": left border must be less than right border.", "calc.fmings");
  aux_fcheckoptionslong(L, 4, &nargs, &tol, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.fmings");  /* 3.12.0 change */
  return aux_minmaxgs(L, nargs, 1.0, a, b, "calc.fmings");
}

static int calc_fmaxgs (lua_State *L) {
  long double a, b, omega, delta, length, tol;
  int samples, adjust, nargs, iters, use64;
  (void)omega; (void)length; (void)tol; (void)samples; (void)adjust; (void)delta; (void)iters; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (b <= a)
    luaL_error(L, "Error in " LUA_QS ": left border must be less than right border.", "calc.fmaxgs");
  aux_fcheckoptionslong(L, 4, &nargs, &tol, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.fmaxgs");  /* 3.12.0 change */
  return aux_minmaxgs(L, nargs, -1.0, a, b, "calc.fmaxgs");
}

/* takes a table or sequence of pairs of numbers and stores them to the arrays x and y */
static void agn_toarrays (lua_State *L, int idx, const char *procname, size_t n, long double *x, long double *y) {
  size_t i;
  if (lua_type(L, idx) == LUA_TTABLE) {
    i = 0;
    lua_pushnil(L);
    while (lua_next(L, idx)) {
      agnL_pairgetilongnumbers(L, procname, -1, &(x[i]), &(y[i])); i++;
    }
  }
  else {
    for (i=0; i < n; i++) {
      lua_seqrawgeti(L, idx, i + 1);  /* push object */
      agnL_pairgetilongnumbers(L, procname, -1, &(x[i]), &(y[i]));
    }
  }
}


int nevillef (lua_State *L) {  /* 2.2.0 RC 3/2.34.1 */
  int n;
  long double t;  /* changed 2.34.10 */
  LongDoubleArray *x, *y;
  t = agn_checknumber(L, 1);
  n = agn_tonumber(L, lua_upvalueindex(3));
  x = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(1));
  y = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(2));
  lua_pushnumber(L, dneville(n, x, y, t));
  return 1;
}

static int calc_neville (lua_State *L) {
  size_t n, i;
  long double t, *x, *y;  /* changed 2.34.10 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE || lua_type(L, 1) == LUA_TSEQ, 1, "table or sequence of pairs expected");  /* 2.35.2 fix */
  n = agn_nops(L, 1);
  if (n < 1)  /* 2.35.2 fix */
    luaL_error(L, "Error in " LUA_QS ": sample is empty.", "calc.neville");
  agn_createlongarray(x, n, "calc.neville");
  agn_createlongarray(y, n, "calc.neville");
  agn_toarrays(L, 1, "calc.neville", n, x, y);
  if (lua_gettop(L) > 1) {
    t = agn_checknumber(L, 2);
    lua_pushnumber(L, neville(n, x, y, t));
  } else {  /* 2.2.0 RC 3, tuned 2.34.1 */
    LongDoubleArray *a, *b;
    luaL_checkstack(L, 3, "not enough stack space");  /* 3.18.4 fix */
    aux_pushud(L, a, n, "calc.neville");  /* push as ud */
    aux_pushud(L, b, n, "calc.neville");  /* push as ud */
    for (i=0; i < n; i++) {
      setldblarray(a, i, x[i]);
      setldblarray(b, i, y[i]);
    }
    lua_pushnumber(L, n);  /* push number of elements */
    lua_pushcclosure(L, &nevillef, 3);
  }
  xfreeall(x, y);  /* 1.12.9 fix, 2.9.8 */
  return 1;
}


/* calc.polyfit(obj, n);

   Returns a sequence of coefficients of an nth-degree polynomial of a sample, in order of descending degree fitting the
   input sequence or sequence obj of pairs x[k]:y[k], with x[k] and y[k] being numbers, and using polynomial regression.
   The degree n must be a positive integer.

   The return may be passed to `calc.polygen` to generate a polynomial function (use `unpack` when passing the coefficient
   vector), e.g. calc.polygen( unpack( calc.polyfit(seq( 1:0, 1:3, 2:1 ), 2 ) ) ).

   For the original C source file see: http://programbank4u.blogspot.de/2013/04/polynomial-regression-c-program.html,
   posted 11th April 2013 by Harika. */

static int calc_polyfit (lua_State *L) {  /* 2.1.3 */
  size_t n, c;
  int m, i, j, k;
#ifndef __ARMCPU  /* 2.37.1 */
  long double *x, *y, *a, *b, sum, temp, q;  /* changed 2.34.10 */
#else
  long double *x, *y, *a, *b, sum, temp;  /* changed 2.34.10 */
  double q;
#endif
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE || lua_type(L, 1) == LUA_TSEQ, 1, "table or sequence of pairs expected");  /* 2.35.2 fix */
  n = agn_nops(L, 1);
  m = agn_checkinteger(L, 2);
  if (m >= n)  /* This if/else statement intercepts empty samples */
    luaL_error(L, "Error in " LUA_QS ": too few items in structure for given degree %d.", "calc.polyfit", m);
  else if (m < 1)
    luaL_error(L, "Error in " LUA_QS ": degree must be positive.", "calc.polyfit");
  agn_createlongarray(x, n, "calc.polyfit");
  agn_createlongarray(y, n, "calc.polyfit");
  agn_toarrays(L, 1, "calc.polyfit", n, x, y);
  agn_createlongarray(a, n*n, "calc.polyfit");
  agn_createlongarray(b, n, "calc.polyfit");
  q = 0;
  for (i=0; i <= m; i++) {
    for (j=0; j <= m; j++) {
      sum = 0;
      for (k=0; k < n; k++)
        /* sum = sum + sun_pow(x[k], i + j, 1); */
        sum = tools_koaddl(sum, tools_powil(x[k], i + j), &q);  /* 2.13.0, 2.29.5 tuning */
      a[i + n*j] = sum;
    }
  }
  q = 0;
  for (i=0; i <= m; i++) {
    sum = 0;
    for (k=0; k < n; k++)
      /* sum = sum + y[k] * sun_pow(x[k], i, 1); */
      sum = tools_koaddl(sum, y[k]*tools_powil(x[k], i), &q);  /* 2.13.0, 2.29.5 tuning */
    b[i] = sum;
  }
  for (k=0; k <= m; k++) {
    for (i=0; i <= m; i++) {
      if (i == k) continue;
      temp = a[i + n*k]/a[k + n*k];
      q = 0;
      for (j=k; j <= m; j++)
        /* a[i + n*j] = a[i + n*j] - temp*a[k + n*j]; */
        a[i + n*j] = tools_koaddl(a[i + n*j], -temp*a[k + n*j], &q);  /* 2.13.0 */
      /* b[i] = b[i] - temp*b[k]; */
      q = 0;
      b[i] = tools_koaddl(b[i], -temp*b[k], &q);  /* 2.13.0 */
    }
  }
  agn_createseq(L, m + 1);
  c = 1;
  for (i=m; i >= 0; i--) {
    agn_seqsetinumber(L, -1, c++, b[i]/a[i + n*i]);
  }
  xfreeall(x, y, a, b);  /* 2.9.8 */
  return 1;
}


int newton (lua_State *L) {
  int i, n;
  long double t, r;  /* changed 2.34.10 */
  LongDoubleArray *x, *nf;
  t = agn_checknumber(L, 1);
  x = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(1));
  nf = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(2));
  n = agn_tonumber(L, lua_upvalueindex(3));
  r = getldblarray(nf, n - 1);
  for (i=n - 2; i >= 0; i--)
    r = r*(t - getldblarray(x, i)) + getldblarray(nf, i);
  lua_pushnumber(L, r);
  return 1;
}

static int calc_interp (lua_State *L) {
  size_t n, i, nargs;
  long double *x, *y, *coeff;  /* changed 2.34.10 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE || lua_type(L, 1) == LUA_TSEQ, 1, "table or sequence of pairs expected");  /* 2.35.2 fix */
  n = agn_nops(L, 1);
  if (n < 1)  /* 2.35.2 fix */
    luaL_error(L, "Error in " LUA_QS ": sample is empty.", "calc.interp");
  nargs = lua_gettop(L);
  agn_createlongarray(x, n, "calc.interp");
  agn_createlongarray(y, n, "calc.interp");
  agn_toarrays(L, 1, "calc.interp", n, x, y);
  if (nargs == 3 && lua_isseq(L, 3)) {
    size_t i;
    coeff = NULL;
    if (agn_seqsize(L, 3) != n) {
      xfreeall(x, y);  /* 2.8.2 patch, 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": number of coefficients must be equal to number of points.", "calc.interp");
    }
    agn_createlongarray(coeff, n, "calc.interp");
    for (i=0; i < n; i++)
      coeff[i] = lua_seqrawgetinumber(L, 3, i + 1);
  } else
    coeff = divdiff(n, x, y);  /* compute the coefficients of the Newton form */
  if (coeff == NULL) {
    xfreeall(x, y);  /* 2.2.0 RC 3 patch, 2.9.8 */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "calc.interp");
  }
  if (nargs > 1)
    lua_pushnumber(L, nf_eval(n, x, coeff, agn_checknumber(L, 2)));
  else {  /* 2.34.1: tuned 3.5 times: use userdata instead of sequences */
    LongDoubleArray *a, *c;
    luaL_checkstack(L, 3, "not enough stack space");  /* 3.18.4 fix */
    aux_pushud(L, a, n, "calc.interp");  /* push x as ud */
    aux_pushud(L, c, n, "calc.interp");  /* push coefficients as ud */
    for (i=0; i < n; i++) {
      setldblarray(a, i, x[i]);
      setldblarray(c, i, coeff[i]);
    }
    lua_pushnumber(L, n);  /* push number of elements */
    lua_pushcclosure(L, &newton, 3);
  }
  xfreeall(x, y, coeff);  /* 1.12.9 fix, 2.9.8 */
  return 1;
}


static int calc_newtoncoeffs (lua_State *L) {
  size_t i, n;
  long double *x, *y, *coeff;  /* changed 2.34.10 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE || lua_type(L, 1) == LUA_TSEQ, 1, "table or sequence of pairs expected");  /* 2.35.2 fix */
  n = agn_nops(L, 1);
  if (n < 1)  /* 2.35.2 fix */
    luaL_error(L, "Error in " LUA_QS ": sample is empty.", "calc.newtoncoeffs");
  agn_createlongarray(x, n, "calc.newtoncoeffs");
  agn_createlongarray(y, n, "calc.newtoncoeffs");
  agn_toarrays(L, 1, "calc.newtoncoeffs", n, x, y);
  coeff = divdiff(n, x, y);
  if (coeff == NULL) {
    xfreeall(x, y);  /* 2.8.2 patch, 2.9.8 */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "calc.newtoncoeffs");
  }
  agn_createseq(L, n);
  for (i=0; i < n; i++) {
    agn_seqsetinumber(L, -1, i + 1, coeff[i]);
  }
  xfreeall(x, y, coeff);  /* 1.12.9 fix, 2.9.8 */
  return 1;
}


static void agn_nakcoeffs (lua_State *L, int mainidx, int idx, long double *x, size_t n) {  /* copies numbers in a sequence to an array */
  size_t i;
  lua_seqrawgeti(L, mainidx, idx);
  if (lua_type(L, -1) != LUA_TSEQ)
    luaL_error(L, "Error in " LUA_QS ": expected a sequence, got %s.", "calc.nakspline", luaL_typename(L, -1));
  if (agn_seqsize(L, -1) != n)
    luaL_error(L, "Error in " LUA_QS ": number of coefficients not equal to number of points, got .", "calc.nakspline");
  for (i=0; i < n; i++) {
    x[i] = lua_seqrawgetinumber(L, -1, i + 1);
  }
  agn_poptop(L);
}


static int calc_naksplinecoeffs (lua_State *L) {
  size_t n;
  long double *x, *y, *b, *c, *d;  /* changed 2.34.10 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE || lua_type(L, 1) == LUA_TSEQ, 1, "table or sequence of pairs expected");  /* 2.35.2 fix */
  n = agn_nops(L, 1);
  if (n < 4) {  /* 1.12.9 fix to prevent invalid array access in cubic_nak */
    lua_pushfail(L);
    return 1;
  }
  agn_createlongarray(x, n, "calc.naksplinecoeffs");
  agn_createlongarray(y, n, "calc.naksplinecoeffs");
  agn_createlongarray(b, n, "calc.naksplinecoeffs");
  agn_createlongarray(c, n, "calc.naksplinecoeffs");
  agn_createlongarray(d, n, "calc.naksplinecoeffs");
  agn_toarrays(L, 1, "calc.naksplinecoeffs", n, x, y);
  if (cubic_nak(n, x, y, b, c, d) == -1) {
    xfreeall(x, y, b, c, d);  /* 1.12.9 / 2.2.0 RC 3 fixes, 2.9.8 */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "calc.naksplinecoeffs");
  }
  agn_createseq(L, 3);
  agn_longarraytoseq(L, b, n);
  lua_seqrawseti(L, -2, 1);
  agn_longarraytoseq(L, c, n);
  lua_seqrawseti(L, -2, 2);
  agn_longarraytoseq(L, d, n);
  lua_seqrawseti(L, -2, 3);
  xfreeall(x, y, b, c, d);  /* 1.12.9 fix, 2.9.8 */
  return 1;
}


static int calc_clampedsplinecoeffs (lua_State *L) {
  size_t n;
  long double *x, *y, *b, *c, *d, da, db;  /* changed 2.34.10 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE || lua_type(L, 1) == LUA_TSEQ, 1, "table or sequence of pairs expected");  /* 2.35.2 fix */
  n = agn_nops(L, 1);
  if (n < 2) {  /* 1.12.9 fix to prevent invalid array access in cubic_nak */
    lua_pushfail(L);
    return 1;
  }
  lua_pushvalue(L, 2);
  agnL_pairgetilongnumbers(L, "calc.clampedsplinecoeffs", -1, &da, &db);  /* removes pair pushed on the stack */
  agn_createlongarray(x, n, "calc.clampedsplinecoeffs");
  agn_createlongarray(y, n, "calc.clampedsplinecoeffs");
  agn_createlongarray(b, n, "calc.clampedsplinecoeffs");
  agn_createlongarray(c, n, "calc.clampedsplinecoeffs");
  agn_createlongarray(d, n, "calc.clampedsplinecoeffs");
  agn_toarrays(L, 1, "calc.clampedsplinecoeffs", n, x, y);
  if (cubic_clamped(n, x, y, b, c, d, da, db) == -1) {
    xfreeall(x, y, b, c, d);  /* 1.12.9 / 2.2.0 RC 3 fix, 2.9.8 */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "calc.clampedsplinecoeffs");
  }
  agn_createseq(L, 3);
  agn_longarraytoseq(L, b, n);
  lua_seqrawseti(L, -2, 1);
  agn_longarraytoseq(L, c, n);
  lua_seqrawseti(L, -2, 2);
  agn_longarraytoseq(L, d, n);
  lua_seqrawseti(L, -2, 3);
  xfreeall(x, y, b, c, d);  /* 1.12.9 fix, 2.9.8 */
  return 1;
}


int nakspline (lua_State *L) {
  size_t i, found, n;
  long double t;  /* changed 2.34.10 */
  LongDoubleArray *x, *y, *b, *c, *d;
  i = 1;
  found = 0;
  t = agn_checknumber(L, 1);
  n = agn_tonumber(L, lua_upvalueindex(6));
  x = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(1));
  y = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(2));
  b = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(3));
  c = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(4));
  d = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(5));
  while (!found && (i < n - 1)) {
    if (t < getldblarray(x, i))
      found = 1;
    else
      i++;
  }
  i--;
  t = y->v[i] + (t - x->v[i])*(b->v[i] + (t - x->v[i])*(c->v[i] + (t - x->v[i])*d->v[i]));
  lua_pushnumber(L, t);
  return 1;
}

static int calc_nakspline (lua_State *L) {
  size_t n, i, nargs;
  long double t, *x, *y, *b, *c, *d;  /* changed 2.34.10 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE || lua_type(L, 1) == LUA_TSEQ, 1, "table or sequence of pairs expected");  /* 2.35.2 fix */
  n = agn_nops(L, 1);
  if (n < 4) {  /* 1.12.9 fix to prevent invalid array access in cubic_nak */
    lua_pushfail(L);
    return 1;
  }
  nargs = lua_gettop(L);
  agn_createlongarray(x, n, "calc.nakspline");
  agn_createlongarray(y, n, "calc.nakspline");
  agn_createlongarray(b, n, "calc.nakspline");
  agn_createlongarray(c, n, "calc.nakspline");
  agn_createlongarray(d, n, "calc.nakspline");
  agn_toarrays(L, 1, "calc.nakspline", n, x, y);
  if (nargs == 3 && lua_type(L, 3) == LUA_TSEQ) {
    if (agn_seqsize(L, 3) != 3) {
      xfreeall(x, y, b, c, d);  /* 2.2.0 RC 3 fix, 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": expected a sequence of three sequences.", "calc.nakspline");
    }
    agn_nakcoeffs(L, 3, 1, b, n);
    agn_nakcoeffs(L, 3, 2, c, n);
    agn_nakcoeffs(L, 3, 3, d, n);
  } else {
    if (cubic_nak(n, x, y, b, c, d) == -1) {
      xfreeall(x, y, b, c, d);  /* 1.12.9 / 2.2.0 RC 3 fix, 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "calc.nakspline");
    }
  }
  if (nargs > 1) {
    t = agn_checknumber(L, 2);
    lua_pushnumber(L, spline_eval(n, x, y, b, c, d, t));
  } else {  /* 2.34.1 tuning, 6.6 times faster than using seqs */
    LongDoubleArray *X, *Y, *B, *C, *D;
    luaL_checkstack(L, 6, "not enough stack space");  /* 3.18.4 fix */
    aux_pushud(L, X, n, "calc.nakspline");  /* push x as ud */
    aux_pushud(L, Y, n, "calc.nakspline");  /* push y as ud */
    aux_pushud(L, B, n, "calc.nakspline");  /* push b as ud */
    aux_pushud(L, C, n, "calc.nakspline");  /* push c as ud */
    aux_pushud(L, D, n, "calc.nakspline");  /* push d as ud */
    for (i=0; i < n; i++) {
      setldblarray(X, i, x[i]);
      setldblarray(Y, i, y[i]);
      setldblarray(B, i, b[i]);
      setldblarray(C, i, c[i]);
      setldblarray(D, i, d[i]);
    }
    lua_pushnumber(L, n);  /* push number of elements */
    lua_pushcclosure(L, &nakspline, 6);
  }
  xfreeall(x, y, b, c, d);  /* 1.12.9 fix, 2.9.8 */
  return 1;
}


static int calc_clampedspline (lua_State *L) {
  size_t n, i, nargs;
  long double t, *x, *y, *b, *c, *d, da, db;  /* changed 2.34.10 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE || lua_type(L, 1) == LUA_TSEQ, 1, "table or sequence of pairs expected");  /* 2.35.2 fix */
  n = agn_nops(L, 1);
  if (n < 2) {  /* 1.12.9 fix to prevent invalid array access in cubic_nak */
    lua_pushfail(L);
    return 1;
  }
  nargs = lua_gettop(L);
  if (nargs < 2) {
    luaL_error(L, "Error in " LUA_QS ": at least two arguments expected.", "calc.clampedspline");
  }
  lua_pushvalue(L, 2);
  agnL_pairgetilongnumbers(L, "calc.clampedsplinecoeffs", -1, &da, &db);  /* removes pair pushed on the stack */
  agn_createlongarray(x, n, "calc.clampedspline");
  agn_createlongarray(y, n, "calc.clampedspline");
  agn_createlongarray(b, n, "calc.clampedspline");
  agn_createlongarray(c, n, "calc.clampedspline");
  agn_createlongarray(d, n, "calc.clampedspline");
  agn_toarrays(L, 1, "calc.clampedspline", n, x, y);
  if (nargs == 4 && lua_type(L, 4) == LUA_TSEQ) {
    if (agn_seqsize(L, 4) != 3) {
      xfreeall(x, y, b, c, d);  /* 2.2.0 RC 3 fix, 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": expected a sequence of three sequences.", "calc.clampedspline");
    }
    agn_nakcoeffs(L, 4, 1, b, n);
    agn_nakcoeffs(L, 4, 2, c, n);
    agn_nakcoeffs(L, 4, 3, d, n);
  } else {
    if (cubic_clamped(n, x, y, b, c, d, da, db) == -1) {
      xfreeall(x, y, b, c, d);  /* 2.2.0 RC 3 fix, 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "calc.clampedspline");
    }
  }
  if (nargs > 2) {
    t = agn_checknumber(L, 3);
    lua_pushnumber(L, spline_eval(n, x, y, b, c, d, t));
  } else {
    LongDoubleArray *X, *Y, *B, *C, *D;
    luaL_checkstack(L, 6, "not enough stack space");  /* 3.18.4 fix */
    aux_pushud(L, X, n, "calc.clampedspline");  /* push x as ud */
    aux_pushud(L, Y, n, "calc.clampedspline");  /* push y as ud */
    aux_pushud(L, B, n, "calc.clampedspline");  /* push b as ud */
    aux_pushud(L, C, n, "calc.clampedspline");  /* push c as ud */
    aux_pushud(L, D, n, "calc.clampedspline");  /* push d as ud */
    for (i=0; i < n; i++) {
      setldblarray(X, i, x[i]);
      setldblarray(Y, i, y[i]);
      setldblarray(B, i, b[i]);
      setldblarray(C, i, c[i]);
      setldblarray(D, i, d[i]);
    }
    lua_pushnumber(L, n);     /* push number of elements */
    lua_pushcclosure(L, &nakspline, 6);
  }
  xfreeall(x, y, b, c, d);  /* 1.12.9 fix, 2.9.8 */
  return 1;
}


/* agn_rawgetinumber and pendants always return zero if they encounter a non-number */
/* printf("i=%d %lf\n", i, (double)(t->v)[i]); */
#define aux_getcoeffvalues(L,realonly,fn,fnsize,a,b,nops,flag,hascplx,procname) { \
  nops = fnsize(L, 1); \
  LongDoubleArray *t = ((flag == 0) ? a : b); \
  int i; \
  aux_pushud(L, t, nops, procname); \
  for (i=0; i < nops; i++) { \
    int rc = 0; \
    setldblarray(t, i, (flag == 0) ? fn(L, 1, i + 1, &rc) : 0); \
    if (!rc) { \
      if (realonly) \
        luaL_error(L, "Error in " LUA_QS ": structure must contain numbers only.", procname); \
      if (lua_istable(L, 1)) lua_rawgeti(L, 1, i + 1); \
      else if (lua_isseq(L, 1)) lua_seqrawgeti(L, 1, i + 1); \
      else if (lua_isreg(L, 1)) agn_regrawgeti(L, 1, i + 1); \
      if (lua_iscomplex(L, -1)) { \
        lua_Number x, y; \
        agn_getcmplxparts(L, -1, &x, &y); \
        setldblarray(t, i, (flag == 0) ? x : y); \
        hascplx = 1; \
      } else if (!agn_isnumber(L, -1)) { \
        int typ = lua_type(L, -1); \
        agn_poptop(L); \
        luaL_error(L, "Error in " LUA_QS ": structure must contain numbers or complex numbers, got %s.", procname, lua_typename(L, typ)); \
      } \
      agn_poptop(L); \
    } \
  } \
}

static int aux_pushcoeffs (lua_State *L, LongDoubleArray *a, LongDoubleArray *b, size_t nops, const char *procname, int flag, int realonly) {
  size_t i;
  int hascplx = 0;
  switch (lua_type(L, 1)) {  /* 2.34.0 extension */
    case LUA_TTABLE : {
      aux_getcoeffvalues(L, realonly, agn_rawgetinumber, agn_asize, a, b, nops, flag, hascplx, procname);
      break;
    }
    case LUA_TSEQ: {
      aux_getcoeffvalues(L, realonly, agn_seqrawgetinumber, agn_seqsize, a, b, nops, flag, hascplx, procname);
      break;
    }
    case LUA_TREG: {
      aux_getcoeffvalues(L, realonly, agn_regrawgetinumber, agn_regsize, a, b, nops, flag, hascplx, procname);
      break;
    }
    default: {
      luaL_checkstack(L, 2, "not enough stack space");
      lua_createtable(L, nops, 0);
      for (i=0; i < nops; i++) {  /* register coefficients in a table and recall function */
        lua_pushvalue(L, i + 1);
        lua_seti(L, -2, i + 1);
      }
      lua_replace(L, 1);
      return aux_pushcoeffs(L, a, b, nops, procname, flag, realonly);
    }
  }
  return hascplx;
}

static int polygenfma_generator (lua_State *L) {
  long double x;  /* changed 2.34.10 */
  LongDoubleArray *a = NULL;
  x = agn_checknumber(L, 1);  /* get argument */
  a = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(1));
  lua_pushnumber(L, tools_polyevalfma(x, a, a->size));
  return 1;
}

static int polygencfma_generator (lua_State *L) {
  long double re, im, nextre, nextim, tmp, xre, xim;
  double xre_d, xim_d;
  LongDoubleArray *a, *b;
  int i;
  a = b = NULL;
  if (!agn_getcmplxparts(L, 1, &xre_d, &xim_d)) {
    luaL_error(L, "Error: in " LUA_QS " closure: expected a (complex) number, got %s.", "calc.polygen", luaL_typename(L, 1));
  }
  xre = (long double)xre_d;
  xim = (long double)xim_d;
  a = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(1));
  b = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(2));
  re = a->v[0]; im = b->v[0];
  for (i=1; i < a->size; i++) {
    /* Goal: next_acc = (acc * z) + coeff[i]
       Real Part: nextre = R*x - I*y + coeffs_r[i]
       Step A: tmp = R*x + coeffs_r[i].
       See file mpf.c/polygen_complexgenerator() */
    tmp = tools_fmal(re, xre, a->v[i]);
    /* Step B: nextre = -I*y + tmp */
    nextim = -im;
    nextre = tools_fmal(nextim, xim, tmp);
    /* Imaginary Part: nextim = R*y + I*x + coeffs_i[i]
       Step A: tmp = R*y + coeffs_i[i] */
    tmp = tools_fmal(re, xim, b->v[i]);
    /* Step B: nextim = I*x + tmp */
    nextim = tools_fmal(im, xre, tmp);
    /* Update accumulator */
    re = nextre;
    im = nextim;
  }
  agn_pushcomplex(L, re, im);
  return 1;
}

static int calc_polygen (lua_State *L) {
  size_t nops;
  int rc;
  LongDoubleArray *a, *b;
  a = b = NULL;  /* changed 2.34.10 */
  nops = agnL_gettop(L, "expected at least one argument", "calc.polygen");
  luaL_checkstack(L, 1, "too many arguments");
  if ( (rc = aux_pushcoeffs(L, a, b, nops, "calc.polygen", 0, 0)) ) {
    luaL_checkstack(L, 1, "not enough stack space");
    aux_pushcoeffs(L, a, b, nops, "calc.polygen", 1, 0);
  }
  /* register userdata in C closure; 2.35.0 speed-up by at least 100 % */
  lua_pushcclosure(L, (rc == 0) ? &polygenfma_generator : &polygencfma_generator, 1 + rc);
  return 1;
}


/* source: http://programbank4u.blogspot.de/2013/04/lagrange-interpolation-c-program.html */
static int linterp (lua_State *L) {  /* 2.1.3 */
  size_t n;
  int i, j;
  long double x0, sum, prod;  /* changed 2.34.10 */
  LongDoubleArray *x, *y;
  x0 = agn_checknumber(L, 1);
  x = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(1));
  y = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(2));
  n = x->size;
  sum = 0;
  for (i=0; i < n; i++) {
    prod = 1;
    for (j=0; j < n; j++) {
      if (j == i) continue;
      prod *= (x0 - getldblarray(x, j))/(getldblarray(x, i) - getldblarray(x, j));
    }
    sum += getldblarray(y, i)*prod;
  }
  lua_pushnumber(L, sum);
  return 1;
}

/* calc.linterp
   returns a function that conducts a Lagrange interpolation for a given sequence or table o of numeric pairs
   x:y where x and y denote a point in the plane. It is often said that Lagrange interpolation is suited for
   theoretical purposes only, since it is also very slow. */

static int calc_linterp (lua_State *L) {
  size_t n, i;
  int type;
  long double x, y;  /* changed 2.34.10 */
  LongDoubleArray *a, *b;
  type = lua_type(L, 1);
  if ((type != LUA_TSEQ && type != LUA_TTABLE) || ( n = agn_nops(L, 1) ) == 0) { /* 2.35.2 fix */
    luaL_error(L, "Error in " LUA_QS ": expected a sequence or table of one or more pairs.", "calc.linterp");
    return 1;
  }
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.18.4 fix */
  agnL_newldblarray(L, n, "calc.linterp");  /* push userdata a */
  agnL_newldblarray(L, n, "calc.linterp");  /* push userdata b */
  a = (LongDoubleArray *)lua_touserdata(L, -2);
  b = (LongDoubleArray *)lua_touserdata(L, -1);
  if (type == LUA_TSEQ) {
    for (i=1; i <= n; i++) {
      lua_seqrawgeti(L, 1, i);
      agnL_pairgetilongnumbers(L, "calc.linterp", -1, &x, &y);  /* pops the pair after evaluation */
      setldblarray(a, i - 1, x);
      setldblarray(b, i - 1, y);
    }
  } else {
    for (i=1; i <= n; i++) {
      lua_rawgeti(L, 1, i);
      agnL_pairgetilongnumbers(L, "calc.linterp", -1, &x, &y);  /* pops the pair after evaluation */
      setldblarray(a, i - 1, x);
      setldblarray(b, i - 1, y);
    }
  }
  lua_pushcclosure(L, &linterp, 2);
  return 1;
}


/* 2.1.5, Simpson-Simpson Adaptive Quadrature, taken from:
   http://www.mymathlib.com/quadrature/adaptive.html, by RLH, Copyright © 2004 RLH. All rights reserved.
   Adapted for Agena. */
#ifndef __ARMCPU
struct Subinterval {
  long double upper_limit;
  long double lower_limit;
  long double function[5];
  struct Subinterval *interval;
};

/* Description:

   Starting at the left-end point, a, find the min power of 2, m, so that the difference between using
   Simpson's rule and the composite Simpson's rule on the interval [a, a+(b-a)/2^m] is less than twice
   the tolerance * (length of the subinterval)/(b-a).  Then repeat the process for integrating over the
   interval [(a+(b-a)/2^m, b] until the right end point, b, is finally reached.

   The integral is then the sum of the integrals of each subinterval. If at any time, the length of the
   subinterval for which the estimates based on Simpson's rule and the composite Simpson's rule is less
   than min_h the process is terminated.

  Arguments:
     double a          The lower limit of the integration interval.
     double b          The upper limit of integration.
     double tolerance  The acceptable error estimate of the integral. The integral of a subinterval is
                       accepted when the magnitude of the estimated error falls below the pro-rated tolerance
                       of the subinterval.
     double min_h      The minimum subinterval length. If no subinterval of length > min_h is found for which
                       the estimated error falls below the pro-rated tolerance, the process terminates.

  Return:
     The integral of f(x) from a to a +  h, or `fail' if no subinterval of length > min_h was found for which
     the estimated error was less that the pro-rated error.


  Source:
     http://www.mymathlib.com/c_source/quadrature/adaptive/simpson_simpson.c, adapted by alexander walz */

static int Simpsons_Rule_Update (lua_State *L, int nargs, struct Subinterval *pinterval, long double *s1, long double *s2) {
  long double h, h4;
  int errorcode;
  h = pinterval->upper_limit - pinterval->lower_limit;
  h4 = 0.25L*h;
  pinterval->function[1] = agnL_fncallx(L, 1, pinterval->lower_limit + h4, 4, nargs, &errorcode, 0);  /* 3.12.0 change for multivariate support */
  if (errorcode != 0) return errorcode;  /* 7.3.4 fix */
  pinterval->function[3] = agnL_fncallx(L, 1, pinterval->upper_limit - h4, 4, nargs, &errorcode, 0);
  if (errorcode != 0) return errorcode;  /* 7.3.4 fix */
  *s1 = pinterval->function[0] + 4.0*pinterval->function[2] + pinterval->function[4];
  *s1 *= 0.166666666666666666666667L*h;
  *s2 = pinterval->function[0] + 4.0L*pinterval->function[1]
      + 2.0*pinterval->function[2] + 4.0L*pinterval->function[3]
      + pinterval->function[4];
  *s2 *= 0.0833333333333333333333333L*h;
  return 0;
}
#endif

static int calc_simaptive64 (lua_State *L);

#define aux_freeinterval(pinterval,qinterval) { \
  while (pinterval != NULL) { \
    qinterval = pinterval->interval; \
    xfree(pinterval); \
    pinterval = qinterval; \
  } \
}

#define aux_simaptive_issueerror(L,errorcode,pn) { \
  if (errorcode != 0) { \
    aux_freeinterval(pinterval,qinterval); \
    if (errorcode == 1) { \
      luaL_error(L, "Error in " LUA_QS ": return of function call is not a number.", pn); \
    } else { \
      luaL_error(L, "Error in " LUA_QS ": %s.", pn, lua_tostring(L, -1)); \
    } \
  } \
}

static int calc_simaptive (lua_State *L) {
#if defined(__ARMCPU)
  return calc_simaptive64(L);
#else
  struct Subinterval *pinterval, *qinterval;
  int err, samples, adjust, nargs, iters, use64, errorcode;
  long double integral, epsilon_density, epsilon, s1, s2, a, b, tolerance, min_h, omega, delta;
  (void)omega; (void)samples; (void)adjust; (void)delta; (void)iters; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (tools_isinf(a) || tools_isinf(b))  /* 4.2.4 fix */
    luaL_error(L, "Error in " LUA_QS ": at least one boundary is +/-infinity.", "calc.simaptive");
  else if (a > b)  /* 4.2.4 fix */
    luaL_error(L, "Error in " LUA_QS ": left border must be less than right border.", "calc.simaptive");
  else if (a == b) {  /* 4.2.4 fix */
    lua_pushnumber(L, 0.0);
    return 1;
  }
  tolerance = 0.5L*agn_getepsilon(L);  /* 3.12.0 change for `eps` option */
  min_h = 1e-7L;                       /* 3.12.0 change for `length` option */
  aux_fcheckoptionslong(L, 4, &nargs, &tolerance, &omega, &samples, &min_h, &adjust, &delta, &iters, &use64, "calc.simaptive");  /* 3.12.0 change */
  integral = 0.0L;
  epsilon_density = 2.0L*tolerance/(b - a);
  /* Create the initial level, with lower_limit = a, upper_limit = b, and f(x) evaluated at a, b, and (a + b) / 2. */
  pinterval = (struct Subinterval*)agn_malloc(L, sizeof(struct Subinterval), "calc.simaptive", NULL);
  pinterval->interval = NULL;
  pinterval->upper_limit = b;
  pinterval->lower_limit = a;
  pinterval->function[0] = agnL_fncallx(L, 1, pinterval->lower_limit, 4, nargs, &errorcode, 0);  /* 3.12.0 change for multivariate support */
  aux_simaptive_issueerror(L, errorcode, "calc.simaptive");  /* 7.3.4 fix */
  pinterval->function[2] = agnL_fncallx(L, 1, 0.5L*(pinterval->upper_limit + pinterval->lower_limit), 4, nargs, &errorcode, 0);
  aux_simaptive_issueerror(L, errorcode, "calc.simaptive");  /* 7.3.4 fix */
  pinterval->function[4] = agnL_fncallx(L, 1, pinterval->upper_limit, 4, nargs, &errorcode, 0);
  aux_simaptive_issueerror(L, errorcode, "calc.simaptive");  /* 7.3.4 fix */
  /* Calculate the tolerance for the current interval. Calculate the single subinterval Simpson rule,
     and the two subintervals composite Simpson rule. */
  err = 1;
  epsilon = epsilon_density*(b - a);
  errorcode = Simpsons_Rule_Update(L, nargs, pinterval, &s1, &s2);
  aux_simaptive_issueerror(L, errorcode, "calc.simaptive");  /* 7.3.4 fix */
  while (pinterval->upper_limit - pinterval->lower_limit > min_h) {
    if (fabsl(s1 - s2) < epsilon) {
      /* If the two estimates are close, then increment the integral and if we are not at the right end,
         set the left end of the new interval to the right end of the old interval and the right end of
         the new interval remains the same (as the previous right end for this interval. */
      integral += s2;
      if (pinterval->interval == NULL) {
        err = 0;
        break;
      }
      qinterval = pinterval->interval;
      qinterval->lower_limit = pinterval->upper_limit;
      qinterval->function[0] = qinterval->function[2];
      qinterval->function[2] = qinterval->function[3];
      xfree(pinterval);
      pinterval = qinterval;
    } else {
      /* If the two estimates are not close, then create a new interval with same left end point and right
         end point at the midpoint of the current interval. */
      qinterval = (struct Subinterval*)agn_malloc(L, sizeof(struct Subinterval), "calc.simaptive", pinterval, NULL);  /* 4.11.5 fix */
      qinterval->interval = pinterval;
      qinterval->lower_limit = pinterval->lower_limit;
      qinterval->upper_limit = 0.5L*(pinterval->upper_limit + pinterval->lower_limit);
      qinterval->function[0] = pinterval->function[0];
      qinterval->function[2] = pinterval->function[1];
      qinterval->function[4] = pinterval->function[2];
      pinterval = qinterval;
    }
    errorcode = Simpsons_Rule_Update(L, nargs, pinterval, &s1, &s2);  /* 7.3.4 fix */
    aux_simaptive_issueerror(L, errorcode, "calc.simaptive");
    epsilon = epsilon_density*(pinterval->upper_limit - pinterval->lower_limit);
  }
  aux_freeinterval(pinterval, qinterval);
  lua_pushnumber(L, (err) ? AGN_NAN : integral);
  return 1;
#endif
}

struct Subinterval64 {
  double upper_limit;
  double lower_limit;
  double function[5];
  struct Subinterval64 *interval;
};

static int Simpsons_Rule_Update64 (lua_State *L, int nargs, struct Subinterval64 *pinterval, double *s1, double *s2) {
  double h, h4;
  int errorcode;
  h = pinterval->upper_limit - pinterval->lower_limit;
  h4 = 0.25*h;
  pinterval->function[1] = agnL_fncallx(L, 1, pinterval->lower_limit + h4, 4, nargs, &errorcode, 0);
  if (errorcode != 0) return errorcode;  /* 7.3.4 fix */
  pinterval->function[3] = agnL_fncallx(L, 1, pinterval->upper_limit - h4, 4, nargs, &errorcode, 0);
  if (errorcode != 0) return errorcode;  /* 7.3.4 fix */
  *s1 = pinterval->function[0] + 4.0*pinterval->function[2] + pinterval->function[4];
  *s1 *= 0.166666666666666666666667*h;
  *s2 = pinterval->function[0] + 4.0*pinterval->function[1]
      + 2.0*pinterval->function[2] + 4.0*pinterval->function[3]
      + pinterval->function[4];
  *s2 *= 0.0833333333333333333333333*h;
  return 0;
}

static int calc_simaptive64 (lua_State *L) {
  struct Subinterval64 *pinterval, *qinterval;
  int err, samples, adjust, nargs, iters, errorcode;
  double integral, epsilon_density, epsilon, s1, s2, a, b, tolerance, min_h, omega, delta;
  (void)omega; (void)samples; (void)adjust; (void)delta; (void)iters;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (tools_isinf(a) || tools_isinf(b))
    luaL_error(L, "Error in " LUA_QS ": at least one boundary is +/-infinity.", "calc.simaptive");
  else if (a > b)
    luaL_error(L, "Error in " LUA_QS ": left border must be less than right border.", "calc.simaptive");
  else if (a == b) {
    lua_pushnumber(L, 0.0);
    return 1;
  }
  tolerance = 0.5*agn_getepsilon(L);
  min_h = 1e-7;
  aux_fcheckoptions(L, 4, &nargs, &tolerance, &omega, &samples, &min_h, &adjust, &delta, &iters, "calc.simaptive");
  integral = 0.0;
  epsilon_density = 2.0*tolerance/(b - a);
  /* Create the initial level, with lower_limit = a, upper_limit = b, and f(x) evaluated at a, b, and (a + b) / 2. */
  pinterval = (struct Subinterval64*)agn_malloc(L, sizeof(struct Subinterval64), "calc.simaptive", NULL);
  pinterval->interval = NULL;
  pinterval->upper_limit = b;
  pinterval->lower_limit = a;
  pinterval->function[0] = agnL_fncallx(L, 1, pinterval->lower_limit, 4, nargs, &errorcode, 0);
  aux_simaptive_issueerror(L, errorcode, "calc.simaptive64");  /* 7.3.4 fix */
  pinterval->function[2] = agnL_fncallx(L, 1, 0.5*(pinterval->upper_limit + pinterval->lower_limit), 4, nargs, &errorcode, 0);
  aux_simaptive_issueerror(L, errorcode, "calc.simaptive64");  /* 7.3.4 fix */
  pinterval->function[4] = agnL_fncallx(L, 1, pinterval->upper_limit, 4, nargs, &errorcode, 0);
  aux_simaptive_issueerror(L, errorcode, "calc.simaptive64");  /* 7.3.4 fix */
  /* Calculate the tolerance for the current interval. Calculate the single subinterval Simpson rule,
     and the two subintervals composite Simpson rule. */
  err = 1;
  epsilon = epsilon_density*(b - a);
  errorcode = Simpsons_Rule_Update64(L, nargs, pinterval, &s1, &s2);
  aux_simaptive_issueerror(L, errorcode, "calc.simaptive64");  /* 7.3.4 fix */
  while (pinterval->upper_limit - pinterval->lower_limit > min_h) {
    if (fabs(s1 - s2) < epsilon) {
      /* If the two estimates are close, then increment the integral and if we are not at the right end,
         set the left end of the new interval to the right end of the old interval and the right end of
         the new interval remains the same (as the previous right end for this interval. */
      integral += s2;
      if (pinterval->interval == NULL) {
        err = 0;
        break;
      }
      qinterval = pinterval->interval;
      qinterval->lower_limit = pinterval->upper_limit;
      qinterval->function[0] = qinterval->function[2];
      qinterval->function[2] = qinterval->function[3];
      xfree(pinterval);
      pinterval = qinterval;
    } else {
      /* If the two estimates are not close, then create a new interval with same left end point and right
         end point at the midpoint of the current interval. */
      qinterval = (struct Subinterval64*)agn_malloc(L, sizeof(struct Subinterval64), "calc.simaptive", pinterval, NULL);
      qinterval->interval = pinterval;
      qinterval->lower_limit = pinterval->lower_limit;
      qinterval->upper_limit = 0.5*(pinterval->upper_limit + pinterval->lower_limit);
      qinterval->function[0] = pinterval->function[0];
      qinterval->function[2] = pinterval->function[1];
      qinterval->function[4] = pinterval->function[2];
      pinterval = qinterval;
    }
    errorcode = Simpsons_Rule_Update64(L, nargs, pinterval, &s1, &s2);
    aux_simaptive_issueerror(L, errorcode, "calc.simaptive64");  /* 7.3.4 fix */
    epsilon = epsilon_density*(pinterval->upper_limit - pinterval->lower_limit);
  }
  aux_freeinterval(pinterval, qinterval);  /* 7.3.4 fix */
  lua_pushnumber(L, (err) ? AGN_NAN : integral);
  return 1;
}


/* Added January 09, 2008; 0.9.0 / July 12, 2008; 0.12.0; C version: 2.1.5, 04.03.2014.
   Integration of a function on an interval using a modified bisection method. Algorithm taken from
   `Fortran 90 Kurs` by G. Schmitt, Oldenbourg Verlag, 1996, p. 119
   The algorithm is slow and imprecise with larger eps. */

static int calc_gtrap64 (lua_State *L);

static int calc_gtrap (lua_State *L) {
#ifdef __ARMCPU
   return calc_gtrap64(L);
#else
  int k, n, samples, adjust, nargs, iters, use64;
  long double delta, dold, h, sum, fnew, fold, a, b, eps, fa, fb, fah, omega, length;  /* 2.34.10 change to longdoubles */
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)iters; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (tools_isinf(a) || tools_isinf(b))
    luaL_error(L, "Error in " LUA_QS ": at least one boundary is +/-infinity.", "calc.gtrap");  /* 4.12.6 error message fix */
  else if (a > b)  /* 4.2.4 fix */
    luaL_error(L, "Error in " LUA_QS ": left border must be less than right border.", "calc.gtrap");
  else if (a == b) {  /* 4.2.4 fix */
    lua_pushnumber(L, 0.0);
    return 1;
  }
  eps = agn_getepsilon(L);  /* 3.12.0 change for `eps` option */
  aux_fcheckoptionslong(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.gtrap");  /* 3.12.0 change */
  dold = 1;
  n = 2;
  h = (b - a)/n;
  fa =  agnL_fncall(L, 1, a, 4, nargs);  /* 3.12.0 change for multivariate support */
  fah = agnL_fncall(L, 1, a + h, 4, nargs);
  fb =  agnL_fncall(L, 1, b, 4, nargs);
  sum = fa + 2.0L*fah + fb;
  fold = 0.5L*h*sum;  /* 2.17.7 tweak */
  do {
    n *= 2;
    h *= 0.5L;  /* 2.17.7 tweak */
    for (k=1; k < n; k = k + 2) {
      sum += 2.0L*agnL_fncall(L, 1, a + k*h, 4, nargs);  /* 3.12.0 change for multivariate support */
    }
    fnew = 0.5L*h*sum;  /* 2.17.7 tweak */
    delta = fabsl((fnew - fold)/fnew);
    fold = fnew;
    dold = delta;
  } while (delta > eps || delta < dold);
  lua_pushnumber(L, fnew);
  return 1;
#endif
}

static int calc_gtrap64 (lua_State *L) {
  int k, n, samples, adjust, nargs, iters;
  double delta, dold, h, sum, fnew, fold, a, b, eps, fa, fb, fah, omega, length, cs, ccs;
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)iters;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (tools_isinf(a) || tools_isinf(b))
    luaL_error(L, "Error in " LUA_QS ": at least one boundary is +/-infinity.", "calc.gtrap64");
  else if (a > b)
    luaL_error(L, "Error in " LUA_QS ": left border must be less than right border.", "calc.gtrap64");
  else if (a == b) {
    lua_pushnumber(L, 0.0);
    return 1;
  }
  eps = agn_getepsilon(L);
  aux_fcheckoptions(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, "calc.gtrap64");
  dold = 1;
  n = 2;
  h = (b - a)/n;
  fa =  agnL_fncall(L, 1, a, 4, nargs);
  fah = agnL_fncall(L, 1, a + h, 4, nargs);
  fb =  agnL_fncall(L, 1, b, 4, nargs);
  sum = fa + 2.0*fah + fb;
  fold = 0.5*h*sum;
  do {
    cs = ccs = 0.0;
    n *= 2;
    h *= 0.5;
    for (k=1; k < n; k = k + 2) {
      sum = tools_kbadd(sum, 2.0*agnL_fncall(L, 1, a + k*h, 4, nargs), &cs, &ccs);
    }
    sum += cs + ccs;
    fnew = 0.5*h*sum;
    delta = fabs((fnew - fold)/fnew);
    fold = fnew;
    dold = delta;
  } while (delta > eps || delta < dold);
  lua_pushnumber(L, fnew);
  return 1;
}


/* Romberg Integration - combines Trapezoidal Rule with Richardson Extrapolation, 6.7.9 */
static int calc_romberg (lua_State *L) {
  int i, j, n, itop, rowi, rown, adjust, iters, samples, use64, nargs;
  long double a, b, eps, f, z, conv, h, p, q, g, d, v, x, omega, delta, length;
  long double A[50 + 1][50 + 1] = { { 0.0L } };
  (void)omega; (void)length; (void)samples; (void)adjust; (void)delta; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (tools_isinf(a) || tools_isinf(b))
    luaL_error(L, "Error in " LUA_QS ": at least one boundary is +/-infinity.", "calc.romberg");
  else if (a > b)
    luaL_error(L, "Error in " LUA_QS ": left border must be less than right border.", "calc.romberg");
  else if (a == b) {
    lua_pushnumber(L, 0.0);
    return 1;
  }
  eps = 1.0e-015L;
  iters = 26;
  aux_fcheckoptionslong(L, 4, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.romberg");
  h = b - a;
  p = agnL_fncall(L, 1, a, 4, nargs);
  q = agnL_fncall(L, 1, b, 4, nargs);
  A[0][0] = 0.5L*h*(p + q);
  /* Trapezoidal Rule with interval halving */
  for (n=1; n <= iters; n++) {
    /* 2^(n-1) remains correct for the step size of the previous level */
    g = h/(long double)(1 << (n - 1));
    itop = (1 << n) - 1;
    d = (long double)(1 << n);
    v = 0.0;
    for (i=1; i <= itop; i += 2) {
      x = a + h*i/d;
      v += agnL_fncall(L, 1, x, 4, nargs);
    }
    if (!tools_isfinite(v)) break;  /* no convergence, so bail out to prevent too much waiting time */
    /* Fill current row's first column (0) using previous row's first column */
    A[n][0] = 0.5*(A[n - 1][0] + g*v);
    /* Richardson Extrapolation */
    rown = n;
    /* j starts at 1 (the second column) and goes up to column n */
    for (j=1; j <= n; j++) {
      rown--;
      /* Since j is now 1-based index, factor f is 4^j */
      f = tools_intpowl(4.0L, j);
      A[rown][j] = (f*A[rown + 1][j - 1] - A[rown][j - 1])/(f - 1.0L);
    }
    /* Check for convergence */
    conv = 1.0e10;
    rowi = n;
    /* We can start checking once we have a few levels of refinement */
    if (n > 3) {
      /* Check all extrapolation columns in the current row */
      for (j = 0; j < n; j++) {
        if (fabsl(A[rowi][j] - A[rowi - 1][j]) <= eps) {
          lua_pushnumber(L, A[rowi][j]);
          return 1;
        }
        /* Determine best solution (smallest difference) */
        z = A[rowi][j] - A[rowi - 1][j];
        if (fabsl(z) < fabsl(conv)) conv = z;
        rowi--;
      }
    }
  }
  lua_pushfail(L);
  return 1;
}


/* The Airy wave function returns both the first independent solution to the differential equation y"(x) = x*y and its first derivative, for any real x. */
static int calc_Ai (lua_State *L) {  /* 2.8.1 */
  lua_Number x, ai, bi, dai, dbi;
  int r;
  x = agn_checknumber(L, 1);
  r = airy(x, &ai, &dai, &bi, &dbi);
  if (r == -1)
    lua_pushfail(L);
  else {
    lua_pushnumber(L, ai);
    lua_pushnumber(L, dai);
  }
  return 1 + (r == 0);
}


/* The Airy wave function returns both the second independent solution to the differential equation y"(x) = x*y and its first derivative, for any real x. */
static int calc_Bi (lua_State *L) {  /* 2.8.1 */
  lua_Number x, ai, bi, dai, dbi;
  int r;
  x = agn_checknumber(L, 1);
  r = airy(x, &ai, &dai, &bi, &dbi);
  if (r == -1)
    lua_pushfail(L);
  else {
    lua_pushnumber(L, bi);
    lua_pushnumber(L, dbi);
  }
  return 1 + (r == 0);
}

/* Computes the Riemann Zeta function in the real domain, using the zetac function.
 *
 *                inf.
 *                 -    -x
 *   zetac(x)  =   >   k   ,   x > 1,
 *                 -
 *                k=2
 *
 * is related to the Riemann zeta function by
 *
 *  Riemann zeta(x) = zetac(x) + 1. */
static int calc_zeta (lua_State *L) {  /* 2.8.1, extended 6.4.5 */
  if (lua_gettop(L) == 1) {
    lua_pushnumber(L, Zeta(agn_checknumber(L, 1)));  /* Zeta(x) = (zetac(x) + 1) */
  } else {  /* this is not equal to Maple's Zeta(n, z) which computes the n'th derivative at x ! */
    lua_pushnumber(L, zeta(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  }
  return 1;
}


/* Returns the polylogarithm of order n at a real point x. The return is a number, or `fail` if n < -1 for this situation is
   not implemented.

 * The polylogarithm of order n is defined by the series
 *
 *              inf   k
 *               -   x
 *  Li (x)  =    >   ---  .
 *    n          -     n
 *              k=1   k
*/

static int calc_polylog (lua_State *L) {  /* 2.8.1 */
  int flag;
  lua_Number r;
  r = polylog(agn_checkinteger(L, 1), agn_checknumber(L, 2), &flag);
  if (flag)  /* not implemented for n < -1 */
    lua_pushfail(L);
  else
    lua_pushnumber(L, r);
  return 1;
}


/* Evaluates the exponential integral
 *
 *                 inf.
 *                   -
 *                  | |   -xt
 *                  |    e
 *      E (x)  =    |    ----  dt.
 *       n          |      n
 *                | |     t
 *                 -
 *                  1

  for non-negative n and real x. The return is a number. */

static int calc_En (lua_State *L) {  /* 2.8.1 */
  lua_pushnumber(L, expn(agn_checkinteger(L, 1), agn_checknumber(L, 2)));
  return 1;
}


/* Evaluates the incomplete gamma integral defined by
 *
 *                           x
 *                            -
 *                   1       | |  -t  a-1
 *  igam(a,x)  =   -----     |   e   t   dt.
 *                  -      | |
 *                 | (a)    -
 *                           0
 *
 *
 * In this implementation both arguments must be positive. */

static int calc_igamma (lua_State *L) {  /* 2.8.1 */
  lua_Number a, x;
  x = agn_checknumber(L, 1);
  a = agn_checknumber(L, 2);
  if (x <= 0 || a <= 0)
    lua_pushfail(L);
  else
    lua_pushnumber(L, igam(a, x));
  return 1;
}


/* Evaluates the complemented incomplete gamma integral defined by
 *
 *  igamc(a,x)   =   1 - igam(a,x)
 *
 *                            inf.
 *                              -
 *                     1       | |  -t  a-1
 *               =   -----     |   e   t   dt.
 *                    -      | |
 *                   | (a)    -
 *                             x
 *
 * In this implementation both arguments must be positive. */

static int calc_igammac (lua_State *L) {  /* 2.8.1 */
  lua_Number a, x;
  x = agn_checknumber(L, 1);
  a = agn_checknumber(L, 2);
  if (x <= 0 || a <= 0)
    lua_pushfail(L);
  else
    lua_pushnumber(L, igamc(a, x));
  return 1;
}


/* Returns the incomplete beta integral of the arguments, evaluated
 * from zero to x.  The function is defined as
 *
 *                  x
 *     -            -
 *    | (a+b)      | |  a-1     b-1
 *  -----------    |   t   (1-t)   dt.
 *   -     -     | |
 *  | (a) | (b)   -
 *                 0
 *
 * The domain of definition is 0 <= x <= 1.  In this
 * implementation a and b are restricted to positive values. */

static int calc_ibeta (lua_State *L) {  /* 2.8.2 */
  lua_Number a, b, x;
  x = agn_checknumber(L, 1);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (a <= 0 || b <= 0)
    lua_pushfail(L);
  else if (x < 0 || x > 1)
    lua_pushundefined(L);
  else
    lua_pushnumber(L, incbet(a, b, x));
  return 1;
}


/* Inverse of incomplete beta integral
 *
 * SYNOPSIS:
 *
 * double a, b, x, y, incbi();
 *
 * x = incbi( a, b, y );
 *
 * DESCRIPTION:
 *
 * Given y, the function finds x such that
 *
 *  incbet( a, b, x ) = y . */

static int calc_invibeta (lua_State *L) {  /* 2.8.2 */
  lua_Number y, a, b;
  y = agn_checknumber(L, 1);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  lua_pushnumber(L, incbi(a, b, y));
  return 1;
}


/* Returns the limit of a function at a given point. If the left or right limit does not exist, `undefined`
   is returned. The function returns `undefined` instead of +/- infinity if the left and the right limit are either
   +infinity or -infinity. 2.7 times faster than the Agena version.

   f := << x -> (x^3+2*x^2-4*x-8)/(x^2+4*x+4) >>, reduced to: f := << x -> x - 2 >>, singularity at x = -2 */

static int calc_limit (lua_State *L) {  /* 2.9.7, 2.34.10 swirch to longdoubles; 3.19.4 back to doubles */
  double x, a, b, eps, delta, r, step;
  int nargs, sided, deriv;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  /* luaL_checkstack is called by agnL_fncall */
  x = agn_checknumber(L, 2);
  (void)delta; (void)deriv; (void)step;
  aux_geteps(L, x, 3, &nargs, &eps, &delta, &step, &sided, &deriv, 0, 10, 11, 0, "calc.limit");
  /* sun_nextafter may return undefined or a wrong finite value in function calls within the vicinity of singularities, e.g.
     f := << x -> 1/x^2 >> at x = 0, or f := << x -> (x^2-3)/(x-3) >> at x = 3 */
  a = agnL_fncall(L, 1, x - eps, 3, nargs);  /* 2.9.8 */
  b = agnL_fncall(L, 1, x + eps, 3, nargs);  /* 2.9.8 */
  switch (sided) {  /* 2.9.8 */
    case 1: {  /* left-sided limit */
      lua_pushnumber(L, fabs(a) < eps ? 0 : a);
      return 1;
    }
    case 2: {  /* right-sided limit */
      lua_pushnumber(L, fabs(b) < eps ? 0 : b);
      return 1;
    }
    case 3: {  /* both left and right-sided limits */
      lua_pushnumber(L, fabs(a) < eps ? 0 : a);
      lua_pushnumber(L, fabs(b) < eps ? 0 : b);
      return 2;
    }
  }
  if (isunordered(a, b))  /* at least one function value is undefined ? */
    lua_pushundefined(L);
  else if ( (a == HUGE_VAL && b == HUGE_VAL) || (a == -HUGE_VAL && b == -HUGE_VAL) )
    lua_pushnumber(L, a);
  else {
    if (tools_approx(a, b, 2*eps)) {  /* 2.9.8 fix, lim sin(x), x=0, returned undefined for check failed */
      r = (a + b)*0.5;  /* 2.17.7 tweak */
      lua_pushnumber(L, (fabs(r) < eps) ? 0 : r);  /* apply `math.zeroin` */
    } else
      lua_pushundefined(L);
  }
  if (sided == 4) {  /* 2.9.8, return left and right-sided limit, as well */
    lua_pushnumber(L, fabs(a) < eps ? 0 : a);
    lua_pushnumber(L, fabs(b) < eps ? 0 : b);
  }
  return 1 + 2*(sided == 4);
}


/* Returns `true` if a real function f is continuous at the given point x (a number), and `false` otherwise.
  Three times faster than the Agena implementation. */
static int calc_iscont (lua_State *L) {  /* 2.9.7, 2.34.10 switch to longdoubles; 3.19.4 back to doubles */
  double x, a, b, fx, eps, step;
  int nargs, iters;
  (void)step; (void)iters;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  x = agn_checknumber(L, 2);
  aux_checkepsandstep(L, 3, &nargs, &eps, &step, &iters, "calc.iscont");
  /* luaL_checkstack is called by agnL_fncall */
  fx = agnL_fncall(L, 1, x, 3, nargs);  /* 2.9.8 */
  if (tools_isnan(fx))  /* 2.10.1, undefined ? */
    lua_pushfalse(L);
  else {
    a = agnL_fncall(L, 1, sun_nextafter(x, -HUGE_VAL), 3, nargs);  /* 2.9.8 */
    b = agnL_fncall(L, 1, sun_nextafter(x, HUGE_VAL), 3, nargs);  /* 2.9.8 */
    lua_pushboolean(L, tools_isfinite(a) && tools_isfinite(b) && tools_approx(a, fx, eps) && tools_approx(fx, b, eps));  /* 2.9.8 */
  }
  return 1;
}


/* Computes the Euclidean distance, i.e. the straight-line distance, of two points (a, f(a)) and
   (b, f(b)) on a curve defined by a function f in one real, in the Euclidean plane. a, b must be numbers.
   Around twice as fast than an Agena implementation. */
static int calc_eucliddist (lua_State *L) {  /* 2.9.7, 2.34.10 change to longdoubles */
  long double a, b, fa, fb;
  int nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  /* while (nargs >= 4 && lua_type(L, nargs) != LUA_TNUMBER) nargs--; */ /* ignore any options and other non-numbers, 3.12.1 */
  /* luaL_checkstack is called by agnL_fncall */
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  /* determine f(a) */
  fa = agnL_fncall(L, 1, a, 4, nargs);  /* 2.9.8 */
  /* determine f(b) */
  fb = agnL_fncall(L, 1, b, 4, nargs);  /* 2.9.8 */
  /* splitting the formula into several separate steps is not faster */
  lua_pushnumber(L, tools_hypotl(a - b, fa - fb));  /* 2.9.8 improvement */
  return 1;
}


/* The function checks for the differentiability of a function f at a point x. It computes the left and right-sided
   difference quotients of f at x. If both are equal and the left and right limit of f at x is equal to f(x), the
   function returns `true`, and `false` otherwise. 2.10.1, 2.34.10 change to longdoubles; 3.19.4 back to doubles */
static int calc_isdiff (lua_State *L) {
  int nargs, r, iters;
  double x, fx, eps, step, fph, fmh, f1ph, f1mh;
  (void)iters;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  x = agnL_checknumber(L, 2);
  aux_checkepsandstep (L, 3, &nargs, &eps, &step, &iters, "calc.isdiff");
  /* check if f is defined at x; luaL_checkstack is called by agnL_fncall */
  fx = agnL_fncall(L, 1, x, 3, nargs);
  if (!tools_isfinite(fx)) {  /* infinity or undefined ? */
    lua_pushfalse(L);
    return 1;
  }
  fmh = agnL_fncall(L, 1, x - eps, 3, nargs);
  f1mh = (fx - fmh)/eps;  /* left-sided difference quotient */
  fph = agnL_fncall(L, 1, x + eps, 3, nargs);
  f1ph = (fph - fx)/eps;  /* right-sided difference quotient */
  /* adaptive approach making eps larger, 3.3.7 fix */
  r = tools_approx(f1mh, f1ph, eps) && tools_approx(fmh, fx, eps) && tools_approx(fph, fx, eps);
  if (!r) {  /* adaptive approach by making eps larger, 3.3.7 fix for points where the graph of the function is steep */
    eps = sqrt(eps);
    r = tools_approx(f1mh, f1ph, eps) && tools_approx(fmh, fx, eps) && tools_approx(fph, fx, eps);
  }
  lua_pushboolean(L, r);
  lua_pushnumber(L, eps);
  return 2;
}


#ifdef __ARMCPU
#define opt_pow(x,n) sun_pow(x,n,1)
#else
#define opt_pow(x,n) tools_powil(x,n)
#endif

static int cheby (lua_State *L) {  /* 2.10.4, switched to longdoubles 2.34.10 */
  long double p, q, x, y, y2, d0, d1, d2;
  int i, deriv;
  LongDoubleArray *a;  /* changed 2.34.10 */
  x = agn_checknumber(L, 1);  /* get argument */
  a = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(1));
  p = getldblarray(a, a->size);
  q = getldblarray(a, a->size + 1);
  deriv = (int)getldblarray(a, a->size + 2);
  d1 = d2 = 0;
  d0 = getldblarray(a, a->size - 1);
  y = (2.0*(x - p)/(q - p)) - 1.0;  /* 7.3.8 fix to avoid catastrophic cancellation */
  if (deriv == 0) {  /* 3.3.2 */
    for (i=a->size - 2; i >= 0; i--) {
      d2 = d1; d1 = d0;
      y2 = 2.0L*y;
      d0 = y2*d1 - d2 + getldblarray(a, i);  /* term 2*y*d1 with factor 2 necessary for deriv = 0 */
    }
    lua_pushnumber(L, 0.5L*(d0 - d2));  /* fabs(q - p)^0 = 1 */
  } else {
    long double e0, e1, e2, f0, f1, f2, g0, g1, g2, h0, h1, h2, i0, i1, i2, j0, j1, j2, k0, k1, k2, l0, l1, l2;
    e1 = e2 = f1 = f2 = g1 = g2 = h1 = h2 = i1 = i2 = j1 = j2 = k1 = k2 = l1 = l2 = 0.0L;
    e0 = f0 = g0 = h0 = i0 = j0 = k0 = l0 = getldblarray(a, a->size - 1);
    for (i=a->size - 2; i >= 0; i--) {
      d2 = d1; d1 = d0;
      y2 = 2.0L*y;
      /* d0 = y2*d1 - d2 + getldblarray(a, i); */
      d0 = fmal(y2, d1, - d2 + getldblarray(a, i));
      if (deriv > 0 && i > 0) {  /* first derivative, `fall through` */
        e2 = e1; e1 = e0;
        /* e0 = d0 - e2 + y2*e1; */
        e0 = fmal(y2, e1, d0 - e2);  /* 3.10.5 accuracy tweak */
      }  /* continue statements slow down the function */
      if (deriv > 1 && i > 1) {  /* second derivative, `fall through` */
        f2 = f1; f1 = f0;
        /* f0 = e0 - f2 + y2*f1; */
        f0 = fmal(y2, f1, e0 - f2);  /* 3.10.5 accuracy tweak */
      }
      if (deriv > 2 && i > 2) {  /* third derivative, `fall through` */
        g2 = g1; g1 = g0;
        /* g0 = f0 - g2 + y2*g1; */
        g0 = fmal(y2, g1, f0 - g2);  /* 3.10.5 accuracy tweak */
      }
      if (deriv > 3 && i > 3) {  /* fourth derivative, `fall through` */
        h2 = h1; h1 = h0;
        /* h0 = g0 - h2 + y2*h1; */
        h0 = fmal(y2, h1, g0 - h2);  /* 3.10.5 accuracy tweak */
      }
      if (deriv > 4 && i > 4) {  /* fifth derivative, `fall through` */
        i2 = i1; i1 = i0;
        /* i0 = h0 - i2 + y2*i1; */
        i0 = fmal(y2, i1, h0 - i2);  /* 3.10.5 accuracy tweak */
      }
      if (deriv > 5 && i > 5) {  /* sixth derivative, `fall through`, 3.3.2 extension */
        j2 = j1; j1 = j0;
        /* j0 = i0 - j2 + y2*j1; */
        j0 = fmal(y2, j1, i0 - j2);  /* 3.10.5 accuracy tweak */
      }
      if (deriv > 6 && i > 6) {  /* seventh derivative, `fall through`, 3.3.2 extension */
        k2 = k1; k1 = k0;
        /* k0 = j0 - k2 + y2*k1; */
        k0 = fmal(y2, k1, j0 - k2);  /* 3.10.5 accuracy tweak */
      }
      if (deriv > 7 && i > 7) {  /* eighth derivative, `fall through`, 3.3.2 extension */
        l2 = l1; l1 = l0;
        /* l0 = k0 - l2 + y2*l1; */
        l0 = fmal(y2, l1, k0 - l2);  /* 3.10.5 accuracy tweak */
      }
    }
    switch (deriv) {  /* factors according to https://sequencedb.net/s/A051711 with a(0) = 1; for n > 0, a(n) = n!*4^n/2 */
      /* substituting call to tools_intpow with z*z*.. is not faster.
         Do not change the order of the factors in the 80-bit version ! */
      case 1:
        lua_pushnumber(L,          2.0L*(e0 - e2)/fabsl(q - p)); break;  /* fabs(q - p)^1 = fabs(q - p) */
      case 2:
        lua_pushnumber(L,         16.0L*(f0 - f2)/opt_pow(fabsl(q - p), 2)); break;
      case 3:
        lua_pushnumber(L,        192.0L*(g0 - g2)/opt_pow(fabsl(q - p), 3)); break;
      case 4:
        lua_pushnumber(L,       3072.0L*(h0 - h2)/opt_pow(fabsl(q - p), 4)); break;
      case 5:
        lua_pushnumber(L,      61440.0L*(i0 - i2)/opt_pow(fabsl(q - p), 5)); break;
      case 6:  /* 3.3.2 extension */
        lua_pushnumber(L,    1474560.0L*(j0 - j2)/opt_pow(fabsl(q - p), 6)); break;
      case 7:  /* 3.3.2 extension */
        lua_pushnumber(L,   41287680.0L*(k0 - k2)/opt_pow(fabsl(q - p), 7)); break;
      case 8:  /* 3.3.2 extension */
        lua_pushnumber(L, 1321205760.0L*(l0 - l2)/opt_pow(fabsl(q - p), 8)); break;
      default:  /* should not happen */
        luaL_error(L, "Error in " LUA_QS ": cannot compute derivative.", "calc.cheby");
    }
  }
  return 1;
}

#undef opt_pow

/* Returns a function computing the Chebyshev interpolant for a given point. `f` is the function to be interpolated,
   p and q represent the domain of the definition, `n` is the order of the interpolant. As a rule of thumb, the larger
   the domain, the larger n should be.

   You may optionally pass the `deriv=k` option as the very last argument to compute the first (k=1), second (k=2)
   or third (k=3) derivative, where k defaults to 0, i.e. the function itself.

   Using this function may speed up numeric computations significantly if the expression to be evaluated consists of
   many subexpressions - and if accuracy is not of primary concern. See also: calc.chebycoeffs.
   The function is thrice as fast as an Agena implementation. */

static int cheby64 (lua_State *L);
static int calc_cheby64 (lua_State *L);

static int calc_cheby (lua_State *L) {  /* 2.10.4, switched to longdoubles 2.34.10 */
#ifdef __ARMCPU
  return calc_cheby64(L);
#else
  long double p, q, x, k, f, *y;
  int i, j, nops, nargs, deriv, sided, errorcode;
  lua_Number eps, delta, step;
  LongDoubleArray *a;
  (void)eps; (void)delta; (void)sided; (void)step;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  /* luaL_checkstack is called by agnL_fncall */
  p = agn_checknumber(L, 2);
  q = agn_checknumber(L, 3);
  if (p > q)
    luaL_error(L, "Error in " LUA_QS ": invalid domain %LF .. %LF.", "calc.cheby", p, q);
  nops = agn_checkposint(L, 4);
  /* the default for deriv will be 0, that is the function itself ! */
  aux_geteps(L, 0.0, 5, &nargs, &eps, &delta, &step, &sided, &deriv, 0, 0, 8, 0, "calc.cheby");
  /* pushes LongDoubleArray userdata of Chebyshev coefficient vector onto stack with three additional information, see below */
  if (nops - deriv < 2)  /* 3.3.2 change */
    luaL_error(L, "Error in " LUA_QS ": vector would have too few coefficients.", "calc.cheby");
  aux_pushud(L, a, nops + 3, "calc.cheby");
  a->size = nops;
  y = malloc(nops*sizeof(long double));
  if (y == NULL) {  /* 3.3.2 improvement */
    agn_poptop(L);  /* remove LongDoubleArray userdata from stack */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "calc.cheby");
  }
  k = M_PIld/(2.0L*nops);
  for (i=0; i < nops; i++) {
    /* 7.3.8 accuracy fix; do NOT use x = 0.5*(p + q + sun_cos(k*(2.0*i + 1))*(q - p)); as this is imprecise on ARM */
    x = (p + q)/2.0L + sun_cosl(k*(2.0L*i + 1.0L))*(q - p)/2.0L;
    y[i] = agnL_fncallx(L, 1, x, 5, nargs, &errorcode, 0);
    if (errorcode != 0) {  /* 7.3.4 fix */
      xfree(y);
      agn_poptop(L);  /* pop userdata */
      luaL_error(L, "Error in " LUA_QS ": function call did not return a number.", "calc.cheby");
    }
  }
  f = 2.0L/nops;
  for (i=0; i < nops; i++) {
    setldblarray(a, i, 0.0L);
    for (j=0; j < nops; j++) {
      setldblarray(a, i, fmal(y[j], sun_cosl(k*(i*(2.0L*j + 1))), getldblarray(a, i)));  /* 4.12.8 change */
    }
    a->v[i] *= f;
  }
  xfree(y);
  setldblarray(a, nops++, p);  /* pack domain p .. q into LongDoubleArray just for speed ... */
  setldblarray(a, nops++, q);
  setldblarray(a, nops, deriv);  /* ... and derivative to be computed, as well */
  lua_pushcclosure(L, &cheby, 1);
  return 1;
#endif
}


static int cheby64 (lua_State *L) {
  double p, q, x, y, y2, d0, d1, d2;
  int i, deriv;
  DoubleArray *a;
  x = agn_checknumber(L, 1);  /* get argument */
  a = (DoubleArray *)lua_touserdata(L, lua_upvalueindex(1));
  p = getdblarray(a, a->size);
  q = getdblarray(a, a->size + 1);
  deriv = (int)getdblarray(a, a->size + 2);
  d1 = d2 = 0;
  d0 = getdblarray(a, a->size - 1);
  y = (2.0*(x - p)/(q - p)) - 1.0;  /* 7.3.8 fix to avoid catastrophic cancellation */
  if (deriv == 0) {
    for (i=a->size - 2; i >= 0; i--) {
      d2 = d1; d1 = d0;
      y2 = 2.0*y;
      d0 = fma(y2, d1, -d2 + getdblarray(a, i));  /* term 2*y*d1 with factor 2 necessary for deriv = 0; 4.12.8 change */
    }
    lua_pushnumber(L, 0.5*(d0 - d2));  /* fabs(q - p)^0 = 1 */
  } else {
    double e0, e1, e2, f0, f1, f2, g0, g1, g2, h0, h1, h2, i0, i1, i2, j0, j1, j2, k0, k1, k2, l0, l1, l2;
    e1 = e2 = f1 = f2 = g1 = g2 = h1 = h2 = i1 = i2 = j1 = j2 = k1 = k2 = l1 = l2 = 0.0;
    e0 = f0 = g0 = h0 = i0 = j0 = k0 = l0 = getdblarray(a, a->size - 1);
    for (i=a->size - 2; i >= 0; i--) {
      d2 = d1; d1 = d0;
      y2 = 2.0*y;
      /* d0 = y2*d1 - d2 + getdblarray(a, i); */
      d0 = fma(y2, d1, -d2 + getdblarray(a, i));
      if (deriv > 0 && i > 0) {  /* first derivative, `fall through` */
        e2 = e1; e1 = e0;
        /* e0 = d0 - e2 + y2*e1; */
        e0 = fma(y2, e1, d0 - e2);
      }  /* continue statements slow down the function */
      if (deriv > 1 && i > 1) {  /* second derivative, `fall through` */
        f2 = f1; f1 = f0;
        /* f0 = e0 - f2 + y2*f1; */
        f0 = fma(y2, f1, e0 - f2);
      }
      if (deriv > 2 && i > 2) {  /* third derivative, `fall through` */
        g2 = g1; g1 = g0;
        /* g0 = f0 - g2 + y2*g1; */
        g0 = fma(y2, g1, f0 - g2);
      }
      if (deriv > 3 && i > 3) {  /* fourth derivative, `fall through` */
        h2 = h1; h1 = h0;
        /* h0 = g0 - h2 + y2*h1; */
        h0 = fma(y2, h1, g0 - h2);
      }
      if (deriv > 4 && i > 4) {  /* fifth derivative, `fall through` */
        i2 = i1; i1 = i0;
        /* i0 = h0 - i2 + y2*i1; */
        i0 = fma(y2, i1, h0 - i2);
      }
      if (deriv > 5 && i > 5) {  /* sixth derivative, `fall through` */
        j2 = j1; j1 = j0;
        /* j0 = i0 - j2 + y2*j1; */
        j0 = fma(y2, j1, i0 - j2);
      }
      if (deriv > 6 && i > 6) {  /* seventh derivative, `fall through` */
        k2 = k1; k1 = k0;
        /* k0 = j0 - k2 + y2*k1; */
        k0 = fma(y2, k1, j0 - k2);
      }
      if (deriv > 7 && i > 7) {  /* eighth derivative, `fall through` */
        l2 = l1; l1 = l0;
        /* l0 = k0 - l2 + y2*l1; */
        l0 = fma(y2, l1, k0 - l2);
      }
    }
    switch (deriv) {  /* factors according to https://sequencedb.net/s/A051711 with a(0) = 1; for n > 0, a(n) = n!*4^n/2 */
      /* substituting call to tools_intpow with z*z*.. is not faster; order of the factors changed in the 64-bit precision
         version 4.12.8 */
      case 1:
        lua_pushnumber(L,          (2.0/fabs(q - p))*(e0 - e2)); break;  /* fabs(q - p)^1 = fabs(q - p) */
      case 2:
        lua_pushnumber(L,         (16.0/sun_pow(fabs(q - p), 2, 1))*(f0 - f2)); break;
      case 3:
        lua_pushnumber(L,        (192.0/sun_pow(fabs(q - p), 3, 1))*(g0 - g2)); break;
      case 4:
        lua_pushnumber(L,       (3072.0/sun_pow(fabs(q - p), 4, 1))*(h0 - h2)); break;
      case 5:
        lua_pushnumber(L,      (61440.0/sun_pow(fabs(q - p), 5, 1))*(i0 - i2)); break;
      case 6:
        lua_pushnumber(L,    (1474560.0/sun_pow(fabs(q - p), 6, 1))*(j0 - j2)); break;
      case 7:
        lua_pushnumber(L,   (41287680.0/sun_pow(fabs(q - p), 7, 1))*(k0 - k2)); break;
      case 8:
        lua_pushnumber(L, (1321205760.0/sun_pow(fabs(q - p), 8, 1))*(l0 - l2)); break;
      default:  /* should not happen */
        luaL_error(L, "Error in " LUA_QS ": cannot compute derivative.", "calc.cheby64");
    }
  }
  return 1;
}

static int calc_cheby64 (lua_State *L) {
  double p, q, x, k, f, *y;
  volatile double cs, ccs;
  int i, j, nops, nargs, deriv, sided, errorcode;
  lua_Number eps, delta, step;
  DoubleArray *a;
  (void)eps; (void)delta; (void)sided; (void)step;
  cs = ccs = 0.0;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  /* luaL_checkstack is called by agnL_fncall */
  p = agn_checknumber(L, 2);
  q = agn_checknumber(L, 3);
  if (p > q)
    luaL_error(L, "Error in " LUA_QS ": invalid domain %LF .. %LF.", "calc.cheby64", p, q);
  nops = agn_checkposint(L, 4);
  /* the default for deriv will be 0, that is the function itself ! */
  aux_geteps(L, 0.0, 5, &nargs, &eps, &delta, &step, &sided, &deriv, 0, 0, 8, 0, "calc.cheby64");
  /* pushes DoubleArray userdata of Chebyshev coefficient vector onto stack with three additional information, see below */
  if (nops - deriv < 2)  /* 3.3.2 change */
    luaL_error(L, "Error in " LUA_QS ": vector would have too few coefficients.", "calc.cheby64");
  aux_pushud64(L, a, nops + 3, "calc.cheby64");
  a->size = nops;
  y = malloc(nops*sizeof(double));
  if (y == NULL) {  /* 3.3.2 improvement */
    agn_poptop(L);  /* remove DoubleArray userdata from stack */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "calc.cheby64");
  }
  k = PI/(2.0*nops);
  for (i=0; i < nops; i++) {
    /* 7.3.8 accuracy fix; do NOT use x = 0.5*(p + q + sun_cos(k*(2.0*i + 1))*(q - p)); as this is imprecise on ARM */
    x = (p + q)/2.0 + sun_cos(k*(2*i + 1))*(q - p)/2.0;
    y[i] = agnL_fncallx(L, 1, x, 5, nargs, &errorcode, 0);
    if (errorcode != 0) {  /* 7.3.4 fix */
      xfree(y);
      agn_poptop(L);  /* pop userdata */
      luaL_error(L, "Error in " LUA_QS ": function call did not return a number.", "calc.cheby64");
    }
  }
  f = 2.0/nops;
  for (i=0; i < nops; i++) {
    setldblarray(a, i, 0.0);
    for (j=0; j < nops; j++) {
      /* tools_kbadd is better than fma here */
      setldblarray(a, i, tools_kbadd(getdblarray(a, i), y[j]*sun_cos(k*(i*(2.0*j + 1))), &cs, &ccs));
    }
    setldblarray(a, i, (getdblarray(a, i) + cs + ccs)*f);
    cs = ccs = 0;
  }
  xfree(y);
  setldblarray(a, nops++, p);  /* pack domain p .. q into DoubleArray just for speed ... */
  setldblarray(a, nops++, q);
  setldblarray(a, nops, deriv);  /* ... and derivative to be computed, as well */
  lua_pushcclosure(L, &cheby64, 1);
  return 1;
}


static int calc_chebycoeffs (lua_State *L) {  /* 3.3.2, based on calc.cheby */
  long double p, q, x, k, f, *a, *y;
  int i, j, nargs, nops, errorcode;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  /* luaL_checkstack is called by agnL_fncall */
  p = agn_checknumber(L, 2);
  q = agn_checknumber(L, 3);
  if (p > q)
    luaL_error(L, "Error in " LUA_QS ": invalid domain %LF .. %LF.", "calc.chebycoeffs", p, q);
  nops = agn_checkposint(L, 4);
  a = agn_malloc(L, nops*sizeof(long double), "calc.chebycoeffs", NULL);
  y = agn_malloc(L, nops*sizeof(long double), "calc.chebycoeffs", a, NULL);  /* 4.11.5 fix */
  k = M_PIld/(2.0*nops);
  for (i=0; i < nops; i++) {
    /* 7.3.8 accuracy fix; do NOT use x = 0.5*(p + q + sun_cos(k*(2.0*i + 1))*(q - p)); as this is imprecise on ARM */
    x = (p + q)/2.0L + sun_cos(k*(2.0L*i + 1.0L))*(q - p)/2.0L;
    y[i] = agnL_fncallx(L, 1, x, 5, nargs, &errorcode, 0);
    if (errorcode != 0) {  /* 7.3.4 fix */
      xfreeall(a, y);
      luaL_error(L, "Error in " LUA_QS ": function call did not return a number.", "calc.chebycoeffs");
    }
  }
  f = 2.0L/nops;
  for (i=0; i < nops; i++) {
    a[i] = 0.0L;
    for (j=0; j < nops; j++) {
      a[i] += y[j]*sun_cosl(k*(i*(2.0L*j + 1)));
    }
    a[i] *= f; /* 2.0/nops; */
  }
  xfree(y);
  lua_createtable(L, nops + 2, 1);
  for (i=0; i < nops; i++) {
    agn_setinumber(L, -1, i + 1, a[i]);
  }
  xfree(a);
  lua_pushstring(L, "domain");
  agn_createpairnumbers(L, p, q);
  lua_rawset(L, -3);
  return 1;
}


static int calc_chebygen (lua_State *L) {  /* 3.3.2 */
  size_t nops;
  int nargs, deriv, sided;
  lua_Number p0, q0, eps, delta, step;
  long double p, q;
  LongDoubleArray *a = NULL;
  LongDoubleArray *b = NULL;
  (void)eps; (void)delta; (void)sided; (void)step;
  luaL_checktype(L, 1, LUA_TTABLE);
  nargs = lua_gettop(L);
  nops = agn_asize(L, 1);
  lua_getfield(L, 1, "domain");  /* pushes domain field onto the stack */
  agnL_pairgetinumbers(L, "calc.chebygen", -1, &p0, &q0);  /* pops the pair */
  p = p0; q = q0;
  if (p >= q)
    luaL_error(L, "Error in " LUA_QS ": invalid coefficient table received, invalid borders in domain.", "calc.chebygen");
  /* the default for deriv will be 0, that is the function itself ! */
  aux_geteps(L, 0.0, 2, &nargs, &eps, &delta, &step, &sided, &deriv, 0, 0, 8, 0, "calc.chebygen");
  /* pushes LongDoubleArray userdata of Chebyshev coefficient vector onto stack with three additional information, see below */
  aux_pushcoeffs(L, a, b, nops, "calc.chebygen", 0, 1);  /* create new userdata and assign table values to it; ud is now at the stack top */
  a = (LongDoubleArray *)lua_touserdata(L, -1);
  a->v = realloc(a->v, (nops + 3)*sizeof(long double));  /* extend for p, q, deriv */
  if (a->v == NULL)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "calc.chebygen");
  a->size = nops;
  setldblarray(a, nops++, p);  /* pack domain p .. q into LongDoubleArray just for speed ... */
  setldblarray(a, nops++, q);
  setldblarray(a, nops, deriv);  /* ... and derivative to be computed, as well */
  lua_pushcclosure(L, &cheby, 1);
  return 1;
}


/* Computes a Savitzky–Golay filter for the univariate function f to smooth its data by returning a factory interpolating f
   at a point.

   It fits successive subsets of neighbouring data with a low-degree polynomial using the linear least-square method. By
   default, 15 equally-spaced points to the left of x and 15 equally-spaced points points to the right of x are examined.
   You can change this `window` by passing another odd value with the 'points' option. All adjacent points are separated by
   distance eps which is 1e-5 by default. You can change the distance with the 'eps' option.

   Alternatively it can also compute derivatives of any degree n by passing the option deriv = <n>. The larger the degree n
   of the derivative, however, the least accurate the results will become.

   The degree d of the smoothing least-square polynomial is 3 by default and can be changed by the degree=<d> option. Recommended
   degrees are d = 2 or 4, with d not exceeding 6.

   The function automatically determines the most suitable settings for the window and the spacing eps of its points,
   but you can switch this off by passing the adaptive=false option.

   See also: calc.savgolcoeffs. */
/* Improved by Gemini AI, 7.3.8 */
#ifndef __ARMCPU
static int savgol (lua_State *L) {
  long double x, signal, eps, compensation;
  int i, c, nleft, nright, deriv;
  long double fact, val;  /* Changed to long double to avoid overflow before use */
  LongDoubleArray *a;
  if (agn_isnumber(L, 1)) {
    x = agn_tonumber(L, 1);
  } else if (isdlong(L, 1)) {
    x = getdlongvalue(L, 1);
  } else {
    luaL_error(L, "Error in " LUA_QS " factory: argument must be a number or long double.", "calc.savgol");
  }
  x = agn_checknumber(L, 1);
  a = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(1));
  /* Assume a->size contains coefficients, followed by metadata */
  nleft  = (int)getldblarray(a, a->size);
  nright = (int)getldblarray(a, a->size + 1);
  deriv  = (int)getldblarray(a, a->size + 2);
  eps    = (long double)getldblarray(a, a->size + 3);
  signal = 0.0L;
  compensation = 0.0L; /* Kahan-Babuska compensation */
  c = 0;
  for (i=nleft; i <= nright; i++) {
    luaL_checkstack(L, 2, "not enough stack space");
    /* Get the function value at the offset point */
    /* val = (long double)agnL_fncall(L, lua_upvalueindex(2), (double)(x + i*eps), 10, 9); */
    lua_pushvalue(L, lua_upvalueindex(2));
    createdlong(L, x + i*eps);
    lua_call(L, 1, 1);
    if (agn_isnumber(L, -1)) {
      val = (long double)agn_tonumber(L, -1);
    } else if (isdlong(L, -1)) {
      val = getdlongvalue(L, -1);
    } else {
      val = 0.0L;  /* to prevent compiler warnings */
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS " factory: function must evaluate to number or long double.", "calc.savgol");
    }
    agn_poptop(L);
    long double term = getldblarray(a, c++) * val;
    /* Kahan-Babuska Summation for signal accumulation */
    long double y_kb = term - compensation;
    long double t_kb = signal + y_kb;
    compensation = (t_kb - signal) - y_kb;
    signal = t_kb;
  }
  /* Compute n! precisely */
  fact = 1.0L;
  for (i=1; i <= deriv; i++) fact *= (long double)i;
  /* * Optimization: Instead of (fact / pow(eps, deriv)),
   * we use the reciprocal of eps and apply it iteratively to maintain
   * better exponent health on ARM's 128-bit long double pipeline. */
  long double inv_eps = 1.0L/eps;
  long double scaling = fact;
  for (i = 0; i < deriv; i++) {
    scaling *= inv_eps;
  }
  lua_pushnumber(L, (double)(signal*scaling));
  return 1;
}
#endif

/* This version for ARM gives somewhat better results for derivatives, but not much. */
static int savgol64 (lua_State *L) {
  double x, eps;
  int i, c, nleft, nright, deriv;
  long double fact, scaling;  /* Changed to long double to avoid overflow before use */
  LongDoubleArray *a;
  /* Double-Double accumulator */
  dd_pair signal_dd = {0.0, 0.0};
  x = agn_checknumber(L, 1);
  a = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(1));
  /* Assume a->size contains coefficients, followed by metadata */
  nleft  = (int)getldblarray(a, a->size);
  nright = (int)getldblarray(a, a->size + 1);
  deriv  = (int)getldblarray(a, a->size + 2);
  eps  = getldblarray(a, a->size + 3);
  c = 0;
  for (i=nleft; i <= nright; i++) {
    /* 1. Get function value */
    double val = (double)agnL_fncall(L, lua_upvalueindex(2), (double)(x + i*eps), 10, 9);
    /* 2. Get coefficient as LONG DOUBLE to preserve all bits */
    long double ld_coeff = getldblarray(a, c++);
    /* 3. Convert long double to dd_pair (This is the secret sauce) */
    dd_pair coeff_dd;
    coeff_dd.hi = (double)ld_coeff;
    coeff_dd.lo = (double)(ld_coeff - (long double)coeff_dd.hi);
    /* 4. Multiply: coeff_dd * val_double */
    dd_pair term = dd_mul_d(coeff_dd, val);
    /* 5. Accumulate into signal */
    signal_dd = dd_add(signal_dd, term);
  }
  /* Compute n! / eps^deriv using long double */
  fact = 1.0L;
  for (i=1; i <= deriv; i++) fact *= (long double)i;
  scaling = fact;
  long double inv_eps = 1.0L/(long double)eps;
  for (i=0; i < deriv; i++) scaling *= inv_eps;
  /* Apply scaling to the DD result. This prevents the 1e-16 error from being amplified into 1e-7. */
  dd_pair final_res = dd_mul_d(signal_dd, (double)scaling);
  lua_pushnumber(L, dd2double(final_res));
  return 1;
}

static void aux_checksavgol (lua_State *L, int pos, int *nargs, int *deriv, int *npoints, long double *eps,
     int *polydeg, int *isadaptive, const char *procname) {
  const char *option;
  int checkoptions = 5;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 7.6.4 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);
    if (!agn_isstring(L, -1)) {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": left-hand side of option must be a string.", procname);
    }
    option = lua_tostring(L, -1);
    if (tools_streq(option, "deriv")) {  /* 2.16.12 tweak */
      agn_pairgeti(L, *nargs, 2);
      if (agn_isnumber(L, -1)) {
        *deriv = lua_tointeger(L, -1);
        agn_poptop(L);
        if (*deriv < 0) {
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": deriv must be non-negative.", procname);
        }
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": right-hand side of deriv option must be a number.", procname);
      }
    } else if (tools_streq(option, "points")) {  /* 2.16.12 tweak */
      agn_pairgeti(L, *nargs, 2);
      if (agn_isnumber(L, -1)) {
        *npoints = lua_tointeger(L, -1);
        agn_poptop(L);
        if (*npoints < 1 || *npoints % 2 == 0) {
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": number of points must be positive and odd.", procname);
        }
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": right-hand side of points option must be a number.", procname);
      }
    } else if (tools_streq(option, "eps")) {  /* 2.16.12 tweak */
      agn_pairgeti(L, *nargs, 2);
      if (agn_isnumber(L, -1)) {
        *eps = agn_tonumber(L, -1);
        agn_poptop(L);
        if (*eps <= 0) {
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": eps must be positive.", procname);
        }
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": right-hand side of eps option must be a number.", procname);
      }
    } else if (tools_streq(option, "degree")) {  /* 2.16.12 tweak */
      agn_pairgeti(L, *nargs, 2);
      if (agn_isnumber(L, -1)) {
        *polydeg = lua_tointeger(L, -1);
        agn_poptop(L);
        if (*polydeg <= 0) {
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": degree must be positive.", procname);
        }
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": right-hand side of degree option must be a number.", procname);
      }
    } else if (tools_streqx(option, "adapt", "adaptive", NULL)) {  /* 2.16.12 tweak */
      agn_pairgeti(L, *nargs, 2);
      if (lua_isboolean(L, -1)) {  /* 7.3.9 fix */
        *isadaptive = lua_toboolean(L, -1);
        agn_poptop(L);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": right-hand side of adaptive option must be a boolean.", procname);
      }
    } else {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": unknown option.", procname);
    }
    (*nargs)--;
  }
}

static int aux_call_savgolcoeffs (lua_State *L, int nleft, int nright, int deriv, int polydeg, int isadaptive, const char *procname) {
  luaL_checkstack(L, 5, "not enough stack space");  /* 2.31.7 fix */
  if (agnL_gettablefield(L, "calc", "savgolcoeffs", procname, 1) != LUA_TFUNCTION)
    luaL_error(L, "Error in " LUA_QS ": could not find `calc.savgolcoeffs`.", procname);
  lua_pushinteger(L, -nleft);
  lua_pushinteger(L, nright);
  lua_pushinteger(L, deriv);
  lua_pushinteger(L, polydeg + deriv*isadaptive);
  lua_call(L, 4, 1);
  if (!lua_isreg(L, -1)) {
    lua_pop(L, 5);
    luaL_error(L, "Error in " LUA_QS ": coefficient vector is invalid.", procname);
  }
  return agn_regsize(L, -1);
}

static void aux_setldblarray (LongDoubleArray *a, int nops, int nleft, int nright, int deriv, long double eps) {
  setldblarray(a, nops++, nleft);
  setldblarray(a, nops++, nright);
  setldblarray(a, nops++, deriv);
  setldblarray(a, nops, eps);
}

static int calc_savgol (lua_State *L) {  /* 2.10.4, 2.34.10 */
  int i, npoints, nleft, nright, polydeg, isadaptive, deriv, nops, nargs;
  long double eps, factor;
  LongDoubleArray *a;
  eps = 1e-5;      /* distance between sample points */
  npoints = 31;    /* number of neighbouring points to be examined */
  deriv = 0;       /* n-th derivative, default: compute smoothed function */
  polydeg = 3;     /* polynomial degree for smoothing */
  isadaptive = 1;  /* use adaptive measures to boost precision depending on the degree of the derivative */
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  aux_checksavgol(L, 2, &nargs, &deriv, &npoints, &eps, &polydeg, &isadaptive, "calc.savgol");
  factor = (isadaptive == 1) ? deriv + 1 : 1;
  eps *= factor;
  nright = 0.5*npoints*factor;  /* 2.17.7 tweak */
  nleft = -nright;
  nops = aux_call_savgolcoeffs(L, nleft, nright, deriv, polydeg, isadaptive, "calc.savgol");
  /* pushes LongDoubleArray userdata of Savgol coefficient vector onto stack that includes four additional information, see below */
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.18.4 fix */
  aux_pushud(L, a, nops + 4, "calc.savgol");
  a->size = nops;
  for (i=0; i < nops; i++) {
    agn_regrawgeti(L, -2, i + 1);
    setldblarray(a, i, ((DLong *)luaL_checkudata(L, -1, "longdouble"))->value);
    agn_poptop(L);
  }
  lua_remove(L, -2);
  aux_setldblarray(a, nops, nleft, nright, deriv, eps);
  lua_pushvalue(L, 1);  /* push function */
#ifndef __ARMCPU
  lua_pushcclosure(L, &savgol, 2);
#else
  lua_pushcclosure(L, &savgol64, 2);
#endif
  return 1;
}


static int calc_savgol64 (lua_State *L) {  /* 7.4.0 */
  int i, npoints, nleft, nright, polydeg, isadaptive, deriv, nops, nargs;
  long double eps, factor;
  LongDoubleArray *a;
  eps = 1e-5;      /* distance between sample points */
  npoints = 31;    /* number of neighbouring points to be examined */
  deriv = 0;       /* n-th derivative, default: compute smoothed function */
  polydeg = 3;     /* polynomial degree for smoothing */
  isadaptive = 1;  /* use adaptive measures to boost precision depending on the degree of the derivative */
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  aux_checksavgol(L, 2, &nargs, &deriv, &npoints, &eps, &polydeg, &isadaptive, "calc.savgol64");
  factor = (isadaptive == 1) ? deriv + 1 : 1;
  eps *= factor;
  nright = 0.5*npoints*factor;  /* 2.17.7 tweak */
  nleft = -nright;
  nops = aux_call_savgolcoeffs(L, nleft, nright, deriv, polydeg, isadaptive, "calc.savgol64");
  /* pushes LongDoubleArray userdata of Savgol coefficient vector onto stack that includes four additional information, see below */
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.18.4 fix */
  aux_pushud(L, a, nops + 4, "calc.savgol64");
  a->size = nops;
  for (i=0; i < nops; i++) {
    agn_regrawgeti(L, -2, i + 1);
    setldblarray(a, i, ((DLong *)luaL_checkudata(L, -1, "longdouble"))->value);
    agn_poptop(L);
  }
  lua_remove(L, -2);
  aux_setldblarray(a, nops, nleft, nright, deriv, eps);
  lua_pushvalue(L, 1);  /* push function */
  lua_pushcclosure(L, &savgol64, 2);
  return 1;
}


/* The function receives a non-negative integer n and any number x and returns 0 if x < 0, 1 if x > 1, and smoothly interpolates,
   using an (2*n+1)'th-degree Hermite polynomial, between 0 and 1, otherwise.

   The slope of the smoothstep function is zero at both edges, so the result is differentiable over the whole real domain.

   Wikipedia: `Smoothstep is a family of sigmoid-like interpolation and clamping functions commonly used in computer graphics and
   video game engines`, for example to naturally accelerate or decelerate an object.

   See also: http://sol.gfxile.net/interpolation, https://en.wikipedia.org/wiki/Smoothstep */
static int calc_smoothstep (lua_State *L) {  /* 2.10.4 */
  long double x, r;
  lua_Integer a;
  int n, KenPerlin;
  x = agn_checknumber(L, 1);
  if (agn_isstring(L, 2) && tools_streq(agn_tostring(L, 2), "perlin")) {  /* 2.14.1, 2.16.12 tweak */
    a = 0; KenPerlin = 1;
  } else {
    a = agn_checknonnegint(L, 2); KenPerlin = 0;
  }
  r = 0;
  if (x < 0) r = 0;
  else if (x > 1) r = 1;
  else {
    /* 2.14.1, see: https://en.wikipedia.org/wiki/Smoothstep: "[Kenneth] Perlin suggests an improved version of the smoothstep function,
       which has zero 1st- and 2nd-order derivatives at x = 0 and x = 1" */
    if (KenPerlin) {
      r = x*x*x*(x*(x*6 - 15) + 10);
    } else {
      for (n=0; n < a; n++) {
        /* for formula, see: https://stackoverflow.com/questions/41195063/general-smoothstep-equation */
        r += tools_powil(-1, n)*tools_powil(x, a + n) *
             tools_binomial(a + n - 1, n)*tools_binomial(2*a - 1, a - n - 1);
      }
    }
  }
  lua_pushnumber(L, r);
  return 1;
}


/* Computes the sigmoid function, having a characteristic `S`-shaped curve or sigmoid curve, for any number x.
   The result is a number between - but excluding - 0 and 1. See: https://en.wikipedia.org/wiki/Sigmoid_function */
static int calc_sigmoid (lua_State *L) {  /* 2.10.4, 2.34.10 switch to long doubles */
  long double x, t;
  x = agn_checknumber(L, 1);
  t = tools_expl(x);
  lua_pushnumber(L, t/(t + 1));
  return 1;
}


/* The logit function is the inverse of the sigmoid, or logistic function and is defined as: logit(x) := ln(x/(1-x)) =
   2*atanh(2*x-1). If x = 0, it will return -infinity, and if x = 1 the result will be +infinity. 2.30.2 */
static int calc_logit (lua_State *L) {
  long double x, r;
  x = agn_checknumber(L, 1);
  r = 2*sun_atanhl(2*x - 1);  /* 25 % faster than sun_log(x) - sun_log(1 - x) */
  if (tools_fpisnanl(r)) {
    /* extend domain to [0, 1]; See: https://calculus.subwiki.org/wiki/Inverse_logistic_function */
    if (x == 0) r = -HUGE_VAL;
    else if (x == 1) r = HUGE_VAL;
  }
  lua_pushnumber(L, r);
  return 1;
}


/* Computes the probit function, the inverse of the cumulative distribution function of the standard normal distribution:
   sqrt(2)*inverf(2p-1). 2.30.2 */
static int calc_probit (lua_State *L) {
  lua_Number x, r;
  x = agn_checknumber(L, 1);
  r = -SQRT2*tools_inverfc(2*x);
  if (r == -0) r = 0;
  if (isnan(r)) {
    /* extend domain to [0, 1]; See: https://calculus.subwiki.org/wiki/Inverse_logistic_function */
    if (x == 0) r = -HUGE_VAL;
    else if (x == 1) r = HUGE_VAL;
  }
  lua_pushnumber(L, r);
  return 1;
}


/* calc.logistic(x [, max [, k [, x0]]])

   Computes the logistic function, having a characteristic `S`-shaped curve or sigmoid curve, for any number x, according
   to the formula

   L(x) = max/(1 + exp(-k*(x - x0))),

   where max is the curve's maximum value, k its steepness, and x0 the x-value of the sigmoid's midpoint. By default,
   max = 1, k = 1 and x0 = 0, computing the sigmoid function. If only x is given, it works like `calc.sigmoid`.

   The result is a number between - but excluding - 0 and 1. See: https://en.wikipedia.org/wiki/Logistic_function */
static int calc_logistic (lua_State *L) {  /* 2.10.4, 2.34.10 switch to long double */
  long double x;
  x = agn_checknumber(L, 1);
  if (lua_gettop(L) == 1) {  /* = sigmoid function */
    long double t = tools_expl(x);  /* 2.41.0 fix */
    lua_pushnumber(L, t/(t + 1.0L));
  } else {
    long double x0, maxi, k;
    maxi = agnL_optnumber(L, 2, 1.0);  /* curve's maximum value */
    k = agnL_optnumber(L, 3, 1.0);     /* steepness of the curve */
    x0 = agnL_optnumber(L, 4, 0.0);    /* x-value of the sigmoid's midpoint */
    lua_pushnumber(L, maxi/(1.0L + tools_expl(-k*(x - x0))));
  }
  return 1;
}


/* Computes the Softsign function f(x) = x/(1 + |x|). 40 percent faster than an Agena implementation */
static int calc_softsign (lua_State *L) {  /* 2.14.1, 2.34.10 switch to long double */
  long double x;
  x = agn_checknumber(L, 1);
  lua_pushnumber(L, x/(1 + fabsl(x)));
  return 1;
}


/* Computes the Gaussian function a*exp(-(x-b)^2/(2*c^2)) where at a real or complex point x, with a, b, c
   being (real) numbers. By default, a = 1, b = 0, c = 1/sqrt(2). The return depends on the type of x.

   See also: expx2.

   See: https://en.wikipedia.org/wiki/Gaussian_function; formula computed using pencil, paper and Maple V Release 4. */
static int calc_gaussian (lua_State *L) {  /* 2.10.5 */
  lua_Number a, b, c;
  a = agnL_optnumber(L, 2, 1);
  b = agnL_optnumber(L, 3, 0);
  c = agnL_optnumber(L, 4, M_SQRT1_2);  /* = 1/sqrt(2) = sqrt(2)/2 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x;
      x = agn_tonumber(L, 1);
      lua_pushnumber(L, a*tools_expx2(M_SQRT1_2*(x - b)/c, -1));
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number x, y, re, im;
      agn_getcmplxparts(L, 1, &x, &y);  /* 7.1.4 change */
      re = tools_cexpx2(0.5*(M_SQRT2*(x - b)/c), M_SQRT1_2*y/c , -1, &im);
      agn_pushcomplex(L, a*re, a*im);
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "calc.gaussian");
  }
  return 1;
}


/* Computes the Weierstraß function (formula) and returns a number. The precision is given by its fourth argument, eps, which is `Eps` by default. */
static int calc_weier (lua_State *L) {  /* 2.12.0 RC 4 */
#ifndef __ARMCPU  /* 2.37.1 */
  volatile long double a, b, x, r, eps, z, q;  /* changed 2.34.10 */
#else
  volatile long double a, b, x, r, eps, z;
  volatile double q;
#endif
  int i;
  x = agn_checknumber(L, 1);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  eps = agnL_optpositive(L, 4, agn_getepsilon(L));  /* 2.13.0 change */
  if (luai_numiseven(b) || !(0 < a && a < 1)) {  /* b even or a out-of-range? -> infinite loops otherwise */
    lua_pushundefined(L);
    return 1;
  }
  i = r = q = 0;
  while (1) {
    z = tools_powil(a, i)*sun_cosl(tools_powil(b, i)*x);
    if (fabsl(z) < eps) break;
    r = tools_koaddl(r, z, &q); i++; /* 2.13.0/2.34.10 change */
  }
  lua_pushnumber(L, r + q);
  return 1;
}


/* Computes the first derivative of the univariate or multivariate function f at real point x, a number.

   If the option eps=h is given, the epsilon value h (a positive number preferably close to zero) is used
   internally for the computation, its default is math.epsilon(x).

   The second, etc. arguments to f may be given right after argument x.

   The return is the imaginary part of f(x + I*h)/h , or `fail` if f did not evaluate to the complex plane.

   This function as at least three times faster than `calc.xpdiff`.

   The idea has been taken from the Euler Math Toolbox, thus its name. See also:
   http://www.euler-math-toolbox.de by Rene Grothmann, version 2018-11-16, source file util/numerical.e,
   function `diffc`;
   http://adl.stanford.edu/hyperdual/Fike_AIAA-2011-886.pdf, page 4;
   https://www.sciencedirect.com/science/article/pii/S0377042712004207

   DO NOT MIGRATE TO LONG DOUBLES due to results less precise than with doubles (division by h) ! */

static int calc_eulerdiff (lua_State *L) {  /* 2.14.3 */
  lua_Number x, eps, delta, step, im;
  int sided, deriv, i, c, nargs;
  (void)sided; (void)delta; (void)step;
  c = 1;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  x = agn_checknumber(L, 2);
  aux_geteps(L, x, 3, &nargs, &eps, &delta, &step, &sided, &deriv, 0, 1, 2, 1, "calc.eulerdiff");
  /* first check whether f is defined at x, ..., otherwise the function would erroneously return a finite result.
     This is not bulletproof, e.g. sqrt(x)' with x = 0 does not exist. */
  if (isnan(agnL_fncall(L, 1, x, 3, nargs))) {
    lua_pushundefined(L);
    return 1;
  }
  luaL_checkstack(L, 2 + (nargs > 2 ? nargs - 3 + 1 : 0), "not enough stack space");  /* 2.31.7 fix */
  if (deriv == 1) {  /* first derivative: << f, x, h -> imag(f(x + I*h)) / h >> */
    lua_pushvalue(L, 1);  /* push function */
    agn_pushcomplex(L, x, eps);  /* push `x + I*h` */
    for (i=3; i <= nargs; i++, c++) lua_pushvalue(L, i);  /* push further arguments if they exist */
    if (lua_pcall(L, c, 1, 0) != 0 || (!lua_iscomplex(L, -1) && !lua_isnumber(L, -1))) {  /* 4.12.0 fix */
      if (lua_isstring(L, -1)) {
        lua_pushfail(L);
        lua_pushvalue(L, -2);  /* move error message string behind */
        lua_remove(L, -3);
        return 2;
      } else {  /* remove whatever non-string */
        agn_poptop(L);
        lua_pushfail(L);
        return 1;
      }
    }
    im = agn_compleximag(L, -1);
    agn_poptop(L);
    lua_pushnumber(L, im/eps);
  } else {  /* second derivative: << f, x, h -> 2*(f(x)-Real[f(x + I*h)])/h^2 >> */
    lua_Number z0, z1, h;
    /* part one */
    lua_pushvalue(L, 1);  /* push function */
    lua_pushnumber(L, x);
    for (i=3; i <= nargs; i++, c++) lua_pushvalue(L, i);  /* push further arguments if they exist */
    if (lua_pcall(L, c, 1, 0) != 0) {
      agn_poptop(L);  /* remove error message string */
      lua_pushfail(L);
      return 1;
    }
    z0 = agn_tonumber(L, -1);
    agn_poptop(L);
    h = eps;
    /* part two; to prevent catastrophic numerical cancellation, we iterate to get an acceptable result: */
    luaL_checkstack(L, 2 + (nargs > 2 ? nargs - 3 + 1 : 0), "not enough stack space");  /* 2.31.7 fix */
    do {
      c = 1;
      lua_pushvalue(L, 1);  /* push function */
      agn_pushcomplex(L, x, h);
      for (i=3; i <= nargs; i++, c++) lua_pushvalue(L, i);  /* push further arguments if they exist */
      if (lua_pcall(L, c, 1, 0) != 0 || (!lua_iscomplex(L, -1) && !lua_isnumber(L, -1))) {
        agn_poptop(L);  /* remove error message string */
        lua_pushfail(L);
        return 1;
      }
      z1 = agn_complexreal(L, -1);
      agn_poptop(L);
      h *= 10.0;
    } while (h < 1.0 && fabs(z0 - z1) < eps);
    h /= 10.0;
    lua_pushnumber(L, 2.0*(z0 - z1)/(h*h));
  }
  return 1;
}


static long double gam0 (long double x) {
/*
c*********************************************************************
c
cc GAM0 computes the Gamma function for the LAMV function.
c
c  Licensing:
c
c    This routine is copyrighted by Shanjie Zhang and Jianming Jin.  However,
c    they give permission to incorporate this routine into a user program
c    provided that the copyright is acknowledged.
c
c  Modified:
c
c    09 July 2012
c
c  Author:
c
c    Shanjie Zhang, Jianming Jin
c
c  Reference:
c
c    Shanjie Zhang, Jianming Jin,
c    Computation of Special Functions,
c    Wiley, 1996,
c    ISBN: 0-471-11963-6,
c    LC: QA351.C45.
c
c  Parameters:
c
c    Input, double precision X, the argument.
c
c    Output, double precision GA, the function value. */
  int k;
  long double y;
  long double g[25] = {
     1.0,
     0.5772156649015329e+00,
    -0.6558780715202538e+00,
    -0.420026350340952e-01,
     0.1665386113822915e+00,
    -0.421977345555443e-01,
    -0.96219715278770e-02,
     0.72189432466630e-02,
    -0.11651675918591e-02,
    -0.2152416741149e-03,
     0.1280502823882e-03,
    -0.201348547807e-04,
    -0.12504934821e-05,
     0.11330272320e-05,
    -0.2056338417e-06,
     0.61160950e-08,
     0.50020075e-08,
    -0.11812746e-08,
     0.1043427e-09,
     0.77823e-11,
    -0.36968e-11,
     0.51e-12,
    -0.206e-13,
    -0.54e-14,
    0.14e-14 };
  y = g[24];
  for (k=23; k >= 0; k--) {
    y = fmal(y, x, g[k]);  /* 2.35.2 tweak */
  }
  return 1.0/(y*x);
}

/* Taken from the Fortran 90 source file:
   https://people.sc.fsu.edu/~jburkardt/f77_src/special_functions/special_functions.f, subroutine lamv

   See also: `Computation of Special Functions`, p. 182ff, by Shanjie Zhang and Jianming Jin.

   Computes:
   - calc.lamv[1] := << v, x -> 2^v*gamma(v + 1)*besselj(v, x)/x^v >> and its derivative
   - calc.lamv[2] := << v, x -> 2*v/x*(calc.lamv[1](v - 1, x) - calc.lamv[1](v, x)) >>
   - the actual order processed, may differ from the input !

   (Maple: lamv := (v, x) -> 2^v*GAMMA(v + 1)*BesselJ(v, x)/x^v; )

c*********************************************************************
c
cc LAMV computes lambda functions and derivatives of [integral] order. Fractional orders have been deliberately
c  switched off due to wrong results, compared with Maple V R4.
c
c  Licensing:
c
c    This routine is copyrighted by Shanjie Zhang and Jianming Jin.  However,
c    they give permission to incorporate this routine into a user program
c    provided that the copyright is acknowledged.
c
c  Modified:
c
c    31 July 2012
c
c  Author:
c
c    Shanjie Zhang, Jianming Jin
c
c  Reference:
c
c    Shanjie Zhang, Jianming Jin,
c    Computation of Special Functions,
c    Wiley, 1996,
c    ISBN: 0-471-11963-6,
c    LC: QA351.C45.
c
c  Parameters:
c
c    Input, double precision V, the order (a posint).
c
c    Input, double precision X, the argument.
c
c    Output, double precision VM, the highest order computed.
c
c    Output, double precision VL(0:*), DL(0:*), the Lambda function and
c    derivative, of orders N+V0. */
static int calc_lambda (lua_State *L) {  /* 2.20.2 */
  long double v, a0, bjv0, bjv1, bk, ck, cs, f, f0, f1, f2, fac, ga, px, qx, r, r0, rc, rp,
    rq, sk, uk, v0, vk, vm, vv, x, x2, xk, *dl, *vl;
  int i, j, k, k0, m, n;
  v = agn_checkposint(L, 1);  /* order; we won't accept fractional orders due to incorrect results */
  x = agn_checknumber(L, 2);  /* argument */
  bjv0 = 0.0L; bjv1 = 0.0L; f = 0.0L;
  x = fabsl(x);
  x2 = x*x;
  n = (int)v;
  vl = agn_malloc(L, (n + 1)*sizeof(long double), "calc.lambda", NULL);  /* the Lambda function, of order N + V0. */
  dl = agn_malloc(L, (n + 1)*sizeof(long double), "calc.lambda", vl, NULL);  /* the derivative, of order N + V0. 4.11.5 fix */
  v0 = v - n;  /* v0 = frac(v), will always be 0 */
  vm = v;  /* highest order computed */
  if (x <= 12.0) {
    long double eps = agnL_optpositive(L, 3, agn_getdblepsilon(L));
    for (k=0; k <= n; k++) {
      vk = v0 + k;
      bk = 1.0L;
      r = 1.0L;
      for (i=1; i <= 50; i++) {
        r = -0.25L*r*x2/(i*(i + vk));
        bk += r;
        if (fabsl(r) < fabsl(bk)*eps) break;
      }
      vl[k]= bk;
      uk = 1.0;
      r = 1.0;
      for (i=1; i <= 50; i++) {
        r = -0.25L*r*x2/(i*(i + vk + 1.0));
        uk += r;
        if (fabsl(r) < fabsl(uk)*eps) break;
      }
      dl[k] = -0.5L*x/(vk + 1.0)*uk;
    }
    goto leave;
  }
  if (x < 35.0L)
    k0 = 11;
  else if (x < 50.0L)
    k0 = 10;
  else
    k0 = 8;
  for (j=0; j <= 1; j++) {
    vv = 4.0L*(j + v0)*(j + v0);
    px = 1.0L;
    rp = 1.0L;
    for (k=1; k <= k0; k++) {
      rp = -0.78125e-02*rp*tools_powil(vv - (4.0L*k - 3.0L), 2)*tools_powil(vv - (4.0L*k - 1.0L), 2)  /* 2.29.5 optimisation */
           / (k*(2.0L*k - 1.0L)*x2);
      px += rp;
    }
    qx = 1.0L;
    rq = 1.0L;
    for (k=1; k <= k0; k++) {
      rq = - 0.78125e-02*rq*tools_powil(vv - (4.0L*k - 1.0L), 2)*tools_powil(vv - (4.0L*k + 1.0L), 2)  /* 2.29.5 optimisation */
           / (k*(2.0L*k + 1.0L)*x2);
      qx += rq;
    }
    qx = 0.125L*(vv - 1.0L)*qx/x;
    xk = x - (0.5L*(j + v0) + 0.25L)*M_PIld;
    a0 = tools_sqrtl(TWOoPIld/x);
    sk = sun_sinl(xk);
    ck = sun_cosl(xk);
    if (j == 0)
      bjv0 = a0*(px*ck - qx*sk);
    else
      bjv1 = a0*(px*ck - qx*sk);
  }
  ga = (v0 == 0.0L) ? 1.0L : v0*gam0(v0);   /* since we allow only integral orders, gam0 is never called */
  fac = tools_powl(2.0L/x, v0)*ga;
  vl[0] = bjv0;
  dl[0] = -bjv1 + v0/x*bjv0;
  vl[1] = bjv1;
  dl[1] = bjv0 - (1.0L + v0)/x*bjv1;
  r0 = 2.0L*(1.0L + v0)/x;
  if (n <= 1) {
    vl[0] = fac*vl[0];
    dl[0] = fac*dl[0] - v0/x*vl[0];
    vl[1] = fac*r0*vl[1];
    dl[1] = fac*r0*dl[1] - (1.0L + v0)/x*vl[1];
    goto leave;
  }
  if (0 && n >= 2 && n <= truncl(0.9*x)) {
    /* this is very imprecise with larger x, so do not run it: */
    f0 = bjv0;
    f1 = bjv1;
    for (k=2; k <= n; k++) {
      f = 2.0*(k + v0 - 1.0)/x*f1 - f0;
      f0 = f1;
      f1 = f;
      vl[k] = f;
    }
  } else if (n >= 2) {
    m = tools_msta1l(x, 200);
    if (m < n)
      n = m;
    else
      m = tools_msta2l(x, n, 15);
    f2 = 0.0L;
    f1 = 1.0e-100L;
    for (k=m; k >= 0; k--) {
      f = 2.0L*(v0 + k + 1.0L)/x*f1 - f2;
      if (k <= n) vl[k] = f;
      f2 = f1;
      f1 = f;
    }
    cs = (fabsl(bjv0) <= fabsl(bjv1)) ? bjv1/f2 : bjv0/f;
    for (k=0; k <= n; k++)
      vl[k] = cs*vl[k];
  }
  vl[0] = fac*vl[0];
  for (j=1; j <= n; j++) {
    rc = fac*r0;
    vl[j] = rc*vl[j];
    dl[j - 1] = -0.5L*x/(j + v0)*vl[j];
    r0 = 2.0L*(j + v0 + 1)/x*r0;
  }
  dl[n] = 2.0L*(v0 + n)*(vl[n - 1] - vl[n])/x;
  vm = n + v0;  /* highest order computed, n = int(v) or modified integer, v0 = frac(v) */
leave:
  lua_pushnumber(L, vl[n]);
  lua_pushnumber(L, dl[n]);
  lua_pushnumber(L, vm);
  xfreeall(dl, vl);
  return 3;
}


/* The following two C functions have been taken from:
   https://github.com/CompPhysics/ComputationalPhysics/blob/master/doc/Programs/LecturePrograms/programs/NumericalIntegration/cpp/program2.cpp
   written by Morten Hjorth-Jensen

   The function gauleg() takes the lower and upper limits of integration x1, x2,
   calculates and return the abcissas in x[0, ..., n - 1] and the weights in
   w[0, ..., n - 1] of length n of the Gauss--Legendre n--point quadrature formulae.
*/

#ifndef __ARMCPU
static void gauleg (long double x1, long double x2, long double x[], long double w[], int n, long double eps) {
  int m, j, i;
  long double z1, z, xm, xl, pp, p3, p2, p1, *x_low, *x_high, *w_low, *w_high;
  /* roots are symmetric in the interval */
  m  = (n + 1)/2;
  xm = 0.5L*(x2 + x1);
  xl = 0.5L*(x2 - x1);
  /* pointer initialisation */
  x_low  = x;
  x_high = x + n - 1;
  w_low  = w;
  w_high = w + n - 1;
  /* loops over desired roots */
  for (i=1; i <= m; i++) {
    z = sun_cosl(M_PIld*(i - 0.25L)/(n + 0.5L));
    /* Starting with the above approximation to the ith root we enter the mani loop of refinement bt Newtons method. */
    do {
      p1 = 1.0L;
      p2 = 0.0L;
      /* loop up recurrence relation to get the Legendre polynomial evaluated at x */
      for (j=1; j <= n; j++) {
        p3 = p2;
        p2 = p1;
        p1 = ((2.0L*j - 1.0L)*z*p2 - (j - 1.0L)*p3)/j;
      }
      /* p1 is now the desired Legrendre polynomial. Next compute ppp its derivative by standard relation involving also p2,
         polynomial of one lower order. */
      pp = n*(z*p1 - p2)/(z*z - 1.0L);
      z1 = z;
      z = z1 - p1/pp;  /* Newton's method */
    } while (fabsl(z - z1) > eps);
    /* Scale the root to the desired interval and put in its symmetric counterpart. Compute the weight and its symmetric
       counterpart */
    *(x_low++)  = xm - xl*z;
    *(x_high--) = xm + xl*z;
    *w_low      = 2.0L*xl/((1.0L - z*z)*pp*pp);
    *(w_high--) = *(w_low++);
  }
}
#endif

/* Gauss-Legendre integration, 7 times faster than calc.integ with same precision */
static int calc_gauleg64 (lua_State *L);

static int calc_gauleg (lua_State *L) {  /* 2.21.4, 2.34.10 switch to long doubles */
#if defined(__ARMCPU)
  return calc_gauleg64(L);
#else
  int i, n, nargs, adjust, iters, use64, errorcode;
  long double a, b, r, eps, length, omega, delta, *x, *w;
  (void)omega; (void)length; (void)adjust; (void)delta; (void)iters; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (tools_isinf(a) || tools_isinf(b))
    luaL_error(L, "Error in " LUA_QS ": at least one boundary is +/-infinity.", "calc.gauleg");
  else if (a > b)
    luaL_error(L, "Error in " LUA_QS ": left border must be less than right border.", "calc.gauleg");
  else if (a == b) {  /* 4.2.4 fix */
    lua_pushnumber(L, 0.0);
    return 1;
  }
  eps = luai_numipow(agn_getepsilon(L), 2);  /* 3.12.0 change for `eps` option */
  omega = length = -1.0L;
  adjust = -1;
  n = (b - a)*20;  /* total sample points, default is dependent on the range given, change for `samples` option */
  if (n == 0) n = 20;  /* 3.11.5 fix */
  aux_fcheckoptionslong(L, 3, &nargs, &eps, &omega, &n, &length, &adjust, &delta, &iters, &use64, "calc.gauleg");  /* 3.12.0 change */
  /* reserve space in memory for vectors containing the mesh points weights and function values for the use of the
     Gauss-Legendre method */
  x = agn_malloc(L, n*sizeof(long double), "calc.gauleg", NULL);  /* 4.11.5 patch */
  w = agn_malloc(L, n*sizeof(long double), "calc.gauleg", x, NULL);  /* 4.11.5 patch */
  /* set up the mesh points and weights */
  gauleg(a, b, x, w, n, eps);
  /* evaluate the integral with the Gauss-Legendre method */
  r = 0.0L;
  for (i=0; i < n; i++) {
    r += w[i]*agnL_fncallx(L, 1, x[i], 4, nargs, &errorcode, 0);  /* 3.11.5 change for multivariate support */
    if (errorcode != 0) {  /* 7.3.4 fix */
      xfreeall(x, w);
      luaL_error(L, "Error in " LUA_QS ": function call did not return a number.", "calc.gauleg");
    }
  }
  lua_pushnumber(L, r);
  xfreeall(x, w);
  return 1;
#endif
}

static void gauleg64 (double x1, double x2, double x[], double w[], int n, double eps) {
  int m, j, i;
  double z1, z, xm, xl, pp, p3, p2, p1, *x_low, *x_high, *w_low, *w_high;
  /* roots are symmetric in the interval */
  m  = (n + 1)/2;
  xm = 0.5*(x2 + x1);
  xl = 0.5*(x2 - x1);
  /* pointer initialisation */
  x_low  = x;
  x_high = x + n - 1;
  w_low  = w;
  w_high = w + n - 1;
  /* loops over desired roots */
  for (i=1; i <= m; i++) {
    z = sun_cos(PI*(i - 0.25)/(n + 0.5));
    /* Starting with the above approximation to the ith root we enter the mani loop of refinement bt Newtons method. */
    do {
      p1 = 1.0;
      p2 = 0.0;
      /* loop up recurrence relation to get the Legendre polynomial evaluated at x */
      for (j=1; j <= n; j++) {
        p3 = p2;
        p2 = p1;
        p1 = ((2.0*j - 1.0)*z*p2 - (j - 1.0)*p3)/j;
      }
      /* p1 is now the desired Legrendre polynomial. Next compute ppp its derivative by standard relation involving also p2,
         polynomial of one lower order. */
      pp = n*(z*p1 - p2)/(z*z - 1.0);
      z1 = z;
      z = z1 - p1/pp;  /* Newton's method */
    } while (fabs(z - z1) > eps);
    /* Scale the root to the desired interval and put in its symmetric counterpart. Compute the weight and its symmetric
       counterpart */
    *(x_low++)  = xm - xl*z;
    *(x_high--) = xm + xl*z;
    *w_low      = 2.0*xl/((1.0 - z*z)*pp*pp);
    *(w_high--) = *(w_low++);
  }
}

static int calc_gauleg64 (lua_State *L) {
  int i, n, nargs, adjust, iters, errorcode;
  double a, b, r, eps, length, omega, delta, *x, *w;
  (void)omega; (void)length; (void)adjust; (void)delta; (void)iters;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (tools_isinf(a) || tools_isinf(b))
    luaL_error(L, "Error in " LUA_QS ": at least one boundary is +/-infinity.", "calc.gauleg64");
  else if (a > b)
    luaL_error(L, "Error in " LUA_QS ": left border must be less than right border.", "calc.gauleg64");
  else if (a == b) {
    lua_pushnumber(L, 0.0);
    return 1;
  }
  eps = luai_numipow(agn_getepsilon(L), 2);  /* 3.12.0 change for `eps` option */
  omega = length = -1.0;
  adjust = -1;
  n = (b - a)*20;  /* total sample points, default is dependent on the range given, change for `samples` option */
  if (n == 0) n = 20;
  aux_fcheckoptions(L, 3, &nargs, &eps, &omega, &n, &length, &adjust, &delta, &iters, "calc.gauleg64");  /* 3.12.0 change */
  /* reserve space in memory for vectors containing the mesh points weights and function values for the use of the
     Gauss-Legendre method */
  x = agn_malloc(L, n*sizeof(double), "calc.gauleg64", NULL);
  w = agn_malloc(L, n*sizeof(double), "calc.gauleg64", x, NULL);
  /* set up the mesh points and weights */
  gauleg64(a, b, x, w, n, eps);
  /* evaluate the integral with the Gauss-Legendre method */
  r = 0.0;
  for (i=0; i < n; i++) {
    r += w[i]*agnL_fncallx(L, 1, x[i], 4, nargs, &errorcode, 0);  /* 3.11.5 change for multivariate support */
    if (errorcode != 0) {  /* 7.3.4 fix */
      xfreeall(x, w);
      luaL_error(L, "Error in " LUA_QS ": function call did not return a number.", "calc.gauleg64");
    }
  }
  lua_pushnumber(L, r);
  xfreeall(x, w);
  return 1;
}


/* Determines the `variance` of a function f in a given interval [a, b] using trapezoidal rule. The return
   is an integer counting how often the trapezoidal rule has been applied. 2.21.4 */
static double trapezoidal_rule (lua_State *L, int idx, int nargs, long double a, long double b, int n) {
  long double r, fa, fb, x, step;
  int j;
  step = (b - a)/((long double)n);
  fa = 0.5L*agnL_fncall(L, 1, a, 4, nargs);
  fb = 0.5L*agnL_fncall(L, 1, b, 4, nargs);
  r = 0.0L;
  for (j=1; j <= n - 1; j++) {
    x = j*step + a;
    r += agnL_fncall(L, 1, x, 4, nargs);
  }
  r = (r + fb + fa)*step;
  return r;
}

#define MAXRECURSIONS   50

static int adaptive_integration (lua_State *L, int idx, int nargs, long double a, long double b, int n, int steps, long double eps) {
  long double c, i0, il, ir;  /* changed 2.34.10 */
  if (steps > MAXRECURSIONS) return steps;
  c = (a + b)*0.5;
  i0 = trapezoidal_rule(L, 1, nargs, a, b, n);  /* the whole integral */
  il = trapezoidal_rule(L, 1, nargs, a, c, n);  /* the left half */
  ir = trapezoidal_rule(L, 1, nargs, c, b, n);  /* the right half */
  if (fabsl(il + ir - i0) >= eps) {  /* trigger recursion */
    int stepsl, stepsr;
    stepsl = adaptive_integration(L, idx, nargs, a, c, n, ++steps, eps);
    stepsr = adaptive_integration(L, idx, nargs, c, b, n, ++steps, eps);
    steps = stepsl + stepsr;
  }  /* else do nothing, just return steps */
  return steps;
}

/* Returns a positive integer that indicates whether a function `f` in one real changes slowly or rapidly on the
   given interval [a, b]. The larger the result, the larger is its rate of change. By default,
   - the result is relative, i.e. given per unit on the abscissa (true for relative, false for absolute),
   - there are `n`=10 points to sample per unit, and
   - the bail-out value `eps` - a positive value close to zero - is `Eps`.
   Internally, the function uses adaptive integration with trapezoidal rule and counts the number of trapezoids
   evaluated. Note that the results are estimates. */
static int calc_variance (lua_State *L) {
  int samples, r, units, nargs, adjust, iters, use64;
  long double a, b, eps, omega, delta, length;  /* changed 2.34.10 */
  (void)omega; (void)length; (void)adjust; (void)delta; (void)iters; (void)use64;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (b <= a)
    luaL_error(L, "Error in " LUA_QS ": left border must be less than right border.", "calc.variance");
  units = sun_roundl(b) - sun_roundl(a);  /* round to nearest integer away from zero */
  samples = units*10;       /* 3.12.0 change for `samples` (n) option */
  eps = agn_getepsilon(L);  /* 3.12.0 change for `eps` option */
  aux_fcheckoptionslong(L, 3, &nargs, &eps, &omega, &samples, &length, &adjust, &delta, &iters, &use64, "calc.variance");  /* 3.11.5 change */
  r = adaptive_integration(L, 1, nargs, a, b, samples, 0, eps);
  lua_pushinteger(L, r/units);
  return 1;
}


/* Computes the scaled complex complementary error (Faddeeva) function w(z) = exp(-z^2)*erfc(-I*z), for numbers
   and complex numbers z. The return is a complex value. 2.21.6 */
#ifndef PROPCMPLX
#define cmplxreal(z) creal(z)
#define cmplximag(z) cimag(z)
#else
#define cmplxreal(z) (z).cmplx[0]
#define cmplximag(z) (z).cmplx[1]
#endif
static int calc_w (lua_State *L) {
  lua_Number eps;
  eps = agnL_optnonnegative(L, 2, 0);
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x;
      x = agn_tonumber(L, 1);
#ifndef PROPCMPLX
      agn_Complex re;
      re = tools_w(x + I*0, eps);
#else
      PROPCMPLX_CMPLX z, re;
      z.cmplx[0] = x; z.cmplx[1] = 0;
      re = tools_w(z, eps);
#endif
      agn_pushcomplex(L, cmplxreal(re), cmplximag(re));
      break;
    }
    case LUA_TCOMPLEX: {
#ifndef PROPCMPLX
      agn_Complex z, re;
      z = agn_tocomplex(L, 1);
#else
      PROPCMPLX_CMPLX z, re;
      cmplxreal(z) = agn_complexreal(L, 1); cmplximag(z) = agn_compleximag(L, 1);
#endif
      re = tools_w(z, eps);
#ifndef PROPCMPLX
      agn_createcomplex(L, re);
#else
      agn_createcomplex(L, cmplxreal(re), cmplximag(re));
#endif
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "calc.w");
  }
  return 1;
}


static int calc_aitken (lua_State *L) {
  /* Finds the limit of the sequence x[n+1] = f(x[n]) with initial x[0] and tolerance eps, with a maximum of iter iterations, using
     Aitken extrapolation. eps by default is DoubleEps and iter is 20.
     Returns either the approximated limit alpha and the first derivative at alpha f'(alpha) if successful, and `undefined` twice
     otherwise. The third return is the number of iterations taken to compute the result.
     For example calc.aitken(<< x -> 1/2*(x + 2/x) >>, 1) ~= sqrt(2).
     See https://en.wikipedia.org/wiki/Aitken%27s_delta-squared_process */
  long double x0, x1, x2, eps, aitken, d1, denom, l;  /* changed 2.34.10 */
  int nargs, iter, rc, i;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);  /* 2.1.4 */
  /* luaL_checkstack is called by agnL_fncall */
  x0 = agn_checknumber(L, 2);  /* start value */
  eps = agnL_optpositive(L, 3, L->DoubleEps);
  iter = agnL_optposint(L, 4, 20);
  l = AGN_NAN;
  rc = 0;
  for (i=0; i < iter; i++) {
    x1 = agnL_fncall(L, 1, x0, 5, nargs);
    x2 = agnL_fncall(L, 1, x1, 5, nargs);
    d1 = x2 - x1;
    denom = d1 - (x1 - x0);
    if (fabsl(denom) < eps) break;
    aitken = x2 - (d1*d1)/denom;
    if (tools_approxl(aitken, x2, eps)) {
      l = fabsl(d1/(x1 - x0));
      rc = 1; i++;
      break;
    }
    x0 = aitken;
  }
  lua_pushnumber(L, rc ? aitken : AGN_NAN);
  lua_pushnumber(L, l);
  lua_pushinteger(L, i);
  return 3;
}


/* Computes the n-th Chebyshev polynomial of the first kind, evaluated at x, with n a non-negative integer and x a number. 2.33.1;
   2.34.10 switch to long doubles; 2.41.3 added hard-coded polynomials & exchanged default formula.
   Maple V Release 4:
   with(orthopoly, T); Digits := 25;
   readlib(C):
   for i from 0 to 30 do
      printf(`    case %d:\n`, i);
      C(convert(T(i, x), horner, x), optimized);
      printf(`      lua_pushnumber(L, ); break;\n`);
   od;
   Rewritten 7.3.10, using Estrin’s Scheme, created by Gemini AI and put to the public domain. 3.5 % faster than the pure Horner version,
   Computed GOTOs do not result in an increase in speed. */
static int calc_chebyt (lua_State *L) {
  long double x, absx, t1 = 0.0L;
  int n = agn_checknonnegint(L, 1);
  x = agn_checknumber(L, 2);
  absx = fabsl(x);
  if (absx > 1) {  /* 2.41.3 fix,
    see `Computing Machine-Efficient Polynomial Approximations`,
    by NICOLAS BRISEBARRE, Université J. Monnet, St-Étienne and LIP-E.N.S. Lyon,
    JEAN-MICHEL MULLER, CNRS, LIP-ENS Lyon, and ARNAUD TISSERAND, INRIA, LIP-ENS Lyon
    Note that the paper gives cosh(n*arccosh(x)) for x > 1 only, and not for x < -1. */
    t1 = tools_coshl(n*sun_acoshl(absx));
  } else if (n > 31) {
    t1 = sun_cosl(n*sun_acosl(x));
  } else {
    long double x2 = x*x;
    long double x4 = x2*x2;
    long double x8 = x4*x4;
    long double x16 = x8*x8;
    switch (n) {
      case 0: t1 = 1.0L; break;
      case 1: t1 = x; break;
      case 2: t1 = 2.0L*x2 - 1.0L; break;
      case 3: t1 = (4.0L*x2 - 3.0L)*x; break;
      case 4: t1 = 8.0L*x4 - 8.0L*x2 + 1.0L; break;
      case 5: t1 = (16.0L*x4 - 20.0L*x2 + 5.0L)*x; break;
      case 6: t1 = (32.0L*x4 - 48.0L*x2 + 18.0L)*x2 - 1.0L; break;
      case 7: t1 = ((64.0L*x4 - 112.0L*x2 + 56.0L)*x2 - 7.0L)*x; break;
      case 8: t1 = (128.0L*x8 - 256.0L*x4*x2) + (160.0L*x4 - 32.0L*x2 + 1.0L); break;
      case 9: t1 = ((256.0L*x8 - 576.0L*x4*x2) + (432.0L*x4 - 120.0L*x2 + 9.0L))*x; break;
      case 10: t1 = (512.0L*x8*x2 - 1280.0L*x8) + (1120.0L*x4*x2 - 400.0L*x4) +
                    (50.0L*x2 - 1.0L); break;
      case 11: t1 = ((1024.0L*x8*x2 - 2816.0L*x8) + (2816.0L*x4*x2 - 1232.0L*x4) +
                    (220.0L*x2 - 11.0L))*x; break;
      case 12: t1 = (2048.0L*x8*x4 - 6144.0L*x8*x2) + (6912.0L*x8 - 3584.0L*x4*x2) +
                    (840.0L*x4 - 72.0L*x2 + 1.0L); break;
      case 13: t1 = ((4096.0L*x8*x4 - 13312.0L*x8*x2) + (16640.0L*x8 - 9984.0L*x4*x2) +
                    (2912.0L*x4 - 364.0L*x2 + 13.0L))*x; break;
      case 14: t1 = (8192.0L*x8*x4*x2 - 28672.0L*x8*x4) + (39424.0L*x8*x2 - 26880.0L*x8) +
                    (9408.0L*x4*x2 - 1568.0L*x4) + (98.0L*x2 - 1.0L); break;
      case 15: t1 = ((16384.0L*x8*x4*x2 - 61440.0L*x8*x4) + (92160.0L*x8*x2 - 70400.0L*x8) +
                    (28800.0L*x4*x2 - 6048.0L*x4) + (560.0L*x2 - 15.0L))*x; break;
      case 16: t1 = (32768.0L*x16 - 131072.0L*x8*x4*x2) + (212992.0L*x8*x4 - 180224.0L*x8*x2) +
                    (84480.0L*x8 - 21504.0L*x4*x2) + (2688.0L*x4 - 128.0L*x2 + 1.0L); break;
      case 17: t1 = ((65536.0L*x16 - 278528.0L*x8*x4*x2) + (487424.0L*x8*x4 - 452608.0L*x8*x2) +
                    (239360.0L*x8 - 71808.0L*x4*x2) + (11424.0L*x4 - 816.0L*x2 + 17.0L))*x; break;
      case 18: t1 = (131072.0L*x16*x2 - 589824.0L*x16) + (1105920.0L*x8*x4*x2 - 1118208.0L*x8*x4) +
                    (658944.0L*x8*x2 - 228096.0L*x8) + (44352.0L*x4*x2 - 4320.0L*x4) + (162.0L*x2 - 1.0L); break;
      case 19: t1 = ((262144.0L*x16*x2 - 1245184.0L*x16) + (2490368.0L*x8*x4*x2 - 2723840.0L*x8*x4) +
                    (1770496.0L*x8*x2 - 695552.0L*x8) + (160512.0L*x4*x2 - 20064.0L*x4) + (1140.0L*x2 - 19.0L))*x; break;
      case 20: t1 = (524288.0L*x16*x4 - 2621440.0L*x16*x2) + (5570560.0L*x16 - 6553600.0L*x8*x4*x2) +
                    (4659200.0L*x8*x4 - 2050048.0L*x8*x2) + (549120.0L*x8 - 84480.0L*x4*x2) +
                    (6600.0L*x4 - 200.0L*x2 + 1.0L); break;
      case 21: t1 = ((1048576.0L*x16*x4 - 5505024.0L*x16*x2) + (12386304.0L*x16 - 15597568.0L*x8*x4*x2) +
                    (12042240.0L*x8*x4 - 5870592.0L*x8*x2) + (1793792.0L*x8 - 329472.0L*x4*x2) +
                    (33264.0L*x4 - 1540.0L*x2 + 21.0L))*x; break;
      case 22: t1 = (2097152.0L*x16*x4*x2 - 11534336.0L*x16*x4) + (27394048.0L*x16*x2 - 36765696.0L*x16) +
                    (30638080.0L*x8*x4*x2 - 16400384.0L*x8*x4) + (5637632.0L*x8*x2 - 1208064.0L*x8) +
                    (151008.0L*x4*x2 - 9680.0L*x4) + (242.0L*x2 - 1.0L); break;
      case 23: t1 = ((4194304.0L*x16*x4*x2 - 24117248.0L*x16*x4) + (60293120.0L*x16*x2 - 85917696.0L*x16) +
                    (76873728.0L*x8*x4*x2 - 44843008.0L*x8*x4) + (17145856.0L*x8*x2 - 4209920.0L*x8) +
                    (631488.0L*x4*x2 - 52624.0L*x4) + (2024.0L*x2 - 23.0L))*x; break;
      case 24: t1 = (8388608.0L*x16*x8 - 50331648.0L*x16*x4*x2) + (132120576.0L*x16*x4 - 199229440.0L*x16*x2) +
                    (190513152.0L*x16 - 120324096.0L*x8*x4*x2) + (50692096.0L*x8*x4 - 14057472.0L*x8*x2) +
                    (2471040.0L*x8 - 256256.0L*x4*x2) + (13728.0L*x4 - 288.0L*x2 + 1.0L); break;
      case 25: t1 = ((16777216.0L*x16*x8 - 104857600.0L*x16*x4*x2) + (288358400.0L*x16*x4 - 458752000.0L*x16*x2) +
                    (466944000.0L*x16 - 317521920.0L*x8*x4*x2) + (146227200.0L*x8*x4 - 45260800.0L*x8*x2) +
                    (9152000.0L*x8 - 1144000.0L*x4*x2) + (80080.0L*x4 - 2600.0L*x2 + 25.0L))*x; break;
      case 26: t1 = (33554432.0L*x16*x8*x2 - 218103808.0L*x16*x8) + (627048448.0L*x16*x4*x2 - 1049624576.0L*x16*x4) +
                    (1133117440.0L*x16*x2 - 825556992.0L*x16) + (412778496.0L*x8*x4*x2 - 141213696.0L*x8*x4) +
                    (32361472.0L*x8*x2 - 4759040.0L*x8) + (416416.0L*x4*x2 - 18928.0L*x4) + (338.0L*x2 - 1.0L); break;
      case 27: t1 = ((67108864.0L*x16*x8*x2 - 452984832.0L*x16*x8) + (1358954496.0L*x16*x4*x2 - 2387607552.0L*x16*x4) +
                    (2724986880.0L*x16*x2 - 2118057984.0L*x16) + (1143078912.0L*x8*x4*x2 - 428654592.0L*x8*x4) +
                    (109983744.0L*x8*x2 - 18670080.0L*x8) + (1976832.0L*x4*x2 - 117936.0L*x4) + (3276.0L*x2 - 27.0L))*x; break;
      case 28: t1 = (134217728.0L*x16*x8*x4 - 939524096.0L*x16*x8*x2) + (2936012800.0L*x16*x8 - 5402263552.0L*x16*x4*x2) +
                    (6499598336.0L*x16*x4 - 5369233408.0L*x16*x2) + (3111714816.0L*x16 - 1270087680.0L*x8*x4*x2) +
                    (361181184.0L*x8*x4 - 69701632.0L*x8*x2) + (8712704.0L*x8 - 652288.0L*x4*x2) +
                    (25480.0L*x4 - 392.0L*x2 + 1.0L); break;
      case 29: t1 = ((268435456.0L*x16*x8*x4 - 1946157056.0L*x16*x8*x2) +
                    (6325010432.0L*x16*x8 - 12163481600.0L*x16*x4*x2) + (15386804224.0L*x16*x4 - 13463453696.0L*x16*x2) +
                    (8341487616.0L*x16 - 3683254272.0L*x8*x4*x2) + (1151016960.0L*x8*x4 - 249387008.0L*x8*x2) +
                    (36095488.0L*x8 - 3281408.0L*x4*x2) + (168896.0L*x4 - 4060.0L*x2 + 29.0L))*x; break;
      case 30: t1 = (536870912.0L*x16*x8*x4*x2 - 4026531840.0L*x16*x8*x4) +
                    (13589544960.0L*x16*x8*x2 - 27262976000.0L*x16*x8) +
                    (36175872000.0L*x16*x4*x2 - 33426505728.0L*x16*x4) +
                    (22052208640.0L*x16*x2 - 10478223360.0L*x16) +
                    (3572121600.0L*x8*x4*x2 - 859955200.0L*x8*x4) +
                    (141892608.0L*x8*x2 - 15275520.0L*x8) +
                    (990080.0L*x4*x2 - 33600.0L*x4) + (450.0L*x2 - 1.0L); break;
      case 31: t1 = ((1073741824.0L*x16*x8*x4*x2 - 8321499136.0L*x16*x8*x4) +
                    (29125246976.0L*x16*x8*x2 - 60850962432.0L*x16*x8) +
                    (84515225600.0L*x16*x4*x2 - 82239815680.0L*x16*x4) +
                    (57567870976.0L*x16*x2 - 29297934336.0L*x16) +
                    (10827497472.0L*x8*x4*x2 - 2870927360.0L*x8*x4) +
                    (533172224.0L*x8*x2 - 66646528.0L*x8) +
                    (5261568.0L*x4*x2 - 236096.0L*x4) +
                    (4960.0L*x2 - 31.0L))*x; break;
    }
  }
  lua_pushnumber(L, (double)t1);
  return 1;
}


/* eta - Dirichlet Eta Function  ***********************************************************
   Taken from: http://www.mymathlib.com/functions/riemann_zeta.html
   Copyright © 2004 RLH. All rights reserved. */

#define CUTOFF 64   /* first argument for which |eta*()| < LDBL_EPSILON */

/* eta_star table: eta_star[0] = eta*(2) */

static long double eta_star[] = {
  -1.775329665758867817640e-1L,   -9.845732263030428595000e-2L,
  -5.296717050275408242350e-2L,   -2.788022955309069406469e-2L,
  -1.444890870256489590109e-2L,   -7.406180077169717329375e-3L,
  -3.766998147352100772398e-3L,   -1.905702458394669232070e-3L,
  -9.604924017284343589277e-4L,   -4.828565019392458559058e-4L,
  -2.423148561418091468203e-4L,   -1.214572367348845078250e-4L,
  -6.082965402028182904581e-5L,   -3.044878690076191736707e-5L,
  -1.523578509389355831723e-5L,   -7.621707958988023062128e-6L,
  -3.812130389886520310774e-6L,   -1.906491828324893143507e-6L,
  -9.533884184778849491574e-7L,   -4.767417844571836833357e-7L,
  -2.383867691774521027951e-7L,   -1.191986815604967761752e-7L,
  -5.960110760537163859686e-8L,   -2.980114303716558486689e-8L,
  -1.490076800343121233819e-8L,   -7.450449515036484147258e-9L,
  -3.725246599891272472330e-9L,   -1.862630581887813253435e-9L,
  -9.313177185460213727205e-10L,  -4.656596685457824853137e-10L,
  -2.328301040485091771847e-10L,  -1.164151419539695273453e-10L,
  -5.820760095468407611963e-11L,  -2.910381047019047741525e-11L,
  -1.455190856611523604489e-11L,  -7.275955393415249942845e-12L,
  -3.637978066831244498071e-12L,  -1.818989156791264449873e-12L,
  -9.094946195211219089515e-13L,  -4.547473234691264255663e-13L,
  -2.273736663041022650156e-13L,  -1.136868346752351239387e-13L,
  -5.684341784534663599526e-14L,  -2.842170909191661190229e-14L,
  -1.421085460237280456180e-14L,  -7.105427319991251978742e-15L,
  -3.552713666263913428962e-15L,  -1.776356835221386912770e-15L,
  -8.881784183071704520900e-16L,  -4.440892093857442903496e-16L,
  -2.220446047702585163771e-16L,  -1.110223024109247193646e-16L,
  -5.551115121406084776810e-17L,  -2.775557560989658683611e-17L,
  -1.387778780590368113294e-17L,  -6.938893903270303154146e-18L,
  -3.469446951741305776978e-18L,  -1.734723475906037622793e-18L,
  -8.673617379648137230824e-19L,  -4.336808689863384988324e-19L,
  -2.168404344944797951956e-19L,  -1.084202172476767461949e-19L,
  -5.421010862398398929744e-20L,  -2.710505431204053338230e-20L,
  -1.355252715603644626907e-20L,  -6.776263578023616327190e-21L,
  -3.388131789013605894484e-21L,  -1.694065894507402190873e-21L,
  -8.470329472539008433134e-22L,  -4.235164736270170042825e-22L,
  -2.117582368135306963498e-22L,  -1.058791184067727462445e-22L,
  -5.293955920338883914541e-23L,  -2.646977960169524158043e-23L,
  -1.323488980084789479279e-23L,  -6.617444900424038730587e-24L,
  -3.308722450212049810024e-24L,  -1.654361225106035053256e-24L,
  -8.271806125530209093757e-25L,  -4.135903062765115822705e-25L,
  -2.067951531382561669961e-25L,  -1.033975765691282087850e-25L,
  -5.169878828456414615482e-26L,  -2.584939414228208699819e-26L,
  -1.292469707114104813935e-26L,  -6.462348535570525616428e-27L,
  -3.231174267785263323798e-27L,  -1.615587133892631833760e-27L,
  -8.077935669463159741673e-28L,  -4.038967834731580061794e-28L,
  -2.019483917365790094549e-28L,  -1.009741958682895068492e-28L,
  -5.048709793414475413185e-29L,  -2.524354896707237730168e-29L,
  -1.262177448353618872942e-29L,  -6.310887241768094390905e-30L,
  -3.155443620884047204184e-30L,  -1.577721810442023605002e-30L,
  -7.888609052210118034714e-31L,  -3.944304526105059020591e-31L,
  -1.972152263052529511373e-31L,  -9.860761315262647560460e-32L,
  -4.930380657631323781428e-32L,  -2.465190328815661891113e-32L,
  -1.232595164407830945690e-32L,  -6.162975822039154728892e-33L,
  -3.081487911019577364594e-33L,  -1.540743955509788682346e-33L,
  -7.703719777548943411895e-34L,  -3.851859888774471706002e-34L,
  -1.925929944387235853019e-34L,  -9.629649721936179265158e-35L,
  -4.814824860968089632599e-35L,  -2.407412430484044816306e-35L,
  -1.203706215242022408155e-35L,  -6.018531076210112040785e-36L,
  -3.009265538105056020395e-36L,  -1.504632769052528010198e-36L,
  -7.523163845262640050994e-37L,  -3.761581922631320025498e-37L,
  -1.880790961315660012749e-37L,  -9.403954806578300063748e-38L,
  -4.701977403289150031874e-38L,  -2.350988701644575015937e-38L,
  -1.175494350822287507969e-38L,  -5.877471754111437539843e-39L,
  -2.938735877055718769922e-39L,  -1.469367938527859384961e-39L,
  -7.346839692639296924805e-40L,  -3.673419846319648462402e-40L,
  -1.836709923159824231201e-40L,  -9.183549615799121156006e-41L,
  -4.591774807899560578003e-41L,  -2.295887403949780289001e-41L,
  -1.147943701974890144501e-41L,  -5.739718509874450722504e-42L,
  -2.869859254937225361252e-42L,  -1.434929627468612680626e-42L,
  -7.174648137343063403129e-43L,  -3.587324068671531701565e-43L,
  -1.793662034335765850782e-43L,  -8.968310171678829253912e-44L,
  -4.484155085839414626956e-44L,  -2.242077542919707313478e-44L,
  -1.121038771459853656739e-44L,  -5.605193857299268283695e-45L,
  -2.802596928649634141847e-45L,  -1.401298464324817070924e-45L,
  -7.006492321624085354619e-46L,  -3.503246160812042677309e-46L,
  -1.751623080406021338655e-46L,  -8.758115402030106693273e-47L,
  -4.379057701015053346637e-47L,  -2.189528850507526673318e-47L,
  -1.094764425253763336659e-47L,  -5.473822126268816683296e-48L,
  -2.736911063134408341648e-48L,  -1.368455531567204170824e-48L,
  -6.842277657836020854120e-49L,  -3.421138828918010427060e-49L,
  -1.710569414459005213530e-49L,  -8.552847072295026067650e-50L,
  -4.276423536147513033825e-50L,  -2.138211768073756516912e-50L,
  -1.069105884036878258456e-50L,  -5.345529420184391292281e-51L,
  -2.672764710092195646141e-51L,  -1.336382355046097823070e-51L,
  -6.681911775230489115351e-52L,  -3.340955887615244557676e-52L,
  -1.670477943807622278838e-52L,  -8.352389719038111394189e-53L,
  -4.176194859519055697095e-53L,  -2.088097429759527848547e-53L,
  -1.044048714879763924274e-53L,  -5.220243574398819621368e-54L,
  -2.610121787199409810684e-54L,  -1.305060893599704905342e-54L,
  -6.525304467998524526710e-55L,  -3.262652233999262263355e-55L,
  -1.631326116999631131678e-55L,  -8.156630584998155658388e-56L,
  -4.078315292499077829194e-56L,  -2.039157646249538914597e-56L,
  -1.019578823124769457298e-56L,  -5.097894115623847286492e-57L,
  -2.548947057811923643246e-57L,  -1.274473528905961821623e-57L,
  -6.372367644529809108116e-58L,  -3.186183822264904554058e-58L,
  -1.593091911132452277029e-58L,  -7.965459555662261385144e-59L,
  -3.982729777831130692572e-59L,  -1.991364888915565346286e-59L,
  -9.956824444577826731431e-60L,  -4.978412222288913365715e-60L,
  -2.489206111144456682858e-60L,  -1.244603055572228341429e-60L,
  -6.223015277861141707144e-61L
};

static FORCE_INLINE long double xDirichlet_Eta_Star_Function_pos_int_arg (int s) {
  static int max_eta_star_index = sizeof(eta_star)/sizeof(long double) - 1;
  int k = s - 2;
  int diff;
  if (s == 0) return -0.5L;
  if (s == 1) return M_LN2ld - 1.0L;
  if (k <= max_eta_star_index) return eta_star[k];
  /* or large positive integer s, use the approximation eta*(s+1) = eta*(s)/2. */
  diff = s - max_eta_star_index - 2;
  return eta_star[max_eta_star_index]/tools_powl(2.0L, (long double)diff);
}


/* real Dirichlet Eta function, taken from Julia, MIT-licensed, 6.4.6.
   See: https://github.com/stevengj/julia/blob/9c24795ac2918df7b8cba9bb129db8c535d321e8/base/math.jl */

static const long double eta_coeffs[] = {
   .99999999999999999997, -.99999999999999999821, .99999999999999994183,
   -.99999999999999875788, .99999999999998040668, -.99999999999975652196,
   .99999999999751767484, -.99999999997864739190, .99999999984183784058,
   -.99999999897537734890, .99999999412319859549, -.99999996986230482845,
   .99999986068828287678, -.99999941559419338151, .99999776238757525623,
   -.99999214148507363026, .99997457616475604912, -.99992394671207596228,
   .99978893483826239739, -.99945495809777621055, .99868681159465798081,
   -.99704078337369034566, .99374872693175507536, -.98759401271422391785,
   .97682326283354439220, -.95915923302922997013, .93198380256105393618,
   -.89273040299591077603, .83945793215750220154, -.77148960729470505477,
   .68992761745934847866, -.59784149990330073143, .50000000000000000000,
   -.40215850009669926857, .31007238254065152134, -.22851039270529494523,
   .16054206784249779846, -.10726959700408922397, .68016197438946063823e-1,
   -.40840766970770029873e-1, .23176737166455607805e-1, -.12405987285776082154e-1,
   .62512730682449246388e-2, -.29592166263096543401e-2, .13131884053420191908e-2,
   -.54504190222378945440e-3, .21106516173760261250e-3, -.76053287924037718971e-4,
   .25423835243950883896e-4, -.78585149263697370338e-5, .22376124247437700378e-5,
   -.58440580661848562719e-6, .13931171712321674741e-6, -.30137695171547022183e-7,
   .58768014045093054654e-8, -.10246226511017621219e-8, .15816215942184366772e-9,
   -.21352608103961806529e-10, .24823251635643084345e-11, -.24347803504257137241e-12,
   .19593322190397666205e-13, -.12421162189080181548e-14, .58167446553847312884e-16,
   -.17889335846010823161e-17, .27105054312137610850e-19
};

#define ETA_COEFFS_SIZE (sizeof(eta_coeffs)/sizeof(eta_coeffs[0]))

static double real_eta (double z) {
  long double s, c, p;
  int n, reflect = 0;
  if (z == 0) return 0.5;
  if (z < 0 && z == sun_round(z/2.0)*2.0) return 0.0;
  if (z < 0.5) {
    z = 1.0 - z;
    reflect = 1;
  }
  if (z == 1.0) return (double)M_LN2ld;
  s = 0.0L;
  for (n=ETA_COEFFS_SIZE; n > 0; n--) {
    c = eta_coeffs[n - 1];
    p = tools_powl(n, -z);
    s = fmal(c, p, s);
  }
  if (reflect) {
    long double z2, b, f, piz;
    z2 = tools_powl(2.0L, z);
    b = 2.0L*(1.0L - z2);
    f = z2 - 2.0L;
    piz = tools_powl(M_PIld, z);
    b = b/f/piz;
    return s*tools_gammal(z)*b*sun_cosl(M_PIO2ld*z);
  }
  return s;
}

static int calc_eta (lua_State *L) {  /* 2.41.0 */
  lua_Number r, s;
  s = agn_checknumber(L, 1);
  if (tools_isnonnegint(s)) {  /* with integral arguments real_eta is very, very slow, 6.4.7 fix */
    if (s < 0) r = 0.0;
    else if (s > CUTOFF) r = 1.0;
    else if (s == 0) r = 0.5;
    else if (s == 1) r = (double)M_LN2ld;
    else r = 1.0L + xDirichlet_Eta_Star_Function_pos_int_arg((int)s);
  } else {  /* 6.4.6 extension */
    r = real_eta(s);
  }
  lua_pushnumber(L, r);
  return 1;
}


/* Computes the confluent hypergeometric function 1F1(a, b; z) aka Kummer's function of the first kind. 3.10.6 */
static int calc_hyp1f1 (lua_State *L) {  /* general solution, comes close to Maple's hypergeom() as possible. */
  lua_pushnumber(L, hyp1f1(agn_checknumber(L, 1), agn_checknumber(L, 2), agn_checknumber(L, 3)));
  return 1;
}


static int calc_hyp1f1a (lua_State *L) {
  lua_Number err, a, b, z;
  a = agn_checknumber(L, 1);
  b = agn_checknumber(L, 2);
  z = agn_checknumber(L, 3);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushnumber(L, hy1f1a(a, b, z, &err));
  lua_pushnumber(L, err);
  return 2;
}


static int calc_hyp1f1p (lua_State *L) {
  lua_Number err, a, b, z;
  a = agn_checknumber(L, 1);
  b = agn_checknumber(L, 2);
  z = agn_checknumber(L, 3);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushnumber(L, hy1f1p(a, b, z, &err));
  lua_pushnumber(L, err);
  return 2;
}


/* Computes the Gaussian or ordinary hypergeometric function 2F1(a, b; c; z). 3.10.6 */
static int calc_hyp2f1 (lua_State *L) {
  lua_pushnumber(L,
    hyp2f1(agn_checknumber(L, 1), agn_checknumber(L, 2),
           agn_checknumber(L, 3), agn_checknumber(L, 4)));
  return 1;
}


/* Computes the complete and incomplete elliptic integral of the first kind, 3.10.7 */
static int calc_elliptic1 (lua_State *L) {
  if (lua_gettop(L) == 1) {
    lua_pushnumber(L, ellpk(1 - agn_checknumber(L, 1)));
  }
  else
    lua_pushnumber(L, ellik(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


/* Computes the complete and incomplete elliptic integral of the second kind, 3.10.7 */
static int calc_elliptic2 (lua_State *L) {
  if (lua_gettop(L) == 1)
    lua_pushnumber(L, ellpe(1 - agn_checknumber(L, 1)));
  else
    lua_pushnumber(L, ellie(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


/* calc.jacobian(u, m);
   Computes the Jacobian elliptic functions sn(u, m), cn(u, m) and dn(u, m) of real parameter m between 0 and 1,
   and real argument u, in this order and also returns phi, the amplitude of u, as a fourth result.

   The relation to the incomplete elliptic integral is as follows: If u = calc.elliptic1(phi, m), then sn(u, m) = sin(phi),
   and cn(u, m) = cos(phi), with phi the amplitude of u. 3.10.7 */
static int calc_jacobian (lua_State *L) {
  double sn, cn, dn, ph;
  ellpj(agn_checknumber(L, 1), agn_checknumber(L, 2), &sn, &cn, &dn, &ph);
  luaL_checkstack(L, 4, "not enough stack space");  /* 4.7.1 fix */
  lua_pushnumber(L, sn);
  lua_pushnumber(L, cn);
  lua_pushnumber(L, dn);
  lua_pushnumber(L, ph);
  return 4;
}


/* In the first form, returns the modified Bessel function of order zero of the argument.
   The function is defined as bessel0(x) = besselj(0, x*I).

   In the second form, returns the modified Bessel function of order zero of the argument, exponentially scaled. */
static int calc_bessel0 (lua_State *L) {  /* 3.10.8 */
  if (lua_gettop(L) == 1)
    lua_pushnumber(L, cephes_i0(agn_checknumber(L, 1)));
  else
    lua_pushnumber(L, cephes_i0e(agn_checknumber(L, 1)));
  return 1;
}


/* In the first form, returns the modified Bessel function of order one of the argument.
   The function is defined as bessel1(x) = besselj(1, x*I).

   In the second form, returns the modified Bessel function of order one of the argument, exponentially scaled. */
static int calc_bessel1 (lua_State *L) {  /* 3.10.8 */
  if (lua_gettop(L) == 1)
    lua_pushnumber(L, cephes_i1(agn_checknumber(L, 1)));
  else
    lua_pushnumber(L, cephes_i1e(agn_checknumber(L, 1)));
  return 1;
}


/* Computes the Bessel function of integer order, 6.4.9, UNDOC, see baselib/besselj */
static int calc_besseljn (lua_State *L) {
  lua_pushnumber(L, cephes_jn(agn_checkinteger(L, 1), agn_checknumber(L, 2)));
  return 1;
}


/* Computes the Bessel function of second kind, order zero, 4.6.9, UNDOC, see baselib/bessely */
static int calc_bessely0 (lua_State *L) {
  lua_pushnumber(L, cephes_y0(agn_checknumber(L, 1)));
  return 1;
}


/* Computes the Bessel function of second kind, order one, 4.6.9, UNDOC, see baselib/bessely */
static int calc_bessely1 (lua_State *L) {
  lua_pushnumber(L, cephes_y1(agn_checknumber(L, 1)));
  return 1;
}


/* Approximates the Lambert W function.
   double x: the argument.
   int branch: indicates the desired branch.
   * 0, the upper branch;
   * nonzero, the lower branch.
   int offset: indicates the interpretation of X.
   * 1, X is actually the offset from -(exp-1), so compute W(X-exp(-1)).
   * not 1, X is the argument; compute W(X); 6.4.5 */
static int calc_lambertw (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  int branch = agnL_optinteger(L, 2, 0);
  int offset = agnL_optinteger(L, 3, 0);
  lua_pushnumber(L, r8_lambert_w(x, branch, offset));
  return 1;
}


/* Transform section *****************************************************************************/

/* corr_unitemplate is based on corr_unitemplate in stats.c; 4.11.7 */
static int trans_unitemplate (lua_State *L, void (*f)(int, int, double [], double []), const char *procname) {
  int type, dst;
  size_t ii;
  lua_Number *a;
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1,
    "table, sequence, register or numarray expected", type);
  ii = 0;
  a = agnL_tonumarray(L, 1, &ii, procname, 1, 0);
  dst = agnL_optposint(L, 2, 1);
  if (dst > 4) {
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": second argument must be between 1 and 4, got %d.", procname, dst);
  }
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2)
    lua_pushfail(L);
  else if (type != LUA_TUSERDATA) {
    size_t i;
    double *c = (double *)agn_malloc(L, ii*sizeof(double), procname, a, NULL);
    (*f)(ii, dst, a, c);
    if (type == LUA_TTABLE) {
      lua_createtable(L, ii, 0);
      for (i=0; i < ii; i++)
        agn_setinumber(L, -1, i + 1, c[i]);
    } else if (type == LUA_TSEQ) {
      agn_createseq(L, ii);
      for (i=0; i < ii; i++)
        agn_seqsetinumber(L, -1, i + 1, c[i]);
    } else {
      agn_createreg(L, ii);
      for (i=0; i < ii; i++)
        agn_regsetinumber(L, -1, i + 1, c[i]);
    }
    xfree(c);
  } else {
    NumArray *b = NULL;
    numarray_createdouble(L, b, ii);
    (*f)(ii, dst, a, b->data.n);
  }
  xfree(a);  /* for all structures, free a */
  return 1;
}


/* Purpose: `calc.dct` finds the Fourier discrete cosine transform (DCT) of type m of
   a one-dimensional data vector v.
   By default, DCT-I is computed, but you can do DCT-II, DCT-III and DCT-IV by passing
   2, 3 or 4 for m.
   The inverse DCTs for types 1, 2, 3, and 4 are 1, 3, 2, and 4, in that order.
   Computation is done using 80-bit precision on Intel platforms, and Kahan-Babuska
   summation on ARM, to reduce round-off errors as much as possible.
   See also: `calc.dst`, `round`.
   Licensing: This code is distributed under the MIT license.
   Author: John Burkardt (26 August 2015), changes by A. Walz
   Parameters:
     Input, integer N, the number of data points.
     Input, double in[N], the vector of data.
     Output, double out[N], COSINE_TRANSFORM_DATA[N], the transform coefficients. */
static void cosine_transform_data (int n, int type, double in[], double out[]) {
#ifdef __ARMCPU
  double s, angle, cs, ccs;
#else
  long double s, angle;
#endif
  int i, j;
  if (in == NULL || n < 2) {
    for (i=0; i < n; i++) out[i] = 0.0;
    return;
  }
  switch (type) {
    case 1: {  /* validated with Mathematica */
#ifdef __ARMCPU
      for (i=0; i < n; i++) {
        s = cs = ccs = 0.0;
        for (j=1; j < n - 1; j++) {
          angle = PI/(n - 1.0)*(j*i);
          s = tools_kbadd(s, sun_cos(angle)*in[j], &cs, &ccs);
        }
        s = tools_kbadd(s, tools_intpow(-1, i) * 0.5*in[n - 1], &cs, &ccs);
        s = tools_kbadd(s, 0.5*in[0], &cs, &ccs);
        s += cs + ccs;
        s *= sqrt(2.0/(double)(n - 1));
        out[i] = s;
      }
#else
      for (i=0; i < n; i++) {
        s = 0.0L;
        for (j=1; j < n - 1; j++) {
          angle = M_PIld/(n - 1.0L)*(j*i);
          s += sun_cosl(angle)*in[j];
        }
        s += tools_intpowl(-1.0L, i) * 0.5L*in[n - 1];
        s += 0.5L*in[0];
        s *= sqrtl(2.0L/(n - 1.0L));
        out[i] = s;
      }
#endif
      break;
    }
    case 2: {  /* validated with Mathematica */
#ifdef __ARMCPU
      for (i=0; i < n; i++) {
        s = cs = ccs = 0.0;
        for (j=0; j < n; j++) {
          angle = PI/n * (j + 0.5)*i;
          s = tools_kbadd(s, sun_cos(angle)*in[j], &cs, &ccs);
        }
        s += cs + ccs;
        s *= 1.0/sqrt(n);
        out[i] = s;
      }
#else
      for (i=0; i < n; i++) {
        s = 0.0L;
        for (j=0; j < n; j++) {
          angle = M_PIld/n * (j + 0.5L)*i;
          s += sun_cosl(angle)*in[j];
        }
        s *= 1.0L/sqrtl(n);
        out[i] = s;
      }
#endif
      break;
    }
    case 3: {  /* validated with Mathematica */
#ifdef __ARMCPU
      for (i=0; i < n; i++) {
        s = cs = ccs = 0.0;
        for (j=1; j < n; j++) {
          angle = PI/n * (j*(i + 0.5));
          s = tools_kbadd(s, sun_cos(angle)*in[j], &cs, &ccs);
        }
        s = tools_kbadd(s, s, &cs, &ccs);
        s = tools_kbadd(s, in[0], &cs, &ccs);
        s += cs + ccs;
        s *= 1.0/sqrt(n);
        out[i] = s;
      }
#else
      for (i=0; i < n; i++) {
        s = 0.0L;
        for (j=1; j < n; j++) {
          angle = M_PIld/n * (j*(i + 0.5L));
          s += sun_cosl(angle)*in[j];
        }
        s *= 2.0L;
        s += in[0];
        s *= 1.0L/sqrtl(n);
        out[i] = s;
      }
#endif
      break;
    }
    case 4: {  /* validated with Mathematica */
#ifdef __ARMCPU
      for (i=0; i < n; i++) {
        s = cs = ccs = 0.0;
        for (j=0; j < n; j++) {
          angle = PI/n * ((i + 0.5)*(j + 0.5));
          s = tools_kbadd(s, sun_cos(angle)*in[j], &cs, &ccs);
        }
        s += cs + ccs;
        s *= sqrt(2.0/n);
        out[i] = s;
      }
#else
      for (i=0; i < n; i++) {
        s = 0.0L;
        for (j=0; j < n; j++) {
          angle = M_PIld/n * ((i + 0.5L)*(j + 0.5L));
          s += sun_cosl(angle)*in[j];
        }
        s *= sqrtl(2.0L/n);
        out[i] = s;
      }
#endif
      break;
    }
    default: {
      for (i=0; i < n; i++) out[i] = 0.0;
    }
  }
}

static int calc_dct (lua_State *L) {
  trans_unitemplate(L, cosine_transform_data, "calc.dct");
  return 1;
}


/* Purpose: SINE_TRANSFORM_DATA does a discrete sine transform (DCT) on data.
   Licensing: This code is distributed under the MIT license.
   Author: John Burkardt (19 February 2012);
           extended for types 2 to 4, 80-bit precision, K-B summation A. Walz
   Parameters:
     Input, integer N, the number of data points.
     Input, double in[N], the vector of data.
     Output, double out[N], the sine transform coefficients.
   Info:
     https://reference.wolfram.com/language/ref/FourierDST.html?q=FourierDST
     https://en.wikipedia.org/wiki/Discrete_sine_transform#DST-IV */
static void sine_transform_data (int n, int type, double in[], double out[]) {
#ifdef __ARMCPU
  double s, angle, cs, ccs;
#else
  long double s, angle;
#endif
  int i, j;
  if (in == NULL) {
    for (i=0; i < n; i++) out[i] = 0.0;
    return;
  }
  switch (type) {
    case 1: {  /* validated with Mathematica */
#ifdef __ARMCPU
      for (i=0; i < n; i++) {
        s = cs = ccs = 0.0;
        for (j=0; j < n; j++) {
          angle = PI/(n + 1.0)*(i + 1.0)*(j + 1.0);
          s = tools_kbadd(s, sun_sin(angle)*in[j], &cs, &ccs);
        }
        s += cs + ccs;
        s *= sqrt(2.0/(n + 1.0));
        out[i] = s;
      }
#else
      for (i=0; i < n; i++) {
        s = 0.0L;
        for (j=0; j < n; j++) {
          angle = M_PIld/(n + 1.0L)*(i + 1.0L)*(j + 1.0L);
          s += sun_sinl(angle)*in[j];
        }
        s *= sqrtl(2.0L/(n + 1.0L));
        out[i] = s;
      }
#endif
      break;
    }
    case 2: {  /* validated with Mathematica */
#ifdef __ARMCPU
      for (i=0; i < n; i++) {
        s = cs = ccs = 0.0;
        for (j=0; j < n; j++) {
          angle = PI/(double)n * (j + 0.5)*(i + 1.0);
          s = tools_kbadd(s, sun_sin(angle)*in[j], &cs, &ccs);
        }
        s += cs + ccs;
        s *= sqrt(1.0/(double)(n));
        out[i] = s;
      }
#else
      for (i=0; i < n; i++) {
        s = 0.0L;
        for (j=0; j < n; j++) {
          angle = M_PIld/n * (j + 0.5L)*(i + 1.0L);
          s += sun_sinl(angle)*in[j];
        }
        s *= sqrtl(1.0L/n);
        out[i] = s;
      }
#endif
      break;
    }
    case 3: {  /* validated with Mathematica */
#ifdef __ARMCPU
      for (i=0; i < n; i++) {
        s = cs = ccs = 0.0;
        for (j=0; j < n - 1; j++) {
          angle = PI/n * (j + 1.0)*(i + 0.5);
          s = tools_kbadd(s, sun_sin(angle)*in[j], &cs, &ccs);
        }
        s = tools_kbadd(2.0*s, tools_intpow(-1, i)*in[n - 1], &cs, &ccs);
        s += cs + ccs;
        s *= sqrt(1.0/n);
        out[i] = s;
      }
#else
      for (i=0; i < n; i++) {
        s = 0.0L;
        for (j=0; j < n - 1; j++) {
          angle = M_PIld/n * (j + 1.0L)*(i + 0.5L);
          s += sun_sinl(angle)*in[j];
        }
        s = 2.0L*s + tools_intpowl(-1.0L, i)*in[n - 1];
        s *= sqrtl(1.0L/n);
        out[i] = s;
      }
#endif
      break;
    }
    case 4: {  /* validated with Mathematica */
#ifdef __ARMCPU
      for (i=0; i < n; i++) {
        s = cs = ccs = 0.0;
        for (j=0; j < n; j++) {
          angle = PI/(double)n * ((i + 0.5)*(j + 0.5));
          s = tools_kbadd(s, sun_sin(angle)*in[j], &cs, &ccs);
        }
        s += cs + ccs;
        s *= sqrt(2.0/n);
        out[i] = s;
      }
#else
      for (i=0; i < n; i++) {
        s = 0.0L;
        for (j=0; j < n; j++) {
          angle = M_PIld/n * ((i + 0.5L)*(j + 0.5L));
          s += sun_sinl(angle)*in[j];
        }
        s *= sqrtl(2.0L/n);
        out[i] = s;
      }
#endif
      break;
    }
    default: {
      for (i=0; i < n; i++) out[i] = 0.0;
    }
  }
}

static int calc_dst (lua_State *L) {
  trans_unitemplate(L, sine_transform_data, "calc.dst");
  return 1;
}


/* Takes a point x and a table, sequence, register or numarray p of coefficients in ascending order of power,
   by default, and evaluates

      p[1] + x * (p[2] + x * (....)),

   i.e. a polynomial via Horner's rule.

   If the third optional argument is `false`, the coefficients in p are in descending order.

   The function internally uses 80-bit floating-point precision when available for better results.

   Example: To evaluate the polynomial 5*x^3 + 3*x^2 + 2*x + 1 at x = 2, issue:

   > calc.horner(2, [1, 2, 3, 5]):
   57

   > calc.horner(2, [5, 3, 2, 1], false):
   57

   Modeled after Julia's `horner` macro:

   macro horner(x, p...)
      ex = p[end]
      for i = length(p)-1:-1:1
         ex = :($(p[i]) + $x * $ex)
      end
      ex
   end

   See: https://github.com/JuliaLang/julia/pull/2987/commits/9c24795ac2918df7b8cba9bb129db8c535d321e8

   Based on stats.c/stats_median, 6.4.6 */
static int calc_horner (lua_State *L) {
  size_t ii;
  int i;
  lua_Number *a;
  long double r, x;
  luaL_checkany(L, 2);
  ii = 0;
  x = agn_checknumber(L, 1);
  a = agnL_tonumarray(L, 2, &ii, "calc.horner", 1, 0);
  if (a == NULL) return 1;  /* issue the error */
  if (agnL_optboolean(L, 3, 1)) {  /* ascending order of coefficients */
    r = a[ii - 1];
    for (i=ii - 2; i >= 0; i--) {
      r = fmal(r, x, (long double)a[i]);
    }
  } else {
    r = a[0];
    for (i=1; i < ii; i++) {
      r = fmal(r, x, (long double)a[i]);
    }
  }
  xfree(a);  /* for all structures, free a */
  lua_pushnumber(L, (lua_Number)r);
  return 1;
}


static const luaL_Reg calclib[] = {
  {"Ai", calc_Ai},                         /* added on June 29, 2015 */
  {"aitken", calc_aitken},                 /* added on September 20, 2022 */
  {"auxSiCi", calc_auxSiCi},               /* added on June 23, 2023 */
  {"bessel0", calc_bessel0},               /* added on March 03, 2024 */
  {"bessel1", calc_bessel1},               /* added on March 03, 2024 */
  {"besseljn", calc_besseljn},             /* added on December 02, 2025 */
  {"bessely0", calc_bessely0},             /* added on December 02, 2025 */
  {"bessely1", calc_bessely1},             /* added on December 02, 2025 */
  {"Bi", calc_Bi},                         /* added on June 29, 2015 */
  {"brent", calc_brent},                   /* added on April 30, 2025 */
  {"chandrupatla", calc_chandrupatla},     /* added on April 28, 2025 */
  {"cheby", calc_cheby},                   /* added on July 05, 2017 */
  {"cheby64", calc_cheby64},               /* added on July 05, 2017 */
  {"chebycoeffs", calc_chebycoeffs},       /* added on August 10, 2023 */
  {"chebygen", calc_chebygen},             /* added on August 10, 2023 */
  {"chebyt", calc_chebyt},                 /* added on October 17, 2022 */
  {"Chi", calc_Chi},                       /* added on May 24, 2010 */
  {"Ci", calc_Ci},                         /* added on May 24, 2010 */
  {"Cin", calc_Cin},                       /* added on June 23, 2023 */
  {"clampedspline", calc_clampedspline},   /* added on November 04, 2012 */
  {"clampedsplinecoeffs", calc_clampedsplinecoeffs},  /* added on November 04, 2012 */
  {"dct", calc_dct},                       /* added on April 24, 2025 */
  {"dawson", calc_dawson},                 /* added on May 25, 2010 */
  {"diff", calc_diff},                     /* added on May 29, 2010 */
  {"digamma", calc_digamma},               /* added on November 22, 2025 */
  {"dilog", calc_dilog},                   /* added on May 25, 2010 */
  {"dst", calc_dst},                       /* added on April 24, 2025 */
  {"Ei", calc_Ei},                         /* added on May 25, 2010 */
  {"Ein", calc_Ein},                       /* added on June 23, 2023 */
  {"elliptic1", calc_elliptic1},           /* added on February 27, 2024 */
  {"elliptic2", calc_elliptic2},           /* added on February 27, 2024 */
  {"En", calc_En},                         /* added on July 07, 2015 */
  {"eps", calc_eps},                       /* added on February 14, 2016 */
  {"eta", calc_eta},                       /* added on June 23, 2023 */
  {"eucliddist", calc_eucliddist},         /* added on January 25, 2016 */
  {"eulerdiff", calc_eulerdiff},           /* added on December 26, 2018 */
  {"expn", calc_expn},                     /* added on June 27, 2023 */
  {"fmaxbr", calc_fmaxbr},                 /* added on April 01, 2024 */
  {"fmaxgs", calc_fmaxgs},                 /* added on April 01, 2024 */
  {"fminbr", calc_fminbr},                 /* added on May 29, 2010 */
  {"fmings", calc_fmings},                 /* added on July 02, 2020 */
  {"fresnelc", calc_fresnelc},             /* added on May 29, 2010 */
  {"fresnels", calc_fresnels},             /* added on May 29, 2010 */
  {"gammainc", calc_gammainc},             /* added on July 27, 2023 */
  {"gauleg", calc_gauleg},                 /* added on July 01, 2020 */
  {"gauleg64", calc_gauleg64},             /* added on May 21, 2025 */
  {"gaussian", calc_gaussian},             /* added on August 06, 2017 */
  {"gtrap", calc_gtrap},                   /* added on March 04, 2014 */
  {"gtrap64", calc_gtrap64},               /* added on May 21, 2025 */
  {"harmonic", calc_harmonic},             /* added on April 11, 2024 */
  {"horner", calc_horner},                 /* added on November 26, 2025 */
  {"hyp1f1", calc_hyp1f1},                 /* added on February 26, 2024 */
  {"hyp1f1a", calc_hyp1f1a},               /* added on December 01, 2025 */
  {"hyp1f1p", calc_hyp1f1p},               /* added on December 01, 2025 */
  {"hyp2f1", calc_hyp2f1},                 /* added on February 26, 2024 */
  {"ibeta", calc_ibeta},                   /* added on July 08, 2015 */
  {"igamma", calc_igamma},                 /* added on July 07, 2015 */
  {"igammac", calc_igammac},               /* added on July 07, 2015 */
  {"intcc", calc_intcc},                   /* added on August 19, 2023 */
  {"intcc64", calc_intcc64},               /* added on May 21, 2025 */
  {"intde", calc_intde},                   /* added on May 18, 2010 */
  {"intde64", calc_intde64},               /* added on May 21, 2025 */
  {"intdei", calc_intdei},                 /* added on May 22, 2010 */
  {"intdei64", calc_intdei64},             /* added on May 21, 2025 */
  {"intdei2", calc_intdei2},               /* added on September 24, 2024 */
  {"intdeo", calc_intdeo},                 /* added on May 22, 2010 */
  {"intdeo64", calc_intdeo64},             /* added on May 21, 2025 */
  {"interp", calc_interp},                 /* added on October 28, 2012 */
  {"invibeta", calc_invibeta},             /* added on July 08, 2015 */
  {"iscont", calc_iscont},                 /* added on January 21, 2016 */
  {"isdiff", calc_isdiff},                 /* added on May 04, 2017 */
  {"itp", calc_itp},                       /* added on April 29, 2025 */
  {"jacobian", calc_jacobian},             /* added on February 28, 2024 */
  {"lambda", calc_lambda},                 /* added on May 17, 2020 */
  {"lambertw", calc_lambertw},             /* added on November 20, 2025 */
  {"Li", calc_Li},                         /* added on November 21, 2025 */
  {"limit", calc_limit},                   /* added on January 21, 2016 */
  {"linterp", calc_linterp},               /* added on January 28, 2014 */
  {"logistic", calc_logistic},             /* added on August 04, 2017 */
  {"logit", calc_logit},                   /* added on August 01, 2022 */
  {"mean", calc_mean},                     /* added on September 24, 2024 */
  {"mean64", calc_mean64},                 /* added on May 24, 2025 */
  {"nakspline", calc_nakspline},           /* added on October 28, 2012 */
  {"naksplinecoeffs", calc_naksplinecoeffs}, /* added on October 28, 2012 */
  {"neville", calc_neville},               /* added on October 28, 2012 */
  {"newtoncoeffs", calc_newtoncoeffs},     /* added on October 28, 2012 */
  {"polyfit", calc_polyfit},               /* added on January 28, 2014 */
  {"polygen", calc_polygen},               /* added on July 21, 2013 */
  {"polylog", calc_polylog},               /* added on July 07, 2015 */
  {"probit", calc_probit},                 /* added on August 01, 2022 */
  {"rc", calc_rc},                         /* added on November 22, 2025 */
  {"regulafalsi", calc_regulafalsi},       /* added on May 29, 2010 */
  {"riesum", calc_riesum},                 /* added on September 24, 2024 */
  {"riesum64", calc_riesum64},             /* added on May 24, 2025 */
  {"romberg", calc_romberg},               /* added on February 13, 2026 */
  {"savgol", calc_savgol},                 /* added on July 16, 2017 */
  {"savgol64", calc_savgol64},             /* added on April 18, 2026 */
  {"scaleddawson", calc_scaleddawson},     /* added on July 21, 2020 */
  {"sections", calc_sections},             /* added on May 29, 2010 */
  {"Shi", calc_Shi},                       /* added on May 24, 2010 */
  {"Si", calc_Si},                         /* added on May 24, 2010 */
  {"SiCi", calc_SiCi},                     /* added on April 24, 2025 */
  {"sigmoid", calc_sigmoid},               /* added on March 04, 2014 */
  {"simaptive", calc_simaptive},           /* added on March 04, 2014 */
  {"simaptive64", calc_simaptive64},       /* added on May 21, 2025 */
  {"smoothstep", calc_smoothstep},         /* added on August 02, 2017 */
  {"softsign", calc_softsign},             /* added on October 18, 2018 */
  {"Ssi", calc_Ssi},                       /* added on May 24, 2010 */
  {"variance", calc_variance},             /* added on July 01, 2020 */
  {"w", calc_w},                           /* added on July 26, 2020 */
  {"weier", calc_weier},                   /* added on June 16, 2018 */
  {"xpdiff", calc_xpdiff},                 /* added on May 29, 2010 */
  {"zeroab", calc_zeroab},                 /* added on August 05, 2024 */
  {"zeroin", calc_zeroin},                 /* added on February 02, 2022 */
  {"zeta", calc_zeta},                     /* added on July 07, 2015 */
  {NULL, NULL}
};


/*
** Open calculus library
*/

LUALIB_API int luaopen_calc (lua_State *L) {
#ifndef __ARMCPU
  intdeini(NSLOTS,  INTEG_TINY, INTEG_EPS, intde_w);   /* 3.3.5 40 percent boost */
  intdeiini(NSLOTS, INTEG_TINY, INTEG_EPS, intdei_w);  /* 3.11.5 40 percent boost */
  intdeoini(NSLOTS, INTEG_TINY, INTEG_EPS, intdeo_w);  /* 3.11.5 40 percent boost */
  intccini(NSLOTS, intcc_w);
#endif
  intdeini64(NSLOTS,  INTEG_TINY64, INTEG_EPS64, intde_w64);   /* 4.12.6 */
  intdeiini64(NSLOTS, INTEG_TINY64, INTEG_EPS64, intdei_w64);  /* 4.12.6 */
  intdeoini64(NSLOTS, INTEG_TINY64, INTEG_EPS64, intdeo_w64);  /* 4.12.6 */
  intccini64(NSLOTS, intcc_w64);  /* 4.12.6 */
  luaL_newmetatable(L, "calc");
  luaL_register(L, NULL, numarray_arraylib);  /* associate __gc method */
  luaL_register(L, AGENA_CALCLIBNAME, calclib);
  return 1;
}

