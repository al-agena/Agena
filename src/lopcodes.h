/*
** $Id: lopcodes.h,v 1.125 2006/03/14 19:04:44 roberto Exp $
** Opcodes for Lua virtual machine
** See Copyright Notice in agena.h
*/

#ifndef lopcodes_h
#define lopcodes_h

#include "llimits.h"


/*===========================================================================
  We assume that instructions are unsigned numbers.
  All instructions have an opcode in the first 6 bits.
  Instructions can have the following fields:
   `A' : 8 bits
   `B' : 9 bits
   `C' : 9 bits
   `Bx' : 18 bits (`B' and `C' together)
   `sBx' : signed Bx

  A signed argument is represented in excess K; that is, the number
  value is the unsigned value minus K. K is exactly the maximum value
  for that argument (so that -max is represented by 0, and +max is
  represented by 2*max), which is half the maximum for the corresponding
  unsigned argument.
===========================================================================*/


enum OpMode {iABC, iABx, iAsBx};  /* basic instruction format */


/*
** size and position of opcode arguments.
*/
#define SIZE_C      9
#define SIZE_B      9
#define SIZE_Bx     (SIZE_C + SIZE_B)
#define SIZE_A      8

#define SIZE_OP     6

#define POS_OP      0
#define POS_A       (POS_OP + SIZE_OP)
#define POS_C       (POS_A + SIZE_A)
#define POS_B       (POS_C + SIZE_C)
#define POS_Bx      POS_C


/*
** limits for opcode arguments.
** we use (signed) int to manipulate most arguments,
** so they must fit in LUAI_BITSINT-1 bits (-1 for sign)
*/
#if SIZE_Bx < LUAI_BITSINT-1
#define MAXARG_Bx        ((1<<SIZE_Bx)-1)
#define MAXARG_sBx       (MAXARG_Bx>>1)         /* `sBx' is signed */
#else
#define MAXARG_Bx        MAX_INT
#define MAXARG_sBx       MAX_INT
#endif


#define MAXARG_A        ((1<<SIZE_A)-1)
#define MAXARG_B        ((1<<SIZE_B)-1)
#define MAXARG_C        ((1<<SIZE_C)-1)


/* creates a mask with `n' 1 bits at position `p' */
#define MASK1(n,p)   ((~((~(Instruction)0)<<n))<<p)

/* creates a mask with `n' 0 bits at position `p' */
#define MASK0(n,p)   (~MASK1(n,p))

/*
** the following macros help to manipulate instructions
*/

#define GET_OPCODE(i)   (cast(OpCode, ((i)>>POS_OP) & MASK1(SIZE_OP,0)))
#define SET_OPCODE(i,o)   ((i) = (((i)&MASK0(SIZE_OP,POS_OP)) | \
      ((cast(Instruction, o)<<POS_OP)&MASK1(SIZE_OP,POS_OP))))

#define GETARG_A(i)   (cast(int, ((i)>>POS_A) & MASK1(SIZE_A,0)))
#define SETARG_A(i,u)   ((i) = (((i)&MASK0(SIZE_A,POS_A)) | \
      ((cast(Instruction, u)<<POS_A)&MASK1(SIZE_A,POS_A))))

#define GETARG_B(i)   (cast(int, ((i)>>POS_B) & MASK1(SIZE_B,0)))
#define SETARG_B(i,b)   ((i) = (((i)&MASK0(SIZE_B,POS_B)) | \
      ((cast(Instruction, b)<<POS_B)&MASK1(SIZE_B,POS_B))))

#define GETARG_C(i)   (cast(int, ((i)>>POS_C) & MASK1(SIZE_C,0)))
#define SETARG_C(i,b)   ((i) = (((i)&MASK0(SIZE_C,POS_C)) | \
      ((cast(Instruction, b)<<POS_C)&MASK1(SIZE_C,POS_C))))

#define GETARG_Bx(i)   (cast(int, ((i)>>POS_Bx) & MASK1(SIZE_Bx,0)))
#define SETARG_Bx(i,b)   ((i) = (((i)&MASK0(SIZE_Bx,POS_Bx)) | \
      ((cast(Instruction, b)<<POS_Bx)&MASK1(SIZE_Bx,POS_Bx))))

#define GETARG_sBx(i)   (GETARG_Bx(i)-MAXARG_sBx)
#define SETARG_sBx(i,b)   SETARG_Bx((i),cast(unsigned int, (b)+MAXARG_sBx))


#define CREATE_ABC(o,a,b,c)   ((cast(Instruction, o)<<POS_OP) \
         | (cast(Instruction, a)<<POS_A) \
         | (cast(Instruction, b)<<POS_B) \
         | (cast(Instruction, c)<<POS_C))

#define CREATE_ABx(o,a,bc)   ((cast(Instruction, o)<<POS_OP) \
         | (cast(Instruction, a)<<POS_A) \
         | (cast(Instruction, bc)<<POS_Bx))


/*
** Macros to operate RK indices
*/

/* this bit 1 means constant (0 means register) */
#define BITRK       (1 << (SIZE_B - 1))

/* test whether value is a constant */
#define ISK(x)      ((x) & BITRK)

/* gets the index of the constant */
#define INDEXK(r)   ((int)(r) & ~BITRK)

#define MAXINDEXRK  (BITRK - 1)

/* code a constant index as a RK value */
#define RKASK(x)    ((x) | BITRK)

/*
** invalid register that fits in 8 bits
*/
#define NO_REG      MAXARG_A

/*
** R(x) - register
** Kst(x) - constant (in constant table)
** RK(x) == if ISK(x) then Kst(INDEXK(x)) else R(x)
*/

typedef enum {
  CMD_CASE, CMD_BYE, CMD_RESTART, CMD_CLS, CMD_GC, CMD_POPBOTTOM, CMD_POPTOP, CMD_IMPORT, CMD_ALIAS,
  CMD_ROTATETOP, CMD_ROTATEBOTTOM, CMD_DUPLICATE, CMD_EXCHANGE, CMD_REMEMBER, CMD_AMONG, CMD_INSERTINTO,
  CMD_DELETEFROM, CMD_PUSHD, CMD_SWITCHD, CMD_IMPORTAS, CMD_STORE
} CmdCode;

/*
** grep "ORDER OP" if you change these enums.
** A maximum of 64 opcodes are allowed on 32-bit machines.
*/

typedef enum {
/*----------------------------------------------------------------------
name      args   description
------------------------------------------------------------------------*/
OP_MOVE,/*   A B   R(A) := R(B)               */
OP_LOADK,/*   A Bx   R(A) := Kst(Bx)               */
OP_LOADBOOL,/*   A B C   R(A) := (Bool)B; if (C) pc++         */
OP_LOADNIL,/*   A B   R(A) := ... := R(B) := NULL         */
OP_GETUPVAL,/*   A B   R(A) := UpValue[B]            */
OP_GETGLOBAL,/*   A Bx   R(A) := Gbl[Kst(Bx)]            */
OP_GETTABLE,/*   A B C   R(A) := R(B)[RK(C)]            */
OP_IN,   /* added 0.7.1 */
OP_SETGLOBAL,/*   A Bx   Gbl[Kst(Bx)] := R(A)            */
OP_SETUPVAL,/*   A B   UpValue[B] := R(A)            */
OP_SETTABLE,/*   A B C   R(A)[RK(B)] := RK(C)            */
OP_NEWTABLE,/*   A B C   R(A) := {} (size = B,C)            */
OP_SELF,/*	A B C	R(A+1) := R(B); R(A) := R(B)[RK(C)]		*/
OP_TSUBSET, /* table subset operator, 0.6.0 */
OP_TXSUBSET, /* binary table subset operator, 0.9.1 */
OP_TUNION, /* table union operator, 0.6.0 */
OP_TMINUS, /* table minus operator, 0.6.0 */
OP_TINTERSECT, /* table intersection operator, 0.6.0 */
OP_CMD,
OP_ADD,/*   A B C   R(A) := RK(B) + RK(C)            */
OP_SUB,/*   A B C   R(A) := RK(B) - RK(C)            */
OP_MUL,/*   A B C   R(A) := RK(B) * RK(C)            */
OP_DIV,/*   A B C   R(A) := RK(B) / RK(C)            */
OP_INTDIV,/*   A B C   R(A) := RK(B) \ RK(C)  added 0.5.4 */
OP_MOD,/*   A B C   R(A) := RK(B) % RK(C)            */
OP_POW,/*   A B C   R(A) := RK(B) ^ RK(C)            */
OP_IPOW,/*   A B C   R(A) := RK(B) ** RK(C)          */
OP_UNM,/*   A B   R(A) := -R(B)                     */
OP_ABSDIFF, /* absolute difference, 2.9.8 */
OP_PAIR,
OP_COMPLEX,
OP_TEEQ,
OP_TAEQ,
OP_NOT,/*   A B   R(A) := not R(B)                  */
OP_LEN,/*   A B   R(A) := length of R(B)            */
OP_CONCAT,/*   A B C   R(A) := R(B).. ... ..R(C)    */
OP_SPLIT,     /* added 0.6.0 */
OP_FN,
OP_FNBIN,   /* added 0.27.0 */
OP_JMP,/*   sBx pc+=sBx               */
OP_TRY,/* 	sBx	pc+=sBx, 2.1 RC 2					*/
OP_ENDTRY,  /* 2.1 RC 2 */
OP_CATCH, /* A  R(A)=errorobj, 2.1 RC 2 */
OP_EQ,/*   A B C   if ((RK(B) == RK(C)) ~= A) then pc++  */
OP_LT,/*   A B C   if ((RK(B) <  RK(C)) ~= A) then pc++  */
OP_LE,/*   A B C   if ((RK(B) <= RK(C)) ~= A) then pc++  */
OP_TEST,/*   A C   if not (R(A) <=> C) then pc++         */
OP_TESTSET,/*   A B C   if (R(B) <=> C) then R(A) := R(B) else pc++   */
OP_CALL,/*   A B C   R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
OP_TAILCALL,/*   A B C   return R(A)(R(A+1), ... ,R(A+B-1)) */
OP_RETURN,/*   A B   return R(A), ... ,R(A+B-2)   (see note) */
OP_FORLOOP,/*   A sBx   R(A)+=R(A+2); if R(A) <?= R(A+1) then { pc+=sBx; R(A+3)=R(A) }*/
OP_FORPREP,/*   A sBx   R(A)-=R(A+2); pc+=sBx            */
OP_TFORLOOP,/*   A C   R(A+3), ... ,R(A+2+C) := R(A)(R(A+1), R(A+2)); if R(A+3) ~= nil then R(A+2)=R(A+3) else pc++ */
OP_SETLIST,/*   A B C   R(A)[(C-1)*FPF+i] := R(A+i), 1 <= i <= B   */
OP_CLOSE,/*   A    close all variables in the stack up to (>=) R(A)*/
OP_CLOSURE,/*   A Bx   R(A) := closure(KPROTO[Bx], R(A), ... ,R(A+n))   */
OP_DOTTED,
OP_TFORPREP, /*   A sBx   if type(R(A)) == table then R(A+1):=R(A), R(A):=next; PC += sBx  0.5.0, taken from Lua 5.0.3 */
OP_SETVECTOR,/*   A B C   R(A)[(C-1)*FPF+i] := R(A+i), 1 <= i <= B   */
} OpCode;

/* 60 assigned opcodes */

#define NUM_OPCODES   (cast(int, OP_SETVECTOR) + 1)  /* 7.0.1 fix, always use the last entry in the table above. */


#define AGENA_DISPATCH_TABLE_INIT \
  &&LABEL_OP_MOVE, \
  &&LABEL_OP_LOADK, \
  &&LABEL_OP_LOADBOOL, \
  &&LABEL_OP_LOADNIL, \
  &&LABEL_OP_GETUPVAL, \
  &&LABEL_OP_GETGLOBAL, \
  &&LABEL_OP_GETTABLE, \
  &&LABEL_OP_IN, \
  &&LABEL_OP_SETGLOBAL, \
  &&LABEL_OP_SETUPVAL, \
  &&LABEL_OP_SETTABLE, \
  &&LABEL_OP_NEWTABLE, \
  &&LABEL_OP_SELF, \
  &&LABEL_OP_TSUBSET, \
  &&LABEL_OP_TXSUBSET, \
  &&LABEL_OP_TUNION, \
  &&LABEL_OP_TMINUS, \
  &&LABEL_OP_TINTERSECT, \
  &&LABEL_OP_CMD, \
  &&LABEL_OP_ADD, \
  &&LABEL_OP_SUB, \
  &&LABEL_OP_MUL, \
  &&LABEL_OP_DIV, \
  &&LABEL_OP_INTDIV, \
  &&LABEL_OP_MOD, \
  &&LABEL_OP_POW, \
  &&LABEL_OP_IPOW, \
  &&LABEL_OP_UNM, \
  &&LABEL_OP_ABSDIFF, \
  &&LABEL_OP_PAIR, \
  &&LABEL_OP_COMPLEX, \
  &&LABEL_OP_TEEQ, \
  &&LABEL_OP_TAEQ, \
  &&LABEL_OP_NOT, \
  &&LABEL_OP_LEN, \
  &&LABEL_OP_CONCAT, \
  &&LABEL_OP_SPLIT, \
  &&LABEL_OP_FN, \
  &&LABEL_OP_FNBIN, \
  &&LABEL_OP_JMP, \
  &&LABEL_OP_TRY, \
  &&LABEL_OP_ENDTRY, \
  &&LABEL_OP_CATCH, \
  &&LABEL_OP_EQ, \
  &&LABEL_OP_LT, \
  &&LABEL_OP_LE, \
  &&LABEL_OP_TEST, \
  &&LABEL_OP_TESTSET, \
  &&LABEL_OP_CALL, \
  &&LABEL_OP_TAILCALL, \
  &&LABEL_OP_RETURN, \
  &&LABEL_OP_FORLOOP, \
  &&LABEL_OP_FORPREP, \
  &&LABEL_OP_TFORLOOP, \
  &&LABEL_OP_SETLIST, \
  &&LABEL_OP_CLOSE, \
  &&LABEL_OP_CLOSURE, \
  &&LABEL_OP_DOTTED, \
  &&LABEL_OP_TFORPREP, \
  &&LABEL_OP_SETVECTOR

/*===========================================================================
  Notes:
  (*) In OP_CALL, if (B == 0) then B = top. C is the number of returns - 1,
      and can be 0: OP_CALL then sets `top' to last_result+1, so
      next open instruction (OP_CALL, OP_RETURN, OP_SETLIST) may use `top'.

  (*) In OP_VARARG, if (B == 0) then use actual number of varargs and
      set top (like in OP_CALL with C == 0).

  (*) In OP_RETURN, if (B == 0) then return up to `top'

  (*) In OP_SETLIST, if (B == 0) then B = `top';
      if (C == 0) then next `instruction' is real C

  (*) For comparisons, A specifies what condition the test should accept
      (true or false).

  (*) All `skips' (pc++) assume that next instruction is a jump
===========================================================================*/

/*
** masks for instruction properties. The format is:
** bits 0-1: op mode
** bits 2-3: C arg mode
** bits 4-5: B arg mode
** bit 6: instruction set register A
** bit 7: operator is a test
*/

enum OpArgMask {
  OpArgN,  /* argument is not used */
  OpArgU,  /* argument is used */
  OpArgR,  /* argument is a register or a jump offset */
  OpArgK   /* argument is a constant or register/constant */
};

LUAI_DATA const lu_byte luaP_opmodes[NUM_OPCODES];

#define getOpMode(m)   (cast(enum OpMode, luaP_opmodes[m] & 3))
#define getBMode(m)    (cast(enum OpArgMask, (luaP_opmodes[m] >> 4) & 3))
#define getCMode(m)    (cast(enum OpArgMask, (luaP_opmodes[m] >> 2) & 3))
#define testAMode(m)   (luaP_opmodes[m] & (1 << 6))
#define testTMode(m)   (luaP_opmodes[m] & (1 << 7))

LUAI_DATA const char *const luaP_opnames[NUM_OPCODES + 1];  /* opcode names */

/* number of list items to accumulate before a SETLIST instruction */
#define LFIELDS_PER_FLUSH   50

#endif

