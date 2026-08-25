/*
** $Id: lcode.h,v 1.48 2006/03/21 19:28:03 roberto Exp $
** Code generator for Lua/Agena
** See Copyright Notice in agena.h
*/

#ifndef lcode_h
#define lcode_h

#include "llex.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)


/*
** grep "ORDER OPR" if you change these enums (array `priority') -> lparser.c
*/
typedef enum BinOpr {
  OPR_ADD,
  OPR_SUB,
  OPR_MUL,
  OPR_DIV,
  OPR_MOD,
  OPR_POW,
  OPR_IPOW,
  OPR_INTDIV,      /* added 0.9.2 & 0.5.4 */
  OPR_CONCAT,      /* added 0.9.2 & 0.5.4 */
  OPR_NE,
  OPR_EQ,
  OPR_LT,
  OPR_LE,
  OPR_GT,
  OPR_GE,
  OPR_AND,
  OPR_OR,
  OPR_IN,          /* added 0.7.1 */
  OPR_TSUBSET,     /* added 0.6.0 */
  OPR_TXSUBSET,    /* added 0.9.1 */
  OPR_TUNION,      /* added 0.6.0 */
  OPR_TMINUS,      /* added 0.6.0 */
  OPR_TINTERSECT,  /* added 0.6.0 */
  OPR_SPLIT,       /* added 0.6.0 */
  OPR_PAIR,        /* added 0.11.1 */
  OPR_COMPLEX,     /* added 0.12.0 */
  OPR_CARTESIAN,   /* added 3.10.5 */
  OPR_TEEQ,        /* added 0.22.0 */
  OPR_TAEQ,        /* added 2.1.4 */
  OPR_NAEQ,        /* added 2.5.2 */
  OPR_XOR,         /* added 0.27.0 */
  OPR_ATENDOF,     /* added 0.27.0 */
  OPR_BAND,        /* added 0.27.0 */
  OPR_BOR,         /* added 0.27.0 */
  OPR_BXOR,        /* added 0.27.0 */
  OPR_BLEFT,       /* added 2.3.0 RC4 */
  OPR_BRIGHT,      /* added 2.3.0 RC4 */
  OPR_TOFTYPE,     /* added 1.3.0 */
  OPR_TNOTOFTYPE,  /* added 1.3.0 */
  OPR_PERCENT,     /* added 1.10.6 */
  OPR_PERCENTRATIO,/* added 1.11.4 */
  OPR_PERCENTADD,  /* added 1.11.3 */
  OPR_PERCENTSUB,  /* added 1.11.3 */
  OPR_PERCENTCHANGE,/* added 2.10.0 */
  OPR_MAP,         /* added 2.2.0 */
  OPR_SELECT,      /* added 2.2.5 */
  OPR_HAS,         /* added 2.23.0 */
  OPR_COUNT,       /* added 4.6.0 */
  OPR_NAND,        /* added 2.5.2 */
  OPR_NOR,         /* added 2.5.2 */
  OPR_XNOR,        /* added 2.8.4 */
  OPR_LBROTATE,    /* added 2.5.4 */
  OPR_RBROTATE,    /* added 2.5.4 */
  OPR_COMPARE,     /* added 2.9.4 */
  OPR_ACOMPARE,    /* added 2.10.5 */
  OPR_ABSDIFF,     /* added 2.9.8 */
  OPR_SYMMOD,      /* added 2.10.0 */
  OPR_ROLL,        /* added 2.13.0 */
  OPR_I32ADD,      /* added 2.15.0 */
  OPR_I32SUB,      /* added 2.15.0 */
  OPR_I32MUL,      /* added 2.15.0 */
  OPR_I32DIV,      /* added 2.15.0 */
  OPR_SQUAREADD,   /* added 2.16.13 */
  OPR_NOTIN,       /* added 2.16.14 */
  OPR_INC,         /* added 2.32.0 */
  OPR_DEC,         /* added 2.32.0 */
  OPR_MULTIPLY,    /* added 2.32.0 */
  OPR_DIVIDE,      /* added 2.32.0 */
  OPR_INTDIVIDE,   /* added 2.32.0 */
  OPR_MODULUS,     /* added 2.32.0 */
  OPR_NOBINOPR
} BinOpr;


/* Unary operators; do not move OPR_ABS to another position as it controls general precedence */
typedef enum UnOpr {
  OPR_MINUS, OPR_PLUS, OPR_NOT, OPR_LEN, OPR_ABS,                     /* 05 */
  OPR_ASSIGNED, OPR_TYPE, OPR_TSUMUP, OPR_TQSUMUP, OPR_TMULUP,        /* 10 */
  OPR_ALGFORM, OPR_ANTILOG2, OPR_ANTILOG10, OPR_ARCTAN, OPR_BEA,      /* 15 */
  OPR_BOTTOM, OPR_CIS, OPR_CONJUGATE, OPR_COS,                        /* 19 */
  OPR_COSH, OPR_COSXX, OPR_DIMSEQ, OPR_DIMREG, OPR_DIMTAB,            /* 24 */
  OPR_DIMDICT, OPR_DIMSET, OPR_ENTIER, OPR_EVEN, OPR_EXP,             /* 29 */
  OPR_FINITE, OPR_INFINITE, OPR_FLIP, OPR_FRACTIONAL, OPR_INTEGRAL,   /* 34 */
  OPR_LNGAMMA, OPR_IMAG, OPR_INT, OPR_FRAC, OPR_LEFT,                 /* 39 */
  OPR_LN, OPR_LOG,                                                    /* 41 */
  OPR_NAN, OPR_NARGS, OPR_NEWSEQ, OPR_NEWREG, OPR_NEWUSET,            /* 46 */
  OPR_NUMERIC, OPR_ODD, OPR_REAL, OPR_RIGHT,                          /* 50 */
  OPR_SIGN, OPR_SIGNUM, OPR_SIN, OPR_SINH, OPR_SINC,                  /* 55 */
  OPR_SQRT, OPR_TAN, OPR_TANH, OPR_TOP,                               /* 59 */
  OPR_TEMPTY, OPR_TFILLED,                                            /* 61 */
  OPR_TYPEOF,  OPR_UNASSIGNED, OPR_ARCSIN, OPR_ARCCOS, OPR_ARCSEC,    /* 66 */
  OPR_RECIP, OPR_BNOT, OPR_POP,                                       /* 69 */
  OPR_QMDEV, OPR_PEPS, OPR_MEPS, OPR_CELL, OPR_PROCNAME,              /* 74 */
  OPR_SQUARE, OPR_CUBE, OPR_ZERO, OPR_NONZERO,                        /* 78 */
  OPR_PRE, OPR_POST, OPR_NEGATE, OPR_STORE, OPR_INVSQRT,              /* 83 */
  OPR_PP, OPR_MM, OPR_MULADD, OPR_MULUP, OPR_UNITY,                   /* 89 */
  OPR_FOREACH, OPR_NOUNOPR                                            /* 91 */
} UnOpr;


#define getcode(fs,e)   ((fs)->f->code[(e)->u.s.info])

#define luaK_codeAsBx(fs,o,A,sBx)   luaK_codeABx(fs,o,A,(sBx)+MAXARG_sBx)

#define luaK_setmultret(fs,e)   luaK_setreturns(fs, e, LUA_MULTRET)

#define hasmultret(k)      ((k) == VCALL || (k) == VVARARG)

LUAI_FUNC int luaK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx);
LUAI_FUNC int luaK_codeABC (FuncState *fs, OpCode o, int A, int B, int C);
LUAI_FUNC void luaK_fixline (FuncState *fs, int line);
LUAI_FUNC void luaK_nil (FuncState *fs, int from, int n);
LUAI_FUNC void luaK_reserveregs (FuncState *fs, int n);
LUAI_FUNC void luaK_checkstack (FuncState *fs, int n);
LUAI_FUNC int luaK_stringK (FuncState *fs, TString *s);
LUAI_FUNC int luaK_numberK (FuncState *fs, lua_Number r);
LUAI_FUNC void luaK_dischargevars (FuncState *fs, expdesc *e);
LUAI_FUNC void luaK_discharge2reg (FuncState *fs, expdesc *e, int reg);
LUAI_FUNC void luaK_exp2reg (FuncState *fs, expdesc *e, int reg);
LUAI_FUNC int luaK_exp2anyreg (FuncState *fs, expdesc *e);
LUAI_FUNC int luaK_exp2nextreg (FuncState *fs, expdesc *e);
LUAI_FUNC void luaK_exp2val (FuncState *fs, expdesc *e);
LUAI_FUNC int luaK_exp2RK (FuncState *fs, expdesc *e);
LUAI_FUNC void luaK_self (FuncState *fs, expdesc *e, expdesc *key);
LUAI_FUNC void luaK_indexed (FuncState *fs, expdesc *t, expdesc *k);
LUAI_FUNC void luaK_goiftrue (FuncState *fs, expdesc *e);
LUAI_FUNC void luaK_goiffalse (FuncState *fs, expdesc *e);  /* added 0.5.3 */
LUAI_FUNC void luaK_storevar (FuncState *fs, expdesc *var, expdesc *e);
LUAI_FUNC void luaK_setreturns (FuncState *fs, expdesc *e, int nresults);
LUAI_FUNC void luaK_setoneret (FuncState *fs, expdesc *e);
LUAI_FUNC int luaK_jump (FuncState *fs);
LUAI_FUNC void luaK_ret (FuncState *fs, int first, int nret);
LUAI_FUNC void luaK_patchlist (FuncState *fs, int list, int target);
LUAI_FUNC void luaK_patchtohere (FuncState *fs, int list);
LUAI_FUNC void luaK_concat (FuncState *fs, int *l1, int l2);
LUAI_FUNC int luaK_getlabel (FuncState *fs);
LUAI_FUNC void luaK_prefix (FuncState *fs, UnOpr op, expdesc *v);
LUAI_FUNC void luaK_infix (FuncState *fs, BinOpr op, expdesc *v);
LUAI_FUNC void luaK_posfix (FuncState *fs, BinOpr op, expdesc *v1, expdesc *v2);
LUAI_FUNC void luaK_setlist (FuncState *fs, int base, int nelems, int tostore, int isvector);
LUAI_FUNC void codearith (FuncState *fs, OpCode op, expdesc *e1, expdesc *e2);  /* added 0.5.2 */
LUAI_FUNC int  luaK_codecompop (FuncState *fs, int base, int nargs, expdesc *e, CmdCode op);  /* 2.20.0 */
LUAI_FUNC void luaK_ifoperation (FuncState *fs, expdesc *e, int base);  /* added 0.7.1 */
LUAI_FUNC int  codearithbypass (FuncState *fs, int flag, expdesc *e1, UnOpr fn);  /* 0.9.0/0.9.1 */
LUAI_FUNC int  codearithbypassmularg (FuncState *fs, int nargs, expdesc *e, UnOpr fn);  /* 2.9.6 */
LUAI_FUNC void agnK_codefnbin (FuncState *fs, expdesc *e1, expdesc *e2, int op);

#endif

