/*
** $Id: lpair.h v 0.2, based on ltable.h v2.10 2006/01/10 13:13:06 roberto Exp $
** Agena Pairs
** See Copyright Notice in agena.h
*/

#ifndef lpair_h
#define lpair_h

#include "lobject.h"

#define pairitem(t,i)    (&(t)->array[i])

LUAI_FUNC const TValue *agnPair_geti (Pair *t, size_t index);
LUAI_FUNC INLINE int agnPair_seti (lua_State *L, Pair *t, int index, const TValue *val);
LUAI_FUNC INLINE int agnPair_rawseti (lua_State *L, Pair *t, const TValue *index, const TValue *val);
LUAI_FUNC INLINE const TValue *agnPair_rawgeti (Pair *t, const TValue *ind);
LUAI_FUNC Pair *agnPair_new (lua_State *L);
LUAI_FUNC Pair *agnPair_create (lua_State *L, const TValue *left, const TValue *right);
LUAI_FUNC void agnPair_free (lua_State *L, Pair *t);
LUAI_FUNC int  agnPair_readonly (lua_State *L, Pair *t, int readonly);

#endif
