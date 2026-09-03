/*
** $Id: agena.h, v 1.218 2009/03/01 15:38:27 $
** Agena - A procedural programming language based on Lua 5.1
** http://agena.sourceforge.net
** Lua.org, PUC-Rio, Brazil (http://www.lua.org)
** based on file $Id: lua.h,v 1.218 2006/06/02 15:34:00 roberto Exp $
** See Copyright Notice at the end of this file
**
** GCC Compilation:  make clean && make config && make mingw -j8
** valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all ./agena
** drmemory -logdir ./logs -suppress drmemory.cfg -debug -brief -- agena.exe
*/

#if defined(__GNUC__) && (__GNUC___ > 5 || (__GNUC__ == 5 && __GNUC_MINOR__ >= 1))
/* just a template/TEMPLATE */
#endif

#ifdef __GNUC_PREREQ
#undef __GNUC_PREREQ
#endif


/*#if defined __GNUC__ && defined __GNUC_MINOR__
# define __GNUC_PREREQ(maj, min) \
  ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
# define __GNUC_PREREQ(maj, min) 0
#endif */

#if defined __GNUC__ && defined __GNUC_MINOR__
# define __GNUC_PREREQ(maj, min) \
  (((__GNUC__) << 16) + (__GNUC_MINOR__) >= (((maj) << 16) + (min)))
#else
# define __GNUC_PREREQ(maj, min) 0
#endif

#ifndef agena_h
#define agena_h

#define AGENA_VERSION     "agena 7.9"
#define AGENA_EDITION     "7.9.6"
#define AGENA_MOTTO       "deimos"
#define AGENA_NAME        "agena"
#define AGENA_LOGO        ">>"
#define AGENA_RELEASE     AGENA_NAME " " AGENA_LOGO " " AGENA_EDITION
#define AGENA_COPYRIGHT   "Interpreter as of " AGENA_BUILDDATE
#define AGENA_LICENCE     "(C) 2006-2026 http://agena.sourceforge.net\n"

#define LUA_AUTHORS       "Lua: R. Ierusalimschy, L. H. de Figueiredo & W. Celes; Agena: A. Walz"


#include <stdarg.h>
#include <stddef.h>

#ifndef __APPLE__
#include <sys/types.h>
#endif

#include <stdint.h>

#include "agncfg.h"
#include "agnconf.h"
#include "agncmpt.h"
#include "agnt64.h"
#include "agnhlps.h"

/* mark for precompiled code (`<esc>Agena') */
#define LUA_SIGNATURE         "\033Agena"

/* option for multiple returns in `lua_pcall' and `lua_call' */
#define LUA_MULTRET   (-1)


/*
** pseudo-indices
*/

/* put the stack before the following constants - otherwise you may need to recompile all plus packages */
#define LUA_REGISTRYINDEX     (-10000)
#define LUA_ENVIRONINDEX      (-10001)
#define LUA_GLOBALSINDEX      (-10002)
#define LUA_CONSTANTSINDEX    (-10003)  /* new 2.19.2 */
#define lua_upvalueindex(i)   (LUA_CONSTANTSINDEX-(i))  /* 2.19.2, formerly: (LUA_GLOBALSINDEX-(i)) */

/* thread status; 0 is OK */
#define LUA_YIELD       1
#define LUA_ERRRUN      2
#define LUA_ERRSYNTAX   3
#define LUA_ERRMEM      4
#define LUA_ERRERR      5


/* TYPEDEF 2.14.4 */
#ifndef off64_t
#define off64_t long long int
#endif


typedef struct lua_State lua_State;

typedef int (*lua_CFunction) (lua_State *L);


/*
** functions that read/write blocks when loading/dumping Lua chunks
*/
typedef const char * (*lua_Reader) (lua_State *L, void *ud, size_t *sz);

typedef int (*lua_Writer) (lua_State *L, const void* p, size_t sz, void* ud);


/*
** prototype for memory-allocation functions
*/
typedef void * (*lua_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*lua_WarnFunction) (void *ud, const char *msg, int tocont);


/*
** basic types; grep "(GREP_POINT) types" if you want to change their order or add new ones;
** also grep "(LAST_TAG)" in lobject.h !
*/
#define LUA_TNONE            (-2)

#define LUA_TFAIL            (-1)
#define LUA_TNIL             0
#define LUA_TBOOLEAN         1
#define LUA_TNUMBER          2
#define LUA_TCOMPLEX         3
#define LUA_TSTRING          4
#define LUA_TFUNCTION        5
#define LUA_TUSERDATA        6
#define LUA_TLIGHTUSERDATA   7
#define LUA_TTHREAD          8
#define LUA_TTABLE           9
#define LUA_TSEQ             10
#define LUA_TREG             11
#define LUA_TPAIR            12
#define LUA_TSET             13
/* following are pseudo-types */
#define LUA_TINTEGER         14
#define LUA_TPOSINT          15
#define LUA_TNONNEGINT       16
#define LUA_TNONZEROINT      17
#define LUA_TPOSITIVE        18
#define LUA_TNEGATIVE        19
#define LUA_TNONNEGATIVE     20
#define LUA_TLISTING         21
#define LUA_TBASIC           22
#define LUA_TANYTHING        23

/* number of types and subtypes including null (= #0)
** basic types; grep "(GREP_POINT) types" if you add new ones
*/
#define LUAI_NTYPELISTMOD   (LUA_TANYTHING - LUA_TNIL + 1)


/* minimum Lua stack available to a C function; see also LUAI_MAXCSTACK in agnconf.h
   formerly 20, changed to 40 in 2.31.7/2.32.2, changed to 128 in 3.6.2, changed to 256 in 3.15.1 */
#define LUA_MINSTACK   256


/*
** generic extra include file
*/
#if defined(LUA_USER_H)
#include LUA_USER_H
#endif


/* type of numbers in Lua */
typedef LUA_NUMBER lua_Number;


/* type for integer functions */
typedef LUA_INTEGER lua_Integer;


/*
** state manipulation
*/
LUA_API lua_State *(lua_newstate) (lua_Alloc f, void *ud);
LUA_API void       (lua_closestate) (lua_State *L);
LUA_API void       (lua_close) (lua_State *L);
LUA_API lua_State *(lua_newthread) (lua_State *L);

LUA_API lua_CFunction (lua_atpanic) (lua_State *L, lua_CFunction panicf);


/*
** basic stack manipulation
*/
LUA_API int   (lua_gettop) (lua_State *L);
LUA_API void  (lua_settop) (lua_State *L, int idx);
LUA_API void  (lua_pushvalue) (lua_State *L, int idx);
LUA_API void  (lua_remove) (lua_State *L, int idx);
LUA_API void  (lua_insert) (lua_State *L, int idx);
LUA_API void  (lua_replace) (lua_State *L, int idx);
LUA_API void  (lua_rotate) (lua_State *L, int idx, int n);  /* taken from Lua 5.4 */
LUA_API void  (lua_copy) (lua_State *L, int fromidx, int toidx);  /* taken from Lua 5.4 */
LUA_API int   (lua_checkstack) (lua_State *L, int sz);

LUA_API void  (lua_xmove) (lua_State *from, lua_State *to, int n);

LUA_API void        (agn_poptop) (lua_State *L);  /* added 0.5.3 */
LUA_API void        (agn_poptoptwo) (lua_State *L);  /* added 0.10.0 */
LUA_API lua_Number  (agn_getinumber) (lua_State *L, int idx, int n);  /* changed 0.5.3 */
LUA_API void        (agn_setinumber) (lua_State *L, int idx, int n, lua_Number v);  /* 3.10.2 */
LUA_API lua_Number  (agn_rawgetinumber) (lua_State *L, int idx, int n, int *rc);  /* 2.27.5 */
LUA_API void        (agn_rawgeticomplex) (lua_State *L, int idx, int n, lua_Number *z, int *rc);  /* 4.5.0 */
LUA_API lua_Number  (agn_rawgetiinteger) (lua_State *L, int idx, int n, int *rc);  /* 4.0.0 */
LUA_API const char  *(agn_rawgetilstring) (lua_State *L, int idx, int n, size_t *len, int *rc);  /* changed 0.5.3, 4.11.1 */
LUA_API int         (agn_checkboolean) (lua_State *L, int idx);  /* 2.28.6 */
LUA_API lua_Number  (agn_checknumber) (lua_State *L, int idx);
LUA_API lua_Integer (agn_checkinteger) (lua_State *L, int idx);
LUA_API lua_Number  (lua_checkinteger) (lua_State *L, int idx);  /* 7.7.11  */
LUA_API lua_Integer (agn_checkposint) (lua_State *L, int idx);  /* 2.7.0 */
LUA_API uint32_t (agn_checkuint32_t) (lua_State *L, int idx);  /* 2.16.11 */
LUA_API uint16_t (agn_checkuint16_t) (lua_State *L, int idx);  /* 2.16.11 */
LUA_API lua_Integer (agn_checknonnegint) (lua_State *L, int idx);  /* 2.9.8 */
LUA_API lua_Integer (agn_checknonzeroint) (lua_State *L, int idx);  /* 4.11.0 */
LUA_API lua_Number  (agn_checkpositive) (lua_State *L, int idx);  /* 2.14.3 */
LUA_API lua_Number  (agn_checknonnegative) (lua_State *L, int idx);  /* 2.14.3 */
#ifndef PROPCMPLX
LUA_API agn_Complex (agn_checkcomplex) (lua_State *L, int idx);
LUA_API agn_Complex (agn_optcomplex) (lua_State *L, int narg, agn_Complex def);  /* 0.12.0 */
/* We need the following explicit assignments to the real and imaginary parts to properly cope with infinity.
   Just `agn_createcomplex(L, (a) + I*(b))` does not suffice. */
#define agn_pushcomplex(L,re,im) { \
  agn_Complex __z; \
  __real__ (__z) = (re); \
  __imag__ (__z) = (im); \
  agn_createcomplex(L, __z); \
}
#define agn_pushcomplexnan(L)  (agn_createcomplex(L, AGN_NAN))
#else
#define agn_pushcomplex(L,a,b) (agn_createcomplex(L, (a), (b)))
#define agn_pushcomplexnan(L)  (agn_createcomplex(L, AGN_NAN, 0))
LUA_API void agn_optcomplex (lua_State *L, int idx, lua_Number *def, lua_Number *re, lua_Number *im);
#endif
LUA_API lua_Number  (agn_complexreal) (lua_State *L, int idx);
LUA_API lua_Number  (agn_compleximag) (lua_State *L, int idx);
LUA_API int (agn_getcmplxparts) (lua_State *L, int idx, double *re, double *im);
LUA_API int (agn_getcmplxpartsl) (lua_State *L, int idx, long double *re, long double *im);

LUA_API lua_Number agn_getepsilon (lua_State *L);
LUA_API lua_Number agn_getdblepsilon (lua_State *L);
LUA_API lua_Number agn_gethepsilon (lua_State *L);
LUA_API lua_Number agn_getstarttime (lua_State *L);

/* Determines the new size of a structure. The growth pattern, taken from Python, is:
   0, 4, 8, 12, 16, 24, 32, 40, 48, 60, 72, 84, 100, 116, 136, etc.
   Taken from: https://news.ycombinator.com/item?id=1996857;
   Agena equivalent:
   newsize := << x -> (x + (x >>> 3) + 6) && ~~3 >>
   x := 0
   while x < 1000 do
      y := newsize(x);
      print(x, y, utils.newsize(x))
      x := y
   od;
   In general, the result is around 113 percent of x (median), rounded up to the next multiple of 4.

   Idea taken from: https://github.com/python/cpython/blob/main/Objects/listobject.c

   formerly:
   #define newsize(x)    (x + (x >> 3) + (x < 9 ? 3 : 6)) which can return odd results. */
#define agn_newsize(L, x)    (((size_t)(x) + ((size_t)(x) >> 3) + 6) & ~(size_t)3)

/* Cache Stack */
LUA_API void lua_pushcachevalue (lua_State *L, int idx);
LUA_API void lua_seqseticachevalue (lua_State *L, int idx, size_t n, int idxcache);

/*
** access functions (stack -> C)
*/

LUA_API int             (lua_isnumber) (lua_State *L, int idx);
LUA_API int             (agn_iscomplex) (lua_State *L, int idx);  /* 4.9.3 */
LUA_API int             (lua_isstring) (lua_State *L, int idx);
LUA_API int             (agn_isinteger) (lua_State *L, int idx);
#define lua_isinteger agn_isinteger
LUA_API int             (agn_isposint) (lua_State *L, int idx);
LUA_API int             (agn_isnonnegint) (lua_State *L, int idx);
LUA_API int             (lua_iscfunction) (lua_State *L, int idx);
LUA_API int             (lua_isuserdata) (lua_State *L, int idx);
LUA_API int             (lua_type) (lua_State *L, int idx);
LUA_API const char     *(lua_typename) (lua_State *L, int tp);
LUA_API int             (lua_equal) (lua_State *L, int idx1, int idx2);
LUA_API int             (agn_equalref) (lua_State *L, int index1, int index2);  /* 2.12.3 */
LUA_API int             (lua_rawequal) (lua_State *L, int idx1, int idx2);
LUA_API int             (lua_rawaequal) (lua_State *L, int idx1, int idx2);  /* 2.3.0 RC 3 */
LUA_API int             (lua_lessthan) (lua_State *L, int idx1, int idx2);
LUA_API int             (lua_compare) (lua_State *L, int index1, int index2, int op);  /* taken from Lua 5.4, 2.21.1 */
LUA_API void            (lua_arith) (lua_State *L, int op);  /* taken from Lua 5.4 */
LUA_API lua_Number      (lua_tonumber) (lua_State *L, int idx);
LUA_API lua_Number      (agn_tonumber) (lua_State *L, int idx);  /* 0.12.0 */
LUA_API lua_Number      (agn_tonumberx) (lua_State *L, int idx, int *exception);
LUA_API const char     *(agn_tostring) (lua_State *L, int idx);  /* 0.24.0 */
LUA_API size_t          (lua_stringtonumber) (lua_State *L, const char *s);  /* taken from Lua 5.4, 2.22.0 */
#ifndef PROPCMPLX
LUA_API agn_Complex     (agn_tocomplex) (lua_State *L, int idx);   /* 0.12.0 */
LUA_API agn_Complex     (agn_tocomplexx) (lua_State *L, int idx, int *exception);  /* added 0.26.1 */
#else
LUA_API void            (agn_tocomplexx) (lua_State *L, int idx, int *exception, lua_Number *a);  /* added 0.26.1 */
#endif
LUA_API lua_Integer     (lua_tointeger) (lua_State *L, int idx);
LUA_API lua_Integer     (agn_tointeger) (lua_State *L, int idx);
LUA_API int32_t         (lua_toint32_t) (lua_State *L, int idx);
LUA_API uint32_t        (lua_touint32_t) (lua_State *L, int idx);
LUA_API off64_t         (lua_tooff64_t) (lua_State *L, int idx);
LUA_API int             (lua_toboolean) (lua_State *L, int idx);
LUA_API int             (lua_istrue) (lua_State *L, int idx);
LUA_API int             (lua_isfalse) (lua_State *L, int idx);
LUA_API int             (lua_isfail) (lua_State *L, int idx);
LUA_API int             (lua_isfalseorfail) (lua_State *L, int idx);
LUA_API int             (lua_isnilfalseorfail) (lua_State *L, int idx);
LUA_API int             (agn_istrue) (lua_State *L, int idx);   /* 0.5.2, June 01, 2007 */
LUA_API int             (agn_isfalse) (lua_State *L, int idx);  /* 0.10.0, April 02, 2008 */
LUA_API int             (agn_isfail) (lua_State *L, int idx);   /* 0.10.0, April 02, 2008 */
#define lua_stringconvertible(L, i) (agn_isnumber(L, i) || agn_isstring(L, i) || lua_isboolean(L, i))
LUA_API const char     *(lua_tolstring) (lua_State *L, int idx, size_t *len);
LUA_API size_t          (lua_objlen) (lua_State *L, int idx);
LUA_API lua_CFunction   (lua_tocfunction) (lua_State *L, int idx);
LUA_API void           *(lua_touserdata) (lua_State *L, int idx);
LUA_API lua_State      *(lua_tothread) (lua_State *L, int idx);
LUA_API const void     *(lua_topointer) (lua_State *L, int idx);

LUA_API size_t          (agn_nops) (lua_State *L, int idx);  /* 0.9.1 */

LUA_API void            (agn_tablestate) (lua_State *L, int idx, size_t a[], int mode);  /* 0.12.3 */
LUA_API void            (agn_tablesize) (lua_State *L, int idx, size_t a[]);      /* 2.3.0 RC 4 */
LUA_API void            (agn_borders) (lua_State *L, int idx, size_t a[]);        /* 2.30.1 */
LUA_API void            (agn_arrayborders) (lua_State *L, int idx, size_t a[]);   /* 2.14.10 */
LUA_API void            (agn_intindices) (lua_State *L, int idx, int *flag);  /* 2.30.1 */
LUA_API void            (agn_intentries) (lua_State *L, int idx, int *flag);  /* 3.9.3 */
LUA_API void            (agn_entries) (lua_State *L, int idx, int *flag);  /* 2.30.1 */
LUA_API void            (agn_parts) (lua_State *L, int idx);      /* 2.30.1 */
LUA_API void            (agn_arraypart) (lua_State *L, int idx);  /* 2.30.1 */
LUA_API void            (agn_hashpart) (lua_State *L, int idx);   /* 2.30.1 */
LUA_API int             (agn_hasarraypart) (lua_State *L, int idx);  /* 3.9.7 */
LUA_API int             (agn_hashashpart) (lua_State *L, int idx);  /* 3.9.7 */
LUA_API void            (agn_toset) (lua_State *L, int idx);      /* 2.30.2 */
LUA_API void            (agn_unique) (lua_State *L, int idx, int approx, int inplace);  /* 2.34.4 */
LUA_API void            (agn_copy) (lua_State *L, int idx, int what);  /* 0.22.1 */
LUA_API void            (agn_join) (lua_State *L, int idx, int nargs);        /* 2.34.5 */
LUA_API void            (agn_replace) (lua_State *L, int idx, int nargs);     /* 2.34.5 */
LUA_API void            (agn_instr) (lua_State *L, int idx, int nargs);       /* 2.34.5 */
LUA_API void            (agn_upper) (lua_State *L, int idx, int nargs);       /* 4.3.3 */
LUA_API void            (agn_lower) (lua_State *L, int idx, int nargs);       /* 4.3.3 */
LUA_API void            (agn_trim) (lua_State *L, int idx, int nargs);        /* 4.3.3 */
LUA_API void            (agn_char) (lua_State *L, int idx);                   /* 4.3.3 */
LUA_API void            (agn_values) (lua_State *L, int idx, int nargs);      /* 2.34.5 */
LUA_API void            (agn_times) (lua_State *L, int idx, int nargs);       /* 2.34.5 */
LUA_API void            (agn_sumup) (lua_State *L, int idx, int type, const char *procname);  /* 4.4.6 */
LUA_API void            (agn_mulup) (lua_State *L, int idx);
LUA_API void            (agn_sumupdiv) (lua_State *L, int idx, lua_Number d, int type, const char *procname);  /* 4.4.6 */
LUA_API void            (agn_qsumupdiv) (lua_State *L, int idx, lua_Number d, int type, const char *procname);  /* 4.5.0 */
LUA_API int             (agn_qmdev) (lua_State *L, int idx, lua_Number *mean, lua_Number *qmdev, const char *procname);  /* 4.6.2 */
LUA_API void            (agn_map) (lua_State *L, int idx1, int idx2);
LUA_API void            (agn_select) (lua_State *L, int idx1, int idx2);
LUA_API void            (agn_checkfor) (lua_State *L, int idx1, int idx2);
LUA_API void            (agn_count) (lua_State *L, int idx1, int idx2);
LUA_API void            (agn_sstate) (lua_State *L, int idx, size_t a[]);     /* 0.10.0 */
LUA_API void            (agn_seqstate) (lua_State *L, int idx, size_t a[]);   /* 0.11.0 */
LUA_API void            (agn_regstate) (lua_State *L, int idx, size_t a[]);   /* 2.3.0 RC 3 */
LUA_API void            (agn_pairstate) (lua_State *L, int idx, size_t a[]);  /* 2.3.0 RC 3 */
LUA_API void            (agn_udstate) (lua_State *L, int idx, size_t a[]);    /* 4.9.0 */

LUA_API int             (agn_minmax) (lua_State *L, int idx, int absolute, lua_Number *min, size_t *minpos, lua_Number *max, size_t *maxpos, int *count);  /* 4.9.2 */
LUA_API int             (agn_lse) (lua_State *L, int idx, lua_Number *lse, lua_Number *maximum, const char *procname);  /* 4.9.2 */

/*
** push functions (C -> stack)
*/
LUA_API void  (lua_pushnil) (lua_State *L);
LUA_API void  (lua_pushnumber) (lua_State *L, lua_Number n);
LUA_API void  (lua_pushundefined) (lua_State *L);  /* added 0.10.0 */
LUA_API void  (lua_pushinteger) (lua_State *L, lua_Integer n);
LUA_API void  (lua_pushlstring) (lua_State *L, const char *s, size_t l);
LUA_API void  (lua_pushstring) (lua_State *L, const char *s);
LUA_API void  (lua_pushchar) (lua_State *L, char s);  /* 2.12.0 RC 2 */
LUA_API const char *(lua_pushvfstring) (lua_State *L, const char *fmt,
                                                      va_list argp);
LUA_API const char *(lua_pushfstring) (lua_State *L, const char *fmt, ...);
LUA_API void  (lua_pushcclosure) (lua_State *L, lua_CFunction fn, int n);
LUA_API void  (lua_pushboolean) (lua_State *L, int b);
LUA_API void  (agn_pushboolean) (lua_State *L, int b);  /* 1.8.16 */
LUA_API void  (lua_pushlightuserdata) (lua_State *L, void *p);
LUA_API int   (lua_pushthread) (lua_State *L);

#define lua_pushtrue(L)     lua_pushboolean(L, 1)
#define lua_pushfalse(L)    lua_pushboolean(L, 0)
#define lua_pushfail(L)     agn_pushboolean(L, -1)

#define agn_isnumber(L, idx)   (lua_type((L), (idx)) == LUA_TNUMBER)   /* 2.11.3 */
#define agn_isstring(L, idx)   (lua_type((L), (idx)) == LUA_TSTRING)   /* 2.11.3 */

/*
** get functions (Lua -> stack)
*/

LUA_API int    (lua_gettable) (lua_State *L, int idx);
LUA_API int    (lua_getfield) (lua_State *L, int idx, const char *k);

LUA_API int    (lua_hasfield) (lua_State *L, int idx, const char *k);
LUA_API int    (lua_rawget) (lua_State *L, int idx);
LUA_API int    (lua_rawgeti) (lua_State *L, int idx, int n);
LUA_API int    (lua_geti) (lua_State *L, int idx, lua_Integer n);  /* 2.21.1 */
LUA_API void   (lua_rawgetirange) (lua_State *L, int idx, int p, int q);  /* 4.0.0 RC 2 */
LUA_API void   (lua_rawgetiposrelat) (lua_State *L, int idx, size_t i, int n, int posrelat);  /* 4.0.0 */
LUA_API int    (agn_arrayorhashgeti) (lua_State *L, int idx, lua_Integer n, int array);  /* 2.37.3 */
LUA_API int    (lua_rawgetp) (lua_State *L, int idx, const void *p);  /* 2.21.2 */
LUA_API void   (lua_createtable) (lua_State *L, int narr, int nrec);
#define agn_createtable lua_createtable
LUA_API void  *(lua_newuserdata) (lua_State *L, size_t sz);
LUA_API void  *(lua_newuserdatauv) (lua_State *L, size_t size, int nuvalue);  /* 2.21.2 */
LUA_API int    (lua_getmetatable) (lua_State *L, int objindex);
LUA_API void   (lua_getfenv) (lua_State *L, int idx);
LUA_API void   (lua_getfenvi) (lua_State *L, int idx, int n);
LUA_API size_t (agn_size) (lua_State *L, int idx);   /* 0.12.3 */
LUA_API size_t (agn_asize) (lua_State *L, int idx);  /* 2.3.0 RC 4 */
LUA_API size_t (agn_hsize) (lua_State *L, int idx);  /* 5.1.2 */
LUA_API void   (agn_resize) (lua_State *L, int idx, int nasize);  /* 4.6.3 */
LUA_API void   (agn_reorder) (lua_State *L, int idx, int arraypartonly);  /* 2.30.1 */
LUA_API void   (agn_clear) (lua_State *L, int idx);
LUA_API int    (agn_deletefrom) (lua_State *L, int idxv, int idx);  /* 5.1.2 */
LUA_API int    (agn_tabpurge) (lua_State *L, int idx, int len, int pos);  /* 3.8.1 */
LUA_API int    (agn_tabsubs) (lua_State *L, int idx, int idxold, int idxnew);  /* 5.1.3 */
LUA_API int    (agn_in) (lua_State *L, int idxv, int idxt, int mode);  /* 3.9.6 */
LUA_API int    (agn_numunion) (lua_State *L, int idxa, int idxb);  /* 3.10.0 */
LUA_API int    (agn_numintersect) (lua_State *L, int idxa, int idxb);  /* 3.10.0 */
LUA_API int    (agn_numminus) (lua_State *L, int idxa, int idxb);  /* 3.10.0 */

LUA_API int    (agn_tisnumber) (lua_State *L, int idx, int integralkeysonly);  /* 3.10.2 */
LUA_API int    (agn_tisnumeric) (lua_State *L, int idx, int integralkeysonly);  /* 3.19.2 */
LUA_API int    (agn_tisintegral) (lua_State *L, int idx, int integralkeysonly);  /* 3.10.2 */
LUA_API int    (agn_tisposint) (lua_State *L, int idx, int integralkeysonly);  /* 3.10.2 */
LUA_API int    (agn_tispositive) (lua_State *L, int idx, int integralkeysonly);  /* 3.10.2 */
LUA_API int    (agn_tisnonnegint) (lua_State *L, int idx, int integralkeysonly);  /* 3.10.2 */
LUA_API int    (agn_tisnonzeroint) (lua_State *L, int idx, int integralkeysonly);  /* 4.11.0 */
LUA_API int    (agn_tisnonnegative) (lua_State *L, int idx, int integralkeysonly);  /* 3.10.2 */
LUA_API int    (agn_tiscomplex) (lua_State *L, int idx, int integralkeysonly);  /* 3.10.2 */
LUA_API int    (agn_tisstring) (lua_State *L, int idx, int integralkeysonly);  /* 3.10.2 */
LUA_API int    (agn_tisboolean) (lua_State *L, int idx, int integralkeysonly);  /* 3.10.2 */

LUA_API int    (agn_usisnumber) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_usisintegral) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_usiscomplex) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_usisstring) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_usisboolean) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_usisposint) (lua_State *L, int idx);  /* 3.19.2 */
LUA_API int    (agn_usispositive) (lua_State *L, int idx);  /* 3.19.2 */
LUA_API int    (agn_usisnonnegint) (lua_State *L, int idx);  /* 3.19.2 */
LUA_API int    (agn_usisnonzeroint) (lua_State *L, int idx);  /* 4.11.0 */
LUA_API int    (agn_usisnonnegative) (lua_State *L, int idx);  /* 3.19.2 */
LUA_API int    (agn_usisnumeric) (lua_State *L, int idx);  /* 3.19.2 */

LUA_API int    (agn_seqisnumber) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_seqisnumeric) (lua_State *L, int idx);  /* 3.19.2 */
LUA_API int    (agn_seqisintegral) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_seqisposint) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_seqispositive) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_seqisnonnegint) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_seqisnonzeroint) (lua_State *L, int idx);  /* 4.11.0 */
LUA_API int    (agn_seqisnonnegative) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_seqiscomplex) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_seqisstring) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_seqisboolean) (lua_State *L, int idx);  /* 3.10.2 */

LUA_API int    (agn_regisnumber) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_regisnumeric) (lua_State *L, int idx);  /* 3.19.2 */
LUA_API int    (agn_regisintegral) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_regisposint) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_regispositive) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_regisnonnegint) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_regisnonzeroint) (lua_State *L, int idx);  /* 4.11.0 */
LUA_API int    (agn_regisnonnegative) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_regiscomplex) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_regisstring) (lua_State *L, int idx);  /* 3.10.2 */
LUA_API int    (agn_regisboolean) (lua_State *L, int idx);  /* 3.10.2 */

LUA_API int    (agn_tblisall) (lua_State *L, int idx, const char *procname);  /* 4.6.2 */
LUA_API int    (agn_usisall) (lua_State *L, int idx, const char *procname);   /* 4.6.2 */
LUA_API int    (agn_seqisall) (lua_State *L, int idx, const char *procname);  /* 4.6.2 */
LUA_API int    (agn_regisall) (lua_State *L, int idx, const char *procname);  /* 4.6.2 */

#define lua_pushglobaltable(L)   lua_pushvalue(L, LUA_GLOBALSINDEX)  /* 2.21.2 */

LUA_API void   (agn_createset) (lua_State *L, int nrec);  /* 0.10.0 */
LUA_API int    (lua_srawget) (lua_State *L, int idx);     /* 0.11.2 */
LUA_API int    (lua_shas) (lua_State *L, int idx, int pop);  /* 2.31.5 */
LUA_API size_t (agn_ssize) (lua_State *L, int idx);  /* 0.12.3 */

LUA_API void   (agn_createseq) (lua_State *L, int nrec);  /* 0.11.0 */
LUA_API void   (agn_createreg) (lua_State *L, int nrec);  /* 2.3.0 RC 3 */

#ifndef PROPCMPLX
LUA_API void   (agn_createcomplex) (lua_State *L, agn_Complex c);  /* 0.12.0 */
#else
LUA_API void   (agn_createcomplex) (lua_State *L, lua_Number a, lua_Number b);  /* 0.12.2 */
#endif

LUA_API void   (lua_seqrawset) (lua_State *L, int idx);
LUA_API void   (lua_seqrawgeti) (lua_State *L, int idx, size_t n);
LUA_API int    (lua_seqrawseti) (lua_State *L, int idx, size_t n);  /* 0.11.0 */
LUA_API void   (agn_seqsetinumber) (lua_State *L, int idx, int n, lua_Number x);  /* 2.14.2 */
LUA_API void   (lua_seqrawgetinoerr) (lua_State *L, int idx, size_t n);  /* 4.0.0 */
LUA_API void   (lua_seqrawgetinoerrrange) (lua_State *L, int idx, int p, int q);  /* 4.0.0 RC 2 */
LUA_API lua_Number (lua_seqrawgetinumber) (lua_State *L, int idx, int n);
LUA_API lua_Number (agn_seqrawgetinumber) (lua_State *L, int idx, int n, int *rc);
LUA_API lua_Number (agn_seqrawgetiinteger) (lua_State *L, int idx, int n, int *rc);  /* 4.12.7 */
LUA_API void   (agn_seqrawgeticomplex) (lua_State *L, int idx, int n, lua_Number *z, int *rc);  /* 4.5.0 */
LUA_API const char *(agn_seqrawgetilstring) (lua_State *L, int idx, int n, size_t *len, int *rc);  /* 4.11.1 */
LUA_API lua_Number (agn_seqgetinumber) (lua_State *L, int idx, int n);  /* 2.2.0 RC 2 */
LUA_API void   (lua_seqinsert) (lua_State *L, int idx);  /* 0.11.0 */
LUA_API int    (agn_seqsubs) (lua_State *L, int idx, int idxold, int idxnew);  /* 5.1.3 */
LUA_API void   (lua_reginsert) (lua_State *L, int idx);  /* 2.34.2 */
LUA_API void   (agn_structinsert) (lua_State *L, int idxs, int idxv);  /* 2.34.2 */
LUA_API int    (agn_freeze) (lua_State *L, int idx, int readonly);  /* 4.8.2 */
LUA_API int    (agn_udfreeze) (lua_State *L, int idx, int readonly);  /* 4.8.2 */
LUA_API size_t (agn_seqsize) (lua_State *L, int idx);  /* 0.11.0 */
LUA_API void   (lua_seqrawget) (lua_State *L, int idx, int pushnil);  /* 0.11.2 */
LUA_API void   (agn_seqresize) (lua_State *L, int idx, size_t newsize);  /* 2.3.0 RC 4 */
LUA_API void   (agn_tabresize) (lua_State *L, int idx, size_t newsize, int checkholes);  /* 2.29.0 */
LUA_API void   (agn_setresize) (lua_State *L, int idx, size_t newsize, int protect);
LUA_API void   (agn_pairrawset) (lua_State *L, int idx);  /* 0.11.2 */
LUA_API void   (agn_pairrawget) (lua_State *L, int idx);  /* 0.11.2 */

LUA_API void   (agn_createpair) (lua_State *L, int idxleft, int idxright);  /* 0.11.1 */
LUA_API void   (agn_createpairnumbers) (lua_State *L, lua_Number l, lua_Number r);  /* 2.9.8 */
LUA_API void   (agn_createpairstrings) (lua_State *L, const char *x, const char *y);  /* 4.6.6 */
LUA_API void   (agn_createpairstringnumber) (lua_State *L, const char *x, lua_Number y);  /* 4.6.7 */
LUA_API int    (agn_pairgeti) (lua_State *L, int idx, int n);  /* 0.11.1 */
LUA_API void   (agn_pairgetiall) (lua_State *L, int idx);  /* 7.7.8 */
LUALIB_API int (agn_pairgetinumbers) (lua_State *L, int idx, lua_Number *x, lua_Number *y);  /* 4.11.2 */
LUALIB_API int (agn_pairgetiintegers) (lua_State *L, int idx, int *x, int *y);  /* 6.1.2 */
LUA_API int    (agn_pairseti) (lua_State *L, int idx, int pos);  /* 2.29.0 */

LUA_API void   (agn_regrawget) (lua_State *L, int idx, int pushnil);  /* 2.3.0 RC 3 */
LUA_API void   (agn_regrawgeti) (lua_State *L, int idx, size_t n);  /* 2.3.0 RC 3 */
LUA_API void   (agn_reggetinoerr) (lua_State *L, int idx, size_t n);  /* 4.0.0 */
LUA_API void   (agn_reggetinoerrrange) (lua_State *L, int idx, int p, int q);  /* 4.0.0 RC 2 */
LUA_API lua_Number (agn_regrawgetinumber) (lua_State *L, int idx, int n, int *rc);  /* 2.27.5 */
LUA_API lua_Number (agn_regrawgetiinteger) (lua_State *L, int idx, int n, int *rc);  /* 4.12.7 */
LUA_API void   (agn_regrawgeticomplex) (lua_State *L, int idx, int n, lua_Number *z, int *rc);  /* 4.5.0 */
LUA_API const char *(agn_regrawgetilstring) (lua_State *L, int idx, int n, size_t *len, int *rc);  /* 4.11.1 */
LUA_API lua_Number (agn_reggetinumber) (lua_State *L, int idx, int n);  /* 2.3.0 RC 3 */
LUA_API void   (agn_regset) (lua_State *L, int idx);  /* 2.3.0 RC 3 */
LUA_API int    (agn_regrawseti) (lua_State *L, int idx, size_t n);  /* 2.3.0 RC 3 */
LUA_API void   (agn_regsetinumber) (lua_State *L, int idx, int n, lua_Number x);  /* 2.14.2 */
LUA_API void   (agn_regpurge) (lua_State *L, int idx, int n);  /* 2.3.0 RC 3 */
LUA_API int    (agn_regsubs) (lua_State *L, int idx, int idxold, int idxnew);  /* 5.1.3 */
LUA_API int    (agn_regsettop) (lua_State *L, int idx);  /* 2.3.0 RC 3 */
LUA_API void   (agn_setregsize) (lua_State *L, size_t x);  /* 2.3.0 RC 3 */
LUA_API size_t (agn_getregsize) (lua_State *L);  /* 2.3.0 RC 3 */
LUA_API size_t (agn_reggettop) (lua_State *L, int idx);  /* 2.3.0 RC 3 */
LUA_API int    (agn_regsize) (lua_State *L, int idx);  /* 2.3.0 RC 3 */
LUA_API int    (agn_regresize) (lua_State *L, int idx, int newsize, int nilthem);  /* 4.6.4 */

LUA_API void   (agn_setutype) (lua_State *L, int idxobj, int idxtype);  /* 0.11.1 */
LUA_API void   (agn_setutypestring) (lua_State *L, int idxobj, const char *type);  /* 2.14.2 */
LUA_API int    (agn_getutype) (lua_State *L, int idx);  /* 0.11.1 */
LUA_API int    (agn_isutype) (lua_State *L, int idx, const char *str);  /* 1.9.1 */
LUA_API int    (agn_isutypeset) (lua_State *L, int idx);  /* 0.12.2 */
LUA_API int    (agn_issequtype) (lua_State *L, int idx, const char *str);  /* 0.12.1 */
LUA_API int    (agn_istableutype) (lua_State *L, int idx, const char *str);  /* 0.12.1 */
LUA_API int    (agn_issetutype) (lua_State *L, int idx, const char *str);  /* 0.14.0 */

/*
** set functions (stack -> Lua)
*/

LUA_API void  (lua_settable) (lua_State *L, int idx);
LUA_API void  (lua_setfield) (lua_State *L, int idx, const char *k);
LUA_API void  (agn_rawsetfield) (lua_State *L, int idx, const char *k);  /* 2.12.1 */
LUA_API void  (agn_deletefield) (lua_State *L, int idx, const char *k);  /* 2.31.8 */
LUA_API void  (agn_rawgetfield) (lua_State *L, int idx, const char *k);  /* 2.12.1 */
LUA_API void  (lua_rawset) (lua_State *L, int idx);
LUA_API void  (lua_rawseti) (lua_State *L, int idx, int n);
LUA_API void  (lua_seti) (lua_State *L, int idx, lua_Integer n);  /* 2.21.1 */
LUA_API void  (agn_rawinsert) (lua_State *L, int idx);  /* 2.11.4 */
LUA_API void  (agn_rawinsertfrom) (lua_State *L, int tidx, int vidx);  /* 2.11.4 */
LUA_API void  (agn_cleanse) (lua_State *L, int idx, int gc);  /* 2.33.2 */
LUA_API int   (lua_setmetatable) (lua_State *L, int objindex);
LUA_API void  (lua_setmetatabletoobject) (lua_State *L, int objindex, const char *k, int utype);  /* 2.14.2 */
LUA_API int   (lua_setfenv) (lua_State *L, int idx);
LUA_API void  (agn_setudmetatable) (lua_State *L, int idx);  /* 2.3.0 RC 3 */
LUA_API void  (lua_rawsetp) (lua_State *L, int idx, const void *p);  /* 2.21.2 */

LUA_API void  (lua_srawset) (lua_State *L, int idx);  /* 0.11.2 */
LUA_API void  (lua_sdelete) (lua_State *L, int idx);  /* 0.10.0 */
#define        lua_sinsert lua_srawset                /* reintroduced 2.31.5 */
LUA_API void  (agn_cleanseset) (lua_State *L, int idx, int gc);  /* 2.33.2 */

/* remember tables functions, initiated 0.9.2 */
LUA_API void  (agn_creatertable) (lua_State *L, int idx, int writemode);
LUA_API int   (agn_getrtable) (lua_State *L, int objindex);
LUA_API void  (agn_setrtable) (lua_State *L, int find, int kind, int vind);
LUA_API void  (agn_deletertable) (lua_State *L, int objindex);
LUA_API int   (agn_getrtablewritemode) (lua_State *L, int idx);  /* 0.22.0 */

LUA_API int   (agn_getfunctiontype) (lua_State *L, int idx);  /* 0.22.1 */

LUA_API int   (agn_getstorage) (lua_State *L, int objindex);  /* 2.33.1 */
LUA_API int   (agn_setstorage) (lua_State *L, int objindex);  /* 3.5.3 */
LUA_API int   (agn_setreadlibbed) (lua_State *L, const char *libname);  /* 0.26.0 */

LUA_API void  (lua_rawsetikey) (lua_State *L, int idx, int n);  /* Added 0.5.3 */
LUA_API void  (lua_rawset2) (lua_State *L, int idx);  /* Added 0.5.3 */
LUA_API lua_Number (agn_checknumber) (lua_State *L, int idx);
LUA_API const char *(agn_checklstring) (lua_State *L, int idx, size_t *len);
LUA_API const char *(agn_checkstring) (lua_State *L, int idx);

LUA_API int   (agn_getbuffersize) (lua_State *L);  /* 2.2.0 */
LUA_API void  (agn_setbuffersize) (lua_State *L, int value);  /* 2.2.0 */

/*
** `load' and `call' functions (load and run Lua code)
*/
LUA_API ptrdiff_t (lua_call) (lua_State *L, int nargs, int nresults);
LUA_API int   (lua_pcall) (lua_State *L, int nargs, int nresults, int errfunc);
LUA_API int   (lua_cpcall) (lua_State *L, lua_CFunction func, void *ud);
LUA_API int   (lua_load) (lua_State *L, lua_Reader reader, void *dt, const char *chunkname);

LUA_API int   (lua_dump) (lua_State *L, lua_Writer writer, void *data, int strip);

LUA_API lua_Number (agn_ncall) (lua_State *L, int nargs, int *error, int quit);  /* 0.9.2 */
LUA_API lua_Number (agn_npcall) (lua_State *L, int nargs, int *error, int quit);  /* 7.3.4 */

#ifndef PROPCMPLX
LUA_API agn_Complex (agn_ccall) (lua_State *L, int nargs);  /* 0.12.0 */
#else
LUA_API void (agn_ccall) (lua_State *L, int nargs, lua_Number *real, lua_Number *imag);  /* 0.27.1 */
#endif
LUA_API int (agn_initmethodcall) (lua_State *L, const char *tablename, int lentablename);  /* 4.4.1 */

LUA_API void  (agn_setbitwise) (lua_State *L, int value);  /* 0.27.0 */
LUA_API int   (agn_getbitwise) (lua_State *L);  /* 0.27.0 */
LUA_API void  (agn_setkahanozawa) (lua_State *L, int value);  /* 2.4.2 */
LUA_API int   (agn_getkahanozawa) (lua_State *L);  /* 2.4.2 */
LUA_API void  (agn_setkahanbabuska) (lua_State *L, int value);  /* 2.30.5 */
LUA_API int   (agn_getkahanbabuska) (lua_State *L);  /* 2.30.5 */
LUA_API void  (agn_setclosetozero) (lua_State *L, lua_Number x);  /* 2.31.12 */
LUA_API lua_Number (agn_getclosetozero) (lua_State *L);  /* 2.31.12 */
LUA_API void  (agn_setforadjust) (lua_State *L, int value);  /* 2.31.0 */
LUA_API int   (agn_getforadjust) (lua_State *L);  /* 2.31.0 */
LUA_API int   (agn_getseqautoshrink) (lua_State *L);  /* 2.29.0 */
LUA_API void  (agn_setseqautoshrink) (lua_State *L, int value);  /* 2.29.0 */
LUA_API int   (agn_getskipagenapath) (lua_State *L);  /* 2.35.4 */
LUA_API void  (agn_setskipagenapath) (lua_State *L, int value);  /* 2.35.4 */
LUA_API void  (agn_setemptyline) (lua_State *L, int value);  /* 0.30.4 */
LUA_API int   (agn_getemptyline) (lua_State *L);  /* 0.30.4 */
LUA_API void  (agn_setlibnamereset) (lua_State *L, int value);  /* 0.32.0 */
LUA_API int   (agn_getlibnamereset) (lua_State *L);  /* 0.32.0 */
LUA_API void  (agn_setlongtable) (lua_State *L, int value);  /* 0.32.0 */
LUA_API int   (agn_getlongtable) (lua_State *L);  /* 0.32.0 */
LUA_API void  (agn_setdebug) (lua_State *L, int value);  /* 0.32.2a */
LUA_API int   (agn_getdebug) (lua_State *L);  /* 0.32.2a */
LUA_API void  (agn_setgui) (lua_State *L, int value);  /* 0.33.3 */
LUA_API int   (agn_getgui) (lua_State *L);  /* 0.33.3 */
LUA_API void  (agn_setzeroedcomplex) (lua_State *L, int value);  /* 1.7.6 */
LUA_API int   (agn_getzeroedcomplex) (lua_State *L);  /* 1.7.6 */
LUA_API void  (agn_setpromptnewline) (lua_State *L, int value);  /* 1.7.6 */
LUA_API int   (agn_getpromptnewline) (lua_State *L);  /* 1.7.6 */
LUA_API void  (agn_setdigits) (lua_State *L, ptrdiff_t x);  /* 0.27.0 */
LUA_API int   (agn_getdigits) (lua_State *L);  /* 0.27.0 */
LUA_API int   (agn_geterrmlinebreak) (lua_State *L);  /* 2.11.4 */
LUA_API void  (agn_seterrmlinebreak) (lua_State *L, int x);  /* 2.11.4 */
LUA_API void  (agn_setepsilon) (lua_State *L, lua_Number eps);  /* 2.1.4 */
LUA_API void  (agn_setdblepsilon) (lua_State *L, lua_Number Eps);  /* 2.21.8 */
LUA_API void  (agn_sethepsilon) (lua_State *L, lua_Number Eps);  /* 2.31.1 */
LUA_API void  (agn_setnoini) (lua_State *L, int value);  /* 2.8.6*/
LUA_API int   (agn_getnoini) (lua_State *L);  /* 0.33.3 */
LUA_API void  (agn_setnomainlib) (lua_State *L, int value);  /* 2.8.6 */
LUA_API int   (agn_getnomainlib) (lua_State *L);  /* 0.33.3 */
LUA_API void  (agn_setiso8601) (lua_State *L, int value);  /* 2.9.8 */
LUA_API int   (agn_getiso8601) (lua_State *L);  /* 2.9.8 */
LUA_API int   (agn_getconstants) (lua_State *L);  /* 2.20.0 */
LUA_API void  (agn_setconstants) (lua_State *L, int value);  /* 2.20.0 */
LUA_API int   (agn_getduplicates) (lua_State *L);  /* 2.20.1 */
LUA_API void  (agn_setduplicates) (lua_State *L, int value);  /* 2.20.1 */
LUA_API int   (agn_getconstanttoobig) (lua_State *L);  /* 2.39.4 */
LUA_API void  (agn_setconstanttoobig) (lua_State *L, int value);  /* 2.39.4 */
LUA_API void  (agn_setenclose) (lua_State *L, int value);  /* 3.10.2 */
LUA_API int   (agn_getenclose) (lua_State *L);  /* 3.10.2 */
LUA_API void  (agn_setenclosedouble) (lua_State *L, int value);  /* 3.10.2 */
LUA_API int   (agn_getenclosedouble) (lua_State *L);  /* 3.10.2 */
LUA_API void  (agn_setenclosebackquotes) (lua_State *L, int value);  /* 3.10.2 */
LUA_API int   (agn_getenclosebackquotes) (lua_State *L);  /* 3.10.2 */
LUA_API void  (agn_sethistory) (lua_State *L, int value);  /* 5.2.2 */
LUA_API int   (agn_gethistory) (lua_State *L);  /* 5.2.2 */
LUA_API void  (agn_setnulliszero) (lua_State *L, int value);  /* 7.7.6 */
LUA_API int   (agn_getnulliszero) (lua_State *L);  /* 7.7.6 */
LUA_API void  (agn_sethistfile) (lua_State *L, char *filename);  /* 5.2.2 */
LUA_API char  *(agn_gethistfile) (lua_State *L);  /* 5.2.2 */
LUA_API void  (lua_setargs) (lua_State *L, int argc, char **argv);  /* 7.8.3 */

#ifndef __DJGPP__
LUA_API void  (agn_getround) (lua_State *L);  /* 2.8.6 */
LUA_API int   (agn_setround) (lua_State *L, const char *what);  /* 2.8.6 */
#endif

LUA_API lua_Number (agn_getstructuresize) (lua_State *L, int idx);

LUA_API void  *(agn_malloc) (lua_State *L, size_t size, const char *procname, ...);  /* 1.9.1 */
LUA_API char  *(agn_stralloc) (lua_State *L, size_t len, const char *procname, ...);  /* 2.16.5 */
LUA_API void  (agn_longarraytoseq) (lua_State *L, long double *a, size_t n);

LUA_API int   (agn_int2fb) (lua_State *L, unsigned int x);
LUA_API int   (agn_fb2int) (lua_State *L, int x);

/*
** coroutine functions
*/
LUA_API int   (lua_yield) (lua_State *L, int nresults);
LUA_API int   (lua_resume) (lua_State *L, int narg);
LUA_API int   (lua_status) (lua_State *L);
/* Returns 1 if the given coroutine can yield, and 0 otherwise. 2.21.1 */
#define lua_isyieldable(L) (yieldable(L))

/*
** Warning-related functions
*/
LUA_API void (lua_setwarnf) (lua_State *L, lua_WarnFunction f, void *ud);  /* taken from Lua 5.4 */
LUA_API int  (lua_getwarnf) (lua_State *L);  /* taken from Lua 5.4.0 RC 4 */
LUA_API void (lua_warning)  (lua_State *L, const char *msg, int tocont);  /* taken from Lua 5.4 */

/*
** garbage-collection function and options
*/

#define LUA_GCSTOP         0
#define LUA_GCRESTART      1
#define LUA_GCCOLLECT      2
#define LUA_GCCOUNT        3
#define LUA_GCCOUNTB       4
#define LUA_GCSTEP         5
#define LUA_GCSETPAUSE     6
#define LUA_GCSETSTEPMUL   7
#define LUA_GCSTATUS       8  /* 2.2.5 */

LUA_API int (lua_gc) (lua_State *L, int what, int data);
LUA_API LUAI_UMEM agn_usedbytes (lua_State *L);

/*
** miscellaneous functions
*/

LUA_API int   (lua_absindex) (lua_State *L, int idx);
LUA_API int   (lua_error) (lua_State *L);

LUA_API int   (lua_next) (lua_State *L, int idx);
LUA_API int   (lua_usnext) (lua_State *L, int idx);
LUA_API int   (lua_seqnext) (lua_State *L, int idx);  /* 0.11.2 */
LUA_API int   (lua_strnext) (lua_State *L, int idx);  /* 0.12.0 */
LUA_API int   (agn_fnext) (lua_State *L, int idxtable, int idxfunction, int mode);  /* added 0.9.1 */
LUA_API int   (lua_regnext) (lua_State *L, int idx);  /* 2.3.0 RC 3 */

LUA_API void  (lua_concat) (lua_State *L, int n);

LUA_API const char *agn_strmatch (lua_State *L, const char *s, size_t slen,
  const char *p, ptrdiff_t init, ptrdiff_t *start, ptrdiff_t *end);

LUA_API lua_Alloc (lua_getallocf) (lua_State *L, void **ud);
LUA_API void  (lua_setallocf) (lua_State *L, lua_Alloc f, void *ud);

#define topnfile(L, n)       ((FILE **)luaL_checkudata(L, n, AGN_FILEHANDLE))
#define topfile(L)           ((FILE **)luaL_checkudata(L, 1, AGN_FILEHANDLE))
#define iotopfile(L)         ((FILE **)luaL_checkudata(L, 1, LUA_FILEHANDLE))
#define iotopnfile(L, n)     ((FILE **)luaL_checkudata(L, n, LUA_FILEHANDLE))

LUA_API int (agn_tofileno) (lua_State *L, int n, int all);
LUA_API uint64_t (lua_stringtouint64) (lua_State *L, const char *s, int issueerror, const char *procname, int *rc);
LUA_API int64_t (lua_stringtoint64) (lua_State *L, const char *s, int issueerror, const char *procname, int *rc);

LUA_API int lua_log2 (lua_State *L, unsigned int x);
LUA_API int agn_log2 (lua_State *L, unsigned int x);

LUA_API void agn_checkvalidpath (lua_State *L, const char *filename, int pop, const char *procname);


/*
** ===============================================================
** some useful macros
** ===============================================================
*/

#define lua_pop(L,n)      lua_settop(L, -(n)-1)

#define lua_newtable(L)      lua_createtable(L, 0, 0)

#define lua_register(L,n,f) (lua_pushcfunction(L, (f)), lua_setglobal(L, (n)))

#define lua_pushcfunction(L,f)   lua_pushcclosure(L, (f), 0)

#define lua_strlen(L,i)      lua_objlen(L, (i))

#define lua_isfunction(L,n)   (lua_type(L, (n)) == LUA_TFUNCTION)
#define lua_istable(L,n)      (lua_type(L, (n)) == LUA_TTABLE)
#define lua_islightuserdata(L,n)   (lua_type(L, (n)) == LUA_TLIGHTUSERDATA)
#define lua_isnil(L,n)        (lua_type(L, (n)) == LUA_TNIL)
#define lua_isboolean(L,n)    (lua_type(L, (n)) == LUA_TBOOLEAN)
#define lua_isthread(L,n)     (lua_type(L, (n)) == LUA_TTHREAD)
#define lua_isnone(L,n)       (lua_type(L, (n)) == LUA_TNONE)
#define lua_isnoneornil(L, n) (lua_type(L, (n)) <= 0)
#define lua_isset(L,n)        (lua_type(L, (n)) == LUA_TSET)
#define lua_isseq(L,n)        (lua_type(L, (n)) == LUA_TSEQ)
#define lua_isreg(L,n)        (lua_type(L, (n)) == LUA_TREG)
#define lua_ispair(L,n)       (lua_type(L, (n)) == LUA_TPAIR)
#define lua_iscomplex(L,n)    (lua_type(L, (n)) == LUA_TCOMPLEX)

#define lua_pushliteral(L,s)  lua_pushstring(L, "" s)  /* 2.22.0, taken from Lua 5.4.0 */
#define lua_tostring(L,i)     lua_tolstring(L, (i), NULL)

#define lua_setglobal(L,s)    lua_setfield(L, LUA_GLOBALSINDEX, (s))
#define lua_getglobal(L,s)    lua_getfield(L, LUA_GLOBALSINDEX, (s))

/* Agena 1.6.7 and later: macros for former C API functions */

/* macros to insert into tables */

/* use agn_setinumber instead, this functionality is used quite often in Agena */
#define lua_rawsetinumber(L, idx, n, num) { \
  lua_pushnumber(L, num); \
  lua_rawseti(L, (idx) - 1, n); }

#define lua_rawsetiinteger(L, idx, n, num) { \
  lua_pushinteger(L, num); \
  lua_rawseti(L, (idx) - 1, n); }

/* -1 = fail, 0 = false, 1 = true */
#define lua_rawsetiboolean(L, idx, n, num) { \
  lua_pushboolean(L, num); \
  lua_rawseti(L, (idx) - 1, n); }

#define lua_rawsetistring(L,idx,n,str) { \
  lua_pushstring(L, str); \
  lua_rawseti(L, (idx) - 1, n); }

#define lua_rawsetilstring(L,idx,n,str,l) { \
  lua_pushlstring(L, str, l); \
  lua_rawseti(L, (idx) - 1, n); }

#define lua_rawsetstringnumber(L, idx, str, n) { \
  lua_pushnumber(L, n); \
  agn_rawsetfield(L, (idx) - 1, str); }

#define lua_rawsetstringinteger(L, idx, str, n) { \
  lua_pushinteger(L, n); \
  agn_rawsetfield(L, (idx) - 1, str); }

#define lua_rawsetstringstring(L, idx, str, text) { \
  lua_pushstring(L, text); \
  agn_rawsetfield(L, (idx) - 1, str); }

#define lua_rawsetstringchar(L, idx, str, chr) { \
  char __rsscchar[2]; \
  __rsscchar[0] = (char)chr; \
  __rsscchar[1] = '\0'; \
  lua_pushlstring(L, __rsscchar, 1); \
  agn_rawsetfield(L, (idx) - 1, str); }

#define lua_rawsetstringboolean(L, idx, str, boolean) { \
  agn_pushboolean(L, boolean); \
  agn_rawsetfield(L, (idx) - 1, str); }

#define lua_rawsetstringpairnumbers(L, idx, str, x, y) { \
  lua_pushnumber(L, (x)); \
  lua_pushnumber(L, (y)); \
  agn_createpair(L, -2, -1); \
  lua_remove(L, -2); \
  lua_remove(L, -2); \
  agn_rawsetfield(L, (idx) - 1, str); }

/* macros to insert into sets */

#define lua_srawsetnumber(L,idx,n) { \
  lua_pushnumber(L, n); \
  lua_srawset(L, (idx) - 1); }

#define lua_srawsetstring(L,idx,str) { \
  lua_pushstring(L, str); \
  lua_srawset(L, (idx) - 1); }

#define lua_srawsetlstring(L,idx,str,l) { \
  lua_pushlstring(L, str, l); \
  lua_srawset(L, (idx) - 1); }

/* macros to insert into sequences */

#define lua_seqrawsetistring(L, idx, n, str) { \
  lua_pushstring(L, str); \
  lua_seqrawseti(L, (idx) - 1, n); }

#define lua_seqrawsetilstring(L, idx, n, str, len) { \
  lua_pushlstring(L, str, len); \
  lua_seqrawseti(L, (idx) - 1, n); }


/* creates an array a of type lua_Number with size n, FREE it ! 7.5.15 Dr. Memory fix */
#define agn_createarray(a, n, procname) { \
  (a) = calloc((n), sizeof(lua_Number)); \
  if ((a) == NULL) \
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", (procname)); \
}

/* creates an array a of type lua_Number with size n, FREE it ! 7.5.15 Dr. Memory fix */
#define agn_createlongarray(a, n, procname) { \
  (a) = calloc((n), sizeof(long double)); \
  if ((a) == NULL) \
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", (procname)); \
}

/*
** compatibility macros
*/

#define lua_open()           luaL_newstate()

#define lua_getregistry(L)   lua_pushvalue(L, LUA_REGISTRYINDEX)
#define lua_getconstants(L)  lua_pushvalue(L, LUA_CONSTANTSINDEX)

#define lua_Chunkreader      lua_Reader
#define lua_Chunkwriter      lua_Writer

#define luaL_boxpointer(L,u)    \
  (*(void **)(lua_newuserdata(L, sizeof(void *))) = (u))
#define  luaL_unboxpointer(L,i,t)  \
  *((void**)luaL_checkudata(L,i,t))


/*
** generic functions
*/

#define ISFLOAT(x)         (TRUNC((x)) != (x))
#define ISINT(x)           (TRUNC((x)) == (x))
#define ISPOSINT(x)        ((x) > 0 && TRUNC((x)) == (x))
#define ISNEGINT(x)        ((x) < 0 && TRUNC((x)) == (x))
#define ISNONNEGINT(x)     ((x) > -1 && TRUNC((x)) == (x))
#undef ABS
#define ABS(x)             ( ((x) < 0) ? -(x) : (x) )

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define LUA_HOOKCALL      0
#define LUA_HOOKRET       1
#define LUA_HOOKLINE      2
#define LUA_HOOKCOUNT     3
#define LUA_HOOKTAILRET   4


/*
** Event masks
*/
#define LUA_MASKCALL    (1 << LUA_HOOKCALL)
#define LUA_MASKRET     (1 << LUA_HOOKRET)
#define LUA_MASKLINE    (1 << LUA_HOOKLINE)
#define LUA_MASKCOUNT   (1 << LUA_HOOKCOUNT)

typedef struct lua_Debug lua_Debug;  /* activation record */


/* Functions to be called by the debugger in specific events */
typedef void (*lua_Hook) (lua_State *L, lua_Debug *ar);


LUA_API int (lua_getstack) (lua_State *L, int level, lua_Debug *ar);
LUA_API int (lua_getinfo) (lua_State *L, const char *what, lua_Debug *ar);
LUA_API void (lua_getnupvalues) (lua_State *L, lua_Debug *ar);
LUA_API const char *(lua_getlocal) (lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *(lua_setlocal) (lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *(lua_getupvalue) (lua_State *L, int funcindex, int n);
LUA_API const char *(lua_setupvalue) (lua_State *L, int funcindex, int n);
LUA_API int (lua_getarity) (lua_State *L, const lua_Debug *ar, int *varargs);  /* 2.12.1 */
LUA_API int (lua_arity) (lua_State *L, lua_Debug *ar);
LUA_API int (lua_nupvalues) (lua_State *L, int funcindex);  /* 2.12.3 */
LUA_API int (lua_sethook) (lua_State *L, lua_Hook func, int mask, int count);
LUA_API lua_Hook (lua_gethook) (lua_State *L);
LUA_API int (lua_gethookmask) (lua_State *L);
LUA_API int (lua_gethookcount) (lua_State *L);


struct lua_Debug {
  int event;
  const char *name;      /* (n) */
  const char *namewhat;  /* (n) `global', `local', `field', `method' */
  const char *what;      /* (S) `Lua', `C', `main', `tail' */
  const char *source;    /* (S) */
  int currentline;       /* (l) */
  int nups;              /* (u) number of upvalues */
  int linedefined;       /* (S) */
  int lastlinedefined;   /* (S) */
  char short_src[LUA_IDSIZE]; /* (S) */
  int arity;             /* (a) 2.1.1 */
  int hasvararg;         /* 2.10.4 */
  /* private part */
  int i_ci;  /* active function */
};

LUA_API const char *agn_gettoken (lua_State *L, int token);  /* 2.31.3 */

/* from Lua 5.4.0 */

LUA_API void lua_len (lua_State *L, int idx);


/* from Lua 5.2.4 */

#define LUA_UNSIGNED   unsigned LUA_INT32

typedef LUA_UNSIGNED   lua_Unsigned;

LUA_API lua_Integer  lua_tointegerx (lua_State *L, int idx, int *isnum);  /* taken from Lua 5.2.4 */
LUA_API lua_Unsigned lua_tounsignedx (lua_State *L, int idx, int *isnum);  /* taken from Lua 5.2.4 */
LUA_API void         lua_pushunsigned (lua_State *L, lua_Unsigned u);  /* taken from Lua 5.2.4 */

LUA_API lua_Number   lua_tonumberx (lua_State *L, int i, int *isnum);
LUA_API void         lua_getuservalue (lua_State *L, int i);
LUA_API void         lua_setuservalue (lua_State *L, int i);

LUA_API void  *lua_getextraspace (lua_State *L);

/* lhf's ae package functions for evaluating mathematical expressions in C */

LUA_API void ae_open (void);
LUA_API void ae_close (void);
LUA_API void ae_set (const char *name, double value);
LUA_API double ae_eval (const char *expression);
LUA_API const char *ae_error (void);


/* }====================================================================== */


/******************************************************************************

* Copyright (C) 1994-2026 Lua.org, PUC-Rio. All rights reserved.
* Copyright Agena: (C) 2006-2026 Alexander Walz. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

#endif

