/*
** $Id: lfractals.c, initiated June 15, 2008 $
** Fractals library
** See Copyright Notice in agena.h
*/

/* for C99 only */

#include <stdlib.h>

#define fractals_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnconf.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "lcomplex.h"  /* for agnCmplx_create, at least DJGPP requires it */

#if !(defined(__OS2__) || defined(__DJGPP__) || defined(LUA_ANSI))
#define AGENA_FRACTLIBNAME "fractals"
LUALIB_API int (luaopen_fractals) (lua_State *L);
#endif


static int fractals_mandel (lua_State *L) {
  lua_Number p, q, x, y, a, r, radius;
  int i, iter, nargs;
  nargs = lua_gettop(L);
  p = agn_checknumber(L, 1);
  q = agn_checknumber(L, 2);
  iter = (int)agn_checknumber(L, 3);
  radius = agn_checknumber(L, 4);
  for (i=nargs; i > 0 && lua_isnil(L, i); i--) nargs--;  /* 7.7.5: some `gdi` functions pass trailing null(s) */
  if (nargs == 4) {  /* 7.7.5 extension for Julia sets */
    x = p; y = q;  /* compute Mandelbrot set */
  } else if (nargs == 5 && lua_iscomplex(L, 5)) {  /* compute Julia set */
    x = agn_complexreal(L, 5); y = agn_compleximag(L, 5);
  } else if (nargs == 6) {  /* ditto */
    x = agn_checknumber(L, 5); y = agn_checknumber(L, 6);
  } else {
    x = y = 0.0;
    luaL_error(L, "Error in " LUA_QS ": wrong number of arguments.", "fractals.mandel");
  }
  if (nargs < 5) {  /* for Mandelbrot sets use Cardioid/Bulb filtering; 7.10.0 */
    lua_Number ci_sq = tools_square(y);
    lua_Number qq = tools_square(x - 0.25) + ci_sq;
    /* Main Cardioid Test */
    if (qq*(qq + (x - 0.25)) < 0.25 * ci_sq) {
      lua_pushnumber(L, iter);
      return 1; /* Point is definitely inside the main black cardioid */
    }
    /* Period-2 Bulb Test */
    if (tools_square(x + 1.0) + ci_sq < 0.0625) {
      lua_pushnumber(L, iter);
      return 1;  /* Point is definitely inside the smaller left black bulb! */
    }
  }
  a = sun_hypot(p, q);  /* 2.9.8 improvement */
  for (i=0; i < iter && a < radius; i++) {
    r = p;
    p = p*p - q*q + x;
    q = 2*r*q + y;
    a = sun_hypot(p, q);  /* 2.9.8 improvement */
  }
  lua_pushnumber(L, i);
  return 1;
}


/* lambdasin */

static int fractals_lsin (lua_State *L) {
  lua_Number p, q, a, t3, t6, radius, psin, pcos, qsinh, qcosh;  /* 1.12.9 */
  int i, iter;
  p = agn_checknumber(L, 1);
  q = agn_checknumber(L, 2);
  iter = (int)agn_checknumber(L, 3);
  radius = agn_checknumber(L, 4);
  a = sun_hypot(p, q);  /* 2.9.8 improvement */
  for (i=0; i < iter && a < radius; i++) {
    sun_sincos(p, &psin, &pcos);    /* 2.11.0 improvement */
    sun_sinhcosh(q, &qsinh, &qcosh);  /* 2.11.0 improvement; 2.40.2 tweak */
    t3 = psin*qcosh;
    t6 = pcos*qsinh;
    p = 1.0*t3 - 0.4*t6;
    q = 0.4*t3 + 1.0*t6;
    a = sun_hypot(p, q);  /* 2.9.8 improvement */
  }
  lua_pushnumber(L, i);
  return 1;
}


/* The fractal properties of Netwon's method to calculate complex roots were first
   discovered by John Hubbard of Orsay, France. */

static int fractals_newton (lua_State *L) {
  lua_Number p, oldp, q, a, t1, t2, t3, t5, t6, t7, t8, t11, t15, t17, radius;
  int i, iter;
  p = agn_checknumber(L, 1);
  q = agn_checknumber(L, 2);
  iter = (int)agn_checknumber(L, 3);
  radius = agn_checknumber(L, 4);
  t1 = p*p;
  t2 = t1*t1;
  t3 = q*q;
  t7 = t3*t3;
  a = sqrt(t2*t1 + 3.0*t2*t3 - 2.0*t1*p + 3.0*t1*t7 + 6.0*p*t3 + 1.0 + t7*t3);
  for (i=0; i < iter && a >= radius; i++) {
    t1 = p*p;
    t3 = q*q;
    t5 = -t1*p/3 + p*t3 + 1.0/3.0;
    t6 = t1-t3;
    t8 = t6*t6;
    t11 = 1/(t8 + 4.0*t1*t3);
    t15 = -t1*q + t3*q/3;
    t17 = q*t11;
    oldp = p;
    p = p + t5*t6*t11 + 2.0*t15*p*t17;
    q = q + t15*t6*t11 - 2.0*t5*oldp*t17;
    t1 = p*p;
    t2 = t1*t1;
    t3 = q*q;
    t7 = t3*t3;
    a = sqrt(t2*t1 + 3.0*t2*t3 - 2.0*t1*p + 3.0*t1*t7 + 6.0*p*t3 + 1.0 + t7*t3);
  }
  lua_pushnumber(L, i);
  return 1;
}

/* Complex Mark's Mandelbrot set, using complex C arithmetic, July 21, 2008/June 06, 2009 */

static int fractals_markmandel (lua_State *L) {
  lua_Number a, b, r, m, n, radius, t1, t2, t4, t5, t7, t9, t11, t12, t13, t16;
  int i, iter;
  a = agn_checknumber(L, 1);
  b = agn_checknumber(L, 2);
  m = a; n = b;
  t4 = m*m;
  t5 = n*n;
  t7 = sun_log(t4 + t5);  /* 2.11.1 tuning */
  t9 = sun_exp(0.5E-1*t7);
  t11 = sun_atan2(n, m);
  t12 = 0.1*t11;
  sun_sincos(t12, &t16, &t13);
  iter = (int)agn_checknumber(L, 3);
  radius = agn_checknumber(L, 4);
  for (i=0; i < iter && sun_hypot(a, b) < radius; i++) {  /* 2.9.8 improvement */
    t1 = a*a;
    t2 = b*b;
    r = a;
    a = (t1 - t2)*t9*t13 - 2.0*a*b*t9*t16 + m;
    b = 2.0*r*b*t9*t13 + (t1 - t2)*t9*t16 + n;
  }
  lua_pushnumber(L, i);
  return 1;
}


static int fractals_esctime (lua_State *L) {
#ifndef PROPCMPLX
  agn_Complex z, c;
#else
  lua_Number z[2], c[2], z0, z1;
#endif
  size_t i, iter;  /* 1.9.3 */
  lua_Number r;
  luaL_checktype(L, 1, LUA_TFUNCTION);
#ifndef PROPCMPLX
  __real__ z = agn_checknumber(L, 2);  /* 2.11.2 */
  __imag__ z = agn_checknumber(L, 3);
  __real__ c = agn_checknumber(L, 4);  /* 2.11.2 */
  __imag__ c = agn_checknumber(L, 5);
#else
  z[0] = agn_checknumber(L, 2);
  z[1] = agn_checknumber(L, 3);
  c[0] = agn_checknumber(L, 4);
  c[1] = agn_checknumber(L, 5);
#endif
  iter = agn_checknumber(L, 6);  /* idx 4: number of iterations, 1.9.3 */
  r = agnL_optnumber(L, 7, 2);   /* idx 5: radius, default is 2 */
  lua_settop(L, 6);  /* get rid of surplus arguments, 4.5.6/7.7.5 change */
#ifndef PROPCMPLX
  agn_createcomplex(L, c);
#else  /* 7.6.6 fix */
  agn_createcomplex(L, c[0], c[1]);
#endif
  lua_insert(L, 6);              /* idx 6: complex c */
#ifndef PROPCMPLX
  for (i=0; i < iter && tools_cabs(z) < r; i++) {
#else
  for (i=0; i < iter && tools_cabs(z[0], z[1]) < r; i++) {
#endif
    luaL_checkstack(L, 3, "not enough stack space");  /* 2.31.7/4.5.6 fix */
    lua_pushvalue(L, 1);
#ifndef PROPCMPLX
    agn_createcomplex(L, z);
#else  /* 7.6.6 fix */
    agn_createcomplex(L, z[0], z[1]);
#endif
    lua_pushvalue(L, 6);
#ifndef PROPCMPLX
    z = agn_ccall(L, 2);  /* agn_ccall pops the computed result */
#else
    agn_ccall(L, 2, &z0, &z1);  /* agn_ccall pops the computed result */
    z[0] = z0; z[1] = z1;
#endif
  }
  lua_pushnumber(L, i);
  return 1;
}


static int fractals_lbea (lua_State *L) {
  lua_Number p, q, a, t3, t6, radius, pntre, pntim, psin, pcos, qsinh, qcosh;  /* 1.12.9 */
#ifndef PROPCMPLX
  agn_Complex pnt;
#endif
  int i, iter;
  p = agn_checknumber(L, 1);
  q = agn_checknumber(L, 2);
  iter = agn_checkposint(L, 3);
  radius = agn_checknonnegative(L, 4);
#ifndef PROPCMPLX
  pnt = agn_optcomplex(L, 5, 1+I*0.4);
  pntre = creal(pnt);
  pntim = cimag(pnt);
#else  /* 3.10.8 */
  lua_Number def[2] = { 1.0, 0.4 };
  agn_optcomplex(L, 5, def, &pntre, &pntim);
#endif
  a = sun_hypot(p, q);  /* 2.9.8 improvement */
  for (i=0; i < iter && a < radius; i++) {
    sun_sincos(p, &psin, &pcos);    /* 2.11.0 improvement */
    sun_sinhcosh(q, &qsinh, &qcosh);  /* 2.11.0 improvement, 2.40.2 tweak, 4.5.7 overflow tweak */
    t3 = psin*qsinh;
    t6 = pcos*qcosh;
    p = pntre*t3 - pntim*t6;
    q = pntim*t3 + pntre*t6;
    a = sun_hypot(p, q);  /* 2.9.8 improvement */
  }
  lua_pushnumber(L, i);
  return 1;
}


/* Computes:

fractals.lambdafn := proc(fn, x, y, iter, radius, c) is
   local z;
   z := x!y;
   c := c or 1!0.4;
   for i from 0 to iter while |z| < radius do
      z := c * fn(z)
   od;
   return i
end;
4.5.6, 21 % faster than the Agena version. */

static int fractals_lambdafn (lua_State *L) {
  lua_Number p, q, radius;
#ifndef PROPCMPLX
  agn_Complex z, pnt;
#else
  lua_Number z[2], z0, z1, pntre, pntim;
#endif
  int i, iter;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  p = agn_checknumber(L, 2);
  q = agn_checknumber(L, 3);
  iter = agn_checkposint(L, 4);
  radius = agn_checknonnegative(L, 5);
#ifndef PROPCMPLX
  pnt = agn_optcomplex(L, 6, 1 + I*0.4);
  z = p + I*q;
#else
  lua_Number pnt[2] = { 1.0, 0.4 };
  agn_optcomplex(L, 6, pnt, &pntre, &pntim);
  z[0] = p; z[1] = q;
#endif
#ifndef PROPCMPLX
  for (i=0; i < iter && tools_cabs(z) < radius; i++) {
#else
  for (i=0; i < iter && tools_cabs(z[0], z[1]) < radius; i++) {
#endif
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushvalue(L, 1);  /* push function */
#ifndef PROPCMPLX
    agn_createcomplex(L, z);  /* push z */
    z = pnt*agn_ccall(L, 1);  /* agn_ccall pops the computed result */
#else
    agn_createcomplex(L, p, q);
    agn_ccall(L, 2, &z0, &z1);  /* agn_ccall pops the computed result */
    agnc_mul(z, pntre, pntim, z0, z1);
#endif
  }
  lua_pushnumber(L, i);
  return 1;
}


/* Produces the Henon map. 20 % faster than the Agena implementation. 7.7.5
   local procedure henon_attractor(x, y, a, b) is
      # Computes the next step in the Henon map
      return 1 - a*x^2 + y, b*x
   end; */
static int fractals_henonattr (lua_State *L) {
   lua_Number x, y, a, b;
   x = agn_checknumber(L, 1);
   y = agn_checknumber(L, 2);
   a = agn_checknumber(L, 3);
   b = agn_checknumber(L, 4);
   luaL_checkstack(L, 2, "not enough stack space");
   lua_pushnumber(L, 1 - a*x*x + y);
   lua_pushnumber(L, b*x);
   return 2;
}


/* Implements the Logistic map function r * x * (1 - x), 13 % faster than the Agena version, 7.7.6 */
static int fractals_logisticmap (lua_State *L) {
   lua_Number r, x;
   r = agn_checknumber(L, 1);
   x = agn_checknumber(L, 2);
   lua_pushnumber(L, r * x * (1 - x));
   return 1;
}


static const luaL_Reg fractlib[] = {
  {"esctime", fractals_esctime},            /* added June 20, 2008 */
  {"henonattr", fractals_henonattr},        /* added July 13, 2026 */
  {"lambdafn", fractals_lambdafn},          /* added November 17, 2024 */
  {"lbea", fractals_lbea},                  /* added July 03, 2008 */
  {"logisticmap", fractals_logisticmap},    /* added July 13, 2026 */
  {"lsin", fractals_lsin},                  /* added June 15, 2008 */
  {"markmandel", fractals_markmandel},      /* added on July 21, 2008 */
  {"mandel", fractals_mandel},              /* added June 15, 2008 */
  {"newton", fractals_newton},              /* added June 15, 2008 */
  {NULL, NULL}
};

/*
** Open fractals library
*/
LUALIB_API int luaopen_fractals (lua_State *L) {
  luaL_register(L, AGENA_FRACTLIBNAME, fractlib);
  return 1;
}
