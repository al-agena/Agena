/*
** $Id: lparser.c,v 2.42 2006/06/05 15:57:59 roberto Exp $
** Lua/Agena Parser
** See Copyright Notice in agena.h
*/

#include <stdio.h>   /* for fputs, stdout, ... */
#include <string.h>

#define lparser_c
#define LUA_CORE

#include "agena.h"

#include "agnconf.h"
#include "agnxlib.h"
#include "lcode.h"
#include "ldo.h"
#include "lfunc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lset.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"

#define getlocvar(fs, i)   ((fs)->f->locvars[(fs)->actvar[i]])

#define luaY_checklimit(fs,v,l,m)   if ((v)>(l)) errorlimit(fs,l,m)

#define str_getname(l)    ((l)->t.seminfo.ts)  /* 2.1.1 / 2.14.9 */

#define CONSTANTS_ON  ((ls->L->settings2 & 8))
#define SHADOW_ON     ((ls->L->settings2 & 16))

/*
** nodes for block list (list of active blocks)
*/
typedef struct BlockCnt {
  struct BlockCnt *previous;  /* chain */
  int breaklist;  /* list of jumps out of this loop */
  int continuelist;  /* list of jumps to the loop's test */
  int redolist;  /* for `redo` statement, Agena 2.1 RC 1 */
  int relaunchlist;  /* for `relaunch` statement, Agena 2.2.0 RC 4 */
  lu_byte nactvar;  /* # active locals outside the breakable structure */
  lu_byte upval;  /* true if some variable in the block is an upvalue */
  lu_byte isbreakable;  /* 0: normal block, 1: loop, 2: try-catch, 2.1 RC 2 */
} BlockCnt;


/*
** structure to chain all variables in the left-hand side of an
** assignment
*/
struct LHS_assign {
  struct LHS_assign *prev;
  expdesc v;  /* variable (global, local, upvalue, oop call, or indexed) */
};


/*
** prototypes for mostly recursive non-terminal functions
*/
static void   chunk (LexState *ls);
static void   fnchunk (LexState *ls, int oneret);  /* changed 0.5.2 */
static void   expr (LexState *ls, expdesc *v);  /* changed 0.5.2 */
static void   funcargexpr (LexState *ls, expdesc *v, int isunaryop);  /* added 0.12.0 */
static int    primaryexp (LexState *ls, expdesc *v, int checkconst, int expression);
static int    simpleexp (LexState *ls, expdesc *v, int expression);  /* added 1.4 */
static void   datasimpleexp (LexState *ls, expdesc *v, int signum);  /* added 2.9.3 */
static BinOpr subexpr (LexState *ls, expdesc *v, unsigned int limit, int isunaryop, int *lasttoken, int expression, int isvector);
static void   localstat (LexState *ls, int mode);
static void   procstat (LexState *ls, int line, int type);
static void   multiargop (LexState *ls, expdesc *e, int line, int tkop, int vmop);
static int    statement (LexState *ls);
static void   changevalueop (LexState *ls, expdesc *v, int mode, int op, unsigned int indices, int intoreg);
static void   changevaluefn (LexState *ls, int op);
static void   plusplus (LexState *ls, expdesc *v, int op, int base, int expression, int fix);
static int    block_follow (int token, int isop);

static void anchor_token (LexState *ls) {
  if (ls->t.token == TK_NAME || ls->t.token == TK_STRING) {
    TString *ts = str_getname(ls);
    luaX_newstring(ls, getstr(ts), ts->tsv.len);
  }
}


/* new 2.20.0, taken from: https://gist.github.com/mingodad/47f61a4997f5be63c9e0a9394a1f68b6,
   written by Domingo Alvarez Duarte */
#define MAXSRC          80

static void parser_warning (LexState *ls, const char *msg) {
  char buff[MAXSRC];
  luaO_chunkid(buff, getstr(ls->source), MAXSRC);
  msg = luaO_pushfstring(ls->L, "%s:%d: warning %s.", buff, ls->linenumber, msg);
  fprintf(stderr, "%s\n", msg);
  fflush(stderr);
}


static void error_expected (LexState *ls, int token) {
  luaX_syntaxerror(ls,
      luaO_pushfstring(ls->L, LUA_QS " expected", luaX_token2str(ls, token)));
}


static void error_expected2 (LexState *ls, int token1, int token2) {
  luaX_syntaxerror(ls,
      luaO_pushfstring(ls->L, LUA_QS " or " LUA_QS " expected",
        luaX_token2str(ls, token1),
        luaX_token2str(ls, token2)));
}


static void errorlimit (FuncState *fs, int limit, const char *what) {
  const char *msg = (fs->f->linedefined == 0) ?
    luaO_pushfstring(fs->L, "main procedure has more than %d %s", limit, what) :
    luaO_pushfstring(fs->L, "procedure at line %d has more than %d %s",
                            fs->f->linedefined, limit, what);
  luaX_lexerror(fs->ls, msg, 0);
}


static int testnext (LexState *ls, int c) {
  if (ls->t.token == c) {
    luaX_next(ls);
    return 1;
  }
  else return 0;
}


static void check (LexState *ls, int c) {
  if (ls->t.token != c)
    error_expected(ls, c);
}


static void check2 (LexState *ls, int c, int d) {
  if (!(ls->t.token == c || ls->t.token == d))
    error_expected(ls, c);
}


static void checknext (LexState *ls, int c) {
  check(ls, c);
  luaX_next(ls);
}


static void checknext2 (LexState *ls, int c, int d) {
  if (ls->t.token == c || ls->t.token == d)
    luaX_next(ls);
  else
    error_expected(ls, c);
}


#define isnext(l, c)   ((l)->t.token == (c))    /* 2.1.1 / 2.14.9 */

#define check_condition(ls,c,msg)   { if (!(c)) luaX_syntaxerror(ls, msg); }

static void check_match (LexState *ls, int what, int who, int where) {
  if (!testnext(ls, what)) {
    if (where == ls->linenumber)
      error_expected(ls, what);
    else {
      luaX_syntaxerror(ls, luaO_pushfstring(ls->L,
             LUA_QS " expected (to close " LUA_QS " at line %d)",
              luaX_token2str(ls, what), luaX_token2str(ls, who), where));
    }
  }
}


static int check_match2 (LexState *ls, int what1, int what2, int who, int where) {  /* 2.0 RC 2 */
  int t1 = 0;
  if (!( (t1 = testnext(ls, what1)) || testnext(ls, what2) )) {
    if (where == ls->linenumber)
      error_expected2(ls, what1, what2);
    else {
      luaX_syntaxerror(ls, luaO_pushfstring(ls->L,
             LUA_QS " or " LUA_QS " expected (to close " LUA_QS " at line %d)",
             luaX_token2str(ls, what1), luaX_token2str(ls, what2),
             luaX_token2str(ls, who), where));
    }
  }
  return (t1) ? 0 : 1;
}


static int check_match3 (LexState *ls, int what1, int what2, int what3, int who, int where) {  /* 2.26.0 */
  int t1 = 0;
  if (!( (t1 = testnext(ls, what1)) || testnext(ls, what2) || testnext(ls, what3))) {
    if (where == ls->linenumber)
      error_expected2(ls, what1, what2);
    else {
      luaX_syntaxerror(ls, luaO_pushfstring(ls->L,
             LUA_QS " or " LUA_QS " expected (to close " LUA_QS " at line %d)",
             luaX_token2str(ls, what1), luaX_token2str(ls, what2),
             luaX_token2str(ls, who), where));
    }
  }
  return (t1) ? 0 : 1;
}


static TString *str_checkname (LexState *ls) {
  TString *ts;
  check2(ls, TK_NAME, TK_QMARK);
  ts = str_getname(ls);
  luaX_next(ls);
  return ts;
}


static void init_exp (expdesc *e, expkind k, int i) {
  e->f = e->t = NO_JUMP;
  e->k = k;
  e->u.s.info = i;
}


static void init_number (FuncState *fs, expdesc *e, lua_Number n) {
  e->f = e->t = NO_JUMP;
  e->k = VKNUM;
  e->u.s.info = 0;
  e->u.nval = (lua_Number)n;
  luaK_exp2anyreg(fs, e);
}


static void codestring (LexState *ls, expdesc *e, TString *s) {
  init_exp(e, VK, luaK_stringK(ls->fs, s));
}


static void checkname (LexState *ls, expdesc *e) {
  codestring(ls, e, str_checkname(ls));
}


static int registerlocalvar (LexState *ls, TString *varname, int isconst) {
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int oldsize;
  oldsize = f->sizelocvars;
  luaM_growvector(ls->L, f->locvars, fs->nlocvars, f->sizelocvars,
                  LocVar, SHRT_MAX, "too many local variables");
  while (oldsize < f->sizelocvars) {
    f->locvars[oldsize].isconst = 0;
    f->locvars[oldsize++].varname = NULL;
  }
  f->locvars[fs->nlocvars].varname = varname;
  f->locvars[fs->nlocvars].isconst = isconst;
  luaC_objbarrier(ls->L, f, varname);
  return fs->nlocvars++;
}


#define new_localvarliteral(ls,v,n) \
  new_localvar(ls, luaX_newstring(ls, "" v, (sizeof(v)/sizeof(char))-1), n, 0, 0)

/* changed 2.20.0, Lua power patch taken from: https://gist.github.com/mingodad/47f61a4997f5be63c9e0a9394a1f68b6 &
   http://lua-users.org/wiki/LuaPowerPatches; written by Domingo Alvarez Duarte, modified by awalz */
static void new_localvar (LexState *ls, TString *name, int n, int check, int isconst) {  /* 2.14.6, compiler warning patch */
  FuncState *fs = ls->fs;
  int nactvarpn = fs->nactvar + n;
  /* allow '_' and internal '(...)' duplicates */
  const char *strname = getstr(name);
  if (check && !(strname[0] == '(' || (name->tsv.len == 1 && strname[0] == '_'))) {
    int vidx = 0;
    for (; vidx < nactvarpn; ++vidx) {
      if (name == getlocvar(fs, vidx).varname) {
        int saved_top = lua_gettop(ls->L);
        if (SHADOW_ON && fs->bl && vidx < fs->bl->nactvar)
          parser_warning(ls, luaO_pushfstring(ls->L,
            "\b: name " LUA_QS " already declared, will be shadowed", strname));
        else if (SHADOW_ON)
          parser_warning(ls, luaO_pushfstring(ls->L,
            "\b: name " LUA_QS " already declared", strname));
        lua_settop(ls->L, saved_top);
      }
    }
  }
  luaY_checklimit(fs, nactvarpn + 1, LUAI_MAXVARS, "local variables");
  fs->actvar[nactvarpn] = cast(unsigned short, registerlocalvar(ls, name, isconst));
}


static void adjustlocalvars (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  fs->nactvar = cast_byte(fs->nactvar + nvars);  /* 7.1.7a change to prevent compiler warnings in GCC 15.2 */
  for (; nvars > 0; nvars--) {
    getlocvar(fs, fs->nactvar - nvars).startpc = fs->pc;
  }
}


static void removevars (LexState *ls, int tolevel) {
  FuncState *fs = ls->fs;
  while (fs->nactvar > tolevel)
    getlocvar(fs, --fs->nactvar).endpc = fs->pc;
}


static int indexupvalue (FuncState *fs, TString *name, expdesc *v) {
  int i;
  Proto *f = fs->f;
  int oldsize = f->sizeupvalues;
  for (i=0; i < f->nups; i++) {
    if (fs->upvalues[i].k == v->k && fs->upvalues[i].info == v->u.s.info) {
      lua_assert(f->upvalues[i] == name);
      return i;
    }
  }
  /* new one */
  luaY_checklimit(fs, f->nups + 1, LUAI_MAXUPVALUES, "upvalues");
  luaM_growvector(fs->L, f->upvalues, f->nups, f->sizeupvalues,
                  TString *, MAX_INT, "");
  while (oldsize < f->sizeupvalues) f->upvalues[oldsize++] = NULL;
  f->upvalues[f->nups] = name;
  luaC_objbarrier(fs->L, f, name);
  lua_assert(v->k == VLOCAL || v->k == VUPVAL);
  fs->upvalues[f->nups].k = cast_byte(v->k);
  fs->upvalues[f->nups].info = cast_byte(v->u.s.info);
  return f->nups++;
}


static int searchvar (FuncState *fs, TString *n) {
  int i;
  for (i=fs->nactvar - 1; i >= 0; i--) {
    if (n == getlocvar(fs, i).varname)
      return i;
  }
  return -1;  /* not found */
}


static void markupval (FuncState *fs, int level) {
  BlockCnt *bl = fs->bl;
  while (bl && bl->nactvar > level) bl = bl->previous;
  if (bl) bl->upval = 1;
}


static int singlevaraux (FuncState *fs, TString *n, expdesc *var, int base) {
  if (fs == NULL) {  /* no more levels? */
    init_exp(var, VGLOBAL, NO_REG);  /* default is global variable */
    return VGLOBAL;
  }
  else {
    int v = searchvar(fs, n);  /* look up at current level */
    if (v >= 0) {
      init_exp(var, VLOCAL, v);
      if (!base)
        markupval(fs, v);  /* local will be used as an upval */
      return VLOCAL;
    }
    else {  /* not found at current level; try upper one */
      if (singlevaraux(fs->prev, n, var, 0) == VGLOBAL)
        return VGLOBAL;
      var->u.s.info = indexupvalue(fs, n, var);  /* else was LOCAL or UPVAL */
      var->k = VUPVAL;  /* upvalue in this level */
      return VUPVAL;
    }
  }
}


static TString *singlevar (LexState *ls, expdesc *var) {
  TString *varname = str_checkname(ls);
  FuncState *fs = ls->fs;
  if (singlevaraux(fs, varname, var, 1) == VGLOBAL)
    var->u.s.info = luaK_stringK(fs, varname);  /* info points to global name */
  return varname;  /* change 2.21.2 */
}

static void adjust_assign (LexState *ls, int nvars, int nexps, expdesc *e) {
  FuncState *fs = ls->fs;
  int extra = nvars - nexps;
  if (hasmultret(e->k)) {
    extra++;  /* includes call itself */
    if (extra < 0) extra = 0;
    luaK_setreturns(fs, e, extra);  /* last exp. provides the difference */
    if (extra > 1) luaK_reserveregs(fs, extra - 1);
  }
  else {
    if (e->k != VVOID) luaK_exp2nextreg(fs, e);  /* close last expression */
    if (extra > 0) {
      int reg = fs->freereg;
      luaK_reserveregs(fs, extra);
      luaK_nil(fs, reg, extra);
    }
  }
}


static void enterlevel (LexState *ls) {
  if (++ls->L->nCcalls > LUAI_MAXCCALLS)
   luaX_lexerror(ls, "chunk has too many syntax levels", 0);
}


#define leavelevel(ls)   ((ls)->L->nCcalls--)


static void enterblock (FuncState *fs, BlockCnt *bl, lu_byte isbreakable) {
  bl->breaklist = NO_JUMP;
  bl->continuelist = NO_JUMP;
  bl->redolist = NO_JUMP;
  bl->relaunchlist = NO_JUMP;
  bl->isbreakable = isbreakable;
  bl->nactvar = fs->nactvar;
  bl->upval = 0;
  bl->previous = fs->bl;
  fs->bl = bl;
  lua_assert(fs->freereg == fs->nactvar);
}


static void leaveblock (FuncState *fs) {
  BlockCnt *bl = fs->bl;
  fs->bl = bl->previous;
  removevars(fs->ls, bl->nactvar);
  if (bl->upval)
    luaK_codeABC(fs, OP_CLOSE, bl->nactvar, 0, 0);
  /* a block either controls scope or breaks (never both) */
  lua_assert(bl->isbreakable != 1 || !bl->upval);  /* 2.1 RC 2 */
  lua_assert(bl->nactvar == fs->nactvar);
  fs->freereg = fs->nactvar;  /* free registers */
  luaK_patchtohere(fs, bl->breaklist);
}


static int check_constant (LexState *ls, expdesc *v, TString *varname, int next) {  /* 2.20.0 */
  FuncState *fs = ls->fs;
  lua_State *K = ls->L;
  int k = v->k;
  if (k == VLOCAL) {
    if (CONSTANTS_ON == 0) return 0;  /* 2.21.2, better sure than sorry */
    varname = NULL;
    LocVar local = getlocvar(fs, v->u.s.info);
    if (local.isconst) varname = local.varname;
  } else if (k == VGLOBAL) {
    UltraSet *us = usvalue(constants(K));
    if (varname == NULL) {
      varname = str_getname(ls);
    }
    if (!agnUS_getstr(us, varname)) varname = NULL;  /* not in constants set */
    else if (!CONSTANTS_ON) agnUS_delstr(K, us, varname);  /* FIXME what's this ? */
  } else {  /* 2.21.5 hotfix FIXME, this will also allow for local constants to be changed in closures as
    they become upvalues */
    varname = NULL;
  }
  if (next == 0 && CONSTANTS_ON && varname) {  /* stolen from Lua 5.4 pre-release */
    const char *msg = luaO_pushfstring(K,
      "attempt to change or clear %s constant " LUA_QS ".", k == VLOCAL ? "local" : "global", getstr(varname));
    luaX_lexerror(fs->ls, msg, 0);
  }
  return varname != NULL;
}


static int cond (LexState *ls) {
  /* cond -> exp */
  expdesc v;
  expr(ls, &v);  /* read condition */
  if (v.k == VNIL || v.k == VFAIL) v.k = VFALSE;  /* `falses' are all equal here */
  luaK_goiftrue(ls->fs, &v);
  return v.f;
}


/* assigns the last iteration value of the numeric loop control variable to a local variable of the same
   name to the surrounding block */
static void leaveblocknumloop (FuncState *fs, TString *varname, expdesc *e) {  /* extended 1.4.0 */
  BlockCnt *bl = fs->bl;
  fs->bl = bl->previous;
  removevars(fs->ls, bl->nactvar);
  if (bl->upval)
    luaK_codeABC(fs, OP_CLOSE, bl->nactvar, 0, 0);
  /* a block either controls scope or breaks (never both) */
  lua_assert(!bl->isbreakable || !bl->upval);
  lua_assert(bl->nactvar == fs->nactvar);
  luaK_patchtohere(fs, bl->breaklist);
  fs->freereg = fs->nactvar + 1;  /* free registers, leave last value of internal control variable on stack */
  /* bind last iteration value to expdesc val, no need to put it into a register for it is already in one */
  if (fs->prev == NULL && bl->previous == NULL) {  /* 2.7.0/2.21.2, executed from the interactive level but not in a SCOPE block ? */
    expdesc val;
    init_exp(&val, VNONRELOC, fs->freereg - 1);  /* make it global */
    /* store last iteration value to loop control variable */
    luaK_storevar(fs, e, &val);
  } else {  /* make it local */
    new_localvar(fs->ls, varname, 0, 0, 0);
    adjustlocalvars(fs->ls, 1);
    luaK_reserveregs(fs, 1);
  }
}


static void pushclosure (LexState *ls, FuncState *func, expdesc *v, int tables) {
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int oldsize = f->sizep;
  int i;
  luaM_growvector(ls->L, f->p, fs->np, f->sizep, Proto *,
                  MAXARG_Bx, "constant table overflow");
  while (oldsize < f->sizep) f->p[oldsize++] = NULL;
  f->p[fs->np++] = func->f;
  luaC_objbarrier(ls->L, f, func->f);
  init_exp(v, VRELOCABLE, luaK_codeABx(fs, OP_CLOSURE, 0, fs->np - 1));
  for (i=0; i < func->f->nups; i++) {
    OpCode o = (func->upvalues[i].k == VLOCAL) ? OP_MOVE : OP_GETUPVAL;
    luaK_codeABC(fs, o, 0, func->upvalues[i].info, 0);
  }
  if (tables > 0) {
    /* 7.5.11 fix: Force the closure out of VRELOCABLE into a concrete register slot right now,
       otherwise you cannot use remember tables and stores with procedures declared local. */
    luaK_exp2nextreg(fs, v);
    if (tools_getbit(tables, 1))
      luaK_codeABC(fs, OP_CMD, fs->freereg - 1, 0, CMD_REMEMBER);
    if (tools_getbit(tables, 2))
      luaK_codeABC(fs, OP_CMD, fs->freereg - 1, 0, CMD_STORE);
  }
}


static void open_func (LexState *ls, FuncState *fs) {
  lua_State *L = ls->L;
  Proto *f = luaF_newproto(L);
  fs->f = f;
  fs->prev = ls->fs;  /* linked list of funcstates */
  fs->ls = ls;
  fs->L = L;
  ls->fs = fs;
  fs->pc = 0;
  fs->lasttarget = -1;
  fs->jpc = NO_JUMP;
  fs->freereg = 0;
  fs->nk = 0;
  fs->np = 0;
  fs->nlocvars = 0;
  fs->nactvar = 0;
  fs->bl = NULL;
  f->source = ls->source;
  f->maxstacksize = 2;  /* registers 0/1 are always valid */
  fs->h = luaH_new(L, 0, 0);
  /* anchor table of constants, and prototype (to avoid being collected) */
  sethvalue2s(L, L->top, fs->h);
  incr_top(L);
  setptvalue2s(L, L->top, f);
  incr_top(L);
  /* if you add new data to be pushed on the stack, grep "(grep) incr_top" lparser.c */
}


static void close_func (LexState *ls) {
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  removevars(ls, 0);
  luaK_ret(fs, 0, 0);  /* final return */
  luaM_reallocvector(L, f->code, f->sizecode, fs->pc, Instruction);
  f->sizecode = fs->pc;
  luaM_reallocvector(L, f->lineinfo, f->sizelineinfo, fs->pc, int);
  f->sizelineinfo = fs->pc;
  luaM_reallocvector(L, f->k, f->sizek, fs->nk, TValue);
  f->sizek = fs->nk;
  luaM_reallocvector(L, f->p, f->sizep, fs->np, Proto *);
  f->sizep = fs->np;
  luaM_reallocvector(L, f->locvars, f->sizelocvars, fs->nlocvars, LocVar);
  f->sizelocvars = fs->nlocvars;
  luaM_reallocvector(L, f->params, f->sizeparams, f->numparams, Param);  /* 2.1.2, for type checks */
  f->sizeparams = f->numparams;
  luaM_reallocvector(L, f->upvalues, f->sizeupvalues, f->nups, TString *);
  f->sizeupvalues = f->nups;
  lua_assert(luaG_checkcode(f));
  lua_assert(fs->bl == NULL);
  ls->fs = fs->prev;
  /* last token read was anchored in defunct function; must reanchor it */
  if (fs) anchor_token(ls);
  L->top -= 2;  /* (grep) incr_top; remove table and prototype from the stack, Lua 5.1.4 patch 11, Agena 1.6.0 */
}


Proto *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff, const char *name) {
  struct LexState lexstate;
  struct FuncState funcstate;
  lexstate.buff = buff;
  luaX_setinput(L, &lexstate, z, luaS_new(L, name));
  open_func(&lexstate, &funcstate);
  funcstate.f->is_vararg = VARARG_ISVARARG;  /* main func. is always vararg */
  luaX_next(&lexstate);  /* read first token */
  chunk(&lexstate);
  check(&lexstate, TK_EOS);
  close_func(&lexstate);
  lua_assert(funcstate.prev == NULL);
  lua_assert(funcstate.f->nups == 0);
  lua_assert(lexstate.fs == NULL);
  return funcstate.f;
}



/*============================================================*/
/* GRAMMAR RULES */
/*============================================================*/


static void field (LexState *ls, expdesc *v) {
  /* field -> ['.' | '@@' | '?'] NAME */
  FuncState *fs = ls->fs;
  expdesc key;
  luaK_exp2anyreg(fs, v);
  luaX_next(ls);  /* skip the dot or colon */
  checkname(ls, &key);
  luaK_indexed(fs, v, &key);
}


static int yindex (LexState *ls, expdesc *v, expdesc *key) {  /* 0.28.0, 1.2 */
  /* index -> '[' { expr | expr1 `to' expr2 } { `,' expr | expr1 `to' expr2 ... } ']' */
  FuncState *fs = ls->fs;
  luaX_next(ls);  /* skip the '[' */
  expr(ls, key);
  if (testnext(ls, TK_TO)) {  /* index range ? */
    v->u.s.aux = fs->freereg;
    luaK_exp2nextreg(fs, key);
    expr(ls, key);
    luaK_exp2nextreg(fs, key);
    checknext(ls, ']');
    return 0;
  } else {  /* conventional indices given */
    luaK_exp2val(fs, key);
    while (testnext(ls, ',')) {
      luaK_indexed(fs, v, key);
      luaK_exp2anyreg(fs, v);
      expr(ls, key);
      luaK_exp2val(fs, key);
    }
    checknext(ls, ']');
  }
  return 1;
}


static void listrange (LexState *ls, int base, expdesc *v, expdesc *key) {  /* process substrings and substructures */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  lua_assert(v->k == VNONRELOC);  /* changed 2.14.5 */
  v->u.s.aux = fs->freereg;
  luaK_exp2nextreg(fs, key);
  expr(ls, key);
  luaK_exp2nextreg(fs, key);
  luaK_codeABC(fs, OP_DOTTED, base, v->u.s.info, v->u.s.aux);
  /* reset position to first value in register (the return) */
  luaK_fixline(fs, line);
  v->u.s.info = base;
  fs->freereg = base + 1;  /* save the return on stack */
}


static void yindexnew (LexState *ls, int base, expdesc *v, expdesc *key) {  /* 0.28.0, 1.2, 2.4.0 */
  /* index -> '[' { expr | expr1 `to' expr2 } { `,' expr ... } ']' */
  int flag;
  FuncState *fs = ls->fs;
  luaX_next(ls);  /* skip the '[' */
  expr(ls, key);
  if ( (flag = testnext(ls, TK_TO)) )  /* index range ? */
    listrange(ls, base, v, key);
  else {  /* conventional indices given */
    luaK_exp2val(fs, key);
    luaK_indexed(fs, v, key);
  }
  while (testnext(ls, ',')) {
    if (!flag) luaK_exp2anyreg(fs, v);
    expr(ls, key);
    if ( (flag = testnext(ls, TK_TO)) )  /* index range ? */
      listrange(ls, base, v, key);
    else {  /* conventional indices given */
      luaK_exp2val(fs, key);
      luaK_indexed(fs, v, key);
    }
  }
  checknext(ls, ']');
}


/*
** {======================================================================
** Rules for Constructors
** =======================================================================
*/


struct ConsControl {
  expdesc v;  /* last list item read */
  expdesc *t;  /* table descriptor */
  int nh;  /* total number of `record' elements */
  int na;  /* total number of array elements */
  int tostore;  /* number of array elements pending to be stored */
};


static void record (LexState *ls, struct ConsControl *cc, expdesc key, int reg) {  /* assign key ~ value pair */
  int rkkey;
  expdesc val;
  FuncState *fs = ls->fs;
  luaY_checklimit(fs, cc->nh, MAX_INT, "items in a constructor");
  cc->nh++;
  rkkey = luaK_exp2RK(fs, &key);
  expr(ls, &val);
  luaK_codeABC(fs, OP_SETTABLE, cc->t->u.s.info, rkkey, luaK_exp2RK(fs, &val));
  fs->freereg = reg;  /* free registers */
}

static void item (LexState *ls, struct ConsControl *cc) {
  /* (recfield -> ((NAME | STRING | NUMBER | exp1) `~' exp1 | NAME `=' exp1) | listfield -> exp */
  int reg = ls->fs->freereg;
  expdesc key, v;
  if (ls->t.token == TK_NAME) {  /* NAME `=' value ? added 2.8.0 */
    int token = luaX_lookahead(ls);
    if (token == TK_EQ || token == TK_SEP) {  /* 2.30.0 extension for `~' */
      checkname(ls, &key);
      checknext(ls, token);
      record(ls, cc, key, reg);
      return;
    }
  }
  expr(ls, &v);
  if (testnext(ls, TK_SEP)) {  /* recordfield ? */
    luaK_exp2val(ls->fs, &v);
    record(ls, cc, v, reg);
  } else {  /* listfield */
    cc->v = v;
    luaY_checklimit(ls->fs, cc->na, MAX_INT, "items in a constructor");  /* Lua 5.1.1 patch 1 */
    cc->na++;
    cc->tostore++;
  }
}


static void datarecord (LexState *ls, struct ConsControl *cc, expdesc key, int reg) {  /* assign key ~ value pair */
  int rkkey;
  expdesc val;
  FuncState *fs = ls->fs;
  luaY_checklimit(fs, cc->nh, MAX_INT, "items in a constructor");
  cc->nh++;
  rkkey = luaK_exp2RK(fs, &key);
  datasimpleexp(ls, &val, 1);
  luaK_codeABC(fs, OP_SETTABLE, cc->t->u.s.info, rkkey, luaK_exp2RK(fs, &val));
  fs->freereg = reg;  /* free registers */
}


static void dataitem (LexState *ls, struct ConsControl *cc) {
  /* (recfield -> ((NAME | STRING | NUMBER | exp1) `~' exp1 | NAME `=' exp1) | listfield -> exp */
  int reg = ls->fs->freereg;
  expdesc key, v;
  if (ls->t.token == TK_NAME) {  /* NAME `=' value ? added 2.8.0 */
    if (luaX_lookahead(ls) == TK_EQ) {
      checkname(ls, &key);
      checknext(ls, TK_EQ);
      datarecord(ls, cc, key, reg);
      return;
    }
  }
  datasimpleexp(ls, &v, 1);
  if (testnext(ls, TK_SEP)) {  /* recordfield ? */
    luaK_exp2val(ls->fs, &v);
    datarecord(ls, cc, v, reg);
  } else {  /* listfield */
    cc->v = v;
    luaY_checklimit(ls->fs, cc->na, MAX_INT, "items in a constructor");  /* Lua 5.1.1 patch 1 */
    cc->na++;
    cc->tostore++;
  }
}


static void seqdataitem (LexState *ls, struct ConsControl *cc) {  /* 2.10.0 */
  /* (recfield -> ((NAME | STRING | NUMBER | exp1) `~' exp1 | NAME `=' exp1) | listfield -> exp */
  expdesc v;
  datasimpleexp(ls, &v, 1);
  cc->v = v;
  luaY_checklimit(ls->fs, cc->na, MAX_INT, "items in a constructor");  /* Lua 5.1.1 patch 1 */
  cc->na++;
  cc->tostore++;
}


/* handles sequences and sets */
static void listfield (LexState *ls, struct ConsControl *cc) {
  expr(ls, &cc->v);
  luaY_checklimit(ls->fs, cc->na, MAX_INT, "items in a constructor");  /* Lua 5.1.2 patch */
  cc->na++;
  cc->tostore++;
}


static void vectorfield (LexState *ls, struct ConsControl *cc) {  /* 4.0.0 RC 1 */
  int dummy;
  subexpr(ls, &cc->v, 0, 1, &dummy, 1, 1);
  luaY_checklimit(ls->fs, cc->na, MAX_INT, "items in a constructor");  /* Lua 5.1.2 patch */
  cc->na++;
  cc->tostore++;
}


static void closelistfield (FuncState *fs, struct ConsControl *cc, int isvector) {
  if (cc->v.k == VVOID) return;  /* there is no list item */
  luaK_exp2nextreg(fs, &cc->v);
  cc->v.k = VVOID;
  if (cc->tostore == LFIELDS_PER_FLUSH) {
    luaK_setlist(fs, cc->t->u.s.info, cc->na, cc->tostore, isvector);  /* flush */
    cc->tostore = 0;  /* no more items pending */
  }
}


static void lastlistfield (FuncState *fs, struct ConsControl *cc, int isvector) {
  if (cc->tostore == 0) return;
  if (hasmultret(cc->v.k)) {
    luaK_setmultret(fs, &cc->v);
    luaK_setlist(fs, cc->t->u.s.info, cc->na, LUA_MULTRET, isvector);
    cc->na--;  /* do not count last expression (unknown number of elements) */
  }
  else {
    if (cc->v.k != VVOID)
      luaK_exp2nextreg(fs, &cc->v);
    luaK_setlist(fs, cc->t->u.s.info, cc->na, cc->tostore, isvector);
  }
}


static void constructor (LexState *ls, expdesc *t) {
  /* table constructor -> ?? */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_codeABC(fs, OP_NEWTABLE, 0, 0, 0);
  struct ConsControl cc;
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VRELOCABLE, pc);
  init_exp(&cc.v, VVOID, 0);  /* no value (yet) */
  luaK_exp2nextreg(fs, t);  /* fix it at stack top (for gc) */
  checknext(ls, '[');
  if (testnext(ls, TK_FROM)) {  /* 6.1.0 */
    int flag = 1;
    expdesc start, step;
    expr(ls, &start);
    /* step size (forward counting) */
    init_exp(&step, VKNUM, 0);
    step.u.nval = (lua_Number)1;
    codearith(fs, OP_SUB, &start, &step);
    checknext2(ls, ',', TK_INSERT);
    /* assign vars */
    do {
      if (ls->t.token == ']') {
        if (flag) luaX_syntaxerror(ls, "need at least one item");
        break;
      }
      flag = 0;
      closelistfield(fs, &cc, 0);  /* also flushes list after LFIELDS_PER_FLUSH fields */
      codearith(fs, OP_ADD, &start, &step);
      luaK_exp2nextreg(fs, &start);  /* put start on stack */
      luaK_exp2val(ls->fs, &start);
      record(ls, &cc, start, ls->fs->freereg);
    } while (testnext(ls, ','));
  } else {
    do {
      lua_assert(cc.v.k == VVOID || cc.tostore > 0);
      if (ls->t.token == ']') break;
      closelistfield(fs, &cc, 0);  /* also flushes list after LFIELDS_PER_FLUSH fields */
      item(ls, &cc);
    } while (testnext(ls, ','));  /* changed, 0.5.2 */
  }
  check_match(ls, ']', '[', line);
  lastlistfield(fs, &cc, 0);
  SETARG_B(fs->f->code[pc], luaO_int2fb(cc.na));  /* set initial array size */
  SETARG_C(fs->f->code[pc], luaO_int2fb(cc.nh));  /* set initial table size */
}


static void setconstructor (LexState *ls, expdesc *t) {  /* 0.10.0 */
  /* set constructor -> ?? */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_codeABC(fs, OP_FN, 0, 0, OPR_NEWUSET);
  struct ConsControl cc;
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VRELOCABLE, pc);
  init_exp(&cc.v, VVOID, 0);  /* no value (yet) */
  luaK_exp2nextreg(fs, t);  /* fix it at stack top (for gc) */
  checknext(ls, '{');
  do {
    lua_assert(cc.v.k == VVOID || cc.tostore > 0);
    if (ls->t.token == '}') break;
    closelistfield(fs, &cc, 0);
    listfield(ls, &cc);
  } while (testnext(ls, ','));
  check_match(ls, '}', '{', line);
  lastlistfield(fs, &cc, 0);
  SETARG_B(fs->f->code[pc], luaO_int2fb(cc.na));  /* set initial set size */
}


static void seqconstructor (LexState *ls, expdesc *t) {  /* 0.11.0 */
  /* sequence constructor -> ?? */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_codeABC(fs, OP_FN, 0, 0, OPR_NEWSEQ);
  struct ConsControl cc;
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VRELOCABLE, pc);
  init_exp(&cc.v, VVOID, 0);  /* no value (yet) */
  luaK_exp2nextreg(fs, t);  /* fix it at stack top (for gc) */
  luaX_next(ls);
  checknext(ls, '(');
  do {
    lua_assert(cc.v.k == VVOID || cc.tostore > 0);
    if (ls->t.token == ')') break;
    closelistfield(fs, &cc, 0);
    listfield(ls, &cc);
  } while (testnext(ls, ','));
  check_match(ls, ')', TK_SEQ, line);
  lastlistfield(fs, &cc, 0);
  SETARG_B(fs->f->code[pc], luaO_int2fb(cc.na));  /* set sequence size */
}


static void vectorconstructor (LexState *ls, expdesc *t) {  /* 4.0.0 RC 1 */
  /* linalg vector constructor -> ?? */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_codeABC(fs, OP_NEWTABLE, 0, 0, 0);
  struct ConsControl cc;
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VRELOCABLE, pc);
  init_exp(&cc.v, VVOID, 0);  /* no value (yet) */
  luaK_exp2nextreg(fs, t);  /* fix it at stack top (for gc) */
  checknext(ls, TK_LT);
  do {
    lua_assert(cc.v.k == VVOID || cc.tostore > 0);
    if (ls->t.token == TK_GT) break;
    closelistfield(fs, &cc, 1);
    vectorfield(ls, &cc);
  } while (testnext(ls, ','));
  check_match(ls, TK_GT, TK_LT, line);
  if (cc.na == 0)
    luaX_syntaxerror(ls, "need at least one vector component, ");
  lastlistfield(fs, &cc, 1);
  SETARG_B(fs->f->code[pc], luaO_int2fb(cc.na));  /* set sequence size */
}


static void regconstructor (LexState *ls, expdesc *t) {  /* 2.3.0 RC 3 */
  /* register constructor -> ?? */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_codeABC(fs, OP_FN, 0, 0, OPR_NEWREG);
  struct ConsControl cc;
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VRELOCABLE, pc);
  init_exp(&cc.v, VVOID, 0);  /* no value (yet) */
  luaK_exp2nextreg(fs, t);  /* fix it at stack top (for gc) */
  luaX_next(ls);
  checknext(ls, '(');
  do {
    lua_assert(cc.v.k == VVOID || cc.tostore > 0);
    if (ls->t.token == ')') break;
    closelistfield(fs, &cc, 0);
    listfield(ls, &cc);
  } while (testnext(ls, ','));
  check_match(ls, ')', TK_REG, line);
  lastlistfield(fs, &cc, 0);
  if (cc.na == 0) cc.na = ls->L->regsize;
  SETARG_B(fs->f->code[pc], cc.na);  /* set register size ; 2.3.1 fix: do not round up- or downwards */
}


static void dataconstructor (LexState *ls, expdesc *t) {  /* 2.9.3 */
  /* data constructor -> ?? */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_codeABC(fs, OP_NEWTABLE, 0, 0, 0);
  struct ConsControl cc;
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VRELOCABLE, pc);
  init_exp(&cc.v, VVOID, 0);  /* no value (yet) */
  luaK_exp2nextreg(fs, t);  /* fix it at stack top (for gc) */
  if (testnext(ls, TK_FROM)) {  /* 6.1.0 */
    int flag = 1;
    expdesc start, step;
    expr(ls, &start);
    /* step size (forward counting) */
    init_exp(&step, VKNUM, 0);
    step.u.nval = (lua_Number)1;
    codearith(fs, OP_SUB, &start, &step);
    checknext2(ls, ',', TK_INSERT);
    /* assign values */
    do {
      if (ls->t.token == TK_ATAD) {
        if (flag) luaX_syntaxerror(ls, "need at least one item");
        break;
      }
      flag = 0;
      if (ls->t.token == ',') { luaX_next(ls); continue; } /* ignore commata */
      closelistfield(fs, &cc, 0);  /* also flushes list after LFIELDS_PER_FLUSH fields */
      if (luaX_lookahead(ls) == TK_SEP) {
        dataitem(ls, &cc);
      } else {
        codearith(fs, OP_ADD, &start, &step);
        luaK_exp2nextreg(fs, &start);  /* put start on stack */
        luaK_exp2val(ls->fs, &start);
        datarecord(ls, &cc, start, ls->fs->freereg);
      }
    } while (1);
  } else {
    do {
      lua_assert(cc.v.k == VVOID || cc.tostore > 0);
      if (ls->t.token == TK_ATAD) break;
      if (ls->t.token == ',') { luaX_next(ls); continue; } /* ignore commata */
      closelistfield(fs, &cc, 0);  /* also flushes list after LFIELDS_PER_FLUSH fields */
      dataitem(ls, &cc);
    } while (1);
  }
  check_match(ls, TK_ATAD, TK_DATA, line);
  lastlistfield(fs, &cc, 0);
  SETARG_B(fs->f->code[pc], luaO_int2fb(cc.na));  /* set initial array size */
  SETARG_C(fs->f->code[pc], luaO_int2fb(cc.nh));  /* set initial table size */
}


static void seqdataconstructor (LexState *ls, expdesc *t) {  /* 2.10.0 */
  /* sequence constructor -> ?? */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_codeABC(fs, OP_FN, 0, 0, OPR_NEWSEQ);
  struct ConsControl cc;
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VRELOCABLE, pc);
  init_exp(&cc.v, VVOID, 0);  /* no value (yet) */
  luaK_exp2nextreg(fs, t);  /* fix it at stack top (for gc) */
  do {
    lua_assert(cc.v.k == VVOID || cc.tostore > 0);
    if (ls->t.token == TK_SEQDATAEND) break;
    if (ls->t.token == ',') { luaX_next(ls); continue; } /* ignore commata */
    closelistfield(fs, &cc, 0);
    seqdataitem(ls, &cc);
  } while (1);
  check_match(ls, TK_SEQDATAEND, TK_SEQDATA, line);
  lastlistfield(fs, &cc, 0);
  SETARG_B(fs->f->code[pc], luaO_int2fb(cc.na));  /* set sequence size */
}


/* }====================================================================== */

/* forward declarations for structure initialisation */
static void tablestat (LexState *ls, int nvars, int type);
static void setstat (LexState *ls, int nvars, int type);
static void seqstat (LexState *ls, int nvars, int type);
static void regstat (LexState *ls, int nvars, int type);
static void dictstat (LexState *ls, int nvars, int type);

/* initiate non-structure vars that have already been passed before setting empty structure */
#define PrepareStructAssign(ls,nvars,isatom) \
  if (nvars != 0) {  \
    initlocals(ls, nvars);\
    nvars = 0;\
  }\
  isatom = 0;

#define nomix   luaX_syntaxerror(ls, "enums or structure declarations cannot be mixed with assignments,");

static void initlocals (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  int reg = fs->freereg;
  luaK_reserveregs(fs, nvars);
  luaK_nil(fs, reg, nvars);
  adjustlocalvars(ls, nvars);
}

/* }====================================================================== */

static void allocparamspace (LexState *ls, int param) {  /* 2.25.0 */
  Proto *f = ls->fs->f;
  int oldpsize = f->sizeparams;
  luaM_growvector(ls->L, f->params, param, f->sizeparams,
                  Param, SHRT_MAX, "too many parameters");
  f->params[param].typearray = 0;  /* 0: no optional type specified, 0.20.0 */
  f->params[param].vartypets = NULL;
  while (oldpsize < f->sizeparams) {
    f->params[oldpsize].vartypets = NULL;
    oldpsize++;
  }
}


static void parameter (LexState *ls, int param) {  /* 2.1.2 */
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  allocparamspace(ls, param);
  if (testnext(ls, TK_DCOLON)) {  /* new type checking method, 0.20.0 */
    if (testnext(ls, '{')) {  /* check for more than one type */
      int c = 0;
      do {
        if (ls->t.token >= TK_TBOOLEAN && ls->t.token <= TK_TANYTHING) {
          f->params[param].typearray += tools_intpow(LUAI_NTYPELISTMOD, c++)*(ls->t.token - TK_TBOOLEAN + 1);
          luaX_next(ls);  /* skip data type name */
        } else
          luaX_syntaxerror(ls, "invalid basic data type");
      } while (testnext(ls, ',') && c < LUAI_NTYPELIST);  /* 2.3.0 RC 3 */
      checknext(ls, '}');
    } else if (ls->t.token >= TK_TBOOLEAN && ls->t.token <= TK_TANYTHING) {  /* check for only one basic type ... */
      f->params[param].typearray = ls->t.token - TK_TBOOLEAN + 1;
      luaX_next(ls);
    } else {  /* 0.28.0, ... or user-defined type */
      TString *vartypets = str_checkname(ls);
      f->params[param].vartypets = vartypets;
      luaC_objbarrier(ls->L, f, vartypets);
    }
    f->is_typegiven = 1;  /* specifies that there is at least one type check for at least one parameter */
  }
}


static void parlist (LexState *ls, int needself) {
  /* parlist -> [ param { `,' param } ] */
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int nparams = 0;
  f->is_vararg = 0;
  f->is_oopfn = 0;
  if (needself) {  /* 2.24.0 fix to avoid conditional jumps or moves on unassigned values ... */
    allocparamspace(ls, 0);  /* ... with OOP functions, initialise internal type check table for `self` local */
    f->is_oopfn = IS_OOPFN;  /* for ldo.c type checks */
  }
  if (ls->t.token != ')') {  /* is `parlist' not empty? */
    do {
      switch (ls->t.token) {
        case TK_NAME: {  /* param -> NAME */
          new_localvar(ls, str_checkname(ls), nparams++, 1, 0);
          parameter(ls, nparams - (needself == 0));  /* 2.1.2, counting from zero */
          break;
        }
        case TK_QMARK: {  /* param -> `?' */
          luaX_next(ls);
          /* use `varargs' as default name; because of collision between `?' and `varargs',
             varargs functionality has been removed */
          new_localvarliteral(ls, "varargs", nparams++);
          f->is_vararg = VARARG_HASARG | VARARG_NEEDSARG;
          f->is_vararg |= VARARG_ISVARARG;
          break;
        }
        default: luaX_syntaxerror(ls, "<name> or " LUA_QL("?") " expected");
      }
    } while (!f->is_vararg && testnext(ls, ','));
  }
  adjustlocalvars(ls, nparams);
  f->numparams = cast_byte(fs->nactvar - (f->is_vararg & VARARG_HASARG));
  luaK_reserveregs(fs, fs->nactvar);  /* reserve registers for parameters */
}


static int fnparlist (LexState *ls, int bracketed, int needself) {  /* optimised 2.0.0 RC 4 */
  /* parlist -> [ param { `,' param } ] */
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int nparams = 0;
  f->is_vararg = 0;
  f->is_oopfn = 0;
  if (needself) {  /* 2.25.0 */
    allocparamspace(ls, 0);
    f->is_oopfn = IS_OOPFN;  /* for ldo.c type checks */
  }
  if ( (bracketed && ls->t.token != ')') || (!(bracketed) && ls->t.token != TK_ARROW) ) {  /* is `parlist' not empty?  Agena 1.2.1 fix */
    do {
      switch (ls->t.token) {
        case TK_NAME:  /* param -> NAME */
          new_localvar(ls, str_checkname(ls), nparams++, 1, 0);  /* 2.25.0 change */
          parameter(ls, nparams - (needself == 0));  /* 2.25.0 change */
          break;
        case TK_QMARK:  /* param -> `?', 2.31.3 */
          luaX_next(ls);
          /* use `varargs' as default name; because of collision between `?' and `varargs',
             varargs functionality has been removed */
          new_localvarliteral(ls, "varargs", nparams++);
          f->is_vararg = VARARG_HASARG | VARARG_NEEDSARG;
          f->is_vararg |= VARARG_ISVARARG;
          break;
        default:
          luaX_syntaxerror(ls, "<name> expected");
      }
    } while (!f->is_vararg && testnext(ls, ','));
  }
  adjustlocalvars(ls, nparams);
  f->numparams = cast_byte(fs->nactvar - (f->is_vararg & VARARG_HASARG));  /* 2.31.3 */
  luaK_reserveregs(fs, fs->nactvar);  /* reserve registers for parameters */
  return nparams;  /* extended 2.12.0 RC 4 */
}


static void fnbody (LexState *ls, expdesc *e, int needself, int line, int token) {
  /* body ->  `<:' parlist `->' chunk `:>' */
  FuncState new_fs;
  int bracketed, nparams, more = 0;
  open_func(ls, &new_fs);
  new_fs.f->linedefined = line;
  bracketed = testnext(ls, '(');
  if (needself) {
    new_localvarliteral(ls, "self", 0);  /* one new localvar at position 0, no check */
    adjustlocalvars(ls, 1);
  }
  nparams = fnparlist(ls, bracketed, needself);
  if (bracketed)
    checknext(ls, ')');
  else if (nparams == 0)
    luaX_syntaxerror(ls, LUA_QL("()") " expected with no parameters");
  while (testnext(ls, TK_WITH) || more) {  /* 7.0.0 allow for multipke WITH clauses */
    localstat(ls, 2);  /* 2.12.0 RC 3, << x -> with n := 1 -> y >> ? -> create local variables */
    more = testnext(ls, ';');
  }
  checknext(ls, TK_ARROW);
  fnchunk(ls, token != TK_LT2);
  new_fs.f->lastlinedefined = ls->linenumber;
  if (token == TK_LT2) check_match(ls, TK_GT2, TK_LT2, line);  /* 6.1.0 change */
  close_func(ls);
  pushclosure(ls, &new_fs, e, 0);
}


static void defbody (LexState *ls, expdesc *e, int needself, int line) {  /* 2.29.1 */
  /* body ->  `DEF/DEFINE funcname(params) ...` */
  FuncState new_fs;
  int bracketed, nparams, more = 0;
  open_func(ls, &new_fs);
  new_fs.f->linedefined = line;
  bracketed = testnext(ls, '(');
  if (needself) {
    new_localvarliteral(ls, "self", 0); /* one new localvar at position 0, no check */
    adjustlocalvars(ls, 1);
  }
  nparams = fnparlist(ls, bracketed, needself);
  if (bracketed)
    checknext(ls, ')');
  else if (nparams == 0)
    luaX_syntaxerror(ls, LUA_QL("()") " expected with no parameters");
  while (testnext(ls, TK_WITH) || more) {  /* 7.0.0 allow for multipke WITH clauses */
    localstat(ls, 2);  /* 2.12.0 RC 3, << x -> with n := 1 -> y >> ? -> create local variables */
    more = testnext(ls, ';');
  }
  if (!testnext(ls, TK_ARROW) && !testnext(ls, TK_EQ) && !testnext(ls, TK_ASSIGN)) {
    testnext(ls, TK_IS);
  }
  fnchunk(ls, 0);
  new_fs.f->lastlinedefined = ls->linenumber;
  close_func(ls);
  pushclosure(ls, &new_fs, e, 0);
}


static void returntype (LexState *ls) {
  if (testnext(ls, TK_DCOLON)) {  /* new return type checking method, 1.3.0 */
    Proto *f = ls->fs->f;
    if (testnext(ls, '{')) {  /* 2.7.0 extension */
      int c = 0;
      f->rettype = 0;
      do {
        if (ls->t.token >= TK_TBOOLEAN && ls->t.token <= TK_TANYTHING) {
          f->rettype += tools_intpow(LUAI_NTYPELISTMOD, c++)*(ls->t.token - TK_TBOOLEAN + 1);
          luaX_next(ls);  /* skip data type name */
        } else
          luaX_syntaxerror(ls, "invalid basic data type");
      } while (testnext(ls, ',') && c < LUAI_NTYPELIST);  /* 2.3.0 RC 3 */
      checknext(ls, '}');
      f->rettypets = NULL;
    } else if (ls->t.token >= TK_TBOOLEAN && ls->t.token <= TK_TANYTHING) {
      f->rettype = ls->t.token - TK_TBOOLEAN + 1;
      f->rettypets = NULL;
      luaX_next(ls);
    }
    else {  /* user-defined type */
      TString *returntypets = str_checkname(ls);
      f->rettypets = returntypets;
      luaC_objbarrier(ls->L, f, returntypets);
      f->rettype = -1;
    }
    f->is_rettypegiven = 1;
  }
}


static void body (LexState *ls, expdesc *e, int needself, int line) {
  /* body ->  `(' parlist `)' chunk END */
  FuncState new_fs;
  int tables = 0;  /* 2.20.0 change */
  open_func(ls, &new_fs);
  new_fs.f->linedefined = line;
  checknext(ls, '(');
  if (needself) {
    new_localvarliteral(ls, "self", 0); /* one new localvar at position 0, no check */
    adjustlocalvars(ls, 1);
  }
  parlist(ls, needself);
  checknext(ls, ')');
  returntype(ls);  /* check for a given return type */
  if (testnext(ls, TK_PRE)) {  /* 2.10.5, a little bit faster than checking multiple conditions with multiargop */
    expdesc v;
    FuncState *fs = ls->fs;  /* do NOT move this assignment to the top, otherwise there were `control structure too long` errors. */
    int base = fs->freereg;
    expr(ls, &v);
    luaK_exp2nextreg(fs, &v);
    luaK_codeABC(fs, OP_FN, base, 1, OPR_PRE);
    fs->freereg -= 1;
  }
  testnext(ls, TK_IS);  /* Agena 1.5.0; changed to optional 3.0.0 */
  if (testnext(ls, TK_FEATURE)) {  /* 2.7.0 */
    do {
      switch (ls->t.token) {
        case TK_REMINISCE:
          tools_setbit(&tables, 1, 1);  /* 2.20.0 change */
          luaX_next(ls);  /* skip REMINISCE */
          break;
        case TK_STORE:  /* 2.16.7 */
          tools_setbit(&tables, 2, 1);  /* 2.20.0 change */
          luaX_next(ls);  /* skip STORAGE */
          break;
        default:
          luaX_syntaxerror(ls, "feature unknown");
      }
    } while (testnext(ls, ','));
    testnext(ls, ';');  /* skip optional semicolon */
  }
  chunk(ls);
  new_fs.f->lastlinedefined = ls->linenumber;
  check_match2(ls, TK_END, TK_PROC, TK_TPROCEDURE, line);  /* changed */
  close_func(ls);
  pushclosure(ls, &new_fs, e, tables);
}


static void check_conflict (LexState *ls, struct LHS_assign *lh, expdesc *v);
static int assignment (LexState *ls, struct LHS_assign *lh, int nvars, lu_byte *from_var);

static int explist1 (LexState *ls, expdesc *v) {
  /* explist1 -> expr { `,' expr } */
  int n = 1;  /* at least one expression */
  expr(ls, v);
  while (testnext(ls, ',')) {
    luaK_exp2nextreg(ls->fs, v);
    expr(ls, v);
    n++;
  }
  return n;
}


static int explist0 (LexState *ls, expdesc *v) {  /* 2.37.2, let operator accept zero or more arguments */
  /* explist1 -> expr { `,' expr } */
  int n = 1;  /* at least one expression */
  if (isnext(ls, ')')) {
    init_exp(v, VNIL, 0);  /* slot for procedure called in VM */
    luaK_exp2nextreg(ls->fs, v);
    n = 0;
  } else {
    expr(ls, v);
    while (testnext(ls, ',')) {
      luaK_exp2nextreg(ls->fs, v);
      expr(ls, v);
      n++;
    }
  }
  return n;
}


static int funcargexplist (LexState *ls, expdesc *v) {
  /* explist1 -> expr { `,' expr } */
  int n = 1;  /* at least one expression */
  funcargexpr(ls, v, 0);
  while (testnext(ls, ',')) {
    luaK_exp2nextreg(ls->fs, v);
    funcargexpr(ls, v, 0);
    n++;
  }
  return n;
}


static int opvmargexplist (LexState *ls, expdesc *v) {  /* multiple arguments for an operator, to be passed in brackets, 2.9.7 */
  /* explist1 -> expr { `,' expr } */
  int n = 0;  /* no or more expressions */
  if (ls->t.token == ')')
    return n;
  else
    n++;
  funcargexpr(ls, v, 0);
  while (testnext(ls, ',')) {
    luaK_exp2nextreg(ls->fs, v);
    funcargexpr(ls, v, 0);
    n++;
  }
  return n;
}


static void funcargs (LexState *ls, expdesc *f) {
  FuncState *fs = ls->fs;
  expdesc args;
  int base, nparams, line;
  line = ls->linenumber;
  switch (ls->t.token) {
    case '(': {  /* funcargs -> `(' [ explist1 ] `)' */
      if (line != ls->lastline)
        luaX_syntaxerror(ls, "ambiguous syntax (procedure call x new statement)");
      luaX_next(ls);
      if (ls->t.token == ')') { /* arg list is empty? */
        args.k = VVOID;
      } else {
        funcargexplist(ls, &args);
        luaK_setmultret(fs, &args);
      }
      check_match(ls, ')', '(', line);
      break;
    }
    case TK_STRING: {  /* funcargs -> STRING */
      codestring(ls, &args, str_getname(ls));
      luaX_next(ls);   /* must use `seminfo' before `next' */
      break;
    }
    default: {
      luaX_syntaxerror(ls, "procedure arguments expected");
      return;
    }
  }
  lua_assert(f->k == VNONRELOC);
  base = f->u.s.info;       /* base register for call */
  if (hasmultret(args.k))
    nparams = LUA_MULTRET;  /* open call */
  else {
    if (args.k != VVOID)
      luaK_exp2nextreg(fs, &args);  /* close last argument */
    nparams = fs->freereg - (base + 1);
  }
  init_exp(f, VCALL, luaK_codeABC(fs, OP_CALL, base, nparams + 1, 2));
  luaK_fixline(fs, line);
  fs->freereg = base + 1;  /* call remove function and arguments and leaves
                              (unless changed) one result */
}


static void binopargs (LexState *ls, int base, expdesc *f, expdesc *v) {  /* 2.6.0, process user-defined binary operator, base = position of left operand */
  FuncState *fs = ls->fs;
  expdesc v1;
  int nparams, base1, callbase, dummy, line;
  line = ls->linenumber;
  base1 = fs->freereg;
  luaK_reserveregs(ls->fs, 1);  /* reserve a register that will contain a copy of left operand */
  subexpr(ls, &v1, HIGHEST_PRIORITY, 0, &dummy, 0, 0);  /* prio = 20, read right operand, avoid parsing built-in binary ops */
  lua_assert(v1.k == VNONRELOC);
  callbase = f->u.s.info;       /* base register for call */
  if (hasmultret(v1.k))
    nparams = LUA_MULTRET;      /* open call */
  else {
    luaK_exp2nextreg(fs, &v1);  /* close last argument */
    nparams = 2;
  }
  luaK_codeABC(fs, OP_MOVE, base1, v->u.s.info, 0);  /* copy left operand right after position of function */
  init_exp(&v1, VCALL, luaK_codeABC(fs, OP_CALL, callbase, nparams + 1, 2));
  v->u.s.info = callbase;
  v->k = VNONRELOC;
  luaK_fixline(fs, line);
  fs->freereg = base + 1;
}


/* for LOG, MULADD, ADDUP, QSUMUP operators, 1.1.0, December 09, 2010 */
static void multiargop (LexState *ls, expdesc *e, int line, int tkop, int vmop) {
  int nargs, base;
  FuncState *fs = ls->fs;
  nargs = 0;
  base = fs->freereg;
  init_exp(e, VRELOCABLE, base);
  switch (ls->t.token) {
    case '(': {  /* opargs -> `(' [ explist1 ] `)' */
      if (line != ls->lastline)
        luaX_syntaxerror(ls, "ambiguous syntax (function call x new statement)");
      luaX_next(ls);
      nargs = explist0(ls, e);
      check_match(ls, ')', tkop, line);
      break;
    }
    default: {
      luaX_syntaxerror(ls, "arguments in parentheses expected");
      return;
    }
  }
  luaK_exp2nextreg(fs, e);  /* close last argument */
  luaK_codeABC(fs, OP_FN, base, nargs, vmop);
  luaK_fixline(fs, line);
  /* reset position to first value in register (the return) */
  e->u.s.info = base;
  fs->freereg -= nargs - 1;
}


/*
** {======================================================================
** Expression parsing
** =======================================================================
*/

/* `..` conditional field operator, originally written by Sven Olsen and published in Lua Wiki/Power Patches */

#define conditional_field_pre(fs,v) { \
  luaK_codeABC(fs, OP_TEST, v->u.s.info, NO_REG, 0); \
  old_free = fs->freereg; \
  vreg = v->u.s.info; \
  j = luaK_codeAsBx(fs, OP_JMP, 0, NO_JUMP); \
}

#define conditional_field_post(fs,v,vreg,j,old_free) { \
  luaK_exp2nextreg(fs, v); \
  fs->freereg = old_free; \
  if (v->u.s.info != vreg) { \
    luaK_codeABC(fs, OP_MOVE, vreg, v->u.s.info, 0); \
    v->u.s.info = vreg; \
  } \
  SETARG_sBx(fs->f->code[j], fs->pc - j - 1); \
}
/* I think the `if (v->u.s.info != vreg)` next check is unnecessary, as any complex key expressions should
   be courteous enough to leave the top of the stack where they found it. */

static void conditional_field (LexState *ls, expdesc *v, int mode) {
  int old_free, vreg, j;
  FuncState *fs = ls->fs;
  expdesc key;
  luaK_exp2nextreg(fs, v);
  conditional_field_pre(fs, v);
  if (mode) {  /* index opened with `{`, 2.21.0 */
    expr(ls, &key);
    if (testnext(ls, TK_TO))  /* index range ? */
      listrange(ls, old_free, v, &key);
    else {  /* conventional indices given */
      luaK_exp2val(fs, &key);
      luaK_indexed(fs, v, &key);
    }
  } else {
    switch (ls->t.token) {
      case '[': {  /* DEPRECATE THIS */
        yindex(ls, v, &key);
        luaK_indexed(fs, v, &key);  /* patched 2.21.0 */
        /* luaK_exp2nextreg(fs, v); */
        break;
      }
      default: {
        checkname(ls, &key);
        luaK_indexed(fs, v, &key);
      }
    }
  }
  conditional_field_post(fs, v, vreg, j, old_free);
  if (mode && ls->t.token == ',') {  /* further indices && index opened with `{`, new 2.21.0 */
    while (testnext(ls, ',')) {
      conditional_field_pre(fs, v);
      luaK_exp2anyreg(fs, v);
      expr(ls, &key);
      if (testnext(ls, TK_TO))  /* index range ? */
        listrange(ls, old_free, v, &key);
      else {  /* conventional indices given */
        luaK_exp2val(fs, &key);
        luaK_indexed(fs, v, &key);
      }
      conditional_field_post(fs, v, vreg, j, old_free);
    }
  }
}


static int prefixexp (LexState *ls, expdesc *v, int checkconst) {
  /* prefixexp -> NAME | '(' expr ')' | '?' | '|';
     return = 0 ? no absolute sign, 1 = absolute sign found */
  FuncState *fs = ls->fs;
  switch (ls->t.token) {
    case '(': {
      int line = ls->linenumber;
      luaX_next(ls);
      expr(ls, v);
      check_match(ls, ')', '(', line);
      luaK_dischargevars(fs, v);
      return 0;
    }
    case TK_NAME: {
      TString *varname = singlevar(ls, v);
      /* [varname++ | varname--], 2.12.2 fix, 2.28.0 change */
      int nexttoken = ls->t.token;
      if (checkconst || nexttoken == TK_MM || nexttoken == TK_PP) {
        int rc = check_constant(ls, v, varname, 0);
        if (rc && CONSTANTS_ON) {
          lua_State *L = ls->L;
          if (ls->z->reader == getS) {  /* 2.21.2, we are on the interactive level, getF is the file reader.
            On the interactive level, blocks are parsed twice so that a constant declaration in a block would always cause an
            error. We avoid this by allowing redeclarations of constants on the command-line. */
            int saved_top = lua_gettop(L);
            if (L->constwarn) {
              parser_warning(ls, "\b: constants can be redeclared on the command-line, but in script files they cannot");
              L->constwarn = 0;  /* do not print this message again with further redeclarations */
            }
            varname = NULL;
            lua_settop(L, saved_top);
          }
          if (varname) {  /* stolen from Lua 5.4 pre-release */
            const char *msg = luaO_pushfstring(L,
              "attempt to assign to %s constant " LUA_QS, v->k == VLOCAL ? "local" : "global", getstr(varname));
            luaX_syntaxerror(ls, msg);  /* error */
          }
        }
      }
      return 0;
    }
    case TK_PROCNAME: {  /* 2.10.4 */
      init_exp(v, VRELOCABLE, luaK_codeABC(fs, OP_FN, 0, 1, OPR_PROCNAME));
      luaK_exp2nextreg(fs, v);
      luaX_next(ls);
      return 0;
    }
    case '|': {
      /* 2.14.4, absolute value, betragsstriche, some binary relation and other operators have lower prio than 3,
         so they can't be put into absolute value `pipes` (|) */
      int line, dummy;
      line = ls->linenumber;
      luaX_next(ls);
      subexpr(ls, v, 3, 0, &dummy, 1, 0);
      check_match(ls, '|', '|', line);
      luaK_dischargevars(fs, v);
      return 1;
    }
    case TK_STORE: {  /* 2.16.9 */
      int base = fs->freereg;
      init_exp(v, VRELOCABLE, luaK_codeABC(fs, OP_FN, 0, 1, OPR_STORE));
      luaX_next(ls);
      switch (ls->t.token) {
        case '.': {  /* field */
          luaK_exp2nextreg(fs, v);
          field(ls, v);
          break;
        }
        case '[': {  /* `[' exp1 `]' */
          expdesc key;
          luaK_exp2anyreg(fs, v);
          yindexnew(ls, base, v, &key);
          break;
        }
        default:
          luaK_exp2nextreg(fs, v);
      }
      return 0;
    }
    case TK_QMARK: {  /* 2.25.0 */
      if (singlevaraux(ls->fs, luaS_newlstr(ls->L, "varargs", 7), v, 1) != VLOCAL)
        luaX_syntaxerror(ls, "no varargs parameter defined,");
      luaX_next(ls);
      return 0;
    }
    default: {
      luaX_syntaxerror(ls, "unexpected symbol");
      return 0;
    }
  }
}


/* expression = 0 -> called by a statement, 1 -> called by expression parsing */
static int primaryexp (LexState *ls, expdesc *v, int checkconst, int expression) {
  /* primaryexp ->
        prefixexp { `.' NAME | `[' exp `]' | `@@' NAME funcargs | funcargs | `$' (...) | NAME++ | NAME-- | `|' NAME `|'} */
  int base, flag, token;
  FuncState *fs;
  fs = ls->fs;
  v->oop = 0;
  v->cnst = 0;
  /* base must be determined at this position so that substring and replace work properly at
     the interactive level with indexed names */
  base = fs->freereg;
  if (prefixexp(ls, v, checkconst) == 1) {  /* 2.14.4, absolute value, betragsstriche */
    luaK_prefix(fs, OPR_ABS, v);  /* don't call codearithbypass here for it does not treat single-char strings properly */
    return -1;
  }
  flag = 0;
  for (;;) {
    switch (ls->t.token) {
      case '.': {  /* field */
        field(ls, v);
        break;
      }
      case '[': {  /* `[' exp1 `]'  */
        expdesc key;
        luaK_exp2anyreg(fs, v);
        yindexnew(ls, base, v, &key);  /* 2.4.0 */
        break;
      }
      case '(': case TK_STRING: {  /* funcargs, changed 0.5.3 */
        luaK_exp2nextreg(fs, v);
        funcargs(ls, v);
        break;
      }
      case '{': {  /* `{' exp1 `}', null-aware indexing 2.21.0 */
        luaX_next(ls);
        conditional_field(ls, v, 1);
        checknext(ls, '}');
        break;
      }
      case TK_DD: {  /* `..', 2.0.0 */
        luaX_next(ls);
        conditional_field(ls, v, 0);
        break;
      }
      /* tablename `@@' funcargs, 2.6.0, taken from Lua 5.1.4, see lcode.c & lvm.c for details */
      case TK_OOP: {
        expdesc key;
        luaX_next(ls);
        checkname(ls, &key);  /* check and load function name */
        if (ls->t.token == '(') {  /* do we have a method call ? */
          luaK_self(fs, v, &key);
          funcargs(ls, v);  /* read arguments */
        } else {  /* maybe we will have an assignment soon */
          luaK_exp2anyreg(fs, v);
          luaK_indexed(fs, v, &key);
          v->oop = 1;
        }
        break;
      }
      default:
        flag = 1;
    }
    if (flag) break;
  }
  token = ls->t.token;
  switch (token) {
    case TK_PP: {  /* NAME++, 2.14.9 */
      luaX_next(ls);
      plusplus(ls, v, OP_ADD, base, expression, OPR_NOBINOPR);
      break;
    }
    case TK_MM: {  /* NAME--, 2.14.9 */
      luaX_next(ls);
      plusplus(ls, v, OP_SUB, base, expression, OPR_NOBINOPR);
      break;
    }
  }
  return token;  /* next token to be read */
}


/* for tablestat, dictstat, etc. parsing: just parse name or indexed name, nothing else, 0.9.0;
   2.21.2: return TString* instead of void */
static TString *name_or_indexedname (LexState *ls, expdesc *v) {
  /* primaryexp ->
        prefixexp { `.' NAME | `[' exp `]'} | `.' fieldname */
  int base;
  TString *ts;
  FuncState *fs = ls->fs;
  base = fs->freereg;
  ts = str_getname(ls);
  prefixexp(ls, v, 1);
  for (;;) {
    switch (ls->t.token) {
      case '.': {  /* field */
        field(ls, v);
        break;
      }
      case '[': {  /* `[' exp1 { `,' exp1 ... } `]' */
        expdesc key;
        luaK_exp2anyreg(fs, v);
        yindexnew(ls, base, v, &key);  /* 2.4.0 */
        break;
      }
      default: return ts;  /* quit immediately */
    }
  }
  return ts;
}


static int check_ifthen_block (LexState *ls, expdesc *e, int base) {
  /* check_ifthen_block -> IF cond THEN exprlist */
  int condexit;
  FuncState *fs = ls->fs;
  condexit = cond(ls);
  checknext(ls, TK_THEN);
  expr(ls, e);
  luaK_ifoperation(fs, e, base);
  return condexit;
}

/* IF operator, 0.8.0, December 20, 2007 */
static void ifop (LexState *ls, expdesc *e, int line) {
  int flist, base, escapelist;
  /* cannot use e for preliminary result of comparison, for results get overwritten in procs */
  expdesc v;
  FuncState *fs = ls->fs;
  base = fs->freereg;
  escapelist = NO_JUMP;
  flist = check_ifthen_block(ls, &v, base);
  while (ls->t.token == TK_ELIF) {  /* 2.19.0 improvement */
    base = fs->freereg;
    luaX_next(ls);
    luaK_concat(fs, &escapelist, luaK_jump(fs));
    luaK_patchtohere(fs, flist);  /* ditto */
    flist = check_ifthen_block(ls, &v, base);
  }
  checknext(ls, TK_ELSE);
  luaK_concat(fs, &escapelist, luaK_jump(fs));
  luaK_patchtohere(fs, flist);
  expr(ls, &v);
  luaK_ifoperation(fs, &v, base);
  luaK_patchtohere(fs, escapelist);
  check_match2(ls, TK_FI, TK_END, TK_IF, line);  /* 2.26.0 */
  /* create final result, assign preliminary result in v to e */
  init_exp(e, VRELOCABLE, v.u.s.info);
  /* fix it at top of stack; if not, results get overwritten in procedures */
  luaK_exp2anyreg(fs, e);
}


static void isopchunk (LexState *ls, expdesc *e, int base) {  /* IF IS operator, 2.14.2 */
  expdesc v;
  BlockCnt bl;
  FuncState *fs = ls->fs;
  int islast = 0;
  enterblock(fs, &bl, 0);
  enterlevel(ls);
  while (!islast && !block_follow(ls->t.token, 1)) {
    islast = statement(ls);
    testnext(ls, ';');
    lua_assert(ls->fs->f->maxstacksize >= ls->fs->freereg &&
               ls->fs->freereg >= ls->fs->nactvar);
    ls->fs->freereg = ls->fs->nactvar;  /* free registers */
  }
  leavelevel(ls);
  lua_assert(bl.breaklist == NO_JUMP);
  checknext(ls, TK_RET);
  expr(ls, &v);
  testnext(ls, ';');  /* 2.20.0 */
  luaK_ifoperation(fs, &v, base);
  init_exp(e, VRELOCABLE, v.u.s.info);
  /* fix it at top of stack; if not, results get overwritten in procedures */
  luaK_exp2anyreg(fs, e);
  leaveblock(fs);
}

static int is_test_then_block (LexState *ls, expdesc *e, int base) {  /* 2.14.2 */
  /* test_then_block -> [IF IS | ELIF] cond THEN block */
  int condexit;
  luaX_next(ls);  /* skip IF IS or ELIF */
  condexit = cond(ls);
  checknext(ls, TK_THEN);
  isopchunk(ls, e, base);
  return condexit;
}

static void isop (LexState *ls, expdesc *e, int starttoken, int endtoken1, int endtoken2, int line) {  /* IF IS operator, 2.14.2 */
  /* [expr :=] is -> IF IS cond THEN block {ELIF cond THEN block} [ELSE block] FI */
  FuncState *fs = ls->fs;
  int escapelist, flist, base;
  escapelist = NO_JUMP;
  base = fs->freereg;
  flist = is_test_then_block(ls, e, base);  /* IF IS cond THEN block */
  while (ls->t.token == TK_ELIF) {  /* changed Oct. 12, 2006, changed 0.4.0 */
    luaK_concat(fs, &escapelist, luaK_jump(fs));  /* this seems to be a jump if all previous conditions failed */
    luaK_patchtohere(fs, flist);  /* ditto */
    flist = is_test_then_block(ls, e, base);  /* ELIF cond THEN block */
  }
  if (ls->t.token == TK_ELSE) {
    luaK_concat(fs, &escapelist, luaK_jump(fs));  /* this seems to be a jump if all previous conditions failed */
    luaK_patchtohere(fs, flist);  /* ditto */
    luaX_next(ls);  /* skip ELSE (after patch, for correct line info) */
    isopchunk(ls, e, base);
  }
  else {
    luaK_concat(fs, &escapelist, flist);
  }
  luaK_patchtohere(fs, escapelist);
  check_match2(ls, endtoken1, endtoken2, starttoken, line);  /* 2.26.0 */
}


static void testindex (LexState *ls, int base, expdesc *v) {  /* 2.28.2 */
  if (ls->t.token == '[') {  /* `[' exp1 `]' */
    expdesc key;
    luaK_exp2anyreg(ls->fs, v);
    yindexnew(ls, base, v, &key);
  }
}

static int simpleexp (LexState *ls, expdesc *v, int expression) {
  /* simpleexp -> NUMBER | STRING | null | true | false | fail | ... |
                  constructor | PROC body | primaryexp */
  FuncState *fs = ls->fs;
  int base = fs->freereg;
  switch (ls->t.token) {
    case TK_NUMBER: {
      init_exp(v, VKNUM, 0);
      v->u.nval = ls->t.seminfo.r;
      break;
    }
    case TK_STRING: {
      codestring(ls, v, str_getname(ls));
      break;
    }
    case TK_NULL: {  /* changed 0.4.0 */
      init_exp(v, VNIL, 0);
      break;
    }
    case TK_TRUE: {
      init_exp(v, VTRUE, 0);
      break;
    }
    case TK_FALSE: {
      init_exp(v, VFALSE, 0);
      break;
    }
    case TK_FAIL: {
      init_exp(v, VFAIL, 0);
      break;
    }
    case '[': {  /* table constructor */
      constructor(ls, v);
      testindex(ls, base, v);  /* 2.28.2 */
      return -1;
    }
    case '{': {  /* set constructor */
      setconstructor(ls, v);
      return -1;
    }
    case TK_SEQ: {  /* sequence constructor */
      seqconstructor(ls, v);
      testindex(ls, base, v);  /* 2.28.2 */
      return -1;
    }
    case TK_LT: {  /* linalg vector constructor, 4.0.0 RC 1 */
      vectorconstructor(ls, v);
      testindex(ls, base, v);
      return -1;
    }
    case TK_REG: {  /* register constructor */
      regconstructor(ls, v);
      testindex(ls, base, v);  /* 2.28.2 */
      return -1;
    }
    case TK_DATA: {  /* // \\ (table) constructor */
      luaX_next(ls);
      dataconstructor(ls, v);
      return -1;
    }
    case TK_SEQDATA: {  /* (/ \) (sequence) constructor */
      luaX_next(ls);
      seqdataconstructor(ls, v);
      return -1;
    }
    case TK_PROC: {  /* changed 0.4.0 */
      luaX_next(ls);
      body(ls, v, 0, ls->linenumber);
      return -1;
    }
    case TK_LT2: case TK_DEF: case TK_DEFINE: {  /* added 0.5.2; revised 0.7.1, extended 6.1.0 */
      int token = ls->t.token;
      luaX_next(ls);
      fnbody(ls, v, 0, ls->linenumber, token);
      return -1;
    }
    case TK_IF: {  /* added 0.7.1, working since 0.8.0, token changed 2.2.2, extended 2.14.2 */
      luaX_next(ls);
      if (ls->t.token == TK_IS)  /* IF IS oerator */
        isop(ls, v, TK_IF, TK_FI, TK_END, ls->linenumber);
      else  /* short IF op */
        ifop(ls, v, ls->linenumber);
      return -1;
    }
    case TK_WITH: {  /* added 2.10.0, example: `with n := 2*x -> if x < 0 then n else 2*n fi` */
      BlockCnt bl;
      int more = 0;
      enterblock(fs, &bl, 0);
      while (testnext(ls, TK_WITH) || more) {  /* 7.0.0 allow for multipke WITH clauses */
        localstat(ls, 2);  /* 2.12.0 RC 3, <: x -> with n := 1 -> y :> ? -> create local variables */
        more = testnext(ls, ';');
      }
      testnext(ls, TK_ARROW);  /* ignore `->' */
      checknext(ls, TK_IF);
      ifop(ls, v, ls->linenumber);
      leaveblock(fs);
      return -1;
    }
    /* typenames; grep "(GREP_POINT) types;" if you want to add new ones, 0.8.0,
       type designators like `number`, `sequence`, `boolean`, etc. are processed here */
    case TK_TNUMBER: case TK_TSTRING: case TK_TTABLE: case TK_TPROCEDURE:
    case TK_TBOOLEAN: case TK_TTHREAD: case TK_TLIGHTUSERDATA: case TK_TUSERDATA:
    case TK_TSEQUENCE: case TK_TREGISTER: case TK_TPAIR: case TK_TCOMPLEX: case TK_TSET:
    case TK_INTEGER: case TK_POSINT: case TK_NONNEGINT: case TK_NONZEROINT: case TK_POSITIVE: case TK_NEGATIVE: case TK_NONNEGATIVE:
    case TK_TANYTHING: case TK_TLISTING: case TK_TBASIC: {
      codestring(ls, v, luaS_new(ls->L, luaX_tokens[ls->t.token - FIRST_RESERVED]));
      break;
    }
    case TK_NARGS: {
      init_exp(v, VRELOCABLE, luaK_codeABC(fs, OP_FN, 0, 1, OPR_NARGS));
      luaK_exp2nextreg(fs, v);
      break;
    }
    case TK_STORE: {  /* 2.16.7 */
      int base, prefix;
      base = fs->freereg;
      prefix = ls->t.token;
      init_exp(v, VRELOCABLE, luaK_codeABC(fs, OP_FN, 0, 1, OPR_STORE));
      luaX_next(ls);
      switch (ls->t.token) {
        case '.': {  /* field */
          luaK_exp2nextreg(fs, v);
          field(ls, v);
          break;
        }
        case '[': {  /* `[' exp1 `]' */
          expdesc key;
          luaK_exp2anyreg(fs, v);
          yindexnew(ls, base, v, &key);
          break;
        }
        default:
          luaK_exp2nextreg(fs, v);
      }
      if (prefix == TK_PP) {  /* ++store{`.' | `['}, 2.28.4 */
        plusplus(ls, v, OP_ADD, base, expression, OPR_NOBINOPR);
        /* to provoke a syntax error with a succeeding postfix in/decrementor in expressions, which
           produced wrong results */
        luaK_exp2nextreg(fs, v);
      } else if (prefix == TK_MM) { /* --store{`.' | `['}, 2.28.4 */
        plusplus(ls, v, OP_SUB, base, expression, OPR_NOBINOPR);
        /* to provoke a syntax error with a succeeding postfix in/decrementor in expressions, which
           produced wrong results */
        luaK_exp2nextreg(fs, v);
      }
      if (ls->t.token == TK_PP) {  /* 2.28.4 extension */
        luaX_next(ls);
        plusplus(ls, v, OP_ADD, base, expression, OPR_NOBINOPR);
        return TK_PP;
      } else if (ls->t.token == TK_MM) {
        luaX_next(ls);
        plusplus(ls, v, OP_SUB, base, expression, OPR_NOBINOPR);
        return TK_MM;
      }
      return -1;
    }
    case TK_LOG: {  /* added 1.9.3 */
      luaX_next(ls);
      multiargop(ls, v, ls->linenumber, TK_LOG, OPR_LOG);
      return -1;
    }
    case TK_MULADD: {  /* added 2.33.0 */
      luaX_next(ls);
      multiargop(ls, v, ls->linenumber, TK_MULADD, OPR_MULADD);
      return -1;
    }
    case TK_FOREACH: {  /* added 3.4.7 */
      luaX_next(ls);
      multiargop(ls, v, ls->linenumber, TK_FOREACH, OPR_FOREACH);
      return -1;
    }
    case TK_SUMUP: {  /* added 3.6.0 */
      luaX_next(ls);
      multiargop(ls, v, ls->linenumber, TK_SUMUP, OPR_TSUMUP);
      return -1;
    }
    case TK_QSUMUP: {  /* added 4.5.0 */
      luaX_next(ls);
      multiargop(ls, v, ls->linenumber, TK_QSUMUP, OPR_TQSUMUP);
      return -1;
    }
    case TK_MULUP: {  /* added 3.11.2 */
      luaX_next(ls);
      multiargop(ls, v, ls->linenumber, TK_MULUP, OPR_MULUP);
      return -1;
    }
    default: {
      return primaryexp(ls, v, 0, expression);
    }
  }
  luaX_next(ls);
  return -1;
}


static void datasimpleexp (LexState *ls, expdesc *v, int signum) {  /* 2.9.3, create simple data */
  /* datasimpleexp -> NUMBER | STRING */
  switch (ls->t.token) {
    case TK_NUMBER: {
      init_exp(v, VKNUM, 0);
      v->u.nval = signum * ls->t.seminfo.r;
      break;
    }
    case TK_STRING: case TK_NAME: {
      codestring(ls, v, str_getname(ls));
      break;
    }
    case '-': case '+': {  /* 2.10.0 fix */
      int token = ls->t.token;  /* save current token, do not remove */
      luaX_next(ls);
      datasimpleexp(ls, v, (token == '-') ? -1 : 1);  /* call me again with correct sign */
      return;
    }
    default: {
      int token = ls->t.token;
      if (token >= TK_ABS && token <= TK_TANYTHING) {  /* 2.14.6 extension, accept keywords and ... */
        codestring(ls, v, luaS_new(ls->L, luaX_tokens[token - FIRST_RESERVED]));  /* convert them to strings */
        break;
      } else
        luaX_syntaxerror(ls, "token is neither a number nor string");
    }
  }
  luaX_next(ls);
}


static UnOpr getunopr (int op) {
  switch (op) {
    case TK_NOT: return OPR_NOT;
    case '-': return OPR_MINUS;
    case '+': return OPR_PLUS;              /* added 2.8.4; August 12, 2015 */
    case TK_SIZE: return OPR_LEN;           /* added 0.6.0 */
    case TK_ABS: return OPR_ABS;            /* added 0.7.1; Nov 18, 2007 */
    case TK_ASSIGNED: return OPR_ASSIGNED;  /* added 0.6.0 */
    case TK_COS: return OPR_COS;            /* added 0.7.1; Nov 18, 2007 */
    case TK_COSH: return OPR_COSH;          /* added 0.23.0; Jun 14, 2009 */
    case TK_ENTIER: return OPR_ENTIER;      /* added 0.7.1; Nov 18, 2007 */
    case TK_EVEN: return OPR_EVEN;          /* added 0.7.1; Nov 22, 2007 */
    case TK_ODD: return OPR_ODD;            /* added 2.9.0; Oct 22, 2015 */
    case TK_EXP: return OPR_EXP;            /* added 0.7.1; Nov 18, 2007 */
    case TK_FILLED: return OPR_TFILLED;     /* added 0.9.1; Jan 11, 2008 */
    case TK_EMPTY: return OPR_TEMPTY;       /* added 2.10.0; Sep 26, 2016 */
    case TK_FINITE: return OPR_FINITE;      /* added 0.9.1; Jan 26, 2008 */
    case TK_INFINITE: return OPR_INFINITE;  /* added 2.10.0; Oct 25, 2016 */
    case TK_FRACTIONAL: return OPR_FRACTIONAL;  /* added 0.23.0; Jun 14, 2009, changed 3.16.6 */
    case TK_INTEGRAL: return OPR_INTEGRAL;  /* added 2.12.1; Jun 22, 2018 */
    case TK_LNGAMMA: return OPR_LNGAMMA;    /* added 0.9.0; Dec 22, 2007 */
    case TK_IMAG: return OPR_IMAG;          /* added 0.12.0; June 15, 2008 */
    case TK_INT: return OPR_INT;            /* added 0.7.1; Nov 18, 2007 */
    case TK_FRAC: return OPR_FRAC;          /* added 2.3.3; Dec 04, 2014 */
    case TK_LEFT: return OPR_LEFT;          /* added 0.11.1; May 22, 2008 */
    case TK_BOTTOM: return OPR_BOTTOM;      /* added 0.29.0; Nov 20, 2009 */
    case TK_LN: return OPR_LN;              /* added 0.7.1; Nov 18, 2007 */
    case TK_REAL: return OPR_REAL;          /* added 0.12.0; June 15, 2008 */
    case TK_RIGHT: return OPR_RIGHT;        /* added 0.11.1; May 22, 2007 */
    case TK_TOP: return OPR_TOP;            /* added 0.29.0; Nov 20, 2009 */
    case TK_SIGN: return OPR_SIGN;          /* added 0.7.1; Nov 18, 2007 */
    case TK_ARCTAN: return OPR_ARCTAN;      /* added 0.7.1; Nov 18, 2007 */
    case TK_ARCSIN: return OPR_ARCSIN;      /* added 0.27.0; Aug 23, 2009 */
    case TK_ARCCOS: return OPR_ARCCOS;      /* added 0.27.0; Aug 23, 2009 */
    case TK_ARCSEC: return OPR_ARCSEC;      /* added 2.1.2; Jan 08, 2014 */
    case TK_SIN: return OPR_SIN;            /* added 0.7.1; Nov 18, 2007 */
    case TK_SINH: return OPR_SINH;          /* added 0.23.0; Jun 14, 2009 */
    case TK_SQRT: return OPR_SQRT;          /* added 0.7.1; Nov 18, 2007 */
    case TK_TAN: return OPR_TAN;            /* added 0.7.1; Nov 18, 2007 */
    case TK_TANH: return OPR_TANH;          /* added 0.23.0; Jun 14, 2009 */
    case TK_TYPE: return OPR_TYPE;          /* added 0.6.0 */
    case TK_TYPEOF: return OPR_TYPEOF;      /* added 0.12.0; Jun 24, 2008 */
    case TK_UNASSIGNED: return OPR_UNASSIGNED;  /* added 0.11.2; June 13, 2008 */
    case TK_BNOT: return OPR_BNOT;          /* added 0.27.0 */
    case TK_RECIP: return OPR_RECIP;        /* added 2.1 RC 1 */
    case TK_NAN: return OPR_NAN;            /* added 2.1.2 */
    case TK_COSXX: return OPR_COSXX;        /* added 2.1.2 */
    case TK_BEA: return OPR_BEA;            /* added 2.1.2 */
    case TK_FLIP: return OPR_FLIP;          /* added 2.1.2 */
    case TK_CONJUGATE: return OPR_CONJUGATE;  /* added 2.1.2 */
    case TK_POP: return OPR_POP;            /* added 2.2.0 */
    case TK_ANTILOG2: return OPR_ANTILOG2;  /* added 2.4.1 */
    case TK_ANTILOG10: return OPR_ANTILOG10; /* added 2.4.1 */
    case TK_SIGNUM: return OPR_SIGNUM;      /* added 2.4.1 */
    case TK_SINC: return OPR_SINC;          /* added 2.4.2 */
    case TK_QMDEV: return OPR_QMDEV;        /* added 2.4.2 */
    case TK_CIS: return OPR_CIS;            /* added 2.5.1 */
    case TK_SQUARE: return OPR_SQUARE;      /* added 2.12.6 */
    case TK_CUBE: return OPR_CUBE;          /* added 2.13.0 */
    case TK_ZERO: return OPR_ZERO;          /* added 2.16.0 */
    case TK_NONZERO: return OPR_NONZERO;    /* added 2.16.0 */
    case TK_INVSQRT: return OPR_INVSQRT;    /* added 2.16.13 */
    case TK_PEPS: return OPR_PEPS;          /* added 2.9.1 */
    case TK_MEPS: return OPR_MEPS;          /* added 2.9.1 */
    case TK_CELL: return OPR_CELL;          /* added 2.9.4 */
    case TK_PP: return OPR_PP;              /* added 2.28.0 */
    case TK_MM: return OPR_MM;              /* added 2.28.0 */
    case TK_UNITY: return OPR_UNITY;        /* added 2.38.3 */
    default: return OPR_NOUNOPR;
  }
}


static BinOpr getbinopr (int op) {
  switch (op) {
    case '+': return OPR_ADD;
    case '-': return OPR_SUB;
    case '*': return OPR_MUL;
    case '/': return OPR_DIV;
    case '%': return OPR_MOD;
    case '^': return OPR_POW;
    case '\\': return OPR_INTDIV;              /* added 0.5.4 */
    case TK_CONCAT: return OPR_CONCAT;
    case TK_SPLIT: return OPR_SPLIT;           /* added 0.6.0 */
    case TK_NEQ: return OPR_NE;                /* changed 0.4.0 */
    case TK_EQ: return OPR_EQ;
    case TK_LT: return OPR_LT;
    case TK_LE: return OPR_LE;
    case TK_GT: return OPR_GT;
    case TK_GE: return OPR_GE;
    case TK_AND: return OPR_AND;
    case TK_OR: return OPR_OR;
    case TK_IN: return OPR_IN;                 /* added 0.6.0 */
    case TK_SUBSET: return OPR_TSUBSET;        /* added 0.6.0 */
    case TK_XSUBSET: return OPR_TXSUBSET;      /* added 0.9.1 */
    case TK_UNION: return OPR_TUNION;          /* added 0.6.0 */
    case TK_MINUS: return OPR_TMINUS;          /* added 0.6.0 */
    case TK_INTERSECT: return OPR_TINTERSECT;  /* added 0.6.0 */
    case TK_IPOW: return OPR_IPOW;             /* added 0.9.2 */
    case ':': return OPR_PAIR;                 /* added 0.11.1 */
    case TK_COMPLEX: return OPR_COMPLEX;       /* added 3.10.5 */
    case TK_CARTESIAN: return OPR_CARTESIAN;   /* added 3.10.5 */
    case TK_DCOLON: return OPR_TOFTYPE;        /* added 1.3.0, Jan 01, 2011 */
    case TK_NOTOFTYPE: return OPR_TNOTOFTYPE;  /* added 1.3.0, Jan 01, 2011 */
    case TK_EEQ: return OPR_TEEQ;              /* added 0.22.0 */
    case TK_AEQ: return OPR_TAEQ;              /* added 2.1.4 */
    case TK_NAEQ: return OPR_NAEQ;             /* added 2.5.2 */
    case TK_XOR: return OPR_XOR;               /* added 0.27.0 */
    case TK_ATENDOF: return OPR_ATENDOF;       /* added 0.27.0 */
    case TK_BAND: return OPR_BAND;             /* added 0.27.0 */
    case TK_BOR: return OPR_BOR;               /* added 0.27.0 */
    case TK_BXOR: return OPR_BXOR;             /* added 0.27.0 */
    case TK_BLEFT: return OPR_BLEFT;           /* added 2.3.0 RC 4 */
    case TK_BRIGHT: return OPR_BRIGHT;         /* added 2.3.0 RC 4 */
    case TK_PERCENT: return OPR_PERCENT;       /* added 1.10.6 */
    case TK_PERCENTRATIO: return OPR_PERCENTRATIO;  /* added 1.11.4 */
    case TK_PERCENTADD: return OPR_PERCENTADD; /* added 1.11.3 */
    case TK_PERCENTSUB: return OPR_PERCENTSUB; /* added 1.11.3 */
    case TK_PERCENTCHANGE: return OPR_PERCENTCHANGE; /* added 2.10.0 */
    case TK_MAP: return OPR_MAP;               /* added 2.2.0 */
    case TK_SELECT: return OPR_SELECT;         /* added 2.2.5 */
    case TK_HAS: return OPR_HAS;               /* added 2.22.2 */
    case TK_COUNT: return OPR_COUNT;           /* added 4.6.0 */
    case TK_NAND: return OPR_NAND;             /* added 2.5.2 */
    case TK_NOR: return OPR_NOR;               /* added 2.5.2 */
    case TK_XNOR: return OPR_XNOR;             /* added 2.8.5 */
    case TK_LBROTATE: return OPR_LBROTATE;     /* added 2.5.4 */
    case TK_RBROTATE: return OPR_RBROTATE;     /* added 2.5.4 */
    case '|': return OPR_COMPARE;              /* added 2.9.4 */
    case TK_ACOMPARE: return OPR_ACOMPARE;     /* added 2.11.0 RC1 */
    case TK_ABSDIFF: return OPR_ABSDIFF;       /* added 2.9.8 */
    case TK_SYMMOD: return OPR_SYMMOD;         /* added 2.10.0 */
    case TK_ROLL: return OPR_ROLL;             /* added 2.13.0 */
    case TK_I32ADD: return OPR_I32ADD;         /* added 2.15.0 */
    case TK_I32SUB: return OPR_I32SUB;         /* added 2.15.0 */
    case TK_I32MUL: return OPR_I32MUL;         /* added 2.15.0 */
    case TK_I32DIV: return OPR_I32DIV;         /* added 2.15.0 */
    case TK_I32INTDIV: return OPR_I32DIV;      /* added 2.15.0 */
    case TK_SQUAREADD: return OPR_SQUAREADD;   /* added 2.16.13 */
    case TK_NOTIN: return OPR_NOTIN;           /* added 2.16.14 */
    case TK_INC: return OPR_INC;               /* added 2.32.0 */
    case TK_DEC: return OPR_DEC;               /* added 2.32.0 */
    case TK_MUL: return OPR_MULTIPLY;          /* added 2.32.0 */
    case TK_DIV: return OPR_DIVIDE;            /* added 2.32.0 */
    case TK_INTDIV: return OPR_INTDIVIDE;      /* added 2.32.0 */
    case TK_MOD: return OPR_MODULUS;           /* added 2.32.0 */
    default: return OPR_NOBINOPR;
  }
}


static const struct {
  lu_byte left;   /* left priority for each binary operator */
  lu_byte right;  /* right priority */
} priority[] = {  /* ORDER OPR -> lcode.h */
   {6, 6}, {6, 6}, {7, 7}, {7, 7}, {7, 7},  /* `+', `-', `*', `/', `%' */
   {10, 9}, {10, 9},                /* `^`, `**`, both are right-associative, i.e. x^y^z = x^(y^z) */
   {7, 7}, {5, 4},                  /* `\` (added 0.5.4), `&` (concat, right associative) */
   {3, 3}, {3, 3},                  /* inequality  and equality */
   {3, 3}, {3, 3}, {3, 3}, {3, 3},  /* order */
   {2, 2}, {1, 1},                  /* logical and / or, in this order */
   {4, 4}, {4, 4}, {4, 4},          /* in, subset, xsubset; added 0.7.1 & 0.5.4, 0.9.1 */
   {4, 4}, {4, 4}, {4, 4},          /* union, minus, intersect operators; added 0.6.0 */
   {6, 6}, {5, 4}, {8, 8}, {8, 8},  /* split, pair constr, complex ! and cartesian constr !! */
   {3, 3}, {3, 3}, {3, 3}, {1, 1},  /* `==` , `~=`, `~<>`, xor */
   {4, 4}, {7, 7}, {6, 6}, {6, 6},  /* atendof, band, bor, bxor */
   {7, 7}, {7, 7}, {3, 3}, {3, 3},  /* <<, >>, ::, :- */
   {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7},  /* *%, /%, +% , -%, %% */
   {5, 4}, {5, 4}, {5, 4}, {5, 4},  /* @, $, $$, $$$ */
   {2, 2}, {1, 1}, {1, 1},          /* logical nand, nor, xnor */
   {7, 7}, {7, 7}, {3, 3}, {3, 3}, {4, 4}, {7, 7}, {7, 7},  /* <<<, >>>, |, ~|, |-, symmod, roll */
   {6, 6}, {6, 6}, {7, 7}, {7, 7}, {7, 7}, {4, 4},   /* `&+`, `&-`, `&*`, `&/`, squareadd, notin */
   {6, 6}, {6, 6}, {7, 7}, {7, 7}, {7, 7}, {7, 7}    /* inc, dec, mul, div, intdiv, mod */
};

/* For UNARY_PRIORITY, UNARYOP_PRIORITY and HIGHEST_PRIORITY see lparser.h */


/* Checks conditions like while f := io.read() do break when f = 'Z'; od; and assignments in if statements -- 2.19.0;
   flag = 1 -> change jump point for while loops, 2.29.4 */

#define MISCSTAT      0
#define WHILESTAT     1
#define IFSTAT        2
#define FORSTAT       3

static int condassign (LexState *ls, int flag, int *init) {
  expdesc e;
  FuncState *fs = ls->fs;
  if (testnext(ls, TK_LOCAL)) {  /* 5.5.0 */
    lu_byte isconst;
    if (flag == WHILESTAT)
      /* use of `%' placeholder and called from WHILE loop ? -> reset jump label and then proceed with next expression, 3.14.0 */
      *init = luaK_getlabel(fs);
    isconst = testnext(ls, TK_CONSTANT);
    if (isnext(ls, TK_NAME) && luaX_lookahead(ls) == TK_ASSIGN) {
      int nvars = 0;
      new_localvar(ls, str_checkname(ls), nvars++, 1, isconst);
      checknext(ls, TK_ASSIGN);
      expr(ls, &e);
      adjust_assign(ls, nvars, 1, &e);
      adjustlocalvars(ls, nvars);
      if (!(testnext(ls, TK_WITH) || testnext(ls, ','))) {  /* evaluate rhs and jump if true */
        luaK_goiftrue(fs, &e);
        return e.f;
      }
    } else {
      luaX_syntaxerror(ls, "invalid expression in condition,");
    }
  } else if (isnext(ls, TK_NAME) && luaX_lookahead(ls) == TK_ASSIGN) {
    expdesc v;
    if (flag == WHILESTAT)
      /* use of `%' placeholder and called from WHILE loop ? -> reset jump label and then proceed with next expression, 3.14.0 */
      *init = luaK_getlabel(fs);
    primaryexp(ls, &v, 1, 1);  /* 2.21.2, and check for constant */
    luaX_next(ls);  /* skip `:=' */
    expr(ls, &e);
    /* check rhs of assignment */
    if (e.k == VNIL || e.k == VFAIL) e.k = VFALSE;  /* `falses' are all equal here */
    /* assign */
    luaK_setoneret(fs, &e);  /* close last expression */
    luaK_storevar(fs, &v, &e);
    if (!(testnext(ls, TK_WITH) || testnext(ls, ','))) {  /* evaluate rhs and jump if true, 2.29.4 */
      luaK_goiftrue(fs, &v);
      return v.f;
    }
  }  /* end of assignment in `if`/`while` clause */
  expr(ls, &e);  /* avoid the 3 percent overhead of calling `cond`; read condition */
  if (e.k == VNIL || e.k == VFAIL) e.k = VFALSE;  /* `falses' are all equal here */
  luaK_goiftrue(fs, &e);
  return e.f;
}


static int condfalse (LexState *ls) {  /* added 0.5.3 */
  /* cond -> exp */
  expdesc v;
  expr(ls, &v);  /* read condition */
  if (v.k == VNIL || v.k == VFAIL) v.k = VFALSE;  /* `falses' are all equal here */
  luaK_goiffalse(ls->fs, &v);
  return v.t;
}


static int condfalseassign (LexState *ls, int flag, int *init) {  /* 2.27.0, 2.29.4 */
  expdesc e;
  FuncState *fs = ls->fs;
  if (isnext(ls, TK_NAME) && luaX_lookahead(ls) == TK_ASSIGN) {
    expdesc v;
    primaryexp(ls, &v, 1, 1);  /* 2.21.2, and check for constant */
    luaX_next(ls);  /* skip `:=' */
    expr(ls, &e);
    /* check rhs of assignment */
    if (e.k == VNIL || e.k == VFAIL) e.k = VFALSE;  /* `falses' are all equal here */
    /* assign */
    luaK_setoneret(fs, &e);  /* close last expression */
    luaK_storevar(fs, &v, &e);
    /* evaluate rhs and jump if true */
    if (flag == MISCSTAT || !(testnext(ls, TK_WITH) || testnext(ls, ','))) {  /* evaluate rhs and jump if true, 2.29.4 */
      luaK_goiffalse(fs, &v);
      return v.t;
    }
    switch (flag) {
      case WHILESTAT:  /* called from WHILE loop ? -> reset jump label and then proceed with next expression */
        *init = luaK_getlabel(fs);
        break;
      default:  /* called from IF statement -> just proceed with next expression */
        break;
    }
  }
  expr(ls, &e);  /* avoid the 3 percent overhead of calling `cond`; read condition */
  if (e.k == VNIL || e.k == VFAIL) e.k = VFALSE;  /* `falses' are all equal here */
  luaK_goiffalse(fs, &e);
  return e.t;
}


/*
** subexpr -> (simpleexp | unop subexpr) { binop subexpr }
** where `binop' is any binary operator with a priority higher than `limit'
** lasttoken includes the token parsed before returning, used to allow `++' & `--' operators to be used as statements
** expression: 0 called by a stetement, 1 called during expression parsing
*/

/* Reads function, given with either a simple name or an indexed name; 2.35.5 */
static void fetchfunction (LexState *ls, int base, expdesc *f) {
  int flag = 0;
  singlevar(ls, f);
  for (;;) {
    switch (ls->t.token) {
      case '.': {  /* field */
        field(ls, f);
        break;
      }
      case '[': {  /* `[' exp1 `]'  */
        expdesc key;
        luaK_exp2anyreg(ls->fs, f);
        yindexnew(ls, base, f, &key);  /* 2.4.0 */
        break;
      }
      default:
        flag = 1;
    }
    if (flag) break;
  }
}

static int canchainopr (BinOpr opr) {
  return opr == OPR_LT || opr == OPR_LE || opr == OPR_GT || opr == OPR_GE;
}

static BinOpr subexpr (LexState *ls, expdesc *v, unsigned int limit, int isunaryop, int *lasttoken, int expression, int isvector) {
  BinOpr op;
  UnOpr uop;
  int base, line, dummy;
  FuncState *fs = ls->fs;
  enterlevel(ls);
  *lasttoken = -1;  /* positive of expression finished with `++' or `--' 2.22.0 */
  base = fs->freereg;
  line = ls->linenumber;  /* check whether name of user-defined binary op is in the same line,
    to cope with situations where a semicolon between two statements is missing */
  uop = getunopr(ls->t.token);
  if (uop != OPR_NOUNOPR) {  /* unary operator ? */
    luaX_next(ls);  /* skip operator */
    subexpr(ls, v, (uop < OPR_ABS) ? UNARY_PRIORITY : UNARYOP_PRIORITY, 1, &dummy, expression, 0);
    luaK_prefix(fs, uop, v);
    if (uop == OPR_PP) { /* ++NAME, 2.28.0 */
      plusplus(ls, v, OP_ADD, base, expression, uop);
      /* 2.28.2; to provoke a syntax error with a succeeding postfix in/decrementor in expressions, which
         produced wrong results */
      luaK_exp2nextreg(fs, v);
    } else if (uop == OPR_MM) { /* --NAME, 2.28.0 */
      plusplus(ls, v, OP_SUB, base, expression, uop);
      /* 2.28.2; to provoke a syntax error with a succeeding postfix in/decrementor in expressions, which
         produced wrong results */
      luaK_exp2nextreg(fs, v);
    }
  } else {
    *lasttoken = simpleexp(ls, v, expression);  /* changed 2.22.0 */
  }
  if (!isunaryop && ls->t.token == TK_NAME && ls->linenumber == line) {  /* 2.6.0, self-defined binary operator */
    expdesc f;
    luaK_setmultret(fs, v);
    luaK_exp2nextreg(fs, v);      /* put left operator into a register */
    fetchfunction(ls, base, &f);  /* read function name, given either by a simple or index name, 2.35.5 fix */
    luaK_exp2nextreg(fs, &f);     /* put function into register */
    binopargs(ls, base, &f, v);   /* process arguments and call function */
  }
  /* expand while operators have priorities higher than `limit' */
  op = getbinopr(ls->t.token);
  while (op != OPR_NOBINOPR && priority[op].left > limit && (!isvector || (isvector && (op < OPR_LT || op > OPR_GE)))) {
    expdesc v2;
    BinOpr nextop;
    luaX_next(ls);
    luaK_infix(fs, op, v);  /* = luaK_exp2RK(fs, v); */
    /* read sub-expression with higher priority */
    nextop = subexpr(ls, &v2, priority[op].right, 0, &dummy, expression, isvector);
    luaK_posfix(fs, op, v, &v2);  /* => o1 = luaK_exp2RK(fs, e1); o2 = luaK_exp2RK(fs, e2); freeexp(fs, e2); freeexp(fs, e1); */
    /* 6.0.0, taken from Pluto 0.12.0, grep "case OPR_LT: case OPR_LE:" in lcode.c:

       MIT License

       Copyright (C) 2022-2025 PlutoLang.org, Ryan Starrett, Sainan.
       Copyright (C) 1994-2024 Lua.org, PUC-Rio.

       Permission is hereby granted, free of charge, to any person obtaining a copy
       of this software and associated documentation files (the "Software"), to deal
       in the Software without restriction, including without limitation the rights
       to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
       copies of the Software, and to permit persons to whom the Software is
       furnished to do so, subject to the following conditions:

       The above copyright notice and this permission notice shall be included in all
       copies or substantial portions of the Software.

       THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
       IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
       FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
       AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
       LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
       OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
       SOFTWARE. */
    if (canchainopr(op) && canchainopr(nextop)) {
      while (1) {
        op = nextop;
        if (v2.k == VNONRELOC) {  /* this is a Lua 5.1-specific modification as the original Pluto code, based on Lua 5.4, will crash. */
          luaK_codeABC(fs, OP_MOVE, fs->freereg, v2.u.s.info, 0);  /* make copy */
          v2.u.s.info = fs->freereg;
          luaK_reserveregs(fs, 1);
        }
        luaX_next(ls);  /* skip operator */
        /* to generate 'a < b < c': 'a < b' is in `v`. 'b' is in `v2`. 'c' can be read from lexer state. */
        luaK_infix(ls->fs, OPR_AND, v);
        expdesc v3;
        luaK_infix(ls->fs, op, &v2);
        nextop = subexpr(ls, &v3, priority[op].right, 0, &dummy, expression, isvector);
        luaK_posfix(ls->fs, op, &v2, &v3);
        luaK_posfix(ls->fs, OPR_AND, v, &v2);
        if (!canchainopr(nextop))
          break;
        v2 = v3;
      }
    }
    op = nextop;
  }
  leavelevel(ls);
  return op;  /* return first untreated operator, base = position of left operand */
}


static void expr (LexState *ls, expdesc *v) {
  int lasttoken;
  subexpr(ls, v, 0, 0, &lasttoken, 1, 0);
}


static void funcargexpr (LexState *ls, expdesc *v, int isunaryop) {
  BinOpr op;
  UnOpr uop;
  int ispair;
  unsigned int limit;
  int base, line, dummy;
  FuncState *fs = ls->fs;
  base = fs->freereg;
  line = ls->linenumber;   /* check whether user-defined binary op is in the same line,
    to cope with situations where a semicolon between two statements is missing */
  enterlevel(ls);
  uop = getunopr(ls->t.token);
  ispair = 0;
  limit = 0;
  if (uop != OPR_NOUNOPR) {  /* unary op ? */
    luaX_next(ls);
    subexpr(ls, v, (uop < OPR_ABS) ? UNARY_PRIORITY : UNARYOP_PRIORITY, 1, &dummy, 1, 0);
    luaK_prefix(fs, uop, v);
    if (uop == OPR_PP)  /* 2.28.0 */
      plusplus(ls, v, OP_ADD, base, 1, uop);
    else if (uop == OPR_MM)
      plusplus(ls, v, OP_SUB, base, 1, uop);
  } else {
    if (ls->t.token == TK_NAME) {
      int lookahead = luaX_lookahead(ls);
      if ( (ispair = (lookahead == TK_EQ || lookahead == TK_SEP)))  /* 0.32.0, `name = | ~ value' option ? */
        checkname(ls, v);  /* convert name to string */
      else
        primaryexp(ls, v, 0, 1);  /* 2.3.4, we already have a name, so skip unnecessary call to simpleexp */
    } else
      simpleexp(ls, v, 1);
  }
  if (!isunaryop && ls->t.token == TK_NAME && ls->linenumber == line) {  /* 2.6.0 */
    expdesc f;
    luaK_setmultret(fs, v);
    luaK_exp2nextreg(fs, v);      /* put left operator into a register */
    fetchfunction(ls, base, &f);  /* read function name, given either by a simple or indexed name, 2.35.5 fix */
    luaK_exp2nextreg(fs, &f);     /* put function into register */
    binopargs(ls, base, &f, v);   /* process arguments and call function */
  }
  /* expand while operators have priorities higher than `limit' */
  op = (ispair) ? OPR_PAIR : getbinopr(ls->t.token);
  while (op != OPR_NOBINOPR && priority[op].left > limit) {
    expdesc v2;
    BinOpr nextop;
    luaX_next(ls);
    luaK_infix(fs, op, v);
    /* read sub-expression with higher priority */
    nextop = subexpr(ls, &v2, priority[op].right, 0, &dummy, 1, 0);
    luaK_posfix(fs, op, v, &v2);
    op = nextop;
  }
  leavelevel(ls);
}


/* }==================================================================== */



/*
** {======================================================================
** Rules for Statements
** =======================================================================
*/


static int block_follow (int token, int isop) {
  if (isop && token == TK_RET) return 1;
  switch (token) {
    case TK_END: case TK_ELSE: case TK_ELIF: case TK_FI: case TK_OD:
    case TK_OF: case TK_ESAC: case TK_GT2: case TK_AS: case TK_EPOCS:
    case TK_UNTIL:      /* added 2.0.0 RC 2 */
    case TK_ONSUCCESS:  /* added 2.0.0 RC 3 */
    case TK_CATCH:      /* added 2.1 RC 2 */
    case TK_YRT:        /* added 2.1 RC 2 */
    case TK_ESLE:       /* added 2.19.0 */
    case TK_EOS:        /* changed 0.5.3 */
      return 1;
    default: return 0;
  }
}


static void block (LexState *ls) {
  /* block -> chunk */
  FuncState *fs = ls->fs;
  BlockCnt bl;
  enterblock(fs, &bl, 0);
  chunk(ls);
  lua_assert(bl.breaklist == NO_JUMP);
  leaveblock(fs);
}


/*
** check whether, in an assignment to a local variable, the local variable
** is needed in a previous assignment to a table. If so, save original
** local value in a safe place and use this safe copy in the previous
** assignment.
*/
static void check_conflict (LexState *ls, struct LHS_assign *lh, expdesc *v) {
  FuncState *fs = ls->fs;
  int extra = fs->freereg;  /* eventual position to save local variable */
  int conflict = 0;
  for (; lh; lh = lh->prev) {
    if (lh->v.k == VINDEXED) {
      if (lh->v.u.s.info == v->u.s.info) {  /* conflict? */
        conflict = 1;
        lh->v.u.s.info = extra;  /* previous assignment will use safe copy */
      }
      if (lh->v.u.s.aux == v->u.s.info) {  /* conflict? */
        conflict = 1;
        lh->v.u.s.aux = extra;  /* previous assignment will use safe copy */
      }
    }
  }
  if (conflict) {
    luaK_codeABC(fs, OP_MOVE, fs->freereg, v->u.s.info, 0);  /* make copy */
    luaK_reserveregs(fs, 1);
  }
}


/* taken from a Lua 5.1 power patch written by Peter Shook, http://lua-users.org/wiki/PeterShook.
   See: http://lua-users.org/wiki/LuaPowerPatches, Unpack Tables by Name (5.2, 5.1, 5.0.2) */
static void getfrom (LexState *ls, expdesc *e, expdesc *v) {  /* 2.8.0 */
  /* e: table, v: name list */
  expdesc key;
  FuncState *fs = ls->fs;
  int k = v->k;
  if (k == VLOCAL)
    codestring(ls, &key, getlocvar(fs, v->u.s.info).varname);
  else if (k == VUPVAL)
    codestring(ls, &key, fs->f->upvalues[v->u.s.info]);
  else if (k == VGLOBAL)
    init_exp(&key, VK, v->u.s.info);
  else
    check_condition(ls, VLOCAL <= k && k <= VGLOBAL, "syntax error in from vars");
  luaK_indexed(fs, e, &key);
}


static void check_or_make_readonly (LexState *ls, expdesc *v) {  /* 2.20.0, idea stolen from Lua 5.4 pre-release */
  FuncState *fs = ls->fs;
  lua_State *L = ls->L;
  TString *varname = NULL;
  if (v->k == VLOCAL) {
    LocVar local = getlocvar(fs, v->u.s.info);
    if (local.isconst) varname = local.varname;
  } else if (v->k == VGLOBAL) {
    int assigned;
    UltraSet *us = usvalue(constants(L));
    varname = str_getname(ls);
    assigned = agnUS_getstr(us, varname);
    if (v->cnst) {  /* variable to be a constant ? -> add constant to internal constants set */
      if (!assigned) {
        agnUS_setstr2set(L, us, varname);  /*changed 2.20.1 */
        varname = NULL;
      } else if (CONSTANTS_ON && ls->z->reader == getS) {  /* 2.21.2, we are on the interactive level, getF is the file reader.
        On the interactive level, blocks are parsed twice so that a constant declaration in a block would always cause an
        error. We avoid this by allowing redeclarations of constants on the command-line. */
        int saved_top = lua_gettop(L);
        if (L->constwarn) {
          parser_warning(ls, "\b: constants can be redeclared on the command-line, but in script files they cannot");
          L->constwarn = 0;  /* do not print this message again with further redeclarations */
        }
        varname = NULL;
        lua_settop(L, saved_top);
      }
    } else {  /* check whether value shall be assigned to a constant (protected name) */
      if (!assigned) varname = NULL;  /* no, it isn't a constant */
    }
  }
  if (CONSTANTS_ON && varname) {  /* stolen from Lua 5.4 pre-release */
    const char *msg = luaO_pushfstring(L,
      "attempt to assign to %s constant " LUA_QS, v->k == VLOCAL ? "local" : "global", getstr(varname));
    luaX_syntaxerror(ls, msg);  /* error */
  }
}

static int assignment (LexState *ls, struct LHS_assign *lh, int nvars, lu_byte *from_var) {
  /* extended 2.8.0 with Peter Shook's unpack table patch */
  expdesc e;
  FuncState *fs = ls->fs;
  int from = 0;
  check_condition(ls, VLOCAL <= lh->v.k && lh->v.k <= VINDEXED,
                      "syntax error in assignment or unexpected expression");  /* 2.27.2 extension */
  check_or_make_readonly(ls, &lh->v);
  if (testnext(ls, ',')) {  /* assignment -> `,' primaryexp assignment */
    struct LHS_assign nv;
    nv.prev = lh;
    nv.v.oop = 0;
    nv.v.cnst = testnext(ls, TK_CONSTANT);  /* 2.20.0/6.0.0 */
    primaryexp(ls, &nv.v, 0, 0);
    if (nv.v.k == VLOCAL)
      check_conflict(ls, lh, &nv.v);
    luaY_checklimit(ls->fs, nvars, LUAI_MAXCCALLS - ls->L->nCcalls,  /* Lua 5.1.3 patch */
                    "variable names");
    from = assignment(ls, &nv, nvars + 1, from_var);
  } else {  /* assignment -> `:=' explist1 */
    if (testnext(ls, TK_ASSIGN)) {  /* default assignment */
      int nexps;
      if (lh->v.oop) {  /* 2.25.0 RC 1, assign oop method to name */
        if (nvars != 1)
          luaX_syntaxerror(ls, "can only assign one method to one name at once,");
        if (testnext(ls, TK_PROC) || testnext(ls, TK_TPROCEDURE)) {
          body(ls, &e, 1, ls->linenumber);
        } else if (testnext(ls, TK_LT2)) {
          fnbody(ls, &e, 1, ls->linenumber, TK_LT2);
        } else if (testnext(ls, TK_DEF) || testnext(ls, TK_DEFINE)) {  /* 6.1.0 */
          fnbody(ls, &e, 1, ls->linenumber, TK_DEF);
        } else
          luaX_syntaxerror(ls, "can only assign a procedure or short-cut function,");
        luaK_setoneret(fs, &e);  /* close last expression */
        luaK_storevar(fs, &lh->v, &e);
        return 0;
      }
      /* now process `normal` assignments */
      nexps = explist1(ls, &e);
      if (nexps != nvars) {
        adjust_assign(ls, nvars, nexps, &e);
        if (nexps > nvars)
          fs->freereg -= nexps - nvars;  /* remove extra values */
      } else {
        luaK_setoneret(fs, &e);  /* close last expression */
        luaK_storevar(fs, &lh->v, &e);
        return 0;  /* avoid default */
      }
    }
    /* assignment -> `->' expr;
       0.6.0, assign right-hand value to all left-hand values; modified 0.7.1 */
    else if (testnext(ls, TK_ARROW)) {
      int i;
      expr(ls, &e);  /* get right-hand value and put it in register */
      for (i=1; i < nvars; i++) {  /* copy it nvars - 1 times */
        luaK_exp2nextreg(fs, &e);  /* put e on stack */
        luaK_reserveregs(fs, 1);   /* register it */
      }
      luaK_setoneret(fs, &e);  /* close last expression */
      luaK_storevar(fs, &lh->v, &e);
      return 0;
    } else if (testnext(ls, TK_IN)) {  /* 2.8.0, taken from a Lua 5.1 power patch written by Peter Shook,
      http://lua-users.org/wiki/PeterShook. See: http://lua-users.org/wiki/LuaPowerPatches, Unpack Tables
      by Name (5.2, 5.1, 5.0.2): with key1 [, key2, ···] in tablename ... */
      new_localvarliteral(ls, "(with)", 0);
      primaryexp(ls, &e, 0, 1);
      luaK_exp2nextreg(fs, &e);
      *from_var = fs->nactvar;
      adjustlocalvars(ls, 1);
      luaK_setoneret(fs, &e);  /* close last expression */
      getfrom(ls, &e, &lh->v);
      luaK_storevar(fs, &lh->v, &e);
      return 1;
    } else {
      lh->v.oop = 0;  /* better be sure than sorry 2.25.0 RC 2 */
      luaX_syntaxerror(ls, "unexpected symbol");
    }
  }
  init_exp(&e, VNONRELOC, fs->freereg - 1);  /* default assignment */
  if (from) getfrom(ls, &e, &lh->v);
  luaK_storevar(fs, &lh->v, &e);
  return from;
}


static int break_or_continue (LexState *ls, const char *msg, size_t offset) {
  FuncState *fs = ls->fs;
  BlockCnt *bl = fs->bl;
  int iswhen, condexit, escapelist, upval;
  upval = condexit = 0;
  escapelist = NO_JUMP;
  if ( (iswhen = testnext(ls, TK_WHEN)) )  /* 2.2.2 */
    condexit = cond(ls);
  else if ( (iswhen = testnext(ls, TK_UNLESS)) )  /* 3.10.0 */
    condexit = condfalse(ls);
  while (bl && bl->isbreakable != 1) {  /* 2.1 RC 2 */
    if (bl->isbreakable == 2)
      luaK_codeABC(fs, OP_ENDTRY, 0, 0, 0);
    upval |= bl->upval;
    bl = bl->previous;
  }
  if (!bl)
    luaX_syntaxerror(ls, msg);
  if (upval)
    luaK_codeABC(fs, OP_CLOSE, bl->nactvar, 0, 0);
  luaK_concat(fs, (int*) (((char*) bl) + offset), luaK_jump(fs));
  if (iswhen) {  /* 2.2.2 */
    luaK_concat(ls->fs, &escapelist, condexit);
    luaK_patchtohere(ls->fs, escapelist);
  }
  return iswhen;
}

static int breakstat (LexState *ls) {   /* changed */
  return break_or_continue(ls, "no loop to break", offsetof(BlockCnt, breaklist));
}

/* skip statement written by Wolfgang Oertl and posted to the Lua Mailing List on 2005-09-12 */
static int skipstat (LexState *ls) {  /* added */
  return break_or_continue(ls, "no loop to skip", offsetof(BlockCnt, continuelist));
}


static void redostat (LexState *ls) {  /* Agena 2.1 RC 1 */
  FuncState *fs = ls->fs;
  BlockCnt *bl = fs->bl;
  int upval = 0;
  while (bl && bl->isbreakable != 1) {  /* 2.1 RC 2 */
    if (bl->isbreakable == 2)
      luaK_codeABC(fs, OP_ENDTRY, 0, 0, 0);
    upval |= bl->upval;
    bl = bl->previous;
  }
  if (!bl || bl->redolist == NO_JUMP)
    luaX_syntaxerror(ls, "no `for` loop to redo");
  if (upval)
    luaK_codeABC(fs, OP_CLOSE, bl->nactvar, 0, 0);
  luaK_patchlist(fs, luaK_jump(fs), bl->redolist);  /* and repeat iteration */
}


static void relaunchstat (LexState *ls) {  /* Agena 2.2.0 RC 4 */
  FuncState *fs = ls->fs;
  BlockCnt *bl = fs->bl;
  int upval = 0;
  while (bl && bl->isbreakable != 1) {  /* 2.1 RC 2 */
    if (bl->isbreakable == 2)
      luaK_codeABC(fs, OP_ENDTRY, 0, 0, 0);
    upval |= bl->upval;
    bl = bl->previous;
  }
  if (!bl || bl->relaunchlist == NO_JUMP)
    luaX_syntaxerror(ls, "no `for` loop to relaunch");
  if (upval)
    luaK_codeABC(fs, OP_CLOSE, bl->nactvar, 0, 0);
  luaK_patchlist(fs, luaK_jump(fs), bl->relaunchlist);  /* and repeat iteration */
}


static void whilestat (LexState *ls, int line) {
  /* whilestat -> WHILE cond DO block OD */
  FuncState *fs = ls->fs;
  int whileinit, condexit;
  BlockCnt bl;
  luaX_next(ls);  /* skip WHILE */
  whileinit = luaK_getlabel(fs);
  condexit = condassign(ls, WHILESTAT, &whileinit);  /* 2.19.0/2.29.4 extension */
  enterblock(fs, &bl, 1);
  checknext(ls, TK_DO);
  block(ls);
  luaK_patchlist(fs, luaK_jump(fs), whileinit);
  luaK_patchlist(fs, bl.continuelist, whileinit);  /* continue goes to start, too; changed */
  check_match2(ls, TK_OD, TK_END, TK_WHILE, line);  /* changed 0.4.0, changed 2.26.0 */
  leaveblock(fs);
  luaK_patchtohere(fs, condexit);  /* false conditions finish the loop */
}


static void dostat (LexState *ls, int line) {
  /* repeatstat -> DO block { AS cond | UNTIL cond | OD } */
  int condexit, repeat_init;
  BlockCnt bl1, bl2;
  FuncState *fs = ls->fs;
  repeat_init = luaK_getlabel(fs);
  luaX_next(ls);  /* skip DO, 2.22.0 */
  if (testnext(ls, TK_NOTHING)) return;  /* 2.22.0 no operation statement */
  enterblock(fs, &bl1, 1);  /* loop block */
  enterblock(fs, &bl2, 0);  /* scope block */
  chunk(ls);
  luaK_patchtohere(fs, bl1.continuelist);
  if (testnext(ls, TK_OD) || testnext(ls, TK_END)) {  /* changed 2.26.0 */
    expdesc v;
    init_exp(&v, VTRUE, 0);
    luaK_goiffalse(fs, &v);
    condexit = v.t;
  } else {
    int r, init;
    r = check_match2(ls, TK_AS, TK_UNTIL, TK_DO, line);  /* 0.6.0 */
    condexit = (r) ? condassign(ls, MISCSTAT, &init) :  /* read condition (inside scope block), 2.27.0 extension; 2.29.5 */
                     condfalseassign(ls, MISCSTAT, &init);
  }
  if (!bl2.upval) {  /* no upvalues? */
    leaveblock(fs);  /* finish scope */
    luaK_patchlist(fs, condexit, repeat_init);  /* close the loop */
  }
  else {  /* complete semantics when there are upvalues */
    breakstat(ls);  /* if condition is false then break */
    luaK_patchtohere(fs, condexit);  /* else... */
    leaveblock(fs);  /* finish scope... */
    luaK_patchlist(fs, luaK_jump(fs), repeat_init);  /* and repeat */
  }
  leaveblock(fs);  /* finish loop */
}


static int exp1 (LexState *ls) {
  expdesc e;
  int k;
  expr(ls, &e);
  k = e.k;
  luaK_exp2nextreg(ls->fs, &e);
  return k;
}


static int fornumbody (LexState *ls, int base, int line) {
  /* forbody [WHILE cond | UNTIL cond] -> DO block
     WHILE part added 0.12.0, June 14, 2008, tweaked 1.4.0, 21.02.2011
     AS part added 2.0.0 RC 2, 14.11.2013 */
  BlockCnt bl;
  int prep, endfor, iswhile, isuntil, isas, condexit;
  FuncState *fs = ls->fs;
  isas = isuntil = 0;
  adjustlocalvars(ls, 7);  /* internal control variables (registers 0 to 6), 2.3.0 RC 1/2.30.5, grep "FORNUMVARS" */
  fs->bl->redolist = prep = luaK_codeAsBx(fs, OP_FORPREP, base, NO_JUMP);  /* 2.1 RC 1 */
  enterblock(fs, &bl, 0);  /* scope for declared variables */
  if ((iswhile = testnext(ls, TK_WHILE)))  /* 0.12.0 */
    condexit = condassign(ls, MISCSTAT, &prep);  /* 2.27.0/2.29.4 extension */
  else if ((isuntil = testnext(ls, TK_UNTIL)))  /* 2.10.2 */
    condexit = condfalseassign(ls, MISCSTAT, &prep);  /* 2.27.0/2.29.4 extension */
  else
    condexit = 0;
  checknext(ls, TK_DO);
  block(ls);
  if (!iswhile && !isuntil) {  /* extended 2.10.2 */
    if ((isas = testnext(ls, TK_AS)))  /* 2.0 RC 2 */
      condexit = cond(ls);
    else if ((isas = testnext(ls, TK_UNTIL)))
      condexit = condfalse(ls);
  }
  leaveblock(fs);  /* end of scope for declared variables */
  luaK_patchtohere(fs, prep);
  luaK_patchtohere(fs, bl.previous->continuelist);  /* skip, if any, jumps to here, changed */
  endfor = luaK_codeAsBx(fs, OP_FORLOOP, base, NO_JUMP);
  if (iswhile || isas || isuntil)  /* extended 2.10.2 */
    luaK_patchtohere(fs, condexit);  /* false conditions finish the loop */
  luaK_fixline(fs, line);  /* pretend that `OP_FOR' starts the loop */
  luaK_patchlist(fs, endfor, prep + 1);
  return isas;  /* 1 if `as` token has been given to prevent the parser from looking for a closing `od` */
}


static int forinlistbody (LexState *ls, int base, int line, int nvars) {
  /* taken from Lua 5.0.3 to get rid of `for index in PAIRS(tablename) do`; NOT slower than Lua 5.1 !
     WHILE part added 0.12.0, June 14, 2008; UNTIL part added 2.10.2, June 08, 2017 */
  BlockCnt bl;
  FuncState *fs = ls->fs;
  int prep, iswhile, isuntil, isas, whileinit, untilinit, condexit;
  isas = iswhile = isuntil = whileinit = untilinit = 0;
  luaK_codeAsBx(fs, OP_TFORPREP, base, NO_JUMP);
  adjustlocalvars(ls, nvars);  /* scope for all variables */
  if ((iswhile = testnext(ls, TK_WHILE))) {  /* 0.12.0 */
    whileinit = luaK_getlabel(fs);
    condexit = cond(ls);
  } else if ((isuntil = testnext(ls, TK_UNTIL))) {  /* 2.10.2 */
    untilinit = luaK_getlabel(fs);
    condexit = condfalse(ls);
  } else
    whileinit = condexit = untilinit = 0;
  checknext(ls, TK_DO);
  if (iswhile)  /* get proper jump position, 2.10.2 */
    prep = whileinit;
  else if (isuntil)
    prep = untilinit;
  else
    prep = luaK_getlabel(fs);
  fs->bl->redolist = prep;
  enterblock(fs, &bl, 0);  /* changed, loop block */
  block(ls);
  if (!iswhile || !isuntil) {
    if ((isas = testnext(ls, TK_AS)))  /* 2.0 RC 2 */
      condexit = cond(ls);
    else if ((isas = testnext(ls, TK_UNTIL)))
      condexit = condfalse(ls);
  }
  leaveblock(fs);
  luaK_patchtohere(fs, prep - 1);
  luaK_patchtohere(fs, bl.previous->continuelist);  /* skip, if any, jumps to here, changed */
  luaK_codeABC(fs, OP_TFORLOOP, base, 0, nvars);  /* nvars - 3, changed 2.12.2, changed 0.5.0 */
  luaK_fixline(fs, line);  /* pretend that `OP_FOR' starts the loop */
  luaK_patchlist(fs, luaK_jump(fs), prep);  /* changed 0.5.0 */
  if (iswhile || isas || isuntil)  /* 0.12.0, 2.10.2 */
    luaK_patchtohere(fs, condexit);  /* false conditions finish the loop */
  return isas;
}


static int fornum (LexState *ls, TString *varname, int line, int skip, int base) {  /* optimised 1.4.0, extended 2.0.0 RC 2, 14.11.2013 */
  /* fornum -> NAME [{FROM | :=} exp1] [TO exp1] [BY exp1] [WHILE cond] forbody */
  int downto = 0;  /* 0: `downto' token not given, else given */
  FuncState *fs = ls->fs;
  /* make loop control variable accessible in outside loop scope (for optional while condition) */
  if (!skip) { new_localvar(ls, varname, 0, 0, 0); }  /* 7.5.0 change */
  new_localvarliteral(ls, "(for limit)", 1);
  new_localvarliteral(ls, "(for step)", 2);
  /* 0.30.0, prevent roundoff errors using the Kahan algorithm */
  new_localvarliteral(ls, "(for downto)", 3);  /* 2.3.0 RC 1, `downto' step size */
  new_localvarliteral(ls, "(for roff)", 4);    /* correction variable #1 (Kahan-Ozawa) */
  new_localvarliteral(ls, "(for rkb2)", 5);    /* correction variable #2 (Kahan-Babuška only) */
  new_localvarliteral(ls, "(for kbaccu)", 6);  /* internal accumulator (Kahan-Babuška only) */
  /* grep "FORNUMVARS" if you add or delete loop control variables */
  if (!skip) {  /* for var from ..., 7.5.0 change */
    fs->bl->relaunchlist = luaK_getlabel(fs);  /* 2.2.0 RC 4 */
    if (testnext(ls, TK_FROM))
      exp1(ls);  /* initial value */
    else {  /* if `from' keyword is missing, assign loop start value to 1 */
      luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, 1));
      luaK_reserveregs(fs, 1);  /* increment fs->freereg by one */
    }
  }
  if (testnext(ls, TK_TO)) /* changed 0.4.0, 0.12.3, 0.31.0, 2.0.0 RC 2 */
    exp1(ls);  /* limit */
  else if (testnext(ls, TK_DOWNTO)) {
    exp1(ls);
    downto = 1;
  } else {  /* if `to' keyword is missing, assign loop stop value to +infinity */
    luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, HUGE_VAL));
    luaK_reserveregs(fs, 1);  /* increment fs->freereg by one */
  }
  if (testnext(ls, TK_BY)) {  /* changed 0.4.0 */
    exp1(ls);  /* optional step */
  } else {  /* default step = 1 */
    luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, 1));
    luaK_reserveregs(fs, 1); /* increment fs->freereg by one */
  }
  /* 2.3.0 RC 1, set flag for `downto mode' */
  luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, downto));
  luaK_reserveregs(fs, 1);
  /* 0.30.0, set vm variable c to prevent roundoff errors */
  luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, HUGE_VAL));  /* 1.4.0 */
  luaK_reserveregs(fs, 1);
  /* 2.30.5, set Kahan-Babuška vm variable ccs */
  luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, HUGE_VAL));
  luaK_reserveregs(fs, 1);
  /* 2.30.5, set internal Kahan-Babuška vm accumulator */
  luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, HUGE_VAL));
  luaK_reserveregs(fs, 1);
  return fornumbody(ls, base, line);  /* 1.4.0 */
}


static int forcond (LexState *ls, TString *varname, int line) {  /* 3.13.0 */
  /* fornum -> NAME `:=' exp {`,', `while', `until'} cond DO body */
  int whileinit, condexit, base;
  BlockCnt bl;
  expdesc e;
  FuncState *fs = ls->fs;
  base = fs->freereg;
  new_localvar(ls, varname, 0, 0, 0);  /* the loop control variable will be accessible outside the loop scope */
  luaX_next(ls);  /* skip `:=' */
  enterblock(fs, &bl, 1);  /* 7.5.0 change */
  fs->bl->relaunchlist = luaK_getlabel(fs);  /* in numeric for loops, redolist will be handled by fornumbody */
  expr(ls, &e);  /* read initial value */
  if (isnext(ls, TK_TO) || isnext(ls, TK_DOWNTO)) {  /* jump to numeric for loop code, 7.5.0 */
    int rc;
    luaK_exp2nextreg(ls->fs, &e);
    rc = fornum(ls, varname, line, 1, base);
    leaveblock(fs);  /* we must explicitly close the block opened before */
    return rc;
  }
  adjust_assign(ls, 1, 1, &e);
  adjustlocalvars(ls, 1);
  fs->bl->redolist = whileinit = luaK_getlabel(fs);
  if (testnext(ls, TK_WHILE) || testnext(ls, ','))
    condexit = condassign(ls, MISCSTAT, &whileinit);
  else if (testnext(ls, TK_UNTIL))
    condexit = condfalseassign(ls, MISCSTAT, &whileinit);
  else {
    condexit = 0;
    luaX_syntaxerror(ls, LUA_QL("while") ", " LUA_QL("until") " or " LUA_QL(",") " expected");
  }
  checknext(ls, TK_DO);
  block(ls);
  luaK_patchlist(fs, luaK_jump(fs), whileinit);
  luaK_patchlist(fs, bl.continuelist, whileinit);  /* continue goes to start, too; changed */
  leaveblock(fs);
  luaK_patchtohere(fs, condexit);  /* false conditions finish the loop */
  return 0;
}


static int forlist (LexState *ls, TString *indexname, int keysflag) {
  /* forlist -> NAME {, NAME} IN explist1 forbody */
  FuncState *fs = ls->fs;
  expdesc e;
  int nvars, line, base;
  nvars = 0;
  base = fs->freereg;
  /* create control variables */
  new_localvarliteral(ls, "(for generator)", nvars++);
  new_localvarliteral(ls, "(for state)", nvars++);
  new_localvarliteral(ls, "(for control)", nvars++);
  if (ls->t.token != ',' && !keysflag) {  /* iterate values only; changed 0.5.2 */
    new_localvarliteral(ls, "(_$_)", nvars++);  /* (_$_) holds the key if only the value variable has been passed */
  } else if (ls->t.token == ',' && keysflag) {
    luaX_syntaxerror(ls, "only one loop variable accepted with " LUA_QL("keys") " token,");
  }
  /* create declared variables */
  new_localvar(ls, indexname, nvars++, 0, 0);
  while (testnext(ls, ','))
    new_localvar(ls, str_checkname(ls), nvars++, 0, 0);
  checknext(ls, TK_IN);
  line = ls->linenumber;
  fs->bl->relaunchlist = luaK_getlabel(fs);  /* 2.2.0 RC 4 */
  adjust_assign(ls, nvars, explist1(ls, &e), &e);
  luaK_checkstack(fs, 3);  /* extra space to call generator, up to three values after `IN' token will be processed later */
  return forinlistbody(ls, base, line, nvars);
}


static void forstat (LexState *ls, int line) {
  /* forstat -> FOR (fornum | forlist) OD */
  FuncState *fs = ls->fs;
  TString *varname;
  expdesc e;
  BlockCnt bl;
  int keysflag, numloop, isas;
  enterblock(fs, &bl, 1);  /* scope for loop and control variables */
  luaX_next(ls);  /* skip `for' */
  keysflag = (testnext(ls, TK_KEYS)) ? 1 : 0;  /* KEYS token ?, added 0.5.2 */
  /* first variable name; assigns name of loop control variable to expdesc e, as well */
  varname = singlevar(ls, &e);
  numloop = 0;  /* 1 if numeric for loop, 0 if not */
  isas = 0;
  switch (ls->t.token) {
    case TK_FROM: case TK_EQ: case TK_TO: case TK_DO: case TK_WHILE: case TK_UNTIL:  /* 2.27.11 fix */
      if (keysflag)
        luaX_syntaxerror(ls, "cannot mix " LUA_QL("keys") " token with numeric loops.");
      numloop = 1;  /* 1 if for/to loop, 0 if not */
      isas = fornum(ls, varname, line, 0, fs->freereg);
      break;  /* changed 0.4.0, 0.12.3 */
    case ',': case TK_IN:
      isas = forlist(ls, varname, keysflag);
      break;  /* changed 0.5.2 */
    case TK_ASSIGN:  /* 3.13.0 */
      if (keysflag)
        luaX_syntaxerror(ls, "cannot mix " LUA_QL("keys") " token with numeric loops.");
      numloop = 1;
      isas = forcond(ls, varname, line);
      break;
    default:
      luaX_syntaxerror(ls, LUA_QL("from") ", " LUA_QL("to") ", " LUA_QL("in") " or " LUA_QL(":=") " expected");
  }
  if (!isas)  /* 2.0 RC 2, 14.11.2013 */
    check_match2(ls, TK_OD, TK_END, TK_FOR, line); /* changed 0.4.0, changed 2.26.0 */
  if (numloop)  /* loop scope (`break' jumps to this point) */
    /* for/to loops: export last value of iteration variable to surrounding block */
    leaveblocknumloop(fs, varname, &e);
  else
    leaveblock(fs);
}


/* `anonymous` loop, 0.6.0; October 19, 2007 */

static void tostat (LexState *ls, int line) {
  /* tostat -> TO (toexpr) DO ... OD */
  int base;
  FuncState *fs = ls->fs;
  BlockCnt bl;
  luaX_next(ls);  /* skip `to' */
  enterblock(fs, &bl, 1);  /* scope for loop and control variables */
  base = fs->freereg;
  new_localvarliteral(ls, "(for index)", 0);
  new_localvarliteral(ls, "(for limit)", 1);
  new_localvarliteral(ls, "(for step)", 2);
  new_localvarliteral(ls, "(for downto)", 3);  /* 2.3.0 RC 1, `downto' flag */
  new_localvarliteral(ls, "(for roff)", 4);
  new_localvarliteral(ls, "(for rkb2)", 5);    /* ccs correction variable #2 (Kahan-Babuška only) */
  new_localvarliteral(ls, "(for kbaccu)", 6);  /* internal accumulator (Kahan-Babuška only) */
  /* grep "FORNUMVARS" if you add or delete loop control variables */
  luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, 1));  /* from 1 */
  luaK_reserveregs(fs, 1);
  exp1(ls);  /* to limit */
  luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, 1));  /* by 1 */
  luaK_reserveregs(fs, 1);  /* reserve register for step */
  /* set `downto' flag, 2.3.0 RC 1 */
  luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, 0));
  luaK_reserveregs(fs, 1);
  /* do not use Kahan precision as step is assumed to be an integer, to be re-evaluated in lvm.c/OP_FORPREP */
  luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, HUGE_VAL));  /* 1.4.0 */
  luaK_reserveregs(fs, 1);
  /* 2.30.5, set Kahan-Babuška vm variable ccs */
  luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, HUGE_VAL));
  luaK_reserveregs(fs, 1);
  /* 2.30.5, set internal Kahan-Babuška vm accumulator */
  luaK_codeABx(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, HUGE_VAL));
  luaK_reserveregs(fs, 1);
  fornumbody(ls, base, line);  /* 1.4.0 */
  check_match2(ls, TK_OD, TK_END, TK_TO, line); /* changed 0.4.0, changed 2.26.0 */
  leaveblock(fs);  /* loop scope (`break' jumps to this point) */
}


static int test_then_block (LexState *ls) {
  /* test_then_block -> [IF | ELIF] cond THEN block */
  int condexit, init;
  luaX_next(ls);  /* skip IF or ELIF */
  condexit = condassign(ls, IFSTAT, &init);  /* 2.19.0/2.29.4 extension */
  checknext(ls, TK_THEN);
  block(ls);  /* `then' part */
  return condexit;
}


static void ifstat (LexState *ls, int line) {
  /* ifstat -> IF cond THEN block {ELIF cond THEN block} {ONSUCCESS block} [ELSE block] FI */
  FuncState *fs = ls->fs;
  int flist;
  int escapelist = NO_JUMP;
  flist = test_then_block(ls);  /* IF cond THEN block */
  while (ls->t.token == TK_ELIF) {  /* changed Oct. 12, 2006, changed 0.4.0 */
    luaK_concat(fs, &escapelist, luaK_jump(fs));  /* this seems to be a jump if all previous conditions failed */
    luaK_patchtohere(fs, flist);  /* ditto */
    flist = test_then_block(ls);  /* ELIF cond THEN block */
  }
  if (ls->t.token == TK_ONSUCCESS) {  /* added 2.0.0 RC 3 */
    luaK_patchtohere(fs, escapelist);  /* ditto */
    luaX_next(ls);  /* skip ONSUCCESS (after patch, for correct line info) */
    block(ls);  /* `onsuccess' part */
    escapelist = NO_JUMP;
  }
  if (ls->t.token == TK_ELSE) {
    luaK_concat(fs, &escapelist, luaK_jump(fs));  /* this seems to be a jump if all previous conditions failed */
    luaK_patchtohere(fs, flist);  /* ditto */
    luaX_next(ls);  /* skip ELSE (after patch, for correct line info) */
    block(ls);  /* `else' part */
  }
  else {
    luaK_concat(fs, &escapelist, flist);
  }
  luaK_patchtohere(fs, escapelist);
  check_match2(ls, TK_FI, TK_END, TK_IF, line);  /* changed 0.4.0, changed 2.26.0 */
}


/* Author of this Lua extension is Hu Qiwei.

   Taken from the Lua User Group. Modified by Alexander Walz (optional specification of error variable.

   Subject: Re: try..catch in Lua
   From: "Hu Qiwei" <huqiwei>
   Date: Tue, 29 Jan 2008 13:34:14 +0800 */

static void trystat (LexState *ls, int line) {  /* 2.1 RC 2 */
  /* trystat -> TRY block [CATCH [err THEN] block] YRT */
  FuncState *fs = ls->fs;
  BlockCnt bl;
  int pc;
  int escapelist = NO_JUMP;
  luaX_next(ls);
  pc = luaK_codeAsBx(fs, OP_TRY, 0, NO_JUMP);
  enterblock(fs, &bl, 2);  /* try-catch block */
  block(ls);
  leaveblock(fs);
  if (ls->t.token == TK_CATCH) {
    int base;
    luaX_next(ls);  /* skip `catch' */
    luaK_codeABC(fs, OP_ENDTRY, 0, 0, 0);
    luaK_concat(fs, &escapelist, luaK_jump(fs));
    luaK_patchtohere(fs, pc);
    base = fs->freereg;
    enterblock(fs, &bl, 0);
    /* local err or implicit declaration of `lasterror' */
    if (testnext(ls, TK_IN)) {  /* 2.40.1 fix: can't peek into following block with luaX_lookahead */
      new_localvar(ls, str_checkname(ls), 0, 0, 0);  /* register user's error variable name */
      checknext(ls, TK_THEN);
    } else {
      new_localvarliteral(ls, "lasterror", 0);
    }
    adjustlocalvars(ls, 1);  /* control variables */
    luaK_reserveregs(fs, 1);
    luaK_codeABC(fs, OP_CATCH, base, 0, 0);  /* OP_CATCH sets error object to local 'varname' or `lasterror' */
    block(ls);
    leaveblock(fs);  /* catch scope (`break' jumps to this point) */
  }
  else {
    luaK_codeABC(fs, OP_ENDTRY, 0, 0, 0);
    luaK_concat(fs, &escapelist, pc);
  }
  luaK_patchtohere(fs, escapelist);
  check_match2(ls, TK_YRT, TK_END, TK_TRY, line);  /* 2.26.0 */
}


static int test_of_block (LexState *ls) {
  /* test_then_block -> OF cond [THEN | `->` | <void>] block */
  int condexit, whileinit;
  luaX_next(ls);  /* skip OF */
  condexit = condassign(ls, MISCSTAT, &whileinit);  /* 2.19.0/2.29.4 extension */
  checknext2(ls, TK_THEN, TK_ARROW);
  block(ls);  /* `then' part */
  return condexit;
}


static void caseofstat (LexState *ls, int line) {
  /* caseofstat -> CASE OF cond [THEN | '->' | void ] block {OF cond THEN block} {ONSUCCESS block} [ELSE block] ESAC */
  FuncState *fs = ls->fs;
  int flist;
  int escapelist = NO_JUMP;
  flist = test_of_block(ls);  /* OF cond THEN block */
  while (ls->t.token == TK_OF) {
    luaK_concat(fs, &escapelist, luaK_jump(fs));  /* this seems to be a jump if all previous conditions failed */
    luaK_patchtohere(fs, flist);  /* ditto */
    flist = test_of_block(ls);  /* OF cond THEN block */
  }
  if (ls->t.token == TK_ONSUCCESS) {
    luaK_patchtohere(fs, escapelist);  /* ditto */
    luaX_next(ls);  /* skip ONSUCCESS (after patch, for correct line info) */
    block(ls);  /* `onsuccess' part */
    escapelist = NO_JUMP;
  }
  if (ls->t.token == TK_ELSE) {
    luaK_concat(fs, &escapelist, luaK_jump(fs));  /* this seems to be a jump if all previous conditions failed */
    luaK_patchtohere(fs, flist);  /* ditto */
    luaX_next(ls);  /* skip ELSE (after patch, for correct line info) */
    block(ls);  /* `else' part */
    testnext(ls, TK_ESLE);
  }
  else {
    luaK_concat(fs, &escapelist, flist);
  }
  luaK_patchtohere(fs, escapelist);
  check_match2(ls, TK_ESAC, TK_END, TK_CASE, line);  /* changed 2.26.0 */
}


/* written by Andreas Falkenhahn and posted to lua.bazar2.conectiva.com.br on 01 Sep 2004
   changed to process multiple values in 0.6.0, November 10, 2007; patched 0.29.1, December 13, 2009;
   patched 1.9.5, February 25, 2013; patched 2.19.0, 15/04/2020 */

static int ofstat (LexState *ls, expdesc *v, int base) {
  /* [OF exprlist | OF expr [TO expr]] THEN */
  FuncState *fs = ls->fs;
  expdesc v2, v3;
  int nargs;
  /* do not set base to 0 for if case is used in (local) loops, it does not work properly */
  luaK_exp2nextreg(fs, v);
  nargs = explist1(ls, &v2);  /* put every value into a register */
  luaK_exp2nextreg(fs, &v2);
  if (testnext(ls, TK_TO)) {
    if (nargs != 1)
      luaX_syntaxerror(ls, " two values expected in " LUA_QL("case/of/to") " clause");
    expr(ls, &v3);
    luaK_exp2nextreg(fs, &v3);
    luaK_codecompop(fs, base, 2 + 1, v, CMD_AMONG);
    nargs = 2;
  } else
    luaK_codecompop(fs, base, nargs + 1, v, CMD_CASE);
  fs->freereg -= nargs + 1;  /* free all registers */
  if (v->k == VNIL || v->k == VFAIL) v->k = VFALSE;
  luaK_goiftrue(fs, v);
  luaK_patchtohere(fs, v->t);
  checknext2(ls, TK_THEN, TK_ARROW);
  block(ls);
  return v->f;  /* 2.20.0 */
}

#define luaK_jumpto(fs,t)  luaK_patchlist(fs, luaK_jump(fs), t)
/* written by Andreas Falkenhahn and posted to lua.bazar2.conectiva.com.br on 01 Sep 2004 */
static void selectstat (LexState *ls, int line) {
  /* selectstat -> CASE val1 OF val2 [TO val3] THEN block [OF valx [TO valy] THEN block [ELSE THEN block [ESLE]]] ESAC */
  int escapelist, flist, base;
  BlockCnt bl;
  expdesc v;
  FuncState *fs = ls->fs;
  escapelist = NO_JUMP;
  base = fs->freereg;  /* 2.19.0 fix for val1 being a function call */
  enterblock(fs, &bl, 0);  /* block to control variable scope */
  expr(ls, &v);
  checknext(ls, TK_OF);
  flist = ofstat(ls, &v, base);  /* 2.19.0 fix for function calls */
  while (ls->t.token == TK_OF) {
    luaK_concat(fs, &escapelist, luaK_jump(fs));
    luaK_patchtohere(fs, flist);
    luaX_next(ls);  /* skip OF */
    flist = ofstat(ls, &v, base);  /* OF value(list) THEN block */
  }
  if (ls->t.token == TK_ONSUCCESS) {   /* added 2.0.0 RC 3 */
    luaK_patchtohere(fs, escapelist);  /* ditto */
    luaX_next(ls);  /* skip ONSUCCESS (after patch, for correct line info) */
    block(ls);  /* `onsuccess' part */
    escapelist = NO_JUMP;
  }
  if (ls->t.token == TK_ELSE) {
    luaK_concat(fs, &escapelist, luaK_jump(fs));
    luaK_patchtohere(fs, flist);
    luaX_next(ls);  /* skip ELSE */
    block(ls);
    testnext(ls, TK_ESLE);
  } else
    luaK_concat(fs, &escapelist, flist);
  luaK_patchtohere(fs, escapelist);
  check_match2(ls, TK_ESAC, TK_END, TK_CASE, line);
  leaveblock(fs);
}


static void localenumstat (LexState *ls) {  /* added 0.9.0, December 22, 2007 */
  /* LOCAL ENUM <var1> [, var2, ...] [FROM expr] */
  expdesc start, step;
  int i;
  int nvars = 0;
  FuncState *fs = ls->fs;
  do {
    new_localvar(ls, str_checkname(ls), nvars++, 1, 1);  /* enums are constant now */
  } while (testnext(ls, ','));
  if (testnext(ls, TK_FROM)) {
    expr(ls, &start);
  }
  else {
    init_exp(&start, VKNUM, 0);
    start.u.nval = (lua_Number)1;
  }
  /* step size (forward counting) */
  init_exp(&step, VKNUM, 0);
  step.u.nval = (lua_Number)1;
  codearith(fs, OP_SUB, &start, &step);
  /* assign vars */
  for (i=0; i < nvars; i++) {
    codearith(fs, OP_ADD, &start, &step);
    luaK_exp2nextreg(fs, &start);  /* put start on stack */
    luaK_reserveregs(fs, 1);
  }
  adjust_assign(ls, nvars, nvars, &start);
  adjustlocalvars(ls, nvars);
}


/* extended to process mixed declarations, August 02, 2008/October 29, 2008 */
static void localstat (LexState *ls, int mode) {
  /* stat -> LOCAL [CONSTANT] NAME {`,' NAME} [{`:='|`->`} explist1 | IN primaryexp] */
  int nvars, nexps, isatom, i, line;
  expdesc e;
  nvars = 0;
  isatom = 1;  /* flag to detect whether simple vars and structures have been mixed */
  nexps = 0;
  line = ls->linenumber;
  do {
    if (mode == 1) {  /* 2.10.0, CREATE LOCAL <structure> ... */
      if (testnext(ls, TK_TTABLE)) {
        PrepareStructAssign(ls, nvars, isatom);
        tablestat(ls, nvars, VLOCAL);
      }
      else if (testnext(ls, TK_TSET)) {
        PrepareStructAssign(ls, nvars, isatom);
        setstat(ls, nvars, VLOCAL);
      }
      else if (testnext(ls, TK_SEQ) || testnext(ls, TK_TSEQUENCE)) {  /* 1.11.8 */
        PrepareStructAssign(ls, nvars, isatom);
        seqstat(ls, nvars, VLOCAL);
      }
      else if (testnext(ls, TK_DICT)) {
        PrepareStructAssign(ls, nvars, isatom);
        dictstat(ls, nvars, VLOCAL);
      }
      else if (testnext(ls, TK_REG) || testnext(ls, TK_TREGISTER)) {  /* 2.3.0 RC 3 */
        PrepareStructAssign(ls, nvars, isatom);
        regstat(ls, nvars, VLOCAL);
      }
    } else {
      if (mode != 2 && testnext(ls, TK_ENUM)) {  /* LOCAL ENUM vars... */
        if (nvars != 0)
          luaX_syntaxerror(ls, "enums cannot be mixed with other declarations,");
        localenumstat(ls);
        return;  /* no further declarations allowed in same statement */
      } else if (mode != 2 && (isnext(ls, TK_PROC) || isnext(ls, TK_TPROCEDURE))) {  /* 2.75.5 */
        procstat(ls, line, VLOCAL);
        return;  /* no further declarations allowed in same statement */
      } else {  /* simple listing of local variable(s) */
        lu_byte isconst = testnext(ls, TK_CONSTANT);  /* 2.20.0 new */
        new_localvar(ls, str_checkname(ls), nvars++, 1, isconst);
      }
    }
  } while (testnext(ls, ','));
  if (mode == 2) {  /* `with' statement */
    checknext(ls, TK_ASSIGN);
    nexps = explist1(ls, &e);
  } else {
    if (testnext(ls, TK_ASSIGN)) {
      if (isatom) {  /* simple vars and structures have NOT been mixed ?, extended 6.0.0 */
        nexps = explist1(ls, &e);
      } else {  /* issue error */
        nomix;
      }
    } else if (testnext(ls, TK_ARROW)) {  /* multiple assignment statement; added 0.6.0 */
      if (isatom) {
        FuncState *fs = ls->fs;
        expr(ls, &e);
        /* i=1 instead of i=0: e was already put once into a register with the preceding
           expr(ls, &e) statement */
        for (i = 1; i < nvars; i++) {
          luaK_exp2nextreg(fs, &e); /* put e on stack */
          luaK_reserveregs(fs, 1);  /* register it */
        }
        nexps = nvars;
      } else {  /* issue error */
        nomix;
      }
    } else if (testnext(ls, TK_IN)) {  /* local duedo, bonn in zips <=> local duedo, bonn := zips.duedo, zips.bonn */
      /* taken from a Lua 5.1 power patch written by Peter Shook, http://lua-users.org/wiki/PeterShook.
         See: http://lua-users.org/wiki/LuaPowerPatches, Unpack Tables by Name (5.2, 5.1, 5.0.2); 2.8.0 */
      int regs, vars;
      lu_byte with_var;
      FuncState *fs = ls->fs;
      regs = fs->freereg;
      vars = fs->nactvar;
      luaK_reserveregs(fs, nvars);
      adjustlocalvars(ls, nvars);
      new_localvarliteral(ls, "(with)", 0);
      primaryexp(ls, &e, 0, 1);
      luaK_exp2nextreg(fs, &e);
      with_var = fs->nactvar;
      adjustlocalvars(ls, 1);
      luaK_setoneret(fs, &e);  /* close last expression */
      for (nexps=0; nexps < nvars; nexps++) {
        expdesc v, key;
        init_exp(&e, VNONRELOC, fs->freereg - 1);  /* fetch table */
        codestring(ls, &key, getlocvar(fs, vars + nexps).varname);  /* get index name */
        luaK_indexed(fs, &e, &key);  /* invoke indexed value */
        init_exp(&v, VLOCAL, regs + nexps);  /* assign table value to shortcut */
        luaK_storevar(fs, &v, &e);
      }
      removevars(ls, with_var);  /* release table, but don't drop it, it is still on the stack */
      return;
    } else {  /* finalise declaration, 7.7.1 fix so that statement can be completed without an explict semicolon */
      e.k = VVOID;
      nexps = 0;
    }
  }
  if (nvars != 0) {  /* only adjust if there are simple variable declarations */
    adjust_assign(ls, nvars, nexps, &e);
    adjustlocalvars(ls, nvars);
  }
}


static int funcname (LexState *ls, expdesc *v) {  /* reintroduced 2.5.4, taken from Lua 5.1.4 code */
  /* funcname -> NAME {field} [`@@' NAME] */
  int needself = 0;
  singlevar(ls, v);
  while (ls->t.token == '.')
    field(ls, v);
  if (ls->t.token == TK_OOP) {
    needself = 1;
    field(ls, v);
  }
  return needself;
}


static void procstat (LexState *ls, int line, int type) {  /* reintroduced in 2.5.1 as recommended by Slobodan */
  /* procstat -> PROC funcname(arguments) IS body */
  FuncState *fs = ls->fs;
  int needself, islocal;
  expdesc v, b;
  luaX_next(ls);  /* skip PROC */
  islocal = type == VLOCAL;
  if (islocal) {  /* new 2.75.5 */
    new_localvar(ls, str_checkname(ls), 0, 1, 0);
    init_exp(&v, VLOCAL, fs->freereg);
    luaK_reserveregs(fs, 1);
    adjustlocalvars(ls, 1);
  }
  needself = (islocal) ? 0 : funcname(ls, &v);
  body(ls, &b, needself, line);
  luaK_storevar(fs, &v, &b);
  luaK_fixline(fs, line);  /* definition `happens' in the first line */
  /* debug information will only see the variable after this point! */
  if (islocal) getlocvar(fs, fs->nactvar - 1).startpc = fs->pc;
}


static void defstat (LexState *ls, int line, int type) {
  /* procstat -> PROC funcname(arguments) IS body */
  FuncState *fs = ls->fs;
  int needself, islocal;
  expdesc v, b;
  luaX_next(ls);  /* skip DEF */
  islocal = testnext(ls, TK_LOCAL);
  if (islocal) {  /* new 2.75.5 */
    new_localvar(ls, str_checkname(ls), 0, 1, 0);
    init_exp(&v, VLOCAL, fs->freereg);
    luaK_reserveregs(fs, 1);
    adjustlocalvars(ls, 1);
  }
  needself = (islocal) ? 0 : funcname(ls, &v);
  defbody(ls, &b, needself, line);
  luaK_storevar(fs, &v, &b);
  luaK_fixline(fs, line);  /* definition `happens' in the first line */
  /* debug information will only see the variable after this point! */
  if (islocal) getlocvar(fs, fs->nactvar - 1).startpc = fs->pc;
}


static void shortifstat (LexState *ls, expdesc *v) {  /* stat -> `?', 2.14.0 */
  FuncState *fs = ls->fs;
  int flist;
  int escapelist = NO_JUMP;
  if (v->k == VNIL || v->k == VFAIL) v->k = VFALSE;  /* `falses' are all equal here */
  luaK_goiftrue(fs, v);
  flist = v->f;
  statement(ls);  /* read exactly one statement */
  luaK_concat(fs, &escapelist, flist);
  luaK_patchtohere(fs, escapelist);
}


static void shortifnotstat (LexState *ls, expdesc *v) {  /* stat -> `?-', 2.21.3 */
  FuncState *fs = ls->fs;
  int flist;
  int escapelist = NO_JUMP;
  if (v->k == VNIL || v->k == VFAIL) v->k = VFALSE;  /* `falses' are all equal here */
  luaK_goiffalse(fs, v);
  flist = v->t;
  statement(ls);  /* read exactly one statement */
  luaK_concat(fs, &escapelist, flist);
  luaK_patchtohere(fs, escapelist);
}


static void exprstat (LexState *ls) {
  /* stat -> func | assignment */
  int lasttoken, firsttoken;
  FuncState *fs = ls->fs;
  struct LHS_assign v;
  unsigned int indices = fs->freereg;
  lu_byte isconst = testnext(ls, TK_CONSTANT);  /* new 2.20.0 */
  v.prev = NULL;
  v.v.oop = 0;
  v.v.cnst = isconst;
  firsttoken = ls->t.token;
  subexpr(ls, &v.v, 0, 0, &lasttoken, 0, 0);
  if (testnext(ls, TK_QMARK)) {  /* stat -> (expr) `?' (statement) */
    shortifstat(ls, &v.v);
  } else if (testnext(ls, TK_MINUSQMARK)) {  /* stat -> (expr) `?-' (statement), 6.0.0 */
    shortifnotstat(ls, &v.v);
  } else if (lasttoken == TK_PP || lasttoken == TK_MM || firsttoken == TK_PP || firsttoken == TK_MM) {  /* 2.28.0 */
    /* do nothing, don't throw a syntax error */
  } else if (v.v.k == VCALL) {  /* stat -> func, statement moved up again, 2.14.2 */
    SETARG_C(getcode(fs, &v.v), 1);  /* call statement uses no results */
  } else if (isnext(ls, TK_COMPADD)) {  /* 2.14.1 */
    changevalueop(ls, &v.v, 0, OP_ADD, indices, 0);
  } else if (isnext(ls, TK_COMPSUB)) {  /* 2.14.1 */
    changevalueop(ls, &v.v, 0, OP_SUB, indices, 0);
  } else if (isnext(ls, TK_COMPMUL)) {  /* 2.14.1 */
    changevalueop(ls, &v.v, 0, OP_MUL, indices, 0);
  } else if (isnext(ls, TK_COMPDIV)) {  /* 2.14.1 */
    changevalueop(ls, &v.v, 0, OP_DIV, indices, 0);
  } else if (isnext(ls, TK_COMPMOD)) {  /* 2.14.1 */
    changevalueop(ls, &v.v, 0, OP_MOD, indices, 0);
  } else if (isnext(ls, TK_COMPCAT)) {  /* 2.14.11 */
    changevalueop(ls, &v.v, 0, OP_CONCAT, indices, 1);
  } else if (isnext(ls, TK_COMPINTDIV)) {  /* 2.15.0 */
    changevalueop(ls, &v.v, 0, OP_INTDIV, indices, 0);
  } else if (isnext(ls, TK_COMPMULOP)) {  /* 2.29.2 */
    changevalueop(ls, &v.v, 1, OPR_MAP, indices, 0);
  } else if (isnext(ls, TK_COMPBAND)) {  /* 6.0.0 */
    changevalueop(ls, &v.v, 1, OPR_BAND, indices, 0);
  } else if (isnext(ls, TK_COMPBOR)) {  /* 6.0.0 */
    changevalueop(ls, &v.v, 1, OPR_BOR, indices, 0);
  } else if (isnext(ls, TK_COMPBXOR)) {  /* 6.0.0 */
    changevalueop(ls, &v.v, 1, OPR_BXOR, indices, 0);
  } else if (isnext(ls, TK_COMPBLEFT)) {  /* 6.0.0 */
    changevalueop(ls, &v.v, 1, OPR_BLEFT, indices, 0);
  } else if (isnext(ls, TK_COMPBRIGHT)) {  /* 6.0.0 */
    changevalueop(ls, &v.v, 1, OPR_BRIGHT, indices, 0);
  } else if (isnext(ls, TK_COMPLBROT)) {  /* 6.0.0 */
    changevalueop(ls, &v.v, 1, OPR_LBROTATE, indices, 0);
  } else if (isnext(ls, TK_COMPRBROT)) {  /* 6.0.0 */
    changevalueop(ls, &v.v, 1, OPR_RBROTATE, indices, 0);
  } else {  /* stat -> assignment, i.e. var0, var1, ... := ... expressions, or var := expression */
    lu_byte from_var;
    if (assignment(ls, &v, 1, &from_var))
      removevars(ls, from_var);
  }
}


static void globalstat (LexState *ls) {
  /* stat -> GLOBAL NAME {`,' NAME} */
  TString *varname;
  expdesc v;
  FuncState *fs = ls->fs;
  do {  /* changed 2.12.0 RC 1 */
    if (ls->t.token != TK_NAME)
      luaX_lexerror(ls, "name expected", 0);
    varname = singlevar(ls, &v);
    if (searchvar(fs, varname) != -1)
      luaX_syntaxerror(ls,
        luaO_pushfstring(ls->L, "variable " LUA_QS " not global,", getstr(varname)));
  } while (testnext(ls, ','));
}


static void createstat (LexState *ls) {
  /* stat -> CREATE [{table, set, seq}] NAME {`,' [{table, set, seq}] NAME} */
  do {
    if (testnext(ls, TK_LOCAL))
      localstat(ls, 1);
    else if (testnext(ls, TK_TTABLE))
      tablestat(ls, 0, VNIL);
    else if (testnext(ls, TK_TSET))
      setstat(ls, 0, VNIL);
    else if (testnext(ls, TK_SEQ) || testnext(ls, TK_TSEQUENCE))
      seqstat(ls, 0, VNIL);
    else if (testnext(ls, TK_DICT))
      dictstat(ls, 0, VNIL);
    else if (testnext(ls, TK_REG) || testnext(ls, TK_TREGISTER))
      regstat(ls, 0, VNIL);
    else
      luaX_syntaxerror(ls, "keywords " LUA_QL("local") ", " LUA_QL("table") ", "
        LUA_QL("set") ", " LUA_QL("seq") " or " LUA_QL("sequence") " expected");
  } while (testnext(ls, ','));
}


static void retstat (LexState *ls, int flag, int oneret) {  /* flag = 1: `RETURN' statement, else short-cut function */
  /* stat -> RETURN explist */
  FuncState *fs = ls->fs;
  BlockCnt *bl = fs->bl;  /* 2.1 RC 2 */
  expdesc e;
  int first, nret, iswhen, condexit, escapelist;  /* registers with returned values */
  condexit = 0;
  escapelist = NO_JUMP;
  first = nret = iswhen = 0;
  if (flag) {
    luaX_next(ls);  /* skip `RETURN' */
    if ( (iswhen = testnext(ls, TK_WHEN)) )  /* 2.3.3 */
      condexit = cond(ls);
    else if ( (iswhen = testnext(ls, TK_UNLESS)) )  /* 3.10.0 */
      condexit = condfalse(ls);
    else {  /* either `when' or `post` clause (`return when' does not return any results) */
      if (testnext(ls, TK_POST)) {  /* 2.10.5, a little bit faster than checking multiple conditions with multiargop */
        expdesc v;
        FuncState *fs = ls->fs;
        int base = fs->freereg;
        expr(ls, &v);
        luaK_exp2nextreg(fs, &v);
        luaK_codeABC(fs, OP_FN, base, 1, OPR_POST);
        fs->freereg -= 1;
        checknext2(ls, TK_IS, TK_WITH);  /* 2.12.2 */
      }
    }
  }
  if (flag && (block_follow(ls->t.token, 0) || ls->t.token == ';')) {  /* `RETURN' statement ? */
    first = nret = 0;  /* return no values */
  } else if (!flag && block_follow(ls->t.token, 0)) {  /* short-cut procedure ? */
    first = nret = 0;
    luaX_syntaxerror(ls, "at least one return value expected");
  } else {  /* RETURN statement or short-cut procedure */
    if (flag && iswhen) {  /* 2.19.1 fix */
      /* any expression after RETURN WHEN cond found ? ... */
      if (!testnext(ls, TK_WITH)) luaX_syntaxerror(ls, LUA_QL("with") " token expected");  /* ... issue error */
    }
    if (oneret) {
      expr(ls, &e);
      nret = 1;
    } else {
      /* including optional return values, do allow `when` clauses here to prevent speed decreases
         and possible errors */
      nret = explist1(ls, &e);
    }
    if (hasmultret(e.k)) {
      luaK_setmultret(fs, &e);
      if (e.k == VCALL && nret == 1) {  /* tail call? */
        SET_OPCODE(getcode(fs, &e), OP_TAILCALL);
        lua_assert(GETARG_A(getcode(fs, &e)) == fs->nactvar);
      }
      first = fs->nactvar;
      nret = LUA_MULTRET;  /* return all values */
    } else {
      if (nret == 1) {  /* only one single value? */
        first = luaK_exp2anyreg(fs, &e);
      } else {
        luaK_exp2nextreg(fs, &e);  /* values must go to the `stack' */
        first = fs->nactvar;  /* return all `active' values */
        lua_assert(nret == fs->freereg - first);
      }
    }
  }
  /* before return, we should exit all try-catch blocks */
  while (bl) {  /* moved into final else condition, 2.2.2 */
    if (bl->isbreakable == 2)
      luaK_codeABC(fs, OP_ENDTRY, 0, 0, 0);
    bl = bl->previous;
  }
  luaK_ret(fs, first, nret);
  if (iswhen) {  /* RETURN WHEN ... ?  2.3.3 */
    luaK_concat(ls->fs, &escapelist, condexit);
    luaK_patchtohere(ls->fs, escapelist);
  }
  /* return 0 = we are not the last statement in a block; 2.12.1 */
}


static void anchor_struct (FuncState *fs, expdesc *v, int pc) {
  expdesc t;
  init_exp(&t, VRELOCABLE, pc);
  luaK_exp2nextreg(fs, &t);  /* fix it at stack top (for gc) */
  luaK_setoneret(fs, &t);    /* close last expression */
  luaK_storevar(fs, v, &t);
}


static TString *fetchexpr (LexState *ls, expdesc *v) {  /* new 2.21.2 */
  check(ls, TK_NAME);  /* expression must be a name, do not move cursor forward */
  return name_or_indexedname(ls, v);  /* get name or indexed name and move further to next token */
}


static void tablestat (LexState *ls, int nvars, int type) {  /* added 0.5.2, June 07, 2007; extended June 17, 2007,
  extended January 01, 2008; changed 0.12.1; 2.9.6, extended to pre-alloc array and hash part; patched 2.21.2 */
  /* CREATE [LOCAL] TABLE <var1[`(`expr`)`]> */
  FuncState *fs = ls->fs;
  TString *varname;
  expdesc e, v;
  int pc, nargs;
  varname = fetchexpr(ls, &v);  /* 2.21.2 patch to prevent changing constants */
  if (type == VLOCAL) {
    new_localvar(ls, varname, nvars, 1, 0);
    init_exp(&v, VLOCAL, fs->freereg);
    luaK_reserveregs(fs, 1);
    adjustlocalvars(ls, 1);
  }
  if (testnext(ls, '(')) {  /* set initial size for simple table (numeric keys) */
    nargs = explist1(ls, &e);
    luaK_exp2nextreg(fs, &e);
    pc = codearithbypassmularg(fs, nargs, &e, OPR_DIMTAB);
    checknext(ls, ')');
  } else
    pc = luaK_codeABC(fs, OP_NEWTABLE, 0, 0, 0);
  anchor_struct(fs, &v, pc);
}


static void dictstat (LexState *ls, int nvars, int type) {  /* added 0.12.2 */
  /* CREATE [LOCAL] DICT <var1> */
  FuncState *fs = ls->fs;
  TString *varname;
  expdesc e, v;
  int pc;
  varname = fetchexpr(ls, &v);  /* 2.21.2 patch to prevent changing constants */
  if (type == VLOCAL) {
    new_localvar(ls, varname, nvars, 1, 0);
    init_exp(&v, VLOCAL, fs->freereg);
    luaK_reserveregs(fs, 1);
    adjustlocalvars(ls, 1);
  }
  if (testnext(ls, '(')) {  /* set initial size for simple table with numeric keys */
    expr(ls, &e);
    luaK_exp2anyreg(fs, &e);
    pc = codearithbypass(fs, 0, &e, OPR_DIMDICT);
    checknext(ls, ')');
  } else
    pc = luaK_codeABC(fs, OP_NEWTABLE, 0, 0, 0);
  anchor_struct(fs, &v, pc);
}


static void setstat (LexState *ls, int nvars, int type) {  /* added 0.10.0; April 17, 2008; changed 0.12.1 */
  /* CREATE SET <var1> */
  FuncState *fs = ls->fs;
  TString *varname;
  expdesc e, v;
  int pc;
  varname = fetchexpr(ls, &v);  /* 2.21.2 patch to prevent changing constants */
  if (type == VLOCAL) {
    new_localvar(ls, varname, nvars, 1, 0);
    init_exp(&v, VLOCAL, fs->freereg);
    luaK_reserveregs(fs, 1);
    adjustlocalvars(ls, 1);
  }
  if (testnext(ls, '(')) {  /* set initial size for simple table with numeric keys */
    expr(ls, &e);
    luaK_exp2anyreg(fs, &e);
    pc = codearithbypass(fs, 0, &e, OPR_DIMSET);
    checknext(ls, ')');
  } else {
    init_number(fs, &e, 0);
    pc = codearithbypass(fs, 0, &e, OPR_DIMSET);
  }
  anchor_struct(fs, &v, pc);
}


static void seqstat (LexState *ls, int nvars, int type) {  /* added 0.11.0; May 18, 2008; extended 0.12.0, June 30, 2008;
   changed 0.12.1 */
  /* CREATE SEQ <var1> */
  FuncState *fs = ls->fs;
  TString *varname;
  expdesc e, v;
  int pc;
  varname = fetchexpr(ls, &v);  /* 2.21.2 patch to prevent changing constants */
  if (type == VLOCAL) {
    new_localvar(ls, varname, nvars, 1, 0);
    init_exp(&v, VLOCAL, fs->freereg);
    luaK_reserveregs(fs, 1);
    adjustlocalvars(ls, 1);
  }
  if (testnext(ls, '(')) {
    expr(ls, &e);
    luaK_exp2anyreg(fs, &e);
    pc = codearithbypass(fs, 0, &e, OPR_DIMSEQ);
    checknext(ls, ')');
  } else {
    init_number(fs, &e, 0);
    pc = codearithbypass(fs, 0, &e, OPR_DIMSEQ);
  }
  anchor_struct(fs, &v, pc);
}


static void regstat (LexState *ls, int nvars, int type) {  /* 2.3.0 RC 3 */
  /* CREATE REG <var1> */
  FuncState *fs = ls->fs;
  TString *varname;
  expdesc e, v;
  int pc;
  varname = fetchexpr(ls, &v);  /* 2.21.2 patch to prevent changing constants */
  if (type == VLOCAL) {
    new_localvar(ls, varname, nvars, 1, 0);
    init_exp(&v, VLOCAL, fs->freereg);
    luaK_reserveregs(fs, 1);
    adjustlocalvars(ls, 1);
  }
  if (testnext(ls, '(')) {
    expr(ls, &e);
    luaK_exp2anyreg(fs, &e);
    pc = codearithbypass(fs, 0, &e, OPR_DIMREG);
    checknext(ls, ')');
  } else {
    init_number(fs, &e, ls->L->regsize);
    pc = codearithbypass(fs, 0, &e, OPR_DIMREG);
  }
  anchor_struct(fs, &v, pc);
}


/* Used by INC, DEC, MUL, DIV, INTDIV and MOD statements
   added 0.5.2, June 08, 2007; extended November 04, 2007; patched May 10, 2010,
   September 17, 2010 */
static void changevalue (LexState *ls, int op) {
  FuncState *fs = ls->fs;
  expdesc v, e1, e2;
  int bracket;
  unsigned int indices;
  bracket = testnext(ls, '(');  /* 2.16.1 change */
  indices = fs->freereg;
  primaryexp(ls, &v, 0, 1);
  check_constant(ls, &v, NULL, 0);  /* 2.21.1 */
  e1 = v;
  if (v.k == VCALL)  /* 2.5.4 fix */
    luaX_syntaxerror(ls, "first argument must not be a function call,");
  if (v.k == VINDEXED)
    luaK_reserveregs(fs, fs->freereg - indices);  /* 1.0.1 patch: increase stack by the number of indices
       <- this call solved the problem with indexed values */
  if (testnext(ls, ',')) {
    luaK_exp2nextreg(fs, &e1);  /* <- this call solved the problem with indexed values, 0.32.0 */
    expr(ls, &e2);
  } else {  /* using special opcodes is not faster */
    init_exp(&e2, VKNUM, 0);
    e2.u.nval = (lua_Integer)1;
  }
  if (bracket) checknext(ls, ')');
  codearith(fs, (OpCode)op, &e1, &e2);
  luaK_setoneret(fs, &e1);  /* close last expression */
  luaK_storevar(fs, &v, &e1);
}


static void changevalueop (LexState *ls, expdesc *v, int mode, int op, unsigned int indices, int intoreg) {  /* 2.14.1 */
  FuncState *fs = ls->fs;
  expdesc e1, e2;
  check_constant(ls, v, NULL, 1);
  luaX_next(ls);
  e1 = *v;
  if (v->k == VINDEXED)
    luaK_reserveregs(fs, fs->freereg - indices);  /* 1.0.1 patch: increase stack by the number of indices
       <- this call solved the problem with indexed values */
  luaK_exp2nextreg(fs, &e1);  /* <- this call solved the problem with indexed values, 0.32.0 */
  expr(ls, &e2);
  if (intoreg)
    luaK_exp2nextreg(fs, &e2);  /* force string constants onto the stack, as well */
  if (mode == 0)
    codearith(fs, (OpCode)op, &e1, &e2);
  else  /* for MAP, 2.29.2 */
    agnK_codefnbin(fs, &e1, &e2, op);
  luaK_setoneret(fs, v);  /* close last expression */
  luaK_storevar(fs, v, &e1);
}


static void changevaluefn (LexState *ls, int op) {  /* 2.16.1 */
  FuncState *fs = ls->fs;
  expdesc v, e1;
  int bracket;
  unsigned int indices;
  bracket = testnext(ls, '(');
  indices = fs->freereg;
  primaryexp(ls, &v, 0, 1);
  check_constant(ls, &v, NULL, 0);  /* 2.21.1 fix */
  e1 = v;
  if (v.k == VCALL)
    luaX_syntaxerror(ls, "first argument must not be a function call,");
  if (v.k == VINDEXED)
    luaK_reserveregs(fs, fs->freereg - indices);  /* 1.0.1 patch: increase stack by the number of indices
       <- this call solved the problem with indexed values */
  if (bracket) checknext(ls, ')');
  codearithbypass(fs, 0, &e1, (UnOpr)op);
  luaK_setoneret(fs, &e1);  /* close last expression */
  luaK_storevar(fs, &v, &e1);
}


/* expression = 0 -> called by a statement, 1 -> called by expression parsing */
static void plusplus (LexState *ls, expdesc *v, int op, int base, int expression, int fix) {  /* 2.14.9 */
  FuncState *fs = ls->fs;
  expdesc e1, e2;
  int indexed = 0;
  e1 = *v;
  if (e1.k == VCALL || e1.k == VKNUM )  /* 2.23.0/2.30.2 fixes */
    luaX_syntaxerror(ls, "operand must not be a constant or function call,");
  if (e1.k == VINDEXED) {  /* 2.23.0 fix; extended 2.24.1 to fully support indexed names */
    indexed = 1;
    luaK_reserveregs(fs, fs->freereg - base);
    luaK_exp2nextreg(fs, &e1);  /* extended 2.24.1 to support indexed names */
  }
  init_number(fs, &e2, 1);
  codearith(fs, (OpCode)op, &e1, &e2);
  luaK_setoneret(fs, v);  /* close last expression */
  luaK_storevar(fs, v, &e1);  /* in-/decrement variable value, and - 2.15.1 - free expression */
  /* 2.22.0: if called as a statement, we won't throw a value onto the stack for it confuses loops and wastes the stack */
  if (expression) {  /* expression ? */
    if (indexed)  /* 2.24.1 discharge indexed value */
      luaK_dischargevars(fs, v);
    if (fix == OPR_NOBINOPR) {  /* NAME++ (postfix !), 2.28.0 extension */
      init_number(fs, &e2, -1);
      codearith(fs, (OpCode)op, v, &e2);  /* return value that we had before in-/decrementation */
    }
  }
}


static void clearstat (LexState *ls) {  /* added 0.5.3, July 01, 2007; patched April 3, 2009 */
  FuncState *fs = ls->fs;
  expdesc e, v;
  int base;
  testnext(ls, '(');
  do {
    primaryexp(ls, &v, 0, 1);
    check_constant(ls, &v, NULL, 0);
    if (v.k == VCALL) {  /* 2.28.0 fix */
      luaX_lexerror(ls, "argument must not be a function call.", 0);
      return;
    }
    init_exp(&e, VNIL, 0);
    luaK_setoneret(fs, &e);
    luaK_storevar(fs, &v, &e);
  } while (testnext(ls, ','));
  testnext(ls, ')');  /* 0.13.3 */
  base = fs->freereg;
  if (ls->z->reader == getS)  /* 2.23.0, only conduct gc when run in the command-line, not run from file */
    luaK_codeABC(fs, OP_CMD, base, 0, CMD_GC);  /* perform garbage collection */
  fs->freereg = base;
}


static void insertdeletestat (LexState *ls, int cmd) {
  /* added 0.6.0, October 14, 2007; extended to process multiple values on Nov 24, 2007;
     extended to support multiple returns on April 03, 2009 (deprecated); patched May 09, 2010 to work
     with indexed names after the `into' token which it did not in very rare situations; rewritten 2.9.1 */
  int nvals, base;
  expdesc value, table;
  FuncState *fs = ls->fs;
  base = fs->freereg;
  nvals = explist1(ls, &value);
  luaK_exp2nextreg(fs, &value);
  checknext(ls, (cmd == CMD_INSERTINTO) ? TK_INTO : TK_FROM);
  expr(ls, &table);
  luaK_exp2nextreg(fs, &table);
  luaK_codeABC(fs, OP_CMD, base, nvals, cmd);
  fs->freereg = base;
}


static void popstat (LexState *ls) {
  /* added 0.24.0, June 20, 2009 */
  int base, islast;
  expdesc seq;
  FuncState *fs = ls->fs;
  islast = 0;
  base = fs->freereg;
  switch (ls->t.token) {
    case TK_TOP:
      islast = 1;
      luaX_next(ls);
      break;
    case TK_BOTTOM:
      islast = 0;
      luaX_next(ls);
      break;
    default:
      luaX_syntaxerror(ls, LUA_QL("bottom") " or " LUA_QL("top") " expected");
  }
  checknext(ls, TK_FROM);
  primaryexp(ls, &seq, 0, 1);
  luaK_exp2nextreg(fs, &seq);  /* 0.24.1 patch */
  luaK_codeABC(fs, OP_CMD, base, 1, islast ? CMD_POPTOP : CMD_POPBOTTOM);
  fs->freereg = base;
}


static void rotatestat (LexState *ls) {  /* 2.2.0 */
  int base, islast;
  expdesc seq;
  FuncState *fs = ls->fs;
  islast = 0;
  base = fs->freereg;
  switch (ls->t.token) {
    case TK_TOP:
      islast = 1;
      luaX_next(ls);
      break;
    case TK_BOTTOM:
      islast = 0;
      luaX_next(ls);
      break;
    default:
      luaX_syntaxerror(ls, LUA_QL("bottom") " or " LUA_QL("top") " expected");
  }
  primaryexp(ls, &seq, 0, 1);
  luaK_exp2nextreg(fs, &seq);
  luaK_codeABC(fs, OP_CMD, base, 1, islast ? CMD_ROTATETOP : CMD_ROTATEBOTTOM);
  fs->freereg = base;
}


static void exchangestat (LexState *ls) {  /* 2.2.0 */
  int base;
  expdesc seq;
  FuncState *fs = ls->fs;
  base = fs->freereg;
  primaryexp(ls, &seq, 0, 1);
  luaK_exp2nextreg(fs, &seq);
  luaK_codeABC(fs, OP_CMD, base, 1, CMD_EXCHANGE);
  fs->freereg = base;
}


static void duplicatestat (LexState *ls) {  /* 2.2.0 */
  int base;
  expdesc seq;
  FuncState *fs = ls->fs;
  base = fs->freereg;
  primaryexp(ls, &seq, 0, 1);
  luaK_exp2nextreg(fs, &seq);
  luaK_codeABC(fs, OP_CMD, base, 1, CMD_DUPLICATE);
  fs->freereg = base;
}


static void stackdstat (LexState *ls, int op) {  /* 2.14.2 */
  int nvals, base, bracket;
  expdesc v;
  FuncState *fs = ls->fs;
  base = fs->freereg;
  luaX_next(ls);  /* skip command */
  bracket = testnext(ls, '(');
  nvals = opvmargexplist(ls, &v);  /* changed 2.9.7 */
  if (op == CMD_PUSHD && nvals == 0)
    luaX_syntaxerror(ls, " at least one argument expected");
  luaK_exp2nextreg(fs, &v);
  luaK_codeABC(fs, OP_CMD, base, nvals, op);
  if (bracket) checknext(ls, ')');
  fs->freereg = base;
}


static void switchdstat (LexState *ls) {
  int base, bracket;
  expdesc v;
  FuncState *fs = ls->fs;
  base = fs->freereg;
  luaX_next(ls);
  bracket = testnext(ls, '(');
  expr(ls, &v);
  luaK_exp2nextreg(fs, &v);
  if (bracket) checknext(ls, ')');
  luaK_codeABC(fs, OP_CMD, base, 1, CMD_SWITCHD);
  fs->freereg = base;
}


static void importstat (LexState *ls) {  /* 2.0.0, November 06 & 11, 2013 */
  int nvals, base;
  expdesc str;
  FuncState *fs = ls->fs;
  base = fs->freereg;
  nvals = 0;  /* 3.0.0 patch */
  init_exp(&str, VNIL, 0);  /* slot for procedure called in VM */
  luaK_exp2nextreg(fs, &str);
  do {
    checkname(ls, &str);
    luaK_exp2nextreg(fs, &str);
    nvals++;
  } while (testnext(ls, ','));
  if (ls->t.token == TK_ALIAS) {
    if (nvals > 1)
      luaX_syntaxerror(ls, "only one package name accepted if " LUA_QL("alias") " given");
    else {
      luaX_next(ls);
      if (ls->t.token == TK_NAME) {
        do {
          checkname(ls, &str);
          luaK_exp2nextreg(fs, &str);
          nvals++;
        } while (testnext(ls, ','));
      } else {  /* else call `with` with only the package name */
        init_exp(&str, VFALSE, 0);  /* do not print anything on screen */
        luaK_exp2nextreg(fs, &str);
        nvals++;
      }
      luaK_codeABC(fs, OP_CMD, base, nvals + 1, CMD_ALIAS);  /* 3.0.0 patch */
    }
  } else if (ls->t.token == TK_AS) {  /* IMPORT <packagename> AS <aliasname>, 2.12.0 RC 1 */
    if (nvals > 1)
      luaX_syntaxerror(ls, "only one package name accepted if " LUA_QL("as") " given");
    else {
      expdesc e, e1;
      luaX_next(ls);
      if (ls->t.token == TK_NAME) {
        primaryexp(ls, &e, 0, 1);
        nvals++;
      } else
        luaX_syntaxerror(ls, "expected a name after " LUA_QL("as") " clause");
      luaK_codeABC(fs, OP_CMD, base, nvals + 1, CMD_IMPORTAS);  /* 3.0.0 patch */
      init_exp(&e1, VNONRELOC, fs->freereg - 2);  /* make it global */
      luaK_setoneret(fs, &e1);  /* close last expression */
      luaK_storevar(fs, &e, &e1);  /* <aliasname> := <packagename> */
    }
  } else {
    init_exp(&str, VFALSE, 0);
    luaK_exp2nextreg(fs, &str);
    nvals++;
    luaK_codeABC(fs, OP_CMD, base, nvals + 1, CMD_IMPORT);  /* 3.0.0 patch */
  }
  fs->freereg = base;
}


void enumglobalstat (LexState *ls, struct LHS_assign *lh, int nvars) {
  check_or_make_readonly(ls, &lh->v);  /* new 2.20.0 */
  if (testnext(ls, ',')) {
    struct LHS_assign v;
    v.prev = lh;
    primaryexp(ls, &v.v, 0, 1);  /* 7.5.0 change */
    luaY_checklimit(ls->fs, nvars, LUAI_MAXCCALLS - ls->L->nCcalls, "variable names");
    enumglobalstat(ls, &v, nvars + 1);
  } else {
    expdesc start, step, offset;
    struct LHS_assign *varname;
    FuncState *fs = ls->fs;
    if (testnext(ls, TK_FROM)) {
      expr(ls, &start);
    } else {
      init_exp(&start, VKNUM, 0);
      start.u.nval = (lua_Number)1;
    }
    /* since loop count is backwards, increase start value by nvars */
    init_exp(&offset, VKNUM, 0);
    offset.u.nval = (lua_Number)(nvars);
    codearith(fs, OP_ADD, &start, &offset);
    luaK_exp2nextreg(fs, &start);  /* 0.32.1, this line solved the problem with indexed names following the `from' keyword */
    /* step size (backward counting) */
    init_exp(&step, VKNUM, 0);
    step.u.nval = (lua_Number)(-1);
    for (varname=lh; ; varname = varname->prev) {  /* traverse lh backwards ! */
      codearith(fs, OP_ADD, &start, &step);
      luaK_exp2nextreg(fs, &start);
      luaK_reserveregs(fs, 1);
      luaK_storevar(fs, &varname->v, &start);
      if (varname->prev == NULL) break;  /* cannot be included into the `to' clause of the for loop */
    }
  }
}


static void enumstat (LexState *ls) {
  struct LHS_assign v;
  /* global enum vars */
  primaryexp(ls, &v.v, 0, 1);
  v.prev = NULL;
  enumglobalstat(ls, &v, 1);
}


static void byestat (LexState *ls) {
  luaK_codeABC(ls->fs, OP_CMD, ls->fs->freereg, 0, CMD_BYE);
}


static void clsstat (LexState *ls) {
  luaK_codeABC(ls->fs, OP_CMD, ls->fs->freereg, 0, CMD_CLS);
}


static void restartstat (LexState *ls) {
  int base;
  FuncState *fs = ls->fs;
  base = fs->freereg;
  luaK_codeABC(fs, OP_CMD, base, 0, CMD_RESTART);
  fs->freereg = base;
}


static void withstat (LexState *ls, int line) {
  /* stat -> (WITH NAME {`,' NAME} {IN | := } primaryexp [, ...] DO | WITH primaryexp DO); */
  int nexps, nvars, regs, vars, lookahead, within, tableat;
  lu_byte with_var;
  expdesc e;
  BlockCnt bl;
  FuncState *fs = ls->fs;
  luaX_next(ls);  /* skip `with' */
  nvars = within = regs = vars = tableat = with_var = 0;
  enterblock(fs, &bl, 0);  /* scope variables, non-breakable */
  lookahead = luaX_lookahead(ls);
  if (lookahead == TK_DO || lookahead == TK_SCOPE || lookahead == TK_BEGIN) {  /* 2.8.3, WITH var DO ? */
    expdesc v;
    new_localvarliteral(ls, "_", 0);  /* table */
    adjustlocalvars(ls, 1);
    primaryexp(ls, &e, 0, 1);  /* table */
    luaK_exp2nextreg(fs, &e);
    luaK_setoneret(fs, &e);  /* close last expression */
    init_exp(&v, VLOCAL, fs->freereg - 1);  /* assign table reference to `_' */
    luaK_storevar(fs, &v, &e);
  } else {
    /* WITH <sequence of names> IN <tablename> DO or WITH <sequence of names> `:=' <sequence of values> DO */
    do {
      new_localvar(ls, str_checkname(ls), nvars++, 0, 0);
    } while (testnext(ls, ','));
    if (testnext(ls, TK_ASSIGN)) {  /* 2.10.0, WITH <sequence of names> `:=' <sequence of values> DO */
      nexps = explist1(ls, &e);
      adjust_assign(ls, nvars, nexps, &e);
      adjustlocalvars(ls, nvars);
    } else if (testnext(ls, TK_IN)) {  /* 2.23.0, WITH <sequence of names> IN <tablename> DO */
      within = 1;
      regs = fs->freereg;
      vars = fs->nactvar;
      luaK_reserveregs(fs, nvars);  /* declare all variables in <sequence of names> local */
      adjustlocalvars(ls, nvars);
      new_localvarliteral(ls, "(with)", 0);  /* create internal reference to table */
      if (ls->t.token != TK_NAME)  /* table name */
        luaX_lexerror(ls, "expected a name", 0);
      primaryexp(ls, &e, 0, 1);  /* table */
      luaK_exp2nextreg(fs, &e);
      tableat = fs->freereg - 1;  /* remember position of internal reference to table */
      with_var = fs->nactvar;
      adjustlocalvars(ls, 1);  /* register as local variable */
      luaK_setoneret(fs, &e);  /* close last expression */
      for (nexps=0; nexps < nvars; nexps++) {  /* assign table values to shortcuts */
        expdesc v, key;
        init_exp(&e, VNONRELOC, tableat);  /* table */
        codestring(ls, &key, getlocvar(fs, vars + nexps).varname);  /* read respective key */
        luaK_indexed(fs, &e, &key);          /* get indexed value */
        init_exp(&v, VLOCAL, regs + nexps);  /* initialise new local variable in the stack and */
        luaK_storevar(fs, &v, &e);           /* assign indexed value to the local variable */
      }
    } else
      luaX_lexerror(ls, "expected `in` or `:=` token", 0);
  }
  if (!(testnext(ls, TK_SCOPE) || testnext(ls, TK_DO) || testnext(ls, TK_BEGIN)))
    luaX_lexerror(ls, "expected `scope` or `do` token", 0);
  chunk(ls);
  check_match3(ls, TK_EPOCS, TK_OD, TK_END, TK_WITH, line);
  if (within) {
    /* WITH <sequence of names> IN <tablename> DO: write values in all local shortcuts back to table, 2.23.0 */
    for (nexps=0; nexps < nvars; nexps++) {
      expdesc v, key;
      init_exp(&e, VNONRELOC, tableat);    /* fetch internal table reference */
      codestring(ls, &key, getlocvar(fs, vars + nexps).varname);  /* get respective key */
      luaK_indexed(fs, &e, &key);          /* get indexed value */
      init_exp(&v, VLOCAL, regs + nexps);  /* fetch value of local variable and */
      luaK_storevar(fs, &e, &v);           /* write back value of local variable to the table */
    }
    removevars(ls, with_var);              /* remove local variable */
    fs->freereg = tableat;                 /* drop local table reference, level the stack, 7.5.0 fix */
  }
  leaveblock(fs);
  return;
}


/* return 1: must be last statement in a chunk, 0: must not. */
static int statement (LexState *ls) {
  int line = ls->linenumber;  /* may be needed for error messages */
  switch (ls->t.token) {
    case TK_IF: {  /* stat -> ifstat */
      ifstat(ls, line);
      return 0;
    }
    case TK_WHILE: {  /* stat -> whilestat */
      whilestat(ls, line);
      return 0;
    }
    case TK_DO: {  /* stat -> dostat, changed 0.5.3 */
      dostat(ls, line);
      return 0;
    }
    case TK_FOR: {  /* stat -> forstat */
      forstat(ls, line);
      return 0;
    }
    case TK_LOCAL: {  /* stat -> localstat */
      luaX_next(ls);  /* skip LOCAL */
      localstat(ls, 0);
      return 0;
    }
    case TK_RET: {  /* changed 0.5.2, stat -> retstat */
      retstat(ls, 1, 0);  /* 2.12.1, 2.3.3 */
      return 0;
    }
    case TK_BREAK: {  /* stat -> breakstat */
      luaX_next(ls);  /* skip BREAK */
      return !breakstat(ls);  /* must be last statement if no `when' clause has been given */
    }
    case TK_SKIP: case TK_NEXT: {   /* stat -> skipstat, changed  */
      luaX_next(ls);  /* skip SKIP, NEXT */
      return !skipstat(ls);  /* must be last statement if no `when' clause has been given */
    }
    case TK_REDO: {   /* stat -> redostat, changed  */
      luaX_next(ls);  /* skip REDO */
      redostat(ls);
      return 1;       /* must be last statement */
    }
    case TK_RELAUNCH: {  /* stat -> relaunchstat, changed  */
      luaX_next(ls);     /* skip RELAUNCH */
      relaunchstat(ls);
      return 1;          /* must be last statement */
    }
    case TK_CREATE: {  /* added 0.12.2, September 13, 2008 */
      luaX_next(ls);   /* skip SET */
      createstat(ls);
      return 0;
    }
    case TK_INC: {  /* added 0.5.2.*/
      luaX_next(ls);
      changevalue(ls, OP_ADD);
      return 0;
    }
    case TK_DEC: {  /* added 0.5.2.*/
      luaX_next(ls);
      changevalue(ls, OP_SUB);
      return 0;
    }
    case TK_MUL: {  /* added 2.2.0.*/
      luaX_next(ls);
      changevalue(ls, OP_MUL);
      return 0;
    }
    case TK_DIV: {  /* added 2.2.0.*/
      luaX_next(ls);
      changevalue(ls, OP_DIV);
      return 0;
    }
    case TK_MOD: {  /* added 2.9.0.*/
      luaX_next(ls);
      changevalue(ls, OP_MOD);
      return 0;
    }
    case TK_INTDIV: {  /* added 2.9.7.*/
      luaX_next(ls);
      changevalue(ls, OP_INTDIV);
      return 0;
    }
    case TK_CASE: {  /* added 0.5.2.*/
      luaX_next(ls);  /* skip `case' */
      if (ls->t.token == TK_OF)  /* 2.9.0 */
        caseofstat(ls, line);
      else
        selectstat(ls, line);
      return 0;
    }
    case TK_INSERT: {  /* added 0.6.0, stat -> insertstat */
      luaX_next(ls);
      insertdeletestat(ls, CMD_INSERTINTO);
      return 0;
    }
    case TK_DELETE: {  /* added 0.6.0, stat -> deletestat */
      luaX_next(ls);
      insertdeletestat(ls, CMD_DELETEFROM);
      return 0;
    }
    case TK_CLEAR: {  /* added 0.5.2.*/
      luaX_next(ls);
      clearstat(ls);
      return 0;
    }
    case TK_IMPORT: {  /* added 2.0.0 */
      luaX_next(ls);
      importstat(ls);
      return 0;
    }
    case TK_ENUM: {  /* added 0.6.0 */
      luaX_next(ls);
      enumstat(ls);
      return 0;
    }
    case TK_SCOPE:  {  /* stat -> SCOPE block EPOCS */
      luaX_next(ls);  /* skip SCOPE */
      block(ls);
      check_match2(ls, TK_EPOCS, TK_END, TK_SCOPE, line);
      return 0;
    }
    case TK_BEGIN:  {  /* stat -> BEGIN block END, 7.0.0 */
      luaX_next(ls);  /* skip SCOPE */
      block(ls);
      check_match(ls, TK_END, TK_BEGIN, line);
      return 0;
    }
    case TK_POP: {
      luaX_next(ls);  /* skip POP */
      popstat(ls);
      return 0;
    }
    case TK_ROTATE: {  /* added 2.2.0 */
      luaX_next(ls);  /* skip ROTATE */
      rotatestat(ls);
      return 0;
    }
    case TK_DUPLICATE: {  /* added 2.2.0 */
      luaX_next(ls);  /* skip DUPLICATE */
      duplicatestat(ls);
      return 0;
    }
    case TK_EXCHANGE: {  /* added 2.2.0 */
      luaX_next(ls);  /* skip EXCHANGE */
      exchangestat(ls);
      return 0;
    }
    case TK_GLOBAL: {  /* stat -> localstat */
      luaX_next(ls);  /* skip GLOBAL */
      globalstat(ls);
      return 0;
    }
    case TK_TO: {  /* stat -> tostat */
      tostat(ls, line);
      return 0;
    }
    case TK_CLS: {  /* added 0.4.0 */
      clsstat(ls);
      luaX_next(ls);
      return 0;  /* changed so that the cls statement can be included in chunks. 2.5.4 */
    }
    case TK_BYE: {
      byestat(ls);
      luaX_next(ls);
      return 0;  /* changed so that the bye statement can be included in chunks. 2.5.4 */
    }
    case TK_RESTART: {
      restartstat(ls);
      luaX_next(ls);
      return 1;
    }
    case TK_TRY: {
      trystat(ls, line);
      return 0;
    }
    case TK_PROC:
    case TK_TPROCEDURE: {  /* PROC funcname(args) IS */
      procstat(ls, line, VGLOBAL);  /* stat -> procstat */
      return 0;
    }
    case TK_WITH: {  /* stat -> withstat */
      withstat(ls, line);
      return 0;
    }
    case TK_NEGATE: {  /* added 2.16.1 */
      luaX_next(ls);
      changevaluefn(ls, OPR_NEGATE);
      return 0;
    }
    case TK_PUSHD: {  /* added 2.9.4 */
      stackdstat(ls, CMD_PUSHD);
      return 0;
    }
    case TK_SWITCHD: {  /* added 2.9.7 */
      switchdstat(ls);
      return 0;
    }
    case TK_DEF: case TK_DEFINE: {  /* added 2.29.1/2.30.2 */
      defstat(ls, line, VGLOBAL);
      return 0;
    }
    default: {
      exprstat(ls);  /* func | assignment */
      return 0;  /* to avoid warnings */
    }
  }
}


/* evaluate short-cut function statement, added 0.5.2 */

static void fnchunk (LexState *ls, int oneret) {
  /* chunk -> { exprlist } */
  enterlevel(ls);
  retstat(ls, 0, oneret);
  lua_assert(ls->fs->f->maxstacksize >= ls->fs->freereg &&
             ls->fs->freereg >= ls->fs->nactvar);
  ls->fs->freereg = ls->fs->nactvar;  /* free registers */
  leavelevel(ls);
}


static void chunk (LexState *ls) {
  /* chunk -> { stat [`;'] } */
  int islast = 0;
  enterlevel(ls);
  while (!islast && !block_follow(ls->t.token, 0)) {
    islast = statement(ls);
    testnext(ls, ';');
    lua_assert(ls->fs->f->maxstacksize >= ls->fs->freereg &&
               ls->fs->freereg >= ls->fs->nactvar);
    ls->fs->freereg = ls->fs->nactvar;  /* free registers */
  }
  leavelevel(ls);
}

/* }====================================================================== */

