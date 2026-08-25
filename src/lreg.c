/*
** $Id: lreg.c v 0.1, based on ltable.c v2.32, 2006/01/18 11:49:02 roberto Exp $
** Implementation of Agena Registers
** See Copyright Notice in agena.h
*/

#define lreg_c
#define LUA_CORE

#include "agena.h"

#include "ldebug.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lreg.h"
#include "llimits.h"  /* for MAX_INT */


/*
** }=============================================================
*/


/* create a new register */

LUAI_FUNC Reg *agnReg_new (lua_State *L, int n) {
  int i;
  Reg *t = luaM_new(L, Reg);
  luaC_link(L, obj2gco(t), LUA_TREG);
  if (n < 1) n = L->regsize;  /* 4.12.1 fix for t->maxsize & t->top */
  /* temporary values (kept only if some malloc fails) */
  t->flags = cast_byte(~0);
  t->readonly = 0;
  t->array = NULL;
  t->top = n;
  t->maxsize = n;
  t->metatable = NULL;
  t->type = NULL;  /* 2.5.3 */
  luaM_reallocvector(L, t->array, 0, n, TValue);  /* 2.5.0 fix */
  for (i=0; i < n; i++)
    setnilvalue(regitem(t, i));
  return t;
}


/* set a new value to a register at `index` i, starting from 1;
   returns:
       0:    index out of range
       1:    success */
LUAI_FUNC int agnReg_seti (lua_State *L, Reg *t, int index, const TValue *val) {
  TValue *n;
  t->flags = 0;
  /* registers must be resized for it has become too small ? */
  if (t->readonly) luaG_runerror(L, "register is read-only.");
  if (index > t->top || index < 1) return 0; /* index out of range ?  2.4.0 fix */
  /* replace value at existing index */
  /* replace old value with new one */
  n = regitem(t, index - 1);
  if (luaO_rawequalObj(n, val)) return 1;
  setobj(L, n, val);  /* 7.6.7 fix */
  return 1;
}


/* check whether a value exists in a register */

LUAI_FUNC int agnReg_get (Reg *t, const TValue *val) {
  TValue *n;
  int i;
  for (i=0; i < t->top; i++) {
    n = regitem(t, i);
    if (luaO_rawequalObj(n, val))
      return 1;
  }
  return 0;
}


/* get the i-th value that has been added to a register */

LUAI_FUNC const TValue *agnReg_geti (Reg *t, size_t index) {
  if ((index > t->top) || index < 1)
    return NULL;  /* index out of range */
  else
    return regitem(t, index - 1);
}


LUAI_FUNC const TValue *agnReg_rawgeti (Reg *t, const TValue *ind) {
  int index;
  if (ttisnumber(ind))
    index = (int)nvalue(ind);
  else
    return NULL;
  if (index > t->top || index < 1)
    return NULL;  /* index out of range */
  return regitem(t, index - 1);
}


LUAI_FUNC int agnReg_delete (lua_State *L, Reg *t, const TValue *val) {
  TValue *n;
  int i, c = 0;
  if (t->readonly) luaG_runerror(L, "register is read-only.");
  if (t->top == 0) return c;
  for (i=0; i < t->top; i++) {
    n = regitem(t, i);
    if (luaO_rawequalObj(n, val)) {
      setnilvalue(n);
      luaC_barrierreg(L, t, val);  /* DELETEVAL */
      c++;
    }
  }
  return c;
}


/* free a register */

LUAI_FUNC void agnReg_free (lua_State *L, Reg *t) {
  luaM_freearray(L, t->array, t->maxsize, TValue);
  luaM_free(L, t);
}


LUAI_FUNC int agnReg_next (lua_State *L, Reg *t, StkId key) {
  int i;
  if (ttisnil(key))  /* first iteration */
    i = 0;
  else if (ttisnumber(key))
    i = nvalue(key);  /* find original element */
  else {
    i = -1;  /* to avoid compiler warnings */
    luaG_runerror(L, "invalid key to " LUA_QL("nextone"));
  }
  if (i < t->top) {
    setnvalue(key, cast_num(i + 1));
    setobj2s(L, key + 1, regitem(t, i));
    return 1;
  }
  return 0;  /* no more elements */
}

/* a version of agnReg_next to iterate functions is not faster than doing it the primitive way */


LUAI_FUNC void agnReg_rotatetop (lua_State *L, Reg *t) {
  size_t i;
  TValue *m, *n;
  if (t->readonly) luaG_runerror(L, "register is read-only.");
  /* duplicate last element and set it to the top of the registers */
  setobj2s(L, L->top, agnReg_geti(t, t->top));
  for (i=t->top - 1; i > 0; i--) {
    m = regitem(t, i);
    n = regitem(t, i - 1);
    setobj(L, m, n);  /* 7.6.7 fix */
  }
  /* set last element to the bottom */
  agnReg_seti(L, t, 1, L->top);
  luaC_barrierreg(L, t, L->top);
  setnilvalue(L->top);
}


LUAI_FUNC void agnReg_rotatebottom (lua_State *L, Reg *t) {
  size_t i;
  TValue *m, *n;
  if (t->readonly) luaG_runerror(L, "register is read-only.");
  /* duplicate first element and set it to the top of the registers */
  setobj2s(L, L->top, agnReg_geti(t, 1));
  /* move items to the bottom */
  for (i=1; i < t->top; i++) {
    m = regitem(t, i - 1);
    n = regitem(t, i);
    setobj(L, m, n);  /* 7.6.7 fix */
  }
  /* set first element to the top */
  agnReg_seti(L, t, t->top, L->top);
  luaC_barrierreg(L, t, L->top);
  setnilvalue(L->top);
}


LUAI_FUNC void agnReg_exchange (lua_State *L, Reg *t) {  /* expects two or more elements on the stack */
  size_t s;
  TValue *m, *n;
  if (t->readonly) luaG_runerror(L, "register is read-only.");
  /* duplicate element just below the top */
  s = t->top;
  setobj2s(L, L->top, agnReg_geti(t, s - 1));
  m = regitem(t, s - 2);
  n = regitem(t, s - 1);
  setobj(L, m, n);  /* 7.6.7 fix */
  agnReg_seti(L, t, s, L->top);
  luaC_barrierreg(L, t, L->top);
  setnilvalue(L->top);
}


LUAI_FUNC int agnReg_duplicate (lua_State *L, Reg *t) {  /* expects at least one element on the stack */
  size_t s;
  s = t->top;
  if (t->readonly) luaG_runerror(L, "register is read-only.");
  if (s == t->maxsize) return 0;
  setobj2s(L, L->top, agnReg_geti(t, s));
  agnReg_seti(L, t, s + 1, L->top);
  luaC_barrierreg(L, t, L->top);
  setnilvalue(L->top);
  t->top++;
  return 1;
}


LUAI_FUNC int agnReg_settop (lua_State *L, Reg *t, const TValue *ind) {
  int index;
  if (t->readonly) luaG_runerror(L, "register is read-only.");
  if (ttisnumber(ind))
    index = (int)nvalue(ind);
  else
    return 0;
  if (index > t->maxsize || index < 0) return 0;  /* index out of range */
  t->top = index;
  return 1;
}


LUAI_FUNC void agnReg_purge (lua_State *L, Reg *t, int index) {
  size_t i;
  TValue *m;
  TValue *n = NULL;  /* to avoid compiler warnings */
  if (t->readonly) luaG_runerror(L, "register is read-only.");
  /* move items right of item to be deleted to the left */
  if (index < t->top) {  /* item to be deleted is not at the end -> move elements */
    for (i=index; i < t->top; i++) {
      m = regitem(t, i - 1);
      n = regitem(t, i);
      setobj(L, m, n);  /* 7.6.7 fix */
    }
  } else
    n = regitem(t, index - 1);
  setnilvalue(n);
  t->top--;
}


LUAI_FUNC int agnReg_resize (lua_State *L, Reg *t, int newsize, int nil) {  /* rewritten 4.6.4 */
  int i;
  if (t->readonly) luaG_runerror(L, "register is read-only.");
  if (newsize == t->maxsize) return 0;  /* do nothing */
  if (newsize < 1) newsize = L->regsize;  /* 7.6.5 fix */
  if (newsize > t->maxsize) {  /* extend */
    luaM_reallocvector(L, t->array, t->maxsize, newsize, TValue);
    for (i=t->maxsize; i < newsize; i++) {
      setnilvalue(regitem(t, i));
    }
    if (t->top == t->maxsize) t->top = newsize;
  } else {  /* shrink */
    for (i=newsize; nil && i < t->maxsize; i++) {  /* nil all excessive slots */
      setnilvalue(regitem(t, i));
    }
    luaM_reallocvector(L, t->array, t->maxsize, newsize, TValue);  /* reduce size of register to newsize slots */
    if (t->top > newsize) t->top = newsize;
  }
  t->maxsize = newsize;
  luaC_checkGC(L);
  return 1;
}


LUAI_FUNC int agnReg_readonly (lua_State *L, Reg *t, int readonly) {  /* 4.8.2 */
  if (readonly < 0) return t->readonly;
  t->readonly = readonly;
  return readonly;
}


LUAI_FUNC int agnReg_subs (lua_State *L, Reg *t, const TValue *oldval, const TValue *newval) {  /* 5.1.3 */
  TValue *n;
  int i, cnt = 0;
  for (i=0; i < t->top; i++) {
    n = regitem(t, i);
    if (luaO_rawequalObj(n, oldval)) {
      agnReg_seti(L, t, i + 1, newval);
      cnt++;
    }
  }
  return cnt;
}


LUAI_FUNC void agnReg_reorder (lua_State *L, Reg *t) {  /* 7.6.4 */
  int i, cnt;
  TValue *val;
  if (t->readonly) luaG_runerror(L, "register is read-only.");
  cnt = 0;
  /* reshuffle */
  for (i=0; i < t->top; i++) {
    val = regitem(t, i);
    if (!ttisnil(val)) {
      TValue *dest = &t->array[cnt++];  /* target the next clean slot */
      if (dest != val) {
        setobj(L, dest, val);    /* raw copy: no metatable check, no overhead */
        setnilvalue(val);        /* clear the old hole */
      }
    }
  }
  agnReg_resize(L, t, cnt == 0 ? 1 : cnt, 0);  /* 7.6.5 fix */
}

