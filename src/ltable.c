/*
** $Id: ltable.c,v 2.32 2006/01/18 11:49:02 roberto Exp $
** Lua/Agena tables (hash)
** See Copyright Notice in agena.h
*/

/*
** Implementation of tables (aka arrays, objects, or hash tables).
** Tables keep its elements in two parts: an array part and a hash part.
** Non-negative integer keys are all candidates to be kept in the array
** part. The actual size of the array is the largest `n' such that at
** least half the slots between 0 and n are in use.
** Hash uses a mix of chained scatter table with Brent's variation.
** A main invariant of these tables is that, if an element is not
** in its main position (i.e. the `original' position that its hash gives
** to it), then the colliding element is in its own main position.
** Hence even when the load factor reaches 100%, performance remains good.
*/

#include <stdio.h>
#include <string.h>

#define ltable_c
#define LUA_CORE

#include "agena.h"

#include "agnhlps.h"
#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"
#include "lvm.h"

/*
** max size of array part is 2^MAXBITS
*/
#if LUAI_BITSINT > 26
#define MAXBITS    26
#else
#define MAXBITS    (LUAI_BITSINT - 2)
#endif

#define MAXASIZE   (1 << MAXBITS)

/*
** number of ints inside a lua_Number
*/
#define numints      cast_int(sizeof(lua_Number)/sizeof(int))

/*
** hash for lua_Numbers
*/
LUAI_FUNC Node *luaH_hashnum (const Table *t, lua_Number n) {
  unsigned int a[numints];
  int i;
  /* n += 1; */ /* normalize number (avoid -0) */
  /* lua_assert(sizeof(a) <= sizeof(n)); */
  if (luai_numeq(n, 0))  /* 5.1.3 patch; avoid problems with -0 */
    return gnode(t, 0);
  tools_memcpy(a, &n, sizeof(a));  /* 2.25.1 tweak */
  for (i=1; i < numints; i++) a[0] += a[i];
  return luaH_hashmod(t, a[0]);
}

/*
** MurmurHash3 Finalizer mixer:
** Scrambles pointer bits to ensure uniform distribution in power-of-2 tables.
** This prevents clustering when pointers are allocated sequentially.
*/
LUAI_FUNC Node *luaH_hashpointer (const Table *t, const void *p) {
  uintptr_t h = (uintptr_t)p;
  h ^= h >> 16;
  h *= 0x85ebca6bU;
  h ^= h >> 13;
  h *= 0xc2b2ae35U;
  h ^= h >> 16;
  return gnode(t, lmod(h, sizenode(t)));
}

/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/
Node *luaH_mainposition (const Table *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return luaH_hashnum(t, nvalue(key));
    case LUA_TSTRING:
      return luaH_hashstr(t, rawtsvalue(key));
    case LUA_TBOOLEAN:
      return luaH_hashboolean(t, bvalue(key));
    case LUA_TLIGHTUSERDATA:
      return luaH_hashpointer(t, pvalue(key)); /* Added 't' */
    default:
      return luaH_hashpointer(t, gcvalue(key)); /* Added 't' */
  }
}

/*
** returns the index for `key' if `key' is an appropriate key to live in
** the array part of the table, -1 otherwise.
*/
static int arrayindex (const TValue *key) {
  if (ttisnumber(key)) {
    lua_Number n = nvalue(key);
    int k;
    lua_number2int(k, n);
    if (luai_numeq(cast_num(k), n))
      return k;
  }
  return -1;  /* `key' did not match some condition */
}


/*
** returns the index of a `key' for table traversals. First goes all
** elements in the array part, then elements in the hash part. The
** beginning of a traversal is signalled by -1.
*/
LUAI_FUNC int luaH_findindex (lua_State *L, Table *t, StkId key) {
  int i;
  if (ttisnil(key)) return -1;  /* first iteration */
  i = arrayindex(key);
  if (0 < i && i <= t->sizearray)  /* is `key' inside array part? */
    return i - 1;  /* yes; that's the index (corrected to C) */
  else {
    Node *n = luaH_mainposition(t, key);
    do {  /* check whether `key' is somewhere in the chain */
      /* key may be dead already, but it is ok to use it in `next' */
      if (luaO_rawequalObj(key2tval(n), key) ||
            (ttype(gkey(n)) == LUA_TDEADKEY && iscollectable(key) &&
             gcvalue(gkey(n)) == gcvalue(key))) {
        i = cast_int(n - gnode(t, 0));  /* key index in hash table */
        /* hash elements are numbered after array ones */
        return i + t->sizearray;
      }
      else n = gnext(n);
    } while (n);
    luaG_runerror(L, "invalid key to " LUA_QL("nextone"));  /* key not found */
    return 0;  /* to avoid warnings */
  }
}


LUAI_FUNC int luaH_next (lua_State *L, Table *t, StkId key) {
  int i = luaH_findindex(L, t, key);  /* find original element */
  for (i++; i < t->sizearray; i++) {  /* try first array part */
    if (!ttisnil(&t->array[i])) {  /* a non-nil value? */
      setnvalue(key, cast_num(i + 1));
      setobj2s(L, key + 1, &t->array[i]);
      return 1;
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {  /* then hash part */
    if (!ttisnil(gval(gnode(t, i)))) {  /* a non-nil value? */
      setobj2s(L, key, key2tval(gnode(t, i)));
      setobj2s(L, key + 1, gval(gnode(t, i)));
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


static int computesizes (int nums[], int *narray) {
  int i;
  int twotoi;  /* 2^i */
  int a = 0;   /* number of elements smaller than 2^i */
  int na = 0;  /* number of elements to go to array part */
  int n = 0;   /* optimal size for array part */
  for (i=0, twotoi=1; twotoi/2 < *narray; i++, twotoi *= 2) {
    if (nums[i] > 0) {
      a += nums[i];
      if (a > twotoi/2) {  /* more than half elements present? */
        n = twotoi;  /* optimal size (till now) */
        na = a;  /* all elements smaller than n will go to array part */
      }
    }
    if (a == *narray) break;  /* all elements already counted */
  }
  *narray = n;
  lua_assert(*narray/2 <= na && na <= *narray);
  return na;
}


static int countint (const TValue *key, int *nums) {
  int k = arrayindex(key);
  if (0 < k && k <= MAXASIZE) {  /* is `key' an appropriate array index? */
    nums[ceillog2(k)]++;  /* count as such */
    return 1;
  }
  else
    return 0;
}


static int numusearray (const Table *t, int *nums) {
  int lg;
  int ttlg;  /* 2^lg */
  int ause = 0;  /* summation of `nums' */
  int i = 1;  /* count to traverse all array keys */
  for (lg=0, ttlg=1; lg <= MAXBITS; lg++, ttlg *= 2) {  /* for each slice */
    int lc = 0;  /* counter */
    int lim = ttlg;
    if (lim > t->sizearray) {
      lim = t->sizearray;  /* adjust upper limit */
      if (i > lim)
        break;  /* no more elements to count */
    }
    /* count elements in range (2^(lg-1), 2^lg] */
    for (; i <= lim; i++) {
      if (!ttisnil(&t->array[i - 1]))
        lc++;
    }
    nums[lg] += lc;
    ause += lc;
  }
  return ause;
}


static int numusehash (const Table *t, int *nums, int *pnasize) {
  int totaluse = 0;  /* total number of elements */
  int ause = 0;  /* summation of `nums' */
  int i = sizenode(t);
  while (i--) {
    Node *n = &t->node[i];
    if (!ttisnil(gval(n))) {
      ause += countint(key2tval(n), nums);
      totaluse++;
    }
  }
  *pnasize += ause;
  return totaluse;
}


static void setarrayvector (lua_State *L, Table *t, int size) {
  int i;
  luaM_reallocvector(L, t->array, t->sizearray, size, TValue);
  for (i=t->sizearray; i < size; i++)
    setnilvalue(&t->array[i]);
  t->sizearray = size;
}


static void setnodevector (lua_State *L, Table *t, int size) {
  int lsize;
  if (size == 0) {  /* no elements to hash part? */
    t->node = cast(Node *, dummynode);  /* use common `dummynode' */
    lsize = 0;
  }
  else {
    int i;
    lsize = ceillog2(size);
    if (lsize > MAXBITS)
      luaG_runerror(L, "table overflow");
    size = twoto(lsize);
    t->node = luaM_newvector(L, size, Node);
    for (i=0; i < size; i++) {
      Node *n = gnode(t, i);
      gnext(n) = NULL;
      setnilvalue(gkey(n));
      setnilvalue(gval(n));
    }
  }
  t->lsizenode = cast_byte(lsize);
  t->lastfree = gnode(t, size);  /* all positions are free */
}


typedef struct {
  Table *t;
  unsigned int nhsize;
} AuxsetnodeT;

static void auxsetnode (lua_State *L, void *ud) {
  AuxsetnodeT *asn = cast(AuxsetnodeT *, ud);
  setnodevector(L, asn->t, asn->nhsize);
}

/*
** This comment has been taken from Lua 5.4.6 and seems to best describe what is happening here.
**
** Resize table 't' for the new given sizes. Both allocations (for
** the hash part and for the array part) can fail, which creates some
** subtleties. If the first allocation, for the hash part, fails, an
** error is raised and that is it. Otherwise, it copies the elements from
** the shrinking part of the array (if it is shrinking) into the new
** hash. Then it reallocates the array part.  If that fails, the table
** is in its original state; the function frees the new hash part and then
** raises the allocation error. Otherwise, it sets the new hash part
** into the table, [initializes the new part of the array (if any) with
** nils] and reinserts the elements of the old hash back into the new
** parts of the table.
*/

static void resize (lua_State *L, Table *t, int nasize, int nhsize) {
  AuxsetnodeT asn;
  int i;
  int oldasize = t->sizearray;
  int oldhsize = t->lsizenode;
  Node *nold = t->node;  /* save old hash ... */
  if (nasize > oldasize)  /* array part must grow? */
    setarrayvector(L, t, nasize);
  /* create new hash part with appropriate size */
  /* setnodevector(L, t, nhsize); */
  asn.t = t; asn.nhsize = nhsize;  /* Lua 5.3.4 patch 7 / 2.29.3 */
  if (luaD_rawrunprotected(L, auxsetnode, &asn) != 0) {  /* mem. error? */
    setarrayvector(L, t, oldasize);  /* array back to its original size */
    luaD_throw(L, LUA_ERRMEM);  /* rethrow memory error */
  }
  if (nasize < oldasize) {  /* array part must shrink? */
    t->sizearray = nasize;  /* pretend array has new size */
    /* re-insert into the hash the elements from vanishing slice */
    for (i=nasize; i < oldasize; i++) {
      if (!ttisnil(&t->array[i]))
        luaH_setint(L, t, i + 1, &t->array[i]);  /* 4.6.3 tweak */
    }
    /* shrink array */
    luaM_reallocvector(L, t->array, oldasize, nasize, TValue);
  }
  /* re-insert elements from hash part into newly aligned parts */
  for (i = twoto(oldhsize) - 1; i >= 0; i--) {
    Node *old = nold + i;
    if (!ttisnil(gval(old)))
      /* doesn't need barrier/invalidate cache, as entry was already present in the table */
      setobjt2t(L, luaH_set(L, t, key2tval(old)), gval(old));
  }
  if (nold != dummynode)  /* not the dummy node? */
    luaM_freearray(L, nold, twoto(oldhsize), Node);  /* free old hash */
}


/* Do not call if the array part has holes. */
LUAI_FUNC void luaH_resizearray (lua_State *L, Table *t, int nasize) {
  int nhsize = (t->node == dummynode) ? 0 : sizenode(t);
  resize(L, t, nasize, nhsize);
}


static void rehash (lua_State *L, Table *t, const TValue *ek) {
  int nasize, na, i, totaluse, nums[MAXBITS + 1];  /* nums[i] = number of keys between 2^(i-1) and 2^i */
  for (i=0; i <= MAXBITS; i++) nums[i] = 0;  /* reset counts */
  nasize = numusearray(t, nums);  /* count keys in array part */
  totaluse = nasize;  /* all those keys are integer keys */
  totaluse += numusehash(t, nums, &nasize);  /* count keys in hash part */
  /* count extra key */
  nasize += countint(ek, nums);
  totaluse++;
  /* compute new size for array part */
  na = computesizes(nums, &nasize);
  /* resize the table to new computed sizes */
  resize(L, t, nasize, totaluse - na);
}


/*
** }=============================================================
*/

LUAI_FUNC Table *luaH_new (lua_State *L, int narray, int nhash) {
  Table *t = luaM_new(L, Table);
  luaC_link(L, obj2gco(t), LUA_TTABLE);
  t->metatable = NULL;
  t->flags = cast_byte(~0);
  /* temporary values (kept only if some malloc fails) */
  t->array = NULL;
  t->sizearray = 0;
  t->lsizenode = 0;
  t->readonly = 0;
  t->type = NULL;
  t->node = cast(Node *, dummynode);
  setarrayvector(L, t, narray);
  setnodevector(L, t, nhash);
  return t;
}


LUAI_FUNC void luaH_free (lua_State *L, Table *t) {
  if (t->node != dummynode)
    luaM_freearray(L, t->node, sizenode(t), Node);
  luaM_freearray(L, t->array, t->sizearray, TValue);
  luaM_free(L, t);
}


static Node *getfreepos (Table *t) {
  while (t->lastfree-- > t->node) {
    if (ttisnil(gkey(t->lastfree)))
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
LUAI_FUNC TValue *luaH_newkey (lua_State *L, Table *t, const TValue *key) {
  Node *mp;
  mp = luaH_mainposition(t, key);
  if (!ttisnil(gval(mp)) || mp == dummynode) {
    Node *othern;
    Node *n = getfreepos(t);  /* get a free place */
    if (n == NULL) {  /* cannot find a free place? */
      rehash(L, t, key);  /* grow table */
      return luaH_set(L, t, key);  /* re-insert key into grown table */
    }
    lua_assert(n != dummynode);
    othern = luaH_mainposition(t, key2tval(mp));
    if (othern != mp) {  /* is colliding node out of its main position? */
      /* yes; move colliding node into free position */
      while (gnext(othern) != mp) othern = gnext(othern);  /* find previous */
      gnext(othern) = n;  /* redo the chain with `n' in place of `mp' */
      *n = *mp;  /* copy colliding node into free pos. (mp->next also goes) */
      gnext(mp) = NULL;  /* now `mp' is free */
      setnilvalue(gval(mp));
    }
    else {  /* colliding node is in its own main position */
      /* new node will go into free position */
      gnext(n) = gnext(mp);  /* chain new position */
      gnext(mp) = n;
      mp = n;
    }
  }
  gkey(mp)->value = key->value; gkey(mp)->tt = key->tt;
  luaC_barriert(L, t, key);
  lua_assert(ttisnil(gval(mp)));
  return gval(mp);
}


/*
** search functions for integers: positive, 0, negative [sic !]
*/
LUAI_FUNC const TValue *luaH_getnum (Table *t, int key) {
  /* (1 <= key && key <= t->sizearray) */
  if (cast(unsigned int, key - 1) < cast(unsigned int, t->sizearray))
    return &t->array[key - 1];
  else {
    lua_Number nk = cast_num(key);
    Node *n = luaH_hashnum(t, nk);
    do {  /* check whether `key' is somewhere in the chain */
      if (ttisnumber(gkey(n)) && luai_numeq(nvalue(gkey(n)), nk))
        return gval(n);  /* that's it */
      else n = gnext(n);
    } while (n);
    return luaO_nilobject;
  }
}


LUAI_FUNC const TValue *luaH_gethashnum (Table *t, int key) {
  lua_Number nk = cast_num(key);
  Node *n = luaH_hashnum(t, nk);
  do {  /* check whether `key' is somewhere in the chain */
    if (ttisnumber(gkey(n)) && luai_numeq(nvalue(gkey(n)), nk))
      return gval(n);  /* that's it */
    else n = gnext(n);
  } while (n);
  return luaO_nilobject;
}


/*
** search function for strings
*/
LUAI_FUNC const TValue *luaH_getstr (Table *t, TString *key) {
  Node *n = luaH_hashstr(t, key);
  do {  /* check whether `key' is somewhere in the chain */
    if (ttisstring(gkey(n)) && rawtsvalue(gkey(n)) == key)
      return gval(n);  /* that's it */
    else n = gnext(n);
  } while (n);
  return luaO_nilobject;
}


/*
** main search function
*/

LUAI_FUNC const TValue *luaH_get (Table *t, const TValue *key) {
  switch (ttype(key)) {
    /* Keep the order of the case clauses unchanged ! Changed 7.3.9 */
    case LUA_TNIL: return luaO_nilobject;
    case LUA_TSTRING: {
      TString *ts = rawtsvalue(key);
      Node *n = luaH_hashstr(t, ts);
      do {  /* check whether `key' is somewhere in the chain */
        if (ttisstring(gkey(n)) && rawtsvalue(gkey(n)) == rawtsvalue(key))
          return gval(n);  /* that's it */
        else n = gnext(n);
      } while (n);
      return luaO_nilobject;
    }
    case LUA_TNUMBER: {
      int k;
      lua_Number n = nvalue(key);
      lua_number2int(k, n);
      if (luai_numeq(cast_num(k), n)) {  /* index is int? */
        if (cast(unsigned int, k - 1) < cast(unsigned int, t->sizearray))
          return &t->array[k - 1];
        else
          return luaH_gethashnum(t, k);  /* fallback to hash-part for out-of-bounds, 7.3.9 change */
      }
      /* else go through */
    }
    default: {
      Node *n = luaH_mainposition(t, key);
      do {  /* check whether `key' is somewhere in the chain */
        if (luaO_rawequalObj(key2tval(n), key))
          return gval(n);  /* that's it */
        else n = gnext(n);
      } while (n);
      return luaO_nilobject;
    }
  }
}


LUAI_FUNC TValue *luaH_set (lua_State *L, Table *t, const TValue *key) {
  const TValue *p = luaH_get(t, key);
  t->flags = 0;
  if (t->readonly) luaG_runerror(L, "table is read-only.");
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else {
  	/* Proposed by Gemini AI, obviously based on LuaJIT or other forks, 7.3.9 */
    if (ttisnumber(key)) { /* is it a potential array key ? 7.3.9 change */
      int k;
      lua_Number n = nvalue(key);
      if (luai_numisnan(n)) luaG_runerror(L, "table index is `undefined`");  /* Agena 1.8.4 */
      lua_number2int(k, n);
      if (luai_numeq(cast_num(k), n) && 1 <= k && k <= t->sizearray)
        return &t->array[k - 1]; /* found in array part! */
    }
    if (ttisnil(key)) luaG_runerror(L, "table index is null");
    else if (ttisnumber(key) && luai_numisnan(nvalue(key)))
      luaG_runerror(L, "table index is `undefined`");  /* Agena 1.8.4 */
    return luaH_newkey(L, t, key);  /* FALLBACK: only now go to hash part */
  }
}


LUAI_FUNC TValue *luaH_setnum (lua_State *L, Table *t, int key) {
  const TValue *p = luaH_getnum(t, key);
  if (t->readonly) luaG_runerror(L, "table is read-only.");
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else {
    TValue k;
    setnvalue(&k, cast_num(key));
    return luaH_newkey(L, t, &k);
  }
}


LUAI_FUNC void luaH_setint (lua_State *L, Table *t, int key, TValue *value) {  /* taken from Lua 5.2.4 */
  const TValue *p = luaH_getnum(t, key);
  TValue *cell;
  if (t->readonly) luaG_runerror(L, "table is read-only.");
  if (p != luaO_nilobject)
    cell = cast(TValue *, p);
  else {
    TValue k;
    setnvalue(&k, cast_num(key));
    cell = luaH_newkey(L, t, &k);
  }
  setobj2t(L, cell, value);
}


LUAI_FUNC TValue *luaH_setstr (lua_State *L, Table *t, TString *key) {
  const TValue *p;
  if (t->readonly) luaG_runerror(L, "table is read-only.");
  p = luaH_getstr(t, key);
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else {
    TValue k;
    setsvalue(L, &k, key);
    return luaH_newkey(L, t, &k);
  }
}


static FORCE_INLINE int unbound_search (Table *t, unsigned int j) {
  unsigned int i = j;  /* i is zero or a present index */
  j++;
  /* find `i' and `j' such that i is present and j is not */
  while (!ttisnil(luaH_getnum(t, j))) {
    i = j;
    j *= 2;
    if (j > cast(unsigned int, MAX_INT)) {  /* overflow? */
      /* table was built with bad purposes: resort to linear search */
      i = 1;
      while (!ttisnil(luaH_getnum(t, i))) i++;
      return i - 1;
    }
  }
  /* now do a binary search between them */
  while (j - i > 1) {
    unsigned int m = tools_midpoint(i, j);  /* 2.38.2 patch */
    if (ttisnil(luaH_getnum(t, m))) j = m;
    else i = m;
  }
  return i;
}


/*
** Try to find a boundary in table `t'. A `boundary' is an integer index
** such that t[i] is non-nil and t[i+1] is nil (and 0 if t[1] is nil).
*/
LUAI_FUNC int luaH_getn (Table *t) {
  unsigned int j = t->sizearray;
  if (j > 0 && ttisnil(&t->array[j - 1])) {
    /* there is a boundary in the array part: (binary) search for it */
    unsigned int i = 0;
    while (j - i > 1) {
      unsigned int m = tools_midpoint(i, j);  /* 2.38.2 patch */
      if (ttisnil(&t->array[m - 1])) j = m;
      else i = m;
    }
    return i;
  }
  /* else must find a boundary in hash part */
  else if (t->node == dummynode)  /* hash part is empty? */
    return j;  /* that is easy... */
  else return unbound_search(t, j);
}


int luaH_isdummy (Node *n) { return n == dummynode; }


/*
** functions added for Agena
*/

/* iterate a table and put the key, the value (if mode = 1), the function f, and then again the value on stack */
LUAI_FUNC int luaHF_next (lua_State *L, Table *t, TValue *f, StkId key, int mode) {
  TValue *val;
  Node *n;
  int i = luaH_findindex(L, t, key);  /* find original element */
  for (i++; i < t->sizearray; i++) {  /* try first array part */
    val = &t->array[i];
    if (!ttisnil(val)) {  /* a non-nil value? */
      setnvalue(key++, cast_num(i + 1));
      if (mode) setobj2s(L, key++, val);
      setobj2s(L, key++, f);
      setobj2s(L, key, val);
      return 1;
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {  /* then hash part */
    n = gnode(t, i);
    val = gval(n);
    if (!ttisnil(val)) {  /* a non-nil value? */
      setobj2s(L, key++, key2tval(n));
      if (mode) setobj2s(L, key++, val);
      setobj2s(L, key++, f);
      setobj2s(L, key, val);
      return 1;
    }
  }
  return 0;  /* no more elements */
}


LUAI_FUNC void agnH_nops (lua_State *L, Table *t, size_t a[]) {
  int i, isnil, firstnil, lastnonnil;
  firstnil = t->sizearray;
  a[0] = a[1] = isnil = lastnonnil = 0;
  /* iterate simple table of the form {v1, v2, ...}, not {1~v1, ...} ;
     if t is a dictionary, this loop is also skipped */
  for (i=0; i < t->sizearray; i++) {
    if (ttisnotnil(&t->array[i])) {
      a[0]++;
      lastnonnil = i;  /* 0.25.2 patch */
    }
    else if (isnil == 0) {
      isnil = 1;
      firstnil = i;    /* 0.25.2 patch */
    }
  }
  /* 0.22.0, array has a hole ?, fixed 0.25.2 */
  a[2] = firstnil < lastnonnil;
  /* iterate dictionary; if t is a simple table only one iteration is done */
  for (i -= t->sizearray; i < sizenode(t); i++) {
    if (ttisnotnil(gval(gnode(t, i)))) a[1]++;
  }
}


LUAI_FUNC int agnH_hasarraypart (lua_State *L, Table *t) {  /* 3.10.0 */
  int i;
  api_check(L, L->top < L->ci->top);
  for (i=0; i < t->sizearray; i++) {
    if (ttisnotnil(&t->array[i])) return 1;
  }
  return 0;
}


LUAI_FUNC int agnH_hashashpart (lua_State *L, Table *t) {  /* 3.10.0 */
  int i;
  api_check(L, L->top < L->ci->top);
  for (i=0; i < sizenode(t); i++) {  /* 4.10.2 fix */
    if (ttisnotnil(gval(gnode(t, i)))) return 1;
  }
  return 0;
}


LUAI_FUNC int agnH_hasholes (lua_State *L, Table *t, int *lastnonnil) {  /* 2.29.0 */
  int i, isnil, firstnil;
  firstnil = t->sizearray;
  isnil = *lastnonnil = 0;
  api_check(L, L->top < L->ci->top);
  /* iterate simple table of the form {v1, v2, ...}, not {1~v1, ...} ;
     if t is a dictionary, this loop is also skipped */
  for (i=0; i < t->sizearray; i++) {
    if (ttisnotnil(&t->array[i])) {
      *lastnonnil = i;
    }
    else if (isnil == 0) {
      isnil = 1;
      firstnil = i;
    }
  }
  return firstnil < *lastnonnil;
}


LUAI_FUNC int agnH_asize (lua_State *L, Table *t) {  /* get size of array part only, 2.11.4 */
  int i, c;
  c = 0;
  api_check(L, L->top < L->ci->top);
  /* iterate simple table of the form {v1, v2, ...}, not {1~v1, ...} ;
     if t is a dictionary, this loop is also skipped */
  for (i=0; i < t->sizearray; i++) {
    if (ttisnotnil(&t->array[i])) c++;
  }
  return c;
}


LUAI_FUNC int agnH_hsize (lua_State *L, Table *t) {  /* 4.10.2 */
  int i, c;
  c = 0;
  api_check(L, L->top < L->ci->top);
  for (i=0; i < sizenode(t); i++) {  /* explicitly start from 0 for all other positions will give wrong results ! */
    if (ttisnotnil(gval(gnode(t, i)))) c++;
  }
  return c;
}


/* get smallest and largest index in array part, returns Lua indices starting at 1, 2.14.10 */
LUAI_FUNC int agnH_aborders (lua_State *L, Table *t, int *low) {
  int i;
  api_check(L, L->top < L->ci->top);
  *low = 0;  /* lowest index, findindex returns -1 at the beginning of a traversal */
  /* iterate simple table of the form {v1, v2, ...}, not {1~v1, ...} ;
     if t is a dictionary, this loop will be skipped. */
  for (i=t->sizearray - 1; i >= *low; i--) {
    if (ttisnotnil(&t->array[i])) {
      *low += 1;
      return i + 1;  /* bail out immediately with largest index of an assigned value */
    }
  }
  return 0;
}


/* get smallest and largest integral index in the array and hash part, returns Lua indices starting at 1, 2.30.1 */
LUAI_FUNC int agnH_borders (lua_State *L, Table *t, int *low) {
  int i, high;
  Node *n;
  TValue *key;
  api_check(L, L->top < L->ci->top);
  *low = 0;  /* lowest index, findindex returns -1 at the beginning of a traversal */
  high = t->sizearray;
  for (i=high - 1; i >= *low; i--) {  /* from right to left ... */
    if (ttisnotnil(&t->array[i])) {
      *low += 1;
      high = i + 1;  /* bail out immediately with largest index */
      break;
    }
  }
  for (i=0; i < sizenode(t); i++) {
    n = gnode(t, i);
    if (ttisnotnil(gval(n))) {
      int idx;
      key = key2tval(n);
      if (!ttisint(key)) continue;
      idx = (int)nvalue(key);
      if (idx < *low || *low == 0) *low = idx;
      if (idx > high || high == 0) high = idx;
    }
  }
  return high;
}


LUAI_FUNC void agnH_rotatebottom (lua_State *L, Table *t) {
  int i, last, flag;
  TValue *m, *n;
  if (t->readonly) luaG_runerror(L, "table is read-only.");
  flag = 1;
  last = 0;
  for (i=0; i < t->sizearray; i++) {
    if (ttisnotnil(&t->array[i])) {
      if (flag) {  /* fetch first element in array part */
        setobj2s(L, L->top, &t->array[i]);
        flag = 0;
        last = i;
        continue;
      }
      /* move items to the bottom */
      m = &t->array[last];
      n = &t->array[i];
      setobj(L, m, n);  /* 7.6.7 fix */
      last = i;
    }
  }
  /* set first element to the top */
  luaH_setint(L, t, last + 1, L->top);  /* 4.6.3 tweak */
  luaC_barriert(L, t, L->top);
  setnilvalue(L->top);
}


LUAI_FUNC void agnH_rotatetop (lua_State *L, Table *t) {
  int i, last, flag;
  TValue *m, *n;
  if (t->readonly) luaG_runerror(L, "table is read-only.");
  flag = 1;
  last = 0;
  for (i=t->sizearray - 1; i >= 0; i--) {
    if (ttisnotnil(&t->array[i])) {
      if (flag) {  /* fetch last element in array part */
        setobj2s(L, L->top, &t->array[i]);
        flag = 0;
        last = i;
        continue;
      }
      /* move items to the top */
      m = &t->array[i];
      n = &t->array[last];
      setobj(L, n, m);  /* 7.6.7 fix */
      last = i;
    }
  }
  /* set first element to the top */
  luaH_setint(L, t, last + 1, L->top);  /* 4.6.3 tweak */
  luaC_barriert(L, t, L->top);
  setnilvalue(L->top);
}


/* Taken from:
   https://github.com/luau-lang/luau
   file: VM/src/ltable.cpp

   Licence: MIT.

   Copyright (c) 2019-2024 Roblox Corporation
   Copyright (c) 1994–2019 Lua.org, PUC-Rio. */

LUAI_FUNC void agnH_clear (lua_State *L, Table* t) {  /* 4.9.0 */
  int i;
  if (t->readonly) luaG_runerror(L, "table is read-only.");
  /* clear array part */
  for (i=0; i < t->sizearray; ++i) {
    setnilvalue(&t->array[i]);
  }
  /* clear hash part */
  if (t->node != dummynode) {
    for (i=0; i < sizenode(t); ++i) {
      Node* n = gnode(t, i);
      setnilvalue(gkey(n));
      setnilvalue(gval(n));
      gnext(n) = 0;
    }
  }
}


LUAI_FUNC int agnH_readonly (lua_State *L, Table *t, int readonly) {  /* 4.9.0 */
  if (readonly < 0) return t->readonly;
  t->readonly = readonly;
  return readonly;
}


LUAI_FUNC int agnH_delete (lua_State *L, Table *t, const TValue *value) {  /* 5.1.2 */
  TValue *val, *oldval;
  int i, c = 0;
  for (i=0; i < t->sizearray; i++) {
    if (ttisnotnil(&t->array[i])) {
      val = &t->array[i];
      if (equalobj(L, val, value)) {
        setnilvalue(L->top);
        luaH_setint(L, t, i + 1, L->top);
        /* no need to call luaC_barriert as nil is not collectable, 7.1.8 */
        c++;
      }
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {  /* tweaked 7.1.8 */
    TValue *item = gval(gnode(t, i));
    if (ttisnotnil(item)) {
      if (equalobj(L, value, item)) {
        setobj2s(L, L->top, key2tval(gnode(t, i)));
        oldval = luaH_set(L, t, L->top);
        if (ttisnotnil(oldval)) {
          setnilvalue(L->top);
          setobj2t(L, oldval, L->top);
          /* no need to call luaC_barriert as nil is not collectable, 7.1.8 */
          c++;
        }
      }
    }
  }
  return c;
}


/* Removes a value at position i from the array part of table t and closes the space by shifting down 
   other elements, if necessary. i is one-based ! asize is the size of the array part. 
   Based on agnH_reorder. 7.6.7 */
LUAI_FUNC int agnH_purge (lua_State *L, Table *t, int i, int asize) {
  TValue *val, *dest;
  if (i < 1 || i > asize || i > t->sizearray) return 0;
  if (t->readonly) luaG_runerror(L, "table is read-only.");
  for (; i < asize; i++) {
    val = &t->array[i];       /* value to be moved left */
    dest = &t->array[i - 1];  /* position to be overwritten */
    setobj(L, dest, val);     /* copy */
  }
  val = &t->array[asize - 1];
  setnilvalue(val);           /* clear last position */
  return 1;
}


LUAI_FUNC int agnH_subs (lua_State *L, Table *t, const TValue *oldvalue, const TValue *newvalue) {  /* 5.1.3 */
  TValue *val, *oldval;
  int i, c = 0;
  for (i=0; i < t->sizearray; i++) {
    if (ttisnotnil(&t->array[i])) {
      val = &t->array[i];
      if (equalobj(L, val, oldvalue)) {
        setobj2s(L, L->top, newvalue);
        luaH_setint(L, t, i + 1, L->top);
        luaC_barriert(L, t, L->top); c++;
      }
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {
    if (ttisnotnil(gval(gnode(t, i)))) {
      L->top++;
      setobj2s(L, L->top, gval(gnode(t, i)));
      if (equalobj(L, oldvalue, L->top)) {
        setobj2s(L, L->top - 1, key2tval(gnode(t, i)));
        oldval = luaH_set(L, t, L->top - 1);
        if (ttisnotnil(oldval)) {
          setobj2s(L, L->top, newvalue);
          setobj2t(L, oldval, L->top);
          luaC_barriert(L, t, L->top); c++;
        }
      }
      L->top--;
    }
  }
  return c;
}


LUAI_FUNC void agnH_reorder (lua_State *L, Table *t, int arraypartonly) {  /* 2.30.1 */
  int i, c;
  TValue *val;
  AuxsetnodeT asn;
  int asize = t->sizearray;
  int hsize = t->lsizenode;
  Node *n = t->node;
  if (t->readonly) luaG_runerror(L, "table is read-only.");
  c = 0;
  /* extend array part to accommodate former hash elements */
  asn.t = t; asn.nhsize = twoto(1);
  if (!arraypartonly) {  /* 3.9.0 */
    if (luaD_rawrunprotected(L, auxsetnode, &asn) != 0) {  /* mem. error? */
      setarrayvector(L, t, asize);  /* set array back to its original size */
      luaD_throw(L, LUA_ERRMEM);  /* rethrow memory error */
    } else {
      /* 3.9.0, try to put elements into the array part instead of the hash part */
      setarrayvector(L, t, asize + hsize < 4 ? 4 : tools_nextpower(asize + hsize, 2, 0));
    }
  }
  /* reshuffle array part */
  for (i=0; i < asize; i++) {
    val = &t->array[i];
    if (!ttisnil(val)) {
      TValue *dest = &t->array[c++];  /* target the next clean slot, 7.3.10 tweak */
      if (dest != val) {
        setobj(L, dest, val);  /* raw copy */
        setnilvalue(val);      /* clear the old hole */
      }
    }
  }
  if (!arraypartonly) {  /* 3.9.0 */
    /* move elements from hash part to the end of array part */
    for (i=twoto(hsize) - 1; i >= 0; i--) {
      Node *old = n + i;
      val = gval(old);
      if (!ttisnil(val)) {
        /* doesn't need barrier/invalidate cache, as entry was already present in the table */
        luaH_setint(L, t, ++c, val);
      }
    }
    if (n != dummynode) {  /* not the dummy node? */
      luaM_freearray(L, n, twoto(hsize), Node);  /* free old hash */
    }
  }
  luaH_resizearray(L, t, c);
}

