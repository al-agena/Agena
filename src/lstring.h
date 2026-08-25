/*
** $Id: lstring.h,v 1.43 2005/04/25 19:24:10 roberto Exp $
** String table (keep all strings handled by Agena)
** See Copyright Notice in agena.h
*/

#ifndef lstring_h
#define lstring_h


#include "lgc.h"
#include "lobject.h"
#include "lstate.h"
#include "agnhlps.h"

#define sizestring(s)	(sizeof(union TString)+((s)->len + 1)*sizeof(char))

#define sizeudata(u)	(sizeof(union Udata)+(u)->len)

#define luaS_new(L, s)	(luaS_newlstr(L, s, tools_strlen(s)))
#define luaS_newliteral(L, s)	(luaS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char)) - 1))

#define luaS_fix(s)	l_setbit((s)->tsv.marked, FIXEDBIT)

LUAI_FUNC void luaS_resize (lua_State *L, int newsize);
LUAI_FUNC Udata *luaS_newudata (lua_State *L, size_t s, Table *e);
LUAI_FUNC TString *luaS_newlstr (lua_State *L, const char *str, size_t l);
LUAI_FUNC TString *luaS_newchar (lua_State *L, const char *str);  /* 0.24.1 */

#endif
