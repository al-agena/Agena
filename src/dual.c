/*
** $Id: dual.c,v 0.1 06/Nov/2018 $
** Dual Library
** See Copyright Notice in agena.h
**
** All the hyper-dual and eight-dual code has been created by Gemini AI, put to the public domain.
**
** If not indicated otherwise, the formulas below have been taken from:
** https://blog.demofox.org/2014/12/30/dual-numbers-automatic-differentiation/
**
** This is the edition using registers to store dual numbers. It is 5 % faster than the one
** implementing dual numbers as userdata.
*/

#define dual_c
#define LUA_LIB

#include <math.h>
#include <stdlib.h>

#include "agena.h"

#include "agenalib.h"
#include "agnconf.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "cephes.h"
#include "dual.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_DUALLIBNAME "dual"
LUALIB_API int (luaopen_dual) (lua_State *L);
#endif


int checkdual (lua_State *L, int idx, const char *procname, int toconstant) {
  if (lua_isreg(L, idx) && agn_isutype(L, idx, AGENA_DUALLIBNAME)) {
    int s = agn_regsize(L, idx);
    if (!(s == 2 || s == 4 || s == 8)) { goto errorlabel; }
    return s;
  }
  if (agn_isnumber(L, idx)) {  /* 6.6.10 extension */
    lua_Number x = agn_tonumber(L, idx);
    if (toconstant) {
      createeightdual(L, x, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);  /* 6.7.4 fix */
    } else {
      createeightdual(L, x, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0);  /* 6.8.1 extension */
    }
    lua_replace(L, idx);
    return tools_isfinite(x) ? 8 : 1;  /* 6.7.4 fix */
  }
errorlabel:
  luaL_error(L, "Error in " LUA_QS ": expected a dual number.", procname);
  return 0;
}


static int mt_tostring (lua_State *L) {  /* at the console, the array is formatted as follows: */
  lua_Number x, y;
  int i, s, flag = 1;
  s = checkdual(L, 1, "dual.__tostring", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }
  if (s == 8) {
    lua_Number x;
    luaL_checkstack(L, 3, "not enough stack space");
    lua_pushfstring(L, "%s(", AGENA_DUALLIBNAME);
    for (i=0; i < 8; i++) {
      x = agn_reggetinumber(L, 1, i + 1);
      lua_pushfstring(L, "%f", x);
      if (i != 7) { lua_pushstring(L, (i == 3) ? "," : ", "); };
      if (i == 3) { lua_pushstring(L, "\n     ") ; }
      lua_concat(L, 2 + (i != 7) + (i == 3));
    }
    lua_pushstring(L, ")");
    lua_concat(L, 2);
    return 1;
  }
  x = agn_reggetinumber(L, 1, 1);
  y = agn_reggetinumber(L, 1, 2);
jumpback:
  luaL_checkstack(L, 3 + (y >= 0), "not enough stack space");
  lua_pushfstring(L, "%f", x);
  if (y == -0) y = 0;
  if (flag == 0) lua_pushstring(L, "e2");
  if (y >= 0) lua_pushstring(L, "+");
  lua_pushfstring(L, "%f", y);
  if (s == 2)
    lua_pushstring(L, "e");
  else
    lua_pushstring(L, flag == 1 ? "e1" : "e1e2");
  lua_concat(L, 3 + (y >= 0) + (flag == 0));
  if (flag && s == 4) {
    x = agn_reggetinumber(L, 1, 3);
    y = agn_reggetinumber(L, 1, 4);
    if (x >= 0) {
      lua_pushstring(L, "+");
      lua_concat(L, 2);
    }
    flag = 0;
    goto jumpback;
  }
  if (!flag) lua_concat(L, 2);
  return 1;
}


static void arrayeq (lua_State *L, int (*fn)(lua_Number, lua_Number, lua_Number), const char *procname) {
  int i, flag, sx, sy;
  lua_Number eps;
  sx = checkdual(L, 1, procname, 1);
  sy = checkdual(L, 2, procname, 1);
  if (sx != sy || sx == 1 || sy == 1) {
    flag = 0;
  } else {  /* 6.7.5 improvement */
    eps = agn_getepsilon(L);
    flag = 1;
    for (i=0; i < sx && flag; i++) {
      flag = fn(agn_reggetinumber(L, 1, i + 1), agn_reggetinumber(L, 2, i + 1), eps);
    }
  }
  lua_pushboolean(L, flag);
}

static int equal (lua_Number x, lua_Number y, lua_Number eps) {  /* strict equality, eps does not matter */
  return x == y;
}

static int mt_aeq (lua_State *L) {
  arrayeq(L, tools_approx, "dual.__aeq");
  return 1;
}


static int mt_eeq (lua_State *L) {
  arrayeq(L, equal, "dual.__(e)eq");
  return 1;
}


/* Class: dualnumber, source file: dualnumber.h,
   adl.stanford.edu/hyperdual/dualnumber.h
   by: Jeffrey A. Fike, Stanford University, Department of Aeronautics and Astronautics */
static int mt_le (lua_State *L) {
  int sx, sy;
  lua_Number a1, b1;
  sx = checkdual(L, 1, "dual.__le", 1);
  sy = checkdual(L, 2, "dual.__le", 1);
  if (sx == 1 || sy == 1) { lua_pushfalse(L); return 1; }  /* 6.7.4 fix */
  if (sx != sy)
    luaL_error(L, "Error in " LUA_QS ": expected dual numbers of the same kind only.", "dual.__le");
  a1 = agn_reggetinumber(L, 1, 1);
  b1 = agn_reggetinumber(L, 2, 1);
  lua_pushboolean(L, a1 <= b1);
  return 1;
}


/* Class: dualnumber, source file: dualnumber.h,
   adl.stanford.edu/hyperdual/dualnumber.h
   by: Jeffrey A. Fike, Stanford University, Department of Aeronautics and Astronautics */
static int mt_lt (lua_State *L) {
  int sx, sy;
  lua_Number a1, b1;
  sx = checkdual(L, 1, "dual.__lt", 1);
  sy = checkdual(L, 2, "dual.__lt", 1);
  if (sx == 1 || sy == 1) { lua_pushfalse(L); return 1; }  /* 6.7.4 fix */
  if (sx != sy)
    luaL_error(L, "Error in " LUA_QS ": expected dual numbers of the same kind only.", "dual.__lt");
  a1 = agn_reggetinumber(L, 1, 1);
  b1 = agn_reggetinumber(L, 2, 1);
  lua_pushboolean(L, a1 < b1);
  return 1;
}


static int mt_unm (lua_State *L) {
  lua_Number a1, a2;
  int s = checkdual(L, 1, "dual.__unm", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a1 = agn_reggetinumber(L, 1, 1);
  a2 = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, -a1, -a2);
  } else if (s == 4) {
    long double a3, a4;
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    createhyperdual(L, -a1, -a2, -a3, -a4);
  } else {
    int i;
    long double a[8], r[8];
    /* Load and negate all 8 slots */
    a[0] = a1; a[1] = a2;
    r[0] = -a[0]; r[1] = -a[1];
    for (i=2; i < 8; i++) {
      a[i] = agn_reggetinumber(L, 1, i + 1);
      r[i] = -a[i];
    }
    createeightdual(L, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
    return 1;
  }
  return 1;
}


/* https://ncatlab.org/nlab/show/dual+number gives just fabs(agn_reggetinumber(L, 1, 1)).
   https://gist.github.com/chris-taylor/2005955 gives createdual(L, fabs(a1), a2*tools_sign(a1));
   6.6.6 change as proposed by Gemini AI. Extended to hyper-duals 6.6.8, code created by Gemini AI. */
static int mt_abs (lua_State *L) {
  long double a[8], r[8], real, df;
  int i, s = checkdual(L, 1, "dual.abs", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  /* Load the real part */
  a[0] = agn_reggetinumber(L, 1, 1);
  if (a[0] == 0.0L) {
    /* The derivative is undefined at the cusp unless all infinitesimal parts are zero
       (the function is locally constant). */
    for (i = 2; i <= s; i++) {
      if (agn_reggetinumber(L, 1, i) != 0.0L) {
        lua_pushundefined(L);
        return 1;
      }
    }
    /* If we reach here, it's a pure real zero */
    if (s == 2) { createdual(L, 0.0L, 0.0L); }
    else if (s == 4) { createhyperdual(L, 0.0L, 0.0L, 0.0L, 0.0L); }
    else { createeightdual(L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L); }
  } else {
    /* Standard Case: f(x) = |x|, f'(x) = sgn(x), f'' = 0, f''' = 0 */
    real = fabsl(a[0]);
    df = (a[0] > 0.0L) ? 1.0L : -1.0L;
    if (s == 2) {
      r[1] = agn_reggetinumber(L, 1, 2);
      createdual(L, real, r[1]*df);
    } else if (s == 4) {
      /* Since ddf=0, interaction terms vanish: r_i = a_i*df */
      r[1] = agn_reggetinumber(L, 1, 2)*df;
      r[2] = agn_reggetinumber(L, 1, 3)*df;
      r[3] = agn_reggetinumber(L, 1, 4)*df;
      createhyperdual(L, real, r[1], r[2], r[3]);
    } else {
      /* 8-slot Tri-Dual: All slots are simply a[i]*sgn(a[0]) */
      for (i = 1; i < 8; i++) {
        r[i] = agn_reggetinumber(L, 1, i + 1)*df;
      }
      createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
    }
  }
  return 1;
}


/* Created by Gemini AI, put to the public domain. 6.6.8 */
static int mt_sign (lua_State *L) {
  long double a;
  int i, s = checkdual(L, 1, "dual.__sign", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  for (i=0; i < s; i++) {
    a = agn_reggetinumber(L, 1, i + 1);
    if (a > 0.0L) {
      lua_pushnumber(L, 1);
      return 1;
    }
    if (a < 0.0L) {
      lua_pushnumber(L, -1);
      return 1;
    }
  }
  lua_pushnumber(L, 0);
  return 1;
}


/* Addition a + b */
static int mt_add (lua_State *L) {  /* extended 2.14.12 */
  long double a1, a2, b1, b2;  /* change to long double 6.6.6 */
  if ((agn_isnumber(L, 1) && !tools_isfinite(agn_tonumber(L, 1))) ||
      (agn_isnumber(L, 2) && !tools_isfinite(agn_tonumber(L, 2)))) {
    /* 6.7.4 fix to prevent freezes */
    lua_settop(L, agn_isnumber(L, 1) ? 1 : 2);
    return 1;
  }
  if (agn_isnumber(L, 1) && isdual(L, 2)) {
    a1 = agn_tonumber(L, 1);
    a2 = 0;
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    createdual(L, a1 + b1, b2);
  } else if (isdual(L, 1) && agn_isnumber(L, 2)) {
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    b1 = agn_tonumber(L, 2);
    b2 = 0;
    createdual(L, a1 + b1, b1);
  } else if (isdual(L, 1) && isdual(L, 2)) {
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    createdual(L, a1 + b1, a2 + b2);
  } else if (ishyperdual(L, 1) && ishyperdual(L, 2)) {
    long double a3, a4, b3, b4;
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    b3 = agn_reggetinumber(L, 2, 3);
    b4 = agn_reggetinumber(L, 2, 4);
    createhyperdual(L, a1 + b1, a2 + b2, a3 + b3, a4 + b4);
  } else if (ishyperdual(L, 1) && agn_isnumber(L, 2)) {  /* 6.6.10 extension */
    createhyperdual(L, agn_tonumber(L, 2), 0.0, 0.0, 0.0);
    lua_replace(L, 2);
    return mt_add(L);
  } else if (agn_isnumber(L, 1) && ishyperdual(L, 2)) {
    createhyperdual(L, agn_tonumber(L, 1), 0.0, 0.0, 0.0);
    lua_replace(L, 1);
    return mt_add(L);
  } else if (iseightdual(L, 1) && iseightdual(L, 2)) {
    int i;
    long double a[8], b[8], r[8];
    checkdual(L, 1, "dual.__add", 1);
    checkdual(L, 2, "dual.__add", 1);
    /* Load Registers (0-based)
       agn_reggetinumber should handle the logic for returning 0.0L
       for slots that don't exist if a standard number is passed. */
    for (i=0; i < 8; i++) {
      a[i] = agn_reggetinumber(L, 1, i + 1);
      b[i] = agn_reggetinumber(L, 2, i + 1);
    }
    /* Linear operation: r[i] = a[i] + b[i] for all 8 slots */
    for (i=0; i < 8; i++) {
      r[i] = a[i] + b[i];
    }
    createeightdual(L, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  } else if (iseightdual(L, 1) && agn_isnumber(L, 2)) {
    createeightdual(L, agn_tonumber(L, 2), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    lua_replace(L, 2);
    return mt_add(L);
  } else if (agn_isnumber(L, 1) && iseightdual(L, 2)) {
    createeightdual(L, agn_tonumber(L, 1), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    lua_replace(L, 1);
    return mt_add(L);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected number or dual number.", "dual.__add");
  }
  return 1;
}


/* Subtraction: a - b */
static int mt_sub (lua_State *L) {  /* extended 2.14.12 */
  long double a1, a2, b1, b2;  /* change to long double 6.6.6 */
  if ((agn_isnumber(L, 1) && !tools_isfinite(agn_tonumber(L, 1))) ||
      (agn_isnumber(L, 2) && !tools_isfinite(agn_tonumber(L, 2)))) {
    /* 6.7.4 fix to prevent freezes */
    lua_settop(L, agn_isnumber(L, 1) ? 1 : 2);
    return 1;
  }
  if (agn_isnumber(L, 1) && isdual(L, 2)) {
    a1 = agn_tonumber(L, 1);
    a2 = 0;
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    createdual(L, a1 - b1, -b2);
  } else if (isdual(L, 1) && agn_isnumber(L, 2)) {
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    b1 = agn_tonumber(L, 2);
    b2 = 0;
    createdual(L, a1 - b1, a2);
  } else if (isdual(L, 1) && isdual(L, 2)) {
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    createdual(L, a1 - b1, a2 - b2);
  } else if (ishyperdual(L, 1) && ishyperdual(L, 2)) {
    long double a3, a4, b3, b4;
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    b3 = agn_reggetinumber(L, 2, 3);
    b4 = agn_reggetinumber(L, 2, 4);
    createhyperdual(L, a1 - b1, a2 - b2, a3 - b3, a4 - b4);
  } else if (ishyperdual(L, 1) && agn_isnumber(L, 2)) {  /* 6.6.10 extension */
    createhyperdual(L, agn_tonumber(L, 2), 0.0, 0.0, 0.0);
    lua_replace(L, 2);
    return mt_sub(L);
  } else if (agn_isnumber(L, 1) && ishyperdual(L, 2)) {
    createhyperdual(L, agn_tonumber(L, 1), 0.0, 0.0, 0.0);
    lua_replace(L, 1);
    return mt_sub(L);
  } else if (iseightdual(L, 1) && iseightdual(L, 2)) {
    int i;
    long double a[8], b[8], r[8];
    checkdual(L, 1, "dual.__sub", 1);
    checkdual(L, 2, "dual.__sub", 1);
    /* Load Registers (0-based)
       Note: agn_reggetinumber abstracts whether the input is a number or dual */
    for (i=0; i < 8; i++) {
      a[i] = agn_reggetinumber(L, 1, i + 1);
      b[i] = agn_reggetinumber(L, 2, i + 1);
    }
    /* Linear operation: r[i] = a[i] - b[i] for all slots */
    for (i=0; i < 8; i++) {
      r[i] = a[i] - b[i];
    }
    createeightdual(L, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  } else if (iseightdual(L, 1) && agn_isnumber(L, 2)) {
    createeightdual(L, agn_tonumber(L, 2), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    lua_replace(L, 2);
    return mt_sub(L);
  } else if (agn_isnumber(L, 1) && iseightdual(L, 2)) {
    createeightdual(L, agn_tonumber(L, 1), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    lua_replace(L, 1);
    return mt_sub(L);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected number or dual number.", "dual.__sub");
  }
  return 1;
}


/* Multiplication x*y */
static int mt_mul (lua_State *L) {
  if ((agn_isnumber(L, 1) && !tools_isfinite(agn_tonumber(L, 1))) ||
      (agn_isnumber(L, 2) && !tools_isfinite(agn_tonumber(L, 2)))) {
    /* 6.7.4 fix to prevent freezes */
    lua_settop(L, agn_isnumber(L, 1) ? 1 : 2);
    return 1;
  }
  if (agn_isnumber(L, 1) && isdual(L, 2)) {
    long double a, b1, b2;  /* change to long double 6.6.6 */
    checkdual(L, 2, "dual.__mul", 1);
    a = agn_tonumber(L, 1);
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    createdual(L, a*b1, a*b2);
  } else if (isdual(L, 1) && agn_isnumber(L, 2)) {  /* 2.14.12 extension */
    long double a1, a2, b;
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    b = agn_tonumber(L, 2);
    createdual(L, a1*b, a2*b);
  } else if (isdual(L, 1) && isdual(L, 2)) {
    long double a1, a2, b1, b2;
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    createdual(L, a1*b1, a1*b2 + a2*b1);
  } else if (ishyperdual(L, 1) && ishyperdual(L, 2)) {
    long double a1, a2, a3, a4, b1, b2, b3, b4;
    long double res1, res2, res3, res4;
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    b3 = agn_reggetinumber(L, 2, 3);
    b4 = agn_reggetinumber(L, 2, 4);
    res1 = a1*b1;
    res2 = fmal(a1, b2, a2*b1);
    res3 = fmal(a1, b3, a3*b1);
    res4 = fmal(a1, b4, fmal(a4, b1, fmal(a2, b3, a3*b2)));
    createhyperdual(L, res1, res2, res3, res4);
  } else if (ishyperdual(L, 1) && agn_isnumber(L, 2)) {  /* 6.6.10 extension */
    createhyperdual(L, agn_tonumber(L, 2), 0.0, 0.0, 0.0);
    lua_replace(L, 2);
    return mt_mul(L);
  } else if (agn_isnumber(L, 1) && ishyperdual(L, 2)) {
    createhyperdual(L, agn_tonumber(L, 1), 0.0, 0.0, 0.0);
    lua_replace(L, 1);
    return mt_mul(L);
  } else if (iseightdual(L, 1) && iseightdual(L, 1)) {
    int i;
    long double a[8], b[8], r[8];
    checkdual(L, 1, "dual.__mul", 1);
    checkdual(L, 2, "dual.__mul", 1);
    /* Pull registers into C arrays (0-based) */
    for (i=0; i < 8; i++) {
      a[i] = agn_reggetinumber(L, 1, i + 1);
      b[i] = agn_reggetinumber(L, 2, i + 1);
    }
    /* Slot 0: real*real */
    r[0] = a[0]*b[0];
    /* Slots 1, 2, 4: First Order (Standard Leibniz Rule) */
    #define MUL_1ST(i) fmal(a[0], b[i], a[i]*b[0])
    r[1] = MUL_1ST(1);
    r[2] = MUL_1ST(2);
    r[4] = MUL_1ST(4);
    /* Slots 3, 5, 6: Second Order (Mixed Leibniz Rule)
       Formula: a*b_mixed + a_mixed*b + (a_part1*b_part2 + a_part2*b_part1) */
    #define MUL_2ND(ia, ib, im) fmal(a[0], b[im], fmal(a[im], b[0], fmal(a[ia], b[ib], a[ib]*b[ia])))
    r[3] = MUL_2ND(1, 2, 3);  /* Interaction of E1, E2 */
    r[5] = MUL_2ND(1, 4, 5);  /* Interaction of E1, E3 */
    r[6] = MUL_2ND(2, 4, 6);  /* Interaction of E2, E3 */
    /* Slot 7: Third Order (The "Complete" Leibniz Rule)
       We sum:
       1. Base terms: a[0]*b[7] + a[7]*b[0]
       2. Cross interactions: a[1]*b[6] + a[2]*b[5] + a[4]*b[3]
       3. Mirror interactions: b[1]*a[6] + b[2]*a[5] + b[4]*a[3] */
    long double cross = (a[1]*b[6]) + (a[2]*b[5]) + (a[4]*b[3]) +
                        (b[1]*a[6]) + (b[2]*a[5]) + (b[4]*a[3]);
    r[7] = fmal(a[0], b[7], fmal(a[7], b[0], cross));
    createeightdual(L, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  } else if (iseightdual(L, 1) && agn_isnumber(L, 2)) {
    createeightdual(L, agn_tonumber(L, 2), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    lua_replace(L, 2);
    return mt_mul(L);
  } else if (agn_isnumber(L, 1) && iseightdual(L, 2)) {
    createeightdual(L, agn_tonumber(L, 1), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    lua_replace(L, 1);
    return mt_mul(L);
  }
   else
    luaL_error(L, "Error in " LUA_QS ": expected number or dual number.", "dual.__mul");
  return 1;
}


/* Division x/y. */
static int mt_div (lua_State *L) {
  if ((agn_isnumber(L, 1) && !tools_isfinite(agn_tonumber(L, 1))) ||
      (agn_isnumber(L, 2) && !tools_isfinite(agn_tonumber(L, 2)))) {
    /* 6.7.4 fix to prevent freezes */
    lua_settop(L, agn_isnumber(L, 1) ? 1 : 2);
    return 1;
  }
  if (agn_isnumber(L, 1) && isdual(L, 2)) {  /* 2.14.12 extension */
    long double a, b1, b2;  /* change to long double 6.6.6 */
    a = agn_tonumber(L, 1);
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    if (b1 == 0.0L)  /* 2.14.12 fix */
      lua_pushundefined(L);
    else {
      createdual(L, a/b1, (-a*b2)/(b1*b1));
    }
  } else if (isdual(L, 1) && agn_isnumber(L, 2)) {
    long double b, a1, a2;
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    b = agn_tonumber(L, 2);
    if (b == 0.0L)  /* 2.14.12 fix */
      lua_pushundefined(L);
    else {
      createdual(L, a1/b, a2/b);
    }
  } else if (isdual(L, 1) && isdual(L, 2)) {
    long double a1, a2, b1, b2;
    checkdual(L, 1, "dual.__div", 1);
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    if (b1 == 0.0L)  /* 2.14.12 fix */
      lua_pushundefined(L);
    else {
      createdual(L, a1/b1, fmal(a2, b1, -a1*b2)/(b1*b1));  /* 6.6.7 optimisation */
    }
  } else if (ishyperdual(L, 1) && ishyperdual(L, 2)) {
    long double a1, a2, a3, a4, b1, b2, b3, b4, res1, res2, res3, res4a, res4b;
    a1 = agn_reggetinumber(L, 1, 1);
    a2 = agn_reggetinumber(L, 1, 2);
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    b3 = agn_reggetinumber(L, 2, 3);
    b4 = agn_reggetinumber(L, 2, 4);
    if (b1 == 0.0L) {  /* 6.6.10 extension */
      lua_pushundefined(L);
    } else {  /* 6.6.10 fix */
      res1 = a1/b1;
      res2 = (a2 - res1*b2)/b1;
      res3 = (a3 - res1*b3)/b1;
      res4a = fmal(res2, b3, res3*b2);
      res4b = (a4 - fmal(res1, b4, res4a))/b1;
      createhyperdual(L, res1, res2, res3, res4b);
    }
  } else if (ishyperdual(L, 1) && agn_isnumber(L, 2)) {  /* 6.6.10 extension */
    createhyperdual(L, agn_tonumber(L, 2), 0.0, 0.0, 0.0);
    lua_replace(L, 2);
    return mt_div(L);
  } else if (agn_isnumber(L, 1) && ishyperdual(L, 2)) {
    createhyperdual(L, agn_tonumber(L, 1), 0.0, 0.0, 0.0);
    lua_replace(L, 1);
    return mt_div(L);
  } else if (iseightdual(L, 1) && iseightdual(L, 2)) {
    int i;
    long double a[8], b[8], r[8];
    checkdual(L, 1, "dual.__div", 1);
    checkdual(L, 2, "dual.__div", 1);
    /* Pull registers into C arrays (0-based) */
    for (i=0; i < 8; i++) {
      a[i] = agn_reggetinumber(L, 1, i + 1);
      b[i] = agn_reggetinumber(L, 2, i + 1);
    }
    if (b[0] == 0.0L) {
      lua_pushundefined(L);
      return 1;
    }
    /* Slot 0: Value */
    r[0] = a[0]/b[0];
    /* Slots 1, 2, 4: First Order (E1, E2, E3)
       Rule: r' = (a' - r*b')/b */
    #define DIV_1ST(i) (a[i] - r[0]*b[i])/b[0]
    r[1] = DIV_1ST(1);
    r[2] = DIV_1ST(2);
    r[4] = DIV_1ST(4);
    /* Slots 3, 5, 6: Second Order (E12, E13, E23)
       Rule: r_ab = (a_ab - (r*b_ab + r_a*b_b + r_b*b_a))/b */
    #define DIV_2ND(ia, ib, im) (a[im] - (r[0]*b[im] + r[ia]*b[ib] + r[ib]*b[ia]))/b[0]
    r[3] = DIV_2ND(1, 2, 3);
    r[5] = DIV_2ND(1, 4, 5);
    r[6] = DIV_2ND(2, 4, 6);
    /* Slot 7: Third Order (E123)
       Rule: r_abc = (a_abc - [r*b_abc + (r_a*b_bc + r_b*b_ac + r_c*b_ab)
                   + (b_a*r_bc + b_b*r_ac + b_c*r_ab)])/b */
    long double interaction_terms =
        (r[0]*b[7]) +
        (r[1]*b[6] + r[2]*b[5] + r[4]*b[3]) +
        (b[1]*r[6] + b[2]*r[5] + b[4]*r[3]);
    r[7] = (a[7] - interaction_terms)/b[0];
    createeightdual(L, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  } else if (iseightdual(L, 1) && agn_isnumber(L, 2)) {
    createeightdual(L, agn_tonumber(L, 2), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    lua_replace(L, 2);
    return mt_div(L);
  } else if (agn_isnumber(L, 1) && iseightdual(L, 2)) {
    createeightdual(L, agn_tonumber(L, 1), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    lua_replace(L, 1);
    return mt_div(L);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected number or dual number.", "dual.__div");
  }
  return 1;
}


/* The inverse 1/x.
   See: https://www.osti.gov/servlets/purl/1368722, sheet #23 &
   http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/other/dualNumbers/functions/index.htm */
static int mt_recip (lua_State *L) {
  long double a[8] = {0.0L}, r[8] = {0.0L}, real, df, ddf, dddf;
  int i, s = checkdual(L, 1, "dual.__recip", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  if (a[0] == 0.0L || !tools_isfinite(a[0])) {  /* 6.7.4 fix against freezes */
    /* 6.7.4 fix to prevent freezes */
    lua_pushnumber(L, (a[0] == 0.0L) ? AGN_NAN : a[0]);
    return 1;
  }
  /* Load only available slots */
  for (i = 1; i < s; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
  real = 1.0L/a[0];
  df   = -1.0L/(a[0]*a[0]);     /* -1/x^2 */
  ddf  = 2.0L/(a[0]*a[0]*a[0]);  /* 2/x^3 */
  dddf = -6.0L/(powl(a[0], 4.0L));   /* -6/x^4 */
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    /* Additive term df*a[3] is required for composition safety! */
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    /* Tri-Dual 8-slot Case */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* Composition-Safe Mixed Slots */
    r[3] = fmal(df, a[3], ddf*a[1]*a[2]);
    r[5] = fmal(df, a[5], ddf*a[1]*a[4]);
    r[6] = fmal(df, a[6], ddf*a[2]*a[4]);
    /* Full Fa� di Bruno Chain Rule for r7 */
    long double mixed_2nd = a[1]*a[6] + a[2]*a[5] + a[3]*a[4];
    r[7] = fmal(df, a[7], fmal(ddf, mixed_2nd, dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* integer power: x ** k, with k an integer (treated as a long double)
   On ARM the function returns wrong results with the origin as input. */
static int mt_ipow (lua_State *L) {
  long double a1, a2, a3, a4, a5, a6, a7, a8, k, val, p_km1, df;
  int s = checkdual(L, 1, "dual.__ipow", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  if ((agn_isnumber(L, 2) && !tools_isfinite(agn_tonumber(L, 2)))) {
    /* 6.7.4 fix to prevent freezes */
    lua_settop(L, 2);
    return 1;
  }
  a1 = agn_reggetinumber(L, 1, 1);
  a2 = agn_reggetinumber(L, 1, 2);
  k = agn_checknumber(L, 2);
  if (a1 == 0.0L && k == 0.0L) {  /* 6.7.4 fix */
    lua_pushundefined(L);
    return 1;
  }
  /* Optimization: Pre-calculate the powers needed */
  val   = tools_powil(a1, k);
  p_km1 = tools_powil(a1, k - 1.0L);
  df    = k*p_km1;
  if (s == 2) {
    createdual(L, val, df*a2);
  } else if (s == 4) {
    /* 6.7.7 change, with the origin do not revert to third-order duals any longer */
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    long double ddf = k*(k - 1.0L)*tools_powil(a1, k - 2.0L);
    createhyperdual(L, val, df*a2, df*a3, fmal(df, a4, ddf*a2*a3));
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    long double ddf  = k*(k - 1.0L)*tools_powil(a1, k - 2.0L);
    long double dddf = k*(k - 1.0L)*(k - 2.0L)*tools_powil(a1, k - 3.0L);
    /* Slot 8 = (f'*a8) + f''*(a2*a7 + a3*a6 + a4*a5) + f'''*(a2*a3*a5) */
    long double term8 = fmal(df, a8, fmal(ddf, (fmal(a2, a7, fmal(a3, a6, a4*a5))), dddf*(a2*a3*a5)));
    createeightdual(L, val, df*a2, df*a3, fmal(df, a4, ddf*a2*a3),
      df*a5, fmal(df, a6, ddf*a2*a5), fmal(df, a7, ddf*a3*a5), term8);
  }
  return 1;
}


/* On ARM the function returns wrong results with the origin as input. */
static int mt_pow (lua_State *L) {
  long double a1, a2, sign;  /* change to long double 6.6.6 */
  int s = checkdual(L, 1, "dual.__pow", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  if (agn_isnumber(L, 2) && !tools_isfinite(agn_tonumber(L, 2))) {
    /* 6.7.4 fix to prevent freezes */
    lua_settop(L, 2);
    return 1;
  }
  a1 = agn_reggetinumber(L, 1, 1);
  a2 = agn_reggetinumber(L, 1, 2);
  if (s == 2 && agn_isnumber(L, 2)) {
    long double k = agn_tonumber(L, 2);
    if ((a1 == 0.0L && k == 0.0L) || (a1 < 0.0L && k < 1.0L)) {  /* 6.7.4 fix */
      lua_pushundefined(L);
      return 1;
    }
    if (a1 < 0.0L) {
      sign = (tools_isodd(k)) ? -1.0L : 1.0L;
      a1 = -a1;
      a2 = -a2;
    }
    createdual(L, tools_powl(a1, k), k*a2*tools_powl(a1, k - 1));
  } else if (s == 2 && isdual(L, 2)) {
    /* http://new.math.uiuc.edu/math198/MA198-2014/rgandre2/seminar.pdf, sheet #51 */
    long double b1, b2;
    b1 = agn_reggetinumber(L, 2, 1);
    b2 = agn_reggetinumber(L, 2, 2);
    if (a1 == 0.0L) { /* 6.6.6 extension, 6.7.4 fix */
      if (b1 == 0.0L) {
        lua_pushundefined(L);
      } else {
        createdual(L, 1.0, 0.0);
      }
      return 1;
    }
    if (a1 < 0.0L && b1 < 1.0L) {
      lua_pushundefined(L);
      return 1;
    }
    long double f = tools_powl(a1, b1);
    createdual(L, f, f*(b2*tools_logl(a1) + a2*b1/a1));
  } else if (s == 4 && ishyperdual(L, 2)) {
    int i, is_const_exponent;
    long double a[8], b[8], res1, res2, res3, res4, ln_u, df_u, df_v, ddf_uu, ddf_vv, ddf_uv;
    a[0] = a1; a[1] = a2;
    for (i=2; i < 4; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    for (i=0; i < 4; i++) b[i] = agn_reggetinumber(L, 2, i + 1);
    /* 6.6.10 extension, 6.7.4/6.7.6 fix: do NOT push zero-const if a[0} = 0 and b[0] <> 0 */
    if ((a[0] == 0.0L && b[0] == 0.0L) || (a[0] < 0.0L && b[0] < 1.0L)) {
      lua_pushundefined(L);
      return 1;
    }
    is_const_exponent = 1;
    for (i=1; i < 4 && is_const_exponent; i++) {
      if (b[i] != 0.0L) is_const_exponent = 0;
    }
    if (is_const_exponent && tools_isint(b[0])) {
      /* 6.7.4, mt_pow cannot correctly compute the third derivative with integral powers, so: */
      lua_pushinteger(L, b[0]);
      lua_replace(L, 2);
      return mt_ipow(L);
    }
    sign = 1.0L;
    if (a[0] < 0.0L) {
      sign = (tools_isodd(b[0])) ? -1.0L : 1.0L;
      a[0] = -a[0];
      a[1] = -a[1];
      a[2] = -a[2];
    }
    /* 1. Calculate the real value */
    res1 = tools_powl(a[0], b[0]);
    /* 2. Pre-calculate terms for efficiency */
    ln_u = tools_logl(a[0]);
    df_u = b[0]*tools_powl(a[0], b[0] - 1.0L);  /* Partial derivative w.r.t u */
    df_v = res1*ln_u;  /* Partial derivative w.r.t v */
    /* 3. Slot 2 & 3 (First Derivatives) */
    res2 = fmal(df_u, a[1], df_v*b[1]);
    res3 = fmal(df_u, a[2], df_v*b[2]);
    /* 4. Slot 4 (The Second Derivative/Mixed Partial)
       This handles u'', v'', and the interaction between u' and v' */
    ddf_uu = b[0]*(b[0] - 1.0L)*tools_powl(a[0], b[0] - 2.0L);
    ddf_vv = res1*ln_u*ln_u;
    ddf_uv = fmal(df_u, ln_u, res1/a[0]); /* The "interaction" term */
    res4 = fmal(df_u, a[3], df_v*b[3])      /* Acceleration terms */
         + (ddf_uu*a[1]*a[2])               /* Base curvature */
         + (ddf_vv*b[1]*b[2])               /* Exponent curvature */
         + (ddf_uv*(fmal(a[1], b[2], a[2]*b[1])));  /* Cross-variable interaction */
    createhyperdual(L, sign*res1, sign*res2, res3, sign*res4);
  } else if (s == 4 && agn_isnumber(L, 2)) {
    createhyperdual(L, agn_tonumber(L, 2), 0.0, 0.0, 0.0);
    lua_replace(L, 2);
    return mt_pow(L);
  } else if (s == 8 && iseightdual(L, 2)) {
    int i, is_const_exponent;
    long double a[8], b[8], r[8];
    long double f, lna, p_ym1, p_ym2, p_ym3;
    long double fx, fy, fxx, fyy, fxy, fxxx, fyyy, fxxy, fxyy;
    /* Load Registers (0-based) */
    a[0] = a1; a[1] = a2;
    for (i=2; i < 8; i++) {
      a[i] = agn_reggetinumber(L, 1, i + 1);
    }
    for (i=0; i < 8; i++) {
      b[i] = agn_reggetinumber(L, 2, i + 1);
    }
    if ((a[0] == 0.0L && b[0] == 0.0L) || (a[0] < 0.0L && b[0] < 1.0L)) {  /* 6.7.4 fix */
      lua_pushundefined(L);
      return 1;
    }
    sign = 1.0L;
    if (a[0] < 0.0L) {
      sign = (tools_isodd(b[0])) ? -1.0L : 1.0L;
      a[0] = -a[0];
      a[1] = -a[1];
      a[2] = -a[2];
      a[4] = -a[4];
    }
    is_const_exponent = 1;
    for (i=1; i < 8 && is_const_exponent; i++) {
      if (b[i] != 0.0L) is_const_exponent = 0;
    }
    if (is_const_exponent) {
      /* 6.7.4, mt_pow cannot correctly compute the third derivative with integral powers, so: */
      if (tools_isint(b[0])) {
        lua_pushinteger(L, b[0]);
        lua_replace(L, 2);
        return mt_ipow(L);
      }
      /* Simplified Power Rule: d/dx [u^k] = k*u^(k-1)*u' */
      long double k = b[0];
      f = tools_powl(a[0], k);
      fx = k*tools_powl(a[0], k - 1.0L);
      fxx = k*(k - 1.0L)*tools_powl(a[0], k - 2.0L);
      fxxx = k*(k - 1.0L)*(k - 2.0L)*tools_powl(a[0], k - 3.0L);
      r[1] = fx*a[1];
      r[2] = fx*a[2];
      r[4] = fx*a[4];
      r[3] = fmal(fx, a[3], fxx*a[1]*a[2]);
      r[5] = fmal(fx, a[5], fxx*a[1]*a[4]);
      r[6] = fmal(fx, a[6], fxx*a[2]*a[4]);
      long double mixed_2nd = fmal(a[1], a[6], fmal(a[2], a[5], a[3]*a[4]));
      r[7] = fmal(fx, a[7], fmal(fxx, mixed_2nd, fxxx*a[1]*a[2]*a[4]));
      createeightdual(L, sign*f, sign*r[1], r[2], sign*r[3], r[4], r[5], r[6], sign*r[7]);
      return 1;
    }
    /* Core Calculation */
    f = tools_powl(a[0], b[0]);
    lna = tools_logl(a[0]);
    p_ym1 = tools_powl(a[0], b[0] - 1.0L);
    p_ym2 = p_ym1/a[0];
    p_ym3 = p_ym2/a[0];
    /* Partial Derivatives */
    fx = b[0]*p_ym1;
    fy = f*lna;
    fxx = b[0]*(b[0] - 1.0L)*p_ym2;
    fyy = f*lna*lna;
    fxy = p_ym1*(fmal(b[0], lna, 1.0L));
    fxxx = b[0]*(b[0] - 1.0L)*(b[0] - 2.0L)*p_ym3;
    fyyy = f*lna*lna*lna;
    fxxy = p_ym2*(2.0L*b[0] + fmal(b[0], (b[0] - 1.0L)*lna, -1.0L));
    fxyy = p_ym1*lna*(fmal(b[0], lna, 2.0L));
    /* 1st order slots (Indices 1, 2, 4) */
    #define C1(i) fmal(fx, a[i], fy*b[i])
    r[0] = 0; r[1] = C1(1); r[2] = C1(2); r[4] = C1(4);
    /* 2nd order slots (Indices 3, 5, 6)
       Formula: f_x*a_mixed + f_y*b_mixed + curvature_terms */
    #define C2(ia, ib, im) \
        fmal(fx, a[im], fy*b[im]) + \
        fmal(fxx, a[ia]*a[ib], fyy*b[ia]*b[ib]) + \
        fxy*fmal(a[ia], b[ib], a[ib]*b[ia])
    r[3] = C2(1, 2, 3);  /* Interaction of E1 and E2 -> Slot E12 */
    r[5] = C2(1, 4, 5);  /* Interaction of E1 and E3 -> Slot E13 */
    r[6] = C2(2, 4, 6);  /* Interaction of E2 and E3 -> Slot E23 */
    /* 3rd order slot (Index 7: E123)
       t1: Linear change */
    long double t1 = fmal(fx, a[7], fy*b[7]);
    /* t2: Curvature interactions (2nd derivatives) */
    long double t2 = fxx*(fmal(a[1], a[6], fmal(a[2], a[5], a[3]*a[4]))) +
                     fyy*(fmal(b[1], b[6], fmal(b[2], b[5], b[3]*b[4]))) +
                     fxy*(fmal(a[1], b[6], fmal(a[2], b[5], a[3]*b[4])) + fmal(b[1], a[6], fmal(b[2], a[5], b[3]*a[4])));
    /* t3: Torsion/Twist interactions (3rd derivatives) */
    long double t3 = fxxx*(a[1]*a[2]*a[4]) +
                     fyyy*(b[1]*b[2]*b[4]) +
                     fxxy*(fmal(a[1], a[2]*b[4], fmal(a[1], b[2]*a[4], b[1]*a[2]*a[4]))) +
                     fxyy*(fmal(b[1], b[2]*a[4], fmal(b[1], a[2]*b[4], a[1]*b[2]*b[4])));
    r[7] = t1 + t2 + t3;
    createeightdual(L, sign*f, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  } else if (s == 8 && agn_isnumber(L, 2)) {
    createeightdual(L, agn_tonumber(L, 2), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    lua_replace(L, 2);
    return mt_pow(L);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected number or dual number.", "dual.__pow");
  }
  return 1;
}


static int mt_square (lua_State *L) {
  long double a1, a2, a3, a4, a5, a6, a7, a8, val, f_p;
  int s = checkdual(L, 1, "dual.__square", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a1 = agn_reggetinumber(L, 1, 1);
  a2 = agn_reggetinumber(L, 1, 2);
  val = a1*a1;    /* f(x) = x^2 */
  f_p = 2.0L*a1;  /* f'(x) = 2x */
  if (s == 2) {
    createdual(L, val, f_p*a2);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    /* f'' = 2.0L */
    createhyperdual(L, val, f_p*a2, f_p*a3, fmal(f_p, a4, 2.0L*a2*a3));
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    /* f' = 2x, f'' = 2, f''' = 0
       Slot 8 = (f'*a8) + f''*(a2*a7 + a3*a6 + a4*a5) + 0 */
    long double term8 = fmal(f_p, a8, 2.0L*(a2*a7 + a3*a6 + a4*a5));
    createeightdual(L, val, f_p*a2, f_p*a3, fmal(f_p, a4, 2.0L*a2*a3),
      f_p*a5, fmal(f_p, a6, 2.0L*a2*a5), fmal(f_p, a7, 2.0L*a3*a5), term8);
  }
  return 1;
}


static int mt_cube (lua_State *L) {
  long double a1, a2, a3, a4, a5, a6, a7, a8, ad, val;
  int s = checkdual(L, 1, "dual.__cube", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a1 = agn_reggetinumber(L, 1, 1);
  a2 = agn_reggetinumber(L, 1, 2);
  ad = a1*a1;   /* x^2 */
  val = ad*a1;  /* x^3 */
  if (s == 2) {
    createdual(L, val, 3.0L*ad*a2);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    /* f' = 3x^2, f'' = 6x */
    createhyperdual(L, val, 3.0L*ad*a2, 3.0L*ad*a3,
      fmal(3.0L*ad, a4, 6.0L*a1*a2*a3));
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    long double f_p  = 3.0L*ad;  /* 3x^2 */
    long double f_pp = 6.0L*a1;  /* 6x */
    long double f_ppp = 6.0L;    /* 6 */
    /* Slot 8 = (f'*a8) + f''*(a2*a7 + a3*a6 + a4*a5) + f'''*(a2*a3*a5) */
    long double term8 = fmal(f_p, a8, fmal(f_pp, (a2*a7 + a3*a6 + a4*a5), f_ppp*(a2*a3*a5)));
    createeightdual(L, val, f_p*a2, f_p*a3, fmal(f_p, a4, f_pp*a2*a3),
      f_p*a5, fmal(f_p, a6, f_pp*a2*a5), fmal(f_p, a7, f_pp*a3*a5), term8);
  }
  return 1;
}


static int mt_sqrt (lua_State *L) {
  long double a1, a2, a3, a4, a5, a6, a7, a8, real, df, ddf, dddf;
  int s = checkdual(L, 1, "dual.__sqrt", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a1 = agn_reggetinumber(L, 1, 1);
  if (a1 < 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  real = sqrtl(a1);
  if (real == 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  a2 = agn_reggetinumber(L, 1, 2);
  df = 0.5L/real;  /* f' = 0.5*x^(-0.5) */
  if (s == 2) {
    createdual(L, real, a2*df);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    ddf = -0.5L*df/a1;  /* f'' = -0.25*x^(-1.5) */
    createhyperdual(L, real, a2*df, a3*df, fmal(df, a4, ddf*a2*a3));
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    ddf = -0.5L*df/a1;    /* Second derivative */
    dddf = -1.5L*ddf/a1;  /* Third derivative: 0.375*x^(-2.5) */
    /* Slot 8 = (f'*a8) + f''*(a2*a7 + a3*a6 + a4*a5) + f'''*(a2*a3*a5) */
    long double term8 = fmal(df, a8, ddf*(a2*a7 + a3*a6 + a4*a5) + dddf*(a2*a3*a5));
    createeightdual(L, real, a2*df, a3*df, fmal(df, a4, ddf*a2*a3),
      a5*df, fmal(df, a6, ddf*a2*a5), fmal(df, a7, ddf*a3*a5), term8);
  }
  return 1;
}


/* Created by Gemini AI; Inverse Square Root for Hyper-dual numbers */
static int mt_invsqrt (lua_State *L) {
  long double a1, a2, a3, a4, a5, a6, a7, a8, f, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.__invsqrt", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a1 = agn_reggetinumber(L, 1, 1);
  if (a1 <= 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  f = 1.0L/sqrtl(a1);
  a2 = agn_reggetinumber(L, 1, 2);
  df = -0.5L*f/a1;
  if (s == 2) {
    createdual(L, f, df*a2);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    ddf = -1.5L*df/a1;
    createhyperdual(L, f, df*a2, df*a3, fmal(df, a4, ddf*a2*a3));
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    ddf = -1.5L*df/a1;
    dddf = -2.5L*ddf/a1;
    /* Slot 8 = (f'*a8) + f''*(a2*a7 + a3*a6 + a4*a5) + f'''*(a2*a3*a5) */
    long double term8 = fmal(df, a8, ddf*(a2*a7 + a3*a6 + a4*a5) + dddf*(a2*a3*a5));
    createeightdual(L, f, df*a2, df*a3, fmal(df, a4, ddf*a2*a3),
      df*a5, fmal(df, a6, ddf*a2*a5), fmal(df, a7, ddf*a3*a5), term8
    );
  }
  return 1;
}


/* Created by Gemini; 2026-02-09, 6.7.5 */
static int dual_pow32 (lua_State *L) {
  long double a1, a2, a3, a4, a5, a6, a7, a8, real, df, ddf, dddf, root;
  int toconstant = agnL_optboolean(L, 2, 0);
  int i, s = checkdual(L, 1, "dual.pow32", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }
  a1 = agn_reggetinumber(L, 1, 1);
  if (a1 < 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  /* Handle x=0 case: f(0)=0, f'(0)=0, but curvature is undefined */
  if (a1 == 0.0L) {
    for (i=2; i <= s; i++) {
      if (agn_reggetinumber(L, 1, i) != 0.0L) { lua_pushundefined(L); return 1; }
    }
    pushzero(L, s);
    return 1;
  }
  root = sqrtl(a1);
  real = a1*root;  /* x*sqrt(x) = x^(1.5) */
  a2 = agn_reggetinumber(L, 1, 2);
  df = 1.5L*root;  /* f' = 1.5*x^(0.5) */
  if (s == 2) {
    createdual(L, real, a2*df);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    ddf = 0.75L/root;  /* f'' = 0.75*x^(-0.5) */
    createhyperdual(L, real, a2*df, a3*df, fmal(df, a4, ddf*a2*a3));
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    ddf = 0.75L/root;             /* Second derivative */
    dddf = -0.5L*ddf/a1;        /* Third derivative: -0.375*x^(-1.5) */
    /* Slot 8 assembly using your specified logic flow */
    long double mixed_inner = fmal(a2, a7, fmal(a3, a6, a4*a5));
    long double cubic_inner = a2*a3*a5;
    long double term8 = fmal(df, a8, fmal(ddf, mixed_inner, dddf*cubic_inner));
    createeightdual(L, real, a2*df, a3*df, fmal(df, a4, ddf*a2*a3),
      a5*df, fmal(df, a6, ddf*a2*a5), fmal(df, a7, ddf*a3*a5), term8);
  }
  return 1;
}


/* Created by Gemini; 2026-02-09, 6.7.5 */
static int dual_pow52 (lua_State *L) {
  long double a1, a2, a3, a4, a5, a6, a7, a8, real, df, ddf, dddf, root;
  int toconstant = agnL_optboolean(L, 2, 0);
  int i, s = checkdual(L, 1, "dual.pow52", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }
  a1 = agn_reggetinumber(L, 1, 1);
  if (a1 < 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  /* Handle x=0 case: f(0), f'(0), and f''(0) are all 0.
     f'''(0) is undefined (infinite). */
  if (a1 == 0.0L) {
    for (i=2; i <= s; i++) {
      if (agn_reggetinumber(L, 1, i) != 0.0L) { lua_pushundefined(L); return 1; }
    }
    pushzero(L, s);
    return 1;
  }
  root = sqrtl(a1);
  real = a1*a1*root; /* x^2*sqrt(x) = x^(2.5) */
  a2 = agn_reggetinumber(L, 1, 2);
  df = 2.5L*a1*root;  /* f' = 2.5*x^(1.5) */
  if (s == 2) {
    createdual(L, real, a2*df);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    ddf = 3.75L*root;  /* f'' = 3.75*x^(0.5) */
    createhyperdual(L, real, a2*df, a3*df, fmal(df, a4, ddf*a2*a3));
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    ddf = 3.75L*root;    /* Second derivative: 3.75*x^0.5 */
    dddf = 1.875L/root;  /* Third derivative: 1.875*x^-0.5 */
    long double mixed_inner = fmal(a2, a7, fmal(a3, a6, a4*a5));
    long double cubic_inner = a2*a3*a5;
    long double term8 = fmal(df, a8, fmal(ddf, mixed_inner, dddf*cubic_inner));
    createeightdual(L, real, a2*df, a3*df, fmal(df, a4, ddf*a2*a3),
      a5*df, fmal(df, a6, ddf*a2*a5), fmal(df, a7, ddf*a3*a5), term8);
  }
  return 1;
}


/* Created by Gemini; 2026-02-11, 6.7.7 */
static int dual_pow72 (lua_State *L) {
  long double a1, a2, a3, a4, a5, a6, a7, a8, a1_2, real, df, ddf, dddf, root;
  int toconstant = agnL_optboolean(L, 2, 0);
  int i, s = checkdual(L, 1, "dual.pow72", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }
  a1 = agn_reggetinumber(L, 1, 1);
  if (a1 < 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  /* Handle x=0 case: f(0), f'(0), f''(0), and f'''(0) are all 0.
     The function is extremely smooth here. */
  if (a1 == 0.0L) {
    for (i=2; i <= s; i++) {
      if (agn_reggetinumber(L, 1, i) != 0.0L) { lua_pushundefined(L); return 1; }
    }
    pushzero(L, s);
    return 1;
  }
  /* protect against overflow */
  root = sqrtl(a1);
  a1_2 = a1*a1;
  real = (a1*root)*a1_2;  /* x^3*sqrt(x) = x^(3.5) */
  a2 = agn_reggetinumber(L, 1, 2);
  df = 3.5L*a1_2*root;  /* f' = 3.5*x^(2.5) */
  if (s == 2) {
    createdual(L, real, a2*df);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    ddf = 8.75L*a1*root;  /* f'' = 8.75*x^(1.5) */
    createhyperdual(L, real, a2*df, a3*df, fmal(df, a4, ddf*a2*a3));
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    ddf = 8.75L*a1*root;  /* Second derivative: 8.75*x^1.5 */
    dddf = 13.125L*root;  /* Third derivative: 13.125*x^0.5 */
    long double mixed_inner = fmal(a2, a7, fmal(a3, a6, a4*a5));
    long double cubic_inner = a2*a3*a5;
    long double term8 = fmal(df, a8, fmal(ddf, mixed_inner, dddf*cubic_inner));
    createeightdual(L, real, a2*df, a3*df, fmal(df, a4, ddf*a2*a3),
      a5*df, fmal(df, a6, ddf*a2*a5), fmal(df, a7, ddf*a3*a5), term8);
  }
  return 1;
}


/* Created by Gemini; 2026-02-11, 6.7.7 */
static int dual_powe (lua_State *L) {
  /* e  approx 2.71828182845904523536L */
  /* e-1 approx 1.71828182845904523536L */
  /* e-2 approx 0.71828182845904523536L */
  /* e-3 approx -0.28171817154095476464L */
  long double a1, a2, a3, a4, a5, a6, a7, a8, real, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int i, s = checkdual(L, 1, "dual.powe", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }
  a1 = agn_reggetinumber(L, 1, 1);
  if (a1 < 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  /* Handle x=0 case:
     f(0)=0, f'(0)=0, f''(0)=0.
     f'''(0) involves x^(e-3), which is 1/x^0.28 -> Infinity. */
  if (a1 == 0.0L) {
    for (i = 2; i <= s; i++) {
      if (agn_reggetinumber(L, 1, i) != 0.0L) { lua_pushundefined(L); return 1; }
    }
    pushzero(L, s);
    return 1;
  }
  real = tools_powl(a1, M_Eld);
  a2 = agn_reggetinumber(L, 1, 2);
  /* f' = e*x^(e-1) */
  df = M_Eld*tools_powl(a1, M_Eld - 1.0L);
  if (s == 2) {
    createdual(L, real, a2*df);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    /* f'' = e*(e-1)*x^(e-2) */
    ddf = M_Eld*(M_Eld - 1.0L)*tools_powl(a1, M_Eld - 2.0L);
    createhyperdual(L, real, a2*df, a3*df, fmal(df, a4, ddf*a2*a3));
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    ddf = M_Eld*(M_Eld - 1.0L)*tools_powl(a1, M_Eld - 2.0L);
    /* f''' = e*(e-1)*(e-2)*x^(e-3) -> Optimized by reusing ddf */
    dddf = ddf*(M_Eld - 2.0L) / a1;
    /* Slot 8 assembly */
    long double mixed_inner = fmal(a2, a7, fmal(a3, a6, a4*a5));
    long double cubic_inner = a2*a3*a5;
    long double term8 = fmal(df, a8, fmal(ddf, mixed_inner, dddf*cubic_inner));
    createeightdual(L, real, a2*df, a3*df, fmal(df, a4, ddf*a2*a3),
      a5*df, fmal(df, a6, ddf*a2*a5), fmal(df, a7, ddf*a3*a5), term8);
  }
  return 1;
}


/* 1 + 1/x, created by Gemini AI, put to the public domain, 1 + 1/x; 6.7.5, 2.5 times faster than genuine 1 + 1/x; UNDOC */
int dual_onepinv (lua_State *L) {
  long double x1, x2, x3, x4, invx, invx2, invx3, invx4, res1, res2, res3, res4;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.onepinv", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }
  x1 = agn_reggetinumber(L, 1, 1);
  x2 = agn_reggetinumber(L, 1, 2);
  if (x1 == 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  invx = 1.0L/x1;
  res1 = 1.0L + invx; /* The real part: 1 + 1/x */
  /* Derivatives of 1/x:
     f'  = -1/x^2
     f'' = 2/x^3
     f'''= -6/x^4  */
  invx2 = -(invx*invx);
  if (s == 2) {
    createdual(L, res1, invx2*x2);
  } else if (s == 4) {
    x3 = agn_reggetinumber(L, 1, 3);
    x4 = agn_reggetinumber(L, 1, 4);
    invx3 = -2.0L*(invx2*invx); /* 2/x^3 */
    res2 = invx2*x2;
    res3 = invx2*x3;
    /* Slot 4 Interaction: f'*x4 + f''*x2*x3 */
    res4 = fmal(invx2, x4, invx3*x2*x3);
    createhyperdual(L, res1, res2, res3, res4);
  } else if (s == 8) {
    long double x5, x6, x7, x8, r2, r3, r4, r5, r6, r7, r8, t1, t2, t3;
    x3 = agn_reggetinumber(L, 1, 3); x4 = agn_reggetinumber(L, 1, 4);
    x5 = agn_reggetinumber(L, 1, 5); x6 = agn_reggetinumber(L, 1, 6);
    x7 = agn_reggetinumber(L, 1, 7); x8 = agn_reggetinumber(L, 1, 8);
    invx3 = -2.0L*(invx2*invx);    /* 2/x^3 */
    invx4 = -3.0L*(invx3*invx);    /* -6/x^4 */
    /* 1st order slots */
    r2 = invx2*x2;
    r3 = invx2*x3;
    r5 = invx2*x5;
    /* 2nd order slots */
    #define CALC_INV_HESS(xa, xb, xm) fmal(invx2, xm, invx3*xa*xb)
    r4 = CALC_INV_HESS(x2, x3, x4);
    r6 = CALC_INV_HESS(x2, x5, x6);
    r7 = CALC_INV_HESS(x3, x5, x7);
    /* 3rd order slot */
    t1 = invx2*x8;
    t2 = invx3*(fmal(x2, x7, fmal(x3, x6, x4*x5)));
    t3 = invx4*(x2*x3*x5);
    r8 = t1 + t2 + t3;
    createeightdual(L, res1, r2, r3, r4, r5, r6, r7, r8);
  } else {
    luaL_error(L, "Error: expected dual number type.", "dual.onepinv");
  }
  return 1;
}


/* Created by Gemini AI, put to the public domain, sqrt(x^2 + y^2); 6.6.7 & 6.6.8 */
int dual_hypot (lua_State *L) {
  long double x1, x2, x3, x4, y1, y2, y3, y4, h, h3, fx, fy, fxx, fyy, fxy, res2, res3, res4;
  int toconstant = agnL_optboolean(L, 3, 0);
  int sx = checkdual(L, 1, "dual.hypot", toconstant);
  int sy = checkdual(L, 2, "dual.hypot", toconstant);
  if (sx == 1 || sy == 1) { lua_settop(L, sx == 1 ? 1 : 2); return 1; }  /* 6.7.4 fix */
  x1 = agn_reggetinumber(L, 1, 1);
  x2 = agn_reggetinumber(L, 1, 2);
  y1 = agn_reggetinumber(L, 2, 1);
  y2 = agn_reggetinumber(L, 2, 2);
  h = tools_hypotl(x1, y1);
  if (h == 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  /* 1st order partials */
  fx = x1/h;
  fy = y1/h;
  if (sx == 2 && sy == 2) {
    createdual(L, h, fmal(fx, x2, fy*y2));
  } else if (sx == 4 && sy == 4) {
    x3 = agn_reggetinumber(L, 1, 3);
    x4 = agn_reggetinumber(L, 1, 4);
    y3 = agn_reggetinumber(L, 2, 3);
    y4 = agn_reggetinumber(L, 2, 4);
    /* 2nd order partials */
    h3 = h*h*h;
    fxx = (y1*y1)/h3;
    fyy = (x1*x1)/h3;
    fxy = -(x1*y1)/h3;
    res2 = fmal(fx, x2, fy*y2);
    res3 = fmal(fx, x3, fy*y3);
    /* Slot 4: Combined Partial Derivative Template
       (fx*x4 + fy*y4) + (fxx*x2*x3 + fyy*y2*y3 + fxy*(x2*y3 + y2*x3)) */
    res4 = fmal(fx, x4, fy*y4) +
           fmal(fxx, x2*x3, fmal(fyy, y2*y3, fxy*(x2*y3 + y2*x3)));
    createhyperdual(L, h, res2, res3, res4);
  } else if (sx == 8 && sy == 8) {
    long double x3, x4, x5, x6, x7, x8, y3, y4, y5, y6, y7, y8;
    long double h, h3, h5, fx, fy, fxx, fyy, fxy, fxxx, fyyy, fxxy, fxyy;
    long double r2, r3, r4, r5, r6, r7, r8, t1, t2, t3;
    /* Loading 8-slot registers for X and Y */
    x3 = agn_reggetinumber(L, 1, 3); x4 = agn_reggetinumber(L, 1, 4);
    x5 = agn_reggetinumber(L, 1, 5); x6 = agn_reggetinumber(L, 1, 6);
    x7 = agn_reggetinumber(L, 1, 7); x8 = agn_reggetinumber(L, 1, 8);
    y3 = agn_reggetinumber(L, 2, 3); y4 = agn_reggetinumber(L, 2, 4);
    y5 = agn_reggetinumber(L, 2, 5); y6 = agn_reggetinumber(L, 2, 6);
    y7 = agn_reggetinumber(L, 2, 7); y8 = agn_reggetinumber(L, 2, 8);
    h = tools_hypotl(x1, y1);
    if (h == 0.0L) { lua_pushundefined(L); return 1; }
    h3 = h*h*h;
    h5 = h3*h*h;
    /* First and Second partials */
    fx = x1/h;
    fy = y1/h;
    fxx = (y1*y1)/h3;
    fyy = (x1*x1)/h3;
    fxy = -(x1*y1)/h3;
    /* Third partials */
    fxxx = (-3.0L*x1*y1*y1)/h5;
    fyyy = (-3.0L*y1*x1*x1)/h5;
    fxxy = (y1*(2.0L*x1*x1 - y1*y1))/h5;
    fxyy = (x1*(2.0L*y1*y1 - x1*x1))/h5;
    /* Slot 2, 3, 5 (First order) */
    r2 = fmal(fx, x2, fy*y2);
    r3 = fmal(fx, x3, fy*y3);
    r5 = fmal(fx, x5, fy*y5);
    /* Slot 4, 6, 7 (Second order) */
    #define CALC_HESSIAN(xa, xb, ya, yb, xm, ym) \
        fmal(fx, xm, fy*ym) + fmal(fxx, xa*xb, fmal(fyy, ya*yb, fxy*(xa*yb + ya*xb)))
    r4 = CALC_HESSIAN(x2, x3, y2, y3, x4, y4);
    r6 = CALC_HESSIAN(x2, x5, y2, y5, x6, y6);
    r7 = CALC_HESSIAN(x3, x5, y3, y5, x7, y7);
    /* Slot 8 (Third order - The "Full House")
       Partials 1: f_x*x8 + f_y*y8 */
    t1 = fmal(fx, x8, fy*y8);
    /* Partials 2: f_xx*(x2*x7 + x3*x6 + x4*x5) + f_yy*(y2*y7 + y3*y6 + y4*y5) + f_xy*(interactions) */
    t2 = fxx*(x2*x7 + x3*x6 + x4*x5) + fyy*(y2*y7 + y3*y6 + y4*y5) +
         fxy*(x2*y7 + x3*y6 + x4*y5 + y2*x7 + y3*x6 + y4*x5);
    /* Partials 3: f_xxx*x2*x3*x5 + f_yyy*y2*y3*y5 + f_xxy*(x2*x3*y5 + x2*y3*x5 + y2*x3*x5) + f_xyy*(...) */
    t3 = fxxx*(x2*x3*x5) + fyyy*(y2*y3*y5) +
         fxxy*(x2*x3*y5 + x2*y3*x5 + y2*x3*x5) +
         fxyy*(y2*y3*x5 + y2*x3*y5 + x2*y3*y5);
    r8 = t1 + t2 + t3;
    createeightdual(L, h, r2, r3, r4, r5, r6, r7, r8);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected number or dual number.", "dual.hypot");
  }
  return 1;
}


/* sqrt(1+x^2), created by Gemini AI, put to the public domain, sqrt(1 + x^2); 6.7.5 */
int dual_hypot2 (lua_State *L) {
  long double x1, x2, x3, x4, h, h3, h5, fx, fxx, fxxx, res2, res3, res4;
  int toconstant = agnL_optboolean(L, 3, 0);
  int s = checkdual(L, 1, "dual.hypot2", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }
  x1 = agn_reggetinumber(L, 1, 1);
  x2 = agn_reggetinumber(L, 1, 2);
  /* y1 is constant 1.0L, all other y_n are 0.0L */
  h = tools_hypotl(x1, 1.0L);
  if (h == 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  /* 1st order partial w.r.t x */
  fx = x1/h;
  if (s == 2) {
    createdual(L, h, fx*x2);
  } else if (s == 4) {
    x3 = agn_reggetinumber(L, 1, 3);
    x4 = agn_reggetinumber(L, 1, 4);
    /* 2nd order partial w.r.t x (fxx) */
    /* Derived from (y^2/h^3) where y=1 */
    h3 = h*h*h;
    fxx = 1.0L/h3;
    res2 = fx*x2;
    res3 = fx*x3;
    /* Interaction terms for y derivatives dropped as they are zero */
    res4 = fmal(fx, x4, fxx*x2*x3);
    createhyperdual(L, h, res2, res3, res4);
  } else if (s == 8) {
    long double x5, x6, x7, x8, r2, r3, r4, r5, r6, r7, r8, t1, t2, t3;
    x3 = agn_reggetinumber(L, 1, 3); x4 = agn_reggetinumber(L, 1, 4);
    x5 = agn_reggetinumber(L, 1, 5); x6 = agn_reggetinumber(L, 1, 6);
    x7 = agn_reggetinumber(L, 1, 7); x8 = agn_reggetinumber(L, 1, 8);
    h3 = h*h*h;
    h5 = h3*h*h;
    fxx = 1.0L/h3;
    /* Third partial w.r.t x: fxxx = -3*x*y^2/h^5 where y=1 */
    fxxx = (-3.0L*x1)/h5;
    /* First order slots */
    r2 = fx*x2;
    r3 = fx*x3;
    r5 = fx*x5;
    /* Second order slots (Hessian simplified for constant y) */
    #define CALC_S_HESSIAN(xa, xb, xm) fmal(fx, xm, fxx*xa*xb)
    r4 = CALC_S_HESSIAN(x2, x3, x4);
    r6 = CALC_S_HESSIAN(x2, x5, x6);
    r7 = CALC_S_HESSIAN(x3, x5, x7);
    /* Third order slot (Full House simplified) */
    t1 = fx*x8;
    t2 = fxx*fmal(x2, x7, fmal(x3, x6, x4*x5));
    t3 = fxxx*(x2*x3*x5);
    r8 = t1 + t2 + t3;
    createeightdual(L, h, r2, r3, r4, r5, r6, r7, r8);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected dual number type.", "dual.hypot2");
  }
  return 1;
}


/* sqrt(1 - x^2), created by Gemini AI, put to the public domain; 6.7.5 */
int dual_hypot3 (lua_State *L) {
  long double x1, x2, x3, x4, h, h3, h5, fx, fxx, fxxx, res2, res3, res4;
  int toconstant = agnL_optboolean(L, 3, 0);
  int s = checkdual(L, 1, "dual.hypot3", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }
  x1 = agn_reggetinumber(L, 1, 1);
  x2 = agn_reggetinumber(L, 1, 2);
  long double inner = 1.0L - x1*x1;
  if (inner <= 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  h = sqrtl(inner);
  /* 1st order partial: -x/h */
  fx = -x1/h;
  if (s == 2) {
    createdual(L, h, fx*x2);
  } else if (s == 4) {
    x3 = agn_reggetinumber(L, 1, 3);
    x4 = agn_reggetinumber(L, 1, 4);
    /* 2nd order partial: -1/h^3 */
    h3 = h*h*h;
    fxx = -1.0L/h3;
    res2 = fx*x2;
    res3 = fx*x3;
    res4 = fmal(fx, x4, fxx*x2*x3);
    createhyperdual(L, h, res2, res3, res4);
  } else if (s == 8) {
    long double x5, x6, x7, x8, r2, r3, r4, r5, r6, r7, r8, t1, t2, t3;
    x3 = agn_reggetinumber(L, 1, 3); x4 = agn_reggetinumber(L, 1, 4);
    x5 = agn_reggetinumber(L, 1, 5); x6 = agn_reggetinumber(L, 1, 6);
    x7 = agn_reggetinumber(L, 1, 7); x8 = agn_reggetinumber(L, 1, 8);
    h3 = h*h*h;
    h5 = h3*h*h;
    fxx = -1.0L/h3;
    /* 3rd order partial: -3*x/h^5 */
    fxxx = (-3.0L*x1)/h5;
    r2 = fx*x2;
    r3 = fx*x3;
    r5 = fx*x5;
    #define CALC_S_HESSIAN_M(xa, xb, xm) fmal(fx, xm, fxx*xa*xb)
    r4 = CALC_S_HESSIAN_M(x2, x3, x4);
    r6 = CALC_S_HESSIAN_M(x2, x5, x6);
    r7 = CALC_S_HESSIAN_M(x3, x5, x7);
    t1 = fx*x8;
    t2 = fxx*fmal(x2, x7, fmal(x3, x6, x4*x5));
    t3 = fxxx*(x2*x3*x5);
    r8 = t1 + t2 + t3;
    createeightdual(L, h, r2, r3, r4, r5, r6, r7, r8);
  } else {
    luaL_error(L, "Error: expected dual number type.", "dual.hypot3");
  }
  return 1;
}


/* Created by Gemini AI; Optimized for precision and stability, sqrt(x^2 - y^2), 6.6.11 */
int dual_cathet (lua_State *L) {
  long double x1, x2, x3, x4, y1, y2, y3, y4, f, f3, fx, fy, fxx, fyy, fxy, res2, res3, res4;
  int toconstant = agnL_optboolean(L, 3, 0);
  int sx = checkdual(L, 1, "dual.cathet", toconstant);
  int sy = checkdual(L, 2, "dual.cathet", toconstant);
  if (sx == 1 || sy == 1) { lua_settop(L, sx == 1 ? 1 : 2); return 1; }  /* 6.7.4 fix */
  x1 = agn_reggetinumber(L, 1, 1);
  x2 = agn_reggetinumber(L, 1, 2);
  y1 = agn_reggetinumber(L, 2, 1);
  y2 = agn_reggetinumber(L, 2, 2);
  /* STABILIZATION: Use (x-y)(x+y) instead of x*x - y*y */
  /* This prevents precision loss when x and y are nearly equal */
  /* g = (x1 - y1)*(x1 + y1); */
  f = tools_mhypotl(x1, y1);
  /* Handle domain: If x^2 - y^2 <= 0, derivatives are infinite or imaginary */
  if (!tools_fpisfinitel(f) || f == 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  /* 1st order partials */
  fx = x1/f;
  fy = -y1/f;
  if (sx == 2 && sy == 2) {
    createdual(L, f, fmal(fx, x2, fy*y2));
  } else if (sx == 4 && sy == 4) {
    x3 = agn_reggetinumber(L, 1, 3);
    x4 = agn_reggetinumber(L, 1, 4);
    y3 = agn_reggetinumber(L, 2, 3);
    y4 = agn_reggetinumber(L, 2, 4);
    /* 2nd order partials: Derived from f = sqrt(x^2 - y^2) */
    f3 = f*f*f;
    fxx = -(y1*y1)/f3;
    fyy = -(x1*x1)/f3;
    fxy = (x1*y1)/f3;
    res2 = fmal(fx, x2, fy*y2);
    res3 = fmal(fx, x3, fy*y3);
    /* Combined Partial Derivative Template */
    res4 = fmal(fx, x4, fy*y4) +
           fmal(fxx, x2*x3, fmal(fyy, y2*y3, fxy*(x2*y3 + y2*x3)));
    createhyperdual(L, f, res2, res3, res4);
  } else if (sx == 8 && sy == 8) {
    long double x3, x4, x5, x6, x7, x8;
    long double y3, y4, y5, y6, y7, y8;
    long double f, f3, f5, fx, fy, fxx, fyy, fxy, fxxx, fyyy, fxxy, fxyy;
    long double r2, r3, r4, r5, r6, r7, r8;
    checkdual(L, 1, "dual.cathet", 1);
    checkdual(L, 2, "dual.cathet", 1);
    /* Load X register */
    x3 = agn_reggetinumber(L, 1, 3); x4 = agn_reggetinumber(L, 1, 4);
    x5 = agn_reggetinumber(L, 1, 5); x6 = agn_reggetinumber(L, 1, 6);
    x7 = agn_reggetinumber(L, 1, 7); x8 = agn_reggetinumber(L, 1, 8);
    /* Load Y register */
    y3 = agn_reggetinumber(L, 2, 3); y4 = agn_reggetinumber(L, 2, 4);
    y5 = agn_reggetinumber(L, 2, 5); y6 = agn_reggetinumber(L, 2, 6);
    y7 = agn_reggetinumber(L, 2, 7); y8 = agn_reggetinumber(L, 2, 8);
    f = tools_mhypotl(x1, y1);
    if (!tools_fpisfinitel(f) || f == 0.0L) { lua_pushundefined(L); return 1; }
    f3 = f*f*f;
    f5 = f3*f*f;
    /* 1st and 2nd partials */
    fx = x1/f;
    fy = -y1/f;
    fxx = -(y1*y1)/f3;
    fyy = -(x1*x1)/f3;
    fxy = (x1*y1)/f3;
    /* 3rd partials */
    fxxx = (3.0L*x1*y1*y1)/f5;
    fyyy = (-3.0L*y1*x1*x1)/f5;
    fxxy = (-y1*(2.0L*x1*x1 + y1*y1))/f5;
    fxyy = (x1*(2.0L*y1*y1 + x1*x1))/f5;
    /* Slots 2, 3, 5 (First order) */
    r2 = fmal(fx, x2, fy*y2);
    r3 = fmal(fx, x3, fy*y3);
    r5 = fmal(fx, x5, fy*y5);
    /* Slots 4, 6, 7 (Second order) - Reusing the Hessian logic */
    #define CALC_HESS4(xa, xb, ya, yb, xm, ym) \
        fmal(fx, xm, fy*ym) + fmal(fxx, xa*xb, fmal(fyy, ya*yb, fxy*(xa*yb + ya*xb)))
    r4 = CALC_HESS4(x2, x3, y2, y3, x4, y4);
    r6 = CALC_HESS4(x2, x5, y2, y5, x6, y6);
    r7 = CALC_HESS4(x3, x5, y3, y5, x7, y7);
    /* Slot 8 (Third order) */
    long double t1 = fmal(fx, x8, fy*y8);
    long double t2 = fxx*(x2*x7 + x3*x6 + x4*x5) +
                     fyy*(y2*y7 + y3*y6 + y4*y5) +
                     fxy*(x2*y7 + x3*y6 + x4*y5 + y2*x7 + y3*x6 + y4*x5);
    long double t3 = fxxx*(x2*x3*x5) + fyyy*(y2*y3*y5) +
                     fxxy*(x2*x3*y5 + x2*y3*x5 + y2*x3*x5) +
                     fxyy*(y2*y3*x5 + y2*x3*y5 + x2*y3*y5);
    r8 = t1 + t2 + t3;
    createeightdual(L, f, r2, r3, r4, r5, r6, r7, r8);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected number or dual number.", "dual.cathet");
  }
  return 1;
}


/* Created by Gemini AI, put to the public domain, 1/sqrt(x^2 + y^2); 6.7.9 */
int dual_invhypot (lua_State *L) {
  long double x1, x2, x3, x4, y1, y2, y3, y4, h, h2, h3, h5, h7, invh;
  long double fx, fy, fxx, fyy, fxy, res2, res3, res4;
  int toconstant = agnL_optboolean(L, 3, 0);
  int sx = checkdual(L, 1, "dual.invhypot", toconstant);
  int sy = checkdual(L, 2, "dual.invhypot", toconstant);
  if (sx == 1 || sy == 1) { lua_settop(L, sx == 1 ? 1 : 2); return 1; }
  x1 = agn_reggetinumber(L, 1, 1);
  x2 = agn_reggetinumber(L, 1, 2);
  y1 = agn_reggetinumber(L, 2, 1);
  y2 = agn_reggetinumber(L, 2, 2);
  /* Use robust hypot to prevent premature underflow/overflow */
  h = tools_hypotl(x1, y1);
  if (h == 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  h2 = h*h;
  h3 = h2*h;
  h5 = h3*h2;
  invh = 1.0L/h;
  /* 1st order partials: d/dx (h^-1) = -x*h^-3 */
  fx = -x1/h3;
  fy = -y1/h3;
  if (sx == 2 && sy == 2) {
    createdual(L, invh, fmal(fx, x2, fy*y2));
  } else if (sx == 4 && sy == 4) {
    x3 = agn_reggetinumber(L, 1, 3);
    x4 = agn_reggetinumber(L, 1, 4);
    y3 = agn_reggetinumber(L, 2, 3);
    y4 = agn_reggetinumber(L, 2, 4);
    /* 2nd order partials */
    fxx = (2.0L*x1*x1 - y1*y1)/h5;
    fyy = (2.0L*y1*y1 - x1*x1)/h5;
    fxy = (3.0L*x1*y1)/h5;
    res2 = fmal(fx, x2, fy*y2);
    res3 = fmal(fx, x3, fy*y3);
    /* res4 = (fx*x4 + fy*y4) + (fxx*x2*x3 + fyy*y2*y3 + fxy*(x2*y3 + y2*x3)) */
    res4 = fmal(fx, x4, fy*y4) +
           fmal(fxx, x2*x3, fmal(fyy, y2*y3, fxy*(x2*y3 + y2*x3)));
    createhyperdual(L, invh, res2, res3, res4);
  } else if (sx == 8 && sy == 8) {
    long double x5, x6, x7, x8, y5, y6, y7, y8;
    long double fxxx, fyyy, fxxy, fxyy;
    long double r2, r3, r4, r5, r6, r7, r8, t1, t2, t3;
    x3 = agn_reggetinumber(L, 1, 3); x4 = agn_reggetinumber(L, 1, 4);
    x5 = agn_reggetinumber(L, 1, 5); x6 = agn_reggetinumber(L, 1, 6);
    x7 = agn_reggetinumber(L, 1, 7); x8 = agn_reggetinumber(L, 1, 8);
    y3 = agn_reggetinumber(L, 2, 3); y4 = agn_reggetinumber(L, 2, 4);
    y5 = agn_reggetinumber(L, 2, 5); y6 = agn_reggetinumber(L, 2, 6);
    y7 = agn_reggetinumber(L, 2, 7); y8 = agn_reggetinumber(L, 2, 8);
    h7 = h5*h2;
    /* 2nd order partials */
    fxx = (2.0L*x1*x1 - y1*y1)/h5;
    fyy = (2.0L*y1*y1 - x1*x1)/h5;
    fxy = (3.0L*x1*y1)/h5;
    /* 3rd order partials */
    fxxx = (3.0L*x1*(3.0L*y1*y1 - 2.0L*x1*x1))/h7;
    fyyy = (3.0L*y1*(3.0L*x1*x1 - 2.0L*y1*y1))/h7;
    fxxy = (3.0L*y1*(4.0L*x1*x1 - y1*y1))/h7;
    fxyy = (3.0L*x1*(4.0L*y1*y1 - x1*x1))/h7;
    r2 = fmal(fx, x2, fy*y2);
    r3 = fmal(fx, x3, fy*y3);
    r5 = fmal(fx, x5, fy*y5);
    #define CALC_HESSIAN_INV(xa, xb, ya, yb, xm, ym) \
        fmal(fx, xm, fy*ym) + fmal(fxx, xa*xb, fmal(fyy, ya*yb, fxy*(xa*yb + ya*xb)))
    r4 = CALC_HESSIAN_INV(x2, x3, y2, y3, x4, y4);
    r6 = CALC_HESSIAN_INV(x2, x5, y2, y5, x6, y6);
    r7 = CALC_HESSIAN_INV(x3, x5, y3, y5, x7, y7);
    /* Slot 8: Third order logic */
    t1 = fmal(fx, x8, fy*y8);
    t2 = fxx*(x2*x7 + x3*x6 + x4*x5) + fyy*(y2*y7 + y3*y6 + y4*y5) +
         fxy*(x2*y7 + x3*y6 + x4*y5 + y2*x7 + y3*x6 + y4*x5);
    t3 = fxxx*(x2*x3*x5) + fyyy*(y2*y3*y5) +
         fxxy*(x2*x3*y5 + x2*y3*x5 + y2*x3*x5) +
         fxyy*(y2*y3*x5 + y2*x3*y5 + x2*y3*y5);
    r8 = t1 + t2 + t3;
    createeightdual(L, invh, r2, r3, r4, r5, r6, r7, r8);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected number or dual number.", "dual.invhypot");
  }
  return 1;
}


/* Created by Gemini AI; Function for f(x,y) = x^2 + y^2, 6.6.11 */
int dual_pytha (lua_State *L) {
  long double x1, x2, x3, x4, y1, y2, y3, y4, h, fx, fy, res2, res3, res4;
  int toconstant = agnL_optboolean(L, 3, 0);
  int sx = checkdual(L, 1, "dual.pytha", toconstant);
  int sy = checkdual(L, 2, "dual.pytha", toconstant);
  if (sx == 1 || sy == 1) { lua_settop(L, sx == 1 ? 1 : 2); return 1; }  /* 6.7.4 fix */
  x1 = agn_reggetinumber(L, 1, 1);
  x2 = agn_reggetinumber(L, 1, 2);
  y1 = agn_reggetinumber(L, 2, 1);
  y2 = agn_reggetinumber(L, 2, 2);
  /* The core function */
  h = tools_pythal(x1, y1);  /* h = x1*x1 + y1*y1; */
  /* 1st order partials: d/dx(x^2+y^2) = 2x */
  fx = 2.0L*x1;
  fy = 2.0L*y1;
  if (sx == 2 && sy == 2) {
    createdual(L, h, fmal(fx, x2, fy*y2));
  } else if (sx == 4 && sy == 4) {
    x3 = agn_reggetinumber(L, 1, 3);
    x4 = agn_reggetinumber(L, 1, 4);
    y3 = agn_reggetinumber(L, 2, 3);
    y4 = agn_reggetinumber(L, 2, 4);
    /* 2nd order partials are CONSTANTS */
    /* fxx = 2, fyy = 2, fxy = 0 */
    res2 = fmal(fx, x2, fy*y2);
    res3 = fmal(fx, x3, fy*y3);
    /* Slot 4 simplifies significantly because fxy is 0:
       res4 = (fx*x4 + fy*y4) + (2*x2*x3 + 2*y2*y3) */
    res4 = fmal(fx, x4, fy*y4) + (2.0L*x2*x3) + (2.0L*y2*y3);
    createhyperdual(L, h, res2, res3, res4);
  } else if (sx == 8 && sy == 8) {
    long double x3, x4, x5, x6, x7, x8;
    long double y3, y4, y5, y6, y7, y8;
    long double h, fx, fy, r2, r3, r4, r5, r6, r7, r8;
    /* Fetch all 8 slots for X */
    x3 = agn_reggetinumber(L, 1, 3); x4 = agn_reggetinumber(L, 1, 4);
    x5 = agn_reggetinumber(L, 1, 5); x6 = agn_reggetinumber(L, 1, 6);
    x7 = agn_reggetinumber(L, 1, 7); x8 = agn_reggetinumber(L, 1, 8);
    /* Fetch all 8 slots for Y */
    y3 = agn_reggetinumber(L, 2, 3); y4 = agn_reggetinumber(L, 2, 4);
    y5 = agn_reggetinumber(L, 2, 5); y6 = agn_reggetinumber(L, 2, 6);
    y7 = agn_reggetinumber(L, 2, 7); y8 = agn_reggetinumber(L, 2, 8);
    h = (x1*x1) + (y1*y1);
    fx = 2.0L*x1;
    fy = 2.0L*y1;
    /* First order (Slots 2, 3, 5) - Partial derivatives wrt epsilon_1, 2, 3 */
    r2 = fmal(fx, x2, fy*y2);
    r3 = fmal(fx, x3, fy*y3);
    r5 = fmal(fx, x5, fy*y5);
    /* Second order (Slots 4, 6, 7) - Mixed partials
       Formula: (fx*x_mixed + fy*y_mixed) + (fxx*xa*xb + fyy*ya*yb) */
    r4 = fmal(fx, x4, fy*y4) + 2.0L*(x2*x3 + y2*y3);
    r6 = fmal(fx, x6, fy*y6) + 2.0L*(x2*x5 + y2*y5);
    r7 = fmal(fx, x7, fy*y7) + 2.0L*(x3*x5 + y3*y5);
    /* Third order (Slot 8) - The "Tri-dual" interaction
       Since fxxx = fyyy = fxyy = 0, we only track the propagation of second partials */
    r8 = fmal(fx, x8, fy*y8) +
         2.0L*(x2*x7 + x3*x6 + x4*x5) +
         2.0L*(y2*y7 + y3*y6 + y4*y5);
    createeightdual(L, h, r2, r3, r4, r5, r6, r7, r8);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected number or dual number.", "dual.pytha");
  }
  return 1;
}


/* Created by Gemini AI; Function for f(x,y) = x^2 - y^2, 6.6.11 */
int dual_pytha4 (lua_State *L) {
  long double x1, x2, x3, x4, y1, y2, y3, y4, h, fx, fy, res2, res3, res4;
  int toconstant = agnL_optboolean(L, 3, 0);
  int sx = checkdual(L, 1, "dual.pytha4", toconstant);
  int sy = checkdual(L, 2, "dual.pytha4", toconstant);
  if (sx == 1 || sy == 1) { lua_settop(L, sx == 1 ? 1 : 2); return 1; }  /* 6.7.4 fix */
  x1 = agn_reggetinumber(L, 1, 1);
  x2 = agn_reggetinumber(L, 1, 2);
  y1 = agn_reggetinumber(L, 2, 1);
  y2 = agn_reggetinumber(L, 2, 2);
  /* Core function: No stabilization needed here as there is no square root */
  h = tools_mpythal(x1, y1);  /* h = x1*x1 - y1*y1; */
  /* 1st order partials */
  fx = 2.0L*x1;
  fy = -2.0L*y1;
  if (sx == 2 && sy == 2) {
    createdual(L, h, fmal(fx, x2, fy*y2));
  } else if (sx == 4 && sy == 4) {
    x3 = agn_reggetinumber(L, 1, 3);
    x4 = agn_reggetinumber(L, 1, 4);
    y3 = agn_reggetinumber(L, 2, 3);
    y4 = agn_reggetinumber(L, 2, 4);
    /* 2nd order partials: fxx = 2, fyy = -2, fxy = 0 */
    res2 = fmal(fx, x2, fy*y2);
    res3 = fmal(fx, x3, fy*y3);
    /* Slot 4:
       res4 = (fx*x4 + fy*y4) + (fxx*x2*x3 + fyy*y2*y3 + fxy*(...))
       res4 = (2*x1*x4 - 2*y1*y4) + (2*x2*x3 - 2*y2*y3) */
    res4 = fmal(fx, x4, fy*y4) + (2.0L*x2*x3) - (2.0L*y2*y3);
    createhyperdual(L, h, res2, res3, res4);
  } else if (sx == 8 && sy == 8) {
    long double x3, x4, x5, x6, x7, x8;
    long double y3, y4, y5, y6, y7, y8;
    long double h, fx, fy, r2, r3, r4, r5, r6, r7, r8;
    /* Fetch all 8 slots for X */
    x3 = agn_reggetinumber(L, 1, 3); x4 = agn_reggetinumber(L, 1, 4);
    x5 = agn_reggetinumber(L, 1, 5); x6 = agn_reggetinumber(L, 1, 6);
    x7 = agn_reggetinumber(L, 1, 7); x8 = agn_reggetinumber(L, 1, 8);
    /* Fetch all 8 slots for Y */
    y3 = agn_reggetinumber(L, 2, 3); y4 = agn_reggetinumber(L, 2, 4);
    y5 = agn_reggetinumber(L, 2, 5); y6 = agn_reggetinumber(L, 2, 6);
    y7 = agn_reggetinumber(L, 2, 7); y8 = agn_reggetinumber(L, 2, 8);
    h = (x1*x1) - (y1*y1);
    fx = 2.0L*x1;
    fy = -2.0L*y1;
    /* First order (Slots 2, 3, 5) */
    r2 = fmal(fx, x2, fy*y2);
    r3 = fmal(fx, x3, fy*y3);
    r5 = fmal(fx, x5, fy*y5);
    /* Second order (Slots 4, 6, 7)
       Formula: f_x*x_mixed + f_y*y_mixed + f_xx*x_a*x_b + f_yy*y_a*y_b */
    r4 = fmal(fx, x4, fy*y4) + 2.0L*(x2*x3 - y2*y3);
    r6 = fmal(fx, x6, fy*y6) + 2.0L*(x2*x5 - y2*y5);
    r7 = fmal(fx, x7, fy*y7) + 2.0L*(x3*x5 - y3*y5);
    /* Third order (Slot 8)
       Formula: f_x*x8 + f_y*y8 + f_xx*(x2*x7 + x3*x6 + x4*x5) + f_yy*(y2*y7 + y3*y6 + y4*y5)
       (f_xxx and others are 0, so those terms vanish) */
    r8 = fmal(fx, x8, fy*y8) +
         2.0L*(x2*x7 + x3*x6 + x4*x5) -
         2.0L*(y2*y7 + y3*y6 + y4*y5);
    createeightdual(L, h, r2, r3, r4, r5, r6, r7, r8);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected number or dual number.", "dual.pytha4");
  }
  return 1;
}


/* Created by Gemini AI, put to the public domain; 6.6.6 */
int dual_cbrt (lua_State *L) {
  long double a1, a2, a3, a4, real;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.cbrt", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a1 = agn_reggetinumber(L, 1, 1);
  a2 = agn_reggetinumber(L, 1, 2);
  /* The real part: cbrt(a1) works for positive, negative, and zero */
  real = tools_cbrtl(a1);
  if (real == 0.0L) {
    /* At real = x = 0, the derivative 1/(3*x^2) is infinite.
       If a2 is 0, we can return (0,0), otherwise it's undefined. */
    if (a2 == 0.0L) {
      createdual(L, 0.0L, 0.0L);
    } else {
      lua_pushundefined(L);
    }
  } else if (s == 2) {
    /* The dual part: a2*(1/(3*x^2)) */
    createdual(L, real, a2/(3.0L*real*real));
  } else if (s == 4) {
    long double r2_3, df, ddf;
    r2_3 = 3.0L*real*real;
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    /* ("first derivative of function(a1)"*a4) + ( ("second derivative of function(a1)")*a2*a3) */
    df = 1.0L/r2_3;
    ddf = -2.0L/(3.0L*a1*r2_3);
    createhyperdual(L, real, a2/r2_3, a3/r2_3, fmal(df, a4, ddf*a2*a3) );
  } else {
    long double a5, a6, a7, a8;
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    long double x_23 = real*real;        /* x^(2/3) */
    long double x_53 = x_23*x_23*real;   /* x^(5/3) */
    long double x_83 = x_53*x_23*real;   /* x^(8/3) */
    long double df   = 1.0L/(3.0L*x_23);
    long double ddf  = -2.0L/(9.0L*x_53);
    long double dddf = 10.0L/(27.0L*x_83);
    long double term8 = fmal(df, a8, ddf*(a2*a7 + a3*a6 + a4*a5) + dddf*(a2*a3*a5));
    createeightdual(L, real, a2*df, a3*df, fmal(df, a4, ddf*a2*a3),
      a5*df, fmal(df, a6, ddf*a2*a5), fmal(df, a7, ddf*a3*a5), term8);
  }
  return 1;
}


/* https://arxiv.org/pdf/1411.0583.pdf */
static int mt_exp (lua_State *L) {
  long double a1, a2, a3, a4, a5, a6, a7, a8, val;
  int s = checkdual(L, 1, "dual.__exp", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a1 = agn_reggetinumber(L, 1, 1);
  a2 = agn_reggetinumber(L, 1, 2);
  val = expl(a1);  /* f = f' = f'' = f''' */
  if (s == 2) {
    createdual(L, val, val*a2);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    createhyperdual(L, val, val*a2, val*a3, val*(a4 + a2*a3));
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    createeightdual(L, val, val*a2, val*a3, val*(a4 + a2*a3),
      val*a5, val*(a6 + a2*a5), val*(a7 + a3*a5), val*(a8 + (a2*a7 + a3*a6 + a4*a5) + (a2*a3*a5)));
  }
  return 1;
}


/* https://gist.github.com/chris-taylor/2005955 &
   https://www.arctbds.com/volume4/arctbds_submission_28.pdf, Chapter 3.4 The dual logarithmic function */
static int mt_ln (lua_State *L) {
  long double a1, a2, a3, a4;  /* change to long double 6.6.6 */
  int s = checkdual(L, 1, "dual.__ln", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a1 = agn_reggetinumber(L, 1, 1);
  a2 = agn_reggetinumber(L, 1, 2);
  if (a1 <= 0.0L) {  /* 6.6.6 extension */
    lua_pushundefined(L);
  } else if (s == 2) {
    createdual(L, tools_logl(a1), a2/a1);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    /* ("first derivative of function(a1)"*a4) + ( ("second derivative of function(a1)")*a2*a3) */
    createhyperdual(L, tools_logl(a1), a2/a1, a3/a1, fmal((1.0L/a1), a4, (-1.0L/(a1*a1))*a2*a3) );
  } else {
    long double a5, a6, a7, a8;
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    long double inv_a1 = 1.0L/a1;
    long double inv_a1_sq = -1.0L/(a1*a1);
    long double inv_a1_cu = 2.0L/(a1*a1*a1);
    /* Slot 8 = (f'*a8) + f''*(a2*a7 + a3*a6 + a4*a5) + f'''*(a2*a3*a5) */
    long double term1 = inv_a1*a8;
    long double term2 = inv_a1_sq*(a2*a7 + a3*a6 + a4*a5);
    long double term3 = inv_a1_cu*(a2*a3*a5);
    createeightdual(L,
      tools_logl(a1), a2*inv_a1, a3*inv_a1,
      fmal(inv_a1, a4, inv_a1_sq*a2*a3), a5*inv_a1, fmal(inv_a1, a6, inv_a1_sq*a2*a5),
      fmal(inv_a1, a7, inv_a1_sq*a3*a5), term1 + term2 + term3);
  }
  return 1;
}


static int mt_sin (lua_State *L) {
  long double a1, a2, a3, a4, a5, a6, a7, a8, s_val, c_val;
  int s = checkdual(L, 1, "dual.__sin", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a1 = agn_reggetinumber(L, 1, 1);
  a2 = agn_reggetinumber(L, 1, 2);
  sun_sincosl(a1, &s_val, &c_val);  /* 6.7.1 change and OS/2 fix (in GCC4OS/2, sinl(), cosl() are buggy.) */
  if (s == 2) {
    createdual(L, s_val, c_val*a2);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    createhyperdual(L, s_val, c_val*a2, c_val*a3, fmal(c_val, a4, -s_val*a2*a3));
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    /* a8 = (cos*a8) - sin*(a2*a7 + a3*a6 + a4*a5) - cos*(a2*a3*a5) */
    long double term8 = c_val*a8 - s_val*(a2*a7 + a3*a6 + a4*a5) - c_val*(a2*a3*a5);
    createeightdual(L, s_val, c_val*a2, c_val*a3, fmal(c_val, a4, -s_val*a2*a3),
      c_val*a5, fmal(c_val, a6, -s_val*a2*a5), fmal(c_val, a7, -s_val*a3*a5), term8);
  }
  return 1;
}


static int mt_cos (lua_State *L) {
  long double a[8], r[8], si, co, df, ddf, dddf;
  int s = checkdual(L, 1, "dual.__cos", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  sun_sincosl(a[0], &si, &co);
  /* Derivatives based on sincos */
  df   = -si;  /* -sin(x) */
  ddf  = -co;  /* -cos(x) */
  dddf = si;   /*  sin(x) */
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, co, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    createhyperdual(L, co, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define COS_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = COS_C2(1, 2, 3);
    r[5] = COS_C2(1, 4, 5);
    r[6] = COS_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123)
       Formula: f'a7 + f''(a1*a6 + a2*a5 + a3*a4) + f'''(a1*a2*a4) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, co, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


static int mt_tan (lua_State *L) {
  long double a[8], r[8], si, co, t, sec2, ddf, dddf;
  int s = checkdual(L, 1, "dual.__tan", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  sun_sincosl(a[0], &si, &co);
  if (co == 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  t = si/co;
  sec2 = fmal(t, t, 1.0L);  /* f' = 1 + tan^2 */
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, t, sec2*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    ddf = 2.0L*sec2*t;  /* f'' = 2*sec^2*tan */
    createhyperdual(L, t, sec2*a[1], sec2*a[2], fmal(sec2, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf = 2.0L*sec2*t;
    /* f''' = 2*sec^2*(sec^2 + 2*tan^2) */
    dddf = 2.0L*sec2*(sec2 + 2.0L*(t*t));
    /* 1st order slots */
    r[1] = sec2*a[1];
    r[2] = sec2*a[2];
    r[4] = sec2*a[4];
    /* 2nd order slots */
    #define TAN_C2(ia, ib, im) fmal(sec2, a[im], ddf*a[ia]*a[ib])
    r[3] = TAN_C2(1, 2, 3);
    r[5] = TAN_C2(1, 4, 5);
    r[6] = TAN_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123)
       Formula: f'a7 + f''(a1*a6 + a2*a5 + a3*a4) + f'''(a1*a2*a4) */
    r[7] = fmal(sec2, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, t, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* arcsin defined in "DNAD a Simple Tool for Automatic Differentiation of Fortran Code.pdf",
   see https://digitalcommons.usu.edu/cgi/viewcontent.cgi?article=1029&context=mae_facpub */
static int mt_arcsin (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, denom, root_denom;
  int s = checkdual(L, 1, "dual.__arcsin", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  if (fabsl(a[0]) >= 1.0L) {
    lua_pushundefined(L);
    return 1;
  }
  real = sun_asinl(a[0]);
  denom = fmal(-a[0], a[0], 1.0L);  /* (1 - x^2) */
  root_denom = safe_sqrtl(denom);   /* sqrt(1 - x^2) */
  df = 1.0L/root_denom;             /* (1 - x^2)^(-1/2) */
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    ddf = a[0]/(denom*root_denom);  /* x*(1 - x^2)^(-3/2) */
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf = a[0]/(denom*root_denom);
    /* f''' = (1 + 2x^2)/(1 - x^2)^(5/2)
       Optimization: dddf = (1 + 2x^2)*(df/denom^2) */
    dddf = (1.0L + 2.0L*a[0]*a[0])/(denom*denom*root_denom);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define ASIN_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = ASIN_C2(1, 2, 3);
    r[5] = ASIN_C2(1, 4, 5);
    r[6] = ASIN_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123)
       Formula: f'a7 + f''(a1*a6 + a2*a5 + a3*a4) + f'''(a1*a2*a4) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


static int mt_arccos (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, denom, root_denom;
  int s = checkdual(L, 1, "dual.__arccos", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  if (fabsl(a[0]) >= 1.0L) {
    lua_pushundefined(L);
    return 1;
  }
  real = sun_acosl(a[0]);
  denom = fmal(-a[0], a[0], 1.0L);  /* (1 - x^2) */
  root_denom = safe_sqrtl(denom);   /* sqrt(1 - x^2) */
  /* Coefficients for arccos are the negative of arcsin */
  df = -1.0L/root_denom;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    ddf = -a[0]/(denom*root_denom);
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf = -a[0]/(denom*root_denom);
    /* f''' coefficient */
    dddf = -(1.0L + 2.0L*a[0]*a[0])/(denom*denom*root_denom);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define ACOS_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = ACOS_C2(1, 2, 3);
    r[5] = ACOS_C2(1, 4, 5);
    r[6] = ACOS_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


static int mt_arctan (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, denom, d2;
  int s = checkdual(L, 1, "dual.__arctan", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  real = sun_atanl(a[0]);
  denom = fmal(a[0], a[0], 1.0L);  /* (1 + x^2) */
  df = 1.0L/denom;                 /* 1/(1 + x^2) */
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    d2 = denom*denom;
    ddf = (-2.0L*a[0])/d2;
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    d2 = denom*denom;
    ddf = (-2.0L*a[0])/d2;
    /* f''' = (6x^2 - 2)/(1 + x^2)^3 */
    dddf = fmal(6.0L*a[0], a[0], -2.0L)/(d2*denom);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define ATAN_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = ATAN_C2(1, 2, 3);
    r[5] = ATAN_C2(1, 4, 5);
    r[6] = ATAN_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI, put to the public domain; 6.6.6, fully patched 6.7.5 */
static int mt_arcsec (lua_State *L) {
  int i;
  long double a[8] = {0}, r[8] = {0}, real, df, ddf, a1_sq, denom_base, root_denom, abs_a1;
  int s = checkdual(L, 1, "dual.__arcsec", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  abs_a1 = fabsl(a[0]);
  if (abs_a1 < 1.0L) {
    lua_pushundefined(L);
    return 1;
  }
  for (i=1; i < s; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
  if (abs_a1 == 1.0L) {
    for (i = 1; i < s; i++) {
      if (a[i] != 0.0L) { lua_pushundefined(L); return 1; }
    }
    real = (a[0] > 0.0L ? 0.0L : sun_acosl(-1.0L));
    if (s == 2) { createdual(L, real, 0.0); }
    else if (s == 4) { createhyperdual(L, real, 0.0, 0.0, 0.0); }
    else { createeightdual(L, real, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0); }
    return 1;
  }
  real = sun_acosl(1.0L/a[0]);
  a1_sq = a[0]*a[0];
  denom_base = a1_sq - 1.0L;
  root_denom = safe_sqrtl(denom_base);
  /* f'(x) */
  df = 1.0L/(abs_a1*root_denom);
  if (s == 2) {
    createdual(L, real, df*a[1]);
    return 1;
  }
  long double sgn_x = (a[0] > 0.0L) ? 1.0L : -1.0L;
  /* f''(x) */
  ddf = -(sgn_x*(2.0L*a1_sq - 1.0L))/(a1_sq*denom_base*root_denom);
  if (s == 4) {
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    /* f'''(x) numerator fix: 6x^4 - 9x^2 + 2 */
    long double x1, x2, x4, t5, t6, t16, dddf, cross_terms, cubic_term;
    /* x1 is the real part, i.e., the value of 1/x */
    x1 = a[0];
    x2 = x1*x1;
    x4 = x2*x2;
    t5 = 1.0L - 1.0L/x2;
    t6 = safe_sqrtl(t5);
    t16 = t5*t5;
    /* f' (df) */
    df = 1.0L/(x2*t6);
    /* f'' (ddf) */
    ddf = -2.0L/(x2*x1*t6) - 1.0L/(x4*x1*t6*t5);
    /* f''' (dddf) */
    /* This matches your -6, -7, -3 logic but with arcsec signs */
    dddf = 6.0L/(x4*t6) + 7.0L/(x4*x2*t6*t5) + 3.0L/(x4*x4*t6*t16);
    r[1] = df*a[1];  /* E1 */
    r[2] = df*a[2];  /* E2 */
    r[4] = df*a[4];  /* E3 */
    /* Interaction Slots (Second Order) */
    r[3] = fmal(ddf, a[1]*a[2], df*a[3]);  /* E12 */
    r[5] = fmal(ddf, a[1]*a[4], df*a[5]);  /* E13 */
    r[6] = fmal(ddf, a[2]*a[4], df*a[6]);  /* E23 */
    /* The Full Composite Chain Rule (Third Order) */
    cross_terms = fmal(a[1], a[6], fmal(a[2], a[5], a[4]*a[3]));
    cubic_term  = a[1]*a[2]*a[4];
    /* r[7] = df*g''' + ddf*(g'*g'' interactions) + dddf*(g'^3) */
    r[7] = fmal(df, a[7], fmal(ddf, cross_terms, dddf*cubic_term));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI, put to the public domain; 6.6.6 */
int dual_arccot (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, denom, d2;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.arccot", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Pi/2 - atan(x) */
  real = (sun_acosl(-1.0L)*0.5L) - sun_atanl(a[0]);
  denom = fmal(a[0], a[0], 1.0L);  /* (1 + x^2) */
  df = -1.0L/denom;                /* -1/(1 + x^2) */
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    d2 = denom*denom;
    ddf = (2.0L*a[0])/d2;
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    d2 = denom*denom;
    ddf = (2.0L*a[0])/d2;
    /* f''' = -(6x^2 - 2)/(1 + x^2)^3 */
    dddf = -fmal(6.0L*a[0], a[0], -2.0L)/(d2*denom);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define ACOT_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = ACOT_C2(1, 2, 3);
    r[5] = ACOT_C2(1, 4, 5);
    r[6] = ACOT_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* https://gist.github.com/chris-taylor/2005955 */
static int mt_sinh (lua_State *L) {
  long double a[8], r[8], sih, coh, df, ddf, dddf;
  int i, s = checkdual(L, 1, "dual.__sinh", 0);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Using cephes_sinhcoshl for speed and accuracy (6.6.6) */
  cephes_sinhcoshl(a[0], &sih, &coh);
  /* Derivatives are cyclical without sign changes */
  df   = coh;  /* cosh(x) */
  ddf  = sih;  /* sinh(x) */
  dddf = coh;  /* cosh(x) */
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, sih, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    createhyperdual(L, sih, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    /* 1st order slots (E1, E2, E3) */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots (Mixed interactions) */
    #define SINH_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = SINH_C2(1, 2, 3);
    r[5] = SINH_C2(1, 4, 5);
    r[6] = SINH_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123)
       Rule: (f'*a7) + f''*(a1*a6 + a2*a5 + a3*a4) + f'''*(a1*a2*a4) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, sih, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* https://gist.github.com/chris-taylor/2005955 */
static int mt_cosh (lua_State *L) {
  long double a[8], r[8], sih, coh, df, ddf, dddf;
  int s = checkdual(L, 1, "dual.__cosh", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* 6.6.6 Optimization: Single call for both components */
  cephes_sinhcoshl(a[0], &sih, &coh);
  /* Swap pattern for cosh */
  df   = sih;  /* sinh(x) */
  ddf  = coh;  /* cosh(x) */
  dddf = sih;  /* sinh(x) */
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, coh, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    createhyperdual(L, coh, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define COSH_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = COSH_C2(1, 2, 3);
    r[5] = COSH_C2(1, 4, 5);
    r[6] = COSH_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123)
       Formula: f'a7 + f''(a1*6 + a2*5 + a4*3) + f'''(a1*a2*a4) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[4]*a[3]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, coh, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* https://gist.github.com/chris-taylor/2005955 &
   https://www.arctbds.com/volume4/arctbds_submission_28.pdf, Chapter 3.3 The dual hyperbolic functions (45) */
static int mt_tanh (lua_State *L) {
  long double a[8], r[8], t, df, ddf, dddf;
  int s = checkdual(L, 1, "dual.__tanh", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  t = tools_tanhl(a[0]);
  /* f' = 1 - t^2 */
  df = fmal(-t, t, 1.0L);
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, t, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    ddf = -2.0L*t*df;
    createhyperdual(L, t, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf = -2.0L*t*df;
    /* f''' = 2*df*(3*t^2 - 1) */
    dddf = 2.0L*df*fmal(3.0L*t, t, -1.0L);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define TANH_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = TANH_C2(1, 2, 3);
    r[5] = TANH_C2(1, 4, 5);
    r[6] = TANH_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, t, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Un-normalised cardinal sine of x (in radians), i.e. sin(a)/a.
   Created by Gemini AI, put to the public domain; 6.6.9 */
static int mt_sinc (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, x2;
  int s = checkdual(L, 1, "dual.__sinc", 1);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  int nearzero = fabsl(a[0]) < 1.0e-4L;
  if (nearzero) {
    x2 = a[0]*a[0];
    real = 1.0L - x2/6.0L + (x2*x2)/120.0L;
    df   = a[0]*(-1.0L/3.0L + x2/30.0L);
    ddf  = -1.0L/3.0L + x2/10.0L - (x2*x2)/168.0L;
    dddf = a[0]*(1.0L/5.0L - x2/42.0L);
  } else {
    long double si, co, x3, x4;
    sun_sincosl(a[0], &si, &co);
    x2 = a[0]*a[0];
    x3 = x2*a[0];
    x4 = x3*a[0];
    real = si/a[0];
    df   = (co/a[0]) - (si/x2);
    ddf  = (-si/a[0]) - (2.0L*co/x2) + (2.0L*si/x3);
    dddf = (-co/a[0]) + (3.0L*si/x2) + (6.0L*co/x3) - (6.0L*si/x4);
  }
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define SINC_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = SINC_C2(1, 2, 3);
    r[5] = SINC_C2(1, 4, 5);
    r[6] = SINC_C2(2, 4, 6);
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


typedef enum {
  TYPE_CSCH = 0,
  TYPE_SECH,
  TYPE_COTH
} HyperType;

/* Implementation for csch, sech, or coth */
static int dual_hyperbolic_reciprocal (lua_State *L, int type, const char *procname) {
  long double a[8], r[8], x, s_val, c_val;
  long double real, aux, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int i, s = checkdual(L, 1, procname, toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  x = agn_reggetinumber(L, 1, 1);
  cephes_sinhcoshl(x, &s_val, &c_val);
  if (type == TYPE_CSCH) {
    if (s_val == 0.0L) { lua_pushundefined(L); return 1; }
    real = 1.0L/s_val;
    aux  = c_val/s_val; /* coth */
    df   = -real*aux;
    ddf  = real*(aux*aux + real*real);
    /* Corrected: Removed the '-' before df */
    dddf = df*(aux*aux + 5.0L*real*real);
  }
  else if (type == TYPE_SECH) {
    real = 1.0L/c_val;
    aux  = s_val/c_val; /* tanh */
    df   = -real*aux;
    ddf  = real*(2.0L*aux*aux - 1.0L); /* Identity: sech(1 - 2sinh^2) */
    /* Corrected: sech jerk identity */
    dddf = -df*(6.0L*real*real - 1.0L);
  }
  else { /* TYPE_COTH */
    if (s_val == 0.0L) { lua_pushundefined(L); return 1; }
    real = c_val/s_val;
    long double csch2 = 1.0L/(s_val*s_val);
    df   = -csch2; /* f' = -csch^2 */
    ddf  = 2.0L*csch2*real; /* f'' = 2*csch^2*coth */
    /* f''' = -2*csch^2(csch^2 + 2*coth^2) */
    dddf = 2.0L*df*(csch2 + 2.0L*real*real);
  }
  /* Standard 8-slot distribution */
  /* Ensure a[1] is loaded! Your previous code skipped it in the loop */
  for (i = 1; i < s; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    r[1] = df*a[1]; r[2] = df*a[2]; r[4] = df*a[4];
    r[3] = fmal(df, a[3], ddf*a[1]*a[2]);
    r[5] = fmal(df, a[5], ddf*a[1]*a[4]);
    r[6] = fmal(df, a[6], ddf*a[2]*a[4]);
    long double mixed_2nd = fmal(a[1], a[6], fmal(a[2], a[5], a[4]*a[3]));
    r[7] = fmal(df, a[7], fmal(ddf, mixed_2nd, dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}

/* Individual wrappers for Agena */
int dual_csch (lua_State *L) {
  return dual_hyperbolic_reciprocal(L, TYPE_CSCH, "csch");
}

int dual_sech (lua_State *L) {
  return dual_hyperbolic_reciprocal(L, TYPE_SECH, "sech");
}

int dual_coth (lua_State *L) {
  return dual_hyperbolic_reciprocal(L, TYPE_COTH, "coth");
}


/* Created by Gemini AI, put to the public domain; 6.6.9 */
int dual_cosc (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, si, co, x2, x3, x4;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.cosc", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Cardinal cosine is undefined at x = 0 */
  if (a[0] == 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  sun_sincosl(a[0], &si, &co);
  x2 = a[0]*a[0];
  x3 = x2*a[0];
  x4 = x3*a[0];
  real = co/a[0];
  /* Safety check for infinity/NaN */
  if (!tools_fpisfinitel(real)) {
    lua_pushundefined(L);
    return 1;
  }
  df   = -(si/a[0]) - (co/x2);
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    ddf = -(co/a[0]) + (2.0L*si/x2) + (2.0L*co/x3);
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf  = -(co/a[0]) + (2.0L*si/x2) + (2.0L*co/x3);
    dddf = (si/a[0]) + (3.0L*co/x2) - (6.0L*si/x3) - (6.0L*co/x4);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define COSC_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = COSC_C2(1, 2, 3);
    r[5] = COSC_C2(1, 4, 5);
    r[6] = COSC_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI, put to the public domain; 6.6.9 */
int dual_tanc (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, x2;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.tanc", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  int nearzero = fabsl(a[0]) < 1.0e-4L;
  if (nearzero) {
    x2 = a[0]*a[0];
    real = 1.0L + x2/3.0L + (2.0L*x2*x2)/15.0L;
    df   = a[0]*(2.0L/3.0L + (8.0L*x2)/15.0L);
    ddf  = 2.0L/3.0L + (8.0L*x2)/5.0L + (34.0L*x2*x2)/21.0L;
    dddf = a[0]*(16.0L/5.0L + (136.0L*x2)/21.0L);
  } else {
    long double t, x3, x4, sec2, ddf_tan;
    t = sun_tanl(a[0]);  /* 6.7.2 fix */
    if (!tools_fpisfinitel(t)) {
      lua_pushundefined(L);
      return 1;
    }
    x2 = a[0]*a[0];
    x3 = x2*a[0];
    x4 = x3*a[0];
    sec2 = fmal(t, t, 1.0L);
    ddf_tan = 2.0L*sec2*t;  /* Second deriv of tan(x) */
    real = t/a[0];
    df   = (sec2/a[0]) - (t/x2);
    ddf  = (ddf_tan/a[0]) - (2.0L*sec2/x2) + (2.0L*t/x3);
    /* Third derivative of tan is 2*sec2*(sec2 + 2*t^2) */
    long double d3_tan = 2.0L*sec2*(sec2 + 2.0L*t*t);
    dddf = (d3_tan/a[0]) - (3.0L*ddf_tan/x2) + (6.0L*sec2/x3) - (6.0L*t/x4);
  }
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define TANC_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = TANC_C2(1, 2, 3);
    r[5] = TANC_C2(1, 4, 5);
    r[6] = TANC_C2(2, 4, 6);
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI, put to the public domain, 6.7.0 */
int dual_sec (lua_State *L) {
  long double a[8], r[8], s_val, c_val;
  long double real, tan, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int i, s = checkdual(L, 1, "dual.sec", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  sun_sincosl(a[0], &s_val, &c_val);
  if (c_val == 0.0L) {
    return luaL_error(L, "domain error: sec(x) at multiple of pi/2");
  }
  real = 1.0L/c_val;
  tan  = s_val/c_val;
  df   = real*tan; /* f' = sec(x)tan(x) */
  for (i=1; i < s; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    ddf = real*(tan*tan + real*real);
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    long double r2 = real*real;
    long double t2 = tan*tan;
    ddf  = real*(t2 + r2);
    dddf = df*(t2 + 5.0L*r2);
    r[1] = df*a[1]; r[2] = df*a[2]; r[4] = df*a[4];
    r[3] = fmal(df, a[3], ddf*a[1]*a[2]);
    r[5] = fmal(df, a[5], ddf*a[1]*a[4]);
    r[6] = fmal(df, a[6], ddf*a[2]*a[4]);
    long double mixed_2nd = fmal(a[1], a[6], fmal(a[2], a[5], a[4]*a[3]));
    r[7] = fmal(df, a[7], fmal(ddf, mixed_2nd, dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI, put to the public domain, 6.7.0 */
int dual_csc (lua_State *L) {
  /* Cosecant function: csc(x) = 1/sin(x) */
  long double a[8], r[8], s_val, c_val;
  long double real, cot, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int i, s = checkdual(L, 1, "dual.csc", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  /* 1. Load the real part and calculate trig components */
  a[0] = agn_reggetinumber(L, 1, 1);
  sun_sincosl(a[0], &s_val, &c_val);
  if (s_val == 0.0L) {
    return luaL_error(L, "domain error: csc(x) at multiple of pi");
  }
  real = 1.0L/s_val;
  cot  = c_val/s_val;
  /* f' = -csc(x)*cot(x) */
  df = -real*cot;
  /* 2. Load all derivative slots into the local array */
  for (i=1; i < s; i++) {
    a[i] = agn_reggetinumber(L, 1, i + 1);
  }
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    /* f'' = csc(x)*(cot^2(x) + csc^2(x)) */
    ddf = real*(cot*cot + real*real);
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    /* s == 8 logic */
    long double r2 = real*real;
    long double c2 = cot*cot;
    /* f'' = real*(c2 + r2) */
    ddf = real*(c2 + r2);
    /* f''' = -csc(x)*cot(x)*(cot^2(x) + 5*csc^2(x))
    *Since df is already (-csc*cot), we multiply by (c2 + 5*r2) directly.
    *REMOVED the extra '-' that was causing the sign flip. */
    dddf = df*(c2 + 5.0L*r2);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots (E12, E13, E23) */
    r[3] = fmal(df, a[3], ddf*a[1]*a[2]);
    r[5] = fmal(df, a[5], ddf*a[1]*a[4]);
    r[6] = fmal(df, a[6], ddf*a[2]*a[4]);
    /* Slot 7: E123 interaction */
    /* mixed_2nd = a1*a6 + a2*a5 + a4*a3 */
    long double mixed_2nd = fmal(a[1], a[6], fmal(a[2], a[5], a[4]*a[3]));
    r[7] = fmal(df, a[7], fmal(ddf, mixed_2nd, dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


int dual_cot (lua_State *L) {
  /* f(x) = cot(x) = cos(x)/sin(x) */
  long double a[8], r[8], s_val, c_val;
  long double real, csc2, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int i, s = checkdual(L, 1, "dual.cot", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  sun_sincosl(a[0], &s_val, &c_val);
  if (s_val == 0.0L) {
    return luaL_error(L, "domain error: cot(x) at multiple of pi");
  }
  real = c_val/s_val;
  csc2 = 1.0L/(s_val*s_val);
  df   = -csc2; /* f' = -csc^2(x) */
  for (i=1; i < s; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    ddf = -2.0L*df*real; /* 2*csc^2*cot */
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    ddf  = -2.0L*df*real;
    /* f''' = -2*csc^2*(2*cot^2 + csc^2) = 2*df*(2*real^2 + csc2) */
    dddf = 2.0L*df*(2.0L*real*real + csc2);
    r[1] = df*a[1]; r[2] = df*a[2]; r[4] = df*a[4];
    r[3] = fmal(df, a[3], ddf*a[1]*a[2]);
    r[5] = fmal(df, a[5], ddf*a[1]*a[4]);
    r[6] = fmal(df, a[6], ddf*a[2]*a[4]);
    long double mixed_2nd = fmal(a[1], a[6], fmal(a[2], a[5], a[4]*a[3]));
    r[7] = fmal(df, a[7], fmal(ddf, mixed_2nd, dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI, put to the public domain, 6.7.0 */
int dual_cas (lua_State *L) {
  /**The cas(x) function: cos(x) + sin(x) = sqrt(2)*sin(x + pi/4)
  *This identity is numerically superior as it uses a single sine call. */
  long double a[8], r[8], real, df, ddf, dddf, x;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.cas", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  x = (long double)agn_reggetinumber(L, 1, 1);
  /* Using the single-term identity for high precision */
  real = M_SQRT2ld*sun_sinl(x + M_PIO4ld);
  /* f'(x) = cos(x) - sin(x) = sqrt(2)*cos(x + pi/4) */
  df = M_SQRT2ld*sun_cosl(x + M_PIO4ld);
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    /* f''(x) = -cas(x) */
    ddf = -real;
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    /* f''(x) = -real, f'''(x) = -df */
    ddf  = -real;
    dddf = -df;
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define CAS_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = CAS_C2(1, 2, 3);
    r[5] = CAS_C2(1, 4, 5);
    r[6] = CAS_C2(2, 4, 6);
    /* Slot 7: E123 interaction */
    long double mixed_2nd = fmal(a[1], a[6], fmal(a[2], a[5], a[3]*a[4]));
    r[7] = fmal(df, a[7], fmal(ddf, mixed_2nd, dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* taken from the dual package for R, https://cran.r-project.org/src/contrib/dual_0.0.3.tar.gz; 2.24.2
   function obj = asinh(a)  # inverse hyperbolic sine
      obj = Dual(asinh(a.x), (1/sqrt((a.x)^2 + 1))*a.d);
   end */
int dual_arcsinh (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, denom, root_denom;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.arcsinh", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  real = sun_asinhl(a[0]);
  /* Pre-calculate denominator base */
  denom = fmal(a[0], a[0], 1.0L);  /* (x^2 + 1) */
  root_denom = safe_sqrtl(denom);  /* sqrt(x^2 + 1) */
  df = 1.0L/root_denom;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    ddf = -a[0]/(denom*root_denom);
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf = -a[0]/(denom*root_denom);
    /* f''' = (2x^2 - 1)/(x^2 + 1)^(5/2) */
    dddf = fmal(2.0L*a[0], a[0], -1.0L)/(denom*denom*root_denom);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define ASINH_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = ASINH_C2(1, 2, 3);
    r[5] = ASINH_C2(1, 4, 5);
    r[6] = ASINH_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* taken from the dual package for R, https://cran.r-project.org/src/contrib/dual_0.0.3.tar.gz; 2.24.2
   function obj = acosh(a)  # inverse hyperbolic cosine
      obj = Dual(acosh(a.x), (1/sqrt((a.x)^2 - 1))*a.d);
   end */
int dual_arccosh (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, denom, root_denom;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.arccosh", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Domain Check: acosh is only defined for x >= 1 */
  if (a[0] <= 1.0L) {
    if (a[0] == 1.0L) {
      /* Derivative is infinite at x = 1 */
      createeightdual(L, 0.0, AGN_NAN, AGN_NAN, AGN_NAN, AGN_NAN, AGN_NAN, AGN_NAN, AGN_NAN);
    } else {
      lua_pushundefined(L);
    }
    return 1;
  }
  real = sun_acoshl(a[0]);
  denom = fmal(a[0], a[0], -1.0L);  /* (x^2 - 1) */
  root_denom = safe_sqrtl(denom);   /* sqrt(x^2 - 1) */
  df = 1.0L/root_denom;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    ddf = -a[0]/(denom*root_denom);
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf = -a[0]/(denom*root_denom);
    /* f''' = (2x^2 + 1)/(x^2 - 1)^(5/2) */
    dddf = fmal(2.0L*a[0], a[0], 1.0L)/(denom*denom*root_denom);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define ACOSH_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = ACOSH_C2(1, 2, 3);
    r[5] = ACOSH_C2(1, 4, 5);
    r[6] = ACOSH_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* taken from the dual package for R, https://cran.r-project.org/src/contrib/dual_0.0.3.tar.gz; 2.24.2
   function obj = atanh(a)  # inverse hyperbolic tangent
      obj = Dual(atanh(a.x), (1/(1 - (a.x)^2))*a.d);
   end */
int dual_arctanh (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, denom, d2;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.arctanh", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Domain Check: atanh is only defined for |x| < 1 */
  if (fabsl(a[0]) >= 1.0L) {
    lua_pushundefined(L);
    return 1;
  }
  real = sun_atanhl(a[0]);
  denom = fmal(-a[0], a[0], 1.0L);  /* (1 - x^2) */
  df = 1.0L/denom;                  /* 1/(1 - x^2) */
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    d2 = denom*denom;
    ddf = (2.0L*a[0])/d2;
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    d2 = denom*denom;
    ddf = (2.0L*a[0])/d2;
    /* f''' = (6x^2 + 2)/(1 - x^2)^3 */
    dddf = fmal(6.0L*a[0], a[0], 2.0L)/(d2*denom);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define ATANH_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = ATANH_C2(1, 2, 3);
    r[5] = ATANH_C2(1, 4, 5);
    r[6] = ATANH_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* taken from the dual package for R, https://cran.r-project.org/src/contrib/dual_0.0.3.tar.gz; 2.24.2
   ifu <- 1.0/x@f/log(2)
   ans <- dual(f = log2(x@f), grad = x@grad*ifu) */
int dual_log2 (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, inv_x;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.log2", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Domain Check: Logarithm is only defined for x > 0 */
  if (a[0] <= 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  real = tools_log2l(a[0]);
  inv_x = 1.0L/a[0];
  /* df = 1/(x*ln(2)) */
  df = inv_x/M_LN2ld;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, df*a[1]);
    return 1;
  }
  a[2] = agn_reggetinumber(L, 1, 3);
  a[3] = agn_reggetinumber(L, 1, 4);
  if (s == 4) {
    ddf = -df*inv_x;  /* -1/(x^2*ln(2)) */
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf = -df*inv_x;
    /* f''' = 2/(x^3*ln(2)) = -2*ddf/x */
    dddf = -2.0L*ddf*inv_x;
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define LOG2_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = LOG2_C2(1, 2, 3);
    r[5] = LOG2_C2(1, 4, 5);
    r[6] = LOG2_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123)
       Rule: f'a7 + f''(a1*a6 + a2*a5 + a3*a4) + f'''(a1*a2*a4) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* taken from the dual package for R, https://cran.r-project.org/src/contrib/dual_0.0.3.tar.gz; 2.24.2
   ifu <- 1.0/x@f/log(10)
   ans <- dual(f = log10(x@f), grad = x@grad*ifu) */
int dual_log10 (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, inv_x;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.log10", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Domain Check: Logarithm is only defined for x > 0 */
  if (a[0] <= 0.0L) {
    lua_pushundefined(L);
    return 1;
  }
  real = tools_log10l(a[0]);
  inv_x = 1.0L/a[0];
  /* df = 1/(x*ln(10)) */
  df = inv_x/M_LN10ld;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, df*a[1]);
    return 1;
  }
  a[2] = agn_reggetinumber(L, 1, 3);
  a[3] = agn_reggetinumber(L, 1, 4);
  if (s == 4) {
    ddf = -df*inv_x;  /* -1/(x^2*ln(10)) */
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf = -df*inv_x;
    /* f''' = 2/(x^3*ln(10)) = -2*ddf/x */
    dddf = -2.0L*ddf*inv_x;
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define LOG10_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = LOG10_C2(1, 2, 3);
    r[5] = LOG10_C2(1, 4, 5);
    r[6] = LOG10_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* taken from the dual package for R, https://cran.r-project.org/src/contrib/dual_0.0.3.tar.gz; 2.24.2
   ifu <- 1.0/(1.0 + x@f)
   ans <- dual(f = log1p(x@f), grad = x@grad*ifu) */
int dual_lnp1 (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, denom;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.lnp1", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Domain Check: ln(1+x) is defined for x > -1 */
  if (a[0] <= -1.0L) {
    lua_pushundefined(L);
    return 1;
  }
  real = tools_log1pl(a[0]);
  denom = 1.0L + a[0];
  df = 1.0L/denom;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    ddf = -(df*df);
    createhyperdual(L, real, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf = -(df*df);
    /* f''' = 2/(1+x)^3 = -2*ddf*df */
    dddf = -2.0L*ddf*df;
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define LNP1_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = LNP1_C2(1, 2, 3);
    r[5] = LNP1_C2(1, 4, 5);
    r[6] = LNP1_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


int dual_exp2 (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.exp2", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  real = tools_exp2l(a[0]);
  /* Each order of derivative adds a factor of ln(2) */
  df   = real*M_LN2ld;
  ddf  = df  *M_LN2ld;
  dddf = ddf *M_LN2ld;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    /* 1st order slots */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots */
    #define EXP2_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = EXP2_C2(1, 2, 3);
    r[5] = EXP2_C2(1, 4, 5);
    r[6] = EXP2_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123)
       Rule: f'a7 + f''(a1a6 + a2a5 + a3a4) + f'''(a1a2a4) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


int dual_exp10 (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.exp10", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  real = tools_exp10l(a[0]);
  /* Recursive derivative coefficients using ln(10) */
  df   = real*M_LN10ld;
  ddf  = df  *M_LN10ld;
  dddf = ddf *M_LN10ld;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    /* 1st order slots (E1, E2, E3) */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order slots (Mixed interactions) */
    #define EXP10_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = EXP10_C2(1, 2, 3);
    r[5] = EXP10_C2(1, 4, 5);
    r[6] = EXP10_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123)
       Rule: f'a7 + f''(a1a6 + a2a5 + a3a4) + f'''(a1a2a4) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* taken from the dual package for R, https://cran.r-project.org/src/contrib/dual_0.0.3.tar.gz; 2.24.2
   tmp <- exp(x@f)
   ans <- dual(f = expm1(x@f), grad = x@grad*tmp) */
int dual_expm1 (lua_State *L) {
  long double a[8], r[8], real, df;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.expm1", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* High precision value for small x */
  real = tools_expm1l(a[0]);
  /* Derivatives are all e^x. Using the stable (real + 1.0) approach. */
  df = real + 1.0L;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    /* df = ddf = e^x */
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], df*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    /* Since f' = f'' = f''' = df, the logic simplifies significantly */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    /* 2nd order interaction slots */
    #define EXPM1_C2(ia, ib, im) fmal(df, a[im], df*a[ia]*a[ib])
    r[3] = EXPM1_C2(1, 2, 3);
    r[5] = EXPM1_C2(1, 4, 5);
    r[6] = EXPM1_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123)
       Rule: df*(a7 + (a1a6 + a2a5 + a3a4) + (a1a2a4)) */
    r[7] = fmal(df, a[7], fmal(df, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), df*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* computes exp(x^2) if sign (2nd arg) is positive and exp(-x^2) if sign is negative, 6.6.8,
   created by Gemini AI, put to the public domain. */
int dual_expx2 (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, x2;
  int toconstant = agnL_optboolean(L, 3, 0);
  int sign, s = checkdual(L, 1, "dual.expx2", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  sign = (int)luaL_checkinteger(L, 2);  /* -1 for exp(-x^2), 1 for exp(x^2) */
  real = expx2l(a[0], sign);
  /* Handle edge cases */
  if (real == 0.0L || !tools_fpisfinitel(real)) {
    if (real == 0.0L) {
      pushzero(L, 8);
    } else {
      lua_pushnumber(L, real);  /* Return Inf/NaN */
    }
    return 1;
  }
  x2 = a[0]*a[0];
  long double s_val = (sign < 0) ? -1.0L : 1.0L;
  /* Derivative coefficients scaled by 'real' */
  df   = (2.0L*s_val*a[0])*real;
  ddf  = fmal(4.0L*x2, 1.0L, 2.0L*s_val)*real;
  /* f''' = (8*s*x^3 + 12*x)*real */
  dddf = (8.0L*s_val*x2*a[0] + 12.0L*a[0])*real;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define EXPX2_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = EXPX2_C2(1, 2, 3);
    r[5] = EXPX2_C2(1, 4, 5);
    r[6] = EXPX2_C2(2, 4, 6);
    /* 3rd order slot (Index 7: E123) */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI, put to the public domain; 6.6.7 & 6.6.8, fully patched 6.7.5.
   Full arccoth implementation with 8-slot composite chain rule support */
int dual_arccoth (lua_State *L) {
  long double a[8] = {0}, r[8] = {0};
  long double x1, x2, denom, den2, den3, real, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int i, s = checkdual(L, 1, "dual.arccoth", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  for (i = 0; i < s; i++) {
    a[i] = agn_reggetinumber(L, 1, i + 1);
  }
  x1 = a[0];
  if (fabsl(x1) <= 1.0L) {
    lua_pushundefined(L);
    return 1;
  }
  /* Core Math: Derivatives */
  /* f(x) = 1/2*ln((x+1)/(x-1)) */
  real = 0.5L*logl((x1 + 1.0L) / (x1 - 1.0L));
  x2 = x1*x1;
  denom = x2 - 1.0L;
  den2 = denom*denom;
  den3 = den2*denom;
  /* f' = 1 / (1 - x^2) */
  df = -1.0L / denom;
  if (s == 2) {
    createdual(L, real, df*a[1]);
    return 1;
  }
  /* f'' = 2x / (x^2 - 1)^2 */
  ddf = (2.0L*x1)/den2;
  /* f''' = -(6x^2 + 2) / (x^2 - 1)^3 */
  if (s == 4) {
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[3] = fmal(ddf, a[1]*a[2], df*a[3]);
    createhyperdual(L, real, r[1], r[2], r[3]);
  } else {
      dddf = -(6.0L*x2 + 2.0L)/den3;
    /* Full 8-slot Tri-Dual logic */
    r[1] = df*a[1]; /* E1 */
    r[2] = df*a[2]; /* E2 */
    r[4] = df*a[4]; /* E3 */
    /* Second-order interactions */
    r[3] = fmal(ddf, a[1]*a[2], df*a[3]); /* E12 */
    r[5] = fmal(ddf, a[1]*a[4], df*a[5]); /* E13 */
    r[6] = fmal(ddf, a[2]*a[4], df*a[6]); /* E23 */
    /* Third-order interaction (Slot 7 / E123) */
    /* Captures the 'Inner Curvature' of composite calls like arccoth(1/x) */
    long double mixed_inner = fmal(a[1], a[6], fmal(a[2], a[5], a[4]*a[3]));
    long double cubic_inner = a[1]*a[2]*a[4];
    r[7] = fmal(df, a[7], fmal(ddf, mixed_inner, dddf*cubic_inner));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


int dual_erf (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, ex2;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.erf", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  real = sun_erfl(a[0]);
  /* Use the robust exp(-x^2) helper */
  ex2 = expx2l(a[0], -1);
  /* df = (2/sqrt(pi))*exp(-x^2) */
  df = M_2OSQRTPIld*ex2;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    ddf = -2.0L*a[0]*df;
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf = -2.0L*a[0]*df;
    /* f''' = (4x^2 - 2)*df */
    dddf = fmal(4.0L*a[0], a[0], -2.0L)*df;
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define ERF_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = ERF_C2(1, 2, 3);
    r[5] = ERF_C2(1, 4, 5);
    r[6] = ERF_C2(2, 4, 6);
    /* Slot 7: E123 interaction */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


int dual_erfc (lua_State *L) {
  long double a[8], r[8], real, df, ddf, dddf, ex2;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.erfc", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* erfc(x) is critical for high precision when x is large */
  real = sun_erfcl(a[0]);
  /* Use the robust exp(-x^2) helper */
  ex2 = expx2l(a[0], -1);
  /* df = -(2/sqrt(pi))*exp(-x^2) */
  df = -M_2OSQRTPIld*ex2;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    /* ddf = -2x*df. Since df is negative, ddf becomes positive for x > 0 */
    ddf = -2.0L*a[0]*df;
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf = -2.0L*a[0]*df;
    /* f''' = (4x^2 - 2)*df */
    dddf = fmal(4.0L*a[0], a[0], -2.0L)*df;
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define ERFC_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = ERFC_C2(1, 2, 3);
    r[5] = ERFC_C2(1, 4, 5);
    r[6] = ERFC_C2(2, 4, 6);
    /* Slot 7: E123 interaction */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI; 6.7.0 */
int dual_erfcx (lua_State *L) {
  /* Indices: 0=Val, 1=E1, 2=E2, 3=E12, 4=E3, 5=E13, 6=E23, 7=E123 */
  long double a[8], r[8], real, df, ddf, dddf, x;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.erfcx", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  x = (long double)agn_reggetinumber(L, 1, 1);
  a[0] = x;
  /* Base function: erfcx(x) */
  real = tools_erfcxl(x);
  /* df = 2x*f(x) - 2/sqrt(pi) */
  df = fmal(2.0L*x, real, -M_2OSQRTPIld);
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    /* ddf = 2*f + 2x*df */
    ddf = 2.0L*real + 2.0L*x*df;
    createhyperdual(L, real, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    /* 8-SLOT TRI-DUAL EXTENSION */
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    /* Recursive derivatives:
       f'   = 2x*f - C
       f''  = 2f + 2x*f'
       f''' = 4f' + 2x*f'' */
    ddf  = 2.0L*real + 2.0L*x*df;
    dddf = 4.0L*df + 2.0L*x*ddf;
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define ERFCX_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = ERFCX_C2(1, 2, 3);
    r[5] = ERFCX_C2(1, 4, 5);
    r[6] = ERFCX_C2(2, 4, 6);
    /* Slot 7: E123 interaction
       f'a7 + f''(a1a6 + a2a5 + a3a4) + f'''(a1a2a4) */
    long double mixed_2nd = fmal(a[1], a[6], fmal(a[2], a[5], a[3]*a[4]));
    r[7] = fmal(df, a[7], fmal(ddf, mixed_2nd, dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI; 6.6.13 */
int dual_inverf (lua_State *L) {
  long double a[8], r[8], real_y, df, ddf, dddf, df2, ex2;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.inverf", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Domain Check: erf is defined on (-1, 1) */
  if (fabsl(a[0]) >= 1.0L) {
    lua_pushundefined(L);
    return 1;
  }
  real_y = tools_inverfl(a[0]);
  /* df = (sqrt(pi)/2)*exp(y^2) */
  ex2 = expx2l(real_y, 1);  /* Use sign=1 for positive exponent */
  df = M_SQRTPI1_2ld*ex2;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real_y, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    df2 = df*df;
    ddf = 2.0L*real_y*df2;
    createhyperdual(L, real_y, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    df2 = df*df;
    ddf = 2.0L*real_y*df2;
    /* f''' = 2*df^2*(1 + 4*y^2*df) */
    dddf = 2.0L*df2*fmal(4.0L*real_y*real_y, df, 1.0L);
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define INVERF_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = INVERF_C2(1, 2, 3);
    r[5] = INVERF_C2(1, 4, 5);
    r[6] = INVERF_C2(2, 4, 6);
    /* Slot 7: E123 interaction */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real_y, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI; 6.6.13 */
int dual_inverfc (lua_State *L) {
  long double a[8], r[8], real_y, df, ddf, dddf, df2, ex2;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.inverfc", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Domain Check: erfc is defined on (0, 2) */
  if (a[0] <= 0.0L || a[0] >= 2.0L) {
    lua_pushundefined(L);
    return 1;
  }
  real_y = tools_inverfcl(a[0]);
  /* df = -(sqrt(pi)/2)*exp(y^2) */
  ex2 = expx2l(real_y, 1);
  df = -M_SQRTPI1_2ld*ex2;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, real_y, a[1]*df);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    df2 = df*df;
    ddf = 2.0L*real_y*df2;
    createhyperdual(L, real_y, a[1]*df, a[2]*df, fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    df2 = df*df;
    ddf = 2.0L*real_y*df2;
    /* f''' = 2*df^2*(1 + 4*y^2*df)
       Note: df is negative here, so the term (1 + 4y^2*df) can cross zero. */
    dddf = 2.0L*df2*fmal(4.0L*real_y*real_y, df, 1.0L);
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define INVERFC_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = INVERFC_C2(1, 2, 3);
    r[5] = INVERFC_C2(1, 4, 5);
    r[6] = INVERFC_C2(2, 4, 6);
    /* Slot 7: E123 interaction */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, real_y, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI; Function for f(a) = Gamma(a), 6.6.11 */
int dual_gamma (lua_State *L) {
  long double a[8], r[8], g, psi, psi1, psi2, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.gamma", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Domain check: Gamma has poles at 0, -1, -2... */
  if (a[0] <= 0.0L && floorl(a[0]) == a[0]) {
    lua_pushundefined(L);
    return 1;
  }
  /* Scalar result */
  g = tools_gammal(a[0]);
  psi = tools_psil(a[0]);  /* Digamma */
  df = g*psi;
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, g, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    psi1 = tools_trigammal(a[0]);  /* Trigamma */
    ddf = g*fmal(psi, psi, psi1);
    createhyperdual(L, g, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    psi1 = tools_trigammal(a[0]);
    psi2 = tools_tetragammal(a[0]);  /* Tetragamma (3rd polygamma) */
    ddf = g*fmal(psi, psi, psi1);
    /* f''' = g*(psi^3 + 3*psi*psi1 + psi2) */
    long double p_sq = psi*psi;
    dddf = g*(p_sq*psi + 3.0L*psi*psi1 + psi2);
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define GAMMA_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = GAMMA_C2(1, 2, 3);
    r[5] = GAMMA_C2(1, 4, 5);
    r[6] = GAMMA_C2(2, 4, 6);
    /* Slot 7: E123 interaction */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + a[2]*a[5] + a[3]*a[4]), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, g, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI; Function for f(a) = ln(Gamma(a)) */
static int mt_lngamma (lua_State *L) {
  long double a[8], r[8], lg, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.__lngamma", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Domain check: Undefined at 0, -1, -2... */
  if (a[0] <= 0.0L && floorl(a[0]) == a[0]) {
    lua_pushundefined(L);
    return 1;
  }
  /* Scalar result and direct derivatives from Polygamma functions */
  lg   = tools_lgammal(a[0]);
  df   = tools_psil(a[0]);  /* Digamma */
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, lg, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);

    ddf = tools_trigammal(a[0]);  /* Trigamma */
    createhyperdual(L, lg, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf  = tools_trigammal(a[0]);
    dddf = tools_tetragammal(a[0]);  /* Tetragamma */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define LNGAMMA_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = LNGAMMA_C2(1, 2, 3);
    r[5] = LNGAMMA_C2(1, 4, 5);
    r[6] = LNGAMMA_C2(2, 4, 6);
    /* Slot 7: E123 interaction */
    r[7] = fmal(df, a[7], fmal(ddf, (a[1]*a[6] + fmal(a[2], a[5], a[3]*a[4])), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, lg, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Created by Gemini AI; Function for f(a) = Digamma(a), 6.6.11 */
int dual_psi (lua_State *L) {
  long double a[8], r[8], val, df, ddf, dddf;
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.psi", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  a[0] = agn_reggetinumber(L, 1, 1);
  /* Domain check: Digamma has poles at 0, -1, -2... */
  if (a[0] <= 0.0L && floorl(a[0]) == a[0]) {
    lua_pushundefined(L);
    return 1;
  }
  /* Scalar result and derivatives mapping directly to Polygamma functions */
  val = tools_psil(a[0]);       /* Digamma */
  df  = tools_trigammal(a[0]);  /* Trigamma */
  a[1] = agn_reggetinumber(L, 1, 2);
  if (s == 2) {
    createdual(L, val, df*a[1]);
  } else if (s == 4) {
    a[2] = agn_reggetinumber(L, 1, 3);
    a[3] = agn_reggetinumber(L, 1, 4);
    ddf = tools_tetragammal(a[0]);  /* Tetragamma */
    createhyperdual(L, val, df*a[1], df*a[2], fmal(df, a[3], ddf*a[1]*a[2]));
  } else {
    int i;
    for (i=2; i < 8; i++) a[i] = agn_reggetinumber(L, 1, i + 1);
    ddf  = tools_tetragammal(a[0]);
    dddf = tools_pentagammal(a[0]);  /* Pentagamma */
    r[1] = df*a[1];
    r[2] = df*a[2];
    r[4] = df*a[4];
    #define PSI_C2(ia, ib, im) fmal(df, a[im], ddf*a[ia]*a[ib])
    r[3] = PSI_C2(1, 2, 3);
    r[5] = PSI_C2(1, 4, 5);
    r[6] = PSI_C2(2, 4, 6);
    /* Slot 7: E123 interaction */
    r[7] = fmal(df, a[7], fmal(ddf, fmal(a[1], a[6], fmal(a[2], a[5], a[3]*a[4])), dddf*(a[1]*a[2]*a[4])));
    createeightdual(L, val, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}


/* Smooth Heaviside function generated by Gemini AI, put to the public domain. 6.7.8 UNDOC */
static int dual_heaviside (lua_State *L) {
  long double a1, a2, a3, a4, a5, a6, a7, a8, val;
  /* Steepness factor k: higher is sharper.
     Usually k=20 to 100 is good for approximations. */
  const long double k = (long double)agnL_optposint(L, 2, 20);
  int toconstant = agnL_optboolean(L, 2, 0);
  int s = checkdual(L, 1, "dual.heaviside", toconstant);
  if (s == 1) { lua_settop(L, 1); return 1; }
  a1 = agn_reggetinumber(L, 1, 1);
  a2 = agn_reggetinumber(L, 1, 2);
  /* The Smooth Heaviside: val = 1 / (1 + exp(-k*a1)) */
  val = 1.0L/(1.0L + tools_expl(-k*a1));
  /* Precompute derivative factors to save cycles */
  long double d1_factor = k*val*(1.0L - val);
  if (s == 2) {
    createdual(L, val, d1_factor*a2);
  } else if (s == 4) {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    /* Second derivative of logistic: f'' = k^2*f*(1-f)*(1-2f) */
    long double d2_factor = d1_factor*k*(1.0L - 2.0L*val);
    createhyperdual(L, val,
      d1_factor*a2,
      d1_factor*a3,
      d2_factor*a2*a3 + d1_factor*a4);
  } else {
    a3 = agn_reggetinumber(L, 1, 3);
    a4 = agn_reggetinumber(L, 1, 4);
    a5 = agn_reggetinumber(L, 1, 5);
    a6 = agn_reggetinumber(L, 1, 6);
    a7 = agn_reggetinumber(L, 1, 7);
    a8 = agn_reggetinumber(L, 1, 8);
    /* For the 8-dual version, we chain the derivative components.
       Note: This uses the chain rule across the slots. */
    long double df  = d1_factor;
    long double d2f = df*k*(1.0L - 2.0L*val);
    createeightdual(L, val,
      df*a2,
      df*a3,
      fmal(d2f, a2*a3, df*a4),
      df*a5,
      fmal(d2f, a2*a5, df*a6),
      fmal(d2f, a3*a5, df*a7),
      fmal(d2f, a4*a5, df*a8));
  }
  return 1;
}


static int dual_new (lua_State *L) {
  int nargs = lua_gettop(L);
  if (nargs == 1) {  /* get dual part */
    checkdual(L, 1, "dual.new", 1);
    agn_regrawgeti(L, 1, 2);
  } else if (nargs == 2) {
    lua_Number x, y;
    x = agn_checknumber(L, 1);
    y = agn_checknumber(L, 2);
    if (tools_isfinite(x) && tools_isfinite(y)) {
      createdual(L, x, y);
    } else
      lua_pushnumber(L, isnan(x) ? x : (isnan(y) ? y : x));
  } else if (nargs > 2 && nargs < 5) {
    lua_Number a, b, c, d;
    a = agn_checknumber(L, 1);
    b = agn_checknumber(L, 2);
    c = agnL_optnumber(L, 3, 0);
    d = agnL_optnumber(L, 4, 0);
    if (tools_isfinite(a) && tools_isfinite(b) && tools_isfinite(c) && tools_isfinite(d)) {
      createhyperdual(L, a, b, c, d);
    } else {
      lua_pushundefined(L);
    }
  } else if (nargs > 4) {
    lua_Number a, b, c, d, e, f, g, h;
    a = agn_checknumber(L, 1);
    b = agn_checknumber(L, 2);
    c = agnL_optnumber(L, 3, 0);
    d = agnL_optnumber(L, 4, 0);
    e = agnL_optnumber(L, 5, 0);
    f = agnL_optnumber(L, 6, 0);
    g = agnL_optnumber(L, 7, 0);
    h = agnL_optnumber(L, 8, 0);
    if (tools_isfinite(a) && tools_isfinite(b) && tools_isfinite(c) && tools_isfinite(d) &&
        tools_isfinite(e) && tools_isfinite(f) && tools_isfinite(g) && tools_isfinite(h)) {
      createeightdual(L, a, b, c, d, e, f, g, h);
    } else {
      lua_pushundefined(L);
    }
  } else
    luaL_error(L, "Error in " LUA_QS ": expected one or two arguments.", "dual.new");
  return 1;
}


static int generate (lua_State *L) {
  int i, s, error, nargs;
  long double x[8] = {0.0L}, f, df, ddf, dddf;  /* 6.6.13 tweak, use stack-allocated buffer */
  const char *procname;
  (void)error;
  nargs = lua_gettop(L);
  /* When called from a metamethod, there might be a null at least at stack index 2, so ignore them */
  for (i=nargs; i > 1; i--) {
    if (lua_isnil(L, i)) nargs--;
  }
  procname = agn_tostring(L, lua_upvalueindex(5));
  s = checkdual(L, 1, procname, 1);
  if (s > 8) s = 8;  /* Better be sure than sorry once the code will be modified. */
  if (s == 1) { lua_settop(L, 1); return 1; }  /* 6.7.4 fix */
  for (i=0; i < s; i++) {
    /* agn_reggetinumber returns 0 with non-numbers */
    x[i] = (long double)agn_reggetinumber(L, 1, i + 1);
  }
  f = agnL_fncallx(L, lua_upvalueindex(1), x[0], 2, nargs, &error, 0);
  df = agnL_fncallx(L, lua_upvalueindex(2), x[0], 2, nargs, &error, 0);
  if (s == 2) {
    createdual(L, f, df*x[1]);
  } else if (s == 4) {
    ddf = agnL_fncallx(L, lua_upvalueindex(3), x[0], 2, nargs, &error, 0);
    createhyperdual(L, f, df*x[1], df*x[2], fmal(ddf*x[1], x[2], df*x[3]));  /* 6.6.13 tweak */
  } else {
    long double mixed_2nd, r[8];
    ddf  = agnL_fncallx(L, lua_upvalueindex(3), x[0], 2, nargs, &error, 0);
    dddf = agnL_fncallx(L, lua_upvalueindex(4), x[0], 2, nargs, &error, 0);
    r[1] = df*x[1];
    r[2] = df*x[2];
    r[4] = df*x[4];
    /* 2nd order slots (E12, E13, E23) */
    r[3] = fmal(df, x[3], ddf*x[1]*x[2]);
    r[5] = fmal(df, x[5], ddf*x[1]*x[4]);
    r[6] = fmal(df, x[6], ddf*x[2]*x[4]);
    /* 3rd order slot (E123)
       Formula: f'x7 + f''(x1x6 + x2x5 + x3x4) + f'''(x1x2x4) */
    mixed_2nd = fmal(x[1], x[6], fmal(x[2],x[5], x[3]*x[4]));
    r[7] = fmal(df, x[7], fmal(ddf, mixed_2nd, dddf*(x[1]*x[2]*x[4])));
    createeightdual(L, f, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  }
  return 1;
}

/* Maple V Release 4:
> restart; Digits := 20:
> # Maple arctan(y, x) is equivalent to C atan2(y, x)
> f := (x, y) -> Beta(x, y):
> # Seed x as (x1, 1, 0, 0, 0, 0, 0, 0) -> Pure E1
> # Seed y as (y1, 0, 1, 0, 1, 0, 0, 0) -> Pure E2 + E3
> vals := [
>   x1=1.5, x2=1, x3=1, x4=0, x5=1, x6=0, x7=0, x8=0,
>   y1=2.0, y2=1, y3=1, y4=0, y5=1, y6=0, y7=0, y8=0
> ]:
> # 1st Order
fx := diff(f(x,y), x);
fy := diff(f(x,y), y);
> # 2nd Order
fxx := diff(f(x,y), x, x);
fyy := diff(f(x,y), y, y);
fxy := diff(f(x,y), x, y);
# 3rd Order
fxxx := diff(f(x,y), x$3);
fyyy := diff(f(x,y), y$3);
fxxy := diff(f(x,y), x, x, y);
fxyy := diff(f(x,y), x, y, y);
S1 := f(x, y):
S2 := fx*x2 + fy*y2:
S3 := fx*x3 + fy*y3:
S4 := (fx*x4 + fy*y4) + (fxx*x2*x3 + fyy*y2*y3 + fxy*(x2*y3 + x3*y2)):
S5 := fx*x5 + fy*y5:
S6 := (fx*x6 + fy*y6) + (fxx*x2*x5 + fyy*y2*y5 + fxy*(x2*y5 + x5*y2)):
S7 := (fx*x7 + fy*y7) + (fxx*x3*x5 + fyy*y3*y5 + fxy*(x3*y5 + y3*x5)):
S8 := (fx*x8 + fy*y8) +
      fxx*(x2*x7 + x3*x6 + x5*x4) + fyy*(y2*y7 + y3*y6 + y5*y4) +
      fxy*(x2*y7 + y2*x7 + x3*y6 + y3*x6 + x5*y4 + y5*x4) +
      fxxx*x2*x3*x5 + fyyy*y2*y3*y5 +
      fxxy*(x2*x3*y5 + x2*x5*y3 + x3*x5*y2) +
      fxyy*(x2*y3*y5 + x3*y2*y5 + x5*y2*y3):
results := evalf(subs({x=x1, y=y1}, vals, [S1, S2, S3, S4, S5, S6, S7, S8])):
for i from 1 to 8 do printf(`Slot %d: %a\n`, i, results[i]) od;
*/
static int generate2 (lua_State *L) {
  const char *procname;
  int i, error, nargs;
  /* Two sets of slots: one for each input dual number */
  long double y[8] = {0.0L}, x[8] = {0.0L}, r[8];
  long double f, fy, fx, fyy, fxx, fxy, fyyy, fxxx, fxxy, fxyy;
  /* 1. Check duals and extract slots */
  /* Assuming upvalue 11 is the procname for generate2 */
  nargs = lua_gettop(L);
  procname = agn_tostring(L, lua_upvalueindex(11));
  int sy = checkdual(L, 1, procname, 0);
  int sx = checkdual(L, 2, procname, 0);
  if (sx != 8 || sx != sy) {
    luaL_error(L, "Error in " LUA_QS ": expected two third-order dual numbers.", procname);
  }
  for (i = 0; i < 8; i++) {
    y[i] = (long double)agn_reggetinumber(L, 1, i + 1);
    x[i] = (long double)agn_reggetinumber(L, 2, i + 1);
  }
  /* 2. Call all partial derivative functions (Upvalues 1-10) */
  /* Note: Passing both y[0] and x[0] to Lua functions */
  lua_pushnumber(L, x[0]);
  lua_replace(L, 2);
  /*
  Index	Symbol	Description	Interaction Category
  1	f	Primary function value	Real Part (r0?)
  2	fy?	First partial w.r.t. y	Gradient (r1?,r2?,r4?)
  3	fx?	First partial w.r.t. x	Gradient (r1?,r2?,r4?)
  4	fyy?	Second pure partial w.r.t. y	Hessian / 2nd Order (r3?,r5?,r6?)
  5	fxx?	Second pure partial w.r.t. x	Hessian / 2nd Order (r3?,r5?,r6?)
  6	fxy?	Second mixed partial	Hessian / 2nd Order (r3?,r5?,r6?)
  7	fyyy?	Third pure partial w.r.t. y	3rd Order Interaction (r7?)
  8	fxxx?	Third pure partial w.r.t. x	3rd Order Interaction (r7?)
  9	fxyy?	Mixed: ?x?y2?3f?	3rd Order Interaction (r7?)
  10	fxxy?	Mixed: ?x2?y?3f?	3rd Order Interaction (r7?) */
  f    = agnL_fncallx(L, lua_upvalueindex(1),  y[0], 2, nargs, &error, 0);
  fx   = agnL_fncallx(L, lua_upvalueindex(2),  y[0], 2, nargs, &error, 0);
  fy   = agnL_fncallx(L, lua_upvalueindex(3),  y[0], 2, nargs, &error, 0);
  fxx  = agnL_fncallx(L, lua_upvalueindex(4),  y[0], 2, nargs, &error, 0);
  fyy  = agnL_fncallx(L, lua_upvalueindex(5),  y[0], 2, nargs, &error, 0);
  fxy  = agnL_fncallx(L, lua_upvalueindex(6),  y[0], 2, nargs, &error, 0);
  fxxx = agnL_fncallx(L, lua_upvalueindex(7),  y[0], 2, nargs, &error, 0);
  fyyy = agnL_fncallx(L, lua_upvalueindex(8),  y[0], 2, nargs, &error, 0);
  fxxy = agnL_fncallx(L, lua_upvalueindex(9),  y[0], 2, nargs, &error, 0);
  fxyy = agnL_fncallx(L, lua_upvalueindex(10), y[0], 2, nargs, &error, 0);
  /* 2. Slot 2 & 3 (Maple S2, S3) */
  r[1] = fmal(fx, x[1], fy*y[1]);
  r[2] = fmal(fx, x[2], fy*y[2]);
  /* 4. Slot 5 (Maple S5) */
  r[4] = fmal(fx, x[4], fy*y[4]);
  /* 3. Slot 4 (Maple S4) */
  /* Matches Maple: (fx*xm + fy*ym) + (fxx*xa*xb + fyy*ya*yb + fxy*(xa*yb + xb*ya)) */
  #define MAPLE_S2(im, ia, ib) (fmal(fx, x[im], fy*y[im]) + \
            fmal(fxx, x[ia]*x[ib], \
            fmal(fyy, y[ia]*y[ib], \
            fxy*fmal(x[ia], y[ib], x[ib]*y[ia]))) \
            )
  r[3] = MAPLE_S2(3, 1, 2); /* Maple S4: x4, x2, x3 */
  r[5] = MAPLE_S2(5, 1, 4); /* Maple S6: x6, x2, x5 */
  r[6] = MAPLE_S2(6, 2, 4); /* Maple S7: x7, x3, x5 */
  /* 7. Slot 8 (Maple S8) - Literal Translation */
  /* Line 1: (fx*x8 + fy*y8) */
  long double s8_line1 = fmal(fx, x[7], fy*y[7]);
  /* Line 2 & 3: fxx*(...) + fyy*(...) + fxy*(...) */
  long double s8_line2 =
    fmal(fxx, (fmal(x[1], x[6], fmal(x[2], x[5], x[4]*x[3]))),
    fmal(fyy, (fmal(y[1], y[6], fmal(y[2], y[5], y[4]*y[3]))),
    fmal(fxy, (fmal(x[1], y[6], fmal(y[1], x[6], fmal(x[2], y[5], fmal(y[2], x[5], fmal(x[4], y[3], y[4]*x[3])))))), 0.0L)));
  /* Line 4 & 5: fxxx*... + fyyy*... + fxxy*... + fxyy*... */
  long double s8_line3 = fmal(fxxx, x[1]*x[2]*x[4],
                         fmal(fyyy, y[1]*y[2]*y[4],
                         fmal(fxxy, (fmal(x[1], x[2]*y[4], fmal(x[1], x[4]*y[2], x[2]*x[4]*y[1]))),
                         fxyy*(fmal(x[1], y[2]*y[4], fmal(x[2], y[1]*y[4], x[4]*y[1]*y[2]))))));
  r[7] = s8_line1 + s8_line2 + s8_line3;
  /* S8 Formula from Maple Script */
  createeightdual(L, f, r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
  return 1;
}

static int dual_generate (lua_State *L) {
  int i, nargs;
  const char *procname;
  nargs = lua_gettop(L);
  if (nargs != 5 && nargs != 11) {
    luaL_error(L, "Error in " LUA_QS ": expected five or eleven arguments.", "dual.generate");
  }
  luaL_checkstack(L, nargs, "not enough stack space");
  for (i=1; i < nargs; i++) {
    luaL_checktype(L, i, LUA_TFUNCTION);
  }
  procname = agnL_optstring(L, nargs, "?");
  for (i=1; i < nargs; i++) {
    lua_pushvalue(L, i);
  }
  lua_pushstring(L, procname);
  lua_pushcclosure(L, nargs == 5 ? &generate : &generate2, nargs);
  return 1;
}


static int dual_isdual (lua_State *L) {  /* 6.6.8 */
  int i, r = 1, nargs = lua_gettop(L);
  for (i=0; i < nargs && r; i++) {
    r = isdual(L, i + 1);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int dual_ishyper (lua_State *L) {  /* 6.6.8 */
  int i, r = 1, nargs = lua_gettop(L);
  for (i=0; i < nargs && r; i++) {
    r = ishyperdual(L, i + 1);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int dual_iseight (lua_State *L) {  /* 6.7.0 */
  int i, r = 1, nargs = lua_gettop(L);
  for (i=0; i < nargs && r; i++) {
    r = iseightdual(L, i + 1);
  }
  lua_pushboolean(L, r);
  return 1;
}


static const struct luaL_Reg dual_metalib [] = {  /* metamethods for numeric userdata `n` */
  {"__unm",       mt_unm},  /* unary minus */
  {"__aeq",       mt_aeq},  /* approximate equality mt */
  {"__eq",        mt_eeq},  /* equality mt */
  {"__eeq",       mt_eeq},  /* strict equality mt */
  {"__lt",        mt_lt},   /* less-than mt */
  {"__le",        mt_le},   /* less-or-equal mt */
  {"__abs",       mt_abs},
  {"__sign",      mt_sign},
  {"__add",       mt_add},
  {"__sub",       mt_sub},
  {"__mul",       mt_mul},
  {"__div",       mt_div},
  {"__square",    mt_square},
  {"__cube",      mt_cube},
  {"__recip",     mt_recip},
  {"__sqrt",      mt_sqrt},
  {"__invsqrt",   mt_invsqrt},
  {"__sin",       mt_sin},
  {"__cos",       mt_cos},
  {"__tan",       mt_tan},
  {"__antilog2",  dual_exp2},
  {"__antilog10", dual_exp10},
  {"__arcsec",    mt_arcsec},
  {"__arcsin",    mt_arcsin},
  {"__arccos",    mt_arccos},
  {"__arctan",    mt_arctan},
  {"__sinh",      mt_sinh},
  {"__cosh",      mt_cosh},
  {"__tanh",      mt_tanh},
  {"__sinc",      mt_sinc},
  {"__ln",        mt_ln},
  {"__lngamma",   mt_lngamma},
  {"__exp",       mt_exp},
  {"__pow",       mt_pow},
  {"__ipow",      mt_ipow},
  {"__tostring",  mt_tostring},  /* for output at the console, e.g. print(n) */
  {NULL, NULL}
};


static const luaL_Reg dualnumlib[] = {
  {"arccosh",   dual_arccosh},
  {"arccot",    dual_arccot},
  {"arccoth",   dual_arccoth},
  {"arcsinh",   dual_arcsinh},
  {"arctanh",   dual_arctanh},
  {"cathet",    dual_cathet},
  {"cbrt",      dual_cbrt},
  {"cosc",      dual_cosc},
  {"cot",       dual_cot},
  {"coth",      dual_coth},
  {"csc",       dual_csc},
  {"csch",      dual_csch},
  {"erf",       dual_erf},
  {"erfc",      dual_erfc},
  {"erfcx",     dual_erfcx},
  {"exp10",     dual_exp10},
  {"exp2",      dual_exp2},
  {"expm1",     dual_expm1},
  {"expx2",     dual_expx2},
  {"gamma",     dual_gamma},
  {"heaviside", dual_heaviside},
  {"hypot",     dual_hypot},
  {"hypot2",    dual_hypot2},
  {"hypot3",    dual_hypot3},
  {"inverf",    dual_inverf},
  {"inverfc",   dual_inverfc},
  {"invhypot",  dual_invhypot},
  {"lnGAMMA",   mt_lngamma},
  {"lnp1",      dual_lnp1},
  {"log10",     dual_log10},
  {"log2",      dual_log2},
  {"onepinv",   dual_onepinv},
  {"pow32",     dual_pow32},
  {"pow52",     dual_pow52},
  {"pow72",     dual_pow72},
  {"powe",      dual_powe},
  {"psi",       dual_psi},
  {"pytha",     dual_pytha},
  {"pytha4",    dual_pytha4},
  {"sec",       dual_sec},
  {"sech",      dual_sech},
  {"tanc",      dual_tanc},
  {"generate",  dual_generate},
  {"new",       dual_new},
  {"isdual",    dual_isdual},
  {"ishyper",   dual_ishyper},
  {"iseight",   dual_iseight},
  {"tostring",  mt_tostring},
  {NULL, NULL}
};


/*
** Open dual library
*/
LUALIB_API int luaopen_dual (lua_State *L) {
  luaL_newmetatable(L, AGENA_DUALLIBNAME);  /* metatable for dual numbers, adds it to the registry with key 'dual' */
  luaL_register(L, NULL, dual_metalib);  /* assign C metamethods to this metatable */
  luaL_register(L, AGENA_DUALLIBNAME, dualnumlib);
  return 1;
}

