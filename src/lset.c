/*
** $Id: lset.c v 0.1, based on ltable.c v2.32, 2006/01/18 11:49:02 roberto Exp $
** Agena UltraSets implementation (hash)
** See Copyright Notice in agena.h
*/

/* Sets keep their elements in a hash implementation - a mix of chained scatter table
   with Brent's variation. A main invariant of these sets is that, if an element is not
   in its main position (i.e. the `original' position that its hash gives to it), then
   the colliding element is in its own main position. Hence even when the load factor
   reaches 100%, performance remains good. */

#include <stdio.h>
#include <string.h>

#define lset_c
#define LUA_CORE

#include "agena.h"

#include "agnhlps.h"
#ifdef PROPCMPLX
#include "lcomplex.h"
#endif
#include "ldebug.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lset.h"
#include "lstate.h"

/*
** max size of array part is 2^MAXBITS
*/
#if LUAI_BITSINT > 26
#define MAXBITS            26
#else
#define MAXBITS            (LUAI_BITSINT - 2)
#endif

#define MAXASIZE           (1 << MAXBITS)

#define hashpow2(t,n)      (glnode(t, lmod((n), sizenode(t))))
#define hashstr(t,str)     (hashpow2(t, (str)->tsv.hash))
#define hashboolean(t,p)   (hashpow2(t, p))

/*
** for some types, it is better to avoid modulus by power of 2, as
** they tend to have many 2 factors.
*/
#define hashmod(t,n)       (glnode(t, ((n) % ((sizenode(t)-1)|1))))
#define hashpointer(t,p)   (hashmod(t, IntPoint(p)))

/*
** number of ints inside a lua_Number
*/
#define numints            (cast_int(sizeof(lua_Number)/sizeof(int)))

/*
** hash for lua_Numbers
*/
static LNode *hashnum (const UltraSet *t, lua_Number n) {
  unsigned int a[numints];
  int i;
  if (luai_numeq(n, 0))  /* 5.1.3 patch; avoid problems with -0 */
    return glnode(t, 0);
  tools_memcpy(a, &n, sizeof(a));  /* 2.25.1 tweak */
  for (i=1; i < numints; i++) a[0] += a[i];
  return hashmod(t, a[0]);
}


/*
** hash for agn_Complex, new 6.8.1
*/
static LNode *hashcmplx (const UltraSet *t, lua_Number n, lua_Number m) {
  unsigned int a[numints], b[numints];
  int i;
  if (luai_numeq(n, 0) && luai_numeq(m, 0))  /* 5.1.3 patch; avoid problems with -0 */
    return glnode(t, 0);
  tools_memcpy(a, &n, sizeof(a));  /* 2.25.1 tweak */
  for (i=1; i < numints; i++) a[0] += a[i];
  tools_memcpy(b, &m, sizeof(b));
  for (i=0; i < numints; i++) a[0] += b[i];
  return hashmod(t, a[0]);
}


/*
** returns the `main' position of an element in a set (that is, the index
** of its hash value)
*/
static LNode *mainposition (const UltraSet *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return hashnum(t, nvalue(key));
    case LUA_TSTRING:
      return hashstr(t, rawtsvalue(key));
    case LUA_TBOOLEAN:
      return hashboolean(t, bvalue(key));
    case LUA_TCOMPLEX: {  /* 6.8.1 fix */
#ifndef PROPCMPLX
      agn_Complex z = cvalue(key);
      return hashcmplx(t, creal(z), cimag(z));
#else
      return hashcmplx(t, (cvalue(key))[0], (cvalue(key))[1]);
#endif
    }
    case LUA_TLIGHTUSERDATA:
      return hashpointer(t, pvalue(key));
    default:
      return hashpointer(t, gcvalue(key));
  }
}


/*
** returns the index of a `key' for set traversals. The
** beginning of a traversal is signalled by -1.
*/
LUAI_FUNC int findsetindex (lua_State *L, UltraSet *t, StkId key) {
  int i;
  LNode *n;
  if (ttisnil(key)) return -1;  /* first iteration */
  n = mainposition(t, key);
  do {  /* check whether `key' is somewhere in the chain */
    /* key may be dead already, but it is ok to use it in `next' */
    if (luaO_rawequalObj(key2tval(n), key) ||
      (ttype(glkey(n)) == LUA_TDEADKEY && iscollectable(key) &&
      gcvalue(glkey(n)) == gcvalue(key))) {
        i = cast_int(n - glnode(t, 0));  /* key index in hash table */
        /* hash elements are numbered after array ones */
        return i;
    }
    else n = glnext(n);
  } while (n);
  luaG_runerror(L, "invalid key to " LUA_QL("nextone"));  /* key not found */
  return 0;  /* to avoid warnings */
}


LUAI_FUNC int agnUS_next (lua_State *L, UltraSet *t, StkId key) {
  LNode *n;
  TValue *v;
  int i = findsetindex(L, t, key);  /* find original element */
  for (i++; i < sizenode(t); i++) {  /* try hash part */
    n = glnode(t, i);
    if (!ttisnil(glkey(n))) {  /* a non-nil value? */
      v = key2tval(n);  /* 2.31.2 change */
      setobj2s(L, key, v);
      setobj2s(L, key + 1, v);
      return 1;
    }
  }
  return 0;  /* no more elements */
}


/*
** {=============================================================
** Rehash
** ==============================================================
*/

static int numusehash (const UltraSet *t) {
  int totaluse = 0;  /* total number of elements */
  int i = sizenode(t);
  while (i--) {
    if (!ttisnil(glkey(&t->node[i]))) {
      totaluse++;
    }
  }
  return totaluse;
}


static void setnodevector (lua_State *L, UltraSet *t, int size) {
  int lsize;
  if (size == 0) {  /* no elements to hash part? */
    t->node = cast(LNode *, dummylnode);  /* use common `dummynode' */
    lsize = 0;
  }
  else {
    int i;
    lsize = ceillog2(size);
    if (lsize > MAXBITS)
      luaG_runerror(L, "set overflow");
    size = twoto(lsize);
    t->node = luaM_newvector(L, size, LNode);
    for (i=0; i < size; i++) {
      LNode *n = glnode(t, i);
      glnext(n) = NULL;
      setnilvalue(glkey(n));
    }
  }
  t->lsizenode = cast_byte(lsize);
  t->lastfree = glnode(t, size);  /* all positions are free */
}


static void resize (lua_State *L, UltraSet *t, int nhsize) {
  int i, c;
  int oldhsize = t->lsizenode;
  LNode *nold = t->node;  /* save old hash ... */
  /* create new hash part with appropriate size */
  setnodevector(L, t, nhsize);
  /* re-insert elements from hash part */
  c = 0;
  for (i = twoto(oldhsize) - 1; i >= 0; i--) {
    LNode *old = nold + i;
    if (!ttisnil(glkey(old))) {
      agnUS_set(L, t, key2tval(old));
      c++;
    }
  }
  t->size = c;  /* 2.31.2 patch as agnUS_set not always increases t->size. */
  if (nold != dummylnode)
    luaM_freearray(L, nold, twoto(oldhsize), LNode);  /* free old array */
}


LUAI_FUNC void agnUS_resize (lua_State *L, UltraSet *t, int nsize) {
  resize(L, t, nsize);
}


static void rehash (lua_State *L, UltraSet *t, const TValue *ek) {
  int totaluse;
  totaluse = numusehash(t);  /* count keys in hash part */
  totaluse++;
  resize(L, t, totaluse);
}


/*
** }=============================================================
*/


LUAI_FUNC UltraSet *agnUS_new (lua_State *L, int nhash) {
  UltraSet *t = luaM_new(L, UltraSet);
  luaC_link(L, obj2gco(t), LUA_TSET);
  t->flags = cast_byte(~0);
  /* temporary values (kept only if some malloc fails) */
  t->lsizenode = 0;
  t->readonly = 0;
  t->node = cast(LNode *, dummylnode);
  t->size = 0;
  t->metatable = NULL;
  t->type = NULL;
  setnodevector(L, t, nhash < 1 ? 1 : nhash);  /* 2.31.1 fix */
  return t;
}


LUAI_FUNC void agnUS_free (lua_State *L, UltraSet *t) {
  if (t->node != dummylnode) {
    luaM_freearray(L, t->node, sizenode(t), LNode);
  }
  luaM_free(L, t);
}


static LNode *getfreepos (UltraSet *t) {
  while (t->lastfree-- > t->node) {
    if (ttisnil(glkey(t->lastfree)))
      return t->lastfree;
  }
  return NULL;  /* could not find a free place */
}


/*
** inserts a new key into a hash table; first, check whether key's main
** position is free. If not, check whether colliding node is in its main
** position or not: if it is not, move colliding node to an empty place and
** put new key in its main position; otherwise (colliding node is in its main
** position), new key goes to an empty position.
*/
static TValue *newkey (lua_State *L, UltraSet *t, const TValue *key) {
  LNode *mp = mainposition(t, key);
  if (!ttisnil(glkey(mp)) || mp == dummylnode) {
    LNode *othern;
    LNode *n = getfreepos(t);  /* get a free place */
    if (n == NULL) {  /* cannot find a free place? */
      rehash(L, t, key);  /* grow set */
      return agnUS_set(L, t, key);  /* re-insert key into grown set */
    }
    lua_assert(n != dummylnode);
    othern = mainposition(t, key2tval(mp));
    if (othern != mp) {  /* is colliding node out of its main position? */
      /* yes; move colliding node into free position */
      while (glnext(othern) != mp) othern = glnext(othern);  /* find previous */
      glnext(othern) = n;  /* redo the chain with `n' in place of `mp' */
      *n = *mp;  /* copy colliding node into free pos. (mp->next also goes) */
      glnext(mp) = NULL;  /* now `mp' is free */
      setnilvalue(glkey(mp));
    }
    else {  /* colliding node is in its own main position */
      /* new node will go into free position */
      glnext(n) = glnext(mp);  /* chain new position */
      glnext(mp) = n;
      mp = n;
    }
  }
  glkey(mp)->value = key->value; glkey(mp)->tt = key->tt;
  luaC_barrierset(L, t, key);
  lua_assert(ttisnil(glkey(mp)));
  t->size++;
  return key2tval(mp);
}


/* delete a key from an UltraSet; 0.10.0, April 07, 2008; patched 0.13.3, April 04, 2009 */
LUAI_FUNC int agnUS_delete (lua_State *L, UltraSet *t, const TValue *key) {
  LNode *n = mainposition(t, key);  /* node to be deleted */
  LNode *othern;
  if (t->readonly) luaG_runerror(L, "set is read-only.");
  /* 0.13.3 patch: not the key has been checked but an item in the set, so some elements will be missed.
     if (ttisnil(glkey(n)) || n == dummylnode) { */
  if (ttisnil(key) || n == dummylnode) {
    return 0;  /* key is null or set is empty -> false */
  } else {  /* key is not null / set is filled */
    lua_assert(n != dummylnode);
    do {  /* check whether `key' is somewhere in the chain */
      if (luaO_rawequalObj(key2tval(n), key)) {
        othern = mainposition(t, key2tval(n));
        if (othern != n) {  /* node to be deleted (n) is not in its main position? */
          while (glnext(othern) != n) othern = glnext(othern);  /* find previous */
          if (glnext(n) != NULL)  /* no successor to n (the one to be deleted) */
            glnext(othern) = glnext(n);  /* link successor to precursor */
          setnilvalue(glkey(n));
        }
        else {  /* key to be deleted is in its own main position */
          if (glnext(n) == NULL) {  /* no successor ? */
            setnilvalue(glkey(n));
          } else {  /* there is a successor */
            LNode *nextn = glnext(n);  /* save successor to n */
            setnilvalue(glkey(n));  /* remove key (n) */
            n = nextn;  /* move successor into key (n) former position */
          }
        }
        t->size--;
        return 1;
      }
      else
        n = glnext(n);
    } while (n);
    return 2;  /* -> key is in UltraSet, but was not found, should not happen -> fail */
  }
}


/*
** search function for numbers
*/
LUAI_FUNC int agnUS_getnum (UltraSet *t, lua_Number key) {
  lua_Number nk = cast_num(key);
  LNode *n = hashnum(t, nk);
  do {  /* check whether `key' is somewhere in the chain */
    if (ttisnumber(glkey(n)) && luai_numeq(nvalue(glkey(n)), nk))
      return 1;
    else n = glnext(n);
  } while (n);
  return 0;
}


#ifndef PROPCMPLX
LUAI_FUNC int agnUS_getcomplex (UltraSet *t, agn_Complex key) {
  agn_Complex nk = (complex double)(key);
  LNode *n = hashcmplx(t, creal(nk), cimag(nk));
  do {  /* check whether `key' is somewhere in the chain */
    agn_Complex z = cvalue(glkey(n));
    if (ttiscomplex(glkey(n)) && luai_numeq(creal(z), creal(nk)) && luai_numeq(cimag(z), cimag(nk)))
      return 1;
    else n = glnext(n);
  } while (n);
  return 0;
}
#else
LUAI_FUNC int agnUS_getcomplex (UltraSet *t, const lua_Number key[]) {
  lua_Number nk[2];
  nk[0] = cast_num(key[0]);
  nk[1] = cast_num(key[1]);
  LNode *n = hashcmplx(t, nk[0], nk[1]);
  do {  /* check whether `key' is somewhere in the chain */
    lua_Number re = complexreal(glkey(n));
    lua_Number im = compleximag(glkey(n));
    if (ttiscomplex(glkey(n)) && luai_numeq(re, nk[0]) && luai_numeq(im, nk[1]))
      return 1;
    else n = glnext(n);
  } while (n);
  return 0;
}
#endif


/*
** search function for strings
*/
LUAI_FUNC int agnUS_getstr (UltraSet *t, TString *key) {
  if (key == NULL) return 0;  /* 2.20.0 */
  LNode *n = hashstr(t, key);
  do {  /* check whether `key' is somewhere in the chain */
    if (ttisstring(glkey(n)) && rawtsvalue(glkey(n)) == key)
      return 1;
    else n = glnext(n);
  } while (n);
  return 0;
}


/*
** main search function
*/
LUAI_FUNC int agnUS_get (UltraSet *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TSTRING: return agnUS_getstr(t, rawtsvalue(key));
    case LUA_TNUMBER: {
      return agnUS_getnum(t, nvalue(key));  /* use specialized version */
      /* else go through */
    }
    case LUA_TCOMPLEX: {
      return agnUS_getcomplex(t, cvalue(key));
    }
    case LUA_TNIL: return 0;
    default: {
      LNode *n = mainposition(t, key);
      do {  /* check whether `key' is somewhere in the chain */
        if (luaO_rawequalObj(key2tval(n), key))
          return 1;
        else n = glnext(n);
      } while (n);
      return 0;
    }
  }
}


/*
** Insertion and deletion
*/

LUAI_FUNC TValue *agnUS_set (lua_State *L, UltraSet *t, const TValue *key) {
  int p = agnUS_get(t, key);
  t->flags = 0;
  if (t->readonly) luaG_runerror(L, "set is read-only.");
  if (p) {
    return cast(TValue *, key);
  } else {
    if (ttisnil(key)) luaG_runerror(L, "set entry is null");
    return newkey(L, t, key);
  }
}


LUAI_FUNC TValue *agnUS_setnum (lua_State *L, UltraSet *t, lua_Number key) {
  int p = agnUS_getnum(t, key);
  if (t->readonly) luaG_runerror(L, "set is read-only.");
  if (p) {
    return key2tval(hashnum(t, cast_num(key)));
  }
  else {
    TValue k;
    setnvalue(&k, cast_num(key));
    return newkey(L, t, &k);
  }
}


/* This is equivalent to:
     TValue o;
     setsvalue(L, &o, varname);
     key = agnUS_set(L, t, &o);

   Usage:
     TValue *idx;
     idx = agnUS_setstr(L, t, key);
     agnUS_set(L, t, idx);
     luaC_barrierset(L, t, idx); */

LUAI_FUNC TValue *agnUS_setstr (lua_State *L, UltraSet *t, TString *key) {
  int p = agnUS_getstr(t, key);
  if (t->readonly) luaG_runerror(L, "set is read-only.");
  if (p)
    return key2tval(hashstr(t, key));
  else {
    TValue k;
    setsvalue(L, &k, key);
    return newkey(L, t, &k);
  }
}


/* Stores a TString into a set and also registers it with the garbage collector. To convert a TValue into TString,
   use rawtsvalue(), and getstr() to convert from TString to const char*. */
LUAI_FUNC void agnUS_setstr2set (lua_State *L, UltraSet *t, TString *str) {  /* 2.20.1 */
  TValue *key = agnUS_setstr(L, t, str);
  if (t->readonly) luaG_runerror(L, "set is read-only.");
  agnUS_set(L, t, key);
  luaC_barrierset(L, t, key);
}


LUAI_FUNC void agnUS_delstr (lua_State *L, UltraSet *t, TString *str) {  /* 2.20.0 */
  if (t->readonly) luaG_runerror(L, "set is read-only.");
  if (agnUS_getstr(t, str)) {
    TValue o;
    setsvalue(L, &o, str);
    agnUS_delete(L, t, &o);  /* luaC_barrierset(L, t, &o);  not needed */
  }
}


LUAI_FUNC int agnUS_readonly (lua_State *L, UltraSet *t, int readonly) {  /* 4.8.2 */
  if (readonly < 0) return t->readonly;
  t->readonly = readonly;
  return readonly;
}


#if defined(LUA_DEBUG)

LNode *agnUS_mainposition (const UltraSet *t, const TValue *key) {
  return mainposition(t, key);
}

int agnUS_isdummy (LNode *n) { return n == dummylnode; }

#endif

