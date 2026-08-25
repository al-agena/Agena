/*
** $Id: lstate.h,v 2.24 2006/02/06 18:27:59 roberto Exp $
** Global State
** See Copyright Notice in agena.h
*/

#ifndef lstate_h
#define lstate_h

#include "agena.h"

#include "lobject.h"
#include "ltm.h"
#include "lzio.h"


struct lua_longjmp;  /* defined in ldo.c */


/* table of globals */
#define gt(L)                 (&L->l_gt)

/* registry */
#define registry(L)           (&G(L)->l_registry)
#define constants(L)          (&G(L)->l_constants)

/* cells */
#define LUA_CELLSTACKSIZE     128
#define LUA_CELLSTACKBOTTOM   0
#define LUA_NUMSTACKS         4
#define LUA_CHARSTACKS        3
#define LUA_CACHESTACKS       1
#define LUA_NSTACKS           (LUA_NUMSTACKS + LUA_CHARSTACKS + LUA_CACHESTACKS)
#define LUA_DEFAULTSTACK      1

#define cells(L, i)           ((L)->cells[(L)->currentstack][i])
#define stackcells(L, st, i)  ((L)->cells[st][i])
#define getcharcell(L, i)     ((const char *)((L)->charcells[(L)->currentstack - LUA_NUMSTACKS] + i))
#define charcell(L, i)        ((L)->charcells[(L)->currentstack - LUA_NUMSTACKS][i])
#define stacktop(L)           ((L)->stacktop[(L)->currentstack])
#define stackmax(L)           ((L)->stackmaxsize[(L)->currentstack])
#define isnumstack(L)         ((L)->currentstack < LUA_NUMSTACKS)
#define ischarstack(L)        (((L)->currentstack >= LUA_NUMSTACKS) && ((L)->currentstack < LUA_NUMSTACKS + LUA_CHARSTACKS))
#define iscachestack(L)       ((L)->currentstack >= (LUA_NUMSTACKS + LUA_CHARSTACKS))

#define agn_getrthits(L)      (L)->rt_hits
#define agn_getrtmisses(L)    (L)->rt_misses

/* extra stack space to handle TM calls and some other extras */
#define EXTRA_STACK           5

#define BASIC_CI_SIZE         8

#define BASIC_STACK_SIZE      (2*LUA_MINSTACK)


typedef struct stringtable {
  GCObject **hash;
  lu_int32 nuse;  /* number of elements */
  int size;
} stringtable;


/*
** information about a call
*/
typedef struct CallInfo {
  StkId base;  /* base for this function */
  StkId func;  /* function index in the stack */
  StkId top;   /* top for this function */
  const Instruction *savedpc;
  int nresults;   /* expected number of results from this function */
  int tailcalls;  /* number of tail calls lost under this entry */
  lu_byte nargs;
} CallInfo;



#define curr_func(L)   (clvalue(L->ci->func))
#define ci_func(ci)    (clvalue((ci)->func))
#define f_isLua(ci)    (!ci_func(ci)->c.isC)
#define isLua(ci)      (ttisfunction((ci)->func) && f_isLua(ci))


/*
** `global state', shared by all threads of this state
*/
typedef struct global_State {
  stringtable strt;  /* hash table for strings */
  lua_Alloc frealloc;  /* function to reallocate memory */
  void *ud;         /* auxiliary data to `frealloc' */
  lu_byte currentwhite;
  lu_byte gcstate;  /* state of garbage collector */
  int sweepstrgc;  /* position of sweep in `strt' */
  GCObject *rootgc;  /* list of all collectable objects */
  GCObject **sweepgc;  /* position of sweep in `rootgc' */
  GCObject *gray;  /* list of gray objects */
  GCObject *grayagain;  /* list of objects to be traversed atomically */
  GCObject *weak;  /* list of weak tables (to be cleared) */
  GCObject *tmudata;  /* last element of list of userdata to be GC */
  Mbuffer buff;  /* temporary buffer for string concatenation */
  lu_mem GCthreshold;
  lu_mem totalbytes;  /* number of bytes currently allocated */
  lu_mem estimate;  /* an estimate of number of bytes actually in use */
  lu_mem gcdept;  /* how much GC is `behind schedule' */
  int gcpause;  /* size of pause between successive GCs */
  int gcstepmul;  /* GC `granularity' */
  lua_CFunction panic;  /* to be called in unprotected errors */
  TValue l_registry;
  TValue l_constants;
  struct lua_State *mainthread;
  UpVal uvhead;  /* head of double-linked list of all open upvalues */
  struct Table *mt[NUM_TAGS];  /* metatables for basic types */
  TString *tmname[TM_N];  /* array with tag-method names */
  lua_WarnFunction warnf;  /* warning function */
  void *ud_warn;         /* auxiliary data to 'warnf' */
} global_State;


/*
** `per thread' state
*/

struct lua_State {
  CommonHeader;
  lu_byte status;
  StkId top;          /* first free slot in the stack */
  StkId base;         /* base of current function */
  global_State *l_G;
  CallInfo *ci;       /* call info for current function */
  const Instruction *savedpc;  /* `savedpc' of current function */
  StkId stack_last;   /* last free slot in the stack */
  StkId stack;        /* stack base */
  CallInfo *end_ci;   /* points after end of ci array*/
  CallInfo *base_ci;  /* array of CallInfo's */
  int stacksize;
  int size_ci;        /* size of array `base_ci' */
  unsigned short nCcalls;     /* number of nested C calls */
  unsigned short baseCcalls;  /* nested C calls when resuming coroutine, 0.13.3 */
  lu_byte hookmask;
  lu_byte allowhook;
  int basehookcount;
  int hookcount;
  lua_Hook hook;
  TValue l_gt;  /* table of globals */
  TValue env;   /* temporary place for environments */
  GCObject *openupval;  /* list of open upvalues in this stack */
  GCObject *gclist;
  struct lua_longjmp *errorJmp;  /* current error recover point */
  ptrdiff_t errfunc;  /* current error handling function (stack index) */
  lu_byte settings;   /* binary hgfedcba,
     a = bitwise_signed flag, b = emptyline flag, c = libname reset option, d = print `LongTable` option
     e = debug infos, f = console/AgenaEdit flag, g = printzeroedcomplex, h = promptemptyline */
  lu_byte settings2;  /* binary hgfedcba,
     a = run ini file(s), b = run lib/library.agn file, c = ISO8601 flag, d = constants activated, duplicate warnings */
  lu_byte settings3;  /* binary hgfedcba
     a = enclose strings in single quotes, b = enclose strings in double quotes, c = enclose strings in backquotes */
  lu_byte vmsettings;  /* settings for vm control, binary hgfedcba,
     a = kahan/ozawa summation round-off prevention */
  char *numberformat;  /* format of numbers, 0.27.0 */
  char *histfile;  /* logfile name, 5.2.2 */
  size_t regsize;  /* default register size, 2.3.0 RC 3*/
  lua_Number Eps;  /* Epsilon for `~=' operator */
  lua_Number DoubleEps;  /* 2.21.8 */
  lua_Number hEps;  /* 2.31.1 */
  int buffersize;
  int errmlinebreak;  /* chars per line in symtax error messages */
  lua_Number *cells[LUA_NUMSTACKS];  /* 2.9.4/2.9.7 */
  unsigned char *charcells[LUA_CHARSTACKS];  /* 2.12.7 */
  int stacktop[LUA_NSTACKS];  /* 2.9.4, do NOT change to size_t due to incorrect comparisons with int stack positions */
  int stackmaxsize[LUA_NSTACKS];    /* 2.9.4, do NOT change to size_t due to incorrect comparisons with int stack positions */
  struct lua_State *C;
  lu_byte currentstack;  /* 2.9.7, number of the current stack */
  lu_byte formerstack;   /* 2.37.2, number of the previous stack, before changing with switchd */
  lu_byte constwarn;     /* 2.21.2, initially set to 1: print constant re-declaration warning if done on the command-line, but do so only once. */
  lu_byte arithstate;    /* 2.32.0, status of a division done by the `mul` & `div` operators; from right to left:
      bit 1: denominator is zero (`div` only),
      bit 2: log2(numerator) - log2(denominator) > log2(environ.system().numberranges.maxdouble), i.e. a very large value is to be divided by
             a very small value close to 0; see: https://stackoverflow.com/questions/18234311/how-close-to-division-by-zero-can-i-get
      bit 3: fabs(numerator) < L->div_closetozero && fabs(denominator) < L->div_closetozero */
  lua_Number div_closetozero;
  lua_Number starttime;
  size_t rt_hits;    /* number of remember table hits, 7.5.4 */
  size_t rt_misses;  /* number of remember table misses, 7.5.4 */
  /* 7.8.3: We cannot use a structure that has been assigned in agena.c = agena.exe and shall be used in loslib.c = agena.dll */
  int agn_argc;
  char **agn_argv;
};

/* 7.8.3: We cannot use a structure that has been assigned in agena.c = agena.exe and is used in loslib.c = agena.dll */
#define agn_get_argc(L)  ((L)->agn_argc)
#define agn_get_argv(L)  ((L)->agn_argv)

#define G(L)   (L->l_G)


/*
** Union of all collectable objects
*/
union GCObject {
  GCheader gch;
  union TString ts;
  union Udata u;
  union Closure cl;
  struct Table h;
  struct Proto p;
  struct UpVal uv;
  struct lua_State th;  /* thread */
  struct UltraSet l;
  struct Seq a;
  struct Reg r;
  struct Pair e;
};


/* macros to convert a GCObject into a specific value */
#define rawgco2ts(o)    check_exp((o)->gch.tt == LUA_TSTRING, &((o)->ts))
#define gco2ts(o)       (&rawgco2ts(o)->tsv)
#define rawgco2u(o)     check_exp((o)->gch.tt == LUA_TUSERDATA, &((o)->u))
#define gco2u(o)        (&rawgco2u(o)->uv)
#define gco2cl(o)       check_exp((o)->gch.tt == LUA_TFUNCTION, &((o)->cl))
#define gco2h(o)        check_exp((o)->gch.tt == LUA_TTABLE, &((o)->h))
#define gco2l(o)        check_exp((o)->gch.tt == LUA_TSET, &((o)->l))
#define gco2a(o)        check_exp((o)->gch.tt == LUA_TSEQ, &((o)->a))
#define gco2r(o)        check_exp((o)->gch.tt == LUA_TREG, &((o)->r))
#define gco2pair(o)     check_exp((o)->gch.tt == LUA_TPAIR, &((o)->e))
#define gco2complex(o)  check_exp((o)->gch.tt == LUA_TCOMPLEX, &((o)->c))
#define gco2p(o)        check_exp((o)->gch.tt == LUA_TPROTO, &((o)->p))
#define gco2uv(o)       check_exp((o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define ngcotouv(o) \
   check_exp((o) == NULL || (o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define gco2th(o)       check_exp((o)->gch.tt == LUA_TTHREAD, &((o)->th))

/* macro to convert any Lua object into a GCObject */
#define obj2gco(v)   (cast(GCObject *, (v)))


LUAI_FUNC lua_State *luaE_newthread (lua_State *L);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);

LUAI_FUNC void luaE_warning (lua_State *L, const char *msg, int tocont);
LUAI_FUNC void luaE_warnerror (lua_State *L, const char *where);

#endif

