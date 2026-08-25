/*
** $Id: factory.c,v 0.1 16/07/2018 $
** factory / Functional Programming
** See Copyright Notice in agena.h
*/

/* Various iterators. */

#define factory_c
#define LUA_LIB

#include <math.h>
#include <stdlib.h>

#include "agena.h"

#include "agenalib.h"
#include "agnhlps.h"
#include "agnxlib.h"

#define AGENA_LIBVERSION  "factory 0.3.0 for Agena as of July 11, 2023\n"


/* Signature so that `factory.reset` knows how to reset a specific iterator */
#define FN_CYTBL            100001
#define FN_CYSTR            100002
#define FN_CYSEQREG         100003
#define FN_COUNTINTSOLO     100010
#define FN_COUNTINTSTEP     100011
#define FN_COUNTDOUBLE      100012

#define aux_autocorrect(L,s0,eps) { \
  if (agn_getforadjust(L) && (fabs(s0) < eps)) { \
    s0 = 0; \
  } \
}

#define aux_bailout(L,s0,stop,step) { \
  if ((step > 0 && s0 > stop) || (step < 0 && s0 < stop)) { \
    lua_pushnil(L); \
    return 1; \
  } \
}

static int sema_ko (lua_State *L) {  /* Kahan-Ozawa, skycrane 2.1.1 */
  volatile lua_Number idx, idxold, step, y, t, u, w, c;
  lua_Number stop, eps;
  eps = agn_gethepsilon(L);
  idx = lua_tonumber(L, lua_upvalueindex(2));
  stop = lua_tonumber(L, lua_upvalueindex(3));  /* 2.14.0 */
  step = lua_tonumber(L, lua_upvalueindex(4));
  c = lua_tonumber(L, lua_upvalueindex(5));
  y = step - c;
  idxold = idx;
  idx += y;
  aux_bailout(L, idx, stop, step);  /* 2.14.0 */
  if (fabs(step) < fabs(c)) {
    t = step;
    step = -c;
    c = t;
  }
  u = (y - step) + c;
  if (fabs(idxold) < fabs(y)) {
    t = idxold;
    idxold = y;
    y = t;
  }
  w = (idx - idxold) - y;
  c = u + w;
  aux_autocorrect(L, idx, eps);  /* 2.34.0 */
  lua_pushnumber(L, idx);  /* new value */
  lua_pushvalue(L, -1);    /* put it on stack for later return */
  lua_replace(L, lua_upvalueindex(2));  /* update upvalue idx */
  lua_pushnumber(L, c);
  lua_replace(L, lua_upvalueindex(5));  /* update upvalue c */
  return 1;  /* return new value */
}

/* import factory
f := factory.count('babuska', 0, 0.1, 100)
c := 0;
while x := f() do
   print(x, c, x |- c);
   c +:= 0.1
od; */
static int sema_kb (lua_State *L) {  /* Kahan-Babuška, 2.30.5 */
  volatile lua_Number x, s, s0, c, cc, cs, ccs, t;
  lua_Number stop, eps;
  eps = agn_gethepsilon(L);
  s = lua_tonumber(L, lua_upvalueindex(2));
  stop = lua_tonumber(L, lua_upvalueindex(3));
  x = lua_tonumber(L, lua_upvalueindex(4));        /* step */
  cs  = agn_tonumber(L, lua_upvalueindex(5));      /* first correction value */
  ccs = agn_tonumber(L, lua_upvalueindex(6));      /* second correction value */
  t = s + x;
  c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
  s = t;
  t = cs + c;
  cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
  cs = t;
  ccs += cc;
  s0 = s + cs + ccs;
  aux_bailout(L, s0, stop, x);
  aux_autocorrect(L, s0, eps);
  /* do not modify s itself for it will corrupt the intermediate sum */
  lua_pushnumber(L, s);   /* new `internal` index for next iteration */
  lua_replace(L, lua_upvalueindex(2));  /* update upvalue idx */
  lua_pushnumber(L, cs);
  lua_replace(L, lua_upvalueindex(5));  /* update upvalue cs */
  lua_pushnumber(L, ccs);
  lua_replace(L, lua_upvalueindex(6));  /* update upvalue ccs */
  lua_pushnumber(L, s0);  /* new `external` index to be returned */
  return 1;
}

static int sema_neumaier (lua_State *L) {  /* 2.30.5, based on factory.c/accu_neumaier */
  volatile lua_Number s, c, t, x, s0;
  lua_Number stop, eps;
  eps = agn_gethepsilon(L);
  s = agn_tonumber(L, lua_upvalueindex(2));   /* accumulator */
  stop = lua_tonumber(L, lua_upvalueindex(3));
  x = lua_tonumber(L, lua_upvalueindex(4));   /* step */
  c = agn_tonumber(L, lua_upvalueindex(5));   /* correction value */
  t = s + x;
  c += (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
  s = t;
  s0 = s + c;
  aux_bailout(L, s0, stop, x);
  aux_autocorrect(L, s0, eps);
  lua_pushnumber(L, s);
  lua_replace(L, lua_upvalueindex(2));  /* update accumulator */
  lua_pushnumber(L, c);
  lua_replace(L, lua_upvalueindex(5));  /* update correction value */
  /* the correction value is added to the final sum, not the intermediate sum. */
  lua_pushnumber(L, s0);  /* new `external` index to be returned */
  return 1;
}

static int sema_kbn (lua_State *L) {  /* 2.30.5, based on factory.c/accu_kbn */
  lua_Number stop, eps;
  volatile lua_Number s, c, t, x, s0;
  eps = agn_gethepsilon(L);
  s = agn_tonumber(L, lua_upvalueindex(2));      /* accumulator */
  stop = lua_tonumber(L, lua_upvalueindex(3));
  x = lua_tonumber(L, lua_upvalueindex(4));      /* step */
  c = agn_tonumber(L, lua_upvalueindex(5));      /* correction value */
  t = s + x;
  c -= (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
  s = t;
  s0 = s - c;  /* s MINUS c */
  aux_bailout(L, s0, stop, x);
  aux_autocorrect(L, s0, eps);
  lua_pushnumber(L, s);
  lua_replace(L, lua_upvalueindex(2));  /* update accumulator */
  lua_pushnumber(L, c);
  lua_replace(L, lua_upvalueindex(5));  /* update c */
  /* the correction value is added to the final sum, not the intermediate sum. */
  lua_pushnumber(L, s0);  /* new `external` index to be returned */
  return 1;
}

static int sema_int (lua_State *L) {
  int val = lua_tonumber(L, lua_upvalueindex(2));
  if (val > lua_tonumber(L, lua_upvalueindex(3)))  /* 2.14.0 */
    lua_pushnil(L);
  else {
    lua_pushnumber(L, ++val);  /* new value */
    lua_pushvalue(L, -1);  /* duplicate it */
    lua_replace(L, lua_upvalueindex(2));  /* update count */
  }
  return 1;  /* return new value */
}

static int sema_intstep (lua_State *L) {
  int val = lua_tonumber(L, lua_upvalueindex(2)) + lua_tonumber(L, lua_upvalueindex(4));
  if (val > lua_tonumber(L, lua_upvalueindex(3)))  /* 2.14.0 */
    lua_pushnil(L);
  else {
    lua_pushnumber(L, val);  /* new value */
    lua_pushvalue(L, -1);  /* duplicate it */
    lua_replace(L, lua_upvalueindex(2));  /* update count */
  }
  return 1;  /* return new value */
}

#define KAHANOZAWA    0
#define BABUSKA       1
#define NEUMAIER      2
#define KBN           3
static int factory_count (lua_State *L) {  /* tuned and extended 2.1.1 */
  int alg, nargs;
  lua_Number count, step, stop, eps;
  alg = BABUSKA;  /* default is Kahan-Babuška, not slower than Kahan-Ozawa */
  nargs = lua_gettop(L);
  if (nargs > 1 && agn_isstring(L, nargs)) {  /* 2.30.5 */
    static const char *const modenames[] = {"ozawa", "babuska", "neumaier", "kbn", NULL};
    alg = luaL_checkoption(L, nargs, "babuska", modenames);
    lua_settop(L, nargs - 1);  /* drop method, prepare for optnumber calls */
  }
  eps = agn_gethepsilon(L);
  count = luaL_optnumber(L, 1, 0);  /* start value */
  step = luaL_optnumber(L, 2, 1);
  if (step == 0)  /* 2.34.0 fix */
    luaL_error(L, "Error in " LUA_QS ": step size is zero.", "factory.count");
  stop = luaL_optnumber(L, 3, HUGE_VAL) + ((step > 0) ? eps : -eps);  /* 2.14.0/2.34.0 */
  if (fabs(step) <= eps)
    luaL_error(L, "Error in " LUA_QS ": step size |%f| <= %f threshold.", "factory.count", (lua_Number)step, (lua_Number)eps);
  if (tools_isint(count) && tools_isint(step)) {  /* all integers ? */
    luaL_checkstack(L, 3 + (step != 1.0), "not enough stack space");  /* 3.18.4 fix */
    if (step == 1)
      lua_pushinteger(L, FN_COUNTINTSOLO);  /* 1 push signature */
    else
      lua_pushinteger(L, FN_COUNTINTSTEP);
    lua_pushnumber(L, count - step);        /* 2 start value (minus step) */
    lua_pushnumber(L, stop);                /* 3 stop value */
    if (step == 1)
      lua_pushcclosure(L, &sema_int, 3);
    else {
      lua_pushnumber(L, step);              /* 4 step */
      lua_pushcclosure(L, &sema_intstep, 4);
    }
    return 1;
  }
  switch (alg) {
    case KAHANOZAWA: {  /* apply Kahan-Ozawa summation */
      volatile lua_Number c, y, u, w, idx, idxold;
      idx = count - step;
      c = 0;
      y = step - c;
      idxold = idx;
      idx = idx + y;
      u = y - step;
      w = (idx - idxold) - y;
      c = u + w;
      luaL_checkstack(L, 5, "not enough stack space");  /* 3.18.4 fix */
      lua_pushinteger(L, FN_COUNTDOUBLE);  /* 1 - signature */
      lua_pushnumber(L, idxold);           /* 2 - count - step */
      lua_pushnumber(L, stop);             /* 3 - stop */
      lua_pushnumber(L, step);             /* 4 - step */
      lua_pushnumber(L, c);                /* 5 - c */
      lua_pushcclosure(L, &sema_ko, 5);    /* pops these five values from the stack */
      break;
    }
    case BABUSKA: {  /* apply Kahan-Babuška summation, 2.30.5 */
      volatile lua_Number x, s, c, cc, cs, ccs, t;
      x = step;
      s = count - x;  /* accumulator */
      cs  = 0;  /* first correction value */
      ccs = 0;  /* second correction value */
      t = s + x;
      c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
      s = t;
      t = cs + c;
      cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
      cs = t;
      ccs += cc;
      luaL_checkstack(L, 6, "not enough stack space");  /* 3.18.4 fix */
      lua_pushinteger(L, FN_COUNTDOUBLE);  /* 1 - signature */
      lua_pushnumber(L, count - step);     /* 2 - count - step */
      lua_pushnumber(L, stop);             /* 3 - stop */
      lua_pushnumber(L, step);             /* 4 - step */
      lua_pushnumber(L, cs);               /* 5 - cs */
      lua_pushnumber(L, ccs);              /* 6 - ccs */
      lua_pushcclosure(L, &sema_kb, 6);    /* pops these six values from the stack */
      break;
    }
    case NEUMAIER: {  /* apply Neumaier summation, 2.30.5 */
      volatile lua_Number s, c, t, x;
      x = step;
      s = count - x;  /* accumulator */
      c = 0;          /* correction value */
      t = s + x;
      c += (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
      luaL_checkstack(L, 5, "not enough stack space");  /* 3.18.4 fix */
      lua_pushinteger(L, FN_COUNTDOUBLE);  /* 1 - signature */
      lua_pushnumber(L, count - step);     /* 2 - count - step */
      lua_pushnumber(L, stop);             /* 3 - stop */
      lua_pushnumber(L, step);             /* 4 - step */
      lua_pushnumber(L, c);                /* 5 - c */
      lua_pushcclosure(L, &sema_neumaier, 5); /* pops these five values from the stack */
      break;
    }
    case KBN: {  /* apply Kahan-Babuška-Neumaier summation, 2.30.5 */
      volatile lua_Number s, c, t, x;
      x = step;
      s = count - x;  /* accumulator */
      c = 0;
      t = s + x;
      c -= (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
      luaL_checkstack(L, 5, "not enough stack space");  /* 3.18.4 fix */
      lua_pushinteger(L, FN_COUNTDOUBLE);  /* 1 - signature */
      lua_pushnumber(L, count - step);     /* 2 - count - step */
      lua_pushnumber(L, stop);             /* 3 - stop */
      lua_pushnumber(L, step);             /* 4 - step */
      lua_pushnumber(L, c);                /* 5 - c */
      lua_pushcclosure(L, &sema_kbn, 5);   /* pops these five values from the stack */
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": should not happen.", "factory.count");
  }
  return 1;
}


/* All of the following functions put upvalues in the following order:
   1 signature
   2 structure
   3 start index
   4 sentinel
   5 original start index for subsequent restart of iteration
   6 sentinel given flag
   7 length
   8 cycle flag */
static int iter_tbl (lua_State *L) {  /* iterate table, 2.12.3 */
  int offset, hassentinel, cycle, status, flag;
  flag = 1;
  hassentinel = lua_tointeger(L, lua_upvalueindex(6));
  cycle = lua_tointeger(L, lua_upvalueindex(8));
cycle:
  offset = 0;
  lua_pushvalue(L, lua_upvalueindex(3));  /* push key */
  if ( (status = lua_next(L, lua_upvalueindex(2))) ) {
    if (hassentinel && lua_equal(L, -1, lua_upvalueindex(4))) {  /* sentinel found ? */
      agn_poptop(L);   /* pop value */
      lua_pushnil(L);  /* push null instead */
    } else {
      lua_pushvalue(L, -2);  /* push key */
      lua_replace(L, lua_upvalueindex(3));  /* update key slot */
      offset++;
    }
    /* return key and value */
  } else
    lua_pushnil(L);
  if (offset == 0) {  /* no more value ?  -> reset iterator to cycle */
    lua_pushvalue(L, lua_upvalueindex(5));
    lua_replace(L, lua_upvalueindex(3));  /* reset key slot */
    if (flag && cycle && status == 0) {
      flag = 0;       /* prevent endless loops with empty tables */
      agn_poptop(L);  /* pop null */
      goto cycle;     /* commence with first element and then exit call */
    }
  }
  return 1 + offset;
}

static int iter_str (lua_State *L) {  /* iterate string, 2.12.3 */
  int offset, hassentinel, c, cycle, flag;
  size_t l;
  const char *str, *sentinel;
  flag = 1;
  str = lua_tostring(L, lua_upvalueindex(2));
  l = lua_tointeger(L, lua_upvalueindex(7));
  hassentinel = lua_tointeger(L, lua_upvalueindex(6));
  cycle = lua_tointeger(L, lua_upvalueindex(8));
cycle:
  offset = 0;
  c = lua_tointeger(L, lua_upvalueindex(3)) - 1;
  if (l == 0 || (c >= l && !cycle)) {  /* quit immediately, 2.15.5 fix */
    lua_pushnil(L);
    return 1;
  }
  lua_pushinteger(L, c + 1);
  if (c < l) {
    if (hassentinel) {
      sentinel = agn_tostring(L, lua_upvalueindex(4));
      if (str[c] != sentinel[0]) {
        lua_pushchar(L, str[c]);
        offset++;
      }
    } else {
      lua_pushchar(L, str[c]);
      offset++;
    }
  }
  if (offset == 0) {
    agn_poptop(L);  /* pop index */
    lua_pushvalue(L, lua_upvalueindex(5));
    lua_replace(L, lua_upvalueindex(3));  /* reset key slot */
    if (flag && cycle) {
      flag = 0;
      goto cycle;  /* commence with first element and then exit call */
    }
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, c + 2);
    lua_replace(L, lua_upvalueindex(3));  /* set new index */
  }
  return 1 + offset;
}

static int iter_seqorreg (lua_State *L) {  /* iterate sequence, 2.12.3 */
  int offset, hassentinel, c, idx, isseq, cycle, flag;
  size_t l;
  flag = 1;
  idx = lua_upvalueindex(2);
  hassentinel = lua_tointeger(L, lua_upvalueindex(6));
  l = lua_tointeger(L, lua_upvalueindex(7));  /* length */
  cycle = lua_tointeger(L, lua_upvalueindex(8));
  isseq = lua_tointeger(L, lua_upvalueindex(9));
cycle:
  offset = 0;
  c = lua_tointeger(L, lua_upvalueindex(3));  /* start index */
  if (l == 0 || (c > l && !cycle)) {  /* quit immediately, 2.15.5 fix */
    lua_pushnil(L);
    return 1;
  }
  lua_pushinteger(L, c);
  if (c <= l) {
    if (isseq)
      lua_seqrawgeti(L, idx, c);
    else
      agn_regrawgeti(L, idx, c);
    if (hassentinel && lua_equal(L, -1, lua_upvalueindex(4))) {  /* sentinel found ? */
      agn_poptop(L);   /* pop value */
      lua_pushnil(L);  /* push null instead */
    } else {
      offset++;
    }
  }
  if (offset == 0) {  /* end of structure or sentinel found ? */
    agn_poptop(L);    /* pop index */
    lua_pushvalue(L, lua_upvalueindex(5));  /* push original start index */
    lua_replace(L, lua_upvalueindex(3));    /* and reset key slot */
    if (flag && cycle) {
      flag = 0;
      goto cycle;    /* commence with first element and then exit call */
    }
    lua_pushnil(L);  /* if the start index is out-of-range, always return null */
  } else {
    lua_pushinteger(L, c + 1);
    lua_replace(L, lua_upvalueindex(3));  /* set new index */
  }
  return 1 + offset;
}

static int iterate_or_cyle (lua_State *L, int cycle, const char *procname) {  /* 2.12.3 */
  int idx, nargs, type;
  size_t nops;
  idx = 1;
  nargs = lua_gettop(L);
  type = lua_type(L, 1);
  nops = agn_nops(L, 1);
  lua_settop(L, 3);  /* `push` structure, start index (or null) and sentinel (or null) */
  switch (type) {
    case LUA_TTABLE: {
      luaL_checkstack(L, 8, "not enough stack space");  /* 3.18.4 fix */
      lua_pushinteger(L, FN_CYTBL);      /* 1 push signature */
      lua_pushvalue(L, 1);               /* 2 push table */
      lua_pushvalue(L, 2);               /* 3 push start index */
      lua_pushvalue(L, 3);               /* 4 push sentinel */
      lua_pushvalue(L, 2);               /* 5 push original start key for subsequent restart of iteration */
      lua_pushinteger(L, nargs >= 3);    /* 6 sentinel given ? */
      lua_pushinteger(L, -1);            /* 7 length */
      lua_pushinteger(L, cycle);         /* 8 cycle ? */
      lua_pushcclosure(L, &iter_tbl, 8);
      break; }
    case LUA_TSTRING:
      luaL_checkstack(L, 8, "not enough stack space");  /* 3.18.4 fix */
      lua_pushinteger(L, FN_CYSTR);      /* 1 push signature */
      lua_pushvalue(L, 1);               /* 2 push string */
      if (!lua_isnil(L, 2)) idx = agn_checkposint(L, 2);
      lua_pushinteger(L, idx);           /* 3 push start index */
      if (!lua_isnil(L, 3)) {            /* 4 push sentinel */
        const char *sentinel = agn_checkstring(L, 3);
        lua_pushchar(L, sentinel[0]);
      } else
        lua_pushnil(L);
      lua_pushvalue(L, -2);              /* 5 push original start index */
      lua_pushinteger(L, nargs >= 3);    /* 6 push sentinel given flag */
      lua_pushinteger(L, nops);          /* 7 length of string */
      lua_pushinteger(L, cycle);         /* 8 cycle ? */
      lua_pushcclosure(L, &iter_str, 8);
      break;
    case LUA_TSEQ: case LUA_TREG:
      luaL_checkstack(L, 9, "not enough stack space");  /* 3.18.4 fix */
      lua_pushinteger(L, FN_CYSEQREG);   /* 1 push signature */
      lua_pushvalue(L, 1);               /* 2 push sequence */
      if (!lua_isnil(L, 2)) idx = agn_checkposint(L, 2);
      lua_pushinteger(L, idx);           /* 3 push start index */
      lua_pushvalue(L, 3);               /* 4 push sentinel */
      lua_pushvalue(L, -2);              /* 5 push original start index */
      lua_pushinteger(L, nargs >= 3);    /* 6 push sentinel given flag */
      lua_pushinteger(L, agn_nops(L, 1));/* 7 length of structure */
      lua_pushinteger(L, cycle);         /* 8 cycle ? */
      lua_pushinteger(L, type == LUA_TSEQ);  /* 9 sequence or register */
      lua_pushcclosure(L, &iter_seqorreg, 9);
      break;
    default: {
      lua_settop(L, 0);  /* level the stack */
      luaL_error(L, "Error in " LUA_QS ": invalid argument #1 of type %s.", procname, luaL_typename(L, 1));
    }
  }
  return 1;
}

/* See also: skycrane.iterate */
static int factory_iterate (lua_State *L) {  /* 2.12.3 */
  iterate_or_cyle(L, 0, "factory.iterate");
  return 1;
}


static int factory_cycle (lua_State *L) {  /* 2.12.3 */
  iterate_or_cyle(L, 1, "factory.cycle");
  return 1;
}


void aux_checkiterator (lua_State *L, int idx, int nups) {  /* 2.12.3 */
  if (nups < idx)
    luaL_error(L, "Error in " LUA_QS ": function or iterator is not supported or corrupt.", "factory.reset");
}

#define aux_reseterror(L) { \
  luaL_error(L, "Error in " LUA_QS ": function or iterator is not supported.", "factory.reset"); \
}

lua_Number aux_checknumber (lua_State *L, int idx) {
  lua_Number r = 0;
  if (!lua_isnumber(L, idx)) {
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": iterator is corrupt.", "factory.reset");
  } else
    r = lua_tonumber(L, idx);
  agn_poptop(L);
  return r;
}

static int factory_reset (lua_State *L) {  /* 2.12.3 */
  int nups, count, signature;
  lua_Number step;
  luaL_checkany(L, 2);
  nups = lua_nupvalues(L, 1);
  if (nups < 0)  /* no function */
    luaL_error(L, "Error in " LUA_QS ": function expected as first argument, got %s.",
      "factory.reset", luaL_typename(L, 1));
  if (nups == 0)  /* no upvalues ? */
    aux_reseterror(L);
  lua_getupvalue(L, 1, 1);  /* get signature */
  signature = aux_checknumber(L, -1);  /* also pops signature */
  switch (signature) {
    case FN_CYTBL:
      aux_checkiterator(L, 8, nups);
      lua_pushvalue(L, 2);
      lua_setupvalue(L, 1, 3);  /* 1 = function, 3rd upvalue */
      break;
    case FN_CYSTR: case FN_CYSEQREG:
      aux_checkiterator(L, 9, nups);
      count = agnL_optposint(L, 2, 1);
      lua_pushinteger(L, count);  /* with an invalid index, the iterator issues an error later */
      lua_setupvalue(L, 1, 3);
      break;
    case FN_COUNTINTSOLO:
      aux_checkiterator(L, 2, nups);
      count = luaL_optinteger(L, 2, 0);
      lua_pushinteger(L, count - 1);
      lua_setupvalue(L, 1, 2);
      break;
    case FN_COUNTINTSTEP: case FN_COUNTDOUBLE:
      aux_checkiterator(L, 3, nups);
      count = luaL_optnumber(L, 2, 0);
      lua_getupvalue(L, 1, 4);  /* get step, changed 2.14.0 */
      step = aux_checknumber(L, -1);
      lua_pushnumber(L, count - step);
      lua_setupvalue(L, 1, 2);
      break;
    default:
      aux_reseterror(L);
  }
  return 0;
}


/* Creates a factory that when called tries each function f, ... with the arguments passed to the factory. `factory.anyof`
   also accepts structures that have a '__call' metamethod (see Chapter 6.19 of the Primer and Reference).

   The factory quits if one of the calls does not return `null`, `false` or `fail`. If a call results in no result at all,
   then the next function or structure with '__call' metamethod will be run. The return is either `null` if none of the
   calls was successful, or the full result of the successful call, including all returns.

   The very first result of a call is taken to determine whether it was successful or not. The functionality is more or less
   equal to:

   anyof := proc(?) is
      local args := ?;
      return proc(?)
         for fn in args do
            if r := fn(unpack(?)) then
               return r
            fi
         od
      end
   end;

   Example:

   isalphanumeric := factory.anyof(strings.isnumber, strings.islatin);
   isalphanumeric('1') -> true
   isalphanumeric('i') -> true
   isalphanumeric('ü') -> null

   The C code is five times faster than the Agena implementation.

   Idea has been taken from Gary V. Vaughan's lyaml package for Lua 5.x, file functional.lua, translated to C.
   The save runtime, the C implementation does not check whether a value is callable at runtime but at creation
   of the factory, a speed gain of around 10 percent.

   Introduced 3.1.0 */
static int anyof_aux (lua_State *L) {
  int nargs, nops, nrets, i, j;
  nargs = lua_gettop(L);  /* number of arguments passed to closure */
  nops = agn_tointeger(L, lua_upvalueindex(1));  /* number of upvalues */
  for (i=2; i <= nops; i++) {
    luaL_checkstack(L, nargs + 1, "not enough stack space");  /* 3.5.5 fix */
    lua_pushvalue(L, lua_upvalueindex(i));  /* push function ... */
    for (j=1; j <= nargs; j++) {  /* ... and arguments */
      lua_pushvalue(L, j);
    }
    lua_call(L, nargs, LUA_MULTRET);
    nrets = lua_gettop(L) - nargs;  /* get the number of returns */
    if (nrets > 0 && !lua_isnilfalseorfail(L, -nrets))
      return nrets;     /* we have a non-nil and non-false/fail result, exit */
    lua_pop(L, nrets);  /* level the stack and try next function */
  }
  lua_pushnil(L);
  return 1;
}

static int factory_anyof (lua_State *L) {  /* 3.1.0 */
  int i, nargs;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": at least one function is required.", "factory.anyof");
  luaL_checkstack(L, nargs + 1, "not enough stack space");
  lua_pushinteger(L, nargs + 1);
  for (i=1; i <= nargs; i++) {
    lua_pushvalue(L, i);
    if (!agnL_iscallable(L, -1)) {
      lua_pop(L, i + 1);  /* level stack */
      luaL_error(L, "Error in " LUA_QS ": argument #%d is not callable.", "factory.anyof", i);
    }
  }
  lua_pushcclosure(L, anyof_aux, nargs + 1);
  return 1;
}


/* The function picks only given results from a function call, by taking a function f and the positions of the results to
   be returned and producing a factory that when called delivers the results of interest. Imagine a function f

   > f := proc() is return 10, 11, 12, 13, end;

   where we want to have only the first and third result of its call, that is numbers 10 and 12. We define

   > import factory;

   > g := factory.pick(f, 1, 3);

   > g():
   10      12

   Introduced 3.1.0 */
static int pick_aux (lua_State *L) {
  int nargs, nops, nrets, onstack, i;
  nargs = lua_gettop(L);  /* number of arguments passed to closure */
  nops = agn_tointeger(L, lua_upvalueindex(1));  /* number of upvalues */
  luaL_checkstack(L, nargs + 1, "not enough stack space");  /* 3.5.5 fix */
  lua_pushvalue(L, lua_upvalueindex(2));  /* push function ... */
  for (i=1; i <= nargs; i++) {  /* ... and closure arguments */
    lua_pushvalue(L, i);
  }
  lua_call(L, nargs, LUA_MULTRET);
  nrets = lua_gettop(L) - nargs;  /* get the number of returns */
  luaL_checkstack(L, nrets, "not enough stack space");
  onstack = -nrets;
  /* push the requested results onto the stack top ... */
  for (i=3; i <= nops; i++) {  /* 3.1.1 tweak */
    lua_pushvalue(L, (onstack--) + lua_tointeger(L, (lua_upvalueindex(i))) - 1);
  }
  return -onstack - nrets;  /* ... finally return them only */
}

static int factory_pick (lua_State *L) {  /* 3.1.0 */
  int i, nargs;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": one function and one index is required.", "factory.pick");
  luaL_checkstack(L, nargs + 1, "not enough stack space");
  lua_pushinteger(L, nargs + 1);
  lua_pushvalue(L, 1);
  for (i=2; i <= nargs; i++) {
    lua_pushvalue(L, i);
    if (!agn_isposint(L, -1)) {
      lua_pop(L, i + 1);
      luaL_error(L, "Error in " LUA_QS ": argument #%d must be a positive integer.", "factory.pick", i);
    }
  }
  lua_pushcclosure(L, pick_aux, nargs + 1);
  return 1;
}


/*
** factory_curry has been originally written by Rici Lake for Lua 5.x, and has been shortened for speed*);
** original code taken from: http://lua-users.org/files/wiki_insecure/users/rici/lfunclib.c
** No copyright notice found.
** *) The speed increase is marginal, i.e. 8 percent only.
*/

/* Transforms a function with multiple arguments into a sequence of single-argument functions, i.e. f(a, b, c, ...) becomes f(a)(b)(c)...
   If only f is given, returns f. */

/* Example usage:
f := << x, y, z -> x*y + z >>
t := curry(f);  # returns f
t(10, 20, 30):
230
t := curry(f, 10);  # returns f(10, y, z)
t(20, 30):
230
u := curry(t, 20);  # returns t(10, 20)(z) = f(10, 20, z)
u(30):
230
*/

/* func:curry(...) -> function
 * The returned function is the original function with the given arguments "filled in" from the left. */
static int f_curry_aux (lua_State *L) {
  int i;
  lua_Debug ar;
  /* we cannot call lua_nupvalues or other functions */
  lua_getstack(L, 0, &ar);   /* get info on the current running function */
  lua_getnupvalues(L, &ar);  /* 8% faster than lua_getinfo; get number of upvals, returns ar.nups=0 with tail calls */
  /* lua_getinfo(L, "u", &ar); */
  luaL_checkstack(L, ar.nups, "insert upvalues");
  for (i=1; i <= ar.nups; i++) {
    lua_pushvalue(L, lua_upvalueindex(i));
    lua_insert(L, i);
  }
  lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
  return lua_gettop(L);
}

static int factory_curry (lua_State *L) {  /* added 2.36.2 */
  luaL_checktype(L, 1, LUA_TFUNCTION);
  int top = lua_gettop(L);
  if (top >= LUAI_MAXUPVALUES) {
    luaL_error(L, "too many curried arguments, maximum is %d", LUAI_MAXUPVALUES);
  } else if (top == 1) {  /* avoid creating a closure with overhead if no function arguments given */
    lua_settop(L, 1);
    return 1;
  }
  if (lua_tocfunction(L, 1) == f_curry_aux) {  /* has closure been created by func.curry ? */
    int i = 1;
    /* We should find out how many there are, really. */
    luaL_checkstack(L, LUAI_MAXUPVALUES, "curry combination");
    /* get all upvalue(s) in closure, insert them between closure and given arguments */
    for (; lua_getupvalue(L, 1, i); ++i) {
      lua_insert(L, i + 1);
    }
    top += i - 2;
  }
  /* push upvalues plus function arguments */
  lua_pushcclosure(L, f_curry_aux, top);
  return 1;
}


static const luaL_Reg factorylib[] = {
  {"anyof",   factory_anyof},         /* added July 11, 2023 */
  {"count",   factory_count},         /* added May 26, 2013 */
  {"curry",   factory_curry},         /* added January 31, 2023 */
  {"cycle",   factory_cycle},         /* added July 15, 2018 */
  {"iterate", factory_iterate},       /* added July 15, 2018 */
  {"pick",    factory_pick},          /* added July 12, 2023 */
  {"reset",   factory_reset},         /* added July 16, 2018 */
  {NULL, NULL}
};


/*
** Open factory library
*/
LUALIB_API int luaopen_factory (lua_State *L) {
  luaL_register(L, AGENA_FACTORYLIBNAME, factorylib);
  lua_rawsetstringstring(L, -1, "initstring", AGENA_LIBVERSION);
  return 1;
}

