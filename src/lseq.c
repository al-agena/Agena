/*
** $Id: lseq.c v 0.1, based on ltable.c v2.32, 2006/01/18 11:49:02 roberto Exp $
** Implementation of Agena Sequences
** See Copyright Notice in agena.h
*/

#define lseq_c
#define LUA_CORE

#include "agena.h"
#include "agnhlps.h"

#include "ldebug.h"
#include "lgc.h"
#include "llimits.h"   /* for MAX_INT */
#include "lmem.h"
#include "lobject.h"
#include "lseq.h"
#include "lstate.h"
#include "prepdefs.h"  /* FORCE_INLINE */

/*
** }=============================================================
*/


static FORCE_INLINE void setarrayvector (lua_State *L, Seq *t, int size) {
  int i;
  luaM_reallocvector(L, t->array, t->maxsize, size, TValue);
  for (i=t->maxsize; i < size; i++) {
    setnilvalue(seqitem(t, i));
  }
  t->maxsize = size;
}

INLINE int agnSeq_bestsize (int r) {  /* 7.1.8 change from size_t to int */
  return tools_nextpower(r, 2, 1);
}


#define SHRINKTHRESHOLD 6  /* t->size >= 2^SHRINKTHRESHOLD, here 2^6 = 64 */

/* (L->settings2 & 32) = autoshrink feature on ?
   Both tools_msb or tools_nextpowerof2 are 15 % slower. The obligatory table lookup in luaO_ceillog2 time is almost insignificant. */
#define seqtobeshrunk(t) ( \
  (t->size >> SHRINKTHRESHOLD) && \
  (L->settings2 & 32) && \
  luaO_log2(t->maxsize) != luaO_log2(t->size) \
)

/* create a new sequence */

LUAI_FUNC Seq *agnSeq_new (lua_State *L, int n) {
  Seq *t = luaM_new(L, Seq);
  luaC_link(L, obj2gco(t), LUA_TSEQ);
  t->flags = cast_byte(~0);
  t->readonly = 0;
  /* temporary values (kept only if some malloc fails) */
  t->array = NULL;
  t->size = 0;
  t->maxsize = 0;
  t->metatable = NULL;
  t->type = NULL;
  setarrayvector(L, t, agnSeq_bestsize(n));
  return t;
}


static void purge (lua_State *L, Seq *t, int index) {  /* 0.28.1 */
  size_t i;  /* Agena 1.1.0 */
  TValue *m, *n;
  if (t->readonly) luaG_runerror(L, "sequence is read-only.");
  n = NULL;  /* to avoid compiler warnings */
  /* move items right of item to be deleted to the left */
  if (index < t->size) {  /* item to be deleted is not at the end -> move elements */
    for (i=index; i < t->size; i++) {
      m = seqitem(t, i - 1);
      n = seqitem(t, i);
      setobj(L, m, n);  /* 7.6.7 fix */
    }
  } else
    n = seqitem(t, index - 1);
  setnilvalue(n);
  t->size--;
  if (seqtobeshrunk(t))
    agnSeq_resize(L, t, agnSeq_bestsize(t->size));
}


/* Set a new value to a sequence at `index` i; returns:
      -2:    attempt to add `null` to sequence (if override = 0)
      -1:    memory allocation error
       0:    index out of range
       1:    success */

LUAI_FUNC int agnSeq_seti (lua_State *L, Seq *t, int index, const TValue *val) {
  size_t sizeplusone = t->size + 1;
  t->flags = 0;
  if (t->readonly) luaG_runerror(L, "sequence is read-only.");
  /* sequence must be resized for it has become too small ? */
  if (index == sizeplusone && t->maxsize < sizeplusone) {
    if (t->maxsize << 1 < MAX_INT) {  /* Agena 1.06 patch; avoid overflows; this check and the << op is 100 % faster than
      the computesizes routine for tables, which as agnSeq_seti also quits with a memory error with
      the same number of entries (100'000'000) */
      agnSeq_resize(L, t, agn_newsize(NULL, sizeplusone));  /* 4.2.2 change: increase space mildly by 13 % if required, don't just increase to the next power of 2. */
    } else {
      luaG_runerror(L, "memory allocation failed.");  /* 0.28.1 */
      return -1;
    }
  } else if ((index > sizeplusone) || index < 1) {
    return 0;  /* index out of range */
  }
  if (index == sizeplusone) {  /* seq is empty or a new value shall be appended ? -> add new value to seq */
    TValue *n;
    if (ttisnil(val)) return -2;  /* null cannot be added, ignore */
    n = seqitem(t, index - 1);  /* get next reserved memory chunk */
    setobj(L, n, val);  /* 7.6.7 fix */
    t->size++;
  } else {  /* replace value at existing index */
    if (!ttisnil(val)) {  /* replace old value with new one */
      TValue *n = seqitem(t, index - 1);
      if (luaO_rawequalObj(n, val)) return 1;
      setobj(L, n, val);  /* 7.6.7 fix */
    } else {  /* delete value from existing position */
      purge(L, t, index);
    }
  }
  return 1;
}


/* Sets an element to any position in a sequence (see SETLIST op), without most checks; reintroduced 2.29.6
   Use with SETLIST op only as it increases the sequence size, too ! */

LUAI_FUNC int agnSeq_addi (lua_State *L, Seq *t, int index, const TValue *val) {
  TValue *nnew;
  if (t->readonly) luaG_runerror(L, "sequence is read-only.");
  t->flags = 0;
  if (index < 1 || index > t->maxsize || ttisnil(val)) return 1;  /* failure */
  nnew = seqitem(t, index - 1);  /* get next reserved memory chunk */
  setobj(L, nnew, val);  /* 7.6.7 */
  t->size++;
  luaC_barrierseq(L, t, val);
  return 0;
}


/* In sequence t, sets any value to 1-based position index, including `null`; 7.6.4 */
LUAI_FUNC int agnSeq_rawseti (lua_State *L, Seq *t, int index, const TValue *val) {
  TValue *nnew;
  if (t->readonly) luaG_runerror(L, "sequence is read-only.");
  t->flags = 0;
  if (index < 1 || index > t->maxsize) return 1;  /* failure */
  nnew = seqitem(t, index - 1);  /* get next reserved memory chunk */
  setobj(L, nnew, val);  /* 7.6.7 */
  luaC_barrierseq(L, t, val);
  return 0;
}


/* get the i-th value that has been added to a sequence */

LUAI_FUNC const TValue *agnSeq_geti (Seq *t, size_t index) {
  return ((index > t->size) || index < 1) ? NULL : seqitem(t, index - 1);
}


LUAI_FUNC const TValue *agnSeq_rawgeti (Seq *t, const TValue *ind) {
  int index;
  if (ttisnumber(ind))
    index = (int)nvalue(ind);
  else
    return NULL;
  if ((index > t->size) || index < 1)
    return NULL;  /* index out of range */
  return seqitem(t, index - 1);
}


LUAI_FUNC void agnSeq_resize (lua_State *L, Seq *t, int newsize) {
  int i;
  if (t->readonly) luaG_runerror(L, "sequence is read-only.");
  /* nullify surplus values before shrinking, 2.29.0 */
  for (i=t->size; i > newsize; i--) {  /* from the right to the left ! */
    purge(L, t, i);  /* purge decrements t->size by 1 */
  }
  newsize += (newsize == 0);  /* newsize must be at least 1 to avoid crashes, 2.29.0 */
  if (newsize == t->maxsize) return;  /* do nothing */
  setarrayvector(L, t, newsize);
}


LUAI_FUNC int agnSeq_delete (lua_State *L, Seq *t, const TValue *val) {
  TValue *n;
  int i, c = 0;
  if (t->readonly) luaG_runerror(L, "sequence is read-only.");
  if (t->size == 0) return c;
  for (i=t->size; i > 0; i--) {
    n = seqitem(t, i - 1);
    if (luaO_rawequalObj(n, val)) {
      purge(L, t, i);  /* 0.28.1 */
      luaC_barrierseq(L, t, val);  /* DELETEVAL */
      c++;
    }
  }
  return c;
}


/* free a sequence */

LUAI_FUNC void agnSeq_free (lua_State *L, Seq *t) {
  luaM_freearray(L, t->array, t->maxsize, TValue);
  luaM_free(L, t);
}


LUAI_FUNC int agnSeq_next (lua_State *L, Seq *t, StkId key) {  /* 0.24.1 */
  int i;
  if (ttisnil(key))  /* first iteration */
    i = 0;
  else if (ttisnumber(key))
    i = nvalue(key);  /* find original element */
  else {
    i = -1;  /* to avoid compiler warnings */
    luaG_runerror(L, "invalid key to " LUA_QL("nextone"));
  }
  if (i < t->size) {
    setnvalue(key, cast_num(i + 1));
    setobj2s(L, key + 1, seqitem(t, i));
    return 1;
  }
  return 0;  /* no more elements */
}

/* a version of agnSeq_next to iterate functions is not faster than doing it the primitive way */


LUAI_FUNC void agnSeq_rotatetop (lua_State *L, Seq *t) {  /* 2.2.0 */
  size_t i;
  TValue *m, *n;
  if (t->readonly) luaG_runerror(L, "sequence is read-only.");
  /* duplicate last element and set it to the top of the sequence */
  setobj2s(L, L->top, agnSeq_geti(t, t->size));
  for (i=t->size - 1; i > 0; i--) {
    m = seqitem(t, i);
    n = seqitem(t, i - 1);
    setobj(L, m, n);  /* 7.6.7 fix */
  }
  /* set last element to the bottom */
  agnSeq_seti(L, t, 1, L->top);
  luaC_barrierseq(L, t, L->top);
  setnilvalue(L->top);
}


LUAI_FUNC void agnSeq_rotatebottom (lua_State *L, Seq *t) {  /* 2.2.0 */
  size_t i;
  TValue *m, *n;
  if (t->readonly) luaG_runerror(L, "sequence is read-only.");
  /* duplicate first element and set it to the top of the sequence */
  setobj2s(L, L->top, agnSeq_geti(t, 1));
  /* move items to the bottom */
  for (i=1; i < t->size; i++) {
    m = seqitem(t, i - 1);
    n = seqitem(t, i);
    setobj(L, m, n);  /* 7.6.7 fix */
  }
  /* set first element to the top */
  agnSeq_seti(L, t, t->size, L->top);
  luaC_barrierseq(L, t, L->top);
  setnilvalue(L->top);
}


LUAI_FUNC void agnSeq_exchange (lua_State *L, Seq *t) {  /* 2.2.0, expects two or more elements on the stack */
  size_t s;
  TValue *m, *n;
  if (t->readonly) luaG_runerror(L, "sequence is read-only.");
  /* duplicate element just below the top */
  s = t->size;
  setobj2s(L, L->top, agnSeq_geti(t, s - 1));
  m = seqitem(t, s - 2);
  n = seqitem(t, s - 1);
  setobj(L, m, n);  /* 7.6.7 fix */
  agnSeq_seti(L, t, s, L->top);
  luaC_barrierseq(L, t, L->top);
  setnilvalue(L->top);
}


LUAI_FUNC void agnSeq_duplicate (lua_State *L, Seq *t) {  /* 2.2.0, expects at least one element on the stack */
  size_t s;
  if (t->readonly) luaG_runerror(L, "sequence is read-only.");
  s = t->size;
  setobj2s(L, L->top, agnSeq_geti(t, s));
  agnSeq_seti(L, t, s + 1, L->top);
  luaC_barrierseq(L, t, L->top);
  setnilvalue(L->top);
}


LUAI_FUNC int agnSeq_subs (lua_State *L, Seq *t, const TValue *oldval, const TValue *newval) {  /* 5.1.3 */
  TValue *n;
  int cnt = 0;
  size_t i = 0;
  while (i < t->size) {
    n = seqitem(t, i++);
    if (luaO_rawequalObj(n, oldval)) {
      agnSeq_seti(L, t, i, newval);
      /* if we delete a value (newval is nil), agnReg_seti closes the space immediately, so i must not be changed */
      if (ttisnil(newval)) i--;
      cnt++;
    }
  }
  return cnt;
}


LUAI_FUNC int agnSeq_readonly (lua_State *L, Seq *t, int readonly) {  /* 4.8.2 */
  if (readonly < 0) return t->readonly;
  t->readonly = readonly;
  return readonly;
}


LUAI_FUNC void agnSeq_reorder (lua_State *L, Seq *t) {  /* 7.6.4 */
  int i, cnt;
  TValue *val;
  if (t->readonly) luaG_runerror(L, "sequence is read-only.");
  cnt = 0;
  /* reshuffle */
  for (i=0; i < t->size; i++) {
    val = seqitem(t, i);
    if (!ttisnil(val)) {
      TValue *dest = &t->array[cnt++];  /* target the next clean slot */
      if (dest != val) {
        setobj(L, dest, val);    /* raw copy: no metatable check, no overhead */
        setnilvalue(val);        /* clear the old hole */
      }
    }
  }
  agnSeq_resize(L, t, cnt);
}

