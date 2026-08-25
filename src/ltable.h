/*
** $Id: ltable.h,v 2.10 2006/01/10 13:13:06 roberto Exp $
** Lua/Agena tables (hash)
** See Copyright Notice in agena.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define gnode(t,i)   (&(t)->node[i])
#define gkey(n)      (&(n)->i_key.nk)
#define key2tval(n)  (&(n)->i_key.tvk)
#define gval(n)      (&(n)->i_val)
#define gnext(n)     ((n)->i_key.nk.next)

LUAI_FUNC INLINE const TValue *luaH_getnum (Table *t, int key);
#define luaH_getint luaH_getnum  /* 4.6.3/4.9.3 */
LUAI_FUNC INLINE TValue *luaH_setnum (lua_State *L, Table *t, int key);
LUAI_FUNC INLINE void luaH_setint (lua_State *L, Table *t, int key, TValue *value);  /* 4.6.3 */
LUAI_FUNC INLINE const TValue *luaH_getstr (Table *t, TString *key);
LUAI_FUNC INLINE TValue *luaH_setstr (lua_State *L, Table *t, TString *key);
LUAI_FUNC INLINE const TValue *luaH_get (Table *t, const TValue *key);
LUAI_FUNC INLINE TValue *luaH_set (lua_State *L, Table *t, const TValue *key);
LUAI_FUNC Table *luaH_new (lua_State *L, int narray, int lnhash);
LUAI_FUNC TValue *luaH_newkey (lua_State *L, Table *t, const TValue *key);
LUAI_FUNC void luaH_resizearray (lua_State *L, Table *t, int nasize);
LUAI_FUNC void luaH_free (lua_State *L, Table *t);
LUAI_FUNC int  luaH_next (lua_State *L, Table *t, StkId key);
LUAI_FUNC int  luaHF_next (lua_State *L, Table *t, TValue *f, StkId key, int mode);  /* 0.9.1 */
LUAI_FUNC INLINE int luaH_getn (Table *t);
LUAI_FUNC int agnH_hasarraypart (lua_State *L, Table *t);
LUAI_FUNC int agnH_hashashpart (lua_State *L, Table *t);
LUAI_FUNC Node *luaH_hashnum (const Table *t, lua_Number n);
LUAI_FUNC const TValue *luaH_gethashnum (Table *t, int key);

LUAI_FUNC INLINE void agnH_nops (lua_State *L, Table *t, size_t a[]);
LUAI_FUNC int  agnH_hasholes (lua_State *L, Table *t, int *lastnonnil);
LUAI_FUNC int  agnH_asize (lua_State *L, Table *t);  /* 2.11.4 */
LUAI_FUNC int  agnH_hsize (lua_State *L, Table *t);  /* 4.10.2 */
LUAI_FUNC int  agnH_borders (lua_State *L, Table *t, int *low);  /* 2.30.1 */
LUAI_FUNC int  agnH_aborders (lua_State *L, Table *t, int *low);  /* 2.14.10 */
LUAI_FUNC void agnH_rotatebottom (lua_State *L, Table *t);
LUAI_FUNC void agnH_rotatetop (lua_State *L, Table *t);
LUAI_FUNC int  luaH_isdummy (Node *n);
LUAI_FUNC void luaH_resizearray (lua_State *L, Table *t, int nasize);  /* 4.6.3 */
LUAI_FUNC void agnH_reorder (lua_State *L, Table *t, int arraypartonly);  /* 2.30.1 */
LUAI_FUNC void agnH_clear (lua_State *L, Table* t);  /* 4.8.2 */
LUAI_FUNC int  agnH_readonly (lua_State *L, Table *t, int readonly);  /* 4.8.2 */
LUAI_FUNC int  agnH_delete (lua_State *L, Table *t, const TValue *value);  /* 5.1.2 */
LUAI_FUNC int  agnH_purge (lua_State *L, Table *t, int i, int len);  /* 7.6.7 */
LUAI_FUNC int  agnH_subs (lua_State *L, Table *t, const TValue *oldvalue, const TValue *newvalue);  /* 5.1.3 */

LUAI_FUNC INLINE int luaH_findindex (lua_State *L, Table *t, StkId key);
LUAI_FUNC Node *luaH_mainposition (const Table *t, const TValue *key);

#define dummynode      (&dummynode_)

static const Node dummynode_ = {
  {{NULL}, LUA_TNIL},  /* value */
  {{{NULL}, LUA_TNIL, NULL}}  /* key */
};


/* The following modified hash functions result in an overall increase in speed of 1 %
   compared to the original ones in Lua 5.1 */

#define luaH_hashpow2(t,n)      (gnode(t, lmod((n), sizenode(t))))
#define luaH_hashstr(t,str)     luaH_hashpow2(t, (str)->tsv.hash)
#define luaH_hashboolean(t,p)   luaH_hashpow2(t, p)
/*
** for some types, it is better to avoid modulus by power of 2, as
** they tend to have many 2 factors.
#define luaH_hashmod(t,n)       (gnode(t, ((n) % ((sizenode(t) - 1)|1))))

#define luaH_hashpointer(t,p)   luaH_hashmod(t, IntPoint(p))
*/

#define luaH_hashpow2(t,n)      (gnode(t, lmod((n), sizenode(t))))
#define luaH_hashstr(t,str)     luaH_hashpow2(t, (str)->tsv.hash)
#define luaH_hashboolean(t,p)   luaH_hashpow2(t, p)

#define luaH_hashmod(t,n)       luaH_hashpow2(t, n)

/* Murmur-based pointer hashing declaration */
LUAI_FUNC Node *luaH_hashpointer (const Table *t, const void *p);

#endif
