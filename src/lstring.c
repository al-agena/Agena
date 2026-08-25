/*
** $Id: lstring.c,v 2.8 2005/12/22 16:19:56 roberto Exp $
** String table (keeps all strings handled by Agena)
** See Copyright Notice in agena.h
*/

#include <stdio.h>
#include <string.h>

#define lstring_c
#define LUA_CORE

#include "agena.h"

#include "agnconf.h"
#include "agnhlps.h"  /* for tools_memcmp, mul2 */
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"

/* To convert a TValue into TString, use rawtsvalue(), and getstr() to convert from TString to const char*. */


/* 7.5.5 patch: It attempts to expand the existing hash vector in-place. This eliminates the need to hold two versions
   of the hash table simultaneously, significantly lowering the risk of allocation failure during growth.
   Written by Robert G. Jakabosky for Lua 5.1.3, see: http://lua-users.org/lists/lua-l/2008-05/msg00476.html */
void luaS_resize (lua_State *L, int newsize) {
  stringtable *tb;
  int i;
  tb = &G(L)->strt;
  if (G(L)->gcstate == GCSsweepstring || newsize == tb->size)
    return;  /* cannot resize during GC traverse or doesn't need to be resized */
  if (newsize > tb->size) {
    luaM_reallocvector(L, tb->hash, tb->size, newsize, GCObject *);
    for (i=tb->size; i<newsize; i++) tb->hash[i] = NULL;
  }
  /* rehash */
  for (i=0; i<tb->size; i++) {
    GCObject *p = tb->hash[i];
    tb->hash[i] = NULL;
    while (p) {  /* for each node in the list */
      GCObject *next = p->gch.next;  /* save next */
      unsigned int h = gco2ts(p)->hash;
      int h1 = lmod(h, newsize);  /* new position */
      lua_assert(cast_int(h % newsize) == lmod(h, newsize));
      p->gch.next = tb->hash[h1];  /* chain it */
      tb->hash[h1] = p;
      p = next;
     }
  }
  if (newsize < tb->size)
    luaM_reallocvector(L, tb->hash, tb->size, newsize, GCObject *);
  tb->size = newsize;
}


static TString *newlstr (lua_State *L, const char *str, size_t l,
                                       unsigned int h) {
  TString *ts;
  stringtable *tb;
  if (l + 1 > (MAX_SIZET - sizeof(TString))/CHARSIZE)
    luaM_toobig(L);
  ts = cast(TString *, luaM_malloc(L, (l + 1)*CHARSIZE+sizeof(TString)));
  ts->tsv.len = l;
  ts->tsv.hash = h;
  ts->tsv.marked = luaC_white(G(L));
  ts->tsv.tt = LUA_TSTRING;
  ts->tsv.reserved = 0;
  tools_memcpy(ts + 1, str, l*CHARSIZE);  /* 2.25.1 tweak */
  ((char *)(ts + 1))[l] = '\0';  /* ending 0 */
  tb = &G(L)->strt;
  h = lmod(h, tb->size);
  ts->tsv.next = tb->hash[h];  /* chain new entry */
  tb->hash[h] = obj2gco(ts);
  tb->nuse++;
  if (tb->nuse > cast(lu_int32, tb->size) && tb->size <= MAX_INT/2)
    luaS_resize(L, mul2(tb->size));  /* too crowded, 2.17.8 tweak */
  return ts;
}


TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  GCObject *o;
  unsigned int h = cast(unsigned int, l);  /* seed */
  size_t step = (l >> 5) + 1;  /* if string is too long, don't hash all its chars */
  size_t l1;
  for (l1=l; l1 >= step; l1 -= step)  /* compute hash */
    h = h ^ ((h << 5) + (h >> 2) + cast(unsigned char, str[l1 - 1]));
  for (o = G(L)->strt.hash[lmod(h, G(L)->strt.size)];
       o != NULL;
       o = o->gch.next) {
    TString *ts = rawgco2ts(o);
    if (ts->tsv.len == l && (tools_memcmp(str, getstr(ts), l) == 0)) {
      /* string may be dead */
      if (isdead(G(L), o)) changewhite(o);
      return ts;
    }
  }
  return newlstr(L, str, l, h);  /* not found */
}


TString *luaS_newchar (lua_State *L, const char *str) {
  GCObject *o;
  unsigned int h;  /* seed */
  /* compute hash, ^ is bitwise XOR, cannot be optimised */
  h = 1 ^ (32 + cast(unsigned char, str[0]));
  for (o = G(L)->strt.hash[lmod(h, G(L)->strt.size)];
       o != NULL;
       o = o->gch.next) {
    TString *ts = rawgco2ts(o);
    /* cannot be optimised, since ts->tsv.len often is <> 1 */
    if (ts->tsv.len == 1 && (tools_memcmp(str, getstr(ts), 1) == 0)) {
      /* string may be dead */
      if (isdead(G(L), o)) changewhite(o);
      return ts;
    }
  }
  return newlstr(L, str, 1, h);  /* not found */
}


Udata *luaS_newudata (lua_State *L, size_t s, Table *e) {
  Udata *u;
  if (s > MAX_SIZET - sizeof(Udata))
    luaM_toobig(L);
  u = cast(Udata *, luaM_malloc(L, s + sizeof(Udata)));
  u->uv.marked = luaC_white(G(L));  /* is not finalized */
  u->uv.tt = LUA_TUSERDATA;
  u->uv.len = s;
  u->uv.metatable = NULL;
  u->uv.type = NULL;  /* 2.3.0 RC 3 */
  u->uv.env = e;
  u->uv.nuvalue = 1;
  u->uv.readonly = 0;  /* 4.8.2 */
  /* chain it on udata list (after main thread) */
  u->uv.next = G(L)->mainthread->next;
  G(L)->mainthread->next = obj2gco(u);
  return u;
}

