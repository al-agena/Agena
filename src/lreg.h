/*
** $Id: lseq.h v 0.2, based on ltable.h v2.10 2006/01/10 13:13:06 roberto Exp $
** Agena Registers
** See Copyright Notice in agena.h
*/

#ifndef lreg_h
#define lreg_h

#include "lobject.h"

#define regitem(t,i)    (&(t)->array[i])

LUAI_FUNC int agnReg_get (Reg *t, const TValue *val);
LUAI_FUNC const TValue *agnReg_geti (Reg *t, size_t index);
LUAI_FUNC const TValue *agnReg_rawgeti (Reg *t, const TValue *ind);
LUAI_FUNC int  agnReg_seti (lua_State *L, Reg *t, int index, const TValue *val);
LUAI_FUNC Reg *agnReg_new (lua_State *L, int n);
LUAI_FUNC void agnReg_free (lua_State *L, Reg *t);
LUAI_FUNC int  agnReg_delete (lua_State *L, Reg *t, const TValue *val);
LUAI_FUNC int  agnReg_resize (lua_State *L, Reg *t, int newsize, int nil);
LUAI_FUNC int  agnReg_next (lua_State *L, Reg *t, StkId key);
LUAI_FUNC void agnReg_rotatetop (lua_State *L, Reg *t);
LUAI_FUNC void agnReg_rotatebottom (lua_State *L, Reg *t);
LUAI_FUNC void agnReg_exchange (lua_State *L, Reg *t);
LUAI_FUNC int  agnReg_duplicate (lua_State *L, Reg *t);
LUAI_FUNC int  agnReg_subs (lua_State *L, Reg *t, const TValue *oldval, const TValue *newval);
LUAI_FUNC void agnReg_purge (lua_State *L, Reg *t, int index);
LUAI_FUNC void agnReg_reorder (lua_State *L, Reg *t);
LUAI_FUNC int  agnReg_settop (lua_State *L, Reg *t, const TValue *ind);
LUAI_FUNC int  agnReg_readonly (lua_State *L, Reg *t, int readonly);

#endif
