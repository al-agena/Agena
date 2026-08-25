/*
** $Id: lreglib.c,v 1.38 2005/10/23 17:38:15 roberto Exp $
** Library for Register Manipulation
** See Copyright Notice in agena.h
*/

#include <stddef.h>

#define lreglib_c
#define LUA_LIB

#include "agena.h"

#include "agncmpt.h"   /* for LUA_OPEQ */
#include "agnxlib.h"
#include "agenalib.h"
#include "linalg.h"
#include "lopcodes.h"
#include "ltablib.h"


static int reg_settop (lua_State *L) {  /* 2.3.0 RC 3 */
  luaL_argcheck(L, lua_isreg(L, 1), 1, "register expected");
  lua_pushvalue(L, 2);
  /* agnReg_settop checks the 2nd argument automatically and would return false in case of a wrong type */
  lua_pushboolean(L, agn_regsettop(L, 1));
  return 1;
}


static int reg_resize (lua_State *L) {  /* 4.6.4 (2.3.0 RC 3) */
  luaL_argcheck(L, lua_isreg(L, 1), 1, "register expected");
  lua_pushboolean(L, agn_regresize(L, 1, agn_checkinteger(L, 2), agnL_optboolean(L, 3, 1)));
  return 1;
}


/* creates a register with values a, a+step ..., b-step, b; based on calc.fseq
   uses a modified version of Kahan's summation algorithm to avoid roundoff errors
   0.30.0, December 30, 2009; merged with calc.fseq 1.3.3, February 01, 2011

   calc.fseq comments: creates a sequence with values f(a), ..., f(b); June 30, 2008;
   20 % faster than an Agena implementation; extended August 20, 2009, 0.27.0
   extended 0.30.0, December 29, 2009 with Kahan summation algorithm to avoid roundoff errors;
   extended 1.8.8, November 07, 2012, to also return non-numeric sequences; extended 2.4.1,
   January 15, 2015, to process the number of elements to be returned.

   For the idea on the Matlab version please see:
   http://de.mathworks.com/help/matlab/ref/linspace.html
   http://www.che.utah.edu/~sutherland/wiki/index.php/Matlab_Arrays#Linspace_and_Logspace */

static int reg_new (lua_State *L) {
  /* use `volatile` so that the compiler does not render the Kahan code effectless */
  /* volatile lua_Number idx, step, c, y, t; formerly nreg, 2.28.1; created October 15, 2014 */
  int isfunc, isint, isdefault;
  size_t counter, i, nargs, total, offset;
  lua_Number a, b, eps;
  volatile lua_Number s, c, cs, ccs, cc, t, idx, step, x;
  luaL_aux_nstructure(L, "registers.new", &nargs, &offset, &a, &b, &step, &eps, &total, &isfunc, &isdefault, &isint);
  luaL_checkstack(L, 1 + isdefault, "not enough stack space");  /* 3.15.4 fix */
  agn_createreg(L, total);
  counter = 0;
  cs = ccs = 0;
  s = idx = a;
  if (isdefault) {  /* create a register of n slots and fill it with one and the same default of any type */
    agn_pairgeti(L, 2, 2);
  }
  /* total > counter: prevents that the last element is inserted even if a roundoff error occurred. */
  if (isfunc) {  /* function passed ? */
    int slots = 2 + (nargs >= 4 + offset)*(nargs - 4 - (int)offset + 1);
    while (idx <= b || tools_approx(idx, b, eps)) {
      luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.5/3.15.4 fix */
      lua_pushvalue(L, offset);  /* push function */
      lua_pushnumber(L, (fabs(idx) < AGN_HEPSILON) ? 0 : idx);  /* quite dirty hack to avoid roundoff errors with 0 */
      for (i=4 + offset; i <= nargs; i++) lua_pushvalue(L, i);
      lua_call(L, slots - 1, 1);
      agn_regrawseti(L, -2, ++counter);
      if (isint) {
        idx += step;
      } else {
        x = step;  /* Kahan-Babuska */
        t = s + x;
        c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
        s = t;
        t = cs + c;
        cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
        cs = t;
        ccs += cc;
        idx = s + cs + ccs;
      }
    }
  } else {
    while (idx <= b || tools_approx(idx, b, AGN_HEPSILON)) {
      if (isdefault) {  /* fill with default value  */
        lua_pushvalue(L, -1);
        agn_regrawseti(L, -3, idx);
      } else
        agn_regsetinumber(L, -1, ++counter, (fabs(idx) < AGN_HEPSILON) ? 0 : idx);
      if (isint) {  /* 2.12.2 */
        idx += step;
      } else {
        x = step;  /* Kahan-Babuska */
        t = s + x;
        c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
        s = t;
        t = cs + c;
        cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
        cs = t;
        ccs += cc;
        idx = s + cs + ccs;
      }
    }
  }
  if (isdefault) agn_poptop(L);
  return 1;
}


/* In register r, the function replaces every occurrence of the value old with value new, in-place, and
   returns the number of substitutions done.
   The function is four times faster than `subs`, avoiding most of all checks and conversions that
   `subs` is performing. 5.1.3 */
static int reg_subs (lua_State *L) {  /* 5.1.3 */
  luaL_checktype(L, 1, LUA_TREG);
  luaL_checkany(L, 3);
  lua_pushinteger(L, agn_regsubs(L, 1, 2, 3));
  return 1;
}


static int reg_numunion (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TREG, 1, "argument is not a register");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TREG, 2, "argument is not a register");
  lua_pushinteger(L, agn_numunion(L, 1, 2));
  return 1;
}


static int reg_numintersect (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TREG, 1, "argument is not a register");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TREG, 2, "argument is not a register");
  lua_pushinteger(L, agn_numintersect(L, 1, 2));
  return 1;
}


static int reg_numminus (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TREG, 1, "argument is not a register");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TREG, 2, "argument is not a register");
  lua_pushinteger(L, agn_numminus(L, 1, 2));
  return 1;
}


static int reg_isall (lua_State *L) {  /* 3.10.2 */
  agn_regisall(L, 1, "registers.isall");
  return 1;
}


static int reg_move (lua_State *L) {  /* based on seq_move, 6.5.4 */
  lua_Integer f = luaL_checkinteger(L, 2);  /* source start position */
  lua_Integer e = luaL_checkinteger(L, 3);  /* source end position */
  lua_Integer t = agn_checkposint(L, 4);    /* destination start position */
  int tt = !lua_isnoneornil(L, 5) ? 5 : 1;  /* index of destination structure */
  luaL_checktype(L, 1, LUA_TREG);
  luaL_checktype(L, tt, LUA_TREG);
  luaL_argcheck(L, agn_regsize(L, tt) + 1 >= t, 4, "destination wrap around");
  if (e >= f) {  /* otherwise, nothing to move */
    lua_Integer n, i;
    luaL_argcheck(L, f > 0 || e < LUA_MAXINTEGER + f, 3, "too many elements to move");
    n = e - f + 1;  /* number of elements to move */
    luaL_argcheck(L, t <= LUA_MAXINTEGER - n + 1, 4, "destination wrap around");
    /* if APPEND: source end pos < dest start pos ||
          SHIFT LEFT: source start pos >= dest start pos ||
          source structure different from destination structure */
    if (t > e || t <= f || (tt != 1 && !lua_compare(L, 1, tt, LUA_OPEQ))) {
      for (i=0; i < n; i++) {  /* from left to right, optionally using metamethods */
        agn_regrawgeti(L, 1, f + i);
        agn_regrawseti(L, tt, t + i);
      }
    } else {  /* SHIFT RIGHT in same structure */
      for (i=n - 1; i >= 0; i--) {  /* from right to left, optionally using metamethods */
        agn_regrawgeti(L, 1, f + i);
        agn_regrawseti(L, tt, t + i);
      }
    }
  }
  lua_pushvalue(L, tt);  /* return destination register */
  return 1;
}


/* Checks register A for subregisters of the same size. Returns both the number of rows and the number of columns found
   if all subregisters have the same size or issues an error otherwise. 6.5.4 */
static int aux_getdim (lua_State *L, int idx, int *rows, int *cols) {
  int i, c, rc;
  luaL_checktype(L, idx, LUA_TREG);
  *cols = c = 0;
  *rows = agn_regsize(L, idx);
  rc = 0;
  for (i=0; i < *rows; i++) {
    agn_regrawgeti(L, idx, i + 1);  /* push item */
    if (lua_isreg(L, -1)) {  /* a row */
      *cols = agn_regsize(L, -1);
      if (i == 0) c = *cols;
      rc = (c != *cols);
    } else {  /* not a row */
      rc = 1;
    }
    agn_poptop(L);
    if (rc) break;
  }
  return rc;
}

static int reg_getdim (lua_State *L) {  /* 6.5.4 */
  int rows, cols;
  if (aux_getdim(L, 1, &rows, &cols)) {
    luaL_error(L, "Error in " LUA_QS ": register is not 2-dimensional.", "registers.getdim");
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushinteger(L, rows);
  lua_pushinteger(L, cols);
  return 2;
}

/* Checks whether register A has the same number of rows as there are elements in each of its subregisters (`columns`).
   The subregisters must be of the same size. Returns `true` or `false` and in case of `true`, also returns the number
   of rows and the number of columns found. 6.5.4 */
static int reg_issquare (lua_State *L) {
  int rows, cols, rc, nrets;
  rc = aux_getdim(L, 1, &rows, &cols);
  nrets = 1 + 2*(rc == 0);
  luaL_checkstack(L, nrets, "not enough stack space");
  lua_pushboolean(L, rc == 0 && rows == cols);
  if (!rc) {
    lua_pushinteger(L, rows);
    lua_pushinteger(L, cols);
  }
  return nrets;
}


/* Checks register A for subregisters of the same size. Returns `true` or `false` and in case of `true`, also returns
   the number of rows and the number of columns found. With square registers, also returns `true`. 6.5.4 */
static int reg_isrectangular (lua_State *L) {
  int rows, cols, rc, nrets;
  rc = aux_getdim(L, 1, &rows, &cols);
  nrets = 1 + 2*(rc == 0);
  luaL_checkstack(L, nrets, "not enough stack space");
  lua_pushboolean(L, rc == 0);
  if (!rc) {
    lua_pushinteger(L, rows);
    lua_pushinteger(L, cols);
  }
  return nrets;
}


static FORCE_INLINE int checkvector (lua_State *L, int idx, const char *procname) {
  if (!(lua_isreg(L, idx)))
    luaL_error(L, "Error in " LUA_QS ": register expected, got %s.", procname, luaL_typename(L, idx));
  return agn_regsize(L, idx);
}

static int reg_col (lua_State *L) {  /* 7.3.7 */
  int i, j, c, l, u, m, n, idx, nargs, ncols;
  luaL_checkany(L, 2);
  nargs = lua_gettop(L);
  m = n = ncols = 0;
  if (aux_getdim(L, 1, &m, &n))  /* checks input structure for correct type, too */
    luaL_error(L, "Error in " LUA_QS ": register is not two-dimensional.", "registers.col");
  for (i=2; i <= nargs && agn_isinteger(L, i); i++, ncols++) {
    idx = tools_posrelat(agn_tointeger(L, i), n);
    if (idx < 1 || idx > n)
      luaL_error(L, "Error in " LUA_QS ": column %d does not exist.", "registers.col", agn_tointeger(L, i));
  }
  if (ncols == 0)
    luaL_error(L, "Error in " LUA_QS ": need at least one index.", "registers.col");
  l = 1; u = m;
  la_getrange(L, 2 + ncols, nargs, &l, &u, m, "registers.col");
  c = 0;
  luaL_checkstack(L, 3 + (ncols != 1), "not enough stack space");
  agn_createreg(L, u - l + 1);
  if (ncols == 1) {
    for (; l <= u; l++) {
      agn_regrawgeti(L, 1, l);  /* push l-th row vector in matrix at idx */
      checkvector(L, -1, "registers.col");
      agn_regrawgeti(L, -1, tools_posrelat(agn_tointeger(L, 2), n));
      agn_regrawseti(L, -3, ++c);
      agn_poptop(L);
    }
  } else {
    for (i=l; i <= u; i++) {   /* for each row vector i in range */
      agn_createreg(L, ncols);
      agn_regrawgeti(L, 1, i);  /* push l-th row vector in matrix at idx */
      checkvector(L, -1, "registers.col");
      for (j=2; j < 2 + ncols; j++) {  /* for each column j */
        agn_regrawgeti(L, -1, tools_posrelat(agn_tointeger(L, j), n));  /* get value at column j */
        agn_regrawseti(L, -3, j - 1);  /* set it into substructure */
      }
      agn_poptop(L);  /* drop row vector, result substructure is on top */
      agn_regrawseti(L, -2, ++c);
    }
  }
  return 1;
}


/* Checks whether register s consists of zeros only and returns `true` or `false`. It returns the index of the
   first non-zero element in s as a second result, or 0 if there is none. The non-zero element is returned as a third
   result, too.

   The function considers any non-numeric values in s as non-zero.

   By default, the check is done against strict zero. You can change this by passing a positive epsilon value as an
   optional second argument so that all elements x with |x| <= epsilon will be considered zero.

   See also: `tables.iszero`. 6.5.4 */
static int reg_iszero (lua_State *L) {
  int i, n, r, rc;
  lua_Number x, eps;
  luaL_checktype(L, 1, LUA_TREG);
  eps = agnL_optnonnegative(L, 2, 0.0);
  r = 1; x = AGN_NAN;
  n = agn_regsize(L, 1);;
  for (i=1; i <= n && r; i++) {  /* from left to right ! */
    x = agn_regrawgetinumber(L, 1, i, &rc);
    r = rc && (fabs(x) <= eps);
  }
  rc = r*(n != 0);
  lua_pushboolean(L, rc);
  lua_pushinteger(L, (!r)*(i - 1));
  if (!rc) lua_pushnumber(L, x);
  return 2 + !rc;
}


static int reg_isone (lua_State *L) {  /* 6.5.4 */
  int i, n, r, rc;
  lua_Number x, eps;
  luaL_checktype(L, 1, LUA_TREG);
  eps = agnL_optnonnegative(L, 2, 0.0);
  r = 1; x = AGN_NAN;
  n = agn_regsize(L, 1);;
  for (i=1; i <= n && r; i++) {  /* from left to right ! */
    x = agn_regrawgetinumber(L, 1, i, &rc);
    r = rc && (fabs(x - 1.0) <= eps);
  }
  rc = r*(n != 0);
  lua_pushboolean(L, rc);
  lua_pushinteger(L, (!r)*(i - 1));
  if (!rc) lua_pushnumber(L, x);
  return 2 + !rc;
}


/* Creates a register of n subregisters, each with n preallocated slots. You can fill the subregisters
   with a default, narray times, by passing the `init=default` option. 6.5.4 */
static int reg_regofregs (lua_State *L) {
  int i, j, narray, nops, isdefault;
  luaL_checkstack(L, 2, "not enough stack space");
  nops = agn_checknonnegint(L, 1);
  narray = agn_checknonnegint(L, 2);
  isdefault = lua_ispair(L, 3);
  agn_createreg(L, nops);
  if (isdefault) {  /* create a register of n slots and fill it with one and the same default of any type */
    agn_pairgeti(L, 3, 2);
  }
  for (i=0; i < nops; i++) {
    agn_createreg(L, narray);
    if (isdefault) {
      for (j=0; j < narray; j++) {
        lua_pushvalue(L, -2);
        agn_regrawseti(L, -2, j + 1);
      }
    }
    agn_regrawseti(L, -2 - isdefault, i + 1);
  }
  if (isdefault) agn_poptop(L);
  return 1;
}


/* }====================================================== */

static const luaL_Reg reg_funcs[] = {
  {"col", reg_col},                      /* added April 05, 2026 */
  {"getdim", reg_getdim},                /* added December 19, 2025 */
  {"isall", reg_isall},                  /* added February 04, 2024 */
  {"isone", reg_isone},                  /* added December 19, 2025 */
  {"isrectangular", reg_isrectangular},  /* added December 19, 2025 */
  {"issquare", reg_issquare},            /* added December 19, 2025 */
  {"iszero", reg_iszero},                /* added December 19, 2025 */
  {"move", reg_move},                    /* added December 18, 2025 */
  {"new", reg_new},                      /* added May 30, 2022 */
  {"numunion", reg_numunion},            /* added January 26, 2024 */
  {"numintersect", reg_numintersect},    /* added January 26, 2024 */
  {"numminus", reg_numminus},            /* added January 26, 2024 */
  {"regofregs", reg_regofregs},          /* added December 19, 2025 */
  {"resize", reg_resize},                /* added December 16, 2024 */
  {"settop", reg_settop},                /* added October 14, 2014 */
  {"subs", reg_subs},                    /* added July 11, 2025 */
  {NULL, NULL}
};


LUALIB_API int luaopen_registers (lua_State *L) {
  luaL_register(L, AGENA_REGLIBNAME, reg_funcs);
  return 1;
}

