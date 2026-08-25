/*
** $Id: llex.h,v 1.58 2006/03/23 18:23:32 roberto Exp $
** Lexical Analyzer
** See Copyright Notice in agena.h
*/

#ifndef llex_h
#define llex_h

#include "lobject.h"
#include "lzio.h"


#define FIRST_RESERVED   257

/* maximum length of a reserved word, currently 13 */
#define TOKEN_LEN   (sizeof("lightuserdata")/sizeof(char))

#define llex_token2str(token) (luaX_tokens[(token) - FIRST_RESERVED])  /* 3.10.2 */

/* WARNING: if you change the order of this enumeration, grep "ORDER RESERVED" */
enum RESERVED {
  /* terminal symbols denoted by reserved words */
  TK_ABS = FIRST_RESERVED, TK_ALIAS, TK_AND, TK_ANTILOG2, TK_ANTILOG10, TK_ARCCOS, TK_ARCSEC, TK_ARCSIN, TK_ARCTAN, TK_AS, TK_ASSIGNED, TK_ATENDOF,  /* 12 */
  TK_BEA, TK_BEGIN, TK_BOTTOM, TK_BREAK, TK_BY, TK_BYE,  /* 18 */
  TK_CASE, TK_CATCH, TK_CELL, TK_CIS, TK_CLEAR, TK_CLS, TK_CONJUGATE, TK_CONSTANT, TK_COS, TK_COSH, TK_COSXX, TK_CREATE, TK_CUBE,  /* 31 */
  TK_DEC, TK_DEF, TK_DEFINE, TK_DELETE, TK_DICT, TK_DIV, TK_DO, TK_DOWNTO, TK_DUPLICATE,
  TK_ELIF, TK_ELSE, TK_EMPTY, TK_END, TK_ENTIER, TK_ENUM, TK_EPOCS, TK_ESAC, TK_ESLE, TK_EVEN, TK_EXCHANGE, TK_EXP,
  TK_FAIL, TK_FALSE, TK_FEATURE, TK_FI, TK_FILLED, TK_FINITE, TK_FLIP, TK_FOR, TK_FOREACH, TK_FRAC, TK_FRACTIONAL, TK_FROM, TK_GLOBAL,
  TK_IF, TK_IMAG, TK_IMPORT, TK_IN, TK_INC, TK_INFINITE, TK_INSERT, TK_INT, TK_INTDIV, TK_INTEGRAL,
  TK_INTERSECT, TK_INTO, TK_INVSQRT, TK_IS, TK_KEYS, TK_LEFT, TK_LN, TK_LNGAMMA, TK_LOCAL, TK_LOG, TK_MINUS,
  TK_MOD, TK_MUL, TK_MULADD, TK_MULUP, TK_NAN, TK_NAND, TK_NARGS, TK_NEGATE, TK_NEXT, TK_NONZERO, TK_NOR, TK_NOT, TK_NOTHING, TK_NOTIN, TK_NULL,
  TK_OD, TK_ODD, TK_OF, TK_ONSUCCESS, TK_OR, TK_POP, TK_POST, TK_PRE, TK_PROC, TK_PROCNAME, TK_PUSHD, TK_QMDEV, TK_QSUMUP,
  TK_REAL, TK_RECIP, TK_REDO, TK_REG, TK_RELAUNCH, TK_REMINISCE, TK_RESTART, TK_RET, TK_RIGHT, TK_ROLL, TK_ROTATE,
  TK_SCOPE, TK_SEQ, TK_SIGN, TK_SIGNUM, TK_SIN, TK_SINC, TK_SINH, TK_SIZE, TK_SKIP, TK_SPLIT, TK_SQRT, TK_SQUARE, TK_SQUAREADD,
  TK_STORE, TK_SUBSET, TK_SUMUP, TK_SWITCHD, TK_SYMMOD, TK_TAN, TK_TANH, TK_THEN, TK_TO, TK_TOP, TK_TRUE, TK_TRY, TK_TYPE, TK_TYPEOF,
  TK_UNASSIGNED, TK_UNITY, TK_UNION, TK_UNLESS, TK_UNTIL, TK_WHEN, TK_WHILE, TK_WITH, TK_XNOR, TK_XOR, TK_XSUBSET, TK_YRT, TK_ZERO,
  /* (GREP_POINT) types; if you change the order of the following keywords or add new ones, make sure that the types
     in ltm.c and agena.h are in the same order and that the definition of NUM_RESERVED (see below) may
     have to be be adjusted if you add new keywords right before the terminal symbols. */
  TK_TBOOLEAN, TK_TNUMBER, TK_TCOMPLEX, TK_TSTRING, TK_TPROCEDURE, TK_TUSERDATA, TK_TLIGHTUSERDATA, TK_TTHREAD, TK_TTABLE,
  TK_TSEQUENCE, TK_TREGISTER, TK_TPAIR, TK_TSET, TK_INTEGER, TK_POSINT, TK_NONNEGINT, TK_NONZEROINT, TK_POSITIVE, TK_NEGATIVE, TK_NONNEGATIVE, TK_TLISTING,
  TK_TBASIC, TK_TANYTHING,
  /* other terminal symbols */
  TK_ASSIGN, TK_SEP, TK_CONCAT, TK_QMARK, TK_MINUSQMARK, TK_EQ, TK_GT, TK_LT, TK_GE, TK_LE, TK_NEQ, TK_EEQ, TK_AEQ, TK_NAEQ, TK_IPOW, TK_MAP, TK_SELECT, TK_HAS, TK_COUNT,
  TK_DCOLON, TK_NOTOFTYPE, TK_BAND, TK_BNOT, TK_BOR, TK_BXOR, TK_BLEFT, TK_BRIGHT, TK_LBROTATE, TK_RBROTATE,
  TK_PERCENT, TK_PERCENTRATIO, TK_PERCENTADD, TK_PERCENTSUB, TK_PERCENTCHANGE, TK_ARROW, TK_LT2, TK_GT2,
  TK_COMPLEX, TK_CARTESIAN, TK_DD, TK_OOP, TK_PP, TK_MM, TK_PEPS, TK_MEPS, TK_DATA, TK_ATAD, TK_SEQDATA, TK_SEQDATAEND, TK_ABSDIFF, TK_ACOMPARE,
  TK_COMPADD, TK_COMPSUB, TK_COMPMUL, TK_COMPMULOP, TK_COMPDIV, TK_COMPINTDIV, TK_COMPMOD, TK_COMPCAT,
  TK_COMPBAND, TK_COMPBOR, TK_COMPBXOR, TK_COMPBLEFT, TK_COMPBRIGHT, TK_COMPLBROT, TK_COMPRBROT,
  TK_I32ADD, TK_I32SUB, TK_I32MUL, TK_I32DIV, TK_I32INTDIV,
  TK_NUMBER, TK_NAME, TK_STRING, TK_EOS
};

/* number of reserved words */
#define NUM_RESERVED   (cast(int, TK_TANYTHING - FIRST_RESERVED + 1))  /* agena 0.6.0 following, changed 2.20.1 */


/* array with token `names' */
LUAI_DATA const char *const luaX_tokens [];


typedef union {
  lua_Number r;
  TString *ts;
} SemInfo;  /* semantics information */


typedef struct Token {
  int token;
  SemInfo seminfo;
} Token;


typedef struct LexState {
  int current;        /* current character (charint) */
  int linenumber;     /* input line counter */
  int lastline;       /* line of last token `consumed' */
  Token t;            /* current token */
  Token lookahead;    /* look ahead token */
  struct FuncState *fs;  /* `FuncState' is private to the parser */
  struct lua_State *L;
  ZIO *z;             /* input stream */
  Mbuffer *buff;      /* buffer for tokens */
  TString *source;    /* current source name, e.g. "stdin" */
  char decpoint;      /* locale decimal point */
} LexState;


LUAI_FUNC void luaX_init (lua_State *L);
LUAI_FUNC void luaX_setinput (lua_State *L, LexState *ls, ZIO *z,
                              TString *source);
LUAI_FUNC TString *luaX_newstring (LexState *ls, const char *str, size_t l);
LUAI_FUNC int luaX_next (LexState *ls);
LUAI_FUNC int  luaX_lookahead (LexState *ls);
LUAI_FUNC void luaX_lexerror (LexState *ls, const char *msg, int token);
LUAI_FUNC void luaX_syntaxerror (LexState *ls, const char *s);
LUAI_FUNC const char *luaX_token2str (LexState *ls, int token);

#endif
