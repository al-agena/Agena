/*
** $Id: ldebug.c,v 2.29 2005/12/22 16:19:56 roberto Exp $
** Debug Interface
** See Copyright Notice in agena.h
*/


#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#define ldebug_c
#define LUA_CORE

#include "agena.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lopcodes.h"
#include "lstate.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"


static const char *getfuncname (lua_State *L, CallInfo *ci, const char **name);


static int currentpc (lua_State *L, CallInfo *ci) {
  if (!isLua(ci)) return -1;  /* function is not a Lua function? */
  if (ci == L->ci)
    ci->savedpc = L->savedpc;
  return pcRel(ci->savedpc, ci_func(ci)->l.p);
}


static int currentline (lua_State *L, CallInfo *ci) {
  int pc = currentpc(L, ci);
  if (pc < 0)
    return -1;  /* only active lua functions have current-line information */
  else
    return getLine(ci_func(ci)->l.p, pc);
}


/*
** this function can be called asynchronous (e.g. during a signal)
*/
LUA_API int lua_sethook (lua_State *L, lua_Hook func, int mask, int count) {
  if (func == NULL || mask == 0) {  /* turn off hooks? */
    mask = 0;
    func = NULL;
  }
  L->hook = func;
  L->basehookcount = count;
  resethookcount(L);
  L->hookmask = cast_byte(mask);
  return 1;
}


LUA_API lua_Hook lua_gethook (lua_State *L) {
  return L->hook;
}


LUA_API int lua_gethookmask (lua_State *L) {
  return L->hookmask;
}


LUA_API int lua_gethookcount (lua_State *L) {
  return L->basehookcount;
}


/* Get information about the interpreter runtime stack.
   This function fills parts of a lua_Debug structure with an identification of the activation record of the function executing
   at a given level. Level 0 is the current running function, whereas level n+1 is the function that has called level n.
   When there are no errors, lua_getstack returns 1; when called with a level greater than the stack depth, it returns 0. */
LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar) {
  int status;
  CallInfo *ci;
  lua_lock(L);
  for (ci = L->ci; level > 0 && ci > L->base_ci; ci--) {
    level--;
    if (f_isLua(ci))  /* Lua function? */
      level -= ci->tailcalls;  /* skip lost tail calls */
  }
  if (level == 0 && ci > L->base_ci) {  /* level found ? */
    status = 1;
    ar->i_ci = cast_int(ci - L->base_ci);
  } else if (level < 0) {  /* level is of a lost tail call ? */
    status = 1;
    ar->i_ci = 0;
  } else
    status = 0;  /* no such level */
  lua_unlock(L);
  return status;
}


static Proto *getluaproto (CallInfo *ci) {
  return (isLua(ci) ? ci_func(ci)->l.p : NULL);
}


static const char *findlocal (lua_State *L, CallInfo *ci, int n) {
  const char *name;
  Proto *fp = getluaproto(ci);
  if (fp && (name = luaF_getlocalname(fp, n, currentpc(L, ci))) != NULL)
    return name;  /* is a local variable in a Lua function */
  else {
    StkId limit = (ci == L->ci) ? L->top : (ci + 1)->func;
    if (limit - ci->base >= n && n > 0)  /* is 'n' inside 'ci' stack? */
      return "(*temporary)";
    else
      return NULL;
  }
}


LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n) {
  CallInfo *ci = L->base_ci + ar->i_ci;
  const char *name = findlocal(L, ci, n);
  lua_lock(L);
  if (name)
    luaA_pushobject(L, ci->base + (n - 1));
  lua_unlock(L);
  return name;
}


LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n) {
  CallInfo *ci = L->base_ci + ar->i_ci;
  const char *name = findlocal(L, ci, n);
  lua_lock(L);
  if (name)
    setobjs2s(L, ci->base + (n - 1), L->top - 1);
  L->top--;  /* pop value */
  lua_unlock(L);
  return name;
}


/* get arity of a function (numbers of parameters) at current level */
LUA_API int lua_getarity (lua_State *L, const lua_Debug *ar, int *varargs) {  /* 2.12.1 */
  CallInfo *ci = L->base_ci + ar->i_ci;
  Proto *p = getluaproto(ci);
  if (p == NULL) return -1;  /* 2.14.10 fix */
  *varargs = (int)p->is_vararg;
  return p->numparams;  /* 2.14.10 fix */
}


/* get arity of a function (numbers of parameters) at top of the stack */
LUA_API int lua_arity (lua_State *L, lua_Debug *ar) {  /* 2.12.1 */
  int status = 1;
  Closure *f = NULL;
  lua_lock(L);
  if (L->top <= L->base) {  /* no function arguments ?, 7.7.8 fix to prevent invalid reads. */
    ar->arity = ar->hasvararg = 0;
  } else {
    StkId func = L->top - 1;
    if ( (status = ttisfunction(func)) ) {
      f = clvalue(func);
      /* 7.7.8 fix: do not corrupt the stack by running: L->top--; ! There is no need to do that */
      if (f->c.isC) {
        ar->arity = ar->hasvararg = -1;
      } else {
        struct Proto *p = f->l.p;
        ar->arity = p->numparams;  /* changed 2.10.4 */
        ar->hasvararg = p->is_vararg;
        status = 0;
      }
    }
  }
  lua_unlock(L);
  return status;  /* 0 = success */
}


static void funcinfo (lua_Debug *ar, Closure *cl) {
  if (cl->c.isC) {
    ar->source = "=[C]";
    ar->linedefined = -1;
    ar->lastlinedefined = -1;
    ar->what = "C";
  }
  else {
    ar->source = getstr(cl->l.p->source);
    ar->linedefined = cl->l.p->linedefined;
    ar->lastlinedefined = cl->l.p->lastlinedefined;
    ar->what = (ar->linedefined == 0) ? "main" : "Agena";
  }
  luaO_chunkid(ar->short_src, ar->source, LUA_IDSIZE);
}


static void info_tailcall (lua_Debug *ar) {
  ar->name = ar->namewhat = "";
  ar->what = "tail";
  ar->lastlinedefined = ar->linedefined = ar->currentline = -1;
  ar->source = "=(tail call)";
  luaO_chunkid(ar->short_src, ar->source, LUA_IDSIZE);
  ar->nups = 0;
}


static void collectvalidlines (lua_State *L, Closure *f) {
  if (f == NULL || f->c.isC) {
    setnilvalue(L->top);
  }
  else {
    Table *t = luaH_new(L, 0, 0);
    int *lineinfo = f->l.p->lineinfo;
    int i;
    for (i=0; i<f->l.p->sizelineinfo; i++)
      setbvalue(luaH_setnum(L, t, lineinfo[i]), 1);
    sethvalue(L, L->top, t);
  }
  incr_top(L);
}


static int auxgetinfo (lua_State *L, const char *what, lua_Debug *ar,
                    Closure *f, CallInfo *ci) {
  int status = 1;
  if (f == NULL) {
    info_tailcall(ar);
    return status;
  }
  for (; *what; what++) {
    switch (*what) {
      case 'S': {
        funcinfo(ar, f);
        break;
      }
      case 'l': {
        ar->currentline = (ci) ? currentline(L, ci) : -1;
        break;
      }
      case 'u': {
        ar->nups = f->c.nupvalues;
        break;
      }
      case 'n': {
        ar->namewhat = (ci) ? getfuncname(L, ci, &ar->name) : NULL;
        if (ar->namewhat == NULL) {
          ar->namewhat = "?";  /* not found, changed 2.10.4, formerly set to the empty string */
          ar->name = NULL;
        }
        break;
      }
      case 'a': {  /* number of arguments expected, arity = `Stelligkeit`, 2.1.1, Rob Hoelz' LuaPowerPatch */
        if (f->c.isC)
          ar->arity = -1;
        else {
          struct Proto *p = f->l.p;
          ar->arity = p->numparams;  /* changed 2.10.4 */
        }
        break;
      }
      case 'v':
        ar->hasvararg = f->l.p->is_vararg;
      case 'g':  /* PATCH - checkglobals, by David Manura, 2008 */
      case 'L':
      case 'V':
      case 'c':
      case 'f':  /* handled by lua_getinfo */
      case 'k':  /* handled by lua_getinfo */
        break;
      default: status = 0;  /* invalid option */
    }
  }
  return status;
}


/* PATCH - checkglobals, by David Manura, 2008.  */
static void auxgetinfoglobals (lua_State *L, Proto *p, Table *t, int *c) {
  TValue *k = p->k;
  int j;
  for (j = 0; j < p->sizecode; j++) {
    const Instruction i = p->code[j];
    OpCode op = GET_OPCODE(i);
    const TValue *ts;
    if (op != OP_GETGLOBAL && op != OP_SETGLOBAL)
      continue;
    ts = k + GETARG_Bx(i);
    lua_assert(ttisstring(ts));
    setobj2t(L, luaH_setnum(L, t, (*c)++),
             (L->top - (OP_GETGLOBAL ? 2 : 1)))
    setobj2t(L, luaH_setnum(L, t, (*c)++), ts);
    if (p->lineinfo) {
      setnvalue(luaH_setnum(L, t, (*c)++), p->lineinfo[j]);
    }
  }
  for (j=0; j < p->sizep; j++) {  /* lexically nested functions */
    auxgetinfoglobals(L, p->p[j], t, c);
  }
}


LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar) {
  int status;
  Closure *f = NULL;
  CallInfo *ci = NULL;
  lua_lock(L);
  if (*what == '>') {  /* we are a function */
    StkId func = L->top - 1;
    luai_apicheck(L, ttisfunction(func));
    what++;  /* skip the '>' */
    f = clvalue(func);
    L->top--;  /* pop function */
  }
  else if (ar->i_ci != 0) {  /* no tail call? */
    ci = L->base_ci + ar->i_ci;
    lua_assert(ttisfunction(ci->func));
    f = clvalue(ci->func);
  }
  status = auxgetinfo(L, what, ar, f, ci);
  if (strchr(what, 'f')) {
    if (f == NULL) setnilvalue(L->top);
    else setclvalue(L, L->top, f);
    incr_top(L);
  }
  if (strchr(what, 'L'))
    collectvalidlines(L, f);
  /* PATCH - checkglobals, by David Manura, 2008, patched by Alexander Walz to avoid crashes
     with some C functions */
  if (strchr(what, 'g')) {
    lua_newtable(L);
    if (f != NULL && !f->c.isC) {
      Table *t = hvalue(L->top - 1);
      int c = 1;
      lua_pushnumber(L, f->l.p->lineinfo ? 3 : 2);
      lua_setfield(L, -2, "ncols");
      lua_pushliteral(L, "GETGLOBAL");
      lua_pushliteral(L, "SETGLOBAL");
      auxgetinfoglobals(L, f->l.p, t, &c);
      lua_pop(L, 2);
    } else {
      /* set step size in getglobals to 1 to avoid error messages in getglobal's for loop (step = null) */
      lua_pushnumber(L, 1);
      lua_setfield(L, -2, "ncols");
    }
  }
  if (strchr(what, 'k')) {  /* new 7.0.1, get values internally treated as `constants` */
    Proto *p;
    int i;
    if (f->c.isC) return 0;
    p = f->l.p;
    lua_createtable(L, p->sizek, 0);
    for (i=0; i < p->sizek; i++) {
      luaA_pushobject(L, p->k + i);
      lua_rawseti(L, -2, i + 1);
    }
  }
  lua_unlock(L);
  return status;  /* returns 0 on failure */
}


LUA_API void lua_getnupvalues (lua_State *L, lua_Debug *ar) {  /* 2.36.2 */
  lua_lock(L);
  if (ar->i_ci != 0) {  /* no tail call? */
    Closure *f;
    CallInfo *ci = L->base_ci + ar->i_ci;
    lua_assert(ttisfunction(ci->func));
    f = clvalue(ci->func);
    ar->nups = f->c.nupvalues;
  } else {
    ar->nups = 0;
  }
  lua_unlock(L);
}


/*
** {======================================================
** Symbolic Execution and code checker
** =======================================================
*/

#define check(x)    if (!(x)) return 0;

#define checkjump(pt,pc)  check(0 <= pc && pc < pt->sizecode)

#define checkreg(pt,reg)  check((reg) < (pt)->maxstacksize)



static int precheck (const Proto *pt) {
  check(pt->maxstacksize <= MAXSTACK);
  /* lua_assert(pt->numparams+(pt->is_vararg & VARARG_HASARG) <= pt->maxstacksize);
  lua_assert(!(pt->is_vararg & VARARG_NEEDSARG) ||  Lua 5.1.3 patch 5 */
  check(pt->numparams+(pt->is_vararg & VARARG_HASARG) <= pt->maxstacksize);
  check(!(pt->is_vararg & VARARG_NEEDSARG) ||
              (pt->is_vararg & VARARG_HASARG));
  check(pt->sizeupvalues <= pt->nups);
  check(pt->sizelineinfo == pt->sizecode || pt->sizelineinfo == 0);
  /* check(GET_OPCODE(pt->code[pt->sizecode - 1]) == OP_RETURN); Lua 5.1.3 patch 5 */
  check(pt->sizecode > 0 && GET_OPCODE(pt->code[pt->sizecode - 1]) == OP_RETURN);
  return 1;
}


#define checkopenop(pt,pc)  luaG_checkopenop((pt)->code[(pc) + 1])

int luaG_checkopenop (Instruction i) {
  switch (GET_OPCODE(i)) {
    case OP_CALL:
    case OP_TAILCALL:
    case OP_RETURN:
    case OP_SETLIST: {
      check(GETARG_B(i) == 0);
      return 1;
    }
    default: return 0;  /* invalid instruction after an open call */
  }
}


static int checkArgMode (const Proto *pt, int r, enum OpArgMask mode) {
  switch (mode) {
    case OpArgN: check(r == 0); break;
    case OpArgU: break;
    case OpArgR: checkreg(pt, r); break;
    case OpArgK:
      check(ISK(r) ? INDEXK(r) < pt->sizek : r < pt->maxstacksize);
      break;
  }
  return 1;
}


static Instruction symbexec (const Proto *pt, int lastpc, int reg) {  /* Lua 5.1.2 patch */
  int pc;
  int last;  /* stores position of last instruction that changed `reg' */
  last = pt->sizecode - 1;  /* points to final return (a `neutral' instruction) */
  check(precheck(pt));
  for (pc = 0; pc < lastpc; pc++) {
    Instruction i = pt->code[pc];
    OpCode op = GET_OPCODE(i);
    int a = GETARG_A(i);
    int b = 0;
    int c = 0;
    check(op < NUM_OPCODES);
    checkreg(pt, a);
    switch (getOpMode(op)) {
      case iABC: {
        b = GETARG_B(i);
        c = GETARG_C(i);
        check(checkArgMode(pt, b, getBMode(op)));
        check(checkArgMode(pt, c, getCMode(op)));
        break;
      }
      case iABx: {
        b = GETARG_Bx(i);
        if (getBMode(op) == OpArgK) check(b < pt->sizek);
        break;
      }
      case iAsBx: {
        b = GETARG_sBx(i);
        if (getBMode(op) == OpArgR) {
          int dest = pc + 1 + b;
          check(0 <= dest && dest < pt->sizecode);
          if (dest > 0) {
            /* cannot jump to a setlist count
            Instruction d = pt->code[dest - 1];
            check(!(GET_OPCODE(d) == OP_SETLIST && GETARG_C(d) == 0)); Lua 5.1.3 patch 7 */
            int j;
            /* check that it does not jump to a setlist count; this
               is tricky, because the count from a previous setlist may
               have the same value of an invalid setlist; so, we must
               go all the way back to the first of them (if any) */
            for (j = 0; j < dest; j++) {
              Instruction d = pt->code[dest - 1 - j];
              if (!(GET_OPCODE(d) == OP_SETLIST && GETARG_C(d) == 0)) break;
            }
            /* if 'j' is even, previous value is not a setlist (even if
               it looks like one) */
            check((j&1) == 0);
          }
        }
        break;
      }
    }
    if (testAMode(op)) {
      if (a == reg) last = pc;  /* change register `a' */
    }
    if (testTMode(op)) {
      check(pc + 2 < pt->sizecode);  /* check skip */
      check(GET_OPCODE(pt->code[pc + 1]) == OP_JMP);
    }
    switch (op) {
      case OP_LOADBOOL: {
        /* check(c == 0 || pc + 2 < pt->sizecode); Lua 5.1.3 patch 5 */ /* check its jump */
        if (c == 1) {  /* does it jump? */
          check(pc + 2 < pt->sizecode);  /* check its jump */
          check(GET_OPCODE(pt->code[pc + 1]) != OP_SETLIST ||
                GETARG_C(pt->code[pc + 1]) != 0);
        }
        break;
      }
      case OP_LOADNIL: {
        if (a <= reg && reg <= b)
          last = pc;  /* set registers from `a' to `b' */
        break;
      }
      case OP_GETUPVAL:
      case OP_SETUPVAL: {
        check(b < pt->nups);
        break;
      }
      case OP_GETGLOBAL:
      case OP_SETGLOBAL: {
        check(ttisstring(&pt->k[b]));
        break;
      }
      case OP_SELF: {  /* 2.10.2 re-activation */
        checkreg(pt, a + 1);
        if (reg == a + 1) last = pc;
        break;
      }
      case OP_CONCAT: {
        check(b < c);  /* at least two operands */
        break;
      }
      case OP_TFORLOOP: {
        check(c >= 1);  /* at least one result (control variable) */
        checkreg(pt, a + 2 + c);  /* space for results */
        if (reg >= a + 2) last = pc;  /* affect all regs above its base */
        break;
      }
      case OP_FORLOOP:
      case OP_FORPREP:
        checkreg(pt, a+3);
        /* go through */
      case OP_JMP: {
        int dest = pc + 1 + b;
        /* not full check and jump is forward and do not skip `lastpc'? */
        if (reg != NO_REG && pc < dest && dest <= lastpc)
          pc += b;  /* do the jump */
        break;
      }
      case OP_CALL:
      case OP_TAILCALL: {
        if (b != 0) {
          checkreg(pt, a + b - 1);
        }
        c--;  /* c = num. returns */
        if (c == LUA_MULTRET) {
          check(checkopenop(pt, pc));
        }
        else if (c != 0)
          checkreg(pt, a + c - 1);
        if (reg >= a) last = pc;  /* affect all registers above base */
        break;
      }
      case OP_RETURN: {
        b--;  /* b = num. returns */
        if (b > 0) checkreg(pt, a + b - 1);
        break;
      }
      case OP_SETLIST: {
        if (b > 0) checkreg(pt, a + b);
        /* if (c == 0) pc++; Lua 5.1.3 patch 5 */
        if (c == 0) {
          pc++;
          check(pc < pt->sizecode - 1);
        }
        break;
      }
      case OP_CLOSURE: {
        int nup, j;
        check(b < pt->sizep);
        nup = pt->p[b]->nups;
        check(pc + nup < pt->sizecode);
        for (j = 1; j <= nup; j++) {
          OpCode op1 = GET_OPCODE(pt->code[pc + j]);
          check(op1 == OP_GETUPVAL || op1 == OP_MOVE);
        }
        if (reg != NO_REG)  /* tracing? */
          pc += nup;  /* do not 'execute' these pseudo-instructions */
        break;
      }
      default: break;
    }
  }
  return pt->code[last];
}


#undef check
#undef checkjump
#undef checkreg

/* }====================================================== */


int luaG_checkcode (const Proto *pt) {
  return (symbexec(pt, pt->sizecode, NO_REG) != 0);
}


static const char *kname (Proto *p, int c) {
  if (ISK(c) && ttisstring(&p->k[INDEXK(c)]))
    return svalue(&p->k[INDEXK(c)]);
  else
    return "?";
}


static const char *getobjname (lua_State *L, CallInfo *ci, int stackpos,
                               const char **name) {
  if (isLua(ci)) {  /* a Lua function? */
    Proto *p = ci_func(ci)->l.p;
    int pc = currentpc(L, ci);
    Instruction i;
    *name = luaF_getlocalname(p, stackpos + 1, pc);
    if (*name)  /* is a local? */
      return "local";
    i = symbexec(p, pc, stackpos);  /* try symbolic execution */
    lua_assert(pc != -1);
    switch (GET_OPCODE(i)) {
      case OP_GETGLOBAL: {
        int g = GETARG_Bx(i);  /* global index */
        lua_assert(ttisstring(&p->k[g]));
        *name = svalue(&p->k[g]);
        return "global";
      }
      case OP_MOVE: {
        int a = GETARG_A(i);
        int b = GETARG_B(i);  /* move from `b' to `a' */
        if (b < a)
          return getobjname(L, ci, b, name);  /* get name for `b' */
        break;
      }
      case OP_GETTABLE: {
        int k = GETARG_C(i);  /* key index */
        *name = kname(p, k);
        return "field";
      }
      case OP_GETUPVAL: {
        int u = GETARG_B(i);  /* upvalue index */
        *name = p->upvalues ? getstr(p->upvalues[u]) : "?";
        return "upvalue";
      }
      case OP_SELF: {  /* 2.10.2 re-activation */
        int k = GETARG_C(i);  /* key index */
        *name = kname(p, k);
        return "method";
      }
      default: break;
    }
  }
  return NULL;  /* no useful name found */
}


static const char *getfuncname (lua_State *L, CallInfo *ci, const char **name) {
  Instruction i;
  if ((isLua(ci) && ci->tailcalls > 0) || !isLua(ci - 1))
    return NULL;  /* calling function is not Lua (or is unknown) */
  ci--;  /* calling function */
  i = ci_func(ci)->l.p->code[currentpc(L, ci)];
  if (GET_OPCODE(i) == OP_CALL || GET_OPCODE(i) == OP_TAILCALL ||
      GET_OPCODE(i) == OP_TFORLOOP)
    return getobjname(L, ci, GETARG_A(i), name);
  else
    return NULL;  /* no useful name can be found */
}


/* only ANSI way to check whether a pointer points to an array */
static int isinstack (CallInfo *ci, const TValue *o) {
  StkId p;
  for (p = ci->base; p < ci->top; p++)
    if (o == p) return 1;
  return 0;
}


void luaG_typeerror (lua_State *L, const TValue *o, const char *op) {
  const char *name = NULL;
  const char *t = luaT_typenames[ttype(o)];
  const char *kind = (isinstack(L->ci, o)) ?
                         getobjname(L, L->ci, cast_int(o - L->base), &name) :
                         NULL;
  if (kind)
    luaG_runerror(L, "attempt to %s %s " LUA_QS " (a %s value)",
                op, kind, name, t);
  else
    luaG_runerror(L, "attempt to %s a %s value", op, t);
}


void luaG_typeerrorx (lua_State *L, const TValue *o, const char *what, const char *opname) {  /* 2.21.0 */
  const char *name = NULL;
  const char *t = luaT_typenames[ttype(o)];
  const char *kind = (isinstack(L->ci, o)) ?
                         getobjname(L, L->ci, cast_int(o - L->base), &name) :
                         NULL;
  if (kind)
    luaG_runerror(L, "in " LUA_QS " attempt to %s %s " LUA_QS " (a %s value)",
                opname, what, kind, name, t);
  else
    luaG_runerror(L, "in " LUA_QS ": attempt to %s a %s value", opname, what, t);
}


void agnG_indexerror (lua_State *L, const TValue *o, const TValue *key) {  /* 2.2.4 */
  const char *name = NULL;
  const char *t = luaT_typenames[ttype(o)];
  const char *k = luaT_typenames[ttype(key)];
  const char *kind = (isinstack(L->ci, o)) ?
                         getobjname(L, L->ci, cast_int(o - L->base), &name) :
                         NULL;
  if (kind) {
    int len =  /* 2.11.4 change */
      tools_strlen("attempt to index ") + tools_strlen(kind) + 1 + 2 + tools_strlen(name) +  /* 2.17.8 tweak */
      tools_strlen(" (a  value) with") + tools_strlen(" a  value") + tools_strlen(t) + tools_strlen(k);
    luaG_runerror(L, "attempt to index %s " LUA_QS " (a %s value) with%sa %s value",
                kind, name, t, len <= L->errmlinebreak ? " " : "\n   ", k);
  } else
    luaG_runerror(L, "attempt to index a %s value with a %s value", t, k);
}

void luaG_concaterror (lua_State *L, StkId p1, StkId p2) {
  if (ttisstring(p1) || ttisnumber(p1)) p1 = p2;  /* Lua 5.1.3 patch */
  lua_assert(!ttisstring(p1) && !ttisnumber(p1));
  luaG_typeerror(L, p1, "concatenate");
}


void luaG_aritherror (lua_State *L, const TValue *p1, const TValue *p2) {
  TValue temp;
  if (luaV_arithmoperand(p1, &temp) == NULL)
    p2 = p1;  /* first operand is wrong */
  luaG_typeerror(L, p2, "perform arithmetic on");
}


int luaG_ordererror (lua_State *L, const TValue *p1, const TValue *p2) {
  const char *t1 = luaT_typenames[ttype(p1)];
  const char *t2 = luaT_typenames[ttype(p2)];
  if (t1[0] == t2[0] && t1[2] == t2[2])  /* 2.10.4 improvement */
    luaG_runerror(L, "attempt to compare two %s values", t1);
  else
    luaG_runerror(L, "attempt to compare %s with %s", t1, t2);
  return 0;
}


static void addinfo (lua_State *L, const char *msg) {
  CallInfo *ci = L->ci;
  if (isLua(ci)) {  /* is Lua code? */
    char buff[LUA_IDSIZE];  /* add file:line information */
    int line = currentline(L, ci);
    luaO_chunkid(buff, getstr(getluaproto(ci)->source), LUA_IDSIZE);
    if (line > 0)
      luaO_pushfstring(L, "Error in %s at line %d:\n   %s", buff, line, msg);  /* 2.11.4 change */
    else
      luaO_pushfstring(L, "Error in %s:\n   %s", buff, msg);  /* 2.11.4 change */
  }
}


void luaG_errormsg (lua_State *L) {
  if (L->errfunc != 0) {  /* is there an error handling function? */
    StkId errfunc = restorestack(L, L->errfunc);
    if (!ttisfunction(errfunc)) luaD_throw(L, LUA_ERRERR);
    setobjs2s(L, L->top, L->top - 1);  /* move argument */
    setobjs2s(L, L->top - 1, errfunc);  /* push function */
    incr_top(L);
    luaD_call(L, L->top - 2, 1, 1);  /* call it */
  }
  luaD_throw(L, LUA_ERRRUN);
}


void luaG_runerror (lua_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  addinfo(L, luaO_pushvfstring(L, fmt, argp));
  va_end(argp);
  luaG_errormsg(L);
}

