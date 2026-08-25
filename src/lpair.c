/*
** $Id: lpair.c v 0.1, based on ltable.c v2.32, 2006/01/18 11:49:02 roberto Exp $
** Implementation of Agena Pairs
** See Copyright Notice in agena.h
*/

#define lpair_c
#define LUA_CORE

#include "agena.h"

#include "ldebug.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lpair.h"
#include "lstate.h"

/*
** }=============================================================
*/


/* create a new pair */

LUAI_FUNC Pair *agnPair_new (lua_State *L) {
  int i;
  Pair *t = luaM_new(L, Pair);
  luaC_link(L, obj2gco(t), LUA_TPAIR);
  t->flags = cast_byte(~0);
  /* temporary values (kept only if some malloc fails) */
  t->array = NULL;
  t->metatable = NULL;
  t->type = NULL;
  t->readonly = 0;
  luaM_reallocvector(L, t->array, 0, 2, TValue);
  for (i=0; i < 2; i++)
    setnilvalue(&t->array[i]);
  return t;
}


LUAI_FUNC Pair *agnPair_create (lua_State *L, const TValue *left, const TValue *right) {
  TValue *m, *n;
  Pair *t = luaM_new(L, Pair);
  luaC_link(L, obj2gco(t), LUA_TPAIR);
  t->flags = cast_byte(~0);
  t->readonly = 0;
  /* temporary values (kept only if some malloc fails) */
  t->array = NULL;
  t->metatable = NULL;
  t->type = NULL;
  luaM_reallocvector(L, t->array, 0, 2, TValue);
  /* now assign left and right value */
  m = pairitem(t, 0);
  *m = *left;
  luaC_barrierpair(L, t, left);
  n = pairitem(t, 1);
  *n = *right;
  luaC_barrierpair(L, t, right);
  return t;
}


/* set an operand in a pair */
LUAI_FUNC int agnPair_seti (lua_State *L, Pair *t, int index, const TValue *val) {
  TValue *n;
  if (t->readonly) luaG_runerror(L, "pair is read-only.");
  if (index > 2 || index < 1)
    return 0;  /* index out of range */
  n = pairitem(t, index - 1);  /* get next reserved memory chunk */
  *n = *val;
  luaC_barrierpair(L, t, val);
  return 1;
}


LUAI_FUNC int agnPair_rawseti (lua_State *L, Pair *t, const TValue *index, const TValue *val) {
  if (t->readonly) {
    luaG_runerror(L, "pair is read-only.");
    return 0;
  } else if (ttisnumber(index))
    return agnPair_seti(L, t, (int)nvalue(index), val);
  else
    return -2;
}


/* get the i-th value that has been added to a pair, 1 = lhd, 2 = rhs */

LUAI_FUNC const TValue *agnPair_geti (Pair *t, size_t index) {
  if (index > 2 || index < 1)
    return NULL;  /* index out of range */
  return pairitem(t, index - 1);
}


LUAI_FUNC const TValue *agnPair_rawgeti (Pair *t, const TValue *ind) {
  int index;
  if (ttisnumber(ind))
    index = (int)nvalue(ind);
  else
    return NULL;
  if (index > 2 || index < 1)
    return NULL;  /* index out of range */
  return pairitem(t, index - 1);
}


/* free a pair */
LUAI_FUNC void agnPair_free (lua_State *L, Pair *t) {
  luaM_freearray(L, t->array, 2, TValue);
  luaM_free(L, t);
}


LUAI_FUNC int agnPair_readonly (lua_State *L, Pair *t, int readonly) {  /* 4.8.2 */
  if (readonly < 0) return t->readonly;
  t->readonly = readonly;
  return readonly;
}