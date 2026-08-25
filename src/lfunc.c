/*
** $Id: lfunc.c,v 2.12 2005/12/22 16:19:56 roberto Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in agena.h
*/


#include <stddef.h>

#define lfunc_c
#define LUA_CORE

#include "agena.h"

#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"


Closure *luaF_newCclosure (lua_State *L, int nelems, Table *e) {
  Closure *c = cast(Closure *, luaM_malloc(L, sizeCclosure(nelems)));
  luaC_link(L, obj2gco(c), LUA_TFUNCTION);
  c->c.isC = 1;
  c->c.env = e;
  c->c.nupvalues = cast_byte(nelems);
  c->c.rtable = NULL;  /* 0.9.2 */
  c->c.storage = NULL;  /* 2.16.7 */
  c->c.metatable = NULL;  /* 2.36.2 */
  c->c.type = NULL;  /* 0.12.0 */
  c->c.updatertable = 0;  /* 0.22.0 */
  c->c.tfor_func = 0;  /* 2.12.2 */
  return c;
}


Closure *luaF_newLclosure (lua_State *L, int nelems, Table *e) {
  Closure *c = cast(Closure *, luaM_malloc(L, sizeLclosure(nelems)));
  luaC_link(L, obj2gco(c), LUA_TFUNCTION);
  c->l.isC = 0;
  c->l.env = e;
  c->l.nupvalues = cast_byte(nelems);
  c->l.rtable = NULL;  /* 0.9.2 */
  c->l.storage = NULL;  /* 2.16.7 */
  c->l.metatable = NULL;  /* 2.36.2 */
  c->l.type = NULL;  /* 0.12.0 */
  c->l.updatertable = 0;  /* 0.22.0 */
  while (nelems--) c->l.upvals[nelems] = NULL;
  c->l.tfor_func = 0;  /* 2.12.2 */
  return c;
}


UpVal *luaF_newupval (lua_State *L) {
  UpVal *uv = luaM_new(L, UpVal);
  luaC_link(L, obj2gco(uv), LUA_TUPVAL);
  uv->v = &uv->u.value;
  setnilvalue(uv->v);
  return uv;
}


UpVal *luaF_findupval (lua_State *L, StkId level) {
  global_State *g = G(L);
  GCObject **pp = &L->openupval;
  UpVal *p, *uv;
  while (*pp != NULL && (p = ngcotouv(*pp))->v >= level) {  /* Lua 5.1.2 patch */
    lua_assert(p->v != &p->u.value);
    if (p->v == level) {  /* found a corresponding upvalue? */
      if (isdead(g, obj2gco(p)))  /* is it dead? */
        changewhite(obj2gco(p));  /* resurrect = revive it */
      return p;
    }
    pp = &p->next;
  }
  uv = luaM_new(L, UpVal);  /* not found: create a new one */
  uv->tt = LUA_TUPVAL;
  uv->marked = luaC_white(g);
  uv->v = level;  /* current value lives in the stack */
  uv->next = *pp;  /* chain it in the proper position */
  *pp = obj2gco(uv);
  uv->u.l.prev = &g->uvhead;  /* double link it in `uvhead' list */
  uv->u.l.next = g->uvhead.u.l.next;
  uv->u.l.next->u.l.prev = uv;
  g->uvhead.u.l.next = uv;
  lua_assert(uv->u.l.next->u.l.prev == uv && uv->u.l.prev->u.l.next == uv);
  return uv;
}


static void unlinkupval (UpVal *uv) {
  lua_assert(uv->u.l.next->u.l.prev == uv && uv->u.l.prev->u.l.next == uv);
  uv->u.l.next->u.l.prev = uv->u.l.prev;  /* remove from `uvhead' list */
  uv->u.l.prev->u.l.next = uv->u.l.next;
}


void luaF_freeupval (lua_State *L, UpVal *uv) {
  if (uv->v != &uv->u.value)  /* is it open? */
    unlinkupval(uv);  /* remove from open list */
  luaM_free(L, uv);  /* free upvalue */
}


void luaF_close (lua_State *L, StkId level) {
  UpVal *uv;
  global_State *g = G(L);
  while (L->openupval != NULL && (uv = ngcotouv(L->openupval))->v >= level) {  /* Lua 5.1.2 patch */
    GCObject *o = obj2gco(uv);
    lua_assert(!isblack(o) && uv->v != &uv->u.value);
    L->openupval = uv->next;  /* remove from `open' list */
    if (isdead(g, o))
      luaF_freeupval(L, uv);  /* free upvalue */
    else {
      unlinkupval(uv);
      setobj(L, &uv->u.value, uv->v);
      uv->v = &uv->u.value;  /* now current value lives here */
      luaC_linkupval(L, uv);  /* link upvalue into `gcroot' list */
    }
  }
}


Proto *luaF_newproto (lua_State *L) {
  Proto *f = luaM_new(L, Proto);
  luaC_link(L, obj2gco(f), LUA_TPROTO);
  f->k = NULL;
  f->sizek = 0;
  f->p = NULL;
  f->sizep = 0;
  f->code = NULL;
  f->sizecode = 0;
  f->sizelineinfo = 0;
  f->sizeupvalues = 0;
  f->nups = 0;
  f->upvalues = NULL;
  f->numparams = 0;
  f->is_vararg = 0;
  f->is_oopfn = 0;
  f->maxstacksize = 0;
  f->lineinfo = NULL;
  f->sizelocvars = 0;
  f->sizeparams = 0;  /* 2.1.2 */
  f->locvars = NULL;
  f->params = NULL;  /* 2.1.2 */
  f->linedefined = 0;
  f->lastlinedefined = 0;
  f->source = NULL;
  f->is_typegiven = 0;  /* 0.20.0 */
  f->is_rettypegiven = 0;  /* return type */
  f->rettype = -1;  /* -1: no optional type specified, 1.2.1 */
  f->rettypets = NULL;  /* 1.2.1 */
  return f;
}


void luaF_freeproto (lua_State *L, Proto *f) {
  luaM_freearray(L, f->code, f->sizecode, Instruction);
  luaM_freearray(L, f->p, f->sizep, Proto *);
  luaM_freearray(L, f->k, f->sizek, TValue);
  luaM_freearray(L, f->lineinfo, f->sizelineinfo, int);
  luaM_freearray(L, f->locvars, f->sizelocvars, struct LocVar);
  luaM_freearray(L, f->params, f->sizeparams, struct Param);  /* 2.1.2 */
  luaM_freearray(L, f->upvalues, f->sizeupvalues, TString *);
  luaM_free(L, f);
}


void luaF_freeclosure (lua_State *L, Closure *c) {
  int size = (c->c.isC) ? sizeCclosure(c->c.nupvalues) :
                          sizeLclosure(c->l.nupvalues);
  luaM_freemem(L, c, size);
}


/*
** Look for n-th local variable at line `line' in function `func'.
** Returns NULL if not found.
*/
const char *luaF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i=0; i < f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {  /* is variable active? */
      local_number--;
      if (local_number == 0)
        return getstr(f->locvars[i].varname);
    }
  }
  return NULL;  /* not found */
}


