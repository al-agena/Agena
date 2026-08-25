/*
** $Id: lcode.c,v 2.25 2006/03/21 19:28:49 roberto Exp $
** Code generator for Lua/Agena
** See Copyright Notice in agena.h
*/

#include <stdlib.h>

#define lcode_c
#define LUA_CORE

#include "agena.h"

#include "agncmpt.h"
#include "lcode.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "ltable.h"

#define hasjumps(e)   ((e)->t != (e)->f)


int luaK_isnumeral (expdesc *e) {
  return (e->k == VKNUM && e->t == NO_JUMP && e->f == NO_JUMP);
}


void luaK_nil (FuncState *fs, int from, int n) {  /* Lua 5.1.2 patch */
  Instruction *previous;
  if (fs->pc > fs->lasttarget) {  /* no jumps to current position? */
    if (fs->pc == 0) {  /* function start? */
      if (from >= fs->nactvar)
        return;  /* positions are already clean */
    }
    else {
      previous = &fs->f->code[fs->pc - 1];
      if (GET_OPCODE(*previous) == OP_LOADNIL) {
        int pfrom = GETARG_A(*previous);
        int pto = GETARG_B(*previous);
        if (pfrom <= from && from <= pto + 1) {  /* can connect both? */
          if (from + n - 1 > pto)
            SETARG_B(*previous, from + n - 1);
          return;
        }
      }
    }
  }
  luaK_codeABC(fs, OP_LOADNIL, from, from + n - 1, 0);  /* else no optimisation */
}


/*
** Create a jump instruction and return its position, so its destination
** can be fixed later (with 'fixjump'). If there are jumps to
** this position (kept in 'jpc'), link them all together so that
** 'patchlistaux' will fix all them directly to the final destination.
*/
int luaK_jump (FuncState *fs) {
  int jpc = fs->jpc;  /* save list of jumps to here */
  int j;
  fs->jpc = NO_JUMP;
  j = luaK_codeAsBx(fs, OP_JMP, 0, NO_JUMP);
  luaK_concat(fs, &j, jpc);  /* keep them on hold */
  return j;
}


/*
** Code a 'return' instruction
*/
void luaK_ret (FuncState *fs, int first, int nret) {
  luaK_codeABC(fs, OP_RETURN, first, nret + 1, 0);
}


/*
** Code a "conditional jump", that is, a test or comparison opcode
** followed by a jump. Return jump position.
*/
static int condjump (FuncState *fs, OpCode op, int A, int B, int C) {
  luaK_codeABC(fs, op, A, B, C);
  return luaK_jump(fs);
}


static void fixjump (FuncState *fs, int pc, int dest) {
  Instruction *jmp = &fs->f->code[pc];
  int offset = dest - (pc + 1);
  lua_assert(dest != NO_JUMP);
  if (abs(offset) > MAXARG_sBx)
    luaX_syntaxerror(fs->ls, "control structure too long");
  SETARG_sBx(*jmp, offset);
}


/*
** returns current `pc' and marks it as a jump target (to avoid wrong
** optimisations with consecutive instructions not in the same basic block).
*/
int luaK_getlabel (FuncState *fs) {
  fs->lasttarget = fs->pc;
  return fs->pc;
}


/*
** Gets the destination address of a jump instruction. Used to traverse
** a list of jumps.
*/
static int getjump (FuncState *fs, int pc) {
  int offset = GETARG_sBx(fs->f->code[pc]);
  if (offset == NO_JUMP)  /* point to itself represents end of list */
    return NO_JUMP;  /* end of list */
  else
    return (pc + 1) + offset;  /* turn offset into absolute position */
}


/*
** Returns the position of the instruction "controlling" a given
** jump (that is, its condition), or the jump itself if it is
** unconditional.
*/
static Instruction *getjumpcontrol (FuncState *fs, int pc) {
  Instruction *pi = &fs->f->code[pc];
  if (pc >= 1 && testTMode(GET_OPCODE(*(pi - 1))))
    return pi - 1;
  else
    return pi;
}


/*
** check whether list has any jump that do not produce a value
** (or produce an inverted value)
*/
static int need_value (FuncState *fs, int list) {
  for (; list != NO_JUMP; list = getjump(fs, list)) {
    Instruction i = *getjumpcontrol(fs, list);
    if (GET_OPCODE(i) != OP_TESTSET) return 1;
  }
  return 0;  /* not found */
}


/*
** Patch destination register for a TESTSET instruction.
** If instruction in position 'node' is not a TESTSET, return 0 ("fails").
** Otherwise, if 'reg' is not 'NO_REG', set it as the destination
** register. Otherwise, change instruction to a simple 'TEST' (produces
** no register value)
*/
static int patchtestreg (FuncState *fs, int node, int reg) {
  Instruction *i = getjumpcontrol(fs, node);
  if (GET_OPCODE(*i) != OP_TESTSET)
    return 0;  /* cannot patch other instructions */
  if (reg != NO_REG && reg != GETARG_B(*i))
    SETARG_A(*i, reg);
  else  /* no register to put value or register already has the value */
    *i = CREATE_ABC(OP_TEST, GETARG_B(*i), 0, GETARG_C(*i));
  return 1;
}


/*
** Traverse a list of tests ensuring no one produces a value
*/
static void removevalues (FuncState *fs, int list) {
  for (; list != NO_JUMP; list = getjump(fs, list))
      patchtestreg(fs, list, NO_REG);
}


/*
** Traverse a list of tests, patching their destination address and
** registers: tests producing values jump to 'vtarget' (and put their
** values in 'reg'), other tests jump to 'dtarget'.
*/
static void patchlistaux (FuncState *fs, int list, int vtarget, int reg,
                          int dtarget) {
  while (list != NO_JUMP) {
    int next = getjump(fs, list);
    if (patchtestreg(fs, list, reg))
      fixjump(fs, list, vtarget);
    else
      fixjump(fs, list, dtarget);  /* jump to default target */
    list = next;
  }
}


/*
** Ensure all pending jumps to current position are fixed (jumping
** to current position with no values) and reset list of pending
** jumps
*/
static void dischargejpc (FuncState *fs) {
  patchlistaux(fs, fs->jpc, fs->pc, NO_REG, fs->pc);
  fs->jpc = NO_JUMP;
}


/*
** Path all jumps in 'list' to jump to 'target'.
** (The assert means that we cannot fix a jump to a forward address
** because we only know addresses once code is generated.)
*/
void luaK_patchlist (FuncState *fs, int list, int target) {
  if (target == fs->pc)
    luaK_patchtohere(fs, list);
  else {
    lua_assert(target < fs->pc);
    patchlistaux(fs, list, target, NO_REG, target);
  }
}


/*
** Add elements in 'list' to list of pending jumps to "here" (current position) =
** patches all jump instructions in the given list to jump to the current instruction ("here").
*/
void luaK_patchtohere (FuncState *fs, int list) {
  luaK_getlabel(fs);  /* mark "here" as a jump target */
  luaK_concat(fs, &fs->jpc, list);
}


/* concatenates two lists of pending jumps; creates a new jump instruction (luaK_jump) and
  add it to the 'escapelist', see http://lua.2524044.n2.nabble.com/About-ifstat-td7586119.html */
void luaK_concat (FuncState *fs, int *l1, int l2) {
  if (l2 == NO_JUMP) return;
  else if (*l1 == NO_JUMP)
    *l1 = l2;
  else {
    int list = *l1;
    int next;
    while ((next = getjump(fs, list)) != NO_JUMP)  /* find last element */
      list = next;
    fixjump(fs, list, l2);
  }
}


/*
** Check register-stack level, keeping track of its maximum size
** in field 'maxstacksize'
*/
void luaK_checkstack (FuncState *fs, int n) {
  int newstack = fs->freereg + n;
  if (newstack > fs->f->maxstacksize) {
    if (newstack >= MAXSTACK)
      luaX_syntaxerror(fs->ls, "procedure or expression is too complex");
    fs->f->maxstacksize = cast_byte(newstack);
  }
}


/*
** Reserve 'n' registers in register stack
*/
void luaK_reserveregs (FuncState *fs, int n) {
  luaK_checkstack(fs, n);
  fs->freereg += n;
}


/*
** Free register 'reg', if it is neither a constant index nor
** a local variable.
*/
static void freereg (FuncState *fs, int reg) {
  if (!ISK(reg) && reg >= fs->nactvar) {
    fs->freereg--;
    lua_assert(reg == fs->freereg);
  }
}


/*
** Free register used by expression 'e' (if any)
*/
static void freeexp (FuncState *fs, expdesc *e) {
  if (e->k == VNONRELOC)
    freereg(fs, e->u.s.info);
}


/*
** Free registers used by expressions 'e1' and 'e2' (if any) in proper
** order. 2.20.2
*/
static void freeexps (FuncState *fs, expdesc *e1, expdesc *e2) {
  int r1 = (e1->k == VNONRELOC) ? e1->u.s.info : -1;
  int r2 = (e2->k == VNONRELOC) ? e2->u.s.info : -1;
  if (r1 > r2) {
    freereg(fs, r1);
    freereg(fs, r2);
  }
  else {
    freereg(fs, r2);
    freereg(fs, r1);
  }
}


static void freeexpsx (FuncState *fs, int o1, int o2, expdesc *e1, expdesc *e2) {
  if (o1 > o2) {
    freeexp(fs, e1);
    freeexp(fs, e2);
  } else {
    freeexp(fs, e2);
    freeexp(fs, e1);
  }
}

/*
** Add constant 'v' to prototype's list of constants (field 'k').
** Use scanner's table to cache position of constants in constant list
** and try to reuse constants. Because some values should not be used
** as keys (nil cannot be a key, integer keys can collapse with float
** keys), the caller must provide a useful 'key' for indexing the cache.
*/
static int addk (FuncState *fs, TValue *k, TValue *v) {
  lua_State *L = fs->L;
  TValue *idx = luaH_set(L, fs->h, k);
  Proto *f = fs->f;
  int oldsize = f->sizek;
  if (ttisnumber(idx)) {
    lua_assert(luaO_rawequalObj(&fs->f->k[cast_int(nvalue(idx))], v));
    return cast_int(nvalue(idx));
  } else {  /* constant not found; create a new entry */
    setnvalue(idx, cast_num(fs->nk));
    luaM_growvector(L, f->k, fs->nk, f->sizek, TValue,
                    MAXARG_Bx, "constant table overflow");
    while (oldsize < f->sizek) setnilvalue(&f->k[oldsize++]);
    setobj(L, &f->k[fs->nk], v);
    luaC_barrier(L, f, v);
    return fs->nk++;
  }
}


/*
** Add a string to list of constants and return its index.
*/
int luaK_stringK (FuncState *fs, TString *s) {
  TValue o;
  setsvalue(fs->L, &o, s);
  return addk(fs, &o, &o);
}


/*
** Add a float to list of constants and return its index.
*/
int luaK_numberK (FuncState *fs, lua_Number r) {
  TValue o;
  setnvalue(&o, r);
  return addk(fs, &o, &o);
}


/*
** Add a boolean to list of constants and return its index.
*/
static int boolK (FuncState *fs, int b) {
  TValue o;
  setbvalue(&o, b);
  return addk(fs, &o, &o);
}


/*
** Add nil to list of constants and return its index.
*/
static int nilK (FuncState *fs) {
  TValue k, v;
  setnilvalue(&v);
  /* cannot use nil as key; instead use table itself to represent nil */
  sethvalue(fs->L, &k, fs->h);
  return addk(fs, &k, &v);
}


/*
** Fix an expression to return the number of results 'nresults'.
** Either 'e' is a multi-ret expression (function call or vararg)
** or 'nresults' is LUA_MULTRET (as any expression can satisfy that).
*/
void luaK_setreturns (FuncState *fs, expdesc *e, int nresults) {
  if (e->k == VCALL) {  /* expression is an open function call? */
    SETARG_C(getcode(fs, e), nresults + 1);
  }
  else if (e->k == VVARARG) {
    SETARG_B(getcode(fs, e), nresults + 1);
    SETARG_A(getcode(fs, e), fs->freereg);
    luaK_reserveregs(fs, 1);
  }
}


/*
** Fix an expression to return one result.
** If expression is not a multi-ret expression (function call or
** vararg), it already returns one result, so nothing needs to be done.
** Function calls become VNONRELOC expressions (as its result comes
** fixed in the base register of the call), while vararg expressions
** become VRELOCABLE (as OP_VARARG puts its results where it wants).
** (Calls are created returning one result, so that does not need
** to be fixed.)
*/
void luaK_setoneret (FuncState *fs, expdesc *e) {
  if (e->k == VCALL) {  /* expression is an open function call? */
    e->k = VNONRELOC;
    e->u.s.info = GETARG_A(getcode(fs, e));
  }
  else if (e->k == VVARARG) {
    SETARG_B(getcode(fs, e), 2);
    e->k = VRELOCABLE;  /* can relocate its simple result */
  }
}


void luaK_dischargevars (FuncState *fs, expdesc *e) {
  switch (e->k) {
    case VLOCAL: {
      e->k = VNONRELOC;
      break;
    }
    case VUPVAL: {
      e->u.s.info = luaK_codeABC(fs, OP_GETUPVAL, 0, e->u.s.info, 0);
      e->k = VRELOCABLE;
      break;
    }
    case VGLOBAL: {
      e->u.s.info = luaK_codeABx(fs, OP_GETGLOBAL, 0, e->u.s.info);
      e->k = VRELOCABLE;
      break;
    }
    case VINDEXED: {
      freereg(fs, e->u.s.aux);
      freereg(fs, e->u.s.info);
      e->u.s.info = luaK_codeABC(fs, OP_GETTABLE, 0, e->u.s.info, e->u.s.aux);
      e->k = VRELOCABLE;
      break;
    }
    case VVARARG:
    case VCALL: {
      luaK_setoneret(fs, e);
      break;
    }
    default: break;  /* there is one value available (somewhere) */
  }
}


static int code_label (FuncState *fs, int A, int b, int jump) {
  luaK_getlabel(fs);  /* those instructions may be jump targets */
  return luaK_codeABC(fs, OP_LOADBOOL, A, b, jump);
}


/*
** Ensures expression value is in register 'reg' (and therefore
** 'e' will become a non-relocatable expression).
*/
void luaK_discharge2reg (FuncState *fs, expdesc *e, int reg) {
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: {
      luaK_nil(fs, reg, 1);
      break;
    }
    case VFALSE: case VTRUE: {
      luaK_codeABC(fs, OP_LOADBOOL, reg, e->k == VTRUE, 0);
      break;
    }
    case VK: {
      luaK_codeABx(fs, OP_LOADK, reg, e->u.s.info);
      break;
    }
    case VKNUM: {
      luaK_codeABx(fs, OP_LOADK, reg, luaK_numberK(fs, e->u.nval));
      break;
    }
    case VRELOCABLE: {
      Instruction *pc = &getcode(fs, e);
      SETARG_A(*pc, reg);
      break;
    }
    case VNONRELOC: {
      if (reg != e->u.s.info)
        luaK_codeABC(fs, OP_MOVE, reg, e->u.s.info, 0);
      break;
    }
    case VFAIL: {
      luaK_codeABC(fs, OP_LOADBOOL, reg, 2, 0);
      break;
    }
    default: {
      lua_assert(e->k == VVOID || e->k == VJMP);
      return;  /* nothing to do... */
    }
  }
  e->u.s.info = reg;
  e->k = VNONRELOC;
}


/*
** Ensures expression value is in any register.
*/
void discharge2anyreg (FuncState *fs, expdesc *e) {
  if (e->k != VNONRELOC) {
    luaK_reserveregs(fs, 1);
    luaK_discharge2reg(fs, e, fs->freereg - 1);
  }
}


/*
** Ensures final expression result (including results from its jump
** lists) is in register 'reg'.
** If expression has jumps, need to patch these jumps either to
** its final position or to "load" instructions (for those tests
** that do not produce values).
*/
static void exp2reg (FuncState *fs, expdesc *e, int reg) {
  luaK_discharge2reg(fs, e, reg);
  if (e->k == VJMP)
    luaK_concat(fs, &e->t, e->u.s.info);  /* put this jump in `t' list */
  if (hasjumps(e)) {
    int final;  /* position after whole expression */
    int p_f = NO_JUMP;  /* position of an eventual LOAD false */
    int p_t = NO_JUMP;  /* position of an eventual LOAD true */
    if (need_value(fs, e->t) || need_value(fs, e->f)) {
      int fj = (e->k == VJMP) ? NO_JUMP : luaK_jump(fs);
      p_f = code_label(fs, reg, 0, 1);
      p_t = code_label(fs, reg, 1, 0);
      luaK_patchtohere(fs, fj);
    }
    final = luaK_getlabel(fs);
    patchlistaux(fs, e->f, final, reg, p_f);
    patchlistaux(fs, e->t, final, reg, p_t);
  }
  e->f = e->t = NO_JUMP;
  e->u.s.info = reg;
  e->k = VNONRELOC;
}


void luaK_exp2reg (FuncState *fs, expdesc *e, int reg) {  /* 7.5.0, used in waived `WITH TABLE NAME DO` */
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VRELOCABLE: {
      /* This handles things like NEWTABLE or CALLs that haven't
         been assigned a destination register yet */
      SETARG_A(getcode(fs, e), reg);
      break;
    }
    case VNONRELOC: {
      /* If it's already in a register, but not the ONE we want, move it */
      if (e->u.s.info != reg)
        luaK_codeABC(fs, OP_MOVE, reg, e->u.s.info, 0);
      break;
    }
    default: {
      if (e->k != VVOID) {
        /* Handles constants, globals, etc. */
        int r = luaK_exp2anyreg(fs, e);
        luaK_codeABC(fs, OP_MOVE, reg, r, 0);
      }
      break;
    }
  }
  e->u.s.info = reg;
  e->k = VNONRELOC;
}


/*
** Ensures final expression result (including results from its jump
** lists) is in next available register.
*/
int luaK_exp2nextreg (FuncState *fs, expdesc *e) {
  luaK_dischargevars(fs, e);
  freeexp(fs, e);
  luaK_reserveregs(fs, 1);
  exp2reg(fs, e, fs->freereg - 1);
  return e->u.s.info;
}


/*
** Ensures final expression result (including results from its jump
** lists) is in some (any) register and return that register.
*/
int luaK_exp2anyreg (FuncState *fs, expdesc *e) {
  luaK_dischargevars(fs, e);
  if (e->k == VNONRELOC) {
    if (!hasjumps(e)) return e->u.s.info;  /* exp is already in a register */
    if (e->u.s.info >= fs->nactvar) {  /* reg. is not a local? */
      exp2reg(fs, e, e->u.s.info);  /* put value on it */
      return e->u.s.info;
    }
  }
  luaK_exp2nextreg(fs, e);  /* default */
  return e->u.s.info;
}


/*
** Ensures final expression result is either in a register or it is
** a constant.
*/
void luaK_exp2val (FuncState *fs, expdesc *e) {
  if (hasjumps(e))
    luaK_exp2anyreg(fs, e);
  else
    luaK_dischargevars(fs, e);
}


/*
** Ensures final expression result is in a valid R/K index
** (that is, it is either in a register or in 'k' with an index
** in the range of R/K indices).
** Returns R/K index.
*/
int luaK_exp2RK (FuncState *fs, expdesc *e) {
  luaK_exp2val(fs, e);
  switch (e->k) {
    case VKNUM:
    case VTRUE:
    case VFALSE:
    case VNIL: {
      if (fs->nk <= MAXINDEXRK) {  /* constant fit in RK operand? */
        e->u.s.info = (e->k == VNIL)  ? nilK(fs) :
                      (e->k == VKNUM) ? luaK_numberK(fs, e->u.nval) :
                                        boolK(fs, (e->k == VTRUE));
        e->k = VK;
        return RKASK(e->u.s.info);
      }
      else break;
    }
    case VK: {
      if (e->u.s.info <= MAXINDEXRK)  /* constant fit in argC? */
        return RKASK(e->u.s.info);
      else break;
    }
    case VFAIL: {
      if (fs->nk <= MAXINDEXRK) {  /* constant fit in RK operand? */
        e->u.s.info = boolK(fs, 2);
        e->k = VK;
        return RKASK(e->u.s.info);
      }
      else break;
    }
    default: break;
  }
  /* not a constant in the right range: put it in a register */
  return luaK_exp2anyreg(fs, e);
}


/* Emit store for LHS expression. */
void luaK_storevar (FuncState *fs, expdesc *var, expdesc *ex) {
  var->oop = 0;  /* reset `propbable-oop-method-assignment`-flag, 2.25.0 RC 2 */
  switch (var->k) {
    case VLOCAL: {
      freeexp(fs, ex);
      exp2reg(fs, ex, var->u.s.info);
      return;
    }
    case VUPVAL: {
      int e = luaK_exp2anyreg(fs, ex);
      luaK_codeABC(fs, OP_SETUPVAL, e, var->u.s.info, 0);
      break;
    }
    case VGLOBAL: {
      int e = luaK_exp2anyreg(fs, ex);
      luaK_codeABx(fs, OP_SETGLOBAL, e, var->u.s.info);
      break;
    }
    case VINDEXED: {
      int e = luaK_exp2RK(fs, ex);
      luaK_codeABC(fs, OP_SETTABLE, var->u.s.info, var->u.s.aux, e);
      break;
    }
    default: {
      lua_assert(0);  /* invalid var kind to store */
      break;
    }
  }
  freeexp(fs, ex);
}


/* static void invertjump (FuncState *fs, expdesc *e) { */
static void invertjump (FuncState *fs, expdesc *e) {
  Instruction *pc = getjumpcontrol(fs, e->u.s.info);
  lua_assert(testTMode(GET_OPCODE(*pc)) && GET_OPCODE(*pc) != OP_TESTSET &&
                                           GET_OPCODE(*pc) != OP_TEST);
  SETARG_A(*pc, !(GETARG_A(*pc)));
}


/*
** Emit instruction to jump if 'e' is 'cond' (that is, if 'cond'
** is true, code will jump if 'e' is true.) Return jump position.
** Optimize when 'e' is 'not' something, inverting the condition
** and removing the 'not'.
*/
static int jumponcond (FuncState *fs, expdesc *e, int cond) {
  if (e->k == VRELOCABLE) {
    Instruction ie = getcode(fs, e);
    if (GET_OPCODE(ie) == OP_NOT) {
      fs->pc--;  /* remove previous OP_NOT */
      return condjump(fs, OP_TEST, GETARG_B(ie), 0, !cond);
    }
    /* else go through */
  }
  discharge2anyreg(fs, e);
  freeexp(fs, e);
  return condjump(fs, OP_TESTSET, NO_REG, e->u.s.info, cond);
}


/*
** Emit code to go through if 'e' is true, jump otherwise.
*/
void luaK_goiftrue (FuncState *fs, expdesc *e) {
  int pc;  /* pc of last jump */
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VK: case VKNUM: case VTRUE: {
      pc = NO_JUMP;  /* always true; do nothing */
      break;
    }
    case VJMP: {  /* condition? */
      invertjump(fs, e);  /* jump when it is false */
      pc = e->u.s.info;   /* save jump position */
      break;
    }
    /* Lua 5.1.4 patch 9 */
    default: {
      pc = jumponcond(fs, e, 0);  /* jump when false */
      break;
    }
  }
  luaK_concat(fs, &e->f, pc);  /* insert new jump in false list */
  luaK_patchtohere(fs, e->t);  /* true list jumps to here (to go through) */
  e->t = NO_JUMP;
}


/*
** Emit code to go through if 'e' is false, jump otherwise, for or operator.
*/
void luaK_goiffalse (FuncState *fs, expdesc *e) {
  int pc;  /* pc of new jump */
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: case VFALSE: case VFAIL: {  /* 1st expression is false -> check 2nd expression */
      pc = NO_JUMP;  /* always false; do nothing */
      break;
    }
    case VJMP: {  /* already jump if true */
      pc = e->u.s.info;
      break;
    }
    /* Lua 5.1.4 patch 9 */
    default: {
      pc = jumponcond(fs, e, 1);
      break;
    }
  }
  luaK_concat(fs, &e->t, pc);  /* insert new jump in 't' list */
  luaK_patchtohere(fs, e->f);  /* false list jumps to here (to go through) */
  e->f = NO_JUMP;
}


/*
** Code 'not e', doing constant folding.
*/
static void codenot (FuncState *fs, expdesc *e) {
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: case VFALSE: case VFAIL: {
      e->k = VTRUE;
      break;
    }
    case VK: case VKNUM: case VTRUE: {
      e->k = VFALSE;
      break;
    }
    case VJMP: {
      invertjump(fs, e);
      break;
    }
    case VRELOCABLE:
    case VNONRELOC: {
      discharge2anyreg(fs, e);
      freeexp(fs, e);
      e->u.s.info = luaK_codeABC(fs, OP_NOT, 0, e->u.s.info, 0);
      e->k = VRELOCABLE;
      break;
    }
    default: {
      lua_assert(0);  /* cannot happen */
      break;
    }
  }
  /* interchange true and false lists */
  { int temp = e->f; e->f = e->t; e->t = temp; }
  removevalues(fs, e->f);
  removevalues(fs, e->t);
}


/*
** Create expression 't[k]'.
*/
void luaK_indexed (FuncState *fs, expdesc *t, expdesc *k) {
  t->u.s.aux = luaK_exp2RK(fs, k);
  t->k = VINDEXED;
}


/*
** Try to "constant-fold" an operation; return 1 iff successful.
** (In this case, 'e1' has the final result.)
*/
static int constfolding (OpCode op, expdesc *e1, expdesc *e2) {
  lua_Number v1, v2, r;
  int isnume1;
  isnume1 = luaK_isnumeral(e1);
  v1 = e1->u.nval;
  v2 = e2->u.nval;
  if (!isnume1 || !luaK_isnumeral(e2) || (isnume1 && v1 == 0)) return 0;  /* 2.8.4 fix for -0 constant (see also patch to function luaK_prefix) */
  switch (op) {
    case OP_ADD: r = luai_numadd(v1, v2); break;
    case OP_SUB: r = luai_numsub(v1, v2); break;
    case OP_MUL: r = luai_nummul(v1, v2); break;
    case OP_DIV:
      if (v2 == 0) return 0;  /* do not attempt to divide by 0 */
      r = luai_numdiv(v1, v2); break;
    case OP_MOD:
      if (v2 == 0) return 0;  /* do not attempt to divide by 0 */
      r = luai_nummod(v1, v2); break;
    case OP_POW: r = luai_numpow(v1, v2); break;
    case OP_IPOW: r = luai_numipow(v1, v2); break;
    case OP_INTDIV: r = luai_numintdiv(v1, v2); break;  /* 2.9.8 fix, 2.17.1 optimisation */
    case OP_ABSDIFF: r = luai_numabsdiff(v1, v2); break;  /* 2.9.8 */
    case OP_UNM: r = luai_numunm(v1); break;
    case OP_LEN: return 0; /* no constant folding for 'len' */
    default: lua_assert(0); r = 0; break;
  }
  if (luai_numisnan(r)) return 0;  /* do not attempt to produce NaN */
  e1->u.nval = r;
  return 1;
}


static int constfoldingbypass (UnOpr fn, expdesc *e) {
  lua_Number v, r;
  if (!luaK_isnumeral(e))
    return 0;
  v = e->u.nval;
  switch (fn) {
    case OPR_ABS: r = luai_numabs(v); break;
    case OPR_ARCTAN: r = luai_numatan(v); break;
    case OPR_COS: r = luai_numcos(v); break;
    case OPR_ENTIER: r = luai_numentier(v); break;
    case OPR_EXP: r = luai_numexp(v); break;
    case OPR_LNGAMMA: r = luai_numlngamma(v); break;
    case OPR_INT: r = luai_numint(v); break;
    case OPR_FRAC: r = luai_numfrac(v); break;  /* 2.3.3 */
    case OPR_LN: r = luai_numln(v); break;
    case OPR_SIGN: r = tools_sign(v); break;
    case OPR_SIN: r = luai_numsin(v); break;
    case OPR_SQRT: r = luai_numsqrt(v); break;
    case OPR_TAN: r = luai_numtan(v); break;
    case OPR_ARCSIN: r = luai_numasin(v); break;
    case OPR_ARCCOS: r = luai_numacos(v); break;
    case OPR_SINH: r = luai_numsinh(v); break;
    case OPR_COSH: r = luai_numcosh(v); break;
    case OPR_TANH: r = luai_numtanh(v); break;
    case OPR_RECIP: r = luai_numrecip(v); break;
    case OPR_COSXX: r = luai_numcos(v); break;
    case OPR_BEA: r = luai_numbea(v); break;
    case OPR_FLIP: r = luai_numflip(v); break;
    case OPR_CONJUGATE: r = luai_numconjugate(v); break;
    case OPR_ANTILOG2: r = luai_numantilog2(v); break;
    case OPR_ANTILOG10: r = luai_numantilog10(v); break;
    case OPR_SIGNUM: r = tools_signum(v); break;  /* 2.17.1 optimisation */
    case OPR_SINC: r = luai_numsinc(v); break;
    case OPR_PEPS: r = luai_numpeps(v); break;
    case OPR_MEPS: r = luai_nummeps(v); break;
    case OPR_SQUARE: r = v*v; break;
    case OPR_CUBE: r = v*v*v; break;
    case OPR_INVSQRT: r = sqrt(v)/v; break;
    default: return 0;
  }
  if (luai_numisnan(r)) return 0;  /* do not attempt to produce NaN */
  e->u.nval = r;
  return 1;
}


void codearith (FuncState *fs, OpCode op, expdesc *e1, expdesc *e2) {  /* Lua 5.1.2 patch */
  if (constfolding(op, e1, e2))
    return;
  else {
    int o2 = (op != OP_UNM && op != OP_LEN) ?
      luaK_exp2RK(fs, e2) : 0;
    int o1 = luaK_exp2RK(fs, e1);
    freeexpsx(fs, o1, o2, e1, e2);  /* 2.20.2 */
    e1->u.s.info = luaK_codeABC(fs, op, 0, o1, o2);
    e1->k = VRELOCABLE;
  }
}


/* function to process bypass OP_FN operations, 0.7.1; Nov 18, 2007; modified Dec 12, 2007
   flag = 0: Do not attempt to invoke fast constfoldingbypass op for arithmetic functions
   flag = 1: Invoke fast constfoldingbypass op for arithmetic functions
   The `if (flag && constfoldingbypass(fn, e))` condition is so fast, it is of no use to provide
   a slimmed down version of codearithbypass for flag=0 function calls. */
int codearithbypass (FuncState *fs, int flag, expdesc *e, UnOpr fn) {
  if (flag && constfoldingbypass(fn, e))
    return 0;
  else {
    int o1 = luaK_exp2RK(fs, e);
    freeexp(fs, e);
    e->u.s.info = luaK_codeABC(fs, OP_FN, 0, o1, fn);
    e->k = VRELOCABLE;
    return e->u.s.info;
  }
}


int codearithbypassmularg (FuncState *fs, int nargs, expdesc *e, UnOpr fn) {
  int i;
  for (i=0; i < nargs; i++)
    freeexp(fs, e);
  e->u.s.info = luaK_codeABC(fs, OP_FN, 0, nargs, fn);
  e->k = VRELOCABLE;
  return e->u.s.info;
}


/* creates a pair or complex number */
static void codepair (FuncState *fs, OpCode op, expdesc *e1, expdesc *e2) {
  int o2 = luaK_exp2RK(fs, e2);
  int o1 = luaK_exp2RK(fs, e1);
  freeexpsx(fs, o1, o2, e1, e2);  /* 2.20.2 */
  e1->u.s.info = luaK_codeABC(fs, (OpCode)op, 0, o1, o2);
  e1->k = VRELOCABLE;
}


/* then apply the proper OP_FNBIN op on this pair, Agena 1.6.3 fix: rewritten 2.5.2
   ! Do not nil values on rb, rc positions in the VM ! */
void agnK_codefnbin (FuncState *fs, expdesc *e1, expdesc *e2, int op) {
  int o1;
  luaK_exp2nextreg(fs, e2);
  o1 = luaK_exp2RK(fs, e1);
  freeexps(fs, e1, e2);
  e1->u.s.info = luaK_codeABC(fs, OP_FNBIN, 0, o1, op);
  e1->k = VRELOCABLE;
}


/*
** Emit code for comparisons.
** 'e1' was already put in R/K form by 'luaK_infix'.
*/
static void codecomp (FuncState *fs, OpCode op, int cond, expdesc *e1,
                                                          expdesc *e2) {
  int o1 = luaK_exp2RK(fs, e1);
  int o2 = luaK_exp2RK(fs, e2);
  freeexps(fs, e1, e2);
  if (cond == 0 && op != OP_EQ) {
    int temp;  /* exchange args to replace by `<' or `<=' */
    temp = o1; o1 = o2; o2 = temp;  /* o1 <==> o2 */
    cond = 1;
  }
  e1->u.s.info = condjump(fs, op, cond, o1, o2);
  e1->k = VJMP;
}


/* 0.6.0: for CASE statement (value comparison), added/rewritten 2.20.0 */
int luaK_codecompop (FuncState *fs, int base, int nargs, expdesc *e, CmdCode op) {
  luaK_codeABC(fs, OP_CMD, base, nargs, op);
  e->u.s.info = luaK_jump(fs);
  e->k = VJMP;
  return e->f;
}


/*
** Apply prefix operation 'op' to expression 'e'.
*/
void luaK_prefix (FuncState *fs, UnOpr op, expdesc *e) {
  expdesc e2;
  e2.t = e2.f = NO_JUMP; e2.k = VKNUM; e2.u.nval = 0;
  switch (op) {
    case OPR_MINUS: {
      if (luaK_isnumeral(e) && e->u.nval != 0.0)  /* 2.8.4 patch taken from Lua 5.2.4 code: minus constant ? (see also patch to function constfolding) */
        e->u.nval = luai_numunm(e->u.nval);  /* fold it */
      else {
        luaK_exp2anyreg(fs, e);
        codearith(fs, OP_UNM, e, &e2);
      }
      break;
    }
    case OPR_NOT: codenot(fs, e); break;
    case OPR_LEN: {
      luaK_exp2anyreg(fs, e);  /* cannot operate on constants */
      codearith(fs, OP_LEN, e, &e2);
      break;
    }
    case OPR_ASSIGNED: case OPR_TSUMUP: case OPR_EVEN: case OPR_ODD:
    case OPR_FINITE: case OPR_INFINITE: case OPR_IMAG: case OPR_LEFT: case OPR_BOTTOM:
    case OPR_NAN: case OPR_NEWSEQ: case OPR_NEWREG: case OPR_NEWUSET: case OPR_REAL: case OPR_RIGHT:
    case OPR_TYPEOF: case OPR_TEMPTY: case OPR_TFILLED: case OPR_TOP:
    case OPR_TYPE: case OPR_UNASSIGNED: case OPR_FRACTIONAL: case OPR_POP:
    case OPR_NUMERIC: case OPR_TMULUP: case OPR_QMDEV: case OPR_INTEGRAL: case OPR_ZERO: case OPR_NONZERO: {
      luaK_exp2anyreg(fs, e);  /* cannot operate on constants */
      codearithbypass(fs, 0, e, op);
      break;
    }
    /* for those functions which return a (complex) number */
    /* GREP "debug.unused" if you change this check (ldblib.c) */
    case OPR_ABS: case OPR_ARCTAN: case OPR_COS: case OPR_ENTIER: case OPR_EXP:
    case OPR_LNGAMMA: case OPR_INT: case OPR_FRAC: case OPR_LN: case OPR_SIGN: case OPR_SIN:
    case OPR_SQRT: case OPR_TAN: case OPR_ARCSIN: case OPR_ARCCOS: case OPR_ARCSEC:
    case OPR_SINH: case OPR_COSH: case OPR_TANH: case OPR_BNOT: case OPR_RECIP: case OPR_COSXX:
    case OPR_BEA: case OPR_FLIP: case OPR_CONJUGATE: case OPR_ANTILOG2: case OPR_ANTILOG10:
    case OPR_SIGNUM: case OPR_SINC: case OPR_CIS: case OPR_PEPS: case OPR_MEPS: case OPR_CELL:
    case OPR_SQUARE: case OPR_CUBE: case OPR_INVSQRT: case OPR_UNITY: {
      if (!luaK_isnumeral(e))
        luaK_exp2anyreg(fs, e);  /* cannot operate on constants */
      codearithbypass(fs, 1, e, op);
      break;
    }
    default: lua_assert(0);
  };
}


/*
** Process 1st operand 'v' of binary operation 'op' before reading
** 2nd operand.
** grep "ORDER OPR" if you change these enums (array `priority') -> lparser.c
*/
void luaK_infix (FuncState *fs, BinOpr op, expdesc *v) {
  switch (op) {
    case OPR_AND: {
      luaK_goiftrue(fs, v);
      break;
    }
    case OPR_OR: {
      luaK_goiffalse(fs, v);
      break;
    }
    case OPR_CONCAT:
    case OPR_SPLIT:          /* added 0.6.0 */
    case OPR_IN:             /* added 0.7.1 */
    case OPR_TSUBSET:        /* added 0.6.0 */
    case OPR_TXSUBSET:       /* added 0.9.1 */
    case OPR_TUNION:         /* added 0.6.0 */
    case OPR_TINTERSECT:     /* added 0.6.0 */
    case OPR_TMINUS:         /* added 0.6.0 */
    case OPR_TEEQ:           /* added 0.22.0 */
    case OPR_TAEQ:           /* added 2.1.4 */
    case OPR_XOR:
    case OPR_ATENDOF:
    case OPR_BAND:
    case OPR_BOR:
    case OPR_BXOR:
    case OPR_TOFTYPE:
    case OPR_TNOTOFTYPE:
    case OPR_PERCENT:
    case OPR_PERCENTRATIO:
    case OPR_PERCENTADD:
    case OPR_PERCENTSUB:
    case OPR_PERCENTCHANGE:
    case OPR_MAP:
    case OPR_SELECT:
    case OPR_HAS:
    case OPR_COUNT:
    case OPR_BLEFT:
    case OPR_BRIGHT:
    case OPR_NAEQ:
    case OPR_NAND:
    case OPR_NOR:
    case OPR_XNOR:
    case OPR_LBROTATE:
    case OPR_RBROTATE:
    case OPR_COMPARE:
    case OPR_ACOMPARE:
    case OPR_SYMMOD:
    case OPR_ROLL:
    case OPR_I32ADD:
    case OPR_I32SUB:
    case OPR_I32MUL:
    case OPR_I32DIV:
    case OPR_SQUAREADD:
    case OPR_NOTIN:
    case OPR_INC:
    case OPR_DEC:
    case OPR_MULTIPLY:
    case OPR_DIVIDE:
    case OPR_INTDIVIDE:
    case OPR_MODULUS:
    case OPR_CARTESIAN: {
      luaK_exp2nextreg(fs, v);  /* operand must be on the `stack' */
      break;
    }
    case OPR_ADD: case OPR_SUB: case OPR_MUL: case OPR_DIV:
    case OPR_MOD: case OPR_POW: case OPR_IPOW: case OPR_ABSDIFF: {
      if (!luaK_isnumeral(v)) luaK_exp2RK(fs, v);
      break;
    }
    /* grep "if (canchainopr(op) && canchainopr(nextop))" in lparser.c */
    case OPR_LT: case OPR_LE:  /* 6.0.0, taken from Pluto 0.12.0, MIT-licenced, check  */
    case OPR_GT: case OPR_GE: {
      if (!luaK_isnumeral(v)) luaK_exp2anyreg(fs, v);
      /* else keep numeral, which may be an immediate operand */
      break;
    }
    default: {
      luaK_exp2RK(fs, v);
      break;
    }
  }
}


/*
** Finalize code for binary operation, after reading 2nd operand.
** For '(a .. b .. c)' (which is '(a .. (b .. c))', because
** concatenation is right associative), merge second CONCAT into first
** one.
*/
#define posfixmacro(fs,e1,e2,op) \
  luaK_exp2val(fs, e2); \
  if (e2->k == VRELOCABLE && GET_OPCODE(getcode(fs, e2)) == op) { \
    lua_assert(e1->u.s.info == GETARG_B(getcode(fs, e2)) - 1); \
    freeexp(fs, e1); \
    SETARG_B(getcode(fs, e2), e1->u.s.info); \
    e1->k = VRELOCABLE; e1->u.s.info = e2->u.s.info; \
  } \
  else { \
    luaK_exp2nextreg(fs, e2); \
    codearith(fs, op, e1, e2); \
  } \
  break;

void luaK_posfix (FuncState *fs, BinOpr op, expdesc *e1, expdesc *e2) {
  switch (op) {
    case OPR_AND: {
      lua_assert(e1->t == NO_JUMP);  /* list must be closed */
      if (e2->k == VFAIL) e2->k = VFALSE;  /* 0.12.3 patch */
      luaK_dischargevars(fs, e2);
      luaK_concat(fs, &e2->f, e1->f);  /* false flags */
      *e1 = *e2;
      break;
    }
    case OPR_OR: {
      lua_assert(e1->f == NO_JUMP);  /* list must be closed */
      luaK_dischargevars(fs, e2);
      luaK_concat(fs, &e2->t, e1->t);  /* true flags */
      *e1 = *e2;
      break;
    }
    case OPR_CONCAT: { posfixmacro(fs, e1, e2, OP_CONCAT); }
    case OPR_ADD: codearith(fs, OP_ADD, e1, e2); break;
    case OPR_SUB: codearith(fs, OP_SUB, e1, e2); break;
    case OPR_MUL: codearith(fs, OP_MUL, e1, e2); break;
    case OPR_DIV: codearith(fs, OP_DIV, e1, e2); break;
    case OPR_MOD: codearith(fs, OP_MOD, e1, e2); break;
    case OPR_POW: codearith(fs, OP_POW, e1, e2); break;
    case OPR_IPOW: codearith(fs, OP_IPOW, e1, e2); break;
    case OPR_INTDIV: codearith(fs, OP_INTDIV, e1, e2); break;                    /* added 0.5.4 */
    case OPR_ABSDIFF: codearith(fs, OP_ABSDIFF, e1, e2); break;                  /* added 2.9.8 */
    case OPR_EQ: codecomp(fs, OP_EQ, 1, e1, e2); break;
    case OPR_NE: codecomp(fs, OP_EQ, 0, e1, e2); break;
    case OPR_LT: codecomp(fs, OP_LT, 1, e1, e2); break;
    case OPR_LE: codecomp(fs, OP_LE, 1, e1, e2); break;
    case OPR_GT: codecomp(fs, OP_LT, 0, e1, e2); break;
    case OPR_GE: codecomp(fs, OP_LE, 0, e1, e2); break;
    case OPR_PAIR: codepair(fs, OP_PAIR, e1, e2); break;                         /* added 0.11.1 */
    case OPR_COMPLEX: codepair(fs, OP_COMPLEX, e1, e2); break;                   /* added 0.11.1 */
    case OPR_CARTESIAN: agnK_codefnbin(fs, e1, e2, OPR_CARTESIAN); break;        /* added 3.10.5 */
    case OPR_SPLIT: { posfixmacro(fs, e1, e2, OP_SPLIT); }                       /* added 0.6.0 */
    case OPR_IN: { posfixmacro(fs, e1, e2, OP_IN); }                             /* added 0.7.1 */
    case OPR_TSUBSET: { posfixmacro(fs, e1, e2, OP_TSUBSET); }                   /* added 0.6.0 */
    case OPR_TXSUBSET: { posfixmacro(fs, e1, e2, OP_TXSUBSET); }                 /* added 0.9.1 */
    case OPR_TUNION: { posfixmacro(fs, e1, e2, OP_TUNION); }                     /* added 0.6.0 */
    case OPR_TMINUS: { posfixmacro(fs, e1, e2, OP_TMINUS); }                     /* added 0.6.0 */
    case OPR_TINTERSECT: { posfixmacro(fs, e1, e2, OP_TINTERSECT); }             /* added 0.6.0 */
    case OPR_TEEQ: { posfixmacro(fs, e1, e2, OP_TEEQ); }                         /* added 0.22.0 */
    case OPR_TAEQ: { posfixmacro(fs, e1, e2, OP_TAEQ); }                         /* added 2.1.4 */
    case OPR_XOR: { agnK_codefnbin(fs, e1, e2, OPR_XOR); break; }                /* added 0.27.0, fixed 1.6.3 */
    case OPR_ATENDOF: { agnK_codefnbin(fs, e1, e2, OPR_ATENDOF); break; }        /* added 0.27.0, fixed 1.6.3 */
    case OPR_BAND: { agnK_codefnbin(fs, e1, e2, OPR_BAND);  break; }             /* added 0.27.0, fixed 1.6.3 */
    case OPR_BOR: { agnK_codefnbin(fs, e1, e2, OPR_BOR); break; }                /* added 0.27.0, fixed 1.6.3 */
    case OPR_BXOR: { agnK_codefnbin(fs, e1, e2, OPR_BXOR); break; }              /* added 0.27.0, fixed 1.6.3 */
    case OPR_TOFTYPE: { agnK_codefnbin(fs, e1, e2, OPR_TOFTYPE); break; }        /* added 1.3.0, fixed 1.6.3 */
    case OPR_TNOTOFTYPE: { agnK_codefnbin(fs, e1, e2, OPR_TNOTOFTYPE); break; }  /* added 1.3.0, fixed 1.6.3 */
    case OPR_PERCENT: { agnK_codefnbin(fs, e1, e2, OPR_PERCENT); break; }        /* added 1.10.6 */
    case OPR_PERCENTRATIO: { agnK_codefnbin(fs, e1, e2, OPR_PERCENTRATIO); break; }  /* added 1.11.4 */
    case OPR_PERCENTCHANGE: { agnK_codefnbin(fs, e1, e2, OPR_PERCENTCHANGE); break; }  /* added 2.10.0*/
    case OPR_PERCENTADD: { agnK_codefnbin(fs, e1, e2, OPR_PERCENTADD); break; }  /* added 1.11.3 */
    case OPR_PERCENTSUB: { agnK_codefnbin(fs, e1, e2, OPR_PERCENTSUB); break; }  /* added 1.11.3 */
    case OPR_MAP: { agnK_codefnbin(fs, e1, e2, OPR_MAP); break; }                /* added 2.2.0 */
    case OPR_SELECT: { agnK_codefnbin(fs, e1, e2, OPR_SELECT); break; }          /* added 2.2.5 */
    case OPR_HAS: { agnK_codefnbin(fs, e1, e2, OPR_HAS); break; }                /* added 2.23.0 */
    case OPR_COUNT: { agnK_codefnbin(fs, e1, e2, OPR_COUNT); break; }            /* added 4.6.0 */
    case OPR_BLEFT: { agnK_codefnbin(fs, e1, e2, OPR_BLEFT); break; }            /* added 2.3.0 RC 4 */
    case OPR_BRIGHT: { agnK_codefnbin(fs, e1, e2, OPR_BRIGHT); break; }          /* added 2.3.0 RC 4 */
    case OPR_NAEQ: { agnK_codefnbin(fs, e1, e2, OPR_NAEQ); break; }              /* added 2.5.2 */
    case OPR_NAND: { agnK_codefnbin(fs, e1, e2, OPR_NAND); break; }              /* added 2.5.2 */
    case OPR_NOR: { agnK_codefnbin(fs, e1, e2, OPR_NOR); break; }                /* added 2.5.2 */
    case OPR_XNOR: { agnK_codefnbin(fs, e1, e2, OPR_XNOR); break; }              /* added 2.8.5 */
    case OPR_LBROTATE: { agnK_codefnbin(fs, e1, e2, OPR_LBROTATE); break; }      /* added 2.5.4 */
    case OPR_RBROTATE: { agnK_codefnbin(fs, e1, e2, OPR_RBROTATE); break; }      /* added 2.5.4 */
    case OPR_COMPARE: { agnK_codefnbin(fs, e1, e2, OPR_COMPARE); break; }        /* added 2.9.4 */
    case OPR_ACOMPARE: { agnK_codefnbin(fs, e1, e2, OPR_ACOMPARE); break; }      /* 2.15.0 fix */
    case OPR_SYMMOD: { agnK_codefnbin(fs, e1, e2, OPR_SYMMOD); break; }          /* added 2.10.0 */
    case OPR_ROLL: { agnK_codefnbin(fs, e1, e2, OPR_ROLL); break; }              /* added 2.13.0 */
    case OPR_I32ADD: { agnK_codefnbin(fs, e1, e2, OPR_I32ADD); break; }          /* added 2.15.0 */
    case OPR_I32SUB: { agnK_codefnbin(fs, e1, e2, OPR_I32SUB); break; }          /* added 2.15.0 */
    case OPR_I32MUL: { agnK_codefnbin(fs, e1, e2, OPR_I32MUL); break; }          /* added 2.15.0 */
    case OPR_I32DIV: { agnK_codefnbin(fs, e1, e2, OPR_I32DIV); break; }          /* added 2.15.0 */
    case OPR_SQUAREADD: { agnK_codefnbin(fs, e1, e2, OPR_SQUAREADD); break; }    /* added 2.16.13 */
    case OPR_NOTIN: { agnK_codefnbin(fs, e1, e2, OPR_NOTIN); break; }            /* added 2.16.14 */
    case OPR_INC: { agnK_codefnbin(fs, e1, e2, OPR_INC); break; }                /* added 2.32.0 */
    case OPR_DEC: { agnK_codefnbin(fs, e1, e2, OPR_DEC); break; }                /* added 2.32.0 */
    case OPR_MULTIPLY: { agnK_codefnbin(fs, e1, e2, OPR_MULTIPLY); break; }      /* added 2.32.0 */
    case OPR_DIVIDE: { agnK_codefnbin(fs, e1, e2, OPR_DIVIDE); break; }          /* added 2.32.0 */
    case OPR_INTDIVIDE: { agnK_codefnbin(fs, e1, e2, OPR_INTDIVIDE); break; }    /* added 2.32.0 */
    case OPR_MODULUS: { agnK_codefnbin(fs, e1, e2, OPR_MODULUS); break; }        /* added 2.32.0 */
    default: lua_assert(0);
  }
}


/*
** Change line information associated with current position.
*/
void luaK_fixline (FuncState *fs, int line) {
  fs->f->lineinfo[fs->pc - 1] = line;
}


static int luaK_code (FuncState *fs, Instruction i, int line) {
  Proto *f = fs->f;
  dischargejpc(fs);  /* `pc' will change */
  /* put new instruction in code array */
  luaM_growvector(fs->L, f->code, fs->pc, f->sizecode, Instruction,
                  MAX_INT, "code size overflow");
  f->code[fs->pc] = i;
  /* save corresponding line information */
  luaM_growvector(fs->L, f->lineinfo, fs->pc, f->sizelineinfo, int,
                  MAX_INT, "code size overflow");
  f->lineinfo[fs->pc] = line;
  return fs->pc++;
}


int luaK_codeABC (FuncState *fs, OpCode o, int a, int b, int c) {
  lua_assert(getOpMode(o) == iABC);
  lua_assert(getBMode(o) != OpArgN || b == 0);
  lua_assert(getCMode(o) != OpArgN || c == 0);
  return luaK_code(fs, CREATE_ABC(o, a, b, c), fs->ls->lastline);
}


int luaK_codeABx (FuncState *fs, OpCode o, int a, unsigned int bc) {
  lua_assert(getOpMode(o) == iABx || getOpMode(o) == iAsBx);
  lua_assert(getCMode(o) == OpArgN);
  return luaK_code(fs, CREATE_ABx(o, a, bc), fs->ls->lastline);
}


/*
** Emit a SETLIST instruction.
** 'base' is register that keeps table;
** 'nelems' is #table plus those to be stored now;
** 'tostore' is number of values (in registers 'base + 1',...) to add to
** table (or LUA_MULTRET to add up to stack top).
*/
void luaK_setlist (FuncState *fs, int base, int nelems, int tostore, int isvector) {
  int c = (nelems - 1)/LFIELDS_PER_FLUSH + 1;
  int b = (tostore == LUA_MULTRET) ? 0 : tostore;
  lua_assert(tostore != 0);
  if (c <= MAXARG_C)
    luaK_codeABC(fs, isvector ? OP_SETVECTOR : OP_SETLIST, base, b, c);
  else {
    luaK_codeABC(fs, isvector ? OP_SETVECTOR : OP_SETLIST, base, b, 0);
    luaK_code(fs, cast(Instruction, c), fs->ls->lastline);
  }
  fs->freereg = base + 1;  /* free registers with list values */
}


/* for IF operator, 0.8.0, December 20, 2007 */
void luaK_ifoperation (FuncState *fs, expdesc *e, int base) {
  int p;
  luaK_exp2nextreg(fs, e);
  p = luaK_exp2RK(fs, e);
  freeexp(fs, e);
  e->u.s.info = luaK_codeABC(fs, OP_MOVE, base, p, 0);
  e->k = VRELOCABLE;
}


/* 2.5.4, taken from Lua 5.1.5: e = table, key = function name; see OP_SELF in lvm.c for what this call achieves */
/*
** Emit SELF instruction (convert expression 'e' into 'e:key(e,').
*/
void luaK_self (FuncState *fs, expdesc *e, expdesc *key) {
  int func;
  luaK_exp2anyreg(fs, e);  /* put table into register */
  freeexp(fs, e);
  func = fs->freereg;
  luaK_reserveregs(fs, 2);  /* reserve two more registers */
  /* copy table, retrieve table.procedure, put procedure into ra slot, put table into ra+1 slot, the result is a
     function call procedure(table, ...) */
  luaK_codeABC(fs, OP_SELF, func, e->u.s.info, luaK_exp2RK(fs, key));
  freeexp(fs, key);
  e->u.s.info = func;
  e->k = VNONRELOC;  /* 2.24.0 fix; formerly VRELOCABLE; VNONRELOC is the original Lua setting */
}

