/*
** $Id: ldo.h,v 2.7 2005/08/24 16:15:49 roberto Exp $
** Stack and Call structure of Lua/Agena
** See Copyright Notice in agena.h
*/

#ifndef ldo_h
#define ldo_h


#include "lobject.h"
#include "lstate.h"
#include "lzio.h"

#include <setjmp.h>   /* Agena 2.1 RC 2 */

#define JMPTYPE_LONGJMP         0
#define JMPTYPE_TRY             1

/* chain list of long jump buffers */
struct lua_longjmp {    /* Agena 2.1 RC 2 */
  struct lua_longjmp *previous;
  luai_jmpbuf b;
  volatile int status;  /* error code */
  int type;             /* JMPTYPE_* */
  Instruction *pc;
  ptrdiff_t old_ci;
  lu_byte old_allowhooks;
  ptrdiff_t old_errfunc;
  int old_top;
  int old_nexeccalls;
  unsigned short oldnCcalls;
};


#define luaD_checkstack(L,n)  \
  if ((char *)L->stack_last - (char *)L->top <= (n)*(int)sizeof(TValue)) \
    luaD_growstack(L, n); \
  else condhardstacktests(luaD_reallocstack(L, L->stacksize - EXTRA_STACK - 1));

#define incr_top(L)        {luaD_checkstack(L,1); L->top++;}

#define savestack(L,p)     ((char *)(p) - (char *)L->stack)
#define restorestack(L,n)  ((TValue *)((char *)L->stack + (n)))

#define saveci(L,p)        ((char *)(p) - (char *)L->base_ci)
#define restoreci(L,n)     ((CallInfo *)((char *)L->base_ci + (n)))


/* results from luaD_precall */
#define PCRLUA      0     /* initiated a call to a Lua function */
#define PCRC        1     /* did a call to a C function */
#define PCRYIELD    2     /* C function yielded */
#define PCRSTRING   3     /* return of SUBSTRING 0.9.0 */
#define PCRREMEMBER 4     /* result comes from remember table, 0.9.2 */

/* grep "(GREP_POINT) types;" if you want to add new types */
#define IAM_INTEGER      ( (TK_INTEGER     - TK_TBOOLEAN + 1) )  /* 2.12.1 */
#define IAM_NONNEGINT    ( (TK_NONNEGINT   - TK_TBOOLEAN + 1) )
#define IAM_POSINT       ( (TK_POSINT      - TK_TBOOLEAN + 1) )
#define IAM_NONZEROINT   ( (TK_NONZEROINT  - TK_TBOOLEAN + 1) )  /* 4.11.0 */
#define IAM_POSITIVE     ( (TK_POSITIVE    - TK_TBOOLEAN + 1) )
#define IAM_NEGATIVE     ( (TK_NEGATIVE    - TK_TBOOLEAN + 1) )  /* 3.3.6 */
#define IAM_NONNEGATIVE  ( (TK_NONNEGATIVE - TK_TBOOLEAN + 1) )

/* type of protected functions, to be ran by `runprotected' */
typedef void (*Pfunc) (lua_State *L, void *ud);

LUAI_FUNC int luaD_protectedparser (lua_State *L, ZIO *z, const char *name);
LUAI_FUNC void luaD_callhook (lua_State *L, int event, int line);
LUAI_FUNC int luaD_precall (lua_State *L, StkId func, int nresults);
LUAI_FUNC void luaD_call (lua_State *L, StkId func, int nResults, int gc);
LUAI_FUNC int luaD_pcall (lua_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
LUAI_FUNC int luaD_poscall (lua_State *L, StkId firstResult);
LUAI_FUNC void luaD_reallocCI (lua_State *L, int newsize);
LUAI_FUNC void luaD_reallocstack (lua_State *L, int newsize);
LUAI_FUNC void luaD_growstack (lua_State *L, int n);

LUAI_FUNC void luaD_throw (lua_State *L, int errcode);
LUAI_FUNC int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud);

LUAI_FUNC void luaD_seterrorobj (lua_State *L, int errcode, StkId oldtop);

LUAI_FUNC void luaD_rtableentry (lua_State *L, LClosure *cl, StkId base, StkId firstResult, int nresults);  /* 0.9.2 */

LUAI_FUNC int hash (const TValue *key);
#define luaD_rthashupdate(h,val) ((h) = (((h) << 5) - (h)) + (unsigned int)hash(val))

#endif

