/*
** $Id: lvm.c,v 2.63 15.03.2009 12:40:39 roberto Exp $
** Lua virtual machine
** See Copyright Notice in agena.h
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROPCMPLX
#include <complex.h>
#endif

#define lvm_c
#define LUA_CORE

#include "agena.h"

#include "agncmpt.h"
#include "agnconf.h"
#include "agnxlib.h"
#include "charbuf.h"
#include "intvec.h"
#include "lcode.h"     /* for enums in Un(ary)Opr */
#include "lcomplex.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "llimits.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lpair.h"
#include "lreg.h"
#include "lseq.h"
#include "lset.h"
#include "lstate.h"
#include "lstring.h"
#include "lstrlib.h"   /* for lmemfind and match */
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"

/* limit for table tag-method chains (to avoid loops) */
#define MAXTAGLOOP   100

#define RA(i)   (base + GETARG_A(i))

#define checkstack(L, text, pn) { \
  if ((L->top - L->ci->top) > LUA_MINSTACK) \
    luaG_runerror(L, text LUA_QS ": stack overflow.", pn); \
}

#define checkstack2(L, required, text1, text2, pn) { \
  if ((L->top - L->ci->top) + (required) > LUA_MINSTACK) \
    luaG_runerror(L, text1 LUA_QS ": stack overflow, you might try " LUA_QS ".", pn, text2); \
}

#define checkstack_bitfield(L, buf, text, pn) { \
  if ((L->top - L->ci->top) > LUA_MINSTACK) { \
    if (buf) bitfield_free(buf); \
    luaG_runerror(L, text LUA_QS ": stack overflow.", pn); \
  } \
}

LUAI_FUNC void agenaV_setmetatable (lua_State *L, TValue *obj, Table *mt, int cfunc) {  /* 2.29.1 */
  switch (ttype(obj)) {
    case LUA_TTABLE: {
      Table *t = hvalue(obj);
      if (t->readonly) luaG_runerror(L, "table is read-only.");  /* 4.9.0 */
      t->metatable = mt;
      if (mt)
        luaC_objbarriert(L, t, mt);
      break;
    }
    case LUA_TUSERDATA: {
      Udata *u = rawuvalue(obj);
      if (u->uv.readonly) luaG_runerror(L, "userdata is read-only.");  /* 4.9.0 */
      u->uv.metatable = mt;
      if (mt)
        luaC_objbarrier(L, rawuvalue(obj), mt);
      break;
    }
    case LUA_TSEQ: {
      Seq *t = seqvalue(obj);
      if (t->readonly) luaG_runerror(L, "sequence is read-only.");  /* 4.9.0 */
      t->metatable = mt;
      if (mt)
        luaC_objbarrierseq(L, t, mt);
      break;
    }
    case LUA_TPAIR: {
      Pair *t = pairvalue(obj);
      if (t->readonly) luaG_runerror(L, "pair is read-only.");  /* 4.9.0 */
      t->metatable = mt;
      if (mt)
        luaC_objbarrierpair(L, t, mt);
      break;
    }
    case LUA_TSET: {
      UltraSet *t = usvalue(obj);
      if (t->readonly) luaG_runerror(L, "set is read-only.");  /* 4.9.0 */
      t->metatable = mt;
      if (mt)
        luaC_objbarrierset(L, t, mt);
      break;
    }
    case LUA_TREG: {
      Reg *t = regvalue(obj);
      if (t->readonly) luaG_runerror(L, "sequence is read-only.");  /* 4.9.0 */
      t->metatable = mt;
      if (mt)
        luaC_objbarrierreg(L, t, mt);
      break;
    }
    case LUA_TFUNCTION: {  /* 2.36.2 */
      Closure *fcl = clvalue(obj);
      if (fcl->c.isC) {
        if (fcl->c.metatable)  /* 3.15.5 fix to avoid memory leaks with GC when overwriting existing mt */
          luaG_runerror(L, "cannot overwrite metatable of C library procedure with a new one,\ninsert metamethods instead, see `addtometatable`.");
        fcl->c.metatable = mt;
      } else {
        fcl->l.metatable = mt;
      }
      if (mt != NULL)
        luaC_objbarrier(L, fcl, mt);
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
}


LUAI_FUNC int agenaV_setstorage (lua_State *L, TValue *obj, Table *t) {  /* 3.5.3 */
  switch (ttype(obj)) {
    case LUA_TFUNCTION: {
      Closure *fcl = clvalue(obj);
      if (fcl->c.isC) {
        if (fcl->c.storage && t) return 0;  /* cannot overwrite an existing storage table */
        fcl->c.storage = t;
      } else {
        if (fcl->l.storage && t) return 0;  /* cannot overwrite an existing storage table */
        fcl->l.storage = t;
      }
      if (t != NULL)
        luaC_objbarrier(L, fcl, t);
      break;
    }
    default: {
      luaG_runerror(L, "procedure expected, got %s.", luaT_typenames[(int)ttype(obj)]);
    }
  }
  return 1;
}


LUAI_FUNC void agenaV_setutype (lua_State *L, TValue *obj, TString *s) {  /* 2.29.1 */
  switch (ttype(obj)) {
    case LUA_TTABLE: {
      Table *t = hvalue(obj);
      if (t->readonly) luaG_runerror(L, "table is read-only.");
      t->type = s;
      if (s)
        luaC_objbarriert(L, t, s);
      break;
    }
    case LUA_TSET: {
      UltraSet *t = usvalue(obj);
      if (t->readonly) luaG_runerror(L, "set is read-only.");
      t->type = s;
      if (s)
        luaC_objbarrierset(L, t, s);
      break;
    }
    case LUA_TUSERDATA: {
      Udata *u = rawuvalue(obj);
      if (u->uv.readonly) luaG_runerror(L, "userdata is read-only.");
      u->uv.type = s;
      if (s)
        luaC_objbarrier(L, rawuvalue(obj), s);
      break;
    }
    case LUA_TSEQ: {
      Seq *t = seqvalue(obj);
      if (t->readonly) luaG_runerror(L, "sequence is read-only.");
      t->type = s;
      if (s)
        luaC_objbarrierseq(L, t, s);
      break;
    }
    case LUA_TREG: {
      Reg *t = regvalue(obj);
      if (t->readonly) luaG_runerror(L, "register is read-only.");
      t->type = s;
      if (s)
        luaC_objbarrierreg(L, t, s);
      break;
    }
    case LUA_TPAIR: {
      Pair *t = pairvalue(obj);
      if (t->readonly) luaG_runerror(L, "pair is read-only.");
      t->type = s;
      if (s)
        luaC_objbarrierpair(L, t, s);
      break;
    }
    case LUA_TFUNCTION: {  /* 2.34.3 */
      Closure *fcl = clvalue(obj);
      if (fcl->c.isC)
        fcl->c.type = s;
      else
        fcl->l.type = s;
      if (s != NULL) luaC_objbarrier(L, clvalue(obj), s);
      break;
    }
    default:
      luaG_runerror(L, "any structure, procedure or userdata expected.");  /* 2.34.3 change */
  }
}


/*
** Try to convert a value from string to a number value.
** If the value is not a string or is a string not representing
** a valid numeral (or if coercions from strings to numbers
** are disabled via macro 'cvt2num'), do not modify 'result'
** and return 0. Taken from Lua 5.4.0 RC 4, 2.21.1.
*/
static int l_strton (const TValue *obj, TValue *result) {
  lua_assert(obj != result);
  if (!cvt2num(obj))  /* is object not a string? */
    return 0;
  else
    return (luaO_str2num(svalue(obj), result) == vslen(obj) + 1);
}

int luaV_tonumber_ (const TValue *obj, lua_Number *n) {  /* taken from Lua 5.4.0 RC 4, 2.21.1 */
  TValue v;
  if (ttisnumber(obj)) {
    *n = cast_num(nvalue(obj));
    return 1;
  } else if (l_strton(obj, &v)) {  /* string coercible to number? */
    *n = nvalue(&v);  /* convert result of 'luaO_str2num' to a float */
    return 1;
  } else if (ttisstring(obj)) {
    if (tools_streq(svalue(obj), "undefined")) {
      *n = AGN_NAN;
      return 1;
    } else if (tools_streq(svalue(obj), "infinity")) {
      *n = HUGE_VAL;
      return 1;
    } else if (tools_streq(svalue(obj), "-infinity")) {
      *n = -HUGE_VAL;
      return 1;
    } else
      return 0;
  } else
    return 0;  /* conversion failed */
}

int luaV_tonumberx_ (const TValue *obj, lua_Number *n) {
  return (ttisnumber(obj) ? (*n = nvalue(obj), 1) : luaV_tonumber_(obj, n));
}


const TValue *luaV_arithmoperand (const TValue *obj, TValue *n) {  /* used in ldebug.c */
  return (ttisnumber(obj) || ttiscomplex(obj)) ? obj : NULL;
}


const TValue *luaV_tocomplex (const TValue *obj, TValue *n) {  /* 0.26.1 */
#ifndef PROPCMPLX
  agn_Complex num;
#else
  lua_Number num[2];
#endif
  if (ttiscomplex(obj)) return obj;
  if (ttisstring(obj)) {
#ifndef PROPCMPLX
    if (luaO_str2c(svalue(obj), &num)) {
#else
    if (luaO_str2c(svalue(obj), num)) {
#endif
      setcvalue(n, num);
      return n;
    }
  }
  return NULL;
}


static const char *const Booleans [] = {"false", "true", "fail"};

static void aux_fillstrwithnum (lua_State *L, char s[], lua_Number n) {  /* 4.5.4 */
  if (n == HUGE_VAL)
    strcpy(s, "infinity\0");
  else if ((-n) == HUGE_VAL)
    strcpy(s, "-infinity\0");
  else if (isnan(n) || isnan(-n))
    strcpy(s, "undefined\0");
  else {
    /* 7.1.6 fix: the function is surprisingly rarely called in real life, so let's check `Digits` here. */
    lua_getglobal(L, "Digits");
    if (agn_isnonnegint(L, -1)) {
      ptrdiff_t n = agn_tointeger(L, -1);
      if (n < 1) n = 1;
      if (n > 17) n = 17;
      agn_setdigits(L, n);
    }
    agn_poptop(L);
    lua_number2str(s, n);
  }
}

/* Adapt lua_stringconvertible in agena.h if you change this */
int luaV_tostring (lua_State *L, StkId obj) {
  if (ttisnumber(obj)) {
    char s[LUAI_MAXNUMBER2STR];
    lua_Number n = nvalue(obj);
    aux_fillstrwithnum(L, s, n);
    setsvalue2s(L, obj, luaS_new(L, s));
    return 1;
  } else if (ttiscomplex(obj)) {  /* 4.5.4 extension */
    Charbuf buf;
    char s0[LUAI_MAXNUMBER2STR];
    char s1[LUAI_MAXNUMBER2STR];
    lua_Number re = complexreal(obj);
    lua_Number im = compleximag(obj);
    aux_fillstrwithnum(L, s0, re);  /* the real part */
    aux_fillstrwithnum(L, s1, im);  /* the imaginary part */
    charbuf_init(L, &buf, 0, LUAI_MAXNUMBER2STR, 0);
    if (re != 0.0) {
      charbuf_append(L, &buf, s0, tools_strlen(s0));
      if (!signbit(im))  /* also covers -0 correctly */
        charbuf_append(L, &buf, "+", 1);
    }
    if (fabs(im) != 1.0) {  /* 4.10.0 improvement */
      charbuf_append(L, &buf, s1, tools_strlen(s1));
      charbuf_append(L, &buf, "*I", 2);
    } else {
      if (signbit(im))
        charbuf_append(L, &buf, "-", 1);
      charbuf_append(L, &buf, "I", 1);
    }
    charbuf_finish(&buf);
    setsvalue2s(L, obj, luaS_newlstr(L, buf.data, buf.size));
    charbuf_free(&buf);
    return 1;
  } else if (ttisboolean(obj)) {  /* 2.2.0 extension */
    setsvalue2s(L, obj, luaS_new(L, Booleans[bvalue(obj)]));
    return 1;
  }
  /* else if (ttisnil(obj)) ...
     do not run this as many functions would not work any longer for they expect the null value instead of the string 'null'.
     This would especially confuse iterators, causing out-of-memory errors, if null is being returned to mark the end of an
     iteration.  */
  return 0;
}


static void traceexec (lua_State *L, const Instruction *pc) {
  lu_byte mask = L->hookmask;
  const Instruction *oldpc = L->savedpc;
  L->savedpc = pc;
  if ((mask & LUA_MASKCOUNT) && L->hookcount == 0) {  /* 5.1.3 patch */
    resethookcount(L);
    luaD_callhook(L, LUA_HOOKCOUNT, -1);
  }
  if (mask & LUA_MASKLINE) {
    Proto *p = ci_func(L->ci)->l.p;
    int npc = pcRel(pc, p);
    int newline = getLine(p, npc);
    /* call linehook when enter a new function, when jump back (loop),
       or when enter a new line */
    if (npc == 0 || pc <= oldpc || newline != getLine(p, pcRel(oldpc, p)))
      luaD_callhook(L, LUA_HOOKLINE, newline);
  }
}


static void callTMres (lua_State *L, StkId res, const TValue *f,
                        const TValue *p1, const TValue *p2) {
  ptrdiff_t result = savestack(L, res);
  setobj2s(L, L->top, f);       /* push function */
  setobj2s(L, L->top + 1, p1);  /* 1st argument */
  setobj2s(L, L->top + 2, p2);  /* 2nd argument */
  luaD_checkstack(L, 3);
  L->top += 3;
  luaD_call(L, L->top - 3, 1, 0);  /* changed to no GC to prevent stack corruption, 2.34.3 */
  res = restorestack(L, result);
  L->top--;
  setobjs2s(L, res, L->top);
}


static void callTM (lua_State *L, const TValue *f, const TValue *p1,
                    const TValue *p2, const TValue *p3) {
  setobj2s(L, L->top, f);       /* push function */
  setobj2s(L, L->top + 1, p1);  /* 1st argument */
  setobj2s(L, L->top + 2, p2);  /* 2nd argument */
  setobj2s(L, L->top + 3, p3);  /* 3th argument */
  luaD_checkstack(L, 4);
  L->top += 4;
  luaD_call(L, L->top - 4, 0, 0);  /* changed to no GC to prevent stack corruption, 2.34.3 */
}


static void callTMdotted (lua_State *L, StkId res, const TValue *f,  /* 2.17.8 */
                        const TValue *p1, const TValue *p2, const TValue *p3) {
  ptrdiff_t result = savestack(L, res);
  setobj2s(L, L->top, f);       /* push function */
  setobj2s(L, L->top + 1, p1);  /* 1st argument */
  setobj2s(L, L->top + 2, p2);  /* 2nd argument */
  setobj2s(L, L->top + 3, p3);  /* 3rd argument */
  luaD_checkstack(L, 4);
  L->top += 4;
  luaD_call(L, L->top - 4, 1, 0);  /* changed to no GC to prevent stack corruption, 2.34.3 */
  res = restorestack(L, result);
  L->top--;
  setobjs2s(L, res, L->top);
}


#define Protect(x)   { L->savedpc = pc; {x;}; base = L->base; }

static inline int aux_isintegralfast (double x) {
  /* catches infinity, NaN, and overflow, too. 7.5.12 */
  if (!(x >= 1.0 && x <= LUA_MAXINTEGER)) return 0;
  return x == (double)((lua_Integer)x);
}

LUAI_FUNC void luaV_gettable (lua_State *L, const TValue *t, TValue *key, StkId val) {
  int loop;
  for (loop=0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;
    if (ttistable(t)) {  /* `t' is a table ? */
      const TValue *res = NULL;
      Table *h = hvalue(t);
      if (ttisnumber(key)) {  /* 7.3.10 change */
        lua_Number x = nvalue(key);
        if (aux_isintegralfast(x)) {
          lua_Integer ix = (lua_Integer)x - 1;
          if ((size_t)ix < (size_t)h->sizearray) {
            /* we do not return here for res might be `null`, try mt later on if existing */
            res = &h->array[ix];
          }
        }
      }
      if (!res) res = luaH_get(h, key);  /* do a primitive get */
      /* There is no gain in speed to explicitly check for string or integer keys here, by "inlining" respective
         code from ltables.c. It actually gives a speed penalty of around 8 %, so we don't do it. */
      if (ttisnotnil(res) ||  /* result is no nil? */
          (tm = fasttm(L, h->metatable, TM_INDEX)) == NULL) {  /* or no TM? */
        setobj2s(L, val, res);
        return;
      }
    }
    else if (ttisuset(t)) {  /* `t' is a set ? */
      UltraSet *h = usvalue(t);
      if (agnUS_get(h, key)) {
        setbvalue(val, 1);
        return;
      }
      else if ((tm = fasttm(L, h->metatable, TM_INDEX)) == NULL) {
        setbvalue(val, 0);
        return;
      }
    }
    else if (ttisseq(t) && ttisnumber(key)) {
      const TValue *res;
      size_t pos;
      Seq *h;
      h = seqvalue(t);
      pos = tools_posrelat(nvalue(key), h->size);  /* 1.11.8 */
      res = ((pos > h->size) || pos < 1) ? NULL : seqitem(h, pos - 1);  /* 7.3.9 `1 % tweak` */
      if (res == NULL) {
        if ((tm = fasttm(L, h->metatable, TM_INDEX)) == NULL) {
          luaG_runerror(L, "Error: sequence index %d out of range.", (int)nvalue(key));
          return;
        }
      }
      else {
        setobj2s(L, val, res);
        return;
      }
    }
    else if (ttisstring(t) && ttisnumber(key)) {  /* 4.3.2 extension to support OOP methods */
      size_t len;
      int index;
      const char *str;
      str = svalue(t);  /* 0.20.1 patch */
      len = tsvalue(t)->len;  /* 2.16.1 patch, do not use strlen since it quits with the first embedded \0 */
      if (len == 0) {
        setfailvalue(val);
        return;
      }
      index = nvalue(key);
      if (index < 0) index = len + index + 1;
      if (index < 1 || index > len)
        luaG_runerror(L, "Error: index %d out range.", index);
      setsvalue(L, val, luaS_newlstr(L, str + index - 1, 1));
      return;
    }
    else if (ttispair(t) && ttisnumber(key)) {
      const TValue *res;
      Pair *h;
      h = pairvalue(t);
      res = agnPair_geti(h, nvalue(key));
      if (res == NULL) {
        if ((tm = fasttm(L, h->metatable, TM_INDEX)) == NULL) {
          luaG_runerror(L, "Error: pair index %d out of range.", (int)nvalue(key));
          return;
        }
      } else {
        setobj2s(L, val, res);
        return;
      }
    }
    else if (ttisreg(t) && ttisnumber(key)) {
      const TValue *res;
      size_t pos;
      Reg *h;
      h = regvalue(t);
      pos = tools_posrelat(nvalue(key), h->top);
      res = ((pos > h->top) || pos < 1) ? NULL : regitem(h, pos - 1);  /* 7.3.9 `1 % tweak */
      if (res == NULL) {
        if ((tm = fasttm(L, h->metatable, TM_INDEX)) == NULL) {
          luaG_runerror(L, "Error: register index %d out of range.", (int)nvalue(key));  /* 2.4.0 */
          return;
        }
      } else {
        setobj2s(L, val, res);
        return;
      }
    }
    else if (ttisfunction(t) && ttisnumber(key)) {
      /* read an upvalue from a closure, either defined in C or Agena. 2.36.2
         Example for a minimal Lua closure named `cl`:
         f := proc() is
            local a, b, c := 10, 20, 30;
            return proc() is return a, b, c end;
         end;
         cl := f(); */
      const TValue *res;
      size_t pos;
      lu_byte nupvalues, isC;
      Closure *fcl = clvalue(t);
      isC = fcl->c.isC;
      if ((nupvalues = isC ? fcl->c.nupvalues : fcl->l.nupvalues) == 0) {
        luaG_runerror(L, "Error: trying to index a procedure with no upvalues.");
      }
      pos = tools_posrelat(nvalue(key), nupvalues) - 1;
      if (pos < nupvalues) {  /* try a primitive get */
        res = isC ? &fcl->c.upvalue[pos] : fcl->l.upvals[pos]->v;
        setobj2s(L, val, res);
        return;
      }
      if ((tm = fasttm(L, isC ? fcl->c.metatable : fcl->l.metatable, TM_INDEX)) == NULL) {
        luaG_runerror(L, "Error: index %d out of range.", (int)nvalue(key));
        return;
      }  /* else call __index metamethod */
    }
    /* else will try the tag method */
    else if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_INDEX)))
      agnG_indexerror(L, t, key);  /* 2.2.4 */
    if (ttisfunction(tm)) {
      callTMres(L, val, tm, t, key);
      return;
    }
    t = tm;  /* else repeat with `tm' */
  }
  luaG_runerror(L, "Error: loop in gettable.");
}


LUAI_FUNC void luaV_settable (lua_State *L, const TValue *t, TValue *key, StkId val) {
  int loop;
  TValue temp;
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;
    if (ttistable(t)) {  /* `t' is a table ? */
      Table *h = hvalue(t);
      if ((tm = fasttm(L, h->metatable, TM_NEWINDEX)) == NULL) {  /* or no TM? */
        TValue *oldval;
        oldval = luaH_set(L, h, key);  /* do a primitive set */
        setobj2t(L, oldval, val);
        h->flags = 0;  /* Lua 5.1.4 patch 10 / 2.29.3 */
        luaC_barriert(L, h, val);
        return;
      }
      /* else will try the tag method */
    }
    else if (ttisseq(t) && ttisnumber(key)) {  /* `t' is a sequence ? */
      Seq *h = seqvalue(t);
      if ((tm = fasttm(L, h->metatable, TM_WRITEINDEX)) == NULL) {
        if (agnSeq_seti(L, h, nvalue(key), val) == 0) {  /* 0.28.1 */
          luaG_runerror(L, "Error: sequence index %d out of range.", (int)nvalue(key));  /* 2.2.4 */
          break;
        }
        h->flags = 0;  /* Lua 5.1.4 patch 10 / 4.6.4 */
        if (!ttisnil(val))  /* we have already barrier'd null in call to lseq.c/purge */
          luaC_barrierseq(L, h, val);
        return;
      }
    }
    else if (ttispair(t) && ttisnumber(key)) {  /* `t' is a pair ? */
      Pair *h = pairvalue(t);
      if ((tm = fasttm(L, h->metatable, TM_WRITEINDEX)) == NULL) {
        if (agnPair_seti(L, h, nvalue(key), val) == 0) {
          luaG_runerror(L, "Error: pair index %d out of range.", (int)nvalue(key));
          break;
        }
        h->flags = 0;  /* Lua 5.1.4 patch 10 / 4.6.4 */
        return;
      }
    }
    else if (ttisreg(t) && ttisnumber(key)) {  /* `t' is a register ? */
      Reg *h = regvalue(t);
      if ((tm = fasttm(L, h->metatable, TM_WRITEINDEX)) == NULL) {
        if (agnReg_seti(L, h, nvalue(key), val) == 0) {
          luaG_runerror(L, "Error: register index %d out of range.", (int)nvalue(key));  /* 2.4.0 */
          break;
        }
        h->flags = 0;  /* Lua 5.1.4 patch 10 / 4.6.4 */
        luaC_barrierreg(L, h, val);
        return;
      }
    }
    else if (ttisuset(t)) {  /* new 2.9.1 */
      UltraSet *h = usvalue(t);
      if ((tm = fasttm(L, h->metatable, TM_WRITEINDEX)) == NULL) {
        if (!ttisnil(val))  /* 0.31.5 */
          agnUS_set(L, usvalue(t), key);
        else
          agnUS_delete(L, usvalue(t), key);
        h->flags = 0;  /* Lua 5.1.4 patch 10 / 4.6.4 */
        luaC_barrierset(L, h, key);
        return;
      }
    }
    /* else will try the tag method */
    else if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_WRITEINDEX))) {
      agnG_indexerror(L, t, key);  /* 2.2.4 */
    }
    if (ttisfunction(tm)) {
      callTM(L, tm, t, key, val);
      return;
    }
    /* else repeat with `tm', 0.24.2 patch */
    setobj(L, &temp, tm);  /* avoid pointing inside table (may rehash) */
    t = &temp;
  }
  luaG_runerror(L, "Error: loop in settable.");
}


LUAI_FUNC lua_Number agenaV_bottomindex (lua_State *L, Table *h) {  /* 1.1.0, tuned 1.3.3 */
  const TValue *k;
  Node *gn;
  lua_Number key_val, min_key;
  int i;
  lua_lock(L);
  min_key = HUGE_VAL;
  for (i=0; i < h->sizearray; i++) {
    if (ttisnotnil(&h->array[i])) {  /* array part is filled */
      min_key = i + 1;
      break;  /* now try the hash part */
    }
  }
  if (min_key != HUGE_VAL) i = h->sizearray;
  for (i -= h->sizearray; i < sizenode(h); i++) {
    gn = gnode(h, i);
    k = key2tval(gn);
    if (ttisnumber(k) && !ttisnil(gval(gn))) {  /* key is a number and is assigned a non-null value ... */
      key_val = nvalue(k);
      if (tools_isint(key_val) && key_val < min_key) {  /* and an integer: is there a smaller key in hash part ?  2.3.0 RC 2 eCS fix */
        min_key = key_val;
      }
    }
  }
  lua_unlock(L);
  return min_key;
}


LUAI_FUNC lua_Number agenaV_topindex (lua_State *L, Table *h) {  /* 1.1.0, tuned 1.3.3 */
  const TValue *k;
  Node *gn;
  lua_Number key_val, max_key;
  int i;
  lua_lock(L);
  max_key = -HUGE_VAL;
  for (i=h->sizearray - 1; i > -1; i--) {
    if (ttisnotnil(&h->array[i])) {  /* array part is filled */
      max_key = i + 1;
      break;  /* now try the hash part */
    }
  }
  for (i=0; i < sizenode(h); i++) {
    gn = gnode(h, i);
    k = key2tval(gn);
    if (ttisnumber(k) && !ttisnil(gval(gn))) {  /* key is a number and is assigned a non-null value ... */
      key_val = nvalue(k);
      if (tools_isint(key_val) && key_val > max_key) {  /* and an integer: is there a smaller key in hash part ? 2.3.0 RC 2 eCS fix */
        max_key = key_val;
      }
    }
  }
  lua_unlock(L);
  return max_key;
}


LUAI_FUNC void agenaV_bottom (lua_State *L, const TValue *t, StkId val) {  /* 0.24.0 */
  lua_lock(L);
  if (ttisseq(t)) {
    Seq *h;
    h = seqvalue(t);
    if (h->size != 0) {
      setobj2s(L, val, agnSeq_geti(h, 1));
    } else
      setnilvalue(val);
  } else if (ttistable(t)) {  /* Agena 1.1.0 */
      Table *h;
      h = hvalue(t);
      setobj2s(L, val, luaH_getnum(h, agenaV_bottomindex(L, h)));
  } else if (ttisreg(t)) {  /* 2.3.0 RC 3 */
    Reg *h;
    h = regvalue(t);
    if (h->top != 0) {
      setobj2s(L, val, agnReg_geti(h, 1));
    } else
      setnilvalue(val);
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.", "bottom",
      luaT_typenames[(int)ttype(t)]);
  }
  lua_unlock(L);
}


LUAI_FUNC lua_Number agenaV_top (lua_State *L, Table *h, StkId val) {  /* 0.24.0, modified 2.2.0 RC 2 */
  lua_Number topindex;
  lua_lock(L);
  topindex = agenaV_topindex(L, h);
  setobj2s(L, val, luaH_getnum(h, topindex));
  lua_unlock(L);
  return topindex;
}


LUAI_FUNC void agenaV_topsequence (lua_State *L, Seq *h, StkId val) {  /* 2.2.0 RC 2, it seems to be 8 % faster than a sequence version of non-void agenaV_top */
  lua_lock(L);
  if (h->size != 0) {
    setobj2s(L, val, agnSeq_geti(h, h->size));
  } else {
    setnilvalue(val);
  }
  lua_unlock(L);
}


LUAI_FUNC void agenaV_topregister (lua_State *L, Reg *h, StkId val) {  /* 2.3.0 RC 3 */
  lua_lock(L);
  if (h->top != 0) {
    setobj2s(L, val, agnReg_geti(h, h->top));
  } else {
    setnilvalue(val);
  }
  lua_unlock(L);
}


/* used by `element IN table/set/sequence/register/string`
   iterates over an object and checks whether the given argument is an element (a value,
   not a key) in this object;
   October 14, 2007 / November 23, 2007; patched Dec 08, 2007; tweaked January 04, 2008;
   tuned 1.3.3 */

LUAI_FUNC void agenaV_in (lua_State *L, const TValue *value, const TValue *what, StkId idx) {
  lua_lock(L);
  switch (ttype(what)) {
    case LUA_TTABLE: {
      Table *t;
      const TValue *tm, *val;
      int i;
      t = hvalue(what);
      if ((tm = fasttm(L, t->metatable, TM_IN)) == NULL) {  /* no metamethod ? */
        for (i=0; i < t->sizearray; i++) {
          val = &t->array[i];
          if (ttisnotnil(val) && equalobj(L, value, val)) {
            setbvalue(idx, 1);
            lua_unlock(L);
            return;
          }
        }
        for (i -= t->sizearray; i < sizenode(t); i++) {  /* added 0.8.0 */
          val = gval(gnode(t, i));
          if (ttisnotnil(val) && equalobj(L, value, val)) {
            setbvalue(idx, 1);
            lua_unlock(L);
            return;
          }
        }
        setbvalue(idx, 0);
      } else if (ttisfunction(tm))
        call_binTM(L, value, what, idx, TM_IN);
      break;
    }
    case LUA_TSTRING: {
      if (ttisstring(value)) {
        ptrdiff_t pos;  /* 2.26.0 fix for 64-bit */
        char *search;
        const char *str = svalue(what);
        const char *sub = svalue(value);
        search = strstr(str, sub);
        if (!search)  /* 2.29.3: catch NULL situation. */
          setnilvalue(idx);
        else {
          pos = search - (char *)str + 1;  /* 2.26.0 fix for 64-bit */
          if (pos < 1 || tsvalue(value)->len == 0)  /* 2.16.1 patch, do not use strlen since it quits with the first embedded \0. */
            setnilvalue(idx);
          else
            setnvalue(idx, pos);
        }
      } else {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": wrong type of argument.", "in");
      }
      break;
    }
    case LUA_TSET: {
      const TValue *tm;
      UltraSet *s;
      s = usvalue(what);
      if ((tm = fasttm(L, s->metatable, TM_IN)) == NULL) { /* no metamethod ? */
        if (agnUS_get(s, value)) {  /* left value is atomic: (complex) numbers, booleans, strings and null ? */
          setbvalue(idx, 1);
        } else {
          /* is left value a structure ? -> iterate right set and try to find it there */
          if (ttype(value) > LUA_TTHREAD) {
            int i;
            for (i=0; i < sizenode(s); i++) {
              if (ttisnotnil(key2tval(gnode(s, i)))) {
                if (equalobj(L, value, key2tval(gnode(s, i)))) {
                  setbvalue(idx, 1);
                  lua_unlock(L);
                  return;
                }
              }
            }  /* of for i */
          }
          setbvalue(idx, 0);
        }
      } else if (ttisfunction(tm))
        call_binTM(L, value, what, idx, TM_IN);
      break;
    }
    case LUA_TSEQ: {
      const TValue *tm, *val;
      Seq *h = seqvalue(what);
      if ((tm = fasttm(L, h->metatable, TM_IN)) == NULL) {  /* no metamethod ? */
        int i;  /* 2.10.2 improvement */
        for (i=0; i < h->size; i++) {
          val = seqitem(h, i);
          if (ttisnotnil(val) && equalobj(L, value, val)) {
            setbvalue(idx, 1);
            lua_unlock(L);
            return;
          }
        }
        setbvalue(idx, 0);
      } else if (ttisfunction(tm))
        call_binTM(L, value, what, idx, TM_IN);
      break;
    }
    case LUA_TPAIR: {
      const TValue *tm;
      Pair *h = pairvalue(what);
      if ((tm = fasttm(L, h->metatable, TM_IN)) == NULL) {  /* no metamethod ? */
        if (ttisnumber(value) && ttisnumber(pairitem(h, 0)) && ttisnumber(pairitem(h, 1)) ) {
          lua_Number x = nvalue(value);
          setbvalue(idx, x >= nvalue(pairitem(h, 0)) && x <= nvalue(pairitem(h, 1)) );
        } else
          setfailvalue(idx);
      } else if (ttisfunction(tm))
        call_binTM(L, value, what, idx, TM_IN);
      break;
    }
    case LUA_TUSERDATA: case LUA_TFUNCTION: {  /* 2.3.0 RC 3, try metamethod; 2.36.2 extension for functions */
      if (!call_binTM(L, value, what, idx, TM_IN))
        luaG_typeerror(L, what, "search in");
      break;
    }
    case LUA_TREG: {
      const TValue *tm, *val;
      Reg *h = regvalue(what);
      if ((tm = fasttm(L, h->metatable, TM_IN)) == NULL) {  /* no metamethod ? */
        int i;  /* 2.10.2 improvement */
        for (i=0; i < h->top; i++) {
          val = regitem(h, i);
          if (equalobj(L, value, val)) {  /* 2.12.2 fix */
            setbvalue(idx, 1);
            lua_unlock(L);
            return;
          }
        }
        setbvalue(idx, 0);
      } else if (ttisfunction(tm))
        call_binTM(L, value, what, idx, TM_IN);
      break;
    }
    default: {
      lua_unlock(L);
      luaG_runerror(L, "Error: in " LUA_QS ": wrong type of argument.", "in");
    }
  }
  lua_unlock(L);
}


LUAI_FUNC void agenaV_notin (lua_State *L, const TValue *value, const TValue *what, StkId idx) {
  lua_lock(L);
  switch (ttype(what)) {
    case LUA_TTABLE: {
      Table *t;
      const TValue *tm, *val;
      int i;
      t = hvalue(what);
      if ((tm = fasttm(L, t->metatable, TM_NOTIN)) == NULL) {  /* no metamethod ? */
        for (i=0; i < t->sizearray; i++) {
          val = &t->array[i];
          if (ttisnotnil(val) && equalobj(L, value, val)) {
            setbvalue(idx, 0);
            lua_unlock(L);
            return;
          }
        }
        for (i -= t->sizearray; i < sizenode(t); i++) {  /* added 0.8.0 */
          val = gval(gnode(t, i));
          if (ttisnotnil(val) && equalobj(L, value, val)) {
            setbvalue(idx, 0);
            lua_unlock(L);
            return;
          }
        }
        setbvalue(idx, 1);
      } else if (ttisfunction(tm))
        call_binTM(L, value, what, idx, TM_NOTIN);
      break;
    }
    case LUA_TSTRING: {
      if (ttisstring(value)) {
        char *search;
        ptrdiff_t pos;  /* 2.26.0 fix for 64-bit */
        const char *str = svalue(what);
        const char *sub = svalue(value);
        search = strstr(str, sub);
        if (!search) { /* 2.29.3: catch NULL situation */
          setbvalue(idx, 1);
        } else {
          pos = search - (char *)str + 1;
          setbvalue(idx, pos < 1 || tsvalue(value)->len == 0);  /* do not use strlen since it quits with the first embedded \0. */
        }
      } else {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": wrong type of argument.", "notin");
      }
      break;
    }
    case LUA_TSET: {
      const TValue *tm;
      UltraSet *s;
      s = usvalue(what);
      if ((tm = fasttm(L, s->metatable, TM_NOTIN)) == NULL) { /* no metamethod ? */
        if (agnUS_get(usvalue(what), value)) {  /* left value is atomic: (complex) numbers, booleans, strings and null ? */
          setbvalue(idx, 0);
        } else {
          /* is left value a structure ? -> iterate right set and try to find it there */
          if (ttype(value) > LUA_TTHREAD) {
            int i;
            for (i=0; i < sizenode(s); i++) {
              if (ttisnotnil(key2tval(gnode(s, i)))) {
                if (equalobj(L, value, key2tval(gnode(s, i)))) {
                  setbvalue(idx, 0);
                  lua_unlock(L);
                  return;
                }
              }
            }  /* of for i */
          }
          setbvalue(idx, 1);
        }
      } else if (ttisfunction(tm))
        call_binTM(L, value, what, idx, TM_NOTIN);
      break;
    }
    case LUA_TSEQ: {
      const TValue *tm, *val;
      Seq *h = seqvalue(what);
      if ((tm = fasttm(L, h->metatable, TM_NOTIN)) == NULL) {  /* no metamethod ? */
        int i;  /* 2.10.2 improvement */
        for (i=0; i < h->size; i++) {
          val = seqitem(h, i);
          if (ttisnotnil(val) && equalobj(L, value, val)) {
            setbvalue(idx, 0);
            lua_unlock(L);
            return;
          }
        }
        setbvalue(idx, 1);
      } else if (ttisfunction(tm))
        call_binTM(L, value, what, idx, TM_NOTIN);
      break;
    }
    case LUA_TPAIR: {
      const TValue *tm;
      Pair *h = pairvalue(what);
      if ((tm = fasttm(L, h->metatable, TM_NOTIN)) == NULL) {  /* no metamethod ? */
        if (ttisnumber(value) && ttisnumber(pairitem(h, 0)) && ttisnumber(pairitem(h, 1)) ) {
          lua_Number x = nvalue(value);
          setbvalue(idx, !(x >= nvalue(pairitem(h, 0)) && x <= nvalue(pairitem(h, 1))) );
        } else
          setfailvalue(idx);
      } else if (ttisfunction(tm))
        call_binTM(L, value, what, idx, TM_NOTIN);
      break;
    }
    case LUA_TUSERDATA: case LUA_TFUNCTION: {  /* 2.3.0 RC 3, try metamethod; 2.36.2 extension for functions */
      if (!call_binTM(L, value, what, idx, TM_NOTIN)) {
        lua_unlock(L);
        luaG_typeerrorx(L, idx, "search in", "notin");  /* 2.21.0 */
      }
      break;
    }
    case LUA_TREG: {
      const TValue *tm, *val;
      Reg *h = regvalue(what);
      if ((tm = fasttm(L, h->metatable, TM_NOTIN)) == NULL) {  /* no metamethod ? */
        int i;  /* 2.10.2 improvement */
        for (i=0; i < h->top; i++) {
          val = regitem(h, i);
          if (equalobj(L, value, val)) {  /* 2.12.2 fix */
            setbvalue(idx, 0);
            lua_unlock(L);
            return;
          }
        }
        setbvalue(idx, 1);
      } else if (ttisfunction(tm))
        call_binTM(L, value, what, idx, TM_NOTIN);
      break;
    }
    default: {
      lua_unlock(L);
      luaG_runerror(L, "Error: in " LUA_QS ": wrong type of argument.", "notin");
    }
  }
  lua_unlock(L);
}


LUAI_FUNC void agenaV_atendof (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisstring(idxb) && ttisstring(idxc)) {
    size_t p_len, s_len;
    const char *s, *p, *olds;
    char *pos;
    s = svalue(idxc);
    p = svalue(idxb);
    s_len = tsvalue(idxc)->len;  /* 7.9.2 7% tweak */
    p_len = tsvalue(idxb)->len;
    if (p_len == 0 || s_len == 0 || s_len <= p_len) {
      /* bail out if string or pattern is empty or if pattern is longer or equal in size */
      setnilvalue(idxa);
    } else {
      olds = s;
      s = s + s_len - p_len;
      pos = strstr(s, p);
      if (pos == s) {
        setnvalue(idxa, pos - olds + 1);
      } else {
        setnilvalue(idxa);
      }
      /* 2.5.2: do not nil idxb and idxc since it will confuse the stack ! */
    }
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error: in " LUA_QS ": strings expected, got %s, %s.", "atendof",
      luaT_typenames[(int)ttype(idxb)], luaT_typenames[(int)ttype(idxc)]);
  }
  lua_unlock(L);
}


/* Compare contents of two tables. October 16, 2007; patched Dec 26, 2007; extended Jan 12, 2008;
   patched October 26, 2008; tweaked January 04, 2008; patched February 05, 2014
   mode = 0: only check for a SUBSET b;
   mode = 1: check a = b and b = a
   mode = 2: check a SUBSET b and a <> b = XSUBSET */

LUAI_FUNC int agenaV_comptables (lua_State *L, Table *a, Table *b, int mode) {
  Table *t1, *t2, *swap;
  TValue *t1val, *t2val;
  Charbuf *seen;  /* flags all the elements in b that have been checked to be equal so that we
    do not check them again with the second pass */
  int i, j, oldi, oldj, flag, pass, traversed;
  t1 = a; t2 = b;
  pass = 0;
  traversed = 0;  /* has array or hash part been traversed ?  2.1.3 patch */
  flag = 1;
  seen = (mode == 0) ? NULL : bitfield_new(L, (t2->sizearray == 0) ? 1 : t2->sizearray, 0);
startofloop:
  L->top++;  /* leave this increment here, otherwise there will be crashes, eg. [[1]] = [[1, 2]] */
  checkstack_bitfield(L, seen, "Error, stack overflow in VM function ", "agenaV_comptables");
  api_check(L, L->top < L->ci->top);
  i = -1;
  oldi = i;
  j = -1;
  oldj = j;  /* save value of j for consecutive iterations of 2nd table */
  for (i++; i < t1->sizearray; i++) {
    if (ttisnotnil(&t1->array[i])) {
      flag = 0;
      t1val = &t1->array[i];
      if (mode == 0) {  /* a subset b */
        for (j++; j < t2->sizearray; j++) {
          if (ttisnotnil(&t2->array[j])) {
            if (!traversed) traversed = 1;  /* 2.1.3 patch */
            t2val = &t2->array[j];
            if (equalobj(L, t1val, t2val)) {
              flag = 1;
              j = t2->sizearray;  /* set j properly so that next loop starts with correct value */
              break;  /* quit this loop if element has been found */
            }
          }
        }  /* of for j */
      } else {
        for (j++; j < t2->sizearray; j++) {
          if (ttisnotnil(&t2->array[j])) {
            if (!traversed) traversed = 1;  /* 2.1.3 patch */
            t2val = &t2->array[j];
            if ((pass && bitfield_get(L, seen, i + 1)) || equalobj(L, t1val, t2val)) {
              flag = 1;
              if (!pass) bitfield_set(L, seen, j + 1);
              j = t2->sizearray;  /* set j properly so that next loop starts with correct value */
              break;  /* quit this loop if element has been found */
            }
          }
        }  /* of for j */
      }
      for (j -= t2->sizearray; j < sizenode(t2); j++) {
        if (ttisnotnil(gval(gnode(t2, j)))) {
          if (!traversed) traversed = 1;  /* 2.1.3 patch */
          t2val = gval(gnode(t2, j));
          if (equalobj(L, t1val, t2val)) {
            flag = 1;
            break;  /* quit this loop if element has been found */
          }
        }
      }  /* of for j */
      j = oldj;  /* reset pointer for 2nd table */
      if (!flag) break;  /* if an element was _not_ found exit for i loop completely */
    }  /* of if */
  }  /* of for i*/
  if (traversed && !flag)  /* 2.1.3 patch; with an equality or subset check, if a member in the array part of a
    has not been found in either the array or hash part of b, then do not allow to check whether an element in
    the hash part of _a_ has been found in b, for in this case the check would be positive, e.g. formerly,
    [4, 1, 'a'~2] = [5, 1, 'a'~2] -> true. */
    goto endofloop;
  i = oldi + 1 + t1->sizearray;  /* reset i to upper bound in case element was not found */
  for (i -= t1->sizearray; i < sizenode(t1); i++) {  /* added 0.8.0 */
    if (ttisnotnil(gval(gnode(t1, i)))) {
      flag = 0;
      t1val = gval(gnode(t1, i));
      for (j++; j < t2->sizearray; j++) {
        if (ttisnotnil(&t2->array[j])) {
          t2val = &t2->array[j];
          if (equalobj(L, t1val, t2val)) {
            flag = 1;
            j = t2->sizearray;  /* set j properly so that next loop starts with correct value */
            break;  /* quit inner loop if element has been found */
          }
        }
      } /* of for j */
      for (j -= t2->sizearray; j < sizenode(t2); j++) {
        if (ttisnotnil(gval(gnode(t2, j)))) {
          t2val = gval(gnode(t2, j));
          if (equalobj(L, t1val, t2val)) {
            flag = 1;
            break;  /* quit inner loop if element has been found */
          }
        }
      } /* of for j */
      j = oldj;  /* reset pointer for 2nd table */
      if (!flag) break;  /* if an element was _not_ found exit loop completely */
    } /* of if */
  } /* of for i*/
endofloop:  /* 2.1.3 patch */
  pass++;
  L->top--;
  /* in equality operation (mode = 1), check 2nd table against 1st table;
     condition: all elements in 1st table were found in 2nd table */
  if (pass == 1) {
    if (mode > 0 && flag == 1) {
      swap = t2;
      t2 = t1;
      t1 = swap;
      goto startofloop;
    }
  } else if (mode == 2) flag = !flag;
  bitfield_free(seen);
  return flag;
}


LUAI_FUNC int agenaV_comptablesonebyone (lua_State *L, Table *t1, Table *t2) {
  int i, j, flag;
  flag = 1;
  for (i=0, j=0; i < t1->sizearray && j < t2->sizearray; i++, j++) {
    if (ttisnotnil(&t1->array[i]) && ttisnotnil(&t2->array[j])) {
      if (!equalobjonebyone(L, &t1->array[i], &t2->array[j])) {
        flag = 0;
        break;
      }
    }
    else if (ttisnotnil(&t1->array[i]) || ttisnotnil(&t2->array[j])) {  /* 0.31.4 patch */
      flag = 0;
      break;
    }
  }
  if (flag) {  /* see whether there are surplus values */
    for (; i < t1->sizearray; i++) {
      if (ttisnotnil(&t1->array[i])) {
        flag = 0;
        break;
      }
    }
    if (flag) {
      for (; j < t2->sizearray; j++) {
        if (ttisnotnil(&t2->array[j])) {
          flag = 0;
          break;
        }
      }
    }
  }
  if (flag) {  /* if array entries were all equal, compare hash part */
    Node *t1node, *t2node;
    TValue *t1key, *t2key;
    const TValue *t1val, *t2val;
    for (i -= t1->sizearray; i < sizenode(t1); i++) {
      t1node = gnode(t1, i);
      if (ttisnotnil(gval(t1node))) {
        t1key = key2tval(t1node);
        t1val = gval(t1node);
        t2val = luaH_get(t2, t1key);
        if (!equalobjonebyone(L, t1val, t2val)) {
          flag = 0;
          break;
        }
      }
    }
    if (flag) {
      for (j -= t2->sizearray; j < sizenode(t2); j++) {
        t2node = gnode(t2, j);
        if (ttisnotnil(gval(t2node))) {
          t2key = key2tval(t2node);
          t2val = gval(t2node);
          t1val = luaH_get(t1, t2key);
          if (!equalobjonebyone(L, t2val, t1val)) {
            flag = 0;
            break;
          }
        }
      }
    }
  }
  return flag;
}


LUAI_FUNC int agenaV_approxcomptablesonebyone (lua_State *L, Table *t1, Table *t2) {  /* 2.1.4 */
  int i, j, flag;
  flag = 1;
  for (i=0, j=0; i < t1->sizearray && j < t2->sizearray; i++, j++) {
    if (ttisnotnil(&t1->array[i]) && ttisnotnil(&t2->array[j])) {
      if (!approxequalobjonebyone(L, &t1->array[i], &t2->array[j])) {
        flag = 0;
        break;
      }
    }
    else if (ttisnotnil(&t1->array[i]) || ttisnotnil(&t2->array[j])) {  /* 0.31.4 patch */
      flag = 0;
      break;
    }
  }
  if (flag) {  /* see whether there are surplus values */
    for (; i < t1->sizearray; i++) {
      if (ttisnotnil(&t1->array[i])) {
        flag = 0;
        break;
      }
    }
    if (flag) {
      for (; j < t2->sizearray; j++) {
        if (ttisnotnil(&t2->array[j])) {
          flag = 0;
          break;
        }
      }
    }
  }
  if (flag) {  /* if array entries were all equal, compare hash part */
    Node *t1node, *t2node;
    TValue *t1key, *t2key;
    const TValue *t1val, *t2val;
    for (i -= t1->sizearray; i < sizenode(t1); i++) {
      t1node = gnode(t1, i);
      if (ttisnotnil(gval(t1node))) {
        t1key = key2tval(t1node);
        t1val = gval(t1node);
        t2val = luaH_get(t2, t1key);
        if (!approxequalobjonebyone(L, t1val, t2val)) {
          flag = 0;
          break;
        }
      }
    }
    if (flag) {
      for (j -= t2->sizearray; j < sizenode(t2); j++) {
        t2node = gnode(t2, j);
        if (ttisnotnil(gval(t2node))) {
          t2key = key2tval(t2node);
          t2val = gval(t2node);
          t1val = luaH_get(t1, t2key);
          if (!approxequalobjonebyone(L, t2val, t1val)) {
            flag = 0;
            break;
          }
        }
      }
    }
  }
  return flag;
}


/* Compare contents of two UltraSets; April 08, 2008
   mode = 0: only check for a SUBSET b;
   mode = 1: check a = b and b = a
   mode = 2: check a XSUBSET b and a <> b.

   We will NOT use bitfields here as this slows down the operators, surprisingly. */

LUAI_FUNC int agenaV_compusets (lua_State *L, UltraSet *a, UltraSet *b, int mode) {
  UltraSet *t1, *t2, *swap;
  TValue *val;
  int i, j, flag, pass;
  t1 = a; t2 = b;
  pass = 0;
  flag = 1;
label:
  for (i=0; i < sizenode(t1); i++) {  /* added 0.8.0 */
    if (ttisnotnil(glkey(gnode(t1, i)))) {
      /* try a primitive comparison */
      val = key2tval(gnode(t1, i));
      /* check whether the element in a (val) is part of b */
      if ((flag = agnUS_get(t2, val))) {  /* left value is atomic: (complex) numbers, booleans, strings and null ? */
        continue;
      }
      if (ttype(val) > LUA_TTHREAD) {  /* is left value a structure ? -> iterate right set and try to find it there */
        for (j=0; j < sizenode(t2); j++) {
          if (ttisnotnil(key2tval(gnode(t2, j)))) {
            if (equalobj(L, val, key2tval(gnode(t2, j)))) {  /* 7.6.1 extension */
              flag = 1;
              break;  /* quit this loop if element has been found */
            }
          }
        } /* of for j */
      }
      if (!flag) break;  /* if an element was _not_ found exit loop immediately */
    } /* of if */
  } /* of for i*/
  pass++;
  /* in equality operation (mode = 1), check 2nd table against 1st table;
     condition: all elements in 1st table were found in 2nd table */
  if (pass == 1) {
    if (mode > 0 && flag == 1) {
      swap = t2;
      t2 = t1;
      t1 = swap;
      goto label;
    }
  } else if (mode == 2) return !flag;
  return flag;
}


LUAI_FUNC int agenaV_approxcompusets (lua_State *L, UltraSet *a, UltraSet *b, int mode) {  /* 2.1.4 */
  UltraSet *t1, *t2, *swap;
  TValue *val;
  int i, j, flag, pass;
  t1 = a; t2 = b;
  pass = 0;
  flag = 1;
label:
  for (i=0; i < sizenode(t1); i++) {  /* added 0.8.0 */
    if (ttisnotnil(glkey(gnode(t1, i)))) {
      /* try a primitive comparison */
      val = key2tval(gnode(t1, i));
      if ((flag = agnUS_get(t2, val))) {  /* left value is atomic: (complex) numbers, booleans, strings and null ? */
        continue;
      }
      if (ttype(val) > LUA_TTHREAD) {  /* is left value a structure ? -> iterate right set and try to find it there */
        for (j=0; j < sizenode(t2); j++) {
          if (ttisnotnil(key2tval(gnode(t2, j)))) {
            if (approxequalobjonebyone(L, val, key2tval(gnode(t2, j)))) {
              flag = 1;
              break;  /* quit this loop if element has been found */
            }
          }
        } /* of for j */
      }
      if (!flag) break;  /* if an element was _not_ found exit loop immediately */
    } /* of if */
  } /* of for i*/
  pass++;
  /* in equality operation (mode = 1), check 2nd table against 1st table;
     condition: all elements in 1st table were found in 2nd table */
  if (pass == 1) {
    if (mode > 0 && flag == 1) {
      swap = t2;
      t2 = t1;
      t1 = swap;
      goto label;
    }
  } else if (mode == 2) return !flag;
  return flag;
}


/* Compare contents of two Sequences; June 10, 2008; tweaked January 04, 2008
   mode = 0: only check for a SUBSET b;
   mode = 1: check a = b and b = a
   mode = 2: check a XSUBSET b and a <> b */

LUAI_FUNC int agenaV_compseqs (lua_State *L, Seq *a, Seq *b, int mode) {
  Seq *t1, *t2, *swap;
  int i, j, flag, pass;
  TValue *val;
  Charbuf *seen;  /* flags all the elements in b that have been checked to be equal so that we
    do not check them again with the second pass */
  t1 = a; t2 = b;  /* seqs will be swapped later, so copy them */
  pass = 0;
  flag = 1;
  seen = (mode == 0) ? NULL : bitfield_new(L, (t2->size == 0) ? 1 : t2->size, 0);
label:
  checkstack_bitfield(L, seen, "Error, stack overflow in VM function ", "agenaV_compseqs");
  api_check(L, L->top < L->ci->top);
  if (mode == 0) {
    for (i=0; i < t1->size; i++) {
      flag = 0;
      val = seqitem(t1, i);  /* faster than putting it on the stack */
      /* check whether value at L->top is in sequence t2 */
      for (j=0; j < t2->size; j++) {
        if (equalobj(L, val, seqitem(t2, j))) {
          flag = 1;
          break;
        }
      }
      if (!flag) break; /* if an element was _not_ found exit loop immediately */
    }
  } else {  /* 7.5.17 20 % to 100 % speed-up */
    for (i=0; i < t1->size; i++) {
      flag = 0;
      val = seqitem(t1, i);  /* faster than putting it on the stack */
      /* check whether value at L->top is in sequence t2 */
      for (j=0; j < t2->size; j++) {
        if ((pass && bitfield_get(L, seen, i + 1)) || equalobj(L, val, seqitem(t2, j))) {
          flag = 1;
          if (!pass) bitfield_set(L, seen, j + 1);
          break;
        }
      }
      if (!flag) break; /* if an element was _not_ found exit loop immediately */
    }
  }
  pass++;
  /* in `=' equality operation (mode = 1), check 2nd table against 1st table;
     condition: all elements in 1st table were found in 2nd table */
  if (pass == 1) {
    if (mode > 0 && flag == 1) {
      swap = t2;
      t2 = t1;
      t1 = swap;
      goto label;
    }
  } else if (mode == 2) flag = !flag;
  bitfield_free(seen);
  return flag;
}


/* Compare contents of two Sequences sequentially; May 28, 2009, 0.22.0 */

LUAI_FUNC int agenaV_compseqsonebyone (lua_State *L, Seq *t1, Seq *t2) {
  int i, flag;
  flag = 1;
  checkstack(L, "Error, stack overflow in VM function ", "agenaV_compseqsonebyone");
  api_check(L, L->top < L->ci->top);
  if (t1->size != t2->size) return 0;
  for (i=0; i < t1->size; i++) {
    if (!equalobjonebyone(L, seqitem(t1, i), seqitem(t2, i))) {
      flag = 0;
      break;
    }
  }
  return flag;
}


LUAI_FUNC int agenaV_approxcompseqsonebyone (lua_State *L, Seq *t1, Seq *t2) {
  int i, flag;
  flag = 1;
  checkstack(L, "Error, stack overflow in VM function ", "agenaV_approxcompseqsonebyone");
  api_check(L, L->top < L->ci->top);
  if (t1->size != t2->size) return 0;
  for (i=0; i < t1->size; i++) {
    if (!approxequalobjonebyone(L, seqitem(t1, i), seqitem(t2, i))) {
      flag = 0;
      break;
    }
  }
  return flag;
}


/* Compare contents of two Sequences; June 10, 2008; tweaked January 04, 2008
   mode = 0: only check for a SUBSET b, even if a = b
   mode = 1: check a = b and b = a
   mode = 2: check a XSUBSET b and a <> b */

LUAI_FUNC int agenaV_compregs (lua_State *L, Reg *a, Reg *b, int mode) {
  Reg *t1, *t2, *swap;
  int i, j, flag, pass;
  TValue *val;
  Charbuf *seen;  /* flags all the elements in b that have been checked to be equal so that we
    do not check them again with the second pass */
  t1 = a; t2 = b;  /* seqs will be swapped later, so copy them */
  pass = 0;
  flag = 1;
  seen = (mode == 0) ? NULL : bitfield_new(L, (t2->top == 0) ? 1 : t2->top, 0);
label:
  checkstack_bitfield(L, seen, "Error, stack overflow in VM function ", "agenaV_compseqs");
  api_check(L, L->top < L->ci->top);
  if (mode == 0) {
    for (i=0; i < t1->top; i++) {
      flag = 0;
      val = regitem(t1, i);  /* faster than putting it on the stack */
      /* check whether value at L->top is in sequence t2 */
      for (j=0; j < t2->top; j++) {
        if (equalobj(L, val, regitem(t2, j))) {
          flag = 1;
          break;
        }
      }
      if (!flag) break; /* if an element was _not_ found exit loop immediately */
    }
  } else {  /* 7.5.17 20 % to 100 % speed-up */
    for (i=0; i < t1->top; i++) {
      flag = 0;
      val = regitem(t1, i);  /* faster than putting it on the stack */
      /* check whether value at L->top is in sequence t2 */
      for (j=0; j < t2->top; j++) {
        if ((pass && bitfield_get(L, seen, i + 1)) || equalobj(L, val, regitem(t2, j))) {
          flag = 1;
          if (!pass) bitfield_set(L, seen, j + 1);
          break;
        }
      }
      if (!flag) break; /* if an element was _not_ found exit loop immediately */
    }
  }
  pass++;
  /* in `=' equality operation (mode = 1), check 2nd table against 1st table;
     condition: all elements in 1st table were found in 2nd table */
  if (pass == 1) {
    if (mode > 0 && flag == 1) {
      swap = t2;
      t2 = t1;
      t1 = swap;
      goto label;
    }
  } else if (mode == 2) flag = !flag;
  bitfield_free(seen);
  return flag;
}



LUAI_FUNC int agenaV_compregsonebyone (lua_State *L, Reg *t1, Reg *t2) {
  int i, flag;
  flag = 1;
  checkstack(L, "Error, stack overflow in VM function ", "agenaV_compregsonebyone");
  api_check(L, L->top < L->ci->top);
  if (t1->top != t2->top) return 0;
  for (i=0; i < t1->top; i++) {
    if (!equalobjonebyone(L, regitem(t1, i), regitem(t2, i))) {
      flag = 0;
      break;
    }
  }
  return flag;
}


LUAI_FUNC int agenaV_approxcompregsonebyone (lua_State *L, Reg *t1, Reg *t2) {  /* 2.3.0 RC 3 */
  int i, flag;
  flag = 1;
  checkstack(L, "Error, stack overflow in VM function ", "agenaV_approxcompregsonebyone");
  api_check(L, L->top < L->ci->top);
  if (t1->top != t2->top) return 0;
  for (i=0; i < t1->top; i++) {
    if (!approxequalobjonebyone(L, regitem(t1, i), regitem(t2, i))) {
      flag = 0;
      break;
    }
  }
  return flag;
}


LUAI_FUNC int agenaV_comppairs (lua_State *L, Pair *a, Pair *b) {
  int i, flag;
  flag = 1;
  api_check(L, L->top < L->ci->top);
  checkstack(L, "Error, stack overflow in VM function ", "agenaV_comppairs");
  for (i=0; i < 2; i++) {
    const TValue *p = pairitem(a, i);
    const TValue *q = pairitem(b, i);
    if (!equalobj(L, p, q)) {
      flag = 0;
      break;
    }
  }
  return flag;
}


LUAI_FUNC int agenaV_approxcomppairs (lua_State *L, Pair *a, Pair *b) {  /* 2.1.4 */
  int i, flag;
  flag = 1;
  api_check(L, L->top < L->ci->top);
  checkstack(L, "Error, stack overflow in VM function ", "agenaV_approxcomppairs");
  for (i=0; i < 2; i++) {
    const TValue *p = pairitem(a, i);
    const TValue *q = pairitem(b, i);
    if (!approxequalobjonebyone(L, p, q)) {
      flag = 0;
      break;
    }
  }
  return flag;
}


/* table (X)SUBSET operator; October 15, 2007; extended to Ultra Sets April 08, 2008 */

LUAI_FUNC void agenaV_subset (lua_State *L, const TValue *tbl1, const TValue *tbl2, StkId idx, int mode) {
  if (ttistable(tbl1) && ttistable(tbl2)) {
    setbvalue(idx, agenaV_comptables(L, hvalue(tbl1), hvalue(tbl2), mode));
  } else if (ttisuset(tbl1) && ttisuset(tbl2)) {
    setbvalue(idx, agenaV_compusets(L, usvalue(tbl1), usvalue(tbl2), mode));
  } else if (ttisseq(tbl1) && ttisseq(tbl2)) {
    setbvalue(idx, agenaV_compseqs(L, seqvalue(tbl1), seqvalue(tbl2), mode));
  } else if (ttisreg(tbl1) && ttisreg(tbl2)) {  /* 2.3.0 RC 3 */
    setbvalue(idx, agenaV_compregs(L, regvalue(tbl1), regvalue(tbl2), mode));
  }
  else
    luaG_runerror(L, "Error in " LUA_QS ": tables, sets, sequences or registers expected.", (mode == 0) ? "subset" : "xsubset");
}


/* returns the number of assigned entries in the array and hash part of a table; this function does not get
   confused by `holes` in a table but is slower as the entire table will be traversed; October 15, 2007;
   tuned on Dec 09, 2007 */
LUAI_FUNC size_t agenaV_nops (lua_State *L, Table *t) {
  size_t i, c;
  i = c = 0; /* number of elements */
  /* iterate simple table of the form {v1, v2, ...}, not {1~v1, ...} ;
     if t is a dictionary, this loop is skipped */
  for (; i < t->sizearray; i++) {  /* 1.8.13 */
    if (ttisnotnil(&t->array[i])) c++;
  }
  /* iterate dictionary; if t is a simple table only one iteration is done */
  for (i -= t->sizearray; i < sizenode(t); i++) {
    if (ttisnotnil(gval(gnode(t, i)))) c++;
  }
  return c;
}


/* tuned 1.3.3, patched 7.1.2 */

LUAI_FUNC void agenaV_filled (lua_State *L, const TValue *tbl, StkId idx) {
  int i;
  if (ttistable(tbl)) {
    Table *t;
    t = hvalue(tbl);
    /* iterate simple table of the form {v1, v2, ...}, not {1~v1, ...} ;
       if t is a dictionary, this loop is also skipped */
    if (fasttm(L, t->metatable, TM_FILLED) == NULL) {  /* no metamethod ? new 2.27.4 */
      for (i=0; i < t->sizearray; i++) {
        if (ttisnotnil(&t->array[i])) {
          setbvalue(idx, 1);
          return;
        }
      }
      /* iterate dictionary; if t is a simple table only one iteration is done */
      for (i -= t->sizearray; i < sizenode(t); i++) {
        if (ttisnotnil(gval(gnode(t, i)))) { setbvalue(idx, 1); return; }
      }
      setbvalue(idx, 0);
    } else if (!call_binTM(L, tbl, luaO_nilobject, idx, TM_FILLED)) {
      luaG_typeerror(L, tbl, "get size of");
    }
  }
  else if (ttisuset(tbl)) {
    UltraSet *s = usvalue(tbl);
    if (fasttm(L, s->metatable, TM_FILLED) == NULL) {  /* no metamethod ? new 2.27.4 */
      setbvalue(idx, s->size != 0);
    } else if (!call_binTM(L, tbl, luaO_nilobject, idx, TM_FILLED)) {
      luaG_typeerror(L, tbl, "get size of");
    }
  }
  else if (ttisseq(tbl)) {
    Seq *s = seqvalue(tbl);
    if (fasttm(L, s->metatable, TM_FILLED) == NULL) {  /* no metamethod ? new 2.27.4 */
      setbvalue(idx, s->size != 0);
    } else if (!call_binTM(L, tbl, luaO_nilobject, idx, TM_FILLED)) {
      luaG_typeerror(L, tbl, "get size of");
    }
  }
  else if (ttisreg(tbl)) {  /* 2.3.0 RC 3 */
    Reg *s = regvalue(tbl);
    if (fasttm(L, s->metatable, TM_FILLED) == NULL) {  /* no metamethod ? new 2.27.4 */
      setbvalue(idx, s->top != 0);
    } else if (!call_binTM(L, tbl, luaO_nilobject, idx, TM_FILLED)) {
      luaG_typeerror(L, tbl, "get size of");
    }
  }
  else if (ttisstring(tbl)) {  /* 2.10.0 */
    setbvalue(idx, tsvalue(tbl)->len != 0);
  }
  else if (ttisuserdata(tbl) || ttisfunction(tbl)) {  /* try metamethod, 2.15.4, 2.36.2 extension for functions */
    if (!call_binTM(L, tbl, luaO_nilobject, idx, TM_FILLED))
      luaG_typeerror(L, tbl, "get size of");
  }
  else
    luaG_runerror(L, "Error in " LUA_QS ": table, set, sequence, register, userdata or string expected, got %s.", "filled",
      luaT_typenames[(int)ttype(tbl)]);
}


LUAI_FUNC void agenaV_empty (lua_State *L, const TValue *tbl, StkId idx) {  /* 2.10.0, based on agenaV_filled, patched 7.1.2 */
  int i;
  if (ttistable(tbl)) {
    Table *t;
    t = hvalue(tbl);
    /* iterate simple table of the form {v1, v2, ...}, not {1~v1, ...} ;
       if t is a dictionary, this loop is also skipped */
    if (fasttm(L, t->metatable, TM_EMPTY) == NULL) {  /* no metamethod ? new 2.27.4 */
      for (i=0; i < t->sizearray; i++) {
        if (ttisnotnil(&t->array[i])) {
          setbvalue(idx, 0);
          return;
        }
      }
      /* iterate dictionary; if t is a simple table only one iteration is done */
      for (i -= t->sizearray; i < sizenode(t); i++) {
        if (ttisnotnil(gval(gnode(t, i)))) { setbvalue(idx, 0); return; }
      }
      setbvalue(idx, 1);
    } else if (!call_binTM(L, tbl, luaO_nilobject, idx, TM_EMPTY)) {
      luaG_typeerror(L, tbl, "get size of");
    }
  }
  else if (ttisuset(tbl)) {
    UltraSet *s = usvalue(tbl);
    if (fasttm(L, s->metatable, TM_EMPTY) == NULL) {  /* no metamethod ? new 2.27.4 */
      setbvalue(idx, s->size == 0);
    } else if (!call_binTM(L, tbl, luaO_nilobject, idx, TM_EMPTY)) {
      luaG_typeerror(L, tbl, "get size of");
    }
  }
  else if (ttisseq(tbl)) {
    Seq *s = seqvalue(tbl);
    if (fasttm(L, s->metatable, TM_EMPTY) == NULL) {  /* no metamethod ? new 2.27.4 */
      setbvalue(idx, s->size == 0);
    } else if (!call_binTM(L, tbl, luaO_nilobject, idx, TM_EMPTY)) {
      luaG_typeerror(L, tbl, "get size of");
    }
  }
  else if (ttisreg(tbl)) {
    Reg *s = regvalue(tbl);
    if (fasttm(L, s->metatable, TM_EMPTY) == NULL) {  /* no metamethod ? new 2.27.4 */
      setbvalue(idx, s->top == 0);
    } else if (!call_binTM(L, tbl, luaO_nilobject, idx, TM_EMPTY)) {
      luaG_typeerror(L, tbl, "get size of");
    }
  }
  else if (ttisstring(tbl)) {
    setbvalue(idx, tsvalue(tbl)->len == 0);
  }
  else if (ttisuserdata(tbl) || ttisfunction(tbl)) {  /* try metamethod, 2.15.4, 2.36.2 extension for functions */
    if (!call_binTM(L, tbl, luaO_nilobject, idx, TM_EMPTY)) {
      luaG_typeerror(L, tbl, "get size of");
    }
  }
  else
    luaG_runerror(L, "Error in " LUA_QS ": table, set, sequence, register, userdata or string expected, got %s.", "empty",
      luaT_typenames[(int)ttype(tbl)]);
}


/* INSERT element INTO table statement; October 14, 2007;
   patched Nov 22, 2007 (crash if table was NULL); extended to support multiple returns on April 3, 2009;
   patched 2.9.1 */
LUAI_FUNC void agenaV_insert (lua_State *L, const TValue *t, StkId idx, int nargs) {
  int c;
  const TValue *tm;
  if (nargs == 0) {  /* are there multiple returns ? */
    nargs = cast_int(L->top - idx);
    L->top = L->ci->top;
  }
  if (ttistable(t)) {
    Table *tbl = hvalue(t);
    int n = luaH_getn(tbl) + 1;
    if (fasttm(L, tbl->metatable, TM_WEAK) != NULL)
      luaG_runerror(L, "Error: " LUA_QS " is not suited for weak tables.", "insert");  /* Agena 1.7.2 */
    if ((tm = fasttm(L, tbl->metatable, TM_WRITEINDEX)) == NULL) {
      for (c=0; c < nargs; c++) {
        luaH_setint(L, tbl, n++, idx + c);  /* 4.6.3 tweak */
        luaC_barriert(L, tbl, idx + c);
      }
      return;
    }
    else {
      if (ttisfunction(tm)) {  /* better sure than sorry */
        for (c=0; c < nargs; c++) {  /* do not overwrite table (which is at the top of the stack) with procedure */
          /* set next index to stack, 2.9.1 fix */
          L->top++;
          setnvalue(L->top, n++);
          luaD_checkstack(L, 1);
          /* call mt */
          L->top++; callTM(L, tm, t, L->top - 1, idx + c); L->top--;
          L->top--;  /* remove index from stack */
        }
        return;
      }  /* else: do nothing */
    }
  } else if (ttisuset(t)) {
    UltraSet *s = usvalue(t);
    if ((tm = fasttm(L, s->metatable, TM_WRITEINDEX)) == NULL) {
      for (c=0; c < nargs; c++) {
        agnUS_set(L, s, idx + c);
        luaC_barrierset(L, s, idx + c);  /* Agena 1.2 fix */
      }
      return;
    }
    else {
      if (ttisfunction(tm)) {  /* better sure than sorry */
        for (c=0; c < nargs; c++) {  /* do not overwrite table (which is at the top of the stack) with procedure */
          L->top++; callTM(L, tm, t, idx + c, idx + c); L->top--;
        }
        return;
      }  /* else: do nothing */
    }
  } else if (ttisseq(t)) {
    int rc;
    TValue *val;
    Seq *s = seqvalue(t);
    if ((tm = fasttm(L, s->metatable, TM_WRITEINDEX)) == NULL) {
      for (c=0; c < nargs; c++) {
        val = idx + c;
        rc = agnSeq_seti(L, s, s->size + 1, val);
        if (rc == -1)
          luaG_runerror(L, "Error in " LUA_QS ": maximum sequence size exceeded.", "insert");
        else if (rc == -2)
          luaG_runerror(L, "Error in " LUA_QS ": null cannot be inserted into sequences.", "insert");
        luaC_barrierseq(L, s, val);
      }
      return;
    }
    else {
      if (ttisfunction(tm)) {  /* better sure than sorry, fixed 2.9.1 */
        for (c=0; c < nargs; c++) {
          L->top++;  /* 2.9.1 fix, do not overwrite table (which is at the top of the stack) with procedure */
          setnvalue(L->top, s->size + 1);  /* set next sequence index number to stack */
          luaD_checkstack(L, 1);
          L->top++; callTM(L, tm, t, L->top - 1, idx + c); L->top--;
          L->top--;  /* delete new index */
        }
        return;
      }  /* else: do nothing */
    }
  }
  else if (ttisreg(t)) {  /* 2.3.0 RC 3 */
    Reg *s = regvalue(t);
    if ((tm = fasttm(L, s->metatable, TM_WRITEINDEX)) == NULL) {
      size_t i, flag;
      for (c=0; c < nargs; c++) {
        flag = 1;
        for (i=1; i <= s->top; i++) {
          if (ttisnil(agnReg_geti(s, i))) {
            agnReg_seti(L, s, i, idx + c);
            flag = 0;
            break;
          }
        }
        if (flag)
          luaG_runerror(L, "Error in " LUA_QS ": register does not include a null value.", "insert");
      }
      return;
    }
    else {
      if (ttisfunction(tm)) {  /* better sure than sorry, fixed 2.9.1 */
        size_t i, pos;
        for (c=0; c < nargs; c++) {  /* do not overwrite table (which is at the top of the stack) with procedure */
          /* search for next null slot */
          pos = 0;
          for (i=1; i <= s->top; i++) {
            if (ttisnil(agnReg_geti(s, i))) {
              pos = i;
              break;
            }
          }
          if (pos == 0)
            luaG_runerror(L, "Error in " LUA_QS ": register does not include a null value.", "insert");
          else {  /* call mt */
            /* set next register index */
            L->top++;
            setnvalue(L->top, pos);
            luaD_checkstack(L, 1);
            L->top++;
            callTM(L, tm, t, L->top - 1, idx + c);
            L->top--;  /* do it */
            L->top--;  /* delete new register index */
          }
        }
        return;
      }  /* else: do nothing */
    }
  }
  else
    luaG_runerror(L, "Error in " LUA_QS ": table, set, sequence or register expected, got %s.", "insert",
      luaT_typenames[(int)ttype(t)]);
}


LUAI_FUNC int agenaV_delete_mt (lua_State *L, const TValue *tm, const TValue *tbl, StkId val, int nargs) {
  int i, r;
  lua_lock(L);
  if ((r = ttisfunction(tm))) {  /* better sure than sorry */
    for (i=0; i < nargs; i++) {  /* do not overwrite table (which is at the top of the stack) with procedure */
      /* push null to stack, 2.9.1 */
      L->top++;
      setnilvalue(L->top);
      luaD_checkstack(L, 1);
      /* for each `delete` value, call mt */
      L->top++; callTM(L, tm, tbl, val + i, L->top - 1); L->top--;
      L->top--;  /* delete null */
    }
  }  /* else do nothing */
  lua_unlock(L);
  return r;
}

/* DELETE element FROM table statement; October 14, 2007; patched Dec 08, 2007; tweaked January 07, 2009;
   extended to support multiple returns on April 3, 2009; changed and extended to support metatables 2.9.1 */
LUAI_FUNC void agenaV_delete (lua_State *L, const TValue *tbl, StkId val, int nargs) {
  const TValue *tm;
  int i;
  if (nargs == 0) {  /* multiple arguments ? */
    nargs = cast_int(L->top - val);
  }
  if (ttistable(tbl)) {
    Table *t;
    t = hvalue(tbl);
    if (fasttm(L, t->metatable, TM_WEAK) != NULL)
      luaG_runerror(L, "Error: " LUA_QS " is not suited for weak tables.", "delete");  /* Agena 1.7.2 */
    if ((tm = fasttm(L, t->metatable, TM_WRITEINDEX)) == NULL) {
      for (i=0; i < nargs; i++) {
        agnH_delete(L, t, val + i);  /* changed 5.1.2 */
      }
    } else /* mt attached */
      if (agenaV_delete_mt(L, tm, tbl, val, nargs)) return;
  } else if (ttisuset(tbl)) {
    UltraSet *s = usvalue(tbl);
    if ((tm = fasttm(L, s->metatable, TM_WRITEINDEX)) == NULL) {
      for (i=0; i < nargs; i++)
        agnUS_delete(L, usvalue(tbl), val + i);
    } else if (agenaV_delete_mt(L, tm, tbl, val, nargs)) return;
  } else if (ttisseq(tbl)) {
    Seq *s = seqvalue(tbl);  /* 0.28.1 */
    if ((tm = fasttm(L, s->metatable, TM_WRITEINDEX)) == NULL) {
      for (i=0; i < nargs; i++) {
        agnSeq_delete(L, s, val + i);
      }
      agnSeq_resize(L, s, s->size);  /* 2.29.0 improvement */
    } else {
      if (agenaV_delete_mt(L, tm, tbl, val, nargs)) {
        agnSeq_resize(L, s, s->size);  /* 2.29.0 */
        return;
      }
    }
  }  /* else do nothing */
  else if (ttisreg(tbl)) {  /* 2.3.0 RC 3 */
    Reg *s = regvalue(tbl);
    if ((tm = fasttm(L, s->metatable, TM_WRITEINDEX)) == NULL) {
      for (i=0; i < nargs; i++)
        agnReg_delete(L, s, val + i);
    } else if (agenaV_delete_mt(L, tm, tbl, val, nargs)) return;
  }
  else
    luaG_runerror(L, "Error in " LUA_QS ": table, set, sequence or register expected, got %s.", "delete",
      luaT_typenames[(int)ttype(tbl)]);
}


/* Deletes all occurrences of `value` from the table, set, sequence or register `what` all other types are simply ignored; 5.1.2 */
LUAI_FUNC int agenaV_deletefrom (lua_State *L, const TValue *value, const TValue *what) {
  int c = 0;
  lua_lock(L);
  switch (ttype(what)) {
    case LUA_TTABLE: {
      c = agnH_delete(L, hvalue(what), value);
      break;
    }
    case LUA_TSET: {
      UltraSet *s = usvalue(what);
      if (agnUS_get(s, value)) {
        agnUS_delete(L, s, value);
        c++;
      }
      break;
    }
    case LUA_TSEQ: {
      Seq *h = seqvalue(what);
      c = agnSeq_delete(L, h, value);
      agnSeq_resize(L, h, h->size);
      break;
    }
    case LUA_TREG: {
      c = agnReg_delete(L, regvalue(what), value);
      break;
    }
    default: c = -1;
  }
  lua_unlock(L);
  return c;
}


/* compute union of two tables, sets, or sequences; used by UNION operator; October 16, 2007.
   All elements in both tables are copied into the new table, regardless
   whether they occur multiple times or not (i.a. [1] union [1] >> [1, 1]);
   tweaked 1.3.3; extended August 01, 2014 */

LUAI_FUNC void union_settodict (lua_State *L, Table *h, Table *t, int i) {
  Node *n = gnode(t, i);
  TValue *val = gval(n);
  if (ttisnotnil(val)) {
    TValue *oldval = luaH_set(L, h, key2tval(n));  /* do a primitive set */
    setobj2t(L, oldval, val);
    h->flags = 0;  /* Lua 5.1.4 patch 10 / 2.29.3 */
    luaC_barriert(L, h, val);
  }
}

#define settypendmetatable(L,idx,t1,t2) { \
  if (t1->metatable) agenaV_setmetatable(L, idx, t1->metatable, 0); \
  if (t1->type) agenaV_setutype(L, idx, t1->type); \
  if (t1->metatable == NULL && t2->metatable) agenaV_setmetatable(L, idx, t2->metatable, 0); \
  if (t1->type == NULL && t2->type) agenaV_setutype(L, idx, t2->type); \
}

LUAI_FUNC void agenaV_union (lua_State *L, const TValue *tbl1, const TValue *tbl2, StkId idx) {
  if (ttistable(tbl1) && ttistable(tbl2)) {
    Table *t1, *t2, *newtable;
    TValue *val;
    const TValue *tm;
    int i, c;
    c = 0;
    t1 = hvalue(tbl1);
    t2 = hvalue(tbl2);
    if ((tm = fasttm(L, t1->metatable, TM_UNION)) != NULL ||
        (tm = fasttm(L, t2->metatable, TM_UNION)) != NULL) { /* 2.3.0, metamethod given ? */
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl2, idx, TM_UNION);
        return;
      }
    }
    /* table for resulting union */
    newtable = luaH_new(L, t1->sizearray + t2->sizearray, 0);
    /* traverse first table */
    for (i=0; i < t1->sizearray; i++) {
      if (ttisnotnil(&t1->array[i])) {
        c++;
        val = &t1->array[i];
        luaH_setint(L, newtable, c, val);  /* 4.6.3 tweak */
        luaC_barriert(L, newtable, val);
      }
    }
    for (i -= t1->sizearray; i < sizenode(t1); i++) {  /* added 0.8.0 */
      union_settodict(L, newtable, t1, i);  /* 2.30.0 change: merge dictionaries appropriately */
    }
    /* traverse second table */
    for (i=0; i < t2->sizearray; i++) {
      if (ttisnotnil(&t2->array[i])) {
        c++;
        val = &t2->array[i];
        luaH_setint(L, newtable, c, val);  /* 4.6.3 tweak */
        luaC_barriert(L, newtable, val);
      }
    }
    for (i -= t2->sizearray; i < sizenode(t2); i++) {  /* added 0.8.0 */
      union_settodict(L, newtable, t2, i);  /* 2.30.0 change: merge dictionaries appropriately */
    }
    /* prepare table for return to Agena environment */
    sethvalue(L, idx, newtable);
    settypendmetatable(L, idx, t1, t2);  /* 2.30.1 improvement */
  } else if (ttisuset(tbl1) && ttisuset(tbl2)) {
    UltraSet *t1, *t2, *newtable;
    TValue *v;
    const TValue *tm;
    LNode *val;
    int i;
    unsigned int c;
    c = 0;
    t1 = usvalue(tbl1);
    t2 = usvalue(tbl2);
    if ((tm = fasttm(L, t1->metatable, TM_UNION)) != NULL ||
        (tm = fasttm(L, t2->metatable, TM_UNION)) != NULL) { /* 2.3.0, metamethod given ? */
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl2, idx, TM_UNION);
        return;
      }
    }
    /* table for resulting union */
    newtable = agnUS_new(L, sizenode(t1) + sizenode(t2));
    /* traverse first set */
    for (i=0; i < sizenode(t1); i++) {  /* added 0.8.0 */
      val = gnode(t1, i);
      if (ttisnotnil(glkey(val))) {
        c++;
        v = key2tval(val);
        agnUS_set(L, newtable, v);
        luaC_barrierset(L, newtable, v);  /* Agena 1.2 fix */
      }
    }
    /* traverse second set */
    for (i=0; i < sizenode(t2); i++) {  /* added 0.8.0 */
      val = gnode(t2, i);
      if (ttisnotnil(glkey(val))) {
        c++;
        v = key2tval(val);
        agnUS_set(L, newtable, v);
        luaC_barrierset(L, newtable, v);  /* Agena 1.2 fix */
      }
    }
    /* prepare set for return to Agena environment */
    setusvalue(L, idx, newtable);
    settypendmetatable(L, idx, t1, t2);  /* 2.30.1 improvement */
  } else if (ttisseq(tbl1) && ttisseq(tbl2)) {
    Seq *t1, *t2, *newseq;
    int i;
    unsigned int c;
    TValue *v;
    const TValue *tm;
    c = 0;
    t1 = seqvalue(tbl1);
    t2 = seqvalue(tbl2);
    if ((tm = fasttm(L, t1->metatable, TM_UNION)) != NULL || (tm = fasttm(L, t2->metatable, TM_UNION)) != NULL) { /* 2.3.0, metamethod given ? */
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl2, idx, TM_UNION);
        return;
      }
    }
    /* stack must be increased to avoid `invalid key to next` errors */
    api_check(L, L->top < L->ci->top);
    /* table for resulting union */
    newseq = agnSeq_new(L, t1->size + t2->size);
    /* traverse first sequence */
    for (i=0; i < t1->size; i++) {  /* added 0.8.0 */
      c++;
      v = seqitem(t1, i);
      agnSeq_seti(L, newseq, c, v);
      luaC_barrierseq(L, newseq, v);  /* Agena 1.2 fix */
    }
    /* traverse second sequence */
    for (i=0; i < t2->size; i++) {  /* added 0.8.0 */
      c++;
      v = seqitem(t2, i);
      agnSeq_seti(L, newseq, c, v);
      luaC_barrierseq(L, newseq, v);  /* Agena 1.2 fix */
    }
    /* prepare sequence for return to Agena environment */
    agnSeq_resize(L, newseq, c);  /* 2.3.0 RC 3 */
    setseqvalue(L, idx, newseq);
    settypendmetatable(L, idx, t1, t2);  /* 2.30.1 improvement */
  } else if (ttisreg(tbl1) && ttisreg(tbl2)) {  /* 2.3.0 RC 3 */
    Reg *t1, *t2, *newreg;
    int i;
    unsigned int c;
    TValue *v;
    const TValue *tm;
    c = 0;
    t1 = regvalue(tbl1);
    t2 = regvalue(tbl2);
    if ((tm = fasttm(L, t1->metatable, TM_UNION)) != NULL || (tm = fasttm(L, t2->metatable, TM_UNION)) != NULL) {
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl2, idx, TM_UNION);
        return;
      }
    }
    /* stack must be increased to avoid `invalid key to next` errors */
    api_check(L, L->top < L->ci->top);
    /* table for resulting union */
    newreg = agnReg_new(L, t1->top + t2->top);
    /* traverse first register */
    for (i=0; i < t1->top; i++) {
      c++;
      v = regitem(t1, i);
      agnReg_seti(L, newreg, c, v);
      luaC_barrierreg(L, newreg, v);
    }
    /* traverse second register */
    for (i=0; i < t2->top; i++) {
      c++;
      v = regitem(t2, i);
      agnReg_seti(L, newreg, c, v);
      luaC_barrierreg(L, newreg, v);
    }
    /* prepare register for return to Agena environment */
    agnReg_resize(L, newreg, c, 1);  /* shrink size */
    setregvalue(L, idx, newreg);
    settypendmetatable(L, idx, t1, t2);  /* 2.30.1 improvement */
  } else if ((ttisuserdata(tbl1) && ttisuserdata(tbl2)) ||
             (ttisfunction(tbl1) && ttisfunction(tbl2)) ) {  /* 2.3.0 RC 3, try metamethod; 2.36.2 extension for functions */
    if (!call_binTM(L, tbl1, tbl2, idx, TM_UNION))
      luaG_typeerror(L, tbl2, "creating union");
  } else if ((ttistable(tbl1) && ttisnil(tbl2)) || (ttisnil(tbl1) && ttistable(tbl2)) ) {  /* new 4.12.1 */
    Table *t1, *newtable;
    TValue *val;
    const TValue *tm;
    int i, c;
    c = 0;
    t1 = (ttistable(tbl1)) ? hvalue(tbl1) : hvalue(tbl2);
    if ((tm = fasttm(L, t1->metatable, TM_UNION)) != NULL ) {
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl1, idx, TM_UNION);
        return;
      }
    }
    /* table for resulting union */
    newtable = luaH_new(L, t1->sizearray, 0);
    /* traverse set */
    for (i=0; i < t1->sizearray; i++) {
      if (ttisnotnil(&t1->array[i])) {
        c++;
        val = &t1->array[i];
        luaH_setint(L, newtable, c, val);
        luaC_barriert(L, newtable, val);
      }
    }
    for (i -= t1->sizearray; i < sizenode(t1); i++) {
      union_settodict(L, newtable, t1, i);
    }
    /* prepare table for return to Agena environment */
    sethvalue(L, idx, newtable);
    settypendmetatable(L, idx, t1, t1);  /* 2.30.1 improvement */
  } else if ((ttisuset(tbl1) && ttisnil(tbl2)) || (ttisnil(tbl1) && ttisuset(tbl2)) ) {  /* new 4.12.1 */
    UltraSet *t1, *newtable;
    TValue *v;
    const TValue *tm;
    LNode *val;
    int i;
    unsigned int c;
    c = 0;
    t1 = (ttisuset(tbl1)) ? usvalue(tbl1) : usvalue(tbl2);
    if ((tm = fasttm(L, t1->metatable, TM_UNION)) != NULL) {
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl1, idx, TM_UNION);
        return;
      }
    }
    /* table for resulting union */
    newtable = agnUS_new(L, sizenode(t1));
    /* traverse first set */
    for (i=0; i < sizenode(t1); i++) {
      val = gnode(t1, i);
      if (ttisnotnil(glkey(val))) {
        c++;
        v = key2tval(val);
        agnUS_set(L, newtable, v);
        luaC_barrierset(L, newtable, v);
      }
    }
    /* prepare set for return to Agena environment */
    setusvalue(L, idx, newtable);
    settypendmetatable(L, idx, t1, t1);
  } else if ((ttisseq(tbl1) && ttisnil(tbl2)) || (ttisnil(tbl1) && ttisseq(tbl2)) ) {  /* new 4.12.1 */
    Seq *t1, *newseq;
    int i;
    unsigned int c;
    TValue *v;
    const TValue *tm;
    c = 0;
    t1 = (ttisseq(tbl1)) ? seqvalue(tbl1) : seqvalue(tbl2);
    if ((tm = fasttm(L, t1->metatable, TM_UNION)) != NULL) {
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl1, idx, TM_UNION);
        return;
      }
    }
    /* stack must be increased to avoid `invalid key to next` errors */
    api_check(L, L->top < L->ci->top);
    /* table for resulting union */
    newseq = agnSeq_new(L, t1->size);
    /* traverse sequence */
    for (i=0; i < t1->size; i++) {
      c++;
      v = seqitem(t1, i);
      agnSeq_seti(L, newseq, c, v);
      luaC_barrierseq(L, newseq, v);
    }
    /* prepare sequence for return to Agena environment */
    agnSeq_resize(L, newseq, c);
    setseqvalue(L, idx, newseq);
    settypendmetatable(L, idx, t1, t1);
  } else if ((ttisreg(tbl1) && ttisnil(tbl2)) || (ttisnil(tbl1) && ttisreg(tbl2)) ) {  /* new 4.12.1 */
    Reg *t1, *newreg;
    int i;
    unsigned int c;
    TValue *v;
    const TValue *tm;
    c = 0;
    t1 = (ttisreg(tbl1)) ? regvalue(tbl1) : regvalue(tbl2);
    if ((tm = fasttm(L, t1->metatable, TM_UNION)) != NULL) {
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl1, idx, TM_UNION);
        return;
      }
    }
    /* stack must be increased to avoid `invalid key to next` errors */
    api_check(L, L->top < L->ci->top);
    /* table for resulting union */
    newreg = agnReg_new(L, t1->top);
    /* traverse first table */
    for (i=0; i < t1->top; i++) {
      c++;
      v = regitem(t1, i);
      agnReg_seti(L, newreg, c, v);
      luaC_barrierreg(L, newreg, v);
    }
    /* prepare register for return to Agena environment */
    agnReg_resize(L, newreg, c, 1);  /* shrink size */
    setregvalue(L, idx, newreg);
    settypendmetatable(L, idx, t1, t1);
  } else if (ttisnil(tbl1) && ttisnil(tbl2)) {  /* new 4.12.1 */
    setnilvalue(idx);
  } else if (!call_binTM(L, tbl1, tbl2, idx, TM_UNION)) {  /* 7.0.3 allow for mixed-type operations */
    luaG_typeerror(L, tbl1, "unify");
  }
}


/* counts the number of elements in a union */
LUAI_FUNC unsigned int agenaV_numunion (lua_State *L, const TValue *tbl1, const TValue *tbl2) {
  int i;
  unsigned int c;
  c = 0;
  if (ttistable(tbl1) && ttistable(tbl2)) {
    Table *t1, *t2;
    t1 = hvalue(tbl1);
    t2 = hvalue(tbl2);
    /* traverse first table */
    for (i=0; i < t1->sizearray; i++) {
      if (ttisnotnil(&t1->array[i])) c++;
    }
    for (i -= t1->sizearray; i < sizenode(t1); i++) {
      if (ttisnotnil(gval(gnode(t1, i)))) c++;
    }
    /* traverse second table */
    for (i=0; i < t2->sizearray; i++) {
      if (ttisnotnil(&t2->array[i])) c++;
    }
    for (i -= t2->sizearray; i < sizenode(t2); i++) {
      if (ttisnotnil(gval(gnode(t2, i)))) c++;
    }
    /* prepare table for return to Agena environment */
  } else if (ttisuset(tbl1) && ttisuset(tbl2)) {
    UltraSet *t1, *t2;
    LNode *val;
    t1 = usvalue(tbl1);
    t2 = usvalue(tbl2);
    /* traverse first set */
    for (i=0; i < sizenode(t1); i++) {
      val = gnode(t1, i);
      if (ttisnotnil(glkey(val))) c++;
    }
    /* traverse second set */
    for (i=0; i < sizenode(t2); i++) {
      val = gnode(t2, i);
      if (ttisnotnil(glkey(val))) c++;
    }
    /* prepare set for return to Agena environment */
  } else if (ttisseq(tbl1) && ttisseq(tbl2)) {
    Seq *t1, *t2;
    t1 = seqvalue(tbl1);
    t2 = seqvalue(tbl2);
    c = t1->size + t2->size;
  } else if (ttisreg(tbl1) && ttisreg(tbl2)) {
    Reg *t1, *t2;
    t1 = regvalue(tbl1);
    t2 = regvalue(tbl2);
    c = t1->top + t2->top;
  } else if ((ttistable(tbl1) && ttisnil(tbl2)) || (ttisnil(tbl1) && ttistable(tbl2)) ) {  /* new 4.12.1 */
    Table *t1;
    t1 = (ttistable(tbl1)) ? hvalue(tbl1) : hvalue(tbl2);
    /* traverse table */
    for (i=0; i < t1->sizearray; i++) {
      if (ttisnotnil(&t1->array[i])) c++;
    }
    for (i -= t1->sizearray; i < sizenode(t1); i++) {
      if (ttisnotnil(gval(gnode(t1, i)))) c++;
    }
    /* prepare table for return to Agena environment */
  } else if ((ttisuset(tbl1) && ttisnil(tbl2)) || (ttisnil(tbl1) && ttisuset(tbl2)) ) {  /* new 4.12.1 */
    UltraSet *t1;
    LNode *val;
    t1 = (ttisuset(tbl1)) ? usvalue(tbl1) : usvalue(tbl2);
    /* traverse set */
    for (i=0; i < sizenode(t1); i++) {
      val = gnode(t1, i);
      if (ttisnotnil(glkey(val))) c++;
    }
    /* prepare set for return to Agena environment */
  } else if ((ttisseq(tbl1) && ttisnil(tbl2)) || (ttisnil(tbl1) && ttisseq(tbl2)) ) {  /* new 4.12.1 */
    Seq *t1;
    t1 = (ttisseq(tbl1)) ? seqvalue(tbl1) : seqvalue(tbl2);
    c = t1->size;
  } else if ((ttisreg(tbl1) && ttisnil(tbl2)) || (ttisnil(tbl1) && ttisreg(tbl2)) ) {  /* new 4.12.1 */
    Reg *t1;
    t1 = (ttisreg(tbl1)) ? regvalue(tbl1) : regvalue(tbl2);
    c = t1->top;
  } else if (ttisnil(tbl1) && ttisnil(tbl2)) {  /* new 4.12.1 */
    c = 0;
  } else {
    luaG_runerror(L, "Error in " LUA_QS ": tables, sets, sequences or registers expected.", "numunion");
  }
  return c;
}


/* Compute intersection or difference sets; October 16, 2007; patched October 26, 2008; tweaked January 07, 2009; extended August 01, 2014
   All elements in both tables are copied into the new table, regardless
   whether they occur multiple times or not (i.a. [1, 1, 2] minus [2] >> [1, 1])
      mode = 0: intersection,
      mode = 1: minus */

LUAI_FUNC void agenaV_setops_settodict (lua_State *L, Table *h, TValue *key, TValue *val) {
  TValue *oldval = luaH_set(L, h, key);  /* do a primitive set */
  setobj2t(L, oldval, val);
  h->flags = 0;  /* Lua 5.1.4 patch 10 / 2.29.3 */
  luaC_barriert(L, h, val);
}

LUAI_FUNC void agenaV_setops (lua_State *L, const TValue *tbl1, const TValue *tbl2, StkId idx, int mode) {
  if ((ttistable(tbl1) && ttistable(tbl2)) ||
      (ttistable(tbl1) && ttisnil(tbl2))   ||
      (ttisnil(tbl1) && ttistable(tbl2)) ) {
    Table *t1, *t2, *newtable;
    TValue *t1key, *t2key, *t1val, *t2val;
    const TValue *tm;
    int i, j, c, oldi, oldj, flag;
    c = 0;
    if (ttisnil(tbl1) || ttisnil(tbl2)) {
      t1 = ttistable(tbl1) ? hvalue(tbl1) : luaH_new(L, 0, 0);
      t2 = ttistable(tbl2) ? hvalue(tbl2) : luaH_new(L, 0, 0);
    } else {
      t1 = hvalue(tbl1);
      t2 = hvalue(tbl2);
    }
    if ( (((tm = fasttm(L, t1->metatable, TM_INTERSECT)) != NULL || (tm = fasttm(L, t2->metatable, TM_INTERSECT)) != NULL) && mode == 0) ||
         (((tm = fasttm(L, t1->metatable, TM_MINUS)) != NULL || (tm = fasttm(L, t2->metatable, TM_MINUS)) != NULL) && mode == 1) ) {
         /* 2.3.0, metamethod given ? */
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl2, idx, (mode == 0 ? TM_INTERSECT : TM_MINUS));
        return;
      }
    }
    i = oldi = j = oldj = -1;  /* save value of j for consecutive iteration of 2nd table */
    newtable = luaH_new(L, t1->sizearray + t2->sizearray, 0);
    flag = 0;
    for (i++; i < t1->sizearray; i++) {
      if (ttisnotnil(&t1->array[i])) {
        flag = 0;
        L->top++;
        setnvalue(L->top, i);
        t1key = L->top;
        t1val = &t1->array[i];  /* value */
        /* increase top for value of 2nd table */
        for (j++; j < t2->sizearray; j++) {
          t2val = &t2->array[j];
          if (ttisnotnil(t2val)) {
            setnvalue(L->top, j);
            t2key = L->top;
            if (equalobj(L, t1key, t2key) && equalobj(L, t1val, t2val)) {
              if (!mode) {  /* intersection operation ? -> put value into newtable */
                luaH_setint(L, newtable, ++c, t2val);  /* 4.6.3 tweak */
                luaC_barriert(L, newtable, t2val);
              }
              j = t2->sizearray;  /* set j properly so that next loop starts with correct value */
              flag = 1;
              break;  /* quit inner loop */
            }
          }
        }  /* of for j */
        for (j -= t2->sizearray; j < sizenode(t2); j++) {
          Node *n = gnode(t2, j);
          t2key = key2tval(n);
          t2val = gval(n);
          if (ttisnotnil(t2val)) {
            if (equalobj(L, t1key, t2key) && equalobj(L, t1val, t2val)) {
              if (!mode) {  /* intersection operation ? -> put value into newtable */
                agenaV_setops_settodict(L, newtable, t2key, t2val);  /* 2.30.0 change: merge dictionaries appropriately */
              }
              flag = 1;
              break;  /* quit inner loop */
            }
          }
        }  /* of for j */
        j = oldj;  /* reset pointer for 2nd table */
        if (mode && !flag) {  /* minus operation and element not found in 2nd table ? */
          luaH_setint(L, newtable, ++c, t1val);  /* 4.6.3 tweak */
          luaC_barriert(L, newtable, t1val);
        }
        L->top--;
      }  /* of if */
    }  /* of for i */
    i = oldi + 1 + t1->sizearray;  /* reset i to upper bound in case element was not found */
    for (i -= t1->sizearray; i < sizenode(t1); i++) {  /* added 0.8.0 */
      Node *n = gnode(t1, i);
      if (ttisnotnil(gval(n))) {
        flag = 0;
        t1key = key2tval(n);
        t1val = gval(n);  /* value */
        for (j++; j < t2->sizearray; j++) {
          t2val = &t2->array[j];
          if (ttisnotnil(t2val)) {
            setnvalue(L->top, j);
            t2key = L->top;
            if (equalobj(L, t1key, t2key) && equalobj(L, t1val, t2val)) {
              if (!mode) {  /* intersection operation ? -> put value into newtable */
                luaH_setint(L, newtable, ++c, t2val);  /* 4.6.3 tweak */
                luaC_barriert(L, newtable, t2val);
              }
              flag = 1;
              j = t2->sizearray;  /* set j properly so that next loop starts with correct value */
              break;  /* quit inner loop */
            }
          }
        }  /* of for j */
        for (j -= t2->sizearray; j < sizenode(t2); j++) {
          Node *n = gnode(t2, j);
          t2val = gval(n);
          if (ttisnotnil(t2val)) {
            t2key = key2tval(n);
            if (equalobj(L, t1key, t2key) && equalobj(L, t1val, t2val)) {
              if (!mode) {  /* intersection operation ? -> put value into newtable */
                agenaV_setops_settodict(L, newtable, t2key, t2val);  /* 2.30.0 change: merge dictionaries appropriately */
              }
              flag = 1;
              break;  /* quit inner loop */
            }
          }
        }  /* of for j */
        j = oldj;  /* reset pointer for 2nd table */
        if (mode && !flag) {  /* minus operation and element not found in 2nd table ? */
          agenaV_setops_settodict(L, newtable, t1key, t1val);  /* 2.30.0 change: merge dictionaries appropriately */
        }
      }  /* of if */
    }  /* of for i */
    luaH_resizearray(L, newtable, c);
    sethvalue(L, idx, newtable);
    settypendmetatable(L, idx, t1, t2);  /* 2.30.1 improvement */
    /* we do not have to free the auxiliary empty structure, according to Valgrind. If we do, sometimes the `bye`
       statement gets stuck */
  } else if ((ttisuset(tbl1) && ttisuset(tbl2)) ||
             (ttisuset(tbl1) && ttisnil(tbl2))  ||
             (ttisnil(tbl1) && ttisuset(tbl2)) ) {
    UltraSet *t1, *t2, *newset;
    TValue *t1val, *t2val;
    const TValue *tm;
    int i, flag;
    unsigned int c;
    c = 0;
    if (ttisnil(tbl1) || ttisnil(tbl2)) {
      t1 = ttisuset(tbl1) ? usvalue(tbl1) : agnUS_new(L, 0);
      t2 = ttisuset(tbl2) ? usvalue(tbl2) : agnUS_new(L, 0);
    } else {
      t1 = usvalue(tbl1);
      t2 = usvalue(tbl2);
    }
    if ( (((tm = fasttm(L, t1->metatable, TM_INTERSECT)) != NULL ||
           (tm = fasttm(L, t2->metatable, TM_INTERSECT)) != NULL) && mode == 0) ||
         (((tm = fasttm(L, t1->metatable, TM_MINUS)) != NULL ||
           (tm = fasttm(L, t2->metatable, TM_MINUS)) != NULL) && mode == 1) ) {
         /* 2.3.0, metamethod given ? */
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl2, idx, (mode == 0 ? TM_INTERSECT : TM_MINUS));
        return;
      }
    }
    newset = agnUS_new(L, sizenode(t1) + sizenode(t2) + 2);
    flag = 0;
    for (i=0; i < sizenode(t1); i++) {  /* added 0.8.0 */
      if (ttisnotnil(glkey(gnode(t1, i)))) {
        flag = 0;
        t1val = key2tval(gnode(t1, i));  /* value */
        if (agnUS_get(t2, t1val)) {
          if (!mode) {  /* intersection operation ? -> put value into newtable */
            c++;
            agnUS_set(L, newset, t1val);
            luaC_barrierset(L, newset, t1val);  /* Agena 1.2 fix */
          }
          flag = 1;
        } else if (ttype(t1val) > LUA_TTHREAD) {
          /* is left object a structure ? -> iterate right set */
          int j;
          for (j=0; j < sizenode(t2); j++) {
            if (ttisnotnil(key2tval(gnode(t2, j)))) {
              t2val = key2tval(gnode(t2, j));
              if (equalobj(L, t1val, t2val)) {
                flag = 1;
                if (!mode) {  /* intersection operation ? -> put value into newtable */
                  c++;
                  agnUS_set(L, newset, t2val);
                  luaC_barrierset(L, newset, t2val);  /* Agena 1.2 fix */
                }
                break;  /* quit this loop if element has been found */
              }
            }
          }  /* of for j */
        }
        if (mode && !flag) {   /* minus operation and element not found in 2nd table ? */
          c++;
          agnUS_set(L, newset, t1val);
          luaC_barrierset(L, newset, t1val);  /* Agena 1.2 fix */
        }
      }  /* of if */
    }  /* of for i */
    agnUS_resize(L, newset, c);  /* FIXED 0.28.1: resize UltraSet */
    setusvalue(L, idx, newset);
    settypendmetatable(L, idx, t1, t2);  /* 2.30.1 improvement */
    /* we do not have to free the auxiliary empty structure, according to Valgrind. If we do, sometimes the `bye`
       statement gets stuck */
  } else if ((ttisseq(tbl1) && ttisseq(tbl2)) ||
             (ttisseq(tbl1) && ttisnil(tbl2)) ||
             (ttisnil(tbl1) && ttisseq(tbl2)) ) {
    Seq *t1, *t2, *newseq;
    TValue *t1val;
    const TValue *tm;
    int i, j, flag;
    unsigned int c;
    c = 0;
    if (ttisnil(tbl1) || ttisnil(tbl2)) {
      t1 = ttisseq(tbl1) ? seqvalue(tbl1) : agnSeq_new(L, 0);
      t2 = ttisseq(tbl2) ? seqvalue(tbl2) : agnSeq_new(L, 0);
    } else {
      t1 = seqvalue(tbl1);
      t2 = seqvalue(tbl2);
    }
    if ( (((tm = fasttm(L, t1->metatable, TM_INTERSECT)) != NULL || (tm = fasttm(L, t2->metatable, TM_INTERSECT)) != NULL) && mode == 0) ||
         (((tm = fasttm(L, t1->metatable, TM_MINUS)) != NULL || (tm = fasttm(L, t2->metatable, TM_MINUS)) != NULL) && mode == 1) ) {
         /* 2.3.0, metamethod given ? */
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl2, idx, (mode == 0 ? TM_INTERSECT : TM_MINUS));
        return;
      }
    }
    newseq = agnSeq_new(L, t1->size + t2->size);
    flag = 0;
    for (i=0; i < t1->size; i++) {
      flag = 0;
      t1val = seqitem(t1, i);  /* value */
      for (j=0; j < t2->size; j++) {
        if (equalobj(L, t1val, seqitem(t2, j))) {
          if (!mode) {  /* intersection operation ? -> put value into newtable */
            c++;
            agnSeq_seti(L, newseq, c, t1val);
            luaC_barrierseq(L, newseq, t1val);  /* Agena 1.2 fix */
          }
          flag = 1;
          break;
        }
      }
      if (mode && !flag) {   /* minus operation and element not found in 2nd seq ? */
        c++;
        agnSeq_seti(L, newseq, c, t1val);
        luaC_barrierseq(L, newseq, t1val);  /* Agena 1.2 fix */
      }
    }
    agnSeq_resize(L, newseq, c);  /* 0.28.1 */
    setseqvalue(L, idx, newseq);
    settypendmetatable(L, idx, t1, t2);  /* 2.30.1 improvement */
    /* we do not have to free the auxiliary empty structure, according to Valgrind. If we do, sometimes the `bye`
       statement gets stuck */
  } else if ((ttisreg(tbl1) && ttisreg(tbl2)) ||
             (ttisreg(tbl1) && ttisnil(tbl2)) ||
             (ttisnil(tbl1) && ttisreg(tbl2)) ) {
    Reg *t1, *t2, *newreg;
    TValue *t1val;
    const TValue *tm;
    int i, j, flag;
    unsigned int c;
    c = 0;
    if (ttisnil(tbl1) || ttisnil(tbl2)) {
      t1 = ttisreg(tbl1) ? regvalue(tbl1) : agnReg_new(L, 0);
      t2 = ttisreg(tbl2) ? regvalue(tbl2) : agnReg_new(L, 0);
    } else {
      t1 = regvalue(tbl1);
      t2 = regvalue(tbl2);
    }
    if ( (((tm = fasttm(L, t1->metatable, TM_INTERSECT)) != NULL || (tm = fasttm(L, t2->metatable, TM_INTERSECT)) != NULL) && mode == 0) ||
         (((tm = fasttm(L, t1->metatable, TM_MINUS)) != NULL || (tm = fasttm(L, t2->metatable, TM_MINUS)) != NULL) && mode == 1) ) {
         /* 2.3.0, metamethod given ? */
      if (ttisfunction(tm)) {
        call_binTM(L, tbl1, tbl2, idx, (mode == 0 ? TM_INTERSECT : TM_MINUS));
        return;
      }
    }
    newreg = agnReg_new(L, t1->top + t2->top);
    flag = 0;
    for (i=0; i < t1->top; i++) {
      flag = 0;
      t1val = regitem(t1, i);  /* value */
      for (j=0; j < t2->top; j++) {
        if (equalobj(L, t1val, regitem(t2, j))) {
          if (!mode) {  /* intersection operation ? -> put value into newtable */
            c++;
            agnReg_seti(L, newreg, c, t1val);
            luaC_barrierreg(L, newreg, t1val);  /* Agena 1.2 fix */
          }
          flag = 1;
          break;
        }
      }
      if (mode && !flag) {   /* minus operation and element not found in 2nd seq ? */
        c++;
        agnReg_seti(L, newreg, c, t1val);
        luaC_barrierreg(L, newreg, t1val);  /* Agena 1.2 fix */
      }
    }
    agnReg_resize(L, newreg, c, 1);  /* pack register, 0.28.1 */
    setregvalue(L, idx, newreg);
    settypendmetatable(L, idx, t1, t2);  /* 2.30.1 improvement */
    /* we do not have to free the auxiliary empty structure, according to Valgrind. If we do, sometimes the `bye`
       statement gets stuck */
  } else if ((ttisuserdata(tbl1) && ttisuserdata(tbl2)) ||
             (ttisfunction(tbl1) && ttisfunction(tbl2))) {  /* 2.3.0 RC 3, try metamethod; 2.36.2 extension for functions */
    if (!call_binTM(L, tbl1, tbl2, idx, (mode == 0 ? TM_INTERSECT : TM_MINUS)))  /* 5.0.0 fixes */
      luaG_typeerror(L, tbl2, (mode == 0 ? "create intersection with" : "create difference with"));
  } else if (ttisnil(tbl1) && ttisnil(tbl2)) {  /* new 4.12.1 */
    setnilvalue(idx);
  } else if (!call_binTM(L, tbl1, tbl2, idx, mode == 0 ? TM_INTERSECT : TM_MINUS)) {
    /* 7.0.3 allow for mixed-type operations */
    luaG_typeerror(L, tbl1, mode == 0 ? "intersect" : "remove");
  }
}


/* Counts number of elements in intersections or difference set, 3.10.0, based on agenaV_setops.
   mode = 0 -> intersection, mode = 1 -> difference set */
LUAI_FUNC unsigned int agenaV_numsetops (lua_State *L, const TValue *tbl1, const TValue *tbl2, int mode) {
  unsigned int c = 0;
  if ((ttistable(tbl1) && ttistable(tbl2)) ||
      (ttistable(tbl1) && ttisnil(tbl2))   ||
      (ttisnil(tbl1) && ttistable(tbl2)) ) {  /* new 4.12.1 */
    Table *t1, *t2;
    TValue *t1key, *t2key, *t1val, *t2val;
    int i, j, oldi, oldj, flag;
    if (ttisnil(tbl1) || ttisnil(tbl2)) {
      t1 = ttistable(tbl1) ? hvalue(tbl1) : luaH_new(L, 0, 0);
      t2 = ttistable(tbl2) ? hvalue(tbl2) : luaH_new(L, 0, 0);
    } else {
      t1 = hvalue(tbl1);
      t2 = hvalue(tbl2);
    }
    i = oldi = j = oldj = -1;  /* save value of j for consecutive iterations of 2nd table */
    flag = 0;
    for (i++; i < t1->sizearray; i++) {
      if (ttisnotnil(&t1->array[i])) {
        flag = 0;
        L->top++;
        setnvalue(L->top, i);
        t1key = L->top;
        t1val = &t1->array[i];  /* value */
        /* increase top for value of 2nd table */
        for (j++; j < t2->sizearray; j++) {
          t2val = &t2->array[j];
          if (ttisnotnil(t2val)) {
            setnvalue(L->top, j);
            t2key = L->top;
            if (equalobj(L, t1key, t2key) && equalobj(L, t1val, t2val)) {
              if (!mode) {  /* intersection operation ? */
                c++;
              }
              j = t2->sizearray;  /* set j properly so that next loop starts with correct value */
              flag = 1;
              break;  /* quit inner loop */
            }
          }
        }  /* of for j */
        for (j -= t2->sizearray; j < sizenode(t2); j++) {
          Node *n = gnode(t2, j);
          t2key = key2tval(n);
          t2val = gval(n);
          if (ttisnotnil(t2val)) {
            if (equalobj(L, t1key, t2key) && equalobj(L, t1val, t2val)) {
              if (!mode) {  /* intersection operation ? */
                c++;
              }
              flag = 1;
              break;  /* quit inner loop */
            }
          }
        }  /* of for j */
        j = oldj;  /* reset pointer for 2nd table */
        if (mode && !flag) {  /* minus operation and element not found in 2nd table ? */
          c++;
        }
        L->top--;
      }  /* of if */
    }  /* of for i */
    i = oldi + 1 + t1->sizearray;  /* reset i to upper bound in case element was not found */
    for (i -= t1->sizearray; i < sizenode(t1); i++) {
      Node *n = gnode(t1, i);
      if (ttisnotnil(gval(n))) {
        flag = 0;
        t1key = key2tval(n);
        t1val = gval(n);  /* value */
        for (j++; j < t2->sizearray; j++) {
          t2val = &t2->array[j];
          if (ttisnotnil(t2val)) {
            setnvalue(L->top, j);
            t2key = L->top;
            if (equalobj(L, t1key, t2key) && equalobj(L, t1val, t2val)) {
              if (!mode) {  /* intersection operation ? */
                c++;
              }
              flag = 1;
              j = t2->sizearray;  /* set j properly so that next loop starts with correct value */
              break;  /* quit inner loop */
            }
          }
        }  /* of for j */
        for (j -= t2->sizearray; j < sizenode(t2); j++) {
          Node *n = gnode(t2, j);
          t2val = gval(n);
          if (ttisnotnil(t2val)) {
            t2key = key2tval(n);
            if (equalobj(L, t1key, t2key) && equalobj(L, t1val, t2val)) {
              if (!mode) {  /* intersection operation ? */
                c++;
              }
              flag = 1;
              break;  /* quit inner loop */
            }
          }
        }  /* of for j */
        j = oldj;  /* reset pointer for 2nd table */
        if (mode && !flag) {  /* minus operation and element not found in 2nd table ? */
          c++;
        }
      }  /* of if */
    }  /* of for i */
  } else if ((ttisuset(tbl1) && ttisuset(tbl2)) ||
             (ttisuset(tbl1) && ttisnil(tbl2))  ||  /* new 4.12.1 */
             (ttisnil(tbl1) && ttisuset(tbl2)) ) {
    UltraSet *t1, *t2;
    TValue *t1val, *t2val;
    int i, flag;
    if (ttisnil(tbl1) || ttisnil(tbl2)) {
      t1 = ttisuset(tbl1) ? usvalue(tbl1) : agnUS_new(L, 0);
      t2 = ttisuset(tbl2) ? usvalue(tbl2) : agnUS_new(L, 0);
    } else {
      t1 = usvalue(tbl1);
      t2 = usvalue(tbl2);
    }
    flag = 0;
    for (i=0; i < sizenode(t1); i++) {
      if (ttisnotnil(glkey(gnode(t1, i)))) {
        flag = 0;
        t1val = key2tval(gnode(t1, i));  /* value */
        if (agnUS_get(t2, t1val)) {
          if (!mode) {  /* intersection operation ? */
            c++;
          }
          flag = 1;
        } else if (ttype(t1val) > LUA_TTHREAD) {
          /* is left object a structure ? -> iterate right set */
          int j;
          for (j=0; j < sizenode(t2); j++) {
            if (ttisnotnil(key2tval(gnode(t2, j)))) {
              t2val = key2tval(gnode(t2, j));
              if (equalobj(L, t1val, t2val)) {
                flag = 1;
                if (!mode) {  /* intersection operation ? */
                  c++;
                }
                break;  /* quit this loop if element has been found */
              }
            }
          }  /* of for j */
        }
        if (mode && !flag) {   /* minus operation and element not found in 2nd table ? */
          c++;
        }
      }  /* of if */
    }  /* of for i */
  } else if ((ttisseq(tbl1) && ttisseq(tbl2)) ||
             (ttisseq(tbl1) && ttisnil(tbl2)) ||  /* new 4.12.1 */
             (ttisnil(tbl1) && ttisseq(tbl2)) ) {
    Seq *t1, *t2;
    TValue *t1val;
    int i, j, flag;
    if (ttisnil(tbl1) || ttisnil(tbl2)) {
      t1 = ttisseq(tbl1) ? seqvalue(tbl1) : agnSeq_new(L, 0);
      t2 = ttisseq(tbl2) ? seqvalue(tbl2) : agnSeq_new(L, 0);
    } else {
      t1 = seqvalue(tbl1);
      t2 = seqvalue(tbl2);
    }
    flag = 0;
    for (i=0; i < t1->size; i++) {
      flag = 0;
      t1val = seqitem(t1, i);  /* value */
      for (j=0; j < t2->size; j++) {
        if (equalobj(L, t1val, seqitem(t2, j))) {
          if (!mode) {  /* intersection operation ? */
            c++;
          }
          flag = 1;
          break;
        }
      }
      if (mode && !flag) {   /* minus operation and element not found in 2nd seq ? */
        c++;
      }
    }
  } else if ((ttisreg(tbl1) && ttisreg(tbl2)) ||
             (ttisreg(tbl1) && ttisnil(tbl2)) ||
             (ttisnil(tbl1) && ttisreg(tbl2)) ) {
    Reg *t1, *t2;
    TValue *t1val;
    int i, j, flag;
    if (ttisnil(tbl1) || ttisnil(tbl2)) {
      t1 = ttisreg(tbl1) ? regvalue(tbl1) : agnReg_new(L, 0);
      t2 = ttisreg(tbl2) ? regvalue(tbl2) : agnReg_new(L, 0);
    } else {
      t1 = regvalue(tbl1);
      t2 = regvalue(tbl2);
    }
    flag = 0;
    for (i=0; i < t1->top; i++) {
      flag = 0;
      t1val = regitem(t1, i);  /* value */
      for (j=0; j < t2->top; j++) {
        if (equalobj(L, t1val, regitem(t2, j))) {
          if (!mode) {  /* intersection operation ? */
            c++;
          }
          flag = 1;
          break;
        }
      }
      if (mode && !flag) {   /* minus operation and element not found in 2nd seq ? */
        c++;
      }
    }
  } else {
    luaG_runerror(L, "Error in " LUA_QS ": tables, sets, sequences or registers expected.",
      ((mode=0) ? "numintersect" : "numminus"));
  }
  return c;
}


/* Returns all integer indices of a table in a new table and puts it onto the top of the stack. flag = 0: no integer indices in hash part,
   flag = 1: there are integer indices in hash part, 2.30.1. https://en.cppreference.com/w/c/algorithm/qsort */
static int compare_ints (const void* a, const void* b) {
  int arg1 = *(const int*)a;
  int arg2 = *(const int*)b;
  return (arg1 > arg2) - (arg1 < arg2);
}

/* Collect all integral table keys in ascending, sorted order; 3.9.3 */
LUAI_FUNC int agenaV_tintindices (lua_State *L, TValue *tbl, int *flag) {
  int i, c;
  Table *h, *t;
  Node *n;
  TValue *key;
  t = hvalue(tbl);
  intvec_init(v, t->sizearray);
  if (intvec_initfail(v))
    luaG_runerror(L, "Error: memory allocation failed.");
  h = luaH_new(L, t->sizearray, 0);
  *flag = 0;
  for (i=1; i <= t->sizearray; i++) {
    if (ttisnotnil(&t->array[i - 1]))  /* to prevent compiler warnings in Stretch */
      intvec_append(v, i);
  }
  for (i=0; i < sizenode(t); i++) {
    n = gnode(t, i);
    if (ttisnotnil(gval(n))) {
      int index;
      key = key2tval(n);
      if (!ttisint(key)) continue;
      index = (int)nvalue(key);
      intvec_append(v, index);
      *flag = 1;
    }
  }
  c = intvec_size(v);
  /* https://en.cppreference.com/w/c/algorithm/qsort */
  qsort((&v)->data, c, sizeof(int), compare_ints);
  /* insert all sorted integer keys into the resulting table */
  for (i=0; i < intvec_size(v); i++) {
    setnvalue(L->top, cast_num(intvec_get(v, i)));
    luaH_setint(L, h, i + 1, L->top);  /* 4.6.3 tweak */
    luaC_barriert(L, h, L->top);  /* 7.6.1 fix just to prevent theoretical GC problems (numbers are not collected) */
  }
  luaH_resizearray(L, h, intvec_size(v));
  sethvalue(L, L->top++, h);
  intvec_free(v);
  return c;
}

/* Collect all the table entries that have integral keys in ascending, sorted order; 3.9.3 */
LUAI_FUNC int agenaV_tintentries (lua_State *L, TValue *tbl, int *flag) {
  int i, c;
  Table *h, *t;
  Node *n;
  TValue *key, *val;
  t = hvalue(tbl);
  intvec_init(v, t->sizearray);
  if (intvec_initfail(v))
    luaG_runerror(L, "Error: memory allocation failed.");
  h = luaH_new(L, t->sizearray, 0);
  *flag = 0;
  for (i=1; i <= t->sizearray; i++) {
    if (ttisnotnil(&t->array[i - 1]))  /* to prevent compiler warnings in Stretch */
      intvec_append(v, i);
  }
  for (i=0; i < sizenode(t); i++) {
    n = gnode(t, i);
    if (ttisnotnil(gval(n))) {
      int index;
      key = key2tval(n);
      if (!ttisint(key)) continue;
      index = (int)nvalue(key);
      intvec_append(v, index);
      *flag = 1;
    }
  }
  c = intvec_size(v);
  /* https://en.cppreference.com/w/c/algorithm/qsort */
  qsort((&v)->data, c, sizeof(int), compare_ints);
  /* iterate over all sorted integer keys and retrieve the respective values of type lua_Number */
  for (i=0; i < intvec_size(v); i++) {
    val = (TValue *)luaH_getnum(t, cast_num(intvec_get(v, i)));
    luaH_setint(L, h, i + 1, val);  /* 4.6.3 tweak */
    luaC_barriert(L, h, val);  /* 7.6.1 fix */
  }
  luaH_resizearray(L, h, intvec_size(v));
  sethvalue(L, L->top++, h);
  intvec_free(v);
  return c;
}


/* flag = 0: no value resides in the hash part, flag = 1: there is at least one element in the hash part. 2.30.1 */
LUAI_FUNC int agenaV_tentries (lua_State *L, TValue *tbl, int *flag) {
  int i, c;
  Table *h, *t;
  TValue *val;
  t = hvalue(tbl);
  h = luaH_new(L, t->sizearray, 0);
  c = 0;
  *flag = 0;
  for (i=0; i < t->sizearray; i++) {
    if (ttisnotnil(&t->array[i])) {
      setobj2s(L, L->top, &t->array[i]);
      luaH_setint(L, h, ++c, L->top);  /* 4.6.3 tweak */
      luaC_barriert(L, h, L->top);
    }
  }
  for (i=0; i < sizenode(t); i++) {
    val = gval(gnode(t, i));
    if (ttisnotnil(val)) {
      setobj2s(L, L->top, val);
      setobj2t(L, luaH_setnum(L, h, cast_num(++c)), L->top);
      luaC_barriert(L, h, L->top);
      *flag = 1;
    }
  }
  luaH_resizearray(L, h, c);
  sethvalue(L, L->top++, h);
  return c;
}


LUAI_FUNC void agenaV_tparts (lua_State *L, TValue *tbl, int hash) {
  int i;
  Table *h, *t;
  TValue *val;
  t = hvalue(tbl);
  h = luaH_new(L, (!hash)*t->sizearray, hash*sizenode(t));
  for (i=0; !hash && i < t->sizearray; i++) {
    if (ttisnotnil(&t->array[i])) {
      setobj2s(L, L->top, &t->array[i]);
      luaH_setint(L, h, i + 1, L->top);  /* 4.6.3 tweak */
      luaC_barriert(L, h, L->top);
    }
  }
  for (i=0; hash && i < sizenode(t); i++) {
    Node *n = gnode(t, i);
    val = gval(n);
    if (ttisnotnil(val)) {
      agenaV_setops_settodict(L, h, key2tval(n), val);
    }
  }
  /* we deliberately do not resize arrays */
  sethvalue(L, L->top++, h);
}


/* int agenaV_tisofnumerictype (lua_State *L, TValue *tbl, int (*fn)(double)):
   agenaV_tisnonnegative -> tools_isnonnegative
   agenaV_tisnonnegint   -> tools_isnonnegint
   agenaV_tispositive    -> tools_ispositive
   agenaV_tisposint      -> tools_isposint       */
LUAI_FUNC int agenaV_tisofnumerictype (lua_State *L, TValue *tbl, int (*fn)(double), int integralkeysonly) {  /* 3.15.4 */
  int i, c;
  TValue *key, *val;
  Node *g;
  Table *t = hvalue(tbl);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < t->sizearray; i++) {
    val = &t->array[i];
    if (ttisnotnil(val)) {
      c++;
      if (!ttisnumber(val) || !fn(nvalue(val))) return 0;
    }
  }
  for (i=0; i < sizenode(t); i++) {
    g = gnode(t, i);
    key = key2tval(g);
    if (integralkeysonly && (!ttisnumber(key) || tools_isfrac(nvalue(key)))) continue;  /* 3.19.2 extension */
    val = gval(g);
    if (ttisnotnil(val)) {
      c++;
      if (!ttisnumber(val) || !fn(nvalue(val))) return 0;
    }
  }
  return c && 1;
}


/* int agenaV_usisofnumerictype (lua_State *L, TValue *set, int (*fn)(double)):
   agenaV_tisnonnegative -> tools_isnonnegative
   agenaV_tisnonnegint   -> tools_isnonnegint
   agenaV_tispositive    -> tools_ispositive
   agenaV_tisposint      -> tools_isposint       */
LUAI_FUNC int agenaV_usisofnumerictype (lua_State *L, TValue *tbl, int (*fn)(double)) {  /* 3.19.2 */
  int i, c;
  TValue *val;
  UltraSet *t = usvalue(tbl);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < sizenode(t); i++) {
    if (ttisnotnil(glkey(gnode(t, i)))) {
      c++;
      val = key2tval(gnode(t, i));
      if (!ttisnumber(val) || !fn(nvalue(val))) return 0;
    }
  }
  return c && 1;
}


/* int agenaV_seqisofnumerictype (lua_State *L, TValue *tbl, int (*fn)(double)):
   agenaV_*isnonnegative -> tools_isnonnegative
   agenaV_*isnonnegint   -> tools_isnonnegint
   agenaV_*ispositive    -> tools_ispositive
   agenaV_*isposint      -> tools_isposint       */
LUAI_FUNC int agenaV_seqisofnumerictype (lua_State *L, TValue *s, int (*fn)(double)) {  /* 3.15.4 */
  int i;
  TValue *val;
  Seq *h = seqvalue(s);
  for (i=0; i < h->size; i++) {
    val = seqitem(h, i);
    if (ttisnotnil(val) && (!ttisnumber(val) || !fn(nvalue(val)))) return 0;
  }
  return h->size && 1;  /* 3.19.2 fix */
}


/* int agenaV_regisofnumerictype (lua_State *L, TValue *tbl, int (*fn)(double)):
   agenaV_*isnonnegative -> tools_isnonnegative
   agenaV_*isnonnegint   -> tools_isnonnegint
   agenaV_*ispositive    -> tools_ispositive
   agenaV_*isposint      -> tools_isposint       */
LUAI_FUNC int agenaV_regisofnumerictype (lua_State *L, TValue *s, int (*fn)(double)) {  /* 3.15.4 */
  int i, c;
  TValue *val;
  Reg *h = regvalue(s);
  c = 0;  /* 3.19.2 fix */
  for (i=0; i < h->top; i++) {
    val = regitem(h, i);
    if (ttisnotnil(val)) {
      c++;
      if (!ttisnumber(val) || !fn(nvalue(val))) return 0;
    }
  }
  return c && 1;  /* 3.19.2 fix */
}


/* SPLIT operator, 0.6.0 & 0.11.2; patched 0.25.5, 05.08.2009 */

LUAI_FUNC void agenaV_split (lua_State *L, const char *s, const char *sep, StkId idx) {
  Seq *h;
  const char *e;
  int c;
  size_t len;
  h = agnSeq_new(L, 16);  /* changed 2.29.1 */
  c = 0;
  len = tools_strlen(sep);  /* 2.17.8 tweak */
  if (len == 0) {  /* 2.37.8, does not recognise embedded zeros */
    size_t l, c;
    l = tools_strlen(s); c = 0;
    while (l--) {
      setsvalue2s(L, L->top, luaS_newlstr(L, s++, 1));
      agnSeq_seti(L, h, cast_num(++c), L->top);
    }
    goto end;
  }
  if (tools_streq(s, sep) || !s) {  /* separator is empty string or s = sep or s == NULL ?, 2.16.12 tweak, 2.29.0 fix */
    setnilvalue(L->top);
    luaC_checkGC(L);
    goto end;
  }
  while ((e=strstr(s, sep)) != NULL) {  /* strtok is slower */
    c++;
    setsvalue2s(L, L->top, luaS_newlstr(L, s, e - s));
    agnSeq_seti(L, h, cast_num(c), L->top);
    luaC_barrierseq(L, h, L->top);
    s = e + len;
  }
  /* process last token */
  setsvalue2s(L, L->top, luaS_newlstr(L, s, tools_strlen(s)));  /* 2.17.8 tweak */
  agnSeq_seti(L, h, cast_num(c + 1), L->top);
end:
  luaC_barrierseq(L, h, L->top);  /* for above exception handling, 2.29.0 fix */
  setseqvalue(L, idx, h);
  luaC_checkGC(L);
}


/* COPY operator, 0.9.0; extended 1.1.0 to treat references accordingly; patched 1.7.1; extended 1.8.9; patched 3.3.7;
   tuned 7.5.5 by adapting ideas taken from the MIT-licenced source code of Luau, available at
   https://github.com/luau-lang/luau/blob/master/VM/src/ltable.cpp
   with the help of Gemini AI. */

#define ttiscontainer(o) (ttype(o) >= LUA_TTABLE && ttype(o) <= LUA_TSET)

LUAI_FUNC void agenaV_copy (lua_State *L, const TValue *tbl, StkId idx, Table *seen, int what) {
  luaD_checkstack(L, 1);  /* since the function calls itself recursively, better be sure than sorry, 2.31.7 fix */
  if (ttistable(tbl)) {
    Table *t, *dest;
    TValue *oldval, *old;
    int i;
    t = hvalue(tbl);
    /* Agena 1.1.0: check whether table has already been traversed. If so, just return its reference, otherwise initialise seen table */
    if (seen == NULL) {
      seen = luaH_new(L, 0, 0);
    } else {
      const TValue *d = luaH_get(seen, tbl);  /* seen[src], if assigned copy the reference only, not the structure itself */
      if (ttisnotnil(d)) {
        setobj2s(L, L->top, d);
        return;
      }
    }
    /* register new table in seen, ala seen[src] := dest */
    dest = luaH_new(L, (what & 1) ? t->sizearray : 0, (what & 2) ? sizenode(t) : 0);
    sethvalue(L, L->top, dest);  /* 1.7.1 patch */
    old = luaH_set(L, seen, tbl);  /* do a primitive set, key = original table */
    setobj2t(L, old, L->top);  /* set new table dest to seen as its value */
    luaC_barriert(L, seen, L->top);
    if (what & 1 && t->sizearray > 0) {
      /* 7.5.5 Bulk-copy all values at once - very fast for numbers/strings; this actually does not change anything
         functionally, as the old version did _not_ create new `duplicate` tables if they were included in the
         array part, but just copied the references to the old tables */
      memcpy(dest->array, t->array, t->sizearray * sizeof(TValue));
      for (i=0; i < dest->sizearray; i++) {
        TValue *v = &dest->array[i];
        /* Check if the copied value is a structure that needs its own deep copy */
        if (ttiscontainer(v)) {
          checkstack(L, "in ", "copy");  /* prevent crashes with deeply nested tables */
          /* Copy nested structure into L->top, then move it into the array slot */
          agenaV_copy(L, v, L->top, seen, what);  /* copy value and put it at L->top */
          setobj2t(L, v, L->top);
          luaC_barrier(L, dest, v);
        }
      }
    }
    if (what & 2) {  /* get hash part */
      for (i=0; i < sizenode(t); i++) {
        Node *n = gnode(t, i);
        if (!ttisnil(gval(n))) {
          setobj2s(L, L->top, key2tval(n));  /* push key */
          if (ttiscontainer(L->top)) {
            checkstack(L, "in ", "copy");  /* prevent crashes with deeply nested tables */
            agenaV_copy(L, key2tval(n), L->top, seen, what);
          }
          /* Process Value */
          L->top++;
          setobj2s(L, L->top, gval(n));
          if (ttiscontainer(L->top)) {
            checkstack(L, "in ", "copy");
            agenaV_copy(L, gval(n), L->top, seen, what);  /* copy value and put it at L->top */
          }
          /* Use luaH_set to ensure keys are hashed correctly in the new table */
          oldval = luaH_set(L, dest, L->top - 1);
          setobj2t(L, oldval, L->top);
          luaC_barriert(L, dest, L->top);
          L->top--;
        }
      }
    }
    /* return new table */
    sethvalue(L, idx, dest);
    if (what == 3) {  /* add metatable of original table, 2.29.1 fix */
      agenaV_setmetatable(L, idx, t->metatable, 0);
      agenaV_setutype(L, idx, t->type);
    }
  } else if (ttisuset(tbl)) {
    UltraSet *t, *dest;
    TValue *old;
    int i;
    t = usvalue(tbl);
    /* Agena 1.1.0: check whether set has already been traversed. If so, just return its reference, otherwise initialise seen table */
    if (seen == NULL) {
      seen = luaH_new(L, 0, 0);
    } else {
      const TValue *dest;
      dest = luaH_get(seen, tbl);  /* seen[src], if assigned copy the reference only, not the structure itself */
      if (ttisnotnil(dest)) {
        setobj2s(L, L->top, dest);
        return;
      }
    }
    /* register new set in seen, ala seen[src] := dest */
    dest = agnUS_new(L, sizenode(t));
    setusvalue(L, L->top, dest);  /* 1.7.1 patch */
    old = luaH_set(L, seen, tbl);  /* do a primitive set */
    setobj2t(L, old, L->top);
    luaC_barriert(L, seen, L->top);
    for (i=0; i < sizenode(t); i++) {
      if (!ttisnil(glkey(glnode(t, i)))) {
        setobj2s(L, L->top, key2tval(glnode(t, i)));
        int tt = ttype(L->top);
        if (tt >= LUA_TTABLE && tt <= LUA_TSET) {  /* make a deep copy, 7.5.5 tweak */
          checkstack(L, "in ", "copy");  /* prevent crashes with deeply nested sets */
          agenaV_copy(L, key2tval(glnode(t, i)), L->top, seen, what);
        }
        agnUS_set(L, dest, L->top);
        luaC_barrierset(L, dest, L->top);  /* Agena 1.2 fix */
      }
    }
    /* return new set */
    setusvalue(L, idx, dest);
    if (what != 7) {  /* 2.27.4 extension */
      /* add metatable of original table, 2.29.1 fix */
      agenaV_setmetatable(L, idx, t->metatable, 0);
      agenaV_setutype(L, idx, t->type);
    }
  } else if (ttisseq(tbl)) {
    Seq *s, *dest;
    TValue *old;
    int i;
    api_check(L, L->top < L->ci->top);
    s = seqvalue(tbl);
    /* Agena 1.1.0: check whether set has already been traversed. If so, just return its reference, otherwise initialise seen table */
    if (seen == NULL) {
      seen = luaH_new(L, 0, 0);
    } else {
      const TValue *dest;
      dest = luaH_get(seen, tbl);  /* seen[src], if assigned copy the reference only, not the structure itself */
      if (ttisnotnil(dest)) {
        setobj2s(L, L->top, dest);
        return;
      }
    }
    /* register new sequence in seen, ala seen[src] := dest */
    dest = agnSeq_new(L, s->maxsize);
    setseqvalue(L, L->top, dest);  /* 1.7.1 patch */
    old = luaH_set(L, seen, tbl);  /* do a primitive set, key = original table */
    setobj2t(L, old, L->top);  /* set new table dest to seen as its value */
    luaC_barriert(L, seen, L->top);
    /* Optimization: Bulk copy the TValue stack and update size immediately */
    if (s->size > 0) {
      memcpy(dest->array, s->array, s->size * sizeof(TValue));
      dest->size = s->size;
      for (i=0; i < dest->size; i++) {
        TValue *v = &dest->array[i];
        if (ttiscontainer(v)) {  /* make a deep copy, 7.5.5 tweak */
          checkstack(L, "in ", "copy");  /* prevent crashes with deeply nested sequences */
          agenaV_copy(L, v, L->top, seen, what);
          setobj2t(L, v, L->top);
          luaC_barrierseq(L, dest, v);
        }
      }
    }
    /* return new sequence */
    setseqvalue(L, idx, dest);
    if (what != 7) {  /* 2.27.4 extension */
      /* add metatable of original table, 2.29.1 fix */
      agenaV_setmetatable(L, idx, s->metatable, 0);
      agenaV_setutype(L, idx, s->type);
    }
  }
  else if (ttispair(tbl)) {  /* added with 1.8.9 */
    Pair *s, *dest;
    TValue *old;
    int i;
    api_check(L, L->top < L->ci->top);
    s = pairvalue(tbl);
    /* Agena 1.1.0: check whether set has already been traversed. If so, just return its reference, otherwise initialise seen table */
    if (seen == NULL) {
      seen = luaH_new(L, 0, 0);
    } else {
      const TValue *dest;
      dest = luaH_get(seen, tbl);  /* seen[src], if assigned copy the reference only, not the structure itself */
      if (ttisnotnil(dest)) {
        setobj2s(L, L->top, dest);
        return;
      }
    }
    /* register new pair in seen, ala seen[src] := dest */
    dest = agnPair_new(L);
    setpairvalue(L, L->top, dest);  /* 1.7.1 patch */
    old = luaH_set(L, seen, tbl);  /* do a primitive set, key = original table */
    setobj2t(L, old, L->top);  /* set new table dest to seen as its value */
    luaC_barriert(L, seen, L->top);
    for (i=0; i < 2; i++) {
      TValue *src_v = &s->array[i];
      TValue *dst_v = &dest->array[i];
      setobj2t(L, dst_v, src_v);
      if (ttiscontainer(dst_v)) {  /* make a deep copy, 7.5.5 tweak */
        checkstack(L, "in ", "copy");  /* prevent crashes with deeply nested pairs */
        /* use dst_v as source for recursion */
        agenaV_copy(L, src_v, L->top, seen, what);
        setobj2t(L, dst_v, L->top);
      }
      luaC_barrierpair(L, dest, dst_v);
    }
    /* return new pair */
    setpairvalue(L, idx, dest);
    if (what != 7) {  /* 2.27.4 extension */
      /* add metatable of original table, 2.29.1 fix */
      agenaV_setmetatable(L, idx, s->metatable, 0);
      agenaV_setutype(L, idx, s->type);
    }
  } else if (ttisreg(tbl)) {
    Reg *s, *dest;
    TValue *old;
    int i;
    api_check(L, L->top < L->ci->top);
    s = regvalue(tbl);
    /* Agena 1.1.0: check whether set has already been traversed. If so, just return its reference, otherwise initialise seen table */
    if (seen == NULL) {
      seen = luaH_new(L, 0, 0);
    } else {
      const TValue *dest;
      dest = luaH_get(seen, tbl);  /* seen[src], if assigned copy the reference only, not the structure itself */
      if (ttisnotnil(dest)) {
        setobj2s(L, L->top, dest);
        return;
      }
    }
    /* register new register in seen, ala seen[src] := dest */
    dest = agnReg_new(L, s->maxsize);
    setregvalue(L, L->top, dest);  /* 1.7.1 patch */
    old = luaH_set(L, seen, tbl);  /* do a primitive set, key = original table */
    setobj2t(L, old, L->top);  /* set new table dest to seen as its value */
    luaC_barriert(L, seen, L->top);
    /* Optimization: Bulk copy the TValue array and update top immediately */
    if (s->top > 0) {
      memcpy(dest->array, s->array, s->top * sizeof(TValue));
      dest->top = s->top;
      for (i=0; i < dest->top; i++) {
        TValue *v = &dest->array[i];
        if (ttiscontainer(v)) {  /* make a deep copy, 7.5.5 tweak */
          checkstack(L, "in ", "copy");  /* prevent crashes with deeply nested sequences */
          /* Recursive copy using L->top as temporary storage */
          agenaV_copy(L, v, L->top, seen, what);
          setobj2t(L, v, L->top);
          luaC_barrierreg(L, dest, v);
        }
      }
    }
    /* return new sequence */
    setregvalue(L, idx, dest);
    if (what != 7) {  /* 2.27.4 extension */
      /* add metatable of original table, 2.29.1 fix */
      agenaV_setmetatable(L, idx, s->metatable, 0);
      agenaV_setutype(L, idx, s->type);
    }
  }
  else
    luaG_runerror(L, "Error in " LUA_QS ": table, set, sequence, register or pair expected, got %s.", "copy",
      luaT_typenames[(int)ttype(tbl)]);
}


/* SUMUP operator for tables, 0.9.0, added 10.01.2008; changed 2.4.3, 17/02/2015 */

/* Kahan-Babuška summation */
#define sumup_initvalues(s,t,c,cc,cs,ccs,x,z) { \
  x = 0; \
  z[0] = s[0] = t[0] = c[0] = cc[0] = cs[0] = ccs[0] = 0; \
  z[1] = s[1] = t[1] = c[1] = cc[1] = cs[1] = ccs[1] = 0; \
}

#define sumup_kahanbabuska(val,x,z,t,c,s,cc,cs,ccs,flag) { \
  if (ttisnumber(val)) { \
    if (flag == LUA_TNIL) flag = LUA_TNUMBER; \
    x = nvalue(val); \
    t[0] = s[0] + x; \
    c[0] = (fabs(s[0]) >= fabs(x)) ? (s[0] - t[0]) + x : (x - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
  } else if (ttiscomplex(val)) { \
    if (flag == LUA_TNIL || flag == LUA_TNUMBER) flag = LUA_TCOMPLEX; \
    z[0] = complexreal(val); z[1] = compleximag(val); \
    t[0] = s[0] + z[0]; \
    c[0] = (fabs(s[0]) >= fabs(z[0])) ? (s[0] - t[0]) + z[0] : (z[0] - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
    t[1] = s[1] + z[1]; \
    c[1] = (fabs(s[1]) >= fabs(z[1])) ? (s[1] - t[1]) + z[1] : (z[1] - t[1]) + s[1]; \
    s[1] = t[1]; \
    t[1] = cs[1] + c[1]; \
    cc[1] = (fabs(cs[1]) >= fabs(c[1])) ? (cs[1] - t[1]) + c[1] : (c[1] - t[1]) + cs[1]; \
    cs[1] = t[1]; \
    ccs[1] = ccs[1] + cc[1]; \
  } \
}

#define sumup_finalise(idx,s,cs,ccs,flag) { \
  switch (flag) { \
    case LUA_TNUMBER: { \
      setnvalue(idx, s[0] + cs[0] + ccs[0]); \
      break; \
    } \
    case LUA_TCOMPLEX: { \
      setrealimagvalue(idx, s[0] + cs[0] + ccs[0], s[1] + cs[1] + ccs[1]); \
      break; \
    } \
    default: { \
      setnilvalue(idx); \
    } \
  } \
}

LUAI_FUNC void agenaV_sumup (lua_State *L, const TValue *obj, StkId idx, TMS event) {
  Table *h;
  Node *key;
  TValue *val;
  const TValue *tm;
  int i;  /* Agena 1.6.0 */
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2];
  sumup_initvalues(s, t, c, cc, cs, ccs, x, z);
  h = hvalue(obj);
  tm = luaT_gettmbyobj(L, obj, event);
  if (ttisnil(tm)) {  /* no metamethod ? */
    int flag = LUA_TNIL;
    for (i=0; i < h->sizearray; i++) {
      val = &h->array[i];
      sumup_kahanbabuska(val, x, z, t, c, s, cc, cs, ccs, flag);
    }
    for (i -= h->sizearray; i < sizenode(h); i++) {
      key = gnode(h, i);
      if (ttisint(key2tval(key))) {  /* 3.15.3, ignore all non-integral keys in the hash part, behave like addup, mulup, qsumup */
        val = gval(key);
        sumup_kahanbabuska(val, x, z, t, c, s, cc, cs, ccs, flag);
      }
    }
    sumup_finalise(idx, s, cs, ccs, flag);
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, event);
}


/* SUMUP operator for sequences, 0.11.2, added 09.06.2008; changed 2.4.3, 17/02/2015 */

LUAI_FUNC void agenaV_seqsumup (lua_State *L, const TValue *obj, StkId idx, TMS event) {
  Seq *h;
  TValue *val;
  const TValue *tm;
  size_t i;  /* Agena 1.6.0 */
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2];
  sumup_initvalues(s, t, c, cc, cs, ccs, x, z);
  h = seqvalue(obj);
  tm = luaT_gettmbyobj(L, obj, event);
  if (ttisnil(tm)) {  /* no metamethod ? */
    int flag = 0;
    for (i=0; i < h->size; i++) {
      val = seqitem(h, i);
      sumup_kahanbabuska(val, x, z, t, c, s, cc, cs, ccs, flag);
    }
    sumup_finalise(idx, s, cs, ccs, flag);
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, event);
}


/* SUMUP operator for registers, added 2.4.0; changed 2.4.3, 17/02/2015 */

LUAI_FUNC void agenaV_regsumup (lua_State *L, const TValue *obj, StkId idx, TMS event) {
  Reg *h;
  TValue *val;
  const TValue *tm;
  size_t i;  /* Agena 1.6.0 */
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2];
  sumup_initvalues(s, t, c, cc, cs, ccs, x, z);
  h = regvalue(obj);
  tm = luaT_gettmbyobj(L, obj, event);
  if (ttisnil(tm)) {  /* no metamethod ? */
    int flag = 0;
    for (i=0; i < h->top; i++) {
      val = regitem(h, i);
      sumup_kahanbabuska(val, x, z, t, c, s, cc, cs, ccs, flag);
    }
    sumup_finalise(idx, s, cs, ccs, flag);
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, event);
}


/* QSUMUP operator for tables, 0.9.1, added 13.01.2008, Kahan-Babuška summation added 2.30.2;

   See: https://stackoverflow.com/questions/3709939/minimizing-the-effect-of-rounding-errors-caused-by-repeated-operations-effective

   "For multiplication, there are algorithms to convert a product of two floating-point numbers into a sum of two floating-point numbers
   without rounding, at which point you can use Kahan summation or some other method, depending on what you need to do next with the
   product.

   If you have FMA (fused multiply-add) available, this can easily be accomplished as follows:

   p = a*b;
   r = fma(a,b,-p);

   After these two operations, if no overflow or underflow occurs, p + r is exactly equal to a * b without rounding. This can also be
   accomplished without FMA, but it is rather more difficult. If you're interested in these algorithms, you might start by downloading
   the crlibm documentation, which details several of them." */

/* Kahan-Babuška summation */

#define qsumup_initvalues(s,p,r,t,c,cc,cs,ccs,x,z) { \
  x = 0; \
  z[0] = p[0] = r[0] = s[0] = t[0] = c[0] = cc[0] = cs[0] = ccs[0] = 0; \
  z[1] = p[1] = r[1] = s[1] = t[1] = c[1] = cc[1] = cs[1] = ccs[1] = 0; \
}

#define qsumup_kahanbabuska(val,x,z,p,t,c,s,cc,cs,ccs,flag) { \
  if (ttisnumber(val)) { \
    if (flag == LUA_TNIL) flag = LUA_TNUMBER; \
    x = nvalue(val); \
    p[0] = x*x; \
    r[0] = fma(x, x, -p[0]); \
    x = p[0] + r[0]; \
    t[0] = s[0] + x; \
    c[0] = (fabs(s[0]) >= fabs(x)) ? (s[0] - t[0]) + x : (x - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
  } else if (ttiscomplex(val)) { \
    if (flag == LUA_TNIL || flag == LUA_TNUMBER) flag = LUA_TCOMPLEX; \
    lua_Number y[2]; \
    y[0] = complexreal(val); y[1] = compleximag(val); \
    p[0] = y[0]*y[0] - y[1]*y[1]; \
    r[0] = fma(y[0] + y[1], y[0] - y[1], -p[0]); \
    z[0] = p[0] + r[0]; \
    t[0] = s[0] + z[0]; \
    c[0] = (fabs(s[0]) >= fabs(z[0])) ? (s[0] - t[0]) + z[0] : (z[0] - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
    p[1] = 2*y[0]*y[1]; \
    r[1] = fma(2*y[0], y[1], -p[1]); \
    z[1] = p[1] + r[1]; \
    t[1] = s[1] + z[1]; \
    c[1] = (fabs(s[1]) >= fabs(z[1])) ? (s[1] - t[1]) + z[1] : (z[1] - t[1]) + s[1]; \
    s[1] = t[1]; \
    t[1] = cs[1] + c[1]; \
    cc[1] = (fabs(cs[1]) >= fabs(c[1])) ? (cs[1] - t[1]) + c[1] : (c[1] - t[1]) + cs[1]; \
    cs[1] = t[1]; \
    ccs[1] = ccs[1] + cc[1]; \
  } \
}

#define qsumup_finalise sumup_finalise

LUAI_FUNC void agenaV_qsumup (lua_State *L, const TValue *obj, StkId idx, int nargs) {
  Table *h;
  Node *key;
  TValue *val;
  const TValue *tm;
  int i;  /* Agena 1.6.0 */
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2], p[2], r[2];
  lua_lock(L);
  if (nargs == 0)
    luaG_runerror(L, "Error in " LUA_QS ": need at least one argument.", "qsumup");
  qsumup_initvalues(s, p, r, t, c, cc, cs, ccs, x, z);
  h = hvalue(obj);
  if (nargs == 1) {
    if ((tm = fasttm(L, h->metatable, TM_QSUMUP)) == NULL) {  /* no metamethod ? */
      int flag = 0;
      for (i=0; i < h->sizearray; i++) {
        val = &h->array[i];
        qsumup_kahanbabuska(val, x, z, p, t, c, s, cc, cs, ccs, flag);
      }
      for (i -= h->sizearray; i < sizenode(h); i++) {
        key = gnode(h, i);
        if (ttisint(key2tval(key))) {  /* 3.15.3, ignore all non-integral keys in the hash part, behave like addup, mulup */
          val = gval(key);
          qsumup_kahanbabuska(val, x, z, p, t, c, s, cc, cs, ccs, flag);
        }
      }
      qsumup_finalise(idx, s, cs, ccs, flag);
    } else if (ttisfunction(tm))
      call_binTM(L, obj, luaO_nilobject, idx, TM_QSUMUP);
  } else if (ttisnumber(idx + 1)) {  /* nargs >= 2 && number, 4.5.0 extension */
    lua_Number d = nvalue(idx + 1);
    agenaV_qsumup_div(L, obj, idx, d);
  }
  lua_unlock(L);
}


/* QSUMUP operator for sequences, 0.11.2, added 09.06.2008 */

LUAI_FUNC void agenaV_seqqsumup (lua_State *L, const TValue *obj, StkId idx, int nargs) {
  Seq *h;
  TValue *val;
  const TValue *tm;
  size_t i;  /* Agena 1.6.0 */
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2], p[2], r[2];
  lua_lock(L);
  if (nargs == 0)
    luaG_runerror(L, "Error in " LUA_QS ": need at least one argument.", "qsumup");
  qsumup_initvalues(s, p, r, t, c, cc, cs, ccs, x, z);
  h = seqvalue(obj);
  api_check(L, L->top < L->ci->top);
  if (nargs == 1) {
    if ((tm = fasttm(L, h->metatable, TM_QSUMUP)) == NULL) {  /* no metamethod ? */
      int flag = 0;
      for (i=0; i < h->size; i++) {
        val = seqitem(h, i);
        qsumup_kahanbabuska(val, x, z, p, t, c, s, cc, cs, ccs, flag);
      }
      qsumup_finalise(idx, s, cs, ccs, flag);
    } else if (ttisfunction(tm))
      call_binTM(L, obj, luaO_nilobject, idx, TM_QSUMUP);
  } else if (ttisnumber(idx + 1)) {  /* nargs >= 2 && number, 4.5.0 extension */
    lua_Number d = nvalue(idx + 1);
    agenaV_seqqsumup_div(L, obj, idx, d);
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": invalid arguments.", "qsumup");
  }
  lua_unlock(L);
}


/* QSUMUP operator for registers, added 2.4.0, 2.30.2 Kahan-Babuška summation */

LUAI_FUNC void agenaV_regqsumup (lua_State *L, const TValue *obj, StkId idx, int nargs) {
  Reg *h;
  TValue *val;
  const TValue *tm;
  size_t i;  /* Agena 1.6.0 */
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2], p[2], r[2];
  lua_lock(L);
  if (nargs == 0)
    luaG_runerror(L, "Error in " LUA_QS ": need at least one argument.", "qsumup");
  qsumup_initvalues(s, p, r, t, c, cc, cs, ccs, x, z);
  h = regvalue(obj);
  api_check(L, L->top < L->ci->top);
  if (nargs == 1) {
    if ((tm = fasttm(L, h->metatable, TM_QSUMUP)) == NULL) {  /* no metamethod ? */
      int flag = 0;
      for (i=0; i < h->top; i++) {
        val = seqitem(h, i);
        qsumup_kahanbabuska(val, x, z, p, t, c, s, cc, cs, ccs, flag);
      }
      qsumup_finalise(idx, s, cs, ccs, flag);
    } else if (ttisfunction(tm))
      call_binTM(L, obj, luaO_nilobject, idx, TM_QSUMUP);
  } else if (ttisnumber(idx + 1)) {  /* nargs >= 2 && number, 4.5.0 extension */
    lua_Number d = nvalue(idx + 1);
    agenaV_regqsumup_div(L, obj, idx, d);
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": invalid arguments.", "qsumup");
  }
  lua_unlock(L);
}


#define qsumup_initvalues_div qsumup_initvalues

#define qsumup_kahanbabuska_div(val,x,z,d,p,t,c,s,cc,cs,ccs,flag) { \
  if (ttisnumber(val)) { \
    if (flag == LUA_TNIL) flag = LUA_TNUMBER; \
    x = nvalue(val); \
    if (d == 0.0) { \
      p[0] = AGN_NAN; \
      r[0] = AGN_NAN; \
    } else { \
      p[0] = (x/d)*x; \
      r[0] = fma(x/d, x, -p[0]); \
    } \
    x = p[0] + r[0]; \
    t[0] = s[0] + x; \
    c[0] = (fabs(s[0]) >= fabs(x)) ? (s[0] - t[0]) + x : (x - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
  } else if (ttiscomplex(val)) { \
    lua_Number y[2]; \
    if (flag == LUA_TNIL || flag == LUA_TNUMBER) flag = LUA_TCOMPLEX; \
    y[0] = complexreal(val); y[1] = compleximag(val); \
    if (d == 0.0) { \
      p[0] = AGN_NAN; \
      r[0] = AGN_NAN; \
    } else { \
      p[0] = (y[0]*y[0] - y[1]*y[1])/d; \
      r[0] = fma((y[0] + y[1])/d, (y[0] - y[1]), -p[0]); \
    } \
    z[0] = p[0] + r[0]; \
    t[0] = s[0] + z[0]; \
    c[0] = (fabs(s[0]) >= fabs(z[0])) ? (s[0] - t[0]) + z[0] : (z[0] - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
    if (d == 0.0) { \
      p[1] = AGN_NAN; \
      r[1] = AGN_NAN; \
    } else { \
      p[1] = 2*y[0]*y[1]/d; \
      r[1] = fma(2*y[0]/d, y[1], -p[1]); \
    } \
    z[1] = p[1] + r[1]; \
    t[1] = s[1] + z[1]; \
    c[1] = (fabs(s[1]) >= fabs(z[1])) ? (s[1] - t[1]) + z[1] : (z[1] - t[1]) + s[1]; \
    s[1] = t[1]; \
    t[1] = cs[1] + c[1]; \
    cc[1] = (fabs(cs[1]) >= fabs(c[1])) ? (cs[1] - t[1]) + c[1] : (c[1] - t[1]) + cs[1]; \
    cs[1] = t[1]; \
    ccs[1] = ccs[1] + cc[1]; \
  } \
}

#define qsumup_finalise_div sumup_finalise

static lua_Number aux_getstructuresize (lua_State *L, const TValue *obj, StkId idx) {  /* 3.6.1 */
  switch (ttype(idx)) {
    case LUA_TTABLE: {
      return agenaV_nops(L, hvalue(obj));
    }
    case LUA_TSEQ: {
      return seqvalue(obj)->size;
    }
    case LUA_TREG: {
      return regvalue(obj)->top;
    }
    case LUA_TUSERDATA: {
      return -1;
    }
    default: {
      luaG_runerror(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.",
        "addup", luaT_typenames[(int)ttype(idx)]);
    }
  }
  return -1;
}

LUAI_FUNC void agenaV_qsumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d) {
  Table *h;
  Node *key;
  TValue *val;
  const TValue *tm;
  int i;  /* Agena 1.6.0 */
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2], p[2], r[2];
  lua_lock(L);
  if (tools_isnan(d) || d == 0)  /* determine structure size automatically */
    d = aux_getstructuresize(L, obj, idx);
  if (d == 0) {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": %s is empty or 2nd argument is zero.", "qsumup", luaT_typenames[(int)ttype(idx)]);
  }
  qsumup_initvalues_div(s, p, r, t, c, cc, cs, ccs, x, z);
  h = hvalue(obj);
  if ((tm = fasttm(L, h->metatable, TM_QSUMUP)) == NULL) {  /* no metamethod ? */
    int flag = 0;
    for (i=0; i < h->sizearray; i++) {
      val = &h->array[i];
      qsumup_kahanbabuska_div(val, x, z, d, p, t, c, s, cc, cs, ccs, flag);
    }
    for (i -= h->sizearray; i < sizenode(h); i++) {
      key = gnode(h, i);
      if (ttisint(key2tval(key))) {  /* 3.15.3, ignore all non-integral keys in the hash part, behave like addup, mulup */
        val = gval(key);
        qsumup_kahanbabuska_div(val, x, z, d, p, t, c, s, cc, cs, ccs, flag);
      }
    }
    qsumup_finalise_div(idx, s, cs, ccs, flag);
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, TM_QSUMUP);
  lua_unlock(L);
}


/* QSUMUP operator for sequences, 0.11.2, added 09.06.2008 */

LUAI_FUNC void agenaV_seqqsumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d) {
  Seq *h;
  TValue *val;
  const TValue *tm;
  size_t i;  /* Agena 1.6.0 */
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2], p[2], r[2];
  lua_lock(L);
  if (tools_isnan(d) || d == 0)  /* determine structure size automatically */
    d = aux_getstructuresize(L, obj, idx);
  if (d == 0) {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": %s is empty or 2nd argument is zero.", "qsumup", luaT_typenames[(int)ttype(idx)]);
  }
  qsumup_initvalues_div(s, p, r, t, c, cc, cs, ccs, x, z);
  h = seqvalue(obj);
  api_check(L, L->top < L->ci->top);
  if ((tm = fasttm(L, h->metatable, TM_QSUMUP)) == NULL) {  /* no metamethod ? */
    int flag = 0;
    for (i=0; i < h->size; i++) {
      val = seqitem(h, i);
      qsumup_kahanbabuska_div(val, x, z, d, p, t, c, s, cc, cs, ccs, flag);
    }
    qsumup_finalise_div(idx, s, cs, ccs, flag);
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, TM_QSUMUP);
  lua_unlock(L);
}


/* QSUMUP operator for registers, added 2.4.0, 2.30.2 Kahan-Babuška summation */

LUAI_FUNC void agenaV_regqsumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d) {
  Reg *h;
  TValue *val;
  const TValue *tm;
  size_t i;  /* Agena 1.6.0 */
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2], p[2], r[2];
  lua_lock(L);
  if (tools_isnan(d) || d == 0)  /* determine structure size automatically */
    d = aux_getstructuresize(L, obj, idx);
  if (d == 0) {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": %s is empty or 2nd argument is zero.", "qsumup", luaT_typenames[(int)ttype(idx)]);
  }
  qsumup_initvalues_div(s, p, r, t, c, cc, cs, ccs, x, z);
  h = regvalue(obj);
  api_check(L, L->top < L->ci->top);
  if ((tm = fasttm(L, h->metatable, TM_QSUMUP)) == NULL) {  /* no metamethod ? */
    int flag = 0;
    for (i=0; i < h->top; i++) {
      val = regitem(h, i);
      qsumup_kahanbabuska_div(val, x, z, d, p, t, c, s, cc, cs, ccs, flag);
    }
    qsumup_finalise_div(idx, s, cs, ccs, flag);
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, TM_QSUMUP);
  lua_unlock(L);
}


/* MULUP operator for tables, sequences and registers, 2.2.0, added May 09, 2014; merged 2.30.2 */

#define mulup_fn(val,x,r,flag,arg) { \
  if (ttisnumber(val)) { \
    x = nvalue(val); \
    r *= (long double)(x); \
    flag = 1; \
  } else if (ttistrue(val) && ttisnumber(arg)) { \
    x = nvalue(arg); \
    r *= (long double)(x); \
    flag = 1; \
  } \
}

LUAI_FUNC void agenaV_mulup (lua_State *L, const TValue *obj, StkId idx, int nargs) {
  TValue *val;
  const TValue *tm;
  long double p;
  int flag;
#ifndef PROPCMPLX
  agn_Complex p_imag = 1 + I*0;
#else
  lua_Number p_imag[2];
  agnCmplx_create(p_imag, 1, 0);
#endif
  flag = 0;
  p = 1.0L;
  api_check(L, L->top < L->ci->top);
  if (nargs > 2 && ttisfunction(idx) && ttisnumber(idx + 1) && ttisnumber(idx + 2)) {  /* approximate product, 3.11.4 */
    StkId f, res;
    ptrdiff_t result;
    lua_Number i, start, stop, step;
    int k, multivariate, complexresult;
    complexresult = 0;
    start = nvalue(idx + 1);
    stop = nvalue(idx + 2);
    step = (nargs >= 4 && ttisnumber(idx + 3)) ? nvalue(idx + 3) : 1;
    multivariate = (nargs > 4)*(nargs - 5 + 1);  /* number of `surplus` arguments to f or 0 if none given */
    if (start > stop && step > 0) step = -step;
    f = idx;
    result = savestack(L, idx);  /* the following function calls may change the stack */
    if (tools_isint(start) && tools_isint(step)) {
      for (i=start; i <= stop; i += step) {
        luaD_checkstack(L, 2 + multivariate);
        setobj2s(L, L->top, f);    /* push function */
        setnvalue(L->top + 1, i);  /* push iteration value */
        for (k=4; k < nargs; k++) setobj2s(L, L->top + k - 2, idx + k);
        L->top += 2 + multivariate;
        luaD_call(L, L->top - 2 - multivariate, 1, 0);
        if (ttisnumber(L->top - 1)) {
          p *= (long double)nvalue(L->top - 1);
          flag = 1;
        } else if (ttiscomplex(L->top - 1)) {  /* 3.15.0 */
          TValue *r = s2v(L->top - 1);
#ifndef PROPCMPLX
          p_imag *= cvalue(r);
#else
          lua_Number z[2];
          z[0] = p_imag[0]; z[1] = p_imag[1];
          agnc_mul(p_imag, z[0], z[1], complexreal(r), compleximag(r));
#endif
          complexresult = 1;
        }
        L->top--;
      }
    } else {
      volatile lua_Number heps, iter_cs, iter_ccs, i, j;
      if (start > stop) goto quit;  /* 3.15.1 */
      iter_cs = iter_ccs = 0;
      i = j = start;
      heps = agn_gethepsilon(L);
      do {
        luaD_checkstack(L, 2 + multivariate);
        setobj2s(L, L->top, f);  /* push function */
        setnvalue(L->top + 1, (fabs(j) < heps) ? 0 : j);  /* push iteration value */
        for (k=4; k < nargs; k++) setobj2s(L, L->top + k - 2, idx + k);
        L->top += 2 + multivariate;
        luaD_call(L, L->top - 2 - multivariate, 1, 0);
        if (ttisnumber(L->top - 1)) {
          /* update result */
          p *= (long double)nvalue(L->top - 1);
          flag = 1;
        } else if (ttiscomplex(L->top - 1)) {  /* 3.15.0 */
          TValue *r = s2v(L->top - 1);
#ifndef PROPCMPLX
          p_imag *= cvalue(r);
#else
          lua_Number z[2];  /* can't both read and write to p_imag, so we use z[2]. */
          z[0] = p_imag[0]; z[1] = p_imag[1];
          agnc_mul(p_imag, z[0], z[1], complexreal(r), compleximag(r));
#endif
          complexresult = 1;
        }
        L->top--;
        /* update loop counter */
        i = tools_kbadd(i, step, &iter_cs, &iter_ccs);
        j = i + iter_cs + iter_ccs;
      } while (luai_numlt(0, step) ? luai_numle(j, stop) : luai_numle(stop, j) || tools_approx(j, stop, heps));
    }
quit:
    res = restorestack(L, result);
    if (complexresult) {  /* 3.15.0 */
      setcvalue(res, p_imag);
    } else {
      setnvalue(res, p);
    }
    return;
  }
  tm = luaT_gettmbyobj(L, obj, TM_MULUP);
  if (ttisnil(tm)) {
    if (ttistable(obj)) {
      Table *t;
      Node *n;
      int i;
      t = hvalue(obj);
      for (i=0; i < t->sizearray; i++) {
        val = &t->array[i];
        if (ttisnumber(val)) {
          p = luai_nummul(p, nvalue(val));  /* 2.30.2 improvement */
          flag = 1;
        }
      }
      for (i -= t->sizearray; i < sizenode(t); i++) {
        n = gnode(t, i);
        val = gval(n);
        if (ttisint(key2tval(n)) && ttisnumber(val)) {  /* changed to integer indices 2.30.2 */
          p = luai_nummul(p, nvalue(val));  /* 2.30.2 improvement */
          flag = 1;
        }
      }
    } else if (ttisseq(obj)) {
      size_t i;
      Seq *t = seqvalue(obj);
      for (i=0; i < t->size; i++) {
        val = seqitem(t, i);
        if (ttisnumber(val)) {
          p = luai_nummul(p, nvalue(val));  /* 2.30.2 improvement */
          flag = 1;
        }
      }
    } else if (ttisreg(obj)) {
      size_t i;
      Reg *t = regvalue(obj);
      for (i=0; i < t->top; i++) {
        val = regitem(t, i);
        if (ttisnumber(val)) {
          p = luai_nummul(p, nvalue(val));  /* 2.30.2 improvement */
          flag = 1;
        }
      }
    } else if (nargs == 2 && ttisfunction(obj)) {  /* apply numeric or Boolean selection function before multiplication, 3.11.2 */
      ptrdiff_t result;
      const TValue *tm;
      TValue *val;
      lua_Number x;
      flag = 0;
      result = savestack(L, idx);  /* the following function calls may change the stack */
      tm = luaT_gettmbyobj(L, idx + 1, TM_MULUP);
      if (ttisfunction(tm) && !call_binTM(L, idx + 1, luaO_nilobject, idx + 1, TM_MULUP)) {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": calling metamethod for userdata failed.", "mulup");
      };  /* try metamethod */
      if (ttistable(idx + 1)) {
        Table *h;
        size_t i, j;
        lua_lock(L);
        h = hvalue(idx + 1);
        api_check(L, L->top < L->ci->top);
        for (i=0; i < h->sizearray; i++) {
          if (ttisnotnil(&h->array[i])) {
            luaD_checkstack(L, nargs);
            setobj2s(L, L->top, idx);  /* push function */
            setobj2s(L, L->top + 1, &h->array[i]);  /* 1st argument */
            for (j=2; j < nargs; j++) setobj2s(L, L->top + j, idx + j);
            L->top += nargs;
            luaD_call(L, L->top - nargs, 1, 0);
            mulup_fn(L->top - 1, x, p, flag, &h->array[i]);
            L->top--;
          }  /* of if */
        }  /* of for i */
        for (i -= h->sizearray; i < sizenode(h); i++) {
          if (ttisnotnil(gval(gnode(h, i)))) {
            luaD_checkstack(L, nargs);
            val = gval(gnode(h, i));
            setobj2s(L, L->top, idx);  /* push function */
            setobj2s(L, L->top + 1, val);  /* 1st argument */
            for (j=2; j < nargs; j++) setobj2s(L, L->top + j, idx + j);
            L->top += nargs;
            luaD_call(L, L->top - nargs, 1, 0);
            mulup_fn(L->top - 1, x, p, flag, val);
            L->top--;
          }
        }
        lua_unlock(L);
      } else if (ttisseq(idx + 1)) {
        Seq *h;
        size_t i, j;
        lua_lock(L);
        h = seqvalue(idx + 1);
        api_check(L, L->top < L->ci->top);
        for (i=0; i < h->size; i++) {
          luaD_checkstack(L, nargs);
          setobj2s(L, L->top, idx);  /* push function */
          val = seqitem(h, i);  /* 3.11.2 extension */
          setobj2s(L, L->top + 1, val);  /* 1st argument */
          for (j=2; j < nargs; j++) setobj2s(L, L->top + j, idx + j);
          L->top += nargs;
          luaD_call(L, L->top - nargs, 1, 0);
          mulup_fn(L->top - 1, x, p, flag, val);
          L->top--;
        }
        lua_unlock(L);
      } else if (ttisreg(idx + 1)) {
        Reg *h;
        size_t i, j;
        lua_lock(L);
        h = regvalue(idx + 1);
        api_check(L, L->top < L->ci->top);
        for (i=0; i < h->top; i++) {
          luaD_checkstack(L, nargs);
          setobj2s(L, L->top, idx);  /* push function */
          val = regitem(h, i);
          setobj2s(L, L->top + 1, val);  /* 1st argument */
          for (j=2; j < nargs; j++) setobj2s(L, L->top + j, idx + j);
          L->top += nargs;
          luaD_call(L, L->top - nargs, 1, 0);
          mulup_fn(L->top - 1, x, p, flag, val);
          L->top--;  /* ditto */
        }
        lua_unlock(L);
      } else if (ttisuserdata(idx)) {
        if (!call_binTM(L, obj, luaO_nilobject, idx, TM_MULUP)) {
          luaG_runerror(L, "Error in " LUA_QS ": calling metamethod for userdata failed.", "mulup");
        }
      } else {
        luaG_runerror(L, "Error in " LUA_QS ": table, sequence, register or userdata expected for 2nd argument, got %s.", "mulup",
          luaT_typenames[(int)ttype(idx)]);
      }
      idx = restorestack(L, result);
    }
  } else if (ttisfunction(tm) && call_binTM(L, obj, luaO_nilobject, idx, TM_MULUP)) {  /* try metamethod, 3.5.6/7 extension */
    flag = ttisnumber(idx);
    if (flag) p = nvalue(idx);
  } else
    luaG_runerror(L, "Error in " LUA_QS ": wrong data type, got %s.", "mulup",
      luaT_typenames[(int)ttype(obj)]);
  if (flag) {
    setnvalue(idx, (lua_Number)p);
  } else {
    setfailvalue(idx);
  }
}


/* QMDEV operator (formerly VARPREP) for tables, sequences ands registers, 2.4.2, added 11.02.2015, merged 2.30.2; see:
      http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
   Computes the sum of the squared deviations of each observation tbl[i]. Uses an improved algorithm published by Donald Knuth,
   who cites B. P. Welford. It is less prone to loss of precision due to massive cancellation. Patched 2.4.4, 3.7.3 change to
   Kahan-Babuska summation.

   Divide the result by the number of elements n for population variance, and by n - 1 for sample variance.

   Maple-friendly test data:

s := [0.022076750417276, 0.2214768008429, 0.68608497347966, 0.0064027665678425, 0.29386666011711, 0.4736157290049, 0.97328889811099,
 0.19280679323376, 0.44732735257006, 0.35152854507395, 0.25772031781157, 0.73549708572007, 0.7504735538971, 0.90146854980917,
 0.41690612277803, 0.77523606003972, 0.15216715477974, 0.93688899392117, 0.34667211391342, 0.64987039479887, 0.25740455685063,
 0.13036784698738, 0.57852118163301, 0.33927565665882, 0.23431251069266, 0.82836450511979, 0.77904554772146, 0.94450442234264,
 0.32257742496327, 0.48516902722659, 0.48406192822571, 0.27551653994039, 0.5379377561798, 0.81462060115981, 0.051995068114249,
 0.69925461229834, 0.88337181316846, 0.39224657434609, 0.43624723350454, 0.28040075827409, 0.32331526667034, 0.94780905239648,
 0.30460720127663, 0.76530110033476, 0.81300643752935, 0.50802283198946, 0.67079900818448, 0.45528036121059, 0.24138525605266,
 0.57601118650102];

Returns 1 on failure and 0 otherwise. */

LUAI_FUNC int agenaV_numqmdev (lua_State *L, const TValue *obj, StkId idx, lua_Number *Mean, lua_Number *Qmdev) {
  Node *key;
  TValue *val;
  volatile lua_Number mean, meantemp, M2, delta, d, x, cs, ccs, meancs, meanccs;  /* 2.4.4 fix, do not optimise */
  int flag = 0;
  mean = M2 = cs = ccs = meancs = meanccs = 0.0;
  api_check(L, L->top < L->ci->top);
  if (ttistable(obj)) {
    int i;
    Table *t = hvalue(obj);
    for (i=0; i < t->sizearray; i++) {
      val = &t->array[i];
      if (ttisnumber(val)) {
        x = nvalue(val);
        delta = x - mean;
        d = delta/(i + 1);
        mean = tools_kbadd(mean, d, &meancs, &meanccs);
        meantemp = mean + meancs + meanccs;
        x = delta*(x - meantemp);
        M2 = tools_kbadd(M2, x, &cs, &ccs);
        flag = 1;
      } else if (ttiscomplex(val)) {  /* 4.4.6 extension */
        *Mean = *Qmdev = AGN_NAN;
        return 1;  /* fail, bail out immediately */
      }
    }
    for (i -= t->sizearray; i < sizenode(t); i++) {
      key = gnode(t, i);
      val = gval(key);
      if (ttisint(key2tval(key)) && ttisnumber(val)) {
        /* 3.15.3 change, ignore all non-integral keys in the hash part, behave like addup, mulup, qsumup */
        x = nvalue(val);
        delta = x - mean;
        d = delta/(i + 1);
        mean = tools_kbadd(mean, d, &meancs, &meanccs);
        meantemp = mean + meancs + meanccs;
        x = delta*(x - meantemp);
        M2 = tools_kbadd(M2, x, &cs, &ccs);
        flag = 1;
      } else if (ttiscomplex(val)) {  /* 4.4.6 extension */
        *Mean = *Qmdev = AGN_NAN;
        return 1;  /* fail, bail out immediately */
      }
    }
  } else if (ttisseq(obj)) {
    size_t i;
    Seq *t = seqvalue(obj);
    for (i=0; i < t->size; i++) {
      val = seqitem(t, i);
      if (ttisnumber(val)) {
        x = nvalue(val);
        delta = x - mean;
        d = delta/(i + 1);
        mean = tools_kbadd(mean, d, &meancs, &meanccs);
        meantemp = mean + meancs + meanccs;
        x = delta*(x - meantemp);
        M2 = tools_kbadd(M2, x, &cs, &ccs);
        flag = 1;
      } else if (ttiscomplex(val)) {  /* 4.4.6 extension */
        *Mean = *Qmdev = AGN_NAN;
        return 1;  /* fail, bail out immediately */
      }
    }
  } else if (ttisreg(obj)) {
    size_t i;
    Reg *t = regvalue(obj);
    for (i=0; i < t->top; i++) {
      val = regitem(t, i);
      if (ttisnumber(val)) {
        x = nvalue(val);
        delta = x - mean;
        d = delta/(i + 1);
        mean = tools_kbadd(mean, d, &meancs, &meanccs);
        meantemp = mean + meancs + meanccs;
        x = delta*(x - meantemp);
        M2 = tools_kbadd(M2, x, &cs, &ccs);
        flag = 1;
      } else if (ttiscomplex(val)) {  /* 4.4.6 extension */
        *Mean = *Qmdev = AGN_NAN;
        return 1;  /* fail, bail out immediately */
      }
    }
  }
  if (flag) {
    *Qmdev = M2;   /* 4.6.2 improvement, do not add cs and ccs. */
    *Mean = mean;  /* with some distributions, `mean' is better, with others `meantemp' is. */
  } else {
    *Mean = *Qmdev = AGN_NAN;
  }
  return !flag;
}


LUAI_FUNC void agenaV_qmdev (lua_State *L, const TValue *obj, StkId idx) {
  const TValue *tm;
  int flag;
  lua_Number mean, M2;
  flag = 0;
  M2 = mean = 0.0;
  api_check(L, L->top < L->ci->top);
  tm = luaT_gettmbyobj(L, obj, TM_QMDEV);
  if (ttisnil(tm)) {
    flag = !agenaV_numqmdev(L, obj, idx, &mean, &M2);
  } else if (ttisfunction(tm) && call_binTM(L, obj, luaO_nilobject, idx, TM_QMDEV)) {  /* try metamethod, 3.5.6/7 extension */
    flag = ttisnumber(idx);
    if (flag) M2 = nvalue(idx);
  } else {
    luaG_runerror(L, "Error in " LUA_QS ": wrong data type, got %s.", "qmdev",
      luaT_typenames[(int)ttype(obj)]);
  }
  if (flag) {
    setnvalue(idx, M2);
  } else {
    setfailvalue(idx);
  }
}


LUAI_FUNC void agenaV_toset (lua_State *L, const TValue *obj, StkId idx) {  /* 2.30.2 */
  int i;
  TValue *v;
  UltraSet *h = NULL;
  if (ttistable(obj)) {
    Table *t = hvalue(obj);
    h = agnUS_new(L, t->sizearray + sizenode(t));
    for (i=0; i < t->sizearray; i++) {
      v = &t->array[i];
      if (ttisnotnil(v)) {
        agnUS_set(L, h, v);
        luaC_barrierset(L, h, v);
      }
    }
    for (i -= t->sizearray; i < sizenode(t); i++) {
      v = gval(gnode(t, i));
      if (ttisnotnil(v)) {
        agnUS_set(L, h, v);
        luaC_barrierset(L, h, v);
      }
    }
  } else if (ttisseq(obj)) {
    Seq *t = seqvalue(obj);
    h = agnUS_new(L, t->size);
    for (i=0; i < t->size; i++) {  /* added 0.8.0 */
      v = seqitem(t, i);
      agnUS_set(L, h, v);
      luaC_barrierset(L, h, v);
    }
  } else if (ttisreg(obj)) {
    Reg *t = regvalue(obj);
    h = agnUS_new(L, t->top);
    for (i=0; i < t->top; i++) {
      v = regitem(t, i);
      agnUS_set(L, h, v);
      luaC_barrierset(L, h, v);
    }
  } else
    luaG_runerror(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.", "toset",
      luaT_typenames[(int)ttype(obj)]);
  setusvalue(L, idx, h);  /* push set onto the stack */
}


LUAI_FUNC void luaV_substring (lua_State *L, StkId string, StkId inds, StkId idx) {  /* Agena 1.2 */
  size_t len;
  int start, stop;
  const char *str;
  start = 0;  /* to prevent compiler warnings */
  str = svalue(string);
  len = stop = tsvalue(string)->len;  /* setting of stop to prevent compiler warnings */
  if (len == 0) {
    setfailvalue(idx);
    return;
  }
  if (ttisnumber(inds) && ttisnumber(inds + 1)) {
    start = nvalue(inds);
    stop = nvalue(inds + 1);
  }
  else
    luaG_runerror(L, "Error in %s: invalid arguments.", "substring op");
  if (start < 0) start = len + start + 1;
  if (stop < 0) stop = len + stop + 1;
  if (start < 1 || start > len)  /* 2.1.1 */
    luaG_runerror(L, "Error in %s: left index %d out of range.", "substring op", start);
  else if (stop < 1)
    luaG_runerror(L, "Error in %s: right index %d out of range.", "substring op", stop);
  else if (stop > len) stop = len;  /* behave like Lua's string.sub, 3.0.0 */
  if (start > stop)
    luaG_runerror(L, "Error in %s: left index (%d) > right index (%d).", "substring op", start, stop);
  /* delete indices, keep substring */
  setnilvalue(inds);
  setnilvalue(inds + 1);
  setsvalue(L, idx, luaS_newlstr(L, str + start - 1, stop - start + 1));
  /* setting L->top to func confuses the stack */
}


LUAI_FUNC void luaV_tablesublist (lua_State *L, StkId t, StkId inds, StkId idx) {  /* Agena 1.2 */
  int i, start, stop, stopp, nops;
  Table *h, *newtable;
  const TValue *k, *v;
  Node *gn;
  lua_Number n;
  lua_lock(L);
  start = stop = nops = 0;  /* to prevent compiler warnings */
  h = hvalue(t);
  if (ttisnumber(inds) && ttisnumber(inds + 1)) {
    start = nvalue(inds) - 1;
    stop = nvalue(inds + 1) - 1;
    if (stop < start) {  /* work like in Maple and return an empty table, 3.9.0 */
      newtable = luaH_new(L, 0, 0);
      goto sublist;
    }
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error in %s: invalid arguments.", "table indexing");
  }
  /* 3.9.0 fix: try to put the values in the array, and not the hash part of the new table */
  nops = stop - start + 1;
  /* We explicitly tell Agena to create an array part with space for some elements, we do NOT let Agena figure out the best size by itself. */
  newtable = luaH_new(L, nops < 4 ? 4 : tools_nextpower(nops, 2, 0), 0);
  i = -1;
  if (start > i && start < h->sizearray) i = start - 1;
  else if (start >= h->sizearray) i = h->sizearray;
  stopp = (stop < h->sizearray) ? stop + 1 : h->sizearray;
  for (i++; i < stopp; i++) {
    v = &h->array[i];
    if (ttisnotnil(v)) {  /* array element is filled */
      luaH_setint(L, newtable, i + 1, (TValue *)v);  /* 4.6.3 tweak */
      luaC_barriert(L, newtable, v);
    }
  }
  for (i=0; i < sizenode(h); i++) {
    gn = gnode(h, i);
    k = key2tval(gn);
    if (ttisnumber(k) && !ttisnil(gval(gn))) {  /* key is a number and is assigned a non-null value ... */
      n = nvalue(k) - 1;  /* 2.9.5 fix */
      if (tools_isint(n) && n >= start && n <= stop) {  /* and an integer: is there a smaller key in hash part ?; 2.3.0 RC 2 eCS fix */
        v = gval(gn);
        luaH_setint(L, newtable, n + 1, (TValue *)v);  /* 4.6.3 tweak */
        luaC_barriert(L, newtable, v);
      }
    }
  }
  /* delete indices, keep substring */
  /* We deliberately do NOT RESIZE to prevent problems with other functions such as tables.reshuffle. */
sublist:
  setnilvalue(inds);
  setnilvalue(inds + 1);
  /* return sublist */
  sethvalue(L, idx, newtable);
  /* setting L->top to func confuses the stack */
  lua_unlock(L);
}


LUAI_FUNC void luaV_seqsublist (lua_State *L, StkId t, StkId inds, StkId idx) {  /* Agena 1.2 */
  int i, start, stop;
  size_t c, length;
  Seq *h, *newtable;
  const TValue *v;
  lua_lock(L);
  start = stop = 0;  /* to prevent compiler warnings */
  h = seqvalue(t);
  length = h->size;
  if (ttisnumber(inds) && ttisnumber(inds + 1)) {
    start = tools_posrelat(nvalue(inds), length);
    stop = tools_posrelat(nvalue(inds + 1), length);
    if (start > stop || (start < 1 || stop > length)) {  /* 1.11.8 */
      /* work like in Maple and return an empty sequence, 3.9.0 */
      newtable = agnSeq_new(L, 0);
      goto sublist;
    }
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error when indexing sequences: invalid arguments.");  /* 1.11.8 */
  }
  newtable = agnSeq_new(L, stop - start + 1);
  i = 1; c = 0;
  if (start > 1 && start <= length) i = start;
  stop = (stop < length) ? stop : h->size;
  for (; i <= stop; i++) {
    c++;
    v = seqitem(h, i - 1);
    agnSeq_seti(L, newtable, c, v);
    luaC_barrierseq(L, newtable, v);
  }
  /* we do not need to resize */
  /* delete indices, keep substring */
sublist:
  setnilvalue(inds);
  setnilvalue(inds + 1);
  /* return sublist */
  setseqvalue(L, idx, newtable);
  /* setting L->top to func confuses the stack */
  lua_unlock(L);
}


LUAI_FUNC void luaV_regsublist (lua_State *L, StkId t, StkId inds, StkId idx) {  /* 2.3.0 RC 3 */
  int i, start, stop;
  size_t c, length;
  Reg *h, *newtable;
  const TValue *v;
  lua_lock(L);
  start = stop = 0;  /* to prevent compiler warnings */
  h = regvalue(t);
  length = h->top;
  if (ttisnumber(inds) && ttisnumber(inds + 1)) {
    start = tools_posrelat(nvalue(inds), length);
    stop = tools_posrelat(nvalue(inds + 1), length);
    if (start > stop || (start < 1 || stop > length)) { /* 1.11.8 */
      /* work like in Maple and return an empty register, 3.9.0 */
      newtable = agnReg_new(L, 0);
      goto sublist;
    }
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error when indexing register: invalid arguments.");  /* 1.11.8 */
  }
  newtable = agnReg_new(L, stop - start + 1);
  i = 1; c = 0;
  if (start > 1 && start <= length) i = start;
  stop = (stop < length) ? stop : h->top;
  for (; i <= stop; i++) {
    c++;
    v = regitem(h, i - 1);
    agnReg_seti(L, newtable, c, v);
    luaC_barrierreg(L, newtable, v);
  }
  /* we do not need to resize */
  /* delete indices, keep substring */
sublist:
  setnilvalue(inds);
  setnilvalue(inds + 1);
  /* return sublist */
  setregvalue(L, idx, newtable);
  /* setting L->top to func confuses the stack */
  lua_unlock(L);
}


/* JOIN function, 0.11.0, patched 1.0.5, changed 1.1.0, changed from operator to function 2.34.5 */

static void agenaV_join_aux (lua_State *L, StkId idx, int nargs, int size, int *start, int *stop) {
  if (nargs > 2 && ttisnumber(idx + 2)) {
    *start = tools_posrelat(nvalue(idx + 2), size) - 1;
    if (*start < 0 || *start >= size)
      luaG_runerror(L, "Error in " LUA_QS ": index out of range.", "join");
  } else
    *start = 0;
  if (nargs > 3 && ttisnumber(idx + 3)) {
    *stop = tools_posrelat(nvalue(idx + 3), size);
    if (*stop > size)
      luaG_runerror(L, "Error in " LUA_QS ": index out of range.", "join");
    if (*stop <= *start)
      luaG_runerror(L, "Error in " LUA_QS ": last index < first index.", "join");
  } else
    *stop = size;
}


/* 4.10.0: Introduced auto-conversion */
#define aux_join_countlen(L,val,c,bufsize) { \
  if (ttisstring(val)) { \
    c++; \
    bufsize += tsvalue(val)->len; \
  } else { \
    setobj2s(L, L->top, val); \
    if (tostring(L, L->top)) { \
      c++; \
      bufsize += tsvalue(L->top)->len; \
    } \
  } \
}

#define aux_join_convert(L,val,buf,delim,delimlen) { \
  if (ttisstring(val)) { \
    charbuf_append(L, &buf, svalue(val), tsvalue(val)->len); \
  } else { \
    setobj2s(L, L->top, val); \
    if (tostring(L, L->top)) { \
      charbuf_append(L, &buf, svalue(L->top), tsvalue(L->top)->len); \
    } else { \
      charbuf_finish(&buf); \
      charbuf_free(&buf); \
      luaG_runerror(L, "Error in " LUA_QS ": strings, (complex) numbers or Booleans expected in structure, got %s.", "join", \
        luaT_typenames[(int)ttype(val)]); \
    } \
  } \
}

LUAI_FUNC void agenaV_join (lua_State *L, StkId idx, int nargs) {
  Charbuf buf;
  const char *delim;
  int i, start, stop;
  size_t delimlen, c, bufsize;
  const TValue *val;
  api_check(L, L->top < L->ci->top);
  lua_lock(L);
  c = 0; bufsize = 0;
  if (nargs > 1 && ttisstring(idx + 1)) {
    delim = svalue(idx + 1);
    delimlen = tsvalue(idx + 1)->len;
  } else {
    delim = NULL;
    delimlen = 0;
  }
  if (ttistable(idx)) {
    Table *t = hvalue(idx);
    /* determine overall size of resulting string at once */
    for (i=0; i < t->sizearray; i++) {
      if (ttisnotnil(&t->array[i])) {
        val = &t->array[i];  /* value */
        aux_join_countlen(L, val, c, bufsize);
      }  /* of if */
    }  /* of for i */
    /* values in dictionaries are joined in random order, so this may be quite useless: */
    for (i -= t->sizearray; i < sizenode(t); i++) {
      if (ttisnotnil(gval(gnode(t, i)))) {
        val = gval(gnode(t, i));
        aux_join_countlen(L, val, c, bufsize);
      }
    }
    charbuf_init(L, &buf, 0, (bufsize + 1) + (c - 1)*delimlen, 1);  /* Agena 1.6.0 */
    /* now concat the strings */
    agenaV_join_aux(L, idx, nargs, t->sizearray, &start, &stop);
    for (i=start; i < stop; i++) {
      if (ttisnotnil(&t->array[i])) {
        val = &t->array[i];  /* value */
        aux_join_convert(L, val, buf, delim, delimlen);
        if (delim) {
          charbuf_append(L, &buf, delim, delimlen);
        }
      }  /* of if */
    }  /* of for i */
    if (stop != t->sizearray) i = t->sizearray;
    /* values in dictionaries are joined in random order, so this may be quite useless: */
    for (i -= t->sizearray; i < sizenode(t); i++) {
      if (ttisnotnil(gval(gnode(t, i)))) {
        val = gval(gnode(t, i));
        aux_join_convert(L, val, buf, delim, delimlen);
        if (delim) {
          charbuf_append(L, &buf, delim, delimlen);
        }
      }
    }
    /* Since the new string now terminates with the delimiter, remove the delimiter.
       Agena 1.6.0 patch: if table contains no or empty strings, do not try to delete the missing trailing delim. */
    if (buf.size != 0 && delim) {  /* Agena 1.6.0 patch */
      char *oldpos = buf.data;
      buf.data += buf.size - delimlen;
      *buf.data = '\0';  /* 7.7.6 fix to prevent segfaults, found by Gemini AI */
      buf.data = oldpos;
      buf.size -= delimlen;
    }
  } else if (ttisseq(idx)) {
    Seq *s = seqvalue(idx);
    /* determine overall size of resulting string at once */
    for (i=0; i < s->size; i++) {
      val = agnSeq_geti(s, i + 1);
      aux_join_countlen(L, val, c, bufsize);
    }
    charbuf_init(L, &buf, 0, (bufsize + 1) + (c - 1)*delimlen, 1);
    /* now concat the strings */
    agenaV_join_aux(L, idx, nargs, s->size, &start, &stop);
    for (i=start; i < stop; i++) {
      val = agnSeq_geti(s, i + 1);
      aux_join_convert(L, val, buf, delim, delimlen);
      if (delim && i < stop - 1)
        charbuf_append(L, &buf, delim, delimlen);
    }
  } else if (ttisreg(idx)) {  /* 2.4.0 */
    Reg *s = regvalue(idx);
    /* determine overall size of resulting string at once */
    for (i=0; i < s->top; i++) {
      val = agnReg_geti(s, i + 1);
      aux_join_countlen(L, val, c, bufsize);
    }
    charbuf_init(L, &buf, 0, (bufsize + 1) + (c - 1)*delimlen, 1);
    /* now concat the strings */
    agenaV_join_aux(L, idx, nargs, s->top, &start, &stop);
    for (i=start; i < stop; i++) {
      val = agnReg_geti(s, i + 1);
      aux_join_convert(L, val, buf, delim, delimlen);
      if (delim && i < stop - 1)
        charbuf_append(L, &buf, delim, delimlen);
    }
  }
  else {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.", "join",
      luaT_typenames[(int)ttype(idx)]);
    return;
  }
  /* return joined string */
  charbuf_finish(&buf);  /* better sure than sorry, 2.39.12 */
  luaC_checkGC(L);
  setsvalue2s(L, idx, luaS_newlstr(L, buf.data, buf.size));
  charbuf_free(&buf);
  lua_unlock(L);
}


/* MULADD operator, 2.33.0 */
LUAI_FUNC void agenaV_muladd (lua_State *L, StkId idx, int nargs) {
  int i, useldbl, allnumbers;
  TValue *r = NULL;
  if (nargs < 3)
    luaG_runerror(L, "Error in " LUA_QS ": at least three (complex) numbers expected, got %d.", "muladd", nargs);
  useldbl = ttistrue(idx + nargs - 1);
  if (useldbl) { setnilvalue(idx + nargs - 1); nargs--; }
  if ((nargs & 1) == 0)  /* number of arguments is not odd */
    luaG_runerror(L, "Error in " LUA_QS ": wrong number of arguments.", "muladd");
  allnumbers = 1;
  lua_lock(L);
  /* Checking for numbers-only and then treating the arguments real-only gives a speed increase of 28 % */
  for (i=0; i < nargs && allnumbers; i++) {
    allnumbers = ttisnumber(idx + i);
  }
  if (allnumbers) {  /* all values are numbers */
    i = nargs - 3;
    if (!useldbl) {
      lua_Number a, b, s;
      s = nvalue(idx + i + 2);
      setnilvalue(idx + i + 2);
      while (i >= 0) {
        a = nvalue(idx + i);
        b = nvalue(idx + i + 1);
        s = fma(a, b, s);
        setnilvalue(idx + i);
        setnilvalue(idx + i + 1);
        i -= 2;
      }
      setnvalue(idx, s);
    } else {
      long double a, b, s;
      s = nvalue(idx + i + 2);
      setnilvalue(idx + i + 2);
      while (i >= 0) {
        a = nvalue(idx + i);
        b = nvalue(idx + i + 1);
        s = tools_fmal(a, b, s);
        setnilvalue(idx + i);
        setnilvalue(idx + i + 1);
        i -= 2;
      }
      setnvalue(idx, s);
    }
    lua_unlock(L);
    return;
  }
  /* From here on we either have a mix of real and complex numbers, or complex numbers only, so result is
     always complex. */
  i = nargs - 3;
  if (!useldbl) {
    lua_Number a, b, c, d, re, im;
    a = b = c = d = re = im = 0.0L;
    if (ttisnumber(idx + i + 2)) {
      re = nvalue(idx + i + 2);
      im = 0.0;
    } else if (ttiscomplex(idx + i + 2)) {
      r = s2v(idx + i + 2);
      re = complexreal(r); im = compleximag(r);
    } else {
      lua_unlock(L);
      luaG_runerror(L, "Error in " LUA_QS ": excepted (complex) numbers only, got %s.", "muladd", luaT_typenames[(int)ttype(idx + i + 2)]);
    }
    setnilvalue(idx + i + 2);
    while (i >= 0) {
      if (ttisnumber(idx + i)) {
        a = nvalue(idx + i);
        b = 0.0;
      } else if (ttiscomplex(idx + i)) {
        r = s2v(idx + i);
        a = complexreal(r); b = compleximag(r);
      } else {
        luaG_runerror(L, "Error in " LUA_QS ": excepted (complex) numbers only, got %s.", "muladd", luaT_typenames[(int)ttype(idx + i)]);
      }
      if (ttisnumber(idx + i + 1)) {
        c = nvalue(idx + i + 1);
        d = 0.0;
      } else if (ttiscomplex(idx + i + 1)) {
        r = s2v(idx + i + 1);
        c = complexreal(r); d = compleximag(r);
      } else {
        luaG_runerror(L, "Error in " LUA_QS ": excepted (complex) numbers only, got %s.", "muladd", luaT_typenames[(int)ttype(idx + i + 1)]);
      }
      setnilvalue(idx + i);
      setnilvalue(idx + i + 1);
      re = fma(a, c, -fma(b, d, -re));
      im = fma(a, d, fma(b, c, im));
      i -= 2;
    }
    setrealimagvalue(idx, re, im);
  } else {
    long double a, b, c, d, re, im;
    a = b = c = d = re = im = 0.0L;
    if (ttisnumber(idx + i + 2)) {
      re = nvalue(idx + i + 2);
      im = 0.0L;
    } else if (ttiscomplex(idx + i + 2)) {
      r = s2v(idx + i + 2);
      re = complexreal(r); im = compleximag(r);
    } else {
      lua_unlock(L);
      luaG_runerror(L, "Error in " LUA_QS ": excepted (complex) numbers only, got %s.", "muladd", luaT_typenames[(int)ttype(idx + i + 2)]);
    }
    setnilvalue(idx + i + 2);
    while (i >= 0) {
      if (ttisnumber(idx + i)) {
        a = nvalue(idx + i);
        b = 0.0L;
      } else if (ttiscomplex(idx + i)) {
        r = s2v(idx + i);
        a = complexreal(r); b = compleximag(r);
      } else {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": excepted (complex) numbers only, got %s.", "muladd", luaT_typenames[(int)ttype(idx + i)]);
      }
      if (ttisnumber(idx + i + 1)) {
        c = nvalue(idx + i + 1);
        d = 0.0L;
      } else if (ttiscomplex(idx + i + 1)) {
        r = s2v(idx + i + 1);
        c = complexreal(r); d = compleximag(r);
      } else {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": excepted (complex) numbers only, got %s.", "muladd", luaT_typenames[(int)ttype(idx + i + 1)]);
      }
      setnilvalue(idx + i);
      setnilvalue(idx + i + 1);
      re = tools_fmal(a, c, -tools_fmal(b, d, -re));
      im = tools_fmal(a, d, tools_fmal(b, c, im));
      i -= 2;
    }
    setrealimagvalue(idx, re, im);
  }
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* FOREACH operator, 3.4.7 */
#define ttisstruct(idx) (ttistable(idx) || ttisseq(idx))

LUAI_FUNC void agenaV_foreach (lua_State *L, StkId idx, int nargs) {
  StkId f, s, res;
  ptrdiff_t result;
  lua_Number i, start, stop;
  volatile lua_Number step, heps;
  int stepgiven, n, structuregiven, summode, reducemode;
  reducemode = ttisboolean(idx) && bvalue(idx) == 1;
  if (reducemode) { nargs--; idx++; }
  switch (nargs) {
    case 3:
      if (!ttisnumber(idx) || !ttisnumber(idx + 1) || !ttisfunction(idx + 2)) {
        luaG_runerror(L, "Error in " LUA_QS ": two numbers and a function expected.", "foreach");
      }
      break;
    case 4:
      if (ttisstruct(idx + 3) && (!ttisnumber(idx) || !ttisnumber(idx + 1) || !ttisfunction(idx + 2))) {
        luaG_runerror(L, "Error in " LUA_QS ": two numbers, a function and a structure expected.", "foreach");
      } else if (ttisfunction(idx + 3) && (!ttisnumber(idx) || !ttisnumber(idx + 1) || !ttisnumber(idx + 2))) {
        luaG_runerror(L, "Error in " LUA_QS ": three numbers and a function expected.", "foreach");
      }
      break;
    case 5:
      if ((!ttisnumber(idx) || !ttisnumber(idx + 1) || !ttisnumber(idx + 2) || !ttisfunction(idx + 3)) ||
        !((ttisstruct(idx + 4)) || (ttisnumber(idx + 4)))) {
        luaG_runerror(L, "Error in " LUA_QS ": 3 numbers, a function and a structure or number expected.", "foreach");
      }
      break;
    default:
      luaG_runerror(L, "Error in " LUA_QS ": three to six values expected, got %d.", "foreach", nargs + reducemode);
  }
  start = nvalue(idx);
  stop = nvalue(idx + 1);
  stepgiven = ttisnumber(idx + 2);
  step = stepgiven ? nvalue(idx + 2) : 1;
  if (start > stop && step > 0) step = -step;
  heps = agn_gethepsilon(L);
  structuregiven = ttisstruct(idx + nargs - 1);
  summode = (ttisnumber(idx + nargs - 1) || ttiscomplex(idx + nargs - 1)) && !reducemode;
  f = idx + 2 + stepgiven;
  s = f + 1;
  result = savestack(L, idx - reducemode);  /* the following function calls may change the stack */
  if (summode) {
    volatile lua_Number s, cs, ccs, s_imag, imag_cs, imag_ccs;
    int complexresult = 0;
    cs = ccs = imag_cs = imag_ccs = 0.0;
    if (ttisnumber(idx + nargs - 1)) {
      s = nvalue(idx + nargs - 1);
      s_imag = 0;  /* to prevent compiler warnings */
    } else {  /* 3.15.0 */
      TValue *r = s2v(idx + nargs - 1);
      s     = complexreal(r);
      s_imag = compleximag(r);
      complexresult = 1;
    }
    if (tools_isint(start) && tools_isint(step)) {
      for (i=start; i <= stop; i += step) {
        luaD_checkstack(L, 2);
        setobj2s(L, L->top, f);    /* push function */
        setnvalue(L->top + 1, i);  /* push corrected iteration value */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        if (ttisnumber(L->top - 1)) {
          s = tools_kbadd(s, nvalue(L->top - 1), &cs, &ccs);
        } else if (ttiscomplex(L->top - 1)) {  /* 3.15.0 */
          TValue *r = s2v(L->top - 1);
          s = tools_kbadd(s, complexreal(r), &cs, &ccs);
          s_imag = tools_kbadd(s_imag, compleximag(r), &imag_cs, &imag_ccs);
          complexresult = 1;
        }
        L->top--;
      }
    } else {
      volatile lua_Number iter_cs, iter_ccs, i, j;
      iter_cs = iter_ccs = 0.0;
      i = j = start;
      do {
        luaD_checkstack(L, 2);
        setobj2s(L, L->top, f);  /* push function */
        setnvalue(L->top + 1, (fabs(j) < heps) ? 0 : j);  /* push iteration value */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        if (ttisnumber(L->top - 1)) {  /* update result */
          s = tools_kbadd(s, nvalue(L->top - 1), &cs, &ccs);
        } else if (ttiscomplex(L->top - 1)) {  /* 3.15.0 */
          TValue *r = s2v(L->top - 1);
          s = tools_kbadd(s, complexreal(r), &cs, &ccs);
          s_imag = tools_kbadd(s_imag, compleximag(r), &imag_cs, &imag_ccs);
          complexresult = 1;
        }
        L->top--;
        /* update loop counter, 3.11.4 fix */
        i = tools_kbadd(i, step, &iter_cs, &iter_ccs);
        j = i + iter_cs + iter_ccs;
      } while (luai_numlt(0, step) ? luai_numle(j, stop) : luai_numle(stop, j) || tools_approx(j, stop, heps));
    }
    res = restorestack(L, result);
    if (complexresult) {  /* 3.15.0 */
      setrealimagvalue(res, s + cs + ccs, s_imag + imag_cs + imag_ccs);
    } else {
      setnvalue(res, s + cs + ccs);   /* no rounding in between */
    }
  } else if (reducemode) {  /* 3.7.7 extension, works similar to the `reduce` function */
    StkId idxaccu = idx + nargs - 1;
    if (tools_isint(start) && tools_isint(step)) {
      for (i=start; i <= stop; i += step) {
        luaD_checkstack(L, 3);
        setobj2s(L, L->top, f);            /* push function */
        setnvalue(L->top + 1, i);          /* push iteration value */
        setobj2s(L, L->top + 2, idxaccu);  /* push accumulator */
        L->top += 3;
        luaD_call(L, L->top - 3, 1, 0);
        setobj2s(L, idxaccu, L->top - 1);  /* update accumulator slot */
        L->top--;
      }
    } else {
      volatile lua_Number cs, ccs, i, j;
      cs = ccs = 0;
      i = j = start;
      do {
        luaD_checkstack(L, 3);
        setobj2s(L, L->top, f);  /* push function */
        setnvalue(L->top + 1, (fabs(j) < heps) ? 0 : j);  /* autocorrection of iteration value at zero */
        setobj2s(L, L->top + 2, idxaccu);  /* push accumulator */
        L->top += 3;
        luaD_call(L, L->top - 3, 1, 0);
        setobj2s(L, idxaccu, L->top - 1);  /* update accumulator slot */
        L->top--;
        /* update loop counter */
        i = tools_kbadd(i, step, &cs, &ccs);
        j = i + cs + ccs;
      } while (luai_numlt(0, step) ? luai_numle(j, stop) : luai_numle(stop, j) || tools_approx(j, stop, heps));
    }
    res = restorestack(L, result);
    setobj2s(L, res, idxaccu);
  } else if (ttistable(s) || !structuregiven) {
    lua_lock(L);
    Table *t = structuregiven ? hvalue(s) : luaH_new(L, tools_numiters(start, stop, step), 0);
    if (fasttm(L, t->metatable, TM_WEAK) != NULL) {
      lua_unlock(L);
      luaG_runerror(L, "Error: " LUA_QS " is not suited for weak tables.", "foreach");
    }
    n = luaH_getn(t) + 1;
    api_check(L, L->top < L->ci->top);
    if (tools_isint(start) && tools_isint(step)) {
      for (i=start; i <= stop; i += step) {
        luaD_checkstack(L, 2);
        setobj2s(L, L->top, f);  /* push function */
        setnvalue(L->top + 1, i);  /* iteration value */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        /* setobj2t(L, luaH_setnum(L, t, cast_num(n++)), L->top - 1); */
        luaH_setint(L, t, n++, L->top - 1);
        luaC_barriert(L, t, L->top - 1);
        L->top--;
      }  /* of for */
    } else {
      volatile lua_Number cs, ccs, i, j;
      cs = ccs = 0;
      i = j = start;
      do {
        luaD_checkstack(L, 2);
        setobj2s(L, L->top, f);  /* push function */
        setnvalue(L->top + 1, (fabs(j) < heps) ? 0 : j);  /* autocorrection of iteration value at zero */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        luaH_setint(L, t, n++, L->top - 1);  /* 4.6.3 tweak */
        luaC_barriert(L, t, L->top - 1);
        L->top--;
        /* update loop counter */
        i = tools_kbadd(i, step, &cs, &ccs);
        j = i + cs + ccs;
      } while (luai_numlt(0, step) ? luai_numle(j, stop) : luai_numle(stop, j) || tools_approx(j, stop, heps));
    }
    /* prepare sequence for return to Agena environment: move new structure to final position */
    res = restorestack(L, result);
    sethvalue(L, res, t);
    lua_unlock(L);
  } else if (ttisseq(s)) {
    lua_lock(L);
    Seq *t = seqvalue(s);
    api_check(L, L->top < L->ci->top);
    if (tools_isint(start) && tools_isint(step)) {
      for (i=start; i <= stop; i += step) {
        luaD_checkstack(L, 2);
        setobj2s(L, L->top, f);  /* push function */
        setnvalue(L->top + 1, i);  /* iteration value */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        if (agnSeq_seti(L, t, t->size + 1, L->top - 1) == -1)
          luaG_runerror(L, "Error in " LUA_QS ": maximum sequence size exceeded.", "foreach");
        luaC_barrierseq(L, t, L->top - 1);
        L->top--;
      }
    } else {
      volatile lua_Number cs, ccs, i, j;
      cs = ccs = 0;
      i = j = start;
      do {
        luaD_checkstack(L, 2);
        setobj2s(L, L->top, f);  /* push function */
        setnvalue(L->top + 1, (fabs(j) < heps) ? 0 : j);  /* autocorrection of iteration value at zero */
        L->top += 2;
        luaD_call(L, L->top - 2, 1, 0);
        if (agnSeq_seti(L, t, t->size + 1, L->top - 1) == -1)
          luaG_runerror(L, "Error in " LUA_QS ": maximum sequence size exceeded.", "foreach");
        luaC_barrierseq(L, t, L->top - 1);
        L->top--;
        /* update loop counter */
        i = tools_kbadd(i, step, &cs, &ccs);
        j = i + cs + ccs;
      } while (luai_numlt(0, step) ? luai_numle(j, stop) : luai_numle(stop, j) || tools_approx(j, stop, heps));
    }  /* of for */
    /* prepare sequence for return to Agena environment: move new structure to final position */
    res = restorestack(L, result);
    setseqvalue(L, res, t);
    lua_unlock(L);
  } else {  /* should not happen */
    luaG_runerror(L, "Error in " LUA_QS ": expected a table or sequence, got %s.", "foreach", luaT_typenames[(int)ttype(s)]);
  }
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* ADDUP operator, 3.6.0
   With one argument, works like sumup.
   With a structure and a number, divides each structure element by that number and sums up.
   With a function, a structure and optional function arguments, applies the function on each element and sums up. */

#define addup_kahanbabuska_div(val,x,z,d,t,c,s,cc,cs,ccs,flag) { \
  if (ttisnumber(val)) { \
    if (flag == LUA_TNIL) flag = LUA_TNUMBER; \
    x = (d == 0.0) ? AGN_NAN : nvalue(val)/d; \
    t[0] = s[0] + x; \
    c[0] = (fabs(s[0]) >= fabs(x)) ? (s[0] - t[0]) + x : (x - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
  } else if (ttiscomplex(val)) { \
    if (flag == LUA_TNIL || flag == LUA_TNUMBER) flag = LUA_TCOMPLEX; \
    if (d == 0.0) { \
      z[0] = AGN_NAN; z[1] = AGN_NAN; \
    } else { \
      z[0] = complexreal(val)/d; z[1] = compleximag(val)/d; \
    } \
    t[0] = s[0] + z[0]; \
    c[0] = (fabs(s[0]) >= fabs(z[0])) ? (s[0] - t[0]) + z[0] : (z[0] - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
    t[1] = s[1] + z[1]; \
    c[1] = (fabs(s[1]) >= fabs(z[1])) ? (s[1] - t[1]) + z[1] : (z[1] - t[1]) + s[1]; \
    s[1] = t[1]; \
    t[1] = cs[1] + c[1]; \
    cc[1] = (fabs(cs[1]) >= fabs(c[1])) ? (cs[1] - t[1]) + c[1] : (c[1] - t[1]) + cs[1]; \
    cs[1] = t[1]; \
    ccs[1] = ccs[1] + cc[1]; \
  } \
}

#define addup_finalise sumup_finalise

LUAI_FUNC void agenaV_addup_sumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d) {
  Table *h;
  Node *key;
  TValue *val;
  const TValue *tm;
  int i;
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2];
  sumup_initvalues(s, t, c, cc, cs, ccs, x, z);
  h = hvalue(obj);
  tm = luaT_gettmbyobj(L, obj, TM_SUMUP);
  if (ttisnil(tm)) {  /* no metamethod ? */
    int flag = 0;
    for (i=0; i < h->sizearray; i++) {
      val = &h->array[i];
      addup_kahanbabuska_div(val, x, z, d, t, c, s, cc, cs, ccs, flag);
    }
    for (i -= h->sizearray; i < sizenode(h); i++) {
      key = gnode(h, i);
      if (ttisint(key2tval(key))) {  /* 3.15.3, ignore all non-integral keys in the hash part, behave like addup, mulup, qsumup */
        val = gval(key);
        addup_kahanbabuska_div(val, x, z, d, t, c, s, cc, cs, ccs, flag);
      }
    }
    addup_finalise(idx, s, cs, ccs, flag);
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, TM_SUMUP);
}

LUAI_FUNC void agenaV_addup_seqsumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d) {
  Seq *h;
  TValue *val;
  const TValue *tm;
  size_t i;  /* Agena 1.6.0 */
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2];
  sumup_initvalues(s, t, c, cc, cs, ccs, x, z);
  h = seqvalue(obj);
  tm = luaT_gettmbyobj(L, obj, TM_SUMUP);
  if (ttisnil(tm)) {  /* no metamethod ? */
    int flag = 0;
    for (i=0; i < h->size; i++) {
      val = seqitem(h, i);
      addup_kahanbabuska_div(val, x, z, d, t, c, s, cc, cs, ccs, flag);
    }
    addup_finalise(idx, s, cs, ccs, flag);
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, TM_SUMUP);
}

LUAI_FUNC void agenaV_addup_regsumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d) {
  Reg *h;
  TValue *val;
  const TValue *tm;
  size_t i;  /* Agena 1.6.0 */
  volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2];
  sumup_initvalues(s, t, c, cc, cs, ccs, x, z);
  h = regvalue(obj);
  tm = luaT_gettmbyobj(L, obj, TM_SUMUP);
  if (ttisnil(tm)) {  /* no metamethod ? */
    int flag = 0;
    for (i=0; i < h->top; i++) {
      val = regitem(h, i);
      addup_kahanbabuska_div(val, x, z, d, t, c, s, cc, cs, ccs, flag);
    }
    addup_finalise(idx, s, cs, ccs, flag);
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, TM_SUMUP);
}

#define addup_kahanbabuska_pmoment(val,x,n,p,mu,t,c,s,cc,cs,ccs,flag) { \
  if (ttisnumber(val)) { \
    x = tools_intpow(nvalue(val) - mu, p)/n; \
    t = s + x; \
    c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s; \
    s = t; \
    t = cs + c; \
    cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs; \
    cs = t; \
    ccs = ccs + cc; \
    flag = 1; \
  } \
}

void addup_sumup_pmoment (lua_State *L, const TValue *obj, StkId idx, lua_Number n, lua_Number p, lua_Number mu) {
  Table *h;
  Node *key;
  TValue *val;
  const TValue *tm;
  int i;
  volatile lua_Number s, cs, ccs, t, c, cc, x;
  s = cs = ccs = 0;
  h = hvalue(obj);
  tm = luaT_gettmbyobj(L, obj, TM_SUMUP);
  if (ttisnil(tm)) {  /* no metamethod ? */
    int flag = 0;
    for (i=0; i < h->sizearray; i++) {
      val = &h->array[i];
      addup_kahanbabuska_pmoment(val, x, n, p, mu, t, c, s, cc, cs, ccs, flag);
    }
    for (i -= h->sizearray; i < sizenode(h); i++) {
      key = gnode(h, i);
      if (ttisint(key2tval(key))) {  /* 3.15.3, ignore all non-integral keys in the hash part, behave like addup, mulup, qsumup, qmdev */
        val = gval(key);
        addup_kahanbabuska_pmoment(val, x, n, p, mu, t, c, s, cc, cs, ccs, flag);
      }
    }
    if (flag) {
      setnvalue(idx, s + cs + ccs);  /* no rounding in between */
    } else {
      setnilvalue(idx);
    }
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, TM_SUMUP);
}

void addup_seqsumup_pmoment (lua_State *L, const TValue *obj, StkId idx, lua_Number n, lua_Number p, lua_Number mu) {
  Seq *h;
  TValue *val;
  const TValue *tm;
  size_t i;  /* Agena 1.6.0 */
  volatile lua_Number s, cs, ccs, t, c, cc, x;
  s = cs = ccs = 0;
  h = seqvalue(obj);
  tm = luaT_gettmbyobj(L, obj, TM_SUMUP);
  if (ttisnil(tm)) {  /* no metamethod ? */
    int flag = 0;
    for (i=0; i < h->size; i++) {
      val = seqitem(h, i);
      addup_kahanbabuska_pmoment(val, x, n, p, mu, t, c, s, cc, cs, ccs, flag);
    }
    if (flag) {
      setnvalue(idx, s + cs + ccs);  /* no rounding in between */
    } else {
      setnilvalue(idx);
    }
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, TM_SUMUP);
}

void addup_regsumup_pmoment (lua_State *L, const TValue *obj, StkId idx, lua_Number n, lua_Number p, lua_Number mu) {
  Reg *h;
  TValue *val;
  const TValue *tm;
  size_t i;  /* Agena 1.6.0 */
  volatile lua_Number s, cs, ccs, t, c, cc, x;
  s = cs = ccs = 0;
  h = regvalue(obj);
  tm = luaT_gettmbyobj(L, obj, TM_SUMUP);
  if (ttisnil(tm)) {  /* no metamethod ? */
    int flag = 0;
    for (i=0; i < h->top; i++) {
      val = regitem(h, i);
      addup_kahanbabuska_pmoment(val, x, n, p, mu, t, c, s, cc, cs, ccs, flag);
    }
    if (flag) {
      setnvalue(idx, s + cs + ccs);  /* no rounding in between */
    } else {
      setnilvalue(idx);
    }
  } else if (ttisfunction(tm))
    call_binTM(L, obj, luaO_nilobject, idx, TM_SUMUP);
}


#define addup_kahanbabuska_fn(val,x,t,c,s,cc,cs,ccs,flag,arg) { \
  if (ttisnumber(val)) { \
    if (flag == LUA_TNIL) flag = LUA_TNUMBER; \
    x = nvalue(val); \
    t[0] = s[0] + x; \
    c[0] = (fabs(s[0]) >= fabs(x)) ? (s[0] - t[0]) + x : (x - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
  } else if (ttistrue(val) && ttisnumber(arg)) { \
    if (flag == LUA_TNIL) flag = LUA_TNUMBER; \
    x = nvalue(arg); \
    t[0] = s[0] + x; \
    c[0] = (fabs(s[0]) >= fabs(x)) ? (s[0] - t[0]) + x : (x - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
  } else if (ttiscomplex(val)) { \
    if (flag == LUA_TNIL || flag == LUA_TNUMBER) flag = LUA_TCOMPLEX; \
    z[0] = complexreal(val); z[1] = compleximag(val); \
    t[0] = s[0] + z[0]; \
    c[0] = (fabs(s[0]) >= fabs(z[0])) ? (s[0] - t[0]) + z[0] : (z[0] - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
    t[1] = s[1] + z[1]; \
    c[1] = (fabs(s[1]) >= fabs(z[1])) ? (s[1] - t[1]) + z[1] : (z[1] - t[1]) + s[1]; \
    s[1] = t[1]; \
    t[1] = cs[1] + c[1]; \
    cc[1] = (fabs(cs[1]) >= fabs(c[1])) ? (cs[1] - t[1]) + c[1] : (c[1] - t[1]) + cs[1]; \
    cs[1] = t[1]; \
    ccs[1] = ccs[1] + cc[1]; \
  } else if (ttistrue(val) && ttiscomplex(arg)) { \
    if (flag == LUA_TNIL || flag == LUA_TNUMBER) flag = LUA_TCOMPLEX; \
    z[0] = complexreal(arg); z[1] = compleximag(arg); \
    t[0] = s[0] + z[0]; \
    c[0] = (fabs(s[0]) >= fabs(z[0])) ? (s[0] - t[0]) + z[0] : (z[0] - t[0]) + s[0]; \
    s[0] = t[0]; \
    t[0] = cs[0] + c[0]; \
    cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0]; \
    cs[0] = t[0]; \
    ccs[0] = ccs[0] + cc[0]; \
    t[1] = s[1] + z[1]; \
    c[1] = (fabs(s[1]) >= fabs(z[1])) ? (s[1] - t[1]) + z[1] : (z[1] - t[1]) + s[1]; \
    s[1] = t[1]; \
    t[1] = cs[1] + c[1]; \
    cc[1] = (fabs(cs[1]) >= fabs(c[1])) ? (cs[1] - t[1]) + c[1] : (c[1] - t[1]) + cs[1]; \
    cs[1] = t[1]; \
    ccs[1] = ccs[1] + cc[1]; \
  } \
}

void agenaV_addup (lua_State *L, const TValue *obj, StkId idx, int nargs) {  /* 3.6.0 */
  lua_lock(L);
  if (nargs == 0) {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": need at least one argument.", "addup");
  } else if (nargs == 1) {
    if (ttistable(idx)) {
      agenaV_sumup(L, obj, idx, TM_SUMUP);
    } else if (ttisseq(idx)) {
      agenaV_seqsumup(L, obj, idx, TM_SUMUP);
    } else if (ttisreg(idx)) {
      agenaV_regsumup(L, obj, idx, TM_SUMUP);
    } else if (ttisuserdata(idx)) {
      if (!call_binTM(L, obj, luaO_nilobject, idx, TM_SUMUP)) {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": calling metamethod for userdata failed.", "addup");
      }
    } else {
      lua_unlock(L);
      luaG_runerror(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.",
        "addup", luaT_typenames[(int)ttype(obj)]);
    }
  } else {  /* nargs >= 2 */
    int i;
    /* divide each element by a number before summing up */
    if (nargs == 2 && ttisnumber(idx + 1)) {
      lua_Number d = nvalue(idx + 1);
      if (tools_isnan(d) || d == 0) {  /* determine structure size automatically */
        d = aux_getstructuresize(L, obj, idx);
      }
      if (d == 0) {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": %s is empty or 2nd argument is zero.", "addup", luaT_typenames[(int)ttype(idx)]);
      }
      if (ttistable(idx)) {
        agenaV_addup_sumup_div(L, obj, idx, d);
        for (i=1; i < nargs; i++) setnilvalue(idx + i);
      } else if (ttisseq(idx)) {
        agenaV_addup_seqsumup_div(L, obj, idx, d);
        for (i=1; i < nargs; i++) setnilvalue(idx + i);
      } else if (ttisreg(idx)) {
        agenaV_addup_regsumup_div(L, obj, idx, d);
        for (i=1; i < nargs; i++) setnilvalue(idx + i);
      } else if (ttisuserdata(idx)) {
        if (!call_binTM(L, obj, luaO_nilobject, idx, TM_SUMUP)) {
          lua_unlock(L);
          luaG_runerror(L, "Error in " LUA_QS ": calling metamethod for userdata failed.", "addup");
        }
        for (i=1; i < nargs; i++) setnilvalue(idx + i);
      } else {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.",
          "addup", luaT_typenames[(int)ttype(idx)]);
      }
    } else if (nargs == 4 && !ttisfunction(idx) && ttisnumber(idx + 1) && ttisnumber(idx + 2) && ttisnumber(idx + 3)) {
      /* p - moment: addup(tbl, n, p, mu) */
      lua_Number n, p, mu;
      n = nvalue(idx + 1);
      p = nvalue(idx + 2);
      mu = nvalue(idx + 3);
      if (tools_isnan(n) || n == 0) {  /* determine structure size automatically */
        n = aux_getstructuresize(L, obj, idx);
      }
      if (n == 0) {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": %s is empty or 2nd argument is zero.", "addup", luaT_typenames[(int)ttype(idx)]);
      }
      if (ttistable(idx)) {
        addup_sumup_pmoment(L, obj, idx, n, p, mu);
        for (i=1; i < nargs; i++) setnilvalue(idx + i);
      } else if (ttisseq(idx)) {
        addup_seqsumup_pmoment(L, obj, idx, n, p, mu);
        for (i=1; i < nargs; i++) setnilvalue(idx + i);
      } else if (ttisreg(idx)) {
        addup_regsumup_pmoment(L, obj, idx, n, p, mu);
        for (i=1; i < nargs; i++) setnilvalue(idx + i);
      } else if (ttisuserdata(idx)) {
        lua_unlock(L);
        if (!call_binTM(L, obj, luaO_nilobject, idx, TM_SUMUP)) {
          luaG_runerror(L, "Error in " LUA_QS ": calling metamethod for userdata failed.", "addup");
        }
        for (i=1; i < nargs; i++) setnilvalue(idx + i);
      } else {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.",
          "addup", luaT_typenames[(int)ttype(idx)]);
      }
    } else if (nargs > 2 && ttisfunction(idx) && ttisnumber(idx + 1) && ttisnumber(idx + 2)) {  /* approximate series, 3.11.4 */
      StkId f, res;
      ptrdiff_t result;
      lua_Number i, start, stop;
      int k, multivariate, complexresult;
      volatile lua_Number s, cs, ccs, s_imag, imag_cs, imag_ccs, step;
      start = nvalue(idx + 1);
      stop = nvalue(idx + 2);
      step = (nargs >= 4 && ttisnumber(idx + 3)) ? nvalue(idx + 3) : 1;
      multivariate = (nargs > 4)*(nargs - 5 + 1);  /* number of `surplus` arguments to f or 0 if none given */
      if (start > stop && step > 0) step = -step;
      f = idx;
      result = savestack(L, idx);  /* the following function calls may change the stack */
      cs = ccs = imag_cs = imag_ccs = 0.0;
      s = 0.0; s_imag = 0.0;
      complexresult = 0;
      if (tools_isint(start) && tools_isint(step)) {
        for (i=start; i <= stop; i += step) {
          luaD_checkstack(L, 2 + multivariate);
          setobj2s(L, L->top, f);    /* push function */
          setnvalue(L->top + 1, i);  /* push corrected iteration value */
          for (k=4; k < nargs; k++) setobj2s(L, L->top + k - 2, idx + k);
          L->top += 2 + multivariate;
          luaD_call(L, L->top - 2 - multivariate, 1, 0);
          if (ttisnumber(L->top - 1)) {  /* update result */
            s = tools_kbadd(s, nvalue(L->top - 1), &cs, &ccs);
          } else if (ttiscomplex(L->top - 1)) {  /* 3.15.0 */
            TValue *r = s2v(L->top - 1);
            s = tools_kbadd(s, complexreal(r), &cs, &ccs);
            s_imag = tools_kbadd(s_imag, compleximag(r), &imag_cs, &imag_ccs);
            complexresult = 1;
          }
          L->top--;
        }
      } else {
        volatile lua_Number heps, iter_cs, iter_ccs, i, j;
        if (start > stop) goto quit;  /* return 0, 3.15.1 */
        iter_cs = iter_ccs = 0.0;
        i = j = start;
        heps = agn_gethepsilon(L);
        do {
          luaD_checkstack(L, 2 + multivariate);
          setobj2s(L, L->top, f);  /* push function */
          setnvalue(L->top + 1, (fabs(j) < heps) ? 0 : j);  /* push iteration value */
          for (k=4; k < nargs; k++) setobj2s(L, L->top + k - 2, idx + k);
          L->top += 2 + multivariate;
          luaD_call(L, L->top - 2 - multivariate, 1, 0);
          if (ttisnumber(L->top - 1)) {  /* update result */
            s = tools_kbadd(s, nvalue(L->top - 1), &cs, &ccs);
          } else if (ttiscomplex(L->top - 1)) {  /* 3.15.0 */
            TValue *r = s2v(L->top - 1);
            s = tools_kbadd(s, complexreal(r), &cs, &ccs);
            s_imag = tools_kbadd(s_imag, compleximag(r), &imag_cs, &imag_ccs);
            complexresult = 1;
          }
          L->top--;
          /* update loop counter */
          i = tools_kbadd(i, step, &iter_cs, &iter_ccs);
          j = i + iter_cs + iter_ccs;
        } while (luai_numlt(0, step) ? luai_numle(j, stop) : luai_numle(stop, j) || tools_approx(j, stop, heps));
      }
quit:
      res = restorestack(L, result);
      if (complexresult) {  /* 3.15.0 */
        setrealimagvalue(res, s + cs + ccs, s_imag + imag_cs + imag_ccs);
      } else {
        setnvalue(res, s + cs + ccs);   /* no rounding in between */
      }
    } else if (ttisfunction(idx)) {
      int flag;
      StkId res;
      ptrdiff_t result;
      const TValue *tm;
      Node *key;
      TValue *val;
      volatile lua_Number s[2], cs[2], ccs[2], t[2], c[2], cc[2], x, z[2];
      sumup_initvalues(s, t, c, cc, cs, ccs, x, z);
      flag = LUA_TNIL;
      result = savestack(L, idx);  /* the following function calls may change the stack */
      tm = luaT_gettmbyobj(L, idx + 1, TM_SUMUP);
      if (ttisfunction(tm) && !call_binTM(L, idx + 1, luaO_nilobject, idx + 1, TM_SUMUP)) {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": calling metamethod for userdata failed.", "addup");
      };
      if (ttistable(idx + 1)) {
        Table *h;
        size_t i, j;
        h = hvalue(idx + 1);
        api_check(L, L->top < L->ci->top);
        for (i=0; i < h->sizearray; i++) {
          val = &h->array[i];
          if (ttisnotnil(val)) {
            luaD_checkstack(L, nargs);
            setobj2s(L, L->top, idx);  /* push function */
            setobj2s(L, L->top + 1, val);  /* 1st argument */
            for (j=2; j < nargs; j++) setobj2s(L, L->top + j, idx + j);
            L->top += nargs;
            luaD_call(L, L->top - nargs, 1, 0);
            addup_kahanbabuska_fn(s2v(L->top - 1), x, t, c, s, cc, cs, ccs, flag, val);
            L->top--;
          }  /* of if */
        }  /* of for i */
        for (i -= h->sizearray; i < sizenode(h); i++) {
          key = gnode(h, i);
          val = gval(key);
          /* 3.15.3, ignore all non-integral keys in the hash part, behave like addup, mulup, qsumup */
          if (ttisnotnil(val) && ttisint(key2tval(key))) {
            luaD_checkstack(L, nargs);
            setobj2s(L, L->top, idx);  /* push function */
            setobj2s(L, L->top + 1, val);  /* 1st argument */
            for (j=2; j < nargs; j++) setobj2s(L, L->top + j, idx + j);
            L->top += nargs;
            luaD_call(L, L->top - nargs, 1, 0);
            addup_kahanbabuska_fn(s2v(L->top - 1), x, t, c, s, cc, cs, ccs, flag, val);
            L->top--;
          }
        }
      } else if (ttisseq(idx + 1)) {
        Seq *h;
        size_t i, j;
        h = seqvalue(idx + 1);
        api_check(L, L->top < L->ci->top);
        for (i=0; i < h->size; i++) {
          luaD_checkstack(L, nargs);
          setobj2s(L, L->top, idx);  /* push function */
          val = seqitem(h, i);  /* 3.11.2 extension */
          setobj2s(L, L->top + 1, val);  /* 1st argument */
          for (j=2; j < nargs; j++) setobj2s(L, L->top + j, idx + j);
          L->top += nargs;
          luaD_call(L, L->top - nargs, 1, 0);
          addup_kahanbabuska_fn(s2v(L->top - 1), x, t, c, s, cc, cs, ccs, flag, val);
          L->top--;
        }
      } else if (ttisreg(idx + 1)) {
        Reg *h;
        size_t i, j;
        h = regvalue(idx + 1);
        api_check(L, L->top < L->ci->top);
        for (i=0; i < h->top; i++) {
          luaD_checkstack(L, nargs);
          setobj2s(L, L->top, idx);  /* push function */
          val = regitem(h, i);
          setobj2s(L, L->top + 1, val);  /* 1st argument */
          for (j=2; j < nargs; j++) setobj2s(L, L->top + j, idx + j);
          L->top += nargs;
          luaD_call(L, L->top - nargs, 1, 0);
          addup_kahanbabuska_fn(s2v(L->top - 1), x, t, c, s, cc, cs, ccs, flag, val);
          L->top--;  /* ditto */
        }
      } else if (ttisuserdata(idx)) {
        if (!call_binTM(L, obj, luaO_nilobject, idx, TM_SUMUP)) {
          lua_unlock(L);
          luaG_runerror(L, "Error in " LUA_QS ": calling metamethod for userdata failed.", "addup");
        }
      } else {
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": table, sequence, register or userdata expected for 2nd argument, got %s.", "addup",
          luaT_typenames[(int)ttype(idx)]);
      }
      res = restorestack(L, result);
      sumup_finalise(res, s, cs, ccs, flag);
    } else {
      lua_unlock(L);
      luaG_runerror(L, "Error in " LUA_QS ": wrong mix of arguments.", "addup");
    }
  }
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


#define a_istrue(o)        (bvalue(o) == 1)
#define a_isfalseorfail(o) (bvalue(o) == 0 || bvalue(o) == 2)  /* explicitly does not check for null */

/* XOR operator, 0.27.0, 23.08.2009, extended 0.27.1, 05.09.2009, patched 1.3.1, 09.01.2011, changed 1.6.3, 04.06.2012,
   changed 2.2.2, 27/06/2014, changed 2.28.4 */

void agenaV_xor (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisboolean(idxb) && ttisboolean(idxc)) {
    /* a bit slower: setbvalue(idxa, ( (bvalue(idxb) == 2) ? 0 : bvalue(idxb) ) != ( (bvalue(idxc) == 2) ? 0 : bvalue(idxc) )); */
    setbvalue(idxa, (a_istrue(idxb) && a_isfalseorfail(idxc)) || (a_isfalseorfail(idxb) && a_istrue(idxc)));  /* tuned 2.8.5 */
  } else if (ttisnil(idxc)) {
    setobjs2s(L, idxa, idxb);
  } else {
    setobjs2s(L, idxa, idxc);
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* XNOR operator (if-and-only-if), 2.8.5, 31.08.2015 */

void agenaV_xnor (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisnumber(idxb) && ttisnumber(idxc)) {  /* 2.12.3 */
    setnvalue(idxa, ~((LUA_INT32)nvalue(idxb) ^ (LUA_INT32)nvalue(idxc)));  /* unsigned operation would not yield sensible results */
  } else if (ttisboolean(idxb) && ttisboolean(idxc)) {
    setbvalue(idxa, (bvalue(idxb) == bvalue(idxc)) || (a_isfalseorfail(idxb) && a_isfalseorfail(idxc)));  /* opposite of xor */
  } else if (ttisnil(idxc)) {
    setobjs2s(L, idxa, idxc);
  } else {
    setobjs2s(L, idxa, idxb);
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* NAND operator, 2.5.2, 04/04/2015 */

void agenaV_nand (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisnumber(idxb) && ttisnumber(idxc)) {  /* 2.12.3 */
    setnvalue(idxa, ~((LUA_INT32)nvalue(idxb) & (LUA_INT32)nvalue(idxc)));  /* unsigned operation would not yield sensible results */
  } else if (ttisboolean(idxb) && ttisboolean(idxc)) {
    setbvalue(idxa, a_isfalseorfail(idxb) || a_isfalseorfail(idxc));  /* tuned 2.8.5 */
  } else if (ttisnil(idxb) || l_isfalseorfail(idxb)) {
    setbvalue(idxa, 1);
  } else if (ttisnil(idxc) || l_isfalseorfail(idxc)) {
    setbvalue(idxa, 1);
  } else {
    setbvalue(idxa, 0);
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* NOR operator, 2.5.2, 04/04/2015 */

void agenaV_nor (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisnumber(idxb) && ttisnumber(idxc)) {  /* 2.12.3 */
    setnvalue(idxa, ~((LUA_INT32)nvalue(idxb) | (LUA_INT32)nvalue(idxc)));  /* unsigned operation would not yield sensible results */
  } else if (ttisboolean(idxb) && ttisboolean(idxc)) {
    setbvalue(idxa, !(a_istrue(idxb) || a_istrue(idxc)));  /* tuned 2.8.5 */
  } else if (ttisnil(idxb) || l_isfalseorfail(idxb)) {
    setbvalue(idxa, ttisnil(idxc) || l_isfalseorfail(idxc));
  } else if (ttisnil(idxc) || l_isfalseorfail(idxc)) {
    setbvalue(idxa, ttisnil(idxb) || l_isfalseorfail(idxb));
  } else {
    setbvalue(idxa, 0);
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* BAND (&&) operator, 0.27.0, 26.08.2009, patched 1.3.1, 09.01.2011, changed 1.6.3, 04.06.2012, extended to 64 bit ints 2.9.0 */

void agenaV_band (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisnumber(idxb) && ttisnumber(idxc)) {
    if (L->settings & 1) {  /* signed bits operation ? */
      setnvalue(idxa, (LUA_INT32)nvalue(idxb) & (LUA_INT32)nvalue(idxc)); }  /* changed back to 32-bit operation 2.15.0 */
    else {
      setnvalue(idxa, (LUA_UINT32)nvalue(idxb) & (LUA_UINT32)nvalue(idxc)); }
  } else if (!call_binTM(L, idxb, idxc, idxa, TM_BAND)) {  /* 4.8.0 */
    luaG_runerror(L, "Error in " LUA_QS ": calling metamethod failed.", "&&");
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* BOR (||) operator, 0.27.0, 26.08.2009, patched 1.3.1, 09.01.2011, changed 1.6.3, 04.06.2012, extended to 64 bit ints 2.9.0 */

void agenaV_bor (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisnumber(idxb) && ttisnumber(idxc)) {
    if (L->settings & 1) {  /* signed bits operation ? */
      setnvalue(idxa, (LUA_INT32)nvalue(idxb) | (LUA_INT32)nvalue(idxc)); }  /* changed back to 32-bit operation 2.15.0 */
    else {
      setnvalue(idxa, (LUA_UINT32)nvalue(idxb) | (LUA_UINT32)nvalue(idxc)); }
  } else if (!call_binTM(L, idxb, idxc, idxa, TM_BOR)) {  /* 4.8.0 */
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": calling metamethod failed.", "||");
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* BXOR (^^) operator, 0.27.0, 26.08.2009, patched 1.3.1, 09.01.2011, changed 1.6.3, 04.06.2012, extended to 64 bit ints 2.9.0  */

void agenaV_bxor (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisnumber(idxb) && ttisnumber(idxc)) {
    if (L->settings & 1) {  /* signed bits operation ? */
      setnvalue(idxa, (LUA_INT32)nvalue(idxb) ^ (LUA_INT32)nvalue(idxc)); }  /* changed back to 32-bit operation 2.15.0 */
    else {
      setnvalue(idxa, (LUA_UINT32)nvalue(idxb) ^ (LUA_UINT32)nvalue(idxc)); }
  } else if (!call_binTM(L, idxb, idxc, idxa, TM_BXOR)) {  /* 4.8.0 */
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": calling metamethod failed.", "^^");
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* PERCENT operator, 1.10.6, 10.04.2013 */

void agenaV_percent (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisnumber(idxb) && ttisnumber(idxc)) {
    setnvalue(idxa, luai_numpercent(nvalue(idxb), nvalue(idxc)));
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": numbers expected, got %s, %s.", "*%",
      luaT_typenames[(int)ttype(idxb)], luaT_typenames[(int)ttype(idxc)]);  /* 3.7.8 improvement */
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* PERCENT operator, 1.11.4, 13.05.2013 */

void agenaV_percentratio (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisnumber(idxb) && ttisnumber(idxc)) {
    setnvalue(idxa, luai_numpercentratio(nvalue(idxb), nvalue(idxc)));
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": numbers expected, got %s, %s.", "/%",
      luaT_typenames[(int)ttype(idxb)], luaT_typenames[(int)ttype(idxc)]);  /* 3.7.8 improvement */
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* PERCENTADD operator, 1.11.3, 09.05.2013 */

void agenaV_percentadd (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisnumber(idxb) && ttisnumber(idxc)) {
    setnvalue(idxa, luai_numpercentadd(nvalue(idxb), nvalue(idxc)));
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": numbers expected, got %s, %s.", "+%",
      luaT_typenames[(int)ttype(idxb)], luaT_typenames[(int)ttype(idxc)]);  /* 3.7.8 improvement */
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* PERCENTSUB operator, 1.11.3, 09.05.2013 */

void agenaV_percentsub (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisnumber(idxb) && ttisnumber(idxc)) {
    setnvalue(idxa, luai_numpercentsub(nvalue(idxb), nvalue(idxc)));
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": numbers expected, got %s, %s.", "-%",
      luaT_typenames[(int)ttype(idxb)], luaT_typenames[(int)ttype(idxc)]);  /* 3.7.8 improvement */
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* PERCENTCHANGE operator, 2.10.0, 13.05.2013 */

void agenaV_percentchange (lua_State *L, StkId idxa, StkId idxb, StkId idxc) {
  lua_lock(L);
  if (ttisnumber(idxb) && ttisnumber(idxc)) {
    setnvalue(idxa, luai_numpercentchange(nvalue(idxb), nvalue(idxc)));
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": numbers expected, got %s, %s.", "%%",
      luaT_typenames[(int)ttype(idxb)], luaT_typenames[(int)ttype(idxc)]);  /* 3.7.8 improvement */
  }
  /* 2.5.2, do not nil idxb and idxc since it will confuse the stack ! */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
}


/* @ operator, 2.2.0, 20.05.2014, fixed 2.2.5: Agena crashed if larger structures have been processed. Rewritten 2.29.3;
   3.17.5 security fix by re-introducing the safe version of 3.2.1 and earlier that uses the `map` function, does not
   iterate over a structure and call the mapping function directly as this changes the stack. */

/* Unfortunately, we cannot directly iterate through a structure and each time call the mapping or selection function in the VM
   as this will sometimes caused invalid read attempts into free'd memory. Thus we have to use the `map` and `select` functions.
   2.29.3 */
#define pushandcall(L,procname) { \
  api_check(L, L->top < L->ci->top); \
  luaD_checkstack(L, 3); \
  setobj2s(L, L->top, luaH_getstr(hvalue(gt(L)), luaS_new(L, procname))); \
  setobj2s(L, L->top + 1, idxb); \
  setobj2s(L, L->top + 2, idxc); \
  L->top += 3; \
  luaD_call(L, L->top - 3, 1, 1); \
}

/* 2.31.1: map and select set user-defined type and metatable themselves, so we do not need to do this here. */
#define returnresult(L,result,res) { \
  res = restorestack(L, result); \
  setobjs2s(L, res, L->top - 1); \
  L->top--; \
}

/* We can not set a Boolean value with setobjs2s, so do this */
#define returnboolresult(L,result,res) { \
  res = restorestack(L, result); \
  setbvalue(res, ttistrue(L->top - 1)); \
  L->top--; \
}


static void agenaV_map (lua_State *L, const TValue *idxb, const TValue *idxc, StkId idxa) {
  StkId res;  /* 2.3.1 fix */
  ptrdiff_t result;
  if (!ttisfunction(idxb)) {
    /* 2.29.2 extension; a @ b := if b = 0 then a else a*b fi */
    if (ttisnumber(idxb) && ttisnumber(idxc)) {
      lua_Number nb = nvalue(idxb), nc = nvalue(idxc);
      if (nc == 0) {
        setnvalue(idxa, nb);
      } else {
        setnvalue(idxa, luai_nummul(nb, nc));
      }
    } else if (ttiscomplex(idxb) && ttiscomplex(idxc)) {
#ifndef PROPCMPLX
      agn_Complex nb = cvalue(idxb), nc = cvalue(idxc);
      if (nc == 0 + 0*I) {
        setcvalue(idxa, nb);
      } else {
        setcvalue(idxa, agnc_mul(nb, nc));
      }
#else
      lua_Number nb1, nb2, nc1, nc2, z[2];
      nb1 = complexreal(idxb); nb2 = compleximag(idxb);
      nc1 = complexreal(idxc); nc2 = compleximag(idxc);
      if (nc1 == 0 && nc2 == 0) {
        z[0] = nb1; z[1] = nb2;
        setcvalue(idxa, z);
      } else {
        agnc_mul(z, nb1, nb2, nc1, nc2); setcvalue(idxa, z);  /* 3.15.0 fix */
      }
#endif
    } else if (ttiscomplex(idxb) && ttisnumber(idxc)) {
#ifndef PROPCMPLX
      agn_Complex nb = cvalue(idxb), nc = nvalue(idxc);
      if (nc == 0 + 0*I) {
        setcvalue(idxa, nb);
      } else {
        setcvalue(idxa, agnc_mul(nb, nc));
      }
#else
      lua_Number nb1, nb2, nc, z[2];
      nb1 = complexreal(idxb); nb2 = compleximag(idxb);
      nc = nvalue(idxc);
      if (nc == 0) {
        z[0] = nb1; z[1] = nb2;
        setcvalue(idxa, z);
      } else {
        agnc_mul(z, nb1, nb2, nc, 0); setcvalue(idxa, z);  /* 3.15.0 fix */
      }
#endif
    } else if (ttisnumber(idxb) && ttiscomplex(idxc)) {
#ifndef PROPCMPLX
      agn_Complex nb = nvalue(idxb), nc = cvalue(idxc);
      if (nc == 0 + 0*I) {
        setcvalue(idxa, nb);
      } else {
        setcvalue(idxa, agnc_mul(nb, nc));
      }
#else
      lua_Number nb, nc1, nc2, z[2];
      nb = nvalue(idxb);
      nc1 = complexreal(idxc); nc2 = compleximag(idxc);
      if (nc1 == 0 && nc2 == 0) {
        z[0] = nb; z[1] = 0;
        setcvalue(idxa, z);
      } else {
        agnc_mul(z, nb, 0, nc1, nc2); setcvalue(idxa, z);  /* 3.15.0 fix */
      }
#endif
    } else if (ttisnumber(idxb) && ttisboolean(idxc)) {
      lua_Number nb = nvalue(idxb), nc = bvalue(idxc);
      if (nc == 2) nc = 0;
      if (nc == 0) {
        setnvalue(idxa, nb);
      } else {
        setnvalue(idxa, luai_nummul(nb, nc));
      }
    } else if (ttisboolean(idxb) && ttisnumber(idxc)) {
      lua_Number nb = bvalue(idxb), nc = nvalue(idxc);
      if (nb == 2) nb = 0;
      if (nc == 0) {
        setnvalue(idxa, nb);
      } else {
        setnvalue(idxa, luai_nummul(nb, nc));
      }
    } else if (ttisboolean(idxb) && ttisboolean(idxc)) {
      lua_Number nb = bvalue(idxb), nc = bvalue(idxc);
      if (nb == 2) nb = 0;
      if (nc == 2) nc = 0;
      if (nc == 0) {
        setnvalue(idxa, nb);
      } else {
        setnvalue(idxa, luai_nummul(nb, nc));
      }
    } else {
      luaG_runerror(L, "Error in " LUA_QS ": procedure, (complex) number or boolean expected, got %s.", "@",
        luaT_typenames[(int)ttype(idxb)]);
    }
    return;
  }
  checkstack2(L, 2 + ttisfunction(idxc), "Error in ", "`vmmap` function", "@");  /* 3.17.6 security fuse */
  result = savestack(L, idxa);  /* the following function calls may change the stack */
  if (ttistable(idxc)) {
    pushandcall(L, "vmmap");
    /* move new structure to final position */
    returnresult(L, result, res);
  } else if (ttisseq(idxc)) {
    pushandcall(L, "vmmap");
    /* move new structure to final position */
    returnresult(L, result, res);
  } else if (ttisuset(idxc)) {
    pushandcall(L, "vmmap");
    /* move new structure to final position */
    returnresult(L, result, res);
  } else if (ttispair(idxc)) {
    pushandcall(L, "vmmap");
    /* move new structure to final position */
    returnresult(L, result, res);
  } else if (ttisreg(idxc)) {  /* 2.3.0 RC 3 */
    pushandcall(L, "vmmap");
    /* move new structure to final position */
    returnresult(L, result, res);
  } else if (ttisfunction(idxc)) {  /* 2.12.0 RC 1, by suggestion of Slobodan;
    fixed 2.25.0 for resulting composite function had previously not been properly anchored into the stack; see lvm.c/callTMres */
    lua_lock(L);
    pushandcall(L, "vmmap");
    res = restorestack(L, result);
    L->top--;
    setobj2s(L, res, L->top);  /* 2.26.0 fix */
    lua_unlock(L);
  } else if (ttisstring(idxc)) {  /* 7.6.11 */
    pushandcall(L, "vmmap");
    /* move new structure to final position */
    returnresult(L, result, res);
  } else if (ttisnil(idxc)) {  /* 4.12.1 */
    pushandcall(L, "vmmap");
    /* move new structure to final position */
    returnresult(L, result, res);
  } else {
    luaG_runerror(L, "Error in " LUA_QS ": structure, string or function expected, got %s.", "@",
      luaT_typenames[(int)ttype(idxc)]);
  }
  return;
}


/* SELECT/$ operator, 2.2.5, 17.07.2014, based on agenaV_map; rewritten 2.29.3; 3.17.6 reintroduced old safe version */
static void agenaV_select (lua_State *L, const TValue *idxb, const TValue *idxc, StkId idxa) {
  StkId res;  /* 2.3.1 fix */
  ptrdiff_t result;
  checkstack2(L, 2, "Error in ", "`vmselect` function", "$");  /* 3.17.6 security fuse */
  if (!ttisfunction(idxb)) {
    luaG_runerror(L, "Error in " LUA_QS ": procedure expected, got %s.", "$", luaT_typenames[(int)ttype(idxb)]);
  }
  result = savestack(L, idxa);  /* the following function calls may change the stack */
  if (ttistable(idxc)) {
    pushandcall(L, "vmselect");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisseq(idxc)) {
    pushandcall(L, "vmselect");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisuset(idxc)) {
    pushandcall(L, "vmselect");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisreg(idxc)) {
    pushandcall(L, "vmselect");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisstring(idxc)) {  /* 7.6.11 */
    pushandcall(L, "vmselect");
    /* move new structure to final position */
    returnresult(L, result, res);
  } else if (ttisnil(idxc)) {  /* 7.6.11 */
    pushandcall(L, "vmselect");
    /* move new structure to final position */
    returnresult(L, result, res);
  } else {
    luaG_runerror(L, "Error in " LUA_QS ": structure expected, got %s.", "$",
      luaT_typenames[(int)ttype(idxc)]);
  }
}


/* `$$` selection operator, 2.22.2; 4.6.0 reintroduced in-situ iteration based on the 3.17.5 version.
    Changed 7.6.11. */
static void agenaV_has (lua_State *L, const TValue *idxb, const TValue *idxc, StkId idxa) {
  StkId res;  /* 2.3.1 fix */
  ptrdiff_t result;
  checkstack2(L, 2, "Error in ", "`vmcheckfor` function", "$$");  /* 3.17.6 security fuse */
  if (!ttisfunction(idxb)) {
    luaG_runerror(L, "Error in " LUA_QS ": procedure expected, got %s.", "$$", luaT_typenames[(int)ttype(idxb)]);
  }
  result = savestack(L, idxa);  /* the following function calls may change the stack */
  if (ttistable(idxc)) {
    pushandcall(L, "vmcheckfor");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisseq(idxc)) {
    pushandcall(L, "vmcheckfor");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisuset(idxc)) {
    pushandcall(L, "vmcheckfor");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisreg(idxc)) {
    pushandcall(L, "vmcheckfor");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisstring(idxc)) {
    pushandcall(L, "vmcheckfor");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisnil(idxc)) {
    pushandcall(L, "vmcheckfor");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else {
    luaG_runerror(L, "Error in " LUA_QS ": structure expected, got %s.", "$$",
      luaT_typenames[(int)ttype(idxc)]);
  }
}


/* $$$ OPERATOR, based on pre-3.17.6 `$$` operator, 4.6.0 */
void agenaV_count (lua_State *L, const TValue *idxb, const TValue *idxc, StkId idxa) {
  StkId res;
  ptrdiff_t result;
  checkstack2(L, 2, "Error in ", "`vmcount` function", "$$$");
  if (!ttisfunction(idxb)) {
    luaG_runerror(L, "Error in " LUA_QS ": procedure expected, got %s.", "$$$", luaT_typenames[(int)ttype(idxb)]);
  }
  result = savestack(L, idxa);  /* the following function calls may change the stack */
  if (ttistable(idxc)) {
    pushandcall(L, "vmcount");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisseq(idxc)) {
    pushandcall(L, "vmcount");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisuset(idxc)) {
    pushandcall(L, "vmcount");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisreg(idxc)) {
    pushandcall(L, "vmcount");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisstring(idxc)) {
    pushandcall(L, "vmcount");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else if (ttisnil(idxc)) {
    pushandcall(L, "vmcount");
    /* move new structure to final position, copy user-defined type and metatable */
    returnresult(L, result, res);
  } else {
    luaG_runerror(L, "Error in " LUA_QS ": structure expected, got %s.", "$$$",
      luaT_typenames[(int)ttype(idxc)]);
  }
}


/* INSTR operator, 0.27.3, 19.10.2009; extended 1.1.0, 09.12.2010, extended 1.3.2, 13.01.2011;
   patched 1.3.3, 06.02.2011; extended 1.10.0, 07.03.2013
   The function calls itself, so leave it here and do not transfer to lapi.c. */

static int agenaV_setpairints (lua_State *L, StkId idx, size_t a, size_t b) {  /* 1.10.0, patched 1.10.4 */
  Pair *p = agnPair_new(L);
  setnvalue(L->top, a);
  agnPair_seti(L, p, 1, L->top);
  luaC_barrierpair(L, p, L->top);
  setnvalue(L->top, b);
  agnPair_seti(L, p, 2, L->top);
  luaC_barrierpair(L, p, L->top);
  setpairvalue(L, idx, p);
  setnilvalue(L->top);  /* do NOT prematurely call luaC_checkGC to avoid memory leaks ! */
  return 1;  /* perform garbage collection later in `protected` VM */
}

int agenaV_instr (lua_State *L, StkId idx, int nargs) {
  int i;
  int dogc;
  dogc = 0;  /* tell VM to finally conduct GC if a pair has been pushed (0 = no, 1 = yes) */
  if (nargs < 2)
    luaG_runerror(L, "Error in " LUA_QS ": at least two arguments expected, got %d.", "strings.instr", nargs);
  lua_lock(L);
  if (ttisstring(idx) && ttisstring(idx + 1)) {  /* 1.1.0: taken from the former string.seek function */
    const char *s, *p;
    int plain, isreverse, numbergiven, borders;
    size_t s_len, p_len;
    ptrdiff_t init;
    plain = 0; isreverse = 0; init = 1; numbergiven = 0; borders = 0;
    for (i=2; i < nargs; i++) {
      if (ttisnumber(idx + i)) { init = nvalue(idx + i); numbergiven = 1; continue; }
      if (ttistrue(idx + i)) { plain = 1; continue; }  /* 7.9.2 fix */
      if (ttisstring(idx + i)) {  /* 1.10.0 */
        if (tools_streq(svalue(idx + i), "reverse")) { isreverse = 1; continue; }  /* 2.16.12 tweak */
        if (tools_streq(svalue(idx + i), "borders")) borders = 1;  /* 2.16.12 tweak */
      }
    }
    s = svalue(idx);
    p = svalue(idx + 1);
    s_len = tsvalue(idx)->len;
    p_len = tsvalue(idx + 1)->len;
    if (p_len == 0) {  /* 2.38.0 */
      for (i=0; i < nargs; i++) setnilvalue(idx + i);  /* push null and delete all arguments */
      return dogc;
    }
    if (!isreverse && plain) {  /* search from left to right */
      char *search;
      ptrdiff_t pos;
      init = tools_posrelat(init, s_len) - 1;
      if (init < 0) init = 0;
      if ((size_t)init >= s_len) {
        for (i=0; i < nargs; i++) setnilvalue(idx + i);  /* push null and delete all arguments */
        lua_unlock(L);
        return dogc;
      }
      search = strstr(s + init, p);
      if (!search)  /* 2.29.3 catch NULL situation */
        setnilvalue(idx);
      else {
        pos = search - s + 1;
        if (pos < 1) {
          setnilvalue(idx);
        } else {
          if (borders) {  /* 1.10.0 */
            dogc = agenaV_setpairints(L, idx, pos, pos + tsvalue(idx + 1)->len - 1);
          } else {
            setnvalue(idx, pos);
          }
        }
      }
    } else if (isreverse) {  /* search from right to left */
      size_t i;
      char *ss, *sp, *search;
      ptrdiff_t pos;
      init = tools_posrelat(-(numbergiven ? init : -1), s_len) - 1;
      if (init < 0) init = 0;
      if ((size_t)init >= s_len) {
        for (i=0; i < nargs; i++) setnilvalue(idx + i);  /* push null and delete all arguments */
        return dogc;
      }
      if (s_len == 0 || p_len == 0) {  /* bail out if string or pattern is empty */
        for (i=0; i < nargs; i++) setnilvalue(idx + i);  /* push null and delete all arguments */
        lua_unlock(L);
        return dogc;
      }
      ss = (char *)malloc(CHARSIZE*(s_len + 1));  /* Agena 1.0.4 */
      if (ss == NULL) luaG_runerror(L, "Error in " LUA_QS ": memory allocation failed.", "strings.instr");
      sp = (char *)malloc(CHARSIZE*(p_len + 1));  /* Agena 1.0.4 */
      if (sp == NULL) {
        xfree(ss);  /* 4.11.5 fix */
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": memory allocation failed.", "strings.instr");
      }
      assert(ss != NULL);
      assert(sp != NULL);
      /* reverse string content */
      for (i = s_len - 1; *s != '\0'; s++, i--) ss[i] = uchar(*s);
      ss[s_len] = '\0';
      for (i = p_len - 1; *p != '\0'; p++, i--) sp[i] = uchar(*p);
      sp[p_len] = '\0';
      search = strstr(ss + init, sp);
      if (!search)  /* 2.29.3: catch NULL situation */
        setnilvalue(idx);
      else {
        pos = search - ss + 1;
        if (pos < 1)  /* nothing has been found; */
          setnilvalue(idx);  /* push null and delete all arguments */
        else {
          if (borders) {  /* 1.10.0 */
            size_t start = s_len - pos - p_len + 2;
            dogc = agenaV_setpairints(L, idx, start, start + p_len - 1);
          } else {
            setnvalue(idx, s_len - pos - p_len + 2);
          }
        }
      }
      xfreeall(ss, sp);  /* Agena 1.0.4/1.10.4 */
    } else {  /* search with pattern matching */
      int anchor;
      const char *s1, *res;
      MatchState ms;
      init = tools_posrelat(init, s_len) - 1;
      if (init < 0) init = 0;
      if ((size_t)init >= s_len) {
        for (i=0; i < nargs; i++) setnilvalue(idx + i);  /* push null and delete all arguments */
        lua_unlock(L);
        return dogc;
      }
      /* checking with strpbrk for special characters gives no gain in speed */
      anchor = (*p == '^') ? (p++, 1) : 0;
      s1 = s + init;
      initMatchState(L, &ms, 0, s, s_len);  /* 7.9.2 7x boost */
      do {
        ms.level = 0;
        if ((res=match(&ms, s1, p, 0)) != NULL) {
          if (borders) {  /* 1.10.0 */
            dogc = agenaV_setpairints(L, idx, s1 - s + 1, res - s);
          } else {
            setnvalue(idx, s1 - s + 1);   /* start */
          }
          break;
        }
      } while (s1++ < ms.src_end && !anchor);
      if (res == NULL) setnilvalue(idx);  /* 1.3.3 fix */
    }
  } else if (ttisstring(idx) && ttisuset(idx + 1)) {  /* 2.32.5 extension */
    UltraSet *t;
    int i, j, rc;
    t = usvalue(idx + 1);
    rc = 0;
    for (i=0; i < sizenode(t); i++) {
      LNode *node = glnode(t, i);
      if (ttisstring(glkey(node))) {
        luaD_checkstack(L, nargs);  /* 3.7.6 hardening */
        setobj2s(L, L->top, idx);
        setobj2s(L, L->top + 1, key2tval(node));
        for (j=3; j <= nargs; j++) {
          setobj2s(L, L->top + j - 1, idx + j - 1);
        }
        luaD_checkstack(L, nargs);  /* 4.3.3 fix */
        L->top += nargs;  /* 3.7.6 fix */
        agenaV_instr(L, L->top - nargs, nargs);  /* 3.7.6 fix */
        L->top -= nargs;  /* 3.7.6 fix */
        if ( (rc = !ttisnil(L->top)) ) {  /* do we have a hit ? */
          setnilvalue(L->top);
          break;
        }
      } else if (!ttisnil(glkey(node))) {  /* 3.7.5 fix */
        lua_unlock(L);
        luaG_runerror(L, "Error in " LUA_QS ": strings in set expected, got %s.", "strings.instr",
          luaT_typenames[(int)ttype(glkey(node))]);  /* 3.7.5 extension */
      }
      setnilvalue(L->top);
    }
    setbvalue(idx, rc);
  } else {
    lua_unlock(L);
    luaG_runerror(L, "Error in " LUA_QS ": strings expected.", "strings.instr");
  }
  for (i=1; i < nargs; i++) setnilvalue(idx + i);  /* pattern not found - delete all arguments, set nulls instead */
  lua_unlock(L);
  /* do not change L->top here since this will confuse the stack */
  return dogc;  /* 1 = pushed a pair, conduct gc, 0 = pushed no pair, no gc */
}


void agenaV_log (lua_State *L, StkId ra, StkId rb) {
  if (ttisnumber(ra) && ttisnumber(rb)) {
    setnvalue(ra, tools_logbase(nvalue(ra), nvalue(rb)));  /* 2.40.0 INLINE change */
  }
  else if (ttiscomplex(ra) && ttisnumber(rb)) {
#ifndef PROPCMPLX
    agn_Complex z, b;
    z = cvalue(ra);
    b = nvalue(rb) + I*0;
    if (z == 0 + 0*I || b == 0 + 0*I || b == 1 + 0*I)
      setcvalue(ra, AGN_NAN)
    else
      setcvalue(ra, tools_clog(z)/tools_clog(b));
#else
/* > Re(t); 1;
     /  2     2\   /  2     2\
   ln\a0  + a1 / ln\b0  + b1 / + 4 argument(a0 + I a1) argument(b0 + I b1)
   -----------------------------------------------------------------------
                                2
                     /  2     2\                         2
                   ln\b0  + b1 /  + 4 argument(b0 + I b1)
> Im(t); 1;
     /  /  2     2\                                             /  2     2\\
   2 \ln\a0  + a1 / argument(b0 + I b1) - argument(a0 + I a1) ln\b0  + b1 //
 - -------------------------------------------------------------------------
                                 2
                      /  2     2\                         2
                    ln\b0  + b1 /  + 4 argument(b0 + I b1)
*/
    lua_Number a[2], z[2], b;
    a[0] = complexreal(ra); a[1] = compleximag(ra);
    b = nvalue(rb);
    if ((a[0] == 0.0 && a[1] == 0.0) || b == 0.0 || b == 1.0) {
      z[0] = AGN_NAN; z[1] = AGN_NAN;
      setcvalue(ra, z);
    } else if (b > 0) {
      lua_Number _a, ta, tb, td, la, lb;
      _a = sun_pytha(a[0], a[1]);  /* 4.5.2 precision tweaks */
      ta = sun_atan2(a[1], a[0]);
      tb = sun_atan2(0.0, b);  /* 4.5.2 tweak */
      la = sun_log(_a);
      lb = 2.0*sun_log(b);
      td = sun_pytha(lb, 2.0*tb);
      z[0] = (la*lb + 4*ta*tb)/td;
      z[1] = -2.0*((la*tb - ta*lb)/td);
    } else {  /* 4.5.4 extension for b < 0 */
      lua_Number _a, _b, ta, tb, td, la, lb;
      _a = sun_pytha(a[0], a[1]);
      _b = b*b;
      ta = sun_atan2(a[1], a[0]);
      tb = sun_atan2(0, b);
      la = sun_log(_a); lb = sun_log(_b);
      td = sun_pytha(lb, 2.0*tb);
      z[0] = (la*lb + 4.0*ta*tb)/td;
      z[1] = -2.0*((la*tb - ta*lb)/td);
    }
    setcvalue(ra, z);
#endif
  }
  else if (ttisnumber(ra) && ttiscomplex(rb)) {
#ifndef PROPCMPLX
    agn_Complex z, b;
    z = nvalue(ra)+I*0;
    b = cvalue(rb);
    if (z == 0 + 0*I || b == 0 + 0*I || b == 1 + 0*I)
      setcvalue(ra, AGN_NAN)
    else
      setcvalue(ra, tools_clog(z)/tools_clog(b));
#else
    lua_Number a, b[2], z[2];
    a = nvalue(ra);
    b[0] = complexreal(rb); b[1] = compleximag(rb);
    if (a == 0.0 || (b[1] == 0.0 && (b[0] == 0.0 || b[0] == 1.0))) {  /* 4.5.5 fix */
      z[0] = AGN_NAN; z[1] = AGN_NAN;
    } else {
      lua_Number _a, _b, ta, tb, td, la, lb;
      _a = a*a;  /* 4.5.2 tweak */
      _b = sun_pytha(b[0], b[1]);
      ta = sun_atan2(0.0, a);  /* 4.5.2 tweak */
      tb = sun_atan2(b[1], b[0]);
      la = sun_log(_a); lb = sun_log(_b);
      td = sun_pytha(lb, 2.0*tb);  /* 4.5.2 precision tweak */
      z[0] = (la*lb + 4.0*ta*tb)/td;
      z[1] = -2.0*((la*tb - ta*lb)/td);  /* 4.5.2 precision tweak */
    }
    setcvalue(ra, z);
#endif
  }
  else if (ttiscomplex(ra) && ttiscomplex(rb)) {
#ifndef PROPCMPLX
    agn_Complex z, b;
    z = cvalue(ra);
    b = cvalue(rb);
    if (z == 0 + 0*I || b == 0 + 0*I || b == 1 + 0*I)
      setcvalue(ra, AGN_NAN)
    else
      setcvalue(ra, tools_clog(z)/tools_clog(b));
#else
    lua_Number a[2], b[2], z[2];
    a[0] = complexreal(ra); a[1] = compleximag(ra);
    b[0] = complexreal(rb); b[1] = compleximag(rb);
    if ((a[0] == 0.0 && a[1] == 0.0) ||
        (b[1] == 0.0 && (b[0] == 0.0 || b[0] == 1.0))) {  /* 4.5.4 fix */
      z[0] = AGN_NAN; z[1] = AGN_NAN;
    } else {
      lua_Number _a, _b, ta, tb, td, la, lb;
      _a = sun_pytha(a[0], a[1]);  /* 4.5.2 precision tweak */
      _b = sun_pytha(b[0], b[1]);  /* 4.5.2 precision tweak */
      ta = sun_atan2(a[1], a[0]);
      tb = sun_atan2(b[1], b[0]);
      la = sun_log(_a); lb = sun_log(_b);
      td = sun_pytha(lb, 2.0*tb);  /* 4.5.2 precision tweak */
      z[0] = (la*lb + 4.0*ta*tb)/td;
      z[1] = -2.0*((la*tb - ta*lb)/td);  /* 4.5.2 precision tweak */
    }
    setcvalue(ra, z);
#endif
  }
  else if (!call_binTM(L, ra, rb, ra, TM_LOG)) {  /* 2.34.7 extension */
    luaG_runerror(L, "Error in " LUA_QS ": both operands must be (complex) numbers, got %s, %s.", "log",
      luaT_typenames[(int)ttype(ra)], luaT_typenames[(int)ttype(rb)]);  /* 3.7.8 improvement */

  }
}


/* directly including the lpair.c code into this function is not much faster */
void agenaV_pair (lua_State *L, const TValue *tbl1, const TValue *tbl2, StkId idx) {
  Pair *eq;
  lua_lock(L);
  eq = agnPair_create(L, tbl1, tbl2);
  setpairvalue(L, idx, eq);
  lua_unlock(L);
}


/*
** Finish the table access 'val = t[key]'.
** if 'slot' is NULL, 't' is not a table; otherwise, 'slot' points to
** t[k] entry (which must be empty).
*/
void luaV_finishget (lua_State *L, const TValue *t, TValue *key, StkId val,
                      const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  const TValue *tm;  /* metamethod */
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    if (slot == NULL) {  /* 't' is not a table? */
      lua_assert(!ttistable(t));
      tm = luaT_gettmbyobj(L, t, TM_INDEX);
      if (l_unlikely(notm(tm)))
        luaG_typeerror(L, t, "index");  /* no metamethod */
      /* else will try the metamethod */
    }
    else {  /* 't' is a table */
      lua_assert(isempty(slot));
      tm = fasttm(L, hvalue(t)->metatable, TM_INDEX);  /* table's metamethod */
      if (tm == NULL) {  /* no metamethod? */
        setnilvalue(s2v(val));  /* result is nil */
        return;
      }
      /* else will try the metamethod */
    }
    if (ttisfunction(tm)) {  /* is metamethod a function? */
      luaT_callTMres(L, val, tm, t, key);  /* call it, 3.15.3 fix */
      return;
    }
    t = tm;  /* else try to access 'tm[key]' */
    if (luaV_fastget(L, t, key, slot, luaH_get)) {  /* fast track? */
      setobj2s(L, val, slot);  /* done */
      return;
    }
    /* else repeat (tail call 'luaV_finishget') */
  }
  luaG_runerror(L, "'__index' chain too long; possible loop");
}

/*
** Finish a table assignment 't[key] = val'.
** If 'slot' is NULL, 't' is not a table.  Otherwise, 'slot' points
** to the entry 't[key]', or to a value with an absent key if there
** is no such entry.  (The value at 'slot' must be empty, otherwise
** 'luaV_fastget' would have done the job.)
*/
void luaV_finishset (lua_State *L, const TValue *t, TValue *key,
                     TValue *val, const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;  /* '__newindex' metamethod */
    if (slot != NULL) {  /* is 't' a table? */
      Table *h = hvalue(t);  /* save 't' table */
      lua_assert(isempty(slot));  /* slot must be empty */
      tm = fasttm(L, h->metatable, TM_NEWINDEX);  /* get metamethod */
      if (tm == NULL) {  /* no metamethod? */
        if (isabstkey(slot))  /* no previous entry? */
          slot = luaH_newkey(L, h, key);  /* create one */
        /* no metamethod and (now) there is an entry with given key */
        setobj2t(L, cast(TValue *, slot), val);  /* set its new value */
        invalidateTMcache(h);
        luaC_barriert(L, h, val);
        return;
      }
      /* else will try the metamethod */
    }
    else {  /* not a table; check metamethod */
      tm = luaT_gettmbyobj(L, t, TM_NEWINDEX);
      if (l_unlikely(notm(tm)))
        luaG_typeerror(L, t, "index");
    }
    /* try the metamethod */
    if (ttisfunction(tm)) {
      luaT_callTM(L, tm, t, key, val);
      return;
    }
    t = tm;  /* else repeat assignment over 'tm' */
    if (luaV_fastget(L, t, key, slot, luaH_get)) {
      luaV_finishfastset(L, t, slot, val);
      return;  /* done */
    }
    /* else 'return luaV_finishset(L, t, key, val, slot)' (loop) */
  }
  luaG_runerror(L, "'__writeindex' chain too long; possible loop");
}

int call_binTM (lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event) {
  const TValue *tm = luaT_gettmbyobj(L, p1, event);  /* try first operand */
  if (ttisnil(tm))
    tm = luaT_gettmbyobj(L, p2, event);  /* try second operand */
  if (ttisnil(tm)) return 0;  /* Lua 5.1.2 patch */
  callTMres(L, res, tm, p1, p2);
  return 1;
}


static const TValue *get_compTM (lua_State *L, Table *mt1, Table *mt2,
                                  TMS event) {
  const TValue *tm1 = fasttm(L, mt1, event);
  const TValue *tm2;
  if (tm1 == NULL) return NULL;  /* no metamethod */
  if (mt1 == mt2) return tm1;  /* same metatables => same metamethods */
  tm2 = fasttm(L, mt2, event);
  if (tm2 == NULL) return NULL;  /* no metamethod */
  if (luaO_rawequalObj(tm1, tm2))  /* same metamethods? */
    return tm1;
  return NULL;
}


static int call_orderTM (lua_State *L, const TValue *p1, const TValue *p2,
                         TMS event) {
  const TValue *tm1 = luaT_gettmbyobj(L, p1, event);
  const TValue *tm2;
  if (ttisnil(tm1)) return -1;  /* no metamethod? */
  tm2 = luaT_gettmbyobj(L, p2, event);
  if (!luaO_rawequalObj(tm1, tm2))  /* different metamethods? */
    return -1;
  callTMres(L, L->top, tm1, p1, p2);
  return !l_isfalse(L->top);
}



/*
** Compare two strings 'ts1' x 'ts2', returning an integer less-equal-
** -greater than zero if 'ts1' is less-equal-greater than 'ts2'.
** The code is a little tricky because it allows '\0' in the strings
** and it uses 'strcoll' (to respect locales) for each segment
** of the strings. Note that segments can compare equal but still
** have different lengths. Taken from Lua 5.5.1, lvm.c. 7.9.2
*/
static int l_strcmp (const TString *ts1, const TString *ts2) {
  const char *s1 = getstr(ts1);
  size_t rl1 = ts1->tsv.len;
  const char *s2 = getstr(ts2);
  size_t rl2 = ts2->tsv.len;
  for (;;) {  /* for each segment */
    int temp = strcoll(s1, s2);
    if (temp != 0)  /* not equal? */
      return temp;  /* done */
    else {  /* strings are equal up to a '\0' */
      size_t zl1 = strlen(s1);  /* index of first '\0' in 's1' */
      size_t zl2 = strlen(s2);  /* index of first '\0' in 's2' */
      if (zl2 == rl2)  /* 's2' is finished? */
        return (zl1 == rl1) ? 0 : 1;  /* check 's1' */
      else if (zl1 == rl1)  /* 's1' is finished? */
        return -1;  /* 's1' is less than 's2' ('s2' is not finished) */
      /* both strings longer than 'zl'; go on comparing after the '\0' */
      zl1++; zl2++;
      s1 += zl1; rl1 -= zl1; s2 += zl2; rl2 -= zl2;
    }
  }
}


int luaV_lessthan (lua_State *L, const TValue *l, const TValue *r, int issueerror) {
  int res;
  if (ttype(l) != ttype(r)) {
    return (issueerror) ? luaG_ordererror(L, l, r) : -10;  /* 2.34.6 */
  } else if (ttisnumber(l))
    return luai_numlt(nvalue(l), nvalue(r));
  else if (ttisstring(l))
    return l_strcmp(rawtsvalue(l), rawtsvalue(r)) < 0;
  else if ((res = call_orderTM(L, l, r, TM_LT)) != -1)
    return res;
  return luaG_ordererror(L, l, r);
}


int luaV_lessequal (lua_State *L, const TValue *l, const TValue *r, int issueerror) {
  int res;
  if (ttype(l) != ttype(r))
    return (issueerror) ? luaG_ordererror(L, l, r) : -10;  /* 2.34.6 */
  else if (ttisnumber(l))
    return luai_numle(nvalue(l), nvalue(r));
  else if (ttisstring(l))
    return l_strcmp(rawtsvalue(l), rawtsvalue(r)) <= 0;
  else if ((res = call_orderTM(L, l, r, TM_LE)) != -1)  /* first try `le' */
    return res;
  else if ((res = call_orderTM(L, r, l, TM_LT)) != -1)  /* else try `lt' */
    return !res;
  return luaG_ordererror(L, l, r);
}


int luaV_equalval (lua_State *L, const TValue *t1, const TValue *t2) {
  const TValue *tm = NULL;
  lua_assert(ttype(t1) == ttype(t2));
  switch (ttype(t1)) {
    case LUA_TNIL: return 1;
    case LUA_TNUMBER: return luai_numeq(nvalue(t1), nvalue(t2));
#ifndef PROPCMPLX
    case LUA_TCOMPLEX: return agnc_eq(cvalue(t1), cvalue(t2));
#else
    case LUA_TCOMPLEX: return agnc_eq(complexreal(t1), compleximag(t1), complexreal(t2), compleximag(t2));
#endif
    case LUA_TBOOLEAN: return bvalue(t1) == bvalue(t2);  /* true must be 1 !! */
    case LUA_TLIGHTUSERDATA: return pvalue(t1) == pvalue(t2);
    case LUA_TUSERDATA: {
      if (unlikely(uvalue(t1) == uvalue(t2))) return 1;
      tm = get_compTM(L, uvalue(t1)->metatable, uvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    case LUA_TTABLE: {
      if (unlikely(hvalue(t1) == hvalue(t2))) return 1;  /* references are the same ? -> return `true' */
      /* compare contents of two tables for equality, added 0.6.0 */
      if (agenaV_comptables(L, hvalue(t1), hvalue(t2), 1)) return 1;
      tm = get_compTM(L, hvalue(t1)->metatable, hvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    case LUA_TSET: {
      if (unlikely(usvalue(t1) == usvalue(t2))) return 1;  /* references are the same ? -> return `true' */
      /* compare contents of two sets for equality, added 0.6.0 */
      if (agenaV_compusets(L, usvalue(t1), usvalue(t2), 1)) return 1;
      tm = get_compTM(L, usvalue(t1)->metatable, usvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    case LUA_TSEQ: {
      if (unlikely(seqvalue(t1) == seqvalue(t2))) return 1;  /* references are the same ? -> return `true' */
      if (agenaV_compseqs(L, seqvalue(t1), seqvalue(t2), 1)) return 1;
      tm = get_compTM(L, seqvalue(t1)->metatable, seqvalue(t2)->metatable, TM_EQ);
      break;
    }
    case LUA_TPAIR: {
      if (unlikely(pairvalue(t1) == pairvalue(t2))) return 1;  /* references are the same ? -> return `true' */
      if (agenaV_comppairs(L, pairvalue(t1), pairvalue(t2))) return 1;
      tm = get_compTM(L, pairvalue(t1)->metatable, pairvalue(t2)->metatable, TM_EQ);
      break;
    }
    case LUA_TREG: {
      if (unlikely(regvalue(t1) == regvalue(t2))) return 1;  /* references are the same ? -> return `true' */
      if (agenaV_compregs(L, regvalue(t1), regvalue(t2), 1)) return 1;
      tm = get_compTM(L, regvalue(t1)->metatable, regvalue(t2)->metatable, TM_EQ);
      break;
    }
    case LUA_TFUNCTION: {  /* 2.36.2 */
      Closure *fcl1, *fcl2;
      Table *mt1, *mt2;
      if (unlikely(clvalue(t1) == clvalue(t2))) return 1;
      fcl1 = clvalue(t1); fcl2 = clvalue(t2);
      mt1 = (fcl1->c.isC) ? fcl1->c.metatable : fcl1->l.metatable;
      mt2 = (fcl2->c.isC) ? fcl2->c.metatable : fcl2->l.metatable;
      tm = get_compTM(L, mt1, mt2, TM_EQ);
      break;  /* will try TM */
    }
    case LUA_TFAIL: return 1;
    default: {
      return gcvalue(t1) == gcvalue(t2);
    }
  }
  if (tm == NULL) return 0;  /* no TM ? */
  callTMres(L, L->top, tm, t1, t2);  /* call TM */
  return !l_isfalseorfail(L->top);
}


int luaV_equalref (lua_State *L, const TValue *t1, const TValue *t2) {  /* 2.12.3 */
  const TValue *tm = NULL;
  lua_assert(ttype(t1) == ttype(t2));
  switch (ttype(t1)) {
    case LUA_TNIL: return 1;
    case LUA_TNUMBER: return luai_numeq(nvalue(t1), nvalue(t2));
#ifndef PROPCMPLX
    case LUA_TCOMPLEX: return agnc_eq(cvalue(t1), cvalue(t2));
#else
    case LUA_TCOMPLEX: return agnc_eq(complexreal(t1), compleximag(t1), complexreal(t2), compleximag(t2));
#endif
    case LUA_TBOOLEAN: return bvalue(t1) == bvalue(t2);  /* true must be 1 !! */
    case LUA_TLIGHTUSERDATA: return pvalue(t1) == pvalue(t2);
    case LUA_TUSERDATA: {
      if (uvalue(t1) == uvalue(t2)) return 1;
      tm = get_compTM(L, uvalue(t1)->metatable, uvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    case LUA_TTABLE: {
      if (hvalue(t1) == hvalue(t2)) return 1;  /* references are the same ? -> return `true' */
      /* compare contents of two tables for equality, added 0.6.0 */
      tm = get_compTM(L, hvalue(t1)->metatable, hvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    case LUA_TSET: {
      if (usvalue(t1) == usvalue(t2)) return 1;  /* references are the same ? -> return `true' */
      /* compare contents of two sets for equality, added 0.6.0 */
      tm = get_compTM(L, usvalue(t1)->metatable, usvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    case LUA_TSEQ: {
      if (seqvalue(t1) == seqvalue(t2)) return 1;  /* references are the same ? -> return `true' */
      tm = get_compTM(L, seqvalue(t1)->metatable, seqvalue(t2)->metatable, TM_EQ);
      break;
    }
    case LUA_TPAIR: {
      if (pairvalue(t1) == pairvalue(t2)) return 1;  /* references are the same ? -> return `true' */
      tm = get_compTM(L, pairvalue(t1)->metatable, pairvalue(t2)->metatable, TM_EQ);
      break;
    }
    case LUA_TREG: {
      if (regvalue(t1) == regvalue(t2)) return 1;  /* references are the same ? -> return `true' */
      tm = get_compTM(L, regvalue(t1)->metatable, regvalue(t2)->metatable, TM_EQ);
      break;
    }
    case LUA_TFUNCTION: {  /* 2.36.2 */
      Closure *fcl1, *fcl2;
      Table *mt1, *mt2;
      if (clvalue(t1) == clvalue(t2)) return 1;
      fcl1 = clvalue(t1); fcl2 = clvalue(t2);
      mt1 = (fcl1->c.isC) ? fcl1->c.metatable : fcl1->l.metatable;
      mt2 = (fcl2->c.isC) ? fcl2->c.metatable : fcl2->l.metatable;
      tm = get_compTM(L, mt1, mt2, TM_EQ);
      break;  /* will try TM */
    }
    case LUA_TFAIL: return 1;
    default: return gcvalue(t1) == gcvalue(t2);
  }
  if (tm == NULL) return 0;  /* no TM ? */
  callTMres(L, L->top, tm, t1, t2);  /* call TM */
  return !l_isfalseorfail(L->top);
}


/* 2.1.4, t1 and t2 should be of the same type, otherwise returns 0. */
int luaV_approxequalvalonebyone (lua_State *L, const TValue *t1, const TValue *t2) {
  const TValue *tm = NULL;
  if (ttype(t1) != ttype(t2)) return 0;  /* 4.12.0 change */
  switch (ttype(t1)) {
    case LUA_TNIL: return 1;
    case LUA_TNUMBER: return tools_approx(nvalue(t1), nvalue(t2), L->Eps);
#ifndef PROPCMPLX
    case LUA_TCOMPLEX:
      return tools_approx(creal(cvalue(t1)), creal(cvalue(t2)), L->Eps) && tools_approx(cimag(cvalue(t1)), cimag(cvalue(t2)), L->Eps);
#else
    case LUA_TCOMPLEX:
      return tools_approx(complexreal(t1), complexreal(t2), L->Eps) && tools_approx(compleximag(t1), compleximag(t2), L->Eps);
#endif
    case LUA_TTABLE: {
      if (hvalue(t1) == hvalue(t2)) return 1;  /* references are the same ? -> return `true' */
      /* compare contents of two tables for approximate equality */
      if (agenaV_approxcomptablesonebyone(L, hvalue(t1), hvalue(t2))) return 1;
      tm = get_compTM(L, hvalue(t1)->metatable, hvalue(t2)->metatable, TM_AEQ);
      break;  /* will try TM */
    }
    case LUA_TSET: {
      if (usvalue(t1) == usvalue(t2)) return 1;  /* references are the same ? -> return `true' */
      /* compare contents of two sets for approximate equality */
      if (agenaV_approxcompusets(L, usvalue(t1), usvalue(t2), 1)) return 1;
      tm = get_compTM(L, usvalue(t1)->metatable, usvalue(t2)->metatable, TM_AEQ);
      break;  /* will try TM */
    }
    case LUA_TSEQ: {
      if (seqvalue(t1) == seqvalue(t2)) return 1;  /* references are the same ? -> return `true' */
      if (agenaV_approxcompseqsonebyone(L, seqvalue(t1), seqvalue(t2))) return 1;
      tm = get_compTM(L, seqvalue(t1)->metatable, seqvalue(t2)->metatable, TM_AEQ);
      break;  /* will try TM */
    }
    case LUA_TPAIR: {
      if (pairvalue(t1) == pairvalue(t2)) return 1;  /* references are the same ? -> return `true' */
      if (agenaV_approxcomppairs(L, pairvalue(t1), pairvalue(t2))) return 1;
      tm = get_compTM(L, pairvalue(t1)->metatable, pairvalue(t2)->metatable, TM_AEQ);
      break;  /* will try TM */
    }
    case LUA_TBOOLEAN: return bvalue(t1) == bvalue(t2);  /* true must be 1 !! */
    case LUA_TLIGHTUSERDATA: return pvalue(t1) == pvalue(t2);
    case LUA_TUSERDATA: {
      if (uvalue(t1) == uvalue(t2)) return 1;
      tm = get_compTM(L, uvalue(t1)->metatable, uvalue(t2)->metatable, TM_AEQ);
      break;  /* will try TM */
    }
    case LUA_TREG: {
      if (regvalue(t1) == regvalue(t2)) return 1;  /* references are the same ? -> return `true' */
      if (agenaV_approxcompregsonebyone(L, regvalue(t1), regvalue(t2))) return 1;
      tm = get_compTM(L, regvalue(t1)->metatable, regvalue(t2)->metatable, TM_AEQ);
      break;  /* will try TM */
    }
    case LUA_TFUNCTION: {  /* 2.36.2 */
      Closure *fcl1, *fcl2;
      Table *mt1, *mt2;
      if (clvalue(t1) == clvalue(t2)) return 1;
      fcl1 = clvalue(t1); fcl2 = clvalue(t2);
      mt1 = (fcl1->c.isC) ? fcl1->c.metatable : fcl1->l.metatable;
      mt2 = (fcl2->c.isC) ? fcl2->c.metatable : fcl2->l.metatable;
      tm = get_compTM(L, mt1, mt2, TM_AEQ);
      break;  /* will try TM */
    }
    case LUA_TFAIL: return 1;
    default: return gcvalue(t1) == gcvalue(t2);
  }
  if (tm == NULL) return 0;  /* no TM ? */
  callTMres(L, L->top, tm, t1, t2);  /* call TM, changed 2.5.2, allow TM only for ~= operator, but not for ~<> */
  return !l_isfalseorfail(L->top);
}


int luaV_equalvalonebyone (lua_State *L, const TValue *t1, const TValue *t2) {
  const TValue *tm = NULL;
  lua_assert(ttype(t1) == ttype(t2));
  switch (ttype(t1)) {
    case LUA_TTABLE: {
      if (hvalue(t1) == hvalue(t2)) return 1;
      /* compare contents of two tables for equality, added 0.6.0 */
      if (agenaV_comptablesonebyone(L, hvalue(t1), hvalue(t2))) return 1;
      tm = get_compTM(L, hvalue(t1)->metatable, hvalue(t2)->metatable, TM_EEQ);
      break;  /* will try TM */
    }
    case LUA_TSEQ: {
      if (seqvalue(t1) == seqvalue(t2)) return 1;
      if (agenaV_compseqsonebyone(L, seqvalue(t1), seqvalue(t2))) return 1;
      tm = get_compTM(L, seqvalue(t1)->metatable, seqvalue(t2)->metatable, TM_EEQ);
      break;
    }
    case LUA_TREG: {
      if (regvalue(t1) == regvalue(t2)) return 1;
      if (agenaV_compregsonebyone(L, regvalue(t1), regvalue(t2))) return 1;
      tm = get_compTM(L, regvalue(t1)->metatable, regvalue(t2)->metatable, TM_EEQ);
      break;
    }
    case LUA_TUSERDATA: {  /* 2.3.0 RC 3 */
      if (uvalue(t1) == uvalue(t2)) return 1;
      tm = get_compTM(L, uvalue(t1)->metatable, uvalue(t2)->metatable, TM_EEQ);
      break;  /* will try TM */
    }
    case LUA_TFUNCTION: {  /* 2.36.2 */
      Closure *fcl1, *fcl2;
      Table *mt1, *mt2;
      if (clvalue(t1) == clvalue(t2)) return 1;
      fcl1 = clvalue(t1); fcl2 = clvalue(t2);
      mt1 = (fcl1->c.isC) ? fcl1->c.metatable : fcl1->l.metatable;
      mt2 = (fcl2->c.isC) ? fcl2->c.metatable : fcl2->l.metatable;
      tm = get_compTM(L, mt1, mt2, TM_EEQ);
      break;  /* will try TM */
    }
    default: return luaV_equalval(L, t1, t2);
  }
  if (tm == NULL) return 0;  /* no TM ? */
  callTMres(L, L->top, tm, t1, t2);  /* call TM */
  return !l_isfalseorfail(L->top);
}


int luaV_equalncomplex (lua_State *L, const TValue *t1, const TValue *t2) {
  if (ttisnumber(t1) && ttiscomplex(t2)) {
#ifndef PROPCMPLX
    return agnc_eq(nvalue(t1), cvalue(t2));
#else
    return agnc_eq(nvalue(t1), 0, complexreal(t2), compleximag(t2));
#endif
  }
  else if (ttiscomplex(t1) && ttisnumber(t2)) {
#ifndef PROPCMPLX
    return agnc_eq(cvalue(t1), nvalue(t2));
#else
    return agnc_eq(complexreal(t1), compleximag(t1), nvalue(t2), 0);
#endif
  }
  return 0;
}


/* taken from Lua 5.2.4, modified */

#define ALLONES    (~(((~(LUA_UINT32)0) << (LUA_NBITS - 1)) << 1UL))

/* macro to trim extra bits */
#define trim(x)    ((x) & ALLONES)

/* builds a number with 'n' ones (1 <= n <= LUA_NBITS) */
#define mask(n)    (~((ALLONES << 1) << ((n) - 1)))

/* Returns the number x (left operand) rotated disp (right operand) bits to the left (<<<< operator)
   or the right (>>>> operator). disp may be any representable integer. For any valid displacement,
   the following identities hold:
     x <<< disp = x <<< disp % 32, x >>> disp = x >>> disp % 32. */

/* changed 2.9.0, extended to 64-bit integers 2.10.0, fixed 2.14.5, changed back to 32-bit operation 2.15.0 */
LUA_UINT32 luaV_brotateunsigned (LUA_UINT32 r, int i) {
  i &= (LUA_NBITS - 1);  /* i = i % NBITS, prevent undefined behaviour */
  r = trim(r);
  if (i != 0)  /* avoid undefined shift of LUA_NBITS when i == 0 */
    r = (r << i) | (r >> (LUA_NBITS - i));
  return trim(r);
}


LUA_INT32 luaV_brotatesigned (LUA_INT32 r, int i) {  /* Changed 2.9.0, extended to 64-bit integers 2.10.0, fixed 2.14.5 */
  i &= (LUA_NBITS - 1);  /* i = i % NBITS, prevent undefined behaviour */
  r = trim(r);
  if (i != 0)  /* avoid undefined shift of LUA_NBITS when i == 0 */
    r = (r << i) | (r >> (LUA_NBITS - i));
  return trim(r);
}

/* end of Lua 5.2.4 code */


void luaV_concat (lua_State *L, int total, int last) {
  do {
    StkId top = L->base + last + 1;
    int i, n = 2;  /* number of elements handled in this pass (at least 2) */
    for (i=1; i <= 2; i++) {  /* substitute `null` with empty string, 5.5.15 */
      if (ttisnil(top - i)) {
        setsvalue2s(L, top - i, luaS_newlstr(L, "", 0));
      }
    }
    if (!(ttisstring(top - 2) || ttisnumber(top - 2) || ttiscomplex(top - 2) ||
          ttisboolean(top - 2) || ttisnil(top - 2)) || !tostring(L, top - 1)) {  /* Lua 5.1.2 patch, 5.35.5 extension for Booleans */
      if (!call_binTM(L, top - 2, top - 1, top - 2, TM_CONCAT)) {  /* any non-atomic data ? */
        luaG_concaterror(L, top - 2, top - 1);
      }
    } else if (tsvalue(top - 1)->len == 0) { /* second op is empty ? Lua 5.1.2 patch */
      (void)tostring(L, top - 2);  /* result is first op (as string) */
    } else {
      /* at least two string values; get as many as possible */
      char *buffer;
      size_t tl = tsvalue(top - 1)->len;  /* collect total length */
      for (n=1; n < total && tostring(L, top - n - 1); n++) {
        size_t l = tsvalue(top - n - 1)->len;
        if (l >= MAX_SIZET - tl) luaG_runerror(L, "Error in " LUA_QS ": string length overflow.", "&");
        tl += l;
      }
      buffer = luaZ_openspace(L, &G(L)->buff, tl);
      tl = 0;
      for (i=n; i > 0; i--) {  /* concat all strings */
        size_t l = tsvalue(top - i)->len;
        tools_memcpy(buffer + tl, svalue(top - i), l);  /* 2.25.1 tweak */
        tl += l;
      }
      setsvalue2s(L, top - n, luaS_newlstr(L, buffer, tl));
    }
    total -= n - 1;  /* got `n' strings to create 1 new */
    last -= n - 1;
  } while (total > 1);  /* repeat until only 1 result left */
}


/*
** Main operation 'ra = size rb'. Idea taken from Lua 5.4.0 RC 5, 2.21.2.
*/
void luaV_objlen (lua_State *L, StkId ra, const TValue *rb) {
  const TValue *tm;
  switch (ttype(rb)) {
    case LUA_TTABLE: {
      Table *t;
      t = hvalue(rb);
      if ((tm = fasttm(L, t->metatable, TM_SIZE)) == NULL) { /* no metamethod ? */
        setnvalue(ra, cast_num(agenaV_nops(L, t)));
      } else if (ttisfunction(tm))
        call_binTM(L, rb, luaO_nilobject, ra, TM_SIZE);  /* 3.5.3 change */
      break;
    }
    case LUA_TSTRING: {
      setnvalue(ra, cast_num(tsvalue(rb)->len));
      break;
    }
    case LUA_TSET: {
      UltraSet *s;
      s = usvalue(rb);
      if ((tm = fasttm(L, s->metatable, TM_SIZE)) == NULL) { /* no metamethod ? */
        setnvalue(ra, cast_num(s->size));
      } else if (ttisfunction(tm))
        call_binTM(L, rb, luaO_nilobject, ra, TM_SIZE);  /* 3.5.3 change */
      break;
    }
    case LUA_TSEQ: {
      Seq *s;
      s = seqvalue(rb);
      if ((tm = fasttm(L, s->metatable, TM_SIZE)) == NULL) { /* no metamethod ? */
        setnvalue(ra, cast_num(seqvalue(rb)->size));
      } else if (ttisfunction(tm))
        call_binTM(L, rb, luaO_nilobject, ra, TM_SIZE);  /* 3.5.3 change */
      break;
    }
    case LUA_TREG: {
      Reg *s;
      s = regvalue(rb);
      if ((tm = fasttm(L, s->metatable, TM_SIZE)) == NULL) {  /* no metamethod ? */
        /* Protect(setnvalue(ra, cast_num(regvalue(rb)->maxsize))); */
        setnvalue(ra, cast_num(regvalue(rb)->top));  /* 2.4.0 */
      } else if (ttisfunction(tm))
        call_binTM(L, rb, luaO_nilobject, ra, TM_SIZE);  /* 3.5.3 change */
      break;
    }
    case LUA_TPAIR: {
      Pair *p;
      p = pairvalue(rb);
      if ((tm = fasttm(L, p->metatable, TM_SIZE)) == NULL) {  /* no metamethod ? */
        setnvalue(ra, 2);
      } else if (ttisfunction(tm))
        call_binTM(L, rb, luaO_nilobject, ra, TM_SIZE);  /* 3.5.3 change */
      break;
    }
    case LUA_TUSERDATA: case LUA_TFUNCTION: {  /* reinstate Lua 5.1 behaviour, 2.3.0 RC 3; 2.36.2 extension for functions */
      /* try metamethod */
      if (!call_binTM(L, rb, luaO_nilobject, ra, TM_SIZE))
        luaG_typeerror(L, rb, "get size of");
      break;
    }
    case LUA_TNIL: {  /* 4.12.3 */
      setnvalue(ra, cast_num(0.0));
      break;
    }
    default: {  /* 2.3.0 extension */
      luaG_runerror(L, "Error in " LUA_QS ": wrong type of argument, got %s.", "size",
        luaT_typenames[(int)ttype(rb)]);
    }
  }
}


static void Arith (lua_State *L, StkId ra, const TValue *rb,
                   const TValue *rc, TMS op) {  /* changed 0.5.3 */
  if (ttisnumber(rb) && ttisnumber(rc)) {
    lua_Number nb = nvalue(rb), nc = nvalue(rc);
    switch (op) {
      case TM_ADD: setnvalue(ra, luai_numadd(nb, nc)); break;
      case TM_SUB: setnvalue(ra, luai_numsub(nb, nc)); break;
      case TM_MUL: setnvalue(ra, luai_nummul(nb, nc)); break;
      case TM_DIV: setnvalue(ra, luai_numdiv(nb, nc)); break;
      case TM_MOD: setnvalue(ra, luai_nummod(nb, nc)); break;
      case TM_POW: setnvalue(ra, luai_numpow(nb, nc)); break;
      case TM_UNM: setnvalue(ra, luai_numunm(nb)); break;
      case TM_INTDIV: setnvalue(ra, luai_numintdiv(nb, nc)); break;  /* added 0.5.4, 2.17.1 optimisation */
      case TM_ABSDIFF: setnvalue(ra, luai_numabsdiff(nb, nc)); break;  /* added 2.9.8 */
      default:  /* 0.26.1 patch */
        if (!call_binTM(L, rb, rc, ra, op)) {  /* added braces 2.10.3 */
          luaG_aritherror(L, rb, rc); break;
        }
    }
  }
#ifndef PROPCMPLX
  else if (ttiscomplex(rb) && ttiscomplex(rc)) {
    agn_Complex nb, nc;
    nb = cvalue(rb); nc = cvalue(rc);
    switch (op) {
      case TM_ADD: setcvalue(ra, agnc_add(nb, nc)); break;
      case TM_SUB: setcvalue(ra, agnc_sub(nb, nc)); break;
      case TM_MUL: setcvalue(ra, agnc_mul(nb, nc)); break;
      case TM_DIV: setcvalue(ra, agnc_div(nb, nc)); break;
      case TM_POW: setcvalue(ra, agnc_pow(nb, nc)); break;
      case TM_UNM: setcvalue(ra, agnc_unm(nb)); break;
      case TM_ABSDIFF: setnvalue(ra, agnc_absdiff(nb, nc)); break;  /* 2.10.2 */
      default:  /* 0.26.1 patch */
        if (!call_binTM(L, rb, rc, ra, op)) {  /* added braces 2.10.3 */
          luaG_aritherror(L, rb, rc); break;
        }
    }
  }
  else if (ttiscomplex(rb) && ttisnumber(rc)) {
    agn_Complex nb, nc;
    nb = cvalue(rb); nc = nvalue(rc);
    switch (op) {
      case TM_ADD: setcvalue(ra, agnc_add(nb, nc)); break;
      case TM_SUB: setcvalue(ra, agnc_sub(nb, nc)); break;
      case TM_MUL: setcvalue(ra, agnc_mul(nb, nc)); break;
      case TM_DIV: setcvalue(ra, agnc_div(nb, nc)); break;
      case TM_POW: setcvalue(ra, agnc_pow(nb, nc)); break;
      case TM_UNM: setcvalue(ra, agnc_unm(nb)); break;
      case TM_ABSDIFF: setnvalue(ra, agnc_absdiff(nb, nc)); break;  /* 2.10.2 */
      default:  /* 0.26.1 patch */
        if (!call_binTM(L, rb, rc, ra, op)) {  /* added braces 2.10.3 */
          luaG_aritherror(L, rb, rc); break;
        }
    }
  }
  else if (ttisnumber(rb) && ttiscomplex(rc)) {
    agn_Complex nb, nc;
    nb = nvalue(rb); nc = cvalue(rc);
    switch (op) {
      case TM_ADD: setcvalue(ra, agnc_add(nb, nc)); break;
      case TM_SUB: setcvalue(ra, agnc_sub(nb, nc)); break;
      case TM_MUL: setcvalue(ra, agnc_mul(nb, nc)); break;
      case TM_DIV: setcvalue(ra, agnc_div(nb, nc)); break;
      case TM_POW: setcvalue(ra, agnc_pow(nb, nc)); break;
      case TM_UNM: setcvalue(ra, agnc_unm(nb)); break;
      case TM_ABSDIFF: setnvalue(ra, agnc_absdiff(nb, nc)); break;  /* 2.10.2 */
      default:  /* 0.26.1 patch */
        if (!call_binTM(L, rb, rc, ra, op)) {  /* added braces 2.10.3 */
          luaG_aritherror(L, rb, rc); break;
        }
    }
  }
#else
  else if (ttiscomplex(rb) && ttiscomplex(rc)) {
    lua_Number nb1, nb2, nc1, nc2, z[2];
    nb1 = complexreal(rb); nb2 = compleximag(rb);
    nc1 = complexreal(rc); nc2 = compleximag(rc);
    switch (op) {
      case TM_ADD: agnc_add(z, nb1, nb2, nc1, nc2); setcvalue(ra, z); break;
      case TM_SUB: agnc_sub(z, nb1, nb2, nc1, nc2); setcvalue(ra, z); break;
      case TM_MUL: agnc_mul(z, nb1, nb2, nc1, nc2); setcvalue(ra, z); break;
      case TM_DIV: agnc_div(z, nb1, nb2, nc1, nc2); setcvalue(ra, z); break;
      case TM_POW: agnc_pow(z, nb1, nb2, nc1, nc2); setcvalue(ra, z); break;
      case TM_UNM: agnc_unm(z, nb1, nb2); setcvalue(ra, z); break;
      case TM_ABSDIFF: setnvalue(ra, agnc_absdiff(nb1, nb2, nc1, nc2)); break;  /* 2.10.2 */
      default:
        if (!call_binTM(L, rb, rc, ra, op)) {  /* 2.11.0 RC2 fix */
          luaG_aritherror(L, rb, rc); break;
        }
    }
  }
  else if (ttiscomplex(rb) && ttisnumber(rc)) {
    lua_Number nb1, nb2, nc, z[2], naught;
    nb1 = complexreal(rb); nb2 = compleximag(rb);
    nc = nvalue(rc);
    naught = 0.0;  /* prevent warning: division by zero [-Wdiv-by-zero] warnings in DJGPP with agnc_div */
    switch (op) {
      case TM_ADD: agnc_add(z, nb1, nb2, nc, 0); setcvalue(ra, z); break;
      case TM_SUB: agnc_sub(z, nb1, nb2, nc, 0); setcvalue(ra, z); break;
      case TM_MUL: agnc_mul(z, nb1, nb2, nc, 0); setcvalue(ra, z); break;
      case TM_DIV: agnc_div(z, nb1, nb2, nc, naught); setcvalue(ra, z); break;
      case TM_POW: agnc_pow(z, nb1, nb2, nc, 0); setcvalue(ra, z); break;
      case TM_UNM: agnc_unm(z, nb1, nb2); setcvalue(ra, z); break;
      case TM_ABSDIFF: setnvalue(ra, agnc_absdiff(nb1, nb2, nc, 0)); break;  /* 2.10.2 */
      default:
        if (!call_binTM(L, rb, rc, ra, op)) {  /* 2.11.0 RC2 fix */
          luaG_aritherror(L, rb, rc); break;
        }
    }
  }
  else if (ttisnumber(rb) && ttiscomplex(rc)) {
    lua_Number nb, nc1, nc2, z[2];
    nb = nvalue(rb); nc1 = complexreal(rc); nc2 = compleximag(rc);
    switch (op) {
      case TM_ADD: agnc_add(z, nb, 0, nc1, nc2); setcvalue(ra, z); break;
      case TM_SUB: agnc_sub(z, nb, 0, nc1, nc2); setcvalue(ra, z); break;
      case TM_MUL: agnc_mul(z, nb, 0, nc1, nc2); setcvalue(ra, z); break;
      case TM_DIV: agnc_div(z, nb, 0, nc1, nc2); setcvalue(ra, z); break;
      case TM_POW: agnc_pow(z, nb, 0, nc1, nc2); setcvalue(ra, z); break;
      case TM_UNM: agnc_unm(z, nb, 0); setcvalue(ra, z); break;
      case TM_ABSDIFF: setnvalue(ra, agnc_absdiff(nb, 0, nc1, nc2)); break;  /* 2.10.2 */
      default:
        if (!call_binTM(L, rb, rc, ra, op)) {  /* 2.11.0 RC2 fix */
          luaG_aritherror(L, rb, rc); break;
        }
    }
  }
#endif
  else if (!call_binTM(L, rb, rc, ra, op))
    luaG_aritherror(L, rb, rc);
  luaC_checkGC(L);  /* 2.34.3 */
}


const char *aux_gettypename (lua_State *L, TValue *rb, int userdefinedtype) {  /* 2.10.0 */
  const TValue *tm;
  if (!userdefinedtype) {  /* 2.20.1 extension */
    return luaT_typenames[rb->tt];
  }
  switch (rb->tt) {
    case LUA_TFUNCTION: {
      Closure *c = clvalue(rb);
      if (c->l.type != NULL)
        return getstr(c->l.type);
      else if (c->c.type != NULL)
        return getstr(c->c.type);
      else
        return luaT_typenames[rb->tt];
    }
    case LUA_TSEQ: {
      Seq *s = seqvalue(rb);
      if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
        if (s->type == NULL)
          return luaT_typenames[rb->tt];
        else
          return getstr(s->type);
      }
      break;
    }
    case LUA_TPAIR: {
      Pair *s = pairvalue(rb);
      if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
        if (s->type == NULL)
          return luaT_typenames[rb->tt];
        else
          return getstr(s->type);
      }
      break;
    }
    case LUA_TTABLE: {
      Table *s = hvalue(rb);
      if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
        if (s->type == NULL)
          return luaT_typenames[rb->tt];
        else
          return getstr(s->type);
      }
      break;
    }
    case LUA_TSET: {
      UltraSet *s = usvalue(rb);
      if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
        if (s->type == NULL)
          return luaT_typenames[rb->tt];
        else
          return getstr(s->type);
      }
      break;
    }
    case LUA_TREG: {
      Reg *s = regvalue(rb);
      if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
        if (s->type == NULL)
          return luaT_typenames[rb->tt];
        else
          return getstr(s->type);
      }
      break;
    }
    case LUA_TUSERDATA: {
      Udata *u = rawuvalue(rb);
      if ((tm = fasttm(L, u->uv.metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
        if (u->uv.type == NULL)
          return luaT_typenames[rb->tt];
        else
          return getstr(u->uv.type);
      }
      break;
    }
    default:
      return luaT_typenames[rb->tt];
  }
  return NULL;
}

/*
** some macros for common tasks in `luaV_execute'
*/

#define runtime_check(L, c)   { if (!(c)) break; }

/* to be used after possible stack reallocation */
#define RB(i)   check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base+GETARG_B(i))
#define RC(i)   check_exp(getCMode(GET_OPCODE(i)) == OpArgR, base+GETARG_C(i))
#define RKB(i)  check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
   ISK(GETARG_B(i)) ? k+INDEXK(GETARG_B(i)) : base+GETARG_B(i))
#define RKC(i)  check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
   ISK(GETARG_C(i)) ? k+INDEXK(GETARG_C(i)) : base+GETARG_C(i))
#define KBx(i)  check_exp(getBMode(GET_OPCODE(i)) == OpArgK, k+GETARG_Bx(i))

#ifndef PROPCMPLX
#define arith_op(L,op,opc,tm) { \
  TValue *rb = RKB(i); \
  TValue *rc = RKC(i); \
  if (ttisnumber(rb) && ttisnumber(rc)) { \
    lua_Number nb = nvalue(rb), nc = nvalue(rc); \
    setnvalue(ra, op(nb, nc)); \
  } \
  else if (ttiscomplex(rb) && ttiscomplex(rc)) { \
    agn_Complex nb = cvalue(rb), nc = cvalue(rc); \
    setcvalue(ra, opc(nb, nc)); \
  } \
  else if (ttiscomplex(rb) && ttisnumber(rc)) { \
    agn_Complex nb = cvalue(rb), nc = nvalue(rc); \
    setcvalue(ra, opc(nb, nc)); \
  } \
  else if (ttisnumber(rb) && ttiscomplex(rc)) { \
    agn_Complex nb = nvalue(rb), nc = cvalue(rc); \
    setcvalue(ra, opc(nb, nc)); \
  } \
  else if (ttisnumber(rb) && ttisboolean(rc)) { \
    lua_Number nb = nvalue(rb), nc = bvalue(rc); \
    if (nc == 2) nc = 0; \
    setnvalue(ra, op(nb, nc)); \
  } \
  else if (ttisboolean(rb) && ttisnumber(rc)) { \
    lua_Number nb = bvalue(rb), nc = nvalue(rc); \
    if (nb == 2.0) nb = 0.0; \
    setnvalue(ra, op(nb, nc)); \
  } \
  else if (ttisboolean(rb) && ttisboolean(rc)) { \
    lua_Number nb = bvalue(rb), nc = bvalue(rc); \
    if (nb == 2.0) nb = 0.0; \
    if (nc == 2.0) nc = 0.0; \
    setnvalue(ra, op(nb, nc)); \
  } \
  else if (agn_getnulliszero(L)) { \
    if (ttisnumber(rb) && ttisnil(rc)) { \
      lua_Number nb = nvalue(rb), nc = 0.0; \
      setnvalue(ra, op(nb, nc)); \
    } \
    else if (ttisnil(rb) && ttisnumber(rc)) { \
      lua_Number nb = 0.0, nc = nvalue(rc); \
      setnvalue(ra, op(nb, nc)); \
    } \
    else if (ttisnil(rb) && ttisnil(rc)) { \
      lua_Number nb = 0.0, nc = 0.0; \
      setnvalue(ra, op(nb, nc)); \
    } \
  } \
  else \
    Protect(Arith(L, ra, rb, rc, tm)); \
}

/*  */

#else
#define arith_op(L,op,opc,tm) { \
  TValue *rb = RKB(i); \
  TValue *rc = RKC(i); \
  if (ttisnumber(rb) && ttisnumber(rc)) { \
    lua_Number nb = nvalue(rb), nc = nvalue(rc); \
    setnvalue(ra, op(nb, nc)); \
  } \
  else if (ttiscomplex(rb) && ttiscomplex(rc)) { \
    lua_Number z[2]; \
    lua_Number nb1 = complexreal(rb), nb2 = compleximag(rb), \
               nc1 = complexreal(rc), nc2 = compleximag(rc); \
    opc(z, nb1, nb2, nc1, nc2); \
    setcvalue(ra, z); \
  } \
  else if (ttiscomplex(rb) && ttisnumber(rc)) { \
    lua_Number z[2]; \
    lua_Number naught = 0.0; \
    lua_Number nb1 = complexreal(rb), nb2 = compleximag(rb), nc = nvalue(rc); \
    opc(z, nb1, nb2, nc, naught); \
    setcvalue(ra, z); \
  } \
  else if (ttisnumber(rb) && ttiscomplex(rc)) { \
    lua_Number z[2]; \
    lua_Number nb = nvalue(rb), nc1 = complexreal(rc), nc2 = compleximag(rc); \
    opc(z, nb, 0, nc1, nc2); \
    setcvalue(ra, z); \
  } \
  else if (ttisnumber(rb) && ttisboolean(rc)) { \
    lua_Number nb, nc; \
    nb = nvalue(rb); nc = bvalue(rc); \
    if (nc == 2) nc = 0; \
    setnvalue(ra, op(nb, nc)); \
  } \
  else if (ttisboolean(rb) && ttisnumber(rc)) { \
    lua_Number nb, nc; \
    nb = bvalue(rb); nc = nvalue(rc); \
    if (nb == 2) nb = 0; \
    setnvalue(ra, op(nb, nc)); \
  } \
  else if (ttisboolean(rb) && ttisboolean(rc)) { \
    lua_Number nb, nc; \
    nb = bvalue(rb); nc = bvalue(rc); \
    if (nb == 2) nb = 0; \
    if (nc == 2) nc = 0; \
    setnvalue(ra, op(nb, nc)); \
  } else if (agn_getnulliszero(L)) { \
    if (ttisnumber(rb) && ttisnil(rc)) { \
      lua_Number nb, nc; \
      nb = nvalue(rb); nc = 0.0; \
      setnvalue(ra, op(nb, nc)); \
    } \
    else if (ttisnil(rb) && ttisnumber(rc)) { \
      lua_Number nb, nc; \
      nb = 0.0; nc = nvalue(rc); \
      setnvalue(ra, op(nb, nc)); \
    } \
    else if (ttisnil(rb) && ttisnil(rc)) { \
      lua_Number nb, nc; \
      nb = 0.0; nc = 0.0; \
      setnvalue(ra, op(nb, nc)); \
    } \
  } \
  else \
    Protect(Arith(L, ra, rb, rc, tm)); \
}
#endif



#define arith_opNumber(op,tm) { \
  TValue *rb = RKB(i); \
  TValue *rc = RKC(i); \
  if (ttisnumber(rb) && ttisnumber(rc)) { \
    lua_Number nb = nvalue(rb), nc = nvalue(rc); \
    setnvalue(ra, op(nb, nc)); \
  } \
  else \
    Protect(Arith(L, ra, rb, rc, tm)); \
}

#define arith_opfnNumber(op,tm) { \
  TValue *rb = RKB(i); \
  if (ttisnumber(rb)) { \
    lua_Number nb = nvalue(rb); \
    setnvalue(ra, op(nb)); \
  } \
  else \
    Protect(Arith(L, ra, rb, luaO_nilobject, tm)); \
}

#ifndef PROPCMPLX
#define arith_opfn(L,op,opc,tm) { \
  TValue *rb = RKB(i); \
  if (ttisnumber(rb)) { \
    lua_Number nb = nvalue(rb); \
    setnvalue(ra, op(nb)); \
  } \
  else if (ttiscomplex(rb)) { \
    agn_Complex nc = cvalue(rb); \
    setcvalue(ra, opc(nc)); \
  } \
  else \
    Protect(Arith(L, ra, rb, luaO_nilobject, tm)); \
}
#else
#define arith_opfn(L,op,opc,tm) { \
  TValue *rb = RKB(i); \
  if (ttisnumber(rb)) { \
    lua_Number nb = nvalue(rb); \
    setnvalue(ra, op(nb)); \
  } \
  else if (ttiscomplex(rb)) { \
    lua_Number z[2]; \
    lua_Number a, b; \
    a = complexreal(rb); \
    b = compleximag(rb); \
    opc(z, a, b); \
    setcvalue(ra, z); \
  } \
  else \
    Protect(Arith(L, ra, rb, luaO_nilobject, tm)); \
}
#endif

#define callmetaoftype(ra,rb,rc,s) { \
  if ( ((s) == NULL && tools_streq(luaT_typenames[rb->tt], svalue(rc))) || \
       ((s) != NULL && tools_streq(getstr((s)), svalue(rc))) \
     ) \
     call_binTM(L, rb, rb, ra, TM_OFTYPE); \
  else { \
    setbvalue(ra, 0); \
  } \
}

#define callmetanotoftype(ra,rb,rc,s) { \
  if ( ((s) == NULL && tools_streq(luaT_typenames[rb->tt], svalue(rc))) || \
    ((s) != NULL && tools_streq(getstr((s)), svalue(rc))) ) { \
    call_binTM(L, rb, rb, ra, TM_OFTYPE); \
    if (ttisboolean((ra))) { \
      setbvalue((ra), l_isfalseorfail((ra))); \
    } else if (ttisnil((ra))) { \
      setbvalue((ra), 1); \
    } \
  } else { \
    setbvalue(ra, 1); \
  } \
}

#define callmetareturnbasictype(L,smetatable,slot,type) { \
  if (((tm = fasttm(L, smetatable, TM_OFTYPE)) != NULL) && ttisfunction(tm)) { \
    if (type != p->rettype) { \
      luaG_runerror(L, "Error in " LUA_QS ": result of type %s expected, got %s.", "return", \
        luaT_typenames[p->rettype], luaT_typenames[(int)type]); \
    } \
    L->top++; \
    call_binTM(L, slot, slot, L->top, TM_OFTYPE); \
    if (l_isfalseorfail(L->top) || ttisnil(L->top)) \
      luaG_runerror(L, "return does not satisfy type check metamethod"); \
    L->top--; \
    break; \
  } \
}

#define callmetareturnutype(L,stype,smetatable,slot,type) { \
  if (((tm = fasttm(L, smetatable, TM_OFTYPE)) != NULL) && ttisfunction(tm)) { \
    if (stype != NULL && tools_strneq(getstr(stype), getstr(p->rettypets))) \
      luaG_runerror(L, "Error in " LUA_QS ": result of type %s expected, got %s.", "return", \
        getstr(p->rettypets), luaT_typenames[type]); \
    L->top++; \
    call_binTM(L, slot, slot, L->top, TM_OFTYPE); \
    if (l_isfalseorfail(L->top) || ttisnil(L->top)) \
      luaG_runerror(L, "return does not satisfy type check metamethod"); \
    L->top--; \
    break; \
  } else if (stype != NULL && tools_strneq(getstr(stype), getstr(p->rettypets))) \
    luaG_runerror(L, "Error in " LUA_QS ": result of type %s expected, got %s.", "return", \
      getstr(p->rettypets), luaT_typenames[type]); \
}

#define arithexceptionfncallandreturn(L,ra,res,n,d,opname,procname) { \
  res = savestack(L, ra); \
  luaD_checkstack(L, 4); \
  setobj2s(L, L->top, luaH_getstr(hvalue(gt(L)), luaS_new(L, "math"))); \
  if (ttistable(L->top)) { \
    setobj2s(L, L->top, luaH_getstr(hvalue(L->top), luaS_new(L, procname))); \
    if (!ttisfunction(L->top)) { \
      luaG_runerror(L, "Error in " LUA_QS ": `math` package function " LUA_QS " does not exist.", opname, procname); \
    } \
  } else \
    luaG_runerror(L, "Error in " LUA_QS ": `math` package table does not exist.", opname); \
  setnvalue(L->top + 1, n); \
  setnvalue(L->top + 2, d); \
  setnvalue(L->top + 3, L->arithstate); \
  L->top += 4; \
  Protect(luaD_call(L, L->top - 4, 1, 1)); \
  ra = restorestack(L, res); \
  setobjs2s(L, ra, L->top - 1); \
  L->top--; \
}

#define setarithstate(L,value,place) { \
  L->arithstate ^= (-(value ? (lu_byte)1 : (lu_byte)0) ^ L->arithstate) & place; \
}

static void checkbuiltinstack (lua_State *L, size_t len) {
  if (stacktop(L) + len - 1 >= stackmax(L)) {  /* stack too small ? */
    size_t newsize, cn;
    cn = L->currentstack;
    if (cn == 0)  /* 2.14.2 */
      luaG_runerror(L, "Error in " LUA_QS ": cannot extend stack 0.", "pushd");
    newsize = stackmax(L) << 1;
    if (isnumstack(L)) {  /* 7.7.7 K&R fix */
      lua_Number *temp = realloc(L->cells[cn], newsize*sizeof(lua_Number));
      if (temp == NULL)
        luaG_runerror(L, "Error in " LUA_QS ": memory allocation failed with number stack %d.", "pushd", cn);
      L->cells[cn] = temp;
    } else if (iscachestack(L)) {
      if (!lua_checkstack(L->C, len)) {  /* cannot call luaL_checkcache to prevent Agena from kernel panicking, i.e. quitting,
        and luaD_checkstack does not return an error when the max. stacksize has been exceeded */
        luaG_runerror(L, "Error in " LUA_QS ": cannot extend cache size beyond %d slots in stack %d.", "pushd", LUAI_MAXCSTACK, L->currentstack);
      }
      newsize = stackmax(L) + len;
    } else {  /* 7.7.7 K&R fix */
      unsigned char *temp = realloc(L->charcells[cn], newsize*sizeof(unsigned char));
      if (temp == NULL)
        luaG_runerror(L, "Error in " LUA_QS ": memory allocation failed with character stack %d.", "pushd", cn);
      L->charcells[cn] = temp;
    }
    stackmax(L) = newsize;
  }
}


/* Pushes a table field onto the stack, incrementing L->top by one. The function ensures there is enough stack space. 4.0.0 RC 1 */
static FORCE_INLINE void aux_gettablefield (lua_State *L, const TValue *t, const char *field) {
  TString *str;
  const TValue *slot;
  lua_lock(L);
  str = luaS_new(L, field);
  luaD_checkstack(L, 1);
  if (luaV_fastget(L, t, str, slot, luaH_getstr)) {
    setobj2s(L, L->top++, slot);
  } else {
    setsvalue2s(L, L->top++, str);
    luaV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  lua_unlock(L);
}


#define dojump(L,pc,i)   {(pc) += (i); luai_threadyield(L);}


static void releasetry (lua_State *L) {  /* 2.1 RC 2, written by Hu Qiwei */
  struct lua_longjmp *pj = L->errorJmp;
  if (pj->type == JMPTYPE_TRY) {
    L->errfunc = pj->old_errfunc;
    L->errorJmp = pj->previous;
    luaM_free(L, pj);
  }
}


static void restoretry (lua_State *L, int seterr, int ra) {  /* 2.1 RC 2, written by Hu Qiwei */
  struct lua_longjmp *pj = L->errorJmp;
  StkId oldtop = restorestack(L, pj->old_top);
  luaF_close(L, oldtop);  /* close eventual pending closures */
  L->nCcalls = pj->oldnCcalls;
  L->ci = restoreci(L, pj->old_ci);
  L->base = L->ci->base;
  L->allowhook = pj->old_allowhooks;
  if (seterr)
    luaD_seterrorobj(L, pj->status, L->base + ra);
  L->top = oldtop;
  if (L->size_ci > LUAI_MAXCALLS) {  /* there was an overflow? */
    int inuse = cast_int(L->ci - L->base_ci);
    if (inuse + 1 < LUAI_MAXCALLS)  /* can `undo' overflow? */
      luaD_reallocCI(L, LUAI_MAXCALLS);
  }
  releasetry(L);
}


void luaV_execute (lua_State *L, int nexeccalls) {
  LClosure *cl;
  StkId base;
  TValue *k;
  const Instruction *pc;
  struct lua_longjmp *pj;
  Instruction i;
  StkId ra;
  static const void *const dispatch_table[] = {
    AGENA_DISPATCH_TABLE_INIT
  };
  /* 3 percent speed up as recommended by Google AI, 7.5.9 */
#define DISPATCH() \
  { \
    if (__builtin_expect(((L->hookmask & (LUA_MASKLINE | LUA_MASKCOUNT)) && \
        (--L->hookcount == 0 || L->hookmask & LUA_MASKLINE)), 0)) { \
      traceexec(L, pc); \
      if (L->status == LUA_YIELD) { \
        L->savedpc = pc - 1; \
        return; \
      } \
      base = L->base; \
    } \
    i = *pc++; \
    ra = RA(i); \
    lua_assert(base == L->base && L->base == L->ci->base); \
    lua_assert(base <= L->top && L->top <= L->stack + L->stacksize); \
    lua_assert(L->top == L->ci->top || luaG_checkopenop(i)); \
    goto *dispatch_table[GET_OPCODE(i)]; \
  }
 reentry:
  lua_assert(isLua(L->ci));
  pc = L->savedpc;
  cl = &clvalue(L->ci->func)->l;
  base = L->base;
  k = cl->p->k;
  DISPATCH();
  LABEL_OP_MOVE: {
    setobjs2s(L, ra, RB(i));
    DISPATCH();
  }
  LABEL_OP_LOADK: {
    setobj2s(L, ra, KBx(i));
    DISPATCH();
  }
  LABEL_OP_LOADBOOL: {
    int b = GETARG_B(i);  /* value b must be nonnegative, although declared signed */
    if (b < 2) {
      setbvalue(ra, b);
    }
    else {
      setfailvalue(ra);  /* 0.9.2 */
    }
    if (GETARG_C(i)) pc++;  /* skip next instruction (if C) */
    DISPATCH();
  }
  LABEL_OP_LOADNIL: {
    TValue *rb = RB(i);
    do {
      setnilvalue(rb--);
    } while (rb >= ra);
    DISPATCH();
  }
  LABEL_OP_GETUPVAL: {
    int b = GETARG_B(i);
    setobj2s(L, ra, cl->upvals[b]->v);
    DISPATCH();
  }
  LABEL_OP_GETGLOBAL: {
    TValue g;
    TValue *rb = KBx(i);
    sethvalue(L, &g, cl->env);
    lua_assert(ttisstring(rb));
    Protect(luaV_gettable(L, &g, rb, ra));
    DISPATCH();
  }
  LABEL_OP_GETTABLE: {
    Protect(luaV_gettable(L, RB(i), RKC(i), ra));
    DISPATCH();
  }
  LABEL_OP_IN: {  /* 0.7.1, November 23, 2007 */
    Protect(agenaV_in(L, RKB(i), RKC(i), ra));
    DISPATCH();
  }
  LABEL_OP_TSUBSET: {
    Protect(agenaV_subset(L, RKB(i), RKC(i), ra, 0));
    DISPATCH();
  }
  LABEL_OP_TXSUBSET: {  /* 0.9.1, January 12, 2008 */
    Protect(agenaV_subset(L, RKB(i), RKC(i), ra, 2));
    DISPATCH();
  }
  LABEL_OP_SETGLOBAL: {
    TValue g;
    sethvalue(L, &g, cl->env);
    lua_assert(ttisstring(KBx(i)));
    Protect(luaV_settable(L, &g, KBx(i), ra));
    DISPATCH();
  }
  LABEL_OP_SETUPVAL: {
    UpVal *uv = cl->upvals[GETARG_B(i)];
    setobj(L, uv->v, ra);
    luaC_barrier(L, uv, ra);
    DISPATCH();
  }
  LABEL_OP_SETTABLE: {
    Protect(luaV_settable(L, ra, RKB(i), RKC(i)));
    DISPATCH();
  }
  LABEL_OP_NEWTABLE: {
    int b = GETARG_B(i);
    int c = GETARG_C(i);
    sethvalue(L, ra, luaH_new(L, luaO_fb2int(b), luaO_fb2int(c)));
    Protect(luaC_checkGC(L));
    DISPATCH();
  }
  LABEL_OP_TUNION: {
    Protect(agenaV_union(L, RKB(i), RKC(i), ra); luaC_checkGC(L));
    DISPATCH();
  }
  LABEL_OP_TMINUS: {
    Protect(agenaV_setops(L, RKB(i), RKC(i), ra, 1); luaC_checkGC(L));
    DISPATCH();
  }
  LABEL_OP_TINTERSECT: {
    Protect(agenaV_setops(L, RKB(i), RKC(i), ra, 0); luaC_checkGC(L));
    DISPATCH();
  }
  LABEL_OP_ADD: {
    arith_op(L, luai_numadd, agnc_add, TM_ADD);
    DISPATCH();
  }
  LABEL_OP_SUB: {
    arith_op(L, luai_numsub, agnc_sub, TM_SUB);
    DISPATCH();
  }
  LABEL_OP_MUL: {
    arith_op(L, luai_nummul, agnc_mul, TM_MUL);
    DISPATCH();
  }
  LABEL_OP_DIV: {
    arith_op(L, luai_numdiv, agnc_div, TM_DIV);
    DISPATCH();
  }
  LABEL_OP_MOD: {
    arith_opNumber(luai_nummod, TM_MOD);
    DISPATCH();
  }
  LABEL_OP_POW: {
    arith_op(L, luai_numpow, agnc_pow, TM_POW);
    DISPATCH();
  }
  LABEL_OP_IPOW: {
    arith_op(L, luai_numipow, agnc_ipow, TM_IPOW);  /* 0.22.2 */
    DISPATCH();
  }
  LABEL_OP_UNM: {  /* unary minus */
    TValue *rb = RB(i);
    if (ttisnumber(rb)) {
      lua_Number nb = nvalue(rb);
      setnvalue(ra, luai_numunm(nb));
    }
    else if (ttiscomplex(rb)) {
#ifndef PROPCMPLX
      agn_Complex nb = cvalue(rb);
      setcvalue(ra, agnc_unm(nb));
#else
      lua_Number nb1 = complexreal(rb), nb2 = compleximag(rb);
      lua_Number z[2];
      agnc_unm(z, nb1, nb2);
      setcvalue(ra, z);
#endif
    }
    else if (ttisboolean(rb)) {  /* 2.31.1 */
      setnvalue(ra, bvalue(rb) == 1 ? -1 : 0);
    }
    else {
      Protect(Arith(L, ra, rb, luaO_nilobject, TM_UNM));  /* 3.5.3 change */
    }
    DISPATCH();
  }
  LABEL_OP_NOT: {
    int res = l_isfalseorfail(RB(i));  /* Agena 1.4.0; next assignment may change this value */
    setbvalue(ra, res);
    DISPATCH();
  }
  LABEL_OP_LEN: {  /* `size' operator, extended to support TH_SIZE metamethod 1.9.1; */
    const TValue *rb = RB(i);
    Protect(luaV_objlen(L, ra, rb));
    DISPATCH();
  }
  LABEL_OP_SPLIT: {  /* added 0.11.2 */
    /* get prefix, must use RB() and not RKB() to display line causing an error correctly */
    TValue *rb = RB(i);
    /* get postfix (the deliminator) */
    TValue *rc = RKC(i);
    if (ttype(rb) == LUA_TSTRING && ttype(rc) == LUA_TSTRING) {
       Protect(agenaV_split(L, svalue(rb), svalue(rc), ra); luaC_checkGC(L));
       /* you must call GC, otherwise split may consume large amounts of memory over time */
    } else {
       luaG_runerror(L, "Error in " LUA_QS ": two strings expected.", "split");
    }
    DISPATCH();
  }
  LABEL_OP_CONCAT: {
    int b = GETARG_B(i);
    int c = GETARG_C(i);
    Protect(luaV_concat(L, c - b + 1, c); luaC_checkGC(L));
    setobjs2s(L, RA(i), base + b);
    DISPATCH();
  }
  LABEL_OP_JMP: {
    dojump(L, pc, GETARG_sBx(i));
    DISPATCH();
  }
  LABEL_OP_EQ: {
    TValue *rb = RKB(i);
    TValue *rc = RKC(i);
    Protect(
      if (equalobj(L, rb, rc) == GETARG_A(i)) {
        dojump(L, pc, GETARG_sBx(*pc));
      }
    )
    pc++;
    DISPATCH();
  }
  LABEL_OP_LT: {
    TValue *rb = RKB(i);
    TValue *rc = RKC(i);
    Protect(
      if (luaV_lessthan(L, rb, rc, 1) == GETARG_A(i)) {
        dojump(L, pc, GETARG_sBx(*pc));
      }
    )
    pc++;
    DISPATCH();
  }
  LABEL_OP_LE: {
    TValue *rb = RKB(i);
    TValue *rc = RKC(i);
    Protect(
      if (luaV_lessequal(L, rb, rc, 1) == GETARG_A(i)) {
        dojump(L, pc, GETARG_sBx(*pc));
      }
    )
    pc++;
    DISPATCH();
  }
  LABEL_OP_TEST: {
    if (l_isfalseorfail(ra) != GETARG_C(i))
      dojump(L, pc, GETARG_sBx(*pc));
    pc++;
    DISPATCH();
  }
  LABEL_OP_TESTSET: {
    TValue *rb = RB(i);
    if (l_isfalseorfail(rb) != GETARG_C(i)) {
      setobjs2s(L, ra, rb);
      dojump(L, pc, GETARG_sBx(*pc));
    }
    pc++;
    DISPATCH();
  }
  /* 0.9.2, March 09, 2008; patched 0.10.0, April 27, 2008 */
  LABEL_OP_DOTTED: {
    /* RKB(i)= object, RKC(i) = indices (one or two), ra: stack index */
    TValue *rb = RKB(i);
    if (ttisstring(rb)) {
      Protect(luaV_substring(L, rb, RKC(i), ra));
    } else if (ttistable(rb)) {
      Protect(luaV_tablesublist(L, rb, RKC(i), ra));
    } else if (ttisseq(rb)) {
      Protect(luaV_seqsublist(L, rb, RKC(i), ra));
    } else if (ttisreg(rb)) {
      Protect(luaV_regsublist(L, rb, RKC(i), ra));
    /* else will try the tag method, 2.17.8 */
    } else if (ttisuserdata(rb)) {
      const TValue *tm;
      if (ttisnil(tm = luaT_gettmbyobj(L, rb, TM_INDEX)))  /* alternative to `fasttm` to fetch mt's > 31 */
        agnG_indexerror(L, rb, RKC(i));
      if (ttisfunction(tm)) {
        callTMdotted(L, ra, tm, rb, RKC(i), RKC(i) + 1);
        /* do not call `return' ! */
      }
    } else
      luaG_runerror(L, "Error in indexing operation: string, table, sequence or register expected, got %s.",
        luaT_typenames[(int)ttype(rb)]);
    /* do not adjust L->top ! The stack will be confused otherwise */
    DISPATCH();
  }
  LABEL_OP_CALL: {
    int b = GETARG_B(i);
    int nresults = GETARG_C(i) - 1;
    if (b != 0) L->top = ra + b;  /* else previous instruction set top */
    L->savedpc = pc;
    switch (luaD_precall(L, ra, nresults)) {
      case PCRLUA: {
        nexeccalls++;
        goto reentry;  /* restart luaV_execute over new Lua function */
      }
      case PCRC: case PCRREMEMBER: {
        /* it was a C function (`precall' called it) or an rtable hit; adjust results */
        if (nresults >= 0) L->top = L->ci->top;
        base = L->base;
        DISPATCH();
      }
      default: {
        return;  /* yield */
      }
    }
  }
  /* for commands returning no values, added 0.6.0; mechanism changed in 0.9.1 (20 % speed gain) */
  LABEL_OP_CMD: {
    int nargs;
    StkId oldtop;
    nargs = GETARG_B(i);  /* number of arguments */
    oldtop = L->top;
    L->top = ra + nargs;
    switch (GETARG_C(i)) {  /* check instruction, changed 0.9.1 */
      case CMD_CASE: {  /* CASE value OF list THEN statement, added 0.6.0 */
        int j, flag;
        flag = 1;
        Protect(
        TValue *left = ra;
        for (j=1; j < nargs; j++) {
          TValue *right = ra + j;
          if (equalobjonebyone(L, left, right) == 1) {  /* 2.9.5 change */
            flag = 0;
            break;
          }
        }
        if (flag)  /* no match */
          dojump(L, pc, GETARG_sBx(*pc));
        )
        pc++;
        break;
      }
      case CMD_AMONG: {  /* CASE value OF value TO value statement, added 2.9.0 */
        int flag;
        lua_Number x, a, b;
        const char *s1, *s2, *s3;
        flag = 1;
        Protect(
        TValue *val = ra;
        TValue *from = ra + 1;
        TValue *to = ra + 2;
        if (ttisnumber(val) && ttisnumber(from) && ttisnumber(to)) {
          x = nvalue(val);
          a = nvalue(from);
          b = nvalue(to);
          if (a <= x && x <= b) flag = 0;
        } else if (ttisstring(val) && ttisstring(from) && ttisstring(to)) {
          s1 = svalue(val);
          s2 = svalue(from);
          s3 = svalue(to);
          if ( (s1 != NULL && s2 != NULL && s3 != NULL) && (*s2 <= *s1 && *s1 <= *s3) ) flag = 0;
        }
        if (flag)  /* no match */
          dojump(L, pc, GETARG_sBx(*pc));
        )
        pc++;
        break;
      }
      case CMD_INSERTINTO: {
        const TValue *t = ra + nargs;
        agenaV_insert(L, t, ra, nargs);
        break;
      }
      case CMD_DELETEFROM: {
        const TValue *t = ra + nargs;
        agenaV_delete(L, t, ra, nargs);
        break;
      }
      case CMD_PUSHD: {  /* pushd instruction (push directly): store the given values to the built-in stack,
        the last one to reside at the top of the stack */
        int isseq, offset;
        size_t k, n, oldstacknr;
        const TValue *res;
        StkId lastarg;
        Seq *h = NULL;
        isseq = ttisseq(ra);
        if (isseq) {
          h = seqvalue(ra);
          n = h->size;
        } else
          n = nargs;
        lastarg = ra + nargs - 1;
        oldstacknr = L->currentstack;
        offset = 0;
        if (ttispair(lastarg)) {  /* 2.9.7 */
          const TValue *left, *right;
          Pair *h;
          int newstacknr;
          h = pairvalue(lastarg);
          left = agnPair_geti(h, 1);
          right = agnPair_geti(h, 2);
          if (!ttisstring(left) || tools_strneq(svalue(left), "stack"))  /* 2.16.12 tweak */
            luaG_runerror(L, "Error in " LUA_QS ": left-hand side of option is not the string \'stack\'.", "pushd");
          if (!ttisnumber(right))
            luaG_runerror(L, "Error in " LUA_QS ": right-hand side of option is not a number, got %s.",
              "pushd", luaT_typenames[(int)ttype(right)]);
          newstacknr = nvalue(right);
          if (newstacknr < 0 || newstacknr > LUA_NUMSTACKS + LUA_CHARSTACKS)
            luaG_runerror(L, "Error in " LUA_QS ": stack number must be from 1 to 6, got %d.", "pushd", newstacknr);
          L->currentstack = newstacknr;
          offset = 1;
        }
        checkbuiltinstack(L, n);  /* stack too small ? Extend it if necessary. Modularised 2.28.3 */
        if (isseq) {  /* sequence given ?  2.9.6 */
          if (isnumstack(L)) {  /* 2.12.7 */
            lua_Number *a = malloc(n * sizeof(lua_Number));
            if (a == NULL) {  /* 4.11.5 fix */
              luaG_runerror(L, "Error in " LUA_QS ": memory allocation failed.", "pushd");
            }
            for (k=0; k < n; k++) {  /* first check all values in sequence before pushing them onto the stack */
              res = agnSeq_geti(h, k + 1);
              if (!ttisnumber(res)) {
                xfree(a);
                luaG_runerror(L, "Error in " LUA_QS ": expected a number in sequence, got %s.", "pushd", luaT_typenames[(int)ttype(res)]);
              }
              a[k] = nvalue(res);
            }
            for (k=0; k < n; k++) {
              cells(L, stacktop(L)++) = a[k];
            }
            xfree(a);
          } else if (iscachestack(L)) {
            luaG_runerror(L, "Error in " LUA_QS ": caches not yet supported.", "pushd");
          } else {  /* char stack */
            size_t len = 0;
            for (k=0; k < n; k++) {  /* first check all values in sequence before pushing them onto the stack */
              res = agnSeq_geti(h, k + 1);
              if (ttisstring(res)) {
                len += tsvalue(res)->len;
              } else if (ttisnumber(res)) {
                len++;
              } else {
                luaG_runerror(L, "Error in " LUA_QS ": expected a string or number in sequence, got %s.", "pushd", luaT_typenames[(int)ttype(res)]);
              }
            }
            checkbuiltinstack(L, len);
            for (k=0; k < n; k++) {
              res = agnSeq_geti(h, k + 1);
              if (ttisstring(res)) {
                int i;
                len = tsvalue(res)->len;
                for (i=0; i < len; i++) {
                  charcell(L, stacktop(L)++) = uchar(svalue(res)[i]);
                }
              } else {  /* number */
                unsigned char n = nvalue(res) + '0';  /* integers 0 to 9 */
                charcell(L, stacktop(L)++) = n;
              }
            }
          }
        } else {  /* one or more numbers or strings have been passed */
          if (isnumstack(L)) {  /* 2.12.7 */
            for (k=0; k < n - offset; k++) {
              if (!ttisnumber(ra + k))
                luaG_runerror(L, "Error in " LUA_QS ": expected a number, got %s.", "pushd", luaT_typenames[(int)ttype(ra + k)]);
              cells(L, stacktop(L)++) = nvalue(ra + k);
            }
          } else if (iscachestack(L)) {  /* cache insert, 2.36.3 */
            for (k=0; k < n - offset; k++) {
              /* cannot push collectable datatypes from one state to another, otherwise Agena might crash, 6.5.8; so far
                 it is unknown why non-collectable complex numbers cannot be inserted, too. */
              if (ttisnumber(ra + k) || ttiscomplex(ra + k)) {
                setobj2s(L->C, L->C->top++, ra + k);
              } else if (ttisstring(ra + k)) {
                const char *str = svalue(ra + k);
                int len = tsvalue(ra + k)->len;
                setsvalue(L->C, L->C->top++, luaS_newlstr(L->C, str, len));
              } else {  /* 7.1.6 change: LUA_TCOMPLEX are save to be pushed, strings cannot */
                luaG_runerror(L, "Error in " LUA_QS ": cannot insert %s objects.", "pushd", luaT_typenames[ttype(ra + k)]);
              }
              stacktop(L)++;
            }
          } else {  /* char stack */
            for (k=0; k < n - offset; k++) {
              StkId idx = ra + k;
              if (ttisstring(idx)) {
                size_t len = tsvalue(idx)->len;
                if (len < 2) {  /* we have already malloced sufficient space for single characters */
                  charcell(L, stacktop(L)++) = uchar(svalue(idx)[0]);
                } else {
                  size_t i;
                  checkbuiltinstack(L, len - 1);  /* we already checked space for first character at the start */
                  for (i=0; i < len; i++) {
                    charcell(L, stacktop(L)++) = uchar(svalue(idx)[i]);
                  }
                }
              } else if (ttisnumber(ra + k)) {
                char n = nvalue(ra + k);  /* 2.14.4 fix */
                if (n < 0 || n > 9 || tools_isfrac(n))  /* 2.14.2 fix */
                  luaG_runerror(L, "Error in " LUA_QS ": expected a digit 0 .. 9, got %d.", "pushd", n);
                n = '0' + n;  /* integers 0 to 9 */
                charcell(L, stacktop(L)++) = n;
              } else
                luaG_runerror(L, "Error in " LUA_QS ": expected a string or number, got %s.", "pushd", luaT_typenames[(int)ttype(ra + k)]);
            }
          }
        }
        if (oldstacknr != L->currentstack) L->currentstack = oldstacknr;
        break;
      }
      case CMD_SWITCHD: {
        int v;
        lu_byte t = L->formerstack;
        if (!ttisnumber(ra))
          luaG_runerror(L, "Error in " LUA_QS ": expected a number, got %s.", "switchd", luaT_typenames[(int)ttype(ra)]);
        v = nvalue(ra);
        if (v < -1 || v >= LUA_NSTACKS)
          luaG_runerror(L, "Error in " LUA_QS ": stack number must be in 1 to %d, got %d.", "switchd", v, LUA_NSTACKS - 1);
        if (v == LUA_NSTACKS - 1 && !L->C)
          luaG_runerror(L, "Error in " LUA_QS ": cache stack %d is not available.", "switchd", v);
        L->formerstack = L->currentstack;  /* 2.12.7 patch; changed 2.14.2; extended 2.37.2 */
        L->currentstack = (v == -1) ? t : v;
        break;
      }
      case CMD_POPTOP: {
        Protect(
        TValue *val = RA(i);
        if (ttisseq(val)) {
          Seq *h = seqvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "pop");
          if (h->size != 0) {
            setnilvalue(L->top);
            api_check(L, L->top < L->ci->top);
            agnSeq_seti(L, h, h->size, L->top);
            luaC_barrierseq(L, h, L->top);  /* Agena 1.2 fix DELETEVAL */
          }
        } else if (ttisreg(val)) {  /* 2.3.0 RC 3 */
          Reg *h = regvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "pop");
          if (h->top != 0) {
            agnReg_purge(L, h, h->top);
            luaC_barrierreg(L, h, L->top);
          }
        } else if (ttistable(val)) {  /* Agena 1.1.0 */
          lua_Number b;
          Table *h = hvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "pop");
          b = agenaV_topindex(L, h);
          if (b != -HUGE_VAL) {  /* integer index found */
            setnilvalue(L->top);
            api_check(L, L->top < L->ci->top);
            luaH_setint(L, h, cast_int(b), L->top);  /* set it to null, tweaked 4.6.3 */
            luaC_barriert(L, h, L->top);
          }  /* else: do nothing */
        } else
          luaG_runerror(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.", "pop/top",
            luaT_typenames[(int)ttype(val)]);
        )
        break;
      }
      case CMD_POPBOTTOM: {
        Protect(
        if (ttisseq(ra)) {
          Seq *h = seqvalue(ra);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "pop");
          if (h->size != 0) {
            setnilvalue(L->top);
            api_check(L, L->top < L->ci->top);
            agnSeq_seti(L, h, 1, L->top);
            luaC_barrierseq(L, h, L->top);  /* Agena 1.2 fix DELETEVAL */
          }
        } else if (ttisreg(ra)) {  /* 2.3.0 RC 3 */
          Reg *h = regvalue(ra);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "pop");
          if (h->top != 0) {
            agnReg_purge(L, h, 1);
            luaC_barrierreg(L, h, L->top);
          }
        } else if (ttistable(ra)) {  /* Agena 1.1.0 */
          lua_Number b;
          Table *h = hvalue(ra);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "pop");
          b = agenaV_bottomindex(L, h);
          if (b != HUGE_VAL) {  /* integer index found */
            setnilvalue(L->top);
            api_check(L, L->top < L->ci->top);
            luaH_setint(L, h, cast_int(b), L->top);  /* 4.6.3 tweak */
            luaC_barriert(L, h, L->top);
          }  /* else: do nothing */
        } else
          luaG_runerror(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.", "pop/bottom",
            luaT_typenames[(int)ttype(ra)]);
        )
        break;
      }
      case CMD_ROTATETOP: {  /* 2.2.0 */
        Protect(
        TValue *val = RA(i);
        if (ttisseq(val)) {
          Seq *h = seqvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "rotate");
          if (h->size != 0)
            agnSeq_rotatetop(L, h);
        } else if (ttisreg(val)) {  /* 2.3.0 RC 3 */
          Reg *h = regvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "rotate");
          if (h->top != 0)
            agnReg_rotatetop(L, h);
        } else if (ttistable(val)) {
          Table *h = hvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "rotate");
          if (h->sizearray != 0)
            agnH_rotatetop(L, h);
        } else
          luaG_runerror(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.", "rotate/top",
            luaT_typenames[(int)ttype(val)]);
        )
        break;
      }
      case CMD_ROTATEBOTTOM: {  /* 2.2.0 */
        Protect(
        TValue *val = RA(i);
        if (ttisseq(val)) {
          Seq *h = seqvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "rotate");
          if (h->size != 0)
            agnSeq_rotatebottom(L, h);
        } else if (ttisreg(val)) {  /* 2.3.0 RC 3 */
          Reg *h = regvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "rotate");
          if (h->top != 0)
            agnReg_rotatebottom(L, h);
        } else if (ttistable(val)) {
          Table *h = hvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "rotate");
          if (h->sizearray != 0)
            agnH_rotatebottom(L, h);
        } else
          luaG_runerror(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.", "rotate/bottom",
            luaT_typenames[(int)ttype(val)]);
        )
        break;
      }
      case CMD_DUPLICATE: {  /* 2.2.0 */
        Protect(
        TValue *val = RA(i);
        if (ttisseq(val)) {
          Seq *h = seqvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "duplicate");
          if (h->size > 0)
            agnSeq_duplicate(L, h);
        } else if (ttisreg(val)) {  /* 2.3.0 RC 3 */
          Reg *h = regvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "duplicate");
          if (h->top > 0) {
            if (agnReg_duplicate(L, h) == 0)
              luaG_runerror(L, "Error in " LUA_QS ": cannot extend register, top is at %d.", "duplicate", h->top);
          }
        } else
          luaG_runerror(L, "Error in " LUA_QS ": sequence or register expected, got %s.", "duplicate",
            luaT_typenames[(int)ttype(val)]);
        )
        break;
      }
      case CMD_EXCHANGE: {  /* 2.2.0 */
        Protect(
        TValue *val = RA(i);
        if (ttisseq(val)) {
          Seq *h = seqvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "exchange");
          if (h->size > 1)
            agnSeq_exchange(L, h);
        } else if (ttisreg(val)) {
          Reg *h = regvalue(val);
          if (fasttm(L, h->metatable, TM_WRITEINDEX) != NULL)  /* Agena 2.9.1 */
            luaG_runerror(L, "Error in " LUA_QS ": tables with __writeindex metamethod not supported.", "exchange");
          if (h->top > 1)
            agnReg_exchange(L, h);
        } else
          luaG_runerror(L, "Error in " LUA_QS ": sequence or register expected, got %s.", "exchange",
            luaT_typenames[(int)ttype(val)]);
        )
        break;
      }
      case CMD_BYE: {
        if (!agn_getgui(L)) {
          agnL_onexit(L, 0);  /* 2.7.0, GC's and frees L->C, too. */
          lua_close(L);
          exit(0);
        }
        break;
      }
      case CMD_CLS: {
        if (!agn_getgui(L))
          #if defined(_WIN32) || defined(__DJGPP__) || defined(__OS2__) || defined(__HAIKU__)
          if (system("cls") == -1)  /* 1.6.4 to prevent compiler warnings */
            luaG_runerror(L, "Error in cls statement: system call failed.");
          #elif defined(__unix__) || defined(__APPLE__)
          if (system("clear") == -1)
            luaG_runerror(L, "Error in cls statement: system call failed.");
          #endif
        break;
      }
      case CMD_GC: {  /* perform garbage collection for clear command, patched 1.6.0 Valgrind */
        /* put environ.gc function on stack */
        setobj2s(L, ra, luaH_getstr(hvalue(gt(L)), luaS_new(L, "environ")));
        if (ttistable(ra)) {
          setobj2s(L, ra, luaH_getstr(hvalue(ra), luaS_new(L, "gc")));
          if (!ttisfunction(ra))
            luaG_runerror(L, "Error in " LUA_QS ": `environ.gc` does not exist.", "clear");  /* 2.0.0 correction */
        } else
          luaG_runerror(L, "Error in " LUA_QS ":`environ` package table does not exist.", "clear");  /* 2.0.0 correction */
        /* do _not_ put null argument on stack */
        /* now conduct the garbage collection */
        Protect(luaD_call(L, ra, nargs, 1));  /* 1.6.0. Valgrind, function, args and ignore result */
        /* delete result of gc() */
        break;
      }
      case CMD_IMPORT: {  /* call readlib from import statement, 2.0.0 */
        /* ra currently is null, now overwrite it with `readlib` */
        setobj2s(L, ra, luaH_getstr(hvalue(gt(L)), luaS_new(L, "readlib")));
        if (!ttisfunction(ra))
          luaG_runerror(L, "Error in " LUA_QS ": `readlib` does not exist.", "import");
        /* now call readlib */
        Protect(luaD_call(L, ra, 0, 1));  /* call readlib with 0 results */
        break;
      }
      case CMD_IMPORTAS: {  /* call readlib from import statement, 2.12.0 RC 1 */
        /* ra currently is null, now overwrite it with `readlib` */
        TString *str = rawtsvalue(ra + 1);
        setobj2s(L, ra, luaH_getstr(hvalue(gt(L)), luaS_new(L, "readlib")));
        if (!ttisfunction(ra))
          luaG_runerror(L, "Error in " LUA_QS ": `readlib` does not exist.", "import");
        /* now call readlib */
        L->top--;  /* do not pass alias to readlib */
        Protect(luaD_call(L, ra, 0, 1));  /* call readlib with 0 results */
        setobj2s(L, ra, luaH_getstr(hvalue(gt(L)), str));
        break;
      }
      case CMD_ALIAS: {  /* call with from import/alias statement, 2.0.0 */
        /* ra currently is null, now overwrite it with `readlib` */
        setobj2s(L, ra, luaH_getstr(hvalue(gt(L)), luaS_new(L, "initialise")));
        if (!ttisfunction(ra))
          luaG_runerror(L, "Error in " LUA_QS ": `initialise` does not exist.", "import");
        /* now call readlib */
        Protect(luaD_call(L, ra, 0, 1));  /* call initialise with 0 results */
        break;
      }
      case CMD_RESTART: {  /* perform restart */
        agnL_onexit(L, 1);  /* 2.7.0, if environ.onexit is assigned a function, it is called */
        /* put __RESTART function on stack */
        setobj2s(L, ra, luaH_getstr(hvalue(gt(L)), luaS_new(L, "environ")));
        if (ttistable(ra)) {
          setobj2s(L, ra, luaH_getstr(hvalue(ra), luaS_new(L, "__RESTART")));
          if (!ttisfunction(ra))
            luaG_runerror(L, "Error in " LUA_QS ": `environ.__RESTART` does not exist.", "restart");
        } else
          luaG_runerror(L, "Error in " LUA_QS ": `environ` package table does not exist.", "restart");
        /* put null argument on stack */
        setnilvalue(ra + 1);  /* do not comment the following lines ! */
        luaD_checkstack(L, 2);  /* Agena 1.9.4 fix */
        L->top = ra + 2;  /* func. + 1 arg */  /* Agena 1.9.4 fix */
        /* restart the environment */
        Protect(luaD_call(L, ra, 0, 1));  /* function, args and 0 result */
        /* do not execute setnilvalue(ra) for it will corrupt the stack if restart is called after
           an error was issued !  Agena 1.0.5 fix */
        /* luaD_call will level the stack automatically, even in case of errors */
        break;  /* 0.22.3 fix */
      }
      case CMD_REMEMBER: {  /* assign read-write remember table to procedure body, 2.7.0 */
        /* the procedure is available at ra */
        Table *rt;
        Closure *c;
        if (!ttisfunction(ra))
          luaG_runerror(L, "Error in reminisce feature: procedure has not been found.");
        c = clvalue(ra);
        /* 7.5.6 fix: force all remember table entries into the array part, to ensure speed and reliability
           GREP "Grep luaH_new(L, LUAI_MAXCALLS" if you change this (ldo.c, near hashnum()) */
        rt = luaH_new(L, AGNI_RTSLOTS, 0);
        if (rt) {
          c->l.rtable = rt;
          c->l.updatertable = 1;  /* write mode */
          luaC_objbarrier(L, c, rt);
        } else
          luaG_runerror(L, "Error: failed to initialise remember table.");
        break;
      }
      case CMD_STORE: {  /* assign internal table to procedure body, 2.16.7 */
        /* the procedure is available at ra */
        Table *rt;
        Closure *c;
        if (!ttisfunction(ra))
          luaG_runerror(L, "Error in storage feature: procedure has not been found.");
        c = clvalue(ra);
        rt = luaH_new(L, 0, 0);
        if (rt) {
          c->l.storage = rt;
          luaC_objbarrier(L, c, rt);
        } else
          luaG_runerror(L, "Error: failed to initialise store.");
        break;
      }
      default: {
        return;
      }
    }
    L->top = oldtop;
    DISPATCH();  /* leave continue here */
  }
  LABEL_OP_FN: {  /* for functions requiring exactly one or more arguments and returning one value;
    added 0.7.1, November 18, 2007 */
    switch(GETARG_C(i)) {
      case OPR_ASSIGNED: {
        switch (ttype(RB(i))) {
          case LUA_TNIL: case LUA_TNONE: case LUA_TFAIL: {
            setbvalue(ra, 0);
            break;
          }
          default: {
            setbvalue(ra, 1);
          }
        }
        DISPATCH();
      }
      case OPR_UNASSIGNED: {  /* added 0.7.1; tuned 0.10.0 */
        setbvalue(ra, ttisnil(RB(i)));
        DISPATCH();
      }
      case OPR_TYPE: {
        setsvalue(L, ra, luaS_new(L, luaT_typenames[RB(i)->tt]));
        DISPATCH();
      }
      case OPR_TYPEOF: {
        switch (RB(i)->tt) {
          case LUA_TFUNCTION: {
            Closure *c = clvalue(RB(i));
            if (c->l.type != NULL) {
              setsvalue(L, ra, c->l.type);
            } else if (c->c.type != NULL) {
              setsvalue(L, ra, c->c.type);
            } else
              setsvalue(L, ra, luaS_new(L, luaT_typenames[RB(i)->tt]));
            break;
          }
          case LUA_TSEQ: {
            Seq *s = seqvalue(RB(i));
            if (s->type == NULL) {
              setsvalue(L, ra, luaS_new(L, luaT_typenames[RB(i)->tt])); }
            else {
              setsvalue(L, ra, s->type); }
            break;
          }
          case LUA_TPAIR: {
            Pair *s = pairvalue(RB(i));
            if (s->type == NULL) {
              setsvalue(L, ra, luaS_new(L, luaT_typenames[RB(i)->tt])); }
            else {
              setsvalue(L, ra, s->type); }
            break;
          }
          case LUA_TTABLE: {
            Table *s = hvalue(RB(i));
            if (s->type == NULL) {
              setsvalue(L, ra, luaS_new(L, luaT_typenames[RB(i)->tt])); }
            else {
              setsvalue(L, ra, s->type); }
            break;
          }
          case LUA_TSET: {
            UltraSet *s = usvalue(RB(i));
            if (s->type == NULL) {
              setsvalue(L, ra, luaS_new(L, luaT_typenames[RB(i)->tt])); }
            else {
              setsvalue(L, ra, s->type); }
            break;
          }
          case LUA_TREG: {  /* 2.5.3 */
            Reg *s = regvalue(RB(i));
            if (s->type == NULL) {
              setsvalue(L, ra, luaS_new(L, luaT_typenames[RB(i)->tt])); }
            else {
              setsvalue(L, ra, s->type); }
            break;
          }
          case LUA_TUSERDATA: {  /* 2.5.4 */
            Udata *u = rawuvalue(RB(i));
            if (u->uv.type == NULL) {
              setsvalue(L, ra, luaS_new(L, luaT_typenames[RB(i)->tt])); }
            else {
              setsvalue(L, ra, u->uv.type); }
            break;
          }
          default:
            setsvalue(L, ra, luaS_new(L, luaT_typenames[RB(i)->tt]));
        }
        DISPATCH();
      }
      case OPR_DIMTAB: {  /* 2.9.6 */
        Table *newtable;
        int nargs, k, x, n[2];
        nargs = GETARG_B(i);
        Protect(
        n[0] = n[1] = 0;
        if (nargs < 1 || nargs > 2)
          Protect(luaG_runerror(L, "Error in " LUA_QS " constructor, one or two settings expected, got %d.",
            "table", nargs));
        for (k=0; k < nargs; k++) {
          if (!ttisnumber(ra + k))
            Protect(luaG_runerror(L, "Error in " LUA_QS " constructor, number expected, got %s.",
              "table", luaT_typenames[(int)ttype(ra + k)]));
          x = nvalue(ra + k);
          if (x < 0) x = 0;
          n[k] = x;
        }
        newtable = luaH_new(L, n[0], n[1]);
        sethvalue(L, ra, newtable);  /* 1.7.10 patch */
        luaC_checkGC(L)
        );
        DISPATCH();
      }
      case OPR_DIMDICT: {
        TValue *rb = RB(i);
        if (ttisnumber(rb)) {
          Table *newtable;
          int nb = nvalue(rb);
          if (nb < 0) nb = 0;  /* 1.7.3 */
          newtable = luaH_new(L, 0, (int)nb);
          sethvalue(L, ra, newtable);
          Protect(luaC_checkGC(L));
        }
        else
          Protect(luaG_runerror(L, "Error in " LUA_QS " constructor, number expected, got %s.",
             "dict", luaT_typenames[(int)ttype(rb)]));
        DISPATCH();
      }
      case OPR_DIMSET: {  /* 0.10.0*/
        TValue *rb = RB(i);
        if (ttisnumber(rb)) {
          int nb = nvalue(rb);
          if (nb < 0) nb = 0;  /* 1.7.3 */
          setusvalue(L, ra, agnUS_new(L, (int)nb));
          Protect(luaC_checkGC(L));
        }
        else
          Protect(luaG_runerror(L, "Error in " LUA_QS " constructor, number expected, got %s.",
             "set", luaT_typenames[(int)ttype(rb)]));
        DISPATCH();
      }
      case OPR_DIMSEQ: {  /* 0.11.0 */
        TValue *rb = RB(i);
        if (ttisnumber(rb)) {
          int nb = nvalue(rb);
          if (nb < 0) nb = 0;  /* 1.7.3 */
          setseqvalue(L, ra, agnSeq_new(L, (int)nb));
          Protect(luaC_checkGC(L));
        }
        else
          Protect(luaG_runerror(L, "Error in " LUA_QS " constructor, number expected, got %s.",
            "seq", luaT_typenames[(int)ttype(rb)]));
        DISPATCH();
      }
      case OPR_DIMREG: {  /* 2.3.0 RC 3 */
        TValue *rb = RB(i);
        if (ttisnumber(rb)) {
          int nb = nvalue(rb);
          if (nb <= 0) nb = L->regsize;
          setregvalue(L, ra, agnReg_new(L, (int)nb));
          Protect(luaC_checkGC(L));
        }
        else
          Protect(luaG_runerror(L, "Error in " LUA_QS " constructor, number expected, got %s.",
            "reg", luaT_typenames[(int)ttype(rb)]));
        DISPATCH();
      }
      case OPR_ABS: {
        TValue *rb = RB(i);
        switch (ttype(rb)) {
          case LUA_TNUMBER: {
            lua_Number nb = nvalue(rb);
            setnvalue(ra, luai_numabs(nb));
            break;
          }
          case LUA_TCOMPLEX: {
#ifndef PROPCMPLX
            agn_Complex nb = cvalue(rb);
            setnvalue(ra, agnc_abs(nb));
#else
            setnvalue(ra, agnc_abs(complexreal(rb), compleximag(rb)));
#endif
            break;
          }
          case LUA_TSTRING: {
            if (tsvalue(rb)->len == 1) {  /* output ASCII value for character */
              const char *s = svalue(rb);
              setnvalue(ra, cast_num(uchar(s[0])));
            } else {
              setfailvalue(ra);
            }
            break;
          }
          case LUA_TBOOLEAN: {  /* convert true -> 1, false -> 0, fail -> -1, like in Algol 68 */
            lua_Number val = cast_num((rb)->value.b);
            if (val < 2) {
              setnvalue(ra, val);
            } else {
              setnvalue(ra, -1);
            }
            break;
          }
          case LUA_TNIL: {  /* convert true -> 1, false -> 0, like in Algol 68 */
            setnvalue(ra, -2);
            break;
          }
          default: {
            Protect(Arith(L, ra, rb, luaO_nilobject, TM_ABS));  /* 3.5.3 change */
          }
        }
        DISPATCH();
      }
      case OPR_LEFT: {  /* 0.11.1, May 22, 2008 */
        TValue *rb = RB(i);
        if (ttype(rb) == LUA_TPAIR) {
          Protect(
            setobj2s(L, ra, pairitem(pairvalue(rb), 0));
          );
        } else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_LEFT))  /* 5.0.0 */
          luaG_typeerror(L, rb, "get left part of");
        DISPATCH();
      }
      case OPR_BOTTOM: {  /* 0.29.0, Nov 20, 2009 */
        TValue *rb = RB(i);
        Protect(
          agenaV_bottom(L, rb, ra); luaC_checkGC(L)
        );
        DISPATCH();
      }
      case OPR_RIGHT: {  /* 0.11.1, May 22, 2008 */
        TValue *rb = RB(i);
        if (ttype(rb) == LUA_TPAIR) {
          Protect(
            setobj2s(L, ra, pairitem(pairvalue(rb), 1));
          );
        } else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_RIGHT))  /* 5.0.0 */
          luaG_typeerror(L, rb, "get right part of");
        DISPATCH();
      }
      case OPR_TOP: {  /* 0.11.1, May 22, 2008 */
        TValue *rb = RB(i);
        if (ttisseq(rb)) {
          Protect(
            agenaV_topsequence(L, seqvalue(rb), ra);
          )
        } else if (ttistable(rb)) {
          Protect(
            agenaV_top(L, hvalue(rb), ra);
          );
        } else if (ttisreg(rb)) {
          Protect(
            agenaV_topregister(L, regvalue(rb), ra);
          )
        } else
          Protect(luaG_runerror(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.",
            "top", luaT_typenames[(int)ttype(rb)]));
        DISPATCH();
      }
      case OPR_POP: {  /* 2.2.0 May 03/23, 2014 */
        TValue *rb = RB(i);
        if (ttisseq(rb)) {
          Protect(
            Seq *h = seqvalue(rb);
            agenaV_topsequence(L, h, ra);
            setnilvalue(L->top);
            agnSeq_seti(L, h, h->size, L->top);
            luaC_barrierseq(L, h, L->top);  /* DELETEVAL */
          )
        } else if (ttisreg(rb)) {  /* 2.3.0 RC 3 */
          Protect(
            Reg *h = regvalue(rb);
            agenaV_topregister(L, h, ra);
            agnReg_purge(L, h, h->top);
            luaC_barrierreg(L, h, L->top);
          )
        } else if (ttistable(rb)) {
          Protect(
            lua_Number topindex;
            Table *h = hvalue(rb);
            topindex = agenaV_top(L, h, ra);
            /* if there is no integer key, i.e. topindex == -HUGE_VAL, prevent that h[-infinity] is deleted*/
            if (topindex != -HUGE_VAL) {
              setnilvalue(L->top);
              luaH_setint(L, h, cast_int(topindex), L->top);  /* 4.6.3 tweak */
              luaC_barriert(L, h, L->top);
            }
          )
        } else {
          Protect(luaG_runerror(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.",
            "pop", luaT_typenames[(int)ttype(rb)]));
        }
        DISPATCH();
      }
      case OPR_REAL: {  /* 0.11.1, May 22, 2008 */
        TValue *rb = RB(i);
        if (ttiscomplex(rb)) {
          Protect(
            setnvalue(ra, complexreal(rb));
          );
        } else if (ttisnumber(rb)) {  /* 3.16.6 extension */
          Protect(
            setnvalue(ra, nvalue(rb));
          );
        } else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_REAL))  /* 2.14.2 */
          luaG_typeerror(L, rb, "get real part of");
        DISPATCH();
      }
      case OPR_IMAG: {  /* 0.11.1, May 22, 2008 */
        TValue *rb = RB(i);
        if (ttiscomplex(rb)) {
          Protect(
            setnvalue(ra, compleximag(rb));
          );
        } else if (ttisnumber(rb)) {  /* 3.16.6 extension */
          Protect(
            setnvalue(ra, 0);
          );
        } else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_IMAG))  /* 2.14.2 */
          luaG_typeerror(L, rb, "get imaginary or dual part of");
        DISPATCH();
      }
      case OPR_ARCTAN: {
        arith_opfn(L, luai_numatan, agnc_atan, TM_ARCTAN);
        DISPATCH();
      }
      case OPR_ARCSIN: {  /* 0.27.0 */
        arith_opfn(L, luai_numasin, agnc_asin, TM_ARCSIN);
        DISPATCH();
      }
      case OPR_COS: {
        arith_opfn(L, luai_numcos, agnc_cos, TM_COS);
        DISPATCH();
      }
      case OPR_ENTIER: {
        arith_opfn(L, luai_numentier, agnc_entier, TM_ENTIER);
        DISPATCH();
      }
      case OPR_EVEN: {   /* 0.7.1, even numbers are beautiful */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          setbvalue(ra, luai_numiseven(nvalue(rb)));
        }
        else if (ttiscomplex(rb)) {
          setbvalue(ra, 2);  /* push fail */
        }
        else {
          Protect(Arith(L, ra, rb, luaO_nilobject, TM_EVEN));  /* 3.5.3 change */
        }
        DISPATCH();
      }
      case OPR_ODD: {   /* 2.9.0 */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          setbvalue(ra, luai_numisodd(nvalue(rb)));
        }
        else if (ttiscomplex(rb)) {
          setbvalue(ra, 2);  /* push fail */
        }
        else {
          Protect(Arith(L, ra, rb, luaO_nilobject, TM_ODD));  /* 3.5.3 change */
        }
        DISPATCH();
      }
      case OPR_EXP: {
        arith_opfn(L, luai_numexp, agnc_exp, TM_EXP);
        DISPATCH();
      }
      case OPR_FINITE: {  /* NOT +/- infinity or NOT undefined ? */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {  /* Sun's s_finite.c is not faster */
          setbvalue(ra, luai_numisfinite(nvalue(rb)));
        } else if (ttiscomplex(rb)) {
          setbvalue(ra, luai_numisfinite(complexreal(rb)) && luai_numisfinite(compleximag(rb)));  /* 2.1.2 */
        } else {
          Protect(Arith(L, ra, rb, luaO_nilobject, TM_FINITE));  /* 3.5.3 change */
        }
        DISPATCH();
      }
      case OPR_INFINITE: {  /* 2.10.0, +/- infinity ? */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          lua_Number nb = nvalue(rb);
          setbvalue(ra, luai_numisinfinite(nb));
        } else if (ttiscomplex(rb)) {
          setbvalue(ra, luai_numisinfinite(complexreal(rb)) && luai_numisinfinite(compleximag(rb)));
        } else {
          Protect(Arith(L, ra, rb, luaO_nilobject, TM_INFINITE));  /* 3.5.3 change */
        }
        DISPATCH();
      }
      case OPR_NAN: {  /* 2.1.2, undefined ? */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          lua_Number nb = nvalue(rb);
          setbvalue(ra, luai_numisnan(nb));
        } else if (ttiscomplex(rb)) {
          setbvalue(ra, luai_numisnan(complexreal(rb)) || luai_numisnan(compleximag(rb)));  /* 2.11.1 change; patched 2.12.0 RC 3 */
        } else {
          Protect(Arith(L, ra, rb, luaO_nilobject, TM_NAN));  /* 3.5.3 change */
        }
        DISPATCH();
      }
      /* added 0.9.0; needed in factorial, binomial, and beta function */
      case OPR_LNGAMMA: {
        arith_opfn(L, luai_numlngamma, agnc_lngamma, TM_LNGAMMA);  /* 0.28.1 */
        DISPATCH();
      }
      case OPR_INT: {
        arith_opfn(L, luai_numint, agnc_int, TM_INT);  /* luai_numint -> trunc (non-DOS & non-OS/2), sun_int (OS/2, DOS) */
        DISPATCH();
      }
      case OPR_FRAC: {
        arith_opfn(L, luai_numfrac, agnc_frac, TM_FRAC);
        DISPATCH();
      }
      case OPR_LN: {
        arith_opfn(L, luai_numln, agnc_ln, TM_LN);
        DISPATCH();
      }
      case OPR_SIGN: {
        TValue *rb = RB(i);
        if (ttisnumber(rb)) {
          arith_opfnNumber(tools_sign, TM_SIGN);  /* 2.17.1 optimisation */
        }
        else if (ttiscomplex(rb)) {
#ifndef PROPCMPLX
          agn_Complex z = cvalue(rb);
          if (creal(z) > 0 || (creal(z) == 0 && cimag(z) > 0)) {
            setnvalue(ra, 1);
          } else if (creal(z) < 0 || (creal(z) == 0 && cimag(z) < 0)) {
            setnvalue(ra, -1);
          } else {
            setnvalue(ra, 0);
          }
#else
          setnvalue(ra, tools_csgn(complexreal(rb), compleximag(rb)));
#endif
        }
        else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_SIGN))  /* 3.5.3 change */
          luaG_typeerror(L, rb, "get sign of");
        DISPATCH();
      }
      case OPR_SIN: {
        arith_opfn(L, luai_numsin, agnc_sin, TM_SIN);
        DISPATCH();
      }
      case OPR_SQRT: {
        arith_opfn(L, luai_numsqrt, agnc_sqrt, TM_SQRT);  /* 2.35.2, max precision fix */
        DISPATCH();
      }
      case OPR_TAN: {
        arith_opfn(L, luai_numtan, agnc_tan, TM_TAN);
        DISPATCH();
      }
      case OPR_RECIP: {
        arith_opfn(L, luai_numrecip, agnc_recip, TM_RECIP);
        DISPATCH();
      }
      case OPR_TFILLED: {
        Protect(agenaV_filled(L, RKB(i), ra); luaC_checkGC(L));
        DISPATCH();
      }
      case OPR_TEMPTY: {  /* 2.10.0 */
        Protect(agenaV_empty(L, RKB(i), ra); luaC_checkGC(L));
        DISPATCH();
      }
      case OPR_TSUMUP: {  /* 3.6.0, `addup` operator (ext. version of sumup) */
        Protect(agenaV_addup(L, ra, ra, GETARG_B(i)); luaC_checkGC(L));  /* 3.6.1 change */
        DISPATCH();
      }
      case OPR_QMDEV: {
        Protect(agenaV_qmdev(L, RKB(i), ra); luaC_checkGC(L));
        DISPATCH();
      }
      case OPR_TMULUP: {
        Protect(agenaV_mulup(L, ra, ra, GETARG_B(i)); luaC_checkGC(L));
        DISPATCH();
      }
      case OPR_NARGS: {
        setnvalue(RA(i), cast_num(L->ci->nargs));
        DISPATCH();
      }
      case OPR_STORE: {
        Closure *c = clvalue(L->ci->func);
        if (c->l.storage == NULL)
          luaG_runerror(L, "Error in " LUA_QS ": internal table not found.", "storage");
        Protect(
        sethvalue(L, ra, c->l.storage);
        luaC_checkGC(L));
        DISPATCH();
      }
      case OPR_PROCNAME: {  /* 2.10.4 */
        /* Deliberately, we use a (protected) keyword instead of a local variable (see OP_SELF, luaK_self, TK_OOP)
           to avoid unnecessary slow-downs on most of the cases where `procname` is not used at all. (Always
           reserving a local variable and assigning the function to it in all OP_CALL calls. */
        setclvalue(L, RA(i), &clvalue(L->ci->func)->l);
        Protect(luaC_checkGC(L));
        DISPATCH();
      }
      case OPR_NEWUSET: {
        int b = GETARG_B(i);
        setusvalue(L, ra, agnUS_new(L, luaO_fb2int(b)));
        Protect(luaC_checkGC(L));
        DISPATCH();
      }
      case OPR_NEWSEQ: {
        int b = GETARG_B(i);
        setseqvalue(L, ra, agnSeq_new(L, luaO_fb2int(b)));
        Protect(luaC_checkGC(L));
        DISPATCH();
      }
      case OPR_NEWREG: {  /* 2.3.0 RC 3 */
        int b = GETARG_B(i);
        setregvalue(L, ra, agnReg_new(L, b));  /* 2.3.1 fix, do not round up- or downwards */
        Protect(luaC_checkGC(L));
        DISPATCH();
      }
      case OPR_MULADD: {  /* 2.33.0, `muladd` operator */
        Protect(agenaV_muladd(L, ra, GETARG_B(i)); luaC_checkGC(L));  /* 3.6.0/1 fix to prevent out-of-memory exceptions */
        DISPATCH();
      }
      case OPR_MULUP: {  /* 3.11.2, `mulup` operator */
        Protect(agenaV_mulup(L, ra, ra, GETARG_B(i)); luaC_checkGC(L));
        DISPATCH();
      }
      case OPR_TQSUMUP: {  /* changed 4.5.0 */
        if (ttistable(ra)) {
          Protect(agenaV_qsumup(L, ra, ra, GETARG_B(i)); luaC_checkGC(L));
        }
        else if (ttisseq(ra)) {
          Protect(agenaV_seqqsumup(L, ra, ra, GETARG_B(i)); luaC_checkGC(L));
        }
        else if (ttisreg(ra)) {  /* 2.4.0 */
          Protect(agenaV_regqsumup(L, ra, ra, GETARG_B(i)); luaC_checkGC(L));
        }
        else if (ttisuserdata(ra)) {  /* 2.3.0 RC 3 */
          Protect(
            if (!call_binTM(L, ra, luaO_nilobject, ra, TM_QSUMUP))
              luaG_typeerror(L, ra, "adding squared elements in");
          );
        } else  /* 2.30.2 */
          Protect(luaG_runerror(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.",
             "qsumup", luaT_typenames[(int)ttype(ra)]));
        DISPATCH();
      }
      case OPR_FOREACH: {  /* 3.4.7, `foreach` operator */
        Protect(agenaV_foreach(L, ra, GETARG_B(i)); luaC_checkGC(L));  /* 3.6.0/1 fix to prevent out-of-memory exceptions */
        DISPATCH();
      }
      case OPR_SINH: {  /* 0.23.0 */
        arith_opfn(L, luai_numsinh, agnc_sinh, TM_SINH);
        DISPATCH();
      }
      case OPR_COSH: {  /* 0.23.0 */
        arith_opfn(L, luai_numcosh, agnc_cosh, TM_COSH);
        DISPATCH();
      }
      case OPR_TANH: {  /* 0.23.0 */
        arith_opfn(L, luai_numtanh, agnc_tanh, TM_TANH);
        DISPATCH();
      }
      case OPR_ARCCOS: {  /* 0.27.0 */
        arith_opfn(L, luai_numacos, agnc_acos, TM_ARCCOS);
        DISPATCH();
      }
      case OPR_ARCSEC: {  /* originally written in Agena for 0.12.0 as of June 30, 2008 */
        arith_opfn(L, luai_numasec, agnc_asec, TM_ARCSEC);
        DISPATCH();
      }
      case OPR_FRACTIONAL: {  /* 0.23.0, sun_isint code directly inserted here does not work here */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          setbvalue(ra, tools_isfrac(nvalue(rb)));  /* 2.14.3 change */
        } else if (ttiscomplex(rb)) {  /* 2.21.8 change */
          setbvalue(ra, tools_isfrac(complexreal(rb)) && compleximag(rb) == 0);
        } else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_FRACTIONAL)) {  /* 2.34.7 extension, 3.5.3 change */
          setbvalue(ra, 2);
        }
        DISPATCH();
      }
      case OPR_INTEGRAL: {  /* 2.12.1, sun_isint code directly inserted here does not work */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          setbvalue(ra, tools_isint(nvalue(rb)));  /* 2.14.3 change */
        } else if (ttiscomplex(rb)) {  /* 2.21.8 change */
          setbvalue(ra, tools_isint(complexreal(rb)) && compleximag(rb) == 0);
        } else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_INTEGRAL)) { /* 2.34.7 extension, 3.5.3 change */
          setbvalue(ra, 2);
        }
        DISPATCH();
      }
      case OPR_BNOT: {  /* 0.27.0, changed 2.10.0 */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {  /* changed 2.12.3, unsigned operation does not yield sensible results */
          if (L->settings & 1) {  /* signed bits operation ? */
            LUA_INT32 nb = nvalue(rb);  /* changed 2.9.0 */
            setnvalue(ra, ~(nb));
          } else {
            LUA_UINT32 nb = nvalue(rb);  /* changed 2.9.0, changed back to 32-bit operation 2.15.0 */
            setnvalue(ra, ~(nb));
          }
        }
        else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_BNOT)) {  /* 4.8.0 */
          Protect(luaG_runerror(L, "Error in " LUA_QS ": calling metamethod failed.", "~~"));
        }
        DISPATCH();
      }
      case OPR_LOG: {  /* 1.9.3, February 19, 2013 */
        int nargs = GETARG_B(i);
        if (nargs != 2) {
          Protect(luaG_runerror(L, "Error in " LUA_QS ": two arguments expected, got %d.",
            "log", nargs));
        }
        lua_lock(L);
        agenaV_log(L, ra, ra + 1);
        setnilvalue(ra + 1);  /* delete second argument */
        lua_unlock(L);
        DISPATCH();
      }
      case OPR_COSXX: {  /* 2.1.2, 40 % faster than the C library version */
        arith_opfn(L, luai_numcos, agnc_cosxx, TM_COSXX);  /* 2.14.13 change from *numcosxx to *numcos */
        DISPATCH();
      }
      case OPR_BEA: {  /* 2.1.2, 40 % faster than the C library version */
        arith_opfn(L, luai_numbea, agnc_bea, TM_BEA);
        DISPATCH();
      }
      case OPR_FLIP: {  /* 2.1.2, 40 % faster than the C library version */
        arith_opfn(L, luai_numflip, agnc_flip, TM_FLIP);
        DISPATCH();
      }
      case OPR_CONJUGATE: {  /* 2.1.2, 40 % faster than the C library version */
        arith_opfn(L, luai_numconjugate, agnc_conjugate, TM_CONJUGATE);
        DISPATCH();
      }
      case OPR_ANTILOG2: {  /* 2.4.1 */
        arith_opfn(L, luai_numantilog2, agnc_antilog2, TM_ANTILOG2);
        DISPATCH();
      }
      case OPR_ANTILOG10: {  /* 2.4.1 */
        arith_opfn(L, luai_numantilog10, agnc_antilog10, TM_ANTILOG10);
        DISPATCH();
      }
      case OPR_SIGNUM: {  /* 2.4.1 */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          setnvalue(ra, tools_signum(nvalue(rb)));
        } else if (ttiscomplex(rb)) {
#ifndef PROPCMPLX
          setcvalue(ra, agnc_signum(cvalue(rb)));
#else
          lua_Number z[2], re, im;
          re = complexreal(rb); im = compleximag(rb);
          agnc_signum(z, re, im);
          setcvalue(ra, z);
#endif
        } else if (ttisboolean(rb)) {  /* 2.40.0 extension */
          setnvalue(ra, bvalue(rb) == 1 ? 1 : -1);
        } else {
          Protect(Arith(L, ra, rb, luaO_nilobject, TM_SIGNUM));  /* 3.5.3 change */
        }
        DISPATCH();
      }
      case OPR_SINC: {  /* 2.4.2 */
        arith_opfn(L, luai_numsinc, agnc_sinc, TM_SINC);
        DISPATCH();
      }
      case OPR_CIS: {  /* 2.5.1, recommended by Slobodan */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          lua_Number x, si, co;
          x = nvalue(rb);
          sun_sincos(x, &si, &co);  /* 2.11.0 tuning */
          setrealimagvalue(ra, co, si);
        }
        else if (ttiscomplex(rb)) {
          /* even with GCC 8.3.0 we get errors here in OS/2, so leave the __OS2__ query here */
#if !defined(PROPCMPLX) && !defined(__OS2__)
          agn_Complex z = cvalue(rb);
          setcvalue(ra, ccos(z) + I*csin(z));
#else
          lua_Number a, b, i, t1, t2, t4, t6, z[2];
          a = complexreal(rb); b = compleximag(rb);
          /* 2.21.7 optimisation */
          sun_sincos(a, &t6, &t1);
          sun_sinhcosh(b, &t4, &t2);  /* 4.5.7 overflow tweak */
          i = t6*(-t4 + t2);  /* 2.21.7 optimisation */
          if (i == -0) i = 0;
          z[0] = t1*(t2 - t4);  /* 2.21.7 optimisation */
          z[1] = i;
#ifndef __OS2__
          setcvalue(ra, z);
#else
          setcvalue(ra, z[0] + I*z[1]);  /* 3.4.5, csin & ccos seem to be buggy in OS/2 GCC 4.4.6 and later */
#endif
#endif
        } else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_CIS))  /* 2.14.2, 3.5.3 change */
          luaG_typeerrorx(L, rb, "evaluate", "cis");  /* 2.21.0 */
        DISPATCH();
      }
      case OPR_SQUARE: {  /* 2.12.6 */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          lua_Number x;
          x = nvalue(rb);
          setnvalue(ra, x*x);
        }
        else if (ttiscomplex(rb)) {
          lua_Number a, b;
          TValue *rx = s2v(rb);
          a = complexreal(rx); b = compleximag(rx);
          setrealimagvalue(ra, a*a - b*b, 2*a*b);
        }
        else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_SQUARE))  /* 2.14.2, 3.5.3 change */
          luaG_typeerror(L, rb, "get square of");
        DISPATCH();
      }
      case OPR_CUBE: {  /* 2.13.0 */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          lua_Number x;
          x = nvalue(rb);
          setnvalue(ra, x * x * x);
        }
        else if (ttiscomplex(rb)) {
          lua_Number a, b, a2, b2;
          TValue *rx = s2v(rb);
          a = complexreal(rx); b = compleximag(rx);
          a2 = a*a; b2 = b*b;
          setrealimagvalue(ra, a*(a2 - 3 * b2), b*(3 * a2 - b2));
        }
        else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_CUBE))  /* 2.14.2, 3.5.3 change */
          luaG_typeerror(L, rb, "get cube of");
        DISPATCH();
      }
      case OPR_ZERO: {  /* 2.16.0 */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          lua_Number x;
          x = nvalue(rb);
          setbvalue(ra, x == 0);
        }
        else if (ttiscomplex(rb)) {
          lua_Number a, b;
          TValue *rx = s2v(rb);
          a = complexreal(rx); b = compleximag(rx);
          setbvalue(ra, a == 0 && b == 0);
        }
        else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_ZERO))  /* 3.5.3 change */
          luaG_typeerrorx(L, rb, "evaluate", "zero");  /* 2.21.0 */
        DISPATCH();
      }
      case OPR_NONZERO: {  /* 2.16.0 */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          lua_Number x;
          x = nvalue(rb);
          setbvalue(ra, x != 0);
        }
        else if (ttiscomplex(rb)) {
          lua_Number a, b;
          TValue *rx = s2v(rb);
          a = complexreal(rx); b = compleximag(rx);
          setbvalue(ra, a != 0 || b != 0);
        }
        else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_NONZERO))  /* 3.5.3 change */
          luaG_typeerrorx(L, rb, "evaluate", "nonzero");  /* 2.21.0 */
        DISPATCH();
      }
      case OPR_PEPS: {  /* 2.9.1, next neighbouring number on the machine to the right */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          lua_Number nb = nvalue(rb);
          setnvalue(ra, luai_numpeps(nb));
        }
        else {
          Protect(luaG_runerror(L, "Error in " LUA_QS ": number expected, got %s.",
            "++", luaT_typenames[(int)ttype(RB(i))]));
        }
        DISPATCH();
      }
      case OPR_MEPS: {  /* 2.9.1, next neighbouring number on the machine to the left */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          lua_Number nb = nvalue(rb);
          setnvalue(ra, luai_nummeps(nb));
        }
        else {
          Protect(luaG_runerror(L, "Error in " LUA_QS ": number expected, got %s.",
            "--", luaT_typenames[(int)ttype(RB(i))]));
        }
        DISPATCH();
      }
      case OPR_UNITY: {  /* 2.38.3, return only one value, even with function calls */
        TValue *rb = RKB(i);
        Protect(
          setobj2s(L, ra, rb);
        );
        DISPATCH();
      }
      case OPR_CELL: {  /* 2.9.4 */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {  /* retrieve value at given register */
          int cell = nvalue(rb);
          if (cell >= stacktop(L)) {
            setnilvalue(ra);  /* changed 2.12.7 */
            DISPATCH();
          } else if (cell < 0) {
            cell = stacktop(L) + cell;
            if (cell < LUA_CELLSTACKBOTTOM) {
              setnilvalue(ra);  /* changed 2.12.7 */
              DISPATCH();
            }
          }
          if (isnumstack(L)) {  /* 2.12.7 */
            setnvalue(ra, cells(L, cell));
          } else if (iscachestack(L)) {  /* 2.36.3 */
            setobj2s(L, ra, L->C->base + cell - 1);
          } else {
            setsvalue(L, ra, luaS_newlstr(L, getcharcell(L, cell), 1));
          }
        } else {
          Protect(luaG_runerror(L, "Error in " LUA_QS ": number expected, got %s.",
            "cell", luaT_typenames[(int)ttype(rb)]));
        }
        DISPATCH();
      }
      case OPR_PRE: {  /* 2.11.0 RC1, idea taken from Sather, see: https://www.gnu.org/software/sather */
        if (l_isfalse(ra))
          luaG_runerror(L, "Error in %s: invalid input.", "pre-condition");
        else if (!ttisboolean(ra))
          luaG_runerror(L, "Error in %s: invalid argument.", "pre-condition");
        /* do not put something an the stack */
        DISPATCH();
      }
      case OPR_POST: {  /* 2.11.0 RC1, idea taken from Sather, see: https://www.gnu.org/software/sather */
        if (!l_istrue(ra))
          luaG_runerror(L, "Error in %s: invalid return.", "post-condition");
        DISPATCH();
      }
      case OPR_NEGATE: {  /* 2.16.1 */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          lua_Number x = nvalue(rb);
          setnvalue(ra, (x == 0) ? 1 : 0);
        } else if (!ttisboolean(rb)) {
          luaG_runerror(L, "Error in %s: invalid argument.", "negate");
        } else {
          setbvalue(ra, l_istrue(rb) ? 0 : 1);
        }
        DISPATCH();
      }
      case OPR_INVSQRT: {  /* inverse square root, 20 % faster than 1/sqrt(z), 2.16.13 */
        TValue *rb = RKB(i);
        if (ttisnumber(rb)) {
          lua_Number x;
          x = nvalue(rb);
          setnvalue(ra, sqrt(x)/x);
        }
        else if (ttiscomplex(rb)) {
#ifndef PROPCMPLX
          agn_Complex z = cvalue(rb);
          setcvalue(ra, slm_crsqrt(z));
#else
          lua_Number z[2], a, b, c, d;
          a = complexreal(rb); b = compleximag(rb);
          slm_crsqrt(a, b, &c, &d);
          z[0] = c; z[1] = d;
          setcvalue(ra, z);
#endif
        }
        else if (!call_binTM(L, rb, luaO_nilobject, ra, TM_INVSQRT))  /* 3.5.3 change */
          luaG_typeerror(L, rb, "get inverse square root of");
        DISPATCH();
      }
      default:  /* this should not happen */
        luaG_runerror(L, "Error in Virtual Machine: invalid operator %d for OP_FN.", GETARG_C(i));
    }
  }
  LABEL_OP_FNBIN: {  /* for binary operators returning one value; added 0.27.0, 23.08.2009; improved 2.5.2, 03/04/2015
    ! Do not nil values on rb, rc positions in the VM after results have been stored to ra ! */
    TValue *rb = RKB(i);  /* get lhs */
    TValue *rc = rb + 1;  /* get rhs */
    switch(GETARG_C(i)) {
      case OPR_TOFTYPE: {  /* added 1.3.0 */
        const TValue *tm;
        if (ttisuset(rc)) {  /* set of types given ?  2.10.0 */
          const char *r0, *r1;
          int orig_tt = rb->tt;
          lua_Number x = (orig_tt == LUA_TNUMBER) ? nvalue(rb) : AGN_NAN;  /* 5.4.2, rb may change subsequently, so get the correct value now */
          UltraSet *us = usvalue(rc);
          TString *listingtoken = luaS_token(L, TK_TLISTING);  /* 2.26.0 fix */
          if (us->size == 0)  /* new 2.20.1 */
            luaG_runerror(L, "Error in `::` operator: set is empty.");
          if (agnUS_getstr(us, listingtoken)) {  /* 2.26.0 fix */
            agnUS_delstr(L, us, listingtoken);
            agnUS_setstr2set(L, us, luaS_token(L, TK_TTABLE));
            agnUS_setstr2set(L, us, luaS_token(L, TK_TSEQUENCE));
            agnUS_setstr2set(L, us, luaS_token(L, TK_TREGISTER));
          }
          if (us->size > LUAI_NTYPELIST)  /* 2.26.0 fix */
            luaG_runerror(L, "Error in `::` operator: not more than %d types allowed, got %d.", LUAI_NTYPELIST, us->size);
          if (agnUS_getstr(us, luaS_token(L, TK_TANYTHING)))
            luaG_runerror(L, "Error in `::` operator: `anything` not allowed in set.");
          r0 = aux_gettypename(L, rb, 1);  /* with user-defined types */
          r1 = (orig_tt > 4 && orig_tt < 14) ? aux_gettypename(L, rb, 0) : NULL;  /* function, ud, lud, thread, table, seq, reg, pair or set ? */
          if (r0 == NULL)
            luaG_runerror(L, "Error in `::` operator: cannot evaluate metamethods.");
          setsvalue2s(L, rb, luaS_new(L, r0));
          Protect(agenaV_in(L, rb, rc, ra));  /* the boolean result ist stored in ra */
          if (r1 && bvalue(ra) != 1) {  /* try basic types if left operand is a structure, function or ud, 2.20.1 */
            setsvalue2s(L, rb, luaS_new(L, r1));
            Protect(agenaV_in(L, rb, rc, ra));
          } else if (orig_tt == LUA_TNUMBER && bvalue(ra) != 1) {  /* 5.4.2 extension */
            /* grep "(GREP_POINT) types;" if you want to add new types */
            if (agnUS_getstr(us, luaS_token(L, TK_INTEGER))) {
              setbvalue(ra, tools_isint(x));
            } else if (agnUS_getstr(us, luaS_token(L, TK_POSINT))) {
              setbvalue(ra, tools_isposint(x));
            } else if (agnUS_getstr(us, luaS_token(L, TK_NONNEGINT))) {
              setbvalue(ra, tools_isnonnegint(x));
            } else if (agnUS_getstr(us, luaS_token(L, TK_NONZEROINT))) {  /* 4.11.0 */
              setbvalue(ra, tools_isnonzeroint(x));
            } else if (agnUS_getstr(us, luaS_token(L, TK_POSITIVE))) {
              setbvalue(ra, x > 0);
            } else if (agnUS_getstr(us, luaS_token(L, TK_NEGATIVE))) {
              setbvalue(ra, x < 0);
            } else if (agnUS_getstr(us, luaS_token(L, TK_NONNEGATIVE))) {
              setbvalue(ra, x >= 0);
            } else {
              setbvalue(ra, 0);
            }
          }
          DISPATCH();
        }  /* end of ttisuset(rc) */
        else if (ttisfunction(rc)) {  /* new 5.4.0 */
          callTMres(L, ra, rc, rb, rb);
          if (!ttisboolean(ra)) {
            luaG_runerror(L, "Error in " LUA_QS ": function must evaluate to a boolean, got %s.", "::",
            luaT_typenames[(int)ttype(ra)]);
          }
          setbvalue(ra, ttistrue(ra));  /* convert `fail` to `false` */
          DISPATCH();
        }
        else if (!ttisstring(rc)) {
          luaG_runerror(L, "Error in " LUA_QS ": right argument must be a string, set or function, got %s.", "::",  /* 2.12.1 */
            luaT_typenames[(int)ttype(rc)]);
        }
        switch (rb->tt) {
          case LUA_TFUNCTION: {
            Closure *c = clvalue(rb);
            const char *rc_str = svalue(rc);
            if (c->l.type != NULL) {  /* user-defined type ? */
              if (tools_streq(getstr(c->l.type), rc_str) || tools_isanything(rc_str)) { setbvalue(ra, 1); }  /* 2.20.1 change */
              else { setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
            } else if (c->c.type != NULL) {  /* user-defined type ? */
              const char *rc_str = svalue(rc);
              if (tools_streq(getstr(c->c.type), rc_str) || tools_isanything(rc_str)) { setbvalue(ra, 1); }  /* 2.20.1 change */
              else { setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str) ); }  /* check basic type, 2.20.1 */
            } else {
              setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanything(rc_str));  /* 2.16.12 tweak */
            }
            break;
          }
          case LUA_TSEQ: {
            Seq *s = seqvalue(rb);
            if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (s->type == NULL) {  /* basic type */
                setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanyorlist(rc_str)); }  /* 2.16.12 tweak */
              else {  /* user-defined type */
                if (tools_streq(getstr(s->type), rc_str) || tools_isanyorlist(rc_str)) { setbvalue(ra, 1); }
                else { setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str) ); }  /* check basic type, 2.20.1 */
              }
            }
            else if (ttisfunction(tm)) {
              callmetaoftype(ra, rb, rc, s->type);
            }
            break;
          }
          case LUA_TPAIR: {
            Pair *s = pairvalue(rb);
            if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (s->type == NULL) {  /* basic type */
                setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanything(rc_str)); }  /* 2.16.12 tweak */
              else {  /* user-defined type */
                if (tools_streq(getstr(s->type), rc_str) || tools_isanything(rc_str)) { setbvalue(ra, 1); }
                else { setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
              }
            }
            else if (ttisfunction(tm)) {
              callmetaoftype(ra, rb, rc, s->type);
            }
            break;
          }
          case LUA_TTABLE: {
            Table *s = hvalue(rb);
            if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (s->type == NULL) {  /* basic type */
                setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanyorlist(rc_str)); }  /* 2.16.12 tweak */
              else {  /* user-defined type */
                if (tools_streq(getstr(s->type), rc_str) || tools_isanyorlist(rc_str)) { setbvalue(ra, 1); }
                else { setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
              }
            }
            else if (ttisfunction(tm)) {
              callmetaoftype(ra, rb, rc, s->type);
            }
            break;
          }
          case LUA_TSET: {
            UltraSet *s = usvalue(rb);
            if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (s->type == NULL) {  /* basic type */
                setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanything(rc_str)); }  /* 2.16.12 tweak */
              else {  /* user-defined type */
                if (tools_streq(getstr(s->type), rc_str) || tools_isanything(rc_str)) { setbvalue(ra, 1); }
                else { setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
              }
            }
            else if (ttisfunction(tm)) {
              callmetaoftype(ra, rb, rc, s->type);
            }
            break;
          }
          case LUA_TREG: {
            Reg *s = regvalue(rb);
            if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (s->type == NULL) {  /* no user-defined type ? */
                setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanyorlist(rc_str) ); }  /* 2.16.12 tweak */
              else {  /* user-defined type */
                if (tools_streq(getstr(s->type), rc_str) || tools_isanyorlist(rc_str)) { setbvalue(ra, 1); }
                else { setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
              }
            }
            else if (ttisfunction(tm)) {
              callmetaoftype(ra, rb, rc, s->type);
            }
            break;
          }
          case LUA_TUSERDATA: {
            Udata *u = rawuvalue(rb);
            if ((tm = fasttm(L, u->uv.metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (u->uv.type == NULL) {  /* no user-defined type ?  Compare given type with actual type. */
                setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanything(rc_str));  /* 2.16.12 tweak */
              } else {  /* user-defined type */
                if (tools_streq(getstr(u->uv.type), rc_str) || tools_isanything(rc_str)) { setbvalue(ra, 1); }  /* 2.16.12 tweak */
                else { setbvalue(ra, tools_streq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
              }
            }
            else if (ttisfunction(tm)) {
              callmetaoftype(ra, rb, rc, u->uv.type);                }
            break;
          }
          default: {
            const char *tname = svalue(rc);
            if (ttisnumber(rb)) {  /* 2.12.1 */
              if (tools_streq(tname, llex_token2str(TK_TNUMBER)) || tools_isanything(tname) || tools_isbasic(tname)) {  /* 2.16.12 tweak, ext. 2.20.1 */
                setbvalue(ra, 1);
              } else {
                /* grep "(GREP_POINT) types;" if you want to add new types */
                lua_Number x = nvalue(rb);
                if (tools_streq(tname, llex_token2str(TK_INTEGER))) {  /* 2.14.13 change, 2.16.12/4.5.2 tweak */
                  setbvalue(ra, tools_isint(x));
                } else if (tools_streq(tname, llex_token2str(TK_POSINT))) {  /* 2.16.12/4.5.2 tweak */
                  setbvalue(ra, tools_isposint(x));
                } else if (tools_streq(tname, llex_token2str(TK_NONNEGINT))) {  /* 2.16.12/4.5.2 tweak */
                  setbvalue(ra, tools_isnonnegint(x));
                } else if (tools_streq(tname, llex_token2str(TK_NONZEROINT))) {  /* 4.11.0 */
                  setbvalue(ra, tools_isnonzeroint(x));
                } else if (tools_streq(tname, llex_token2str(TK_POSITIVE))) {  /* 2.16.12/4.5.2 tweak */
                  setbvalue(ra, x > 0);
                } else if (tools_streq(tname, llex_token2str(TK_NEGATIVE))) {  /* 3.3.6/4.5.2 */
                  setbvalue(ra, x < 0);
                } else if (tools_streq(tname, llex_token2str(TK_NONNEGATIVE))) {  /* 2.16.12/4.5.2 tweak */
                  setbvalue(ra, x >= 0);
                } else {
                  setbvalue(ra, 0);
                }
              }
            } else if (tools_isanything(tname)) {  /* new 2.20.1 */
              setbvalue(ra, 1);
            } else if (tools_isbasic(tname)) {  /* new 2.21.0 */
              setbvalue(ra, ttisstring(rb) || ttisboolean(rb) || ttisnil(rb));
            } else {
              setbvalue(ra, tools_streq(luaT_typenames[rb->tt], tname));  /* 2.16.12 tweak */
            }
          }
        }
        DISPATCH();
      }
      case OPR_TNOTOFTYPE: {  /* added 1.3.0 */
        const TValue *tm;
        if (ttisuset(rc)) {  /* set of types given ?  2.10.0 */
          const char *r0, *r1;
          int orig_tt = rb->tt;
          lua_Number x = (orig_tt == LUA_TNUMBER) ? nvalue(rb) : AGN_NAN;  /* 5.4.2, rb may change subsequently, so get the correct value now */
          UltraSet *us = usvalue(rc);
          TString *listingtoken = luaS_token(L, TK_TLISTING);  /* 2.26.0 fix */
          if (us->size == 0)  /* new 2.20.1 */
            luaG_runerror(L, "Error in `:-` operator: set is empty.");
          orig_tt = rb->tt;
          if (agnUS_getstr(us, listingtoken)) {  /* 2.26.0 fix */
            agnUS_delstr(L, us, listingtoken);
            agnUS_setstr2set(L, us, luaS_token(L, TK_TTABLE));
            agnUS_setstr2set(L, us, luaS_token(L, TK_TSEQUENCE));
            agnUS_setstr2set(L, us, luaS_token(L, TK_TREGISTER));
          }
          if (us->size > LUAI_NTYPELIST)  /* 2.26.0 fix */
            luaG_runerror(L, "Error in `:-` operator: not more than %d types allowed, got %d.", LUAI_NTYPELIST, us->size);
          if (agnUS_getstr(us, luaS_token(L, TK_TANYTHING)))
            luaG_runerror(L, "Error in `:-` operator: `anything` not allowed in set.");
          r0 = aux_gettypename(L, rb, 1);  /* with user-defined types */
          r1 = (orig_tt > 4 && orig_tt < 14) ? aux_gettypename(L, rb, 0) : NULL;  /* without */
          if (r0 == NULL)
            luaG_runerror(L, "Error in `:-` operator: cannot evaluate metamethods.");
          setsvalue2s(L, rb, luaS_new(L, r0));
          Protect(agenaV_in(L, rb, rc, ra));  /* the boolean result ist stored in ra */
          setbvalue(ra, !bvalue(ra));  /* negate result */
          if (r1 && bvalue(ra) == 1) {  /* try basic types if left operand is a structure, function or ud, 2.20.1 */
            setsvalue2s(L, rb, luaS_new(L, r1));
            Protect(agenaV_in(L, rb, rc, ra));
            setbvalue(ra, !bvalue(ra));  /* negate result */
          } else if (orig_tt == LUA_TNUMBER && bvalue(ra) == 1) {  /* 5.4.2 extension */
            /* grep "(GREP_POINT) types;" if you want to add new types */
            if (agnUS_getstr(us, luaS_token(L, TK_INTEGER))) {
              setbvalue(ra, !tools_isint(x));
            } else if (agnUS_getstr(us, luaS_token(L, TK_POSINT))) {
              setbvalue(ra, !tools_isposint(x));
            } else if (agnUS_getstr(us, luaS_token(L, TK_NONNEGINT))) {
              setbvalue(ra, !tools_isnonnegint(x));
            } else if (agnUS_getstr(us, luaS_token(L, TK_NONZEROINT))) {  /* 4.11.0 */
              setbvalue(ra, !tools_isnonnegint(x));
            } else if (agnUS_getstr(us, luaS_token(L, TK_POSITIVE))) {
              setbvalue(ra, x <= 0);
            } else if (agnUS_getstr(us, luaS_token(L, TK_NEGATIVE))) {
              setbvalue(ra, x >= 0);
            } else if (agnUS_getstr(us, luaS_token(L, TK_NONNEGATIVE))) {
              setbvalue(ra, x < 0);
            } else {
              setbvalue(ra, 1);
            }
          }
          DISPATCH();
        }
        else if (ttisfunction(rc)) {  /* new 5.4.0 */
          callTMres(L, ra, rc, rb, rb);
          if (!ttisboolean(ra)) {
            luaG_runerror(L, "Error in " LUA_QS ": function must evaluate to a boolean, got %s.", ":-",  /* 2.12.1 */
            luaT_typenames[(int)ttype(ra)]);
          }
          setbvalue(ra, !ttistrue(ra));  /* convert `fail` to `false` */
          DISPATCH();
        }
        else if (!ttisstring(rc)) {
          luaG_runerror(L, "Error in " LUA_QS ": right argument must be a string, got %s.", ":-",
            luaT_typenames[(int)ttype(rc)]);
        }
        switch (rb->tt) {
          case LUA_TFUNCTION: {
            Closure *c = clvalue(rb);
            const char *rc_str = svalue(rc);
            if (c->l.type != NULL) {  /* user-defined type ? */
              if (tools_streq(getstr(c->l.type), rc_str) || tools_isanything(rc_str)) { setbvalue(ra, 0); }  /* 2.20.1 change */
              else { setbvalue(ra, tools_strneq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
            } else if (c->c.type != NULL) {  /* user-defined type ? */
              const char *rc_str = svalue(rc);
              if (tools_streq(getstr(c->c.type), rc_str) || tools_isanything(rc_str)) { setbvalue(ra, 0); }  /* 2.20.1 change */
              else { setbvalue(ra, tools_strneq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
            } else {
              setbvalue(ra, !(tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanything(rc_str)));  /* 2.16.12 tweak */
            }
            break;
          }
          case LUA_TSEQ: {
            Seq *s = seqvalue(rb);
            if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (s->type == NULL) {
                setbvalue(ra, !(tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanyorlist(rc_str))); }  /* 2.16.12 tweak */
              else {  /* user-defined type */
                if (tools_streq(getstr(s->type), rc_str) || tools_isanyorlist(rc_str)) { setbvalue(ra, 0); }
                else { setbvalue(ra, tools_strneq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
              }
            }
            else if (ttisfunction(tm)) {
              callmetanotoftype(ra, rb, rc, s->type);                }
            break;
          }
          case LUA_TPAIR: {
            Pair *s = pairvalue(rb);
            if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (s->type == NULL) {
                setbvalue(ra, !(tools_streq(luaT_typenames[rb->tt], svalue(rc)) || tools_isanything(rc_str))); }  /* 2.16.12 tweak */
              else {  /* user-defined type */
                if (tools_streq(getstr(s->type), rc_str) || tools_isanything(rc_str)) { setbvalue(ra, 0); }
                else { setbvalue(ra, tools_strneq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
              }
            }
            else if (ttisfunction(tm)) {
              callmetanotoftype(ra, rb, rc, s->type);
            }
            break;
          }
          case LUA_TTABLE: {
            Table *s = hvalue(rb);
            if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (s->type == NULL) {
                setbvalue(ra, !(tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanyorlist(rc_str))); }  /* 2.16.12 tweak */
              else {  /* user-defined type */
                if (tools_streq(getstr(s->type), rc_str) || tools_isanyorlist(rc_str)) { setbvalue(ra, 0); }
                else { setbvalue(ra, tools_strneq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
              }
            } else if (ttisfunction(tm)) {
              callmetanotoftype(ra, rb, rc, s->type);
            }
            break;
          }
          case LUA_TSET: {
            UltraSet *s = usvalue(rb);
            if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (s->type == NULL) {
                setbvalue(ra, !(tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanything(rc_str))); }  /* 2.16.12 tweak */
              else {  /* user-defined type */
                if (tools_streq(getstr(s->type), rc_str) || tools_isanything(rc_str)) { setbvalue(ra, 0); }
                else { setbvalue(ra, tools_strneq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
              }
            } else if (ttisfunction(tm)) {
              callmetanotoftype(ra, rb, rc, s->type);
            }
            break;
          }
          case LUA_TREG: {
            Reg *s = regvalue(rb);
            if ((tm = fasttm(L, s->metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (s->type == NULL) {
                setbvalue(ra, !(tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanyorlist(rc_str))); }  /* 2.16.12 tweak */
              else {  /* user-defined type */
                if (tools_streq(getstr(s->type), rc_str) || tools_isanyorlist(rc_str)) { setbvalue(ra, 0); }
                else { setbvalue(ra, tools_strneq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
              }
            } else if (ttisfunction(tm)) {
              callmetanotoftype(ra, rb, rc, s->type);
            }
            break;
          }
          case LUA_TUSERDATA: {
            Udata *u = rawuvalue(rb);
            if ((tm = fasttm(L, u->uv.metatable, TM_OFTYPE)) == NULL) {  /* no metamethod ? */
              const char *rc_str = svalue(rc);
              if (u->uv.type == NULL) {  /* no user-defined type ?  Compare given type with actual type. */
                setbvalue(ra, !(tools_streq(luaT_typenames[rb->tt], rc_str) || tools_isanything(rc_str)));  /* 2.16.12 tweak */
              } else {  /* user-defined type */
                if (tools_streq(getstr(u->uv.type), rc_str) || tools_isanything(rc_str)) { setbvalue(ra, 0); }  /* 2.16.12 tweak */
                else { setbvalue(ra, tools_strneq(luaT_typenames[rb->tt], rc_str)); }  /* check basic type, 2.20.1 */
              }
            } else if (ttisfunction(tm)) {
              callmetanotoftype(ra, rb, rc, u->uv.type);  /* 5.1.6 fix */
            }
            break;
          }
          default: {
            const char *tname = svalue(rc);
            if (ttisnumber(rb)) {  /* 2.12.1 */
              if (tools_streq(tname, llex_token2str(TK_TNUMBER)) || tools_streq(tname, llex_token2str(TK_TANYTHING)) || tools_streq(tname, llex_token2str(TK_TBASIC))) {
                /* 2.16.12 tweak, ext. 2.20.1 */
                setbvalue(ra, 0);
              } else {
                lua_Number x = nvalue(rb);
                /* grep "(GREP_POINT) types;" if you want to add new types */
                if (tools_streq(tname, llex_token2str(TK_INTEGER))) {  /* 2.14.13 change, 2.16.12/4.5.2 tweak */
                  setbvalue(ra, !tools_isint(x));
                } else if (tools_streq(tname, llex_token2str(TK_POSINT))) {  /* 2.16.12/4.5.2 tweak */
                  setbvalue(ra, !tools_isposint(x));
                } else if (tools_streq(tname, llex_token2str(TK_NONNEGINT))) {  /* 2.16.12/4.5.2 tweak */
                  setbvalue(ra, !tools_isnonnegint(x));
                } else if (tools_streq(tname, llex_token2str(TK_NONZEROINT))) {  /* 4.11.0 */
                  setbvalue(ra, !tools_isnonzeroint(x));
                } else if (tools_streq(tname, llex_token2str(TK_POSITIVE))) {  /* 2.16.12/4.5.2 tweak */
                  setbvalue(ra, x <= 0);
                } else if (tools_streq(tname, llex_token2str(TK_NEGATIVE))) {  /* 3.3.6/4.5.2 */
                  setbvalue(ra, x >= 0);
                } else if (tools_streq(tname, llex_token2str(TK_NONNEGATIVE))) {  /* 2.16.12/4.5.2 tweak */
                  setbvalue(ra, x < 0);
                } else {
                  setbvalue(ra, 1);
                }
              }
            } else if (tools_streq(tname, llex_token2str(TK_TANYTHING)) || tools_streq(tname, llex_token2str(TK_TBASIC))) {  /* new 2.20.1 */
              setbvalue(ra, 0);
            } else if (tools_streq(tname, llex_token2str(TK_TLISTING))) {  /* new 2.20.1 */
              setbvalue(ra, !(rb->tt == LUA_TTABLE || rb->tt == LUA_TSEQ || rb->tt == LUA_TREG));
            } else
              setbvalue(ra, tools_strneq(luaT_typenames[rb->tt], tname));  /* 2.16.12 tweak */
          }
        }
        DISPATCH();
      }
      case OPR_XOR: {  /* added 0.27.0 */
        Protect(agenaV_xor(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_ATENDOF: {  /* added 0.27.0 */
        Protect(agenaV_atendof(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_BAND: {  /* added 0.27.0 */
        Protect(agenaV_band(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_BOR: {  /* added 0.27.0 */
        Protect(agenaV_bor(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_BXOR: {  /* `^^`, added 0.27.0 */
        Protect(agenaV_bxor(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_PERCENT: {  /* added 1.10.6 */
        Protect(agenaV_percent(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_PERCENTRATIO: {  /* added 1.11.4 */
        Protect(agenaV_percentratio(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_PERCENTADD: {  /* added 1.11.3 */
        Protect(agenaV_percentadd(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_PERCENTSUB: {  /* added 1.11.3 */
        Protect(agenaV_percentsub(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_PERCENTCHANGE: {  /* added 2.10.0 */
        Protect(agenaV_percentchange(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_MAP: {  /* added 2.2.0 */
        Protect(agenaV_map(L, rb, rc, ra); luaC_checkGC(L););
        DISPATCH();
      }
      case OPR_SELECT: {  /* added 2.2.5 */
        Protect(agenaV_select(L, rb, rc, ra); luaC_checkGC(L););
        DISPATCH();
      }
      case OPR_HAS: {  /* added 2.22.2 */
        Protect(agenaV_has(L, rb, rc, ra); luaC_checkGC(L););
        DISPATCH();
      }
      case OPR_COUNT: {  /* added 4.6.0 */
        Protect(agenaV_count(L, rb, rc, ra); luaC_checkGC(L););
        DISPATCH();
      }
      case OPR_BLEFT: {  /* `<<` operator, added 2.3.0 RC 4 */
        if (!(ttisnumber(rb) && ttisnumber(rc))) {
          if (!call_binTM(L, rb, rc, ra, TM_BSHL))  /* 2.4.6 fix/4.8.0 extension */
            luaG_runerror(L, "Error in " LUA_QS ": calling metamethod failed.", "<<");
        } else {
          if (L->settings & 1) {  /* signed bits operation ? */
            LUA_INT32 a, b;  /* 2.9.0, extended to 64-bit integers 2.10.0 */
            a = (LUA_INT32)tools_reducerange(nvalue(rb), -2147483648.0, 2147483648.0);  /* new 4.12.5 */
            b = nvalue(rc);
            if (b >= LUA_NBITS) { setnvalue(ra, 0); }  /* 2.5.0 RC 3 */
            else { setnvalue(ra, a << b); }
          } else {  /* unsigned mode */
            LUA_UINT32 a, b;  /* 2.9.0, extended to 64-bit integers 2.10.0, changed back to 32-bit operation 2.15.0 */
            a = (LUA_UINT32)tools_reducerange(nvalue(rb), 0, 4294967296.0);
            b = nvalue(rc);
            if (b >= LUA_NBITS) { setnvalue(ra, 0); }  /* 2.5.0 RC 3 */
            else {
              a <<= b;  /* 5.0.2 portability change */
              setnvalue(ra, trim(a));
            }
          }
        }
        DISPATCH();
      }
      case OPR_BRIGHT: {  /* `>>` operator, added 2.3.0 RC 4 */
        if (!(ttisnumber(rb) && ttisnumber(rc))) {
          if (!call_binTM(L, rb, rc, ra, TM_BSHR))  /* 2.4.6 fix/4.8.0 extension */
            luaG_runerror(L, "Error in " LUA_QS ": calling metamethod failed.", ">>");
        } else {
          if (L->settings & 1) {  /* signed bits operation ? */
            LUA_INT32 a, b;  /* 2.9.0, extended to 64-bit integers 2.10.0, changed back to 32-bit operation 2.15.0 */
            a = (LUA_INT32)tools_reducerange(nvalue(rb), -2147483648.0, 2147483648.0);  /* new 4.12.5 */
            b = nvalue(rc);
            if (b >= LUA_NBITS) { setnvalue(ra, 0); }   /* 2.5.0 RC 3 */
            else { setnvalue(ra, tools_sar32(a, b)); }  /* force arithmetic right shift on all platforms, like in Visual C, 2.31.0 */
          } else {  /* unsigned mode */
            LUA_UINT32 a, b;  /* 2.9.0, extended to 64-bit integers 2.10.0, changed back to 32-bit operation 2.15.0 */
            a = (LUA_UINT32)tools_reducerange(nvalue(rb), 0, 4294967296.0);
            b = nvalue(rc);
            if (b >= LUA_NBITS) { setnvalue(ra, 0); }  /* 2.5.0 RC 3 */
            else {
              a >>= b;  /* 5.0.2 portability change */
              setnvalue(ra, trim(a));
            }
          }
        }
        DISPATCH();
      }
      case OPR_LBROTATE: {  /* `<<<` operator, added 2.5.4 */
        if (!(ttisnumber(rb) && ttisnumber(rc))) {
          if (!call_binTM(L, rb, rc, ra, TM_BROTL))  /* 6.1.4 extension */
            luaG_runerror(L, "Error in " LUA_QS ": calling metamethod failed.", "<<<");
        } else if (L->settings & 1) {  /* signed bits operation ? */
          setnvalue(ra, luaV_brotatesigned((LUA_INT32)tools_reducerange(nvalue(rb), -2147483648.0, 2147483648.0), (int)nvalue(rc)));  /* new 4.12.5 */
        } else {  /* unsigned mode */
          setnvalue(ra, luaV_brotateunsigned((LUA_UINT32)tools_reducerange(nvalue(rb), 0, 4294967296.0), (unsigned int)nvalue(rc)));  /* new 4.12.5 */
        }
        DISPATCH();
      }
      case OPR_RBROTATE: {  /* `>>>` operator, added 2.5.4 */
        if (!(ttisnumber(rb) && ttisnumber(rc))) {
          if (!call_binTM(L, rb, rc, ra, TM_BROTR))  /* 6.1.4 extension */
            luaG_runerror(L, "Error in " LUA_QS ": calling metamethod failed.", ">>>");
        } else if (L->settings & 1) {  /* signed bits operation ? */
          setnvalue(ra, luaV_brotatesigned((LUA_INT32)tools_reducerange(nvalue(rb), -2147483648.0, 2147483648.0), -(int)nvalue(rc)));  /* new 4.12.5 */
        } else {  /* unsigned mode */
          setnvalue(ra, luaV_brotateunsigned((LUA_UINT32)tools_reducerange(nvalue(rb), 0, 4294967296.0), -(unsigned int)nvalue(rc)));  /* new 4.12.5 */
        }
        DISPATCH();
      }
      case OPR_NAEQ: {  /* 2.5.2, approximate inequality check (`~<>' operator) */
        if (ttype(rb) == ttype(rc)) {
          setbvalue(ra, !luaV_approxequalvalonebyone(L, rb, rc));
        } else if (ttisnumber(rb) && ttiscomplex(rc)) {  /* 2.32.5 patch */
          lua_Number re = complexreal(rc);
          lua_Number im = compleximag(rc);
          if (im == 0) {
            setbvalue(ra, !tools_approx(re, nvalue(rb), L->Eps));
          } else {
            setbvalue(ra, !(tools_approx(nvalue(rb), re, L->Eps) &&
                            tools_approx(0, im, L->Eps)));
          }
        } else if (ttiscomplex(rb) && ttisnumber(rc)) {  /* 2.32.5 patch */
          lua_Number re = complexreal(rb);
          lua_Number im = compleximag(rb);
          if (im == 0) {
            setbvalue(ra, !tools_approx(re, nvalue(rc), L->Eps));
          } else {
            setbvalue(ra, !(tools_approx(nvalue(rc), re, L->Eps) &&
                            tools_approx(0, im, L->Eps)));
          }
        } else {
          setbvalue(ra, 1);
        }
        DISPATCH();
      }
      case OPR_NAND: {  /* added 2.5.2 */
        Protect(agenaV_nand(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_NOR: {  /* added 2.5.2 */
        Protect(agenaV_nor(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_XNOR: {  /* added 2.8.5 */
        Protect(agenaV_xnor(L, ra, rb, rc));
        DISPATCH();
      }
      case OPR_COMPARE: {  /* `|' binary op, 2.9.4 */
        lua_Number a, b;
        if ((ttisnumber(rb) && ttisnumber(rc))) {
          a = nvalue(rb); b = nvalue(rc);
          if (a < b) {
            setnvalue(ra, -1);
          } else if (a == b) {
            setnvalue(ra, 0);
          } else if (a > b) {
            setnvalue(ra, 1);
          } else {  /* at least one operand is not finite */
            setnvalue(ra, AGN_NAN);
          }
        } else if (ttisstring(rb) && ttisstring(rc)) {  /* 6.0.0 */
          const char *s1 = getstr(rawtsvalue(rb));
          const char *s2 = getstr(rawtsvalue(rc));
          int rc = tools_strverscmp(s1, s2);
          if (rc < 0) {
            setnvalue(ra, -1);
          } else if (rc == 0) {
            setnvalue(ra, 0);
          } else {
            setnvalue(ra, 1);
          }
        } else {
          luaG_runerror(L, "Error in " LUA_QS ": expected either numbers or strings, got %s, %s.", "|",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        }
        DISPATCH();
      }
      case OPR_ACOMPARE: {  /* `~|'  approximate comparison operator, 2.11.0 RC1 */
        lua_Number a, b, delta, diff;
        int e;
        if (!(ttisnumber(rb) && ttisnumber(rc)))
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "~|",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        a = nvalue(rb); b = nvalue(rc);
        /* exponent, allows to reliably compare to zero, too */
        sun_frexp((fabs(a) > fabs(b)) ? a : b, &e);  /* 2.11.1 tuning, 15 % faster */
        delta = sun_ldexp(L->Eps, e);
        diff = a - b;  /* difference */
        /* the following correctly treats +/- infinity, i.e. evalb(+/- infinity = +/- infinity) -> true */
        if (diff > delta) {  /* a > b */
          setnvalue(ra, 1);
        } else if (diff < -delta) {  /* a < b */
          setnvalue(ra, -1);
        } else {  /* a ~= b <=> -delta <= diff <= delta */
          setnvalue(ra, 0);
        }
        DISPATCH();
      }
      case OPR_SYMMOD: {  /* added 2.10.0 */
        if (!(ttisnumber(rb) && ttisnumber(rc)))
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "symmod",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        setnvalue(ra, fmod(nvalue(rb), nvalue(rc)));
        DISPATCH();
      }
      case OPR_ROLL: {  /* added 2.13.0, formerly `rot` baselib function, ten percent faster */
        /* 2.8.3, Rotates a two-dimensional vector, represented by the complex number z, through the angel r (given in
           radians) counterclockwise and returns a new complex number. To convert degrees to radians, multiply
           by Pi/180. If z is just a number, it is internally converted to the complex number z + 0*I. */
        lua_Number r, x, y, c, s;
        if (!(ttisnumber(rb) || ttiscomplex(rb)))
          luaG_runerror(L, "Error in " LUA_QS ": left-hand side must be a (complex) number, got %s.", "rotate",
            luaT_typenames[(int)ttype(rb)]);  /* 3.7.8 improvement */
        if (!(ttisnumber(rc)))
          luaG_runerror(L, "Error in " LUA_QS ": right-hand side must be a number, got %s.", "rotate",
            luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        r = nvalue(rc);
        sun_sincos(r, &s, &c);  /* 2.11.1 tuning */
        if (ttisnumber(rb)) {
          x = nvalue(rb); y = 0.0;
        } else  {
          TValue *rx = s2v(rb);
          x = complexreal(rx); y = compleximag(rx);
        }
        setrealimagvalue(ra, x*c - y*s, x*s + y*c);
        DISPATCH();
      }
      case OPR_I32ADD: {  /* added 2.15.0 */
        if (!(ttisnumber(rb) && ttisnumber(rc)))
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "&+",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        if (L->settings & 1) {  /* signed operation ? */
          setnvalue(ra, (LUA_INT32)((LUA_INT32)nvalue(rb) + (LUA_INT32)nvalue(rc)));
        } else {
          setnvalue(ra, (LUA_UINT32)((LUA_UINT32)nvalue(rb) + (LUA_UINT32)nvalue(rc)));
        }
        DISPATCH();
      }
      case OPR_I32SUB: {  /* added 2.15.0 */
        if (!(ttisnumber(rb) && ttisnumber(rc)))
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "&-",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        if (L->settings & 1) {  /* signed operation ? */
          setnvalue(ra, (LUA_INT32)((LUA_INT32)nvalue(rb) - (LUA_INT32)nvalue(rc)));
        } else {
          setnvalue(ra, (LUA_UINT32)((LUA_UINT32)nvalue(rb) - (LUA_UINT32)nvalue(rc)));
        }
        DISPATCH();
      }
      case OPR_I32MUL: {  /* added 2.15.0 */
        if (!(ttisnumber(rb) && ttisnumber(rc)))
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "&*",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        if (L->settings & 1) {  /* signed operation ? */
          setnvalue(ra, (LUA_INT32)((LUA_INT32)nvalue(rb) * (LUA_INT32)nvalue(rc)));
        } else {
          setnvalue(ra, (LUA_UINT32)((LUA_UINT32)nvalue(rb) * (LUA_UINT32)nvalue(rc)));
        }
        DISPATCH();
      }
      case OPR_I32DIV: {  /* added 2.15.0 */
        if (!(ttisnumber(rb) && ttisnumber(rc)))
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "&/",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        if (L->settings & 1) {  /* signed operation ? */
          setnvalue(ra, (LUA_INT32)((LUA_INT32)nvalue(rb) / (LUA_INT32)nvalue(rc)));
        } else {
          setnvalue(ra, (LUA_UINT32)((LUA_UINT32)nvalue(rb) / (LUA_UINT32)nvalue(rc)));
        }
        DISPATCH();
      }
      case OPR_SQUAREADD: {  /* added 2.16.13 */
        if (ttisnumber(rb) && ttisnumber(rc)) {
          lua_Number z = nvalue(rb);
          setnvalue(ra, fma(z, z, nvalue(rc)));
        } else if (ttiscomplex(rb) && ttiscomplex(rc)) {
          lua_Number a, b, p, q;
          TValue *rx = s2v(rb);
          TValue *ry = s2v(rc);
          a = complexreal(rx); b = compleximag(rx); p = complexreal(ry); q = compleximag(ry);
          /* non-fma version is only slightly faster; 2.40.2 accuracy extension */
          setrealimagvalue(ra, fma(a, a, -fma(b, b, -p)), fma(a, b, fma(b, a, q)));
        } else if (ttisnumber(rb) && ttiscomplex(rc)) {  /* 2.31.2 extension */
          lua_Number a, p, q;
          TValue *rx = s2v(rc);
          a = nvalue(rb); p = complexreal(rx); q = compleximag(rx);
          setrealimagvalue(ra, fma(a, a, p), q);
        } else if (ttiscomplex(rb) && ttisnumber(rc)) {  /* 2.31.2 extension */
          lua_Number a, b, p;
          TValue *rx = s2v(rb);
          a = complexreal(rx); b = compleximag(rx); p = nvalue(rc);
          /* non-fma version is only slightly faster; 2.40.2 accuracy extension */
          setrealimagvalue(ra, fma(a, a, -fma(b, b, -p)), 2.0*a*b);
        } else if (!call_binTM(L, rb, rc, ra, TM_SQUAREADD))  /* 2.34.6 fix */
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be (complex) numbers, got %s, %s.", "squareadd",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        DISPATCH();
      }
      case OPR_NOTIN: {
        Protect(agenaV_notin(L, rb, rc, ra));
        DISPATCH();
      }
      case OPR_MULTIPLY: {  /* 2.32.0/1, `mul` statement */
        if (ttisnumber(rb) && ttisnumber(rc)) {
          int num, den, max, value;
          ptrdiff_t res;
          lua_Number n = nvalue(rb);
          lua_Number d = nvalue(rc);
          L->arithstate = 0;  /* reset */
          sun_frexp(DBL_MAX, &max);
          sun_frexp(n, &num);
          sun_frexp(d, &den);
          /* log2(numerator) - log2(denominator) > log2(environ.system().numberranges.maxdouble), i.e. a very large value is to be multiplied
             by a very small value close to 0 */
          value = (num - den > max);
          setarithstate(L, value, 2);
          /* log2(numerator) + log2(denominator) > log2(environ.system().numberranges.maxdouble), i.e. a very large value is to be multiplied
             by a very large value; see: https://stackoverflow.com/questions/18234311/how-close-to-division-by-zero-can-i-get */
          if (!value) {
            value = (num + den > max);
            setarithstate(L, value, 4);
          }
          if (!value) {
            /* are both operands close to zero ? */
            value = fabs(n) < L->div_closetozero && fabs(d) < L->div_closetozero;
            setarithstate(L, value, 8);
          }
          /* put math.multiply function on stack and execute it */
          arithexceptionfncallandreturn(L, ra, res, n, d, "mul", "multiply");
        } else
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "mul",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        DISPATCH();
      }
      case OPR_DIVIDE: {  /* 2.32.0/1, `div` statement */
        if (ttisnumber(rb) && ttisnumber(rc)) {
          int value;
          ptrdiff_t res;
          lua_Number n = nvalue(rb);
          lua_Number d = nvalue(rc);
          L->arithstate = 0;  /* reset */
          value = (d == 0);
          /* denominator is zero ? */
          setarithstate(L, value, 1);
          if (!value) {
            int num, den, max;
            sun_frexp(DBL_MAX, &max);
            sun_frexp(n, &num);
            sun_frexp(d, &den);
            /* log2(numerator) - log2(denominator) > log2(environ.system().numberranges.maxdouble), i.e. a very large value is to be divided
               by a very small value close to 0; see: https://stackoverflow.com/questions/18234311/how-close-to-division-by-zero-can-i-get */
            value = (num - den > max);
            setarithstate(L, value, 2);
          }
          if (!value) {
            /* are both numerator and denominator close to zero ? */
            value = fabs(n) < L->div_closetozero && fabs(d) < L->div_closetozero;
            setarithstate(L, value, 8);
          }
          /* put math.divide function on stack and execute it */
          arithexceptionfncallandreturn(L, ra, res, n, d, "div", "divide");
        } else
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "div",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        DISPATCH();
      }
      case OPR_INTDIVIDE: {  /* 2.32.0/1 */
        if (ttisnumber(rb) && ttisnumber(rc)) {
          int value;
          ptrdiff_t res;
          lua_Number n = nvalue(rb);
          lua_Number d = nvalue(rc);
          L->arithstate = 0;  /* reset */
          value = (d == 0);
          /* denominator is zero ? */
          setarithstate(L, value, 1);
          if (!value) {
            int num, den, max;
            sun_frexp(DBL_MAX, &max);
            sun_frexp(n, &num);
            sun_frexp(d, &den);
            /* log2(numerator) - log2(denominator) > log2(environ.system().numberranges.maxdouble), i.e. a very large value is to be divided
               by a very small value close to 0; see: https://stackoverflow.com/questions/18234311/how-close-to-division-by-zero-can-i-get */
            value = (num - den > max);
            setarithstate(L, value, 2);
          }
          if (!value) {
            /* are both numerator and denominator close to zero ? */
            value = fabs(n) < L->div_closetozero && fabs(d) < L->div_closetozero;
            setarithstate(L, value, 8);
          }
          /* put math.intdivide function on stack and execute it */
          arithexceptionfncallandreturn(L, ra, res, n, d, "intdiv", "intdivide");
        } else
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "intdiv",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        DISPATCH();
      }
      case OPR_MODULUS: {  /* 2.32.0/1 */
        if (ttisnumber(rb) && ttisnumber(rc)) {
          int value;
          ptrdiff_t res;
          lua_Number n = nvalue(rb);
          lua_Number d = nvalue(rc);
          L->arithstate = 0;  /* reset */
          value = (d == 0);
          /* denominator is zero ? */
          setarithstate(L, value, 1);
          if (!value) {
            int num, den, max;
            sun_frexp(DBL_MAX, &max);
            sun_frexp(n, &num);
            sun_frexp(d, &den);
            /* log2(numerator) - log2(denominator) > log2(environ.system().numberranges.maxdouble), i.e. a very large value is to be divided
               by a very small value close to 0; see: https://stackoverflow.com/questions/18234311/how-close-to-division-by-zero-can-i-get */
            value = (num - den > max);
            setarithstate(L, value, 2);
          }
          if (!value) {
            /* are both numerator and denominator close to zero ? */
            value = fabs(n) < L->div_closetozero && fabs(d) < L->div_closetozero;
            setarithstate(L, value, 8);
          }
          /* put math.intdivide function on stack */
          arithexceptionfncallandreturn(L, ra, res, n, d, "mod", "modulo");
        } else
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "mod",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        DISPATCH();
      }
      case OPR_INC: {  /* 2.32.0/1 */
        if (ttisnumber(rb) && ttisnumber(rc)) {
          int num, den, max, value;
          ptrdiff_t res;
          lua_Number n = nvalue(rb);
          lua_Number d = nvalue(rc);
          L->arithstate = 0;  /* reset */
          sun_frexp(DBL_MAX, &max);
          sun_frexp(n, &num);
          sun_frexp(d, &den);
          /* log2(numerator) - log2(denominator) > log2(environ.system().numberranges.maxdouble), i.e. a very large value is to be added
             to a very small value close to 0 */
          value = (num - den > max);
          setarithstate(L, value, 2);
          /* log2(numerator) + log2(denominator) > log2(environ.system().numberranges.maxdouble), i.e. a very large value is to be added
             by a very large value; see: https://stackoverflow.com/questions/18234311/how-close-to-division-by-zero-can-i-get */
          if (!value) {
            value = (num + den > max);
            setarithstate(L, value, 4);
          }
          if (!value) {
            /* are both operands close to zero ? */
            value = fabs(n) < L->div_closetozero && fabs(d) < L->div_closetozero;
            setarithstate(L, value, 8);
          }
          /* put math.add function on stack */
          arithexceptionfncallandreturn(L, ra, res, n, d, "inc", "add");
        } else
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "inc",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        DISPATCH();
      }
      case OPR_DEC: {  /* 2.32.0/1 */
        if (ttisnumber(rb) && ttisnumber(rc)) {
          int num, den, max, value;
          ptrdiff_t res;
          lua_Number n = nvalue(rb);
          lua_Number d = nvalue(rc);
          L->arithstate = 0;  /* reset */
          sun_frexp(DBL_MAX, &max);
          sun_frexp(n, &num);
          sun_frexp(d, &den);
          /* is this of any concern ??? log2(numerator) + log2(denominator) > log2(environ.system().numberranges.maxdouble), i.e. a very
             large value is to be subtracted from a very large value;
             see: https://stackoverflow.com/questions/18234311/how-close-to-division-by-zero-can-i-get */
          value = (num - den > max);
          setarithstate(L, value, 2);
          /* log2(numerator) + log2(denominator) > log2(environ.system().numberranges.maxdouble), i.e. a very large value is to be added
             by a very large value; see: https://stackoverflow.com/questions/18234311/how-close-to-division-by-zero-can-i-get */
          if (!value) {
            value = (num + den > max);
            setarithstate(L, value, 4);
          }
          if (!value) {
            /* are both operands close to zero ? */
            value = fabs(n) < L->div_closetozero && fabs(d) < L->div_closetozero;
            setarithstate(L, value, 8);
          }
          /* put math.subtract function on stack */
          arithexceptionfncallandreturn(L, ra, res, n, d, "dec", "subtract");
        } else
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "dec",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
        DISPATCH();
      }
      case OPR_CARTESIAN: {  /* added 3.10.5 */
        if (ttisnumber(rb) && ttisnumber(rc)) {
          lua_Number x, y, si, co;
          x = nvalue(rb);  /* magnitude */
          y = nvalue(rc);  /* argument */
          sun_sincos(y, &si, &co);
          setrealimagvalue(ra, x*co, x*si);
        } else
          luaG_runerror(L, "Error in " LUA_QS ": both operands must be numbers, got %s, %s.", "!!",
            luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);
        DISPATCH();
      }
      default:  /* this should not happen */
        luaG_runerror(L, "Error in Virtual Machine: invalid operator for OP_FNBIN.");
    }
  }
  LABEL_OP_PAIR: {
    Protect(agenaV_pair(L, RKB(i), RKC(i), ra); luaC_checkGC(L));
    DISPATCH();
  }
  LABEL_OP_COMPLEX: {
    TValue *rb = RKB(i);
    TValue *rc = RKC(i);
    if (ttisnumber(rb) && ttisnumber(rc)) {  /* 0.22.2 */
      /* Explicitly first save the two numbers in rb, rc to two new variables, then call the setrealimagvalue macro
         with these two variables. Otherwise, we will get problems in DOS (DJGPP) with suddenly nvalue(rb) = nvalue(rc). */
      lua_Number re = nvalue(rb);
      lua_Number im = nvalue(rc);
      setrealimagvalue(ra, re, im);
    }
    else
      luaG_runerror(L, "Error: numbers expected for " LUA_QS " operator, got %s, %s.", "!",
        luaT_typenames[(int)ttype(rb)], luaT_typenames[(int)ttype(rc)]);  /* 3.7.8 improvement */
    DISPATCH();
  }
  LABEL_OP_TEEQ: {  /* 0.22.0, strict equality check (`==' operator) */
    TValue *rb = RKB(i);
    TValue *rc = RKC(i);
    if (ttype(rb) == ttype(rc)) {
      setbvalue(ra, luaV_equalvalonebyone(L, rb, rc));
    }
    DISPATCH();
  }
  LABEL_OP_TAEQ: {  /* 2.1.4, approximate equality check (`~=' operator) */
    TValue *rb = RKB(i);
    TValue *rc = RKC(i);
    if (ttype(rb) == ttype(rc)) {
      setbvalue(ra, luaV_approxequalvalonebyone(L, rb, rc));
    } else if (ttisnumber(rb) && ttiscomplex(rc)) {  /* 2.32.5 patch */
      lua_Number re = complexreal(rc);
      lua_Number im = compleximag(rc);
      if (im == 0) {
        setbvalue(ra, tools_approx(re, nvalue(rb), L->Eps));
      } else {
        setbvalue(ra, tools_approx(nvalue(rb), re, L->Eps) &&
                      tools_approx(0, im, L->Eps));
      }
    } else if (ttiscomplex(rb) && ttisnumber(rc)) {  /* 2.32.5 patch */
      lua_Number re = complexreal(rb);
      lua_Number im = compleximag(rb);
      if (im == 0) {
        setbvalue(ra, tools_approx(re, nvalue(rc), L->Eps));
      } else {
        setbvalue(ra, tools_approx(nvalue(rc), re, L->Eps) &&
                      tools_approx(0, im, L->Eps));
      }
    }
    DISPATCH();
  }
  LABEL_OP_TAILCALL: {
    int b = GETARG_B(i);
    if (b != 0) L->top = ra + b;  /* else previous instruction set top */
    L->savedpc = pc;
    lua_assert(GETARG_C(i) - 1 == LUA_MULTRET);
    switch (luaD_precall(L, ra, LUA_MULTRET)) {
      case PCRLUA: {
        int aux;
        /* tail call: put new frame in place of previous one */
        CallInfo *ci = L->ci - 1;  /* previous frame */
        StkId func = ci->func;
        StkId pfunc = (ci + 1)->func;  /* previous function index */
        ci->nargs = (ci + 1)->nargs;  /* nargs at current ci is incorrect, so get correct one, 0.13.1 patch 2 */
        if (L->openupval) luaF_close(L, ci->base);
        L->base = ci->base = ci->func + ((ci + 1)->base - pfunc);
        for (aux=0; pfunc + aux < L->top; aux++)  /* move frame down */
          setobjs2s(L, func + aux, pfunc + aux);
        ci->top = L->top = func + aux;  /* correct top */
        lua_assert(L->top == L->base + clvalue(func)->l.p->maxstacksize);
        ci->savedpc = L->savedpc;
        ci->tailcalls++;  /* one more call lost */
        L->ci--;  /* remove new frame */
        if (cl->rtable != NULL && cl->updatertable) {  /* at this moment, the function written in Agena language
          has _not_ already been called, the function and its arguments are still on the stack. Unfortunately,
          LABEL_OP_RETURN cannot detect tailcalls so we cannot add returns to a remember tables reliably.

          So we are now switching the remember table into read-only mode.

          Gemini AI explains: "In a normal recursive function (without tailcalls), every single step gets recorded
          because every step hits OP_RETURN. For example, if you call factorial(3), it calls factorial(2), which
          calls factorial(1). As the stack unwinds, OP_RETURN triggers for every single one of those calls,
          allowing your luaD_rtableentry to intercept and cache the results for 1, 2, and 3.
          With a tailcall, that entire unwinding process is deleted.
          Normal Call Stack Unwinding (Hits OP_RETURN every time):

          procedure rec_sqrt(n, g) is
             feature reminisce;
             local next_g := (g + n/g)/2
             if |g - next_g| < 1e-12 then
                return next_g
             else
                return rec_sqrt(n, next_g) + 0
             end
          end;

          rec_sqrt(2, 1);  # -> 1.4142135623731

          debug.getrtable(rec_sqrt):
          [482542 ~ [[2, 1.4142135623731] ~ 1.4142135623731], 487789 ~ [[2, 1] ~ 1.4142135623731],
           489725 ~ [[2, 1.4142135623747] ~ 1.4142135623731], 492103 ~ [[2, 1.5] ~ 1.4142135623731],
           498337 ~ [[2, 1.4142156862745] ~ 1.4142135623731], 499419 ~ [[2, 1.41666...] ~ 1.4142135623731]]

          rec_sqrt(2) -> rec_sqrt(1.5) -> rec_sqrt(1.414)
                ¦               ¦               ¦
             Caches          Caches          Caches
             result          result          result

          Tailcall Stack (Bypasses intermediate OP_RETURNS):
          [ rec_sqrt(2) overwritten by rec_sqrt(1.5) overwritten by rec_sqrt(1.414) ]
                                                                          ¦
                                                                      Only Caches
                                                                      Final Result

          Because OP_TAILCALL reuses the stack frame, the intermediate steps never hit OP_RETURN.
          If you didn't intercept OP_TAILCALL back then, your remember table would only cache the very final
          base case (e.g., caching that rec_sqrt(j, last_g) equals 5). It would completely fail to cache the
          intermediate steps like rec_sqrt(j, j/2). The next time your program called rec_sqrt, it would
          have to re-run the entire recursion chain from scratch because the initial arguments were never saved!" */
          /* luaG_runerror(L, "Error: cannot add tailcall results to remember table, add a neutral term to\n   the "
            LUA_QS " statement, such as " LUA_QS " or " LUA_QS ", to prevent tailcalls.", "return", "+ 0", "& ''"); */
          cl->updatertable = 0;
        }
        goto reentry;
      }
      case PCRC: case PCRREMEMBER: {  /* it was a C function (`precall' called it) or a hit in a remember table */
        base = L->base;
        DISPATCH();
      }
      default: {
        return;  /* yield */
      }
    }
  }
  LABEL_OP_RETURN: {
    int b = GETARG_B(i);  /* b - 1 = nrets */
    Proto *p = cl->p;
    if (b != 0) L->top = ra + b - 1;
    if (L->openupval) luaF_close(L, base);
    if (p->is_rettypegiven) {  /* return type given ? Agena 1.3.0 */
      const TValue *tm;
      int type, flag, j, x;
      Protect(
      StkId slot;
      if (ra == L->top) /* 2.3.4 fix, function does not return anything but return type has been given ? */
        luaG_runerror(L, "Error in " LUA_QS ": result of type %s expected, but nothing to return.", "return",
          (p->rettype != -1) ? luaT_typenames[p->rettype] : getstr(p->rettypets));
      if (p->rettype != -1) {
        for (slot = ra; slot < L->top; slot++) {
          type = ttype(slot);
          flag = 0;  /* 2.7.0 extension: check type of result to up to four basic types */
          x = p->rettype;
          for (j=0; j < LUAI_NTYPELIST; j++) {
            lua_typecheck curtype = x % LUAI_NTYPELISTMOD;
            if (curtype == 0) break;  /* we have no more types passed by the user, bail out, 2.12.1 */
            /* if (type != 0 && type == curtype) { */ /* type matches */
            if (type == curtype || curtype == LUA_TANYTHING ||  /* type matches, extended 2.20.1 */
              (curtype == LUA_TLISTING && (type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG)) ) {
              flag = 1;
              break;  /* 2.5.3 improvement: bail out immediately */
            } else if (type == LUA_TNUMBER) {  /* (GREP_POINT) types; if you add new number `types', 2.12.1 extension */
              lua_Number x = nvalue(slot);
              switch (curtype) {
                case IAM_INTEGER:
                  flag = tools_isint(x);  /* 2.14.13 tuning */
                  break;
                case IAM_POSINT:
                  flag = tools_isposint(x);
                  break;
                case IAM_NONNEGINT:
                  flag = tools_isnonnegint(x);
                  break;
                case IAM_NONZEROINT:  /* 4.11.0 */
                  flag = tools_isint(x) && x != 0;
                  break;
                case IAM_POSITIVE:
                  flag = x > 0;
                  break;
                case IAM_NEGATIVE:  /* 3.3.6 */
                  flag = x < 0;
                  break;
                case IAM_NONNEGATIVE:
                  flag = x >= 0;
                  break;
              }
              if (flag) break;
            }
            x /= LUAI_NTYPELISTMOD;
          }
          if (!flag) {
            if (p->rettype < LUAI_NTYPELISTMOD)  /* only one type or return given ? */
               luaG_runerror(L, "Error in " LUA_QS ": result of type %s expected, got %s.", "return",
                 luaT_typenames[p->rettype], luaT_typenames[(int)type]);
            else
              luaG_runerror(L, "Error in " LUA_QS ": unexpected type %s in return.", "return",
                luaT_typenames[(int)type]);
          }
          switch (type) {  /* 2.5.3 */
            case LUA_TTABLE: {
              Table *s = hvalue(slot);
              callmetareturnbasictype(L, s->metatable, slot, type);
              break;
            }
            case LUA_TSEQ: {
              Seq *s = seqvalue(slot);
              callmetareturnbasictype(L, s->metatable, slot, type);
              break;
            }
            case LUA_TPAIR: {
              Pair *s = pairvalue(slot);
              callmetareturnbasictype(L, s->metatable, slot, type);
              break;
            }
            case LUA_TREG: {
              Reg *s = regvalue(slot);
              callmetareturnbasictype(L, s->metatable, slot, type);
              break;
            }
            case LUA_TUSERDATA: {
              Udata *u = rawuvalue(base + i);
              callmetareturnbasictype(L, u->uv.metatable, slot, type);
              break;
            }
            default: {
              lua_assert(0);
            }
          }
        }
      } else {  /* optional user-defined type specified */
        for (slot = ra; slot < L->top; slot++) {
          int type = ttype(slot);
          switch (ttype(slot)) {
            case LUA_TSEQ: {
              Seq *s = seqvalue(slot);
              callmetareturnutype(L, s->type, s->metatable, slot, type);
              break;
            }
            case LUA_TTABLE: {
              Table *s = hvalue(slot);
              callmetareturnutype(L, s->type, s->metatable, slot, type);
              break;
            }
            case LUA_TSET: {
              UltraSet *s = usvalue(slot);
              callmetareturnutype(L, s->type, s->metatable, slot, type);
              break;
            }
            case LUA_TPAIR: {
              Pair *s = pairvalue(slot);
              callmetareturnutype(L, s->type, s->metatable, slot, type);
              break;
            }
            case LUA_TFUNCTION: {
              Closure *c = clvalue(slot);
              if ((c->l.type != NULL && tools_strneq(getstr(c->l.type), getstr(p->rettypets))) ||  /* 2.16.12 tweak */
                 (c->c.type != NULL && tools_strneq(getstr(c->c.type), getstr(p->rettypets))))     /* 2.16.12 tweak */
                  luaG_runerror(L, "Error in " LUA_QS ": result of type %s expected, got %s.",
                  getstr(p->rettypets), luaT_typenames[(int)ttype(slot)]);
              break;
            }
            case LUA_TREG: {  /* 2.5.3 */
              Reg *s = regvalue(slot);
              callmetareturnutype(L, s->type, s->metatable, slot, type);
              break;
            }
            case LUA_TUSERDATA: {  /* 2.5.3 */
              Udata *u = rawuvalue(slot);
              callmetareturnutype(L, u->uv.type, u->uv.metatable, slot, type);
              break;
            }
            default:
              luaG_runerror(L, "Error in " LUA_QS ": result must be of type %s, got %s.",
                "return", getstr(p->rettypets), luaT_typenames[type]);  /* 2.5.3 */
          }
        }
      }
      )
    }
    L->savedpc = pc;
    if (cl->rtable != NULL && cl->updatertable) {  /* does remember table exist and there is NO corresponding
      entry in the rtable ? Then enter args and results into it. If the return statement includes a tailcall
      to a function written in Agena, the following statement will not be called to add the result, as
      LABEL_OP_TAILCALL would have thrown an error before as cl->rtable and cl->updatertable would be `null`
      otherwise. See exception and explanation in LABEL_OP_TAILCALL. */
      luaD_rtableentry(L, cl, base, ra, L->top - ra);  /* 2.10.2 fix: do not pass b + LUA_MULTRET as #results ! */
    }
    b = luaD_poscall(L, ra);
    if (--nexeccalls == 0)  /* was previous function running `here'? */
      return;  /* no: return */
    else {  /* yes: continue its execution */
      if (b) L->top = L->ci->top;
      lua_assert(isLua(L->ci));
      lua_assert(GET_OPCODE(*((L->ci)->savedpc - 1)) == OP_CALL);
      goto reentry;
    }
  }
  LABEL_OP_FORLOOP: {  /* tweaked 1.4.0 */
    volatile lua_Number idx, limit, step;
    limit = nvalue(ra + 1);
    step = nvalue(ra + 2);
    if (nvalue(ra + 4) != HUGE_VAL) {  /* by checking for correction variable: step size is non-integral ?
      Getting higher word and doing IEEE math is not faster. */
      volatile lua_Number c, t, y;
      c = nvalue(ra + 4);  /* correction value c */
      if (L->vmsettings > 1) {  /* not the default; grep "VMSETTINGS" */
        if (L->vmsettings & 2) {  /* 2.4.2, increment index: apply Kahan-Ozawa summation algorithm to avoid round-off errors */
          volatile lua_Number u, w, s0, absstep;
          idx = nvalue(ra);
          y = step - c;
          s0 = idx;
          idx += y;
          absstep = fabs(step);
          if (absstep < fabs(c)) {
            t = step;
            step = -c;
            c = t;
          }
          u = (y - step) + c;
          if (fabs(s0) < fabs(y)) {
            t = s0;
            s0 = y;
            y = t;
          }
          w = (idx - s0) - y;
          c = u + w;  /* idx = incremented new value, c = new correction variable */
          if (L->vmsettings & 1 && idx == -0) idx = 0; /* 2.34.1 change */
          if (luai_numlt(0, step) ? luai_numle(idx, limit)
                                  : luai_numle(limit, idx)) {
            dojump(L, pc, GETARG_sBx(i));  /* jump back */
            setnvalue(ra + 4, c);  /* update internal roundoff prevention variable c */
          }
        } else if (L->vmsettings & 4) {  /* 2.30.5, apply Kahan-Babuška summation */
          volatile lua_Number cc, ccs, cx, absstep;
          ccs = nvalue(ra + 5);  /* second correction value */
          y = nvalue(ra + 6);    /* internal uncorrected accumulator */
          t = y + step;
          absstep = fabs(step);
          cx = (fabs(y) >= absstep) ? (y - t) + step : (step - t) + y;
          y = t;
          /* we cannot optimise the following by merging the query into the above |y| >= |step| query; and we have
             to check for zero here and not below. */
          if (fabs(y) < AGN_EPSILON) { y = 0; c = 0; ccs = 0; }
          t = c + cx;
          cc = (fabs(c) >= fabs(cx)) ? (c - t) + cx : (cx - t) + c;
          c = t;
          ccs += cc;
          idx = y + c + ccs;
          if (luai_numlt(0, step) ? luai_numle(idx, limit)
                                  : luai_numle(limit, idx)) {
            dojump(L, pc, GETARG_sBx(i));  /* jump back */
            setnvalue(ra + 4, c);    /* update internal roundoff prevention variable c (cs) */
            setnvalue(ra + 5, ccs);  /* update internal roundoff prevention variable ccs */
            setnvalue(ra + 6, y);    /* update internal uncorrected accumulator */
          }
        } else
          luaG_runerror(L, "Error: incorrect loop auto-correction setting.");
      } else {  /* by default (2.31.0) apply Adapted Neumaier summation algorithm to avoid round-off errors with fractional step sizes */
        volatile lua_Number s, t, absstep;
        s = nvalue(ra + 5);  /* internal uncorrected accumulator */
        t = s + step;
        absstep = fabs(step);
        c += (fabs(s) >= absstep) ? (s - t) + step : (step - t) + s;
        s = t;
        idx = s + c;
        /* Neumaier and Kx summation are inaccuarate over -step - 1 .. step + 1, so we have to adjust */
        if (L->vmsettings & 1 && fabs(idx) < L->hEps) idx = 0;  /* 2.34.0/2.34.1 extension to check for close-zero */
        if (luai_numlt(0, step) ? luai_numle(idx, limit)
                                : luai_numle(limit, idx)) {
          dojump(L, pc, GETARG_sBx(i));  /* jump back */
          setnvalue(ra + 4, c);  /* update internal roundoff prevention variable c (cs) */
          setnvalue(ra + 5, s);  /* internal uncorrected accumulator */
        }
      }
    } else {  /* step size is an integer */
      idx = luai_numadd(nvalue(ra), step);  /* increment index */
      if (luai_numlt(0, step) ? luai_numle(idx, limit)
                              : luai_numle(limit, idx)) {
        dojump(L, pc, GETARG_sBx(i));  /* jump back */
      }
    }
    setnvalue(ra, idx);  /* update internal index */  /* 0.12.3 */
    DISPATCH();
  }
  LABEL_OP_FORPREP: {
    volatile lua_Number step = 1;  /* to avoid compiler warnings */
    const TValue *init = ra;
    const TValue *plimit = ra + 1;
    const TValue *pstep = ra + 2;
    L->savedpc = pc;  /* next steps may throw errors */
    if (!ttisnumber(init) || nvalue(init) == HUGE_VAL)
      luaG_runerror(L, "Error: " LUA_QL("for") " initial value must be a number, got %s.", luaT_typenames[(int)ttype(init)]);
    else if (!ttisnumber(plimit))  /* infinity is again allowed, 2.0.0 RC 2 */
      luaG_runerror(L, "Error: " LUA_QL("for") " limit must be a number, got %s.", luaT_typenames[(int)ttype(plimit)]);
    else if (!ttisnumber(pstep) || (step = nvalue(pstep)) == HUGE_VAL)  /* 2.3.0 RC 1 security fix */
      luaG_runerror(L, "Error: " LUA_QL("for") " step must be a number, got %s.", luaT_typenames[(int)ttype(pstep)]);
    if (nvalue(ra + 3)) {  /* `downto' token given ?  2.3.0 RC 1 */
      step *= -1;
      setnvalue(ra + 2, step);
      setnvalue(ra + 3, 0);  /* and change sign of step only once, so that `redo' etc. will not get confused */
    }
    if (tools_isfrac(step)) {  /* 0.30.2, do not use (int)(step) != step since 1e300 also is an integer */
      volatile lua_Number c, t, y, idx, absstep;
      idx = nvalue(ra);
      /* 2.31.1: we add a small eps value to prevent that iteration stops prior to reaching the limit */
      if ( ( absstep = fabs(step)) <= agn_gethepsilon(L))
        luaG_runerror(L, "Error: step size is less than hEps threshold.");  /* 6.7.9 fix to prevent meaningless message consisting of zeros only */
      setnvalue(ra + 1, nvalue(plimit) + tools_negatewhen(agn_gethepsilon(L), step < 0));
      if (L->vmsettings > 1) {  /* grep "VMSETTINGS"; apply Kahan-Ozawa summation algorithm to avoid round-off errors, 2.4.2 */
        if (L->vmsettings & 2) {
          volatile lua_Number u, w, sold;
          c = 0;
          y = step - 0;
          idx -= step;
          sold = idx;
          idx += y;
          if (fabs(step) < fabs(c)) {
            t = step;
            step = -c;
            c = t;
          }
          u = (y - step) + c;
          if (fabs(sold) < fabs(y)) {
            t = sold;
            sold = y;
            y = t;
          }
          w = (idx - sold) - y;
          c = u + w;
          idx = sold;  /* idx = incremented new value, c = new correction variable */
        } else if (L->vmsettings & 4) {  /* Kahan-Babuška summation, 2.30.5 */
          volatile lua_Number x, s, cc, cs, ccs, t;
          x = step;
          s = idx - x;  /* accumulator */
          cs  = 0;  /* first correction value */
          ccs = 0;  /* second correction value */
          t = s + x;
          c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
          s = t;
          t = cs + c;
          cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
          cs = t;
          ccs += cc;
          idx -= step;
          c = cs;
          setnvalue(ra + 5, ccs);
          setnvalue(ra + 6, idx);
        } else
          luaG_runerror(L, "Error: incorrect loop auto-correction setting.");
      } else {  /* Adapted Neumaier summation */
        c = 0;
        setnvalue(ra + 5, idx - step);  /* internal accumulator */
      }
      setnvalue(ra, idx);    /* update internal index */
      setnvalue(ra + 4, c);  /* correction */
    }  /* end of if ISFLOAT */
    else
      setnvalue(ra, luai_numsub(nvalue(ra), step));
    dojump(L, pc, GETARG_sBx(i));
    DISPATCH();
  }
  LABEL_OP_TFORLOOP: {
    int isfunc;
    StkId cb = ra + 3;  /* call base */
    setobjs2s(L, cb + 2, ra + 2);
    setobjs2s(L, cb + 1, ra + 1);
    setobjs2s(L, cb, ra);
    isfunc = clvalue(ra)->c.tfor_func;
    L->top = cb + 3;  /* function + 2 args (state and index) */
    Protect(luaD_call(L, cb, GETARG_C(i), 1));
    L->top = L->ci->top;
    cb = RA(i) + 3;  /* previous call may change the stack */
    if (ttisnotnil(cb)) {  /* continue loop? */
      setobjs2s(L, cb - 1, cb);  /* save control variable */
      if (isfunc && ttisnil(cb + 1)) {  /* 2.12.2, key has been set but value is empty ? -> we are iterating a function */
        /* With loops of style "for i in <function> do .. od", we set the value slot (for control var i) not to null
           but to the function return.
           With loops of style "for i, j in <function> do .. od", both i and j are set to the function return. */
        setobjs2s(L, cb + 1, cb);
      }
      dojump(L, pc, GETARG_sBx(*pc));  /* jump back */
    }
    /* exit loop */
    pc++;
    DISPATCH();
  }
  LABEL_OP_TFORPREP: {  /* changed 0.5.0, taken from Lua 5.0.3 */
    if (ttistable(ra) || ttisuset(ra) || ttisseq(ra) || ttisstring(ra) || ttisreg(ra)) {
      /* this must be checked, else iterator functions get confused */
      setobjs2s(L, ra + 1, ra);
      setobj2s(L, ra, luaH_getstr(hvalue(gt(L)), luaS_new(L, "nextone")));
      clvalue(ra)->c.tfor_func = 0;  /* 2.12.2 */
    }
    else if (ttisnil(ra)) {  /* 0.25.2 */
      setseqvalue(L, ra + 1, agnSeq_new(L, 0));  /* do nothing */
      Protect(luaC_checkGC(L));
      setobj2s(L, ra, luaH_getstr(hvalue(gt(L)), luaS_new(L, "nextone")));
      clvalue(ra)->c.tfor_func = 0;  /* 2.12.2 */
    }
    else if (ttisfunction(ra)) {  /* new 7.1.0 */
      const TValue *tm = luaT_gettmbyobj(L, ra, TM_ITER);
      if (ttisfunction(tm)) {
        setobjs2s(L, ra, tm);
      }  /* else leave original function in slot */
      clvalue(ra)->c.tfor_func = 1;  /* 2.12.2 */
    } else if (ttisuserdata(ra)) {  /* new 7.1.0 */
      const TValue *tm = luaT_gettmbyobj(L, ra, TM_ITER);
      if (ttisfunction(tm)) {
        setobjs2s(L, ra, tm);
        clvalue(ra)->c.tfor_func = 1;
      } else {
        luaG_runerror(L, "Error in for/in: expected an iterator function, got %s.",
        luaT_typenames[(int)ttype(tm)]);
      }
    }
    else {
      luaG_runerror(L, "Error in for/in: cannot iterate a %s value.",
        luaT_typenames[(int)ttype(ra)]);
    }
    dojump(L, pc, GETARG_sBx(i));
    DISPATCH();
  }
  LABEL_OP_SETLIST: {
    int n = GETARG_B(i);
    int c = GETARG_C(i);
    int last;
    if (n == 0) {
      n = cast_int(L->top - ra) - 1;
      L->top = L->ci->top;
    }
    if (c == 0) c = cast_int(*pc++);
    if (ttistable(ra)) {  /* 0.11.0 */
      Table *h;
      h = hvalue(ra);
      last = ((c - 1)*LFIELDS_PER_FLUSH) + n;
      if (last > h->sizearray)  /* needs more space? */
        luaH_resizearray(L, h, last);  /* pre-alloc it at once */
      for (; n > 0; n--) {
        TValue *val = ra + n;
        luaH_setint(L, h, last--, val);  /* 4.6.3 tweak */
        luaC_barriert(L, h, val);
      }
    } else if (ttisuset(ra)) {  /* 0.12.2 */
      UltraSet *h;
      h = usvalue(ra);
      last = ((c - 1)*LFIELDS_PER_FLUSH) + n;
      if (last > h->size)  /* needs more space? */
        agnUS_resize(L, h, last);  /* pre-alloc it at once */
      for (; n > 0; n--) {
        TValue *val = ra + n;
        agnUS_set(L, h, val);
        luaC_barrierset(L, h, val);  /* Agena 1.2 fix */
      }
    } else if (ttisseq(ra)) {  /* fixed 2.29.6 */
      Seq *h = seqvalue(ra);
      last = ((c - 1)*LFIELDS_PER_FLUSH) + n;
      if (last > h->maxsize)  /* needs more space? */
        agnSeq_resize(L, h, last);  /* pre-allocate it at once */
      for (; n > 0; n--) {  /* from right to left do */
        TValue *val = ra + n;
        Protect(
          if (agnSeq_addi(L, h, last--, val))
            luaG_runerror(L, "Error in " LUA_QL("seq") ", null not allowed in sequences.");
        );
        luaC_barrierseq(L, h, val);  /* 4.6.4 fix */
      }
    } else if (ttisreg(ra)) {  /* fixed 2.29.6 */
      Reg *h = regvalue(ra);
      last = ((c - 1)*LFIELDS_PER_FLUSH) + n;
      if (last > h->maxsize)  /* needs more space? */
        agnReg_resize(L, h, last, 0);  /* pre-allocate it at once */
      for (; n > 0; n--) {  /* from right to left do */
        TValue *val = ra + n;
        Protect(agnReg_seti(L, h, last--, val););
        luaC_barrierreg(L, h, val);  /* 4.6.4 fix */
      }
    }
    DISPATCH();
  }
  LABEL_OP_SETVECTOR: {  /* create a linalg vector, 4.0.0 RC 1 */
    int n = GETARG_B(i);
    int c = GETARG_C(i);
    int last;
    if (n == 0) {
      n = cast_int(L->top - ra) - 1;
      L->top = L->ci->top;
    }
    if (c == 0) c = cast_int(*pc++);
    if (ttistable(ra)) {
      Table *h;
      TString *r;
      int hasvector, nops, vdim, tt;
      hasvector = 0;
      vdim = MAX_INT;
      nops = n;
      h = hvalue(ra);
      last = ((c - 1)*LFIELDS_PER_FLUSH) + n;
      if (last > h->sizearray)  /* needs more space? */
        luaH_resizearray(L, h, last);  /* pre-alloc it at once */
      /* set vector components */
      tt = ttype(ra + n);
      for (; n > 0; n--) {
        /* set component into table */
        TValue *val = ra + n;
        luaH_setint(L, h, last--, val);  /* 4.6.3 tweak */
        luaC_barriert(L, h, val);
        r = (ttistable(val)) ? hvalue(val)->type : NULL;
        hasvector = (r == NULL) ? 0 : tools_streq(getstr(r), "vector");
        if (ttype(val) != tt)
          luaG_runerror(L,
            "Error in %s construction, components must be of the same type.", (hasvector) ? "matrix" : "vector");
        if (hasvector) {  /* we have a vector as a component, so create a matrix */
          int vdim2;
          lua_lock(L);
          /* push vector dimension onto the stack; reserves one slot if needed */
          aux_gettablefield(L, val, "dim");
          if (!ttisnumber(L->top - 1)) {
            lua_unlock(L);
            luaG_runerror(L, "Error in matrix construction, malformed vector found.");
          }
          vdim2 = nvalue(L->top - 1);
          L->top--;
          if (vdim == MAX_INT) vdim = vdim2;
          else if (vdim != vdim2) {
            lua_unlock(L);
            luaG_runerror(L, "Error in matrix construction, vectors have different dimensions.");
          } else {
            vdim = vdim2;
          }
          lua_unlock(L);
        }
      }
      luaD_checkstack(L, 1);
      /* set vector or matrix dimension(s) */
      if (!hasvector) {
        setnvalue(L->top++, nops);
      } else {
        agn_createpairnumbers(L, nops, vdim);
      }
      setobj2t(L, luaH_setstr(L, h, luaS_new(L, "dim")), L->top - 1);
      luaC_barriert(L, h, L->top - 1);
      L->top--;  /* pop dimension */
      /* set metatable and user-defined type */
      api_check(L, L->top < L->ci->top);
      setobj2s(L, L->top, luaH_getstr(hvalue(gt(L)), luaS_new(L, "linalg")));
      if (!ttistable(L->top))
        luaG_runerror(L, "Error in vector construction, " LUA_QL("linalg") " package not found.");
      aux_gettablefield(L, L->top, (hasvector) ? "mmt" : "vmt");
      if (!ttistable(L->top - 1))
        luaG_runerror(L, "Error in vector construction, " LUA_QL("linalg.vmt") " metatable not found.");
      agenaV_setmetatable(L, ra, hvalue(L->top - 1), 0);
      agenaV_setutype(L, ra, hasvector ? luaS_new(L, "matrix") : luaS_new(L, "vector"));
      L->top--;
    }
    DISPATCH();
  }
  LABEL_OP_CLOSE: {
    luaF_close(L, ra);
    DISPATCH();
  }
  LABEL_OP_CLOSURE: {
    Proto *p;
    Closure *ncl;
    int nup, j;
    p = cl->p->p[GETARG_Bx(i)];
    nup = p->nups;
    ncl = luaF_newLclosure(L, nup, cl->env);
    ncl->l.p = p;
    for (j=0; j<nup; j++, pc++) {
      if (GET_OPCODE(*pc) == OP_GETUPVAL)
        ncl->l.upvals[j] = cl->upvals[GETARG_B(*pc)];
      else {
        lua_assert(GET_OPCODE(*pc) == OP_MOVE);
        ncl->l.upvals[j] = luaF_findupval(L, base + GETARG_B(*pc));
      }
    }
    setclvalue(L, ra, ncl);
    Protect(luaC_checkGC(L));
    DISPATCH();
  }
  LABEL_OP_INTDIV: {  /* added 0.5.4 */
    arith_opNumber(sun_intdiv, TM_INTDIV);  /* 2.3.0 fix, 2.17.1 optimisation */
    DISPATCH();
  }
  LABEL_OP_ABSDIFF: {  /* added 2.9.8 */
    arith_opNumber(luai_numabsdiff, TM_ABSDIFF);
    DISPATCH();
  }
  LABEL_OP_TRY: {  /* 2.1 RC 2, written by Hu Qiwei */
    int status;
    pj = cast(struct lua_longjmp *, luaM_malloc(L, sizeof(struct lua_longjmp)));
    pj->type = JMPTYPE_TRY;
    pj->status = 0;
    pj->pc = (Instruction *)pc + GETARG_sBx(i);
    pj->previous = L->errorJmp;
    pj->oldnCcalls = L->nCcalls;
    pj->old_ci = saveci(L, L->ci);
    pj->old_allowhooks = L->allowhook;
    pj->old_errfunc = L->errfunc;
    pj->old_nexeccalls = nexeccalls;
    pj->old_top = savestack(L, L->top);
    L->errorJmp = pj;
    L->errfunc = 0;
    status = setjmp(pj->b);  /* status is 1 if an error occurred, else 0 */
    if (status) {
      pc = L->errorJmp->pc;
      nexeccalls = L->errorJmp->old_nexeccalls;
      restoretry(L, GET_OPCODE(*pc) == OP_CATCH, GETARG_A(*pc));
      L->savedpc = pc;
      goto reentry;
    }
    DISPATCH();
  }
  LABEL_OP_ENDTRY: {  /* 2.1 RC 2, written by Hu Qiwei */
    releasetry(L);
    DISPATCH();
  }
  LABEL_OP_CATCH: {  /* 2.1 RC 2, written by Hu Qiwei */
    /* dummy opcode, do nothing here ! */
    DISPATCH();
  }
  LABEL_OP_SELF: {  /* 2.6.0, taken from Lua 5.1.5 code; for more information, check http://www.lua.org/pil/16.html. */
    /* called by: luaK_codeABC(fs, OP_SELF, func, e->u.s.info, luaK_exp2RK(fs, key)); */
    StkId rb = RB(i);
    setobjs2s(L, ra + 1, rb);  /* copy table for automatic assignment to the local name "self" */
    Protect(luaV_gettable(L, rb, RKC(i), ra));  /* RKC(i) = procedure; determine table.procedure and put procedure into ra [ra, sic !]
                                                   this transforms table@proc into the function call proc(table, ...) */
    DISPATCH();  /* now OP_CALL will be executed */
  }
}

