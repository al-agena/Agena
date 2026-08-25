/*
** $Id: lseq.h v 0.2, based on ltable.h v2.10 2006/01/10 13:13:06 roberto Exp $
** Agena Sequences
** See Copyright Notice in agena.h
*/

#ifndef lseq_h
#define lseq_h

#include "lobject.h"

#define seqitem(t,i)        (&(t)->array[i])

LUAI_FUNC INLINE const TValue *agnSeq_geti (Seq *t, size_t index);
LUAI_FUNC INLINE const TValue *agnSeq_rawgeti (Seq *t, const TValue *ind);
LUAI_FUNC INLINE int agnSeq_seti (lua_State *L, Seq *t, int index, const TValue *val);
LUAI_FUNC INLINE int agnSeq_addi (lua_State *L, Seq *t, int index, const TValue *val);
LUAI_FUNC int agnSeq_rawseti (lua_State *L, Seq *t, int index, const TValue *val);
LUAI_FUNC Seq *agnSeq_new (lua_State *L, int n);
LUAI_FUNC void agnSeq_free (lua_State *L, Seq *t);
LUAI_FUNC int  agnSeq_delete (lua_State *L, Seq *t, const TValue *val);
LUAI_FUNC int  agnSeq_subs (lua_State *L, Seq *t, const TValue *oldval, const TValue *newval);
LUAI_FUNC void agnSeq_resize (lua_State *L, Seq *t, int newsize);
LUAI_FUNC int  agnSeq_next (lua_State *L, Seq *t, StkId key);
LUAI_FUNC void agnSeq_rotatetop (lua_State *L, Seq *t);
LUAI_FUNC void agnSeq_rotatebottom (lua_State *L, Seq *t);
LUAI_FUNC void agnSeq_exchange (lua_State *L, Seq *t);
LUAI_FUNC void agnSeq_duplicate (lua_State *L, Seq *t);
LUAI_FUNC void agnSeq_reorder (lua_State *L, Seq *t);
LUAI_FUNC INLINE int agnSeq_bestsize (int r);
LUAI_FUNC int agnSeq_readonly (lua_State *L, Seq *t, int readonly);

#endif
