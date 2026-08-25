/*
** $Id: numarray.c,v 0.1 12.10.2015 $
** numarray - Numarray Library
** See Copyright Notice in agena.h
*/

#define numarray_c
#define LUA_LIB

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "ksort.h"
#include "llex.h"
#include "long.h"
#include "numarray.h"
#include "prepdefs.h"  /* FORCE_INLINE */

#ifndef PROPCMPLX
#define setcdarrayz2(v,i,p)       do { v[i] = p[0] + I*p[1]; } while (0)  /* set contents of double array z[2] to array */
#define setcdarrayp(v,i,p)        do { v[i] = p; } while (0)  /* set contents of agn_pComplex pointer into array */
#define copycdarray(to,i,from,j)  do { to[i] = from[j]; } while (0)
#define assigncdarray(v,i,re,im)  do { v[i] = re + I*im; } while (0)
#define iseqcdarray(v,i,w,j)      (v[i] == w[j])
#define iseqcdarrayp(v,i,w)       (v[i] == w)
#else
#define setcdarrayz2(v,i,p)       do { v[i][0] = p[0]; v[i][1] = p[1]; } while (0)
#define setcdarrayp(v,i,p)        do { v[i][0] = p[0]; v[i][1] = p[1]; } while (0)
#define copycdarray(to,i,from,j)  do { to[i][0] = from[j][0]; to[i][1] = from[j][1]; } while (0)
#define assigncdarray(v,i,re,im)  do { v[i][0] = re; v[i][1] = im; } while (0)
#define iseqcdarray(v,i,w,j)      (v[i][0] == w[j][0] && v[i][1] == w[j][1])
#define iseqcdarrayp(v,i,w)       (v[i][0] == w[0] && v[i][1] == w[1])
#endif

static void aux_pushcomplex (lua_State *L, agn_pComplex c) {  /* 4.11.3 */
  setcvalue(L->top++, c);
}


/* numarray.[uchar|double|integer](n)

   Creates a userdata array of unsigned chars or (signed) doubles with the given number of entries n, with n an integer,
   and with each slot set to the number 0. Initially, the number of elements can be zero or more, use numarray.resize to
   extend the array before assigning values.

   Due to advice by Luiz Henrique de Figueiredo the implementation uses explicit malloc's of the arrays, since
   resizing userdata via lmem functionality seems to be tricky:
   http://stackoverflow.com/questions/29806478/reallocating-resizing-lua-5-1-userdata-in-c */

/* leaves a numarray on top of the stack, 4.8.1. */

static int numarray_iterate (lua_State *L);

/* Expects numarray at the stack top */
static void aux_additerator (lua_State *L) {
  luaL_checkstack(L, 7, "not enough stack space");
  lua_getmetatable(L, -1);
  lua_pushcfunction(L, &numarray_iterate);  /* push numarray.iterate */
  lua_pushvalue(L, -3);   /* push numarray */
  lua_pushinteger(L, 1);  /* starting position */
  lua_pushinteger(L, 1);  /* step size */
  lua_pushfalse(L);       /* no bits */
  lua_pushtrue(L);        /* retrieve both index and value */
  lua_call(L, 5, 1);      /* create iterator */
  lua_setfield(L, -2, "__iter");  /* assign to metatable */
  agn_poptop(L);  /* pop metatable and leave numarray on the stack top, to be returned */
}

static NumArray *createarray (lua_State *L, char what, int64_t nops, int ndims, int64_t dims[], const char *procname) {
  NumArray *a;
  luaL_checkstack(L, 2, "not enough stack space");  /* for numarray userdata and its registry aux table, 4.11.3 patch */
  a = (NumArray *)lua_newuserdata(L, sizeof(NumArray));
  memset(a, 0, sizeof(NumArray));
  a->size = nops;
  if (nops > MAX_INT) {
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": array too large, max. of 2147483647 elements is accepted.", procname);
  }
  if (ndims < 1 || ndims > NAMAXDIMS) {
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": cannot have more than %d dimensions.", procname, NAMAXDIMS);
  }
  a->ndims = ndims;
  if (ndims == 1) {  /* the default */
    tools_bzero(a->dims, NAMAXDIMS*sizeof(int64_t));
    a->dims[0] = nops;  /* ditto */
  } else {
    int i;
    for (i=0; i < NAMAXDIMS; i++) a->dims[i] = dims[i];
  }
  a->datatype = what;
  switch (what) {
    case NAUCHAR: {
      a->data.c = calloc(nops, sizeof(unsigned char));
      if (a->data.c == NULL)
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      agn_setutypestring(L, -1, "uchar");
      break;
    }
    case NADOUBLE: {
      a->data.n = calloc(nops, sizeof(lua_Number));
      if (a->data.n == NULL)
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      agn_setutypestring(L, -1, "double");
      break;
    }
    case NACDOUBLE: {
      a->data.z = calloc(nops, sizeof(agn_Complex));
      if (a->data.z == NULL)
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      agn_setutypestring(L, -1, "cdouble");
      break;
    }
    case NAINT32: {
      a->data.i = calloc(nops, sizeof(int32_t));
      if (a->data.i == NULL)
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      agn_setutypestring(L, -1, "int32");
      break;
    }
    case NAUINT16: {
      a->data.us = calloc(nops, sizeof(uint16_t));
      if (a->data.us == NULL)
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      agn_setutypestring(L, -1, "ushort");
      break;
    }
    case NAUINT32: {
      a->data.ui = calloc(nops, sizeof(uint32_t));
      if (a->data.ui == NULL)
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      agn_setutypestring(L, -1, "uint32");
      break;
    }
    case NALDOUBLE: {
#ifndef __ARMCPU
      a->data.ld = calloc(nops, SIZEOFLDBL);
      if (a->data.ld == NULL) \
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      agn_setutypestring(L, -1, "longdouble");
#else
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", procname);
#endif
      break;
    }
    case NAUINT64: {  /* 6.2.3 */
      a->data.ui64 = calloc(nops, sizeof(uint64_t));
      if (a->data.ui64 == NULL)
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      agn_setutypestring(L, -1, "uint64");
      break;
    }
    case NAINT64: {  /* 6.2.3 */
      a->data.i64 = calloc(nops, sizeof(int64_t));
      if (a->data.i64 == NULL)
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      agn_setutypestring(L, -1, "int64");
      break;
    }
    default: {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": datatype %d does not exist.", procname, what);
    }
  }
  lua_setmetatabletoobject(L, -1, "numarray", 0);
  aux_additerator(L);  /* 7.1.0 */
  lua_createtable(L, 0, 0);
  a->registry = luaL_ref(L, LUA_REGISTRYINDEX);
  return a;
}


static void numarray_checkoptions (lua_State *L, int pos, int *nargs, int *newstruct, int *equality, const char *procname) {
  int checkoptions;
  *newstruct = 1;  /* returns a new structure, do not work in-place */
  *equality = 1;   /* 1 = strict equality, 0 = approximate equality */
  /* check for options, here `map in-place` */
  if (*nargs >= pos && lua_istrue(L, *nargs)) {  /* 2.36.2 optimisation, 4.8.0 fix */
    (*nargs)--; *newstruct = 0;
  }
  checkoptions = 2;  /* check n options, 2.29.0 */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {  /* 4.8.0 fix */
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("inplace", option)) {
        *newstruct = !agn_checkboolean(L, -1);
      } else if (tools_streq("strict", option)) {
        *equality = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
}


#define aux_prefill(L,ds,nops,nargs,fn,pn) { \
  int i, rc; \
  for (i=0; i < nops; i++) { \
    (ds)[i] = fn(L, nargs, i + 1, &rc); \
    if (!rc) { \
      luaL_error(L, "Error in " LUA_QS ": number expected as initialiser.", pn); \
    } \
  } \
}

#define aux_prefillwrap(L,ds,type,nops,nargs,pn) { \
  switch (type) { \
    case LUA_TTABLE: { \
      aux_prefill(L, ds, nops, nargs, agn_rawgetinumber, pn); \
      break; \
    } \
    case LUA_TSEQ: { \
      aux_prefill(L, ds, nops, nargs, agn_seqrawgetinumber, pn); \
      break; \
    } \
    case LUA_TREG: { \
      aux_prefill(L, ds, nops, nargs, agn_regrawgetinumber, pn); \
      break; \
    } \
    default: lua_assert(0); \
  } \
}

static uint64_t aux_getuint64 (lua_State *L, int idx) {
  uint64_t v = 0ULL;
  if (luaL_isudata(L, idx, "ints")) {
    Ints *b = (Ints *)lua_touserdata(L, idx);
    if (b->datatype != UINT64T) {
      if (idx < 0) agn_poptop(L);
      luaL_error(L, "Error: unsigned 64-bit integer expected.");
    }
    v = b->data.ui;
  } else if (agn_isnonnegint(L, idx)) {
    v = agn_tonumber(L, idx);
  } else {
    if (idx < 0) lua_remove(L, idx);
    luaL_error(L, "Error: unsigned 64-bit integer or number expected.");
  }
  return v;
}

static int64_t aux_getint64 (lua_State *L, int idx) {
  int64_t v = 0LL;
  if (luaL_isudata(L, idx, "ints")) {
    Ints *b = (Ints *)lua_touserdata(L, idx);
    if (b->datatype != INT64T) {
      if (idx < 0) agn_poptop(L);
      luaL_error(L, "Error: signed 64-bit integer expected.");
    }
    v = b->data.i;
  } else if (agn_isnonnegint(L, idx)) {
    v = agn_tonumber(L, idx);
  } else {
    if (idx < 0) lua_remove(L, idx);
    luaL_error(L, "Error: signed 64-bit integer or number expected.");
  }
  return v;
}

/* n = number of slots to initialise, nargs = number of arguments to buildfn */
static void aux_prefillarray (lua_State *L, int n, int nargs, const char *procname) {
  int type, nops;
  if (nargs < 2 || agn_isinteger(L, nargs)) return;
  NumArray *a = checknumarray(L, -1);
  type = lua_type(L, nargs);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG, nargs,
    "table, sequence or register expected", type);
  nops = (type == LUA_TTABLE) ? luaL_getn(L, nargs) : agn_nops(L, nargs);
  if (nops > n)
    luaL_error(L, "Error in " LUA_QS ": too many elements in initialiser, need %d.", procname, n);
  switch (a->datatype) {
    case NAUCHAR: {
      aux_prefillwrap(L, a->data.c, type, nops, nargs, procname);
      break;
    }
    case NADOUBLE: {
      aux_prefillwrap(L, a->data.n, type, nops, nargs, procname);
      break;
    }
    case NACDOUBLE: {
      int i, rc;
      lua_Number z[2];
      switch (type) {
        case LUA_TTABLE: {
          for (i=0; i < nops; i++) {
            agn_rawgeticomplex(L, nargs, i + 1, z, &rc);
            if (!rc)
              luaL_error(L, "Error in " LUA_QS ": (complex) number expected as initialiser.", procname);
            setcdarrayz2(a->data.z, i, z);
          }
          break;
        }
        case LUA_TSEQ: {
          for (i=0; i < nops; i++) {
            agn_seqrawgeticomplex(L, nargs, i + 1, z, &rc);
            if (!rc)
              luaL_error(L, "Error in " LUA_QS ": (complex) number expected as initialiser.", procname);
            setcdarrayz2(a->data.z, i, z);
          }
          break;
        }
        case LUA_TREG: {
          for (i=0; i < nops; i++) {
            agn_regrawgeticomplex(L, nargs, i + 1, z, &rc);
            if (!rc)
              luaL_error(L, "Error in " LUA_QS ": (complex) number expected as initialiser.", procname);
            setcdarrayz2(a->data.z, i, z);
          }
          break;
        }
        default:
          lua_assert(0);
      }
      break;
    }
    case NAINT32: {
      aux_prefillwrap(L, a->data.i, type, nops, nargs, procname);
      break;
    }
    case NAUINT16: {
      aux_prefillwrap(L, a->data.us, type, nops, nargs, procname);
      break;
    }
    case NAUINT32: {
      aux_prefillwrap(L, a->data.ui, type, nops, nargs, procname);
      break;
    }
    case NAUINT64: {
      int i;
      switch (type) {
        case LUA_TTABLE: {
          for (i=0; i < nops; i++) {
            lua_rawgeti(L, nargs, i + 1);
            a->data.ui64[i] = agnL_checkuint64(L, -1, procname);
            agn_poptop(L);
          }
          break;
        }
        case LUA_TSEQ: {
          for (i=0; i < nops; i++) {
            lua_seqrawgeti(L, nargs, i + 1);
            a->data.ui64[i] = agnL_checkuint64(L, -1, procname);
            agn_poptop(L);
          }
          break;
        }
        case LUA_TREG: {
          for (i=0; i < nops; i++) {
            agn_regrawgeti(L, nargs, i + 1);
            a->data.ui64[i] = agnL_checkuint64(L, -1, procname);
            agn_poptop(L);
          }
          break;
        }
        default:
          lua_assert(0);
      }
      break;
    }
    case NAINT64: {
      int i;
      switch (type) {
        case LUA_TTABLE: {
          for (i=0; i < nops; i++) {
            lua_rawgeti(L, nargs, i + 1);
            a->data.i64[i] = agnL_checkint64(L, -1, procname);
            agn_poptop(L);
          }
          break;
        }
        case LUA_TSEQ: {
          for (i=0; i < nops; i++) {
            lua_seqrawgeti(L, nargs, i + 1);
            a->data.i64[i] = agnL_checkint64(L, -1, procname);
            agn_poptop(L);
          }
          break;
        }
        case LUA_TREG: {
          for (i=0; i < nops; i++) {
            agn_regrawgeti(L, nargs, i + 1);
            a->data.i64[i] = agnL_checkint64(L, -1, procname);
            agn_poptop(L);
          }
          break;
        }
        default:
          lua_assert(0);
      }
      break;
    }
#ifndef __ARMCPU
    case NALDOUBLE: {
      aux_prefillwrap(L, a->data.ld, type, nops, nargs, procname);
      break;
    }
#else
    case NALDOUBLE: {
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", procname);
      break;
    }
#endif
    default: luaL_error(L, "Error in " LUA_QS ": this should not happen/1.", procname);
  }
}


#define NumarrayHeader   int nargs; int64_t n; int64_t dims[NAMAXDIMS]; int ndims = 1, i = 1, accu;

/* 4.12.8 fix for non-positive integer indices */
#define filldims(n,dims,ndims,accu,procname) { \
  tools_bzero(dims, NAMAXDIMS*sizeof(int64_t)); \
  accu = dims[0] = n; \
  for (; i < NAMAXDIMS && agn_isinteger(L, i + 1); i++) { \
    int64_t x = agn_tointeger(L, i + 1); \
    if (x < 1) { \
      luaL_error(L, "Error in " LUA_QS ": dimension is non-positive.", procname); \
    } \
    dims[i] = x; \
    ndims++; \
    accu *= dims[i]; \
  } \
  n = accu; \
}

/* Pushes a numarray of type `what' with nops elements, all set to zero, onto the stack. */
NumArray *numarray_createarray (lua_State *L, natype what, int64_t nops, const char *procname) {
  NumarrayHeader;
  (void)n; (void)nargs;
  filldims(nops, dims, ndims, accu, procname);
  return createarray(L, what, nops, ndims, dims, procname);
}


static int numarray_uchar (lua_State *L) {
  NumarrayHeader;
  nargs = lua_gettop(L);
  n = agnL_optnonnegint(L, 1, 0);
  filldims(n, dims, ndims, accu, "numarray.uchar");  /* 4.12.8 `filldims` fix */
  createarray(L, NAUCHAR, n, ndims, dims, "numarray.uchar");
  aux_prefillarray(L, n, nargs, "numarray.uchar");
  return 1;
}


static int numarray_double (lua_State *L) {
  NumarrayHeader;
  nargs = lua_gettop(L);
  n = agnL_optnonnegint(L, 1, 0);
  filldims(n, dims, ndims, accu, "numarray.double");  /* 4.12.8 `filldims` fix */
  createarray(L, NADOUBLE, n, ndims, dims, "numarray.double");
  aux_prefillarray(L, n, nargs, "numarray.double");
  return 1;
}


static int numarray_cdouble (lua_State *L) {
  NumarrayHeader;
  nargs = lua_gettop(L);
  n = agnL_optnonnegint(L, 1, 0);
  filldims(n, dims, ndims, accu, "numarray.cdouble");  /* 4.12.8 `filldims` fix */
  createarray(L, NACDOUBLE, n, ndims, dims, "numarray.cdouble");
  aux_prefillarray(L, n, nargs, "numarray.cdouble");
  return 1;
}


static int numarray_int32 (lua_State *L) {  /* name changed 2.12.1 due to new integer op */
  NumarrayHeader;
  nargs = lua_gettop(L);
  n = agnL_optnonnegint(L, 1, 0);
  filldims(n, dims, ndims, accu, "numarray.int32");  /* 4.12.8 `filldims` fix */
  createarray(L, NAINT32, n, ndims, dims, "numarray.int32");
  aux_prefillarray(L, n, nargs, "numarray.int32");
  return 1;
}


static int numarray_ushort (lua_State *L) {  /* 2.18.2 */
  NumarrayHeader;
  nargs = lua_gettop(L);
  n = agnL_optnonnegint(L, 1, 0);
  filldims(n, dims, ndims, accu, "numarray.ushort");  /* 4.12.8 `filldims` fix */
  createarray(L, NAUINT16, n, ndims, dims, "numarray.ushort");
  aux_prefillarray(L, n, nargs, "numarray.ushort");
  return 1;
}


static int numarray_uint32 (lua_State *L) {  /* 2.22.1 */
  NumarrayHeader;
  nargs = lua_gettop(L);
  n = agnL_optnonnegint(L, 1, 0);
  filldims(n, dims, ndims, accu, "numarray.uint32");  /* 4.12.8 `filldims` fix */
  createarray(L, NAUINT32, n, ndims, dims, "numarray.uint32");
  aux_prefillarray(L, n, nargs, "numarray.uint32");
  return 1;
}


static int numarray_longdouble (lua_State *L) {  /* 2.35.0 */
#ifdef __ARMCPU  /* 2.37.1 */
  luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.longdouble");
#else
  NumarrayHeader;
  nargs = lua_gettop(L);
  n = agnL_optnonnegint(L, 1, 0);
  filldims(n, dims, ndims, accu, "numarray.longdouble");  /* 4.12.8 `filldims` fix */
  createarray(L, NALDOUBLE, n, ndims, dims, "numarray.longdouble");
  aux_prefillarray(L, n, nargs, "numarray.longdouble");
#endif
  return 1;
}


static int numarray_uint64 (lua_State *L) {  /* 6.2.3 */
  NumarrayHeader;
  nargs = lua_gettop(L);
  n = agnL_optnonnegint(L, 1, 0);
  filldims(n, dims, ndims, accu, "numarray.uint64");
  createarray(L, NAUINT64, n, ndims, dims, "numarray.uint64");
  aux_prefillarray(L, n, nargs, "numarray.uint64");
  return 1;
}


static int numarray_int64 (lua_State *L) {  /* 6.2.3 */
  NumarrayHeader;
  nargs = lua_gettop(L);
  n = agnL_optnonnegint(L, 1, 0);
  filldims(n, dims, ndims, accu, "numarray.int64");
  createarray(L, NAINT64, n, ndims, dims, "numarray.int64");
  aux_prefillarray(L, n, nargs, "numarray.int64");
  return 1;
}


#ifdef PROPCMPLX
static void tocmplx (lua_State *L, int idx, union ldshape ds, agn_Complex z) {
  if (lua_isnumber(L, idx)) {
    z[0] = ds.d;
    z[1] = 0.0;
  } else {
    z[0] = ds.z[0];
    z[1] = ds.z[1];
  }
}
#else
#define tocmplx(L,idx,ds,v)  do { (v) = (lua_isnumber(L, (idx)) ? ds.d : ds.z); } while (0)
#endif

/* numarray.setitem (a, i, v)

   With a any numarray, sets number v to a[i], where i, the index, is an integer counting from 1. */
static int numarray_setitem (lua_State *L) {  /* set a number to the given slot. */
  NumArray *a = checknumarray(L, 1);
  union ldshape d;
  int64_t idx = tools_posrelat(agn_checkinteger(L, 2), a->size) - 1;  /* realign Lua/Agena index to C index, starting from 0, extended 2.15.1 */
  agnL_checkudfreezed(L, 1, "numarray", "numarray.setitem");  /* 4.9.0 */
  if (idx < 0 || idx >= a->size) {  /* 2.16.7 fix */
    if (idx == -1) {
      lua_settop(L, 3);  /* remove any surplus arguments, so that argument #3 can be put into the registry */
      lua_rawseti(L, LUA_REGISTRYINDEX, a->registry);
      return 0;
    } else
      luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "numarray.setitem", idx + 1);
  }
  getanynumber(L, d, 3, "numarray.setitem");
  switch (a->datatype) {
    case NAUCHAR:   a->data.c[idx]  = d.d; break;
    case NADOUBLE:  a->data.n[idx]  = d.d; break;
    case NACDOUBLE: {
      tocmplx(L, 3, d, a->data.z[idx]);
      break;
    }
    case NAINT32:   a->data.i[idx]  = d.d; break;
    case NAUINT16:  a->data.us[idx] = d.d; break;  /* 2.18.2 */
    case NAUINT32:  a->data.ui[idx] = d.d; break;  /* 2.22.1 */
    case NAUINT64: {
      if (lua_isnumber(L, 3))
        a->data.ui64[idx] = d.d;
      else if (isints(L, 3))
        a->data.ui64[idx] = d.ui64;
      else
        luaL_error(L, "Error in " LUA_QS ": expected a number or unsigned 64-bit integer.", "numarray.setitem");
      break;
    }  /* 6.2.3 */
    case NAINT64: {
      if (lua_isnumber(L, 3))
        a->data.i64[idx] = d.d;
      else if (isints(L, 3))
        a->data.i64[idx] = d.i64;
      else
        luaL_error(L, "Error in " LUA_QS ": expected a number or unsigned 64-bit integer.", "numarray.setitem");
      break;
    }
    case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
      a->data.ld[idx] = tolong(L, 3, d);
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.setitem");
#endif
      break;  /* 2.35.0 */
    default: lua_assert(0);
  }
  return 0;
}


/* Sets a+I*b, with a, b numbers, to the given position idx in a cdouble array (complex double array).
   With all other numarray types, issues an error. 4.11.3 */
static int numarray_setparts (lua_State *L) {
  NumArray *a = checknumarray(L, 1);
  int64_t idx = tools_posrelat(agn_checkinteger(L, 2), a->size) - 1;  /* realign Lua/Agena index to C index, starting from 0, extended 2.15.1 */
  if (idx < 0 || idx >= a->size)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "numarray.setparts", idx + 1);
  switch (a->datatype) {
    case NACDOUBLE: {
      agnL_checkudfreezed(L, 1, "numarray", "numarray.setparts");
#ifndef PROPCMPLX
      a->data.z[idx] = agn_checknumber(L, 3) + I*agn_checknumber(L, 4);
#else
      a->data.z[idx][0] = agn_checknumber(L, 3);
      a->data.z[idx][1] = agn_checknumber(L, 4);
#endif
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": expected a cdouble (complex double) array.", "numarray.setparts");
  }
  return 0;
}


/* Computes the one-dimensional index equivalent for multidimensional indices, 4.8.1, minor tweak 4.8.2 */
static FORCE_INLINE int64_t aux_getcoord (lua_State *L, NumArray *a, int to, const char *procname) {
  int64_t j, i = to, idx = 0LL, size = 1LL;
  while (1) {
    j = tools_posrelat(agn_checkinteger(L, i), a->dims[i - 2]);
    if (j < 1LL || j > a->dims[i - 2])
      luaL_error(L, "Error in " LUA_QS ": argument #%d is zero or out-of-range.", procname, i);
    idx += (j - 1LL)*size;
    if (i == 2) break;
    size *= a->dims[i - 2];
    i--;
  }
  return idx;
}

/* Sets number x to the slot denoted by its coordinates (one or more) i1, i2, ...; for example with a two-dimensional
   array, i1 denotes the row and i2 the column. The function works with one-dimensional arrays, as well. The function
   returns nothing. 4.8.1 */
static int seti (lua_State *L) {
  int64_t idx, size, i, n, x;
  int nargs, rc;
  NumArray *a;
  union ldshape d;
  idx = 0;
  size = 1;
  nargs = lua_gettop(L);
  a = (NumArray *)lua_touserdata(L, lua_upvalueindex(1));  /* the numarray */
  agnL_checkudfreezed(L, lua_upvalueindex(1), "numarray", "numarray.seti");
  if (lua_istable(L, 1)) {  /* the coordinates are in a table */
    i = n = luaL_getn(L, 1);
    if (n != a->ndims)
      luaL_error(L, "Error in " LUA_QS ": need %d table entries, got %d.", "numarray.seti", a->ndims, n);
    while (i--) {
      x = (int64_t)tools_posrelat(agn_rawgetiinteger(L, 1, i + 1, &rc), a->dims[i]) - 1;
      if (!rc || x < 0 || x >= a->dims[i]) {
        luaL_error(L, "Error in " LUA_QS ": entry #%d is not an integer, is zero or out-of-range.", "numarray.seti", i);
      }
      idx += x*size;
      if (i == 0) break;
      size *= a->dims[i];
    }
  } else {  /* the arguments are the coordinates */
    i = n = nargs - 1;
    if (n != a->ndims) {
      if (lua_isseq(L, 1) || lua_isreg(L, 1))
        luaL_error(L, "Error in " LUA_QS ": need a table of %d integers.", "numarray.seti", a->ndims);
      else
        luaL_error(L, "Error in " LUA_QS ": need %d integer arguments, got %d.", "numarray.seti", a->ndims, n);
    }
    while (i--) {
      x = (int64_t)tools_posrelat(agn_checkinteger(L, i + 1), a->dims[i]) - 1;
      if (x < 0 || x >= a->dims[i]) {
        luaL_error(L, "Error in " LUA_QS ": entry #%d is zero or out-of-range.", "numarray.seti", i);
      }
      idx += x*size;
      if (i == 0) break;
      size *= a->dims[i];
    }
  }
  getanynumber(L, d, nargs, "numarray.seti");
  switch (a->datatype) {
    case NAUCHAR:   { a->data.c[idx] = uchar(d.d);          break; }
    case NADOUBLE:  { a->data.n[idx] = d.d;                 break; }
    case NACDOUBLE: { tocmplx(L, nargs, d, a->data.z[idx]); break; }
    case NAINT32:   { a->data.i[idx] = (int32_t)d.d;        break; }
    case NAUINT16:  { a->data.us[idx] = (uint16_t)d.d;      break; }
    case NAUINT32:  { a->data.ui[idx] = (uint32_t)d.d;      break; }
    case NAUINT64: {
      if (lua_isnumber(L, nargs))
        a->data.ui64[idx] = d.d;
      else if (isints(L, nargs))
        a->data.ui64[idx] = d.ui64;
      else
        luaL_error(L, "Error in " LUA_QS ": expected a number or unsigned 64-bit integer.", "numarray.seti");
      break;
    }
    case NAINT64: {
      if (lua_isnumber(L, nargs))
        a->data.i64[idx] = d.d;
      else if (isints(L, nargs))
        a->data.i64[idx] = d.i64;
      else
        luaL_error(L, "Error in " LUA_QS ": expected a number or signed 64-bit integer.", "numarray.seti");
      break;
    }
    case NALDOUBLE: {
#ifndef __ARMCPU
      a->data.ld[idx] = tolong(L, nargs, d);
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.seti");
#endif
      break;
    }
    default: lua_assert(0);
  }
  return 0;
}

static int numarray_seti (lua_State *L) {
  int nargs;
  int64_t idx;
  NumArray *a;
  union ldshape d;
  nargs = lua_gettop(L);
  a = checknumarray(L, 1);
  agnL_checkudfreezed(L, 1, "numarray", "numarray.seti");  /* 4.9.0 */
  if (nargs == 1) {
    lua_pushvalue(L, 1);  /* push numarray */
    lua_pushcclosure(L, &seti, 1);
    return 1;
  }
  if (nargs < 3)
    luaL_error(L, "Error in " LUA_QS ": need at least an array and one index.", "numarray.seti");
  if (a->ndims == 1) {
    idx = tools_posrelat(agn_checkinteger(L, 2), a->size) - 1;  /* realign Lua/Agena index to C index, starting from 0, extended 2.15.1 */
    if (idx < 0 || idx >= a->size) {  /* if the index is zero, return internal status table */
      luaL_error(L, "Error in " LUA_QS ": argument 2 is out of range.", "numarray.seti");  /* 2.17.9 fix */
    }
  } else {
    if (a->ndims != nargs - 2)
      luaL_error(L, "Error in " LUA_QS ": expected %d coordinates, got %d.", "numarray.seti", a->ndims, nargs - 2);
    idx = aux_getcoord(L, a, nargs - 1, "numarray.seti");
  }
  getanynumber(L, d, nargs, "numarray.seti");
  switch (a->datatype) {
    case NAUCHAR:   { a->data.c[idx]  = d.d; break; }
    case NADOUBLE:  { a->data.n[idx]  = d.d; break; }
    case NACDOUBLE: { tocmplx(L, nargs, d, a->data.z[idx]); break; }  /* 4.12.9 fix */
    case NAINT32:   { a->data.i[idx]  = d.d; break; }
    case NAUINT16:  { a->data.us[idx] = d.d; break; }
    case NAUINT32:  { a->data.ui[idx] = d.d; break; }
    case NAUINT64: {
      if (lua_isnumber(L, nargs))
        a->data.ui64[idx] = d.d;
      else if (isints(L, nargs))
        a->data.ui64[idx] = d.ui64;
      else
        luaL_error(L, "Error in " LUA_QS ": expected a number or unsigned 64-bit integer.", "numarray.seti");
      break;
    }
    case NAINT64: {
      if (lua_isnumber(L, nargs))
        a->data.i64[idx] = d.d;
      else if (isints(L, nargs))
        a->data.i64[idx] = d.i64;
      else
        luaL_error(L, "Error in " LUA_QS ": expected a number or signed 64-bit integer.", "numarray.seti");
      break;
    }
    case NALDOUBLE: {
#ifndef __ARMCPU
      a->data.ld[idx] = tolong(L, nargs, d);
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.seti");
#endif
      break;
    }
    default: lua_assert(0);
  }
  return 0;
}


/* numarray.getitem (a, i)

  With a any numarray, returns the value stored at a[i], where i, the index, is an integer counting from 1.

  2.15.2, returns succeeding values from a numeric array. It is at least twice as fast as individually reading the items. */
static int numarray_getitem (lua_State *L) {
  int64_t i, n, idx;
  NumArray *a = checknumarray(L, 1);
  idx = tools_posrelat(agn_checkinteger(L, 2), a->size) - 1;  /* realign Lua/Agena index to C index, starting from 0, extended 2.15.1 */
  n = agnL_optposint(L, 3, 1);
  luaL_checkstack(L, n, "too many values");  /* 2.31.7 fix */
  if (idx < 0 || idx + n - 1 >= a->size) {  /* if the index is zero, return internal status table */
    if (idx == -1) {  /* 2.15.1 */
      agnL_checkudfreezed(L, 1, "numarray", "numarray.getitem");  /* 5.1.6 fix */
      lua_rawgeti(L, LUA_REGISTRYINDEX, a->registry);
      return 1;
    }
    luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "numarray.getitem", (int)idx + 1);  /* 2.17.9 fix */
  }
  switch (a->datatype) {
    case NAUCHAR:
      for (i=0; i < n; i++) {
        lua_pushnumber(L, a->data.c[idx + i]);
      }
      break;
    case NADOUBLE:
      for (i=0; i < n; i++) {
        lua_pushnumber(L, a->data.n[idx + i]);
      }
      break;
    case NACDOUBLE: {
      agn_pComplex z;
      for (i=0; i < n; i++) {
        z = a->data.z[idx + i];
        aux_pushcomplex(L, z);
      }
      break;
    }
    case NAINT32:
      for (i=0; i < n; i++) {
        lua_pushnumber(L, a->data.i[idx + i]);
      }
      break;
    case NAUINT16:  /* 2.18.2 */
      for (i=0; i < n; i++) {
        lua_pushnumber(L, a->data.us[idx + i]);
      }
      break;
    case NAUINT32:  /* 2.22.1 */
      for (i=0; i < n; i++) {
        lua_pushnumber(L, a->data.ui[idx + i]);
      }
      break;
    case NAUINT64:  /* 6.2.3 */
      for (i=0; i < n; i++) {
        createuint64(L, a->data.ui64[idx + i]);
      }
      break;
    case NAINT64:  /* 6.2.3 */
      for (i=0; i < n; i++) {
        createint64(L, a->data.i64[idx + i]);
      }
      break;
    case NALDOUBLE:  /* 2.35.0 */
#ifndef __ARMCPU  /* 2.37.1 */
      for (i=0; i < n; i++) {
        createdlong(L, a->data.ld[idx + i]);
      }
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.getitem");
#endif
      break;
    default: lua_assert(0);
  }
  return n;
}


/* Starting from position pos, returns the real and imaginary parts of n consecutive complex numbers in a complex
   double array. By default, pos is 1 and n is 1. 4.11.3 */
static int numarray_getparts (lua_State *L) {
  int64_t i, n, idx;
  agn_pComplex z;
  NumArray *a = checknumarray(L, 1);
  idx = tools_posrelat(agn_checkinteger(L, 2), a->size) - 1;  /* realign Lua/Agena index to C index, starting from 0 */
  n = agnL_optposint(L, 3, 1);
  luaL_checkstack(L, 2*n, "too many values");
  if (idx < 0 || idx + n - 1 >= a->size) {
    luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "numarray.getparts", (int)idx + 1);
  }
  if (a->datatype == NACDOUBLE) {
    for (i=0; i < n; i++) {
      z = a->data.z[idx + i];
      lua_pushnumber(L, real(z));
      lua_pushnumber(L, imag(z));
    }
  } else
    luaL_error(L, "Error in " LUA_QS ": expected a cdouble (complex double) array.", "numarray.getparts");
  return 2*n;
}


/* Retrieves the value in multidimensional array a by passing its respective indices i1, i2, ... For a two-dimensional
   array, i1 denotes the row and i2 the column. The function works with one-dimensional arrays, as well. */

static int geti (lua_State *L) {
  int64_t idx, size, i, n, x;
  int rc;
  NumArray *a;
  idx = 0;
  size = 1;
  a = (NumArray *)lua_touserdata(L, lua_upvalueindex(1));  /* the numarray */
  if (lua_istable(L, 1)) {  /* the coordinates are in a table */
    i = n = luaL_getn(L, 1);
    if (n != a->ndims)
      luaL_error(L, "Error in " LUA_QS ": need %d table entries, got %d.", "numarray.geti", a->ndims, n);
    while (i--) {
      x = (int64_t)tools_posrelat(agn_rawgetiinteger(L, 1, i + 1, &rc), a->dims[i]) - 1;
      if (!rc || x < 0 || x >= a->dims[i]) {
        luaL_error(L, "Error in " LUA_QS ": entry #%d is not an integer, is zero or out-of-range.", "numarray.geti", i);
      }
      idx += x*size;
      if (i == 0) break;
      size *= a->dims[i];
    }
  } else {  /* the arguments are the coordinates */
    i = n = lua_gettop(L);
    if (n != a->ndims) {
      if (lua_isseq(L, 1) || lua_isreg(L, 1))
        luaL_error(L, "Error in " LUA_QS ": need a table of %d integers.", "numarray.geti", a->ndims);
      else
        luaL_error(L, "Error in " LUA_QS ": need %d integer arguments, got %d.", "numarray.geti", a->ndims, n);
    }
    while (i--) {
      x = (int64_t)tools_posrelat(agn_checkinteger(L, i + 1), a->dims[i]) - 1;
      if (x < 0 || x >= a->dims[i]) {
        luaL_error(L, "Error in " LUA_QS ": entry #%d is zero or out-of-range.", "numarray.geti", i);
      }
      idx += x*size;
      if (i == 0) break;
      size *= a->dims[i];
    }
  }
  switch (a->datatype) {
    case NAUCHAR:
      lua_pushnumber(L, a->data.c[idx]);
      break;
    case NADOUBLE:
      lua_pushnumber(L, a->data.n[idx]);
      break;
    case NACDOUBLE: {
      agn_pComplex z = a->data.z[idx];
      aux_pushcomplex(L, z);
      break;
    }
    case NAINT32: {
      lua_pushnumber(L, a->data.i[idx]);
      break;
    }
    case NAUINT16: {
      lua_pushnumber(L, a->data.us[idx]);
      break;
    }
    case NAUINT32: {
      lua_pushnumber(L, a->data.ui[idx]);
      break;
    }
    case NAUINT64: {
      createuint64(L, a->data.ui64[idx]);
      break;
    }
    case NAINT64: {
      createint64(L, a->data.i64[idx]);
      break;
    }
    case NALDOUBLE: {
#ifndef __ARMCPU
      createdlong(L, a->data.ld[idx]);
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.geti");
#endif
      break;
    }
    default: lua_assert(0);
  }
  return 1;
}

static int numarray_geti (lua_State *L) {
  int nargs;
  int64_t idx;
  nargs = lua_gettop(L);
  NumArray *a = checknumarray(L, 1);
  if (nargs == 1) {  /* closure variant, 4.12.9 */
    lua_pushvalue(L, 1);  /* push numarray */
    lua_pushcclosure(L, &geti, 1);
    return 1;
  }
  if (a->ndims == 1) {
    idx = tools_posrelat(agn_checkinteger(L, 2), a->size) - 1;  /* realign Lua/Agena index to C index, starting from 0, extended 2.15.1 */
    if (idx < 0 || idx >= a->size) {  /* if the index is zero, return internal status table */
      if (idx == -1) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, a->registry);
        return 1;
      }
      luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "numarray.geti", (int)idx + 1);  /* 2.17.9 fix */
    }
  } else {
    if (nargs < 2)
      luaL_error(L, "Error in " LUA_QS ": need at least an array and one index.", "numarray.geti");
    if (a->ndims != nargs - 1) {
      luaL_error(L, "Error in " LUA_QS ": expected %d coordinates, got %d.", "numarray.geti", a->ndims, nargs - 1);
    }
    idx = aux_getcoord(L, a, nargs, "numarray.geti");
  }
  switch (a->datatype) {
    case NAUCHAR:
      lua_pushnumber(L, a->data.c[idx]);
      break;
    case NADOUBLE:
      lua_pushnumber(L, a->data.n[idx]);
      break;
    case NACDOUBLE: {
      agn_pComplex z = a->data.z[idx];
      aux_pushcomplex(L, z);
      break;
    }
    case NAINT32: {
      lua_pushnumber(L, a->data.i[idx]);
      break;
    }
    case NAUINT16: {
      lua_pushnumber(L, a->data.us[idx]);
      break;
    }
    case NAUINT32: {
      lua_pushnumber(L, a->data.ui[idx]);
      break;
    }
    case NAUINT64: {
      createuint64(L, a->data.ui64[idx]);
      break;
    }
    case NAINT64: {
      createint64(L, a->data.i64[idx]);
      break;
    }
    case NALDOUBLE: {
#ifndef __ARMCPU
      createdlong(L, a->data.ld[idx]);
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.geti");
#endif
      break;
    }
    default: lua_assert(0);
  }
  return 1;
}


/* Converts coordinates i1, i2, ... in the multidimensional array a to a one-dimensional index, starting from 1.
   For a two-dimensional array, i1 denotes the row and i2 the column. The function works with one-dimensional arrays, as well.
   4.8.1 */
static int numarray_one (lua_State *L) {
  int nargs;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": need at least an array and one index.", "numarray.one");
  NumArray *a = checknumarray(L, 1);
  if (a->ndims != nargs - 1)
    luaL_error(L, "Error in " LUA_QS ": expected %d coordinates, got %d.", "numarray.one", a->ndims, nargs - 1);
  lua_pushinteger(L, aux_getcoord(L, a, nargs, "numarray.one") + 1);
  return 1;
}


/* The procedure is also used to conduct OOP-style method calls via `__index' mmt. 2.17.9 */
static int numarray_subarray (lua_State *L) {
  int nargs;
  int64_t i, n, start, start0, stop;
  nargs = lua_gettop(L);
  NumArray *b = checknumarray(L, 1);
  if (nargs == 2 && !agn_isinteger(L, 2))  /* 4.3.2 call OOP method, 4.4.1 change */
    return agn_initmethodcall(L, AGENA_NUMARRAYLIBNAME, sizeof(AGENA_NUMARRAYLIBNAME) - 1);
  start0 = agn_tointeger(L, 2);  /* 2.19.1 fix: avoid returning registry table with index < 0 and -index >= size */
  start  = tools_posrelat(start0, b->size);  /* realign Lua/Agena index to C index, starting from 0, extended 2.15.1 */
  stop   = tools_posrelat(agnL_optinteger(L, 3, start), b->size);
  n = stop - start + 1;
  start--;
  luaL_checkstack(L, n, "too many values");  /* 2.31.7 fix */
  if (nargs == 2) {  /* simple indexing a[idx] */
    if (start < 0 || start + n - 1 >= b->size) {
      if (start0 == 0) {  /* 2.15.1; 2.19.1 fix */
        lua_rawgeti(L, LUA_REGISTRYINDEX, b->registry);
        return 1;
      }
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "numarray.subarray", (int)start0);
    }
    switch (b->datatype) {
      case NAUCHAR:
        for (i=0; i < n; i++) {
          lua_pushnumber(L, b->data.c[start + i]);
        }
        break;
      case NADOUBLE:
        for (i=0; i < n; i++) {
          lua_pushnumber(L, b->data.n[start + i]);
        }
        break;
      case NACDOUBLE: {
        agn_pComplex z;
        for (i=0; i < n; i++) {
          z = b->data.z[start + i];
          aux_pushcomplex(L, z);
        }
        break;
      }
      case NAINT32:
        for (i=0; i < n; i++) {
          lua_pushnumber(L, b->data.i[start + i]);
        }
        break;
      case NAUINT16:  /* 2.18.2 */
        for (i=0; i < n; i++) {
          lua_pushnumber(L, b->data.us[start + i]);
        }
        break;
      case NAUINT32:  /* 2.22.1 */
        for (i=0; i < n; i++) {
          lua_pushnumber(L, b->data.ui[start + i]);
        }
        break;
      case NAUINT64:  /* 6.2.3 */
        for (i=0; i < n; i++) {
          createuint64(L, b->data.ui64[start + i]);
        }
        break;
      case NAINT64:  /* 6.2.3 */
        for (i=0; i < n; i++) {
          createint64(L, b->data.i64[start + i]);
        }
        break;
      case NALDOUBLE:  /* 2.35.0 */
#ifndef __ARMCPU  /* 2.37.1 */
        for (i=0; i < n; i++) {
          createdlong(L, b->data.ld[start + i]);
        }
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.subarray");
#endif
        break;
      default: lua_assert(0);
    }
    return n;
  } else {  /* a[start to stop] indexing */
    NumArray *a;
    if (stop == 0 || start < 0 || start + 1 > stop || start + n - 1 >= b->size) {
      luaL_error(L, "Error in " LUA_QS ": indices %d to %d out of range.", "numarray.subarray", (int)start + 1, (int)stop);
    }
    a = createarray(L, b->datatype, n, b->ndims, b->dims, "numarray.subarray");
    switch (b->datatype) {
      case NAUCHAR:
        for (i=0; i < n; i++) {
          a->data.c[i] = b->data.c[start + i];
        }
        break;
      case NADOUBLE:
        for (i=0; i < n; i++) {
          a->data.n[i] = b->data.n[start + i];
        }
        break;
      case NACDOUBLE:
        for (i=0; i < n; i++) {
          copycdarray(a->data.z, i, b->data.z, start + i);
        }
        break;
      case NAINT32:
        for (i=0; i < n; i++) {
          a->data.i[i] = b->data.i[start + i];
        }
        break;
      case NAUINT16:  /* 2.18.2 */
        for (i=0; i < n; i++) {
          a->data.us[i] = b->data.us[start + i];
        }
        break;
      case NAUINT32:  /* 2.22.1 */
        for (i=0; i < n; i++) {
          a->data.ui[i] = b->data.ui[start + i];
        }
        break;
      case NAUINT64:  /* 6.2.3 */
        for (i=0; i < n; i++) {
          a->data.ui64[i] = b->data.ui64[start + i];
        }
        break;
      case NAINT64:  /* 6.2.3 */
        for (i=0; i < n; i++) {
          a->data.i64[i] = b->data.i64[start + i];
        }
        break;
      case NALDOUBLE:  /* 2.35.0 */
#ifndef __ARMCPU  /* 2.37.1 */
        for (i=0; i < n; i++) {
          a->data.ld[i] = b->data.ld[start + i];
        }
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.subarray");
#endif
        break;
      default: lua_assert(0);
    }
    return 1;
  }
}


static FORCE_INLINE void aux_copy (lua_State *L, NumArray *to, NumArray *from, const char *procname) {
  int64_t i, n;
  n = from->size;
  switch (from->datatype) {
    case NAUCHAR:
      for (i=0; i < n; i++) {
        to->data.c[i] = from->data.c[i];
      }
      break;
    case NADOUBLE:
      for (i=0; i < n; i++) {
        to->data.n[i] = from->data.n[i];
      }
      break;
    case NACDOUBLE:
      for (i=0; i < n; i++) {
        copycdarray(to->data.z, i, from->data.z, i);
      }
      break;
    case NAINT32:
      for (i=0; i < n; i++) {
        to->data.i[i] = from->data.i[i];
      }
      break;
    case NAUINT16:
      for (i=0; i < n; i++) {
        to->data.us[i] = from->data.us[i];
      }
      break;
    case NAUINT32:
      for (i=0; i < n; i++) {
        to->data.ui[i] = from->data.ui[i];
      }
      break;
    case NAUINT64:
      for (i=0; i < n; i++) {
        to->data.ui64[i] = from->data.ui64[i];
      }
      break;
    case NAINT64:
      for (i=0; i < n; i++) {
        to->data.i64[i] = from->data.i64[i];
      }
      break;
    case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
      for (i=0; i < n; i++) {
        to->data.ld[i] = from->data.ld[i];
      }
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", procname);
#endif
      break;
    default: lua_assert(0);
  }
}

static int numarray_replicate (lua_State *L) {  /* 2.31.3, based on numarray.subarray */
  NumArray *a = NULL;
  NumArray *b = checknumarray(L, 1);
  agnL_checkudfreezed(L, 1, "numarray", "numarray.replicate");  /* 4.9.0 */
  a = createarray(L, b->datatype, b->size, b->ndims, b->dims, "numarray.replicate");
  aux_copy(L, a, b, "numarray.replicate");
  return 1;
}


/* Gemini AI: Initialize ksort for the types if not already done in the file scope */
KSORT_INIT(uint8_t, uint8_t, ks_lt_generic)
KSORT_INIT(int32_t, int32_t, ks_lt_generic)
KSORT_INIT(uint16_t, uint16_t, ks_lt_generic)
KSORT_INIT(uint32_t, uint32_t, ks_lt_generic)
KSORT_INIT(uint64_t, uint64_t, ks_lt_generic)
KSORT_INIT(int64_t, int64_t, ks_lt_generic)
#ifndef KS_INTROSORT_LONG_DOUBLE_DEFINED
#define KS_INTROSORT_LONG_DOUBLE_DEFINED
KSORT_INIT(long_double, long double, ks_lt_generic)
#endif

static FORCE_INLINE void aux_klibintrosort (lua_State *L, NumArray *a) {
  switch (a->datatype) {
    case NAUCHAR:  ks_introsort(uint8_t, a->size, a->data.c); break;
    case NADOUBLE: klib_dintrosort(a->data.n, a->size); break;
    case NAINT32:  ks_introsort(int32_t, a->size, a->data.i); break;
    case NAUINT16: ks_introsort(uint16_t, a->size, a->data.us); break;
    case NAUINT32: ks_introsort(uint32_t, a->size, a->data.ui); break;
    case NAUINT64: ks_introsort(uint64_t, a->size, a->data.ui64); break;
    case NAINT64:  ks_introsort(int64_t, a->size, a->data.i64); break;
    case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
      ks_introsort(long_double, a->size, a->data.ld);
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.introsort");
#endif
      break;
    case NACDOUBLE: luaL_error(L, "Error in " LUA_QS ": complex doubles cannot be sorted.", "numarray.introsort");
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.kintrosort");
  }
}

static int numarray_introsort (lua_State *L) {
  NumArray *a = checknumarray(L, 1);
  agnL_checkudfreezed(L, 1, "numarray", "numarray.introsort");
  aux_klibintrosort(L, a);
  return 0;
}


static int numarray_sorted (lua_State *L) {  /* 2.31.8, based on numarray.replicate and numarray.sort */
  NumArray *a = NULL;
  NumArray *b = checknumarray(L, 1);
  agnL_checkudfreezed(L, 1, "numarray", "numarray.sorted");  /* 4.9.0 */
  a = createarray(L, b->datatype, b->size, b->ndims, b->dims, "numarray.sorted");
  aux_copy(L, a, b, "numarray.sorted");
  aux_klibintrosort(L, a);
  return 1;
}


#define NBITS (int64_t)(sizeof(unsigned char) * 8)  /* ! we need the cast to int64_t ! */

/* Source: https://stackoverflow.com/questions/2525310/how-to-define-and-work-with-an-array-of-bits-in-c */

/* numarray.setbit (a, idx, x)
   Sets bit n at index position idx of uchar array a. n must be either 0 or 1. idx starts from 1, the rightmost bit,
   not zero. The function returns nothing. */
static int numarray_setbit (lua_State *L) {  /* set a bit (0 or 1) to the given slot of a uchar array; 2.12.0 RC 4 */
  int64_t idx, x;
  NumArray *a = checknumarray(L, 1);
  if (a->size == 0)
    luaL_error(L, "Error in " LUA_QS ": array is of size zero.", "numarray.setbit");
  if (a->datatype != NAUCHAR)
    luaL_error(L, "Error in " LUA_QS ": can set bits only with uchar arrays.", "numarray.setbit");
  agnL_checkudfreezed(L, 1, "numarray", "numarray.setbit");  /* 4.9.0 */
  idx = tools_posrelat(agn_checkinteger(L, 2), a->size) - 1;  /* realign Lua/Agena index to C index, starting from 0, extended 2.15.1 */
  x = agn_checkinteger(L, 3);  /* realign Lua/Agena index to C index, starting from 0 */
  if (idx < 0 || idx >= a->size * NBITS)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "numarray.setbit", idx + 1);
  if (x < 0 || x > 1)
    luaL_error(L, "Error in " LUA_QS ": bit must be 0 or 1, got %d.", "numarray.setbit", x);
  if (x)
    /* void SetBit( int A[],  int k ) {  A[k/32] |= 1 << (k%32); }  set the bit at the k-th position in A[i] */
    a->data.c[idx / NBITS] |= 1 << (idx % NBITS);
  else
    /* #define ClearBit(A,k)   ( A[(k/32)] &= ~(1 << (k%32)) ) */
    a->data.c[idx / NBITS] &= ~(1 << (idx % NBITS));
  return 0;
}

/*
import numarray as n
a := n.uchar(1)
for i to 8 do print(n.getbit(a, i)) od
n.get(a, 1):
*/
/* numarray.getbit (a, i)
   Returns the bit at index position i of uchar array a. i starts from 1, the rightmost bit, not zero.
   The return is either 0 or 1. */
static int numarray_getbit (lua_State *L) {  /* get a bit from the given Lua/Agena index. */
  int64_t idx;
  NumArray *a = checknumarray(L, 1);
  if (a->size == 0)
    luaL_error(L, "Error in " LUA_QS ": array is of size zero.", "numarray.getbit");
  if (a->datatype != NAUCHAR)
    luaL_error(L, "Error in " LUA_QS ": can get bits only with uchar arrays.", "numarray.getbit");
  idx = tools_posrelat(agn_checkinteger(L, 2), a->size) - 1;  /* realign Lua/Agena index to C index, starting from 0, extended 2.15.1 */
  if (idx < 0 || idx >= a->size * NBITS)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "numarray.getbit", idx + 1);
  /* #define TestBit(A,k)    ( A[(k/32)] & (1 << (k%32)) ) */
  lua_pushnumber(L, ((a->data.c[idx / NBITS]) & (1 << (idx % NBITS))) != 0);
  return 1;
}


#define aux_purge(L,idx,datastruct,datatype) { \
  x = datastruct[idx]; \
  y = 0; \
  for (i=idx; i < a->size - 1; i++) \
    datastruct[i] = datastruct[i + 1]; \
  if (resize) { \
    if ((datastruct = realloc(datastruct, (--a->size + zerosize) * sizeof(datatype))) == NULL) \
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.purge"); \
  } else { \
    datastruct[a->size - 1] = 0; \
  } \
}

#ifdef PROPCMPLX
#define prop_purge(L,idx,datastruct,datatype) { \
  x = datastruct[idx][0]; \
  y = datastruct[idx][1]; \
  for (i=idx; i < a->size - 1; i++) { \
    datastruct[i][0] = datastruct[i + 1][0]; \
    datastruct[i][1] = datastruct[i + 1][1]; \
  } \
  if (resize) { \
    if ((datastruct = realloc((datastruct), (--a->size + zerosize) * sizeof(datatype))) == NULL) \
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.purge"); \
  } else { \
    datastruct[a->size - 1][0] = 0.0; \
    datastruct[a->size - 1][1] = 0.0; \
  } \
}
#else
#define prop_purge(L,idx,datastruct,datatype) { \
  x = creal(datastruct[idx]); \
  y = cimag(datastruct[idx]); \
  for (i=idx; i < a->size - 1; i++) { \
    datastruct[i] = datastruct[i + 1]; \
  } \
  if (resize) { \
    if ((datastruct = realloc((datastruct), (--a->size + zerosize) * sizeof(datatype))) == NULL) \
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.purge"); \
  } else { \
    datastruct[a->size - 1] = 0.0; \
  } \
}
#endif


static int numarray_purge (lua_State *L) {  /* 2.9.1, delete a number at a given slot. */
  int64_t idx;
  size_t i;
  int resize, zerosize;  /* realloc array after deletion ? */
  lua_Number x, y;
  x = 0; y = 0;
  NumArray *a = checknumarray(L, 1);
  agnL_checkudfreezed(L, 1, "numarray", "numarray.purge");  /* 4.9.0 */
  idx = tools_posrelat(agn_checkinteger(L, 2), a->size) - 1;  /* realign Lua/Agena index to C index, starting from 0, extended 2.15.1 */
  resize = agnL_optboolean(L, 3, 1);
  if (idx < 0 || idx >= a->size || a->size < 1)
    luaL_error(L, "Error in " LUA_QS ": index %d out-of-range or array empty.", "numarray.purge", idx + 1);
  zerosize = a->size == 1;  /* shrinking to zero blocks causes segmentation faults, so prevent this */
  switch (a->datatype) {
    case NAUCHAR:   aux_purge(L, idx, a->data.c, unsigned char); break;
    case NADOUBLE:  aux_purge(L, idx, a->data.n, lua_Number); break;
    case NACDOUBLE: {
      prop_purge(L, idx, a->data.z, agn_Complex);
      break;
    }
    case NAINT32:   aux_purge(L, idx, a->data.i, int32_t); break;
    case NAUINT16:  aux_purge(L, idx, a->data.us, uint16_t); break;  /* 2.18.2 */
    case NAUINT32:  aux_purge(L, idx, a->data.ui, uint32_t); break;  /* 2.22.1 */
    case NAUINT64:  aux_purge(L, idx, a->data.ui64, uint64_t); break;  /* 6.2.3 */
    case NAINT64:   aux_purge(L, idx, a->data.i64, int64_t); break;  /* 6.2.3 */
    case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
      aux_purge(L, idx, a->data.ld, long double);
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.purge");
#endif
      break;  /* 2.23.12 */
    default: luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.purge");
  }
  if (a->datatype == NACDOUBLE) {
#ifdef PROPCMPLX
    agn_pushcomplex(L, x, y);
#else
    agn_createcomplex(L, x + y*I);
#endif
  } else {
    lua_pushnumber(L, x);
  }
  return 1;
}


/* numarray.resize (a, n): The function resizes a numarray userdata structure a to the given number of entries n. Thus you
   can extend or shrink a numarray. [DELETED :: When shrinking a numarray, the function first overwrites the entries to be `deleted`
   with zeros.] When extending, it fills the new array slots with zeros.

   The function returns the new size, an integer.

   The function issues an error if the new size is less than 1 (an array cannot be shrunk to zero bytes). */

#define aux_resize(L,datastruct,datatype) { \
  if ((datastruct = realloc(datastruct, (n + zerosize) * sizeof(datatype))) == NULL) \
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.resize"); \
  if (n > a->size) { for (i=a->size; i < n; i++) datastruct[i] = 0; }; \
}  /* last line: fill added space with zeros */

#define cmplx_resize(L,datastruct,datatype) { \
  if ((datastruct = realloc(datastruct, (n + zerosize) * sizeof(datatype))) == NULL) \
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.resize"); \
  if (n > a->size) { for (i=a->size; i < n; i++) { assigncdarray(datastruct, i, 0, 0); }  }; \
}  /* last line: fill added space with zeros */

static int numarray_resize (lua_State *L) {
  int64_t n, i;
  int zerosize;
  NumArray *a = checknumarray(L, 1);
  agnL_checkudfreezed(L, 1, "numarray", "numarray.resize");  /* 4.9.0 */
  zerosize = 0;  /* added 2.9.1 */
  n = agnL_checkint(L, 2);  /* absolute new size */
  if (n > MAX_INT)
    luaL_error(L, "Error in " LUA_QS ": array too large, max. of 2147483645 elements accepted.", "numarray.resize");
  if (n  < 0)
    luaL_error(L, "Error in " LUA_QS ": invalid new size %d.", "numarray.resize", n);
  else if (a->size == 0 && n == 0) {  /* do nothing */
    lua_pushinteger(L, 0);
    return 1;
  } else if (n == 0)
    zerosize = 1;  /* trying to realloc to 0 bytes will cause segmentation faults later, so leave at least one block assigned */
  if (n != a->size) {  /* extend or shrink (or do nothing if a->size == n) */
    switch (a->datatype) {
      case NAUCHAR:   aux_resize(L, a->data.c, unsigned char); break;
      case NADOUBLE:  aux_resize(L, a->data.n, lua_Number); break;
      case NACDOUBLE: cmplx_resize(L, a->data.z, agn_Complex); break;
      case NAINT32:   aux_resize(L, a->data.i,  int32_t); break;
      case NAUINT16:  aux_resize(L, a->data.us, uint16_t); break;
      case NAUINT32:  aux_resize(L, a->data.ui, uint32_t); break;
      case NAUINT64:  aux_resize(L, a->data.ui64, uint64_t); break;
      case NAINT64:   aux_resize(L, a->data.i64, int64_t); break;
#ifndef __ARMCPU  /* 2.37.1 */
      case NALDOUBLE: aux_resize(L, a->data.ld, long double); break;
#else
      case NALDOUBLE:
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "mumarray.resize");
        break;
#endif
      default:
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.resize");
    }
    a->size = n + zerosize;  /* 2.15.1 fix */
  }
  lua_pushinteger(L, a->size);
  return 1;
}


/* numarray.iterate (a [, i [, p]])
Returns an iterator function that when called returns the next value in the numarray userdata structure
a, or null if there are no further entries in the structure.

If an index i is passed, the first call to the iterator function returns the i-th element in
the numarray list and with subsequent calls, the respective elements after index i.

You may also pass a positive integer step p to the iterator function: If given, then in subsequent
calls the p-th element after the respective current one is returned, equivalent to giving an optional
step size in numeric for loops.

Example:

> import numarray

> a := numarray.double(3)

> for i to 3 do a[i] := i * Pi od

> f := numarray.iterate(a, 2):  # return all values starting with index 2
procedure(01CDC200)

> f():
6.2831853071796

> f():
9.4247779607694

> f():  # no more values in a
null

import numarray as n
a := n.uchar(1)
for i to 8 do n.setbit(a, i, 1) od
n.get(a, 1):

f := numarray.iterate(a, 1, 1, true)
f():
*/

static int iteratebits (lua_State *L) {  /* 2.12.0 RC 4 */
  NumArray *a;
  int64_t pos, step;
  a = lua_touserdata(L, lua_upvalueindex(1));
  pos = lua_tointeger(L, lua_upvalueindex(2));
  step = lua_tointeger(L, lua_upvalueindex(3));
  if (pos > a->size * NBITS)
    lua_pushnil(L);
  else {
    lua_pushnumber(L, (a->data.c[(pos - 1) / NBITS] & ((1 << ((pos - 1) % NBITS)))) != 0);
    lua_pushinteger(L, pos + step);
    lua_replace(L, lua_upvalueindex(2));
  }
  return 1;
}

static int iterate (lua_State *L) {
  NumArray *a;
  size_t pos, step, pushidx;
  a = lua_touserdata(L, lua_upvalueindex(1));
  pos = lua_tointeger(L, lua_upvalueindex(2));
  step = lua_tointeger(L, lua_upvalueindex(3));
  /* pushidx is for for/in loops: if set to 1, the iterator a) returns both index and value, b) resets itself to the beginning
     so that for/in can loop over it again */
  pushidx = lua_tointeger(L, lua_upvalueindex(4));
  if (pos > a->size) {
    if (pushidx) {  /* reset */
      lua_pushinteger(L, pushidx);
      lua_replace(L, lua_upvalueindex(2));
      pushidx = 0;
    }
    lua_pushnil(L);  /* return an endmark for for/in */
  } else {
    if (pushidx) {
      luaL_checkstack(L, 1, "not enough stack space");
      lua_pushinteger(L, pos);
    }
    switch (a->datatype) {
      case NAUCHAR:  lua_pushnumber(L, a->data.c[pos - 1]);  break;
      case NADOUBLE: lua_pushnumber(L, a->data.n[pos - 1]);  break;
      case NACDOUBLE: {
        agn_pComplex z = a->data.z[pos - 1];
        aux_pushcomplex(L, z);
        break;
      }
      case NAINT32:  lua_pushnumber(L, a->data.i[pos - 1]);  break;
      case NAUINT16: lua_pushnumber(L, a->data.us[pos - 1]); break;  /* 2.18.2 */
      case NAUINT32: lua_pushnumber(L, a->data.ui[pos - 1]); break;  /* 2.22.1 */
      case NAUINT64: createuint64(L, a->data.ui64[pos - 1]); break;  /* 6.2.3 */
      case NAINT64:  createint64(L, a->data.i64[pos - 1]); break;  /* 6.2.3 */
#ifndef __ARMCPU  /* 2.37.1 */
      case NALDOUBLE: { createdlong(L, a->data.ui[pos - 1]); break; } /* 2.35.0 */
#else
      case NALDOUBLE:
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.iterate");
        break;
#endif
      default:
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.iterate factory");
    }
    lua_pushinteger(L, pos + step);
    lua_replace(L, lua_upvalueindex(2));
  }
  return 1 + (pushidx != 0);
}

static int numarray_iterate (lua_State *L) {
  int64_t pos, step, pushidx;
  int bits;
  NumArray *a = checknumarray(L, 1);
  pos = tools_posrelat(agnL_optinteger(L, 2, 1), a->size);  /* extended 2.15.1 */
  step = agnL_optposint(L, 3, 1);
  bits = agnL_optboolean(L, 4, 0);  /* 2.12.0 RC 4 */
  pushidx = agnL_optboolean(L, 5, 0);  /* 7.1.0 */
  if (bits == 0) {
    if (pos < 1 || (pos > a->size && a->size != 0))  /* when iterating, return null if the array is empty */
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "numarray.iterate", pos);
  } else {
    if (pos < 1 || ((pos * NBITS) > (a->size * NBITS) && a->size != 0))  /* when iterating, return null if the array is empty */
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "numarray.iterate", pos);
  }
  luaL_checkstack(L, 3, "not enough stack space");  /* 3.18.4 fix */
  lua_pushvalue(L, 1);
  lua_pushinteger(L, pos);  /* we iterate from index position 1, not 0 */
  lua_pushinteger(L, step);
  lua_pushinteger(L, (pushidx) ? pos : 0);
  lua_pushcclosure(L, (bits) ? &iteratebits : &iterate, 4);
  return 1;
}


static int cyclebits (lua_State *L) {
  NumArray *a;
  int64_t pos, step;
  a = lua_touserdata(L, lua_upvalueindex(1));
  pos = lua_tointeger(L, lua_upvalueindex(2));
  step = lua_tointeger(L, lua_upvalueindex(3));
  if (pos > a->size * NBITS)
    pos = lua_tointeger(L, lua_upvalueindex(4));
  lua_pushnumber(L, (a->data.c[(pos - 1) / NBITS] & ((1 << ((pos - 1) % NBITS)))) != 0);
  lua_pushinteger(L, pos + step);
  lua_replace(L, lua_upvalueindex(2));
  return 1;
}

static int cycle (lua_State *L) {
  NumArray *a;
  size_t pos, step;
  a = lua_touserdata(L, lua_upvalueindex(1));
  pos = lua_tointeger(L, lua_upvalueindex(2));
  step = lua_tointeger(L, lua_upvalueindex(3));
  if (pos > a->size)
    pos = lua_tointeger(L, lua_upvalueindex(4));
  switch (a->datatype) {
    case NAUCHAR:  lua_pushnumber(L, a->data.c[pos - 1]);  break;
    case NADOUBLE: lua_pushnumber(L, a->data.n[pos - 1]);  break;
    case NACDOUBLE: {
      agn_pComplex z = a->data.z[pos - 1];
      aux_pushcomplex(L, z);
      break;
    }
    case NAINT32:  lua_pushnumber(L, a->data.i[pos - 1]);  break;
    case NAUINT16: lua_pushnumber(L, a->data.us[pos - 1]); break;  /* 2.18.2 */
    case NAUINT32: lua_pushnumber(L, a->data.ui[pos - 1]); break;  /* 2.22.1 */
    case NAUINT64: createuint64(L, a->data.ui64[pos - 1]); break;  /* 6.2.3 */
    case NAINT64:  createint64(L, a->data.i64[pos - 1]); break;  /* 6.2.3 */
#ifndef __ARMCPU  /* 2.37.1 */
    case NALDOUBLE: { createdlong(L, a->data.ui[pos - 1]); break; } /* 2.35.0 */
#else
    case NALDOUBLE:
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.cycle factory");
      break;
#endif
    default:
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.iterate");
  }
  lua_pushinteger(L, pos + step);
  lua_replace(L, lua_upvalueindex(2));
  return 1;
}

static int numarray_cycle (lua_State *L) {  /* 2.15.5, based on numarray_iterate */
  int64_t pos, step;
  int bits;
  NumArray *a = checknumarray(L, 1);
  pos = tools_posrelat(agnL_optinteger(L, 2, 1), a->size);
  step = agnL_optinteger(L, 3, 1);
  bits = agnL_optboolean(L, 4, 0);
  if (bits == 0) {
    if (pos < 1 || (pos > a->size && a->size != 0))
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "numarray.cycle", pos);
  } else {
    if (pos < 1 || ((pos * NBITS) > (a->size * NBITS) && a->size != 0))
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "numarray.cycle", pos);
  }
  if (step < 1)
    luaL_error(L, "Error in " LUA_QS ": step size must be a positive.", "numarray.cycle");
  luaL_checkstack(L, 4, "not enough stack space");  /* 3.18.4 fix */
  lua_pushvalue(L, 1);
  lua_pushinteger(L, pos);  /* we cycle from index position 1, not 0 */
  lua_pushinteger(L, step);
  lua_pushinteger(L, pos);  /* original start value */
  lua_pushcclosure(L, (bits) ? &cyclebits : &cycle, 4);
  return 1;
}


static int mt_getsize (lua_State *L) {  /* returns the number of allocated slots in the userdata numeric array */
  NumArray *a = checknumarray(L, 1);
  lua_pushinteger(L, a->size);
  return 1;
}


static int mt_empty (lua_State *L) {  /* 2.15.4 */
  NumArray *a = checknumarray(L, 1);
  lua_pushboolean(L, a->size == 0);
  return 1;
}


static int mt_filled (lua_State *L) {  /* 2.15.4 */
  NumArray *a = checknumarray(L, 1);
  lua_pushboolean(L, a->size != 0);
  return 1;
}


static int mt_array2string (lua_State *L) {  /* at the console, the array is formatted as follows: */
  NumArray *a = checknumarray(L, 1);
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%u)", a->size);
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid array.", "numarray.__tostring");
  return 1;
}


static int mt_sumup (lua_State *L) {  /* 3.5.6 */
  size_t i;
  volatile lua_Number s, cs, ccs, t, c, cc, x;
  NumArray *a = checknumarray(L, 1);
  if (a->datatype != NADOUBLE)
    luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "numarray.__sumup mt");
  s = cs = ccs = 0.0;
  for (i=0; i < a->size; i++) {  /* Kahan-Babuška summation */
    x = a->data.n[i];
    t = s + x;
    c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
    s = t;
    t = cs + c;
    cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
    cs = t;
    ccs = ccs + cc;
  }
  if (a->size == 0)
    lua_pushfail(L);
  else
    lua_pushnumber(L, s + cs + ccs);  /* no rounding in between */
  return 1;
}


static int mt_qsumup (lua_State *L) {  /* 3.5.6 */
  size_t i;
  volatile lua_Number s, cs, ccs, t, c, cc, x, p, r;
  NumArray *a = checknumarray(L, 1);
  if (a->datatype != NADOUBLE)
    luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "numarray.__qsumup mt");
  s = cs = ccs = 0.0;
  for (i=0; i < a->size; i++) {  /* Kahan-Babuška summation */
    x = a->data.n[i];
    /* prepare x to be used in Kahan-Babuška algorithm */
    p = x*x;
    r = fma(x, x, -p);
    x = p + r;
    /* end of preparation */
    t = s + x;  /* 2.4.3, Kahan-Babuška summation */
    c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
    s = t;
    t = cs + c;
    cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
    cs = t;
    ccs = ccs + cc;
  }
  if (a->size == 0)
    lua_pushfail(L);
  else
    lua_pushnumber(L, s + cs + ccs);  /* no rounding in between */
  return 1;
}


static int mt_mulup (lua_State *L) {  /* 3.5.6 */
  size_t i;
  NumArray *a = checknumarray(L, 1);
  long double p = 1.0;
  if (a->datatype != NADOUBLE)
    luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "numarray.__mulup mt");
  for (i=0; i < a->size; i++) {  /* Kahan-Babuška summation */
    p = luai_nummul(p, a->data.n[i]);
  }
  if (a->size == 0)
    lua_pushfail(L);
  else
    lua_pushnumber(L, p);  /* no rounding in between */
  return 1;
}


static int mt_qmdev (lua_State *L) {  /* 3.5.6 */
  int i;
  volatile lua_Number mean, M2, delta, d, x, cs, ccs, meantemp, meancs, meanccs;
  NumArray *a = checknumarray(L, 1);
  mean = M2 = cs = ccs = meancs = meanccs = 0;
  if (a->datatype != NADOUBLE)
    luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "numarray.__qmdev mt");
  for (i=0; i < a->size; i++) {
    x = a->data.n[i];
    delta = x - mean;
    d = delta/(i + 1);
    mean = tools_kbadd(mean, d, &meancs, &meanccs);  /* 3.7.3 change to Kahan-Babuska summation */
    meantemp = mean + meancs + meanccs;  /* we must not modify 'mean' directly */
    x = delta*(x - meantemp);
    M2 = tools_kbadd(M2, x, &cs, &ccs);  /* 3.7.2/3 change to Kahan-Babuska summation */
  }
  if (a->size == 0)
    lua_pushfail(L);
  else
    lua_pushnumber(L, M2 + cs + ccs);
  return 1;
}


#define aux_allocappend(a,nop,tot,datatype,x) { \
  if (nop + 1 > tot) { \
    tot = agn_newsize(NULL, (nop + 1)); \
    (a) = realloc((a), tot*sizeof(datatype)); \
    if ((a) == NULL) \
      luaL_error(L, "Error: memory allocation failed."); \
  } \
  (a)[nop] = (x); \
  (nop)++; \
}

#define cmplx_allocappend(a,nop,tot,datatype,x) { \
  if (nop + 1 > tot) { \
    tot = agn_newsize(NULL, (nop + 1)); \
    (a) = realloc((a), tot*sizeof(datatype)); \
    if ((a) == NULL) \
      luaL_error(L, "Error: memory allocation failed."); \
  } \
  setcdarrayp(a, nop, x); \
  (nop)++; \
}

static size_t safenewsize (size_t newsize, int typesize) {  /* 4.9.4 fix: avoid segfaults with new size zero */
  return (newsize == 0) ? typesize : newsize*typesize;
}

static int mt_intersect (lua_State *L) {  /* 4.2.0 */
  size_t i, j;
  int flag, allocated;
  NumArray *a, *b, *c;
  a = checknumarray(L, 1);
  b = checknumarray(L, 2);
  if (a->datatype != b->datatype) {
    luaL_error(L, "Error in " LUA_QS ": numarrays are of different types.", "numarray.intersect mt");
  }
  allocated = (a->size > b->size ? agn_log2(L, a->size): agn_log2(L, b->size));
  c = createarray(L, a->datatype, allocated, a->ndims, a->dims, "numarray.intersect mt");
  c->size = 0;
  switch (a->datatype) {
    case NAUCHAR: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.c[i] == b->data.c[j]) {
            aux_allocappend(c->data.c, c->size, allocated, unsigned char, a->data.c[i]); flag = 0;
          }
        }
      }
      c->data.c = realloc(c->data.c, safenewsize(c->size, sizeof(unsigned char)));
      if (c->data.c == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.intersect mt");
      }
      break;
    }
    case NADOUBLE: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.n[i] == b->data.n[j]) {
            aux_allocappend(c->data.n, c->size, allocated, lua_Number, a->data.n[i]); flag = 0;
          }
        }
      }
      c->data.n = realloc(c->data.n, safenewsize(c->size, sizeof(lua_Number)));
      if (c->data.n == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.intersect mt");
      }
      break;
    }
    case NACDOUBLE: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (iseqcdarray(a->data.z, i, b->data.z, j)) {
            cmplx_allocappend(c->data.z, c->size, allocated, agn_Complex, a->data.z[i]);
            flag = 0;
          }
        }
      }
      c->data.z = realloc(c->data.z, safenewsize(c->size, sizeof(agn_Complex)));
      if (c->data.z == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.intersect mt");
      }
      break;
    }
    case NAINT32: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.i[i] == b->data.i[j]) {
            aux_allocappend(c->data.i, c->size, allocated, int32_t, a->data.i[i]); flag = 0;
          }
        }
      }
      c->data.i = realloc(c->data.i, safenewsize(c->size, sizeof(int32_t)));
      if (c->data.i == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.intersect mt");
      }
      break;
    }
    case NAUINT16: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.us[i] == b->data.us[j]) {
            aux_allocappend(c->data.us, c->size, allocated, uint16_t, a->data.us[i]); flag = 0;
          }
        }
      }
      c->data.us = realloc(c->data.us, safenewsize(c->size, sizeof(uint16_t)));
      if (c->data.us == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.intersect mt");
      }
      break;
    }
    case NAUINT32: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.ui[i] == b->data.ui[j]) {
            aux_allocappend(c->data.ui, c->size, allocated, uint32_t, a->data.ui[i]); flag = 0;
          }
        }
      }
      c->data.ui = realloc(c->data.ui, safenewsize(c->size, sizeof(uint32_t)));
      if (c->data.ui == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.intersect mt");
      }
      break;
    }
    case NAUINT64: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.ui64[i] == b->data.ui64[j]) {
            aux_allocappend(c->data.ui64, c->size, allocated, uint64_t, a->data.ui64[i]); flag = 0;
          }
        }
      }
      c->data.ui64 = realloc(c->data.ui64, safenewsize(c->size, sizeof(uint64_t)));
      if (c->data.ui64 == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.intersect mt");
      }
      break;
    }
    case NAINT64: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.i64[i] == b->data.i64[j]) {
            aux_allocappend(c->data.i64, c->size, allocated, int64_t, a->data.i64[i]); flag = 0;
          }
        }
      }
      c->data.i64 = realloc(c->data.i64, safenewsize(c->size, sizeof(int64_t)));
      if (c->data.i64 == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.intersect mt");
      }
      break;
    }
#ifndef __ARMCPU
    case NALDOUBLE: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.ld[i] == b->data.ld[j]) {
            aux_allocappend(c->data.ld, c->size, allocated, long double, a->data.ld[i]); flag = 0;
          }
        }
      }
      c->data.ld = realloc(c->data.ld, safenewsize(c->size, sizeof(long double)));
      if (c->data.ld == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.intersect mt");
      }
      break;
    }
#else
    case NALDOUBLE:
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.intersect mt");
      break;
#endif
    default:
      lua_assert(0);
  }
  return 1;
}


static int mt_minus (lua_State *L) {  /* 4.2.0 */
  size_t i, j;
  int flag, allocated;
  NumArray *a, *b, *c;
  a = checknumarray(L, 1);
  b = checknumarray(L, 2);
  if (a->datatype != b->datatype) {
    luaL_error(L, "Error in " LUA_QS ": numarrays are of different types.", "numarray.minus mt");
  }
  allocated = (a->size > b->size ? agn_log2(L, a->size): agn_log2(L, b->size));
  c = createarray(L, a->datatype, allocated, a->ndims, a->dims, "numarray.minus mt");
  c->size = 0;
  switch (a->datatype) {
    case NAUCHAR: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.c[i] == b->data.c[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.c, c->size, allocated, unsigned char, a->data.c[i]);
        }
      }
      c->data.c = realloc(c->data.c, safenewsize(c->size, sizeof(unsigned char)));
      if (c->data.c == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.minus mt");
      }
      break;
    }
    case NADOUBLE: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.n[i] == b->data.n[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.n, c->size, allocated, lua_Number, a->data.n[i]);
        }
      }
      c->data.n = realloc(c->data.n, safenewsize(c->size, sizeof(lua_Number)));
      if (c->data.n == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.minus mt");
      }
      break;
    }
    case NACDOUBLE: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (iseqcdarray(a->data.z, i, b->data.z, j)) flag = 0;
        }
        if (flag) {
          cmplx_allocappend(c->data.z, c->size, allocated, agn_Complex, a->data.z[i]);
        }
      }
      c->data.z = realloc(c->data.z, safenewsize(c->size, sizeof(agn_Complex)));
      if (c->data.z == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.minus mt");
      }
      break;
    }
    case NAINT32: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.i[i] == b->data.i[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.i, c->size, allocated, int32_t, a->data.i[i]);
        }
      }
      c->data.i = realloc(c->data.i, safenewsize(c->size, sizeof(int32_t)));
      if (c->data.i == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.minus mt");
      }
      break;
    }
    case NAUINT16: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.us[i] == b->data.us[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.us, c->size, allocated, uint16_t, a->data.us[i]);
        }
      }
      c->data.us = realloc(c->data.us, safenewsize(c->size, sizeof(uint16_t)));
      if (c->data.us == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.minus mt");
      }
      break;
    }
    case NAUINT32: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.ui[i] == b->data.ui[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.ui, c->size, allocated, uint32_t, a->data.ui[i]);
        }
      }
      c->data.ui = realloc(c->data.ui, safenewsize(c->size, sizeof(uint32_t)));
      if (c->data.ui == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.minus mt");
      }
      break;
    }
    case NAUINT64: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.ui64[i] == b->data.ui64[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.ui64, c->size, allocated, uint64_t, a->data.ui64[i]);
        }
      }
      c->data.ui64 = realloc(c->data.ui64, safenewsize(c->size, sizeof(uint64_t)));
      if (c->data.ui64 == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.minus mt");
      }
      break;
    }
    case NAINT64: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.i64[i] == b->data.i64[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.i64, c->size, allocated, int64_t, a->data.i64[i]);
        }
      }
      c->data.i64 = realloc(c->data.i64, safenewsize(c->size, sizeof(int64_t)));
      if (c->data.i64 == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.minus mt");
      }
      break;
    }
#ifndef __ARMCPU
    case NALDOUBLE: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < b->size; j++) {
          if (a->data.ld[i] == b->data.ld[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.ld, c->size, allocated, long double, a->data.ld[i]);
        }
      }
      c->data.ld = realloc(c->data.ld, safenewsize(c->size, sizeof(long double)));
      if (c->data.ld == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.minus mt");
      }
      break;
    }
#else
    case NALDOUBLE:
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.minus mt");
      break;
#endif
    default:
      lua_assert(0);
  }
  return 1;
}


static int mt_union (lua_State *L) {  /* 4.2.0 */
  size_t i, j;
  NumArray *a, *b, *c;
  a = checknumarray(L, 1);
  b = checknumarray(L, 2);
  if (a->datatype != b->datatype) {
    luaL_error(L, "Error in " LUA_QS ": numarrays are of different types.", "numarray.union mt");
  }
  c = createarray(L, a->datatype, a->size + b->size, a->ndims, a->dims, "numarray.union mt");
  switch (a->datatype) {
    case NAUCHAR: {
      for (i=0; i < a->size; i++) { c->data.c[i] = a->data.c[i]; }
      for (j=0; j < b->size; j++) { c->data.c[i++] = b->data.c[j]; }
      break;
    }
    case NADOUBLE: {
      for (i=0; i < a->size; i++) { c->data.n[i] = a->data.n[i]; }
      for (j=0; j < b->size; j++) { c->data.n[i++] = b->data.n[j]; }
      break;
    }
    case NACDOUBLE: {
      for (i=0; i < a->size; i++) { copycdarray(c->data.z, i, a->data.z, i); }
      for (j=0; j < b->size; j++) { copycdarray(c->data.z, i, b->data.z, j); i++; }
      break;
    }
    case NAINT32: {
      for (i=0; i < a->size; i++) { c->data.i[i]   = a->data.i[i]; }
      for (j=0; j < b->size; j++) { c->data.i[i++] = b->data.i[j]; }
      break;
    }
    case NAUINT16: {
      for (i=0; i < a->size; i++) { c->data.us[i]   = a->data.us[i]; }
      for (j=0; j < b->size; j++) { c->data.us[i++] = b->data.us[j]; }
      break;
    }
    case NAUINT32: {
      for (i=0; i < a->size; i++) { c->data.ui[i]   = a->data.ui[i]; }
      for (j=0; j < b->size; j++) { c->data.ui[i++] = b->data.ui[j]; }
      break;
    }
    case NAUINT64: {
      for (i=0; i < a->size; i++) { c->data.ui64[i]   = a->data.ui64[i]; }
      for (j=0; j < b->size; j++) { c->data.ui64[i++] = b->data.ui64[j]; }
      break;
    }
    case NAINT64: {
      for (i=0; i < a->size; i++) { c->data.i64[i]   = a->data.i64[i]; }
      for (j=0; j < b->size; j++) { c->data.i64[i++] = b->data.i64[j]; }
      break;
    }
#ifndef __ARMCPU
    case NALDOUBLE: {
      for (i=0; i < a->size; i++) { c->data.ld[i]   = a->data.ld[i]; }
      for (j=0; j < b->size; j++) { c->data.ld[i++] = b->data.ld[j]; }
      break;
    }
#else
    case NALDOUBLE:
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.union mt");
      break;
#endif
    default:
      lua_assert(0);
  }
  return 1;
}


static int numarray_unique (lua_State *L) {  /* 4.2.0, based on mt_minus */
  size_t i, j;
  int flag, allocated;
  NumArray *a, *c;
  a = checknumarray(L, 1);
  allocated = agn_log2(L, a->size);
  c = createarray(L, a->datatype, allocated, a->ndims, a->dims, "numarray.unique");
  c->size = 0;
  switch (a->datatype) {
    case NAUCHAR: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < c->size; j++) {
          if (a->data.c[i] == c->data.c[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.c, c->size, allocated, unsigned char, a->data.c[i]);
        }
      }
      c->data.c = realloc(c->data.c, safenewsize(c->size, sizeof(unsigned char)));
      if (c->data.c == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.unique");
      }
      break;
    }
    case NADOUBLE: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < c->size; j++) {
          if (a->data.n[i] == c->data.n[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.n, c->size, allocated, lua_Number, a->data.n[i]);
        }
      }
      c->data.n = realloc(c->data.n, safenewsize(c->size, sizeof(lua_Number)));
      if (c->data.n == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.unique");
      }
      break;
    }
    case NACDOUBLE: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < c->size; j++) {
          if (iseqcdarray(a->data.z, i, c->data.z, j)) flag = 0;
        }
        if (flag) {
          cmplx_allocappend(c->data.z, c->size, allocated, agn_Complex, a->data.z[i]);
        }
      }
      c->data.z = realloc(c->data.z, safenewsize(c->size, sizeof(agn_Complex)));
      if (c->data.z == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.unique");
      }
      break;
    }
    case NAINT32: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < c->size; j++) {
          if (a->data.i[i] == c->data.i[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.i, c->size, allocated, int32_t, a->data.i[i]);
        }
      }
      c->data.i = realloc(c->data.i, safenewsize(c->size, sizeof(int32_t)));
      if (c->data.i == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.unique");
      }
      break;
    }
    case NAUINT16: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < c->size; j++) {
          if (a->data.us[i] == c->data.us[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.us, c->size, allocated, uint16_t, a->data.us[i]);
        }
      }
      c->data.us = realloc(c->data.us, safenewsize(c->size, sizeof(uint16_t)));
      if (c->data.us == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.unique");
      }
      break;
    }
    case NAUINT32: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < c->size; j++) {
          if (a->data.ui[i] == c->data.ui[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.ui, c->size, allocated, uint32_t, a->data.ui[i]);
        }
      }
      c->data.ui = realloc(c->data.ui, safenewsize(c->size, sizeof(uint32_t)));
      if (c->data.ui == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.unique");
      }
      break;
    }
    case NAUINT64: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < c->size; j++) {
          if (a->data.ui64[i] == c->data.ui64[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.ui64, c->size, allocated, uint64_t, a->data.ui64[i]);
        }
      }
      c->data.ui64 = realloc(c->data.ui64, safenewsize(c->size, sizeof(uint64_t)));
      if (c->data.ui64 == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.unique");
      }
      break;
    }
    case NAINT64: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < c->size; j++) {
          if (a->data.i64[i] == c->data.i64[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.i64, c->size, allocated, int64_t, a->data.i64[i]);
        }
      }
      c->data.i64 = realloc(c->data.i64, safenewsize(c->size, sizeof(int64_t)));
      if (c->data.i64 == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.unique");
      }
      break;
    }
#ifndef __ARMCPU
    case NALDOUBLE: {
      for (i=0; i < a->size; i++) {
        flag = 1;
        for (j=0; flag && j < c->size; j++) {
          if (a->data.ld[i] == c->data.ld[j]) flag = 0;
        }
        if (flag) {
          aux_allocappend(c->data.ld, c->size, allocated, long double, a->data.ld[i]);
        }
      }
      c->data.ld = realloc(c->data.ld, safenewsize(c->size, sizeof(long double)));
      if (c->data.ld == NULL) {  /* 7.7.7 K&R fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "numarray.unique");
      }
      break;
    }
#else
    case NALDOUBLE:
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.unique");
      break;
#endif
    default: lua_assert(0);

  }
  return 1;
}


static int numarray_member (lua_State *L) {  /* 4.2.0 */
  size_t i;
  int approximate;
  NumArray *a;
  lua_Number y, eps;
  a = checknumarray(L, 2);
  eps = agnL_optpositive(L, 3, 0.0);
  approximate = eps != 0.0;
  switch (a->datatype) {
    case NAUCHAR: {
      lua_Number x = agn_checknumber(L, 1);
      for (i=0; i < a->size; i++) {
        y = (lua_Number)a->data.c[i];
        if (y == x || (approximate && tools_approx(y, x, eps))) {
          lua_pushinteger(L, i + 1);
          return 1;
        }
      }
      break;
    }
    case NADOUBLE: {
      lua_Number x = agn_checknumber(L, 1);
      for (i=0; i < a->size; i++) {
        y = (lua_Number)a->data.n[i];
        if (y == x || (approximate && tools_approx(y, x, eps))) {
          lua_pushinteger(L, i + 1);
          return 1;
        }
      }
      break;
    }
    case NACDOUBLE: {
      lua_Number a1, b1;
      agn_pComplex z;
      if (!agn_getcmplxparts(L, 1, &a1, &b1)) {
        luaL_error(L, "Error in " LUA_QS ": (complex) number expected, got %s.", "numarray.member", luaL_typename(L, 1));
      }
      for (i=0; i < a->size; i++) {
        z = a->data.z[i];
        if (tools_capprox(a1, real(z), b1, imag(z), eps)) {
          lua_pushinteger(L, i + 1);
          return 1;
        }
      }
      break;
    }
    case NAINT32: {
      lua_Number x = agn_checknumber(L, 1);
      for (i=0; i < a->size; i++) {
        y = (lua_Number)a->data.i[i];
        if (y == x || (approximate && tools_approx(y, x, eps))) {
          lua_pushinteger(L, i + 1);
          return 1;
        }
      }
      break;
    }
    case NAUINT16: {
      lua_Number x = agn_checknumber(L, 1);
      for (i=0; i < a->size; i++) {
        y = (lua_Number)a->data.us[i];
        if (y == x || (approximate && tools_approx(y, x, eps))) {
          lua_pushinteger(L, i + 1);
          return 1;
        }
      }
      break;
    }
    case NAUINT32: {
      lua_Number x = agn_checknumber(L, 1);
      for (i=0; i < a->size; i++) {
        y = (lua_Number)a->data.ui[i];
        if (y == x || (approximate && tools_approx(y, x, eps))) {
          lua_pushinteger(L, i + 1);
          return 1;
        }
      }
      break;
    }
    case NAUINT64: {
      uint64_t x = agnL_checkuint64(L, 1, "numarray.member");
      for (i=0; i < a->size; i++) {
        if (a->data.ui64[i] == x) {
          lua_pushinteger(L, i + 1);
          return 1;
        }
      }
      break;
    }
    case NAINT64: {
      int64_t x = agnL_checkint64(L, 1, "numarray.member");
      for (i=0; i < a->size; i++) {
        if (a->data.i64[i] == x) {
          lua_pushinteger(L, i + 1);
          return 1;
        }
      }
      break;
    }
#ifndef __ARMCPU
    case NALDOUBLE: {
      longdouble x, y;
      x = checkandgetdlongnum(L, 1);
      for (i=0; i < a->size; i++) {
        y = a->data.ld[i];
        if (y == x || (approximate && tools_approxl(y, x, eps))) {
          lua_pushinteger(L, i + 1);
          return 1;
        }
      }
      break;
    }
#else
    case NALDOUBLE:
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.union mt");
      break;
#endif
    default:
      lua_assert(0);
  }
  lua_pushnil(L);
  return 1;
}


/* numarray.find (obj, x, [, eps])
   Searches x in the numeric array obj and if successful returns `true` and `false` otherwise. The search is
   linear. If you pass any positive value for eps, then the function will conduct an approximate check with the
   given epsilon value, for example `Eps`, see `approx`.
   See also: numarray.binsearch, numarray.member, numarray.whereis. 6.5.14 */
static int numarray_find (lua_State *L) {
  int isnil, nargs = lua_gettop(L);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushvalue(L, 1);  /* swap first and second argument */
  lua_pushvalue(L, 2);
  lua_replace(L, 1);
  lua_replace(L, 2);
  lua_settop(L, nargs);  /* make sure the eps option, if given, is preserved */
  numarray_member(L);
  isnil = lua_isnil(L, -1);  /* substitute index position or `null`*/
  agn_poptop(L);
  lua_pushboolean(L, !isnil);
  return 1;
}


/* Returns a sequence with the minimum and maximum of all values in numarray obj, in this order.

   If the option 'sorted' is passed than the function assumes that all values in obj are sorted
   in ascending order so that execution is much faster.

   With complex number arrays, the function computes the modulus to determine the result.

   See also: stats.minmax. 7.1.9 */
static int numarray_minmax (lua_State *L) {
  size_t i, n;
  NumArray *a = checknumarray(L, 1);
  int issorted = agnL_optboolean(L, 2, 0);
  n = a->size;
  agn_createseq(L, 2);
  switch (a->datatype) {
    case NAUCHAR: {
      unsigned char min, max, number;
      if (issorted) {
        agn_seqsetinumber(L, -1, 1, a->data.c[0]);
        agn_seqsetinumber(L, -1, 2, a->data.c[n - 1]);
        return 1;
      }
      min = a->data.c[0];
      max = min;
      for (i=1; i < n; i++) {
        number = a->data.c[i];
        if (number < min)
          min = number;
        if (number > max)
          max = number;
      }
      agn_seqsetinumber(L, -1, 1, min);
      agn_seqsetinumber(L, -1, 2, max);
      break;
    }
    case NADOUBLE: {
      lua_Number min, max, number;
      if (issorted) {
        agn_seqsetinumber(L, -1, 1, a->data.n[0]);
        agn_seqsetinumber(L, -1, 2, a->data.n[n - 1]);
        return 1;
      }
      min = a->data.n[0];
      max = min;
      for (i=1; i < n; i++) {
        number = a->data.n[i];
        if (number < min)
          min = number;
        if (number > max)
          max = number;
      }
      agn_seqsetinumber(L, -1, 1, min);
      agn_seqsetinumber(L, -1, 2, max);
      break;
    }
    case NACDOUBLE: {
      size_t minidx, maxidx;
      agn_pComplex z;
      lua_Number hmin, hmax, hnumber;
      if (issorted) {
        agn_pushcomplex(L, real(a->data.z[0]), imag(a->data.z[0]));
        lua_seqrawseti(L, -2, 1);
        agn_pushcomplex(L, real(a->data.z[n - 1]), imag(a->data.z[n - 1]));
        lua_seqrawseti(L, -2, 2);
        return 1;
      }
      minidx = maxidx = 0;
      z = a->data.z[0];
      hmin = sun_hypot(real(z), imag(z));
      hmax = hmin;
      for (i=1; i < n; i++) {
        z = a->data.z[i];
        hnumber = sun_hypot(real(z), imag(z));
        if (hnumber < hmin) {
          hmin = hnumber;
          minidx = i;
        }
        if (hnumber > hmax) {
          hmax = hnumber;
          maxidx = i;
        }
      }
      agn_pushcomplex(L, real(a->data.z[minidx]), imag(a->data.z[minidx]));
      lua_seqrawseti(L, -2, 1);
      agn_pushcomplex(L, real(a->data.z[maxidx]), imag(a->data.z[maxidx]));
      lua_seqrawseti(L, -2, 2);
      break;
    }
    case NAUINT16: {
      uint16_t min, max, number;
      if (issorted) {
        agn_seqsetinumber(L, -1, 1, a->data.us[0]);
        agn_seqsetinumber(L, -1, 2, a->data.us[n - 1]);
        return 1;
      }
      min = a->data.us[0];
      max = min;
      for (i=1; i < n; i++) {
        number = a->data.us[i];
        if (number < min)
          min = number;
        if (number > max)
          max = number;
      }
      agn_seqsetinumber(L, -1, 1, min);
      agn_seqsetinumber(L, -1, 2, max);
      break;
    }
    case NAINT32: {
      int32_t min, max, number;
      if (issorted) {
        agn_seqsetinumber(L, -1, 1, a->data.i[0]);
        agn_seqsetinumber(L, -1, 2, a->data.i[n - 1]);
        return 1;
      }
      min = a->data.i[0];
      max = min;
      for (i=1; i < n; i++) {
        number = a->data.i[i];
        if (number < min)
          min = number;
        if (number > max)
          max = number;
      }
      agn_seqsetinumber(L, -1, 1, min);
      agn_seqsetinumber(L, -1, 2, max);
      break;
    }
    case NAUINT32: {
      uint32_t min, max, number;
      if (issorted) {
        agn_seqsetinumber(L, -1, 1, a->data.ui[0]);
        agn_seqsetinumber(L, -1, 2, a->data.ui[n - 1]);
        return 1;
      }
      min = a->data.ui[0];
      max = min;
      for (i=1; i < n; i++) {
        number = a->data.ui[i];
        if (number < min)
          min = number;
        if (number > max)
          max = number;
      }
      agn_seqsetinumber(L, -1, 1, min);
      agn_seqsetinumber(L, -1, 2, max);
      break;
    }
    case NAUINT64: {
      uint64_t min, max, number;
      if (issorted) {
        agn_seqsetinumber(L, -1, 1, a->data.ui64[0]);
        agn_seqsetinumber(L, -1, 2, a->data.ui64[n - 1]);
        return 1;
      }
      min = a->data.ui64[0];
      max = min;
      for (i=1; i < n; i++) {
        number = a->data.ui64[i];
        if (number < min)
          min = number;
        if (number > max)
          max = number;
      }
      agn_seqsetinumber(L, -1, 1, min);
      agn_seqsetinumber(L, -1, 2, max);
      break;
    }
    case NAINT64: {
      int64_t min, max, number;
      if (issorted) {
        agn_seqsetinumber(L, -1, 1, a->data.i64[0]);
        agn_seqsetinumber(L, -1, 2, a->data.i64[n - 1]);
        return 1;
      }
      min = a->data.i64[0];
      max = min;
      for (i=1; i < n; i++) {
        number = a->data.i64[i];
        if (number < min)
          min = number;
        if (number > max)
          max = number;
      }
      agn_seqsetinumber(L, -1, 1, min);
      agn_seqsetinumber(L, -1, 2, max);
      break;
    }
#ifndef __ARMCPU
    case NALDOUBLE: {
      longdouble min, max, number;
      if (issorted) {
        agn_seqsetinumber(L, -1, 1, a->data.ld[0]);
        agn_seqsetinumber(L, -1, 2, a->data.ld[n - 1]);
        return 1;
      }
      min = a->data.ld[0];
      max = min;
      for (i=1; i < n; i++) {
        number = a->data.ld[i];
        if (number < min)
          min = number;
        if (number > max)
          max = number;
      }
      agn_seqsetinumber(L, -1, 1, min);
      agn_seqsetinumber(L, -1, 2, max);
      break;
    }
#endif
    default:
      lua_assert(0);
  }
  return 1;
}


static int mt_call (lua_State *L) {  /* 5.1.6 */
  NumArray *a = checknumarray(L, 1);
  return agnL_calludata(L, lua_gettop(L), a->registry, "numarray");  /* 5.4.0 change */
}


static int mt_arraygc (lua_State *L) {  /* please do not forget to garbage collect deleted userdata */
  NumArray *a = checknumarray(L, 1);
  switch (a->datatype) {
    case NAUCHAR:   xfree(a->data.c);  break;
    case NADOUBLE:  xfree(a->data.n);  break;
    case NACDOUBLE: xfree(a->data.z);  break;
    case NAINT32:   xfree(a->data.i);  break;
    case NAUINT16:  xfree(a->data.us); break;  /* 2.18.2 */
    case NAUINT32:  xfree(a->data.ui); break;  /* 2.22.1 */
    case NAUINT64:  xfree(a->data.ui64); break;  /* 6.2.3 */
    case NAINT64:   xfree(a->data.i64); break;  /* 6.2.3 */
#ifndef __ARMCPU  /* 2.37.1 */
    case NALDOUBLE: xfree(a->data.ld); break;  /* 2.35.0 */
#endif
    default:
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray garbage collection");
  }
  lua_setmetatabletoobject(L, 1, NULL, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, a->registry);  /* 2.15.1, delete registry table */
  return 0;
}


/* Returns the index for a given value what in the numarray a. By default, the search starts at the beginning of the array,
   but you may pass any valid position pos (a positive integer) to determine where to start the search. The return is
   the index position, a positive number, or `null` if `what` could not be found in a.

   By default, the function checks for exact equality to detect the existence of a value. By passing the fourth argument
   eps, a non-negative number, the function also compares the values approximately with the given maximum deviation eps.
   See approx for more details. */

#define aux_whereis(L,datastruct,what,cond) { \
  for (i=pos; i < a->size; i++) { \
    if (datastruct[i] == (what) || (cond)) { \
      lua_pushinteger(L, i + 1); \
      return 1; \
    } \
  } \
}

#ifdef PROPCMPLX
#define prop_whereis(L,datastruct,what,cond) { \
  for (i=pos; i < a->size; i++) { \
    if ( (datastruct[i][0] == (what)[0] || (cond)) && (datastruct[i][1] == (what)[1] || (cond)) ) { \
      lua_pushinteger(L, i + 1); \
      return 1; \
    } \
  } \
}
#endif

static int numarray_whereis (lua_State *L) {
  int64_t i, pos;
  lua_Number eps;
  union ldshape d;
  NumArray *a = checknumarray(L, 1);  /* 2.15.5 fix */
  getanynumber(L, d, 2, "numarray.whereis");
  pos = tools_posrelat(agnL_optinteger(L, 3, 1), a->size) - 1;  /* extended 2.15.1 */
  eps = luaL_optnumber(L, 4, 0.0);
  if (eps < 0)
    luaL_error(L, "Error in " LUA_QS ": fourth argument must be non-negative.", "numarray.whereis");
  if (pos < 0 || pos >= a->size)
    luaL_error(L, "Error in " LUA_QS ": index %d out-of-range.", "numarray.whereis", pos + 1);
  switch (a->datatype) {
    case NAUCHAR:  aux_whereis(L, a->data.c,  d.d, 0); break;
    case NADOUBLE: aux_whereis(L, a->data.n,  d.d, (eps == 0.0 && tools_approx(a->data.n[i], d.d, eps))); break;
    case NACDOUBLE: {  /* 4.9.5, FIXME: allow for Knuth approximation */
#ifndef PROPCMPLX
      aux_whereis(L, a->data.z, d.z, (eps == 0.0 && tools_capprox(creal(a->data.z[i]), creal(d.z), cimag(a->data.z[i]), cimag(d.z), eps)));
#else
      prop_whereis(L, a->data.z, d.z, (eps == 0.0 && tools_capprox(a->data.z[i][0], d.z[0], a->data.z[i][1], d.z[1], eps)));
#endif
      break;
    }
    case NAINT32:  aux_whereis(L, a->data.i,  d.d, 0);      break;
    case NAUINT16: aux_whereis(L, a->data.us, d.d, 0);      break;
    case NAUINT32: aux_whereis(L, a->data.ui, d.d, 0);      break;
    case NAUINT64: aux_whereis(L, a->data.ui64, d.ui64, 0); break;
    case NAINT64:  aux_whereis(L, a->data.i64, d.i64, 0);   break;
    case NALDOUBLE: {
#ifndef __ARMCPU  /* 2.37.1 */
      long double v = tolong(L, 2, d);
      aux_whereis(L, a->data.ld, v, (eps == 0.0 && tools_approxl(a->data.ld[i], v, eps)));
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.whereis");
#endif
      break;
    }
    default: luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.whereis");
  }
  lua_pushnil(L);
  return 1;
}

#define aux_countitems(L,datastruct,what,cond,nops,c) { \
  for (i=0; i < nops; i++) { \
    if (datastruct[i] == (what) || (cond)) c++; \
  } \
}

#define cmplx_countitems(L,datastruct,what,cond,nops,c) { \
  for (i=0; i < nops; i++) { \
    if ((iseqcdarrayp(datastruct, i, what)) || (cond)) c++; \
  } \
}

#define aux_countitemsfn(datastruct) { \
  for (i=0; i < nops; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space"); \
    lua_pushvalue(L, 1); \
    lua_pushnumber(L, datastruct[i]); \
    for (j=3; j <= nargs; j++) \
      lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    if (lua_istrue(L, -1)) c++; \
    agn_poptop(L); \
  } \
}

#define aux_countitemsfncmplx(datastruct) { \
  for (i=0; i < nops; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space"); \
    lua_pushvalue(L, 1); \
    aux_pushcomplex(L, datastruct[i]); \
    for (j=3; j <= nargs; j++) \
      lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    if (lua_istrue(L, -1)) c++; \
    agn_poptop(L); \
  } \
}

#define aux_countitemsuint64(datastruct) { \
  for (i=0; i < nops; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space"); \
    lua_pushvalue(L, 1); \
    createuint64(L, datastruct[i]); \
    for (j=3; j <= nargs; j++) \
      lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    if (lua_istrue(L, -1)) c++; \
    agn_poptop(L); \
  } \
}

#define aux_countitemsint64(datastruct) { \
  for (i=0; i < nops; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space"); \
    lua_pushvalue(L, 1); \
    createint64(L, datastruct[i]); \
    for (j=3; j <= nargs; j++) \
      lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    if (lua_istrue(L, -1)) c++; \
    agn_poptop(L); \
  } \
}

#define aux_countitemsdlong(datastruct) { \
  for (i=0; i < nops; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space"); \
    lua_pushvalue(L, 1); \
    createdlong(L, datastruct[i]); \
    for (j=3; j <= nargs; j++) \
      lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    if (lua_istrue(L, -1)) c++; \
    agn_poptop(L); \
  } \
}

static int numarray_countitems (lua_State *L) {  /* 4.2.0 */
  int64_t i, isapprox, c;
  int nops, nargs;
  lua_Number eps;
  union ldshape d;
  NumArray *a;
  nargs = lua_gettop(L);
  luaL_checkany(L, 1);
  a = checknumarray(L, 2);
  c = 0;
  nops = a->size;
  if (lua_type(L, 1) == LUA_TFUNCTION) {
    int j;
    switch (a->datatype) {
      case NAUCHAR:   aux_countitemsfn(a->data.c);    break;
      case NADOUBLE:  aux_countitemsfn(a->data.n);    break;
      case NACDOUBLE: aux_countitemsfncmplx(a->data.z); break;  /* 4.9.5 change */
      case NAINT32:   aux_countitemsfn(a->data.i);    break;
      case NAUINT16:  aux_countitemsfn(a->data.us);   break;
      case NAUINT32:  aux_countitemsfn(a->data.ui);   break;
      case NAUINT64:  aux_countitemsuint64(a->data.ui64); break;
      case NAINT64:   aux_countitemsint64(a->data.i64);  break;
      case NALDOUBLE: {
#ifndef __ARMCPU
        aux_countitemsdlong(a->data.ld);
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.countitems");
#endif
        break;
      }
      default: luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.countitems");
    }
    lua_pushinteger(L, c);
    return 1;
  }
  getanynumber(L, d, 1, "numarray.countitems");
  eps = luaL_optnumber(L, 3, 0);
  if (eps < 0)
    luaL_error(L, "Error in " LUA_QS ": third argument must be non-negative.", "numarray.countitems");
  isapprox = (nargs == 3);
  switch (a->datatype) {
    case NAUCHAR:  aux_countitems(L, a->data.c,  d.d, 0, nops, c); break;
    case NADOUBLE: aux_countitems(L, a->data.n,  d.d, (isapprox && tools_approx(a->data.n[i], d.d, eps)), nops, c); break;
    case NACDOUBLE:
      cmplx_countitems(L, a->data.z,  d.z, 0, nops, c);
      break;  /* 4.9.5 change */
    case NAINT32:  aux_countitems(L, a->data.i,  d.d, 0, nops, c); break;
    case NAUINT16: aux_countitems(L, a->data.us, d.d, 0, nops, c); break;
    case NAUINT32: aux_countitems(L, a->data.ui, d.d, 0, nops, c); break;
    case NAUINT64: aux_countitems(L, a->data.ui64, d.ui64, 0, nops, c); break;
    case NAINT64:  aux_countitems(L, a->data.i64, d.i64, 0, nops, c); break;
    case NALDOUBLE: {
#ifndef __ARMCPU
      long double v = tolong(L, 1, d);
      aux_countitems(L, a->data.ld, v, (isapprox && tools_approxl(a->data.ld[i], v, eps)), nops, c);
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.countitems");
#endif
      break;
    }
    default: luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.countitems");
  }
  lua_pushinteger(L, c);
  return 1;
}


#define aux_arrayeq(L, fn, ds1, ds2, eps) { \
  for (i=0; i < a->size; i++) { \
    if (!fn(ds1[i], ds2[i], (eps))) { \
      flag = 0; \
      goto endofarrayeq; \
    } \
  } \
}

#ifndef PROPCMPLX
#define aux_arrayeqcomplex(L, fn, ds1, ds2, eps) { \
  for (i=0; i < a->size; i++) { \
    int r1 = fn(creal(ds1[i]), creal(ds2[i]), (eps)); \
    int r2 = fn(cimag(ds1[i]), cimag(ds2[i]), (eps)); \
    if (!(r1 || r2)) { \
      flag = 0; \
      goto endofarrayeq; \
    } \
  } \
}
#else
#define aux_arrayeqcomplex(L, fn, ds1, ds2, eps) { \
  for (i=0; i < a->size; i++) { \
    int r1 = fn(ds1[i][0], ds2[i][0], (eps)); \
    int r2 = fn(ds1[i][1], ds2[i][1], (eps)); \
    if (!(r1 || r2)) { \
      flag = 0; \
      goto endofarrayeq; \
    } \
  } \
}
#endif

/* goto statement: exit in case of inequality */
static void arrayeq (lua_State *L, int (*fn)(lua_Number, lua_Number, lua_Number)) {  /* based on the linalg package (function meq) */
  size_t i;
  int flag;
  lua_Number eps;
  NumArray *a, *b;
  a = checknumarray(L, 1);
  b = checknumarray(L, 2);
  flag = 1;
  if (a->size != b->size || a->datatype != b->datatype)
    lua_pushfalse(L);
  else {  /* arrays have same dimension */
    eps = agn_getepsilon(L);
    switch (a->datatype) {
      case NAUCHAR:  aux_arrayeq(L, fn, a->data.c, b->data.c, eps);   break;
      case NADOUBLE: aux_arrayeq(L, fn, a->data.n, b->data.n, eps);   break;
      case NACDOUBLE: aux_arrayeqcomplex(L, fn, a->data.z, b->data.z, eps); break;
      case NAINT32:  aux_arrayeq(L, fn, a->data.i, b->data.i, eps);   break;
      case NAUINT16: aux_arrayeq(L, fn, a->data.us, b->data.us, eps); break;  /* 2.18.2 */
      case NAUINT32: aux_arrayeq(L, fn, a->data.ui, b->data.ui, eps); break;  /* 2.22.1 */
      case NAUINT64: aux_arrayeq(L, fn, a->data.ui64, b->data.ui64, eps); break;  /* 6.2.3 */
      case NAINT64:  aux_arrayeq(L, fn, a->data.i64, b->data.i64, eps); break;  /* 6.2.3 */
      case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
        aux_arrayeq(L, fn, a->data.ld, b->data.ui, eps);
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "equality metamethod");
#endif
        break;  /* 2.35.0 */
      default: luaL_error(L, "Error in " LUA_QS ": this should not happen.", "equality operation");
    }
  }
endofarrayeq:
  lua_pushboolean(L, flag);
}

static int equal (lua_Number x, lua_Number y, lua_Number eps) {  /* strict equality, eps does not matter */
  return x == y;
}

static int mt_aeq (lua_State *L) {
  arrayeq(L, tools_approx);
  return 1;
}


static int mt_eeq (lua_State *L) {
  arrayeq(L, equal);
  return 1;
}


/* numarray.read (fh [, bufsize])

Reads data from the file denoted by its filehandle fh and returns a numarray userdata structure
of unsigned C chars.

The file should before be opened with `binio.open` and should finally be closed with `binio.close`.

In general, the function reads in a limited amount of bytes per call. If only fh is passed, the
number of bytes read is determined by the environ.kernel('buffersize') setting, usually 512 bytes.

You can pass the second argument bufsize, a positive integer, to read less or more bytes. Passing
the bufsize argument may also be necessary if your platform requires that an internal input buffer
is aligned to a certain block size.

The function increments the file position thereafter so that the next bytes in the file can be read
with a new call to numarray.read.

If the end of the file has been reached, or there is nothing to read at all, `null` is returned.
In case of an error, it quits with the respective error.

See also: numarray.readdoubles, numarray.readintegers, numarray.readuchars. */

/* based on binio.readbytes. */

/* 2.15.5 Hotfixes, get binio userdata as file handles, not integers */

static int numarray_read (lua_State *L) {
  int64_t s, i;
  int hnd, n, en;
  NumArray *a;
  unsigned char *buffer;
  hnd = agn_tofileno(L, 1, 1);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "numarray.read");  /* 2.37.5 */
  s = luaL_optinteger(L, 2, agn_getbuffersize(L));  /* reading a lot of bytes at one stroke will cause block size errors, so keep the buffer small */
  if (s == -1)
    luaL_error(L, "Error in " LUA_QS " with file #%d: file I/O error.", "numarray.read", hnd);
  else if (s <= 0)
    s = agn_getbuffersize(L);  /* 2.34.9 adaption */
  buffer = malloc(s * sizeof(unsigned char));
  if (buffer == NULL)
    luaL_error(L, "Error in " LUA_QS " with file #%d: memory allocation failed.", "numarray.read", hnd);
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  n = read(hnd, buffer, s * sizeof(unsigned char));
  en = errno;
  if (n > 0) {
    int64_t dims[NAMAXDIMS];
    tools_bzero(dims, NAMAXDIMS*sizeof(int64_t));
    dims[0] = n;
    a = createarray(L, 0, n, 1, dims, "numarray.read");  /* 4.8.1 simplification */
    for (i=0; i < n; i++)
      a->data.c[i] = buffer[i];
  }
  else if (n == 0)
    lua_pushnil(L);
  else {
    xfree(buffer);
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "numarray.read", hnd, my_ioerror(en));
  }
  xfree(buffer);
  return 1;
}


/* numarray.write (fh, a [, pos [, nvalues]])

based on `binio.writebytes`; writes unsigned chars, doubles, or integers stored in a numarray userdata a to the file denoted by its
numeric file handle fh. The file should be opened with binio.open and closed with binio.close.

The start position `pos` is 1 by default but can be changed to any other valid position in the numarray.

The number of values (not bytes 1) `nvalues` to be written can be changed by passing an optional fourth argument, a positive
number, and by default equals the total number of entries in `a`. Depending on the data type stored to the numarray, the
function automatically computes the number of bytes to be written. Passing the `nvalues` argument may also be necessary if your
platform requires that buffers must be aligned to a particular block size.

The function returns the index of the next start position (an integer) for a further call to write a entirely, or `null`,
if the end of the numarray has been reached. When the function returns `null`, then it also automatically flushes all unwritten
content to the file so that you do not have to call `binio.sync` manually if you want to read the file subsequently.

No further information is stored to the file created, so you always must know the type of data you want to read in later.

Example on how to write an entire array of 4,096 integers piece-by-piece:

> a := numarray.integer(4 * 1024);
> fd := binio.open('uchar.bin');
> pos := 1;
> do
>    pos := numarray.write(fd, a, pos, 1024)  # write 1024 values per each call
> until pos = null;
> binio.close(fd);

Use `binio.sync` if you want to make sure that any unwritten content is written to the file when calling `numarray.write`
multiple times on one numarray. */

static size_t aux_fillbuffer (lua_State *L, unsigned char *buffer, NumArray *a, int64_t start, int64_t nvals, size_t buffersize) {
  int64_t i;
  int j;
  size_t c;
  unsigned char *dst;
  tools_bzero(buffer, buffersize);
  c = 0;
  switch (a->datatype) {
    case NAUCHAR:
      for (i=start; i < start + nvals; i++) {
        buffer[i - start] = a->data.c[i]; c++;
      }
      break;
    case NADOUBLE: {
      uint64_t u;
      for (i=start; i < start + nvals; i++) {
#if BYTE_ORDER == BIG_ENDIAN
        u = tools_doubletouint64andswap(a->data.n[i]);  /* 2.16.6 fix, convert to Little Endian `integer` */
#else
        u = tools_doubletouint64(a->data.n[i]);  /* convert to Little Endian `integer` */
#endif
        dst = (unsigned char *)&u;
        for (j=0; j < sizeof(lua_Number); j++)
          buffer[c++] = dst[j];
      }
      break;
    }
    case NACDOUBLE: {
      uint64_t u, v;
      agn_pComplex z;
      lua_Number zr, zi;
      unsigned char *dstu, *dstv;
      for (i=start; i < start + nvals; i++) {
        z = a->data.z[i];
        zr = real(z); zi = imag(z);
#if BYTE_ORDER == BIG_ENDIAN
        u = tools_doubletouint64andswap(zr);  /* 2.16.6 fix, convert to Little Endian `integer` */
        v = tools_doubletouint64andswap(zi);
#else
        u = tools_doubletouint64(zr);  /* convert to Little Endian `integer` */
        v = tools_doubletouint64(zi);
#endif
        dstu = (unsigned char *)&u;
        dstv = (unsigned char *)&v;
        for (j=0; j < sizeof(lua_Number); j++)
          buffer[c++] = dstu[j];
        for (j=0; j < sizeof(lua_Number); j++)
          buffer[c++] = dstv[j];
      }
      break;
    }
    case NAINT32: {
      int32_t u;
      for (i=start; i < start + nvals; i++) {
        u = a->data.i[i];
#if BYTE_ORDER == BIG_ENDIAN  /* 2.16.1 fix */
        tools_swapint32_t(&u);  /* convert to Little Endian 32-bit `integer` */
#endif
        dst = (unsigned char *)&u;
        for (j=0; j < sizeof(int32_t); j++)
          buffer[c++] = dst[j];
      }
      break;
    }
    case NAUINT16: {  /* 2.18.2 */
      uint16_t u;
      for (i=start; i < start + nvals; i++) {
        u = a->data.us[i];
#if BYTE_ORDER == BIG_ENDIAN  /* 2.16.1 fix */
        tools_swapuint16_t(&u);  /* convert to Little Endian 32-bit `integer` */
#endif
        dst = (unsigned char *)&u;
        for (j=0; j < sizeof(uint16_t); j++)
          buffer[c++] = dst[j];
      }
      break;
    }
    case NAUINT32: {  /* 2.22.1 */
      uint32_t u;
      for (i=start; i < start + nvals; i++) {
        u = a->data.ui[i];
#if BYTE_ORDER == BIG_ENDIAN  /* 2.16.1 fix */
        tools_swapuint32_t(&u);  /* convert to Little Endian 32-bit `integer` */
#endif
        dst = (unsigned char *)&u;
        for (j=0; j < sizeof(uint32_t); j++)
          buffer[c++] = dst[j];
      }
      break;
    }
    case NAUINT64: {  /* 6.2.3 */
      uint64_t u;
      for (i=start; i < start + nvals; i++) {
        u = a->data.ui64[i];
#if BYTE_ORDER == BIG_ENDIAN
        tools_swapuint64_t(&u);  /* convert to Little Endian 64-bit `integer` */
#endif
        dst = (unsigned char *)&u;
        for (j=0; j < sizeof(uint64_t); j++)
          buffer[c++] = dst[j];
      }
      break;
    }
    case NAINT64: {  /* 6.2.3 */
      int64_t u;
      for (i=start; i < start + nvals; i++) {
        u = a->data.i64[i];
#if BYTE_ORDER == BIG_ENDIAN
        tools_swapint64_t(&u);  /* convert to Little Endian 64-bit `integer` */
#endif
        dst = (unsigned char *)&u;
        for (j=0; j < sizeof(int64_t); j++)
          buffer[c++] = dst[j];
      }
      break;
    }
#ifndef __ARMCPU  /* 2.37.1 */
    case NALDOUBLE: {  /* 2.35.0 */
      union ldshape d;
      for (i=start; i < start + nvals; i++) {
        /* memset(d.c, 0, SIZEOFLDBL);
        d.i.m = d.i.se = d.i.junk = 0; */
        d.f = a->data.ld[i];
        for (j=0; j < SIZEOFLDBL; j++) {
#if BYTE_ORDER == BIG_ENDIAN
          buffer[c++] = d.c[SIZEOFLDBL - j - 1];
#else
          buffer[c++] = d.c[j];
#endif
        }
      }
      break;
    }
#endif
    default:
      lua_assert(0);
  }
  return c;
}

static int numarray_write (lua_State *L) {
  int hnd, en, bufdatasize;
  int64_t nvals, start;
  ssize_t nbyteswritten;
  size_t valuestowrite, nbytes, bytestowrite;
  unsigned char *buffer;
  NumArray *a;
  hnd = agn_tofileno(L, 1, 1);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "numarray.write");  /* 2.37.5 */
  a = checknumarray(L, 2);
  switch (a->datatype) {
    case NAUCHAR:   nbytes = sizeof(unsigned char); break;
    case NADOUBLE:  nbytes = sizeof(lua_Number); break;
    case NACDOUBLE: nbytes = sizeof(agn_Complex); break;
    case NAINT32:   nbytes = sizeof(int32_t); break;
    case NAUINT16:  nbytes = sizeof(uint16_t); break;  /* 2.18.2 */
    case NAUINT32:  nbytes = sizeof(uint32_t); break;  /* 2.22.1 */
    case NAUINT64:  nbytes = sizeof(uint64_t); break;  /* 6.2.3 */
    case NAINT64:   nbytes = sizeof(int64_t); break;  /* 6.2.3 */
    case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
      nbytes = SIZEOFLDBL;
#else
      nbytes = 0;
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.write");
#endif
      break;
    default: {
      nbytes = 0;
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.write");  /* avoid compiler warnings */
    }
  }
  bufdatasize = agn_getbuffersize(L)/nbytes;
  if (bufdatasize < 1)
    luaL_error(L, "Error in " LUA_QS ": buffer too small, increase it with environ.kernel/buffersize.", "numarray.write", hnd);
  start = agnL_optinteger(L, 3, 1) - 1;
  if (start < 0 || start >= a->size)
    luaL_error(L, "Error in " LUA_QS " with file #%d: start position %d out-of-range.", "numarray.write", hnd, start + 1);
  nvals = agnL_optinteger(L, 4, a->size);  /* number of values (not bytes) in array to be written */
  if (start + nvals > a->size) nvals = a->size - start;  /* adjust to actual size */
  if (nvals < 1)
    luaL_error(L, "Error in " LUA_QS " with file #%d: invalid number of bytes to be written.", "numarray.write", hnd);
  /* from here on rewritten 2.18.0 RC 2 */
  valuestowrite = (nvals > bufdatasize) ? bufdatasize : nvals;
  bytestowrite = valuestowrite*nbytes*sizeof(unsigned char);
  buffer = (unsigned char *)malloc(bytestowrite);
  if (buffer == NULL)
    luaL_error(L, "Error in " LUA_QS " with file #%d: memory allocation failed.", "numarray.write", hnd);
  do {
    if (aux_fillbuffer(L, buffer, a, start, valuestowrite, bytestowrite) != bytestowrite)
      luaL_error(L, "Error in " LUA_QS " with file #%d: memory error.", "numarray.write", hnd);
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if ( (nbyteswritten = write(hnd, buffer, bytestowrite)) == -1) {
      en = errno;
      xfree(buffer);
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "numarray.write", hnd, my_ioerror(en));
    }
    if (nbyteswritten != bytestowrite) {
      en = errno;  /* 4.0.2 */
      xfree(buffer);
      luaL_error(L, "Error in " LUA_QS " with file #%d: I/O error.", "numarray.write", hnd);
    }
    nvals -= valuestowrite;
    start += valuestowrite;
    if (nvals < valuestowrite) {
      valuestowrite = nvals;
      bytestowrite = valuestowrite*nbytes;
    }
  } while (nvals > 0);
  start++;
  if (start > a->size)  /* no more bytes to be written ? */
    lua_pushnil(L);
  else  /* return the next start position for a further call to write further parts of the numarray; 2.17.8 change */
    lua_pushnumber(L, start);
  tools_fsync(hnd);  /* 2.17.8, 2.35.0 */
  xfree(buffer);
  return 1;
}


/* numarray.toseq (a)

Receives a numarray userdata structure a and converts it into a sequence of numbers, the return. */

static int numarray_toseq (lua_State *L) {
  size_t i;
  NumArray *a = checknumarray(L, 1);
  agn_createseq(L, a->size);
  switch (a->datatype) {
    case NAUCHAR:
      for (i=0; i < a->size; i++)
        agn_seqsetinumber(L, -1, i + 1, a->data.c[i]);
      break;
    case NADOUBLE:
      for (i=0; i < a->size; i++)
        agn_seqsetinumber(L, -1, i + 1, a->data.n[i]);
      break;
    case NACDOUBLE: {
      agn_pComplex z;
      for (i=0; i < a->size; i++) {
        z = a->data.z[i];
        aux_pushcomplex(L, z);
        lua_seqrawseti(L, -2, i + 1);
      }
      break;
    }
    case NAINT32:
      for (i=0; i < a->size; i++)
        agn_seqsetinumber(L, -1, i + 1, a->data.i[i]);
      break;
    case NAUINT16:  /* 2.18.2 */
      for (i=0; i < a->size; i++)
        agn_seqsetinumber(L, -1, i + 1, a->data.us[i]);
      break;
    case NAUINT32:  /* 2.22.1 */
      for (i=0; i < a->size; i++)
        agn_seqsetinumber(L, -1, i + 1, a->data.ui[i]);
      break;
    case NAUINT64:  /* 6.2.3, FIXME */
      for (i=0; i < a->size; i++) {
        createuint64(L, a->data.ui64[i]);
        lua_seqrawseti(L, -2, i + 1);
      }
      break;
    case NAINT64:  /* 6.2.3, FIXME */
      for (i=0; i < a->size; i++) {
        createint64(L, a->data.i64[i]);
        lua_seqrawseti(L, -2, i + 1);
      }
      break;
    case NALDOUBLE:  /* 2.35.0 */
#ifndef __ARMCPU  /* 2.37.1 */
      for (i=0; i < a->size; i++) {
        createdlong(L, a->data.ld[i]);
        lua_seqrawseti(L, -2, i + 1);
      }
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.toseq");
#endif
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.toseq");
  }
  return 1;
}


/* numarray.toreg (a)

Receives a numarray userdata structure a and converts it into a register of numbers, the return. */

static int numarray_toreg (lua_State *L) {
  size_t i;
  NumArray *a = checknumarray(L, 1);
  agn_createreg(L, a->size);
  switch (a->datatype) {
    case NAUCHAR:
      for (i=0; i < a->size; i++) {
        agn_regsetinumber(L, -1, i + 1, a->data.c[i]);
      }
      break;
    case NADOUBLE:
      for (i=0; i < a->size; i++) {
        agn_regsetinumber(L, -1, i + 1, a->data.n[i]);
      }
      break;
    case NACDOUBLE: {
      agn_pComplex z;
      for (i=0; i < a->size; i++) {
        z = a->data.z[i];
        aux_pushcomplex(L, z);
        agn_regrawseti(L, -2, i + 1);
      }
      break;
    }
    case NAINT32:
      for (i=0; i < a->size; i++) {
        agn_regsetinumber(L, -1, i + 1, a->data.i[i]);
      }
      break;
    case NAUINT16:  /* 2.18.2 */
      for (i=0; i < a->size; i++) {
        agn_regsetinumber(L, -1, i + 1, a->data.us[i]);
      }
      break;
    case NAUINT32:  /* 2.22.1 */
      for (i=0; i < a->size; i++) {
        agn_regsetinumber(L, -1, i + 1, a->data.ui[i]);
      }
      break;
    case NAUINT64:  /* 6.2.3, FIXME */
      for (i=0; i < a->size; i++) {
        createuint64(L, a->data.ui64[i]);
        agn_regrawseti(L, -2, i + 1);
      }
      break;
    case NAINT64:  /* 6.2.3, FIXME */
      for (i=0; i < a->size; i++) {
        createint64(L, a->data.i64[i]);
        agn_regrawseti(L, -2, i + 1);
      }
      break;
    case NALDOUBLE:  /* 2.35.0 */
#ifndef __ARMCPU  /* 2.37.1 */
      for (i=0; i < a->size; i++) {
        createdlong(L, a->data.ld[i]);
        agn_regrawseti(L, -2, i + 1);
      }
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.toreg");
#endif
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.toreg");
  }
  return 1;
}


static int numarray_totable (lua_State *L) {  /* 3.5.6 */
  size_t i;
  NumArray *a = checknumarray(L, 1);
  lua_createtable(L, a->size, 0);
  switch (a->datatype) {
    case NAUCHAR:
      for (i=0; i < a->size; i++)
        agn_setinumber(L, -1, i + 1, a->data.c[i]);
      break;
    case NADOUBLE:
      for (i=0; i < a->size; i++)
        agn_setinumber(L, -1, i + 1, a->data.n[i]);
      break;
    case NACDOUBLE: {
      agn_pComplex z;
      for (i=0; i < a->size; i++) {
        z = a->data.z[i];
        aux_pushcomplex(L, z);
        lua_rawseti(L, -2, i + 1);
      }
      break;
    }
    case NAINT32:
      for (i=0; i < a->size; i++)
        agn_setinumber(L, -1, i + 1, a->data.i[i]);
      break;
    case NAUINT16:  /* 2.18.2 */
      for (i=0; i < a->size; i++)
        agn_setinumber(L, -1, i + 1, a->data.us[i]);
      break;
    case NAUINT32:  /* 2.22.1 */
      for (i=0; i < a->size; i++)
        agn_setinumber(L, -1, i + 1, a->data.ui[i]);
      break;
    case NAUINT64:  /* 6.2.3, FIXME */
      for (i=0; i < a->size; i++) {
        createuint64(L, a->data.ui64[i]);
        lua_rawseti(L, -2, i + 1);
      }
      break;  /* 6.3.1 fix */
    case NAINT64:  /* 6.2.3, FIXME */
      for (i=0; i < a->size; i++) {
        createint64(L, a->data.i64[i]);
        lua_rawseti(L, -2, i + 1);
      }
      break;  /* 6.3.1 fix */
    case NALDOUBLE:  /* 2.35.0 */
#ifndef __ARMCPU  /* 2.37.1 */
      for (i=0; i < a->size; i++) {
        createdlong(L, a->data.ld[i]);
        lua_rawseti(L, -2, i + 1);
      }
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.totable");
#endif
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.totable");
  }
  return 1;
}


/* numarray.toarray(o [, option])

   Converts a sequence or register o of numbers into a numarray. By default, a double array is returned;
   if the second argument `option` is the string 'uchar', an unsigned char array is created; if it is the
   string 'integer', an integer array is returned. By default, option is the string 'double'.

   If a value in o is not a number, zero is written to the array. */

static int numarray_toarray (lua_State *L) {
  int64_t i, n, dims[NAMAXDIMS];
  int what;
  NumArray *a;
  static const char *const datatypes[] =  /* GREP "typedef enum { NAUCHAR" in numarray.h */
    {"uchar", "double", "int32", "ushort", "uint32", "uint64", "int64", "longdouble", "cdouble", NULL};  /* 4.9.5 extension */
  what = agnL_checkoption(L, 2, "double", datatypes, 0);
  tools_bzero(dims, NAMAXDIMS*sizeof(int64_t));
  switch (lua_type(L, 1)) {
    case LUA_TTABLE: {  /* 2.31.3 */
      n = agn_asize(L, 1);
      dims[0] = n;
      a = createarray(L, what, n, 1, dims, "numarray.toarray");
      switch (a->datatype) {
        case NAUCHAR:
          for (i=0; i < n; i++)
            a->data.c[i] = (unsigned char)agn_getinumber(L, 1, i + 1);
          break;
        case NADOUBLE:
          for (i=0; i < n; i++)
            a->data.n[i] = agn_getinumber(L, 1, i + 1);
          break;
        case NACDOUBLE: {
          int rc;
          lua_Number z[2];
          for (i=0; i < n; i++) {
            agn_rawgeticomplex(L, 1, i + 1, z, &rc);
            if (!rc)
              luaL_error(L, "Error in " LUA_QS ": (complex) number expected as initialiser.", "numarray.toarray");
            setcdarrayz2(a->data.z, i, z);
          }
          break;
        }
        case NAINT32:
          for (i=0; i < n; i++)
            a->data.i[i] = (int32_t)agn_getinumber(L, 1, i + 1);
          break;
        case NAUINT16:
          for (i=0; i < n; i++)
            a->data.us[i] = (uint16_t)agn_getinumber(L, 1, i + 1);
          break;
        case NAUINT32:
          for (i=0; i < n; i++)
            a->data.ui[i] = (uint32_t)agn_getinumber(L, 1, i + 1);
          break;
        case NAUINT64:
          for (i=0; i < n; i++) {
            lua_rawgeti(L, 1, i + 1);
            a->data.ui64[i] = agnL_checkuint64(L, -1, "numarray.toarray");
            agn_poptop(L);
          }
          break;
        case NAINT64:
          for (i=0; i < n; i++) {
            lua_rawgeti(L, 1, i + 1);
            a->data.i64[i] = agnL_checkint64(L, -1, "numarray.toarray");
            agn_poptop(L);
          }
          break;
        case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
          for (i=0; i < n; i++) {
            lua_rawgeti(L, 1, i + 1);
            a->data.ld[i] = agnL_checkdlongnum(L, -1, "numarray.toarray");
            agn_poptop(L);
          }
#else
          luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.toarray");
#endif
          break;
        default:
          luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.toarray");
      }
      break;
    }
    case LUA_TSEQ: {
      n = agn_seqsize(L, 1);
      dims[0] = n;
      a = createarray(L, what, n, 1, dims, "numarray.toarray");
      switch (a->datatype) {
        case NAUCHAR:
          for (i=0; i < n; i++)
            a->data.c[i] = (unsigned char)lua_seqrawgetinumber(L, 1, i + 1);
          break;
        case NADOUBLE:
          for (i=0; i < n; i++)
            a->data.n[i] = lua_seqrawgetinumber(L, 1, i + 1);
          break;
        case NACDOUBLE: {
          int rc;
          lua_Number z[2];
          for (i=0; i < n; i++) {
            agn_seqrawgeticomplex(L, 1, i + 1, z, &rc);
            setcdarrayz2(a->data.z, i, z);
          }
          break;
        }
        case NAINT32:
          for (i=0; i < n; i++)
            a->data.i[i] = (int32_t)lua_seqrawgetinumber(L, 1, i + 1);
          break;
        case NAUINT16:  /* 2.18.2 */
          for (i=0; i < n; i++)
            a->data.us[i] = (uint16_t)lua_seqrawgetinumber(L, 1, i + 1);
          break;
        case NAUINT32:  /* 2.22.1 */
          for (i=0; i < n; i++)
            a->data.ui[i] = (uint32_t)lua_seqrawgetinumber(L, 1, i + 1);
          break;
        case NAUINT64:  /* 6.2.3 */
          for (i=0; i < n; i++) {
            lua_seqrawgeti(L, 1, i + 1);
            a->data.ui64[i] = agnL_checkuint64(L, -1, "numarray.toarray");
            agn_poptop(L);
          }
          break;
        case NAINT64:  /* 6.2.3 */
          for (i=0; i < n; i++) {
            lua_seqrawgeti(L, 1, i + 1);
            a->data.i64[i] = agnL_checkint64(L, -1, "numarray.toarray");
            agn_poptop(L);
          }
          break;
        case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
          for (i=0; i < n; i++) {
            lua_seqrawgeti(L, 1, i + 1);
            a->data.ld[i] = agnL_checkdlongnum(L, -1, "numarray.toarray");
            agn_poptop(L);
          }
#else
          luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.toarray");
#endif
          break;
        default:
          luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.toarray");
      }
      break;
    }
    case LUA_TREG: {
      n = agn_reggettop(L, 1);
      dims[0] = n;
      a = createarray(L, what, n, 1, dims, "numarray.toarray");
      switch (a->datatype) {
        case NAUCHAR:
          for (i=0; i < n; i++)
            a->data.c[i] = (unsigned char)agn_reggetinumber(L, 1, i + 1);
          break;
        case NADOUBLE:
          for (i=0; i < n; i++)
            a->data.n[i] = agn_reggetinumber(L, 1, i + 1);
          break;
        case NACDOUBLE: {
          int rc;
          lua_Number z[2];
          for (i=0; i < n; i++) {
            agn_seqrawgeticomplex(L, 1, i + 1, z, &rc);
            setcdarrayz2(a->data.z, i, z);
          }
          break;
        }
        case NAINT32:
          for (i=0; i < n; i++)
            a->data.i[i] = (int32_t)agn_reggetinumber(L, 1, i + 1);
          break;
        case NAUINT16:  /* 2.18.2 */
          for (i=0; i < n; i++)
            a->data.us[i] = (uint16_t)agn_reggetinumber(L, 1, i + 1);
          break;
        case NAUINT32:  /* 2.22.1 */
          for (i=0; i < n; i++)
            a->data.ui[i] = (uint32_t)agn_reggetinumber(L, 1, i + 1);
          break;
        case NAUINT64:  /* 6.2.3 */
          for (i=0; i < n; i++) {
            agn_regrawgeti(L, 1, i + 1);
            a->data.ui64[i] = agnL_checkuint64(L, -1, "numarray.toarray");
            agn_poptop(L);
          }
          break;
        case NAINT64:  /* 6.2.3 */
          for (i=0; i < n; i++) {
            agn_regrawgeti(L, 1, i + 1);
            a->data.i64[i] = agnL_checkint64(L, -1, "numarray.toarray");
            agn_poptop(L);
          }
          break;
        case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
          for (i=0; i < n; i++) {
            agn_regrawgeti(L, 1, i + 1);
            a->data.ld[i] = agnL_checkdlongnum(L, -1, "numarray.toarray");
            agn_poptop(L);
          }
#else
          luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.toarray");
#endif
          break;
        default:
          luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.toarray");
      }
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.",
        "numarray.toarray", luaL_typename(L, 1));
  }
  return 1;
}


/* numarray.include(a, pos, b)

   Copies all values in the numarray b into the numarray a, starting at index pos (a number) of a. The function
   returns nothing. Both numarrays must by of the same type: either be uchar, integer, or double arrays. */

#define aux_include_ud(L,ds1,ds2) { \
  for (i=pos; i < pos + b->size; i++) { \
    ds1[i] = ds2[i - pos]; \
  } \
}

#define cmplx_include_ud(L,ds1,ds2) { \
  for (i=pos; i < pos + b->size; i++) { \
    copycdarray(ds1, i, ds2, i - pos); \
  } \
}

#define aux_include_shift(a,ds,pos,offset) { \
  for (i=(a)->size - 1; i > pos; i--) { \
    (ds)[i] = (ds)[i - offset]; \
  } \
}

#define cmplx_include_shift(a,ds,pos,offset) { \
  for (i=(a)->size - 1; i > pos; i--) { \
    copycdarray(ds, i, ds, i - offset); \
  } \
}

#define aux_include_insert(ds,nops,pos) { \
  for (i=3; i <= nops + 2; i++) { \
    getanynumber(L, d, i, "numarray.include"); \
    (ds)[pos++] = d.d; \
  } \
}

#ifdef PROPCMPLX
#define prop_include_insert(ds,nops,pos) { \
  for (i=3; i <= nops + 2; i++) { \
    getanynumber(L, d, i, "numarray.include"); \
    if (lua_isnumber(L, i)) { \
      (ds)[pos][0] = d.d; \
      (ds)[pos++][1] = 0.0; \
    } else { \
      (ds)[pos][0] = d.z[0]; \
      (ds)[pos++][1] = d.z[1]; \
    } \
  } \
}
#else
#define prop_include_insert(ds,nops,pos) { \
  for (i=3; i <= nops + 2; i++) { \
    getanynumber(L, d, i, "numarray.include"); \
    if (lua_isnumber(L, i)) { \
      (ds)[pos++] = d.d; \
    } else { \
      (ds)[pos++] = d.z; \
    } \
  } \
}
#endif

#define aux_na_enlargeby(L,nops,ds,datatype,procname) { \
  if ((ds = realloc(ds, (a->size + nops) * sizeof(datatype))) == NULL) \
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname); \
}

static int numarray_include (lua_State *L) {
  int64_t i, pos;
  NumArray *a, *b;
  a = checknumarray(L, 1);
  agnL_checkudfreezed(L, 1, "numarray", "numarray.include");  /* 4.9.0 */
  pos = agn_checkinteger(L, 2);  /* extended 2.15.1, 2.15.2 fix, 2.15.5 fix */
  if (pos == 0)
    luaL_error(L, "Error in " LUA_QS ": index 0 is out-of-range.", "numarray.include");
  pos = tools_posrelat(pos, a->size) - 1;  /* 2.15.5 fix */
  if (isnumarray(L, 3)) {
    b = lua_touserdata(L, 3);
    if (b->size + pos > a->size)
      luaL_error(L, "Error in " LUA_QS ": first numarray not large enough.", "numarray.include");
    if (a->datatype != b->datatype)
      luaL_error(L, "Error in " LUA_QS ": both numarrays must by of the same type.", "numarray.include");
    switch (a->datatype) {  /* if size of b is 0, nothing happens */
      case NAUCHAR:  aux_include_ud(L, a->data.c,  b->data.c);  break;
      case NADOUBLE: aux_include_ud(L, a->data.n,  b->data.n);  break;
      case NACDOUBLE:
        cmplx_include_ud(L, a->data.z,  b->data.z);
        break;
      case NAINT32:  aux_include_ud(L, a->data.i,  b->data.i);  break;
      case NAUINT16: aux_include_ud(L, a->data.us, b->data.us); break;
      case NAUINT32: aux_include_ud(L, a->data.ui, b->data.ui); break;
      case NAUINT64: aux_include_ud(L, a->data.ui64, b->data.ui64); break;
      case NAINT64:  aux_include_ud(L, a->data.i64, b->data.i64); break;
#ifndef __ARMCPU  /* 2.37.1 */
      case NALDOUBLE: aux_include_ud(L, a->data.ld, b->data.ld); break;
#else
      case NALDOUBLE:
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.include");
        break;
#endif
      default: luaL_error(L, "Error in " LUA_QS ": this should not happen/1.", "numarray.include");
    }
  } else {  /* added 2.9.1/2.35.0; rewritten 4.2.1 to insert multiple numbers with one call */
    union ldshape d;
    int nops = lua_gettop(L);
    if (pos > a->size)  /* allow index pos = a->size + 1 */
      luaL_error(L, "Error in " LUA_QS ": index %d is out-of-range.", "numarray.include", pos + 1);
    if (nops < 3)
      luaL_error(L, "Error in " LUA_QS ": need at least three arguments.", "numarray.include");
    /* first check whether we have all numbers */
    for (i=3; i <= nops; i++) {  /* 4.9.4 fix */
      if (!(lua_isnumber(L, i) ||
           (a->datatype == NACDOUBLE && lua_iscomplex(L, i)) ||  /* 6.3.1 fix */
           (a->datatype == NALDOUBLE && luaL_isudata(L, i, "longdouble")) ||
           (luaL_isudata(L, i, "ints") && (a->datatype == NAUINT64 || a->datatype == NAINT64))
          ))
        luaL_error(L, "Error in " LUA_QS ": expected a (complex) number or ints for arg #%d, got %s.",
          "numarray.include", i, luaL_typename(L, i));
    }
    nops -= 2;  /* number of elements to insert */
    /* enlarge array by nops slots, shift values to open space and insert new values starting from the given position */
    switch (a->datatype) {
      case NAUCHAR: {
        aux_na_enlargeby(L, nops, a->data.c, unsigned char, "numarray.include");
        a->size += nops;
        aux_include_shift(a, a->data.c, pos, nops);
        aux_include_insert(a->data.c, nops, pos);
        break;
      }
      case NADOUBLE: {
        aux_na_enlargeby(L, nops, a->data.n, lua_Number, "numarray.include");
        a->size += nops;
        aux_include_shift(a, a->data.n, pos, nops);
        aux_include_insert(a->data.n, nops, pos);
        break;
      }
      case NACDOUBLE: {
        aux_na_enlargeby(L, nops, a->data.z, agn_Complex, "numarray.include");
        a->size += nops;
        cmplx_include_shift(a, a->data.z, pos, nops);
        prop_include_insert(a->data.z, nops, pos);
        break;
      }
      case NAINT32: {
        aux_na_enlargeby(L, nops, a->data.i, int32_t, "numarray.include");
        a->size += nops;
        aux_include_shift(a, a->data.i, pos, nops);
        aux_include_insert(a->data.i, nops, pos);
        break;
      }
      case NAUINT16: {
        aux_na_enlargeby(L, nops, a->data.us, uint16_t, "numarray.include");
        a->size += nops;
        aux_include_shift(a, a->data.us, pos, nops);
        aux_include_insert(a->data.us, nops, pos);
        break;
      }
      case NAUINT32: {
        aux_na_enlargeby(L, nops, a->data.ui, uint32_t, "numarray.include");
        a->size += nops;
        aux_include_shift(a, a->data.ui, pos, nops);
        aux_include_insert(a->data.ui, nops, pos);
        break;
      }
      case NAUINT64: {
        aux_na_enlargeby(L, nops, a->data.ui64, uint64_t, "numarray.include");
        a->size += nops;
        aux_include_shift(a, a->data.ui64, pos, nops);
        for (i=3; i <= nops + 2; i++) {
          getanynumber(L, d, i, "numarray.include");  /* 6.3.1 fix */
          a->data.ui64[pos++] = d.ui64;
        }
        break;
      }
      case NAINT64: {
        aux_na_enlargeby(L, nops, a->data.i64, int64_t, "numarray.include");
        a->size += nops;
        aux_include_shift(a, a->data.i64, pos, nops);
        for (i=3; i <= nops + 2; i++) {
          getanynumber(L, d, i, "numarray.include");  /* 6.3.1 fix */
          a->data.i64[pos++] = d.i64;
        }
        break;
      }
      case NALDOUBLE: {
#ifndef __ARMCPU
        aux_na_enlargeby(L, nops, a->data.ld, long double, "numarray.include");
        a->size += nops;
        aux_include_shift(a, a->data.ld, pos, nops);
        for (i=3; i <= nops + 2; i++) {
          getanynumber(L, d, i, "numarray.include");
          a->data.ld[pos++] = tolong(L, i, d);
        }
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.include");
#endif
        break;
      }
      default: luaL_error(L, "Error in " LUA_QS ": this should not happen/2.", "numarray.include");
    }
  }
  return 0;
}


#define aux_prepend_shift(a,ds,pos) { \
  for (i=(a)->size - 1; i >= pos; i--) { \
    (ds)[i] = (ds)[i - pos]; \
  } \
}

#define cmplx_prepend_shift(a,ds,pos) { \
  for (i=(a)->size - 1; i >= pos; i--) { \
    copycdarray(ds, i, ds, i - pos); \
  } \
}

#define aux_prepend_insert(ds,nops,pos) { \
  for (i=2; i <= nops + 1; i++) { \
    getanynumber(L, d, i, "numarray.prepend"); \
    (ds)[pos++] = d.d; \
  } \
}

#define cmplx_prepend_insert(ds,nops,pos) { \
  for (i=2; i <= nops + 1; i++) { \
    getanynumber(L, d, i, "numarray.prepend"); \
    if (lua_isnumber(L, i)) { \
      assigncdarray(ds, pos, d.d, 0); \
    } else { \
      setcdarrayp(ds, pos, d.z); \
    } \
    (pos)++; \
  } \
}

static int numarray_prepend (lua_State *L) {  /* 4.2.1 */
  int64_t i, pos;
  int nargs;
  NumArray *a, *b;
  a = checknumarray(L, 1);
  agnL_checkudfreezed(L, 1, "numarray", "numarray.prepend");  /* 4.9.0 */
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": need at least two arguments.", "numarray.prepend");
  pos = 0;
  if (isnumarray(L, 2)) {
    b = lua_touserdata(L, 2);
    if (a->datatype != b->datatype)
      luaL_error(L, "Error in " LUA_QS ": both numarrays must by of the same type.", "numarray.prepend");
    switch (a->datatype) {
      case NAUCHAR: {
        aux_na_enlargeby(L, b->size, a->data.c, unsigned char, "numarray.prepend");
        a->size += b->size;
        aux_prepend_shift(a, a->data.c, b->size);
        for (i=0; i < b->size; i++) a->data.c[i] = b->data.c[i];
        break;
      }
      case NADOUBLE: {
        aux_na_enlargeby(L, b->size, a->data.n, lua_Number, "numarray.prepend");
        a->size += b->size;
        aux_prepend_shift(a, a->data.n, b->size);
        for (i=0; i < b->size; i++) a->data.n[i] = b->data.n[i];
        break;
      }
      case NACDOUBLE: {
        aux_na_enlargeby(L, b->size, a->data.z, agn_Complex, "numarray.prepend");
        a->size += b->size;
        cmplx_prepend_shift(a, a->data.z, b->size);
        for (i=0; i < b->size; i++) { copycdarray(a->data.z, i, b->data.z, i); }
        break;
      }
      case NAINT32: {
        aux_na_enlargeby(L, b->size, a->data.i, int32_t, "numarray.prepend");
        a->size += b->size;
        aux_prepend_shift(a, a->data.i, b->size);
        for (i=0; i < b->size; i++) a->data.i[i] = b->data.i[i];
        break;
      }
      case NAUINT16: {
        aux_na_enlargeby(L, b->size, a->data.us, uint16_t, "numarray.prepend");
        a->size += b->size;
        aux_prepend_shift(a, a->data.us, b->size);
        for (i=0; i < b->size; i++) a->data.us[i] = b->data.us[i];
        break;
      }
      case NAUINT32: {
        aux_na_enlargeby(L, b->size, a->data.ui, uint32_t, "numarray.prepend");
        a->size += b->size;
        aux_prepend_shift(a, a->data.ui, b->size);
        for (i=0; i < b->size; i++) a->data.ui[i] = b->data.ui[i];
        break;
      }
      case NAUINT64: {
        aux_na_enlargeby(L, b->size, a->data.ui64, uint64_t, "numarray.prepend");
        a->size += b->size;
        aux_prepend_shift(a, a->data.ui64, b->size);
        for (i=0; i < b->size; i++) a->data.ui64[i] = b->data.ui64[i];
        break;
      }
      case NAINT64: {
        aux_na_enlargeby(L, b->size, a->data.i64, int64_t, "numarray.prepend");
        a->size += b->size;
        aux_prepend_shift(a, a->data.i64, b->size);
        for (i=0; i < b->size; i++) a->data.i64[i] = b->data.i64[i];
        break;
      }
      case NALDOUBLE: {
#ifndef __ARMCPU
        aux_na_enlargeby(L, b->size, a->data.ld, long double, "numarray.prepend");
        a->size += b->size;
        aux_prepend_shift(a, a->data.ld, b->size);
        for (i=0; i < b->size; i++) a->data.ld[i] = b->data.ld[i];
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.prepend");
#endif
        break;
      }
      default: luaL_error(L, "Error in " LUA_QS ": this should not happen/1.", "numarray.prepend");
    }
  } else {  /* insert multiple items with one call */
    union ldshape d;
    /* first check whether we have all numbers */
    for (i=2; i <= nargs; i++) {
      if (!(lua_isnumber(L, i) ||
           (a->datatype == NACDOUBLE && lua_iscomplex(L, i)) ||  /* 6.3.1 fix */
           (a->datatype == NALDOUBLE && luaL_isudata(L, i, "longdouble")) ||
           (luaL_isudata(L, i, "ints") && (a->datatype == NAUINT64 || a->datatype == NAINT64))
          ))
        luaL_error(L, "Error in " LUA_QS ": expected a (complex) number or ints for arg #%d, got %s.",
          "numarray.prepend", i, luaL_typename(L, i));
    }
    nargs--;  /* number of elements to insert */
    /* enlarge array by nops slots, shift values to open space and insert new values starting from the given position */
    switch (a->datatype) {
      case NAUCHAR: {
        aux_na_enlargeby(L, nargs, a->data.c, unsigned char, "numarray.prepend");
        a->size += nargs;
        aux_prepend_shift(a, a->data.c, nargs);
        aux_prepend_insert(a->data.c, nargs, pos);
        break;
      }
      case NADOUBLE: {
        aux_na_enlargeby(L, nargs, a->data.n, lua_Number, "numarray.prepend");
        a->size += nargs;
        aux_prepend_shift(a, a->data.n, nargs);
        aux_prepend_insert(a->data.n, nargs, pos);
        break;
      }
      case NACDOUBLE: {
        aux_na_enlargeby(L, nargs, a->data.z, agn_Complex, "numarray.prepend");
        a->size += nargs;
        cmplx_prepend_shift(a, a->data.z, nargs);
        cmplx_prepend_insert(a->data.z, nargs, pos);
        break;
      }
      case NAINT32: {
        aux_na_enlargeby(L, nargs, a->data.i, int32_t, "numarray.prepend");
        a->size += nargs;
        aux_prepend_shift(a, a->data.i, nargs);
        aux_prepend_insert(a->data.i, nargs, pos);
        break;
      }
      case NAUINT16: {
        aux_na_enlargeby(L, nargs, a->data.us, uint16_t, "numarray.prepend");
        a->size += nargs;
        aux_prepend_shift(a, a->data.us, nargs);
        aux_prepend_insert(a->data.us, nargs, pos);
        break;
      }
      case NAUINT32: {
        aux_na_enlargeby(L, nargs, a->data.ui, uint32_t, "numarray.prepend");
        a->size += nargs;
        aux_prepend_shift(a, a->data.ui, nargs);
        aux_prepend_insert(a->data.ui, nargs, pos);
        break;
      }
      case NAUINT64: {
        aux_na_enlargeby(L, nargs, a->data.ui64, uint64_t, "numarray.prepend");
        a->size += nargs;
        aux_prepend_shift(a, a->data.ui64, nargs);
        for (i=2; i <= nargs + 1; i++) {
          a->data.ui64[pos++] = agnL_checkuint64(L, i, "numarray.prepend");
        }
        break;
      }
      case NAINT64: {
        aux_na_enlargeby(L, nargs, a->data.i64, int64_t, "numarray.prepend");
        a->size += nargs;
        aux_prepend_shift(a, a->data.i64, nargs);
        for (i=2; i <= nargs + 1; i++) {
          a->data.i64[pos++] = agnL_checkint64(L, i, "numarray.prepend");
        }
        break;
      }
      case NALDOUBLE: {
#ifndef __ARMCPU
        aux_na_enlargeby(L, nargs, a->data.ld, long double, "numarray.prepend");
        a->size += nargs;
        aux_prepend_shift(a, a->data.ld, nargs);
        for (i=2; i <= nargs + 1; i++) {
          getanynumber(L, d, i, "numarray.prepend");
          a->data.ld[pos++] = tolong(L, i, d);
        }
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.prepend");
#endif
        break;
      }
      default: luaL_error(L, "Error in " LUA_QS ": this should not happen/2.", "numarray.prepend");
    }
  }
  return 0;
}


#define aux_append_ln(ds,x) { \
  (ds)[a->size++] = x; \
}

#define cmplx_append_ln(ds,x) { \
  setcdarrayp(ds, a->size, x); \
  a->size++; \
}

static int numarray_append (lua_State *L) {  /* 4.2.0 */
  int i, nargs;
  NumArray *a = checknumarray(L, 1);
  agnL_checkudfreezed(L, 1, "numarray", "numarray.append");  /* 4.9.0 */
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": need at least two arguments.", "numarray.append");
  if (isnumarray(L, 2)) {  /* 4.2.1 extension */
    NumArray *b = lua_touserdata(L, 2);
    if (a->datatype != b->datatype)
      luaL_error(L, "Error in " LUA_QS ": numarrays are of different types.", "numarray.append");
    switch (a->datatype) {
      case NAUCHAR: {
        aux_na_enlargeby(L, b->size, a->data.c, unsigned char, "numarray.append");
        for (i=0; i < b->size; i++) a->data.c[a->size + i] = b->data.c[i];
        break;
      }
      case NADOUBLE: {
        aux_na_enlargeby(L, b->size, a->data.n, lua_Number, "numarray.append");
        for (i=0; i < b->size; i++) a->data.n[a->size + i] = b->data.n[i];
        break;
      }
      case NACDOUBLE: {
        aux_na_enlargeby(L, b->size, a->data.z, agn_Complex, "numarray.append");
        for (i=0; i < b->size; i++) { copycdarray(a->data.z, a->size + i, b->data.z, i); }
        break;
      }
      case NAINT32: {
        aux_na_enlargeby(L, b->size, a->data.i, int32_t, "numarray.append");
        for (i=0; i < b->size; i++) a->data.i[a->size + i] = b->data.i[i];
        break;
      }
      case NAUINT16: {
        aux_na_enlargeby(L, b->size, a->data.us, uint16_t, "numarray.append");
        for (i=0; i < b->size; i++) a->data.us[a->size + i] = b->data.us[i];
        break;
      }
      case NAUINT32: {
        aux_na_enlargeby(L, b->size, a->data.ui, uint32_t, "numarray.append");
        for (i=0; i < b->size; i++) a->data.ui[a->size + i] = b->data.ui[i];
        break;
      }
      case NAUINT64: {
        aux_na_enlargeby(L, b->size, a->data.ui64, uint64_t, "numarray.append");
        for (i=0; i < b->size; i++) a->data.ui64[a->size + i] = b->data.ui64[i];
        break;
      }
      case NAINT64: {
        aux_na_enlargeby(L, b->size, a->data.i64, int64_t, "numarray.append");
        for (i=0; i < b->size; i++) a->data.i64[a->size + i] = b->data.i64[i];
        break;
      }
      case NALDOUBLE: {
#ifndef __ARMCPU
        aux_na_enlargeby(L, b->size, a->data.ld, long double, "numarray.append");
        for (i=0; i < b->size; i++) a->data.ld[a->size + i] = b->data.ld[i];
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.append");
#endif
        break;
      }
      default: luaL_error(L, "Error in " LUA_QS ": this should not happen/1.", "numarray.append");
    }
    a->size += b->size;
    return 0;
  } else {
    union ldshape d;
    /* first check whether we have all numbers */
    for (i=2; i <= nargs; i++) {
      if (!(lua_isnumber(L, i) ||
           (a->datatype == NACDOUBLE && lua_iscomplex(L, i)) ||  /* 6.3.1 fix */
           (a->datatype == NALDOUBLE && luaL_isudata(L, i, "longdouble")) ||
           (luaL_isudata(L, i, "ints") && (a->datatype == NAUINT64 || a->datatype == NAINT64))
          ))
        luaL_error(L, "Error in " LUA_QS ": expected a (complex) number or ints for arg #%d, got %s.",
          "numarray.append", i, luaL_typename(L, i));
    }
    switch (a->datatype) {
      case NAUCHAR: {
        aux_na_enlargeby(L, nargs - 1, a->data.c, unsigned char, "numarray.append");
        for (i=2; i <= nargs; i++) {
          getanynumber(L, d, i, "numarray.append"); aux_append_ln(a->data.c, d.d);
        }
        break;
      }
      case NADOUBLE: {
        aux_na_enlargeby(L, nargs - 1, a->data.n, lua_Number, "numarray.append");
        for (i=2; i <= nargs; i++) {
          getanynumber(L, d, i, "numarray.append"); aux_append_ln(a->data.n, d.d);
        }
        break;
      }
      case NACDOUBLE: {
        aux_na_enlargeby(L, nargs - 1, a->data.z, agn_Complex, "numarray.append");
        for (i=2; i <= nargs; i++) {
          getanynumber(L, d, i, "numarray.append");
          cmplx_append_ln(a->data.z, d.z);
        }
        break;
      }
      case NAINT32: {
        aux_na_enlargeby(L, nargs - 1, a->data.i, int32_t, "numarray.append");
        for (i=2; i <= nargs; i++) {
          getanynumber(L, d, i, "numarray.append"); aux_append_ln(a->data.i, d.d);
        }
        break;
      }
      case NAUINT16: {
        aux_na_enlargeby(L, nargs - 1, a->data.us, uint16_t, "numarray.append");
        for (i=2; i <= nargs; i++) {
          getanynumber(L, d, i, "numarray.append"); aux_append_ln(a->data.us, d.d);
        }
        break;
      }
      case NAUINT32: {
        aux_na_enlargeby(L, nargs - 1, a->data.ui, uint32_t, "numarray.append");
        for (i=2; i <= nargs; i++) {
          getanynumber(L, d, i, "numarray.append"); aux_append_ln(a->data.ui, d.d);
        }
        break;
      }
      case NAUINT64: {
        aux_na_enlargeby(L, nargs - 1, a->data.ui64, uint64_t, "numarray.append");
        for (i=2; i <= nargs; i++) {
          getanynumber(L, d, i, "numarray.append");
          if (agn_isnumber(L, i)) d.ui64 = d.d;  /* 6.3.5 fix */
          aux_append_ln(a->data.ui64, d.ui64);
        }
        break;
      }
      case NAINT64: {
        aux_na_enlargeby(L, nargs - 1, a->data.i64, int64_t, "numarray.append");
        for (i=2; i <= nargs; i++) {
          getanynumber(L, d, i, "numarray.append");
          if (agn_isnumber(L, i)) d.i64 = d.d;  /* 6.3.5 fix */
          aux_append_ln(a->data.i64, d.i64);
        }
        break;
      }
      case NALDOUBLE: {
#ifndef __ARMCPU
        aux_na_enlargeby(L, nargs - 1, a->data.ld, long double, "numarray.append");
        for (i=2; i <= nargs; i++) {
          getanynumber(L, d, i, "numarray.append"); aux_append_ln(a->data.ld, d.f);  /* 6.2.3 fix */
        }
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.append");
#endif
        break;
      }
      default: luaL_error(L, "Error in " LUA_QS ": this should not happen/1.", "numarray.append");
    }
  }
 return 0;
}


/* Returns the number of bytes consumed by the given array. */

static int64_t aux_used (lua_State *L, int idx, NumArray *a, const char *procname) {
  int64_t r;
  size_t size;
  r = sizeof(NumArray) + sizeof(Udata);  /* 6.3.1 fix */
  size = a->size;
  switch (a->datatype) {
    case NAUCHAR:
      if (size == 0 && a->data.c != NULL) size = 1;
      r += size * sizeof(unsigned char);
      break;
    case NADOUBLE:
      if (size == 0 && a->data.n != NULL) size = 1;
      r += size * sizeof(lua_Number);
      break;
    case NACDOUBLE:
#ifndef PROPCMPLX
      if (size == 0 && a->data.z != NULL) size = 1;
#else
      if (size == 0) size = 1;
#endif
      r += size * sizeof(agn_Complex);
      break;
    case NAINT32:
      if (size == 0 && a->data.i != NULL) size = 1;
      r += size * sizeof(int32_t);
      break;
    case NAUINT16:  /* 2.18.2 */
      if (size == 0 && a->data.us != NULL) size = 1;
      r += size * sizeof(uint16_t);
      break;
    case NAUINT32:  /* 2.22.1 */
      if (size == 0 && a->data.ui != NULL) size = 1;
      r += size * sizeof(uint32_t);
      break;
    case NAUINT64:  /* 6.2.3 */
      if (size == 0 && a->data.ui64 != NULL) size = 1;
      r += size * sizeof(uint64_t);
      break;
    case NAINT64:  /* 6.2.3 */
      if (size == 0 && a->data.i64 != NULL) size = 1;
      r += size * sizeof(int64_t);
      break;
    case NALDOUBLE:  /* 2.35.0 */
#ifndef __ARMCPU  /* 2.37.1 */
      if (size == 0 && a->data.ld != NULL) size = 1;
      r += size * SIZEOFLDBL;
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", procname);
#endif
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", procname);
  }
  return r;

}

static int numarray_used (lua_State *L) {  /* changed 3.5.4 */
  NumArray *a = checknumarray(L, 1);
  lua_pushnumber(L, aux_used(L, 1, a, "numarray.used"));
  return 1;
}


static int numarray_attrib (lua_State *L) {  /* 3.5.4 */
  int i;
  NumArray *a = checknumarray(L, 1);
  luaL_checkstack(L, 5, "not enough stack space");
  if (!agn_getutype(L, 1))  /* pushes the numarray type */
    luaL_error(L, "Error in " LUA_QS ": invalid array.", "numarray.attrib");
  lua_pushnumber(L, a->size);  /* size of first dimension */
  lua_pushnumber(L, aux_used(L, 1, a, "numarray.attrib"));
  lua_pushinteger(L, a->ndims);
  lua_createtable(L, a->ndims, 0);
  for (i=0; i < a->ndims; i++) {
    lua_rawsetiinteger(L, -1, i + 1, a->dims[i]);
  }
  return 5;
}


static int numarray_redim (lua_State *L) {  /* 4.8.1 */
  int64_t dims[NAMAXDIMS];
  int i, ndims, accu = 1, nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": must get at least one dimension.", "numarray.redim");
  NumArray *a = checknumarray(L, 1);
  agnL_checkudfreezed(L, 1, "numarray", "numarray.redim");  /* 4.9.0 */
  tools_bzero(dims, sizeof(int64_t)*NAMAXDIMS);
  for (i=2; i <= nargs && agn_isinteger(L, i); i++) {
    dims[i - 2] = agn_tointeger(L, i);
    if (dims[i - 2] < 1)
      luaL_error(L, "Error in " LUA_QS ": dimension must be positive.", "numarray.redim");
    accu *= dims[i - 2];
  }
  ndims = i - 2;
  if (ndims > NAMAXDIMS)
    luaL_error(L, "Error in " LUA_QS ": up to %d dimensions are supported, got %d.", "numarray.redim", NAMAXDIMS, ndims);
  if (a->size != accu)
    luaL_error(L, "Error in " LUA_QS ": dimensions mismatch with 1D size %d.", "numarray.redim", a->size);
  a->ndims = ndims;
  tools_bzero(a->dims, sizeof(int64_t)*NAMAXDIMS);  /* reset dim vector entirely */
  for (i=0; i < ndims; i++) {
    a->dims[i] = dims[i];
  }
  return 0;
}


static int mt_in (lua_State *L) {  /* `__in` mt, 2.18.0 */
  NumArray *a;
  union ldshape d;
  size_t i;
  getanynumber(L, d, 1, "numarray.__in");
  a = checknumarray(L, 2);
  switch (a->datatype) {
    case NAUCHAR:
      for (i=0; i < a->size && a->data.c[i] != d.d; i++) { }
      break;
    case NADOUBLE:
      for (i=0; i < a->size && a->data.n[i] != d.d; i++) { }
      break;
    case NACDOUBLE:
      for (i=0; i < a->size && !(iseqcdarrayp(a->data.z, i, d.z)); i++) { }
      break;
    case NAINT32:
      for (i=0; i < a->size && a->data.i[i] != d.d; i++) { }
      break;
    case NAUINT16:  /* 2.18.2 */
      for (i=0; i < a->size && a->data.us[i] != d.d; i++) { }
      break;
    case NAUINT32:  /* 2.22.1 */
      for (i=0; i < a->size && a->data.ui[i] != d.d; i++) { }
      break;
    case NAUINT64:  /* 6.2.3 */
      for (i=0; i < a->size && a->data.ui64[i] != d.ui64; i++) { }
      break;
    case NAINT64:  /* 6.2.3 */
      for (i=0; i < a->size && a->data.i64[i] != d.i64; i++) { }
      break;
    case NALDOUBLE:  /* 2.35.0 */
#ifndef __ARMCPU  /* 2.37.1 */
      for (i=0; i < a->size && a->data.ld[i] != tolong(L, 1, d); i++) { }
#else
      i = 0;
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "in metamethod");
#endif
      break;
    default:
      i = 0;
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "in metamethod");
  }
  lua_pushboolean(L, i != a->size);
  return 1;
}


static int mt_notin (lua_State *L) {  /* `__notin` mt, 2.18.0 */
  NumArray *a;
  union ldshape d;
  size_t i;
  getanynumber(L, d, 1, "numarray.__notin");
  a = checknumarray(L, 2);
  switch (a->datatype) {
    case NAUCHAR:
      for (i=0; i < a->size && a->data.c[i] != d.d; i++) { }
      break;
    case NADOUBLE:
      for (i=0; i < a->size && a->data.n[i] != d.d; i++) { }
      break;
    case NACDOUBLE:
      for (i=0; i < a->size && !(iseqcdarrayp(a->data.z, i, d.z)); i++) { }
      break;
    case NAINT32:
      for (i=0; i < a->size && a->data.i[i] != d.d; i++) { }
      break;
    case NAUINT16:  /* 2.18.2 */
      for (i=0; i < a->size && a->data.us[i] != d.d; i++) { }
      break;
    case NAUINT32:  /* 2.22.1 */
      for (i=0; i < a->size && a->data.ui[i] != d.d; i++) { }
      break;
    case NAUINT64:  /* 6.2.3 */
      for (i=0; i < a->size && a->data.ui64[i] != d.ui64; i++) { }
      break;
    case NAINT64:  /* 6.2.3 */
      for (i=0; i < a->size && a->data.i64[i] != d.i64; i++) { }
      break;
    case NALDOUBLE:  /* 2.35.0 */
#ifndef __ARMCPU  /* 2.37.1 */
      for (i=0; i < a->size && a->data.ld[i] != tolong(L, 1, d); i++) { }
#else
      i = 0;
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "notin metamethod");
#endif
      break;
    default:
      i = 0;
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "notin metamethod");
  }
  lua_pushboolean(L, i == a->size);
  return 1;
}


/* Checks whether all elements in the numarray are zeros and returns `true` or `false`. 2.17.9 */
static int mt_zero (lua_State *L) {
  size_t i;
  NumArray *a = checknumarray(L, 1);
  switch (a->datatype) {
    case NAUCHAR:
      for (i=0; i < a->size && a->data.c[i] == 0; i++) { }
      break;
    case NADOUBLE:
      for (i=0; i < a->size && a->data.n[i] == 0; i++) { }
      break;
    case NACDOUBLE:
#ifndef PROPCMPLX
      for (i=0; i < a->size && a->data.z[i] == 0 + 0*I; i++) { }
#else
      for (i=0; i < a->size && a->data.z[i][0] == 0 && a->data.z[i][1] == 0; i++) { }
#endif
      break;
    case NAINT32:
      for (i=0; i < a->size && a->data.i[i] == 0; i++) { }
      break;
    case NAUINT16:  /* 2.18.2 */
      for (i=0; i < a->size && a->data.us[i] == 0; i++) { }
      break;
    case NAUINT32:  /* 2.22.1 */
      for (i=0; i < a->size && a->data.ui[i] == 0; i++) { }
      break;
    case NAUINT64:  /* 6.2.3 */
      for (i=0; i < a->size && a->data.ui64[i] == 0ULL; i++) { }
      break;
    case NAINT64:  /* 6.2.3 */
      for (i=0; i < a->size && a->data.i64[i] == 0LL; i++) { }
      break;
    case NALDOUBLE:  /* 2.35.0 */
#ifndef __ARMCPU  /* 2.37.1 */
      for (i=0; i < a->size && a->data.ld[i] == 0.0L; i++) { }
#else
      i = 0;
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "zero metamethod");
#endif
      break;
    default:
      i = 0;
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "zero metamethod");
  }
  lua_pushboolean(L, i == a->size);
  return 1;
}


/* Checks whether all elements in the numarray are non-zeros and returns `true` or `false`. 2.17.9 */
static int mt_nonzero (lua_State *L) {  /* 2.17.9 */
  size_t i;
  NumArray *a = checknumarray(L, 1);
  switch (a->datatype) {
    case NAUCHAR:
      for (i=0; i < a->size && a->data.c[i] != 0; i++) { }
      break;
    case NADOUBLE:
      for (i=0; i < a->size && a->data.n[i] != 0; i++) { }
      break;
    case NACDOUBLE:
#ifndef PROPCMPLX
      for (i=0; i < a->size && a->data.z[i] != 0 + 0*I; i++) { }
#else
      for (i=0; i < a->size && a->data.z[i][0] != 0 && a->data.z[i][1] != 0; i++) { }
#endif
      break;
    case NAINT32:
      for (i=0; i < a->size && a->data.i[i] != 0; i++) { }
      break;
    case NAUINT16:  /* 2.18.2 */
      for (i=0; i < a->size && a->data.us[i] != 0; i++) { }
      break;
    case NAUINT32:  /* 2.22.1 */
      for (i=0; i < a->size && a->data.ui[i] != 0; i++) { }
      break;
    case NAUINT64:  /* 6.2.3 */
      for (i=0; i < a->size && a->data.ui64[i] != 0ULL; i++) { }
      break;
    case NAINT64:  /* 6.2.3 */
      for (i=0; i < a->size && a->data.i64[i] != 0LL; i++) { }
      break;
    case NALDOUBLE:  /* 2.35.0 */
#ifndef __ARMCPU  /* 2.37.1 */
      for (i=0; i < a->size && a->data.ld[i] != 0.0L; i++) { }
#else
      i = 0;
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "nonzero metamethod");
#endif
      break;
    default:
      i = 0;
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "nonzero metamethod");
  }
  lua_pushboolean(L, i == a->size);
  return 1;
}


/* numarray.satisfy (f, a [, ···])
   With any numarray `a`, checks each element by calling function `f` which should return `true` or `false`. If at least
   one element in `a` does not satisfy the condition checked by `f`, the result is `false`, and `true` otherwise. 2.17.9 */

#define fn_checkresult(L,fn,procname,type,drop) { \
  if (!fn(L, -1)) { \
    int tt = lua_type(L, -1); \
    lua_pop(L, drop); \
    luaL_error(L, "Error in " LUA_QS ": procedure must evaluate to a %s, returned %s.", \
      procname, type, lua_typename(L, tt)); \
  } \
}

#define aux_satisfy(L,fn,ds) { \
  for (i=0; i < a->size; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "too many arguments"); \
    lua_pushvalue(L, 1); \
    fn(L, ds[i]); \
    for (j=3; j <= nargs; j++) lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    fn_checkresult(L, lua_isboolean, "numarray.satisfy", "boolean", 1); \
    if (agn_isfalse(L, -1)) return 1; \
  } \
}

static int numarray_satisfy (lua_State *L) {
  NumArray *a;
  int i, j, nargs;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": requires at least two arguments.", "numarray.satisfy");
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = checknumarray(L, 2);
  switch (a->datatype) {
    case NAUCHAR:   aux_satisfy(L, lua_pushnumber, a->data.c);  break;
    case NADOUBLE:  aux_satisfy(L, lua_pushnumber, a->data.n);  break;
    case NACDOUBLE: aux_satisfy(L, aux_pushcomplex, a->data.z); break;
    case NAINT32:   aux_satisfy(L, lua_pushnumber, a->data.i);  break;
    case NAUINT16:  aux_satisfy(L, lua_pushnumber, a->data.us); break;  /* 2.18.2 */
    case NAUINT32:  aux_satisfy(L, lua_pushnumber, a->data.ui); break;  /* 2.22.1 */
    case NAUINT64:  aux_satisfy(L, createuint64, a->data.ui64); break;  /* 6.2.3 */
    case NAINT64:   aux_satisfy(L, createint64, a->data.i64); break;  /* 6.2.3 */
#ifndef __ARMCPU  /* 2.37.1 */
    case NALDOUBLE: aux_satisfy(L, createdlong,    a->data.ld); break;  /* 2.35.0 */
#else
    case NALDOUBLE:
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.satisfy");
      break;
#endif
    default: luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.satisfy");
  }
  lua_pushtrue(L);
  return 1;
}


/* typenames; grep "(GREP_POINT) types;" if you want to change their order or add new ones */
static int numarray_isall (lua_State *L) {  /* 3.10.2 */
  int i;
  NumArray *a = checknumarray(L, 1);
  const char *type = agn_checkstring(L, 2);
  if (a->datatype != NADOUBLE)
    luaL_error(L, "Error in " LUA_QS ": only C doubles are supported.", "numarray.satisfy");
  if (tools_streq(type, llex_token2str(TK_INTEGER))) {
    for (i=0; i < a->size; i++) {
      if (tools_isfrac(a->data.n[i])) {
        lua_pushfalse(L);
        return 1;
      }
    }
  } else if (tools_streq(type, llex_token2str(TK_POSINT))) {
    for (i=0; i < a->size; i++) {
      if (!tools_isposint(a->data.n[i])) {
        lua_pushfalse(L);
        return 1;
      }
    }
  } else if (tools_streq(type, llex_token2str(TK_NONNEGINT))) {
    for (i=0; i < a->size; i++) {
      if (!tools_isnonnegint(a->data.n[i])) {
        lua_pushfalse(L);
        return 1;
      }
    }
  } else if (tools_streq(type, llex_token2str(TK_NONZEROINT))) {  /* 4.11.0 */
    for (i=0; i < a->size; i++) {
      if (!tools_isnonzeroint(a->data.n[i])) {
        lua_pushfalse(L);
        return 1;
      }
    }
  } else if (tools_streq(type, llex_token2str(TK_POSITIVE))) {
    for (i=0; i < a->size; i++) {
      if (!tools_ispositive(a->data.n[i])) {
        lua_pushfalse(L);
        return 1;
      }
    }
  } else if (tools_streq(type, llex_token2str(TK_NONNEGATIVE))) {
    for (i=0; i < a->size; i++) {
      if (!tools_isnonnegative(a->data.n[i])) {
        lua_pushfalse(L);
        return 1;
      }
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": unsupported type " LUA_QS ".", "numarray.isall", type);
  }
  lua_pushtrue(L);
  return 1;
}


/* numarray.map (f, a [, ···])
   Maps a function f on each element in the numarray `a` and returns a new numarray with the mapped results, i.e. the new
   array includes the values f(a[1]), f(a[2]), f(a[3]), etc. `f` must always return a number. See also: `numarray.convert`.
   2.17.9 */

#define aux_map(L,fn,check,convert,ds1,ds2) { \
  for (i=0; i < n; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "too many arguments"); \
    lua_pushvalue(L, 1); \
    fn(L, ds2[i]); \
    for (j=3; j <= nargs; j++) lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    fn_checkresult(L, check, "numarray.map", "number or longdouble", 1 + newstruct); \
    ds1[i] = convert(L, -1); \
    agn_poptop(L); \
  } \
}

#define cmplx_map(L,fn,ds1,ds2) { \
  for (i=0; i < n; i++) { \
    lua_Number re, im; \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "too many arguments"); \
    lua_pushvalue(L, 1); \
    fn(L, ds2[i]); \
    for (j=3; j <= nargs; j++) lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    if (!agn_getcmplxparts(L, -1, &re, &im)) { \
      int tt = lua_type(L, -1); \
      lua_pop(L, 1 + newstruct); \
      luaL_error(L, "Error in " LUA_QS ": procedure must evaluate to a (complex) number, returned %s.", \
        "numarray.map", lua_typename(L, tt)); \
    } \
    assigncdarray(ds1, i, re, im); \
    agn_poptop(L); \
  } \
}

#ifndef __ARMCPU
static int aux_isdlong (lua_State *L, int idx) {
  return luaL_isudata(L, idx, "longdouble");
}
#endif

static int aux_isints (lua_State *L, int idx) {
  return luaL_isudata(L, idx, "ints");
}

static int numarray_map (lua_State *L) {
  NumArray *a, *b;
  int i, j, newstruct, equality, nargs;
  char type;  /* 2.31.3 fix */
  size_t n;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": requires at least two arguments.", "numarray.map");
  luaL_checktype(L, 1, LUA_TFUNCTION);
  numarray_checkoptions(L, 3, &nargs, &newstruct, &equality, "numarray.map");
  b = checknumarray(L, 2);
  type = b->datatype;
  n = b->size;
  if (newstruct) {
    a = createarray(L, type, n, b->ndims, b->dims, "numarray.map");
    switch (b->datatype) {
      case NAUCHAR:  aux_map(L, lua_pushinteger, lua_isnumber, agn_tonumber, a->data.c,  b->data.c); break;
      case NADOUBLE: aux_map(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.n,  b->data.n); break;
      case NACDOUBLE: {
        cmplx_map(L, aux_pushcomplex, a->data.z, b->data.z);
        break;
      }
      case NAINT32:  aux_map(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.i,  b->data.i); break;
      case NAUINT16: aux_map(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.us, b->data.us); break;
      case NAUINT32: aux_map(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.ui, b->data.ui); break;
      case NAUINT64: aux_map(L, createuint64,    isints,       aux_getuint64, a->data.ui64, b->data.ui64); break;
      case NAINT64:  aux_map(L, createint64,     isints,       aux_getint64, a->data.i64, b->data.i64); break;
#ifndef __ARMCPU  /* 2.37.1 */
      case NALDOUBLE: aux_map(L, createdlong, aux_isdlong, getdlongvalue, a->data.ld, b->data.ld); break;
#else
      case NALDOUBLE:
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.map");
        break;
#endif
      default:
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.map");
    }  /* return new numarray */
  } else {  /* in-place operation, 2.31.3 */
    agnL_checkudfreezed(L, 2, "numarray", "numarray.map");  /* 4.9.0 */
    lua_pushvalue(L, 2);  /* push numarray for later return */
    switch (b->datatype) {
      case NAUCHAR:  aux_map(L, lua_pushinteger, lua_isnumber, agn_tonumber, b->data.c,  b->data.c); break;
      case NADOUBLE: aux_map(L, lua_pushnumber,  lua_isnumber, agn_tonumber, b->data.n,  b->data.n); break;
      case NACDOUBLE: {  /* 4.7.4 fix */
        cmplx_map(L, aux_pushcomplex, b->data.z, b->data.z);
        break;
      }
      case NAINT32:  aux_map(L, lua_pushnumber,  lua_isnumber, agn_tonumber, b->data.i,  b->data.i); break;
      case NAUINT16: aux_map(L, lua_pushnumber,  lua_isnumber, agn_tonumber, b->data.us, b->data.us); break;
      case NAUINT32: aux_map(L, lua_pushnumber,  lua_isnumber, agn_tonumber, b->data.ui, b->data.ui); break;
      case NAUINT64: aux_map(L, createuint64,    isints,       aux_getuint64, b->data.ui64, b->data.ui64); break;
      case NAINT64:  aux_map(L, createint64,     isints,       aux_getint64, b->data.i64, b->data.i64); break;
#ifndef __ARMCPU  /* 2.37.1 */
      case NALDOUBLE: aux_map(L, createdlong, aux_isdlong, getdlongvalue, b->data.ld, b->data.ld); break;
#else
      case NALDOUBLE:
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.map");
        break;
#endif
      default:
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.map");
    }  /* return existing numarray */
  }
  return 1;
}


#define aux_zip(L,luapush,check,convert,ds1,ds2,ds3) { \
  for (i=0; i < n; i++) { \
    luaL_checkstack(L, 3 + (nargs > 2)*(nargs - 2), "too many arguments"); \
    lua_pushvalue(L, 1); \
    luapush(L, ds2[i]); \
    luapush(L, ds3[i]); \
    for (j=4; j <= nargs; j++) lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    fn_checkresult(L, check, "numarray.zip", "number or longdouble", 1 + newstruct); \
    ds1[i] = convert(L, -1); \
    agn_poptop(L); \
  } \
}

#define cmplx_zip(L,luapush,ds1,ds2,ds3) { \
  for (i=0; i < n; i++) { \
    lua_Number re, im; \
    luaL_checkstack(L, 3 + (nargs > 2)*(nargs - 2), "too many arguments"); \
    lua_pushvalue(L, 1); \
    luapush(L, ds2[i]); \
    luapush(L, ds3[i]); \
    for (j=4; j <= nargs; j++) lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    if (!agn_getcmplxparts(L, -1, &re, &im)) { \
      int tt = lua_type(L, -1); \
      lua_pop(L, 1 + newstruct); \
      luaL_error(L, "Error in " LUA_QS ": procedure must evaluate to a (complex) number, returned %s.", \
        "numarray.zip", lua_typename(L, tt)); \
    } \
    assigncdarray(ds1, i, re, im); \
    agn_poptop(L); \
  } \
}

static int numarray_zip (lua_State *L) {  /* 4.2.1 */
  NumArray *a, *b, *c;
  int i, j, newstruct, equality, nargs;
  char type;  /* 2.31.3 fix */
  size_t n;
  nargs = lua_gettop(L);
  if (nargs < 3)
    luaL_error(L, "Error in " LUA_QS ": requires at least three arguments.", "numarray.zip");
  luaL_checktype(L, 1, LUA_TFUNCTION);
  numarray_checkoptions(L, 4, &nargs, &newstruct, &equality, "numarray.zip");
  b = checknumarray(L, 2);
  type = b->datatype;
  c = checknumarray(L, 3);
  n = b->size;
  if (n != c->size)
    luaL_error(L, "Error in " LUA_QS ": arrays must have the same size.", "numarray.zip");
  if (type != c->datatype)
    luaL_error(L, "Error in " LUA_QS ": arrays must be of the same kind.", "numarray.zip");
  if (newstruct) {
    a = createarray(L, type, n, b->ndims, b->dims, "numarray.zip");
    switch (b->datatype) {
      case NAUCHAR:  aux_zip(L, lua_pushinteger, lua_isnumber, agn_tonumber, a->data.c,  b->data.c,  c->data.c); break;
      case NADOUBLE: aux_zip(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.n,  b->data.n,  c->data.n); break;
      case NACDOUBLE:
        cmplx_zip(L, aux_pushcomplex, a->data.z,  b->data.z,  c->data.z);
        break;
      case NAINT32:  aux_zip(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.i,  b->data.i,  c->data.i); break;
      case NAUINT16: aux_zip(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.us, b->data.us, c->data.us); break;
      case NAUINT32: aux_zip(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.ui, b->data.ui, c->data.ui); break;
      case NAUINT64: aux_zip(L, createuint64,    isints,       aux_getuint64, a->data.ui64, b->data.ui64, c->data.ui64); break;
      case NAINT64:  aux_zip(L, createint64,     isints,       aux_getint64, a->data.i64, b->data.i64, c->data.i64); break;
#ifndef __ARMCPU  /* 2.37.1 */
      case NALDOUBLE: aux_zip(L, createdlong, aux_isdlong, getdlongvalue, a->data.ld, b->data.ld, c->data.ld); break;
#else
      case NALDOUBLE:
        agn_poptop(L);
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.zip");
        break;
#endif
      default: {
        agn_poptop(L);
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.zip");
      }
    }  /* return new numarray */
  } else {  /* in-place operation, 2.31.3 */
    agnL_checkudfreezed(L, 2, "numarray", "numarray.zip");  /* 4.9.0 */
    lua_pushvalue(L, 2);  /* push numarray for later return */
    switch (b->datatype) {
      case NAUCHAR:  aux_zip(L, lua_pushinteger, lua_isnumber, agn_tonumber, b->data.c,  b->data.c,  c->data.c); break;
      case NADOUBLE: aux_zip(L, lua_pushnumber,  lua_isnumber, agn_tonumber, b->data.n,  b->data.n,  c->data.n); break;
      case NACDOUBLE:
        cmplx_zip(L, aux_pushcomplex, b->data.z,  b->data.z,  c->data.z);
        break;
      case NAINT32:  aux_zip(L, lua_pushnumber,  lua_isnumber, agn_tonumber, b->data.i,  b->data.i,  c->data.i); break;
      case NAUINT16: aux_zip(L, lua_pushnumber,  lua_isnumber, agn_tonumber, b->data.us, b->data.us, c->data.us); break;
      case NAUINT32: aux_zip(L, lua_pushnumber,  lua_isnumber, agn_tonumber, b->data.ui, b->data.ui, c->data.ui); break;
      case NAUINT64: aux_zip(L, lua_pushnumber,  lua_isnumber, agn_tonumber, b->data.ui64, b->data.ui64, c->data.ui64); break;
      case NAINT64:  aux_zip(L, lua_pushnumber,  lua_isnumber, agn_tonumber, b->data.i64, b->data.i64, c->data.i64); break;
#ifndef __ARMCPU  /* 2.37.1 */
      case NALDOUBLE: aux_zip(L, createdlong, aux_isdlong, getdlongvalue, b->data.ld, b->data.ld, c->data.ld); break;
#else
      case NALDOUBLE:
        agn_poptop(L);
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.zip");
        break;
#endif
      default: {
        agn_poptop(L);
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.zip");
      }
    }  /* return existing numarray */
  }
  return 1;
}


#define aux_select(L,luapush,ds1,ds2) { \
  for (i=0; i < n; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "too many arguments"); \
    lua_pushvalue(L, 1); \
    luapush(L, ds2[i]); \
    for (j=3; j <= nargs; j++) lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    fn_checkresult(L, lua_isboolean, "numarray.select", "boolean", 1 + newstruct); \
    if (agn_istrue(L, -1)) ds1[c++] = ds2[i]; \
    agn_poptop(L); \
  } \
}

#define cmplx_select(L,luapush,ds1,ds2) { \
  for (i=0; i < n; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "too many arguments"); \
    lua_pushvalue(L, 1); \
    luapush(L, ds2[i]); \
    for (j=3; j <= nargs; j++) lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    fn_checkresult(L, lua_isboolean, "numarray.select", "boolean", 1 + newstruct); \
    if (agn_istrue(L, -1)) { copycdarray(ds1, c, ds2, i); } \
    c++; \
    agn_poptop(L); \
  } \
}

#define aux_resizeselect(L,ds,datatype,c,pn) { \
  if ((c) != 0) { \
    ds = realloc(ds, safenewsize((c), sizeof(datatype))); \
    if (ds == NULL) \
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", pn); \
  } \
}

static int numarray_select (lua_State *L) {  /* 2.31.3 */
  NumArray *a, *b;
  int i, j, c, nargs, newstruct, equality;
  char type;
  size_t n;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": requires at least two arguments.", "numarray.select");
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = NULL;
  b = checknumarray(L, 2);
  type = b->datatype;
  n = b->size;
  numarray_checkoptions(L, 3, &nargs, &newstruct, &equality, "numarray.select");
  c = 0;
  if (newstruct) {
    a = createarray(L, type, n, b->ndims, b->dims, "numarray.select");
    switch (b->datatype) {
      case NAUCHAR:
        aux_select(L, lua_pushinteger, a->data.c, b->data.c);
        aux_resizeselect(L, a->data.c, unsigned char, c, "numarray.select");
        break;
      case NADOUBLE:
        aux_select(L, lua_pushnumber, a->data.n, b->data.n);
        aux_resizeselect(L, a->data.n, lua_Number, c, "numarray.select");
        break;
      case NACDOUBLE:
        cmplx_select(L, aux_pushcomplex, a->data.z, b->data.z);
        aux_resizeselect(L, a->data.z, agn_Complex, c, "numarray.select");
        break;
      case NAINT32:
        aux_select(L, lua_pushnumber, a->data.i, b->data.i);
        aux_resizeselect(L, a->data.i, int32_t, c, "numarray.select");
        break;
      case NAUINT16:
        aux_select(L, lua_pushnumber, a->data.us, b->data.us);
        aux_resizeselect(L, a->data.us, uint16_t, c, "numarray.select");
        break;
      case NAUINT32:
        aux_select(L, lua_pushnumber, a->data.ui, b->data.ui);
        aux_resizeselect(L, a->data.ui, uint32_t, c, "numarray.select");  /* 2.35.0 patch */
        break;
      case NAUINT64:
        aux_select(L, createuint64, a->data.ui64, b->data.ui64);
        aux_resizeselect(L, a->data.ui64, uint64_t, c, "numarray.select");
        break;
      case NAINT64:
        aux_select(L, createint64, a->data.i64, b->data.i64);
        aux_resizeselect(L, a->data.i64, int64_t, c, "numarray.select");
        break;
      case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
        aux_select(L, createdlong, a->data.ld, b->data.ld);
        aux_resizeselect(L, a->data.ld, long double, c, "numarray.select");
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.select");
#endif
        break;
      default:
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.select");
    }  /* return new numarray or null */
  } else {
    agnL_checkudfreezed(L, 2, "numarray", "numarray.select");  /* 4.9.0 */
    lua_pushvalue(L, 2);  /* push numarray for later return */
    switch (b->datatype) {
      case NAUCHAR:
        aux_select(L, lua_pushinteger, b->data.c, b->data.c);
        aux_resizeselect(L, b->data.c, unsigned char, c, "numarray.select");
        break;
      case NADOUBLE:
        aux_select(L, lua_pushnumber, b->data.n, b->data.n);
        aux_resizeselect(L, b->data.n, lua_Number, c, "numarray.select");
        break;
      case NACDOUBLE:
        cmplx_select(L, aux_pushcomplex, b->data.z, b->data.z);
        aux_resizeselect(L, b->data.z, agn_Complex, c, "numarray.select");
        break;
      case NAINT32:
        aux_select(L, lua_pushnumber, b->data.i, b->data.i);
        aux_resizeselect(L, b->data.i, int32_t, c, "numarray.select");
        break;
      case NAUINT16:
        aux_select(L, lua_pushnumber, b->data.us, b->data.us);
        aux_resizeselect(L, b->data.us, uint16_t, c, "numarray.select");
        break;
      case NAUINT32:
        aux_select(L, lua_pushnumber, b->data.ui, b->data.ui);
        aux_resizeselect(L, b->data.ui, uint32_t, c, "numarray.select");  /* 2.35.0 patch */
        break;
      case NAUINT64:
        aux_select(L, createuint64, b->data.ui64, b->data.ui64);
        aux_resizeselect(L, b->data.ui64, uint64_t, c, "numarray.select");
        break;
      case NAINT64:
        aux_select(L, createint64, b->data.i64, b->data.i64);
        aux_resizeselect(L, b->data.i64, int64_t, c, "numarray.select");
        break;

      case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
        aux_select(L, createdlong, b->data.ld, b->data.ld);
        aux_resizeselect(L, b->data.ld, long double, c, "numarray.select");
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.select");
#endif
        break;
      default:
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.select");
    }  /* return numarray or null */
  }
  if (c == 0) {
    agn_poptop(L);
    lua_pushnil(L);
  } else if (newstruct)
    a->size = c;
  else
    b->size = c;
  return 1;
}


#define aux_remove(L,luapush,ds1,ds2) { \
  for (i=0; i < n; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "too many arguments"); \
    lua_pushvalue(L, 1); \
    luapush(L, ds2[i]); \
    for (j=3; j <= nargs; j++) lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    fn_checkresult(L, lua_isboolean, "numarray.remove", "boolean", 1 + newstruct); \
    if (agn_isfalse(L, -1)) ds1[c++] = ds2[i]; \
    agn_poptop(L); \
  } \
}

#define cmplx_remove(L,luapush,ds1,ds2) { \
  for (i=0; i < n; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "too many arguments"); \
    lua_pushvalue(L, 1); \
    luapush(L, ds2[i]); \
    for (j=3; j <= nargs; j++) lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    fn_checkresult(L, lua_isboolean, "numarray.remove", "boolean", 1 + newstruct); \
    if (agn_isfalse(L, -1)) { copycdarray(ds1, c, ds2, i); } \
    c++; \
    agn_poptop(L); \
  } \
}

static int numarray_remove (lua_State *L) {  /* 2.31.3 */
  NumArray *a, *b;
  int i, j, c, newstruct, equality, nargs;
  char type;
  size_t n;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": requires at least two arguments.", "numarray.remove");
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = NULL;
  b = checknumarray(L, 2);
  type = b->datatype;
  n = b->size;
  c = 0;
  numarray_checkoptions(L, 3, &nargs, &newstruct, &equality, "numarray.remove");
  if (newstruct) {
    a = createarray(L, type, n, b->ndims, b->dims, "numarray.remove");
    switch (b->datatype) {
      case NAUCHAR:
        aux_remove(L, lua_pushinteger, a->data.c, b->data.c);
        aux_resizeselect(L, a->data.c, unsigned char, c, "numarray.remove");
        break;
      case NADOUBLE:
        aux_remove(L, lua_pushnumber, a->data.n, b->data.n);
        aux_resizeselect(L, a->data.n, lua_Number, c, "numarray.remove");
        break;
      case NACDOUBLE:
        cmplx_remove(L, aux_pushcomplex, a->data.z, b->data.z);
        aux_resizeselect(L, a->data.z, agn_Complex, c, "numarray.remove");
        break;
      case NAINT32:
        aux_remove(L, lua_pushnumber, a->data.i, b->data.i);
        aux_resizeselect(L, a->data.i, int32_t, c, "numarray.remove");
        break;
      case NAUINT16:
        aux_remove(L, lua_pushnumber, a->data.us, b->data.us);
        aux_resizeselect(L, a->data.us, uint16_t, c, "numarray.remove");  /* 2.35.0 patch */
        break;
      case NAUINT32:
        aux_remove(L, lua_pushnumber, a->data.ui, b->data.ui);
        aux_resizeselect(L, a->data.ui, uint32_t, c, "numarray.remove");
        break;
      case NAUINT64:
        aux_remove(L, createuint64, a->data.ui64, b->data.ui64);
        aux_resizeselect(L, a->data.ui64, uint64_t, c, "numarray.remove");
        break;
      case NAINT64:
        aux_remove(L, createint64, a->data.i64, b->data.i64);
        aux_resizeselect(L, a->data.i64, int64_t, c, "numarray.remove");
        break;
#ifndef __ARMCPU  /* 2.37.1 */
      case NALDOUBLE:
        aux_remove(L, createdlong, a->data.ld, b->data.ld);
        aux_resizeselect(L, a->data.ld, long double, c, "numarray.remove");
        break;
#else
      case NALDOUBLE:
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.remove");
        break;
#endif
      default:
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.remove");
    }  /* return new numarray or null */
  } else {
    agnL_checkudfreezed(L, 2, "numarray", "numarray.remove");  /* 4.9.0 */
    lua_pushvalue(L, 2);  /* push numarray for later return */
    switch (b->datatype) {
      case NAUCHAR:
        aux_remove(L, lua_pushinteger, b->data.c, b->data.c);
        aux_resizeselect(L, b->data.c, unsigned char, c, "numarray.remove");
        break;
      case NADOUBLE:
        aux_remove(L, lua_pushnumber, b->data.n, b->data.n);
        aux_resizeselect(L, b->data.n, lua_Number, c, "numarray.remove");
        break;
      case NACDOUBLE:
        cmplx_remove(L, aux_pushcomplex, b->data.z, b->data.z);
        aux_resizeselect(L, a->data.z, agn_Complex, c, "numarray.remove");
        break;
      case NAINT32:
        aux_remove(L, lua_pushnumber, b->data.i, b->data.i);
        aux_resizeselect(L, b->data.i, int32_t, c, "numarray.remove");
        break;
      case NAUINT16:
        aux_remove(L, lua_pushnumber, b->data.us, b->data.us);
        aux_resizeselect(L, b->data.us, uint16_t, c, "numarray.remove");
        break;
      case NAUINT32:
        aux_remove(L, lua_pushnumber, b->data.ui, b->data.ui);
        aux_resizeselect(L, b->data.ui, uint32_t, c, "numarray.remove");  /* 2.35.0 patch */
        break;
      case NAUINT64:
        aux_remove(L, createuint64, b->data.ui64, b->data.ui64);
        aux_resizeselect(L, b->data.ui64, uint64_t, c, "numarray.remove");
        break;
      case NAINT64:
        aux_remove(L, createint64, b->data.i64, b->data.i64);
        aux_resizeselect(L, b->data.i64, int64_t, c, "numarray.remove");
        break;
      case NALDOUBLE:
#ifndef __ARMCPU  /* 2.37.1 */
        aux_remove(L, createdlong, b->data.ld, b->data.ld);
        aux_resizeselect(L, b->data.ld, long double, c, "numarray.remove");
#else
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.remove");
#endif
        break;
      default:
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.remove");
    }  /* return new numarray or null */
  }
  if (c == 0) {
    agn_poptop(L);
    lua_pushnil(L);
  } else if (newstruct)
    a->size = c;
  else
    b->size = c;
  return 1;
}


static int numarray_subs (lua_State *L) {
  NumArray *a, *b, *y;
  int i, j, eq, nargs, newstruct, equality;
  char type;
  size_t n;
  lua_Number p, q, x;
#ifndef __ARMCPU  /* 2.37.1 */
  long double pl, ql, xl;
  pl = ql = 0.0L;
#endif
#ifndef PROPCMPLX
  agn_Complex pz;
  pz = 0.0 + 0*I;
#else
  agn_Complex pz, qz;
  pz[0] = 0.0;
  pz[1] = 0.0;
#endif
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  numarray_checkoptions(L, 3, &nargs, &newstruct, &equality, "numarray.subs");
  for (i=1; i < nargs; i++)
    luaL_checktype(L, i, LUA_TPAIR);
  a = checknumarray(L, nargs);
  n = a->size;
  type = a->datatype;
  if (newstruct) {
    b = createarray(L, type, n, a->ndims, a->dims, "numarray.subs");
    y = b;
    aux_copy(L, b, a, "numarray.subs");
  } else {  /* push structure explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
    agnL_checkudfreezed(L, nargs, "numarray", "numarray.subs");  /* 4.9.0 */
    b = NULL; lua_pushvalue(L, nargs); y = a;
  }
  /* start traversal */
  for (i=0; i < n; i++) {  /* array */
    switch (type) {
      case NAUCHAR:  p = a->data.c[i];  break;
      case NADOUBLE: p = a->data.n[i];  break;
      case NAINT32:  p = a->data.i[i];  break;
      case NAUINT16: p = a->data.us[i]; break;
      case NAUINT32: p = a->data.ui[i]; break;
#ifndef __ARMCPU  /* 2.37.1 */
      case NALDOUBLE: pl = a->data.ld[i]; break;
#else
      case NALDOUBLE:
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.subs");
        break;
#endif
      case NACDOUBLE: {
#ifndef PROPCMPLX
        pz = a->data.z[i];
#else  /* 4.9.5 change */
        pz[0] = a->data.z[i][0];
        pz[1] = a->data.z[i][1];
#endif
        break;
      }
      case NAUINT64: case NAINT64:
        p = 0;
#ifndef PROPCMPLX
        pz = 0.0 + 0.0*I;
#else
        pz[0] = 0.0; pz[1] = 0;
#endif
        break;
      default:
        p = 0;
#ifndef PROPCMPLX
        pz = 0.0 + 0.0*I;
#else
        pz[0] = 0.0; pz[1] = 0;
#endif
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.subs");
    }
    if (type < NAUINT64) {  /* no long doubles, int64s, complex doubles ? */
      for (j=1; j < nargs; j++) {  /* subst list */
        if (!agn_pairgetinumbers(L, j, &q, &x))  /* 4.11.2 tweak */
          luaL_error(L, "Error in " LUA_QS ": non-pair at position %d or pair with non-numbers.", "numarray.subs", j);
        eq = (equality) ? p == q : tools_approx(p, q, L->Eps);
        if (eq) {
          switch (type) {
            case NAUCHAR:  y->data.c[i]  = x; break;
            case NADOUBLE: y->data.n[i]  = x; break;
            case NAINT32:  y->data.i[i]  = x; break;
            case NAUINT16: y->data.us[i] = x; break;
            case NAUINT32: y->data.ui[i] = x; break;
            default: {
              luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.subs");
            }
          }
          break;
        }  /* of equality check  */
      }
    } else if (type == NAUINT64) {  /* unsigned 64-bit integers, 6.3.1 fix */
      for (j=1; j < nargs; j++) {  /* subst list */
        Ints *p, *q;
        luaL_checkstack(L, 2, "not enough stack space");
        if (!lua_ispair(L, j))
          luaL_error(L, "Error in " LUA_QS ": expected a pair for argument %d.", "numarray.subs", j);
        agn_pairgeti(L, j, 1);
        if (!isints(L, -1)) {
          agn_poptop(L);
          luaL_error(L, "Error in " LUA_QS ": argument %d is pair with non-64-bit integers.", "numarray.subs", j);
        }
        p = (Ints *)lua_touserdata(L, -1);
        if (p->datatype != UINT64T) {
          agn_poptop(L);
          luaL_error(L, "Error in " LUA_QS ": argument %d is pair with signed 64-bit integer.", "numarray.subs", j);
        }
        agn_pairgeti(L, j, 2);
        if (!isints(L, -1)) {
          lua_pop(L, 2);
          luaL_error(L, "Error in " LUA_QS ": argument %d is pair with non-64-bit integers.", "numarray.subs", j);
        }
        q = (Ints *)lua_touserdata(L, -1);
        if (q->datatype != UINT64T) {
          lua_pop(L, 2);
          luaL_error(L, "Error in " LUA_QS ": argument %d is pair with signed 64-bit integer.", "numarray.subs", j);
        }
        if (a->data.ui64[i] == p->data.ui) y->data.ui64[i] = q->data.ui;
        lua_pop(L, 2);
      }
    } else if (type == NAINT64) {  /* signed 64-bit integers, 6.3.1 fix */
      for (j=1; j < nargs; j++) {  /* subst list */
        Ints *p, *q;
        luaL_checkstack(L, 2, "not enough stack space");
        if (!lua_ispair(L, j))
          luaL_error(L, "Error in " LUA_QS ": expected a pair for argument %d.", "numarray.subs", j);
        agn_pairgeti(L, j, 1);
        if (!isints(L, -1)) {
          agn_poptop(L);
          luaL_error(L, "Error in " LUA_QS ": argument %d is pair with non-64-bit integers.", "numarray.subs", j);
        }
        p = (Ints *)lua_touserdata(L, -1);
        if (p->datatype != INT64T) {
          agn_poptop(L);
          luaL_error(L, "Error in " LUA_QS ": argument %d is pair with unsigned 64-bit integer.", "numarray.subs", j);
        }
        agn_pairgeti(L, j, 2);
        if (!isints(L, -1)) {
          lua_pop(L, 2);
          luaL_error(L, "Error in " LUA_QS ": argument %d is pair with non-64-bit integers.", "numarray.subs", j);
        }
        q = (Ints *)lua_touserdata(L, -1);
        if (q->datatype != INT64T) {
          lua_pop(L, 2);
          luaL_error(L, "Error in " LUA_QS ": argument %d is pair with unsigned 64-bit integer.", "numarray.subs", j);
        }
        if (a->data.i64[i] == p->data.i) y->data.i64[i] = q->data.i;
        lua_pop(L, 2);
      }
    } else if (type == NALDOUBLE) {  /* long doubles */
#ifndef __ARMCPU  /* 2.37.1 */
      for (j=1; j < nargs; j++) {  /* subst list */
        agnL_pairgetilongnumbers(L, "numarray.subs", j, &ql, &xl);
        eq = (equality) ? pl == ql : tools_approxl(pl, ql, L->hEps);
        if (eq) {
          y->data.ld[i] = xl;
          break;
        }  /* of equality check  */
      }
#else
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.subs");
#endif
    } else {  /* complex double array, 4.9.4 */
      lua_Number pzreal, pzimag, qzreal, qzimag, xzreal, xzimag;
      pzreal = real(pz); pzimag = imag(pz);
      for (j=1; j < nargs; j++) {  /* subst list */
        agnL_pairgeticomplex(L, "numarray.subs", j, &qzreal, &qzimag, &xzreal, &xzimag);
        eq = (equality) ? pzreal == qzreal && pzimag == qzimag :
                          tools_capprox(pzreal, pzimag, qzreal, qzimag, L->hEps);
        if (eq) {
          assigncdarray(y->data.z, i, xzreal, xzimag);
          break;
        }  /* of equality check  */
      }
    }
  }
  return 1;  /* return numarray */
}


/* numarray.convert (f, a [, ···])
   Same as `numarray.map` but processes in-place: Maps a function f on each element in the numarray `a` and changes the entries accordingly,
   i.e. the array elements will be transformed from a[1] to f(a[1]), etc. `f` must always return a number. */

#define aux_convert(L,luapush,check,convert,ds) { \
  for (i=0; i < n; i++) { \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "too many arguments"); \
    lua_pushvalue(L, 1); \
    luapush(L, ds[i]); \
    for (j=3; j <= nargs; j++) lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    fn_checkresult(L, (check), "numarray.convert", "number, ints or longdouble", 1); \
    ds[i] = convert(L, -1); \
    agn_poptop(L); \
  } \
}


#define cmplx_convert(L,luapush,ds) { \
  for (i=0; i < n; i++) { \
    lua_Number re, im; \
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "too many arguments"); \
    lua_pushvalue(L, 1); \
    luapush(L, ds[i]); \
    for (j=3; j <= nargs; j++) lua_pushvalue(L, j); \
    lua_call(L, nargs - 1, 1); \
    if (!agn_getcmplxparts(L, -1, &re, &im)) { \
      int tt = lua_type(L, -1); \
      agn_poptop(L); \
      luaL_error(L, "Error in " LUA_QS ": procedure must evaluate to a (complex) number, returned %s.", \
        "numarray.convert", lua_typename(L, tt)); \
    } \
    assigncdarray(ds, i, re, im); \
    agn_poptop(L); \
  } \
}

static int numarray_convert (lua_State *L) {  /* 2.17.9 */
  NumArray *a;
  int i, j, nargs;
  size_t n;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": requires at least two arguments.", "numarray.convert");
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = checknumarray(L, 2);
  n = a->size;
  switch (a->datatype) {
    case NAUCHAR:   aux_convert(L, lua_pushinteger, lua_isnumber, agn_tonumber, a->data.c);  break;
    case NADOUBLE:  aux_convert(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.n);  break;
    case NACDOUBLE: cmplx_convert(L, aux_pushcomplex, a->data.z); break;  /* 4.9.5 change */
    case NAINT32:   aux_convert(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.i);  break;
    case NAUINT16:  aux_convert(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.us); break;
    case NAUINT32:  aux_convert(L, lua_pushnumber,  lua_isnumber, agn_tonumber, a->data.ui); break;
    case NAUINT64:  aux_convert(L, createuint64, aux_isints, getuint64value, a->data.ui64); break;
    case NAINT64:   aux_convert(L, createint64,  aux_isints, getuint64value, a->data.i64); break;
#ifndef __ARMCPU  /* 2.37.1 */
    case NALDOUBLE: aux_convert(L, createdlong, aux_isdlong, getdlongvalue, a->data.ld); break;
#else
    case NALDOUBLE:
      luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", "numarray.convert");
      break;
#endif
    default:
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.convert");
  }
  return 0;  /* return nothing */
}


static int numarray_checkarray (lua_State *L) {  /* 4.11.4 */
  int i, nargs, rc;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "not enough stack space");
  if (nargs > 1 && agn_isstring(L, nargs)) {
    const char *requestedtype, *actualtype;
    requestedtype = agn_tostring(L, nargs--);
    for (i=1; i <= nargs; i++) {
      rc = agn_getutype(L, i);
      if (rc) {  /* agn_getutype pushes nothing if no user-defined type has been set */
        actualtype = agn_tostring(L, -1);
        rc = isnumarray(L, i) && tools_streq(requestedtype, actualtype);
        if (!rc) agn_poptop(L);  /* prepare for exit: drop current type */
      }
      if (!rc) {
        lua_pop(L, i - 1);  /* pop previously pushed types */
        luaL_error(L, "Error in " LUA_QS ": argument #%d is not %s %s array.", "numarray.checkarray", i,
          tools_isvowel(requestedtype[0], 0) ? "an" : "a", requestedtype);
      }
    }
  } else {
    for (i=1; i <= nargs; i++) {
      if (!isnumarray(L, i)) {
        lua_pop(L, i - 1);  /* pop previously pushed types */
        luaL_error(L, "Error in " LUA_QS ": argument #%d is not a numarray.", "numarray.checkarray", i);
      }
      agn_getutype(L, i);
    }
  }
  return nargs;
}


static int numarray_isarray (lua_State *L) {  /* 4.11.4 */
  int i, nargs, rc;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "not enough stack space");
  if (nargs > 1 && agn_isstring(L, nargs)) {
    const char *requestedtype;
    requestedtype = agn_tostring(L, nargs--);
    for (i=1; i <= nargs; i++) {
      rc = agn_getutype(L, i);
      if (!rc) {  /* if rc = 0, agn_getutype pushes nothing */
        rc = 0;
      } else {
        rc = isnumarray(L, i) && tools_streq(requestedtype, agn_tostring(L, -1));
        agn_poptop(L);
      }
      lua_pushboolean(L, rc);
    }
  } else {
    for (i=1; i <= nargs; i++) {
      lua_pushboolean(L, isnumarray(L, i));
    }
  }
  return nargs;
}


/* CHECK the TESTCASES if you change this ! */
#define aux_zipop(L,n,op,ds1,ds2,ds3) { \
  int i; \
  for (i=0; i < n; i++) { \
    ds1[i] = ds2[i] op ds3[i]; \
  } \
}

/* CHECK the TESTCASES if you change this ! */
#define aux_zipunop(L,n,op,ds1,ds2) { \
  int i; \
  for (i=0; i < n; i++) { \
    ds1[i] = op(ds2[i]); \
  } \
}

/* CHECK the TESTCASES if you change this ! */

static void aux_checkbasicmath (lua_State *L, NumArray **b, NumArray **c, int *newstruct, const char *procname) {
  int nargs, equality;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": requires at least two arguments.", procname);
  luaL_checkstack(L, nargs, "too many arguments");
  numarray_checkoptions(L, 3, &nargs, newstruct, &equality, procname);
  *b = checknumarray(L, 1);
  if (!(*newstruct) && agn_udfreeze(L, 1, -1))  /* 4.9.0 */
    luaL_error(L, "Error in " LUA_QS ": numarray is read-only.", procname);
  *c = checknumarray(L, 2);
  if ((*b)->size != (*c)->size)
    luaL_error(L, "Error in " LUA_QS ": arrays must have the same size.", procname);
  if ((*b)->datatype != (*c)->datatype)
    luaL_error(L, "Error in " LUA_QS ": arrays must be of the same kind.", procname);
}

/* CHECK the TESTCASES if you change this ! */
#ifndef PROPCMPLX
#define aux_dobasicmath(L,fn,newstruct,b,c,pn) { \
  if (newstruct) { \
    NumArray *a = createarray(L, b->datatype, b->size, b->ndims, b->dims, pn); \
    switch (b->datatype) { \
      case NAUCHAR:  aux_zipop(L, b->size, fn,  a->data.c,  b->data.c,  c->data.c);  break; \
      case NADOUBLE: aux_zipop(L, b->size, fn,  a->data.n,  b->data.n,  c->data.n);  break; \
      case NACDOUBLE: aux_zipop(L, b->size, fn, a->data.z,  b->data.z,  c->data.z);  break; \
      case NAINT32:  aux_zipop(L, b->size, fn,  a->data.i,  b->data.i,  c->data.i);  break; \
      case NAUINT16: aux_zipop(L, b->size, fn,  a->data.us, b->data.us, c->data.us); break; \
      case NAUINT32: aux_zipop(L, b->size, fn,  a->data.ui, b->data.ui, c->data.ui); break; \
      case NAUINT64: aux_zipop(L, b->size, fn,  a->data.ui64, b->data.ui64, c->data.ui64); break; \
      case NAINT64:  aux_zipop(L, b->size, fn,  a->data.i64, b->data.i64, c->data.i64); break; \
      case NALDOUBLE: aux_zipop(L, b->size, fn, a->data.ld, b->data.ld, c->data.ld); break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } else { \
    lua_pushvalue(L, 1); \
    switch (b->datatype) { \
      case NAUCHAR:  aux_zipop(L,  b->size, fn, b->data.c,  b->data.c,  c->data.c);  break; \
      case NADOUBLE: aux_zipop(L,  b->size, fn, b->data.n,  b->data.n,  c->data.n);  break; \
      case NACDOUBLE: aux_zipop(L, b->size, fn, b->data.z,  b->data.z,  c->data.z);  break; \
      case NAINT32:  aux_zipop(L,  b->size, fn, b->data.i,  b->data.i,  c->data.i);  break; \
      case NAUINT16: aux_zipop(L,  b->size, fn, b->data.us, b->data.us, c->data.us); break; \
      case NAUINT32: aux_zipop(L,  b->size, fn, b->data.ui, b->data.ui, c->data.ui); break; \
      case NAUINT64: aux_zipop(L,  b->size, fn, b->data.ui64, b->data.ui64, c->data.ui64); break; \
      case NAINT64:  aux_zipop(L,  b->size, fn, b->data.i64, b->data.i64, c->data.i64); break; \
      case NALDOUBLE: aux_zipop(L, b->size, fn, b->data.ld, b->data.ld, c->data.ld); break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } \
}

/* CHECK the TESTCASES if you change this ! */
#define aux_dobasicmathARM(L,fn,newstruct,b,c,pn) { \
  if (newstruct) { \
    NumArray *a = createarray(L, b->datatype, b->size, b->ndims, b->dims, pn); \
    switch (b->datatype) { \
      case NAUCHAR:  aux_zipop(L,  b->size, fn, a->data.c,  b->data.c,  c->data.c);  break; \
      case NADOUBLE: aux_zipop(L,  b->size, fn, a->data.n,  b->data.n,  c->data.n);  break; \
      case NACDOUBLE: aux_zipop(L, b->size, fn, a->data.z,  b->data.z,  c->data.z);  break; \
      case NAINT32:  aux_zipop(L,  b->size, fn, a->data.i,  b->data.i,  c->data.i);  break; \
      case NAUINT16: aux_zipop(L,  b->size, fn, a->data.us, b->data.us, c->data.us); break; \
      case NAUINT32: aux_zipop(L,  b->size, fn, a->data.ui, b->data.ui, c->data.ui); break; \
      case NAUINT64: aux_zipop(L,  b->size, fn, a->data.ui64, b->data.ui64, c->data.ui64); break; \
      case NAINT64:  aux_zipop(L,  b->size, fn, a->data.i64, b->data.i64, c->data.i64); break; \
      case NALDOUBLE: \
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", pn); \
        break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } else { \
    lua_pushvalue(L, 1); \
    switch (b->datatype) { \
      case NAUCHAR:  aux_zipop(L,  b->size, fn, b->data.c,  b->data.c,  c->data.c);  break; \
      case NADOUBLE: aux_zipop(L,  b->size, fn, b->data.n,  b->data.n,  c->data.n);  break; \
      case NACDOUBLE: aux_zipop(L, b->size, fn, b->data.z,  b->data.z,  c->data.z);  break; \
      case NAINT32:  aux_zipop(L,  b->size, fn, b->data.i,  b->data.i,  c->data.i);  break; \
      case NAUINT16: aux_zipop(L,  b->size, fn, b->data.us, b->data.us, c->data.us); break; \
      case NAUINT32: aux_zipop(L,  b->size, fn, b->data.ui, b->data.ui, c->data.ui); break; \
      case NAUINT64: aux_zipop(L,  b->size, fn, b->data.ui64, b->data.ui64, c->data.ui64); break; \
      case NAINT64:  aux_zipop(L,  b->size, fn, b->data.i64, b->data.i64, c->data.i64); break; \
      case NALDOUBLE: \
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", pn); \
        break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } \
}
#else
#define aux_dobasicmath(L,fn,newstruct,b,c,pn) { \
  if (newstruct) { \
    NumArray *a = createarray(L, b->datatype, b->size, b->ndims, b->dims, pn); \
    switch (b->datatype) { \
      case NAUCHAR:  aux_zipop(L,  b->size, fn, a->data.c,  b->data.c,  c->data.c);  break; \
      case NADOUBLE: aux_zipop(L,  b->size, fn, a->data.n,  b->data.n,  c->data.n);  break; \
      case NAINT32:  aux_zipop(L,  b->size, fn, a->data.i,  b->data.i,  c->data.i);  break; \
      case NAUINT16: aux_zipop(L,  b->size, fn, a->data.us, b->data.us, c->data.us); break; \
      case NAUINT32: aux_zipop(L,  b->size, fn, a->data.ui, b->data.ui, c->data.ui); break; \
      case NAUINT64: aux_zipop(L,  b->size, fn, a->data.ui64, b->data.ui64, c->data.ui64); break; \
      case NAINT64:  aux_zipop(L,  b->size, fn, a->data.i64, b->data.i64, c->data.i64); break; \
      case NALDOUBLE: aux_zipop(L, b->size, fn, a->data.ld, b->data.ld, c->data.ld); break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } else { \
    lua_pushvalue(L, 1); \
    switch (b->datatype) { \
      case NAUCHAR:  aux_zipop(L,  b->size, fn, b->data.c,  b->data.c,  c->data.c);  break; \
      case NADOUBLE: aux_zipop(L,  b->size, fn, b->data.n,  b->data.n,  c->data.n);  break; \
      case NAINT32:  aux_zipop(L,  b->size, fn, b->data.i,  b->data.i,  c->data.i);  break; \
      case NAUINT16: aux_zipop(L,  b->size, fn, b->data.us, b->data.us, c->data.us); break; \
      case NAUINT32: aux_zipop(L,  b->size, fn, b->data.ui, b->data.ui, c->data.ui); break; \
      case NAUINT64: aux_zipop(L,  b->size, fn, b->data.ui64, b->data.ui64, c->data.ui64); break; \
      case NAINT64:  aux_zipop(L,  b->size, fn, b->data.i64, b->data.i64, c->data.i64); break; \
      case NALDOUBLE: aux_zipop(L, b->size, fn, b->data.ld, b->data.ld, c->data.ld); break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } \
}

/* CHECK the TESTCASES if you change this ! */
#define aux_dobasicmathARM(L,fn,newstruct,b,c,pn) { \
  if (newstruct) { \
    NumArray *a = createarray(L, b->datatype, b->size, b->ndims, b->dims, pn); \
    switch (b->datatype) { \
      case NAUCHAR:  aux_zipop(L, b->size, fn, a->data.c,  b->data.c,  c->data.c);  break; \
      case NADOUBLE: aux_zipop(L, b->size, fn, a->data.n,  b->data.n,  c->data.n);  break; \
      case NAINT32:  aux_zipop(L, b->size, fn, a->data.i,  b->data.i,  c->data.i);  break; \
      case NAUINT16: aux_zipop(L, b->size, fn, a->data.us, b->data.us, c->data.us); break; \
      case NAUINT32: aux_zipop(L, b->size, fn, a->data.ui, b->data.ui, c->data.ui); break; \
      case NAUINT64: aux_zipop(L, b->size, fn, a->data.ui64, b->data.ui64, c->data.ui64); break; \
      case NAINT64:  aux_zipop(L, b->size, fn, a->data.i64, b->data.i64, c->data.i64); break; \
      case NALDOUBLE: \
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", pn); \
        break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } else { \
    lua_pushvalue(L, 1); \
    switch (b->datatype) { \
      case NAUCHAR:  aux_zipop(L, b->size, fn, b->data.c,  b->data.c,  c->data.c);  break; \
      case NADOUBLE: aux_zipop(L, b->size, fn, b->data.n,  b->data.n,  c->data.n);  break; \
      case NAINT32:  aux_zipop(L, b->size, fn, b->data.i,  b->data.i,  c->data.i);  break; \
      case NAUINT16: aux_zipop(L, b->size, fn, b->data.us, b->data.us, c->data.us); break; \
      case NAUINT32: aux_zipop(L, b->size, fn, b->data.ui, b->data.ui, c->data.ui); break; \
      case NAUINT64: aux_zipop(L, b->size, fn, b->data.ui64, b->data.ui64, c->data.ui64); break; \
      case NAINT64:  aux_zipop(L, b->size, fn, b->data.i64, b->data.i64, c->data.i64); break; \
      case NALDOUBLE: \
        luaL_error(L, "Error in " LUA_QS ": long doubles are not supported on ARM platforms.", pn); \
        break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } \
}
#endif

static int numarray_xadd (lua_State *L) {  /* 4.8.0 */
  NumArray *b, *c;
  int newstruct;
  aux_checkbasicmath(L, &b, &c, &newstruct, "numarray.xadd");
#ifndef __ARMCPU
  aux_dobasicmath(L, +, newstruct, b, c, "numarray.xadd");
#else
  aux_dobasicmathARM(L, +, newstruct, b, c, "numarray.xadd");
#endif
  return 1;
}


static int numarray_xsub (lua_State *L) {  /* 4.8.0 */
  NumArray *b, *c;
  int newstruct;
  aux_checkbasicmath(L, &b, &c, &newstruct, "numarray.xsub");
#ifndef __ARMCPU
  aux_dobasicmath(L, -, newstruct, b, c, "numarray.xsub");
#else
  aux_dobasicmathARM(L, -, newstruct, b, c, "numarray.xsub");
#endif
  return 1;
}


static int numarray_xmul (lua_State *L) {  /* 4.8.0 */
  NumArray *b, *c;
  int newstruct;
  aux_checkbasicmath(L, &b, &c, &newstruct, "numarray.xmul");
#ifndef __ARMCPU
  aux_dobasicmath(L, *, newstruct, b, c, "numarray.xmul");
#else
  aux_dobasicmathARM(L, *, newstruct, b, c, "numarray.xmul");
#endif
  return 1;
}


static int numarray_xdiv (lua_State *L) {  /* 4.8.0 */
  NumArray *b, *c;
  int newstruct;
  aux_checkbasicmath(L, &b, &c, &newstruct, "numarray.xdiv");
#ifndef __ARMCPU
  aux_dobasicmath(L, /, newstruct, b, c, "numarray.xdiv");
#else
  aux_dobasicmathARM(L, /, newstruct, b, c, "numarray.xdiv");
#endif
  return 1;
}


/* CHECK the TESTCASES if you change this ! */

static void aux_checkbargs (lua_State *L, NumArray **b, NumArray **c, int *newstruct, const char *procname) {
  int nargs, equality;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": requires at least two arguments.", procname);
  luaL_checkstack(L, nargs, "too many arguments");
  numarray_checkoptions(L, 3, &nargs, newstruct, &equality, procname);
  *b = checknumarray(L, 1);
  if (!(*newstruct) && agn_udfreeze(L, 1, -1))  /* 4.9.0 */
    luaL_error(L, "Error in " LUA_QS ": numarray is read-only.", procname);
  *c = checknumarray(L, 2);
  if ((*b)->size != (*c)->size)
    luaL_error(L, "Error in " LUA_QS ": arrays must have the same size.", procname);
  if ((*b)->datatype != (*c)->datatype)
    luaL_error(L, "Error in " LUA_QS ": arrays must be of the same kind.", procname);
  if (!((*b)->datatype == NAUCHAR || (*b)->datatype == NAUINT16 || (*b)->datatype == NAUINT32))
    luaL_error(L, "Error in " LUA_QS ": need unsigned integer arrays.", procname);
}


/* CHECK the TESTCASES if you change this ! */
#define aux_dobinop(L,fn,newstruct,b,c,pn) { \
  if (newstruct) { \
    NumArray *a = createarray(L, b->datatype, b->size, b->ndims, b->dims, pn); \
    switch (b->datatype) { \
      case NAUCHAR:  aux_zipop(L, b->size, fn, a->data.c,  b->data.c,  c->data.c);  break; \
      case NAUINT16: aux_zipop(L, b->size, fn, a->data.us, b->data.us, c->data.us); break; \
      case NAUINT32: aux_zipop(L, b->size, fn, a->data.ui, b->data.ui, c->data.ui); break; \
      case NAUINT64: aux_zipop(L, b->size, fn, a->data.ui64, b->data.ui64, c->data.ui64); break; \
      case NAINT64:  aux_zipop(L, b->size, fn, a->data.i64, b->data.i64, c->data.i64); break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } else { \
    lua_pushvalue(L, 1); \
    switch (b->datatype) { \
      case NAUCHAR:  aux_zipop(L, b->size, fn, b->data.c,  b->data.c,  c->data.c);  break; \
      case NAUINT16: aux_zipop(L, b->size, fn, b->data.us, b->data.us, c->data.us); break; \
      case NAUINT32: aux_zipop(L, b->size, fn, b->data.ui, b->data.ui, c->data.ui); break; \
      case NAUINT64: aux_zipop(L, b->size, fn, b->data.ui64, b->data.ui64, c->data.ui64); break; \
      case NAINT64:  aux_zipop(L, b->size, fn, b->data.i64, b->data.i64, c->data.i64); break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } \
}

static int numarray_band (lua_State *L) {  /* 4.8.0 */
  NumArray *b, *c;
  int newstruct;
  aux_checkbargs(L, &b, &c, &newstruct, "numarray.band");
  aux_dobinop(L, &, newstruct, b, c, "numarray.band")
  return 1;
}


static int numarray_bor (lua_State *L) {  /* 4.8.0 */
  NumArray *b, *c;
  int newstruct;
  aux_checkbargs(L, &b, &c, &newstruct, "numarray.bor");
  aux_dobinop(L, |, newstruct, b, c, "numarray.bor")
  return 1;
}


static int numarray_bxor (lua_State *L) {  /* 4.8.0 */
  NumArray *b, *c;
  int newstruct;
  aux_checkbargs(L, &b, &c, &newstruct, "numarray.bxor");
  aux_dobinop(L, ^, newstruct, b, c, "numarray.bxor")
  return 1;
}


static int numarray_bshl (lua_State *L) {  /* 4.8.0 */
  NumArray *b, *c;
  int newstruct;
  aux_checkbargs(L, &b, &c, &newstruct, "numarray.bshl");
  aux_dobinop(L, <<, newstruct, b, c, "numarray.bshl")
  return 1;
}


static int numarray_bshr (lua_State *L) {  /* 4.8.0 */
  NumArray *b, *c;
  int newstruct;
  aux_checkbargs(L, &b, &c, &newstruct, "numarray.bshr");
  aux_dobinop(L, >>, newstruct, b, c, "numarray.bshr")
  return 1;
}


static int numarray_bnot (lua_State *L) {  /* 4.8.0 */
  NumArray *a, *b;
  int newstruct, equality, nargs;
  size_t n;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": requires at least one argument.", "numarray.bnot");
  luaL_checkstack(L, nargs, "too many arguments");
  numarray_checkoptions(L, 2, &nargs, &newstruct, &equality, "numarray.bnot");
  b = checknumarray(L, 1);
  n = b->size;
  if (!(b->datatype == NAUCHAR || b->datatype == NAUINT16 || b->datatype == NAUINT32))
    luaL_error(L, "Error in " LUA_QS ": need unsigned integer arrays.", "numarray.bnot");
  if (newstruct) {
    a = createarray(L, b->datatype, n, b->ndims, b->dims, "numarray.bnot");
    switch (b->datatype) {
      case NAUCHAR:  aux_zipunop(L, n, ~, a->data.c,  b->data.c);  break;
      case NAUINT16: aux_zipunop(L, n, ~, a->data.us, b->data.us); break;
      case NAUINT32: aux_zipunop(L, n, ~, a->data.ui, b->data.ui); break;
      case NAUINT64: aux_zipunop(L, n, ~, a->data.ui64, b->data.ui64); break;
      case NAINT64:  aux_zipunop(L, n, ~, a->data.i64, b->data.i64); break;
      default:
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.bnot");
    }  /* return new numarray */
  } else {  /* in-place operation */
    agnL_checkudfreezed(L, 1, "numarray", "numarray.bnot");  /* 4.9.0 */
    lua_pushvalue(L, 1);  /* push numarray for later return */
    switch (b->datatype) {
      case NAUCHAR:  aux_zipunop(L, n, ~, b->data.c,  b->data.c);  break;
      case NAUINT16: aux_zipunop(L, n, ~, b->data.us, b->data.us); break;
      case NAUINT32: aux_zipunop(L, n, ~, b->data.ui, b->data.ui); break;
      case NAUINT64: aux_zipunop(L, n, ~, b->data.ui64, b->data.ui64); break;
      case NAINT64:  aux_zipunop(L, n, ~, b->data.i64, b->data.i64); break;
      default:
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", "numarray.bnot");
    }  /* return existing numarray */
  }
  return 1;
}


/* CHECK the TESTCASES if you change this ! */

static NumArray *aux_checkargs (lua_State *L, int *newstruct, const char *procname) {  /* 4.8.1 */
  int equality, nargs;
  NumArray *b = NULL;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": requires at least one argument.", procname);
  luaL_checkstack(L, nargs, "too many arguments");
  numarray_checkoptions(L, 2, &nargs, newstruct, &equality, procname);
  b = checknumarray(L, 1);
  if (!(*newstruct) && agn_udfreeze(L, 1, -1))  /* 4.9.0 */
    luaL_error(L, "Error in " LUA_QS ": numarray is read-only.", procname);
  if (b->datatype != NADOUBLE && b->datatype != NACDOUBLE)
    luaL_error(L, "Error in " LUA_QS ": need a (complex) double array.", procname);
  return b;
}

/* CHECK the TESTCASES if you change this ! */

#define aux_dounimath(L,fn,newstruct,b,pn) { \
  if (newstruct) { \
    NumArray *a = createarray(L, b->datatype, b->size, b->ndims, b->dims, pn); \
    switch (b->datatype) { \
      case NADOUBLE: aux_zipunop(L, b->size, fn, a->data.n, b->data.n); break; \
      case NACDOUBLE: luaL_error(L, "Error in " LUA_QS ": complex double arrays are not supported.", pn); break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } else { \
    lua_pushvalue(L, 1); \
    switch (b->datatype) { \
      case NADOUBLE: aux_zipunop(L, b->size, fn, b->data.n, b->data.n); break; \
      case NACDOUBLE: luaL_error(L, "Error in " LUA_QS ": complex double arrays are not supported.", pn); break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } \
}

#define aux_dounimathx(L,fn,cfn,newstruct,b,pn) { \
  if (newstruct) { \
    NumArray *a = createarray(L, b->datatype, b->size, b->ndims, b->dims, pn); \
    switch (b->datatype) { \
      case NADOUBLE:  aux_zipunop(L, b->size, fn, a->data.n, b->data.n); break; \
      case NACDOUBLE: aux_zipunop(L, b->size, cfn, a->data.z, b->data.z); break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } else { \
    lua_pushvalue(L, 1); \
    switch (b->datatype) { \
      case NADOUBLE:  aux_zipunop(L, b->size, fn, b->data.n, b->data.n); break; \
      case NACDOUBLE: aux_zipunop(L, b->size, cfn, b->data.z, b->data.z); break; \
      default: \
        luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } \
}

#define aux_recip(x) (1/(x))
static int numarray_xrecip (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xrecip");
#ifdef PROPCMPLX
  aux_dounimath(L, aux_recip, newstruct, b, "numarray.xrecip");
#else
  aux_dounimathx(L, aux_recip, aux_recip, newstruct, b, "numarray.xrecip");
#endif
  return 1;
}


static int numarray_xln (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xln");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_log, newstruct, b, "numarray.xln");
#else
  aux_dounimathx(L, sun_log, tools_clog, newstruct, b, "numarray.xln");
#endif
  return 1;
}


static int numarray_xexp (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xexp");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_exp, newstruct, b, "numarray.xexp");
#else
  aux_dounimathx(L, sun_exp, tools_cexp, newstruct, b, "numarray.xexp");
#endif
  return 1;
}


#ifndef PROPCMPLX
static agn_Complex aux_csin (agn_Complex z) {
  return (agnc_sin(z));
}

static agn_Complex aux_ccos (agn_Complex z) {
  return (agnc_cos(z));
}

static agn_Complex aux_csinh (agn_Complex z) {
  return (agnc_sinh(z));
}

static agn_Complex aux_ccosh (agn_Complex z) {
  return (agnc_cosh(z));
}

static agn_Complex aux_cantilog2 (agn_Complex z) {
  return (agnc_antilog2(z));
}

static agn_Complex aux_clog2 (agn_Complex z) {
  return (tools_clog(z)*INVLN2);
}
#endif


static int numarray_xlog2 (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xlog2");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_log2, newstruct, b, "numarray.xlog2");  /* 4.9.4 */
#else
  aux_dounimathx(L, sun_log2, aux_clog2, newstruct, b, "numarray.xlog2");
#endif
  return 1;
}


static int numarray_xantilog2 (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xantilog2");
#ifdef PROPCMPLX
  aux_dounimath(L, luai_numantilog2, newstruct, b, "numarray.xantilog2");  /* 4.9.4 */
#else
  aux_dounimathx(L, luai_numantilog2, aux_cantilog2, newstruct, b, "numarray.xantilog2");
#endif
  return 1;
}


static int numarray_xsqrt (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xsqrt");
#ifdef PROPCMPLX
  aux_dounimath(L, sqrt, newstruct, b, "numarray.xsqrt");
#else
  aux_dounimathx(L, sqrt, tools_csqrt, newstruct, b, "numarray.xsqrt");
#endif
  return 1;
}


#define aux_square(x) ((x)*(x))
static int numarray_xsquare (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xsquare");
#ifdef PROPCMPLX
  aux_dounimath(L, aux_square, newstruct, b, "numarray.xsquare");
#else
  aux_dounimathx(L, aux_square, aux_square, newstruct, b, "numarray.xsquare");
#endif
  return 1;
}


static int numarray_xsin (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xsin");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_sin, newstruct, b, "numarray.xsin");
#else
  aux_dounimathx(L, sun_sin, aux_csin, newstruct, b, "numarray.xsin");
#endif
  return 1;
}


static int numarray_xcos (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xcos");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_cos, newstruct, b, "numarray.xcos");
#else
  aux_dounimathx(L, sun_cos, aux_ccos, newstruct, b, "numarray.xcos");
#endif
  return 1;
}


static int numarray_xtan (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xtan");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_tan, newstruct, b, "numarray.xtan");
#else
  aux_dounimathx(L, sun_tan, tools_ctan, newstruct, b, "numarray.xtan");
#endif
  return 1;
}


static int numarray_xsinh (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xsinh");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_sinh, newstruct, b, "numarray.xsinh");
#else
  aux_dounimathx(L, sun_sinh, aux_csinh, newstruct, b, "numarray.xsinh");
#endif
  return 1;
}


static int numarray_xcosh (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xcosh");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_cosh, newstruct, b, "numarray.xcosh");
#else
  aux_dounimathx(L, sun_cosh, aux_ccosh, newstruct, b, "numarray.xcosh");
#endif
  return 1;
}


static int numarray_xtanh (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xtanh");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_tanh, newstruct, b, "numarray.xtanh");
#else
  aux_dounimathx(L, sun_tanh, tools_ctanh, newstruct, b, "numarray.xtanh");
#endif
  return 1;
}


static int numarray_xarcsin (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xarcsin");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_asin, newstruct, b, "numarray.xarcsin");
#else
  aux_dounimathx(L, sun_asin, tools_casin, newstruct, b, "numarray.xarcsin");
#endif
  return 1;
}


static int numarray_xarccos (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xarccos");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_acos, newstruct, b, "numarray.xarccos");
#else
  aux_dounimathx(L, sun_acos, tools_cacos, newstruct, b, "numarray.xarccos");
#endif
  return 1;
}


static int numarray_xarctan (lua_State *L) {  /* 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.xarctan");
#ifdef PROPCMPLX
  aux_dounimath(L, sun_atan, newstruct, b, "numarray.xarctan");
#else
  aux_dounimathx(L, sun_atan, tools_catan, newstruct, b, "numarray.xarctan");
#endif
  return 1;
}


#define fahren(x) (((x)*9.0/5.0) + 32.0)
static int numarray_fahren (lua_State *L) {  /* from deg C to deg F, 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.fahren");
  aux_dounimath(L, fahren, newstruct, b, "numarray.fahren");
  return 1;
}


#define celsius(x) (((x) - 32.0)*5.0/9.0)
static int numarray_celsius (lua_State *L) {  /* from deg F to deg C, 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.celsius");
  aux_dounimath(L, celsius, newstruct, b, "numarray.celsius");
  return 1;
}


#define mile(x) (x/1.6093440006)
static int numarray_mile (lua_State *L) {  /* from kilometers to statute miles, 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.mile");
  aux_dounimath(L, mile, newstruct, b, "numarray.mile");
  return 1;
}


#define km(x) (x*1.6093440006)
static int numarray_km (lua_State *L) {  /* from statute miles to kilometers, 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.km");
  aux_dounimath(L, km, newstruct, b, "numarray.km");
  return 1;
}


#define gram(x) (x*28.349523125)
static int numarray_gram (lua_State *L) {  /* from [International] avoirdupois ounces to grams, 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.gram");
  aux_dounimath(L, gram, newstruct, b, "numarray.gram");
  return 1;
}


#define ounce(x) (x/28.349523125)
static int numarray_ounce (lua_State *L) {  /* from grams to [International] avoirdupois ounces, 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.ounce");
  aux_dounimath(L, ounce, newstruct, b, "numarray.ounce");
  return 1;
}


#define floz(x) (x*33.8140227018429971686)
static int numarray_floz (lua_State *L) {  /* from litres to US fluid ounces, 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.floz");
  aux_dounimath(L, floz, newstruct, b, "numarray.floz");
  return 1;
}


#define litre(x) (x*0.0295735295625)
static int numarray_litre (lua_State *L) {  /* from US fluid ounces to litres, 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.litre");
  aux_dounimath(L, litre, newstruct, b, "numarray.litre");
  return 1;
}


#define gallon(x) (x/3.785411784)
static int numarray_gallon (lua_State *L) {  /* from litres to US liquid gallons, 4.8.0 */
  int newstruct;
  NumArray *b = aux_checkargs(L, &newstruct, "numarray.gallon");
  aux_dounimath(L, gallon, newstruct, b, "numarray.gallon");
  return 1;
}


static const struct luaL_Reg numarray_arraylib [] = {  /* metamethods for numeric userdata `n` */
  {"__aeq",       mt_aeq},              /* approximate equality mt */
  {"__eq",        mt_eeq},              /* equality mt */
  {"__eeq",       mt_eeq},              /* strict equality mt */
  {"__call",      mt_call},             /* call mt for access to the registry table */
  {"__index",     numarray_subarray},   /* n[p], with p the index, counting from 1, NO support of n[p to q] indexing since the VM can only return one value */
  {"__writeindex", numarray_setitem},   /* n[p] := value, with p the index, counting from 1 */
  {"__tostring",  mt_array2string},     /* for output at the console, e.g. print(n) */
  {"__size",      mt_getsize},          /* metamethod for `size` operator */
  {"__empty",     mt_empty},            /* metamethod for `empty` operator */
  {"__filled",    mt_filled},           /* metamethod for `filled` operator */
  {"__zero",      mt_zero},             /* metamethod for `zero` operator */
  {"__nonzero",   mt_nonzero},          /* metamethod for `nonzero` operator */
  {"__gc",        mt_arraygc},          /* garbage collection */
  {"__in",        mt_in},               /* metamethod for `in` operator */
  {"__notin",     mt_notin},            /* metamethod for `notin` operator */
  {"__sumup",     mt_sumup},            /* metamethod for `sumup` operator */
  {"__qsumup",    mt_qsumup},           /* metamethod for `qsumup` operator */
  {"__mulup",     mt_mulup},            /* metamethod for `mulup` operator */
  {"__qmdev",     mt_qmdev},            /* metamethod for `qmdev` operator */
  {"__intersect", mt_intersect},        /* metamethod for `intersect` operator */
  {"__minus",     mt_minus},            /* metamethod for `minus` operator */
  {"__union",     mt_union},            /* metamethod for `union` operator */
  {"__add",       numarray_xadd},       /* metamethod for `+` operator */
  {"__sub",       numarray_xsub},       /* metamethod for `-` operator */
  {"__mul",       numarray_xmul},       /* metamethod for `*` operator */
  {"__div",       numarray_xdiv},       /* metamethod for `/` operator */
  {"__recip",     numarray_xrecip},     /* metamethod for `recip` operator */
  {"__sqrt",      numarray_xsqrt},      /* metamethod for `sqrt` operator */
  {"__square",    numarray_xsquare},    /* metamethod for `square` operator */
  {"__ln",        numarray_xln},        /* metamethod for `ln` operator */
  {"__exp",       numarray_xexp},       /* metamethod for `exp` operator */
  {"__antilog2",  numarray_xantilog2},  /* metamethod for `antilog2` operator */
  {"__arccos",    numarray_xarccos},    /* metamethod for `arccos` operator */
  {"__arcsin",    numarray_xarcsin},    /* metamethod for `arcsin` operator */
  {"__arctan",    numarray_xarctan},    /* metamethod for `arctan` operator */
  {"__cos",       numarray_xcos},       /* metamethod for `cos` operator */
  {"__sin",       numarray_xsin},       /* metamethod for `sin` operator */
  {"__tan",       numarray_xtan},       /* metamethod for `tan` operator */
  {"__cosh",      numarray_xcosh},      /* metamethod for `cosh` operator */
  {"__sinh",      numarray_xsinh},      /* metamethod for `sinh` operator */
  {"__tanh",      numarray_xtanh},      /* metamethod for `tanh` operator */
  {"__band",      numarray_band},       /* metamethod for `&&` operator */
  {"__bnot",      numarray_bnot},       /* metamethod for `~~` operator */
  {"__bor",       numarray_bor},        /* metamethod for `||` operator */
  {"__bxor",      numarray_bxor},       /* metamethod for `^^` operator */
  {"__bshl",      numarray_bshl},       /* metamethod for `<<` operator */
  {"__bshr",      numarray_bshr},       /* metamethod for `>>` operator */
  {"__iter",      numarray_iterate},    /* iterator */
  {NULL, NULL}
};


static const luaL_Reg numarraylib[] = {
  {"append",     numarray_append},
  {"attrib",     numarray_attrib},
  {"band",       numarray_band},
  {"bnot",       numarray_bnot},
  {"bor",        numarray_bor},
  {"bxor",       numarray_bxor},
  {"bshl",       numarray_bshl},
  {"bshr",       numarray_bshr},
  {"cdouble",    numarray_cdouble},
  {"checkarray", numarray_checkarray},
  {"convert",    numarray_convert},
  {"countitems", numarray_countitems},
  {"cycle",      numarray_cycle},
  {"double",     numarray_double},
  {"find",       numarray_find},
  {"geti",       numarray_geti},
  {"getitem",    numarray_getitem},
  {"getparts",   numarray_getparts},
  {"getbit",     numarray_getbit},
  {"getsize",    mt_getsize},
  {"include",    numarray_include},
  {"int32",      numarray_int32},
  {"int64",      numarray_int64},
  {"introsort",  numarray_introsort},
  {"isall",      numarray_isall},
  {"isarray",    numarray_isarray},
  {"iterate",    numarray_iterate},
  {"longdouble", numarray_longdouble},
  {"map",        numarray_map},
  {"member",     numarray_member},
  {"minmax",     numarray_minmax},
  {"one",        numarray_one},
  {"prepend",    numarray_prepend},
  {"purge",      numarray_purge},
  {"read",       numarray_read},
  {"redim",      numarray_redim},
  {"remove",     numarray_remove},
  {"replicate",  numarray_replicate},
  {"resize",     numarray_resize},
  {"satisfy",    numarray_satisfy},
  {"select",     numarray_select},
  {"setbit",     numarray_setbit},
  {"seti",       numarray_seti},
  {"setitem",    numarray_setitem},
  {"setparts",   numarray_setparts},
  {"sorted",     numarray_sorted},
  {"subs",       numarray_subs},
  {"subarray",   numarray_subarray},
  {"toarray",    numarray_toarray},
  {"toreg",      numarray_toreg},
  {"toseq",      numarray_toseq},
  {"totable",    numarray_totable},
  {"uchar",      numarray_uchar},
  {"uint32",     numarray_uint32},
  {"uint64",     numarray_uint64},
  {"unique",     numarray_unique},
  {"ushort",     numarray_ushort},
  {"used",       numarray_used},
  {"whereis",    numarray_whereis},
  {"write",      numarray_write},
  {"xadd",       numarray_xadd},
  {"xantilog2",  numarray_xantilog2},
  {"xarccos",    numarray_xarccos},
  {"xarcsin",    numarray_xarcsin},
  {"xarctan",    numarray_xarctan},
  {"xcos",       numarray_xcos},
  {"xcosh",      numarray_xcosh},
  {"xdiv",       numarray_xdiv},
  {"xexp",       numarray_xexp},
  {"xln",        numarray_xln},
  {"xlog2",      numarray_xlog2},
  {"xmul",       numarray_xmul},
  {"xrecip",     numarray_xrecip},
  {"xsin",       numarray_xsin},
  {"xsinh",      numarray_xsinh},
  {"xsquare",    numarray_xsquare},
  {"xsqrt",      numarray_xsqrt},
  {"xsub",       numarray_xsub},
  {"xtan",       numarray_xtan},
  {"xtanh",      numarray_xtanh},
  {"zip",        numarray_zip},
  {"mile",       numarray_mile},
  {"km",         numarray_km},
  {"fahren",     numarray_fahren},
  {"celsius",    numarray_celsius},
  {"gram",       numarray_gram},
  {"ounce",      numarray_ounce},
  {"floz",       numarray_floz},
  {"litre",      numarray_litre},
  {"gallon",     numarray_gallon},
  {NULL, NULL}
};


/*
** Open numarray library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_NUMARRAYLIBNAME);  /* create metatable */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, numarray_arraylib);  /* methods */
}

LUALIB_API int luaopen_numarray (lua_State *L) {
  createmeta(L);
  luaL_register(L, AGENA_NUMARRAYLIBNAME, numarraylib);
  return 1;
}

