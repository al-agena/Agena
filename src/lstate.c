/*
** $Id: lstate.c,v 2.36 2006/05/24 14:15:50 roberto Exp $
** Global State
** See Copyright Notice in agena.h
*/


#include <stddef.h>
#include <stdlib.h>  /* for malloc */
#include <string.h>  /* for memset */

#define lstate_c
#define LUA_CORE

#include "agena.h"

#include "agnxlib.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lset.h"
#include "ltm.h"
#include "agnconf.h"
#include "agnhlps.h"

#define state_size(x)  (sizeof(x) + LUAI_EXTRASPACE)
#define fromstate(l)   (cast(lu_byte *, (l)) - LUAI_EXTRASPACE)
#define tostate(l)     (cast(lua_State *, cast(lu_byte *, l) + LUAI_EXTRASPACE))

/*
** Main thread combines a thread state and the global state
*/
typedef struct LG {
  lua_State l;
  global_State g;
} LG;



static void stack_init (lua_State *L1, lua_State *L) {
  /* initialize CallInfo array */
  L1->base_ci = luaM_newvector(L, BASIC_CI_SIZE, CallInfo);
  L1->ci = L1->base_ci;
  L1->size_ci = BASIC_CI_SIZE;
  L1->end_ci = L1->base_ci + L1->size_ci - 1;
  /* initialize stack array */
  L1->stack = luaM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, TValue);
  L1->stacksize = BASIC_STACK_SIZE + EXTRA_STACK;
  L1->top = L1->stack;
  L1->stack_last = L1->stack + (L1->stacksize - EXTRA_STACK) - 1;
  /* initialize first ci */
  L1->ci->func = L1->top;
  setnilvalue(L1->top++);  /* `function' entry for this `ci' */
  L1->base = L1->ci->base = L1->top;
  L1->ci->top = L1->top + LUA_MINSTACK;
}


static void freestack (lua_State *L, lua_State *L1) {  /* changed 2.1 RC 2 */
  struct lua_longjmp *pj, *pprev, *pnext;
  luaM_freearray(L, L1->base_ci, L1->size_ci, CallInfo);
  luaM_freearray(L, L1->stack, L1->stacksize, TValue);
  /* free try-catch info */
  pj = L->errorJmp;
  pnext = NULL;
  while (pj) {
    pprev = pj->previous;
    if (pj->type == JMPTYPE_TRY) {
      if (pnext == NULL)
        L->errorJmp = pprev;
      else
        pnext->previous = pprev;
      luaM_free(L, pj);
    }
    else
      pnext = pj;
    pj = pprev;
  }
}


/*
** open parts that may cause memory-allocation errors
*/
static void f_luaopen (lua_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack */
  sethvalue(L, gt(L), luaH_new(L, 0, 2));  /* table of globals */
  sethvalue(L, registry(L), luaH_new(L, 0, 2));  /* registry */
  setusvalue(L, constants(L), agnUS_new(L, 8));  /* global constants, 2.18.2 */
  luaS_resize(L, MINSTRTABSIZE);  /* initial size of string table */
  luaT_init(L);
  luaX_init(L);
  luaS_fix(luaS_newliteral(L, MEMERRMSG));
  g->GCthreshold = 4*g->totalbytes;
}


static void preinit_state (lua_State *L, global_State *g) {
  size_t i;
  G(L) = g;
  L->stack = NULL;
  L->stacksize = 0;
  L->errorJmp = NULL;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->size_ci = 0;
  L->nCcalls = L->baseCcalls = 0;  /* 0.13.3 */
  L->status = 0;
  L->base_ci = L->ci = NULL;
  L->savedpc = NULL;
  L->errfunc = 0;
  /* from right to left: bitwise_signed flag = false, emptyline flag = true, bitwise setting */
  L->settings = 2;    /* = 0b00000010 */
  /* from right to left: noini = false, nomainlib = false, iso8601 flag = true, constants = true,
                         duplicate warnings = true, seqautoshrink = true, skipagenapath = false;
                         constanttoobig = false */
  L->settings2 = 60;  /* = 0b00111100 */
  /* from right to left: numeric for loop adjustment around zero; kahan/ozawa summation round-off prevention;
                         kahan/babuska summation. Use ONLY for loop auto-correction algorithms (grep "VMSETTINGS" lvm.c */
  L->settings3 = 0;   /* = 0b00000000 */
  /* from right to left: &1) print strings in single quotes, &2) print strings in double quotes,
  &4) print strings in backquotes, &8) history, &16) in basic arithmetic, null is zero (default is: no) */
  L->vmsettings = 1;  /* = 0b00000001 */
  /* from right to left: for-loop adjustment, for loop increment with kahan-ozawa, for loop increment with  kahan-babuska */
  /* 2.8.6, string literal must be duplicated so that it can be changed with agn_setdigits */
  L->numberformat = tools_strdup(LUA_NUMBER_FMT);
  if (!L->numberformat)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.");
  /* 5.2.2, logfile name */
  L->histfile = tools_strdup(AGN_LOGFILE);
  if (!L->histfile) {

  }
  L->regsize = LUAI_REGSIZE;  /* 2.3.0 RC 3 */
  L->Eps = AGN_EPSILON;
  L->DoubleEps = DBL_EPSILON;
  L->hEps = AGN_HEPSILON;  /* 2.31.1 */
  L->buffersize = LUAL_BUFFERSIZE;
  L->errmlinebreak = 70;  /* 2.11.4 */
  setnilvalue(gt(L));
  L->currentstack = LUA_DEFAULTSTACK;  /* changed 2.14.2 */
  L->formerstack = LUA_DEFAULTSTACK;   /* added 2.37.2 */
  for (i=0; i < LUA_NUMSTACKS; i++) {
    L->cells[i] = malloc(LUA_CELLSTACKSIZE * sizeof(lua_Number));
    if (L->cells[i] == NULL) {
      size_t j;
      for (j=0; j < i; j++) { xfree(L->cells[j]); }
      return;
    }
    L->stacktop[i] = LUA_CELLSTACKBOTTOM;  /* first free slot, 2.9.4 */
    L->stackmaxsize[i] = LUA_CELLSTACKSIZE;  /* 2.9.4 */
  }
  /* initialise stack 0, 2.14.2 */
  for (i=0; i < LUA_CELLSTACKSIZE; i++)
    L->cells[0][i] = 0;
  L->stacktop[0] = LUA_CELLSTACKSIZE;
  for (i=0; i < LUA_CHARSTACKS; i++) {  /* 2.12.7 */
    L->charcells[i] = malloc(LUA_CELLSTACKSIZE * sizeof(unsigned char));
    L->stacktop[i + LUA_NUMSTACKS] = LUA_CELLSTACKBOTTOM;  /* first free slot */
    L->stackmaxsize[i + LUA_NUMSTACKS] = LUA_CELLSTACKSIZE;
  }
  /* cache stack */
  for (i=0; i < LUA_CACHESTACKS; i++) {  /* 2.37.0 */
    L->stacktop[i + LUA_NUMSTACKS + LUA_CHARSTACKS] = LUA_CELLSTACKBOTTOM;  /* first free slot */
    L->stackmaxsize[i + LUA_NUMSTACKS + LUA_CHARSTACKS] = LUA_CELLSTACKSIZE;
  }
  L->C = NULL;  /* 2.37.0 */
  L->constwarn = 1;  /* print constant re-declaration warning if done on the command-line, but do so only once. 2.21.2 */
  L->arithstate = 0;
  L->div_closetozero = L->DoubleEps;
  L->starttime = tools_time();
  L->rt_hits = 0;  /* remember tbale, 7.5.4 */
  L->rt_misses = 0;  /* ditto */
  /* 7.8.3: We cannot use a structure that has been assigned in agena.c = agena.exe and is used in loslib.c = agena.dll */
  L->agn_argc = 0;
  L->agn_argv = NULL;
}


static void close_mallocedmem (lua_State *L) {
  size_t i;
  for (i=0; i < LUA_NUMSTACKS; i++) {  /* 2.9.7 */
    xfree(L->cells[i]);
  }
  for (i=0; i < LUA_CHARSTACKS; i++) {  /* 2.14.2 */
    xfree(L->charcells[i]);
  }
  xfree(L->numberformat);  /* 2.9.6 fix */
  xfree(L->histfile);       /* 5.2.2 */
}


/* The cache stack is closed via agnL_onexit to free all memory correctly without Valgrind complaining. */
static void close_state (lua_State *L) {
  global_State *g = G(L);
  close_mallocedmem(L);
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  luaC_freeall(L);  /* collect all objects */
  lua_assert(g->rootgc == obj2gco(L));
  lua_assert(g->strt.nuse == 0);
  luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size, TString *);
  luaZ_freebuffer(L, &g->buff);
  freestack(L, L);
  lua_assert(g->totalbytes == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), state_size(LG), 0);
}


LUA_API void lua_closestate (lua_State *L) {  /* OS/2 and DOS do not like this function ... */
  close_state(L);
}


lua_State *luaE_newthread (lua_State *L) {
  lua_State *L1 = tostate(luaM_malloc(L, state_size(lua_State)));
  luaC_link(L, obj2gco(L1), LUA_TTHREAD);
  preinit_state(L1, G(L));
  stack_init(L1, L);  /* init stack */
  setobj2n(L, gt(L1), gt(L));  /* share table of globals */
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  lua_assert(iswhite(obj2gco(L1)));
  return L1;
}


void luaE_freethread (lua_State *L, lua_State *L1) {
  luaF_close(L1, L1->stack);  /* close all upvalues for this thread */
  lua_assert(L1->openupval == NULL);
  luai_userstatefree(L1);
  freestack(L, L1);
  luaM_freemem(L, fromstate(L1), state_size(lua_State));
}


LUA_API lua_State *lua_newstate (lua_Alloc f, void *ud) {
  int i;
  lua_State *L;
  global_State *g;
  void *l = (*f)(ud, NULL, 0, state_size(LG));
  if (l == NULL) return NULL;
  L = tostate(l);
  g = &((LG *)L)->g;
  L->next = NULL;
  L->tt = LUA_TTHREAD;
  g->currentwhite = bit2mask(WHITE0BIT, FIXEDBIT);
  L->marked = luaC_white(g);
  set2bits(L->marked, FIXEDBIT, SFIXEDBIT);
  preinit_state(L, g);
  g->frealloc = f;
  g->ud = ud;
  g->mainthread = L;
  g->uvhead.u.l.prev = &g->uvhead;
  g->uvhead.u.l.next = &g->uvhead;
  g->GCthreshold = 0;  /* mark it as unfinished state */
  g->strt.size = 0;
  g->strt.nuse = 0;
  g->strt.hash = NULL;
  setnilvalue(registry(L));
  setnilvalue(constants(L));  /* 2.18.2 */
  luaZ_initbuffer(L, &g->buff);
  g->panic = NULL;
  g->gcstate = GCSpause;
  g->rootgc = obj2gco(L);
  g->sweepstrgc = 0;
  g->sweepgc = &g->rootgc;
  g->gray = NULL;
  g->grayagain = NULL;
  g->weak = NULL;
  g->tmudata = NULL;
  g->totalbytes = sizeof(LG);
  g->gcpause = LUAI_GCPAUSE;
  g->gcstepmul = LUAI_GCMUL;
  g->gcdept = 0;
  g->warnf = NULL;
  g->ud_warn = NULL;
  for (i=0; i < NUM_TAGS; i++) g->mt[i] = NULL;
  if (luaD_rawrunprotected(L, f_luaopen, NULL) != 0) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  else
    luai_userstateopen(L);
  return L;
}


static void callallgcTM (lua_State *L, void *ud) {
  UNUSED(ud);
  luaC_callGCTM(L);  /* call GC metamethods for all udata */
}


LUA_API void lua_close (lua_State *L) {
  L = G(L)->mainthread;  /* only the main thread can be closed */
  lua_lock(L);
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  luaC_separateudata(L, 1);  /* separate udata that have GC metamethods */
  L->errfunc = 0;  /* no error function during GC metamethods */
  do {  /* repeat until no more errors */
    L->ci = L->base_ci;
    L->base = L->top = L->ci->base;
    L->nCcalls = L->baseCcalls = 0;  /* 0.13.3 */
  } while (luaD_rawrunprotected(L, callallgcTM, NULL) != 0);
  lua_assert(G(L)->tmudata == NULL);
  luai_userstateclose(L);
  close_state(L);
}


void luaE_warning (lua_State *L, const char *msg, int tocont) {
  lua_WarnFunction wf = G(L)->warnf;
  if (wf != NULL)
    wf(G(L)->ud_warn, msg, tocont);
}


/*
** Generate a warning from an error message
*/
void luaE_warnerror (lua_State *L, const char *where) {
  TValue *errobj = s2v(L->top - 1);  /* error object */
  const char *msg = (ttisstring(errobj))
                  ? svalue(errobj)
                  : "error object is not a string";
  /* produce warning "error in %s (%s)" (where, msg) */
  luaE_warning(L, "error in ", 1);
  luaE_warning(L, where, 1);
  luaE_warning(L, " (", 1);
  luaE_warning(L, msg, 1);
  luaE_warning(L, ")", 0);
}

