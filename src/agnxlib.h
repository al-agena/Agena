/*
** $Id: agnxlib.h, 2009/03/01 15:41:13 $
** Auxiliary functions for building Agena libraries
** See Copyright Notice in agena.h
*/

#ifndef agnxlib_h
#define agnxlib_h


#include <stddef.h>
#include <stdio.h>

#include "agena.h"
#include "llimits.h"  /* for defines: lua_assert, lua_lock, lua_unlock */

#if defined(LUA_COMPAT_GETN)
LUALIB_API int (luaL_getn)  (lua_State *L, int t);
LUALIB_API void (luaL_setn) (lua_State *L, int t, int n);
#else
#define luaL_getn(L,i)          ((int)lua_objlen(L, i))
#define luaL_setn(L,i,j)        ((void)0)  /* no op! */
#endif

#if defined(LUA_COMPAT_OPENLIB)
#define luaI_openlib  luaL_openlib
#endif


/* extra error code for `luaL_load' */
#define LUA_ERRFILE     (LUA_ERRERR + 1)

typedef struct luaL_Reg {
  const char *name;
  lua_CFunction func;
} luaL_Reg;


LUALIB_API void (luaI_openlib) (lua_State *L, const char *libname,
                                const luaL_Reg *l, int nup);
LUALIB_API int  (luaI_libsize) (const luaL_Reg *l);
LUALIB_API void (luaL_register) (lua_State *L, const char *libname,
                                const luaL_Reg *l);
LUALIB_API int (luaL_getmetafield) (lua_State *L, int obj, const char *e);
LUALIB_API int (agnL_getmetafield) (lua_State *L, int obj, const char *event);
LUALIB_API void (luaL_setmetatype) (lua_State *L, int idx);
LUALIB_API int (luaL_callmeta) (lua_State *L, int obj, const char *e);
LUALIB_API int (luaL_argerror) (lua_State *L, int numarg, const char *extramsg);
LUALIB_API int (luaL_typerror) (lua_State *L, int narg, const char *tname);
LUALIB_API int (luaL_typeerror) (lua_State *L, int narg, const char *extramsg, int type);  /* Agena 1.1.0 */
#define luaL_argexpected(L,cond,arg,tname)  \
    ((void)((cond) || luaL_typeerror(L, (arg), (tname))))  /* 2.21.2 */

LUALIB_API const char *(luaL_checklstring) (lua_State *L, int numArg, size_t *l);
LUALIB_API const char *(luaL_checklstringornil) (lua_State *L, int numArg, size_t *l);
LUALIB_API const char *(luaL_optlstring) (lua_State *L, int numArg,
                                          const char *def, size_t *l);
LUALIB_API const char *(agnL_optstring) (lua_State *L, int narg, const char *def);  /* 2.9.0 */
LUALIB_API const char *(agnL_tolstringx) (lua_State *L, int idx, size_t *len, const char *procname);  /* 2.26.1 */
LUALIB_API int         (agnL_strtonumber) (lua_State *L, int idx);  /* 2.39.1 */
LUALIB_API int         (agnL_strtocomplex) (lua_State *L, int idx);  /* 3.4.9 */
LUALIB_API int         (agnL_strunwrap) (lua_State *L, int idx, const char *delim, size_t delimlen);  /* 2.39.1 */

LUALIB_API lua_Number (luaL_checknumber) (lua_State *L, int numArg);
LUALIB_API lua_Number (luaL_optnumber) (lua_State *L, int nArg, lua_Number def);
LUALIB_API int        (agnL_checkboolean) (lua_State *L, int narg);
LUALIB_API lua_Number (agnL_checknumber) (lua_State *L, int numArg);  /* Agena 1.4.3/1.5.0 */
LUALIB_API lua_Number (agnL_optnumber) (lua_State *L, int nArg, lua_Number def);  /* Agena 1.4.3/1.5.0 */
LUALIB_API int        (agnL_optboolean) (lua_State *L, int narg, int def);  /* Agena 1.6.0 */

LUALIB_API lua_Integer (luaL_checkinteger) (lua_State *L, int numArg);
LUALIB_API lua_Integer (luaL_optinteger) (lua_State *L, int nArg, lua_Integer def);
LUALIB_API lua_Integer (agnL_optposint) (lua_State *L, int nArg, lua_Integer def);  /* 2.9.4 */
LUALIB_API lua_Integer (agnL_optnonnegint) (lua_State *L, int narg, lua_Integer def);  /* 2.10.0 */
LUALIB_API lua_Integer (agnL_checkinteger) (lua_State *L, int numArg);  /* Agena 1.4.3/1.5.0 */
LUALIB_API lua_Integer (agnL_optinteger) (lua_State *L, int nArg,  /* Agena 1.4.3/1.5.0 */
                                          lua_Integer def);
LUALIB_API lua_Number (agnL_optpositive) (lua_State *L, int narg, lua_Number def);   /* Agena 2.12.5 */
LUALIB_API lua_Number (agnL_optnonnegative) (lua_State *L, int narg, lua_Number def);  /* Agena 2.12.5 */

LUALIB_API int32_t (luaL_optint32_t) (lua_State *L, int narg, int32_t def);
LUALIB_API LUA_UINT32 (agnL_optuint32_t) (lua_State *L, int narg, uint32_t def);
LUALIB_API off64_t (luaL_optoff64_t) (lua_State *L, int narg, off64_t def);
LUALIB_API int32_t (luaL_checkint32_t) (lua_State *L, int narg);
LUALIB_API uint32_t (luaL_checkuint32_t) (lua_State *L, int narg);
LUALIB_API off64_t (luaL_checkoff64_t) (lua_State *L, int narg);

LUALIB_API void (luaL_checkstack) (lua_State *L, int sz, const char *msg);
LUALIB_API void (luaL_checkcache) (lua_State *LABC, int space, const char *procname);
LUALIB_API void (luaL_checktype) (lua_State *L, int narg, int t);
LUALIB_API void (luaL_checkany) (lua_State *L, int narg);

LUALIB_API int   (luaL_newmetatable) (lua_State *L, const char *tname);
LUALIB_API void *(luaL_checkudata) (lua_State *L, int ud, const char *tname);
LUALIB_API void *(luaL_getudata) (lua_State *L, int ud, const char *tname, int *result);  /* 0.12.2 */
LUALIB_API int   (luaL_isudata) (lua_State *L, int ud, const char *tname);  /* 2.14.6 */
LUALIB_API int   (agnL_calludata) (lua_State *L, int nargs, int regidx, const char *mt);  /* 5.4.0 */

LUALIB_API void (luaL_where) (lua_State *L, int lvl);
LUALIB_API int (luaL_error) (lua_State *L, const char *fmt, ...);

LUALIB_API int (luaL_checkoption) (lua_State *L, int narg, const char *def,
                                   const char *const lst[]);
LUALIB_API int (agnL_checkoption) (lua_State *L, int narg, const char *def, const char *const lst[], int ignorecase);  /* 2.9.0/2.16.1 */
LUALIB_API int (luaL_checksetting) (lua_State *L, int narg, const char *const lst[], const char *errmsg);

LUALIB_API int (agnL_getsetting) (lua_State *L, int idx, const char *const *modenames, const int *mode, const char *procname);
LUALIB_API int (luaL_ref) (lua_State *L, int t);
LUALIB_API void (luaL_unref) (lua_State *L, int t, int ref);

LUALIB_API int (luaL_loadfile) (lua_State *L, const char *filename);
LUALIB_API int (luaL_loadbuffer) (lua_State *L, const char *buff, size_t sz,
                                  const char *name);
LUALIB_API int (luaL_loadstring) (lua_State *L, const char *s);

LUALIB_API lua_State *(luaL_newstate) (void);

LUALIB_API const char *(luaL_gsub) (lua_State *L, const char *s, const char *p, const char *r);

LUALIB_API const char *(luaL_findtable) (lua_State *L, int idx, const char *fname, int szhint);

LUALIB_API void tag_error (lua_State *L, int narg, int tag);

LUALIB_API void (agnL_setLibname) (lua_State *L, int warning, int debug, int skipagenapath);
LUALIB_API void (agnL_initialise) (lua_State *L, int skipinis, int debug, int skipmainlib, int skipagenapath);

/* initialise the cache stack, new 2.37.0, fix 5.3.4; will be closed by agnL_onexit at exit and reset by `restart` */
LUALIB_API void agnL_initvaluecache (lua_State *L, const char *procname);

LUALIB_API const char *(luaL_pushnexttemplate) (lua_State *L, const char *path);
LUALIB_API int (agnL_pushvstring) (lua_State *L, const char *str, ...);

LUALIB_API void (agnL_printnonstruct) (lua_State *L, int i);
LUALIB_API int  (agnL_gettablefield) (lua_State *L, const char *table, const char *field, const char *procname, int issueerror);  /* 1.6.4 */
LUALIB_API int  (luaL_getsubtable) (lua_State *L, int idx, const char *fname);
LUALIB_API void (luaL_setfuncs) (lua_State *L, const luaL_Reg *l, int nup);
LUALIB_API int  (agnL_geti) (lua_State *L, int idx, int k);
LUALIB_API void (agnL_pairgetinumbers) (lua_State *L, const char *procname, int idx, lua_Number *x, lua_Number *y);
LUALIB_API void (agnL_pairgetilongnumbers) (lua_State *L, const char *procname, int idx, long double *x, long double *y);
LUALIB_API void (agnL_pairgeticomplex) (lua_State *L, const char *procname, int idx,
                    lua_Number *xreal, lua_Number *ximag, lua_Number *yreal, lua_Number *yimag);
LUALIB_API void (agnL_pairgetiintegers) (lua_State *L, const char *procname, int idx, int validrange, int *x, int *y);
LUALIB_API void (agnL_pairgetiposints) (lua_State *L, const char *procname, int idx, int validrange, int *x, int *y);
LUALIB_API void (agnL_pairgetinonnegints) (lua_State *L, const char *procname, int idx, int validrange, int *x, int *y);

LUALIB_API int  (agnL_gettop) (lua_State *L, const char *message, const char *procname);  /* 2.9.8 */

LUALIB_API lua_Number (agnL_fncall) (lua_State *L, int idx, lua_Number x, int optstart, int optstop);  /* 2.10.4 */
LUALIB_API lua_Number (agnL_fncallx) (lua_State *L, int idx, lua_Number x, int optstart, int optstop, int *error, int quit);  /* 6.6.11 */
LUALIB_API lua_Number (agnL_fneps) (lua_State *L, int fidx, lua_Number x, int n, int p, int q, lua_Number *origh, lua_Number *abserr);  /* 2.14.3 */
LUALIB_API int  (agnL_iscallable) (lua_State *L, int idx);
LUALIB_API int  (agnL_islinalgvector) (lua_State *L, int idx, size_t *dim);  /* 2.3.0 RC 4 */
LUALIB_API int  (agnL_fillarray) (lua_State *L, int idx, int type, lua_Number *a, size_t *ii, int left, int right, int reverse);  /* 2.10.4 */
LUALIB_API Time64_T (agnL_datetosecs) (lua_State *L, int idx, const char *procname, int err, int withLSDbug, int *ms);  /* 2.9.8/2.10.0 */

LUALIB_API void (agnL_onexit) (lua_State *L, int restart);  /* 2.7.0 */

LUALIB_API void (agnL_readlines) (lua_State *L, FILE *f, const char *procname, int ispipe);  /* 2.16.10 */
LUALIB_API int  (agnL_pexecute) (lua_State *L, const char *str, const char *procname);  /* 2.16.10 */

LUALIB_API void (agnL_issuememfileerror) (lua_State *L, size_t wrongnewsize, const char *procname);  /* 2.26.3 */
LUALIB_API lua_Number *(agnL_tonumarray) (lua_State *L, int idx, size_t *size, const char *procname, int duplicate, int throwerr);  /* 2.27.5 */
LUALIB_API void (luaL_aux_nstructure) (lua_State *L, const char *fn, size_t *nargs, size_t *offset,  /* moved from lbaselib.c 2.28.1 */
    lua_Number *a, lua_Number *b, volatile lua_Number *step, lua_Number *eps, size_t *total,
    int *isfunc, int *isdefault, int *isint);

LUALIB_API char *(agnL_strmatch) (lua_State *L, const char *data, int datalen, const char *pattern, int patternlen);
LUALIB_API int  (agnL_isvalidpattern) (lua_State *L, const char *p, size_t l, int *hasspecials, int pusherror);

LUALIB_API lua_Number (luaL_str2d) (lua_State *L, const char *s, int *overflow);

LUALIB_API int agnL_newldblarray (lua_State *L, size_t n, const char *procname);
LUALIB_API void agnL_pushcoeffs (lua_State *L, LongDoubleArray *a, size_t nops, const char *procname);
LUALIB_API void agnL_pushhex (lua_State *L, int idx, int classic);


/* Some macros, we use agnL_pushvstring only to prevent -Wformat-security warnings about missing format strings in Debian */
#define agnL_debuginfo(debug, string1) { \
  if (debug) { \
    agnL_pushvstring(L, string1, NULL); \
    fprintf(stderr, "%s", lua_tostring(L, -1)); fflush(stderr); \
    agn_poptop(L); \
  } \
}

#define agnL_debuginfo2(debug, string1, string2) { \
  if (debug) { \
    agnL_pushvstring(L, string1, string2, NULL); \
    fprintf(stderr, "%s", lua_tostring(L, -1)); fflush(stderr); \
    agn_poptop(L); \
  } \
}

#define agnL_debuginfo3(debug, string1, string2, string3) { \
  if (debug) { \
    agnL_pushvstring(L, string1, string2, string3, NULL); \
    fprintf(stderr, "%s", lua_tostring(L, -1)); fflush(stderr); \
    agn_poptop(L); \
  } \
}

#define agnL_rawget(L,procname) { \
  { int s; \
    s = lua_type(L, 1); \
    luaL_checkany(L, 2); \
    lua_settop(L, 2); \
    switch (s) { \
      case LUA_TTABLE: \
        lua_rawget(L, 1); \
        break; \
      case LUA_TSET: \
        lua_srawget(L, 1); \
        break; \
      case LUA_TSEQ: \
        lua_seqrawget(L, 1, 0); \
        break; \
      case LUA_TPAIR: \
        agn_pairrawget(L, 1); \
        break; \
      case LUA_TREG: \
        agn_regrawget(L, 1, 0); \
        break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": structure expected, got %s.", procname,  \
          lua_typename(L, s)); \
    } \
  } \
}


/* Compatibility with Lua 5.4 */
LUALIB_API int          (luaL_fileresult) (lua_State *L, int stat, const char *fname);
LUALIB_API int          (luaL_execresult) (lua_State *L, int stat);

/* Compatibility with Lua 5.2 */
LUALIB_API lua_Unsigned (luaL_optunsigned) (lua_State *L, int i, lua_Unsigned def);
LUALIB_API const char  *(luaL_tolstring) (lua_State *L, int idx, size_t *len);
LUALIB_API void         (luaL_setmetatable) (lua_State *L, const char *tname);
LUALIB_API int          (luaL_len) (lua_State *L, int i);
LUALIB_API void        *(luaL_testudata) (lua_State *L, int i, const char *tname);
LUALIB_API void         (luaL_traceback) (lua_State *L, lua_State *L1, const char *msg, int level);
LUALIB_API void         (luaL_requiref) (lua_State *L, char const* modname, lua_CFunction openf, int glb);

/* End of compatibility defs */

/* print a string */
#if !defined(lua_writestring)
#define lua_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(lua_writeline)
#define lua_writeline()        (lua_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(lua_writestringerror)
#define lua_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

#define luaL_nonumorcmplx(L,idx,procname) { \
  luaL_error(L, "Error in " LUA_QS ": (complex) number expected, got %s.", procname, luaL_typename(L, idx)); \
}

#define luaL_nonumorcmplxordual(L,idx,procname) { \
  luaL_error(L, "Error in " LUA_QS ": (complex, dual) number expected, got %s.", procname, luaL_typename(L, idx)); \
}

#define luaL_nonumordual(L,idx,procname) { \
  luaL_error(L, "Error in " LUA_QS ": (dual) number expected, got %s.", procname, luaL_typename(L, idx)); \
}

#define agnL_checkudfreezed(L,idx,what,pn) { \
  if (agn_udfreeze(L, idx, -1)) \
    luaL_error(L, "Error in " LUA_QS ": %s is read-only.", pn, what); \
}

/*
** ===============================================================
** some useful macros
** ===============================================================
*/

#define luaL_argcheck(L, cond,numarg,extramsg)  \
    ((void)((cond) || luaL_argerror(L, (numarg), (extramsg))))
#define luaL_typecheck(L, cond,numarg,extramsg,receivedtype)  \
    ((void)((cond) || luaL_typeerror(L, (numarg), (extramsg), (receivedtype))))
#define luaL_checkstring(L,n)  (luaL_checklstring(L, (n), NULL))
#define luaL_optstring(L,n,d)  (luaL_optlstring(L, (n), (d), NULL))
#define luaL_checkint(L,n)  ((int)luaL_checkinteger(L, (n)))
#define luaL_optint(L,n,d)  ((int)luaL_optinteger(L, (n), (d)))
#define luaL_checklong(L,n)  ((long)luaL_checkinteger(L, (n)))
#define agnL_checkint(L,n)  ((int)agnL_checkinteger(L, (n)))  /* Agena 1.4.3/1.5.0 */
#define luaL_optlong(L,n,d)  ((long)luaL_optinteger(L, (n), (d)))

#define luaL_typename(L,i)  lua_typename(L, lua_type(L,(i)))

#define luaL_dofile(L, fn) \
  (luaL_loadfile(L, fn) || lua_pcall(L, 0, LUA_MULTRET, 0))

#define luaL_dostring(L, s) \
  (luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))

#define luaL_getmetatable(L,n)  (lua_getfield(L, LUA_REGISTRYINDEX, (n)))

#define luaL_opt(L,f,n,d)  (lua_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

typedef struct luaL_Buffer {
  char *p;  /* current position in buffer */
  int lvl;  /* number of strings in the stack (level) */
  lua_State *L;
  char buffer[LUAL_BUFFERSIZE];
} luaL_Buffer;

#define luaL_addchar(B,c) \
  ((void)((B)->p < ((B)->buffer+LUAL_BUFFERSIZE) || luaL_prepbuffer(B)), \
   (*(B)->p++ = (char)(c)))

/* compatibility only */
#define luaL_putchar(B,c)  luaL_addchar(B,c)

#define luaL_addsize(B,n)  ((B)->p += (n))

LUALIB_API void (luaL_buffinit) (lua_State *L, luaL_Buffer *B);
LUALIB_API char *(luaL_prepbuffer) (luaL_Buffer *B);
LUALIB_API char *(luaL_prepbuffsize) (luaL_Buffer *B, size_t sz);
LUALIB_API void (luaL_addlstring) (luaL_Buffer *B, const char *s, size_t l);
LUALIB_API void (luaL_addstring) (luaL_Buffer *B, const char *s);
LUALIB_API void (luaL_addvalue) (luaL_Buffer *B);
LUALIB_API void (luaL_pushresult) (luaL_Buffer *B);
LUALIB_API void (luaL_clearbuffer) (luaL_Buffer *B);

LUALIB_API const char *getS (lua_State *L, void *ud, size_t *size);
LUALIB_API const char *getF (lua_State *L, void *ud, size_t *size);

/* }====================================================== */

/* compatibility with ref system */

/* pre-defined references */
#define LUA_NOREF       (-2)
#define LUA_REFNIL      (-1)

#define lua_ref(L,lock) ((lock) ? luaL_ref(L, LUA_REGISTRYINDEX) : \
      (lua_pushstring(L, "unlocked references are obsolete"), lua_error(L), 0))

#define lua_unref(L,ref)        luaL_unref(L, LUA_REGISTRYINDEX, (ref))

#define lua_getref(L,ref)       lua_rawgeti(L, LUA_REGISTRYINDEX, (ref))

#define luaL_reg  luaL_Reg

/* Stack Referencing functions by Rici Lake */

typedef struct luaRef luaRef;

LUALIB_API luaRef *luaL_newref (lua_State *L, int idx);
LUALIB_API void luaL_pushref (lua_State *L, luaRef *r);
LUALIB_API void luaL_freeref (lua_State *L, luaRef *r);

/* compatibility with luaposix package */

LUALIB_API void luaL_checknargs (lua_State *L, int maxargs);
LUALIB_API void luaL_checktypex (lua_State *L, int narg, int t, const char *expected);
LUALIB_API int  luaL_pusherror  (lua_State *L, const char *info);



#endif

