/*
** $Id: ldo.c,v 2.38 2006/06/05 19:36:14 roberto Exp $
** Stack and Call structure of Lua/Agena
** See Copyright Notice in agena.h
*/

#include <stdlib.h>
#include <string.h>

#define ldo_c
#define LUA_CORE

#include "agena.h"

#include "agnhlps.h"
#include "agnxlib.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"

/*
** {======================================================
** Error-recovery functions
** =======================================================
*/


/* chain list of long jump buffers */
void luaD_seterrorobj (lua_State *L, int errcode, StkId oldtop) {  /* 2.1.0, try/catch patch */
  switch (errcode) {
    case LUA_ERRMEM: {
      setsvalue2s(L, oldtop, luaS_newliteral(L, MEMERRMSG));
      break;
    }
    case LUA_ERRERR: {
      setsvalue2s(L, oldtop, luaS_newliteral(L, "error in error handling"));
      break;
    }
    case LUA_ERRSYNTAX:
    case LUA_ERRRUN: {
      setobjs2s(L, oldtop, L->top - 1);  /* error message on current top */
      break;
    }
  }
  L->top = oldtop + 1;
}


static void restore_stack_limit (lua_State *L) {
  lua_assert(L->stack_last - L->stack == L->stacksize - EXTRA_STACK - 1);
  if (L->size_ci > LUAI_MAXCALLS) {  /* there was an overflow? */
    int inuse = cast_int(L->ci - L->base_ci);
    if (inuse + 1 < LUAI_MAXCALLS)  /* can `undo' overflow? */
      luaD_reallocCI(L, LUAI_MAXCALLS);
  }
}


static void resetstack (lua_State *L, int status) {
  L->ci = L->base_ci;
  L->base = L->ci->base;
  luaF_close(L, L->base);  /* close eventual pending closures */
  luaD_seterrorobj(L, status, L->base);
  L->nCcalls = L->baseCcalls;  /* 0.13.3 */
  L->allowhook = 1;
  restore_stack_limit(L);
  L->errfunc = 0;
  L->errorJmp = NULL;
}


void luaD_throw (lua_State *L, int errcode) {
  if (L->errorJmp) {
    L->errorJmp->status = errcode;
    LUAI_THROW(L, L->errorJmp);
  }
  else {
    L->status = cast_byte(errcode);
    if (G(L)->panic) {
      resetstack(L, errcode);
      lua_unlock(L);
      G(L)->panic(L);
    }
    agnL_onexit(L, 0);  /* 2.7.0 */
    exit(EXIT_FAILURE);
  }
}


int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud) {
  struct lua_longjmp lj;
  lj.type = JMPTYPE_LONGJMP;  /* changed Agena 2.1 RC 2, written by Hu Qiwei */
  lj.status = 0;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  LUAI_TRY(L, &lj,
    (*f)(L, ud);
  );
  L->errorJmp = lj.previous;  /* restore old error handler */
  return lj.status;
}

/* ************************************************************************* */


static void correctstack (lua_State *L, TValue *oldstack) {
  CallInfo *ci;
  GCObject *up;
  L->top = (L->top - oldstack) + L->stack;
  for (up = L->openupval; up != NULL; up = up->gch.next)
    gco2uv(up)->v = (gco2uv(up)->v - oldstack) + L->stack;
  for (ci = L->base_ci; ci <= L->ci; ci++) {
    ci->top = (ci->top - oldstack) + L->stack;
    ci->base = (ci->base - oldstack) + L->stack;
    ci->func = (ci->func - oldstack) + L->stack;
  }
  L->base = (L->base - oldstack) + L->stack;
}


void luaD_reallocstack (lua_State *L, int newsize) {
  TValue *oldstack = L->stack;
  int realsize = newsize + 1 + EXTRA_STACK;
  lua_assert(L->stack_last - L->stack == L->stacksize - EXTRA_STACK - 1);
  luaM_reallocvector(L, L->stack, L->stacksize, realsize, TValue);
  L->stacksize = realsize;
  L->stack_last = L->stack+newsize;
  correctstack(L, oldstack);
}


void luaD_reallocCI (lua_State *L, int newsize) {
  CallInfo *oldci = L->base_ci;
  luaM_reallocvector(L, L->base_ci, L->size_ci, newsize, CallInfo);
  L->size_ci = newsize;
  L->ci = (L->ci - oldci) + L->base_ci;
  L->end_ci = L->base_ci + L->size_ci - 1;
}


void luaD_growstack (lua_State *L, int n) {
  if (n <= L->stacksize)  /* double size is enough? */
    luaD_reallocstack(L, 2*L->stacksize);
  else
    luaD_reallocstack(L, L->stacksize + n);
}


static CallInfo *growCI (lua_State *L) {
  if (L->size_ci > LUAI_MAXCALLS)  /* overflow while handling overflow? */
    luaD_throw(L, LUA_ERRERR);
  else {
    luaD_reallocCI(L, 2*L->size_ci);
    if (L->size_ci > LUAI_MAXCALLS)
      luaG_runerror(L, "stack overflow");
  }
  return ++L->ci;
}


void luaD_callhook (lua_State *L, int event, int line) {
  lua_Hook hook = L->hook;
  if (hook && L->allowhook) {
    ptrdiff_t top = savestack(L, L->top);
    ptrdiff_t ci_top = savestack(L, L->ci->top);
    lua_Debug ar;
    ar.event = event;
    ar.currentline = line;
    if (event == LUA_HOOKTAILRET)
      ar.i_ci = 0;  /* tail call; no debug information about it */
    else
      ar.i_ci = cast_int(L->ci - L->base_ci);
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    L->ci->top = L->top + LUA_MINSTACK;
    lua_assert(L->ci->top <= L->stack_last);
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    lua_unlock(L);
    (*hook)(L, &ar);
    lua_lock(L);
    lua_assert(!L->allowhook);
    L->allowhook = 1;
    L->ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
  }
}


static StkId adjust_varargs (lua_State *L, Proto *p, int actual) {
  int i;
  int nfixargs = p->numparams;
  Table *htab = NULL;
  StkId base, fixed;
  for (; actual < nfixargs; ++actual)
    setnilvalue(L->top++);
  if (p->is_vararg & VARARG_NEEDSARG) { /* compat. with old-style vararg? */
    int nvar = actual - nfixargs;  /* number of extra arguments */
    lua_assert(p->is_vararg & VARARG_HASARG);
    luaC_checkGC(L);
    luaD_checkstack(L, p->maxstacksize);  /* 7.9.2 fix taken from Lunacy: a highly nested script
      with excessive function argument manipulation can no longer cause a stack overflow */
    htab = luaH_new(L, nvar, 1);  /* create `arg' table */
    for (i=0; i < nvar; i++)  /* put extra arguments into `arg' table */
      setobj2n(L, luaH_setnum(L, htab, i + 1), L->top - nvar + i);
    /* store counter in field `n' */
  }
  /* move fixed parameters to final position */
  fixed = L->top - actual;  /* first fixed argument */
  base = L->top;  /* final position of first argument */
  for (i=0; i < nfixargs; i++) {
    setobjs2s(L, L->top++, fixed + i);
    setnilvalue(fixed + i);
  }
  /* add `arg' parameter */
  if (htab) {
    sethvalue(L, L->top++, htab);
    lua_assert(iswhite(obj2gco(htab)));
  }
  return base;
}


static StkId tryfuncTM (lua_State *L, StkId func) {
  const TValue *tm = luaT_gettmbyobj(L, func, TM_CALL);
  StkId p;
  ptrdiff_t funcr = savestack(L, func);
  if (!ttisfunction(tm))
    luaG_typeerror(L, func, "call");
  /* Open a hole inside the stack at `func' */
  for (p = L->top; p > func; p--) setobjs2s(L, p, p - 1);
  incr_top(L);
  func = restorestack(L, funcr);  /* previous call may change stack */
  setobj2s(L, func, tm);  /* tag method is the new function to be called */
  return func;
}


/* ======================================================
** Remember tables.
*  ====================================================== */

/* Hashing routines for remember tables */
/* GREP "Grep luaH_new(L, AGNI_RTSLOTS"
   if you change AGNI_RTSLOTS (lvm.c, case CMD_REMEMBER), lapi.c (agn_setrtable) */

#define hashpow2(n)      lmod((n), AGNI_RTSLOTS)

#define hashstr(str)     hashpow2((str)->tsv.hash)
#define hashboolean(p)   hashpow2(p)

#define hashmod(n)       ((n) % ((AGNI_RTSLOTS - 1)|1))

/* number of ints inside a lua_Number */
#define numints          cast_int(sizeof(lua_Number)/sizeof(int))

/* hash for lua_Numbers */
static int hashnum (lua_Number n) {
  unsigned int a[numints];
  int i;
  if (luai_numeq(n, 0))
    return 0;
  tools_memcpy(a, &n, sizeof(a));
  for (i=1; i < numints; i++) a[0] += a[i];
  return hashmod(a[0]);
}

LUAI_FUNC int hash (const TValue *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return hashnum(nvalue(key));
    case LUA_TSTRING:
      return hashstr(rawtsvalue(key));
    case LUA_TBOOLEAN:
      return hashboolean(bvalue(key));
    default:
      return 0;
  }
}

/* 0.22.1: function to search a remember table, returns the index of the first result. */
static StkId searchrtable (lua_State *L, LClosure *cl, StkId func, int nargs, int *flag) {
  int i, j, k, res, paramc;
  Table *rk, *rt, *r;
  const TValue *entry;
  StkId rbase;
  Node *node;
  rbase = func;  /* to avoid warnings */
  rt = cl->rtable;
  *flag = res = 0;
  unsigned int hashval = 0;
  /* traverse remember table to check whether it already includes a result; compute hash for all arguments passed first */
  for (i=0; i < nargs; i++) {  /* 4x runtime boost with multi-args as advised by Gemini AI, 7.5.4 */
    luaD_rthashupdate(hashval, func + 1 + i);
  }
  entry = luaH_getnum(rt, hashval + 1);
  if (!ttisnil(entry) && nargs > 0) {  /* we are now traversing the one remember table entry for the potential hit at hashval */
    r = hvalue(entry);
    for (i=0; i < sizenode(r); i++) {  /* the entry for hashval is a hash table */
      node = gnode(r, i);
      TValue *rets = gval(node);
      if (ttisnotnil(rets)) {  /* the function results have been found */
        TValue *args = key2tval(node);  /* arguments */
        if (ttistable(args)) {
          rk = hvalue(args);  /* assign argument table to Table object rk */
          /* prepare argument table traversal */
          *flag = 1;  /* argument found in rtable ? */
          /* check whether arguments in cached key table and function arguments
             are the same and are in the correct order */
          paramc = 0;  /* number of arguments in rtable value table */
          for (j=0; j < rk->sizearray && *flag; j++) {
            if (ttisnotnil(&rk->array[j])) {  /* check current argument */
              paramc++;
              /* do not check values beyond func + paramc */
              *flag = (paramc > nargs || equalobj(L, &rk->array[j], func + paramc));
            }
          }  /* of for */
        } else if (ttisnotnil(args))  {  /* just one argument, 5 % plus tweak, 7.5.6 */
          *flag = equalobj(L, args, func + 1); paramc = 1;
        } else {
          paramc = 0; *flag = 0;
          luaG_runerror(L, "malformed remember table argument list");
        }
        if (*flag && nargs == paramc) {
          /* set results in rtable to stack; since we do not call a function we do not need
             calls to savestack()/restorestack(). */
          rbase = L->top;  /* index of first result */
          if (ttistable(rets)) {
            Table *r = hvalue(rets);
            luaD_checkstack(L, r->sizearray);  /* 2.8.0 change: ensure minimum stack size */
            for (k=0; k < r->sizearray; k++) {
              setobj2s(L, L->top++, &r->array[k]);
            }
          } else if (ttisnotnil(rets))  {  /* just one result */
            luaD_checkstack(L, 1);
            setobj2s(L, L->top++, rets);
          } else {
            luaG_runerror(L, "malformed remember table result list");
          }
          L->rt_hits++;
          return rbase;  /* quit rtable iteration, 7.5.4 fix */
        }
      }
    }
  }
  L->rt_misses++;
  return func;
}


/* enters the results of a function into the remember table (if created); 0.9.2; patched 2.8.0
   base: the first argument of the function */
void luaD_rtableentry (lua_State *L, LClosure *cl, StkId base, StkId firstResult, int nresults) {
  int i, nargs, type;
  Table *rkey, *rval, *rtab;
  StkId basepi;
  const TValue *entry;
  nargs = cl->p->numparams;
  unsigned int hashval = 0;
  if (nargs != 1) {
    /* create a table array for the arguments, this table will be a key in the rtable */
    rkey = luaH_new(L, nargs, 0);
    for (i=0; i < nargs; i++) {
      basepi = base + i;
      type = ttype(basepi);
      if (type > LUA_TSTRING || type < LUA_TBOOLEAN)
        /* 0.12.0: else out of memory errors */
        luaG_runerror(L, "only basic types allowed in remember tables, i.e. numbers,\n"
                         "strings and booleans including fail. Do not pass %s.",
                         luaL_typename(L, type));
      /* 4x runtime boost with multi-args as advised by Gemeini AI, 7.5.4 */
      luaD_rthashupdate(hashval, base + i);
      luaH_setint(L, rkey, i + 1, basepi);  /* 4.6.3 tweak */
      luaC_barriert(L, rkey, basepi);
    }
    sethvalue(L, L->top, rkey);
  } else {
    type = ttype(base);
    if (type > LUA_TSTRING || type < LUA_TBOOLEAN)
        luaG_runerror(L, "only basic types allowed in remember tables, i.e. numbers,\n"
                         "strings and booleans including fail. Do not pass %s.",
                         luaL_typename(L, type));
    /* 4x runtime boost with multi-args as advised by Gemeini AI, 7.5.4 */
    hashval = (unsigned int)hash(base);
    setobj2s(L, L->top, base);
  }
  luaD_checkstack(L, 1);
  L->top++;
  if (nresults == 1) {  /* 5 % plus tweak, 7.5.6 */
    setobj2s(L, L->top, firstResult);
  } else {
    /* create a table array for the results */
    rval = luaH_new(L, nresults, 0);
    /* enter results into the table array */
    for (i=0; i < nresults; i++) {
      luaH_setint(L, rval, i + 1, firstResult + i);  /* 4.6.3 tweak */
      luaC_barriert(L, rval, firstResult + i);
    }
    /* set table with results to stack top */
    sethvalue(L, L->top, rval);
  }  /* L->top-1: argument table
     L->top:   result table */
  entry = luaH_getnum(cl->rtable, hashval + 1);
  if ttisnil(entry) {  /* no entry in remember table yet ? Create one. */
    rtab = luaH_new(L, 0, 1);
  } else {
    rtab = hvalue(entry);
  }
  /* enter arguments~results pair into table */
  setobjt2t(L, luaH_set(L, rtab, L->top - 1), L->top);
  luaC_barriert(L, rtab, L->top);
  /* set table to stack top */
  L->top++;  /* we must increase L->top ! */
  sethvalue(L, L->top, rtab);
  /* enter hashkey ~ [keytable]~[valuetable] (hashkey -> arguments~results) into rtable */
  luaH_setint(L, cl->rtable, hashval + 1, L->top);  /* 4.6.3 tweak */
  luaC_barriert(L, cl->rtable, L->top);
  L->top -= 2;
}


#define checkutype(L,s) \
  if ((s) == NULL || tools_strneq(getstr((s)), typets)) \
    luaG_runerror(L, "type %s expected for argument #%d, got %s.", \
      typets, i + 1, luaT_typenames[(int)ttype(base + i)]);

#define inc_ci(L) \
  ((L->ci == L->end_ci) ? growCI(L) : \
   (condhardstacktests(luaD_reallocCI(L, L->size_ci)), ++L->ci))

/* fixed in Agena 1.4.2 */

void luaD_genparerror (lua_State *L, lua_typecheck typearray) {
  int c, j;
  c = 0;
  for (j=0; j < LUAI_NTYPELIST; j++) {
    if (typearray % LUAI_NTYPELISTMOD != 0) {  /* print type name if type <> null */
      lua_pushstring(L, luaT_typenames[(lua_typecheck)typearray % LUAI_NTYPELISTMOD]);
      lua_pushstring(L, ", ");
      c++;
    }
    typearray /= LUAI_NTYPELISTMOD;
  }
  agn_poptop(L);  /* pop last separator */
  if (c > 1) {
    lua_remove(L, -2);  /* 2.30.3: beautify message, so pop separator */
    lua_pushstring(L, " or ");
    lua_insert(L, -2);
  }
  lua_concat(L, 2*c - 1);
}

/* > if (((tm = fasttm(L, smetatable, TM_OFTYPE)) != NULL) && ttisfunction(tm)):
   is there an __oftype metamethod ?
   > L->top++;
   increase stack
   > call_binTM(L, base + i, base + i, L->top, TM_OFTYPE);
   and put result of metamethod an top of stack
   > if (l_isfalseorfail(L->top) || ttisnil(L->top))
   if result is false, issue an error at function invocation */

#define checkargbasictype(L,smetatable,base,i) { \
  if (((tm = fasttm(L, smetatable, TM_OFTYPE)) != NULL) && ttisfunction(tm)) { \
    L->top++; \
    call_binTM(L, base + i, base + i, L->top, TM_OFTYPE); \
    if (l_isfalseorfail(L->top) || ttisnil(L->top)) \
      luaG_runerror(L, "argument #%d does not satisfy type check metamethod", i + 1); \
    L->top--; \
  } \
}

#define checkargutype(L,stype,smetatable,base,i) { \
  if (((tm = fasttm(L, smetatable, TM_OFTYPE)) != NULL) && ttisfunction(tm)) { \
    if ((stype) == NULL || tools_strneq(getstr((stype)), typets)) \
      luaG_runerror(L, "type %s expected for argument #%d, got %s.", \
        typets, i + 1, luaT_typenames[(int)ttype(base + i)]); \
    L->top++; \
    call_binTM(L, base + i, base + i, L->top, TM_OFTYPE); \
    if (l_isfalseorfail(L->top) || ttisnil(L->top)) \
      luaG_runerror(L, "argument #%d does not satisfy type check metamethod", i + 1); \
    L->top--; \
  } else \
    if ((stype) == NULL || tools_strneq(getstr((stype)), typets)) \
      luaG_runerror(L, "type %s expected for argument #%d, got %s.", \
        typets, i + 1, luaT_typenames[(int)ttype(base + i)]); \
}

#define isoopfn(p)  ((p)->is_oopfn)

int luaD_precall (lua_State *L, StkId func, int nresults) {
  LClosure *cl;
  StkId rbase;
  ptrdiff_t funcr;
  if (!ttisfunction(func)) { /* `func' is not a function? */
    func = tryfuncTM(L, func);  /* check the `function' tag method */
  }
  funcr = savestack(L, func);
  cl = &clvalue(func)->l;
  L->ci->savedpc = L->savedpc;
  rbase = NULL;
  if (!cl->isC) {  /* Agena function ?  Prepare its call */
    CallInfo *ci;
    StkId st, base;
    Proto *p = cl->p;
    int nargs;
    luaD_checkstack(L, p->maxstacksize + p->numparams);  /* 7.9.2 fix taken from Lunacy,
      to prevent segfaults with an abnormally massive number of arguments to a function */
    func = restorestack(L, funcr);
    nargs = cast_int(L->top - func) - 1;
    if (!p->is_vararg) {  /* no varargs? */
      base = func + 1;
      if (L->top > base + p->numparams)
        L->top = base + p->numparams;
    } else {  /* vararg function */
      base = adjust_varargs(L, p, nargs);
      func = restorestack(L, funcr);  /* previous call may change the stack */
    }
    ci = inc_ci(L);  /* now `enter' new function */
    ci->func = func;
    L->base = ci->base = base;
    ci->top = L->base + p->maxstacksize;
    lua_assert(ci->top <= L->stack_last);
    L->savedpc = p->code;  /* starting point */
    ci->tailcalls = 0;
    ci->nresults = nresults;
    /* determine number of arguments for NARGS sysvar */
    ci->nargs = nargs;  /* cast_byte(oldtop - func - 1); */
    if (p->is_typegiven) {  /* new type checking method, 0.20.0, modified 0.21.0; is at least one type check given in the parameter list ? */
      int i, j;
      unsigned int type;
      const char *typets;
      const TValue *tm;
      /* p->numparams must never be exceeded, which will be the case if surplus arguments are passed ! */
      for (i=0; i < p->numparams; i++) {
        /* first check the arguments actually passed */
        if (p->params[i].typearray != 0) {  /* extended 2.1.2; (optional) basic type specified ? */
          lua_typecheck typearray;
          typearray = p->params[i].typearray;
          if (i < ci->nargs) {  /* any argument given ? */
            int flag = 0;
            type = ttype(base + i);
            for (j=0; j < LUAI_NTYPELIST; j++) {
              lua_typecheck curtype = typearray % (lua_typecheck)LUAI_NTYPELISTMOD;
              if (curtype == 0) break;  /* we have no more types passed by the user, bail out, 2.12.1 */
              if (type == curtype || curtype == LUA_TANYTHING ||  /* type matches, extended 2.20.1 */
                (curtype == LUA_TLISTING && (type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG)) ||
                (curtype == LUA_TBASIC && (type == LUA_TNUMBER || type == LUA_TSTRING || type == LUA_TBOOLEAN || type == LUA_TNIL)) ) {
                flag = 1;
                break;  /* 2.5.3 improvement: bail out immediately */
                /* (GREP_POINT) types; if you add new number `types', 2.12.1 extension */
              } else if (type == LUA_TNUMBER) {  /* check for number `subtypes` */
                lua_Number x = nvalue(base + i);
                switch (curtype) {
                  case IAM_INTEGER:  /* 2.14.13 change */
                    flag = tools_isint(x);
                    break;
                  case IAM_POSINT:
                    flag = tools_isposint(x);
                    break;
                  case IAM_NONNEGINT:
                    flag = tools_isnonnegint(x);
                    break;
                  case IAM_NONZEROINT:  /* 4.11.0 */
                    flag = tools_isint(x) && x != 0;
                    break;
                  case IAM_POSITIVE:
                    flag = x > 0;
                    break;
                  case IAM_NEGATIVE:
                    flag = x < 0;
                    break;
                  case IAM_NONNEGATIVE:
                    flag = x >= 0;
                    break;
                }
                if (flag) break;
              }
              typearray /= LUAI_NTYPELISTMOD;
            }
            if (flag) {
              switch (type) {  /* 2.5.3 */
                case LUA_TTABLE: {
                  Table *s = hvalue(base + i);
                  checkargbasictype(L, s->metatable, base, i);
                  break;
                }
                case LUA_TSET: {
                  UltraSet *s = usvalue(base + i);
                  checkargbasictype(L, s->metatable, base, i);
                  break;
                }
                case LUA_TSEQ: {
                  Seq *s = seqvalue(base + i);
                  checkargbasictype(L, s->metatable, base, i);
                  break;
                }
                case LUA_TPAIR: {
                  Pair *s = pairvalue(base + i);
                  checkargbasictype(L, s->metatable, base, i);
                  break;
                }
                case LUA_TREG: {
                  Reg *s = regvalue(base + i);
                  checkargbasictype(L, s->metatable, base, i);
                  break;
                }
                case LUA_TUSERDATA: {
                  Udata *u = rawuvalue(base + i);
                  checkargbasictype(L, u->uv.metatable, base, i);
                  break;
                }
              }
              continue;
            }
            luaD_genparerror(L, p->params[i].typearray);  /* generate error message */
            luaG_runerror(L, "type %s expected for argument #%d, got %s.",
              lua_tostring(L, -1), i + 1 - isoopfn(p), luaT_typenames[(int)type]);  /* 2.24.0 fix */
          } else {
            luaD_genparerror(L, p->params[i].typearray);  /* generate error message */
            luaG_runerror(L, "missing argument #%d (of type %s).",
              i + 1 - isoopfn(p), lua_tostring(L, -1));  /* 2.24.0 fix */
          }
        }
        else if (p->params[i].vartypets != NULL) {  /* optional user-defined type specified ? */
          typets = getstr(p->params[i].vartypets);
          if (i < ci->nargs) {
            switch (ttype(base + i)) {
              case LUA_TSEQ: {
                Seq *s = seqvalue(base + i);
                checkargutype(L, s->type, s->metatable, base, i);
                break;
              }
              case LUA_TTABLE: {
                Table *s = hvalue(base + i);
                checkargutype(L, s->type, s->metatable, base, i);
                break;
              }
              case LUA_TSET: {
                UltraSet *s = usvalue(base + i);
                checkargutype(L, s->type, s->metatable, base, i);
                break;
              }
              case LUA_TPAIR: {
                Pair *s = pairvalue(base + i);
                checkargutype(L, s->type, s->metatable, base, i);
                break;
              }
              case LUA_TREG: {  /* 2.5.3 */
                Reg *s = regvalue(base + i);
                checkargutype(L, s->type, s->metatable, base, i);
                break;
              }
              case LUA_TUSERDATA: {  /* 2.3.0 RC 3 */
                Udata *u = rawuvalue(base + i);
                checkargutype(L, u->uv.type, u->uv.metatable, base, i);
                break;
              }
              case LUA_TFUNCTION: {
                Closure *c = clvalue(base + i);
                if ((c->l.type != NULL && tools_strneq(getstr(c->l.type), typets)) ||  /* 2.16.12 tweak */
                    (c->c.type != NULL && tools_strneq(getstr(c->c.type), typets)))    /* 2.16.12 tweak */
                  luaG_runerror(L, "type %s expected for argument #%d, got %s.",
                    typets, i + 1 - isoopfn(p), luaT_typenames[(int)ttype(base + i)]);
                break;
              }
              default:  /* all other types */
                luaG_runerror(L, "type %s expected for argument #%d, got %s.",
                  typets, i + 1 - isoopfn(p), luaT_typenames[(int)ttype(base + i)]);
            }
          } else  /* Agena 1.4.2 */
            luaG_runerror(L, "missing argument #%d (of type %s).", i + 1 - isoopfn(p), typets);
        }
      }
    }
    for (st = L->top; st < ci->top; st++)
      setnilvalue(st);
    L->top = ci->top;
    if (L->hookmask & LUA_MASKCALL) {
      L->savedpc++;  /* hooks assume 'pc' is already incremented */
      luaD_callhook(L, LUA_HOOKCALL, -1);
      L->savedpc--;  /* correct 'pc' */
    }
    if (cl->rtable == NULL) {
      return PCRLUA;
    }
    else {  /* search remember table */
      int flag;
      rbase = searchrtable(L, cl, ci->func, p->numparams, &flag);
      if (flag) {
        /* entry found and put on stack, do not execute function body */
        luaD_poscall(L, rbase);
        return PCRREMEMBER;
      }
      else {  /* no entry found in rtable, execute body */
        return PCRLUA;
      }
    }
  }
  else {  /* if is a C function, call it */
    CallInfo *ci;
    StkId base;
    int n;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    ci = inc_ci(L);  /* now `enter' new function */
    ci->func = restorestack(L, funcr);
    base = L->base = ci->base = ci->func + 1;
    ci->top = L->top + LUA_MINSTACK;
    lua_assert(ci->top <= L->stack_last);
    ci->nresults = nresults;
    if (cl->rtable != NULL) { /* search remember table, new 0.22.1 */
      int flag;
      rbase = searchrtable(L, cl, func, L->top - base, &flag);
      if (flag) {
        /* entry found and put on stack, do not execute C function */
        luaD_poscall(L, rbase);
        return PCRC;
      }
    }
    if (L->hookmask & LUA_MASKCALL)
      luaD_callhook(L, LUA_HOOKCALL, -1);
    lua_unlock(L);
    n = (*curr_func(L)->c.f)(L);  /* do the actual call */
    lua_lock(L);
    if (n < 0)  /* yielding? */
      return PCRYIELD;
    else {
      luaD_poscall(L, L->top - n);
      return PCRC;
    }
  }
}


static StkId callrethooks (lua_State *L, StkId firstResult) {
  ptrdiff_t fr = savestack(L, firstResult);  /* next call may change stack */
  luaD_callhook(L, LUA_HOOKRET, -1);
  if (f_isLua(L->ci)) {  /* Lua function? */
    while ((L->hookmask & LUA_MASKRET) && L->ci->tailcalls--) /* tail calls, 0.13.3 */
      luaD_callhook(L, LUA_HOOKTAILRET, -1);
  }
  return restorestack(L, fr);
}


int luaD_poscall (lua_State *L, StkId firstResult) {
  StkId res;
  int wanted, i;
  CallInfo *ci;
  if (L->hookmask & LUA_MASKRET)
    firstResult = callrethooks(L, firstResult);
  ci = L->ci--;
  res = ci->func;  /* res == final position of 1st result */
  wanted = ci->nresults;
  L->base = (ci - 1)->base;  /* restore base */
  L->savedpc = (ci - 1)->savedpc;  /* restore savedpc */
  /* move results to correct place */
  for (i=wanted; i != 0 && firstResult < L->top; i--)
    setobjs2s(L, res++, firstResult++);
  while (i-- > 0)
    setnilvalue(res++);
  L->top = res;
  return (wanted - LUA_MULTRET);  /* 0 iff wanted == LUA_MULTRET */
}


/*
** Call a function (C or Lua). The function to be called is at *func.
** The arguments are on the stack, right after the function.
** When returns, all the results are on the stack, starting at the original
** function position.
*/
void luaD_call (lua_State *L, StkId func, int nResults, int gc) {
  if (++L->nCcalls >= LUAI_MAXCCALLS) {
    if (L->nCcalls == LUAI_MAXCCALLS)
      luaG_runerror(L, "C stack overflow");
    else if (L->nCcalls >= (LUAI_MAXCCALLS + (LUAI_MAXCCALLS >> 3)))
      luaD_throw(L, LUA_ERRERR);  /* error while handling stack error */
  }
  if (luaD_precall(L, func, nResults) == PCRLUA) { /* is a Lua function? */
    luaV_execute(L, 1);  /* call it */
  }
  L->nCcalls--;
  /* to prevent invalid reads; do not perform gc when called in a loop, only gc after loop has been completed, 2.29.3 */
  if (gc) luaC_checkGC(L);
}


static void resume (lua_State *L, void *ud) {
  StkId firstArg = cast(StkId, ud);
  CallInfo *ci = L->ci;
  if (L->status == 0) {  /* start coroutine? */
    lua_assert(ci == L->base_ci && firstArg > L->base);
    if (luaD_precall(L, firstArg - 1, LUA_MULTRET) != PCRLUA)
      return;
  }
  else {  /* resuming from previous yield */
    lua_assert(L->status == LUA_YIELD);
    L->status = 0;
    if (!f_isLua(ci)) {  /* `common' yield? */
      /* finish interrupted execution of `OP_CALL' */
      lua_assert(GET_OPCODE(*((ci - 1)->savedpc - 1)) == OP_CALL ||
                 GET_OPCODE(*((ci - 1)->savedpc - 1)) == OP_TAILCALL);
      if (luaD_poscall(L, firstArg))  /* complete it... */
        L->top = L->ci->top;  /* and correct top if not multiple results */
    }
    else  /* yielded inside a hook: just continue its execution */
      L->base = L->ci->base;
  }
  luaV_execute(L, cast_int(L->ci - L->base_ci));
}


static int resume_error (lua_State *L, const char *msg) {
  L->top = L->ci->base;
  setsvalue2s(L, L->top, luaS_new(L, msg));
  incr_top(L);
  lua_unlock(L);
  return LUA_ERRRUN;
}


LUA_API int lua_resume (lua_State *L, int nargs) {  /* 0.13.3 */
  int status;
  lua_lock(L);
  if (L->status != LUA_YIELD && (L->status != 0 || L->ci != L->base_ci))
    return resume_error(L, "cannot resume non-suspended coroutine");
  if (L->nCcalls >= LUAI_MAXCCALLS)
    return resume_error(L, "C stack overflow");
  luai_userstateresume(L, nargs);
  lua_assert(L->errfunc == 0);
  L->baseCcalls = ++L->nCcalls;
  status = luaD_rawrunprotected(L, resume, L->top - nargs);
  if (status != 0) {  /* error? */
    L->status = cast_byte(status);  /* mark thread as `dead' */
    luaD_seterrorobj(L, status, L->top);
    L->ci->top = L->top;
  }
  else {
    lua_assert(L->nCcalls == L->baseCcalls);
    status = L->status;
  }
  --L->nCcalls;
  lua_unlock(L);
  return status;
}


LUA_API int lua_yield (lua_State *L, int nresults) {
  luai_userstateyield(L, nresults);
  lua_lock(L);
  if (L->nCcalls > L->baseCcalls)  /* 0.13.3 */
    luaG_runerror(L, "attempt to yield across metamethod/C-call boundary");
  L->base = L->top - nresults;  /* protect stack slots below */
  L->status = LUA_YIELD;
  lua_unlock(L);
  return -1;
}


int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  unsigned short oldnCcalls = L->nCcalls;
  ptrdiff_t old_ci = saveci(L, L->ci);
  lu_byte old_allowhooks = L->allowhook;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = luaD_rawrunprotected(L, func, u);
  if (status != 0) {  /* an error occurred? */
    StkId oldtop = restorestack(L, old_top);
    luaF_close(L, oldtop);  /* close eventual pending closures */
    luaD_seterrorobj(L, status, oldtop);
    L->nCcalls = oldnCcalls;
    L->ci = restoreci(L, old_ci);
    L->base = L->ci->base;
    L->savedpc = L->ci->savedpc;
    L->allowhook = old_allowhooks;
    restore_stack_limit(L);
  }
  L->errfunc = old_errfunc;
  return status;
}


/*
** Execute a protected parser.
*/
struct SParser {  /* data to `f_parser' */
  ZIO *z;
  Mbuffer buff;  /* buffer to be used by the scanner */
  const char *name;
};

static void f_parser (lua_State *L, void *ud) {
  int i;
  Proto *tf;
  Closure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = luaZ_lookahead(p->z);
  luaC_checkGC(L);
  tf = ((c == LUA_SIGNATURE[0]) ? luaU_undump : luaY_parser)(L, p->z, &p->buff, p->name);
  cl = luaF_newLclosure(L, tf->nups, hvalue(gt(L)));
  cl->l.p = tf;
  for (i = 0; i < tf->nups; i++)  /* initialize eventual upvalues */
    cl->l.upvals[i] = luaF_newupval(L);
  setclvalue(L, L->top, cl);
  incr_top(L);
}


int luaD_protectedparser (lua_State *L, ZIO *z, const char *name) {
  struct SParser p;
  int status;
  p.z = z; p.name = name;
  luaZ_initbuffer(L, &p.buff);
  status = luaD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
  luaZ_freebuffer(L, &p.buff);
  return status;
}

