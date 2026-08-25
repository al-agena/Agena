/*
** $Id: lvm.h,v 2.5 2005/08/22 18:54:49 roberto Exp $
** Lua virtual machine
** See Copyright Notice in agena.h
*/

#ifndef lvm_h
#define lvm_h

#include "ldo.h"
#include "lobject.h"
#include "ltm.h"

#define tostring(L,o) ((ttype(o) == LUA_TSTRING) || (luaV_tostring(L, o)))

/* convert an object to a float (including string coercion) */
#define tonumberx(o,n) \
  (ttisnumber(o) ? (*(n) = nvalue(o), 1) : luaV_tonumber_(o, n))

/* 0.26.1 */
#define tocomplex(o,n)  (ttype(o) == LUA_TCOMPLEX || \
                         (((o) = luaV_tocomplex(o,n)) != NULL))

#define equalobj(L,o1,o2) \
  (ttype(o1) == ttype(o2) ? luaV_equalval(L, o1, o2) : \
     (ttype(o1) == LUA_TCOMPLEX || ttype(o2) == LUA_TCOMPLEX ? luaV_equalncomplex(L,o1,o2) : 0 ) )

#define equalobjonebyone(L,o1,o2) \
  (ttype(o1) == ttype(o2) ? luaV_equalvalonebyone(L, o1, o2) : \
     (ttype(o1) == LUA_TCOMPLEX || ttype(o2) == LUA_TCOMPLEX ? luaV_equalncomplex(L,o1,o2) : 0 ) )

#define approxequalobjonebyone(L,o1,o2) \
  (ttype(o1) == ttype(o2) ? luaV_approxequalvalonebyone(L, o1, o2) : \
     (ttype(o1) == LUA_TCOMPLEX || ttype(o2) == LUA_TCOMPLEX ? luaV_equalncomplex(L,o1,o2) : 0 ) )

#define equalref(L,o1,o2) \
  (ttype(o1) == ttype(o2) && luaV_equalref(L, o1, o2))

#define tools_isanything(rc_str)   tools_streq(rc_str, llex_token2str(TK_TANYTHING))
#define tools_islisting(rc_str)    tools_streq(rc_str, llex_token2str(TK_TLISTING))
#define tools_isbasic(rc_str)      tools_streq(rc_str, llex_token2str(TK_TBASIC))
#define tools_isanyorlist(rc_str)  (tools_isanything(rc_str) || tools_islisting(rc_str))
#define luaS_token(L,tkn)          luaS_new(L, llex_token2str(tkn))

/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define luaV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */

#define luaV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->sizearray) \
              ? &hvalue(t)->array[k - 1] : luaH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */

/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/

#define luaV_finishfastset(L,t,slot,v) \
    { setobj2t(L, cast(TValue *,slot), v); \
      luaC_barriert(L, hvalue(t), v); }

LUAI_FUNC int    luaV_equalval (lua_State *L, const TValue *t1, const TValue *t2);
LUAI_FUNC int    luaV_lessthan (lua_State *L, const TValue *l, const TValue *r, int issueerror);
LUAI_FUNC int    luaV_lessequal (lua_State *L, const TValue *l, const TValue *r, int issueerror);
LUAI_FUNC int    luaV_equalref (lua_State *L, const TValue *t1, const TValue *t2);  /* 2.12.3 */
LUAI_FUNC int    luaV_approxequalvalonebyone (lua_State *L, const TValue *t1, const TValue *t2);
LUAI_FUNC int    luaV_equalncomplex (lua_State *L, const TValue *t1, const TValue *t2);
LUAI_FUNC const  TValue *luaV_tonumber (const TValue *obj, TValue *n);
LUAI_FUNC const  TValue *luaV_arithmoperand (const TValue *obj, TValue *n);  /* 0.28.2 */
LUAI_FUNC const  TValue *luaV_tocomplex (const TValue *obj, TValue *n);  /* 0.26.1 */
LUAI_FUNC int    luaV_tostring (lua_State *L, StkId obj);
LUAI_FUNC void   luaV_gettable (lua_State *L, const TValue *t, TValue *key, StkId val);
LUAI_FUNC void   luaV_settable (lua_State *L, const TValue *t, TValue *key, StkId val);
LUAI_FUNC void   luaV_execute (lua_State *L, int nexeccalls);
LUAI_FUNC void   luaV_concat (lua_State *L, int total, int last);
LUAI_FUNC void   luaV_finishget (lua_State *L, const TValue *t, TValue *key,
                               StkId val, const TValue *slot);
LUAI_FUNC void   luaV_finishset (lua_State *L, const TValue *t, TValue *key,
                               TValue *val, const TValue *slot);
LUAI_FUNC int    luaV_tonumber_ (const TValue *obj, lua_Number *n);
LUAI_FUNC int    luaV_tonumberx_ (const TValue *obj, lua_Number *n);

/* added in Agena: */
LUAI_FUNC size_t agenaV_nops (lua_State *L, Table *t);  /* 0.9.1, in lapi.c */
LUAI_FUNC void   luaV_objlen (lua_State *L, StkId ra, const TValue *rb);  /* 2.21.2, in lapi.c */
LUAI_FUNC int    agenaV_comptablesonebyone (lua_State *L, Table *t1, Table *t2);  /* 0.22.0, in lapi.c */
LUAI_FUNC void   agenaV_in (lua_State *L, const TValue *value, const TValue *what, StkId idx);
LUAI_FUNC void   agenaV_copy (lua_State *L, const TValue *tbl, StkId idx, Table *seen, int what);  /* 0.22.1, in lapi.c */
LUAI_FUNC void   agenaV_join (lua_State *L, StkId idx, int nargs);     /* 2.34.5 */
LUAI_FUNC int    agenaV_deletefrom (lua_State *L, const TValue *value, const TValue *what);  /* 5.1.2 */
LUAI_FUNC int    agenaV_instr (lua_State *L, StkId idx, int nargs);    /* 2.34.5 */
LUAI_FUNC int    luaV_equalvalonebyone (lua_State *L, const TValue *t1, const TValue *t2);  /* 2.5.3, in lvm.h itself */
LUAI_FUNC int    call_binTM (lua_State *L, const TValue *p1, const TValue *p2, StkId res, TMS event);  /* 2.5.3, in ldo.c */
LUAI_FUNC int    agenaV_tintindices (lua_State *L, TValue *tbl, int *flag);  /* 2.30.1 */
LUAI_FUNC int    agenaV_tintentries (lua_State *L, TValue *tbl, int *flag);  /* 3.9.3 */
LUAI_FUNC int    agenaV_tentries (lua_State *L, TValue *tbl, int *flag);  /* 2.30.1 */
LUAI_FUNC void   agenaV_tparts (lua_State *L, TValue *tbl, int hash);  /* 2.30.1 */
LUAI_FUNC void   agenaV_toset (lua_State *L, const TValue *obj, StkId idx);  /* 2.30.2 */
LUAI_FUNC void   agenaV_mulup (lua_State *L, const TValue *obj, StkId idx, int nargs);  /* 2.30.2; DEFUNCT, just a working demonstrator */
LUAI_FUNC void   agenaV_setmetatable (lua_State *L, TValue *obj, Table *mt, int cfunc);  /* 2.34.3 */
LUAI_FUNC int    agenaV_setstorage (lua_State *L, TValue *obj, Table *mt);  /* 3.5.3 */
LUAI_FUNC void   agenaV_setutype (lua_State *L, TValue *obj, TString *s);  /* 2.34.3 */

LUAI_FUNC void   agenaV_setops_settodict (lua_State *L, Table *h, TValue *key, TValue *val);
LUAI_FUNC unsigned int agenaV_numsetops (lua_State *L, const TValue *tbl1, const TValue *tbl2, int mode);  /* 3.10.0 */
LUAI_FUNC unsigned int agenaV_numunion (lua_State *L, const TValue *tbl1, const TValue *tbl2);  /* 3.10.0 */

LUAI_FUNC int    agenaV_tisofnumerictype (lua_State *L, TValue *tbl, int (*fn)(double), int integralkeysonly);  /* 3.15.4 */
LUAI_FUNC int    agenaV_usisofnumerictype (lua_State *L, TValue *tbl, int (*fn)(double));  /* 3.19.2 */
LUAI_FUNC int    agenaV_seqisofnumerictype (lua_State *L, TValue *s, int (*fn)(double));  /* 3.15.4 */
LUAI_FUNC int    agenaV_regisofnumerictype (lua_State *L, TValue *s, int (*fn)(double));  /* 3.15.4 */

LUAI_FUNC void   agenaV_sumup (lua_State *L, const TValue *obj, StkId idx, TMS event);  /* 4.4.6 */
LUAI_FUNC void   agenaV_seqsumup (lua_State *L, const TValue *obj, StkId idx, TMS event);  /* 4.4.6 */
LUAI_FUNC void   agenaV_regsumup (lua_State *L, const TValue *obj, StkId idx, TMS event);  /* 4.4.6 */

LUAI_FUNC void   agenaV_addup_sumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d);  /* 4.4.6 */
LUAI_FUNC void   agenaV_addup_seqsumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d);  /* 4.4.6 */
LUAI_FUNC void   agenaV_addup_regsumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d);  /* 4.4.6 */

LUAI_FUNC void   agenaV_qsumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d);  /* 4.4.8 */
LUAI_FUNC void   agenaV_seqqsumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d);  /* 4.4.8 */
LUAI_FUNC void   agenaV_regqsumup_div (lua_State *L, const TValue *obj, StkId idx, lua_Number d);  /* 4.4.8 */

LUAI_FUNC void   agenaV_count (lua_State *L, const TValue *idxb, const TValue *idxc, StkId idxa);  /* 4.6.1 */
LUAI_FUNC int    agenaV_numqmdev (lua_State *L, const TValue *obj, StkId idx, lua_Number *Mean, lua_Number *Qmdev);  /* 4.6.2 */
#endif

