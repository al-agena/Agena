/*
** $Id: lapi.c,v 2.55 2006/06/07 12:37:17 roberto Exp $
** Lua API
** See Copyright Notice in agena.h
*/

#include <assert.h>
#include "prepdefs.h"

#if (defined(__SOLARIS) || defined(__linux__) || defined(__APPLE__))  /* 2.8.4, trunc, log2, exp2, fma, fdim */
#include <tgmath.h>
#endif

#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>  /* C99 will complain otherwise */
#include <ctype.h>  /* for isdigit */

#ifndef __DJGPP__
#include <fenv.h>   /* for feg/setround */
#endif

#define lapi_c
#define LUA_CORE

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnconf.h"
#include "agnhlps.h"  /* for tools_isfrac */
#include "agnxlib.h"

#include "charbuf.h"
#include "lapi.h"
#include "lcomplex.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lpair.h"
#include "lreg.h"
#include "lseq.h"
#include "lset.h"
#include "lstate.h"
#include "lstring.h"
#include "lstrlib.h"
#include "ltable.h"
#include "ltm.h"
#include "lucase.def"
#include "lundump.h"
#include "lvm.h"

const char lua_ident[] =
  "$Agena: " AGENA_RELEASE " " AGENA_COPYRIGHT " $\n"
  "$Authors: " LUA_AUTHORS " $\n"
  "$URL: agena.sourceforge.net $\n";

#define api_checknelems(L, n)   api_check(L, (n) <= (L->top - L->base))

#define api_checkvalidindex(L, i)   api_check(L, (i) != luaO_nilobject)

/* Converting index2adr to a macro or declaring it inline (ala __attribute__((always_inline)))
   actually slows down the interpreter significantly by 6 % */
LUA_API TValue *index2adr (lua_State *L, int idx) {
  if (idx > 0) {
    TValue *o = L->base + (idx - 1);
    api_check(L, idx <= L->ci->top - L->base);
    if (o >= L->top) return cast(TValue *, luaO_nilobject);
    else return o;
  }
  else if (idx > LUA_REGISTRYINDEX) {
    api_check(L, idx != 0 && -idx <= L->top - L->base);
    return L->top + idx;
  }
  else switch (idx) {  /* pseudo-indices */
    case LUA_REGISTRYINDEX: return registry(L);
    case LUA_ENVIRONINDEX: {
      Closure *func = curr_func(L);
      sethvalue(L, &L->env, func->c.env);
      return &L->env;
    }
    case LUA_GLOBALSINDEX: return gt(L);
    case LUA_CONSTANTSINDEX: return constants(L);
    default: {  /* upvalues */
      Closure *func = curr_func(L);
      idx = lua_upvalueindex(idx);  /* changed 2.20.0 */
      /* idx now is positive */
      return (idx <= func->c.nupvalues)
                ? &func->c.upvalue[idx - 1]
                : cast(TValue *, luaO_nilobject);
    }
  }
}

static Table *getcurrenv (lua_State *L) {
  if (L->ci == L->base_ci)  /* no enclosing function? */
    return hvalue(gt(L));  /* use global table as environment */
  else {
    Closure *func = curr_func(L);
    return func->c.env;
  }
}


LUA_API void luaA_pushobject (lua_State *L, const TValue *o) {
  setobj2s(L, L->top, o);
  api_incr_top(L);
}


LUA_API void luaA_pushgcobject (lua_State *L, GCObject *o) {  /* 7.5.14 */
  switch (o->gch.tt) {
    case LUA_TTABLE: {
      Table *h = gco2h(o);
      sethvalue(L, L->top, h);
      break;
    }
    case LUA_TSET: {
      UltraSet *us = gco2l(o);
      setusvalue(L, L->top, us);
      break;
    }
    case LUA_TSEQ: {
      Seq *s = gco2a(o);
      setseqvalue(L, L->top, s);
      break;
    }
    case LUA_TREG: {
      Reg *r = gco2r(o);
      setregvalue(L, L->top, r);
      break;
    }
    case LUA_TPAIR: {
      Pair *p = gco2pair(o);
      setpairvalue(L, L->top, p);
      break;
    }
    case LUA_TFUNCTION: {
      Closure *cl = gco2cl(o);
      setclvalue(L, L->top, cl);
      break;
    }
    case LUA_TUSERDATA: {
      Udata *ud = rawgco2u(o);
      setuvalue(L, L->top, ud);
      break;
    }
    case LUA_TPROTO: {
      Proto *p = gco2p(o);
      setptvalue(L, L->top, p);
      break;
    }
    case LUA_TTHREAD: {
      lua_State *th = gco2th(o);
      if (th != L && th != G(L)->mainthread) {
        setthvalue(L, L->top, th);
      } else {
        setfailvalue(L->top);
      }
      break;
    }
    case LUA_TSTRING: {
      TString *ts = rawgco2ts(o);
      setsvalue(L, L->top, ts);
      break;
    }
    case LUA_TUPVAL: {
      /* UpVal *uv = gco2uv(o); */
      setfailvalue(L->top);
      break;
    }
    default: {
      setnilvalue(L->top);
    }
  }
  api_incr_top(L);
}


LUA_API int lua_checkstack (lua_State *L, int size) {
  /* 5.1.4 patch 3 */
  int res = 1;
  lua_lock(L);
  if (size > LUAI_MAXCSTACK || (L->top - L->base + size) > LUAI_MAXCSTACK)
    res = 0;  /* stack overflow */
  else if (size > 0) {
    luaD_checkstack(L, size);
    if (L->ci->top < L->top + size)
      L->ci->top = L->top + size;
  }
  lua_unlock(L);
  return res;
}


LUA_API void lua_xmove (lua_State *from, lua_State *to, int n) {
  int i;
  if (from == to) return;
  lua_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to));
  api_check(from, to->ci->top - to->top >= n);
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobj2s(to, to->top++, from->top + i);
  }
  lua_unlock(to);
}


LUA_API lua_CFunction lua_atpanic (lua_State *L, lua_CFunction panicf) {
  lua_CFunction old;
  lua_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  lua_unlock(L);
  return old;
}


LUA_API lua_State *lua_newthread (lua_State *L) {
  lua_State *L1;
  lua_lock(L);
  luaC_checkGC(L);
  L1 = luaE_newthread(L);
  setthvalue(L, L->top, L1);
  api_incr_top(L);
  lua_unlock(L);
  luai_userstatethread(L, L1);
  return L1;
}


/*
** basic stack manipulation
*/

/*
** convert an acceptable stack index into an absolute index; taken from Lua 5.4.0 RC 2, 2.21.2
*/
LUA_API int lua_absindex (lua_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top - L->ci->func) + idx;
}


LUA_API int lua_gettop (lua_State *L) {
  return cast_int(L->top - L->base);
}


LUA_API void lua_settop (lua_State *L, int idx) {
  lua_lock(L);
  if (idx >= 0) {
    api_check(L, idx <= L->stack_last - L->base);
    while (L->top < L->base + idx)
      setnilvalue(L->top++);
    L->top = L->base + idx;
  }
  else {
    api_check(L, -(idx + 1) <= (L->top - L->base));
    L->top += idx + 1;  /* `subtract' index (index is negative) */
  }
  lua_unlock(L);
}


LUA_API void lua_remove (lua_State *L, int idx) {
  StkId p;
  lua_lock(L);
  p = index2adr(L, idx);
  api_checkvalidindex(L, p);
  while (++p < L->top) setobjs2s(L, p - 1, p);
  L->top--;
  lua_unlock(L);
}


LUA_API void lua_insert (lua_State *L, int idx) {
  StkId p, q;
  lua_lock(L);
  p = index2adr(L, idx);
  api_checkvalidindex(L, p);
  for (q = L->top; q > p; q--) setobjs2s(L, q, q - 1);
  setobjs2s(L, p, L->top);
  lua_unlock(L);
}


LUA_API void lua_replace (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  /* explicit test for incompatible code */
  if (idx == LUA_ENVIRONINDEX && L->ci == L->base_ci)
    luaG_runerror(L, "Error: no calling environment.");
  api_checknelems(L, 1);
  o = index2adr(L, idx);
  api_checkvalidindex(L, o);
  if (idx == LUA_ENVIRONINDEX) {
    Closure *func = curr_func(L);
    api_check(L, ttistable(L->top - 1));
    func->c.env = hvalue(L->top - 1);
    luaC_barrier(L, func, L->top - 1);
  } else {
    setobj(L, o, L->top - 1);
    if (idx < LUA_CONSTANTSINDEX)  /* function upvalue ?  Fix 2.20.3 */
      luaC_barrier(L, curr_func(L), L->top - 1);
  }
  L->top--;
  lua_unlock(L);
}


LUA_API void lua_pushvalue (lua_State *L, int idx) {
  lua_lock(L);
  setobj2s(L, L->top, index2adr(L, idx));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushcachevalue (lua_State *L, int idx) {
  StkId p;
  lua_lock(L);
  p = ((idx < 0) ? L->C->top : L->C->base) + idx;
  if (ttisstring(p)) {  /* 7.1.6 fix: strings must be duplicated when copied from L->C to L as they are collectible */
    const char *str = svalue(p);
    int len = tsvalue(p)->len;
    setsvalue(L, L->top, luaS_newlstr(L, str, len));
  } else {  /* numbers, booleans */
    setobj2s(L, L->top, p);
  }
  api_incr_top(L);
  lua_unlock(L);
}


/*
** access functions (stack -> C)
*/


LUA_API int lua_type (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  return (o == luaO_nilobject) ? LUA_TNONE : ttype(o);
}


LUA_API const char *lua_typename (lua_State *L, int t) {
  UNUSED(L);
  return (t == LUA_TNONE) ? "no value" : luaT_typenames[t];
}


LUA_API int lua_iscfunction (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  return iscfunction(o);
}


LUA_API int lua_isnumber (lua_State *L, int idx) {  /* tuned by 40 %, 2.21.1 */
  lua_Number n;
  const TValue *o = index2adr(L, idx);
  return tonumberx(o, &n);
}


LUA_API int agn_iscomplex (lua_State *L, int idx) {  /* 4.9.3 */
  return ttype((index2adr(L, idx))) == LUA_TCOMPLEX;
}


LUA_API int lua_isstring (lua_State *L, int idx) {
  int t = lua_type(L, idx);
  return (t == LUA_TSTRING || t == LUA_TNUMBER);
}


LUA_API int lua_isuserdata (lua_State *L, int idx) {
  const TValue *o = index2adr(L, idx);
  return (ttisuserdata(o) || ttislightuserdata(o));
}


LUA_API int lua_rawequal (lua_State *L, int index1, int index2) {
  StkId o1 = index2adr(L, index1);
  StkId o2 = index2adr(L, index2);
  return (o1 == luaO_nilobject || o2 == luaO_nilobject) ? 0
         : luaO_rawequalObj(o1, o2);
}


LUA_API int lua_rawaequal (lua_State *L, int index1, int index2) {  /* 2.3.0 RC 3 */
  StkId o1 = index2adr(L, index1);
  StkId o2 = index2adr(L, index2);
  return (o1 == luaO_nilobject || o2 == luaO_nilobject) ? 0
         : luaV_approxequalvalonebyone(L, o1, o2);
}


LUA_API int lua_equal (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  lua_lock(L);  /* may call tag method */
  o1 = index2adr(L, index1);
  o2 = index2adr(L, index2);
  i = (o1 == luaO_nilobject || o2 == luaO_nilobject) ? 0 : equalobj(L, o1, o2);
  lua_unlock(L);
  return i;
}


LUA_API int agn_equalref (lua_State *L, int index1, int index2) {  /* 2.12.3, Lua 5.1.4's lua_equal API function */
  StkId o1, o2;
  int i;
  lua_lock(L);  /* may call tag method */
  o1 = index2adr(L, index1);
  o2 = index2adr(L, index2);
  i = (o1 == luaO_nilobject || o2 == luaO_nilobject) ? 0 : equalref(L, o1, o2);
  lua_unlock(L);
  return i;
}


LUA_API int lua_lessthan (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  lua_lock(L);  /* may call tag method */
  o1 = index2adr(L, index1);
  o2 = index2adr(L, index2);
  i = (o1 == luaO_nilobject || o2 == luaO_nilobject) ? 0
       : luaV_lessthan(L, o1, o2, 1);
  lua_unlock(L);
  return i;
}


LUA_API lua_Number lua_tonumber (lua_State *L, int idx) {  /* tuned by 40 %, 2.21.1 */
  int r;
  lua_Number n;
  const TValue *o = index2adr(L, idx);
  r = tonumberx(o, &n);
  return (r) ? n : 0;  /* 0 = failure, could not convert value */
}


LUA_API lua_Number agn_tonumber (lua_State *L, int idx) {
  const TValue *o = index2adr(L, idx);
  return nvalue(o);
}


/* do not delete this function since it is absolutely needed by `toNumber` to convert strings to numbers
   (agn_tonumber does not do this) */
LUA_API lua_Number agn_tonumberx (lua_State *L, int idx, int *exception) {  /* added October 18, 2008 */
  int r;  /* tuned 2.21.1 */
  lua_Number n;
  const TValue *o;
  o = index2adr(L, idx);
  r = tonumberx(o, &n);
  *exception = r == 0;
  return (r) ? n : 0;  /* 0 = failure, could not convert value */
}


#ifndef PROPCMPLX
LUA_API agn_Complex agn_tocomplexx (lua_State *L, int idx, int *exception) {  /* added 10.08.2009, 0.26.1 */
  TValue n;
  const TValue *o = index2adr(L, idx);
  if (tocomplex(o, &n)) {
    *exception = 0;
    return ttisnumber(o) ? nvalue(o) + 0*I : cvalue(o);
  } else {  /* failure, could not convert value */
    *exception = 1;
    return 0;
  }
}

LUA_API agn_Complex agn_tocomplex (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  return ttisnumber(o) ? nvalue(o) + 0*I : cvalue(o);  /* 4.11.11 fix */
}

LUA_API lua_Number agn_complexreal (lua_State *L, int idx) {  /* 2.21.8 */
  StkId o = index2adr(L, idx);
  return ttisnumber(o) ? nvalue(o) : creal(cvalue(o));  /* 4.11.11 fix */
}

LUA_API lua_Number agn_compleximag (lua_State *L, int idx) {  /* 2.21.8 */
  StkId o = index2adr(L, idx);
  return ttisnumber(o) ? 0 : cimag(cvalue(o));  /* 4.11.11 fix */
}

#else

LUA_API void agn_tocomplexx (lua_State *L, int idx, int *exception, lua_Number *a) {  /* added 10.08.2009, 0.26.1 */
  TValue n;
  const TValue *o = index2adr(L, idx);
  if (tocomplex(o, &n)) {
    *exception = 0;
    if (ttisnumber(o)) {  /* 4.11.11 fix */
      a[0] = nvalue(o);
      a[1] = 0;
    } else {
      a[0] = cvalue(o)[0];
      a[1] = cvalue(o)[1];
    }
  }
  else {  /* failure, could not convert value */
    a[0] = a[1] = 0;
    *exception = 1;
  }
}


LUA_API lua_Number agn_complexreal (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  return ttisnumber(o) ? nvalue(o) : cvalue(o)[0];  /* 4.11.11 fix */
}

LUA_API lua_Number agn_compleximag (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  return ttisnumber(o) ? 0 : cvalue(o)[1];  /* 4.11.11 fix */
}
#endif


/* Expects a complex number at stack position idx and returns its real and imaginary part in re and im.
   If the value at idx is a complex number, returns 1 and 0 otherwise. The function can be used both with
   Agena versions using standard complex.h complex functions and those using proprietary complex arithmetic.
   2.17.6 */
LUA_API int agn_getcmplxparts (lua_State *L, int idx, double *re, double *im) {
  if (lua_iscomplex(L, idx)) {
    StkId o = index2adr(L, idx);
#ifndef PROPCMPLX
    agn_Complex z = cvalue(o);
    *re = creal(z); *im = cimag(z);
#else
    *re = cvalue(o)[0]; *im = cvalue(o)[1];
#endif
    return 1;
  } else if (agn_isnumber(L, idx)) {  /* 2.31.6 */
    *re = agn_tonumber(L, idx);
    *im = 0;
    return 1;
  }
  return 0;
}


LUA_API int agn_getcmplxpartsl (lua_State *L, int idx, long double *re, long double *im) {  /* 6.6.9 */
  if (lua_iscomplex(L, idx)) {
    StkId o = index2adr(L, idx);
#ifndef PROPCMPLX
    agn_Complex z = cvalue(o);
    *re = (long double)creal(z); *im = (long double)cimag(z);
#else
    *re = (long double)(cvalue(o)[0]); *im = (long double)(cvalue(o)[1]);
#endif
    return 1;
  } else if (agn_isnumber(L, idx)) {  /* 2.31.6 */
    *re = (long double)agn_tonumber(L, idx);
    *im = 0.0L;
    return 1;
  }
  return 0;
}


LUA_API lua_Integer lua_tointeger (lua_State *L, int idx) {  /* tuned 2.21.1 */
  lua_Number n;
  const TValue *o = index2adr(L, idx);
  if (tonumberx(o, &n)) {
    lua_Integer res;
    lua_number2integer(res, n);
    return res;
  }
  else
    return 0;
}


LUA_API lua_Integer agn_tointeger (lua_State *L, int idx) {  /* 2.27.8 */
  const TValue *o = index2adr(L, idx);
  return (lua_Integer)(nvalue(o));
}


/* Signed 32-bit integers are in the range from -2,147,483,648 = -2^31 through 2,147,483,647 = 2^31-1.
   Unsigned 32-bit integers are between (including) 0 and 4294967295 = 2^32-1. */
LUA_API int32_t lua_toint32_t (lua_State *L, int idx) {  /* tuned 2.21.2, removed ARM correction in 4.12.5 */
  lua_Number n;
  const TValue *o = index2adr(L, idx);
  if (tonumberx(o, &n)) {
    /* 2.39.1, 4.12.5 patch for all platforms, clamp betw. -2^31 .. 2^31-1; we have to use +2^31 instead of 2^31-1
       as reducerange clamps between [min, max), not [min, max]. Note that this will overflow, that is if
       n = +2147483648, the result will be -2147483648.0, etc.; and if n = -2147483647, then the result is +2147483647. */
    n = tools_reducerange(n, -2147483648.0, 2147483648.0);
    return (int32_t)n;
  }
  else
    return 0;
}


LUA_API uint32_t lua_touint32_t (lua_State *L, int idx) {  /* 2.14.5, tuned 2.21.2, patched 2.31.1 for PPC, ARM, Apple */
  lua_Number n;
  const TValue *o = index2adr(L, idx);
  if (tonumberx(o, &n)) {
    n = tools_reducerange(n, 0, 4294967296.0);  /* 4.12.5 patch for all platforms, see above */
    return (uint32_t)n;
  }
  else
    return 0;
}


LUA_API off64_t lua_tooff64_t (lua_State *L, int idx) {  /* tuned 2.21.2 */
  lua_Number n;
  const TValue *o = index2adr(L, idx);
  if (tonumberx(o, &n)) {
    off64_t res;
    res = (off64_t)n;
    return res;
  }
  else
    return 0;
}


LUA_API int agn_isinteger (lua_State *L, int idx) {  /* 2.16.6 */
  StkId o = index2adr(L, idx);
  return (o == luaO_nilobject || ttype(o) != LUA_TNUMBER) ? 0 : tools_isint(nvalue(o));
}


LUA_API int agn_isposint (lua_State *L, int idx) {  /* 3.1.0 */
  StkId o = index2adr(L, idx);
  return (o == luaO_nilobject || ttype(o) != LUA_TNUMBER) ? 0 : tools_isposint(nvalue(o));
}


LUA_API int agn_isnonnegint (lua_State *L, int idx) {  /* 3.1.0 */
  StkId o = index2adr(L, idx);
  return (o == luaO_nilobject || ttype(o) != LUA_TNUMBER) ? 0 : tools_isnonnegint(nvalue(o));
}


LUA_API int agn_isnonzeroint (lua_State *L, int idx) {  /* 4.11.0 */
  StkId o = index2adr(L, idx);
  return (o == luaO_nilobject || ttype(o) != LUA_TNUMBER) ? 0 : tools_isnonzeroint(nvalue(o));
}


LUA_API int lua_toboolean (lua_State *L, int idx) {
  const TValue *o = index2adr(L, idx);
  return (l_isfail(o)) ? LUA_TFAIL : !l_isfalse(o);  /* LUA_TFAIL = -1; with non-booleans, returns !0 == 1 */
}


/* do not modify the string returned, it would change the input string */
LUA_API const char *lua_tolstring (lua_State *L, int idx, size_t *len) {
  StkId o = index2adr(L, idx);
  if (!ttisstring(o)) {
    lua_lock(L);  /* `luaV_tostring' may create a new string */
    if (!luaV_tostring(L, o)) {  /* conversion failed? */
      if (len != NULL) *len = 0;
      lua_unlock(L);
      return NULL;
    }
    luaC_checkGC(L);
    o = index2adr(L, idx);  /* previous call may reallocate the stack */
    lua_unlock(L);
  }
  if (len != NULL) *len = tsvalue(o)->len;
  return svalue(o);
}


LUA_API const char *agn_tostring (lua_State *L, int idx) {  /* 0.24.0 */
  StkId o = index2adr(L, idx);
  return svalue(o);
}


LUA_API size_t lua_objlen (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  switch (ttype(o)) {
    case LUA_TSTRING: return tsvalue(o)->len;
    case LUA_TUSERDATA: return uvalue(o)->len;
    case LUA_TTABLE: return luaH_getn(hvalue(o));
    case LUA_TNUMBER: {
      size_t l;
      lua_lock(L);  /* `luaV_tostring' may create a new string */
      l = (luaV_tostring(L, o) ? tsvalue(o)->len : 0);
      lua_unlock(L);
      return l;
    }
    default: return 0;
  }
}


LUA_API lua_CFunction lua_tocfunction (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  return (!iscfunction(o)) ? NULL : clvalue(o)->c.f;
}


LUA_API void *lua_touserdata (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  switch (ttype(o)) {
    case LUA_TUSERDATA: return (rawuvalue(o) + 1);
    case LUA_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


LUA_API lua_State *lua_tothread (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


LUA_API const void *lua_topointer (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  switch (ttype(o)) {
    case LUA_TTABLE: return hvalue(o);
    case LUA_TFUNCTION: return clvalue(o);
    case LUA_TTHREAD: return thvalue(o);
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
      return lua_touserdata(L, idx);
    case LUA_TSET: return usvalue(o);     /* added 0.10.0 */
    case LUA_TSEQ: return seqvalue(o);    /* added 0.11.0 */
    case LUA_TREG: return regvalue(o);    /* added 2.3.0 RC 3 */
    case LUA_TPAIR: return pairvalue(o);  /* added 0.11.1 */
    case LUA_TSTRING: return rawtsvalue(o);  /* added 7.5.14 */
    default: return NULL;
  }
}


/*
** push functions (C -> stack)
*/


LUA_API void lua_pushnil (lua_State *L) {
  lua_lock(L);
  setnilvalue(L->top);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushnumber (lua_State *L, lua_Number n) {
  lua_lock(L);
  setnvalue(L->top, n);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushundefined (lua_State *L) {
  lua_lock(L);
  setnvalue(L->top, AGN_NAN);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushinteger (lua_State *L, lua_Integer n) {
  lua_lock(L);
  setnvalue(L->top, cast_num(n));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushlstring (lua_State *L, const char *s, size_t len) {
  lua_lock(L);
  luaC_checkGC(L);
  setsvalue2s(L, L->top, luaS_newlstr(L, s, len));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushstring (lua_State *L, const char *s) {
  if (s == NULL)
    lua_pushnil(L);
  else
    lua_pushlstring(L, s, tools_strlen(s));  /* 2.17.8 tweak */
}


LUA_API void lua_pushchar (lua_State *L, char s) {  /* 2.12.0 RC 2 */
  char str[2];
  str[0] = s;
  str[1] = '\0';
  lua_pushstring(L, str);
}


LUA_API const char *lua_pushvfstring (lua_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  lua_lock(L);
  luaC_checkGC(L);
  ret = luaO_pushvfstring(L, fmt, argp);
  lua_unlock(L);
  return ret;
}


LUA_API const char *lua_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  lua_lock(L);
  luaC_checkGC(L);
  va_start(argp, fmt);
  ret = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  lua_unlock(L);
  return ret;
}


LUA_API void lua_pushcclosure (lua_State *L, lua_CFunction fn, int n) {
  Closure *cl;
  lua_lock(L);
  luaC_checkGC(L);
  api_checknelems(L, n);
  cl = luaF_newCclosure(L, n, getcurrenv(L));
  cl->c.f = fn;
  L->top -= n;
  while (n--)
    setobj2n(L, &cl->c.upvalue[n], L->top + n);
  setclvalue(L, L->top, cl);
  lua_assert(iswhite(obj2gco(cl)));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushboolean (lua_State *L, int b) {
  lua_lock(L);
  setbvalue(L->top, b != 0);  /* ensure that true is 1 */
  api_incr_top(L);
  lua_unlock(L);
}


/* b: -1 = fail, 0 = false, 1 = true */
LUA_API void agn_pushboolean (lua_State *L, int b) {  /* 16.01.2013 */
  lua_lock(L);
  setbvalue(L->top, (b == -1) + (b != 0));  /* ensure that true is 1, no branching 2.17.4 */
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushlightuserdata (lua_State *L, void *p) {
  lua_lock(L);
  setpvalue(L->top, p);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API int lua_pushthread (lua_State *L) {
  lua_lock(L);
  setthvalue(L, L->top, L);
  api_incr_top(L);
  lua_unlock(L);
  return (G(L)->mainthread == L);
}


/*
** get functions (Lua -> stack)
*/


/* taken from Lua 5.4.0 RC 4, 2.21.2, 5 % faster */
LUA_API int lua_gettable (lua_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  lua_lock(L);
  t = index2value(L, idx);
  if (luaV_fastget(L, t, s2v(L->top - 1), slot, luaH_get)) {
    setobj2s(L, L->top - 1, slot);
  } else
    luaV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  lua_unlock(L);
  return ttype(s2v(L->top - 1));
}


/* taken from Lua 5.4.0 RC 4, 2.21.2, 10 % faster */
LUA_API int lua_getfield (lua_State *L, int idx, const char *k) {
  const TValue *slot, *t;
  TString *str;
  lua_lock(L);
  t = index2value(L, idx);
  str = luaS_new(L, k);
  if (luaV_fastget(L, t, str, slot, luaH_getstr)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  } else {
    setsvalue2s(L, L->top, str);
    api_incr_top(L);
    luaV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  lua_unlock(L);
  return ttype(s2v(L->top - 1));
}


/* Checks whether a table at index idx includes a given field (a string key) and returns 0 (false) or 1 (true).
   Metamethods are ignored. The stack is not changed. 2.21.0 */
LUA_API int lua_hasfield (lua_State *L, int idx, const char *k) {
  StkId t;
  TValue key;
  int r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_checkvalidindex(L, t);
  setsvalue(L, &key, luaS_new(L, k));
  r = ttisnotnil(luaH_get(hvalue(t), &key));  /* do a primitive get */
  lua_unlock(L);
  return r;
}


/* Pushes onto the stack the value t[i], where t is the value at the given index. As in Lua, this function may trigger a
   metamethod for the "index" event. Returns the type of the pushed value. 2.21.1, taken from Lua 5.4.0 RC 4 */
LUA_API int lua_geti (lua_State *L, int idx, lua_Integer n) {
  TValue *t;
  const TValue *slot;
  lua_lock(L);
  t = index2value(L, idx);
  if (luaV_fastgeti(L, t, n, slot)) {
    setobj2s(L, L->top, slot);
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    luaV_finishget(L, t, &aux, L->top, slot);
  }
  api_incr_top(L);
  lua_unlock(L);
  return ttype(s2v(L->top - 1));
}


LUA_API int agn_arrayorhashgeti (lua_State *L, int idx, lua_Integer n, int array) {  /* 2.37.3 */
  TValue *t;
  Table *h;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttistable(t));
  h = hvalue(t);
  if (array) {
    /* (1 <= key && key <= t->sizearray) */
    setobj2s(L, L->top, (cast(unsigned int, n - 1) < cast(unsigned int, h->sizearray)) ? &h->array[n - 1] : luaO_nilobject);
  } else {
    const TValue *p = NULL;
    lua_Number nk = cast_num(n);
    Node *n = luaH_hashnum(h, nk);
    do {  /* check whether `key' is somewhere in the chain */
      if (ttisnumber(gkey(n)) && luai_numeq(nvalue(gkey(n)), nk)) {
        p = gval(n);  /* that's it */
        break;
      } else
        n = gnext(n);
    } while (n);
    if (!p) p = luaO_nilobject;
    setobj2s(L, L->top, p);
  }
  api_incr_top(L);
  lua_unlock(L);
  return ttype(s2v(L->top - 1));  /* return result type */
}


/* Does the equivalent to t[n] = v, where t is the value at the given index and v is the value at the top of the stack.
   This function pops the value from the stack. As in Lua, this function may trigger a metamethod for the "newindex" event.
   2.21.1, taken from Lua 5.4.0 RC 4 */
LUA_API void lua_seti (lua_State *L, int idx, lua_Integer n) {
  TValue *t;
  const TValue *slot;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2value(L, idx);
  if (luaV_fastgeti(L, t, n, slot)) {
    luaV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    luaV_finishset(L, t, &aux, s2v(L->top - 1), slot);
  }
  L->top--;  /* pop value */
  lua_unlock(L);
}


LUA_API int lua_rawget (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttistable(t));
  setobj2s(L, L->top - 1, luaH_get(hvalue(t), L->top - 1));
  lua_unlock(L);
  return ttype(s2v(L->top - 1));  /* from Lua 5.4.0 RC 4; 2.21.1 */
}


/* check whether the value at the stack top exists in the set at stack index idx. Pops the value and puts true or false in its place. */
LUA_API int lua_srawget (lua_State *L, int idx) {  /* 0.10.0 */
  StkId t;
  int r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisuset(t));
  r = agnUS_get(usvalue(t), L->top - 1);
  setbvalue(L->top - 1, r);
  lua_unlock(L);
  return r;  /* 2.12.3 */
}


/* Checks whether the value at the stack top exists in the set at stack index idx. Returns 0 or 1. If pop is 1, pops the value, otherwise leaves
   the stack unchanged. */
LUA_API int lua_shas (lua_State *L, int idx, int pop) {  /* 2.31.5 */
  StkId t;
  int r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisuset(t));
  r = agnUS_get(usvalue(t), L->top - 1);
  L->top -= pop;
  lua_unlock(L);
  return r;  /* 2.12.3 */
}


LUA_API void lua_seqrawget (lua_State *L, int idx, int pushnil) {
  StkId t;
  const TValue *r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  r = agnSeq_rawgeti(seqvalue(t), L->top - 1);
  if (r != NULL) {
    setobj2s(L, L->top - 1, r);
  } else if (pushnil) {
    setobj2s(L, L->top - 1, luaO_nilobject);
  } else {
    luaG_runerror(L, "Error: index out of range.");
  }
  lua_unlock(L);
}


LUA_API void agn_regrawgeti (lua_State *L, int idx, size_t n) {  /* 2.3.0 RC 3 */
  StkId t;
  const TValue *r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisreg(t));
  r = agnReg_geti(regvalue(t), n);
  if (r != NULL) {
    setobj2s(L, L->top, r);
    api_incr_top(L);
  }
  else
    luaG_runerror(L, "Error: index out of %s range.", (regvalue(t)->top < regvalue(t)->maxsize) ? "current" : "\b");
  lua_unlock(L);
}


LUA_API void agn_reggetinoerr (lua_State *L, int idx, size_t n) {  /* 3.20.0 */
  StkId t;
  const TValue *r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisreg(t));
  r = agnReg_geti(regvalue(t), n);
  if (r == NULL) {
    setnilvalue(L->top);
  } else {
    setobj2s(L, L->top, r);
  }
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void agn_reggetinoerrrange (lua_State *L, int idx, int p, int q) {  /* 3.20.0 */
  StkId t;
  Reg *s;
  const TValue *r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisreg(t));
  s = regvalue(t);
  r = agnReg_geti(s, p);
  if (r == NULL) { setnilvalue(L->top); } else { setobj2s(L, L->top, r); }
  api_incr_top(L);
  while (p++ < q) {  /* push arg[i + 1...e] */
    r = agnReg_geti(s, p);
    if (r == NULL) { setnilvalue(L->top); } else { setobj2s(L, L->top, r); }
    api_incr_top(L);
  }
  lua_unlock(L);
}


LUA_API lua_Number agn_reggetinumber (lua_State *L, int idx, int n) {  /* 2.3.0 RC 3, tuned by 4 % 2.21.2 */
  StkId t;
  const TValue *o;
  lua_Number p;
  t = index2adr(L, idx);
  api_check(L, ttisreg(t));
  o = agnReg_geti(regvalue(t), n);
  if (tonumberx(o, &p))
    return nvalue(o);
  else
    return 0;  /* changed 2.75.5 */
}


LUA_API lua_Number agn_regrawgetinumber (lua_State *L, int idx, int n, int *rc) {  /* 2.27.5 */
  StkId t;
  const TValue *o;
  lua_Number p;
  t = index2adr(L, idx);
  api_check(L, ttisreg(t));
  o = agnReg_geti(regvalue(t), n);
  if ( (*rc = tonumberx(o, &p)) )
    return nvalue(o);
  else
    return 0;  /* 4.4.7 change */
}


/* Returns the integer in the register at stack index idx at position n, with starting n starting from 1. If successful,
   sets rc to 1 and 0 otherwise. The return is zero if the element is not an integer.
   See also: agn_rawgetiinteger, agn_seqrawgetiinteger. */
LUA_API lua_Number agn_regrawgetiinteger (lua_State *L, int idx, int n, int *rc) {  /* 4.12.7 */
  StkId t;
  const TValue *o;
  lua_Number p;
  t = index2adr(L, idx);
  api_check(L, ttisreg(t));
  o = agnReg_geti(regvalue(t), n);
  if ( (*rc = tonumberx(o, &p)) ) {
    if (tools_isint(p)) return p;
    *rc = 0;
  }
  return 0;
}


LUA_API const char *agn_regrawgetilstring (lua_State *L, int idx, int n, size_t *len, int *rc) {  /* 4.11.1 */
  StkId t;
  const TValue *o;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisreg(t));
  o = agnReg_geti(regvalue(t), n);
  *rc = ttype(o) == LUA_TSTRING;
  lua_unlock(L);
  if (*rc) {
    if (len != NULL) *len = tsvalue(o)->len;
    return svalue(o);
  } else {
    if (len != NULL) *len = 0;
    return NULL;
  }
}


/* Returns the real and imaginary parts of the number or complex number that resides at index n of table t at stack index idx.
   The real part is stored in z[0], the imaginary part in z[1]. The function also returns the type of value found at t[n] in rc:
   LUA_TNUMBER if t[n] is a number, LUA_TCOMPLEX if t[n] is a complex number or LUA_TNIL if t[n] is any other value. In the
   latter case (LUA_TNIL), z[0] = z[1] = 0. If t[n] is a number, z[1] is set to zero. 4.5.0 */
LUA_API void agn_regrawgeticomplex (lua_State *L, int idx, int n, lua_Number *z, int *rc) {
  StkId t;
  TValue *p;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisreg(t));
  /* older GCC compilers weirdly complain with complexreal & compleximag: `assignment of read-only variable`, so `unconst` */
  p = (TValue *)agnReg_geti(regvalue(t), n);
  if ( (*rc = ttype(p)) == LUA_TNUMBER ) {
    z[0] = nvalue(p); z[1] = 0;
  } else if ( (*rc = ttype(p)) == LUA_TCOMPLEX ) {
    z[0] = complexreal(p); z[1] = compleximag(p);
  } else {
    z[0] = 0; z[1] = 0; *rc = LUA_TNIL;
  }
  lua_unlock(L);
}


LUA_API void agn_regrawget (lua_State *L, int idx, int pushnil) {  /* 2.3.0 RC 3 */
  StkId t;
  const TValue *r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisreg(t));
  r = agnReg_rawgeti(regvalue(t), L->top - 1);
  if (r != NULL) {
    setobj2s(L, L->top - 1, r);
  } else if (pushnil) {
    setobj2s(L, L->top - 1, luaO_nilobject);
  } else
    luaG_runerror(L, "Error: index out of %s range.", (regvalue(t)->top < regvalue(t)->maxsize) ? "current" : "\b");
  lua_unlock(L);
}


/* Extends or shrinks the given register at stack index idx to - and not by - the given number of elements n.

   When extending, all the elements already residing in r are kept.

   When shrinking, all surplus elements are removed. If the current top pointer is greater than n, it is reset to n.

   Returns 1 if the register has been shrunk or extended, and 0 if n is equal to the maximum (!) size.

   2.3.0 RC 3 / 4.6.4, `nil' will not be accepted on Mac OS X, so use the name `nilthem'. */
LUA_API int agn_regresize (lua_State *L, int idx, int newsize, int nilthem) {
  int r;
  StkId t;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisreg(t));
  r = agnReg_resize(L, regvalue(t), newsize, nilthem);
  lua_unlock(L);
  return r;
}


LUA_API void agn_seqresize (lua_State *L, int idx, size_t newsize) {  /* 2.3.0 RC 4 */
  StkId t;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  agnSeq_resize(L, seqvalue(t), newsize);
  lua_unlock(L);
}


#define actnodesize(t)    ( ((t)->lsizenode) == 0 ? 0 : (1 << ((t)->lsizenode)))  /* patched Agena 1.0.3 */

/* Resizes the set at stack index `idx` to `newsize` pre-allocated slots. `protect` controls whether to allow only a set to be shrunk
   without dropping any elements (protect = 1), shrunk or enlarged without dropping any elements (protect == 2), or whether to have
   full control what may happen: shrinking or expanding, dropping or not dropping any elements (protect == 0). With protect == 0, it is
   advised that the set is empty. 2.33.4 */
LUA_API void agn_setresize (lua_State *L, int idx, size_t newsize, int protect) {
  StkId t;
  UltraSet *us;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisuset(t));
  us = usvalue(t);
  if ( protect == 0 ||  /* resize whatever might happen, should be chosen only if set is empty */
      (protect == 1 && newsize >= us->size && newsize < actnodesize(us)) ||  /* only shrink if possible, never drop anything */
      (protect == 2 && newsize >= us->size && newsize != actnodesize(us)) )  /* enlarge or shrink if possible, never drop anything */
    agnUS_resize(L, us, newsize);
  lua_unlock(L);
}


/* Resizes the array part of a table to the given number newsize of pre-allocated slots.
   If you do not know whether there are still non-null values beyond the new size in the array part,
   then set checkholes to 1 instead of 0 as otherwise Agena will crash in such situations. If set
   to 1, the function will not resize the table if there are any excess non-null values. */
LUA_API void agn_tabresize (lua_State *L, int idx, size_t newsize, int checkholes) {  /* 2.29.0 */
  StkId t;
  Table *h;
  int lastnonnil;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttistable(t));
  h = hvalue(t);
  if (newsize == h->sizearray || newsize == 0) {
    lua_unlock(L);
    return;
  }
  if (checkholes) {
    agnH_hasholes(L, h, &lastnonnil);
    if (lastnonnil >= newsize) {
      lua_unlock(L);
      return;
    }
  }
  luaH_resizearray(L, h, newsize);
  lua_unlock(L);
}


/* Resizes the size of the array part of the table at stack index idx to nasize allocated slots. nasize must be a
   non-negative integer. If nasize is less than the number of currently stored values in the array part, surplus
   values are cut off. If the array part includes embedded `null`'s, they are removed shifting down the other
   elements to close the space. The function leaves the hash part unchanged. 4.6.3 */
LUA_API void agn_resize (lua_State *L, int idx, int nasize) {
  StkId o;
  Table *h;
  TValue *val;
  int i, c, oasize;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  h = hvalue(o);
  oasize = h->sizearray;
  c = 0;
  /* remove holes in array part if they exist */
  for (i=0; i < oasize && c < nasize; i++) {
    val = &h->array[i];
    if (!ttisnil(val)) {
      if (++c - 1 != i) {
        /* this always sets the value into the array part, but only if needed, that is
           if there are holes in the array part so that values really need to be shifted */
        luaH_setint(L, h, c, val);  /* the value has already been GC-registered */
        setnilvalue(val);  /* delete shifted value from original position */
      }
    }
  }
  /* we have to explicitly null surplus elements (if existing) in the table array, otherwise the function will crash. */
  for (; i < oasize; i++) {
    setnilvalue(&h->array[i]);
  }
  luaH_resizearray(L, h, nasize);
  lua_unlock(L);
}


/* agn_pairrawget: idx should be a stack value k with k=1 for the left hand side, or
   k=2 for the right hand side */
LUA_API void agn_pairrawget (lua_State *L, int idx) {
  StkId t;
  const TValue *r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttispair(t));
  r = agnPair_rawgeti(pairvalue(t), L->top - 1);
  if (r != NULL) {
    setobj2s(L, L->top - 1, r);
  }
  else
    luaG_runerror(L, "Error, index out of range.");
  lua_unlock(L);
}


/* Pushes onto the stack the sequence value t[n], where t is the sequence at the given valid index idx.
   The function does not invoke any metamethods. Contrary to lua_rawgeti, it issues an error if n is
   out of range.*/
LUA_API void lua_seqrawgeti (lua_State *L, int idx, size_t n) {  /* patched 0.31.6 */
  StkId t;
  const TValue *r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  r = agnSeq_geti(seqvalue(t), n);
  if (r != NULL) {
    setobj2s(L, L->top, r);
    api_incr_top(L);
  }
  else
    luaG_runerror(L, "Error, index out of range.");
  lua_unlock(L);
}


LUA_API void lua_seqrawgetinoerr (lua_State *L, int idx, size_t n) {  /* 3.20.0 */
  StkId t;
  const TValue *r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  r = agnSeq_geti(seqvalue(t), n);
  if (r == NULL) {
    setnilvalue(L->top);
  } else {
    setobj2s(L, L->top, r);
  }
  api_incr_top(L);
  lua_unlock(L);
}


/* Pushes all the sequence members from index p to index q (p > 0 & q > 0, p <= q) onto the stack.
   If q is out-of-bound, returns `null` instead of issuing an error or crashing.
   YOU MUST PROVIDE ENOUGH STACK SPACE ! 3.20.0 */
LUA_API void lua_seqrawgetinoerrrange (lua_State *L, int idx, int p, int q) {
  StkId t;
  Seq *s;
  const TValue *r;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  s = seqvalue(t);
  r = agnSeq_geti(s, p);
  /* push arg[i] (avoiding overflow problems) */
  if (r == NULL) { setnilvalue(L->top); } else { setobj2s(L, L->top, r); }
  api_incr_top(L);
  while (p++ < q) {  /* push arg[i + 1...e] */
    r = agnSeq_geti(s, p);
    if (r == NULL) { setnilvalue(L->top); } else { setobj2s(L, L->top, r); }
    api_incr_top(L);
  }
  lua_unlock(L);
}


LUA_API lua_Number lua_seqrawgetinumber (lua_State *L, int idx, int n) {  /* 0.12.0, tuned by 4 % 2.21.2 */
  StkId t;
  const TValue *o;
  lua_Number p;
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  o = agnSeq_geti(seqvalue(t), n);
  if (tonumberx(o, &p))
    return p;
  else
    return HUGE_VAL;
}


LUA_API lua_Number agn_seqgetinumber (lua_State *L, int idx, int n) {  /* 2.2.0 RC 2, tuned by 4 % 2.21.2 */
  StkId t;
  const TValue *o;
  lua_Number p;
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  o = agnSeq_geti(seqvalue(t), n);
  if (tonumberx(o, &p))
    return p;
  else
    return 0.0;
}


LUA_API lua_Number agn_seqrawgetinumber (lua_State *L, int idx, int n, int *rc) {  /* 2.27.5 */
  StkId t;
  const TValue *o;
  lua_Number p;
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  o = agnSeq_geti(seqvalue(t), n);
  if ( (*rc = tonumberx(o, &p)) )
    return p;
  else
    return 0.0;  /* 4.4.7 change */
}


/* Returns the integer in the sequence at stack index idx at position n, with starting n starting from 1. If successful,
   sets rc to 1 and 0 otherwise. The return is zero if the element is not an integer.
   See also: agn_rawgetiinteger, agn_regrawgetiinteger */
LUA_API lua_Number agn_seqrawgetiinteger (lua_State *L, int idx, int n, int *rc) {  /* 4.12.7 */
  StkId t;
  const TValue *o;
  lua_Number p;
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  o = agnSeq_geti(seqvalue(t), n);
  if ( (*rc = tonumberx(o, &p)) ) {
    if (tools_isint(p)) return p;
    *rc = 0;
  }
  return 0.0;
}


LUA_API void agn_seqrawgeticomplex (lua_State *L, int idx, int n, lua_Number *z, int *rc) {  /* 4.5.0 */
  StkId t;
  TValue *p;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  /* older GCC compilers weirdly complain with complexreal & compleximag: `assignment of read-only variable`, so `unconst` */
  p = (TValue *)agnSeq_geti(seqvalue(t), n);
  if ( (*rc = ttype(p)) == LUA_TNUMBER ) {
    z[0] = nvalue(p); z[1] = 0;
  } else if ( (*rc = ttype(p)) == LUA_TCOMPLEX ) {
    z[0] = (lua_Number)complexreal(p); z[1] = (lua_Number)compleximag(p);
  } else {
    z[0] = 0; z[1] = 0; *rc = LUA_TNIL;
  }
  lua_unlock(L);
}


LUA_API const char *agn_seqrawgetilstring (lua_State *L, int idx, int n, size_t *len, int *rc) {  /* 4.11.1 */
  StkId t;
  const TValue *o;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  o = agnSeq_geti(seqvalue(t), n);
  *rc = ttype(o) == LUA_TSTRING;
  lua_unlock(L);
  if (*rc) {
    if (len != NULL) *len = tsvalue(o)->len;
    return svalue(o);
  } else {
    if (len != NULL) *len = 0;
    return NULL;
  }
}


LUA_API void agn_createpair (lua_State *L, int idxleft, int idxright) {  /* 0.11.1; tuned 0.12.2 */
  StkId l, r;
  lua_lock(L);
  l = index2adr(L, idxleft);
  r = index2adr(L, idxright);
  luaC_checkGC(L);  /* 2.34.4, call GC before pair creation */
  setpairvalue(L, L->top, agnPair_create(L, l, r));
  api_incr_top(L);
  lua_unlock(L);
}


/* Sets the value at the stack top to the pair at index idx and pops the value. If pos is 1, then the value is put into the left part of the pair,
   if pos is 2, then the right part is set. Returns 0 = index out of range, 1 = success. */
LUA_API int agn_pairseti (lua_State *L, int idx, int pos) {  /* 2.29.0 */
  StkId t;
  int rc;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2adr(L, idx);
  api_check(L, ttispair(t));
  rc = agnPair_seti(L, pairvalue(t), pos, index2adr(L, -1));  /* agnPair_seti sets the barrier */
  L->top--;
  lua_unlock(L);
  return rc;
}


LUA_API void agn_createpairnumbers (lua_State *L, lua_Number x, lua_Number y) {  /* Reintroduced 2.9.8, modified 2.14.2 & 2.34.1 */
  StkId l, r;
  Pair *p;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_lock(L);
  setnvalue(L->top, x);
  api_incr_top(L);
  setnvalue(L->top, y);
  api_incr_top(L);
  l = index2adr(L, -2);
  r = index2adr(L, -1);
  luaC_checkGC(L);  /* 2.34.4, call GC before pair creation */
  p = agnPair_create(L, l, r);
  L->top -= 2;
  setpairvalue(L, L->top, p);
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);
}


/* From strings x and y, creates a pair with x stored to the left-hand side and y to the right-hand side. The function pushes the
   new pair onto the stack. 4.6.6, based on agn_createpairnumbers */
LUA_API void agn_createpairstrings (lua_State *L, const char *x, const char *y) {
  StkId l, r;
  Pair *p;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_lock(L);
  setsvalue(L, L->top, luaS_new(L, x));
  api_incr_top(L);
  setsvalue(L, L->top, luaS_new(L, y));
  api_incr_top(L);
  l = index2adr(L, -2);
  r = index2adr(L, -1);
  luaC_checkGC(L);  /* 2.34.4, call GC before pair creation */
  p = agnPair_create(L, l, r);
  L->top -= 2;
  setpairvalue(L, L->top, p);
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);
}


/* From strings x and y, creates a pair with x stored to the left-hand side and y to the right-hand side. The function pushes the
   new pair onto the stack. 4.6.7, based on agn_createpairnumbers */
LUA_API void agn_createpairstringnumber (lua_State *L, const char *x, lua_Number y) {
  StkId l, r;
  Pair *p;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_lock(L);
  setsvalue(L, L->top, luaS_new(L, x));
  api_incr_top(L);
  setnvalue(L->top, y);
  api_incr_top(L);
  l = index2adr(L, -2);
  r = index2adr(L, -1);
  luaC_checkGC(L);  /* 2.34.4, call GC before pair creation */
  p = agnPair_create(L, l, r);
  L->top -= 2;
  setpairvalue(L, L->top, p);
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);
}


#ifndef PROPCMPLX
LUA_API void agn_createcomplex (lua_State *L, agn_Complex c) {  /* 0.12.0, changed 0.22.1 */
  setcvalue(L->top, c);
  api_incr_top(L);
}
#else
LUA_API void agn_createcomplex (lua_State *L, lua_Number a, lua_Number b) {  /* 0.12.0 */
  lua_Number z[2];
  z[0] = a;
  z[1] = b;
  setcvalue(L->top, z);
  api_incr_top(L);
}
#endif


/* get left (n=1) or right (n=2) value in a pair, FIXME: make this consistent */
LUA_API int agn_pairgeti (lua_State *L, int idx, int n) {  /* 0.11.1 */
  StkId t;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttispair(t));
  setobj2s(L, L->top, agnPair_geti(pairvalue(t), n));
  api_incr_top(L);
  lua_unlock(L);
  return ttype(s2v(L->top - 1));
}


LUA_API void agn_pairgetiall (lua_State *L, int idx) {  /* 7.7.8 */
  StkId t;
  int i;
  lua_lock(L);
  if (!lua_checkstack(L, 2))
    luaL_error(L, "stack overflow (%s)", "not enough stack space");
  t = index2adr(L, idx);
  api_check(L, ttispair(t));
  for (i=1; i <= 2; i++) {
    setobj2s(L, L->top, agnPair_geti(pairvalue(t), i));
    api_incr_top(L);
  }
  lua_unlock(L);
}


LUALIB_API int agn_pairgetinumbers (lua_State *L, int idx, lua_Number *x, lua_Number *y) {  /* 4.11.2, based on agnL_pairgetinumbers */
  StkId o;
  Pair *p;
  const TValue *v;
  lua_lock(L);
  o = index2adr(L, idx);
  if (!ttispair(o)) return 0;
  p = pairvalue(o);
  v = agnPair_geti(p, 1);
  if (!ttisnumber(v)) return 0;
  *x = nvalue(v);
  v = agnPair_geti(p, 2);
  if (!ttisnumber(v)) return 0;
  *y = nvalue(v);
  lua_unlock(L);
  return 1;
}


LUALIB_API int agn_pairgetiintegers (lua_State *L, int idx, int *x, int *y) {  /* 4.11.2, based on agnL_pairgetinumbers */
  StkId o;
  Pair *p;
  const TValue *v;
  lua_lock(L);
  o = index2adr(L, idx);
  if (!ttispair(o)) return 0;
  p = pairvalue(o);
  v = agnPair_geti(p, 1);
  if (!ttisnumber(v) || !tools_isint(nvalue(v))) return 0;
  *x = nvalue(v);
  v = agnPair_geti(p, 2);
  if (!ttisnumber(v) || !tools_isint(nvalue(v))) return 0;
  *y = nvalue(v);
  lua_unlock(L);
  return 1;
}


LUA_API int lua_rawgeti (lua_State *L, int idx, int n) {  /* parameter n changed back from size_t to int in 3.20.0 */
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  setobj2s(L, L->top, luaH_getint(hvalue(o), n));
  api_incr_top(L);
  lua_unlock(L);
  return ttype(s2v(L->top - 1));  /* taken from Lua 5.4.0 RC 4; 7.3.9 */
}


/* Pushes all the table members with integral keys from index p to index q ("starting from 1", that is no C indices) onto
   the stack. The function traverses both the array and the hash part of the table. p > 0 & q > 0 & p <= q, p, q can be
   any integers, including 0 and negative ones; 3.20.0; YOU MUST PROVIDE ENOUGH STACK SPACE ! */
LUA_API void lua_rawgetirange (lua_State *L, int idx, int p, int q) {
  StkId o;
  Table *h;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  h = hvalue(o);
  setobj2s(L, L->top, luaH_getnum(h, p));  /* push arg[i] (avoiding overflow problems) */
  api_incr_top(L);
  while (p++ < q) {  /* push arg[i + 1...e] */
    setobj2s(L, L->top, luaH_getnum(h, p));
    api_incr_top(L);
  }
  lua_unlock(L);
}


/* for agnB_op. Similar to lua_rawgeti, but if key i is negative - referring to the |i|-th element from the right
   side of a table array -, then if posrelat is 1, it is converted to the respective positive integer position, see
   `utils.posrelat` for details. n is the size of the table array, see luaL_getn. With i=0, returns 0. 3.20.0 */
LUA_API void lua_rawgetiposrelat (lua_State *L, int idx, size_t i, int n, int posrelat) {
  StkId o;
  const TValue *v;
  lua_Number x;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  v = luaH_getnum(hvalue(o), i);
  if (posrelat && ttisnumber(v)) {
    x = nvalue(v);
    if (tools_isnegint(x)) {
      setnvalue(L->top, tools_posrelat(x, n));
      goto label;
    }
  }
  setobj2s(L, L->top, v);
label:
  api_incr_top(L);
  lua_unlock(L);
}


/* Pushes onto the stack the value t[k], where t is the table at the given index and k is the pointer p represented as a
   light userdata. The access is raw; that is, it does not use the __index metavalue.

   Returns the type of the pushed value. Taken from Lua 5.4.0 RC 5, 2.21.2 */

static INLINE int finishrawget (lua_State *L, const TValue *val) {
  if (isempty(val))  /* avoid copying empty items to the stack, isempty = ttisnil */
    setnilvalue(s2v(L->top));
  else
    setobj2s(L, L->top, val);
  api_incr_top(L);
  lua_unlock(L);
  return ttype(s2v(L->top - 1));
}

static INLINE Table *gettable (lua_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t));
  return hvalue(t);
}


LUA_API int lua_rawgetp (lua_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  lua_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, luaH_get(t, &k));
}


LUA_API void lua_createtable (lua_State *L, int narray, int nrec) {
  lua_lock(L);
  luaC_checkGC(L);
  sethvalue(L, L->top, luaH_new(L, narray, nrec));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void agn_createset (lua_State *L, int nrec) {  /* 0.10.0 */
  lua_lock(L);
  luaC_checkGC(L);
  setusvalue(L, L->top, agnUS_new(L, nrec));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void agn_createseq (lua_State *L, int nrec) {  /* 0.11.0 */
  lua_lock(L);
  luaC_checkGC(L);
  setseqvalue(L, L->top, agnSeq_new(L, nrec));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void agn_createreg (lua_State *L, int nrec) {  /* 2.3.0 RC 3 */
  lua_lock(L);
  luaC_checkGC(L);
  setregvalue(L, L->top, agnReg_new(L, nrec));
  api_incr_top(L);
  lua_unlock(L);
}


#define aux_checkfreezed(L,idx,o) { \
  if (o) luaL_error(L, "Error in " LUA_QS ": %s is read-only.", "getmetatable", luaL_typename(L, idx)); \
}

LUA_API int lua_getmetatable (lua_State *L, int idx) {
  const TValue *obj;
  Table *mt = NULL;
  int res;
  lua_lock(L);
  obj = index2adr(L, idx);
  switch (ttype(obj)) {
    case LUA_TTABLE: {
      Table *t = hvalue(obj);
      aux_checkfreezed(L, 1, t->readonly);
      mt = t->metatable;
      break;
    }
    case LUA_TUSERDATA: {
      Udata *u;
      u = rawuvalue(obj);
      aux_checkfreezed(L, 1, u->uv.readonly);
      mt = u->uv.metatable;
      break;
    }
    case LUA_TSET: {  /* 0.11.0 */
      UltraSet *t;
      t = usvalue(obj);
      aux_checkfreezed(L, 1, t->readonly);
      mt = t->metatable;
      break;
    }
    case LUA_TSEQ: {  /* 0.11.0 */
      Seq *t;
      t = seqvalue(obj);
      aux_checkfreezed(L, 1, t->readonly);
      mt = t->metatable;
      break;
    }
    case LUA_TPAIR: {  /* 0.11.0 */
      Pair *t;
      t = pairvalue(obj);
      aux_checkfreezed(L, 1, t->readonly);
      mt = t->metatable;
      break;
    }
    case LUA_TREG: {  /* 0.11.0 */
      Reg *t;
      t = regvalue(obj);
      aux_checkfreezed(L, 1, t->readonly);
      mt = t->metatable;
      break;
    }
    case LUA_TFUNCTION: {  /* 2.36.2 */
      Closure *fcl = clvalue(obj);
      mt = (fcl->c.isC) ? fcl->c.metatable : fcl->l.metatable;
      break;
    }
    default:
      mt = G(L)->mt[ttype(obj)];
      break;
  }
  if (mt == NULL) {
    res = 0;
  } else {
    sethvalue(L, L->top, mt);
    api_incr_top(L);
    res = 1;
  }
  lua_unlock(L);
  return res;
}


LUA_API void lua_getfenv (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_checkvalidindex(L, o);
  switch (ttype(o)) {
    case LUA_TFUNCTION:
      sethvalue(L, L->top, clvalue(o)->c.env);
      break;
    case LUA_TUSERDATA:
      sethvalue(L, L->top, uvalue(o)->env);
      break;
    case LUA_TTHREAD:
      setobj2s(L, L->top, gt(thvalue(o)));
      break;
    default:
      setnilvalue(L->top);
      break;
  }
  api_incr_top(L);
  lua_unlock(L);
}


/* Pushes the value at integral index i in the environment table of the userdata or
   function at index idx onto the stack. 5.4.4 */
LUA_API void lua_getfenvi (lua_State *L, int idx, int n) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_checkvalidindex(L, o);
  switch (ttype(o)) {
    case LUA_TFUNCTION:
      setobj2s(L, L->top, luaH_getnum(clvalue(o)->c.env, n));
      break;
    case LUA_TUSERDATA:
      setobj2s(L, L->top, luaH_getnum(uvalue(o)->env, n));
      break;
    default:
      setnilvalue(L->top);
      break;
  }
  api_incr_top(L);
  lua_unlock(L);
}


/* Pushes onto the stack the n-th user value associated with the full userdata at the given index and returns
   the type of the pushed value.

   If the userdata does not have that value or the value at idx is not a userdata, pushes null and returns LUA_TNONE.
   n should always be 1 as the function would always return LUA_TNONE otherwise.

   Taken from Lua 5.4.0 RC 5, 2.21.2 */

LUA_API int lua_getiuservalue (lua_State *L, int idx, int n) {
  TValue *o;
  int t;
  lua_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o));
  if (!ttisuserdata(o) || (n <= 0 || n > uvalue(o)->nuvalue)) {
    setnilvalue(s2v(L->top));
    t = LUA_TNONE;
  }
  else {
    setobj2s(L, L->top, o);
    t = ttype(s2v(L->top));
  }
  api_incr_top(L);
  lua_unlock(L);
  return t;
}

/*
** set functions (stack -> Lua)
**
** t[k] = value at the top of the stack (where 'k' is a string)
*/

/* taken from Lua 5.4.0 RC 4, 2.21.2. Compared to the 5.1 implementation, a little bit faster with string keys,
   not faster with number keys. */
LUA_API void lua_settable (lua_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  lua_lock(L);
  api_checknelems(L, 2);
  t = index2value(L, idx);
  if (luaV_fastget(L, t, s2v(L->top - 2), slot, luaH_get)) {
    luaV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else
    luaV_finishset(L, t, s2v(L->top - 2), s2v(L->top - 1), slot);
  L->top -= 2;  /* pop index and value */
  lua_unlock(L);
}


/* taken from Lua 5.4.0 RC 4, 2.21.2 */
LUA_API void lua_setfield (lua_State *L, int idx, const char *k) {
  const TValue *slot, *t;
  TString *str;
  lua_lock(L);  /* unlock done in 'auxsetstr' */
  t = index2value(L, idx);
  str = luaS_new(L, k);
  api_checknelems(L, 1);
  if (luaV_fastget(L, t, str, slot, luaH_getstr)) {
    luaV_finishfastset(L, t, slot, s2v(L->top - 1));
    L->top--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    luaV_finishset(L, t, s2v(L->top - 1), s2v(L->top - 2), slot);
    L->top -= 2;  /* pop value and key */
  }
  lua_unlock(L);  /* lock done by caller */
}


LUA_API void agn_rawsetfield (lua_State *L, int idx, const char *k) {  /* 2.12.1, do not invoke metamethods */
  StkId t;
  Table *h;
  TString *str;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2adr(L, idx);
  api_checkvalidindex(L, t);
  h = hvalue(t);
  str = luaS_new(L, k);
  setobj2t(L, luaH_setstr(L, h, str), L->top - 1);
  luaC_barriert(L, h, L->top - 1);
  L->top--;  /* pop value */
  lua_unlock(L);
}


LUA_API void agn_deletefield (lua_State *L, int idx, const char *k) {  /* 2.31.8, do not invoke metamethods */
  StkId t;
  Table *h;
  TString *str;
  lua_lock(L);
  api_checknelems(L, 0);
  t = index2adr(L, idx);
  api_checkvalidindex(L, t);
  h = hvalue(t);
  str = luaS_new(L, k);
  setnilvalue(L->top);
  api_incr_top(L);
  setobj2t(L, luaH_setstr(L, h, str), L->top - 1);
  luaC_barriert(L, h, L->top - 1);
  L->top--;  /* pop value */
  lua_unlock(L);
}


LUA_API void agn_rawgetfield (lua_State *L, int idx, const char *k) {  /* 2.12.1, do not invoke metamethods */
  StkId t;
  TString *str;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2adr(L, idx);
  api_checkvalidindex(L, t);
  str = luaS_new(L, k);
  setobj2s(L, L->top, luaH_getstr(hvalue(t), str));
  api_incr_top(L);  /* put table value at top of stack */
  lua_unlock(L);
}


LUA_API void lua_rawset (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = index2adr(L, idx);
  api_check(L, ttistable(t));
  setobj2t(L, luaH_set(L, hvalue(t), L->top - 2), L->top - 1);
  luaC_barriert(L, hvalue(t), L->top - 1);
  L->top -= 2;
  lua_unlock(L);
}


LUA_API void lua_rawseti (lua_State *L, int idx, int n) {
  StkId o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  luaH_setint(L, hvalue(o), n, L->top - 1);  /* 4.6.3 tweak */
  luaC_barriert(L, hvalue(o), L->top - 1);
  L->top--;
  lua_unlock(L);
}


LUA_API void agn_setinumber (lua_State *L, int idx, int n, lua_Number v) {  /* 3.10.2 */
  StkId o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  setnvalue(++L->top, v);
  luaH_setint(L, hvalue(o), n, L->top);  /* 4.6.3 tweak */
  luaC_barriert(L, hvalue(o), L->top);
  setnilvalue(L->top--);
  lua_unlock(L);
}


/* Does the equivalent of t[p] = v, where t is the table at the given index, p is encoded as a light userdata, and v
   is the value on the top of the stack.

   This function pops the value from the stack. The assignment is raw, that is, it does not use the __writeindex metavalue.

   Taken from Lua 5.4.0 RC 5, 2.21.2 */
LUA_API void lua_rawsetp (lua_State *L, int idx, const void *p) {
  Table *t;
  TValue *slot, key;
  int n = 1;
  setpvalue(&key, cast_voidp(p));
  lua_lock(L);
  api_checknelems(L, n);
  t = gettable(L, idx);
  slot = luaH_set(L, t, &key);
  setobj2t(L, slot, s2v(L->top - 1));
  invalidateTMcache(t);
  luaC_barriert(L, t, s2v(L->top - 1));
  L->top -= n;
  lua_unlock(L);
}


/* 0.22.1: returns a true, deep copy of the structure at stack index idx. The copy is put on top of the stack, but the
   original structure is not removed. Extended 2.34.4.
   what = 1: array only
   what = 2: hash only
   what = 3: array and hash part;
   what = 7: do not copy metatable and user-defined type */
LUA_API void agn_copy (lua_State *L, int idx, int what) {
  lua_lock(L);
  luaC_checkGC(L);  /* 2.34.4: collect garbage _before_ calling "VM", see lua_concat */
  agenaV_copy(L, index2value(L, idx), L->top, NULL, what);
  api_incr_top(L);
  lua_unlock(L);
}


/* UNIQUE function, 0.5.4; patched 0.8.0; tuned January 07, 2009 - 0.12.3; merged 2.34.4; 4.3.3 integration into lapi.c */
LUA_API void agn_unique (lua_State *L, int idx, int approx, int inplace) {  /* 2.34.4/4.12.0/7.6.4 change */
  TValue *t1val, *t2val;
  int i, j, c, flag;
  c = 0;
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* 2.34.4: collect garbage _before_ calling "VM", see lua_concat */
  api_check(L, L->top < L->ci->top);
  switch (ttype(base)) {
    case LUA_TTABLE: {
      Table *t = NULL, *h = NULL;
      int reorder = 0;
      t = hvalue(base);
      if (!inplace) h = luaH_new(L, t->sizearray, 0);
      for (i=0; i < t->sizearray; i++) {
        t1val = &t->array[i];
        if (ttisnotnil(t1val)) {
          flag = 1;
          /* traversing h is much slower ! */
          for (j=i + 1; j < t->sizearray; j++) {
            t2val = &t->array[j];
            if (ttisnotnil(t2val)) {
              if ( (!approx && equalobj(L, t1val, t2val)) ||  /* 4.12.0 change */
                    (approx && approxequalobjonebyone(L, t1val, t2val)) ) {
                flag = 0;  /* same value comes again later, so ignore previous ones */
                break;  /* quit inner loop if element has been found */
              }
            }
          }  /* of for j */
          if (flag && !inplace) {  /* if an element was _not_ found put it into new table */
            luaH_setint(L, h, ++c, t1val);  /* 4.6.3 tweak */
            luaC_barriert(L, h, t1val);  /* 7.6.1 fix */
          } else if (!flag && inplace) {  /* new 7.6.4 */
            setnilvalue(L->top);
            luaH_setint(L, t, i + 1, L->top);
            luaC_barriert(L, t, L->top);
            reorder = 1;
          }
        }  /* of if */
      }  /* of for i */
      if (inplace && reorder) {  /* new 7.6.4 */
        agn_reorder(L, idx, 1);
        break;
      }
      for (i -= t->sizearray; i < sizenode(t); i++) {  /* added 0.8.0 */
        /* changed 2.30.0: hash part by definition is always unique, so just copy it */
        Node *n = gnode(t, i);
        t1val = gval(n);
        if (ttisnotnil(t1val)) {
          agenaV_setops_settodict(L, h, key2tval(n), t1val);
          c++;
        }
      }
      if (c > 0) luaH_resizearray(L, h, c);
      sethvalue(L, base, inplace ? t : h);
      break;
    }
    case LUA_TSEQ: {
      Seq *t = NULL, *newseq = NULL;
      int reorder = 0;
      t = seqvalue(base);
      if (!inplace) newseq = agnSeq_new(L, t->size);
      for (i=0; i < t->size; i++) {
        flag = 1;
        t1val = seqitem(t, i);  /* value */
        for (j=i + 1; j < t->size; j++) {
          t2val = seqitem(t, j);
          if ( (!approx && equalobj(L, t1val, t2val)) ||  /* 4.12.0 change */
               (approx && approxequalobjonebyone(L, t1val, t2val)) ) {
            flag = 0;  /* same value comes again later, so ignore previous ones */
            break;  /* quit inner loop if element has been found */
          }
        }
        /* if an element was _not_ found put it into new sequence */
        if (flag && !inplace) {  /* if an element was _not_ found put it into new table */
          agnSeq_seti(L, newseq, ++c, t1val);
          luaC_barrierseq(L, newseq, t1val);  /* Agena 1.2 fix */
        } else if (!flag && inplace) {  /* new 7.6.4 */
          /* we will, exceptionally, temporarily null sequence positions */
          setnilvalue(L->top);
          agnSeq_rawseti(L, t, i + 1, L->top);
          luaC_barrierseq(L, t, L->top);
          reorder = 1;
        }
      }
      if (!reorder && c > 0) agnSeq_resize(L, newseq, c);  /* 0.28.1 */
      if (reorder) agnSeq_reorder(L, t);
      setseqvalue(L, base, inplace ? t : newseq);
      break;
    }
    case LUA_TREG: {
      Reg *t = NULL, *newreg = NULL;
      int reorder = 0;
      t = regvalue(base);
      if (!inplace) newreg = agnReg_new(L, t->top);
      for (i=0; i < t->top; i++) {
        flag = 1;
        t1val = regitem(t, i);  /* value */
        for (j=i + 1; j < t->top; j++) {
          t2val = regitem(t, j);
          if ( (!approx && equalobj(L, t1val, t2val)) ||  /* 4.12.0 change */
               (approx && approxequalobjonebyone(L, t1val, t2val)) ) {
            flag = 0;  /* same value comes again later, so ignore previous ones */
            break;  /* quit inner loop if element has been found */
          }
        }
        /* if an element was _not_ found put it into new register */
        if (flag && !inplace) {  /* if an element was _not_ found put it into new table */
          agnReg_seti(L, newreg, ++c, t1val);
          luaC_barrierreg(L, newreg, t1val);  /* Agena 1.2 fix */
        } else if (!flag && inplace) {  /* new 7.6.4 */
          /* we will, exceptionally, temporarily null sequence positions */
          setnilvalue(L->top);
          agnReg_seti(L, t, i + 1, L->top);
          luaC_barrierreg(L, t, L->top);
          reorder = 1;
        }

      }
      if (!reorder && c > 0) agnReg_resize(L, newreg, c, 1);
      if (reorder) agnReg_reorder(L, t);
      setregvalue(L, base, inplace ? t : newreg);
      break;
    }
    case LUA_TNIL: {  /* 4.12.1 */
      setnilvalue(base);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.",
        "unique", luaT_typenames[(int)ttype(base)]);
  }
  /* luaC_checkGC(L); */
  L->top = base + 1;
  lua_unlock(L);
}


LUA_API void agn_join (lua_State *L, int idx, int nargs) {  /* 2.34.5 */
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  agenaV_join(L, base, nargs);
  L->top = base + 1;
  lua_unlock(L);
}


/* REPLACE function, 0.10.0, 29.03.2008; extended 0.12.2, October 19/25, 2008; tweaked January 10, 2009;
   extended 0.26.0, 05.08.2009; patched 1.0.3, 11.11.2010; 2.34.5; moved to lapi.c 4.3.3 */

LUA_API void agn_replace (lua_State *L, int idx, int nargs) {
  int init;
  size_t l1, l2, l3;
  const char *src, *p, *repl, *s2;
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  if (!ttisstring(base))
    luaL_error(L, "Error in " LUA_QS ": string expected for argument #1, got %s.", "strings.replace", luaL_typename(L, idx));
  if (nargs == 3) {
    /* three strings passed */
    src = svalue(base); l1 = tsvalue(base)->len;
    if (ttisstring(base + 1) && ttisstring(base + 2)) {
      p = svalue(base + 1); l2 = tsvalue(base + 1)->len;
      if (l2 == 0) {
        setfailvalue(base);
        return;
      }
      init = 0;  /* cursor */
      s2 = tools_lmemfind(src + init, l1 - init, p, l2);  /* 2.16.12 tweak */
      if (s2) {
        Charbuf buf;
        charbuf_init(L, &buf, 0, l1, 1);
        repl = svalue(base + 2); l3 = tsvalue(base + 2)->len;
        while (1) {
          if (init) /* do not conduct very first search again */
            s2 = tools_lmemfind(src + init, l1 - init, p, l2);  /* 2.16.12 tweak */
          if (s2) {
            charbuf_append(L, &buf, src + init, s2 - (src + init));
            charbuf_append(L, &buf, repl, l3);
            init += (s2 - (src + init)) + l2;
          } else {
            charbuf_append(L, &buf, src + init, l1 - init);
            break;
          }
        }
        charbuf_finish(&buf);  /* better sure than sorry, 2.39.12 */
        setsvalue2s(L, base, luaS_newlstr(L, buf.data, buf.size));
        charbuf_free(&buf);
      }
      lua_unlock(L);
      /* do not change L->top here since this will confuse the stack */
    } else if (ttisnumber(base + 1) && ttisstring(base + 2)) {  /* inspired by the REXX function `changestr` */
      size_t pos;
      Charbuf buf;
      pos = tools_posrelat(nvalue(base + 1), l1);  /* 2nd argument is the position */
      if (pos < 1 || pos > l1)
        luaL_error(L, "Error in " LUA_QL("strings.replace") ": index out of range.");
      charbuf_init(L, &buf, 0, l1, 1);
      repl = svalue(base + 2); l3 = tsvalue(base + 2)->len;
      charbuf_append(L, &buf, src, pos - 1);  /* push left part of original string */
      charbuf_append(L, &buf, repl, l3);  /* push new string */
      charbuf_append(L, &buf, src + pos, tools_strlen(src + pos));  /* push rest of original string */ /* 2.17.8 tweak */
      charbuf_finish(&buf);  /* better sure than sorry, 2.39.12 */
      setsvalue2s(L, base, luaS_newlstr(L, buf.data, buf.size));
      charbuf_free(&buf);
    }
    else
      luaL_error(L, "Error in " LUA_QS ": wrong kind of argument.", "strings.replace");
  } else if (nargs == 2 && ttistable(base + 1)) {
    /* string and table passed */
    Pair *nt;
    TValue *val, *lval, *rval;
    int j;
    Table *t = hvalue(base + 1);
    for (j=0; j < t->sizearray; j++) {  /* iterate the substitution list */
      /* FIXME: handle dictionaries, as well. */
      if (ttisnotnil(&t->array[j])) {
        src = svalue(base); l1 = tsvalue(base)->len;
        val = &t->array[j];  /* push pair on stack */
        if (!ttispair(val))
          luaL_error(L, "Error in " LUA_QS ": pair expected, got %s.", "strings.replace", luaT_typenames[(int)ttype(val)]);
        else {
          nt = pairvalue(val);
          /* get pattern */
          lval = pairitem(nt, 0);
          if (!ttisstring(lval))
            luaL_error(L, "Error in " LUA_QS ": pair of strings expected.", "strings.replace");
          p = svalue(lval); l2 = tsvalue(lval)->len;
          if (l2 == 0) {
            setfailvalue(base);
            return;
          }
          init = 0;  /* cursor for every pass */
          s2 = tools_lmemfind(src + init, l1 - init, p, l2);  /* 2.16.12 tweak */
          if (s2) {  /* only replace if the pattern has been found */
            Charbuf buf;
            /* get replacement */
            rval = pairitem(nt, 1);
            if (!ttisstring(rval))
              luaL_error(L, "Error in " LUA_QS ": pair of strings expected.", "strings.replace");
            repl = svalue(rval); l3 = tsvalue(rval)->len;
            /* now substitute */
            charbuf_init(L, &buf, 0, l1, 1);
            while (1) {
              if (init)  /* do not conduct very first search again */
                s2 = tools_lmemfind(src + init, l1 - init, p, l2);  /* 2.16.12 tweak */
              if (s2) {
                charbuf_append(L, &buf, src + init, s2 - (src + init));
                charbuf_append(L, &buf, repl, l3);
                init += (s2 - (src + init)) + l2;
              } else {
                charbuf_append(L, &buf, src + init, l1 - init);
                break;
              }
            } /* of while */
            /* reset stack top */
            charbuf_finish(&buf);  /* better sure than sorry, 2.39.12 */
            setsvalue2s(L, base, luaS_newlstr(L, buf.data, buf.size));
            charbuf_free(&buf);
          }
        } /* of !ttispair */
      } /* of ttisnotnil */
    } /* of for j */
  } else if (nargs == 2 && ttisseq(base + 1)) {
    /* string and sequence passed */
    Seq *t;
    Pair *nt;
    TValue *val, *lval, *rval;
    int j;
    t = seqvalue(base + 1);
    api_check(L, L->top < L->ci->top);
    for (j=0; j < t->size; j++) {  /* iterate the substitution list */
      src = svalue(base); l1 = tsvalue(base)->len;
      val = seqitem(t, j);  /* push pair */
      if (!ttispair(val))
        luaL_error(L, "Error in " LUA_QS ": pair expected, got %s.", "strings.replace", luaT_typenames[(int)ttype(val)]);
      else {
        nt = pairvalue(val);
        /* get pattern */
        lval = pairitem(nt, 0);
        if (!ttisstring(lval))
          luaL_error(L, "Error in " LUA_QS ": pair of strings expected.", "strings.replace");
        p = svalue(lval); l2 = tsvalue(lval)->len;
        if (l2 == 0) {
          setfailvalue(base);
          return;
        }
        init = 0;  /* cursor for each pass */
        s2 = tools_lmemfind(src + init, l1 - init, p, l2);  /* 2.16.12 tweak */
        if (s2) {  /* only replace if the pattern has been found */
          Charbuf buf;
          /* get replacement */
          rval = pairitem(nt, 1);
          if (!ttisstring(rval))
            luaL_error(L, "Error in " LUA_QS ": pair of strings expected.", "strings.replace");
          repl = svalue(rval); l3 = tsvalue(rval)->len;
          /* now substitute */
          charbuf_init(L, &buf, 0, l1, 1);
          while (1) {
            if (init)  /* do not conduct very first search again */
              s2 = tools_lmemfind(src + init, l1 - init, p, l2);  /* 2.16.12 tweak */
            if (s2) {
              charbuf_append(L, &buf, src + init, s2 - (src + init));
              charbuf_append(L, &buf, repl, l3);
              init += (s2 - (src + init)) + l2;
            } else {
              charbuf_append(L, &buf, src + init, l1 - init);
              break;
            }
          } /* of while */
          /* reset stack top */
          charbuf_finish(&buf);  /* better sure than sorry, 2.39.12 */
          setsvalue2s(L, base, luaS_newlstr(L, buf.data, buf.size));
          charbuf_free(&buf);
        }
      } /* of !ttispair */
    } /* of for j */
  } else if (nargs == 2 && ttisreg(base + 1)) {  /* 2.4.0 */
    /* string and sequence passed */
    Reg *t;
    Pair *nt;
    TValue *val, *lval, *rval;
    int j;
    t = regvalue(base + 1);
    api_check(L, L->top < L->ci->top);
    for (j=0; j < t->top; j++) {  /* iterate the substitution list */
      src = svalue(base); l1 = tsvalue(base)->len;
      val = regitem(t, j);  /* push pair */
      if (!ttispair(val))
        luaL_error(L, "Error in " LUA_QS ": pair expected, got %s.", "strings.replace", luaT_typenames[(int)ttype(val)]);
      else {
        nt = pairvalue(val);
        /* get pattern */
        lval = pairitem(nt, 0);
        if (!ttisstring(lval))
          luaL_error(L, "Error in " LUA_QS ": pair of strings expected.", "replace");
        p = svalue(lval); l2 = tsvalue(lval)->len;
        if (l2 == 0) {
          setfailvalue(base);
          return;
        }
        init = 0;  /* cursor for each pass */
        s2 = tools_lmemfind(src + init, l1 - init, p, l2);  /* 2.16.12 tweak */
        if (s2) {  /* only replace if the pattern has been found */
          Charbuf buf;
          /* get replacement */
          rval = pairitem(nt, 1);
          if (!ttisstring(rval))
            luaL_error(L, "Error in " LUA_QS ": pair of strings expected.", "strings.replace");
          repl = svalue(rval); l3 = tsvalue(rval)->len;
          /* now substitute */
          charbuf_init(L, &buf, 0, l1, 1);
          while (1) {
            if (init)  /* do not conduct very first search again */
              s2 = tools_lmemfind(src + init, l1 - init, p, l2);  /* 2.16.12 tweak */
            if (s2) {
              charbuf_append(L, &buf, src + init, s2 - (src + init));
              charbuf_append(L, &buf, repl, l3);
              init += (s2 - (src + init)) + l2;
            } else {
              charbuf_append(L, &buf, src + init, l1 - init);
              break;
            }
          }  /* of while */
          /* reset stack top */
          charbuf_finish(&buf);  /* better sure than sorry, 2.39.12 */
          setsvalue2s(L, base, luaS_newlstr(L, buf.data, buf.size));
          charbuf_free(&buf);
        }
      }  /* of !ttispair */
    }  /* of for j */
  }
  else if (nargs == 2)
    luaL_error(L, "Error in " LUA_QS ": string and table, sequence or register of pairs expected.", "strings.replace");
  else
    luaL_error(L, "Error in " LUA_QS ": two or three arguments expected, got %d.", "strings.replace", nargs);  /* 3.7.8 */
  L->top = base + 1;
  lua_unlock(L);
}


LUA_API void agn_instr (lua_State *L, int idx, int nargs) {  /* 2.34.5 */
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  agenaV_instr(L, base, nargs);
  L->top = base + 1;
  lua_unlock(L);
}


LUA_API void agn_upper (lua_State *L, int idx, int nargs) {  /* 4.3.3 */
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  if (ttisstring(base)) {
    size_t i, len;
    const char *s = svalue(base);
    char *str;
    len = tsvalue(base)->len;
    str = (char *)tools_malloc(len*CHARSIZE, 0);  /* 1.9.1, 2.27.0 change */
    if (str == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.upper");  /* Agena 1.0.4 */
    if (nargs == 1) {
      for (i=0; i < len; i++) {
        str[i] = tools_uppercase[uchar(s[i])];
      }
    } else {
      for (i=0; i < len; i++) {
        str[i] = toupper(uchar(s[i]));
      }
    }
    str[len] = '\0';
    setsvalue(L, base, luaS_newlstr(L, str, len));
    xfree(str);  /* 1.10.4 */
  } else {
    luaL_error(L, "Error in " LUA_QS ": string expected, got %s.", "strings.upper", luaL_typename(L, idx));
  }
  L->top = base + 1;
  lua_unlock(L);
}


LUA_API void agn_lower (lua_State *L, int idx, int nargs) {  /* 4.3.3 */
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  if (ttisstring(base)) {
    size_t i, len;
    const char *s = svalue(base);
    char *str;
    len = tsvalue(base)->len;
    str = (char *)tools_malloc(len*CHARSIZE, 0);  /* 1.9.1, 2.27.0 change */
    if (str == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "strings.lower");  /* Agena 1.0.4 */
    if (nargs == 1) {
      for (i=0; i < len; i++) {
        str[i] = tools_lowercase[uchar(s[i])];
      }
    } else {
      for (i=0; i < len; i++) {
        str[i] = tolower(uchar(s[i]));
      }
    }
    str[len] = '\0';
    setsvalue(L, base, luaS_newlstr(L, str, len));
    xfree(str);  /* 1.10.4 */
  } else {
    luaL_error(L, "Error in " LUA_QS ": string expected, got %s.", "strings.lower", luaL_typename(L, idx));
  }
  L->top = base + 1;
  lua_unlock(L);
}


LUA_API void agn_trim (lua_State *L, int idx, int nargs) {  /* 4.3.3 */
  size_t i, len;
  int c, start;
  char *r, chr;
  const char *s;
  StkId base;
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  base = index2adr(L, idx);
  if (!ttisstring(base)) {
    luaL_error(L, "Error in " LUA_QS ": expected a string for 1st argument, got %s.", "trim", luaL_typename(L, idx));
  }
  s = svalue(base);
  len = tsvalue(base)->len;
  if (len == 0) {  /* 2.16.12, query moved up */
    setsvalue(L, base, luaS_newlstr(L, "", 0));
    return;
  }
  r = (char *)tools_malloc(CHARSIZE*len, 0);  /* 2.27.0 change */
  if (r == NULL)  /* Agena 1.0.4 */
    luaG_runerror(L, "Error in " LUA_QS ": memory allocation failed.", "strings.trim");
  if (nargs == 1) {
    chr = ' ';
  } else {
    if (!ttisstring(base + 1)) {
      luaL_error(L, "Error in " LUA_QS ": expected a string for 2nd argument, got %s.", "trim", luaL_typename(L, idx + 1));
    }
    chr = (svalue(base + 1))[0];
  }
  c = 0;
  for (i=0; i < len; i++) {
    if (!(uchar(s[i]) == chr && uchar(s[i + 1]) == chr)) {
      r[c++] = uchar(s[i]);
    }
  }
  r[c] = '\0';
  start = 0;
  while (r[start] == chr) start++;
  while (c && r[--c] == chr) {};  /* 4.3.3 optimisation, 5.6.1 fix */
  setsvalue(L, base, luaS_newlstr(L, r + start, c - start + 1));
  xfree(r);  /* 1.10.4 */
  L->top = base + 1;
  lua_unlock(L);
}


LUA_API void agn_char (lua_State *L, int idx) {  /* 4.3.3 */
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  if (ttisnumber(base)) {
    char s[1];
    lua_Number nb = nvalue(base);
    if (nb < 0 || nb > 255) {
      luaL_error(L, "Error in " LUA_QS ": invalid value.", "char");
    } else {
      s[0] = uchar(nb);  /* better sure than sorry, 1.5.0 */
      setsvalue(L, base, luaS_newlstr(L, s, 1));
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": number expected, got %s.", "char", luaL_typename(L, idx));
  }
  L->top = base + 1;
  lua_unlock(L);
}


/* VALUES function, 1.3.0, 02.01.2011; nargs must be at least two; lapi.c version 2.34.5 */
LUA_API void agn_values (lua_State *L, int idx, int nargs) {
  int i, index;
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  api_check(L, L->top < L->ci->top);
  switch (ttype(base)) {
    case LUA_TSEQ: {
      Seq *s, *newseq;
      TValue *v;
      int c;
      c = 0;
      s = seqvalue(base);
      /* stack must be increased to avoid `invalid key to next` errors */
      newseq = agnSeq_new(L, nargs - 1);
      /* traverse sequence (first arg) */
      for (i=1; i < nargs; i++) {
        if (ttisnumber(base + i)) {
          index = tools_posrelat((int)nvalue(base + i), s->size) - 1;
          if (index < 0 || index >= s->size) {
            lua_unlock(L);
            luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "values", index + 1);  /* 3.7.8 fix */
          }
          c++;
          v = seqitem(s, index);
          agnSeq_seti(L, newseq, c, v);
          luaC_barrierseq(L, newseq, v);
        }
      }
      /* prepare sequence for return to Agena environment */
      setseqvalue(L, base, newseq);
      break;
    }
    case LUA_TREG: {  /* 2.3.0 RC 3 */
      Reg *s, *newreg;
      TValue *v;
      int c;
      c = 0;
      s = regvalue(base);
      /* stack must be increased to avoid `invalid key to next` errors */
      newreg = agnReg_new(L, nargs - 1);
      /* traverse sequence (first arg) */
      for (i=1; i < nargs; i++) {
        if (ttisnumber(base + i)) {
          index = tools_posrelat((int)nvalue(base + i), s->top) - 1;
          if (index < 0 || index >= s->top) {
            lua_unlock(L);
            luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "values", index + 1);  /* 2.4.0, 3.7.8 fix */
          }
          c++;
          v = regitem(s, index);
          agnReg_seti(L, newreg, c, v);
          luaC_barrierreg(L, newreg, v);
        }
      }
      /* prepare register for return to Agena environment */
      setregvalue(L, base, newreg);
      break;
    }
    case LUA_TTABLE: {
      Table *h, *newtable;
      h = hvalue(base);
      /* stack must be increased to avoid `invalid key to next` errors */
      newtable = luaH_new(L, 0, 0);  /* 2.8.0 fix to avoid problems with the strict equality check (`==` operator) */
      /* traverse first table */
      for (i=1; i < nargs; i++) {
        const TValue *k = luaH_get(h, base + i);  /* do a primitive get */
        TValue *oldval = luaH_set(L, newtable, base + i);  /* do a primitive set */
        setobj2t(L, oldval, k);
        luaC_barriert(L, newtable, k);
      }
      /* prepare table for return to Agena environment */
      sethvalue(L, base, newtable);
      break;
    }
    default: {
      lua_unlock(L);
      luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected as first argument, got %s.", "values",
        luaT_typenames[(int)ttype(base)]);
      }
  }
  for (i=1; i < nargs; i++) setnilvalue(base + i);  /* delete second to n-th argument */
  L->top = base + 1;
  lua_unlock(L);
}


/* nargs must be at least three; 2.34.5; lapi.c integration 4.3.3 */
LUA_API void agn_times (lua_State *L, int idx, int nargs) {
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  if (ttisfunction(base) && ttisnumber(base + 2)) {
    lua_Number b, r, oldr, eps;
    int i, j, c, epsbailout, offset;
    StkId top, res;
    ptrdiff_t result = savestack(L, base);  /* the following function calls may change the stack, 3.6.1 */
    res = base;
    lua_lock(L);
    b = nvalue(base + 2);
    /* use bailout epsilon if third arg is `infinity` and fourth arg is an epsilon value */
    epsbailout = nargs >= 4 && ttisnumber(base + 3) && isinf(b);
    offset = 4 + epsbailout;
    if (epsbailout) {
      eps = nvalue(base + 3);
      if (eps < 0)
        luaL_error(L, "Error in " LUA_QS ": fourth argument must be non-negative.", "times");
      r = oldr = -HUGE_VAL;
    } else {
      eps = 0; r = 0; oldr = 0;
    }
    if (b < 1) setnilvalue(base + 1);
    for (i=0; i < b; i++) {
      lua_lock(L);
      setobj2s(L, L->top, base);  /* push function */
      setobj2s(L, L->top + 1, base + 1);  /* value */
      c = 2;
      for (j=offset; j <= nargs; j++) {
        setobj2s(L, L->top + c++, base + j - 1);  /* value */
      }
      luaD_checkstack(L, c);  /* 3.5.5: check stack size every time, not just once */
      L->top += c;
      luaD_call(L, L->top - c, 1, 0);
      top = L->top - 1;
      res = restorestack(L, result);  /* 3.6.1 */
      /* new 2.29.5: bail out as soon as the function returns null, false or fail */
      if (ttisfalse(top)) {
        if (i == 0) { setobj2s(L, res + 1, top); }
        L->top--;  /* pop false, return previous result, new 2.29.5 */
        break;
      } else if (epsbailout) {
        /* 2.29.7, use epsilon value - the fourth argument - as bailout condition */
        if (ttisnumber(top)) {
          r = nvalue(top);
          if (isnan(r) || isinf(r)) {  /* prevent to iterate infinitely */
            setnvalue(res + 1, r);
            L->top--; break;
          } else if (fabs(r - oldr) <= eps) {  /* bail out */
            L->top--; break;
          } else oldr = r;
        } else {
          luaL_error(L,
            "Error in " LUA_QS ": function call did not evaluate to a number or boolean, got %s.",
            "times", luaT_typenames[(int)ttype(top)]);
        }
      }
      setobj2s(L, res + 1, --L->top);  /* set result to 2nd argument position */
      lua_lock(L);
    }
    setobj2s(L, res, res + 1);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid arguments.", "times");
  L->top = base + 1;
  lua_unlock(L);
}


/* Sums up all the numbers or complex numbers in the distribution at stack index idx and pushes a number or complex number onto the stack. 4.4.6 */
LUA_API void agn_sumup (lua_State *L, int idx, int type, const char *procname) {
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  switch (type) {  /* 4.4.7 change */
    case LUA_TTABLE:
      agenaV_sumup(L, base, L->top, TM_SUMUP);
      break;
    case LUA_TSEQ:
      agenaV_seqsumup(L, base, L->top, TM_SUMUP);
      break;
    case LUA_TREG:
      agenaV_regsumup(L, base, L->top, TM_SUMUP);
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": invalid argument.", procname);
  }
  api_incr_top(L);
  lua_unlock(L);
}


/* Sums up all the numbers or complex numbers divided by d in the distribution at idx and pushes a number or complex number onto the stack.
   The function is great to compute the arithmetic mean if you already know the number of samples. 4.4.6 */
LUA_API void agn_sumupdiv (lua_State *L, int idx, lua_Number d, int type, const char *procname) {
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  switch (type) {  /* 4.4.7 change */
    case LUA_TTABLE:
      agenaV_addup_sumup_div(L, base, L->top, d);
      break;
    case LUA_TSEQ:
      agenaV_addup_seqsumup_div(L, base, L->top, d);
      break;
    case LUA_TREG:
      agenaV_addup_regsumup_div(L, base, L->top, d);
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": invalid argument.", procname);
  }
  api_incr_top(L);
  lua_unlock(L);
}



/* Divides all the numbers or complex numbers x in the distribution at idx by d, then multiplies by x and sums up the quotients.
   The function pushes a number or complex number onto the stack. 4.5.0 */
LUA_API void agn_qsumupdiv (lua_State *L, int idx, lua_Number d, int type, const char *procname) {
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  switch (type) {  /* 4.4.7 change */
    case LUA_TTABLE:
      agenaV_qsumup_div(L, base, L->top, d);
      break;
    case LUA_TSEQ:
      agenaV_seqqsumup_div(L, base, L->top, d);
      break;
    case LUA_TREG:
      agenaV_regqsumup_div(L, base, L->top, d);
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": invalid argument.", procname);
  }
  api_incr_top(L);
  lua_unlock(L);
}


/* Computes both the arithmetic mean and the quadratic mean deviation of the numbers in table, sequence or register obj. Returns 1 on failure
   and 0 otherwise. The mean is stored in mean, the deviation in qmdev. Mathemetically, qmdev is the variance multiplied by the number of
   samples. 4.6.2 */
LUA_API int agn_qmdev (lua_State *L, int idx, lua_Number *mean, lua_Number *qmdev, const char *procname) {
  int rc;
  StkId base = index2adr(L, idx);
  lua_lock(L);
  luaC_checkGC(L);  /* collect garbage _before_ calling "VM", see lua_concat */
  rc = agenaV_numqmdev(L, base, L->top, mean, qmdev);
  lua_unlock(L);  /* 4.10.0 fix */
  return rc;
}


/* Empties a table at index idx and leaves the stack unchanged. The function also frees allocated, previously occupied memory
   in the array and hash parts and optionally conducts a garbage collection if gc is set to 1. 2.33.2 */
LUA_API void agn_cleanse (lua_State *L, int idx, int gc) {
  if (!lua_istable(L, idx)) return;
  lua_pushvalue(L, idx);
  agn_clear(L, -1);  /* 4.9.0 */
  agn_reorder(L, -1, 0);  /* shrink array and hash part, i.e. free unused memory */
  agn_poptop(L);  /* pop table from stack */
  if (gc) lua_gc(L, LUA_GCCOLLECT, 0);
}


/* Empties a set at index idx and leaves the stack unchanged. The function optionally conducts a garbage collection if gc is
   set to 1. 2.33.2 */
LUA_API void agn_cleanseset (lua_State *L, int idx, int gc) {
  if (!lua_isset(L, idx)) return;
  lua_pushvalue(L, idx);
  lua_pushnil(L);
  while (lua_usnext(L, -2)) {
    lua_sdelete(L, -3); /* delete value in the set and pop it */
    agn_poptop(L);  /* pop the key from stack, too, since it cannot be used for next iteration as it has been purged from the set */
    lua_pushnil(L); /* restart iteration */
  }
  agn_poptop(L);  /* pop set from stack */
  agn_setresize(L, idx, 1, 0);
  if (gc) lua_gc(L, LUA_GCCOLLECT, 0);
}


LUA_API void lua_srawset (lua_State *L, int idx) {
  StkId t;
  UltraSet *s;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2adr(L, idx);
  s = usvalue(t);
  api_check(L, ttisuset(t));
  agnUS_set(L, s, L->top - 1);
  luaC_barrierset(L, s, L->top - 1);
  L->top--;
  lua_unlock(L);
}


LUA_API void lua_sdelete (lua_State *L, int idx) {  /* 0.10.0 */
  StkId t;
  UltraSet *s;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2adr(L, idx);
  s = usvalue(t);
  api_check(L, ttisuset(t));
  agnUS_delete(L, s, L->top - 1);
  L->top--;  /* pop value */
  lua_unlock(L);
}


LUA_API void agn_pairrawset (lua_State *L, int idx) {
  StkId t;
  int r;
  Pair *p;
  lua_lock(L);
  api_checknelems(L, 2);
  t = index2adr(L, idx);
  p = pairvalue(t);  /* 2.2.0 */
  api_check(L, ttispair(t));
  r = agnPair_rawseti(L, p, L->top - 2, L->top - 1);
  if (r != 1)
    luaG_runerror(L, "Error in rawset (agn_pairrawset).");
  luaC_barrierpair(L, p, L->top - 1);
  L->top -= 2;
  lua_unlock(L);
}


LUA_API void lua_seqinsert (lua_State *L, int idx) {  /* 0.11.0 */
  StkId t;
  TValue *val;
  Seq *s;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  s = seqvalue(t);
  val = L->top - 1;
  if (agnSeq_seti(L, s, s->size + 1, val) == -1)
    luaG_runerror(L, "Error in rawset (lua_seqinsert): memory allocation failed.");
  luaC_barrierseq(L, s, val);
  L->top--;  /* pop value */
  lua_unlock(L);
}


LUA_API void lua_reginsert (lua_State *L, int idx) {  /* 2.34.2 */
  int i, flag;
  StkId t;
  TValue *val;
  Reg *s;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  s = regvalue(t);
  val = L->top - 1;
  flag = 1;
  for (i=1; i <= s->top; i++) {
    if (ttisnil(agnReg_geti(s, i))) {
      agnReg_seti(L, s, i, val);
      luaC_barrierreg(L, s, val);
      flag = 0;
      break;
    }
  }
  if (flag)  /* XXX: can be improved */
    luaG_runerror(L, "Error while inserting: no free slot in register found.");
  L->top--;  /* pop value */
  lua_unlock(L);
}


/* Inserts the object at stack index idxv into the table, set, sequence or register at stack index idxs. The function
   does not change the stack. */
LUA_API void agn_structinsert (lua_State *L, int idxs, int idxv) {  /* 2.34.2 */
  StkId o;
  TValue *val;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2adr(L, idxs);
  val = index2value(L, idxv);
  switch (ttype(o)) {
    case LUA_TTABLE: {
      Table *h = hvalue(o);
      luaH_setint(L, h, luaH_getn(h) + 1, val);  /* 4.6.3 tweak */
      luaC_barriert(L, h, val);
      break;
    }
    case LUA_TSET: {
      UltraSet *s = usvalue(o);
      agnUS_set(L, s, val);
      luaC_barrierset(L, s, val);
      break;
    }
    case LUA_TSEQ: {
      Seq *s = seqvalue(o);
      if (agnSeq_seti(L, s, s->size + 1, val) == -1)
        luaG_runerror(L, "Error while inserting: memory allocation failed.");
      luaC_barrierseq(L, s, val);
      break;
    }
    case LUA_TREG: {
      int i, flag;
      Reg *s = regvalue(o);
      flag = 1;
      for (i=1; i <= s->top; i++) {
        if (ttisnil(agnReg_geti(s, i))) {
          agnReg_seti(L, s, i, val);
          luaC_barrierreg(L, s, val);
          flag = 0;
          break;
        }
      }
      if (flag)  /* XXX: can be improved */
        luaG_runerror(L, "Error while inserting: no free slot in register found.");
      break;
    }
    default:
      luaG_runerror(L, "Error while inserting: structure expected, got %s.", luaL_typename(L, idxv));
  }
  lua_unlock(L);
}


/* Set readonly to 1 to set read-only status, and 0 to unset it. To get read-only status,
   set readonly to -1. 4.9.0 */
LUA_API int agn_freeze (lua_State *L, int idx, int readonly) {
  StkId o;
  int r = 0;
  lua_lock(L);
  o = index2adr(L, idx);
  switch (ttype(o)) {
    case LUA_TTABLE: {
      r = agnH_readonly(L, hvalue(o), readonly);
      break;
    }
    case LUA_TSET: {
      r = agnUS_readonly(L, usvalue(o), readonly);
      break;
    }
    case LUA_TSEQ: {
      r = agnSeq_readonly(L, seqvalue(o), readonly);
      break;
    }
    case LUA_TREG: {
      r = agnReg_readonly(L, regvalue(o), readonly);
      break;
    }
    case LUA_TPAIR: {
      r = agnPair_readonly(L, pairvalue(o), readonly);
      break;
    }
    case LUA_TUSERDATA: {
      Udata *u = rawuvalue(o);
      if (readonly >= 0) u->uv.readonly = readonly;
      r = u->uv.readonly;
      break;
    }
    case LUA_TFUNCTION: {
      /* do nothing */
      break;
    }
    default: {
      if (readonly != -1) {  /* 5.4.0 extension */
        luaG_runerror(L, "Error: structure or userdata expected, got %s.", luaL_typename(L, idx));
      }
    }
  }
  lua_unlock(L);
  return r;
}


/* With readonly = -1, just reads the current read-only status */
LUA_API int agn_udfreeze (lua_State *L, int idx, int readonly) {  /* 4.9.0 */
  StkId o;
  Udata *u;
  int r;
  lua_lock(L);
  o = index2adr(L, idx);
  u = rawuvalue(o);
  if (readonly >= 0) u->uv.readonly = readonly;
  r = u->uv.readonly;
  lua_unlock(L);
  return r;
}


LUA_API void lua_seqrawset (lua_State *L, int idx) {
  StkId t;
  int r;
  Seq *h;
  lua_lock(L);
  api_checknelems(L, 2);
  t = index2adr(L, idx);
  h = seqvalue(t);  /* 2.2.0 */
  api_check(L, ttisseq(t));
  r = agnSeq_seti(L, h, (int)nvalue(L->top - 2), L->top - 1);
  switch (r) {  /* 2.29.0 improved error handling */
    case 0:
      luaG_runerror(L, "Error in internal " LUA_QS " with sequence, given index is not next free index.", "rawset");
      break;
    case -1:
      luaG_runerror(L, "Error in internal " LUA_QS " with sequence, memory allocation failed.", "rawset");
      break;
    case -2:
      luaG_runerror(L, "Error in internal " LUA_QS " with sequence, null cannot be added.", "rawset");
      break;
  }
  luaC_barrierseq(L, h, L->top - 1);
  L->top -= 2;
  lua_unlock(L);
}


/* Set a new value which is at the top of the stack to a sequence at `index` i; returns:
    -2:    attempt to add `null` to sequence
    -1:    memory allocation error
     0:    index out of range
     1:    success
     The value is popped. */
LUA_API int lua_seqrawseti (lua_State *L, int idx, size_t n) {  /* 0.11.0 */
  StkId t;
  Seq *h;
  int rc;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2adr(L, idx);
  h = seqvalue(t);  /* 2.2.0 */
  api_check(L, ttisseq(t));
  rc = agnSeq_seti(L, h, n, L->top - 1);
  if (rc == 1) { /* 7.3.10 fix */
    luaC_barrierseq(L, h, L->top - 1);
  }
  L->top--;  /* pop inserted value */
  lua_unlock(L);
  return rc;
}


LUA_API void lua_seqseticachevalue (lua_State *L, int idx, size_t n, int idxcache) {  /* 2.37.2 */
  StkId t, q;
  Seq *h;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2adr(L, idx);
  h = seqvalue(t);  /* 2.2.0 */
  api_check(L, ttisseq(t));
  q = ((idx < 0) ? L->C->top : L->C->base) + idxcache;
  if (ttisstring(q)) {  /* 7.1.6 fix, we must create a duplicate when copying from L->C to L */
    const char *str = svalue(q);
    int len = tsvalue(q)->len;
    setsvalue(L, L->top, luaS_newlstr(L, str, len));
    agnSeq_seti(L, h, n, L->top);
  } else {
    agnSeq_seti(L, h, n, q);
  }
  luaC_barrierseq(L, h, q);
  lua_unlock(L);
}


LUA_API void agn_seqsetinumber (lua_State *L, int idx, int n, lua_Number x) {  /* 2.14.2 */
  StkId t;
  Seq *h;
  lua_lock(L);
  setnvalue(L->top, x);
  api_incr_top(L);
  t = index2adr(L, idx - (idx < 0));
  h = seqvalue(t);
  api_check(L, ttisseq(t));
  agnSeq_seti(L, h, n, L->top - 1);
  luaC_barrierseq(L, h, L->top - 1);
  L->top--;  /* pop inserted value */
  lua_unlock(L);
}


/* assumes that the value is `at the top` of the stack and the numeric key just below the stack */
LUA_API void agn_regset (lua_State *L, int idx) {  /* 2.3.0 RC 3 */
  StkId t;
  int r;
  Reg *h;
  lua_lock(L);
  api_checknelems(L, 2);
  t = index2adr(L, idx);
  h = regvalue(t);
  api_check(L, ttisreg(t));
  r = agnReg_seti(L, h, (int)nvalue(L->top - 2), L->top - 1);
  if (r != 1)
    luaG_runerror(L, "Error in internal " LUA_QS " with register, given index is not next free index.", "rawset");
  luaC_barrierreg(L, h, L->top - 1);
  L->top -= 2;
  lua_unlock(L);
}


LUA_API void agn_regpurge (lua_State *L, int idx, int n) {  /* 2.3.0 RC 3, n must be positive ! */
  StkId t;
  Reg *h;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2adr(L, idx);
  h = regvalue(t);  /* 2.2.0 */
  api_check(L, ttisreg(t));
  agnReg_purge(L, h, n);
  lua_unlock(L);
}


/* set a new value to a register at `index` i, starting from 1;
   returns:
       0:    index out of range
       1:    success */
LUA_API int agn_regrawseti (lua_State *L, int idx, size_t n) {  /* 2.3.0 RC 3 */
  StkId t;
  Reg *h;
  int rc;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2adr(L, idx);
  h = regvalue(t);  /* 2.2.0 */
  api_check(L, ttisreg(t));
  rc = agnReg_seti(L, h, n, L->top - 1);
  luaC_barrierreg(L, h, L->top - 1);
  L->top--;  /* pop inserted value */
  lua_unlock(L);
  return rc;
}


LUA_API void agn_regsetinumber (lua_State *L, int idx, int n, lua_Number x) {  /* 2.14.2 */
  StkId t;
  Reg *h;
  lua_lock(L);
  setnvalue(L->top, x);
  api_incr_top(L);
  t = index2adr(L, idx - (idx < 0));
  h = regvalue(t);  /* 2.2.0 */
  api_check(L, ttisreg(t));
  agnReg_seti(L, h, n, L->top - 1);
  luaC_barrierreg(L, h, L->top - 1);
  L->top--;  /* pop inserted value */
  lua_unlock(L);
}


LUA_API void agn_setutype (lua_State *L, int idxobj, int idxtype) {  /* 0.11.1; extended 0.14.0, patched 0.28.0,
  patched 1.2.1, extended 2.3.0 RC 3 */
  StkId t, ty;
  TString *str;
  lua_lock(L);
  t = index2adr(L, idxobj);
  ty = index2adr(L, idxtype);
  if (ttisstring(ty))
    str = luaS_new(L, svalue(ty));
  else if (ttisnil(ty))
    str = NULL;
  else {
    luaG_runerror(L, "Error: string or null expected.");
    return;
  }
  agenaV_setutype(L, t, str);
  lua_unlock(L);
}


LUA_API void agn_setutypestring (lua_State *L, int idxobj, const char *type) {  /* 2.14.2, based on agn_setutype */
  lua_lock(L);
  if (type == NULL)  /* 2.15.1 extension */
    agenaV_setutype(L, index2adr(L, idxobj), NULL);
  else
    agenaV_setutype(L, index2adr(L, idxobj), luaS_new(L, type));
  lua_unlock(L);
}


static FORCE_INLINE TString *getutype (lua_State *L, int idx) {
  StkId t;
  TString *r;
  lua_lock(L);
  t = index2adr(L, idx);
  if (ttistable(t))
    r = hvalue(t)->type;
  else if (ttisseq(t))
    r = seqvalue(t)->type;
  else if (ttisreg(t))  /* 2.5.3 */
    r = regvalue(t)->type;
  else if (ttisuset(t))
    r = usvalue(t)->type;
  else if (ttispair(t))
    r = pairvalue(t)->type;
  else if (ttisuserdata(t)) { /* 2.3.0 RC 3 */
    r = uvalue(t)->type;
  }
  else if (ttisfunction(t)) {  /* 2.15.1 hardening */
    Closure *fcl = clvalue(t);
    if (fcl->c.isC && fcl->c.type != NULL)
      r = fcl->c.type;
    else if (!fcl->c.isC && fcl->l.type != NULL)
      r = fcl->l.type;
    else
      r = NULL;
  } else  /* 2.15.1 fix */
    r = NULL;
  lua_unlock(L);
  return r;
}

LUA_API int agn_getutype (lua_State *L, int idx) {  /* 0.11.1; extended 0.14.0, modified 0.28.0 */
  TString *r = getutype(L, idx);
  if (r == NULL)
    return 0;
  else {
    setsvalue(L, L->top, r);
    api_incr_top(L);
  }
  lua_unlock(L);
  return 1;
}


LUA_API int agn_isutype (lua_State *L, int idx, const char *str) {  /* based on agn_getutype, 1.9.1, 2.2.6 */
  TString *r = getutype(L, idx);
  return (r == NULL) ? 0 : tools_streq(getstr(r), str);
}


LUA_API int agn_isutypeset (lua_State *L, int idx) {  /* 0.12.2; extended 0.14.0, modified 0.28.0, optimised 3.15.4 */
  return getutype(L, idx) != NULL;
}


LUA_API int agn_istableutype (lua_State *L, int idx, const char *str) {  /* 0.12.2, modified 0.28.0 */
  StkId t;
  TString *r;
  lua_lock(L);
  t = index2adr(L, idx);
  r = (ttistable(t)) ? hvalue(t)->type : NULL;
  lua_unlock(L);
  return (r == NULL) ? 0 : tools_streq(getstr(r), str);
}


LUA_API int agn_issetutype (lua_State *L, int idx, const char *str) {  /* 0.14.0, modified 0.28.0 */
  StkId t;
  TString *r;
  lua_lock(L);
  t = index2adr(L, idx);
  r = (ttisuset(t)) ? usvalue(t)->type : NULL;
  lua_unlock(L);
  return (r == NULL) ? 0 : tools_streq(getstr(r), str);
}


LUA_API int agn_issequtype (lua_State *L, int idx, const char *str) {  /* 0.12.1, modified 0.28.0 */
  StkId t;
  TString *r;
  lua_lock(L);
  t = index2adr(L, idx);
  r = (ttisseq(t)) ? seqvalue(t)->type : NULL;
  lua_unlock(L);
  return (r == NULL) ? 0 : tools_streq(getstr(r), str);
}


/* Sets a metatable (last argument) to the object at index idx. The metatable is dropped thereafter, the object is not. */
LUA_API int lua_setmetatable (lua_State *L, int idx) {
  TValue *obj;
  Table *mt;
  lua_lock(L);
  api_checknelems(L, 1);
  obj = index2adr(L, idx);
  api_checkvalidindex(L, obj);
  if (ttisnil(L->top - 1))
    mt = NULL;
  else {
    api_check(L, ttistable(L->top - 1));
    mt = hvalue(L->top - 1);
  }
  agenaV_setmetatable(L, obj, mt, 0);
  L->top--;
  lua_unlock(L);
  return 1;
}


/* If k is non-NULL, sets metatable `k' to the structure at index position idx. The function does not change the stack.
   If k is NULL, deletes the metatable from the object at idx and if settype == 1, the user-defined type. */
LUA_API void lua_setmetatabletoobject (lua_State *L, int idx, const char *k, int settype) {
  StkId t;
  TValue *obj;
  lua_lock(L);
  t = index2adr(L, LUA_REGISTRYINDEX);
  api_checkvalidindex(L, t);
  if (k == NULL) {
    obj = index2adr(L, idx);  /* address of object at `idx` */
    api_checkvalidindex(L, obj);
    /* we must remove read-only status if set, otherwise the object cannot be collected properly. 5.1.6 fix */
    agn_udfreeze(L, idx, 0);
    agenaV_setmetatable(L, obj, NULL, 1);  /* delete metatable to object at `idx` */
    if (settype) agenaV_setutype(L, obj, NULL);
  } else {
    Table *mt;
    TValue key;
    TString *kstr;
    kstr = luaS_new(L, k);
    setsvalue(L, &key, kstr);
    luaV_gettable(L, t, &key, L->top);
    api_incr_top(L);
    obj = index2adr(L, idx - 1);
    api_checkvalidindex(L, obj);
    if (settype) agenaV_setutype(L, obj, kstr);  /* set user-defined type to object at `idx`, 2.21.9 optimisation */
    if (ttisnil(L->top - 1))  /* no metatable found, invalid string index ? */
      mt = NULL;
    else {
      api_check(L, ttistable(L->top - 1));
      mt = hvalue(L->top - 1);
    }
    agenaV_setmetatable(L, obj, mt, 1);  /* set metatable to object at `idx` */
    L->top--;  /* drop metatable */
  }
  lua_unlock(L);
}


/* Sets a storage table that is at the top of the stack to a procedure at index indx. If `null` is at the top of the stack, the storage is
   deleted. If the storage table already exists, then the contents of the table at the stack top is inserted into it. Finally, the table
   or `null` value at the stack top is popped and the procedure is left untouched at stack index idx. 3.5.3 */
LUA_API int agn_setstorage (lua_State *L, int idx) {
  TValue *obj;
  Table *t;
  lua_lock(L);
  api_checknelems(L, 1);
  obj = index2adr(L, idx);
  api_checkvalidindex(L, obj);
  if (ttisnil(L->top - 1))
    t = NULL;
  else {
    api_check(L, ttistable(L->top - 1));
    t = hvalue(L->top - 1);
  }
  if (!agenaV_setstorage(L, obj, t)) {
    /* storage already exists, add contents of non-null t to it */
    agn_getstorage(L, idx);  /* get storage table for procedure at idx and put it onto the stack.
      Storage is at -1, the table passed at -2 */
    lua_pushnil(L);  /* Storage is at -2, the table passed at -3 */
    while (lua_next(L, -3)) {
      lua_pushvalue(L, -2);  /* duplicate key */
      lua_pushvalue(L, -2);  /* duplicate value */
      lua_settable(L, -5);
      agn_poptop(L);  /* drop value, proceed with next key */
    }
    L->top--;  /* drop storage */
  }
  L->top--;  /* drop table or null, leave procedure untouched */
  lua_unlock(L);
  return 1;
}


LUA_API int lua_setfenv (lua_State *L, int idx) {
  StkId o;
  int res = 1;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2adr(L, idx);
  api_checkvalidindex(L, o);
  api_check(L, ttistable(L->top - 1));
  switch (ttype(o)) {
    case LUA_TFUNCTION:
      clvalue(o)->c.env = hvalue(L->top - 1);
      break;
    case LUA_TUSERDATA:
      uvalue(o)->env = hvalue(L->top - 1);
      break;
    case LUA_TTHREAD:
      sethvalue(L, gt(thvalue(o)), hvalue(L->top - 1));
      break;
    default:
      res = 0;
      break;
  }
  if (res) luaC_objbarrier(L, gcvalue(o), hvalue(L->top - 1));  /* Lua 5.1.3 patch */
  L->top--;
  lua_unlock(L);
  return res;
}


LUA_API lua_Number agn_getepsilon (lua_State *L) {  /* 7.6.3 */
  return L->Eps;
}


LUA_API lua_Number agn_getdblepsilon (lua_State *L) {  /* 7.6.3 */
  return L->DoubleEps;
}


LUA_API lua_Number agn_gethepsilon (lua_State *L) {  /* 7.6.3 */
  return L->hEps;
}


LUA_API lua_Number agn_getstarttime (lua_State *L) {  /* 7.6.3 */
  return L->starttime;
}


/*
** `load' and `call' functions (run Lua code)
*/

#define adjustresults(L,nres) \
    { if (nres == LUA_MULTRET && L->top >= L->ci->top) L->ci->top = L->top; }

#define checkresults(L,na,nr) \
     api_check(L, (nr) == LUA_MULTRET || (L->ci->top - L->top >= (nr) - (na)))


LUA_API ptrdiff_t lua_call (lua_State *L, int nargs, int nresults) {
  StkId func;
  lua_lock(L);
  api_checknelems(L, nargs + 1);
  checkresults(L, nargs, nresults);
  func = L->top - (nargs + 1);
  luaD_call(L, func, nresults, 1);
  adjustresults(L, nresults);
  lua_unlock(L);
  return L->top - func;  /* number of results actually pushed */
}


/* Calls the function at stack index 1 and returns a numeric result, it leaves nothing on the stack. Always returns the first result of
   the function call or zero in case of an error. Patched 2.31.10 to leave nothing on the stack. */
LUA_API lua_Number agn_ncall (lua_State *L, int nargs, int *error, int quit) {
  StkId func;
  const TValue *o;
  lua_Number r;
  lua_lock(L);
  *error = 0;
  api_checknelems(L, nargs + 1);
  checkresults(L, nargs, 1);
  func = L->top - (nargs + 1);
  luaD_call(L, func, 1, 0);  /* changed to exactly one result to avoid problems with incorrect results and stack corruption, 2.31.10 */
  o = L->top - 1;
  if (!ttisnumber(o)) {
    *error = 1;
    lua_unlock(L);
    if (quit) luaG_runerror(L, "Error: return of function call is not a number, but a %s.", lua_typename(L, ttype(o)));
    return 0.0;
  }
  r = nvalue(o);
  L->top--;   /* delete result from stack (it has already been stored to r) */
  lua_unlock(L);
  return r;
}


LUA_API lua_Number agn_npcall (lua_State *L, int nargs, int *error, int quit) {  /* new 7.3.4 */
  const TValue *o;
  lua_Number r;
  lua_lock(L);
  *error = 0;
  api_checknelems(L, nargs + 1);
  checkresults(L, nargs, 1);
  if (lua_pcall(L, nargs, 1, 0) != 0) {
    *error = 2;
    lua_unlock(L);
    if (quit) luaG_runerror(L, "Error: function call failed.");
    return 0.0;
  }
  o = L->top - 1;
  if (!ttisnumber(o)) {
    *error = 1;
    lua_unlock(L);
    if (quit) luaG_runerror(L, "Error: return of function call is not a number, but a %s.", lua_typename(L, ttype(o)));
    return 0.0;
  }
  r = nvalue(o);
  L->top--;   /* delete result from stack (it has already been stored to r) */
  lua_unlock(L);
  return r;
}


/* Calls a function and returns a complex result; used by fractals package. Always returns the first result of
   the function call. Patched 2.31.10 to really leave nothing on the stack. */
#ifndef PROPCMPLX
LUA_API agn_Complex agn_ccall (lua_State *L, int nargs) {
  StkId func;
  const TValue *o;
  agn_Complex r;
  lua_lock(L);
  api_checknelems(L, nargs + 1);
  checkresults(L, nargs, 1);
  func = L->top - (nargs + 1);
  luaD_call(L, func, 1, 0);  /* changed to exactly one result to avoid problems with incorrect results and stack corruption, 2.31.10 */
  o = L->top - 1;
  if (ttiscomplex(o)) {
    r = cvalue(o);
  } else if (ttisnumber(o)) {  /* 3.10.8 extension */
    r = nvalue(o) + I*0;
  } else {
    r = 0 + I*0;  /* to prevent compiler warnings */
    luaG_runerror(L, "Error: return of function call is not a (complex) number, but a %s.", lua_typename(L, ttype(o)));  /* new in 0.27.1, 1.9.3 */
  }
  L->top--;  /* delete result from stack (it has already been stored to r) */
  lua_unlock(L);
  return r;
}
#else
LUA_API void agn_ccall (lua_State *L, int nargs, lua_Number *real, lua_Number *imag) {  /* 0.27.1 */
  StkId func;
  const TValue *o;
  lua_lock(L);
  api_checknelems(L, nargs + 1);
  checkresults(L, nargs, 1);
  func = L->top - (nargs + 1);
  luaD_call(L, func, 1, 0);  /* changed to exactly one result to avoid problems with incorrect results and stack corruption, 2.31.10 */
  o = L->top - 1;
  if (ttiscomplex(o)) {
    *real = complexreal(o);
    *imag = compleximag(o);
  } else if (ttisnumber(o)) {  /* 3.10.8 extension */
    *real = nvalue(o);
    *imag = 0;
  } else {
    *real = 0; *imag = 0;  /* to prevent compiler warnings */
    luaG_runerror(L, "Error: return of function call is not a (complex) number, but a %s.", lua_typename(L, ttype(o)));  /* new in 0.27.1, 1.9.3 */
  }
  L->top--;  /* delete result from stack (it has already been stored to r) */
  lua_unlock(L);
}
#endif


/* The function prepares an OOP method call and should be embedded into the function that is called by the `__index' mt handler; 4.4.1.
   With `packagename.procname` representing a function, the C API function works as follows: the tag string (`procname`) is at stack position -1,
   the packagename userdata on -2, and the method arguments, zero, one or more, are looming somewhere else. There might be a procedure at -3,
   but it is not the method we want to call. The function actually does not call the OOP `function` as this is done by the VM automatically when
   the `__index' mt returns.

   tablename is the name of the package, and lentablename the length of tablename in characters without a finalising \0.

   This is the low-level implementation of the code below which is around 17 percent slower than agn_initmethodcall.

   int nargs = lua_gettop(L);  // nargs to be returned by this C function
   // fetch the procedure we want to call
   if (!agn_isstring(L, -1))  // do not return the original confusing error message
     luaL_error(L, "Error in " LUA_QS ": wrong number or type of arguments.", procname);
   agnL_gettablefield(L, tablename, agn_tostring(L, -1), "__index metatag", 1);
   luaL_checkstack(L, 1, "not enough stack space");
   lua_pushvalue(L, -2);  // push bfield userdata
   // after return, the VM will call the procedure with all its arguments and return the result, if any.
   return nargs;

   Speed test:

   import heaps
   a := avl.new();
   watch();
   for i to 100k do
      a@@include(i)
      a@@remove()
   od;
   watch():
   */

LUA_API int agn_initmethodcall (lua_State *L, const char *tablename, int lentablename) {
  StkId base, p;
  const TValue *slot, *t;
  TString *str;
  int nargs = cast_int(L->top - L->base);
  base = L->top - 1;
  lua_lock(L);
  /* fetch the procedure we want to call */
  if (!ttisstring(base)) goto methodcall;  /* do a traditional rawget */
  if (lua_checkstack(L, 3) == 0)
    luaL_error(L, "Error in " LUA_QS ": not enough stack space.", "__index metatag");
  str = luaS_newlstr(L, tablename, lentablename);
  if (luaV_fastget(L, gt(L), str, slot, luaH_getstr)) {
    setobj2s(L, L->top++, slot);
  } else {
    setsvalue2s(L, L->top++, str);
    luaV_finishget(L, gt(L), s2v(L->top - 1), L->top - 1, slot);
  }
  t = L->top - 1;  /* table fetched */
  if (!ttistable(t)) {
    L->top--;  /* in case of an error, remove object on the top */
    luaL_error(L, "Error in " LUA_QS ": table " LUA_QS " does not exist.", "__index metatag", tablename);
  }
  str = luaS_newlstr(L, svalue(base), tsvalue(base)->len);
  if (luaV_fastget(L, t, str, slot, luaH_getstr)) {
    setobj2s(L, L->top++, slot);
  } else {
    setsvalue2s(L, L->top++, str);
    luaV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  if (!ttisfunction(L->top - 1)) {
    L->top--;  /* remove object on the top and do a traditional rawget */
methodcall:
    agnL_rawget(L, "__index metatag");
    nargs = 1;
  } else {
    p = L->top - 2;
    while (++p < L->top) setobjs2s(L, p - 1, p);
    setobj2s(L, L->top - 1, L->top - 3);
    /* after return, the VM will call the procedure with all its arguments and return the result, if any. */
  }
  lua_unlock(L);
  return nargs;
}


/*
** Execute a protected call.
*/
struct CallS {  /* data to `f_call' */
  StkId func;
  int nresults;
};


static void f_call (lua_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  luaD_call(L, c->func, c->nresults, 1);
}



LUA_API int lua_pcall (lua_State *L, int nargs, int nresults, int errfunc) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  lua_lock(L);
  api_checknelems(L, nargs + 1);
  checkresults(L, nargs, nresults);
  if (errfunc == 0)
    func = 0;
  else {
    StkId o = index2adr(L, errfunc);
    api_checkvalidindex(L, o);
    func = savestack(L, o);
  }
  c.func = L->top - (nargs + 1);  /* function to be called */
  c.nresults = nresults;
  status = luaD_pcall(L, f_call, &c, savestack(L, c.func), func);
  adjustresults(L, nresults);
  lua_unlock(L);
  return status;  /* returns 0 on success */
}


/*
** Execute a protected C call.
*/
struct CCallS {  /* data to `f_Ccall' */
  lua_CFunction func;
  void *ud;
};


static void f_Ccall (lua_State *L, void *ud) {
  struct CCallS *c = cast(struct CCallS *, ud);
  Closure *cl;
  cl = luaF_newCclosure(L, 0, getcurrenv(L));
  cl->c.f = c->func;
  setclvalue(L, L->top, cl);  /* push function */
  api_incr_top(L);
  setpvalue(L->top, c->ud);  /* push only argument */
  api_incr_top(L);
  luaD_call(L, L->top - 2, 0, 1);
}


LUA_API int lua_cpcall (lua_State *L, lua_CFunction func, void *ud) {
  struct CCallS c;
  int status;
  lua_lock(L);
  c.func = func;
  c.ud = ud;
  status = luaD_pcall(L, f_Ccall, &c, savestack(L, L->top), 0);
  lua_unlock(L);
  return status;
}


LUA_API int lua_dump (lua_State *L, lua_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = s2v(L->top - 1);
  if (isLfunction(o))
    status = luaU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  lua_unlock(L);
  return status;
}


LUA_API int lua_load (lua_State *L, lua_Reader reader, void *data,
                      const char *chunkname) {
  ZIO z;
  int status;
  lua_lock(L);
  if (!chunkname) chunkname = "?";
  luaZ_init(L, &z, reader, data);
  status = luaD_protectedparser(L, &z, chunkname);
  lua_unlock(L);
  return status;
}


LUA_API int lua_status (lua_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/

LUA_API int lua_gc (lua_State *L, int what, int data) {
  int res = 0;
  global_State *g;
  lua_lock(L);
  g = G(L);
  switch (what) {
    case LUA_GCSTOP: {
      g->GCthreshold = MAX_LUMEM;
      break;
    }
    case LUA_GCRESTART: {
      g->GCthreshold = g->totalbytes;
      break;
    }
    case LUA_GCCOLLECT: {
      luaC_fullgc(L);
      break;
    }
    case LUA_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(g->totalbytes >> 10);
      break;
    }
    case LUA_GCCOUNTB: {
      res = cast_int(g->totalbytes & 0x3ff);  /* in kBytes (0x3ff = 1023) */
      break;
    }
    case LUA_GCSTEP: {
      lu_mem a = (cast(lu_mem, data) << 10);
      if (a <= g->totalbytes)
        g->GCthreshold = g->totalbytes - a;
      else
        g->GCthreshold = 0;
      while (g->GCthreshold <= g->totalbytes) {  /* Lua 5.1.3 patch 10 */
        luaC_step(L);
        if (g->gcstate == GCSpause) {  /* end of cycle? */
          res = 1;  /* signal it */
          break;
        }
      }
      break;
    }
    case LUA_GCSETPAUSE: {
      /* controls how long the GC waits before starting a new cycle once the previous one has finished */
      res = g->gcpause;
      g->gcpause = data;
      break;
    }
    case LUA_GCSETSTEPMUL: {
      /* controls the speed of the GC relative to the speed of memory allocation */
      res = g->gcstepmul;
      g->gcstepmul = data;
      break;
    }
    case LUA_GCSTATUS: {  /* 2.2.5 */
      res = !(g->GCthreshold == MAX_LUMEM);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  lua_unlock(L);
  return res;
}


LUA_API LUAI_UMEM agn_usedbytes (lua_State *L) {
  global_State *g;
  LUAI_UMEM res;
  lua_lock(L);
  g = G(L);
  res = (LUAI_UMEM)g->totalbytes;
  lua_unlock(L);
  return res;
}


/*
** miscellaneous functions
*/


LUA_API int lua_error (lua_State *L) {
  lua_lock(L);
  api_checknelems(L, 1);
  luaG_errormsg(L);
  lua_unlock(L);
  return 0;  /* to avoid warnings */
}


/* The traversal is undefined if you assign a value to a non-existent field. You may change values, hewever, or even set them to `null`. */
LUA_API int lua_next (lua_State *L, int idx) {
  StkId t;
  int more;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttistable(t));
  more = luaH_next(L, hvalue(t), L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top--;  /* remove key */
  lua_unlock(L);
  return more;
}


LUA_API int lua_usnext (lua_State *L, int idx) {  /* 0.10.0 */
  StkId t;
  int more;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisuset(t));
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.1 fix */
  more = agnUS_next(L, usvalue(t), L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top--;  /* remove key */
  lua_unlock(L);
  return more;
}


LUA_API int lua_seqnext (lua_State *L, int idx) {  /* 0.11.2 */
  StkId t;
  int more;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisseq(t));
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.1 fix */
  more = agnSeq_next(L, seqvalue(t), L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top--;  /* remove key */
  lua_unlock(L);
  return more;
}


LUA_API int lua_regnext (lua_State *L, int idx) {  /* 2.3.0 RC 3 */
  StkId t;
  int more;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttisreg(t));
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.1 fix */
  more = agnReg_next(L, regvalue(t), L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top--;  /* remove key */
  lua_unlock(L);
  return more;
}


/* 0.12.0, used by for i in string loop; optimized 0.24.1, improved 2.16.1 */
LUA_API int lua_strnext (lua_State *L, int idx) {
  StkId t, below;
  int i, more;
  lua_lock(L);
  below = L->top - 1;
  t = index2adr(L, idx);
  api_check(L, ttisstring(t));
  if (ttisnil(below))  /* first iteration */
    i = 0;
  else if (ttisnumber(below))
    i = nvalue(below);  /* get current string position */
  else {
    i = -1;
    luaG_runerror(L, "Error: invalid key to " LUA_QL("nextone") ".");
  }
  if (i < tsvalue(t)->len) {  /* 2.16.1 patch, do not use strlen since it quits with the first embedded \0 */
    luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.1 fix */
    more = 1;
    setnvalue(below, cast_num(i + 1));  /* set new key */
    setsvalue(L, L->top, luaS_newchar(L, svalue(t) + i));  /* set character */
    api_incr_top(L);
  }
  else {  /* no more elements */
    more = 0;
    L->top--;  /* remove key */
  }
  lua_unlock(L);
  return more;
}


LUA_API void lua_concat (lua_State *L, int n) {
  lua_lock(L);
  api_checknelems(L, n);
  if (n >= 2) {
    luaC_checkGC(L);
    luaV_concat(L, n, cast_int(L->top - L->base) - 1);
    L->top -= (n - 1);
  }
  else if (n == 0) {  /* push empty string */
    setsvalue2s(L, L->top, luaS_newlstr(L, "", 0));
    api_incr_top(L);
  }
  /* else n == 1; nothing to do */
  lua_unlock(L);
}


/* Returns the memory-allocation function of a given state. If ud is not NULL, Lua stores in *ud
   the opaque pointer given when the memory-allocator function was set. */
LUA_API lua_Alloc lua_getallocf (lua_State *L, void **ud) {
  lua_Alloc f;
  lua_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  lua_unlock(L);
  return f;
}


/* Changes the allocator function of a given state to f with user data ud. */
LUA_API void lua_setallocf (lua_State *L, lua_Alloc f, void *ud) {
  lua_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  lua_unlock(L);
}


LUA_API void *lua_newuserdata (lua_State *L, size_t size) {
  Udata *u;
  lua_lock(L);
  luaC_checkGC(L);
  u = luaS_newudata(L, size, getcurrenv(L));
  setuvalue(L, L->top, u);
  api_incr_top(L);
  lua_unlock(L);
  return u + 1;
}


/* This function creates and pushes on the stack a new full userdata, with nuvalue associated Agena values, called user values,
   plus an associated block of raw memory with size bytes. (The user values can be set and read with the functions lua_setiuservalue
   and lua_getiuservalue.) In Agena, nuvalue should always be 1, otherwise the function issues an error to avoid segmentation faults.

   The function returns the address of the block of memory. 2.21.2 */
LUA_API void *lua_newuserdatauv (lua_State *L, size_t size, int nuvalue) {
  if (nuvalue != 1)
    luaL_error(L, "third argument to lua_newuserdatauv must always be 1.");
  return lua_newuserdata(L, size);
}


static const char *aux_upvalue (StkId fi, int n, TValue **val) {
  Closure *f;
  if (!ttisfunction(fi)) return NULL;
  f = clvalue(fi);
  if (f->c.isC) {
    if (!(1 <= n && n <= f->c.nupvalues)) return NULL;
    *val = &f->c.upvalue[n - 1];
    return "";
  }
  else {
    Proto *p = f->l.p;
    if (!(1 <= n && n <= p->sizeupvalues)) return NULL;
    *val = f->l.upvals[n - 1]->v;
    return getstr(p->upvalues[n - 1]);
  }
}


LUA_API const char *lua_getupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TValue *val;
  lua_lock(L);
  name = aux_upvalue(index2adr(L, funcindex), n, &val);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  lua_unlock(L);
  return name;
}


LUA_API const char *lua_setupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TValue *val;
  StkId fi;
  lua_lock(L);
  fi = index2adr(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val);
  if (name) {
    L->top--;
    setobj(L, val, L->top);
    luaC_barrier(L, clvalue(fi), L->top);
  }
  lua_unlock(L);
  return name;
}


LUA_API int lua_nupvalues (lua_State *L, int funcindex) {  /* 2.12.3 */
  StkId idx;
  Closure *f;
  lua_lock(L);
  idx = index2adr(L, funcindex);
  if (!ttisfunction(idx)) return -1;
  f = clvalue(idx);
  return ( (f->c.isC) ? f->c.nupvalues : f->l.p->sizeupvalues );
}


/* changed/additions */


/* agn_poptop: pops the value at the top of the stack; written July 12, 2007 */
LUA_API void agn_poptop (lua_State *L) {
  lua_lock(L);
  api_check(L, 1 <= (L->top - L->base));
  L->top--;  /* decrease top */
  lua_unlock(L);
}


/* agn_poptop: pops the value at the top of the stack; written April 2, 2008 */
LUA_API void agn_poptoptwo (lua_State *L) {
  lua_lock(L);
  api_check(L, 1 <= (L->top - L->base));
  L->top -= 2;  /* decrease top */
  lua_unlock(L);
}


/* gets the i-th number from the table at the given index idx and returns it as a lua_Number;
   written July 12, 2007 */
LUA_API lua_Number agn_getinumber (lua_State *L, int idx, int n) {
  StkId o;
  const TValue *p;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  p = luaH_getnum(hvalue(o), n);
  lua_unlock(L);
  return (ttype(p) == LUA_TNUMBER) ? nvalue(p) : 0.0;
}


LUA_API lua_Number agn_rawgetinumber (lua_State *L, int idx, int n, int *rc) {  /* 2.27.5 */
  StkId o;
  const TValue *p;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  p = luaH_getnum(hvalue(o), n);
  *rc = ttype(p) == LUA_TNUMBER;
  lua_unlock(L);
  return (*rc) ? nvalue(p) : 0.0;
}


LUA_API lua_Number agn_rawgetiinteger (lua_State *L, int idx, int n, int *rc) {  /* 3.20.0 */
  StkId o;
  const TValue *p;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  p = luaH_getnum(hvalue(o), n);
  if ( (*rc = (ttype(p) == LUA_TNUMBER)) ) {
    lua_Number x = nvalue(p);
    if (tools_isint(x)) return x;
    *rc = 0;
  }
  lua_unlock(L);
  return 0.0;
}


LUA_API void agn_rawgeticomplex (lua_State *L, int idx, int n, lua_Number *z, int *rc) {  /* 4.5.0 */
  StkId o;
  TValue *p;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  /* older GCC compilers weirdly complain with complexreal & compleximag: `assignment of read-only variable`, so `unconst` */
  p = (TValue *)luaH_getnum(hvalue(o), n);
  lua_unlock(L);
  if ( (*rc = ttype(p)) == LUA_TNUMBER ) {
    z[0] = nvalue(p); z[1] = 0;
  } else if ( (*rc = ttype(p)) == LUA_TCOMPLEX ) {
    z[0] = (lua_Number)complexreal(p); z[1] = (lua_Number)compleximag(p);
  } else {
    z[0] = 0; z[1] = 0; *rc = LUA_TNIL;
  }
}


/* gets the i-th string from the table at the given index idx and returns it;
   written July 21, 2007, extended 4.11.1 */
LUA_API const char *agn_rawgetilstring (lua_State *L, int idx, int n, size_t *len, int *rc) {
  StkId o;
  const TValue *p;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  p = luaH_getnum(hvalue(o), n);
  *rc = ttype(p) == LUA_TSTRING;
  lua_unlock(L);
  if (*rc) {
    if (len != NULL) *len = tsvalue(p)->len;
    return svalue(p);
  } else {
    if (len != NULL) *len = 0;
    return NULL;
  }
}


/* for fast determination of indices in tables.indices; sets the value just below the top of the stack to the table at idx and pops
   the topmost value. Used by tables.indices. */
LUA_API void lua_rawsetikey (lua_State *L, int idx, int n) {
  StkId o;
  lua_lock(L);
  api_checknelems(L, 2);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  luaH_setint(L, hvalue(o), n, L->top - 2);  /* 4.6.3 tweak */
  luaC_barriert(L, hvalue(o), L->top - 2);
  L->top--;
  lua_unlock(L);
}


/* Inserts the value at the top of the stack to the table at index idx, more precisely, it is added to the end of the array part of the table.
   The value is popped from the stack. */
LUA_API void agn_rawinsert (lua_State *L, int idx) {  /* 2.11.4 */
  StkId o;
  Table *h;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  h = hvalue(o);
  luaH_setint(L, h, luaH_getn(h) + 1, L->top - 1);  /* works like INSERT statement; tweaked 4.6.3 */
  luaC_barriert(L, h, L->top - 1);
  L->top--;
  lua_unlock(L);
}

/* Inserts the value at stack index vidx to the table residing at index idx, more precisely, it is added to the end of the array part of the table.
   If vidx = -1, then the value is popped from the stack, otherwise the stack is left untouched. */
LUA_API void agn_rawinsertfrom (lua_State *L, int tidx, int vidx) {  /* 2.11.4 */
  StkId o, p;
  Table *h;
  lua_lock(L);
  o = index2adr(L, tidx);
  api_check(L, ttistable(o));
  h = hvalue(o);
  p = index2adr(L, vidx);
  luaH_setint(L, h, luaH_getn(h) + 1, p);  /* works like INSERT statement; tweaked 4.6.3 */
  luaC_barriert(L, h, p);
  if (vidx == -1) L->top--;  /* if value is at the top of the stack, remove it, otherwise leave it in the stack */
  lua_unlock(L);
}


/* same as lua_rawset, but only deleting the value, and not the key; August 04, 2007 */
LUA_API void lua_rawset2 (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = index2adr(L, idx);
  api_check(L, ttistable(t));
  setobj2t(L, luaH_set(L, hvalue(t), L->top - 2), L->top - 1);
  luaC_barriert(L, hvalue(t), L->top - 1);
  L->top--;  /* pop value from the stack */
  lua_unlock(L);
}


/* fast Boolean check, 2.28.6; June 17, 2022, returns 0 for `false` and `fail`, 1 for `true`, and issues an error otherwise.
   This also means that the function will reject `null`. */
LUA_API int agn_checkboolean (lua_State *L, int idx) {
  StkId o;
  int t;
  lua_lock(L);
  o = index2adr(L, idx);
  t = ttype(o);
  if (t != LUA_TBOOLEAN && t != LUA_TFAIL)
    tag_error(L, idx, LUA_TBOOLEAN);
  lua_unlock(L);
  return bvalue(o) == 1;  /* 2.37.3 fix */
}


/* fast number check, 0.6.0; October 25, 2007 */
LUA_API lua_Number agn_checknumber (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TNUMBER)
    tag_error(L, idx, LUA_TNUMBER);
  lua_unlock(L);
  return nvalue(o);
}


LUA_API lua_Integer agn_checkinteger (lua_State *L, int idx) {  /* 1.12.9 */
  StkId o;
  lua_Number x;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TNUMBER)
    tag_error(L, idx, LUA_TNUMBER);
  x = nvalue(o);
  if (tools_isfrac(x))  /* 2.14.13 change */
    luaL_error(L, "Wrong argument #%d: expected an integer, got fraction.", idx);
  lua_unlock(L);
  return x;
}


LUA_API lua_Number lua_checkinteger (lua_State *L, int idx) {  /* 7.7.11 */
  StkId o;
  lua_Number x;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TNUMBER)
    tag_error(L, idx, LUA_TNUMBER);
  x = nvalue(o);
  if (tools_isfrac(x))  /* 2.14.13 change */
    luaL_error(L, "Wrong argument #%d: expected an integer, got fraction.", idx);
  lua_unlock(L);
  return x;
}


LUA_API lua_Number agn_checkpositive (lua_State *L, int idx) {  /* 2.14.3 */
  StkId o;
  lua_Number x;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TNUMBER)
    tag_error(L, idx, LUA_TNUMBER);
  x = nvalue(o);
  if (x <= 0)
    luaL_error(L, "Wrong argument #%d: expected a positive number.", idx);
  lua_unlock(L);
  return x;
}


LUA_API lua_Number agn_checknonnegative (lua_State *L, int idx) {  /* 2.14.3 */
  StkId o;
  lua_Number x;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TNUMBER)
    tag_error(L, idx, LUA_TNUMBER);
  x = nvalue(o);
  if (x < 0)
    luaL_error(L, "Wrong argument #%d: expected a non-negative number.", idx);
  lua_unlock(L);
  return x;
}


LUA_API lua_Integer agn_checkposint (lua_State *L, int idx) {  /* 2.7.0 */
  StkId o;
  lua_Number x;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TNUMBER)
    tag_error(L, idx, LUA_TNUMBER);
  x = nvalue(o);
  if (!tools_isposint(x))  /* 2.14.13 change */
    luaL_error(L, "Wrong argument #%d: expected a positive integer.", idx);
  lua_unlock(L);
  return x;
}


LUA_API lua_Integer agn_checknonnegint (lua_State *L, int idx) {  /* 2.9.8 */
  StkId o;
  lua_Number x;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TNUMBER)
    tag_error(L, idx, LUA_TNUMBER);
  x = nvalue(o);
  if (!tools_isnonnegint(x))  /* 2.14.13 change */
    luaL_error(L, "Wrong argument #%d: expected a non-negative integer.", idx);
  lua_unlock(L);
  return x;
}


LUA_API lua_Integer agn_checknonzeroint (lua_State *L, int idx) {  /* 4.11.0 */
  StkId o;
  lua_Number x;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TNUMBER)
    tag_error(L, idx, LUA_TNUMBER);
  x = nvalue(o);
  if (!tools_isnonzeroint(x))
    luaL_error(L, "Wrong argument #%d: expected a non-zero integer.", idx);
  lua_unlock(L);
  return x;
}


LUA_API uint32_t agn_checkuint32_t (lua_State *L, int idx) {  /* 2.16.11 */
  StkId o;
  lua_Number x;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TNUMBER)
    tag_error(L, idx, LUA_TNUMBER);
  x = nvalue(o);
  if (!tools_isuint(x, 32))  /* 0xffffffff = 2^32 - 1 */
    luaL_error(L, "Wrong argument #%d: expected a %d-byte unsigned integer.", idx, sizeof(uint32_t));
  lua_unlock(L);
  return x;
}


LUA_API uint16_t agn_checkuint16_t (lua_State *L, int idx) {  /* 2.16.11, UNUSED */
  StkId o;
  lua_Number x;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TNUMBER)
    tag_error(L, idx, LUA_TNUMBER);
  x = nvalue(o);
  if (!tools_isuint(x, 16))  /* 0xffff = 2^16 - 1 */
    luaL_error(L, "Wrong argument #%d: expected 2-byte unsigned integer.", idx);
  lua_unlock(L);
  return x;
}


#ifndef PROPCMPLX
LUA_API agn_Complex agn_checkcomplex (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TCOMPLEX)
    tag_error(L, idx, LUA_TCOMPLEX);
  lua_unlock(L);
  return cvalue(o);
}


LUA_API agn_Complex agn_optcomplex (lua_State *L, int narg, agn_Complex def) {
  return luaL_opt(L, agn_checkcomplex, narg, def);
}
#else
LUA_API lua_Number *agn_checkcomplex (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) != LUA_TCOMPLEX)
    tag_error(L, idx, LUA_TCOMPLEX);
  lua_unlock(L);
  return cvalue(o);
}


/* def must be a lua_Number array of two slots; the result is stored in re and im. 3.10.8 */
LUA_API void agn_optcomplex (lua_State *L, int idx, lua_Number *def, lua_Number *re, lua_Number *im) {
  StkId o;
  lua_lock(L);
  *re = 0.0; *im = 0.0;
  o = index2adr(L, idx);
  if (lua_isnoneornil(L, idx)) {
    *re = def[0];
    *im = def[1];
  } else if (ttype(o) != LUA_TCOMPLEX) {
    tag_error(L, idx, LUA_TCOMPLEX);
  } else {
    *re = cvalue(o)[0];
    *im = cvalue(o)[1];
  }
}
#endif


/* string check without conversion of numbers, 0.9.1, 20.01.2008 */
LUA_API const char *agn_checklstring (lua_State *L, int idx, size_t *len) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  if (!ttisstring(o)) {
    tag_error(L, idx, LUA_TSTRING);
    if (len != NULL) *len = 0;
    lua_unlock(L);
    return NULL;
  }
  if (len != NULL) *len = tsvalue(o)->len;
  lua_unlock(L);
  return svalue(o);
}


/* string check without conversion of numbers, 0.9.1, 20.01.2008 */
LUA_API const char *agn_checkstring (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  if (!ttisstring(o)) {
    tag_error(L, idx, LUA_TSTRING);
    lua_unlock(L);
    return NULL;
  }
  lua_unlock(L);
  return svalue(o);
}


LUA_API int lua_istrue (lua_State *L, int idx) {  /* new 7.6.2 */
  StkId o = index2adr(L, idx);
  return o != luaO_nilobject && ttype(o) == LUA_TBOOLEAN && bvalue(o) == 1;  /* 1 is true, 2 is fail, 0 is false */
}


LUA_API int lua_isfalse (lua_State *L, int idx) {  /* new 7.6.2 */
  StkId o = index2adr(L, idx);
  return o != luaO_nilobject && ttype(o) == LUA_TBOOLEAN && bvalue(o) == 0;  /* 1 is true, 2 is fail, 0 is false */
}


LUA_API int lua_isfail (lua_State *L, int idx) {  /* new 7.6.2 */
  StkId o = index2adr(L, idx);
  return o != luaO_nilobject && ttype(o) == LUA_TBOOLEAN && bvalue(o) == 2;  /* 1 is true, 2 is fail, 0 is false */
}


LUA_API int lua_isfalseorfail (lua_State *L, int idx) {  /* new 7.6.2 */
  StkId o = index2adr(L, idx);
  return o != luaO_nilobject && ttype(o) == LUA_TBOOLEAN && bvalue(o) != 1;  /* 1 is true, 2 is fail, 0 is false */
}


LUA_API int lua_isnilfalseorfail (lua_State *L, int idx) {  /* new 7.6.2 */
  StkId o = index2adr(L, idx);
  int tt = ttype(o);
  return o != luaO_nilobject && (tt == LUA_TNIL || (tt == LUA_TBOOLEAN && bvalue(o) != 1));
}


LUA_API int agn_istrue (lua_State *L, int idx) {  /* 0.5.2, June 01, 2007; tuned April 02, 2008 */
  return bvalue(index2adr(L, idx)) == 1;  /* 1 is true, 2 is fail, 0 is false */
}


LUA_API int agn_isfalse (lua_State *L, int idx) {  /* 0.10.0, April 02, 2008 ? */
  return bvalue(index2adr(L, idx)) == 0;  /* 1 is true, 2 is fail, 0 is false */
}


LUA_API int agn_isfail (lua_State *L, int idx) {  /* 0.10.0, April 02, 2008 */
  return bvalue(index2adr(L, idx)) == 2;  /* 1 is true, 2 is fail, 0 is false, 7.6.2 change */
}


LUA_API int agn_fnext (lua_State *L, int idxt, int idxf, int mode) {
  StkId t, f;
  int more;
  lua_lock(L);
  t = index2adr(L, idxt);
  api_check(L, ttistable(t));
  f = index2adr(L, idxf);
  api_check(L, ttisfunction(f));
  /* api_incr_top* aka luai_apicheck do not check whether there is enough stack space, so call luaL_checkstack explicitly, 3.15.1 fix */
  luaL_checkstack(L, 3 + mode, "not enough stack space");
  more = luaHF_next(L, hvalue(t), f, L->top - 1, mode);
  if (more) {
    if (mode == 1) {
      api_incr_top_by_three(L);
    }
    else {
      api_incr_top_by_two(L);
    }
  }
  else  /* no more elements */
    L->top--;  /* remove key */
  lua_unlock(L);
  return more;
}


/* extended 0.22.0 to report holes in an array */
LUA_API void agn_tablestate (lua_State *L, int idx, size_t a[], int mode) {
  StkId o;
  Table *t;
  int lowestidx;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  t = hvalue(o);
  if (mode)
    agnH_nops(L, t, a);  /* a[0] = assigned elements in array, a[1] = assigned elements in hash part,
                            a[2] = info whether array contains at least one hole */
  else {
    a[0] = a[1] = a[2] = 0;
  }
  a[3] = t->sizearray;  /* number of allocated elements in array */
  /* number of allocated elements in hash part: */
  a[4] = (a[1] == 1 && actnodesize(t) == 0) ? 1 : actnodesize(t);  /* Agena 1.0.5 patch */
  a[5] = 0;  /* DELETE ME, formerly t->hasnil flag */
  a[6] = t->metatable != NULL;
  a[7] = luaH_isdummy(t->node);  /* 2.3.0 RC 4 */
  a[8] = luaH_getn(t);  /* 2.3.0 RC 4 */
  a[10] = agnH_aborders(L, t, &lowestidx);  /* 4.9.0 fix, avoid unallocated values */
  a[9] = lowestidx;  /* ditto */
  a[11] = agnH_readonly(L, t, -1);
  lua_unlock(L);
}


LUA_API void agn_tablesize (lua_State *L, int idx, size_t a[]) {  /* 2.3.0 RC 4 */
  StkId o;
  Table *t;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  t = hvalue(o);
  a[0] = luaH_getn(t);  /* rough guess on the size of a table using a logarithmic method */
  a[1] = luaH_isdummy(t->node);  /* hash part empty ? */
  a[2] = agnH_hsize(L, t); /* actual hash size, no guess; 4.10.2 */
  lua_unlock(L);
}


LUA_API void agn_arrayborders (lua_State *L, int idx, size_t a[]) {  /* 2.14.10 */
  StkId o;
  Table *t;
  int low;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  t = hvalue(o);
  a[1] = agnH_aborders(L, t, &low);
  a[0] = low;
  lua_unlock(L);
}


/* Returns the smallest and largest assigned index - in this order - in the array and hash part of a table, in array a.
   If zeros are returned, the array and hash parts of the table are empty. 2.30.1 */
LUA_API void agn_borders (lua_State *L, int idx, size_t a[]) {
  StkId o;
  Table *t;
  int low;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  t = hvalue(o);
  a[1] = agnH_borders(L, t, &low);
  a[0] = low;
  lua_unlock(L);
}


/* Returns all integer indices of the table t at stack index idx in a new table and puts it onto the top of the stack.
   It also sets flag to 0 if there are no integer indices in hash part of t, and flag to 1 if there is at least one
   integer index in the hash part. 2.30.1 */
LUA_API void agn_intindices (lua_State *L, int idx, int *flag) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  agenaV_tintindices(L, o, flag);  /* increases the stack top by one */
  lua_unlock(L);
}


LUA_API void agn_intentries (lua_State *L, int idx, int *flag) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  agenaV_tintentries(L, o, flag);  /* increases the stack top by one */
  lua_unlock(L);
}



/* Returns all the values stored to the table at stack index idx in a new table and sets it to the top of the stack.
   flag is set to 0 if no value are residing in the hash part, and to 1 if there is at least one element in the hash
   part. 2.30.1 */
LUA_API void agn_entries (lua_State *L, int idx, int *flag) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  agenaV_tentries(L, o, flag);  /* increases the stack top by one */
  lua_unlock(L);
}


/* Pushes two tables onto the top of the stack: one with all the values in the array part of the table at index idx,
  and one with the values in its hash part. 2.30.1 */
LUA_API void agn_parts (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  agenaV_tparts(L, o, 0);  /* increases the stack top by one */
  agenaV_tparts(L, o, 1);  /* increases the stack top by one, as well */
  lua_unlock(L);
}


/* Pushes a table with all the values in the array part of the table at index idx onto the top of the stack. 2.30.1 */
LUA_API void agn_arraypart (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  agenaV_tparts(L, o, 0);  /* increases the stack top by one */
  lua_unlock(L);
}


/* Pushes a table with all the values in the hash part of the table at index idx onto the top of the stack. 2.30.1 */
LUA_API void agn_hashpart (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  agenaV_tparts(L, o, 1);  /* increases the stack top by one */
  lua_unlock(L);
}


/* Checks whether the table at stack index idx has at least one element assigned in its array part. The return is either 1
  (at least one element is in the array part) or 0 otherwise. The function pushes nothing and leaves the stack unchanged. */
LUA_API int agn_hasarraypart (lua_State *L, int idx) {  /* 3.10.0 */
  int rc;
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  rc = agnH_hasarraypart(L, hvalue(o));
  lua_unlock(L);
  return rc;
}


/* Checks whether the table at stack index idx has at least one element assigned in its hash part. The return is either 1
  (at least one element is in the hash part) or 0 otherwise. The function pushes nothing and leaves the stack unchanged. */
LUA_API int agn_hashashpart (lua_State *L, int idx) {  /* 3.10.0 */
  int rc;
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  rc = agnH_hashashpart(L, hvalue(o));
  lua_unlock(L);
  return rc;
}


/* Checks whether the value at stack index idxv is included in the structure at idxt. If idxt does not refer to a structure,
   the function triggers an error. The function returns 1 if the element has been found and 0 otherwise.
   If mode is 1 then the function pushes `true` or `false` on the stack. 3.9.6 */
LUA_API int agn_in (lua_State *L, int idxv, int idxt, int mode) {
  int rc;
  lua_lock(L);
  agenaV_in(L, index2adr(L, idxv), index2adr(L, idxt), L->top);
  rc = ttistrue(L->top);
  if (mode) {  /* push true or false on stack ? */
    api_incr_top(L);  /* increase the stack top by one */
  } else {  /* push nothing */
    setnilvalue(L->top);
  }
  lua_unlock(L);
  return rc;
}


/* Deletes all occurrences of the value at stack index idxv from the table, set, sequence
   or register at index idx. All other types are simply ignored. Returns the number of
   deletions, which might be zero if the values could not be found. 5.1.2 */
LUA_API int agn_deletefrom (lua_State *L, int idxv, int idx) {
  int cnt;
  lua_lock(L);
  cnt = agenaV_deletefrom(L, index2adr(L, idxv), index2adr(L, idx));
  lua_unlock(L);
  return cnt;
}


/* Removes a value at position pos from the array part of a table at stack index idx and closes the space by shifting down
   other elements, if necessary. pos must be positive ! len is the length of the array part; if len is -1, then the function
   automatically determines it. The function does not change the stack. 3.9.0 */
LUA_API int agn_tabpurge (lua_State *L, int idx, int len, int pos) {
  if (len == -1) len = luaL_getn(L, idx);
  /* Lua 5.1.3 patch; position is outside bounds ? -> nothing to remove */
  if (!agnH_purge(L, hvalue(index2adr(L, idx)), pos, len)) return 0;
  luaL_setn(L, idx, len - 1);  /* t.n = n-1 */
  return 1;
}


LUA_API int agn_tabsubs (lua_State *L, int idx, int idxold, int idxnew) {  /* 5.1.3 */
  int cnt;
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  cnt = agnH_subs(L, hvalue(o), index2adr(L, idxold), index2adr(L, idxnew));
  lua_unlock(L);
  return cnt;
}


LUA_API int agn_seqsubs (lua_State *L, int idx, int idxold, int idxnew) {  /* 5.1.3 */
  int cnt;
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttisseq(o));
  cnt = agnSeq_subs(L, seqvalue(o), index2adr(L, idxold), index2adr(L, idxnew));
  lua_unlock(L);
  return cnt;
}


LUA_API int agn_regsubs (lua_State *L, int idx, int idxold, int idxnew) {  /* 5.1.3 */
  int cnt;
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttisreg(o));
  cnt = agnReg_subs(L, regvalue(o), index2adr(L, idxold), index2adr(L, idxnew));
  lua_unlock(L);
  return cnt;
}


/* Counts the number of elements in the union a, b. 3.10.0 */
LUA_API int agn_numunion (lua_State *L, int idxa, int idxb) {
  int r;
  lua_lock(L);
  r = agenaV_numunion(L, index2adr(L, idxa), index2adr(L, idxb));
  lua_unlock(L);
  return r;
}


/* Counts the number of elements in the intersection a, b. 3.10.0 */
LUA_API int agn_numintersect (lua_State *L, int idxa, int idxb) {
  int r;
  lua_lock(L);
  r = agenaV_numsetops(L, index2adr(L, idxa), index2adr(L, idxb), 0);
  lua_unlock(L);
  return r;
}


/* Counts the number of elements in the difference set a \ b. 3.10.0 */
LUA_API int agn_numminus (lua_State *L, int idxa, int idxb) {
  int r;
  lua_lock(L);
  r = agenaV_numsetops(L, index2adr(L, idxa), index2adr(L, idxb), 1);
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a table are of type number. 3.10.2 */
LUA_API int agn_tisnumber (lua_State *L, int idx, int integralkeysonly) {
  int i, c;
  TValue *key, *val;
  Node *g;
  Table *t = hvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;
  for (i=0; i < t->sizearray; i++) {
    val = &t->array[i];
    if (ttisnotnil(val)) {  /* 3.19.2 fix */
      c++;
      if (!ttisnumber(val)) return 0;  /* 3.19.2 extension */
    }
  }
  for (i=0; i < sizenode(t); i++) {
    g = gnode(t, i);
    key = key2tval(g);
    if (integralkeysonly && (!ttisnumber(key) || tools_isfrac(nvalue(key)))) continue;  /* 3.19.2 extension */
    val = gval(g);
    if (ttisnotnil(val)) {  /* 3.19.2 fix */
      c++;
      if (!ttisnumber(val)) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a table are of type number or complex. 3.19.2 */
LUA_API int agn_tisnumeric (lua_State *L, int idx, int integralkeysonly) {
  int i, c;
  TValue *key, *val;
  Node *g;
  Table *t = hvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < t->sizearray; i++) {
    val = &t->array[i];
    if (ttisnotnil(val)) {
      c++;
      if (!(ttiscomplex(val) || ttisnumber(val))) return 0;
    }
  }
  for (i=0; i < sizenode(t); i++) {
    g = gnode(t, i);
    key = key2tval(g);
    if (integralkeysonly && (!ttisnumber(key) || tools_isfrac(nvalue(key)))) continue;
    val = gval(g);
    if (ttisnotnil(val)) {
      c++;
      if (!(ttiscomplex(val) || ttisnumber(val))) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a table are of type number and are all integral. 3.10.2 */
LUA_API int agn_tisintegral (lua_State *L, int idx, int integralkeysonly) {
  int i, c;
  TValue *key, *val;
  Node *g;
  Table *t = hvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;
  for (i=0; i < t->sizearray; i++) {
    val = &t->array[i];
    if (ttisnotnil(val)) {  /* 3.19.2 fix */
      c++;
      if (!ttisnumber(val) || tools_isfrac(nvalue(val))) return 0;  /* 3.19.2 extension */
    }
  }
  for (i=0; i < sizenode(t); i++) {
    g = gnode(t, i);
    key = key2tval(g);
    if (integralkeysonly && (!ttisnumber(key) || tools_isfrac(nvalue(key)))) continue;  /* 3.19.2 extension */
    val = gval(g);
    if (ttisnotnil(val)) {  /* 3.19.2 fix */
      c++;
      if (!ttisnumber(val) || tools_isfrac(nvalue(val))) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* int agenaV_tisofnumerictype (lua_State *L, TValue *tbl, int (*fn)(double)):

   agenaV_tisnonnegative -> tools_isnonnegative
   agenaV_tisnonnegint   -> tools_isnonnegint
   agenaV_tispositive    -> tools_ispositive
   agenaV_tisposint      -> tools_isposint       */

/* Checks whether all elements in a table are of type number and are all positive integers. 3.10.2 */
LUA_API int agn_tisposint (lua_State *L, int idx, int integralkeysonly) {
  int r;
  lua_lock(L);
  r = agenaV_tisofnumerictype(L, index2adr(L, idx), tools_isposint, integralkeysonly);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a table are of type number and are all positive numbers. 3.10.2 */
LUA_API int agn_tispositive (lua_State *L, int idx, int integralkeysonly) {
  int r;
  lua_lock(L);
  r = agenaV_tisofnumerictype(L, index2adr(L, idx), tools_ispositive, integralkeysonly);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a table are of type number and are all non-negative integers. 3.10.2 */
LUA_API int agn_tisnonnegint (lua_State *L, int idx, int integralkeysonly) {
  int r;
  lua_lock(L);
  r = agenaV_tisofnumerictype(L, index2adr(L, idx), tools_isnonnegint, integralkeysonly);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a table are of type number and are all non-zero integers. 4.11.0 */
LUA_API int agn_tisnonzeroint (lua_State *L, int idx, int integralkeysonly) {
  int r;
  lua_lock(L);
  r = agenaV_tisofnumerictype(L, index2adr(L, idx), tools_isnonzeroint, integralkeysonly);
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a table are of type number and are all non-negative integers. 3.10.2 */
LUA_API int agn_tisnonnegative (lua_State *L, int idx, int integralkeysonly) {
  int r;
  lua_lock(L);
  r = agenaV_tisofnumerictype(L, index2adr(L, idx), tools_isnonnegative, integralkeysonly);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a table are of type complex. 3.10.2 */
LUA_API int agn_tiscomplex (lua_State *L, int idx, int integralkeysonly) {
  int i, c;
  TValue *key, *val;
  Node *g;
  Table *t = hvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < t->sizearray; i++) {
    val = &t->array[i];
    if (ttisnotnil(val)) {
      c++;
      if (!ttiscomplex(val)) return 0;
    }
  }
  for (i=0; i < sizenode(t); i++) {
    g = gnode(t, i);
    key = key2tval(g);
    if (integralkeysonly && (!ttisnumber(key) || tools_isfrac(nvalue(key)))) continue;  /* 3.19.2 extension */
    val = gval(g);
    if (ttisnotnil(val)) {
      c++;
      if (!ttiscomplex(val)) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a table are of type string. 3.10.2 */
LUA_API int agn_tisstring (lua_State *L, int idx, int integralkeysonly) {
  int i, c;
  TValue *key, *val;
  Node *g;
  Table *t = hvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < t->sizearray; i++) {
    val = &t->array[i];
    if (ttisnotnil(val)) {
      c++;
      if (!ttisstring(val)) return 0;
    }
  }
  for (i=0; i < sizenode(t); i++) {
    g = gnode(t, i);
    key = key2tval(g);
    if (integralkeysonly && (!ttisnumber(key) || tools_isfrac(nvalue(key)))) continue;  /* 3.19.2 extension */
    val = gval(g);
    if (ttisnotnil(val)) {
      c++;
      if (!ttisstring(val)) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a table are of type boolean. 3.10.2 */
LUA_API int agn_tisboolean (lua_State *L, int idx, int integralkeysonly) {
  int i, c;
  TValue *key, *val;
  Node *g;
  Table *t = hvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < t->sizearray; i++) {
    val = &t->array[i];
    if (ttisnotnil(val)) {
      c++;
      if (!ttisboolean(val)) return 0;
    }
  }
  for (i=0; i < sizenode(t); i++) {
    g = gnode(t, i);
    key = key2tval(g);
    if (integralkeysonly && (!ttisnumber(key) || tools_isfrac(nvalue(key)))) continue;  /* 3.19.2 extension */
    val = gval(g);
    if (ttisnotnil(val)) {
      c++;
      if (!ttisboolean(val)) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a set are of type number. 3.10.2 */
LUA_API int agn_usisnumber (lua_State *L, int idx) {
  int i, c;
  TValue *val;
  UltraSet *t = usvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < sizenode(t); i++) {
    if (ttisnotnil(glkey(gnode(t, i)))) {
      c++;
      val = key2tval(gnode(t, i));
      if (!ttisnumber(val)) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a set are of type number and are all integral. 3.10.2 */
LUA_API int agn_usisintegral (lua_State *L, int idx) {
  int i, c;
  TValue *val;
  UltraSet *t = usvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < sizenode(t); i++) {
    if (ttisnotnil(glkey(gnode(t, i)))) {
      c++;
      val = key2tval(gnode(t, i));
      if (!ttisnumber(val) || tools_isfrac(nvalue(val))) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a set are of type complex. 3.10.2 */
LUA_API int agn_usiscomplex (lua_State *L, int idx) {
  int i, c;
  TValue *val;
  UltraSet *t = usvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < sizenode(t); i++) {
    if (ttisnotnil(glkey(gnode(t, i)))) {
      c++;
      val = key2tval(gnode(t, i));
      if (!ttiscomplex(val)) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a set are of type string. 3.10.2 */
LUA_API int agn_usisstring (lua_State *L, int idx) {
  int i, c;
  TValue *val;
  UltraSet *t = usvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < sizenode(t); i++) {
    if (ttisnotnil(glkey(gnode(t, i)))) {
      c++;
      val = key2tval(gnode(t, i));
      if (!ttisstring(val)) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a set are of type boolean. 3.10.2 */
LUA_API int agn_usisboolean (lua_State *L, int idx) {
  int i, c;
  TValue *val;
  UltraSet *t = usvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < sizenode(t); i++) {
    if (ttisnotnil(glkey(gnode(t, i)))) {
      c++;
      val = key2tval(gnode(t, i));
      if (!ttisboolean(val)) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a sequence are of type number and are all positive integers. 3.19.2 */
LUA_API int agn_usisposint (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_usisofnumerictype(L, index2adr(L, idx), tools_isposint);
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a sequence are of type number and are all positive numbers. 3.19.2 */
LUA_API int agn_usispositive (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_usisofnumerictype(L, index2adr(L, idx), tools_ispositive);
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a sequence are of type number and are all non-negative integers. 3.19.2 */
LUA_API int agn_usisnonnegint (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_usisofnumerictype(L, index2adr(L, idx), tools_isnonnegint);
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a sequence are of type number and are all non-zero integers. 4.11.0 */
LUA_API int agn_usisnonzeroint (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_usisofnumerictype(L, index2adr(L, idx), tools_isnonzeroint);
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a sequence are of type number and are all non-negative numbers. 3.19.2 */
LUA_API int agn_usisnonnegative (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_usisofnumerictype(L, index2adr(L, idx), tools_isnonnegative);
  lua_unlock(L);
  return r;
}


LUA_API int agn_usisnumeric (lua_State *L, int idx) {  /* 3.19.2 */
  int i, c;
  TValue *val;
  UltraSet *t = usvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < sizenode(t); i++) {
    if (ttisnotnil(glkey(gnode(t, i)))) {
      c++;
      val = key2tval(gnode(t, i));
      if (ttisnotnil(val) && !(ttisnumber(val) || ttiscomplex(val))) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a sequence are of type number. 3.10.2 */
LUA_API int agn_seqisnumber (lua_State *L, int idx) {
  int i;
  TValue *val;
  Seq *h = seqvalue(index2adr(L, idx));
  lua_lock(L);
  for (i=0; i < h->size; i++) {
    val = seqitem(h, i);
    if (ttisnotnil(val) && !ttisnumber(val)) return 0;
  }
  lua_unlock(L);
  return h->size && 1;  /* 3.19.2 fix */
}


/* Checks whether all elements in a sequence are of type number or complex. 3.19.2 */
LUA_API int agn_seqisnumeric (lua_State *L, int idx) {
  int i;
  TValue *val;
  Seq *h = seqvalue(index2adr(L, idx));
  lua_lock(L);
  for (i=0; i < h->size; i++) {
    val = seqitem(h, i);
    if (ttisnotnil(val) && !(ttisnumber(val) || ttiscomplex(val))) return 0;
  }
  lua_unlock(L);
  return h->size && 1;
}


/* Checks whether all elements in a sequence are of type number and are all integral. 3.10.2 */
LUA_API int agn_seqisintegral (lua_State *L, int idx) {
  int i;
  TValue *val;
  Seq *h = seqvalue(index2adr(L, idx));
  lua_lock(L);
  for (i=0; i < h->size; i++) {
    val = seqitem(h, i);
    if (ttisnotnil(val) && (!ttisnumber(val) || tools_isfrac(nvalue(val)))) return 0;
  }
  lua_unlock(L);
  return h->size && 1;  /* 3.19.2 fix */
}


/* Checks whether all elements in a sequence are of type number and are all positive integers. 3.10.2 */
LUA_API int agn_seqisposint (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_seqisofnumerictype(L, index2adr(L, idx), tools_isposint);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a sequence are of type number and are all positive numbers. 3.10.2 */
LUA_API int agn_seqispositive (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_seqisofnumerictype(L, index2adr(L, idx), tools_ispositive);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a sequence are of type number and are all non-negative integers. 3.10.2 */
LUA_API int agn_seqisnonnegint (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_seqisofnumerictype(L, index2adr(L, idx), tools_isnonnegint);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a sequence are of type number and are all non-zero integers. 4.11.0 */
LUA_API int agn_seqisnonzeroint (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_seqisofnumerictype(L, index2adr(L, idx), tools_isnonzeroint);
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a sequence are of type number and are all non-negative numbers. 3.10.2 */
LUA_API int agn_seqisnonnegative (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_seqisofnumerictype(L, index2adr(L, idx), tools_isnonnegative);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a sequence are of type complex. 3.10.2 */
LUA_API int agn_seqiscomplex (lua_State *L, int idx) {
  int i;
  TValue *val;
  Seq *h = seqvalue(index2adr(L, idx));
  lua_lock(L);
  for (i=0; i < h->size; i++) {
    val = seqitem(h, i);
    if (ttisnotnil(val) && !ttiscomplex(val)) return 0;
  }
  lua_unlock(L);
  return h->size && 1;  /* 3.19.2 fix */
}


/* Checks whether all elements in a sequence are of type string. 3.10.2 */
LUA_API int agn_seqisstring (lua_State *L, int idx) {
  int i;
  TValue *val;
  Seq *h = seqvalue(index2adr(L, idx));
  lua_lock(L);
  for (i=0; i < h->size; i++) {
    val = seqitem(h, i);
    if (ttisnotnil(val) && !ttisstring(val)) return 0;
  }
  lua_unlock(L);
  return h->size && 1;  /* 3.19.2 fix */
}


/* Checks whether all elements in a sequence are of type boolean. 3.10.2 */
LUA_API int agn_seqisboolean (lua_State *L, int idx) {
  int i;
  TValue *val;
  Seq *h = seqvalue(index2adr(L, idx));
  lua_lock(L);
  for (i=0; i < h->size; i++) {
    val = seqitem(h, i);
    if (ttisnotnil(val) && !ttisboolean(val)) return 0;
  }
  lua_unlock(L);
  return h->size && 1;  /* 3.19.2 fix */
}


/* Checks whether all elements in a register are of type number. 3.10.2 */
LUA_API int agn_regisnumber (lua_State *L, int idx) {
  int i, c;
  TValue *val;
  lua_lock(L);
  Reg *h = regvalue(index2adr(L, idx));
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < h->top; i++) {
    val = regitem(h, i);
    if (ttisnotnil(val)) {
      c++;
      if (!ttisnumber(val)) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a register are of type number or complex. 3.19.2 */
LUA_API int agn_regisnumeric (lua_State *L, int idx) {
  int i, c;
  TValue *val;
  lua_lock(L);
  Reg *h = regvalue(index2adr(L, idx));
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < h->top; i++) {
    val = regitem(h, i);
    if (ttisnotnil(val)) {
      c++;
      if (!(ttisnumber(val) || ttiscomplex(val))) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;
}


/* Checks whether all elements in a register are of type number and are all integral. 3.10.2 */
LUA_API int agn_regisintegral (lua_State *L, int idx) {
  int i, c;
  TValue *val;
  lua_lock(L);
  Reg *h = regvalue(index2adr(L, idx));
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < h->top; i++) {
    val = regitem(h, i);
    if (ttisnotnil(val)) {
      c++;
      if (!ttisnumber(val) || tools_isfrac(nvalue(val))) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;  /* 3.19.2 fix */
}


/* Checks whether all elements in a register are of type number and are all positive integers. 3.10.2 */
LUA_API int agn_regisposint (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_regisofnumerictype(L, index2adr(L, idx), tools_isposint);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a register are of type number and are all positive numbers. 3.10.2 */
LUA_API int agn_regispositive (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_regisofnumerictype(L, index2adr(L, idx), tools_ispositive);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a register are of type number and are all non-negative integers. 3.10.2 */
LUA_API int agn_regisnonnegint (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_regisofnumerictype(L, index2adr(L, idx), tools_isnonnegint);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a register are of type number and are all non-zero integers. 4.11.0 */
LUA_API int agn_regisnonzeroint (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_regisofnumerictype(L, index2adr(L, idx), tools_isnonnegint);
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a register are of type number and are all non-negative numbers. 3.10.2 */
LUA_API int agn_regisnonnegative (lua_State *L, int idx) {
  int r;
  lua_lock(L);
  r = agenaV_regisofnumerictype(L, index2adr(L, idx), tools_isnonnegative);  /* 3.15.4 change */
  lua_unlock(L);
  return r;
}


/* Checks whether all elements in a register are of type complex. 3.10.2 */
LUA_API int agn_regiscomplex (lua_State *L, int idx) {
  int i, c;
  TValue *val;
  lua_lock(L);
  Reg *h = regvalue(index2adr(L, idx));
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < h->top; i++) {
    val = regitem(h, i);
    if (ttisnotnil(val)) {
      c++;
      if (!ttiscomplex(val)) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;  /* 3.19.2 fix */
}


/* Checks whether all elements in a register are of type string. 3.10.2 */
LUA_API int agn_regisstring (lua_State *L, int idx) {
  int i, c;
  TValue *val;
  lua_lock(L);
  Reg *h = regvalue(index2adr(L, idx));
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < h->top; i++) {
    val = regitem(h, i);
    if (ttisnotnil(val)) {
      c++;
      if (!ttisstring(val)) return 0;
    }
  }
  lua_unlock(L);
  return c && 1;  /* 3.19.2 fix */
}


/* Checks whether all elements in a register are of type boolean. 3.10.2 */
LUA_API int agn_regisboolean (lua_State *L, int idx) {
  int i, c;
  TValue *val;
  Reg *h = regvalue(index2adr(L, idx));
  lua_lock(L);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < h->top; i++) {
    val = regitem(h, i);
    if (ttisnotnil(val)) {
      c++;
      if (!ttisboolean(val)) return 0;
    }
  }
  return c && 1;  /* 3.19.2 fix */
  lua_unlock(L);
}


/* Checks whether all elements in a register are null. 4.10.3 */
LUA_API int agn_regisnil (lua_State *L, int idx) {
  int i;
  lua_lock(L);
  Reg *h = regvalue(index2adr(L, idx));
  for (i=0; i < h->top; i++) {
    if (ttisnotnil(regitem(h, i))) return 0;
  }
  lua_unlock(L);
  return 1;
}


/* Checks whether all elements in table t are of type x, a string, and returns true or false. Eligible values for x are
   "number", "integer" (numbers that are all integral), "complex", "string", "boolean", "posint", "positive",
   "nonnegint", "nonzeroint", "nonnegative". 3.10.2, externalised 4.6.2

   The table is at stack index idx, the typename on idx + 1 and the integralkeysonly flag on idx + 1. */
/* typenames; grep "(GREP_POINT) types;" if you want to change their order or add new ones */
LUA_API int agn_tblisall (lua_State *L, int idx, const char *procname) {
  int integralkeysonly;
  const char *type;
  luaL_checktype(L, idx, LUA_TTABLE);
  type = agn_checkstring(L, idx + 1);
  integralkeysonly = agnL_optboolean(L, idx + 2, 0);
  if (tools_streq(type, llex_token2str(TK_TNUMBER)))
    lua_pushboolean(L, agn_tisnumber(L, idx, integralkeysonly));
  else if (tools_streq(type, llex_token2str(TK_INTEGER)))
    lua_pushboolean(L, agn_tisintegral(L, idx, integralkeysonly));
  else if (tools_streq(type, llex_token2str(TK_TCOMPLEX)))
    lua_pushboolean(L, agn_tiscomplex(L, idx, integralkeysonly));
  else if (tools_streq(type, llex_token2str(TK_TSTRING)))
    lua_pushboolean(L, agn_tisstring(L, idx, integralkeysonly));
  else if (tools_streq(type, llex_token2str(TK_TBOOLEAN)))
    lua_pushboolean(L, agn_tisboolean(L, idx, integralkeysonly));
  else if (tools_streq(type, llex_token2str(TK_POSINT)))
    lua_pushboolean(L, agn_tisposint(L, idx, integralkeysonly));
  else if (tools_streq(type, llex_token2str(TK_POSITIVE)))
    lua_pushboolean(L, agn_tispositive(L, idx, integralkeysonly));
  else if (tools_streq(type, llex_token2str(TK_NONNEGINT)))
    lua_pushboolean(L, agn_tisnonnegint(L, idx, integralkeysonly));
  else if (tools_streq(type, llex_token2str(TK_NONZEROINT)))  /* 4.11.0 */
    lua_pushboolean(L, agn_tisnonzeroint(L, idx, integralkeysonly));
  else if (tools_streq(type, llex_token2str(TK_NONNEGATIVE)))
    lua_pushboolean(L, agn_tisnonnegative(L, idx, integralkeysonly));
  else if (tools_streq(type, "numeric"))  /* 3.19.2 extension */
    lua_pushboolean(L, agn_tisnumeric(L, idx, integralkeysonly));
  else
    luaL_error(L, "Error in " LUA_QS ": unsupported type " LUA_QS ".", procname, type);
  return 1;
}


/* Checks whether all elements in set s are of type x, a string, and returns true or false. Eligible values for x are
   "number", "integer" (numbers that are all integral), "complex", "string" and "boolean". 3.10.2; externalised 4.6.2
   The set is at stack index idx, the typename on idx + 1. Externalised 4.6.2 */
/* typenames; grep "(GREP_POINT) types;" if you want to change their order or add new ones */
LUA_API int agn_usisall (lua_State *L, int idx, const char *procname) {
  const char *type;
  luaL_checktype(L, idx, LUA_TSET);
  type = agn_checkstring(L, idx + 1);
  if (tools_streq(type, llex_token2str(TK_TNUMBER)))
    lua_pushboolean(L, agn_usisnumber(L, idx));
  else if (tools_streq(type, llex_token2str(TK_INTEGER)))
    lua_pushboolean(L, agn_usisintegral(L, idx));
  else if (tools_streq(type, llex_token2str(TK_TCOMPLEX)))
    lua_pushboolean(L, agn_usiscomplex(L, idx));
  else if (tools_streq(type, llex_token2str(TK_TSTRING)))
    lua_pushboolean(L, agn_usisstring(L, idx));
  else if (tools_streq(type, llex_token2str(TK_TBOOLEAN)))
    lua_pushboolean(L, agn_usisboolean(L, idx));
  else if (tools_streq(type, llex_token2str(TK_POSINT)))  /* 3.19.2 extension */
    lua_pushboolean(L, agn_usisposint(L, idx));
  else if (tools_streq(type, llex_token2str(TK_POSITIVE)))  /* 3.19.2 extension */
    lua_pushboolean(L, agn_usispositive(L, idx));
  else if (tools_streq(type, llex_token2str(TK_NONNEGINT)))  /* 3.19.2 extension */
    lua_pushboolean(L, agn_usisnonnegint(L, idx));
  else if (tools_streq(type, llex_token2str(TK_NONZEROINT)))  /* 4.11.0 */
    lua_pushboolean(L, agn_usisnonzeroint(L, idx));
  else if (tools_streq(type, llex_token2str(TK_NONNEGATIVE)))  /* 3.19.2 extension */
    lua_pushboolean(L, agn_usisnonnegative(L, idx));
  else if (tools_streq(type, "numeric"))  /* 3.19.2 extension */
    lua_pushboolean(L, agn_usisnumeric(L, idx));
  else
    luaL_error(L, "Error in " LUA_QS ": unsupported type " LUA_QS ".", procname, type);
  return 1;
}


/* Checks whether all elements in sequence s are of type x, a string, and returns true or false. Eligible values for x are
   "number", "integer" (numbers that are all integral), "complex", "string" and "boolean". 3.10.2; externalised 4.6.2
   The sequence is at stack index idx, the typename on idx + 1. */
/* typenames; grep "(GREP_POINT) types;" if you want to change their order or add new ones */
LUA_API int agn_seqisall (lua_State *L, int idx, const char *procname) {
  const char *type;
  luaL_checktype(L, idx, LUA_TSEQ);
  type = agn_checkstring(L, idx + 1);
  if (tools_streq(type, llex_token2str(TK_TNUMBER)))
    lua_pushboolean(L, agn_seqisnumber(L, idx));
  else if (tools_streq(type, llex_token2str(TK_INTEGER)))
    lua_pushboolean(L, agn_seqisintegral(L, idx));
  else if (tools_streq(type, llex_token2str(TK_TCOMPLEX)))
    lua_pushboolean(L, agn_seqiscomplex(L, idx));
  else if (tools_streq(type, llex_token2str(TK_TSTRING)))
    lua_pushboolean(L, agn_seqisstring(L, idx));
  else if (tools_streq(type, llex_token2str(TK_TBOOLEAN)))
    lua_pushboolean(L, agn_seqisboolean(L, idx));
  else if (tools_streq(type, llex_token2str(TK_POSINT)))
    lua_pushboolean(L, agn_seqisposint(L, idx));
  else if (tools_streq(type, llex_token2str(TK_POSITIVE)))
    lua_pushboolean(L, agn_seqispositive(L, idx));
  else if (tools_streq(type, llex_token2str(TK_NONNEGINT)))
    lua_pushboolean(L, agn_seqisnonnegint(L, idx));
  else if (tools_streq(type, llex_token2str(TK_NONZEROINT)))  /* 4.11.0 */
    lua_pushboolean(L, agn_seqisnonzeroint(L, idx));
  else if (tools_streq(type, llex_token2str(TK_NONNEGATIVE)))
    lua_pushboolean(L, agn_seqisnonnegative(L, idx));
  else if (tools_streq(type, "numeric"))  /* 3.19.2 extension */
    lua_pushboolean(L, agn_seqisnumeric(L, idx));
  else
    luaL_error(L, "Error in " LUA_QS ": unsupported type " LUA_QS ".", "sequences.isall", type);
  return 1;
}


/* Checks whether all elements in register s are of type x, a string, and returns true or false. Eligible values for x are
   "number", "integer" (numbers that are all integral), "complex", "string" and "boolean". 3.10.2; externalised 4.6.2
   The register is at stack index idx, the typename on idx + 1. */
/* typenames; grep "(GREP_POINT) types;" if you want to change their order or add new ones */
LUA_API int agn_regisall (lua_State *L, int idx, const char *procname) {
  const char *type;
  luaL_checktype(L, idx, LUA_TREG);
  type = agn_checkstring(L, idx + 1);
  if (tools_streq(type, llex_token2str(TK_TNUMBER)))
    lua_pushboolean(L, agn_regisnumber(L, idx));
  else if (tools_streq(type, llex_token2str(TK_INTEGER)))
    lua_pushboolean(L, agn_regisintegral(L, idx));
  else if (tools_streq(type, llex_token2str(TK_TCOMPLEX)))
    lua_pushboolean(L, agn_regiscomplex(L, idx));
  else if (tools_streq(type, llex_token2str(TK_TSTRING)))
    lua_pushboolean(L, agn_regisstring(L, idx));
  else if (tools_streq(type, llex_token2str(TK_TBOOLEAN)))
    lua_pushboolean(L, agn_regisboolean(L, idx));
  else if (tools_streq(type, llex_token2str(TK_POSINT)))
    lua_pushboolean(L, agn_regisposint(L, idx));
  else if (tools_streq(type, llex_token2str(TK_POSITIVE)))
    lua_pushboolean(L, agn_regispositive(L, idx));
  else if (tools_streq(type, llex_token2str(TK_NONNEGINT)))
    lua_pushboolean(L, agn_regisnonnegint(L, idx));
  else if (tools_streq(type, llex_token2str(TK_NONZEROINT)))  /* 4.11.0 */
    lua_pushboolean(L, agn_regisnonzeroint(L, idx));
  else if (tools_streq(type, llex_token2str(TK_NONNEGATIVE)))
    lua_pushboolean(L, agn_regisnonnegative(L, idx));
  else if (tools_streq(type, "numeric"))  /* 3.19.2 extension */
    lua_pushboolean(L, agn_regisnumeric(L, idx));
  else if (tools_streq(type, llex_token2str(TK_NULL)))  /* 4.10.3 extension */
    lua_pushboolean(L, agn_regisnil(L, idx));
  else
    luaL_error(L, "Error in " LUA_QS ": unsupported type " LUA_QS ".", procname, type);
  return 1;
}


/* Removes all null values from the array part of a table and moves the remaining non-null values in the array part to
   close the space. If arraypartonly is 0 (zero), then also moves all values in the hash part of the table at stack
   index idx to the end of its array part, emptying the hash part.
   The function works destructively and in-place. The function does not change the stack. 2.30.1 */
LUA_API void agn_reorder (lua_State *L, int idx, int arraypartonly) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  agnH_reorder(L, hvalue(o), arraypartonly);
  lua_unlock(L);
}


/* Clears the entire array and hash parts of a table, leaves possible metatables untouched. The function does not
   change the stack. 4.9.0 */
LUA_API void agn_clear (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  agnH_clear(L, hvalue(o));
  lua_unlock(L);
}


/* Converts a table, sequence or register into a set; 2.30.2; grep "TEMPLATE to call the VM via the API" */
LUA_API void agn_toset (lua_State *L, int idx) {
  lua_lock(L);
  luaC_checkGC(L);  /* 2.34.4: collect garbage _before_ calling "VM", see lua_concat */
  agenaV_toset(L, index2adr(L, idx), L->top);  /* increases the stack top by one */
  api_incr_top(L);  /* increase the stack top by one */
  lua_unlock(L);
}


/* Determines the product of all the numbers in a table, sequence or register; DEFUNCT for calling an
   operator is 38 % faster. However, this demonstrator works flawlessly without Valgrind complaining;

   From a C library, call the function like this:

   static int agnB_mulup (lua_State *L) {
     agn_mulup(L, 1);
     return 1;
   };  2.30.2 */
LUA_API void agn_mulup (lua_State *L, int idx) {
  lua_lock(L);
  luaC_checkGC(L);  /* 2.34.4: collect garbage _before_ calling "VM", see lua_concat */
  agenaV_mulup(L, index2value(L, idx), L->top, lua_gettop(L));
  api_incr_top(L);  /* increase the stack top by one */
  lua_unlock(L);
}


LUA_API void agn_sstate (lua_State *L, int idx, size_t a[]) {
  StkId o;
  UltraSet *t;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttisuset(o));
  t = usvalue(o);
  a[0] = t->size;
  a[1] = actnodesize(t);
  a[2] = t->metatable != NULL;
  a[3] = agnUS_readonly(L, t, -1);
  lua_unlock(L);
}


LUA_API void agn_seqstate (lua_State *L, int idx, size_t a[]) {
  StkId o;
  Seq *t;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttisseq(o));
  t = seqvalue(o);
  a[0] = t->size;
  a[1] = t->maxsize;
  a[2] = t->metatable != NULL;
  a[3] = agnSeq_readonly(L, t, -1);
  lua_unlock(L);
}


/* Returns the current top pointer, the total number of items, and a flag indicating whether a metatable has been assigned to the register at index idx in a, a C array with three entries. The position of the top pointer is stored to a[0], the total number of entries to a[1]. The metatable flag is stored to a[2], where 1 indicates that the sequence features a metatable, and 0 means it does not. */

LUA_API void agn_regstate (lua_State *L, int idx, size_t a[]) {  /* 2.3.0 RC 3 */
  StkId o;
  Reg *t;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttisreg(o));
  t = regvalue(o);
  a[0] = t->top;
  a[1] = t->maxsize;
  a[2] = t->metatable != NULL;
  a[3] = agnReg_readonly(L, t, -1);
  lua_unlock(L);
}


LUA_API void agn_pairstate (lua_State *L, int idx, size_t a[]) {  /* 2.3.0 RC 3 */
  StkId o;
  Pair *t;  /* 4.9.0 fix */
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttisreg(o));
  t = pairvalue(o);  /* 4.9.0 fix */
  a[0] = t->metatable != NULL;
  a[1] = agnPair_readonly(L, t, -1);
  lua_unlock(L);
}


LUA_API void agn_udstate (lua_State *L, int idx, size_t a[]) {  /* 4.9.0 */
  StkId o;
  Udata *u;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttisuserdata(o));
  u = rawuvalue(o);
  a[0] = u->uv.readonly;
  a[1] = u->uv.len;
  a[2] = u->uv.nuvalue;
  lua_unlock(L);
}


/* 0.9.1, January 12, 2008; extended 0.10.0 to handle sets and sequences */
LUA_API size_t agn_nops (lua_State *L, int idx) {
  StkId o;
  o = index2adr(L, idx);
  switch (ttype(o)) {
    case LUA_TTABLE:  /* return the number of assigned entries in the array and hash part of a table; linear traversal */
      return agenaV_nops(L, hvalue(o));
    case LUA_TSET:
      return usvalue(o)->size;
    case LUA_TSEQ:  /* actual number of assigned elements */
      return seqvalue(o)->size;
    case LUA_TREG:  /* current top of the register, that is non-null elements plus null's */
      return regvalue(o)->top;
    case LUA_TPAIR:
      return 2;  /* 2.10.0, for unpack */
    case LUA_TSTRING:  /* 2.12.1 */
      return tsvalue(o)->len;
    default:
      return 0;
  }
}


/* returns number of assigned (not allocated) elements in the array and hash part of a table;
   O(n) as the entire table will be traversed */
LUA_API size_t agn_size (lua_State *L, int idx) {
  StkId o;
  Table *t;
  size_t r;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  t = hvalue(o);
  r = agenaV_nops(L, t);  /* 5.5.7 change */
  lua_unlock(L);
  return r;
}


/* returns number of assigned (not allocated) elements in the array part of a table;
   O(n) as the entire array part will be traversed. See also: lua_objlen/luaL_getn
   for an O(log2) alternative. */
LUA_API size_t agn_asize (lua_State *L, int idx) {
  StkId o;
  Table *t;
  size_t r;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  t = hvalue(o);
  r = agnH_asize(L, t);  /* 5.5.7 change */
  lua_unlock(L);
  return r;  /* tuned 2.11.4 */
}


/* returns number of assigned (not allocated) elements in the hash part of a table, 4.10.2 */
LUA_API size_t agn_hsize (lua_State *L, int idx) {
  StkId o;
  Table *t;
  size_t r;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  t = hvalue(o);
  r = agnH_hsize(L, t);  /* 5.5.7 change */
  lua_unlock(L);
  return r;
}


LUA_API size_t agn_ssize (lua_State *L, int idx) {
  StkId o;
  UltraSet *t;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttisuset(o));
  t = usvalue(o);
  lua_unlock(L);
  return t->size;
}


LUA_API size_t agn_seqsize (lua_State *L, int idx) {
  StkId o;
  Seq *s;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttisseq(o));
  s = seqvalue(o);
  lua_unlock(L);
  return s->size;
}


LUA_API int agn_regsize (lua_State *L, int idx) {  /* 2.3.0 RC 3 */
  StkId o;
  Reg *r;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttisreg(o));
  r = regvalue(o);
  return r->maxsize;
}


LUA_API size_t agn_reggettop (lua_State *L, int idx) {  /* 2.3.0 RC 3 */
  StkId o;
  Reg *r;
  o = index2adr(L, idx);
  api_checkvalidindex(L, o);
  lua_lock(L);
  api_check(L, ttisreg(o));
  r = regvalue(o);
  return r->top;
}


LUA_API int agn_regsettop (lua_State *L, int idx) {  /* 2.3.0 RC 3 */
  StkId o;
  int res;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttisreg(o));
  res = agnReg_settop(L, regvalue(o), L->top - 1);
  lua_unlock(L);
  L->top--;
  return res;
}


/* 0.22.0: returns 1 if the function at idx is a C function, 0 if the function at idx is an Agena function,
   and -1 if it is no function at all. */
LUA_API int agn_getfunctiontype (lua_State *L, int idx) {
  const TValue *o;
  int fn;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) == LUA_TFUNCTION)
    fn = clvalue(o)->l.isC;
  else
    fn = -1;
  lua_unlock(L);
  return fn;
}


LUA_API void agn_creatertable (lua_State *L, int idx, int writemode) {
  TValue *o;
  Table *nullt;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2adr(L, idx);
  if (isLfunction(o)) {
    nullt = luaH_new(L, 0, 0);
    if (nullt) {
      clvalue(o)->l.rtable = nullt;
      luaC_objbarrier(L, clvalue(o), nullt);
    } else
      if (writemode)
        luaG_runerror(L, "Error: rtable initialisation failure.");
      else
        luaG_runerror(L, "Error: rotable initialisation failure.");
    if (writemode) clvalue(o)->l.updatertable = 1;  /* 0.22.0 */
  }
  else if (iscfunction(o) && !writemode) {
    nullt = luaH_new(L, 0, 0);
    if (nullt) {
      clvalue(o)->c.rtable = nullt;
      luaC_objbarrier(L, clvalue(o), nullt);
    }
    else
      luaG_runerror(L, "Error: rotable initialisation failure.");
  }
  else
    luaG_runerror(L, "Error: rtables are available for Agena procedures only.");
  lua_unlock(L);
}


LUA_API int agn_getrtable (lua_State *L, int idx) {
  const TValue *o;
  Table *mt = NULL;
  int res;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) == LUA_TFUNCTION)
    mt = clvalue(o)->l.rtable;
  else
    luaG_runerror(L, "Error: remember tables are available for procedures only.");
  if (mt == NULL) {
    res = 0; }
  else {
    sethvalue(L, L->top, mt);
    api_incr_top(L);
    res = 1;
  }
  lua_unlock(L);
  return res;
}


/* 0.22.0: returns 0 if rtable cannot be updated by the RETURN statement, 1 if it can,
   2 if `idx` has no remember table at all, and -1 if `idx` is not a function. */
LUA_API int agn_getrtablewritemode (lua_State *L, int idx) {
  const TValue *o;
  int mode = 0;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) == LUA_TFUNCTION)
    mode = (clvalue(o)->l.rtable == NULL) ? 2 : clvalue(o)->l.updatertable;
  else
    mode = -1;
  lua_unlock(L);
  return mode;
}


/* agn_setrtable completely rewritten and simplified with the help of Gemini AI, 7.5.8 */
static TValue *aux_getcomponent (lua_State *L, int idx) {
  TValue *o = index2adr(L, idx);
  /* Wenn es eine Tabelle mit nur einem Element ist, extrahiere den Wert */
  if (ttistable(o) && agnH_asize(L, hvalue(o)) == 1) {
    return (TValue *)luaH_getint(hvalue(o), 1);
  }
  return o;
}

static void aux_checktableentry (lua_State *L, int idx, const char *what) {
  if lua_istable(L, idx) {
    int dummy;
    Table *targs = hvalue(index2adr(L, idx));
    if (agnH_hashashpart(L, targs) || agnH_hasholes(L, targs, &dummy)) {
      luaG_runerror(L, "Error: %s table is malformed.", what);
    }
  }
}

LUA_API void agn_setrtable (lua_State *L, int find, int kind, int vind) {
  TValue *o_func, *arg_key, *res_val;
  const TValue *entry;
  Table *rt, *dict;
  int hashval = 0;
  lua_lock(L);
  o_func = index2adr(L, find);
  if (!ttisfunction(o_func))
    luaG_runerror(L, "Error: expected a function as first argument, got %s.", luaL_typename(L, find));
  rt = clvalue(o_func)->l.rtable;
  if (!rt) luaG_runerror(L, "Error: remember table has not been initialised.");
  aux_checktableentry (L, kind, "argument");
  aux_checktableentry (L, vind, "result");
  if (lua_istable(L, kind)) {  /* 7.5.7 fix */
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushnil(L);
    while (lua_next(L, kind) != 0) {
      luaD_rthashupdate(hashval, L->top - 1);
      agn_poptop(L);
    }
  } else {
    luaD_rthashupdate(hashval, index2adr(L, kind));
  }
  entry = luaH_getnum(rt, hashval + 1);
  /* create new argument ~ results table or get currebnt one */
  if (ttisnil(entry)) {
    dict = luaH_new(L, 0, 1);
    sethvalue(L, L->top, dict);
    luaH_setint(L, rt, hashval + 1, L->top);
    luaC_barriert(L, rt, L->top);
  } else {
    dict = hvalue(entry);
  }
  /* set new arguments ~ result pair, overwriting the existing one */
  arg_key = aux_getcomponent(L, kind);
  res_val = aux_getcomponent(L, vind);
  setobjt2t(L, luaH_set(L, dict, arg_key), res_val);
  /* Garbage Collector über die Änderung informieren */
  luaC_barriert(L, dict, res_val);
  lua_unlock(L);
}


LUA_API void agn_deletertable (lua_State *L, int objindex) {
  TValue *o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2adr(L, objindex);
  api_checkvalidindex(L, o);  /* Agena 1.0.3 fix */
  /* do not luaH_free remember table, leave this to gc; Valgrind does not complain */
  if (isLfunction(o)) {
    clvalue(o)->l.rtable = NULL;
    clvalue(o)->l.updatertable = 0; }
  else if (iscfunction(o)) {
    clvalue(o)->c.rtable = NULL;
    clvalue(o)->c.updatertable = 0; }
  else
    luaG_runerror(L, "Error: remember tables are available for procedures only.");
  lua_unlock(L);
}


LUA_API int agn_getstorage (lua_State *L, int idx) {  /* 2.33.1 */
  const TValue *o;
  Table *mt = NULL;
  int res;
  lua_lock(L);
  o = index2adr(L, idx);
  if (ttype(o) == LUA_TFUNCTION)
    mt = isLfunction(o) ? clvalue(o)->l.storage : clvalue(o)->c.storage;
  else
    luaG_runerror(L, "Error: storage tables are available for procedures only.");
  if (mt == NULL) {
    res = 0;
  } else {
    sethvalue(L, L->top, mt);
    api_incr_top(L);
    res = 1;
  }
  lua_unlock(L);
  return res;
}


/* sets the name of a readlibbed package to the registry, leaves nothing on the stack */
LUA_API int agn_setreadlibbed (lua_State *L, const char *libname) {  /* rewritten 2.9.1 */
  int rc = 0;
  lua_lock(L);
  lua_getfield(L, LUA_REGISTRYINDEX, "_READLIBBED");
  if (!lua_isset(L, -1)) {
    fprintf(stderr, "Warning: internal registry _READLIBBED set not found.\n");
  } else if (!luaL_isstandardlib(L, libname)) {  /* 2.31.9 fix especially for OS/2, DOS and ANSI */
    lua_pushstring(L, libname);  /* insert package name into _READLIBBED set */
    lua_srawset(L, -2);
    rc = 1;
  }
  agn_poptop(L);  /* pop _READLIBBED (or anything) from stack */
  lua_unlock(L);
  return rc;
}


LUA_API void agn_setclosetozero (lua_State *L, lua_Number x) {  /* 2.32.0 */
  if (x <= 0)
    luaG_runerror(L, "Error in " LUA_QS ": value must be positive.", "environ.kernel/closetozero");
  L->div_closetozero = x;
}


LUA_API lua_Number agn_getclosetozero (lua_State *L) {  /* 2.32.0 */
  return L->div_closetozero;
}


LUA_API void agn_setforadjust (lua_State *L, int value) {  /* 2.31.0 */
  L->vmsettings ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->vmsettings) & 1;
}


LUA_API int agn_getforadjust (lua_State *L) {  /* 2.31.0 */
  return L->vmsettings & 1;
}


LUA_API void agn_setkahanozawa (lua_State *L, int value) {  /* 2.4.2 */
  L->vmsettings ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->vmsettings) & 2;
  L->vmsettings ^= (-((lu_byte)0) ^ L->vmsettings) & 4;  /* 2.30.5 */
}


LUA_API int agn_getkahanozawa (lua_State *L) {  /* 2.4.2 */
  return L->vmsettings & 2;
}


LUA_API void agn_setkahanbabuska (lua_State *L, int value) {  /* 2.30.5 */
  L->vmsettings ^= (-((lu_byte)0) ^ L->vmsettings) & 2;  /* 2.30.5 */
  L->vmsettings ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->vmsettings) & 4;
}


LUA_API int agn_getkahanbabuska (lua_State *L) {  /* 2.30.5 */
  return L->vmsettings & 4;
}


/* see: http://graphics.stanford.edu/~seander/bithacks.html on how to modify bits, 0.32.0 */
LUA_API void agn_setbitwise (lua_State *L, int value) {
  L->settings ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings) & 1;
}


LUA_API int agn_getbitwise (lua_State *L) {  /* 0.32.0 */
  return L->settings & 1;
}


LUA_API void agn_setemptyline (lua_State *L, int value) {  /* 0.32.0 */
  L->settings ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings) & 2;
}


LUA_API int agn_getemptyline (lua_State *L) {  /* 0.32.0 */
  return L->settings & 2;
}


LUA_API void agn_setlibnamereset (lua_State *L, int value) {  /* 0.32.0 */
  L->settings ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings) & 4;
}


LUA_API int agn_getlibnamereset (lua_State *L) {  /* 0.32.0 */
  return L->settings & 4;
}


LUA_API void agn_setlongtable (lua_State *L, int value) {  /* 0.32.0 */
  L->settings ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings) & 8;
}


LUA_API int agn_getlongtable (lua_State *L) {  /* 0.32.0 */
  return L->settings & 8;
}


LUA_API void agn_setdebug (lua_State *L, int value) {  /* 0.32.2a */
  L->settings ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings) & 16;
}


LUA_API int agn_getdebug (lua_State *L) {  /* 0.32.0 */
  return L->settings & 16;
}


LUA_API void agn_setgui (lua_State *L, int value) {  /* 0.33.3 */
  L->settings ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings) & 32;
}


LUA_API int agn_getgui (lua_State *L) {  /* 0.33.3 */
  return L->settings & 32;
}


LUA_API void agn_setzeroedcomplex (lua_State *L, int value) {  /* 1.7.6 */
  L->settings ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings) & 64;
}


LUA_API int agn_getzeroedcomplex (lua_State *L) {  /* 1.7.6 */
  return L->settings & 64;
}


LUA_API void agn_setpromptnewline (lua_State *L, int value) {  /* 1.7.6 */
  L->settings ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings) & 128;
}


LUA_API int agn_getpromptnewline (lua_State *L) {  /* 1.7.6 */
  return L->settings & 128;
}


LUA_API void agn_setnoini (lua_State *L, int value) {  /* 2.8.6 */
  L->settings2 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings2) & 1;
}


LUA_API int agn_getnoini (lua_State *L) {  /* 2.8.6 */
  return L->settings2 & 1;
}


LUA_API void agn_setnomainlib (lua_State *L, int value) {  /* 2.8.6 */
  L->settings2 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings2) & 2;
}


LUA_API int agn_getnomainlib (lua_State *L) {  /* 2.8.6 */
  return L->settings2 & 2;
}


LUA_API void agn_setiso8601 (lua_State *L, int value) {  /* 2.9.8 */
  L->settings2 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings2) & 4;
}


LUA_API int agn_getiso8601 (lua_State *L) {  /* 2.9.8 */
  return L->settings2 & 4;
}


LUA_API int agn_getconstants (lua_State *L) {  /* 2.20.0 */
  return L->settings2 & 8;
}


LUA_API void agn_setconstants (lua_State *L, int value) {  /* 2.20.0 */
  L->settings2 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings2) & 8;
}


LUA_API int agn_getduplicates (lua_State *L) {  /* 2.20.1 */
  return L->settings2 & 16;
}


LUA_API void agn_setduplicates (lua_State *L, int value) {  /* 2.20.1 */
  L->settings2 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings2) & 16;
}


LUA_API int agn_getseqautoshrink (lua_State *L) {  /* 2.29.0 */
  return L->settings2 & 32;
}


LUA_API void agn_setseqautoshrink (lua_State *L, int value) {  /* 2.29.0 */
  L->settings2 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings2) & 32;
}


LUA_API int agn_getskipagenapath (lua_State *L) {  /* 2.35.4 */
  return L->settings2 & 64;
}


LUA_API void agn_setskipagenapath (lua_State *L, int value) {  /* 2.35.4 */
  L->settings2 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings2) & 64;
}


LUA_API int agn_getconstanttoobig (lua_State *L) {  /* 2.39.4 */
  return L->settings2 & 128;
}


LUA_API void agn_setconstanttoobig (lua_State *L, int value) {  /* 2.39.4 */
  L->settings2 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings2) & 128;
}


LUA_API void agn_setenclose (lua_State *L, int value) {  /* 3.10.2 */
  if (value) {
    agn_setenclosedouble(L, 0);
    agn_setenclosebackquotes(L, 0);
  }
  L->settings3 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings3) & 1;
}


LUA_API int agn_getenclose (lua_State *L) {  /* 3.10.2 */
  return L->settings3 & 1;
}


LUA_API void agn_setenclosedouble (lua_State *L, int value) {  /* 3.10.2 */
  if (value) {
    agn_setenclose(L, 0);
    agn_setenclosebackquotes(L, 0);
  }
  L->settings3 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings3) & 2;
}


LUA_API int agn_getenclosedouble (lua_State *L) {  /* 3.10.2 */
  return L->settings3 & 2;
}


LUA_API void agn_setenclosebackquotes (lua_State *L, int value) {  /* 3.10.2 */
  if (value) {
    agn_setenclose(L, 0);
    agn_setenclosedouble(L, 0);
  }
  L->settings3 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings3) & 4;
}


LUA_API int agn_getenclosebackquotes (lua_State *L) {  /* 3.10.2 */
  return L->settings3 & 4;
}


LUA_API void agn_sethistory (lua_State *L, int value) {  /* 5.2.2 */
  L->settings3 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings3) & 8;
}


LUA_API int agn_gethistory (lua_State *L) {  /* 5.2.2 */
  return L->settings3 & 8;
}


LUA_API void agn_setnulliszero (lua_State *L, int value) {  /* 7.7.6 */
  L->settings3 ^= (-(value > 0 ? (lu_byte)1 : (lu_byte)0) ^ L->settings3) & 16;
}


LUA_API int agn_getnulliszero (lua_State *L) {  /* 7.7.6 */
  return L->settings3 & 16;
}


LUA_API void agn_sethistfile (lua_State *L, char *filename) {  /* 5.2.2 */
  if (
#if defined(_WIN32) || defined(__OS2__) || defined(__DJGPP__)
    !tools_iswinpath(filename)
#else
    !tools_isunixpath(filename)
#endif
  ) {
    agn_sethistory(L, 0);  /* 5.2.3 extension */
    luaG_runerror(L, "Error in " LUA_QS ": filename is invalid.", "environ.kernel/histfile");
  } else {
    xfree(L->histfile);
    char *temp = tools_strdup(filename);
    luaG_runerror(L, "Error in " LUA_QS ": memory allocation failed.", "environ.kernel/histfile");
    L->histfile = temp;
  }
}


LUA_API char *agn_gethistfile (lua_State *L) {  /* 5.2.2 */
  return L->histfile;
}


LUA_API void agn_setbuffersize (lua_State *L, int value) {  /* 2.2.0 */
  if (value > 1048576 || value < 256)  /* 7.9.5 fix */
    /* In Windows the maximum size of a Variable-Length Array (VLA) is 1 MegaBytes, Linux 2 to 8 MegaBytes and
       Solaris 10 from 1 to 2 MegaBytes. Larger VLAs will segfault the interpreter. */
    luaG_runerror(L, "Error in " LUA_QS ": value must be in the range [256, 1024^2].", "environ.kernel/buffersize");
  else
    L->buffersize = value;
}


LUA_API int agn_getbuffersize (lua_State *L) {  /* 2.2.0 */
  return L->buffersize;
}


LUA_API void agn_setdigits (lua_State *L, ptrdiff_t x) {  /* x must be a number in [1, 17]; rewritten 2.8.6 */
  char *num, *oldpos;
  if (x < 1 || x > 17) luaG_runerror(L, LUA_QS "Error: setting must be an integer in [1, 17].", "environ.kernel/digits");
  num = L->numberformat;
  oldpos = L->numberformat;
  while (!isdigit(*num)) { num++; }
  if (x < 10) { *num = '0'; num++; *num = '0' + x; }
  else { *num = '1'; num++; *num = '0' + x - 10; }
  L->numberformat = oldpos;
}


LUA_API void agn_setregsize (lua_State *L, size_t x) {  /* 2.3.0 RC 3, set default register size */
  if (x < 0 || x > MAX_INT)
    luaG_runerror(L, LUA_QS "Error: setting must be an integer in [1, &ld].", "environ.kernel/regsize", MAX_INT);
  L->regsize = x;
}


LUA_API int agn_getdigits (lua_State *L) {  /* rewritten 2.8.6 */
  int n;
  lua_getglobal(L, "Digits");  /* extended 7.0.0 */
  if (agn_isnonnegint(L, -1)) {
    agn_setdigits(L, agn_tointeger(L, -1));
  }
  agn_poptop(L);
  sscanf(L->numberformat, "%*[^0123456789]%d", &n);
  return n;
}


LUA_API int agn_geterrmlinebreak (lua_State *L) {  /* 2.11.4 */
  return L->errmlinebreak;
}


LUA_API void agn_seterrmlinebreak (lua_State *L, int x) {  /* 2.11.4, set number of chars per line in syntax error messages. */
  if (x < 1 || tools_isfrac(x))
    luaG_runerror(L, LUA_QS "Error: setting must be a positive integer.", "environ.kernel/errmlinebreak");
  L->errmlinebreak = x;
}



LUA_API void agn_setepsilon (lua_State *L, lua_Number eps) {  /* 2.1.4 */
  if (eps < 0) luaG_runerror(L, "Error in " LUA_QS ": value must not be negative.", "environ.kernel/eps");
  L->Eps = eps;
  lua_pushnumber(L, eps);  /* 2.21.8 fix: change "Eps" in environment, too */
  lua_setglobal(L, "Eps");
}


LUA_API void agn_setdblepsilon (lua_State *L, lua_Number dbleps) {  /* 2.21.8 */
  if (dbleps < 0) luaG_runerror(L, "Error in " LUA_QS ": value must be non-negative.", "environ.kernel/doubleeps");
  L->DoubleEps = dbleps;
  lua_pushnumber(L, dbleps);  /* change "DoubleEps" in environment, too */
  lua_setglobal(L, "DoubleEps");
}


LUA_API void agn_sethepsilon (lua_State *L, lua_Number heps) {  /* 2.31.1 */
  if (heps < 0) luaG_runerror(L, "Error in " LUA_QS ": value must be non-negative.", "environ.kernel/heps");
  L->hEps = heps;
}


LUA_API size_t agn_getregsize (lua_State *L) {  /* default register size, 2.3.0 RC 3 */
  return L->regsize;
}


LUA_API lua_Number agn_getstructuresize (lua_State *L, int idx) {
  const TValue *o;
  o = index2adr(L, idx);
  api_checkvalidindex(L, o);  /* Agena 1.0.3 fix */
  switch (ttype(o)) {
    case LUA_TTABLE: {
      Table *h = hvalue(o);
      return (lua_Number)(sizeof(Table) + sizeof(TValue) * h->sizearray + sizeof(Node) * actnodesize(h));
    }
    case LUA_TSET: {
      UltraSet *s = usvalue(o);
      return (lua_Number)(sizeof(UltraSet) + sizeof(LNode)*actnodesize(s));
    }
    case LUA_TSEQ: {
      Seq *s = seqvalue(o);
      return (lua_Number)(sizeof(Seq) + sizeof(TValue)*s->maxsize);
    }
    case LUA_TPAIR: {
      return (lua_Number)(sizeof(Pair) + sizeof(TValue)*2);
    }
    case LUA_TFUNCTION: {
      Closure *cl = clvalue(o);
      return (lua_Number)((cl->c.isC) ? sizeCclosure(cl->c.nupvalues) :
                           sizeLclosure(cl->l.nupvalues));
    }
    case LUA_TREG: {
      Reg *s = regvalue(o);
      return (lua_Number)(sizeof(Reg) + sizeof(TValue)*s->maxsize);
    }
  }
  return 0;
}


/* Allocates size bytes of memory and returns a pointer to the newly allocated block. In case memory could not be
   allocated, it returns an error message including procname that called agn_malloc. The function optionally can
   free one or more other objects referenced by their pointers in case memory allocation failed.

   In all cases, the last argument must be NULL. */
LUA_API void *agn_malloc (lua_State *L, size_t size, const char *procname, ...) {  /* 1.9.1 */
  void *ptr;
  ptr = tools_malloc(size, 0);  /* 2.27.0 */
  if (ptr == NULL) {
    void *s;
    va_list args;
    va_start(args, procname);
    while ((s = va_arg(args, void *))) {
      xfree(s);
    }
    va_end(args);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
  }
  return ptr;
}


/* Allocates a string buffer by internally determining its most efficient size, aligned along the "long" boundary. The
   return is a char* pointer to the beginning of the string. The function zeros only the last few bytes and assumes that
   the `leading` rest will be filled by real characters later on. Just pass l as the number of characters, excluding the
   terminating \0, and do not multiply it by sizeof(char). The function automatically adds a terminating \0.

   The function can optionally free variables passed after `procname` in case memory allocation fails internally. In any case,
   the last argument must always be NULL.

   The function is around six percent faster than agn_calloc. See also tools_stralloc in agnhlps.c. */
LUA_API char *agn_stralloc (lua_State *L, size_t len, const char *procname, ...) {  /* 2.16.5 */
  char *buf;
  size_t l, rem;
  /* determine optimal size, including terminating \0 */
  l = len;
  rem = (++l) % AGN_BLOCKSIZE;  /* length including \0 */
  l += (rem != 0)*(AGN_BLOCKSIZE - rem);
  buf = (char *)malloc(l*CHARSIZE);
  if (buf == NULL) {
    void *s;
    va_list args;
    va_start(args, procname);             /* initialize the argument list */
    while ((s = va_arg(args, void *))) {  /* get the next argument value */
      xfree(s);
    }
    va_end(args);                         /* clean up */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
  }
  memset(buf + len, 0, l - len);  /* only zero the bytes that will not be filled by characters later on */
  return buf;
}


LUA_API void agn_longarraytoseq (lua_State *L, long double *a, size_t n) {  /* converts an array to a sequence and pushes it on the top of the stack */
  size_t i;
  agn_createseq(L, n);
  for (i=0; i < n; i++)
    agn_seqsetinumber(L, -1, i + 1, a[i]);
}


/* Expects a valid userdata metatable at the top of the stack, assigns it to the userdata residing at stack
   index idx, and pops the value at the top of the stack thereafter. If the value at the top of the
   stack is null, then a metatable assigned to a userdatum is deleted, and null is popped from the stack. */
LUA_API void agn_setudmetatable (lua_State *L, int idx) {  /* 2.3.0 RC 3 */
  TValue *obj;
  Table *mt;
  lua_lock(L);
  obj = index2adr(L, idx);
  api_checkvalidindex(L, obj);
  if (ttisnil(L->top - 1))
    mt = NULL;
  else {
    api_check(L, ttistable(L->top - 1));
    mt = hvalue(L->top - 1);
  }
  uvalue(obj)->metatable = mt;
  if (mt)
    luaC_objbarrier(L, rawuvalue(obj), mt);
  L->top--;
  lua_unlock(L);
}


LUA_API lua_Integer lua_tointegerx (lua_State *L, int idx, int *isnum) {  /* taken from Lua 5.2.4, tuned 2.21.2 */
  lua_Number n;
  const TValue *o = index2adr(L, idx);
  if (tonumberx(o, &n)) {
    lua_Integer res;
    lua_number2integer(res, n);
    if (isnum) *isnum = 1;
    return res;
  }
  else {
    if (isnum) *isnum = 0;
    return 0;
  }
}


LUA_API lua_Unsigned lua_tounsignedx (lua_State *L, int idx, int *isnum) {  /* taken from Lua 5.2.4, tuned 2.21.2 */
  lua_Number n;
  const TValue *o = index2adr(L, idx);
  if (tonumberx(o, &n)) {
    lua_Unsigned res;
    lua_number2unsigned(res, n);
    if (isnum) *isnum = 1;
    return res;
  }
  else {
    if (isnum) *isnum = 0;
    return 0;
  }
}


/* Pushes an unsigned int onto the stack. */
LUA_API void lua_pushunsigned (lua_State *L, lua_Unsigned u) {  /* taken from Lua 5.2.4 */
  lua_Number n;
  lua_lock(L);
  n = lua_unsigned2number(u);
  setnvalue(L->top, n);
  api_incr_top(L);
  lua_unlock(L);
}


/* Gets the current rounding mode. Returns "downward" for FE_DOWNWARD, "upward" for FE_UPWARD, "nearest" for FE_TONEAREST,
   and "zero" for FE_TOWARDZERO. Not available in DOS. */
#ifndef __DJGPP__
LUA_API void agn_getround (lua_State *L) {  /* 2.8.6 */
  int r = fegetround();
  if (r < 0) {
    setnvalue(L->top, AGN_NAN);
  } else {
    switch (r) {
      case FE_DOWNWARD: {
        setsvalue(L, L->top, luaS_new(L, "downward"));
        break; /* rounding direction is set to the next lower integer. */
      }
      case FE_UPWARD: {
        setsvalue(L, L->top, luaS_new(L, "upward"));
        break;  /* rounding direction is set to the next greater integer. */
      }
      case FE_TONEAREST: {
        setsvalue(L, L->top, luaS_new(L, "nearest"));
        break;  /* round up or down toward whichever integer is nearest */
      }
      case FE_TOWARDZERO: {
        setsvalue(L, L->top, luaS_new(L, "towardzero"));
        break;  /* round positive values downward and negative values upward */
      }
      default: {
        setbvalue(L->top, 2);  /* push fail */
      }
    }
  }
  api_incr_top(L);
  return;
}


/* Sets the rounding mode. `what` may be "downward" for DE_DOWNWARD, "upward" for FE_UPWARD, "nearest" for FE_TONEAREST,
   and "zero" for FE_TOWARDZERO. Not available in DOS. */
LUA_API int agn_setround (lua_State *L, const char *what) {  /* 2.8.6 */
  int old, r;
  old = fegetround();
  if (tools_streq(what, "downward"))  /* 2.16.12 tweak */
    r = fesetround(FE_DOWNWARD);
  else if (tools_streq(what, "upward"))  /* 2.16.12 tweak */
    r = fesetround(FE_UPWARD);
  else if (tools_streq(what, "nearest"))  /* 2.16.12 tweak */
    r = fesetround(FE_TONEAREST);
  else if (tools_streq(what, "towardzero"))  /* 2.16.12 tweak */
    r = fesetround(FE_TOWARDZERO);
  else r = -1;
  if (r != 0)
    fesetround(old);
  return (r == 0);
}
#endif


LUA_API int agn_int2fb (lua_State *L, unsigned int x) {  /* 2.13.0 */
  return luaO_int2fb(x);
}


LUA_API int agn_fb2int (lua_State *L, int x) {  /* 2.13.0 */
  return luaO_fb2int(x);
}


LUA_API int agn_tofileno (lua_State *L, int n, int all) {  /* 2.17.8, for io and binio files */
  int hnd;
  FILE **p;
  if (luaL_isudata(L, 1, AGN_FILEHANDLE))
    p = topnfile(L, n);
  else if (all && luaL_isudata(L, 1, LUA_FILEHANDLE))  /* 2.37.5 extension */
    p = iotopnfile(L, n);
  else
    return -1;
  hnd = (*p) ? fileno(*p) : -1;  /* 2.37.3 */
  /* do not NULL p */
  return hnd;
}


/* start, end, init starting from 1, not 0. See also: agnL_strmatch. */
LUA_API const char *agn_strmatch (lua_State *L, const char *s, size_t s_len,
    const char *p, ptrdiff_t init, ptrdiff_t *start, ptrdiff_t *end) {  /* 2.36.1 */
  int anchor;
  const char *s1, *res;
  MatchState ms;
  init = tools_posrelat(init, s_len) - 1;
  if (init < 0) init = 0;
  *start = *end = 0;
  if ((size_t)init >= s_len) return NULL;
  /* checking with strpbrk for special characters gives no gain in speed */
  anchor = (*p == '^') ? (p++, 1) : 0;
  s1 = s + init;
  ms.L = L;
  ms.src_init = s;
  ms.src_end = s + s_len;
  do {
    ms.level = 0;
    if ((res=match(&ms, s1, p, 0)) != NULL) {
      *start = s1 - s + 1; *end = res - s;
      return s1;
    }
  } while (s1++ < ms.src_end && !anchor);
  return NULL;
}


#define aux_getminmax(a,v,i,x,mi,mipo,ma,mapo,c) { \
  if (ttisnumber(v)) { \
    (c)++; \
    x = nvalue(v); \
    if (a) { \
      if (fabs(x) > fabs(ma)) { (ma) = x; (mapo) = i + 1; } \
      if (fabs(x) < fabs(mi)) { (mi) = x; (mipo) = i + 1; } \
    } else { \
      if (x > (ma)) { (ma) = x; (mapo) = i + 1; } \
      if (x < (mi)) { (mi) = x; (mipo) = i + 1; } \
    } \
  } \
}

LUA_API int agn_minmax (lua_State *L, int idx, int absolute, lua_Number *min, size_t *minpos, lua_Number *max, size_t *maxpos, int *count) {
  int i, rc;
  TValue *val;
  lua_Number x;
  lua_lock(L);
  StkId o = index2adr(L, idx);
  if (absolute) {
    *min = +HUGE_VAL;  /* ??? FIXME */
    *max = 0.0;
  } else {
    *min = +HUGE_VAL;
    *max = -HUGE_VAL;
  }
  *minpos = 0;
  *maxpos = 0;
  *count = 0;
  rc = 1;
  switch (ttype(o)) {
    case LUA_TTABLE: {
      Table *h = hvalue(o);
      for (i=0; i < h->sizearray; ++i) {
        val = &h->array[i];
        aux_getminmax(absolute, val, i, x, *min, *minpos, *max, *maxpos, *count);
      }
      if (h->node != dummynode) {
        for (i -= h->sizearray; i < sizenode(h); i++) {  /* then hash part */
          val = gval(gnode(h, i));
          aux_getminmax(absolute, val, i, x, *min, *minpos, *max, *maxpos, *count);
        }
      }
      break;
    }
    case LUA_TSET: {
      UltraSet *s = usvalue(o);
      for (i=0; i < sizenode(s); i++) {
        val = key2tval(gnode(s, i));
        aux_getminmax(absolute, val, i, x, *min, *minpos, *max, *maxpos, *count);
        (*count)++;
      }
      break;
    }
    case LUA_TSEQ: {
      Seq *h = seqvalue(o);
      for (i=0; i < h->size; i++) {
        val = seqitem(h, i);
        aux_getminmax(absolute, val, i, x, *min, *minpos, *max, *maxpos, *count);
      }
      *count = h->size;
      break;
    }
    case LUA_TREG: {
      Reg *h = regvalue(o);
      for (i=0; i < h->top; i++) {
        val = regitem(h, i);
        aux_getminmax(absolute, val, i, x, *min, *minpos, *max, *maxpos, *count);
      }
      *count = h->top;
      break;
    }
    default:
      rc = 0;
  }
  lua_unlock(L);
  return rc && *count > 1;  /* 1 = got a result, 0 = wrong type or too few samples, no result */
}


#define aux_lse(val,x,max,s,cs,ccs) { \
  if (ttisnumber(val)) { \
    x = nvalue(val); \
    s = tools_kbadd(s, sun_exp(x - max), cs, ccs); \
  } \
}

/* See stats_lse() in stats.c. 4.9.2  */
LUA_API int agn_lse (lua_State *L, int idx, lua_Number *lse, lua_Number *maximum, const char *procname) {
  int i, rc, c;
  size_t minpos, maxpos;
  TValue *val;
  lua_Number x, min, max, s, cs, ccs;
  lua_lock(L);
  StkId o = index2adr(L, idx);
  if (!agn_minmax(L, idx, 0, &min, &minpos, &max, &maxpos, &c)) {  /* too few samples or wrong type */
    return 0;
  }
  rc = 1;
  s = cs = ccs = 0.0;
  switch (ttype(o)) {
    case LUA_TTABLE: {
      Table *h = hvalue(o);
      for (i=0; i < h->sizearray; ++i) {
        val = &h->array[i];
        aux_lse(val, x, max, s, &cs, &ccs);
      }
      if (h->node != dummynode) {
        for (i -= h->sizearray; i < sizenode(h); i++) {
          val = gval(gnode(h, i));
          aux_lse(val, x, max, s, &cs, &ccs);
        }
      }
      break;
    }
    case LUA_TSEQ: {
      Seq *h = seqvalue(o);
      for (i=0; i < h->size; i++) {
        val = seqitem(h, i);
        aux_lse(val, x, max, s, &cs, &ccs);
      }
      break;
    }
    case LUA_TREG: {
      Reg *h = regvalue(o);
      for (i=0; i < h->top; i++) {
        val = regitem(h, i);
        aux_lse(val, x, max, s, &cs, &ccs);
      }
      break;
    }
    default: {
      rc = 0; max = s = cs = ccs = AGN_NAN;
    }
  }
  *lse = max + sun_log(s + cs + ccs);
  *maximum = max;
  lua_unlock(L);
  return rc;
}


LUA_API void agn_checkvalidpath (lua_State *L, const char *filename, int pop, const char *procname) {
#if defined(_WIN32) || defined(__DJGPP__) || defined(__OS2__)
  if (!tools_iswinpath(filename)) {
    if (pop < 1) { lua_pop(L, pop); }
    luaL_error(L, "Error in " LUA_QS ": %s is an invalid path.", procname, filename);
  }
#else
  if (!tools_isunixpath(filename)) {
    if (pop < 1) { lua_pop(L, pop); }
    luaL_error(L, "Error in " LUA_QS ": %s is an invalid path.", procname, filename);
  }
#endif
}


/* for `@` operator; based on 3.17.4 agenaV_map, 7.6.11 revision, 7.7.6 fix proposed by Gemini AI to
   prevent invalid reads, succesfully tested with Valgrind. There is no increase in speed integrating
   this code into lvm.c. */
LUA_API void agn_map (lua_State *L, int idx1, int idx2) {
  StkId idxb = index2adr(L, idx1);
  StkId idxc = index2adr(L, idx2);
  if (!ttisfunction(idxb))
    luaL_error(L, "Error in " LUA_QS ": left operand must be a function, got %s.", "@", luaL_typename(L, idx1));
  /* Safe stack calculations based on offsets relative to L->base */
  ptrdiff_t func_off = idxb - L->base;
  ptrdiff_t src_off = idxc - L->base;
  if (ttistable(idxc)) {
    Table *t, *newtable;
    TValue *val, *oldval;
    size_t i;
    lua_lock(L);
    t = hvalue(L->base + src_off);
    api_check(L, L->top < L->ci->top);
    newtable = luaH_new(L, t->sizearray, sizenode(t));
    sethvalue(L, L->top++, newtable);
    /* we deliberately reserve explitic slots in the loop, to prevent invalid reads */
    for (i=0; i < t->sizearray; i++) {
      if (ttisnotnil(&t->array[i])) {
        luaD_checkstack(L, 2);
        /* Refresh base layout references in case the stack reallocated */
        StkId current_idxb = L->base + func_off;
        t = hvalue(L->base + src_off);
        setobj2s(L, L->top, current_idxb);  /* push function */
        setobj2s(L, L->top + 1, &t->array[i]);  /* 1st argument */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        setobj2t(L, luaH_setnum(L, newtable, cast_num(i + 1)), L->top - 1);
        luaC_barriert(L, newtable, L->top - 1);
        L->top--;
      }  /* of if */
    }  /* of for i */
    /* values in dictionaries are joined in random order, so this may be quite useless: */
    for (i -= t->sizearray; i < sizenode(t); i++) {
      if (ttisnotnil(gval(gnode(t, i)))) {
        luaD_checkstack(L, 3);
        /* Refresh base layout references */
        StkId current_idxb = L->base + func_off;
        t = hvalue(L->base + src_off);
        val = gval(gnode(t, i));
        setobj2s(L, L->top, current_idxb);  /* push function */
        setobj2s(L, L->top + 1, val);  /* 1st argument */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        setobj2s(L, L->top, key2tval(gnode(t, i)));  /* push key */
        oldval = luaH_set(L, newtable, L->top);  /* do a primitive set */
        setobj2t(L, oldval, L->top - 1);
        luaC_barriert(L, newtable, L->top - 1);
        L->top--;
      }
    }
    /* prepare table for return to Agena environment: move new structure to final position */
    /* add metatable and user-defined type of original table */
    t = hvalue(L->base + src_off);
    agenaV_setmetatable(L, L->top - 1, t->metatable, 0);
    agenaV_setutype(L, L->top - 1, t->type);
    lua_unlock(L);
  } else if (ttisseq(idxc)) {
    Seq *t, *newseq;
    size_t i;
    lua_lock(L);
    t = seqvalue(L->base + src_off);
    /* stack must be increased to avoid `invalid key to next` errors */
    api_check(L, L->top < L->ci->top);
    newseq = agnSeq_new(L, t->size);
    /* Anchor immediately to prevent GC collect */
    setseqvalue(L, L->top, newseq);
    L->top++;
    for (i=0; i < t->size; i++) {
      luaD_checkstack(L, 2);
      StkId current_idxb = L->base + func_off;
      t = seqvalue(L->base + src_off);
      setobj2s(L, L->top, current_idxb);  /* push function */
      setobj2s(L, L->top + 1, seqitem(t, i));  /* 1st argument */
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      agnSeq_seti(L, newseq, i + 1, L->top - 1);
      luaC_barrierseq(L, newseq, L->top - 1);
      L->top--;
    }
    /* prepare sequence for return to Agena environment: move new structure to final position */
    t = seqvalue(L->base + src_off);
    /* add metatable and user-defined type of original table */
    agenaV_setmetatable(L, L->top - 1, t->metatable, 0);
    agenaV_setutype(L, L->top - 1, t->type);
    lua_unlock(L);
  } else if (ttisuset(idxc)) {
    UltraSet *t, *newset;
    size_t i;
    lua_lock(L);
    t = usvalue(L->base + src_off);
    /* stack must be increased to avoid `invalid key to next` errors */
    api_check(L, L->top < L->ci->top);
    newset = agnUS_new(L, sizenode(t));
    /* Anchor immediately to prevent GC collect */
    setusvalue(L, L->top, newset);
    L->top++;
    for (i=0; i < sizenode(t); i++) {
      if (ttisnotnil(glkey(gnode(t, i)))) {
        luaD_checkstack(L, 2);
        StkId current_idxb = L->base + func_off;
        t = usvalue(L->base + src_off);
        setobj2s(L, L->top, current_idxb);  /* push function */
        setobj2s(L, L->top + 1, key2tval(gnode(t, i)));  /* 1st argument */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        agnUS_set(L, newset, L->top - 1);
        luaC_barrierset(L, newset, L->top - 1);
        L->top--;
      }
    }
    /* prepare set for return to Agena environment: move new structure to final position */
    t = usvalue(L->base + src_off);
    /* add metatable and user-defined type of original table */
    agenaV_setmetatable(L, L->top - 1, t->metatable, 0);
    agenaV_setutype(L, L->top - 1, t->type);
    lua_unlock(L);
  } else if (ttispair(idxc)) {
    Pair *t, *newpair;
    int i;
    lua_lock(L);
    t = pairvalue(L->base + src_off);
    /* stack must be increased to avoid `invalid key to next` errors */
    api_check(L, L->top < L->ci->top);
    newpair = agnPair_new(L);
    /* Anchor immediately to prevent GC collect */
    setpairvalue(L, L->top, newpair);
    L->top++;
    for (i=0; i < 2; i++) {
      luaD_checkstack(L, 2);
      StkId current_idxb = L->base + func_off;
      t = pairvalue(L->base + src_off);
      setobj2s(L, L->top, current_idxb);  /* push function */
      setobj2s(L, L->top + 1, agnPair_geti(t, i + 1));  /* 1st argument */
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      agnPair_seti(L, newpair, i + 1, L->top - 1);
      luaC_barrierpair(L, newpair, L->top - 1);
      L->top--;
    }
    /* prepare pair for return to Agena environment: move new structure to final position */
    t = pairvalue(L->base + src_off);
    /* add metatable and user-defined type of original table */
    agenaV_setmetatable(L, L->top - 1, t->metatable, 0);
    agenaV_setutype(L, L->top - 1, t->type);
    lua_unlock(L);
  } else if (ttisreg(idxc)) {
    Reg *t, *newreg;
    size_t i;
    lua_lock(L);
    t = regvalue(L->base + src_off);
    /* stack must be increased to avoid `invalid key to next` errors */
    api_check(L, L->top < L->ci->top);
    newreg = agnReg_new(L, t->top);
    /* Anchor immediately to prevent GC collect */
    setregvalue(L, L->top, newreg);
    L->top++;
    for (i=0; i < t->top; i++) {
      luaD_checkstack(L, 2);
      StkId current_idxb = L->base + func_off;
      t = regvalue(L->base + src_off);
      setobj2s(L, L->top, current_idxb);  /* push function */
      setobj2s(L, L->top + 1, regitem(t, i));  /* 1st argument */
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      agnReg_seti(L, newreg, i + 1, L->top - 1);
      luaC_barrierreg(L, newreg, L->top - 1);  /* ditto */
      L->top--;  /* ditto */
    }
    /* prepare register for return to Agena environment: move new structure to final position */
    t = regvalue(L->base + src_off);
    /* add metatable and user-defined type of original table */
    agenaV_setmetatable(L, L->top - 1, t->metatable, 0);
    agenaV_setutype(L, L->top - 1, t->type);
    lua_unlock(L);
  } else if (ttisfunction(idxc)) {  /* by suggestion of Slobodan */
    StkId base = L->top;
    lua_lock(L);
    luaD_checkstack(L, 3);
    setobj2s(L, L->top, luaH_getstr(hvalue(gt(L)), luaS_new(L, "map")));
    setobj2s(L, L->top + 1, L->base + func_off);  /* push function */
    setobj2s(L, L->top + 2, L->base + src_off);  /* push function */
    L->top += 3;
    luaD_call(L, L->top - 3, 1, 0);
    L->top--;
    setobj2s(L, base, L->top);
    lua_unlock(L);
    L->top++;
  } else if (ttisstring(idxc)) {
    size_t i, len;
    Seq *newseq;
    lua_lock(L);
    len = tsvalue(L->base + src_off)->len;
    api_check(L, L->top < L->ci->top);
    newseq = agnSeq_new(L, len);
    /* Anchor immediately to prevent GC collect */
    setseqvalue(L, L->top, newseq);
    L->top++;
    for (i=0; i < len; i++) {
      luaD_checkstack(L, 2);
      const char *str = svalue(L->base + src_off);
      StkId current_idxb = L->base + func_off;
      setobj2s(L, L->top, current_idxb);  /* push function */
      setsvalue(L, L->top + 1, luaS_newlstr(L, str + i, 1));
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      agnSeq_seti(L, newseq, i + 1, L->top - 1);
      luaC_barrierseq(L, newseq, L->top - 1);
      L->top--;
    }
    /* prepare sequence for return to Agena environment: move new structure to final position */
    lua_unlock(L);
  } else if (ttisnil(idxc)) {
    setnilvalue(L->top++);
  } else {
    luaG_runerror(L, "Error in " LUA_QS ": structure, string, null or function expected, got %s.", "@",
      luaL_typename(L, idx2));
  }
}


/* for `$` operator, 2.2.5, 17.07.2014, based on agenaV_map; rewritten 2.29.3; 7.6.11 revision;
   7.7.6 fix proposed by Gemini AI to prevent invalid reads, succesfully tested with Valgrind.
   There is no increase in speed integrating this code into lvm.c. */
LUA_API void agn_select (lua_State *L, int idx1, int idx2) {
  StkId idxb = index2adr(L, idx1);
  StkId idxc = index2adr(L, idx2);
  if (!ttisfunction(idxb))
    luaL_error(L, "Error in " LUA_QS ": left operand must be a function, got %s.", "$", luaL_typename(L, idx1));
  ptrdiff_t func_off = idxb - L->base;
  ptrdiff_t src_off = idxc - L->base;
  if (ttistable(L->base + src_off)) {
    Table *t, *newtable;
    TValue *val, *oldval;
    size_t i, c;
    lua_lock(L);
    t = hvalue(L->base + src_off);
    api_check(L, L->top < L->ci->top);
    newtable = luaH_new(L, t->sizearray, sizenode(t));
    sethvalue(L, L->top++, newtable);
    c = 0;
    /* we deliberately reserve explitic slots in the loop, to prevent invalid reads */
    for (i=0; i < hvalue(L->base + src_off)->sizearray; i++) {
      t = hvalue(L->base + src_off);
      if (ttisnotnil(&t->array[i])) {
        lua_lock(L);
        val = &t->array[i];
        setobj2s(L, L->top, L->base + func_off);  /* push function */
        setobj2s(L, L->top + 1, val); /* 1st argument */
        luaD_checkstack(L, 2);
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);  /* L->top is now decreased by 1 */
        if (!ttisboolean(L->top - 1))
          luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$");
        else if (l_istrue(L->top - 1)) {
          t = hvalue(L->base + src_off);
          val = &t->array[i];
          setobj2t(L, luaH_setnum(L, hvalue(L->top - 2), cast_num(c++ + 1)), val);
          luaC_barriert(L, hvalue(L->top - 2), val);
        }
        L->top--;
        lua_unlock(L);
      }  /* of if */
    }  /* of for i */
    t = hvalue(L->base + src_off);
    for (i -= t->sizearray; i < sizenode(t); i++) {
      t = hvalue(L->base + src_off);
      if (ttisnotnil(gval(gnode(t, i)))) {
        lua_lock(L);
        val = gval(gnode(t, i));
        setobj2s(L, L->top, L->base + func_off);   /* push function */
        setobj2s(L, L->top + 1, val);  /* 1st argument */
        luaD_checkstack(L, 2);
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        if (!ttisboolean(L->top - 1))
          luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$");
        else if (l_istrue(L->top - 1)) {
          t = hvalue(L->base + src_off);
          setobj2s(L, L->top, key2tval(gnode(t, i)));  /* push key */
          val = gval(gnode(t, i));
          oldval = luaH_set(L, hvalue(L->top - 2), val);  /* do a primitive set */
          setobj2t(L, oldval, val);  /* set it to table value */
          luaC_barriert(L, hvalue(L->top - 2), val);
        }
        L->top--;
        lua_unlock(L);
      }
    }
    /* prepare sequence for return to Agena environment: move new structure to final position */
    t = hvalue(L->base + src_off);
    newtable = hvalue(L->top - 1);
    sethvalue(L, L->top - 1, newtable);
    /* add metatable and user-defined type of original table */
    agenaV_setmetatable(L, L->top - 1, t->metatable, 0);
    agenaV_setutype(L, L->top - 1, t->type);
    lua_unlock(L);
  } else if (ttisseq(L->base + src_off)) {
    Seq *t, *newseq;
    size_t i, c;
    TValue *arg;
    lua_lock(L);
    t = seqvalue(L->base + src_off);
    api_check(L, L->top < L->ci->top);
    newseq = agnSeq_new(L, t->size);
    setseqvalue(L, L->top++, newseq);
    c = 0;
    for (i=0; i < seqvalue(L->base + src_off)->size; i++) {
      lua_lock(L);
      t = seqvalue(L->base + src_off);
      arg = seqitem(t, i);
      setobj2s(L, L->top, L->base + func_off);  /* push function */
      setobj2s(L, L->top + 1, arg);  /* 1st argument */
      luaD_checkstack(L, 2);
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      if (!ttisboolean(L->top - 1))
        luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$");
      else if (l_istrue(L->top - 1)) {
        t = seqvalue(L->base + src_off);
        arg = seqitem(t, i);
        agnSeq_seti(L, seqvalue(L->top - 2), ++c, arg);
        luaC_barrierseq(L, seqvalue(L->top - 2), arg);
      }
      L->top--;
      lua_unlock(L);
    }
    /* prepare sequence for return to Agena environment: move new structure to final position */
    t = seqvalue(L->base + src_off);
    newseq = seqvalue(L->top - 1);
    setseqvalue(L, L->top - 1, newseq);
    /* add metatable and user-defined type of original table */
    agenaV_setmetatable(L, L->top - 1, t->metatable, 0);
    agenaV_setutype(L, L->top - 1, t->type);
    lua_unlock(L);
  } else if (ttisuset(L->base + src_off)) {
    UltraSet *t, *newset;
    size_t i;
    TValue *arg;
    lua_lock(L);
    t = usvalue(L->base + src_off);
    api_check(L, L->top < L->ci->top);
    newset = agnUS_new(L, sizenode(t));
    setusvalue(L, L->top++, newset);
    for (i=0; i < sizenode(usvalue(L->base + src_off)); i++) {
      t = usvalue(L->base + src_off);
      if (ttisnotnil(glkey(gnode(t, i)))) {
        lua_lock(L);
        arg = key2tval(gnode(t, i));
        setobj2s(L, L->top, L->base + func_off);  /* push function */
        setobj2s(L, L->top + 1, arg);  /* 1st argument */
        luaD_checkstack(L, 2);
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        if (!ttisboolean(L->top - 1))
          luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$");
        else if (l_istrue(L->top - 1)) {
          t = usvalue(L->base + src_off);
          arg = key2tval(gnode(t, i));
          agnUS_set(L, usvalue(L->top - 2), arg);
          luaC_barrierset(L, usvalue(L->top - 2), arg);
        }
        L->top--;
        lua_unlock(L);
      }
    }
    /* prepare set for return to Agena environment: move new structure to final position */
    t = usvalue(L->base + src_off);
    newset = usvalue(L->top - 1);
    setusvalue(L, L->top - 1, newset);
    /* add metatable and user-defined type of original table */
    agenaV_setmetatable(L, L->top - 1, t->metatable, 0);
    agenaV_setutype(L, L->top - 1, t->type);
    lua_unlock(L);
  } else if (ttisreg(L->base + src_off)) {
    Reg *t, *newreg;
    size_t i, c;
    TValue *arg;
    lua_lock(L);
    t = regvalue(L->base + src_off);
    api_check(L, L->top < L->ci->top);
    newreg = agnReg_new(L, t->top);
    setregvalue(L, L->top++, newreg);
    c = 0;
    for (i=0; i < regvalue(L->base + src_off)->top; i++) {
      lua_lock(L);
      t = regvalue(L->base + src_off);
      arg = regitem(t, i);
      setobj2s(L, L->top, L->base + func_off);  /* push function */
      setobj2s(L, L->top + 1, arg);  /* 1st argument */
      luaD_checkstack(L, 2);
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      if (!ttisboolean(L->top - 1))
        luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$");
      else if (l_istrue(L->top - 1)) {
        t = regvalue(L->base + src_off);
        arg = regitem(t, i);
        agnReg_seti(L, regvalue(L->top - 2), ++c, arg);
        luaC_barrierreg(L, regvalue(L->top - 2), arg);
      }
      L->top--;
      lua_lock(L);
    }
    agnReg_resize(L, regvalue(L->top - 1), c, 0);
    /* prepare register for return to Agena environment: move new structure to final position */
    t = regvalue(L->base + src_off);
    newreg = regvalue(L->top - 1);
    setregvalue(L, L->top - 1, newreg);
    /* add metatable and user-defined type of original table */
    agenaV_setmetatable(L, L->top - 1, t->metatable, 0);
    agenaV_setutype(L, L->top - 1, t->type);
    lua_unlock(L);
  } else if (ttisstring(L->base + src_off)) {
    const char *str = svalue(L->base + src_off);
    size_t i, len = tsvalue(L->base + src_off)->len;
    api_check(L, L->top < L->ci->top);
    Charbuf buf;
    charbuf_init(L, &buf, 0, len, 1);
    for (i=0; i < tsvalue(L->base + src_off)->len; i++) {
      lua_lock(L);
      setobj2s(L, L->top, L->base + func_off);  /* push function */
      str = svalue(L->base + src_off);
      setsvalue(L, L->top + 1, luaS_newlstr(L, str + i, 1));
      luaD_checkstack(L, 2);
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      if (!ttisboolean(L->top - 1)) {
        charbuf_finish(&buf);  /* better sure than sorry */
        charbuf_free(&buf);
        luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$");
      } else if (l_istrue(L->top - 1)) {
        str = svalue(L->base + src_off);
        charbuf_append(L, &buf, str + i, 1);
      }
      L->top--;
      lua_unlock(L);
    }
    /* prepare sequence for return to Agena environment: move new structure to final position */
    charbuf_finish(&buf);  /* better sure than sorry */
    setsvalue2s(L, L->top, luaS_newlstr(L, buf.data, buf.size));
    charbuf_free(&buf);
    lua_unlock(L);
    L->top++;
  } else if (ttisnil(L->base + src_off)) {
    setnilvalue(L->top++);
  } else {
    luaG_runerror(L, "Error in " LUA_QS ": structure, null or string expected, got %s.", "$",
      luaL_typename(L, idx2));
  }
}


/* for `$$` has operator, 2.22.2, based on pre-2.29.3 agenaV_select implementation; 7.6.11 revision;
   7.7.6 fix proposed by Gemini AI to prevent invalid reads, succesfully tested with Valgrind.
   There is no increase in speed integrating this code into lvm.c.  */
LUA_API void agn_checkfor (lua_State *L, int idx1, int idx2) {
  TValue *arg;
  size_t i;
  ptrdiff_t base_off = L->top - L->base;
  StkId idxb = index2adr(L, idx1);
  StkId idxc = index2adr(L, idx2);
  ptrdiff_t func_off = idxb - L->base;
  ptrdiff_t src_off = idxc - L->base;
  if (!ttisfunction(idxb))
    luaL_error(L, "Error in " LUA_QS ": left operand must be a function, got %s.", "$$", luaL_typename(L, idx1));
  if (ttistable(idxc)) {
    TValue *val;
    Table *t = hvalue(idxc);
    api_check(L, L->top < L->ci->top);
    /* we deliberately reserve explitic slots in the loop, to prevent invalid reads */
    lua_lock(L);
    for (i=0; i < t->sizearray; i++) {
      if (ttisnotnil(&t->array[i])) {
        luaD_checkstack(L, 2);
        StkId current_idxb = L->base + func_off;
        t = hvalue(L->base + src_off);
        arg = &t->array[i];
        setobj2s(L, L->top, current_idxb);  /* push function */
        setobj2s(L, L->top + 1, arg); /* 1st argument */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);  /* L->top is now decreased by 1 */
        if (!ttisboolean(L->top - 1)) {
          lua_unlock(L);
          luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$");
        }
        else if (l_istrue(L->top - 1)) {
          setbvalue(L->base + base_off, 1);
          L->top = L->base + base_off + 1;
          lua_unlock(L);
          return;
        }
        L->top--;  /* pop Boolean */
      }  /* of if */
    }  /* of for i */
    t = hvalue(L->base + src_off);
    for (i -= t->sizearray; i < sizenode(t); i++) {
      if (ttisnotnil(gval(gnode(t, i)))) {
        luaD_checkstack(L, 2);
        StkId current_idxb = L->base + func_off;
        t = hvalue(L->base + src_off);
        val = gval(gnode(t, i));
        setobj2s(L, L->top, current_idxb);   /* push function */
        setobj2s(L, L->top + 1, val);  /* 1st argument */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        if (!ttisboolean(L->top - 1)) {
          lua_unlock(L);
          luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$");
        }
        else if (l_istrue(L->top - 1)) {
          setbvalue(L->base + base_off, 1);
          L->top = L->base + base_off + 1;
          lua_unlock(L);
          return;
        }
        L->top--;  /* pop Boolean */
      }
    }
    setbvalue(L->base + base_off, 0);
    L->top = L->base + base_off + 1;
    lua_unlock(L);
  } else if (ttisuset(idxc)) {
    UltraSet *t = usvalue(idxc);
    api_check(L, L->top < L->ci->top);
    lua_lock(L);
    for (i=0; i < sizenode(t); i++) {
      if (ttisnotnil(glkey(gnode(t, i)))) {
        luaD_checkstack(L, 2);
        StkId current_idxb = L->base + func_off;
        t = usvalue(L->base + src_off);
        arg = key2tval(gnode(t, i));
        setobj2s(L, L->top, current_idxb);   /* push function */
        setobj2s(L, L->top + 1, arg);  /* 1st argument */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        if (!ttisboolean(L->top - 1)) {
          lua_unlock(L);
          luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$");
        }
        else if (l_istrue(L->top - 1)) {
          setbvalue(L->base + base_off, 1);
          L->top = L->base + base_off + 1;
          lua_unlock(L);
          return;
        }
        L->top--;
      }
    }
    setbvalue(L->base + base_off, 0);
    L->top = L->base + base_off + 1;
    lua_unlock(L);
  } else if (ttisseq(idxc)) {
    Seq *t = seqvalue(idxc);
    api_check(L, L->top < L->ci->top);
    lua_lock(L);
    for (i=0; i < t->size; i++) {
      luaD_checkstack(L, 2);
      StkId current_idxb = L->base + func_off;
      t = seqvalue(L->base + src_off);
      arg = seqitem(t, i);
      setobj2s(L, L->top, current_idxb);   /* push function */
      setobj2s(L, L->top + 1, arg);  /* 1st argument */
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      if (!ttisboolean(L->top - 1)) {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$");
      }
      else if (l_istrue(L->top - 1)) {
        setbvalue(L->base + base_off, 1);
        L->top = L->base + base_off + 1;
        lua_unlock(L);
        return;
      }
      L->top--;
    }
    setbvalue(L->base + base_off, 0);
    L->top = L->base + base_off + 1;
    lua_unlock(L);
  } else if (ttisreg(idxc)) {
    Reg *t = regvalue(idxc);
    api_check(L, L->top < L->ci->top);
    lua_lock(L);
    for (i=0; i < t->top; i++) {
      luaD_checkstack(L, 2);
      StkId current_idxb = L->base + func_off;
      t = regvalue(L->base + src_off);
      arg = regitem(t, i);
      setobj2s(L, L->top, current_idxb);   /* push function */
      setobj2s(L, L->top + 1, arg);  /* 1st argument */
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      if (!ttisboolean(L->top - 1)) {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$");
      }
      else if (l_istrue(L->top - 1)) {
        setbvalue(L->base + base_off, 1);
        L->top = L->base + base_off + 1;
        lua_unlock(L);
        return;
      }
      L->top--;
    }
    setbvalue(L->base + base_off, 0);
    L->top = L->base + base_off + 1;
    lua_unlock(L);
  } else if (ttisstring(idxc)) {
    size_t len = tsvalue(idxc)->len;
    api_check(L, L->top < L->ci->top);
    lua_lock(L);
    for (i=0; i < len; i++) {
      luaD_checkstack(L, 2);
      StkId current_idxb = L->base + func_off;
      const char *str = svalue(L->base + src_off);
      setobj2s(L, L->top, current_idxb);  /* push function */
      setsvalue(L, L->top + 1, luaS_newlstr(L, str + i, 1));
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      if (!ttisboolean(L->top - 1)) {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$");
      } else if (l_istrue(L->top - 1)) {
        setbvalue(L->base + base_off, 1);
        L->top = L->base + base_off + 1;
        lua_unlock(L);
        return;
      }
      L->top--;
    }
    setbvalue(L->base + base_off, 0);
    L->top = L->base + base_off + 1;
    lua_unlock(L);
  } else if (ttisnil(idxc)) {
    setnilvalue(L->base + base_off);
    L->top = L->base + base_off + 1;
  } else {
    luaG_runerror(L, "Error in " LUA_QS ": structure expected, got %s.", "$$",
      luaL_typename(L, idx2));
  }
}


/* for `$$$` count operator, based on agenaV_checkfor; 7.6.11 revision; 7.7.6 fix proposed
   by Gemini AI to prevent invalid reads, successfully tested with Valgrind.
   There is no increase in speed integrating this code into lvm.c. */
LUA_API void agn_count (lua_State *L, int idx1, int idx2) {
  TValue *arg;
  size_t i, count = 0;
  ptrdiff_t base_off = L->top - L->base;
  StkId idxb = index2adr(L, idx1);
  StkId idxc = index2adr(L, idx2);
  ptrdiff_t func_off = idxb - L->base;
  ptrdiff_t src_off = idxc - L->base;
  if (!ttisfunction(idxb))
    luaL_error(L, "Error in " LUA_QS ": left operand must be a function, got %s.", "$$$", luaL_typename(L, idx1));
  if (ttistable(idxc)) {
    TValue *val;
    Table *t = hvalue(idxc);
    api_check(L, L->top < L->ci->top);
    /* we deliberately reserve explitic slots in the loop, to prevent invalid reads */
    lua_lock(L);
    for (i=0; i < t->sizearray; i++) {
      if (ttisnotnil(&t->array[i])) {
        luaD_checkstack(L, 2);
        StkId current_idxb = L->base + func_off;
        t = hvalue(L->base + src_off);
        arg = &t->array[i];
        setobj2s(L, L->top, current_idxb);  /* push function */
        setobj2s(L, L->top + 1, arg); /* 1st argument */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);  /* L->top is now decreased by 1 */
        if (!ttisboolean(L->top - 1)) {
          lua_unlock(L);
          luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$$");
        }
        count += l_istrue(L->top - 1);
        L->top--;  /* pop Boolean */
      }  /* of if */
    }  /* of for i */
    t = hvalue(L->base + src_off);
    for (i -= t->sizearray; i < sizenode(t); i++) {
      if (ttisnotnil(gval(gnode(t, i)))) {
        luaD_checkstack(L, 2);
        StkId current_idxb = L->base + func_off;
        t = hvalue(L->base + src_off);
        val = gval(gnode(t, i));
        setobj2s(L, L->top, current_idxb);   /* push function */
        setobj2s(L, L->top + 1, val);  /* 1st argument */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        if (!ttisboolean(L->top - 1)) {
          lua_unlock(L);
          luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$$");
        }
        count += l_istrue(L->top - 1);
        L->top--;  /* pop Boolean */
      }
    }
    lua_unlock(L);
  } else if (ttisuset(idxc)) {
    UltraSet *t = usvalue(idxc);
    api_check(L, L->top < L->ci->top);
    lua_lock(L);
    for (i=0; i < sizenode(t); i++) {
      if (ttisnotnil(glkey(gnode(t, i)))) {
        luaD_checkstack(L, 2);
        StkId current_idxb = L->base + func_off;
        t = usvalue(L->base + src_off);
        arg = key2tval(gnode(t, i));
        setobj2s(L, L->top, current_idxb);   /* push function */
        setobj2s(L, L->top + 1, arg);  /* 1st argument */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        if (!ttisboolean(L->top - 1)) {
          lua_unlock(L);
          luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$$");
        }
        count += l_istrue(L->top - 1);
        L->top--;
        lua_unlock(L);
      }
    }
    lua_unlock(L);
  } else if (ttisseq(idxc)) {
    Seq *t = seqvalue(idxc);
    api_check(L, L->top < L->ci->top);
    lua_lock(L);
    for (i=0; i < t->size; i++) {
      luaD_checkstack(L, 2);
      StkId current_idxb = L->base + func_off;
      t = seqvalue(L->base + src_off);
      arg = seqitem(t, i);
      setobj2s(L, L->top, current_idxb);   /* push function */
      setobj2s(L, L->top + 1, arg);  /* 1st argument */
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      if (!ttisboolean(L->top - 1)) {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$$");
      }
      count += l_istrue(L->top - 1);
      L->top--;
    }
    lua_unlock(L);
  } else if (ttisreg(idxc)) {
    Reg *t = regvalue(idxc);
    api_check(L, L->top < L->ci->top);
    lua_lock(L);
    for (i=0; i < t->top; i++) {
      luaD_checkstack(L, 2);
      StkId current_idxb = L->base + func_off;
      t = regvalue(L->base + src_off);
      arg = regitem(t, i);
      setobj2s(L, L->top, current_idxb);   /* push function */
      setobj2s(L, L->top + 1, arg);  /* 1st argument */
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      if (!ttisboolean(L->top - 1)) {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$$");
      }
      count += l_istrue(L->top - 1);
      L->top--;
    }
    lua_unlock(L);
  } else if (ttisstring(idxc)) {
    size_t len = tsvalue(idxc)->len;
    api_check(L, L->top < L->ci->top);
    lua_lock(L);
    for (i=0; i < len; i++) {
      luaD_checkstack(L, 2);
      StkId current_idxb = L->base + func_off;
      const char *str = svalue(L->base + src_off);
      setobj2s(L, L->top, current_idxb);  /* push function */
      setsvalue(L, L->top + 1, luaS_newlstr(L, str + i, 1));
      L->top += 2;
      luaD_call(L, L->top - 2, 1, 0);
      if (!ttisboolean(L->top - 1)) {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": function must return a boolean.", "$$$");
      }
      count += l_istrue(L->top - 1);
      L->top--;
    }
    lua_unlock(L);
  } else if (ttisnil(idxc)) {
    setnilvalue(L->base + base_off);
    L->top = L->base + base_off + 1;
    return;
  } else {
    luaG_runerror(L, "Error in " LUA_QS ": structure expected, got %s.", "$$$",
      luaL_typename(L, idx2));
  }
  setnvalue(L->base + base_off, cast_num(count));
  L->top = L->base + base_off + 1;
}


LUA_API void lua_setargs (lua_State *L, int argc, char **argv) {  /* 7.8.3 */
  lua_lock(L);
  L->agn_argc = argc;
  L->agn_argv = argv;
  lua_unlock(L);
}


/* +++++++++++++++++++++++++++++++++++++++++++++++++ LUA 5.4 API FUNCTIONS +++++++++++++++++++++++++++++++++++++++++++++++ */

/* Returns the length of the value at the given index. It is equivalent to the 'size' operator in Lua and may trigger a
   metamethod for the "length" event. The result is pushed on the stack. Taken from Lua 5.4.0 RC 5, 2.21.2. */
LUA_API void lua_len (lua_State *L, int idx) {
  TValue *t;
  lua_lock(L);
  t = index2value(L, idx);
  luaV_objlen(L, L->top, t);
  api_incr_top(L);
  lua_unlock(L);
}


/* Performs an arithmetic operation over the two values (or one, in the case of negations) at the top of the stack, with the
   value at the top being the second operand, pops these values, and pushes the result of the operation. The function follows
   the semantics of the corresponding Agena operator (that is, it may call metamethods).

   The value of op must be one of the following constants:

   OP_ADD: performs addition (+)
   OP_SUB: performs subtraction (-)
   OP_MUL: performs multiplication (*)
   OP_DIV: performs float division (/)
   OP_INTDIV: performs integer division (\)
   OP_MOD: performs modulo (%)
   OP_POW: performs exponentiation (^)
   OP_IPOW: performs integer exponentiation (**)
   OP_UNM: performs mathematical negation (unary -)

   Taken from Lua 5.4.4, 2.21.1, 2.26.2 */
LUA_API void lua_arith (lua_State *L, int op) {
  lua_lock(L);
  if (op != LUA_OPUNM && op != LUA_OPBNOT) {
    api_checknelems(L, 2);  /* all other operations expect two operands */
  } else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top, L->top - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result go to top - 2 */
  luaO_arith(L, op, s2v(L->top - 2), s2v(L->top - 1), L->top - 2);
  L->top--;  /* remove second operand */
  lua_unlock(L);
}


/* Compares two Agena values. Returns 1 if the value at index index1 satisfies op when compared with the value at index index2,
   following the semantics of the corresponding Lua operator (that is, it may call metamethods). Otherwise returns 0. Also returns
   0 if any of the indices is not valid.

   The value of op must be one of the following constants:

     OP_EQ: compares for equality (==)
     OP_LT: compares for less than (<)
     OP_LE: compares for less or equal (<=)

   Taken from Lua 5.4.4, 2.21.1 */
LUA_API int lua_compare (lua_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  lua_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case OP_EQ: i = luaV_equalobj(L, o1, o2); break;
      case OP_LT: i = luaV_lessthan(L, o1, o2, 1); break;
      case OP_LE: i = luaV_lessequal(L, o1, o2, 1); break;
      default: api_check(L, "invalid option");
    }
  }
  lua_unlock(L);
  return i;
}


/* Sets the warning function to be used by Lua to emit warnings (see lua_WarnFunction). The ud parameter sets
   the value ud passed to the warning function. Taken from Lua 5.4.4, 2.21.1 */
LUA_API void lua_setwarnf (lua_State *L, lua_WarnFunction f, void *ud) {
  lua_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  lua_unlock(L);
}


/*
** 0 - warning system is off;
** 1 - ready to start a new message;
** 2 - previous message is to be continued.
*/
LUA_API int lua_getwarnf (lua_State *L) {
  int *warnstate = (int *)(G(L)->ud_warn);
  return *warnstate;
}


/* Emits a warning with the given message. A message in a call with tocont true should be continued in another call to this function.
   Taken from Lua 5.4.4, 2.21.1 */
LUA_API void lua_warning (lua_State *L, const char *msg, int tocont) {
  lua_lock(L);
  luaE_warning(L, msg, tocont);
  lua_unlock(L);
}


/* Taken from Lua 5.4.4, 2.21.1 */
/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'lua_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
static void reverse (lua_State *L, StkId from, StkId to) {
  for (; from < to; from++, to--) {
    TValue temp;
    setobj(L, &temp, s2v(from));
    setobjs2s(L, from, to);
    setobj2s(L, to, &temp);
  }
}


/* Taken from Lua 5.4.4, 2.21.1 */
static StkId index2stack (lua_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func + idx;
    api_check(L, o < L->top);
    return o;
  }
  else {  /* non-positive index */
    api_check(L, idx != 0 && -idx <= L->top - (ci->func + 1));
    api_check(L, !ispseudo(idx));
    return L->top + idx;
  }
}

/* Rotates the stack elements between the valid index idx and the top of the stack. The elements are rotated n positions
   in the direction of the top, for a positive n, or -n positions in the direction of the bottom, for a negative n.
   The absolute value of n must not be greater than the size of the slice being rotated. This function cannot be called
   with a pseudo-index, because a pseudo-index is not an actual stack position.
   Taken from Lua 5.4.4, 2.21.1

   Let x = AB, where A is a prefix of length 'n'. Then,
   rotate x n == BA. But BA == (A^r . B^r)^r.
*/
LUA_API void lua_rotate (lua_State *L, int idx, int n) {
  StkId p, t, m;
  lua_lock(L);
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1));
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  lua_unlock(L);
}


/* Copies the element at index fromidx into the valid index toidx, replacing the value at that position. Values at other
   positions are not affected. Taken from Lua 5.4.4, 2.21.1 */
LUA_API void lua_copy (lua_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  lua_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to));
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    luaC_barrier(L, clvalue(s2v(L->ci->func)), fr);
  /* LUA_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  lua_unlock(L);
}


/* Converts the zero-terminated string s to a number, pushes that number into the stack, and returns the total size of
   the string, that is, its length plus one. The conversion can result in an integer or a float, according to the lexical
   conventions of Agena. The string may have leading and trailing whitespaces and a sign. If the string is not a valid
   numeral, returns 0 and pushes nothing. (Note that the result can be used as a boolean, true if the conversion succeeds.)
   Taken from Lua 5.4.4, 2.21.1 */
LUA_API size_t lua_stringtonumber (lua_State *L, const char *s) {
  size_t sz = luaO_str2num(s, s2v(L->top));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


LUA_API uint64_t lua_stringtouint64 (lua_State *L, const char *s, int issueerror, const char *procname, int *rc) {
  uint64_t result;
  const char *r = luaO_str2uint64(s, &result);
  *rc = 0;
  if (r == NULL) {
    *rc = result;
    if (issueerror) {
      if (result == 1)
        luaL_error(L, "Error in " LUA_QS ": overflow in the input.", procname);
      else
        luaL_error(L, "Error in " LUA_QS ": could not convert string to integer.", procname);
    }
  }
  return result;
}


LUA_API int64_t lua_stringtoint64 (lua_State *L, const char *s, int issueerror, const char *procname, int *rc) {
  int64_t result;
  const char *r = luaO_str2int64(s, &result);
  *rc = 0;
  if (r == NULL) {
    *rc = result;
    if (issueerror) {
      if (result == 1)
        luaL_error(L, "Error in " LUA_QS ": overflow in the input.", procname);
      else
        luaL_error(L, "Error in " LUA_QS ": could not convert string to integer.", procname);
    }
  }
  return result;
}


LUA_API const char *agn_gettoken (lua_State *L, int token) {
  if (token < FIRST_RESERVED) return NULL;
  return luaX_tokens[token - FIRST_RESERVED];
}


/***** Lua 5.2-compat functions, taken from lcurses-9.0.0.1 library, files /ext/include/compat-5.2.*; Agena 2.27.1 *****/

LUA_API lua_Number lua_tonumberx (lua_State *L, int i, int *isnum) {
  lua_Number n = lua_tonumber(L, i);
  if (isnum != NULL) {
    *isnum = (n != 0 || lua_isnumber(L, i));
  }
  return n;
}


#define PACKAGE_KEY "_COMPAT52_PACKAGE"

static void push_package_table (lua_State *L) {
  lua_pushliteral(L, PACKAGE_KEY);
  lua_rawget(L, LUA_REGISTRYINDEX);
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    /* try to get package table from globals */
    lua_pushliteral(L, "package");
    lua_rawget(L, LUA_GLOBALSINDEX);
    if (lua_istable(L, -1)) {
      lua_pushliteral(L, PACKAGE_KEY);
      lua_pushvalue(L, -2);
      lua_rawset(L, LUA_REGISTRYINDEX);
    }
  }
}

/* Pushes onto the stack the Lua value associated with the userdata at the given index.
   This Lua value must be a table or nil.  See Lua's getenv() function. */
LUA_API void lua_getuservalue (lua_State *L, int i) {
  luaL_checktype(L, i, LUA_TUSERDATA);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_getfenv(L, i);
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  if (lua_rawequal(L, -1, -2)) {
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_replace(L, -2);
  } else {
    lua_pop(L, 1);
    push_package_table(L);
    if (lua_rawequal(L, -1, -2)) {
      lua_pop(L, 1);
      lua_pushnil(L);
      lua_replace(L, -2);
    } else
      lua_pop(L, 1);
  }
}


/* Pops a table or nil from the stack and sets it as the new value associated to the
   userdata at the given index. See Lua's setenv() function. */
LUA_API void lua_setuservalue (lua_State *L, int i) {
  luaL_checktype(L, i, LUA_TUSERDATA);
  if (lua_isnil(L, -1)) {
    luaL_checkstack(L, 1, "not enough stack space");
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_replace(L, -2);
  }
  lua_setfenv(L, i);
}


LUA_API int lua_log2 (lua_State *L, unsigned int x) {
  return luaO_log2(x);
}


LUA_API int agn_log2 (lua_State *L, unsigned int x) {
  return agnO_log2(x);
}


/* lhf's ae API, 5.0.0:
 * A library for evaluating mathematical expressions in C programs based on Lua
 * Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br>
 * 11 Aug 2018 16:44:34
 * This code is hereby placed in the public domain and also under the MIT license.
 *
 * Reference manual
 *
 * ae_open()
 *	Opens ae to be used. Call it once before calling the others.
 *	Does nothing if ae is already open.
 *
 * ae_close()
 *	Closes ae after use. All variables are deleted. All memory is released.
 *	Does nothing if ae is already closed.
 *
 * ae_set(name, value)
 *	Sets the value of a variable.
 *	The value persists until it is set again or ae is closed.
 *	Undefined variables evaluate to 0.
 *
 * ae_eval(expression)
 *	Evaluates the given expression and returns its value as a double.
 *	Once ae has seen an expression, ae can evaluate it repeatedly quickly
 *  as it is internally cached.
 *	Returns 0 if there is an error. ae_error returns the error message.
 *
 * ae_error()
 *	Returns the last error message or NULL if there is none.
 *
 * Example:
  int main (void) {
   double x;
   ae_open();
   ae_set("a", 1.0);
   ae_set("b", -5.0);
   ae_set("c", 6.0);
   printf("%s\t%g\n", "a", ae_eval("a"));
   printf("%s\t%g\n", "b", ae_eval("b"));
   printf("%s\t%g\n", "c", ae_eval("c"));
   printf("%s\t%s\n", "x", "a*x^2+b*x+c");
   for (x=0.0; x < 4.0; x += 0.25) {
     ae_set("x", x);
     printf("%g\t%g\n", x, ae_eval("a*x^2+b*x+c"));
   }
   ae_close();
   return 0;
 } */

#undef  iscsym
#define iscsym(c)	(isalnum(c) || ((c) == '_'))

static lua_State *LA = NULL;

LUA_API void ae_open (void) {
  if (LA != NULL) return;
  LA = luaL_newstate();
  if (LA == NULL) return;
  lua_settop(LA, 0);
  lua_pushnil(LA);  /* slot for error message */
}

LUA_API void ae_close (void) {
  if (LA == NULL) return;
  lua_close(LA);
  LA = NULL;
}

LUA_API void ae_set (const char *name, double value) {
  if (LA == NULL) return;
  lua_pushnumber(LA, value);
  lua_setglobal(LA, name);
}

LUA_API const char* ae_error (void) {
  if (LA == NULL) return "ae not open"; else return lua_tostring(LA, 1);
}

#define LA_FUNCTION	"procedure"
#define LA_RETURN		"return "

static int badinput (const char *expression) {
  const char *b = expression;
  const char *e;
  for (;;) {
    int p, q;
    b = strstr(b, LA_FUNCTION); if (b == NULL) return 0;
    e = b + sizeof(LA_FUNCTION) - 2;
    p = (b == expression) ? 0 : b[-1];
    q = e[1];
    if (!iscsym(p) && !iscsym(q)) return 1;
    b = e + 1;
  }
  return 1;
}

typedef struct LoadS { const char *text[3]; size_t size[3]; int i; } LoadS;

static const char *getSLA (lua_State* L, void *data, size_t *size) {
  LoadS* S = (LoadS*)data;
  int i = S->i++;
  *size = S->size[i];
  (void)L;
  return S->text[i];
}

static int compile (const char *expression) {
  LoadS S;
  if (badinput(expression)) return 1;
  S.text[0] = LA_RETURN;	S.size[0] = sizeof(LA_RETURN) - 1;
  S.text[1] = expression;	S.size[1] = strlen(expression);
  S.text[2] = NULL;	S.size[2] = 0;
  S.i = 0;
  lua_pushliteral(LA, "ae:1: syntax error");
  return lua_load(LA, getSLA, &S, "(ae)");
}

LUA_API double ae_eval (const char *expression) {
  double value;
  int error = 0;
  if (LA == NULL) return 0.0;
  lua_getglobal(LA, expression);		/* is it cached? */
  switch (lua_type(LA, -1)) {
    case LUA_TNIL:  /* no: compile, cache, and call it */
      error = compile(expression);
      if (error) break;
      lua_pushvalue(LA, -1);
      lua_setglobal(LA, expression);
    case LUA_TFUNCTION:  /* yes: call it */
      if (!lua_iscfunction(LA, -1)) error = lua_pcall(LA, 0, 1, 0);
  }
  if (error) {
    value = 0.0;
    lua_pushstring(LA, strchr(lua_tostring(LA, -1), ' ') + 1);
  } else if (lua_type(LA, -1) == LUA_TNUMBER) {
    value = lua_tonumber(LA, -1);
    lua_pushnil(LA);
  } else if (lua_type(LA, -1) == LUA_TBOOLEAN) {
    value = lua_toboolean(LA, -1);
    lua_pushnil(LA);
  } else {
    value = 0.0;
    lua_pushliteral(LA, "not a number");
  }
  lua_replace(LA, 1);  /* save error message */
  lua_settop(LA, 1);
  return value;
}


/*********************************************************************
* This file contains parts of Lua 5.2 and 5.4 source code:
*
* Copyright (C) 1994-2025 Lua.org, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*********************************************************************/


/* +++++++++++++++++++++++++++++++++++++++++++++++++     MISCELLANEOUS     ++++++++++++++++++++++++++++++++++++++++++++++ */

/* Taken from https://github.com/lunarmodules/lua-compat-5.3, v0.15.1, as of July 18, 2026 */

/* Gemini AI explains on July 18, 2026:
This function lua_getextraspace() backports the Lua 5.3+ lua_getextraspace feature to Lua 5.1/5.2 by using a registry-bound,
thread-keyed weak table to store and retrieve custom user data pointers allocated for individual coroutines (lua_State).

Core Purpose
In Lua 5.3 and newer, every lua_State has a dedicated memory block (LUA_EXTRASPACE bytes) directly preceding the state
pointer. C code uses lua_getextraspace(L) to quickly access this buffer for storing application-specific context (like
object wrappers or host application pointers). Because Lua 5.1 does not natively allocate this memory layout, this
compatibility wrapper emulates the behavior via Lua's registry table.

Detailed Breakdown

Registry Table Management: The function checks for a hidden table stored under the key "__compat53_extraspace" inside the
LUA_REGISTRYINDEX. If it does not exist, it initializes a new table equipped with a weak-keyed metatable (__mode = "k").
This prevents memory leaks by allowing dead Lua threads to be garbage collected naturally.

Thread-Keyed Lookup: It uses the current execution thread (lua_pushthread(L)) as a unique hash key inside this tracking
table to find the associated memory buffer.

Lazy Allocation: If no buffer exists yet for the given thread, the function creates a new one using
lua_newuserdata(L, LUA_EXTRASPACE).

State Inheritance:

- If the thread is the main thread, the new buffer is zeroed out using memset.
- If the thread is a coroutine, it searches for the main thread's extraspace buffer. If found, it copies the main thread's
  data using memcpy to pass context down; otherwise, it zeroes it out.

Stack Cleanup: The function cleans up all internal working values pushed to the Lua stack during its lifecycle before
returning a direct pointer (void *) to the allocated user data memory.

To help you integrate this smoothly, could you tell me:

    Which Lua host version (5.1, 5.2, or Luajit) are you targeting?
    What specific application data are you looking to store in the extra space?

LUA 5.1.5

To use this compatibility function effectively in Lua 5.1.5, you must account for a critical limitation: Lua 5.1 cannot
dynamically grow or shrink its memory blocks via standard pointers.

Because this wrapper simulates the extra space using standard Lua userdata rather than a fixed C-side offset, your
implementation must adhere to specific rules.

Performance Implications in 5.1.5

Execution Overhead: In native Lua 5.3+, lua_getextraspace is a lightning-fast macro that subtracts a pointer offset.
In Lua 5.1.5, calling this function triggers table lookups, garbage collection checks, and potential memory allocations.
Avoid calling it inside tight loops.

Garbage Collection (GC): The wrapper uses a weak table. The extra space memory stays alive as long as the coroutine
(lua_State) is alive. Once the thread is garbage-collected, the extra space memory is automatically freed.

Critical Implementation Rules

Define LUA_EXTRASPACE Early: You must define the byte size of your extra space layout before compiling the library code.

#define LUA_EXTRASPACE sizeof(struct MyCustomContext)

Use code with caution.

No Reallocation: The chunk size is completely fixed at initialization. You cannot change the size of the block after
compilation.

Treat Pointers as Ephemeral: Never store the void* pointer returned by this function globally or pass it across cycles.
If the Lua thread goes out of scope and gets collected, the pointer instantly becomes dangling. Always fetch it fresh
using lua_getextraspace(L).

Mind the Copy Behavior: As seen in the source code, child coroutines created via lua_newthread will copy the contents
of the parent thread's extra space at the moment of creation. Changes made to the parent thread's extra space after
spawning the coroutine will not sync to the child.

What data structure are you planning to store in the extra space? If you share your use case, I can help you set up
the structural alignment or suggest more performant Lua 5.1 alternatives.

To get a clear picture of how this works, think of lua_getextraspace as a "secret pocket" attached to every Lua
thread (lua_State *).

Whenever Lua calls into your C code, it passes a pointer to the state (L). The extra space feature allows you to
instantly retrieve your own C data from that state without passing extra arguments or using slow global variables.

Here is a practical guide on why you use it and how to write it in your C code.

The Problem It Solves
Imagine you are building a game engine or a server application. You have a C struct representing your main
application context:

typedef struct {
  int request_count;
  char *database_url;
} AppContext;

When Lua triggers a C function, Lua only gives you lua_State *L. It does not know about your AppContext. Without
extra space, you would have to store your AppContext pointer in a messy global C variable, which breaks if you
want to run multiple independent server instances.

How to Use It (Step-by-Step)

1. Define the size of the pocket
Before including your headers or defining the function, you must tell the compatibility layer how many bytes
your structure needs.

#include <stdio.h>
#include <string.h>

// 1. Tell Lua how big the secret pocket needs to be
#define LUA_EXTRASPACE sizeof(AppContext*)

// (Paste the COMPAT53_API lua_getextraspace function code here)

2. Initialize the data at startup
When you first start your application and create the main Lua state, you put your C data pointer into that pocket.

int main() {
  lua_State *L = luaL_newstate();
  // Create your C application data
  AppContext my_app;
  my_app.request_count = 0;
  my_app.database_url = "localhost:5432";
  // Get the pocket and store the pointer to your app inside it
  void *pocket = lua_getextraspace(L);
  *(AppContext**)pocket = &my_app;
  // Now run your Lua scripts...
  lua_close(L);
  return 0;
}

3. Retrieve the data inside any C function
Now, any time Lua calls a C function, you can effortlessly grab your AppContext back out of the L variable.

// A C function that you expose to Lua
int C_LogRequest (lua_State *L) {
  // 1. Grab the pocket from the state
  void *pocket = lua_getextraspace(L);
  // 2. Extract your AppContext pointer
  AppContext *app = *(AppContext**)pocket;
  // 3. Use your C data safely!
  app->request_count++;
  printf("Request #%d processed.\n", app->request_count);
  return 0; // Return 0 values to Lua
}

Why use this instead of standard Lua tables ?

Hidden from Lua: A Lua script cannot accidentally overwrite or corrupt this pointer because it exists
entirely on the C side.

Speed: In modern Lua, it is incredibly fast. Even in this Lua 5.1 compatibility version, it prevents you from
having to look up variables in the global environment (LUA_GLOBALSINDEX), keeping your C-side execution
clean. */

#ifndef LUA_EXTRASPACE
#define LUA_EXTRASPACE (sizeof(void*))
#endif

LUA_API void *lua_getextraspace (lua_State *L) {
  int is_main = 0;
  void *ptr = NULL;
  luaL_checkstack(L, 4, "not enough stack slots available");
  lua_pushliteral(L, "__compat53_extraspace");
  lua_pushvalue(L, -1);
  lua_rawget(L, LUA_REGISTRYINDEX);
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    lua_createtable(L, 0, 2);
    lua_createtable(L, 0, 1);
    lua_pushliteral(L, "k");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -2);
    lua_pushvalue(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);
  }
  lua_replace(L, -2);
  is_main = lua_pushthread(L);
  lua_rawget(L, -2);
  ptr = lua_touserdata(L, -1);
  if (!ptr) {
    lua_pop(L, 1);
    ptr = lua_newuserdata(L, LUA_EXTRASPACE);
    if (is_main) {
      memset(ptr, '\0', LUA_EXTRASPACE);
      lua_pushthread(L);
      lua_pushvalue(L, -2);
      lua_rawset(L, -4);
      lua_pushboolean(L, 1);
      lua_pushvalue(L, -2);
      lua_rawset(L, -4);
    } else {
      void* mptr = NULL;
      lua_pushboolean(L, 1);
      lua_rawget(L, -3);
      mptr = lua_touserdata(L, -1);
      if (mptr)
        memcpy(ptr, mptr, LUA_EXTRASPACE);
      else
        memset(ptr, '\0', LUA_EXTRASPACE);
      lua_pop(L, 1);
      lua_pushthread(L);
      lua_pushvalue(L, -2);
      lua_rawset(L, -4);
    }
  }
  lua_pop(L, 2);
  return ptr;
}

