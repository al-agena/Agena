/*
** $Id: lopcodes.c,v 1.37 2005/11/08 19:45:36 roberto Exp $
** See Copyright Notice in agena.h
*/


#define lopcodes_c
#define LUA_CORE


#include "lopcodes.h"


/* ORDER OP */
/* A maximum of 64 opcodes are allowed on 32-bit machines */

const char *const luaP_opnames[NUM_OPCODES + 1] = {
  "MOVE",
  "LOADK",
  "LOADBOOL",
  "LOADNIL",
  "GETUPVAL",
  "GETGLOBAL",
  "GETTABLE",
  "IN",         /* added 0.7.1 */
  "SETGLOBAL",
  "SETUPVAL",
  "SETTABLE",
  "NEWTABLE",
  "SELF",
  "TSUBSET",    /* added 0.6.0 */
  "TXSUBSET",   /* added 0.9.1 */
  "TUNION",     /* added 0.6.0 */
  "TMINUS",     /* added 0.6.0 */
  "TINTERSECT", /* added 0.6.0 */
  "CMD",
  "ADD",
  "SUB",
  "MUL",
  "DIV",
  "INTDIV",     /* added 0.5.4 */
  "MOD",
  "POW",
  "IPOW",       /* added 0.9.2 */
  "UNM",
  "ABSDIFF",    /* added 2.9.8 */
  "PAIR",       /* added 0.11.1 */
  "COMPLEX",    /* added 0.12.0 */
  "TEEQ",       /* added 0.22.0 */
  "TAEQ",       /* added 2.1.4 */
  "NOT",
  "LEN",
  "CONCAT",
  "SPLIT",      /* added 0.6.0 */
  "FN",         /* added 0.7.1 */
  "FNBIN",      /* added 0.27.0 */
  "JMP",
  "TRY",        /* added 2.1 RC 2 */
  "ENDTRY",     /* added 2.1 RC 2 */
  "CATCH",      /* added 2.1 RC 2 */
  "EQ",
  "LT",
  "LE",
  "TEST",
  "TESTSET",
  "CALL",
  "TAILCALL",
  "RETURN",
  "FORLOOP",
  "FORPREP",
  "TFORLOOP",
  "SETLIST",
  "CLOSE",
  "CLOSURE",
  "DOTTED",
  "TFORPREP",
  "SETVECTOR",
  NULL
};


#define opmode(t,a,b,c,m) (((t)<<7) | ((a)<<6) | ((b)<<4) | ((c)<<2) | (m))

const lu_byte luaP_opmodes[NUM_OPCODES] = {
/*       T  A    B       C     mode       opcode   */
  opmode(0, 1, OpArgR, OpArgN, iABC)      /* OP_MOVE */
 ,opmode(0, 1, OpArgK, OpArgN, iABx)      /* OP_LOADK */
 ,opmode(0, 1, OpArgU, OpArgU, iABC)      /* OP_LOADBOOL */
 ,opmode(0, 1, OpArgR, OpArgN, iABC)      /* OP_LOADNIL */
 ,opmode(0, 1, OpArgU, OpArgN, iABC)      /* OP_GETUPVAL */
 ,opmode(0, 1, OpArgK, OpArgN, iABx)      /* OP_GETGLOBAL */
 ,opmode(0, 1, OpArgR, OpArgK, iABC)      /* OP_GETTABLE */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)      /* OP_IN, added 0.7.1 */
 ,opmode(0, 0, OpArgK, OpArgN, iABx)      /* OP_SETGLOBAL */
 ,opmode(0, 0, OpArgU, OpArgN, iABC)      /* OP_SETUPVAL */
 ,opmode(0, 0, OpArgK, OpArgK, iABC)      /* OP_SETTABLE */
 ,opmode(0, 1, OpArgU, OpArgU, iABC)      /* OP_NEWTABLE */
 ,opmode(0, 1, OpArgR, OpArgK, iABC)      /* OP_SELF */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)      /* OP_TSUBSET, added 0.6.0 */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)      /* OP_TXSUBSET, added 0.9.1 */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)      /* OP_TUNION, added 0.6.0 */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)      /* OP_TMINUS, added 0.6.0 */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)      /* OP_TINTERSECT, added 0.6.0 */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)      /* OP_CMD, added 0.6.0 */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_ADD */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_SUB */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_MUL */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_DIV */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_INTDIV, added 0.5.4 */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_MOD */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_POW */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_IPOW, added 0.9.2 */
 ,opmode(0, 1, OpArgR, OpArgN, iABC)      /* OP_UNM */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_ABSDIFF, added 2.9.8 */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_PAIR, added 0.11.1, changed 1.6.1 */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_COMPLEX, added 0.12.0 */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)      /* OP_TEEQ, added 0.22.0 */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)      /* OP_TAEQ, added 2.1.4 */
 ,opmode(0, 1, OpArgR, OpArgN, iABC)      /* OP_NOT */
 ,opmode(0, 1, OpArgR, OpArgN, iABC)      /* OP_LEN */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)      /* OP_CONCAT */
 ,opmode(0, 1, OpArgR, OpArgR, iABC)      /* OP_SPLIT, added 0.6.0 */
 ,opmode(0, 1, OpArgR, OpArgU, iABC)      /* OP_FN, added 0.7.1; changed from OpArgR to OpArgU, 1.6.1 */
 ,opmode(0, 1, OpArgR, OpArgU, iABC)      /* OP_FNBIN, added 0.27.0; changed from OpArgR to OpArgU, 1.6.1 */
 ,opmode(0, 0, OpArgR, OpArgN, iAsBx)     /* OP_JMP */
 ,opmode(0, 0, OpArgR, OpArgN, iAsBx)     /* OP_TRY, 2.1 RC 2 */
 ,opmode(0, 0, OpArgN, OpArgN, iABC)	    /* OP_ENDTRY, 2.1 RC 2 */
 ,opmode(0, 1, OpArgN, OpArgN, iABC)	    /* OP_CATCH, 2.1 RC 2 */
 ,opmode(1, 0, OpArgK, OpArgK, iABC)      /* OP_EQ */
 ,opmode(1, 0, OpArgK, OpArgK, iABC)      /* OP_LT */
 ,opmode(1, 0, OpArgK, OpArgK, iABC)      /* OP_LE */
 ,opmode(1, 1, OpArgR, OpArgU, iABC)      /* OP_TEST */
 ,opmode(1, 1, OpArgR, OpArgU, iABC)      /* OP_TESTSET */
 ,opmode(0, 1, OpArgU, OpArgU, iABC)      /* OP_CALL */
 ,opmode(0, 1, OpArgU, OpArgU, iABC)      /* OP_TAILCALL */
 ,opmode(0, 0, OpArgU, OpArgN, iABC)      /* OP_RETURN */
 ,opmode(0, 1, OpArgR, OpArgN, iAsBx)     /* OP_FORLOOP */
 ,opmode(0, 1, OpArgR, OpArgN, iAsBx)     /* OP_FORPREP */
 ,opmode(1, 0, OpArgN, OpArgU, iABC)      /* OP_TFORLOOP */
 ,opmode(0, 0, OpArgU, OpArgU, iABC)      /* OP_SETLIST */
 ,opmode(0, 0, OpArgN, OpArgN, iABC)      /* OP_CLOSE */
 ,opmode(0, 1, OpArgU, OpArgN, iABx)      /* OP_CLOSURE */
 ,opmode(0, 1, OpArgK, OpArgK, iABC)      /* OP_DOTTED, added 1.2 */
 ,opmode(0, 0, OpArgR, OpArgN, iAsBx)     /* OP_TFORPREP  0.5.0, taken from Lua 5.0.3 */
 ,opmode(0, 0, OpArgU, OpArgU, iABC)      /* OP_SETVECTOR, 7.0.1 fix */
};

