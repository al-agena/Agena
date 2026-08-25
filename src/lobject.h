/*
** $Id: lobject.h,v 2.20 2006/01/18 11:37:34 roberto Exp $
** Type definitions for Lua & Agena objects
** See Copyright Notice in agena.h
*/


#ifndef lobject_h
#define lobject_h

#include <stdarg.h>

#include "llimits.h"
#include "agena.h"
#include "agnhlps.h"


static const lu_byte log_2table[256] = { 0 };  /* 2.3.1/3.4.6 fix for AgenaEdit  */


/* tags for values visible from Lua; changed 0.10.0 */
#define LAST_TAG    LUA_TANYTHING  /* 2.25.5, formerly LUA_TSET (in Lua LUA_TTHREAD); grep "(LAST_TAG)" in agena.h */

#define NUM_TAGS    (LAST_TAG + 1)


/*
** Extra tags for non-values
*/
#define LUA_TPROTO    (LAST_TAG + 1)
#define LUA_TUPVAL    (LAST_TAG + 2)
#define LUA_TDEADKEY  (LAST_TAG + 3)


/*
** Union of all collectable objects
*/
typedef union GCObject GCObject;


/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
#define CommonHeader    GCObject *next; lu_byte tt; lu_byte marked


/*
** Common header in struct form
*/
typedef struct GCheader {
  CommonHeader;
} GCheader;


/*
** Union of all Lua values
*/
typedef union {
  GCObject *gc;
  void *p;
  lua_Number n;
#ifndef PROPCMPLX
  agn_Complex c;
#else
  lua_Number c[2];
#endif
  int b;
} Value;


/*
** Tagged Values
*/

#define TValuefields    Value value; int tt

typedef struct lua_TValue {
  TValuefields;
} TValue;


/* Macros to test type */
#define ttisnil(o)           (ttype(o) == LUA_TNIL)
#define ttisnoneornil(o)     (ttype(o) == LUA_TNIL || ttype(o) == LUA_TNONE)
#define ttisnotnoneornil(o)  (ttype(o) != LUA_TNIL && ttype(o) != LUA_TNONE)
#define ttisnotnil(o)        (!(ttype(o) == LUA_TNIL))
#define ttisnumber(o)        (ttype(o) == LUA_TNUMBER)
#define ttisstring(o)        (ttype(o) == LUA_TSTRING)
#define ttistable(o)         (ttype(o) == LUA_TTABLE)
#define ttisfunction(o)      (ttype(o) == LUA_TFUNCTION)
#define ttisboolean(o)       (ttype(o) == LUA_TBOOLEAN)
#define ttisuserdata(o)      (ttype(o) == LUA_TUSERDATA)
#define ttisthread(o)        (ttype(o) == LUA_TTHREAD)
#define ttislightuserdata(o) (ttype(o) == LUA_TLIGHTUSERDATA)
#define ttisuset(o)          (ttype(o) == LUA_TSET)
#define ttisseq(o)           (ttype(o) == LUA_TSEQ)
#define ttisreg(o)           (ttype(o) == LUA_TREG)
#define ttispair(o)          (ttype(o) == LUA_TPAIR)
#define ttiscomplex(o)       (ttype(o) == LUA_TCOMPLEX)
#define ttistrue(o)          (ttisboolean(o) && bvalue(o) == 1)
#define ttisfalse(o)         (ttisboolean(o) && bvalue(o) == 0)

#define ttisint(o)           (ttype(o) == LUA_TNUMBER && tools_isint(nvalue(o)))
#define ttisnegint(o)        (ttype(o) == LUA_TNUMBER && tools_isnegint(nvalue(o)))
#define ttisposint(o)        (ttype(o) == LUA_TNUMBER && tools_isposint(nvalue(o)))
#define ttisnonnegint(o)     (ttype(o) == LUA_TNUMBER && tools_isnonnegint(nvalue(o)))
#define ttisnonposint(o)     (ttype(o) == LUA_TNUMBER && tools_isnonposint(nvalue(o)))
#define ttispositive(o)      (ttype(o) == LUA_TNUMBER && tools_ispositive(nvalue(o)))
#define ttisnonnegative(o)   (ttype(o) == LUA_TNUMBER && tools_isnonnegative(nvalue(o)))

/* Macros to access values */
#define ttype(o)      ((o)->tt)
#define gcvalue(o)    check_exp(iscollectable(o), (o)->value.gc)
#define pvalue(o)     check_exp(ttislightuserdata(o), (o)->value.p)
#define nvalue(o)     check_exp(ttisnumber(o), (o)->value.n)
#define cvalue(o)     check_exp(ttiscomplex(o), (o)->value.c)
#define rawtsvalue(o) check_exp(ttisstring(o), &(o)->value.gc->ts)
#define tsvalue(o)    (&rawtsvalue(o)->tsv)
#define rawuvalue(o)  check_exp(ttisuserdata(o), &(o)->value.gc->u)
#define uvalue(o)     (&rawuvalue(o)->uv)
#define clvalue(o)    check_exp(ttisfunction(o), &(o)->value.gc->cl)
#define hvalue(o)     check_exp(ttistable(o), &(o)->value.gc->h)
#define bvalue(o)     check_exp(ttisboolean(o), (o)->value.b)
#define thvalue(o)    check_exp(ttisthread(o), &(o)->value.gc->th)
#define usvalue(o)    check_exp(ttisuset(o), &(o)->value.gc->l)
#define seqvalue(o)   check_exp(ttisseq(o), &(o)->value.gc->a)
#define pairvalue(o)  check_exp(ttispair(o), &(o)->value.gc->e)
#define regvalue(o)   check_exp(ttisreg(o), &(o)->value.gc->r)

#define l_isfalse(o)  (ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))
#define l_istrue(o)   (ttisboolean(o) && bvalue(o) == 1)
#define l_isfail(o)   (ttisboolean(o) && bvalue(o) == 2)  /* 0.9.2 */
#define l_isfalseorfail(o)  (ttisnil(o) || (ttisboolean(o) && (bvalue(o) == 0 || bvalue(o) == 2)))

/*
** for internal debug only
*/
#define checkconsistency(obj) \
  lua_assert(!iscollectable(obj) || (ttype(obj) == (obj)->value.gc->gch.tt))

#define checkliveness(g,obj) \
  lua_assert(!iscollectable(obj) || \
  ((ttype(obj) == (obj)->value.gc->gch.tt) && !isdead(g, (obj)->value.gc)))

/* Macros to set values */
#define setnilvalue(obj) ((obj)->tt=LUA_TNIL)

#define setfailvalue(obj) \
  { TValue *i_o=(obj); i_o->value.b=(2); i_o->tt=LUA_TBOOLEAN; }

#define setnvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.n=(x); i_o->tt=LUA_TNUMBER; }

#define setpvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.p=(x); i_o->tt=LUA_TLIGHTUSERDATA; }

#define setbvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.b=(x); i_o->tt=LUA_TBOOLEAN; }

#define setsvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TSTRING; \
    checkliveness(G(L),i_o); }

#define setuvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TUSERDATA; \
    checkliveness(G(L),i_o); }

#define setthvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTHREAD; \
    checkliveness(G(L),i_o); }

#define setclvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TFUNCTION; \
    checkliveness(G(L),i_o); }

#define sethvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTABLE; \
    checkliveness(G(L),i_o); }

#define setptvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TPROTO; \
    checkliveness(G(L),i_o); }

/* 0.10.0 */
#define setusvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TSET; \
    checkliveness(G(L),i_o); }

/* 0.11.0 */
#define setseqvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TSEQ; \
    checkliveness(G(L),i_o); }

/* 0.11.1 */
#define setpairvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TPAIR; \
    checkliveness(G(L),i_o); }

/* 2.3.0 RC 3 */
#define setregvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TREG; \
    checkliveness(G(L),i_o); }

#ifndef PROPCMPLX
#define setcvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.c=(x); i_o->tt=LUA_TCOMPLEX; }
#else
#define setcvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.c[0]=((x)[0]); i_o->value.c[1]=((x)[1]); i_o->tt=LUA_TCOMPLEX; }
#endif

#ifndef PROPCMPLX
#define setrealimagvalue(obj,__re,__im) { \
  TValue *i_o=(obj); i_o->value.c=(__re)+(I*(__im)); i_o->tt=LUA_TCOMPLEX; \
}
#else
#define setrealimagvalue(obj,__re,__im) { \
  TValue *i_o=(obj); i_o->value.c[0]=(__re); i_o->value.c[1]=(__im); i_o->tt=LUA_TCOMPLEX; \
}
#endif

#define setobj(L,obj1,obj2) \
  { const TValue *o2=(obj2); TValue *o1=(obj1); \
    o1->value = o2->value; o1->tt=o2->tt; \
    checkliveness(G(L),o1); }


/*
** different types of sets, according to destination
*/

/* from stack to (same) stack */
#define setobjs2s      setobj
/* to stack (not from same stack) */
#define setobj2s       setobj
#define setsvalue2s    setsvalue
#define setrvalue2s    setrregvalue
#define sethvalue2s    sethvalue
#define setusvalue2s   setuvalue
#define setptvalue2s   setptvalue
/* from table to same table */
#define setobjt2t      setobj
/* to table */
#define setobj2t       setobj
/* to new object */
#define setobj2n       setobj
#define setsvalue2n    setsvalue

#define setttype(obj, tt)  (ttype(obj) = (tt))

#define iscollectable(o)   (ttype(o) >= LUA_TSTRING)

typedef TValue *StkId;  /* index to stack elements */

/*
** String headers for string table
*/
typedef union TString {
  L_Umaxalign dummy;  /* ensures maximum alignment for strings */
  struct {
    CommonHeader;
    lu_byte reserved;
    unsigned int hash;
    size_t len;
  } tsv;
} TString;


#define getstr(ts)    cast(const char *, (ts) + 1)
#define svalue(o)     getstr(rawtsvalue(o))  /* Lua 5.1.3 patch 12 */


typedef union Udata {
  L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
  struct {
    CommonHeader;
    struct Table *metatable;
    struct Table *env;
    size_t len;  /* number of bytes */
    TString *type;  /* 2.3.0 RC 3 */
    unsigned short nuvalue;  /* number of user values, new 2.21.x */
    lu_byte readonly;
    lu_byte padding[5];  /* reclaim the 5-byte alignment hole */
  } uv;
} Udata;


/*
** Function Prototypes; if you change it, you may extend ldump.c and lundump.c
*/
typedef struct Proto {
  CommonHeader;
  TValue *k;                /* constants used by the function */
  Instruction *code;
  struct Proto **p;         /* functions defined inside the function */
  int *lineinfo;            /* map from opcodes to source lines */
  struct LocVar *locvars;   /* information about local variables */
  struct Param *params;     /* type information about parameters, 2.1.2 */
  TString **upvalues;       /* upvalue names */
  TString  *source;         /* e.g. stdin, ... */
  int sizeupvalues;
  int sizek;                /* size of `k' */
  int sizecode;
  int sizelineinfo;
  int sizep;                /* size of `p' */
  int sizelocvars;
  int sizeparams;           /* 2.1.2 */
  int linedefined;
  int lastlinedefined;
  GCObject *gclist;
  lu_byte nups;             /* number of upvalues */
  lu_byte numparams;
  lu_byte is_vararg;
  lu_byte is_oopfn;         /* this is an OOP-style function */
  lu_byte is_typegiven;     /* parameter type */
  lu_byte is_rettypegiven;  /* return type */
  lua_typecheck rettype;    /* return type, 1.2.1, 2.12.1, for definition see agnconf.h */
  TString *rettypets;       /* user-defined return type, 1.2.1 */
  lu_byte maxstacksize;
  lu_byte padding;          /* Explicit padding to reach 8-byte boundary to the multiple of 8 */
} Proto;

#define getproto(o)   (clvalue(o)->l.p)  /* for compatibility to Lua 5.4 */

/* masks for new-style vararg */
#define VARARG_HASARG        1
#define VARARG_ISVARARG      2
#define VARARG_NEEDSARG      4

/* mask for OOP function detection, 2.24.0 */
#define IS_OOPFN             1  /* must be non-zero ! */

typedef struct LocVar {
  TString *varname;
  lu_byte isconst;  /* 2.20.0 */
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;


typedef struct Param {      /* 2.1.2, contains type information on parameters */
  TString *vartypets;       /* user defined type, 0.28.0 */
  lua_typecheck typearray;  /* holds maximum of five basic types, 2.1.2, 2.12.1, for definition see agnconf.h */
} Param;


/*
** Upvalues
*/

typedef struct UpVal {
  CommonHeader;
  TValue *v;  /* points to stack or to its own value */
  union {
    TValue value;  /* the value (when closed) */
    struct {  /* double linked list (when open) */
      struct UpVal *prev;
      struct UpVal *next;
    } l;
  } u;
} UpVal;


/*
** Closures
*/

#define ClosureHeader \
    CommonHeader; lu_byte isC; lu_byte nupvalues; GCObject *gclist; \
    struct Table *env; struct Table *rtable; lu_byte updatertable; TString *type; \
    lu_byte tfor_func; struct Table *storage; struct Table *metatable

typedef struct CClosure {
  ClosureHeader;
  lua_CFunction f;
  TValue upvalue[1];
} CClosure;


typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;
  UpVal *upvals[1];
} LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
  /* this is a UNION, not a structure ! Do not add further entries. */
} Closure;


#define iscfunction(o)    (ttype(o) == LUA_TFUNCTION && clvalue(o)->c.isC)
#define isLfunction(o)    (ttype(o) == LUA_TFUNCTION && !clvalue(o)->c.isC)


/*
** Tables
*/

typedef union TKey {
  struct {
    TValuefields;
    struct Node *next;  /* for chaining */
  } nk;
  TValue tvk;
} TKey;

typedef struct Node {
  TValue i_val;
  TKey i_key;
} Node;

typedef struct Table {
  CommonHeader;
  lu_byte flags;      /* 1 << p means tagmethod(p) is not present */
  lu_byte lsizenode;  /* log2 of size of `node' array */
  lu_byte readonly;   /* read-only flag */
  lu_byte padding[3]; /* padding for maximum alignment */
  struct Table *metatable;
  TValue *array;      /* array part */
  Node *node;
  Node *lastfree;     /* any free position is before this position */
  GCObject *gclist;
  TString *type;
  int sizearray;      /* size of `array' array */
  int pad;            /* final 4-byte pad to keep total size a multiple of 8 */
} Table;


/* Definition of the UltraSet, added 0.10.0; extended 0.14.0 */

/* added 0.10.0; a union means that LTKey is either a struct nk or a TValue tvk */
typedef union LTKey {
  struct {
    TValuefields;
    struct LNode *next;  /* for chaining */
  } nk;
  TValue tvk;
} LTKey;

/* added 0.10.0;
   there will be no increase in speed when substituting `LTKey i_key;`
   with the direct definition of the LTKey _UNION_; also there will be no savings
   in memory consumption since i_key is not a pointer to the LNode object but only
   a declaration which is substituted by the compiler */
typedef struct LNode {
  LTKey i_key;
} LNode;

typedef struct UltraSet {
  CommonHeader;
  lu_byte flags;      /* 1 << p means tagmethod(p) is not present */
  lu_byte lsizenode;  /* log2 of size of `node' array */
  lu_byte readonly;   /* read-only flag */
  lu_byte padding[3]; /* padding for maximum alignment */
  LNode *node;
  LNode *lastfree;    /* any free position is before this position */
  GCObject *gclist;
  size_t size;
  struct Table *metatable;
  TString *type;      /* 0.14.0 / 0.28.0 */
} UltraSet;


typedef struct Seq {
  CommonHeader;
  lu_byte flags;      /* 1 << p means tagmethod(p) is not present */
  lu_byte readonly;   /* read-only flag */
  lu_byte padding[4]; /* padding for maximum alignment */
  TValue *array;
  size_t size;
  size_t maxsize;
  GCObject *gclist;
  struct Table *metatable;
  TString *type;
} Seq;


typedef struct Pair {
  CommonHeader;
  lu_byte flags;     /* 1 << p means tagmethod(p) is not present */
  lu_byte readonly;  /* read-only flag */
  lu_byte padding[4]; /* padding for maximum alignment */
  TValue *array;
  GCObject *gclist;
  struct Table *metatable;
  TString *type;
} Pair;


typedef struct Reg {
  CommonHeader;
  lu_byte flags;     /* 1 << p means tagmethod(p) is not present */
  lu_byte readonly;  /* read-only flag */
  lu_byte padding[4]; /* padding for maximum alignment */
  TValue *array;
  size_t top;
  size_t maxsize;
  GCObject *gclist;
  struct Table *metatable;
  TString *type;  /* 2.5.3 */
} Reg;

/*
** `module' operation for hashing (size is always a power of 2)
*/
#define lmod(s,size)   (check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))

#define twoto(x)       (1<<(x))
#define sizenode(t)    (twoto((t)->lsizenode))


#define luaO_nilobject (&luaO_nilobject_)

LUAI_DATA const TValue luaO_nilobject_;


#define ceillog2(x)    (luaO_log2((x)-1) + 1)

/* rounds x up to the next multiple of four if x is not already a multiple of four */
#define agnO_aligntowordboundary(x)  ((x + (size_t)3)) & ~(size_t)3

LUAI_FUNC int luaO_log2 (unsigned int x);
LUAI_FUNC int agnO_log2 (unsigned int x);  /* 3.20.1 */
LUAI_FUNC int luaO_int2fb (unsigned int x);
LUAI_FUNC int luaO_fb2int (int x);
LUAI_FUNC int luaO_rawequalObj (const TValue *t1, const TValue *t2);
LUAI_FUNC int luaO_str2d (const char *s, lua_Number *result, int *overflow);
LUAI_FUNC size_t luaO_str2num (const char *s, TValue *o);
LUAI_FUNC int luaO_rawarith (lua_State *L, int op, const TValue *p1,
                             const TValue *p2, TValue *res);
LUAI_FUNC void luaO_arith (lua_State *L, int op, const TValue *p1,
                           const TValue *p2, StkId res);

#ifndef PROPCMPLX
LUAI_FUNC int luaO_str2c (const char *s, agn_Complex *result);  /* 0.26.1 */
#else
LUAI_FUNC int luaO_str2c (const char *s, lua_Number *result);  /* 0.26.1 */
#endif
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t len);
LUAI_FUNC int luaO_hexavalue (int c);
LUAI_FUNC int luaO_utf8esc (char *buff, unsigned long x);
LUAI_FUNC const char *luaO_str2uint64 (const char *s, uint64_t *result);
LUAI_FUNC const char *luaO_str2int64 (const char *s, int64_t *result);

int lisbdigit (unsigned char x);
int lisodigit (unsigned char x);
int lishdigit (unsigned char x);

lua_Number lua_strany2number (const char *s, char **endptr, int base,
                              char tbase, char Tbase, int (*f)(unsigned char),
                              int check, int *overflow);

#define UTF8BUFFSZ    8


#endif

