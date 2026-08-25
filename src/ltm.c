/*
** $Id: ltm.c,v 2.8 2006/01/10 12:50:00 roberto Exp $
** Tag methods
** See Copyright Notice in agena.h
*/

#include <string.h>

#define ltm_c
#define LUA_CORE

#include "agena.h"

#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


/* typenames; grep "(GREP_POINT) types;" if you want to change their order or add new ones */

const char *const luaT_typenames[] = {
  "null", "boolean", "number", "complex",
  "string", "procedure", "userdata", "userdata", "thread",
  "table", "sequence", "register", "pair", "set",
  "integer", "posint", "nonnegint", "nonzeroint", "positive", "negative", "nonnegative",
  "listing", "basic", "anything", "proto", "upval"  /* 0.4.0 */
};


void luaT_init (lua_State *L) {
  static const char *const luaT_eventname[] = {  /* ORDER TM -> ltm.h */
    "__index", "__writeindex",
    "__gc", "__weak", "__eq",
    "__add", "__sub", "__mul", "__div", "__intdiv",
    "__mod", "__pow", "__ipow", "__unm", "__lt", "__le",
    "__concat", "__call",  /* 18 */
    /* with the exception of '__intdiv', additional Agena metamethods start from here, added 0.5.4 and later */
    "__eeq", "__aeq", "__size", "__abs",  /* 22 */
    "__minus", "__in", "__notin", "__oftype", "__intersect", "__union",  /* 28 */
    "__empty", "__filled",   /* 30, 2.15.4, moved into 32-area */
    "__qsumup", "__sumup",   /* 32 */
    /* further metamethods not supported by fasttm start from here */
    "__tan", "__arctan", "__cos", "__int",  "__frac", "__even", "__exp",  /* 39 */
    "__ln", "__sign", "__sinh", "__cosh",  "__tanh", "__arcsin", "__arccos",  /* 46 */
    "__recip", "__nan", "__cosxx", "__bea", "__flip", "__conjugate",  /* 52 */
    "__arcsec", "__lngamma", "__finite", "__sqrt", "__entier", "__antilog2",  /* 58 */
    "__antilog10", "__signum", "__sin", "__sinc", "__qmdev", "__cis",  /* 64 */
    "__odd", "__absdiff", "__infinite", "__square", "__cube",  /* 69 */
    "__real", "__imag", "__zero", "__nonzero", "__invsqrt", "__squareadd",  /* 75 */
    "__integral", "__fractional", "__log", "__mulup",  /* 79, 2.16.1, 2.16.13, 2.16.13, 2.16.14, 2.34.7, 3.5.6 */
    "__band", "__bor", "__bxor", "__bnot", "__bshl", "__bshr", "__brotl", "__brotr",  /* 87, 4.8.0/6.1.4 */
    "__left", "__right", "__iter"  /* 90, 4.12.11, 7.1.0 */
  };
  int i;
  for (i=0; i < TM_N; i++) {
    G(L)->tmname[i] = luaS_new(L, luaT_eventname[i]);
    luaS_fix(G(L)->tmname[i]);  /* never collect these names */
  }
}


/*
** function to be used with macro "fasttm": optimized for absence of
** tag methods
*/
const TValue *luaT_gettm (Table *events, TMS event, TString *ename) {
  const TValue *tm = luaH_getstr(events, ename);
  lua_assert(event <= TM_EQ);
  if (ttisnil(tm)) {  /* no tag method? */
    events->flags |= cast_byte(1u << event);  /* cache this fact */
    return NULL;
  }
  else return tm;
}


const TValue *luaT_gettmbyobj (lua_State *L, const TValue *o, TMS event) {
  Table *mt;
  switch (ttype(o)) {
    case LUA_TTABLE:
      mt = hvalue(o)->metatable;
      break;
    case LUA_TUSERDATA:
      mt = uvalue(o)->metatable;
      break;
    case LUA_TSEQ:
      mt = seqvalue(o)->metatable;
      break;
    case LUA_TSET:
      mt = usvalue(o)->metatable;
      break;
    case LUA_TPAIR:
      mt = pairvalue(o)->metatable;
      break;
    case LUA_TREG:
      mt = regvalue(o)->metatable;
      break;
    case LUA_TFUNCTION: { /* 2.36.2 */
      Closure *fcl = clvalue(o);
      mt = (fcl->c.isC) ? fcl->c.metatable : fcl->l.metatable;
      break;
    }
    default:
      mt = G(L)->mt[ttype(o)];
  }
  return (mt ? luaH_getstr(mt, G(L)->tmname[event]) : luaO_nilobject);
}

