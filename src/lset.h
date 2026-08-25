/*
** $Id: lset.h v 0.1, based on ltable.h v2.10 2006/01/10 13:13:06 roberto Exp $
** Agena UltraSets (hash)
** See Copyright Notice in agena.h
*/

#ifndef lset_h
#define lset_h

#include "lobject.h"


#define glnode(t,i)    (&(t)->node[i])
#define glkey(n)       (&(n)->i_key.nk)
#define glnext(n)      ((n)->i_key.nk.next)

#define key2tval(n)    (&(n)->i_key.tvk)

LUAI_FUNC int      findsetindex (lua_State *L, UltraSet *t, StkId key);
LUAI_FUNC UltraSet *agnUS_new (lua_State *L, int lnhash);
LUAI_FUNC int      agnUS_get (UltraSet *t, const TValue *key);
LUAI_FUNC TValue   *agnUS_set (lua_State *L, UltraSet *t, const TValue *key);
LUAI_FUNC int      agnUS_delete (lua_State *L, UltraSet *t, const TValue *key);
LUAI_FUNC int      agnUS_getnum (UltraSet *t, lua_Number key);
LUAI_FUNC TValue   *agnUS_setnum (lua_State *L, UltraSet *t, lua_Number key);
LUAI_FUNC int      agnUS_getstr (UltraSet *t, TString *key);
LUAI_FUNC TValue   *agnUS_setstr (lua_State *L, UltraSet *t, TString *key);
LUAI_FUNC void     agnUS_setstr2set (lua_State *L, UltraSet *us, TString *key);
LUAI_FUNC void     agnUS_delstr (lua_State *L, UltraSet *t, TString *key);
LUAI_FUNC void     agnUS_free (lua_State *L, UltraSet *t);
LUAI_FUNC int      agnUS_next (lua_State *L, UltraSet *t, StkId key);
LUAI_FUNC void     agnUS_resize (lua_State *L, UltraSet *t, int nsize);
LUAI_FUNC int      agnUS_readonly (lua_State *L, UltraSet *t, int readonly);

#if defined(LUA_DEBUG)
LUAI_FUNC LNode *agnL_mainposition (const UltraSet *t, const TValue *key);
LUAI_FUNC int agnL_isdummy (LNode *n);
#endif


#define dummylnode      (&dummylnode_)

static const LNode dummylnode_ = {
  {{{NULL}, LUA_TNIL, NULL}}  /* key */
/*  Value,  tt,       *next  */
};


#endif

