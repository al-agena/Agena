/*
** $Id: lparser.h,v 1.57 2006/03/09 18:14:31 roberto Exp $
** Lua Parser
** See Copyright Notice in agena.h
*/

#ifndef lparser_h
#define lparser_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/*
** Expression descriptor  (comments refer to Lua 5.3 and were tkane from:
   https://the-ravi-programming-language.readthedocs.io/en/latest/lua-parser.html)
*/

typedef enum {
  VVOID,	/* 0 no value; This is used to indicate the lack of value - e.g. function call with no arguments, the rhs of local variable declaration, and empty table constructor */
  VNIL,   /* 1 */
  VTRUE,  /* 2 */
  VFALSE, /* 3 */
  VK,		  /* 4 info = index of constant in `k' */
  VKNUM,	/* 5 nval = numerical value */
  VLOCAL,	/* 6 info = local register; This is used when referencing local variables. u.info is set to the local variable’s register. */
  VUPVAL, /* 7 info = index of upvalue in `upvalues' */
  VGLOBAL,	/* 8 info = index of table; aux = index of global name in `k' */
  VINDEXED,	/* 9 info = table register; aux = index register (or `k'); This expression represents a table access. The u.ind.t parameter is set to the register or upvalue? that holds the table, the u.ind.idx is set to the register or constant that is the key, and u.ind.vt is either VLOCAL or VUPVAL */
  VJMP,		/* 10 info = instruction pc */
  VRELOCABLE,	/* 11 info = instruction pc; This is used to indicate that the result from expression needs to be set to a register. The operation that created the expression is referenced by the u.info parameter which contains an offset into the code of the function that is being compiled So you can access this instruction by calling getcode(FuncState *, expdesc *) The operations that result in a VRELOCABLE object include OP_CLOSURE OP_NEWTABLE OP_GETUPVAL OP_GETTABUP OP_GETTABLE OP_NOT and code for binary and unary expressions that produce values (arithmetic operations, bitwise operations, concat, length). The associated code instruction has operand A unset (defaulted to 0) - this the VRELOCABLE expression must be later transitioned to VNONRELOC state when the register is set. 	In terms of transitions the following expression kinds convert to VRELOCABLE: VVARARG VUPVAL (OP_GETUPVAL VINDEXED (OP_GETTABUP or OP_GETTABLE And following expression states can result from a VRELOCABLE expression: VNONRELOC which means that the result register in the instruction operand A has been set. */
  VNONRELOC,	/* 12 info = result register; This state indicates that the output or result register has been set. The register is referenced in u.info parameter. Once set the register cannot be changed for this expression; subsequent operations involving this expression can refer to the register to obtain the result value. 	As for transitions, the VNONELOC state results from VRELOCABLE after a register is assigned to the operation referenced by VRELOCABLE. Also a VCALL expression transitions to VNONRELOC expression - u.info is set to the operand A in the call instruction. VLOCAL VNIL VTRUE VFALSE VK VKINT VKFLT and VJMP expressions transition to VNONRELOC. */
  VCALL,	/* 13 info = instruction pc; This results from a function call. The OP_CALL instruction is referenced by u.info parameter and may be retrieved by calling getcode(FuncState *, expdesc *). The OP_CALL instruction gets changed to OP_TAILCALL if the function call expression is the value of a RETURN statement. The instructions operand C gets updated when it is known the number of expected results from the function call. */
  VVARARG,	/* 14 info = instruction pc */
  VFAIL,    /* 0.9.2 */
} expkind;


typedef struct expdesc {
  expkind k;
  union {
    struct { int info, aux; } s;
    lua_Number nval;
  } u;
  int t;            /* patch list of `exit when true' */
  int f;            /* patch list of `exit when false' */
  lu_byte oop;      /* flag indicating that expr will probably become an OOP method, 2.25.0 */
  lu_byte cnst;     /* is variable a constant, 6.0.0 */
  char padding[2];  /* explicit padding (2 bytes remaining in the block) */
} expdesc;


typedef struct upvaldesc {
  lu_byte k;
  lu_byte info;
} upvaldesc;


struct BlockCnt;  /* defined in lparser.c */


/* state needed to generate code for a given function */
typedef struct FuncState {
  Proto *f;  /* current function header */
  Table *h;  /* table to find (and reuse) elements in `k' */
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  struct lua_State *L;  /* copy of the Lua state */
  struct BlockCnt *bl;  /* chain of current blocks */
  int pc;  /* next position to code (equivalent to `ncode') */
  int lasttarget;  /* `pc' of last `jump target' */
  int jpc;  /* list of pending jumps to `pc' */
  int freereg;  /* first free register */
  int nk;  /* number of elements in `k' */
  int np;  /* number of elements in `p' */
  short nlocvars;  /* number of elements in `locvars' */
  lu_byte nactvar;  /* number of active local variables */
  upvaldesc upvalues[LUAI_MAXUPVALUES];  /* upvalues */
  unsigned short actvar[LUAI_MAXVARS];  /* declared-variable stack */
  expdesc with_obj;  /* 7.5.0, WITH TABLE NAME DO ... */
} FuncState;


LUAI_FUNC Proto *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff,
                                            const char *name);

#define UNARY_PRIORITY   8   /* priority for unary operators */

#define UNARYOP_PRIORITY 11  /* priority for new unary operators */

#define HIGHEST_PRIORITY 20  /* priority for functions called in `binary operator mode` */

#endif

