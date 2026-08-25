/*
** $Id: linalg.c, initiated September 04, 2008 $
** Linear Algebra library
** See Copyright Notice in agena.h
*/

#include <stdlib.h>
#include <math.h>
#include <float.h>  /* for DBL_EPSILON */
#include <stdio.h>
#include <string.h>

#define linalg_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"   /* for trunc in Linux */
#include "agnhlps.h"   /* for isfloat */
#include "agnxlib.h"
#include "lcomplex.h"  /* for agnCmplx_create */
#include "linalg.h"
#include "long.h"
#include "mapm.h"

/************************************************************************************************************************************/
/* Auxiliary functions                                                                                                              */
/************************************************************************************************************************************/

/* checks whether the two arguments are vectors of the same dimension, modified Agena 1.4.3/1.5.0 */
#define checkvectors(L,a,b,p) { \
  if (!(agn_istableutype(L, (a), "vector") && agn_istableutype(L, (b), "vector")) ) \
    luaL_error(L, "Error in " LUA_QS ": two vectors expected.", (p)); \
  luaL_checkstack(L, 1, "not enough stack space"); \
  lua_getfield(L, (a), "dim"); \
  sizea = agn_checknumber(L, -1); \
  agn_poptop(L); \
  lua_getfield(L, (b), "dim"); \
  sizeb = agn_checknumber(L, -1); \
  agn_poptop(L); \
  if (sizea != sizeb) \
    luaL_error(L, "Error in " LUA_QS ": got vectors of different size.", p); \
}


/* checks whether the argument is a vector represented by a table, modified Agena 1.4.3/1.5.0; checking metatables is 80 % slower */
#define checkvector(L,a,p) { \
  if (!(agn_istableutype(L, (a), "vector")) ) \
    luaL_error(L, "Error in " LUA_QS ": vector expected, got %s.", (p), luaL_typename(L, (a))); \
  luaL_checkstack(L, 1, "not enough stack space"); \
  lua_getfield(L, (a), "dim"); \
  size = agn_checknumber(L, -1); \
  agn_poptop(L); \
}


static FORCE_INLINE int checkVector (lua_State *L, int idx, const char *procname) {  /* 2.1.4, proper error message added */
  int size;
  checkvector(L, idx, procname);
  return size;
}


/* Checks whether an object at stack index idx is an Agena matrix.

   If retdims is not 0, assigns its dimensions to p and q, and also puts the dimension pair onto the stack. It returns an error
   if issquare is not 0 and the dimensions are not the same.

   If retdims is 0, the the function does not change the stack. */

static void linalg_auxcheckmatrix (lua_State *L, int idx, int retdims, int issquare, const char *procname, int *p, int *q) {
  if (!agn_istableutype(L, idx, "matrix"))
    luaL_error(L, "Error in " LUA_QS ": matrix expected, got %s.", procname, luaL_typename(L, idx));  /* Agena 1.8.1 */
  else if (retdims) {
    luaL_checkstack(L, 2, "not enough stack space");  /* 3.16.5 fix */
    lua_getfield(L, idx, "dim");  /* pushes dimensions onto the stack */
    if (!lua_ispair(L, -1)) {
      agn_poptop(L);  /* 3.16.5 fix */
      luaL_error(L, "Error in " LUA_QS ": invalid matrix received, missing dimensions.", procname);  /* Agena 1.8.1 */
    }
    agn_pairgeti(L, -1, 1);
    *p = agn_checkinteger(L, -1);
    agn_poptop(L);  /* pop left value, 3.17.5 change */
    agn_pairgeti(L, -1, 2);
    *q = agn_checkinteger(L, -1);
    agn_poptop(L);  /* pop right value, 3.17.5 change */
    if (issquare && ( *p != *q ))
      luaL_error(L, "Error in " LUA_QS ": square matrix expected.", procname);  /* Agena 1.8.1 */
  }
}


/* Checks whether the matrix at stack index idx has a `dim' field and returns the row and column dimension in p and q.
   The function leaves the stack unchanged. */
static void linalg_auxcheckmatrixlight (lua_State *L, int idx, int *p, int *q, const char *procname) {  /* 3.17.4 */
  luaL_checkstack(L, 3, "not enough stack space");
  lua_getfield(L, idx, "dim");  /* pushes dimensions onto the stack */
  if (!lua_ispair(L, -1)) {
    agn_poptop(L);  /* 3.16.5 fix */
    luaL_error(L, "Error in " LUA_QS ": invalid matrix received, missing dimensions.", procname);  /* Agena 1.8.1 */
  }
  agn_pairgeti(L, -1, 1);
  *p = agn_checkinteger(L, -1);
  agn_pairgeti(L, -2, 2);
  *q = agn_checkinteger(L, -1);
  lua_pop(L, 3);  /* pop left and right value and field `dim' */
}


/* Set dimensions of a matrix. The matrix must be at the top of the stack.
   Optimised 2.1.3, 2.34.1; 3.17.5 security fix */
#define setmatrixdims(L, a, b) { \
  luaL_checkstack(L, 2, "not enough stack space"); \
  lua_pushstring(L, "dim"); \
  agn_createpairnumbers(L, (a), (b)); \
  lua_rawset(L, -3); \
}


/* Set dimensions of a vector. The vector must be at the top of the stack. 3.17.5 security fix */
#define setvectordim(L, a) { \
  luaL_checkstack(L, 2, "not enough stack space"); \
  lua_pushstring(L, "dim"); \
  lua_pushnumber(L, a); \
  lua_rawset(L, -3); \
}


/* Set metatable to a vector or matrix. o either is the string "vmt" or "mmt".
   The vector must be at the top of the stack. 3.17.5 security fix */
#define setmetatable(L, o) { \
  luaL_checkstack(L, 2, "not enough stack space"); \
  lua_getglobal(L, "linalg"); \
  lua_getfield(L, -1, o); \
  lua_setmetatable(L, -3); \
  agn_poptop(L); \
}


/* set vector attributes: user-defined type, metatable, and dimension to a vector
   which must be at the top of the stack */
#define setvattribs(L, a) { \
  agn_setutypestring(L, -1, "vector"); \
  setmetatable(L, "vmt"); \
  setvectordim(L, (a)); \
}


/* set matrix attributes: user-defined type, metatable, and dimensions to a matrix
   which must be at the top of the stack */
#define setmattribs(L, a, b) { \
  agn_setutypestring(L, -1, "matrix"); \
  setmetatable(L, "mmt"); \
  setmatrixdims(L, (a), (b)); \
}


#define createarrayld(a, n, procname) { \
  if ((n) < 1) \
    luaL_error(L, "Error in " LUA_QS ": table or sequence with at least one entry expected.", (procname)); \
  (a) = malloc((n)*sizeof(long double)); \
  if ((a) == NULL) \
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", (procname)); \
}


/* expects an m x n C matrix array and puts an Agena matrix at the top of the stack. Creates a sparse matrix
   if at least one element in the C array is zero. */
static int creatematrix (lua_State *L, lua_Number *a, int m, int n, int sparse) {
  /* create new matrix with the elements of C `matrix` a */
  int i, j;
  lua_Number item;
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 security fix */
  lua_createtable(L, m, 1);
  for (i=0; i < m; i++) {
    lua_createtable(L, n, 1);  /* create new vector */
    for (j=0; j < n; j++) {
      item = a[i*n + j];
      if (!sparse || (sparse && item != 0.0)) {  /* create sparse matrix if requested */
        agn_setinumber(L, -1, j + 1, (item == -0.0) ? 0.0 : item);  /* 4.1.1 change */
      }
    }
    setvattribs(L, n);
    lua_rawseti(L, -2, i + 1);
  }
  setmattribs(L, m, n);
  return 1;
}


static int creatematrixld (lua_State *L, long double *a, int m, int n) {
  /* create new matrix with the elements of C `matrix` a */
  int i, j;
  long double item;
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.16.5 fix */
  lua_createtable(L, m, 1);
  for (i=0; i < m; i++) {
    lua_createtable(L, n, 1);  /* create new vector */
    for (j=0; j < n; j++) {
      item = a[i*n + j];
      if (item != 0.0L) agn_setinumber(L, -1, j + 1, item);  /* create sparse matrix if possible */
    }
    setvattribs(L, n);
    lua_rawseti(L, -2, i + 1);
  }
  setmattribs(L, m, n);
  return 1;
}


static int createrawmatrix (lua_State *L, int m, int n) {
  /* create new sparse Agena matrix with preallocated row vectors */
  int i;
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.16.5 fix */
  lua_createtable(L, m, 1);
  for (i=0; i < m; i++) {
    lua_createtable(L, n, 1);    /* create new row vector */
    setvattribs(L, n);
    lua_rawseti(L, -2, i + 1);   /* and set it to new matrix */
  }
  setmattribs(L, m, n);
  return 1;
}


/* expects a vector `a' of dimension n and puts an Agena vector at the top of the stack. 2.1.3 */
int createvector (lua_State *L, double *a, int n) {
  /* create new vector with the elements of C array a */
  int i;
  luaL_checkstack(L, 1, "not enough stack space");  /* 3.17.5 security fix */
  lua_createtable(L, n, 1);
  for (i=0; i < n; i++) {
    if (a[i] != 0) agn_setinumber(L, -1, i + 1, a[i]);  /* 2.1.4: create sparse vector if possible */
  }
  setvattribs(L, n);
  return 1;
}


/* expects a vector of dimension n and puts an Agena vector at the top of the stack. 2.1.3 */
static int createvectorld (lua_State *L, long double *a, int n) {
  /* create new vector with the elements of C array a */
  int i;
  luaL_checkstack(L, 1, "not enough stack space");  /* 3.17.5 security fix */
  lua_createtable(L, n, 1);
  for (i=0; i < n; i++) {
    if (a[i] != 0.0L) agn_setinumber(L, -1, i + 1, a[i]);  /* 2.1.4: create sparse vector if possible */
  }
  setvattribs(L, n);
  return 1;
}


/* expects an m x n-Agena matrix of n-dimensional row vectors at index idx and creates a C matrix array a. The
   function leaves the stack untouched. 2.1.3
   gaps: allows to prefill #gaps columns at the right end of the m x n matrix with zeros even if the row vector
   has less elements than needed. */
static int fillmatrix (lua_State *L, int idx, lua_Number *a, int m, int n, int gaps, const char *procname) {
  int i, j;
  for (i=0; i < m; i++) {
    lua_rawgeti(L, idx, i + 1);  /* push row vector on stack */
    if (checkVector(L, -1, procname) + gaps != n) {
      /* row vector has wrong dimension; 7.3.4 fix */
      return 1;
    }
    for (j=0; j < n; j++)
      a[i*n + j] = agn_getinumber(L, -1, j + 1);  /* with non-numbers, sets zero */
    agn_poptop(L);  /* pop row vector */
  }
  return 0;
}


/* expects an n-dimensional Agena row vector at index idx and creates a C array a. The function leaves
   the stack untouched. long double version. */
int fillmatrixld (lua_State *L, int idx, long double *a, int m, int n, int gaps, const char *procname) {
  int i, j;
  luaL_checkstack(L, 2, "not enough stack space");
  for (i=0; i < m; i++) {
    lua_rawgeti(L, idx, i + 1);  /* push row vector on stack */
    if (checkVector(L, -1, procname) + gaps != n) {
      /* row vector has wrong dimension; 7.3.4 fix */
      return 1;
    }
    for (j=0; j < n; j++) {  /* extended 7.3.10 to assign long double */
      lua_rawgeti(L, -1, j + 1);  /* do not call lua_geti */
      if (agn_isnumber(L, -1)) {
        a[i*n + j] = agn_tonumber(L, -1);
      } else if (isdlong(L, -1)) {
        a[i*n + j] = getdlongvalue(L, -1);
      } else {  /* with non-numbers, sets zero */
        a[i*n + j] = 0.0L;
      }
      agn_poptop(L);  /* pop row vector element */
    }
    agn_poptop(L);  /* pop row vector */
  }
  return 0;
}


/* expects an n-dimensional Agena row vector at index idx and creates a C array a. The function leaves
   the stack untouched. long double version. */
static void fillvectorld (lua_State *L, int idx, long double *a, int n, const char *procname) {
  int i;
  if (checkVector(L, idx, procname) != n)
    luaL_error(L, "Error in " LUA_QS ": vector has wrong dimension.", procname);
  for (i=0; i < n; i++) {
    lua_rawgeti(L, idx, i + 1);  /* do not call lua_geti */
    if (agn_isnumber(L, -1)) {
      a[i] = agn_tonumber(L, -1);
    } else if (isdlong(L, -1)) {
      a[i] = getdlongvalue(L, -1);
    } else {  /* with non-numbers, sets zero */
      a[i] = 0.0L;
    }
    agn_poptop(L);
  }
}


/* Check options for linalg.extend and linalg.countitems */
void linalg_aux_fcheckoptions (lua_State *L, int pos, int *nargs, int *inplace, int *approx, const char *procname) {
  int checkoptions;
  *inplace = 0;  /* 0 = return a new structure, do not work in-place */
  *approx = 0;   /* 0 = strict equality check, 1 = approximate equality check */
  checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("inplace", option)) {
        *inplace = agn_checkboolean(L, -1);
      } else if (tools_streq("approx", option)) {
        *approx = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
  /* lhf, May 29, 2011 at 12:20 at
     https://stackoverflow.com/questions/6167555/how-can-i-safely-iterate-a-lua-table-while-keys-are-being-removed
     "You can safely remove entries while traversing a table but you cannot create new entries, that is, new keys.
      You can modify the values of existing entries, though. (Removing an entry being a special case of that rule.)" */
  /* if (*multret && !(*newstruct))
    luaL_error(L, "Error in " LUA_QS ": illegal combination of " LUA_QS " and " LUA_QS " option.", procname, "multret", "inplace"); */
}


/* Check options for linalg.vzero, linalg.unitvector, linalg.inverse 4.1.1 */
static void aux_checkvmoptions (lua_State *L, int pos, int *nargs, int *sparse, int *columnv, const char *procname) {
  int checkoptions;
  *sparse = 1;       /* 1 = create sparse vector */
  *columnv = 0;      /* 1 = return n x 1 matrix for column vector, 0 = return row vector */
  /* check for options, here `map in-place` */
  checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 fix */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "sparse")) {
        *sparse = agn_checkboolean(L, -1);
      } else if (tools_streq(option, "column")) {
        *columnv = agn_checkboolean(L, -1);
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


static int aux_getdims (lua_State *L, int nargs, int *m, int *n, const char *procname) {
  int nrets = 1;
  if (lua_ispair(L, 1)) {
    luaL_checkstack(L, 2, "not enough stack space");
    agn_pairgeti(L, 1, 1);
    *m = agn_checkinteger(L, -1);
    agn_pairgeti(L, 1, 2);
    *n = agn_checkinteger(L, -1);
    agn_poptoptwo(L);
  } else if (agn_istableutype(L, 1, "matrix")) {
    luaL_checkstack(L, 3, "not enough stack space");
    lua_getfield(L, 1, "dim");  /* pushes dimensions onto the stack */
    if (!lua_ispair(L, -1)) {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": invalid matrix received, missing dimensions.", procname);
    }
    agn_pairgeti(L, -1, 1);
    *m = agn_checkinteger(L, -1);
    agn_pairgeti(L, -2, 2);
    *n = agn_checkinteger(L, -1);
    lua_pop(L, 3);  /* pop left and right value and `dim'  field */
  } else {
    *m = agn_checkinteger(L, 1);
    *n = agnL_optinteger(L, 2, *m);
    nrets = 1 + (nargs > 1);
  }
  return nrets;
}


/* Checks whether the matrix at stack index idx is in (reduced) row echelon form. Returns 1 if so and 0 otherwise.
   rref = 0: check for row echelon form, rref = 1: check for reduced row echelon form, 4.0.1 */
static int aux_isef (lua_State *L, int idx, int rref, const char *procname) {
  int i, j, k, p, q, rc, flag, l, max, previous;
  lua_Number x;
  p = q = l = max = previous = 0;
  flag = 1;
  x = AGN_NAN;
  linalg_auxcheckmatrix(L, idx, 1, 0, procname, &p, &q);
  agn_poptop(L);
  for (i=0; i < p; i++) {  /* for each row vector */
    luaL_checkstack(L, 1, "not enough stack space");  /* better sure than sorry */
    rc = 0;
    lua_rawgeti(L, idx, i + 1);  /* push row vector of original matrix */
    checkVector(L, -1, procname);
    /* is row vector a zero vector ? */
    for (j=0; j < q && !rc; j++) {
      x = agn_getinumber(L, -1, j + 1);
      rc = (x != 0.0);
    }
    agn_poptop(L);  /* pop row vector */
    l = rc*j;  /* index of the first non-zero element in non-zero row vector - if it exists */
    if (rref && flag && l > max && x == 1.0) {  /* we have a possible pivot */
      int c = 0;
      max = l;
      /* the pivot is the only nonzero entry of its column; now fetch column of possible pivot
         and check whether the rest of the elements in there are zeros only. */
      for (k=0; k < p; k++) {
        lua_rawgeti(L, idx, k + 1);  /* push row vector of original matrix */
        c += (agn_getinumber(L, -1, l) == 0.0);
        agn_poptop(L);
      }
      if (c != p - 1) {
        return 0;
      }
    } else if (!rref && flag && l > max) {  /* we have a possible pivot */
      max = l;
    } else if (flag && l == 0 && previous > 0) {
      /* we have a zero vector but the previous one was non-zero */
      flag = 0;  /* now all the following rows must be zero */
    } else if (!(!flag && l == 0 && previous == 0)) {
      /* we do not (have a zero vector and the previous one was zero, too) */
      return 0;
    }
    previous = l;
  }
  return 1;
}


/* n must be the total number of the elements in the matrix */
static FORCE_INLINE int aux_isintegral (lua_Number *a, int n) {
  int i;
  for (i=0; i < n; i++) {
    if (tools_isfrac(a[i])) return 0;
  }
  return 1;
}


static FORCE_INLINE int aux_isintegrall (long double *a, int n) {
  int i;
  for (i=0; i < n; i++) {
    if (tools_isfrac((double)a[i])) return 0;
  }
  return 1;
}


/************************************************************************************************************************************/
/* Following are the linalg library functions                                                                                       */
/************************************************************************************************************************************/

/* Add two vectors. The result is a new vector. Tuned 2.1.3 */
static int linalg_add (lua_State *L) {
  int i, sizea, sizeb;
  lua_Number val;
  checkvectors(L, 1, 2, "linalg.add");
  lua_createtable(L, sizea, 1);
  /* now traverse vectors */
  for (i=1; i <= sizea; i++) {
    val = agn_getinumber(L, 1, i) + agn_getinumber(L, 2, i);  /* 3.20.2: be as sparse as possible */
    if (val != 0.0) agn_setinumber(L, -1, i, val);  /* store result to new sequence */
  }
  /* set attributes */
  setvattribs(L, sizea);
  return 1;
}


/* Subtract two vectors. The result is a new vector. Tuned 2.1.3 */
static int linalg_sub (lua_State *L) {
  int i, sizea, sizeb;
  lua_Number val;
  checkvectors(L, 1, 2, "linalg.sub");
  lua_createtable(L, sizea, 1);
  /* now traverse vectors */
  for (i=1; i <= sizea; i++) {
    val = agn_getinumber(L, 1, i) - agn_getinumber(L, 2, i);  /* 3.20.2: be as sparse as possible */
    if (val != 0.0) agn_setinumber(L, -1, i, val);  /* store result to new sequence */
  }
  /* set attributes */
  setvattribs(L, sizea);
  return 1;
}


static int linalg_scalarmul (lua_State *L) {  /* tuned & extended 2.1.3 */
  int i, size, nidx, vidx, rc;
  lua_Number a, v, n;
  if (agn_isnumber(L, 1) && agn_istableutype(L, 2, "vector")) {
    nidx = 1; vidx = 2;
  } else if (agn_istableutype(L, 1, "vector") && agn_isnumber(L, 2)) {
    vidx = 1; nidx = 2;
  } else {
    nidx = vidx = 0;  /* to avoid compiler warnings */
    luaL_error(L, "Error in " LUA_QS ": number and vector expected.", "linalg.scalarmul");
  }
  n = agn_tonumber(L, nidx);  /* 7.1.3 fix */
  lua_getfield(L, vidx, "dim");
  size = agn_checkinteger(L, -1);  /* Agena 1.4.3/1.5.0 */
  agn_poptop(L);
  lua_createtable(L, size, 1);
  /* now traverse vector */
  for (i=0; i < size; i++) {
    a = agn_rawgetinumber(L, vidx, i + 1, &rc);
    v = n*a;
    if (rc && v != 0.0) {  /* 3.20.2 change to preserve sparseness, 7.1.3 change */
      agn_setinumber(L, -1, i + 1, v);  /* store result to new vector */
    }
  }
  /* set attributes */
  setvattribs(L, size);
  return 1;
}


static int linalg_scalardiv (lua_State *L) {  /* 7.1.3 */
  int i, size, rc;
  lua_Number a, v, n;
  if (agn_istableutype(L, 1, "vector") && agn_isnumber(L, 2)) {
    n = agn_tonumber(L, 2);
    lua_getfield(L, 1, "dim");
    size = agn_checkinteger(L, -1);
    agn_poptop(L);
    lua_createtable(L, size, 1);
    /* now traverse vector */
    for (i=0; i < size; i++) {
      a = agn_rawgetinumber(L, 1, i + 1, &rc);
      v = a/n;
      if (rc && v != 0.0) {  /* preserve sparseness */
        agn_setinumber(L, -1, i + 1, v);  /* store result to new vector */
      }
    }
  } else if (agn_isnumber(L, 1) && agn_istableutype(L, 2, "vector")) {  /* 7.1.5 */
    n = agn_tonumber(L, 1);
    lua_getfield(L, 2, "dim");
    size = agn_checkinteger(L, -1);
    agn_poptop(L);
    lua_createtable(L, size, 1);
    /* now traverse vector */
    for (i=0; i < size; i++) {
      a = agn_rawgetinumber(L, 2, i + 1, &rc);
      v = n/a;
      if (rc && v != 0.0) {  /* preserve sparseness */
        agn_setinumber(L, -1, i + 1, v);  /* store result to new vector */
      }
    }
  } else {
    size = 0;
    luaL_error(L, "Error in " LUA_QS ": vector and number expected.", "linalg.scalardiv");
  }
  /* set attributes */
  setvattribs(L, size);
  return 1;
}


static int linalg_mscalarmul (lua_State *L) {  /* 3.18.2, UNDOC */
  int i, j, m, n, matidx;
  double *a, c;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  if (agn_istableutype(L, 1, "matrix") && agn_isnumber(L, 2)) {
    linalg_auxcheckmatrixlight(L, 1, &m, &n, "linalg.mscalarmul");  /* m = rowdim, n = coldim */
    c = agn_tonumber(L, 2);
    matidx = 1;
  } else if (agn_isnumber(L, 1) && agn_istableutype(L, 2, "matrix")) {
    c = agn_tonumber(L, 1);
    linalg_auxcheckmatrixlight(L, 2, &m, &n, "linalg.mscalarmul");  /* m = rowdim, n = coldim */
    matidx = 2;
  } else {
    c = 0.0L;
    matidx = 0;
    luaL_error(L, "Error in " LUA_QS ": expected a matrix and a scalar.", "linalg.mscalarmul");
  }
  la_createarray(L, a, m*n, "linalg.mscalarmul");
  if (fillmatrix(L, matidx, a, m, n, 0, "linalg.mscalarmul")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.mscalarmul");
  }
  for (i=0; i < m; i++) {  /* traverse row vectors */
    for (j=0; j < n; j++) {  /* traverse vector components */
      a[i*n + j] *= c;
    }
  }
  creatematrix(L, a, m, n, 1);
  xfree(a);
  return 1;
}


static int linalg_mscalardiv (lua_State *L) {  /* 3.18.5, UNDOC */
  int i, j, m, n, matidx;
  double *a, c;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  if (agn_istableutype(L, 1, "matrix") && agn_isnumber(L, 2)) {
    linalg_auxcheckmatrixlight(L, 1, &m, &n, "linalg.mscalardiv");  /* m = rowdim, n = coldim */
    c = agn_tonumber(L, 2);
    matidx = 1;
  } else if (agn_isnumber(L, 1) && agn_istableutype(L, 2, "matrix")) {
    c = agn_tonumber(L, 1);
    linalg_auxcheckmatrixlight(L, 2, &m, &n, "linalg.mscalardiv");  /* m = rowdim, n = coldim */
    matidx = 2;
  } else {
    c = 0.0;
    matidx = 0;
    luaL_error(L, "Error in " LUA_QS ": expected a matrix and a scalar.", "linalg.mscalardiv");
  }
  la_createarray(L, a, m*n, "linalg.mscalardiv");
  if (fillmatrix(L, matidx, a, m, n, 0, "linalg.mscalardiv")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.mscalardiv");
  }
  if (matidx == 1) {
    for (i=0; i < m; i++) {  /* traverse row vectors */
      for (j=0; j < n; j++) {  /* traverse vector components */
        if (c == 0.0)  /* 2.30.2 change from infinity to undefined */
          a[i*n + j] = AGN_NAN;
        else
          a[i*n + j] /= c;
      }
    }
  } else {  /* scalar divided by matrix elements */
    for (i=0; i < m; i++) {  /* traverse row vectors */
      for (j=0; j < n; j++) {  /* traverse vector components */
        if (a[i*n + j] == 0)
          a[i*n + j] = AGN_NAN;
        else
          a[i*n + j] = c/a[i*n + j];
      }
    }
  }
  creatematrix(L, a, m, n, 1);
  xfree(a);
  return 1;
}


static int linalg_madd (lua_State *L) {  /* 3.18.2, UNDOC, tuned 3.20.2 */
  int i, j, m, n, p, q;
  lua_Number *a;
  m = n = p = q = 0;  /* just to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.madd", &m, &n);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  linalg_auxcheckmatrix(L, 2, 1, 0, "linalg.madd", &p, &q);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  if (m != p || n != q)
    luaL_error(L, "Error in " LUA_QS ": matrix dimensions are incompatible.", "linalg.madd");
  la_createarray(L, a, m*n, "linalg.madd");
  if (fillmatrix(L, 1, a, m, n, 0, "linalg.madd")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.madd");
  }
  for (i=0; i < m; i++) {  /* traverse row vectors */
    lua_rawgeti(L, 2, i + 1);  /* push row vector on stack */
    if (checkVector(L, -1, "linalg.madd") != n)
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.madd");
    for (j=0; j < n; j++) {  /* traverse vector components */
      a[i*n + j] += agn_getinumber(L, -1, j + 1);
    }
    agn_poptop(L);
  }
  creatematrix(L, a, m, n, 1);
  xfree(a);
  return 1;
}


static int linalg_msub (lua_State *L) {  /* 3.18.2, UNDOC, tuned 3.20.2 */
  int i, j, m, n, p, q;
  lua_Number *a;
  m = n = p = q = 0;  /* just to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.msub", &m, &n);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  linalg_auxcheckmatrix(L, 2, 1, 0, "linalg.msub", &p, &q);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  if (m != p || n != q)
    luaL_error(L, "Error in " LUA_QS ": matrix dimensions are incompatible.", "linalg.msub");
  la_createarray(L, a, m*n, "linalg.msub");
  if (fillmatrix(L, 1, a, m, n, 0, "linalg.msub")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.msub");
  }
  for (i=0; i < m; i++) {  /* traverse row vectors */
    lua_rawgeti(L, 2, i + 1);  /* push row vector on stack */
    if (checkVector(L, -1, "linalg.msub") != n)
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.msub");
    for (j=0; j < n; j++) {  /* traverse vector components */
      a[i*n + j] -= agn_getinumber(L, -1, j + 1);
    }
    agn_poptop(L);
  }
  creatematrix(L, a, m, n, 1);
  xfree(a);
  return 1;
}


static int linalg_dotprod (lua_State *L) {  /* 2.1.3 */
  int i, sizea, sizeb;
  volatile lua_Number s, cs, ccs;
  checkvectors(L, 1, 2, "linalg.dotprod");
  s = cs = ccs = 0.0;
  /* now traverse vectors */
  for (i=1; i <= sizea; i++)
    s = tools_kbadd(s, agn_getinumber(L, 1, i) * agn_getinumber(L, 2, i), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
  lua_pushnumber(L, s + cs + ccs);
  return 1;
}


/*
A := linalg.hilbert(5);
linalg.det(A):

Singular matrix taken from https://www.cuemath.com/algebra/singular-matrix/

B := matrix([1, 2, 2], [1, 2, 2], [3, 2, -1]):
linalg.det(B):
*/

/* Determines the determinant of a square matrix, suited for matrices containing fractional elements. The algorithm heavily
   relies on division leading to round-off errors that even Kahan summation cannot compensate. */
static lua_Number determinant_frac (lua_Number *a, int n) {
  int i, j, i1, k, k1, piv[n];
  lua_Number d, x;
  for (i=0; i < n; i++) piv[i] = i;
  d = 1.0;
  for (k=0; k < n - 1; k++) {
    for (i=k; k < n && a[piv[i]*n + k] == 0.0; i++) { };
    if (n < i + 1) return 0.0;  /* singular matrix, but see below */
    for (j=i + 1; j < n; j++) {
      if (a[piv[j]*n + k] != 0.0) {
        if (fabs(a[piv[i]*n + k]) < fabs(a[piv[j]*n + k])) i = j;
      }
    }
    k1 = piv[i];
    if (i != k) { piv[i] = piv[k]; piv[k] = k1; d = -d; }
    for (i=k + 1; i < n; i++) {
      i1 = piv[i];
      x = a[i1*n + k]/a[k1*n + k];
      if (x != 0.0) {
        for (j=k + 1; j < n; j++)
          a[i1*n + j] -= x*a[k1*n + j];
      }
    }
  }
  for (i=0; i < n; i++) {
    i1 = piv[i];
    d *= a[i1*n + i];
  }
  return (d == -0.0) ? 0.0 : d;  /* tracking the flow in Maple with a singular matrix, we might get here, too */
}


/* Taken from: https://www.geeksforgeeks.org/determinant-of-a-matrix/
   C++ program to find Determinant of a matrix; changed to doubles
   The algorithm is very good by avoiding division that lead to round-off errors. KB destroys accuracy,
   so we do not use it. */
static lua_Number determinant_integ (lua_Number *a, int n) {
  int i, j, k, idx;  /* initialize result */
  lua_Number n1, n2, d, total, temp, t[n];  /* t[] is temporary array to store row */
  d = 1.0; total = 1.0;
  for (i=0; i < n; i++) {  /* loop for traversing the diagonal elements */
    idx = i;  /* initialize the index finding the index which has non zero value */
    while (idx < n && a[idx*n + i] == 0.0) idx++;
    if (idx == n) continue;  /* if there is non zero element the determinant of matrix as zero */
    if (idx != i) {  /* loop for swapping the diagonal element row and idx row */
      for (j=0; j < n; j++)  {
        temp = a[idx*n + j];
        a[idx*n + j] = a[i*n + j];
        a[i*n + j] = temp;
      }
      /* determinant sign changes when we shift rows go through determinant properties */
      d *= tools_intpow(-1, idx - i);
    }
    /* store the values of diagonal row elements */
    for (j=0; j < n; j++) t[j] = a[i*n + j];
    /* traverse every row below the diagonal element */
    for (j=i + 1; j < n; j++)  {
      n1 = t[i];  /* value of diagonal element */
      n2 = a[j*n + i];  /* value of next row element */
      /* traverse every column of row and multiply to every row; multiply to make the diagonal element
         and next row element equal */
      for (k=0; k < n; k++) {
        a[j*n + k] = n1*a[j*n + k] - n2*t[k];
      }
      total *= n1;  /* det(kA) = k*det(A); */
      if (total == 0.0) return AGN_NAN;
    }
  }
  /* multiply the diagonal elements to get determinant */
  for (i=0; i < n; i++) {
    d *= a[i*n + i];
  }
  d /= total;  /* 2.30.4 */
  d += 0.0;    /* normalise -0 to +0, 7.5.2 */
  return d;    /* det(kA)/k = det(A); */
}


static lua_Number aux_determinant (lua_State *L, int idx, lua_Number *a, int n, const char *procname) {
  lua_Number d = determinant_integ(a, n);  /* the `integral` approach mostly works beautifully also with fractional matrix components */
  if (isnan(d)) {  /* we might have a singular matrix, but make sure of this */
    /* 7.3.4 fix: do not create new matrix and `free` old one in helper function ! */
    memset(a, 0, n*n*sizeof(lua_Number));  /* better be sure than sorry. */
    fillmatrix(L, idx, a, n, n, 0, procname);  /* fill with original input matrix components again */
    d = determinant_frac(a, n);
  }
  d += 0.0;  /* normalise -0 to +0, 7.3.6 */
  return d;
}

static int linalg_det (lua_State *L) {  /* 2.1.3 */
  int m, n, nargs;
  lua_Number *a, d;
  nargs = lua_gettop(L);
  m = n = 0;  /* just to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.det", &m, &n);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  la_createarray(L, a, n * n, "linalg.det");
  if (fillmatrix(L, 1, a, n, n, 0, "linalg.det")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.det");
  }
  if (nargs == 2 && lua_isboolean(L, 2)) {  /* 3.20.4 */
    d = agn_istrue(L, 2) ? determinant_integ(a, n) : determinant_frac(a, n);
  } else {
    d = aux_determinant(L, 1, a, n, "linalg.det");
  }
  xfree(a);
  lua_pushnumber(L, d);
  return 1;
}


/* Recursive definition of permanent using expansion by minors. Taken from:
   http://paulbourke.net/miscellaneous/determinant and adapted to one-dimensional C matrix array */
static lua_Number permanent (lua_State *L, lua_Number *a, int n) {  /* 2.1.3 */
  volatile lua_Number s;
  if (n == 1) { /* shouldn't get used */
    s = a[0];
  } else if (n == 2) {
    s = a[0]*a[3] + a[2]*a[1];
  } else {
    int i, j, j1, j2, nn;
    lua_Number *m;
    volatile double cs, ccs;
    s = cs = ccs = 0.0;
    nn = n - 1;
    la_createarray(L, m, nn * nn, "linalg.permanent");
    for (j1=0; j1 < n; j1++) {
      for (i=1; i < n; i++) {
        j2 = 0;
        for (j=0; j < n; j++) {
          if (j == j1) continue;
          m[(i - 1)*nn + j2] = a[i*n + j];
          j2++;
        }
      }
      s = tools_kbadd(s, a[j1]*permanent(L, m, nn), &cs, &ccs);
    }
    xfree(m);
    s += cs + ccs;
  }
  return s;
}


/* n must be positive */
static inline int *charvector (long n, int dim) {
  int i, *r;
  r = (int *)calloc(dim + 1, sizeof(int));
  if (!r) return NULL;  /* 7.3.4 fix */
  i = dim - 1;
  while (n) {
    r[i] = n % 2;
    r[dim] += r[i--];  /* sum of r[0 .. dim-1] */
    n >>= 1;  /* division by two */
  }
  return r;
}

static lua_Number perm (lua_State *L, lua_Number *a, int n) {  /* this algorithm is by way faster than permanent() but
  a little less accurate */
  double k, max;
  volatile double prows, rows, s, cs, ccs, cr, ccr;
  int i, j, *cv;
  s = cs = ccs = 0.0;
  max = tools_intpow(2.0, n);
  for (k=1.0; k < max; k++) {  /* loop all 2**n submatrices */
    prows = 1.0;               /* product of row sums */
    cv = charvector(k, n);     /* characteristic vector */
    if (!cv)  /* 7.3.4 fix */
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "linalg.permanent");
    for (i=0; i < n; i++) {    /* loop columns */
      rows = cr = ccr = 0.0;   /* row sum */
      for (j=0; j < n; j++)    /* loop rows */
        rows = tools_kbadd(rows, cv[j]*a[i*n + j], &cr, &ccr);
      prows *= rows + cr + ccr;
      if (prows == 0.0) break;
    }
    s = tools_kbadd(s, tools_intpow(-1.0, n - cv[n])*prows, &cs, &ccs);
    xfree(cv);
  }
  return s + cs + ccs;
}

static int linalg_permanent (lua_State *L) {  /* 3.20.5 */
  int m, n, paulbourke;
  lua_Number *a, d;
  m = n = 0;  /* just to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.permanent", &m, &n);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  paulbourke = agnL_optboolean(L, 2, 1);
  la_createarray(L, a, n * n, "linalg.permanent");
  if (fillmatrix(L, 1, a, n, n, 0, "linalg.permanent")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.permanent");
  }
  d = paulbourke ? permanent(L, a, n) : perm(L, a, n);
  xfree(a);
  lua_pushnumber(L, d);
  return 1;
}


/* Find the cofactor matrix of a square matrix, the result is returned in b.
   Source: http://paulbourke.net/miscellaneous/Determinant of a square matrix.htm,
   written by Paul Bourke; modified for Agena */
static void la_cofactor (lua_State *L, lua_Number *a, int n, lua_Number *b, const char *procname) {
  int i, j, ii, jj, i1, j1, nn;
  lua_Number det, *c;
  nn = n - 1;
  la_createarray(L, c, nn * nn, procname);
  for (j=0; j < n; j++) {
    for (i=0; i < n; i++) { /* form the adjoint a_ij */
      i1 = 0;
      for (ii=0; ii < n; ii++) {
        if (ii == i) continue;
        j1 = 0;
        for (jj=0; jj < n; jj++) {
          if (jj == j) continue;
          c[i1*nn + j1] = a[ii*n + jj];
          j1++;
        }
        i1++;
      }
      det = aux_determinant(L, 1, c, nn, procname);  /* calculate the determinate */
      b[i*n + j] = tools_intpow(-1, i + j + 2) * det;  /* fill in the elements of the cofactor */
    }
  }
  xfree(c);
}

/* Transpose of a square matrix, do it in place
   Source: http://paulbourke.net/miscellaneous/Determinant of a square matrix.htm,
   written by Paul Bourke; modified for Agena */
void la_transpose (lua_Number *a, int n) {
  int i, j;
  lua_Number tmp;
  for (i=1; i < n; i++) {
    for (j=0; j < i; j++) {
      tmp = a[i*n + j];
      a[i*n + j] = a[j*n + i];
      a[j*n + i] = tmp;
    }
  }
}

static int linalg_inverse (lua_State *L) {  /* 2.1.3 */
  int i, j, m, n, nargs, sparse, columnv;
  lua_Number d, *a, *b;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  nargs = lua_gettop(L);
  sparse = columnv = 0;
  aux_checkvmoptions(L, 2, &nargs, &sparse, &columnv, "linalg.inverse");  /* 4.1.1 change */
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.inverse", &m, &n);
  agn_poptop(L);
  la_createarray(L, a, n * n, "linalg.inverse");
  la_createarray(L, b, n * n, "linalg.inverse");
  if (fillmatrix(L, 1, a, n, n, 0, "linalg.inverse")) {  /* 7.3.4 fix */
    xfreeall(a, b);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.inverse");
  }
  d = aux_determinant(L, 1, a, n, "linalg.inverse");
  xfree(a);
  if (d == 0.0) {  /* 3.20.4 */
    xfree(b);
    luaL_error(L, "Error in " LUA_QS ": matrix is singular.", "linalg.inverse");
  } else {
    la_createarray(L, a, n * n, "linalg.inverse");
    if (fillmatrix(L, 1, a, n, n, 0, "linalg.inverse")) {  /* 7.3.4 fix */
      xfreeall(a, b);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.inverse");
    }
    la_cofactor(L, a, n, b, "linalg.inverse");
    la_transpose(b, n);  /* create adjoint matrix, in place, the adjoint matrix is the transpose of the cofactor matrix. */
    for (i=0; i < n; i++) {
      for (j=0; j < n; j++) b[i*n + j] = b[i*n + j]/d;
    }
    creatematrix(L, b, n, n, sparse);  /* create sparse matrix by default */
    xfreeall(a, b);  /* 2.9.8 */
  }
  return 1;
}


/* Finds the co-factor matrix (adjoint) of square matrix A. A co-factor matrix is a matrix having the co-factors as the elements
   of the matrix. The co-factor of a matrix element is obtained when the minor M[i, j] of the element is multiplied
   with (-1)^(i+j). The minor of a matrix is for each element of the matrix and is equal to the part of the matrix
   remaining after excluding the row and the column containing that particular element. 3.18.5
   See also: https://www.cuemath.com/algebra/cofactor-matrix & https://www.cuemath.com/algebra/minor-of-matrix */
static int linalg_adjoint (lua_State *L) {
  int m, n;
  lua_Number *a, *b;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.adjoint", &m, &n);
  agn_poptop(L);
  la_createarray(L, a, n * n, "linalg.adjoint");
  la_createarray(L, b, n * n, "linalg.adjoint");
  if (fillmatrix(L, 1, a, n, n, 0, "linalg.adjoint")) {  /* 7.3.4 fix */
    xfreeall(a, b);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.adjoint");
  }
  la_cofactor(L, a, n, b, "linalg.adjoint");
  creatematrix(L, b, n, n, 1);
  xfreeall(a, b);
  return 1;
}


static int linalg_transpose (lua_State *L) {  /* 2.1.3 */
  int m, n;
  lua_Number *a;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.transpose", &m, &n);
  agn_poptop(L);
  la_createarray(L, a, m * n, "linalg.transpose");
  if (fillmatrix(L, 1, a, m, n, 0, "linalg.transpose")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.transpose");
  }
  if (m == n) {
    la_transpose(a, n);
    creatematrix(L, a, n, n, 1);
  } else {
    lua_Number *b;
    int i, j, c;
    la_createarray(L, b, m * n, "linalg.transpose");
    c = 0;
    for (j=0; j < n; j++) {
      for (i=0; i < m; i++) {
        b[c++] = a[i*n + j];
      }
    }
    creatematrix(L, b, n, m, 1);
    xfree(b);
  }
  xfree(a);
  return 1;
}


/* minor := proc(A, r :: posint, c :: posint) is
   local i, j, k, l, m, n, M, localA, islist;
   m, n := linalg.dim(A, true);  # also checks whether A is a matrix
   if m < r or n < c then
      error('Error in `linalg.minor`: row or column index is out of range.')
   elif m <> n then
      error('Error in `linalg.minor`: expected a square matrix.');
   fi;
   M := linalg.matrix(m - 1, n - 1, []);
   k := 0;
   for i to m do
      skip when i = r;
      k++;
      l := 0
      for j to n do
         j <> c ? M[k, ++l] := A[i, j]
      od
   od;
   return M
end;

A := matrix([[1, 2, 3], [4, 5, 6], [7, 8, 9]]);

minor(A, 1, 1): */
/* Find the minor of a square matrix for row r and column c, the resulting matrix is returned in m.
   r, c must start from zero. Based on la_cofactor(). The C version is twice as fast as the Agena version above. 3.18.7
   Source: http://paulbourke.net/miscellaneous/Determinant of a square matrix.htm,
   Originally written by Paul Bourke; modified for Agena */
static void aux_minor (lua_State *L, lua_Number *a, int n, int r, int c, lua_Number *m, const char *procname) {
  int i, j, k, l, nn;
  nn = n - 1;
  i = c;
  k = 0;
  for (i=0; i < n; i++) {
    if (i == c) continue;
    l = 0;
    for (j=0; j < n; j++) {
      if (j == r) continue;
      m[k*nn + l] = a[i*n + j];
      l++;
    }
    k++;
  }
}

static int linalg_minor (lua_State *L) {  /* 3.18.7 */
  int m, n, r, c;
  lua_Number *a, *b, d;
  m = n = 0;
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.minor", &m, &n);
  agn_poptop(L);
  r = agn_checkposint(L, 2);
  c = agn_checkposint(L, 3);
  if (r > n)
    luaL_error(L, "Error in " LUA_QS ": row index %d is out of range.", "linalg.minor", r);
  if (c > n)
    luaL_error(L, "Error in " LUA_QS ": column index %d is out of range.", "linalg.minor", c);
  la_createarray(L, a, n*n, "linalg.minor");
  la_createarray(L, b, (n - 1)*(n - 1), "linalg.minor");
  if (fillmatrix(L, 1, a, m, n, 0, "linalg.minor")) {  /* 7.3.4 fix */
    xfreeall(a, b);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.minor");
  }
  aux_minor(L, a, n, r - 1, c - 1, b, "linalg.minor");
  luaL_checkstack(L, 2, "not enough stack space");
  creatematrix(L, b, n - 1, n - 1, 1);
  xfreeall(a, b);
  la_createarray(L, a, (n - 1)*(n - 1), "linalg.minor");
  if (fillmatrix(L, -1, a, n - 1, n - 1, 0, "linalg.minor")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.minor");
  }
  d = aux_determinant(L, -1, a, n - 1, "linalg.minor");
  lua_pushnumber(L, d);
  xfree(a);
  return 2;
}


static int linalg_mmul (lua_State *L) {  /* 2.1.3 */
  int i, j, k, m, n, p, q;
  lua_Number *a, *b;
  m = n = p = q = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.mmul", &m, &n);
  linalg_auxcheckmatrix(L, 2, 1, 0, "linalg.mmul", &p, &q);
  agn_poptoptwo(L);  /* pop the two dimension pairs of the matrices */
  if (n != p)
    luaL_error(L, "Error in " LUA_QS ": incompatible dimensions, must get an m x n- & an n x p-matrix, got %d x %d & %d x %d.", "linalg.mmul", m, n, p, q);
  la_createarray(L, a, m * n, "linalg.mmul");
  la_createarray(L, b, p * q, "linalg.mmul");
  if (fillmatrix(L, 1, a, m, n, 0, "linalg.mmul")) {  /* 7.3.4 fix */
    xfreeall(a, b);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.mmul");
  }
  if (fillmatrix(L, 2, b, p, q, 0, "linalg.mmul")) {  /* 7.3.4 fix */
    xfreeall(a, b);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.mmul");
  }
  createrawmatrix(L, m, q);
  if (aux_isintegral(a, m*n) && aux_isintegral(b, p*q)) {  /* 4.0.1 */
    lua_Number s;
    for (i=0; i < m; i++) {  /* for each row in resulting matrix */
      lua_rawgeti(L, -1, i + 1);  /* push empty row vector and avoid too many pushes ... */
      for (k=0; k < q; k++) {  /* for each column in B */
        s = 0.0;
        for (j=0; j < n; j++) {   /* for each element in A */
          s += a[i*n + j] * b[j*q + k];
        }
        agn_setinumber(L, -1, k + 1, s);  /* ... and set its respective element */
      }
      agn_poptop(L);  /* pop row vector */
    }
  } else {
    volatile double s, cs, ccs;
    for (i=0; i < m; i++) {  /* for each row in resulting matrix */
      lua_rawgeti(L, -1, i + 1);  /* push empty row vector and avoid too many pushes ... */
      for (k=0; k < q; k++) {  /* for each column in B */
        s = cs = ccs = 0.0;
        for (j=0; j < n; j++) {   /* for each element in A */
          s = tools_kbadd(s, a[i*n + j] * b[j*q + k], &cs, &ccs);
        }
        agn_setinumber(L, -1, k + 1, s + cs + ccs);  /* ... and set its respective element */
      }
      agn_poptop(L);  /* pop row vector */
    }
  }
  xfreeall(a, b);  /* 2.9.8 */
  return 1;
}


/* Raises a square matrix A to the power of a positive integer n, that is multiplies A n times with itself.
   The function works in O(log[2](n)) time. The algorithm has been taken from:
   https://stackoverflow.com/questions/30253662/raising-a-2d-array-to-a-power-in-c, solution by user `samgak`
   3.16.5; C port 4.0.1, thrice as fast the Agena implementation */

/* Multiplies two square matrices a, b of dimension n, FREE the result ! 4.0.1 */
static FORCE_INLINE void aux_msquared (lua_State *L, lua_Number *into, lua_Number *a, lua_Number *b, int n) {
  int i, j, k, nn;
  lua_Number *z;
  nn = n*n;
  la_createarray(L, z, nn, "linalg.mpow");
  if (aux_isintegral(a, nn) && aux_isintegral(b, nn)) {  /* this gives a boost of 45 percent */
    lua_Number s;
    for (i=0; i < n; i++) {
      for (j=0; j < n; j++) {
        s = 0.0;
        for (k=0; k < n; k++) {
          s += a[i*n + k] * b[k*n + j];
        }
        z[i*n + j] = s;
      }
    }
  } else {
    volatile double s, cs, ccs;
    for (i=0; i < n; i++) {
      for (j=0; j < n; j++) {
        s = cs = ccs = 0.0;
        for (k=0; k < n; k++) {
          s = tools_kbadd(s, a[i*n + k] * b[k*n + j], &cs, &ccs);
        }
        z[i*n + j] = s + cs + ccs;
      }
    }
  }
  for(i=0; i < nn; i++) into[i] = z[i];
  xfree(z);
}

static int linalg_mpow (lua_State *L) {  /* 4.0.1 */
  int i, j, m, n, x;
  lua_Number *a, *b;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.mpow", &m, &n);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  x = agn_checkposint(L, 2);
  la_createarray(L, a, n * n, "linalg.mpow");
  la_createarray(L, b, n * n, "linalg.mpow");
  /* copy matrix into a */
  if (fillmatrix(L, 1, a, n, n, 0, "linalg.mpow")) {  /* 7.3.4 fix */
    xfreeall(a, b);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.mpow");
  }
  /* fill identity matrix b */
  for (i=0; i < n; i++) {
    for (j=0; j < n; j++) {
      b[i*n + j] = (i == j);
    }
  }
  /* now assemble the result */
  while (x) {
    if (x & 1)
      aux_msquared(L, b, b, a, n);
    if (x != 1)
      aux_msquared(L, a, a, a, n);
    x >>= 1;
  }
  creatematrix(L, b, n, n, 1);  /* create sparse matrix if possible */
  xfreeall(a, b);
  return 1;
}


static FORCE_INLINE void meq (lua_State *L, int (*fn)(lua_Number, lua_Number, lua_Number)) {  /* 2.1.3, can compare normal with sparse matrices */
  int i, j, m, n, p, q, flag;
  m = n = p = q = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.meq", &m, &n);
  linalg_auxcheckmatrix(L, 2, 1, 0, "linalg.meq", &p, &q);
  agn_poptoptwo(L);  /* pop the two dimension pairs of the matrices A and B */
  flag = 1;
  if (m != p || n != q)
    lua_pushfalse(L);
  else {  /* matrices have same dimensions */
    lua_Number eps;
    eps = agn_getepsilon(L);
    for (i=1; i <= m; i++) {
      lua_rawgeti(L, 1, i);  /* push row vector of A */
      checkVector(L, -1, "linalg.meq");  /* 3.17.4 security fix */
      lua_rawgeti(L, 2, i);  /* push row vector of B */
      checkVector(L, -1, "linalg.meq");  /* 3.17.4 security fix */
      for (j=1; j <= n; j++) {  /* now compare each item */
        if (!fn(agn_getinumber(L, -2, j), agn_getinumber(L, -1, j), eps)) {
          flag = 0;
          agn_poptoptwo(L);  /* pop both row vectors */
          goto endofmeq;  /* exit both loops in case of inequality */
        }
      }
      agn_poptoptwo(L);  /* pop both row vectors */
    }  /* end of for i */
  }
endofmeq:
  lua_pushboolean(L, flag);
}

static FORCE_INLINE int equal (lua_Number x, lua_Number y, lua_Number eps) {  /* strict equality, eps does not matter */
  return x == y;
}


static int linalg_maeq (lua_State *L) {  /* 2.1.3 */
  meq(L, tools_approx);
  return 1;
}


static int linalg_meeq (lua_State *L) {  /* 2.1.4 */
  meq(L, equal);
  return 1;
}


static int linalg_trace (lua_State *L) {  /* 2.1.4 */
  int i, m, n;
  volatile double s, cs, ccs, x;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.trace", &m, &n);
  agn_poptop(L);  /* pop dimension pair of matrix A */
  s = cs = ccs = 0.0;
  for (i=1; i <= m; i++) {
    lua_rawgeti(L, 1, i);  /* push row vector of A */
    checkVector(L, -1, "linalg.trace");  /* 3.17.4 security fix */
    x = agn_getinumber(L, -1, i);  /* get i-th element */
    s = tools_kbadd(s, x, &cs, &ccs);  /* change to Kahan-Babuska summation */
    agn_poptop(L);  /* pop row vector */
  }
  lua_pushnumber(L, s + cs + ccs);
  return 1;
}


static int linalg_getdiagonal (lua_State *L) {  /* 2.1.4 */
  int i, m, n, rc;
  lua_Number x;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.getdiagonal", &m, &n);
  agn_poptop(L);  /* pop dimension pair of matrix A */
  lua_createtable(L, m, 1);
  for (i=1; i <= m; i++) {
    lua_rawgeti(L, 1, i);  /* push row vector of A */
    checkVector(L, -1, "linalg.getdiagonal");  /* 3.17.4 security fix */
    x = agn_rawgetinumber(L, -1, i, &rc);
    if (!rc) continue;  /* 3.20.4 change to preserve sparseness */
    agn_setinumber(L, -2, i, x);
    agn_poptop(L);  /* pop row vector */
  }
  setvattribs(L, m);
  return 1;
}


static int linalg_getantidiagonal (lua_State *L) {  /* 3.18.7 */
  int i, m, n, rc;
  lua_Number x;
  m = n = 0;
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.getantidiagonal", &m, &n);
  agn_poptop(L);  /* pop dimension pair of matrix A */
  lua_createtable(L, m, 1);
  for (i=1; i <= m; i++) {
    lua_rawgeti(L, 1, i);  /* push row vector of A */
    checkVector(L, -1, "linalg.getantidiagonal");
    x = agn_rawgetinumber(L, -1, n - i + 1, &rc);
    if (!rc) continue;  /* 3.20.4 change to preserve sparseness */
    agn_setinumber(L, -2, i, x);
    agn_poptop(L);  /* pop row vector */
  }
  setvattribs(L, m);
  return 1;
}


static int linalg_diagonal (lua_State *L) {  /* 2.1.4 */
  int i, size, rc;
  lua_Number x;
  checkvector(L, 1, "linalg.diagonal");
  createrawmatrix(L, size, size);
  for (i=1; i <= size; i++) {  /* for each row in A */
    lua_rawgeti(L, -1, i);  /* push row vector of new matrix */
    x = agn_rawgetinumber(L, 1, i, &rc);
    if (!rc) continue;  /* ... and set its respective element; 3.20.4 change to preserve sparseness */
    agn_setinumber(L, -1, i, x);
    agn_poptop(L);  /* pop row vector of new matrix */
  }
  return 1;
}


static int linalg_antidiagonal (lua_State *L) {  /* 3.18.7 */
  int i, size, rc;
  lua_Number x;
  checkvector(L, 1, "linalg.antidiagonal");
  createrawmatrix(L, size, size);
  for (i=1; i <= size; i++) {  /* for each row in A */
    lua_rawgeti(L, -1, i);  /* push row vector of new matrix */
    x = agn_rawgetinumber(L, 1, i, &rc);
    if (!rc) continue;  /* ... and set its respective element; 3.20.4 change to preserve sparseness */
    agn_setinumber(L, -1, size - i + 1, x);
    agn_poptop(L);  /* pop row vector of A and row vector of new matrix */
  }
  return 1;
}


static int linalg_vaeq (lua_State *L) {  /* 2.1.3, triggers mts correctly */
  int i, size;
  lua_Number eps;
  size = checkVector(L, 1, "linalg.veq");
  if (size != checkVector(L, 2, "linalg.veq")) {
    lua_pushfalse(L);
    return 1;
  }
  eps = agn_getepsilon(L);
  for (i=1; i <= size && tools_approx(agn_getinumber(L, 1, i), agn_getinumber(L, 2, i), eps); i++);
  lua_pushboolean(L, i > size);
  return 1;
}


static int linalg_veeq (lua_State *L) {  /* 2.1.4, triggers mts correctly */
  int i, size;
  size = checkVector(L, 1, "linalg.veq");
  if (size != checkVector(L, 2, "linalg.veq")) {
    lua_pushfalse(L);
    return 1;
  }
  for (i=1; i <= size && agn_getinumber(L, 1, i) == agn_getinumber(L, 2, i); i++);
  lua_pushboolean(L, i > size);
  return 1;
}


/* Gauﬂ Elimination with Pivoting, 2.1.3, modified 2.1.5

   Merge of code taken from:
   - http://www.dailyfreecode.com/code/basic-gauss-elimination-method-gauss-2949.aspx posted by Alexander Evans, and
   - http://paulbourke.net/miscellaneous/gausselim published by Paul Bourke;
   modified for Agena.

   At completion, a will hold the upper triangular matrix, and x the solution vector. */

static void aux_mcopy (lua_State *L, int addrows, int addcols, lua_Number def, int setattribs, int inplace, int dense, const char *procname);

static int gsolve (long double *a, int n, long double *x, long double eps) {
  int i, j, k, maxrow, nn, issingular;
  long double tmp, max;
  nn = n + 1;
  issingular = 0;
  for (j=0; j < n; j++) {  /* modified, for (j=0; j < n-1; j++) { */
    max = fabsl(a[j*nn + j]);
    maxrow = j;
    for (i=j + 1; i < n; i++)  /* find the row with the largest first value */
      if (fabsl(a[i*nn + j]) > max) {
        max = a[i*nn + j];
        maxrow = i;
      }
    /* if (maxrow != j) { */
    for (k=j; k < n + 1; k++) {  /* swap the maxrow and jth row */
      tmp = a[j*nn + k];
      a[j*nn + k] = a[maxrow*nn + k];
      a[maxrow*nn + k] = tmp;
     }
    /* } */
    if (issingular == 0 && fabsl(a[j*nn + j]) < eps) issingular = 1;  /* statement added to detect singular matrices */
    for (i=j + 1; i < n; i++) {  /* eliminate the ith element of the jth row */
      tmp = a[i*nn + j]/a[j*nn + j];
      for (k=n; k >= j; k--)
        a[i*nn + k] -=  a[j*nn + k] * tmp;
    }
  }
  for (j=n - 1; j >= 0; j--) {  /* conduct back substitution */
    tmp = 0;
    for (k=j + 1; k < n; k++)
      tmp += a[j*nn + k] * x[k];
    x[j] = (a[j*nn + n] - tmp)/a[j*nn + j];
  }
  return issingular;
}

static int linalg_linsolve (lua_State *L) {  /* 2.1.3, 2.34.10 switch to long double */
  int i, m, n, nargs, retut, iszero, isundefined;
  long double *a, *x, eps;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  nargs = lua_gettop(L);
  retut = (lua_isboolean(L, nargs) && lua_toboolean(L, nargs) == 1);
  if (retut == 1) nargs--;  /* return upper triangular matrix, as well. */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.linsolve", &m, &n);
  agn_poptop(L);  /* pop dimension pair of the matrix */
  eps = agn_getepsilon(L);
  if (nargs == 1) {
    if (m == n) {  /* 3.18.1 extension for square matrices: add zero column vector automatically */
      aux_mcopy(L, 0, 1, 0, 1, 0, 0, "linalg.linsolve");  /* add zero column vector */
      lua_replace(L, 1);
      n++;
    } else if (m + 1 != n) {
      luaL_error(L, "Error in " LUA_QS ": matrix has wrong dimensions.", "linalg.linsolve");
    }
  } else if (nargs == 2) {
    if (m != n)
      luaL_error(L, "Error in " LUA_QS ": expected a square matrix.", "linalg.linsolve");
    if (n != checkVector(L, 2, "linalg.linsolve"))
      luaL_error(L, "Error in " LUA_QS ": expected matrix and vector with equal dimensions.", "linalg.linsolve");
    n++;
  } else
    luaL_error(L, "Error in " LUA_QS ": one or two arguments expected.", "linalg.linsolve");
  createarrayld(a, m*n, "linalg.linsolve");
  createarrayld(x, m, "linalg.linsolve");
  if (nargs == 1) {
    if (fillmatrixld(L, 1, a, m, n, 0, "linalg.linsolve")) {  /* 7.3.4 fix */
      xfreeall(a, x);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.linsolve");
    }
  } else {  /* augment square matrix with vector */
    int i;
    long double *b;
    createarrayld(b, m, "linalg.linsolve");
    if (fillmatrixld(L, 1, a, m, n, 1, "linalg.linsolve")) {  /* 7.3.4 fix */
      xfreeall(a, x, b);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.linsolve");
    }
    fillvectorld(L, 2, b, m, "linalg.linsolve");
    for (i=0; i < m; i++) a[i*n + m] = b[i];
    xfree(b);
  }
  gsolve(a, m, x, eps);
  iszero = 1; isundefined = 0;
  /* now inspect augmented row reduced echelon matrix */
  for (i=0; i < n - 1; i++) {
    if (!tools_approxl(a[(m - 1)*n + i], 0, eps)) {
      iszero = 0;
      break;
    }
  }
  for (i=0; i < n; i++) {
    if (tools_fpisnanl(a[(m - 1)*n + i])) {  /* 2.10.1, changed to isnan instead of tools_isnan */
      isundefined = 1;
      break;
    }
  }
  if (isundefined) {  /* example: m := matrix([1, 1, 1], [2, 2, 5], [4, 4, 8]), b := vector(-1, -8, -14) */
    lua_pushfail(L);
    retut = 0;  /* 7.5.1 change */
  } else if (iszero) {
    lua_pushnumber(L, tools_approxl(a[(m - 1)*n + (n - 1)], 0, eps) ? HUGE_VAL : AGN_NAN);  /* infinite number of solutions or no solution */
    retut = 0;  /* 7.5.1 change */
  } else
    createvectorld(L, x, m);
  if (retut) creatematrixld(L, a, m, n);  /* return upper triangular matrix, too. */
  xfreeall(a, x);
  return 1 + retut;
}


/*
  Form linear combinations of matrix rows or columns
  The call linalg.addrow(A, r1, r2, m) returns a copy of the matrix A in which row r2 is replaced by m*linalg.row(A, r1) + linalg.row(A, r2).
  Similarly linalg.addcol(A, c1, c2, m) returns a copy of the matrix A in which column c2 is replaced by m*linalg.col(A, c1) + linalg.col(A, c2).
  In both cases, if the number m is not given, m default to 1.
  These are clones of Maple's linalg[addrow] and linalg[addcol] functions. 3.17.8, ported to C 3.18.2
*/

/* n, k starting fdrom one, r1, r2 starting from zero ! */
static void aux_addrow (lua_State *L, int n, int k, int r1, int r2, long double l, const char *procname) {
  int j;
  long double *a;
  createarrayld(a, n*k, procname);
  if (fillmatrixld(L, 1, a, n, k, 0, procname)) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", procname);
  }
  for (j=0; j < k; j++) {
    a[r2*k + j] += l*a[r1*k + j];
  }
  creatematrixld(L, a, n, k);
  xfree(a);
}

static int linalg_addrow (lua_State *L) {  /* 3.18.2 */
  int n, k, r1, r2;
  long double l;
  n = k = 0;
  r1 = agn_checkposint(L, 2);
  r2 = agn_checkposint(L, 3);
  l = agnL_optnumber(L, 4, 1);
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.addrow", &n, &k);  /* n = rowdim, k = coldim */
  agn_poptop(L);  /* pop dimension pair of the matrix */
  if (r1 > n || r2 > n)
    luaL_error(L, "Error in " LUA_QS ": wrong type or number of arguments.", "linalg.addrow");
  aux_addrow(L, n, k, --r1, --r2, l, "linalg.addrow");
  return 1;
}

/* n, k starting fdrom one, r1, r2 starting from zero ! */
static void aux_addcol (lua_State *L, int n, int k, int c1, int c2, long double l, const char *procname) {
  int i;
  long double *a;
  createarrayld(a, n*k, procname);
  if (fillmatrixld(L, 1, a, n, k, 0, procname)) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", procname);
  }
  for (i=0; i < n; i++) {
    a[i*k + c2] += l*a[i*k + c1];
  }
  creatematrixld(L, a, n, k);
  xfree(a);
}

static int linalg_addcol (lua_State *L) {  /* 3.18.2 */
  int n, k, c1, c2;
  long double l;
  n = k = 0;
  c1 = agn_checkposint(L, 2);
  c2 = agn_checkposint(L, 3);
  l = agnL_optnumber(L, 4, 1);
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.addcol", &n, &k);  /* n = rowdim, k = coldim */
  agn_poptop(L);  /* pop dimension pair of the matrix */
  if (c1 > k || c2 > k)  /* 3.18.3 fix */
    luaL_error(L, "Error in " LUA_QS ": wrong type or number of arguments.", "linalg.addcol");
  aux_addcol(L, n, k, --c1, --c2, l, "linalg.addcol");
  return 1;
}


/* Create a generalized n x n Hilbert matrix, it has 1/(i+j-x) as its (i,j)-th entry.
   If x is not specified, then x is 1. Written 18.12.2008, improved 1.3.3, January 31, 2011. Ported to C 3.18.2 */
static int linalg_hilbert (lua_State *L) {
  int i, j, n;
  long double *a, x;
  n = agn_checkposint(L, 1);
  x = agnL_optnumber(L, 2, 1);
  createarrayld(a, n*n, "linalg.hilbert");
  for (i=0; i < n; i++) {
    for (j=0; j < n; j++) {
      a[i*n + j] = 1/(2 + i + j - x);
    }
  }
  creatematrixld(L, a, n, n);
  xfree(a);
  return 1;
}


/*
Credit: Agena's `linalg.scale` is a port of the ALGOL 60 function REASCL, being part of the NUMAL package,
originally published by The Stichting Centrum Wiskunde & Informatica, Amsterdam, The Netherlands.

The Stichting Centrum Wiskunde & Informatica (Stichting CWI) (legal successor of Stichting Mathematisch
Centrum) at Amsterdam has granted permission to Paul McJones to attach the integral NUMAL library manual
to his software preservation project web page.

( URL: http://www.softwarepreservation.org/projects/ALGOL/applications/ )

It may be freely used. It may be copied provided that the name NUMAL and the attribution to the Stichting
CWI are retained.

Original ALGOL 60 credits to REASCL:

AUTHORS  : T.J. DEKKER, W. HOFFMANN.
CONTRIBUTORS: W. HOFFMANN, S.P.N. VAN KAMPEN.
INSTITUTE: MATHEMATICAL CENTRE.
RECEIVED: 731030.

BRIEF DESCRIPTION:

The procedure REASCL (scale) normalises the (non-null) columns of a matrix
in such a way that, in each column, an element of maximum absolute value
equals 1. The normalised vectors are delivered in the corresponding columns
of the new matrix.

RUNNING TIME: PROPORTIONAL TO N * (N2 - N1 + 1).

LANGUAGE:   ALGOL 60.

METHOD AND PERFORMANCE: SEE REF [1].

REFERENCES:
     [1].T.J. DEKKER AND W. HOFFMANN.
         ALGOL 60 PROCEDURES IN NUMERICAL ALGEBRA, PART 2.
         MC TRACT 23, 1968, MATH. CENTR., AMSTERDAM.

SOURCE TEXT(S):

 CODE 34183;
     COMMENT MCA 2413;
     PROCEDURE REASCL(A, N, N1, N2); VALUE N, N1, N2;
     INTEGER N, N1, N2; ARRAY A;
     BEGIN INTEGER I, J; REAL S;
         FOR J:= N1 STEP 1 UNTIL N2 DO
         BEGIN S:= 0;
             FOR I:= 1 STEP 1 UNTIL N DO
                 IF ABS(A[I,J]) > ABS(S) THEN S:= A[I,J];
             IF S ^= 0 THEN
                 FOR I:= 1 STEP 1 UNTIL N DO A[I,J]:= A[I,J] / S
         END
     END REASCL;
         EOP
*/

static FORCE_INLINE void aux_getbounds (lua_State *L, int idx, int nargs, int *n1, int *n2, int n, const char *what, const char *procname) {
  if (nargs == idx && lua_ispair(L, idx)) {
    agnL_pairgetiposints(L, "linalg.scale", idx, 1, n1, n2);
    if (*n1 > n || *n2 > n)
      luaL_error(L, "Error in " LUA_QS ": %s indices are out-of-bounds.", procname, what);
  }
}

static int linalg_scale (lua_State *L) {
  int i, j, m, n, n1, n2, nargs;
  long double *a, s;
  nargs = lua_gettop(L);
  m = n = 0;  /* 4.2.0, to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.scale", &m, &n);  /* m = rowdim, n = coldim */
  n1 = 1; n2 = n;
  agn_poptop(L);  /* pop dimension pair */
  aux_getbounds(L, 2, nargs, &n1, &n2, n, "column", "linalg.scale");  /* 3.18.5 extension */
  n1--;
  createarrayld(a, m*n, "linalg.scale");
  if (fillmatrixld(L, 1, a, m, n, 0, "linalg.scale")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.scale");
  }
  for (j=n1; j < n2; j++) {  /* traverse columns */
    s = 0.0L;
    for (i=0; i < m; i++) {  /* traverse all rows and get maximum for the column */
      if (fabsl(a[i*n + j]) > fabsl(s)) {
        s = a[i*n + j];
      }
    }
    if (s != 0.0L) {
      for (i=0; i < m; i++) {  /* traverse all rows */
        a[i*n + j] /= s;
      }
    }
  }
  creatematrixld(L, a, m, n);
  xfree(a);
  return 1;
}


/*
 AUTHOR:     P.A.BEENTJES.

 INSTITUTE:  MATHEMATICAL CENTRE.

 RECEIVED:   730715.

 BRIEF DESCRIPTION:

     This section contains two procedures.
     rotcol replaces the column vector x given in array A[L:U, I:I] and
     the column vector y given in array A[L:U, J:J] by the vectors
     c*x + s*y and c*y - s*x.

     rotrow replaces the row vector x given in array A[I:I, L:U] and the
     row vector y given in array A[J:J, L:U] by the vector Cx + Sy and
     c*y - s*x.

 KEYWORDS:

     ELEMENTARY PROCEDURE, VECTOR OPERATIONS, ROTATION.

 SUBSECTION: ROTCOL.

 CALLING SEQUENCE:

     HEADING:
     "PROCEDURE" ROTCOL(L, U, I, J, A, C, S); "VALUE" L,U,I,J,C,S;
     "INTEGER" L,U,I,J; "REAL" C,S; "ARRAY" A;
     "CODE" 34040;

     FORMAL PARAMETERS:
     L,U:    <ARITHMETIC EXPRESSION>;
             lower and upper bound of the running subscript [i.e. row number];
     I,J:    <ARITHMETIC EXPRESSION>;
             column-indices of the column vectors of array A
     A:      <ARRAY IDENTIFIER>;
             "ARRAY" A[L : U, P : Q]; P and Q should satisfy:
             P <= I, P <= J, Q >= I AND Q >= J;
     C,S:    <ARITHMETIC EXPRESSION>;
             rotation factors.

 LANGUAGE: COMPASS [ALGOL 60 actually].

 SUBSECTION: ROTROW.

 CALLING SEQUENCE:

     HEADING:
     "PROCEDURE" ROTROW(L, U, I, J, A, C, S); "VALUE" L,U,I,J,C,S;
     "INTEGER" L,U,I,J; "REAL" C,S; "ARRAY" A;
     "CODE" 34041;

     FORMAL PARAMETERS:
     I,J:    <ARITHMETIC EXPRESSION>;
             row-indices of the row-vectors of array A [i.e. row number];
     L,U:    <ARITHMETIC EXPRESSION>;
             lower and upper bound of the running [column] subscript
     A:      <ARRAY IDENTIFIER>;
             "ARRAY" A[P : Q, L : U]; P and Q should satisfy:
             P <= I, P <= J, Q >= I and Q >= J;
     C,S:    <ARITHMETIC EXPRESSION>;
             rotation factors.

 LANGUAGE: COMPASS [ALGOL 60 actually].

 REFERENCES:

     [1].T.J.DEKKER.
         ALGOL 60 Procedures In Numerical Algebra, Part 1,
         Mathematical Centre Tract 22, Amsterdam (1970).

 SOURCE TEXT(S):

 The following procedures are written in COMPASS, an equivalent ALGOL 60
 text of these COMPASS routines is given.

 CODE 34040;
     PROCEDURE ROTCOL(L, U, I, J, A, C, S); VALUE L,U,I,J,C,S;
     INTEGER L,U,I,J; REAL C,S; ARRAY A;
     BEGIN REAL X, Y;
         FOR L:= L STEP 1 UNTIL U DO
         BEGIN X:= A[L,I]; Y:= A[L,J];
             A[L,I]:= X * C + Y * S;
             A[L,J]:= Y * C - X * S
         END
     END ROTCOL;
         EOP

 CODE 34041;
     PROCEDURE ROTROW(L, U, I, J, A, C, S); VALUE L,U,I,J,C,S;
     INTEGER L,U,I,J; REAL C,S; ARRAY A;
     BEGIN REAL X, Y;
         FOR L:= L STEP 1 UNTIL U DO
         BEGIN X:= A[I,L]; Y:= A[J,L];
             A[I,L]:= X * C + Y * S;
             A[J,L]:= Y * C - X * S
         END
     END ROTROW;
         EOP

 See: https://github.com/JeffBezanson/numal/blob/master/newnumal5p1.txt#L2339 */

/* rotcol replaces the i-th column vector x in any m x n matrix A and the j-th column vector y in A
   by the vectors c*x + s*y and c*y - s*x, respectively. By default all rows are changed, but you might
   limit this to row p to q by passing the pair p:q as the sixth argument.
   The return is a new matrix, with A left unchanged. 3.18.5*/
static int linalg_rotcol (lua_State *L) {
  int i, j, m, n, l, u, nargs;
  long double *a, x, y, c, s;
  nargs = lua_gettop(L);
  m = n = 0;  /* 4.2.0, to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.rotcol", &m, &n);  /* m = rowdim, n = coldim */
  agn_poptop(L);  /* pop dimension pair */
  l = 1; u = m;   /* row indices */
  i = 1; j = n;   /* column indices */
  i = agn_checkposint(L, 2);  /* i-th column */
  j = agn_checkposint(L, 3);  /* j-th column */
  c = agn_checknumber(L, 4);
  s = agn_checknumber(L, 5);
  if (i > n || j > n)
    luaL_error(L, "Error in " LUA_QS ": column indices are out-of-bounds.", "linalg.rotcol");
  /* process optional row positions l to u */
  aux_getbounds(L, 6, nargs, &l, &u, m, "row", "linalg.rotcol");
  l--; i--; j--;
  createarrayld(a, m*n, "linalg.rotcol");
  if (fillmatrixld(L, 1, a, m, n, 0, "linalg.rotcol")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.rotcol");
  }
  for (; l < u; l++) {  /* iterate row vectors */
    x = a[l*n + i];
    y = a[l*n + j];
    a[l*n + i] = x*c + y*s;
    a[l*n + j] = y*c - x*s;
  }
  creatematrixld(L, a, m, n);
  xfree(a);
  return 1;
}


/* rotrow replaces the i-th row vector x given in any m x n array A and the j-th row vector y
   in A by the vectors c*x + s*y and c*y - s*x. By default all columns are changed, but you might
   limit this to column p to q by passing the pair p:q as the sixth argument.
   The return is a new matrix, with A left unchanged. 3.18.5 */
static int linalg_rotrow (lua_State *L) {
  int i, j, m, n, l, u, nargs;
  long double *a, x, y, c, s;
  nargs = lua_gettop(L);
  m = n = 0;  /* 4.2.0, to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.rotrow", &m, &n);  /* m = rowdim, n = coldim */
  agn_poptop(L);  /* pop dimension pair */
  i = 1; j = m;   /* row indices */
  l = 1; u = n;   /* column indices */
  i = agn_checkposint(L, 2);  /* i-th row */
  j = agn_checkposint(L, 3);  /* j-th row */
  c = agn_checknumber(L, 4);
  s = agn_checknumber(L, 5);
  if (i > m || j > m)
    luaL_error(L, "Error in " LUA_QS ": row indices are out-of-bounds.", "linalg.rotrow");
  /* process optional column positions l to u */
  aux_getbounds(L, 6, nargs, &l, &u, n, "column", "linalg.rotrow");
  l--; i--; j--;
  createarrayld(a, m*n, "linalg.rotrow");
  if (fillmatrixld(L, 1, a, m, n, 0, "linalg.rotrow")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.rotrow");
  }
  for (; l < u; l++) {  /* iterate column vectors */
    x = a[i*n + l];
    y = a[j*n + l];
    a[i*n + l] = x*c + y*s;
    a[j*n + l] = y*c - x*s;
  }
  creatematrixld(L, a, m, n);
  xfree(a);
  return 1;
}


/* Taken from: https://github.com/JeffBezanson/numal/blob/master/newnumal5p1.txt#L2339

 FROM: NUMAL PACKAGE FOR ALGOL 60 BY THE STICHTING CENTRUM WISKUNDE & INFORMATICA,
       AMSTERDAM, THE NETHERLANDS.

 AUTHORS: C.G. VAN DER LAAN AND J.C.P. BUS.

 CONTRIBUTOR: J.C.P. BUS.

 INSTITUTE: MATHEMATICAL CENTRE.

 RECEIVED: 740921.

 BRIEF DESCRIPTION:

     INFNRMVEC CALCULATES THE INFINITY-NORM OF A VECTOR;
     INFNRMROW CALCULATES THE INFINITY-NORM OF A ROW VECTOR;
     (*) INFNRMCOL CALCULATES THE INFINITY-NORM OF A COLUMN VECTOR;
     INFNRMMAT CALCULATES THE INFINITY-NORM OF A MATRIX;
     ONENRMVEC CALCULATES THE ONE-NORM OF A VECTOR;
     (*) ONENRMROW CALCULATES THE ONE-NORM OF A ROW VECTOR;
     (*) ONENRMCOL CALCULATES THE ONE-NORM OF A COLUMN VECTOR;
     ONENRMMAT CALCULATES THE ONE-NORM OF A MATRIX;
     ABSMAXMAT CALCULATES FOR A GIVEN MATRIX THE MODULUS OF AN ELEMENT
     WHICH IS OF MAXIMUM ABSOLUTE VALUE;

     (*) = used by other functions

 KEYWORDS:

     VECTOR NORMS, MATRIX NORMS.

--------------------------------------------------------------------------

 SUBSECTION: INFNRMVEC - CALCULATES THE INFINITY-NORM OF A VECTOR. 3.18.5

 CALLING SEQUENCE:

     THE HEADING OF THE PROCEDURE READS:
     "REAL" "PROCEDURE" INFNRMVEC(L, U, K, A);
     "VALUE" L, U; "INTEGER" L, U, K; "ARRAY" A;
     "CODE" 31061;

     INFNRMVEC := MAX( ABS(A[I]), I= L, ..., U );

     THE MEANING OF THE FORMAL PARAMETERS IS:
     L, U:   <ARITHMETIC EXPRESSION>;
             ENTRY:THE LOWER BOUND AND UPPER BOUND OF THE INDEX OF THE
                   VECTOR A, RESPECTIVELY;
     K:      <VARIABLE>;
             EXIT:THE FIRST INDEX FOR WHICH ABS(A[I]), I = L, ..., U,
                  IS MAXIMAL;
     A:      <ARRAY IDENTIFIER>;
             "ARRAY" A[L:U].

 PROCEDURES USED: NONE.

0CODE 31061;
  REAL PROCEDURE INFNRMVEC(L, U, K, A); VALUE L, U;
  INTEGER L, U, K; ARRAY A;
  BEGIN REAL R, MAX;
     MAX:= 0; K:= L;
     FOR L:= L STEP 1 UNTIL U DO
     BEGIN
         R:= ABS(A[L]);
         IF R > MAX THEN
             BEGIN MAX:= R; K:= L END
     END;
     INFNRMVEC:= MAX
 END INFNRMVEC;
         EOP
*/


/* Calculates the infinity-norm of vector v. By default, the whole vector is traversed. You can limit this
   to an index range p to q by passing the optional pair p:q with p and q valid indices starting from 1.
   The return is the maximum absolute vector component, equal to max(<< x -> abs x >> @ v). The second return
   is the first row index for which the infinity-norm is maximal. 3.18.5

   In numal_infnrmvec, l, u, k all start from zero, not one ! Expects a valid row vector at idx. */
static FORCE_INLINE lua_Number numal_infnrmvec (lua_State *L, int idx, int l, int u, int *k) {
  lua_Number r, max;
  max = 0.0;
  for (; l <= u; l++) {
    r = fabs(agn_getinumber(L, idx, l + 1));  /* with non-numbers, sets zero */
    if (r > max) {
      max = r; *k = l;
    }
  }
  return max;
}

static int linalg_infnorm (lua_State *L) {
  int l, u, k, n, nargs;
  nargs = lua_gettop(L);
  n = checkVector(L, 1, "linalg.infnorm");
  l = 1; u = n; k = 0;
  aux_getbounds(L, 2, nargs, &l, &u, n, "column", "linalg.infnorm");
  lua_pushnumber(L, numal_infnrmvec(L, 1, l - 1, u - 1, &k));
  lua_pushinteger(L, k + 1);
  return 2;
}


/* Calculates the one-norm of vector v. By default, the whole vector is traversed. You can limit this
   to an index range p to q by passing the optional pair p:q with p and q valid indices starting from 1.
   The return is the sum of the absolute values of the vector components, equal to
   sumup(map(<< x -> |x| >>, v)). 3.18.5

   l, u must start from zero. Expects a valid row vector at idx. */
static FORCE_INLINE lua_Number numal_onenrmvec (lua_State *L, int idx, int l, int u) {
  volatile lua_Number s, cs, ccs;
  s = cs = ccs = 0.0;
  for (; l <= u; l++) {
    s = tools_kbadd(s, fabs(agn_getinumber(L, idx, l + 1)), &cs, &ccs);
  }
  return s + cs + ccs;
}

static int linalg_onenorm (lua_State *L) {
  int l, u, n, nargs;
  nargs = lua_gettop(L);
  n = checkVector(L, 1, "linalg.onenorm");
  l = 1; u = n;
  aux_getbounds(L, 2, nargs, &l, &u, n, "column", "linalg.onenorm");
  lua_pushnumber(L, numal_onenrmvec(L, 1, l - 1, u - 1));
  return 1;
}


/* Calculates the n-norm of vector v, with n a positive integer. By default, the whole vector is traversed.
   You can limit this to an index range p to q by passing the optional pair p:q with p and q valid indices
   starting from 1. The return is the sum of the absolute values of the vector components, equal to
   root(sumup(map(<< x -> |x|**n >>, v)), n). 3.18.5

   l, u must start from zero. Expects a valid row vector at idx. */
static FORCE_INLINE lua_Number numal_nnrmvec (lua_State *L, int idx, int n, int l, int u, int root) {
  volatile lua_Number s, cs, ccs;
  s = cs = ccs = 0.0;
  for (; l <= u; l++) {
    s = tools_kbadd(s, tools_intpow(fabs(agn_getinumber(L, idx, l + 1)), n), &cs, &ccs);
  }
  return (root) ? sun_pow(s + cs + ccs, 1.0/n, 0) : s + cs + ccs;
}

static int linalg_nnorm (lua_State *L) {
  int n, l, u, size, nargs;
  nargs = lua_gettop(L);
  n = agn_checkposint(L, 1);
  size = checkVector(L, 2, "linalg.nnorm");
  l = 1; u = size;
  aux_getbounds(L, 3, nargs, &l, &u, size, "column", "linalg.nnorm");
  lua_pushnumber(L, numal_nnrmvec(L, 2, n, l - 1, u - 1, 1));
  return 1;
}


/*
 FROM: NUMAL PACKAGE FOR ALGOL 60 BY THE STICHTING CENTRUM WISKUNDE & INFORMATICA,
       AMSTERDAM, THE NETHERLANDS.

 SUBSECTION: INFNRMCOL - CALCULATES THE INFINITY-NORM OF A COLUMN VECTOR.

 CALLING SEQUENCE:

     THE HEADING OF THE PROCEDURE READS:
     "REAL" "PROCEDURE" INFNRMCOL(L, U, J, K, A);
     "VALUE" L, U, J; "INTEGER" L, U, J, K; "ARRAY" A;
     "CODE" 31063;

     INFNRMCOL := MAX( ABS(A[I,J]), I= L, ..., U );

     THE MEANING OF THE FORMAL PARAMETERS IS:
     L, U:   <ARITHMETIC EXPRESSION>;
             ENTRY:THE LOWER BOUND AND UPPER BOUND OF THE ROW INDEX OF
                   THE COLUMN VECTOR A, RESPECTIVELY;
     J:      <ARITHMETIC EXPRESSION>;
             ENTRY:THE COLUMN INDEX;
     K:      <VARIABLE>;
             EXIT:THE FIRST INDEX FOR WHICH ABS(A[I,J]), I = L, ..., U,
                  IS MAXIMAL;
     A:      <ARRAY IDENTIFIER>;
             "ARRAY" A[L:U,J:J].

 PROCEDURES USED: NONE.

0CODE 31063;
  REAL PROCEDURE INFNRMCOL(L, U, J, K, A); VALUE L, U, J;
  INTEGER L, U, J, K; ARRAY A;
  BEGIN REAL R, MAX;
     MAX:= 0; K:= L;
     FOR L:= L STEP 1 UNTIL U DO
     BEGIN R:= ABS(A[L,J]); IF R > MAX THEN
         BEGIN MAX:= R; K:= L END
     END;
     INFNRMCOL:= MAX
 END INFNRMCOL
         EOP
*/
/* Calculates the infinity-norm of column vector c in matrix A. By default, all rows will be processed but you
   may limit this by passing the lower row bound p and the upper row bound q as the pair p:q for the third argument.
   The result is the maximum of the absolute values of all components in the column vector plus the first row index
   for which the infinity-norm is maximal. 3.18.5 */

void la_getrange (lua_State *L, int idx, int nargs, int *a, int *b, int m, const char *procname) {
  if (nargs >= idx && lua_ispair(L, idx)) {
    agnL_pairgetiposints(L, procname, idx, 1, a, b);
    if (*a > m)
      luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", procname, *a);
    if (*b > m)
      luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", procname, *b);
  }
}

static FORCE_INLINE void la_getrange2 (lua_State *L, int idx, int nargs, int *a, int *b, int m, const char *procname) {
  if (nargs >= idx && lua_ispair(L, idx)) {
    agnL_pairgetinonnegints(L, procname, idx, 0, a, b);
    if (*a > m)
      luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", procname, *a);
    if (*b > m)
      luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", procname, *b);
  }
}

static int linalg_infcolnorm (lua_State *L) {  /* 3.18.5, based on linalg.mcol */
  int m, n, i, k, a, b, c, d, nargs;
  lua_Number r, max;
  m = n = k = 0;
  nargs = lua_gettop(L);
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.infcolnorm", &m, &n);
  agn_poptop(L);
  a = c = 1;
  b = 0; d = m;
  (void)b;
  /* check column number */
  a = agn_checkposint(L, 2);
  if (a > n)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.infcolnorm", a);
  /* optionally get specific row(s) */
  la_getrange(L, 3, nargs, &c, &d, m, "linalg.infcolnorm");
  max = 0.0;
  for (i=c; i <= d; i++) {  /* for each row vector */
    lua_rawgeti(L, 1, i);   /* push row vector of matrix */
    checkVector(L, -1, "linalg.infcolnorm");
    r = fabs(agn_getinumber(L, -1, a));  /* get a-th component */
    if (r > max) {
      max = r; k = i;
    }
    agn_poptop(L);  /* pop row vector */
  }
  lua_pushnumber(L, max);
  lua_pushinteger(L, k);
  return 2;
}

/*
 FROM: NUMAL PACKAGE FOR ALGOL 60 BY THE STICHTING CENTRUM WISKUNDE & INFORMATICA,
       AMSTERDAM, THE NETHERLANDS.

 SUBSECTION: ONENRMCOL - CALCULATES THE ONE-NORM OF A COLUMN VECTOR.

 CALLING SEQUENCE:

     THE HEADING OF THE PROCEDURE READS:
     "REAL" "PROCEDURE" ONENRMCOL(L, U, J, A);
     "VALUE" L, U, J; "INTEGER" L, U, J; "ARRAY" A;
     "CODE" 31067;

     ONENRMCOL := SUM( ABS(A[I,J]), I= L, ..., U );

     THE MEANING OF THE FORMAL PARAMETERS IS:
     L, U:   <ARITHMETIC EXPRESSION>;
             ENTRY:THE LOWER BOUND AND UPPER BOUND OF THE ROW INDEX OF
                   THE COLUMN VECTOR A, RESPECTIVELY;
     J:      <ARITHMETIC EXPRESSION>;
             ENTRY: THE COLUMN INDEX;
     A:      <ARRAY IDENTIFIER>;
             "ARRAY" A[L:U,J:J].

 PROCEDURES USED: NONE.

0CODE 31067;
  REAL PROCEDURE ONENRMCOL(L, U, J, A); VALUE L, U, J;
  INTEGER L, U, J; ARRAY A;
  BEGIN REAL SUM;
     SUM:= 0;
     FOR L:= L STEP 1 UNTIL U DO
         SUM:= SUM + ABS(A[L,J]);
     ONENRMCOL:= SUM
     END ONENRMCOL;
         EOP
*/

/* Calculates the one-norm of column vector c in matrix A. By default, all rows will be processed but you may limit
   this by passing the lower row bound p and the upper row bound q as the pair p:q for the third argument.
   The result is the sum of the absolute values of all components in the column vector. 3.18.5

   Expects a valid _matrix_ at idx; a, c, d start from one, a is the column index, c, d are the lower and
   upper bounds of the rows. */
static FORCE_INLINE lua_Number numal_onenrmcol (lua_State *L, int idx, int a, int c, int d, const char *procname) {
  volatile lua_Number s, cs, ccs;
  int i;
  s = cs = ccs = 0.0;
  for (i=c; i <= d; i++) {   /* for each row vector in matrix ... */
    lua_rawgeti(L, idx, i);  /* ... push it */
    checkVector(L, -1, procname);
    s = tools_kbadd(s, fabs(agn_getinumber(L, -1, a)), &cs, &ccs);  /* get a-th component */
    agn_poptop(L);  /* pop row vector */
  }
  return s + cs + ccs;
}

static int linalg_onecolnorm (lua_State *L) {  /* 3.18.5, based on linalg_infcolnorm */
  int m, n, a, b, c, d, nargs;
  m = n = 0;
  nargs = lua_gettop(L);
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.onecolnorm", &m, &n);
  agn_poptop(L);
  a = c = 1;
  b = 0; d = m;
  (void)b;
  /* check column number */
  a = agn_checkposint(L, 2);
  if (a > n)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.onecolnorm", a);
  /* optionally get specific row(s) */
  la_getrange(L, 3, nargs, &c, &d, m, "linalg.onecolnorm");
  lua_pushnumber(L, numal_onenrmcol(L, 1, a, c, d, "linalg.onecolnorm"));
  return 1;
}


/* Calculates the n-norm of column vector c in matrix A, with n a positive integer. By default, all rows will be
   processed but you may limit this by passing the lower row bound p and the upper row bound q as the pair p:q
   for the fourth argument.

   The result is the sum of the absolute values, raised to the power of n, of all components in the column vector.
   The sum, finally, is taken to the n-th root before the function returns. 3.18.5

   Expects a valid _matrix_ at idx; a, c, d start from one, a is the column index, c, d are the lower and
   upper bounds of the rows. */
static FORCE_INLINE lua_Number numal_nnrmcol (lua_State *L, int idx, int n, int a, int c, int d, const char *procname) {
  volatile lua_Number s, cs, ccs;
  int i;
  s = cs = ccs = 0.0;
  for (i=c; i <= d; i++) {   /* for each row vector in matrix ... */
    lua_rawgeti(L, idx, i);  /* ... push it */
    checkVector(L, -1, procname);
    s = tools_kbadd(s, tools_intpow(fabs(agn_getinumber(L, -1, a)), n), &cs, &ccs);  /* get a-th component and raise it to the power of n */
    agn_poptop(L);  /* pop row vector */
  }
  return sun_pow(s + cs + ccs, 1.0/n, 0);
}

static int linalg_ncolnorm (lua_State *L) {  /* 3.18.5, based on linalg_infcolnorm */
  int m, n, a, b, c, d, nargs, myn;
  m = n = 0;
  nargs = lua_gettop(L);
  myn = agn_checkposint(L, 1);
  linalg_auxcheckmatrix(L, 2, 1, 0, "linalg.ncolnorm", &m, &n);
  agn_poptop(L);
  a = c = 1; b = 0; d = m;
  (void)b;
  /* check column number */
  a = agn_checkposint(L, 3);
  if (a > n)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.ncolnorm", a);
  /* optionally get specific row(s) */
  la_getrange(L, 4, nargs, &c, &d, m, "linalg.ncolnorm");
  lua_pushnumber(L, numal_nnrmcol(L, 2, myn, a, c, d, "linalg.ncolnorm"));
  return 1;
}


/*
 FROM: NUMAL PACKAGE FOR ALGOL 60 BY THE STICHTING CENTRUM WISKUNDE & INFORMATICA,
       AMSTERDAM, THE NETHERLANDS.

 SUBSECTION: INFNRMMAT - CALCULATES THE INFINITY-NORM OF A MATRIX.

 CALLING SEQUENCE:

     THE HEADING OF THE PROCEDURE READS:
     "REAL" "PROCEDURE" INFNRMMAT(LR, UR, LC, UC, KR, A);
     "VALUE" LR, UR, LC, UC; "INTEGER" LR, UR, LC, UC, KR; "ARRAY" A;
     "CODE" 31064;

     INFNRMMAT := MAX( ONENRMROW(LC, UC, I, A), I=LR, ..., UR );

     THE MEANING OF THE FORMAL PARAMETERS IS:
     LR, UR: <ARITHMETIC EXPRESSION>;
             ENTRY:THE LOWER BOUND AND UPPER BOUND OF THE ROW INDEX,
                   RESPECTIVELY;
     LC, UC: <ARITHMETIC EXPRESSION>;
             ENTRY:THE LOWER BOUND AND UPPER BOUND OF THE COLUMN INDEX,
                   RESPECTIVELY;
     KR:     <VARIABLE>;
             EXIT:THE FIRST ROW INDEX FOR WHICH THE ONE-NORM IS MAXIMAL;
     A:      <ARRAY IDENTIFIER>;
             "ARRAY" A[LR:UR,LC:UC].

 PROCEDURES USED: ONENRMROW.

0CODE 31064;
  REAL PROCEDURE INFNRMMAT(LR, UR, LC, UC, KR, A);
  VALUE LR, UR, LC, UC; INTEGER LR, UR, LC, UC, KR; ARRAY A;
  BEGIN REAL R, MAX;
     MAX:= 0; KR:= LR;
     FOR LR:= LR STEP 1 UNTIL UR DO
     BEGIN
         R:= ONENRMROW(LC, UC, LR, A);
         IF R > MAX THEN
             BEGIN MAX:= R; KR:= LR
         END
     END;
     INFNRMMAT:= MAX
 END INFNRMMAT;
         EOP
*/

static FORCE_INLINE void la_getranges (lua_State *L, int idx, int nargs, int *a, int *b, int *c, int *d,
  int m, int n, const char *procname) {
  /* optionally get specific row(s) */
  if (nargs >= idx && lua_ispair(L, idx)) {
    agnL_pairgetiposints(L, procname, idx, 1, a, b);
    if (*a > m)
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", procname, *a);
    if (*b > m)
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", procname, *b);
  }
  /* optionally get specific columns(s) */
  if (nargs == idx + 1 && lua_ispair(L, idx + 1)) {
    agnL_pairgetiposints(L, procname, idx + 1, 1, c, d);
    if (*c > n)
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", procname, *c);
    if (*d > n)
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", procname, *d);
  }
}

/* Calculates the infinity-norm of matrix A. By default the entire matrix will be processed,
   but you may limit this to rows p to q and/or columns s to t by passing the respective index
   ranges p:q, s:t as optional second and third arguments, in this order. The infinity-norm
   of a matrix is the maximum of the individual one-norms of its row vectors.
   The second return is the first row index for which the one-norm is maximal. */
static int linalg_matinfnorm (lua_State *L) {  /* 3.18.5, based on linalg.infcolnorm */
  int m, n, i, k, a, b, c, d, nargs, nopos;
  lua_Number r, max;
  m = n = k = 0;
  nargs = lua_gettop(L);
  nopos = lua_isfalse(L, nargs);
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.matinfnorm", &m, &n);
  agn_poptop(L);
  a = c = 1; b = m; d = n;
  la_getranges(L, 2, nargs, &a, &b, &c, &d, m, n, "linalg.matinfnorm");
  max = 0.0;
  for (i=a; i <= b; i++) {  /* for each row vector */
    lua_rawgeti(L, 1, i);   /* push row vector of matrix */
    checkVector(L, -1, "linalg.matinfnorm");
    r = numal_onenrmvec(L, -1, c - 1, d - 1);
    if (r > max) {
      max = r;
      k = i;
    }
    agn_poptop(L);  /* pop row vector */
  }
  lua_pushnumber(L, max);
  if (!nopos) lua_pushinteger(L, k);
  return 2 - nopos;
}


/*
 FROM: NUMAL PACKAGE FOR ALGOL 60 BY THE STICHTING CENTRUM WISKUNDE & INFORMATICA,
       AMSTERDAM, THE NETHERLANDS.

 SUBSECTION: ONENRMMAT - CALCULATES THE ONE-NORM OF A MATRIX.

 CALLING SEQUENCE:

     THE HEADING OF THE PROCEDURE READS:
     "REAL" "PROCEDURE" ONENRMMAT(LR, UR, LC, UC, KC, A);
     "VALUE" LR, UR, LC, UC; "INTEGER" LR, UR, LC, UC, KC; "ARRAY" A;
     "CODE" 31068;

     ONENRMMAT := MAX(ONENRMCOL(LR, UR, J, A), J=LC, ..., UC);

     THE MEANING OF THE FORMAL PARAMETERS IS:
     LR, UR: <ARITHMETIC EXPRESSION>;
             ENTRY:THE LOWER BOUND AND UPPER BOUND OF THE ROW INDEX,
                   RESPECTIVELY;
     LC, UC: <ARITHMETIC EXPRESSION>;
             ENTRY:THE LOWER BOUND AND UPPER BOUND OF THE COLUMN INDEX,
                   RESPECTIVELY;
     KC:     <VARIABLE>;
             EXIT:THE FIRST COLUMN INDEX FOR WHICH THE ONE-NORM IS
                  MAXIMAL;
     A:      <ARRAY IDENTIFIER>;
             "ARRAY" A[LR:UR,LC:UC].

 PROCEDURES USED: ONENRMCOL.

0CODE 31068;
  REAL PROCEDURE ONENRMMAT(LR, UR, LC, UC, KC, A);
  VALUE LR, UR, LC, UC; INTEGER LR, UR, LC, UC, KC; ARRAY A;
  BEGIN REAL MAX, R;
     MAX:= 0; KC:= LC;
     FOR LC:= LC STEP 1 UNTIL UC DO
     BEGIN
         R:= ONENRMCOL(LR, UR, LC, A);
         IF R > MAX THEN
             BEGIN MAX:= R; KC:= LC
         END
     END;
     ONENRMMAT:= MAX
 END ONENRMMAT;
         EOP
*/

/* Calculates the one-norm of matrix A. By default the entire matrix will be processed,
   but you may limit this to rows p to q and/or columns s to t by passing the respective index
   ranges p:q, s:t as optional second and third arguments, in this order. The one-norm
   of a matrix is the maximum of the individual one-norms of its column vectors. */
static int linalg_matonenorm (lua_State *L) {  /* 3.18.5, based on linalg_onecolnorm */
  int i, m, n, a, b, c, d, nargs;
  lua_Number r, max;
  m = n = 0;
  nargs = lua_gettop(L);
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.matonenorm", &m, &n);
  agn_poptop(L);
  a = c = 1; b = m; d = n;
  la_getranges(L, 2, nargs, &a, &b, &c, &d, m, n, "linalg.matonenorm");
  max = 0.0;
  for (i=c; i <= d; i++) {  /* for each column */
    r = numal_onenrmcol(L, 1, i, a, b, "linalg.matonenorm");
    if (r > max) {
      max = r;
    }
  }
  lua_pushnumber(L, max);
  return 1;
}


/* Calculates the n-norm of matrix A. By default the entire matrix will be processed,
   but you may limit this to rows p to q and/or columns s to t by passing the respective index
   ranges p:q, s:t as optional second and third arguments, in this order. With n > 1, the n-norm
   of a matrix is the sum of the n-norms of its row vectors. With n = 1, the function computes
   the one-norm of A, which is the maximum of the individual one-norms of its column vectors. */
static int linalg_matnnorm (lua_State *L) {  /* 3.18.5, based on linalg_monenorm */
  int i, m, n, a, b, c, d, nargs, myn;
  lua_Number r;
  m = n = 0;
  nargs = lua_gettop(L);
  myn = agn_checkposint(L, 1);
  linalg_auxcheckmatrix(L, 2, 1, 0, "linalg.matnnorm", &m, &n);
  agn_poptop(L);
  a = c = 1; b = m; d = n;
  la_getranges(L, 3, nargs, &a, &b, &c, &d, m, n, "linalg.matnnorm");
  switch (myn) {
    case 1: {  /* 1-norm */
      lua_Number max = 0.0;
      for (i=c; i <= d; i++) {
        r = numal_onenrmcol(L, 2, i, a, b, "linalg.matnnorm");
        if (r > max) max = r;
      }
      lua_pushnumber(L, max);
      break;
    }
    default: {  /* with myn = 2, computes the Frobenius norm, not the '2'-norm */
      lua_Number s = 0.0;
      for (i=a; i <= b; i++) {  /* for each row vector */
        lua_rawgeti(L, 2, i);   /* push row vector of matrix */
        checkVector(L, -1, "linalg.matnnorm");
        s += numal_nnrmvec(L, -1, myn, c - 1, d - 1, 0);
        agn_poptop(L);  /* pop row vector */
      }
      lua_pushnumber(L, sun_pow(s, 1.0/myn, 0));
    }
  }
  return 1;
}


/* Performs Gaussian elimination with row pivoting on any rectangular or square m x n matrix A and returns the resulting upper
   triangular matrix, the rank and the determinant of linalg.submatrix(A, 1:n). Ported from Maple V Release 4.
   See also: linalg.linsolve, linalg.gaussjord. 3.18.1.

   A := matrix(3, 3, [3, 1, 0, 0, 0, 1, 1, 2, 1]);
   linalg.gausselim(A):
   [ 3,               1, 0 ]
   [ 0, 1.6666666666667, 1 ]
   [ 0,               0, 1 ]       3       -5
*/
static int maple_length (double x, int *overflow) {
  int sizeint, sizefrac;
  if (x < 0) x = -x;
  sizeint = 1 + ((x < 1) ? -1 : sun_floor(tools_logbase(x, 10)));
  sizefrac = tools_isfrac(x)*(4 + tools_ndigplaces(x, overflow));  /* ignore overflow set as result will be zero in this case */
  return sizeint + sizefrac;
}

static int linalg_gausselim (lua_State *L) {  /* 3.18.2 */
  int i, j, m, n, r, c, overflow, nargs, matvec;
  long double *a, aic, ajc, d, t;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  nargs = lua_gettop(L);
  matvec = (nargs == 2 && agn_istableutype(L, 2, "vector"));
  linalg_auxcheckmatrix(L, 1, 1, matvec, "linalg.gausselim", &n, &m);  /* n = rowdim, m = coldim */
  agn_poptop(L);  /* pop dimension pair of the matrix */
  createarrayld(a, n*(m + matvec), "linalg.gausselim");
  if (matvec) {  /* 3.18.9 extension, for square matrices and a column vector only */
    if (checkVector(L, 2, "linalg.gausselim") != n) {
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": column vector has wrong dimension.", "linalg.gausselim");
    }
    m++;
    for (i=0; i < n; i++) {  /* for each row */
      lua_rawgeti(L, 1, i + 1);  /* push row vector on stack */
      for (j=0; j < m - 1; j++) {  /* for each column in matrix */
        a[i*m + j] = agn_getinumber(L, -1, j + 1);  /* with non-numbers, sets zero */
      }
      agn_poptop(L);
      a[i*m + j] = agn_getinumber(L, 2, i + 1);
    }
  } else {
    if (fillmatrixld(L, 1, a, n, m, 0, "linalg.gausselim")) {  /* 7.3.4 fix */
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.gausselim");
    }
  }
  d = 1.0L;
  r = 0;
  for (c=0; c < m && r < n; c++) {
    i = -1;
    for (j=r; j < n; j++) {
      ajc = a[j*m + c];
      if (ajc == 0) continue;
      if (i == -1 ||
          maple_length(ajc, &overflow) < maple_length(a[i*m + c], &overflow)) {  /* I really wonder what the calls to length are good for ... */
        i = j;
      }
    }
    if (i != -1) {
      if (i != r) {
        d = -d;
        for (j=c; j < m; j++) {
          t = a[i*m + j];
          a[i*m + j] = a[r*m + j];
          a[r*m + j] = t;
        }
      }
      d *= a[r*m + c];
      for (i=r + 1; i < n; i++) {
        aic = a[i*m + c];
        if (aic == 0) continue;
        t = aic/a[r*m + c];
        for (j=c; j < m; j++) {
          a[i*m + j] -= t*a[r*m + j];
        }
        a[i*m + c] = 0;
      }
      r++;
    }
  }
  luaL_checkstack(L, 3, "not enough stack space");
  creatematrixld(L, a, n, m);
  lua_pushinteger(L, r);  /* rank */
  lua_pushnumber(L, n == r ? d : 0);  /* determinant */
  xfree(a);
  return 3;
}


/* Computes the rank of any m x n matrix, by performing Gaussian elimination on the rows of the given matrix.
   The rank of the matrix A is the number of non-zero rows in the resulting matrix. 3.18.2 */
static int linalg_rank (lua_State *L) {  /* 3.18.2 */
  int i, j, m, n, r, c, overflow;
  long double *a, aic, ajc, d, t;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.rank", &n, &m);  /* n = rowdim, m = coldim */
  agn_poptop(L);  /* pop dimension pair of the matrix */
  createarrayld(a, m*n, "linalg.rank");
  if (fillmatrixld(L, 1, a, n, m, 0, "linalg.rank")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.rank");
  }
  d = 1.0L;
  r = 0;
  for (c=0; c < m && r < n; c++) {
    i = -1;
    for (j=r; j < n; j++) {
      ajc = a[j*m + c];
      if (ajc == 0) continue;
      if (i == -1 ||
          maple_length(ajc, &overflow) < maple_length(a[i*m + c], &overflow)) {  /* I really wonder what the calls to length are good for ... */
        i = j;
      }
    }
    if (i != -1) {
      if (i != r) {
        d = -d;
        for (j=c; j < m; j++) {
          t = a[i*m + j];
          a[i*m + j] = a[r*m + j];
          a[r*m + j] = t;
        }
      }
      d *= a[r*m + c];
      for (i=r + 1; i < n; i++) {
        aic = a[i*m + c];
        if (aic == 0) continue;
        t = aic/a[r*m + c];
        for (j=c; j < m; j++) {
          a[i*m + j] -= t*a[r*m + j];
        }
        a[i*m + c] = 0;
      }
      r++;
    }
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushinteger(L, r);  /* rank */
  lua_pushnumber(L, n == r ? d : 0);  /* determinant */
  xfree(a);
  return 2;
}


/* Performs Gauss-Jordan elimination with partial pivoting on any rectangular or square m x n matrix A and returns the resulting upper
   triangular matrix, the rank and the determinant of linalg.submatrix(A, 1:n). Ported from Maple V Release 4. 3.18.1.
   See also: linalg.linsolve, linalg.gaussjord.

   A := matrix([[4, -6, 1, 0], [-6, 12, 0, 1], [-2, 6, 1, 1]]);
   linalg.gaussjord(A):
   [ 1, 0,   1,              0.5 ]
   [ 0, 1, 0.5, 0.33333333333333 ]
   [ 0, 0,   0,                0 ] 2       0
*/
static int linalg_gaussjord (lua_State *L) {  /* 3.18.2 */
  int i, j, m, n, r, c, overflow, nargs, matvec;
  long double *a, ajc, d, t;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  nargs = lua_gettop(L);
  matvec = (nargs == 2 && agn_istableutype(L, 2, "vector"));
  linalg_auxcheckmatrix(L, 1, 1, matvec, "linalg.gaussjord", &n, &m);  /* n = rowdim, m = coldim */
  agn_poptop(L);  /* pop dimension pair of the matrix */
  createarrayld(a, n*(m + matvec), "linalg.gaussjord");
  if (matvec) {  /* 3.18.9 extension, for square matrices and a column vector only */
    if (checkVector(L, 2, "linalg.gausselim") != n) {
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": column vector has wrong dimension.", "linalg.gaussjord");
    }
    m++;
    for (i=0; i < n; i++) {  /* for each row */
      lua_rawgeti(L, 1, i + 1);  /* push row vector on stack */
      for (j=0; j < m - 1; j++) {  /* for each column in matrix */
        a[i*m + j] = agn_getinumber(L, -1, j + 1);  /* with non-numbers, sets zero */
      }
      agn_poptop(L);
      a[i*m + j] = agn_getinumber(L, 2, i + 1);
    }
  } else {
    if (fillmatrixld(L, 1, a, n, m, 0, "linalg.gaussjord")) {  /* 7.3.4 fix */
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.gaussjord");
    }
  }
  d = 1.0L;
  r = 0;
  for (c=0; c < m && r < n; c++) {
    i = -1;
    for (j=r; j < n; j++) {
      ajc = a[j*m + c];
      if (ajc == 0) continue;
      if (i == -1 ||
          maple_length(ajc, &overflow) < maple_length(a[i*m + c], &overflow)) {  /* I really wonder what the calls to length are good for ... */
        i = j;
      }
    }
    if (i != -1) {
      if (i != r) {
        d = -d;
        for (j=c; j < m; j++) {
          t = a[i*m + j];
          a[i*m + j] = a[r*m + j];
          a[r*m + j] = t;
        }
      }
      for (j=c + 1; j < m; j++) {
        a[r*m + j] /= a[r*m + c];
      }
      d *= a[r*m + c];
      a[r*m + c] = 1;
      for (i=0; i < n; i++) {
        if (i == r || a[i*m + c] == 0) continue;
        for (j=c + 1; j < m; j++) {
          a[i*m + j] -= a[i*m + c]*a[r*m + j];
        }
        a[i*m + c] = 0;
      }
      r++;
    }
  }
  luaL_checkstack(L, 3, "not enough stack space");
  creatematrixld(L, a, n, m);
  lua_pushinteger(L, r);  /* rank */
  lua_pushnumber(L, n == r ? d : 0);  /* determinant */
  xfree(a);
  return 3;
}


/* Taken from: https://github.com/JeffBezanson/numal/blob/master/newnumal5p1.txt#L2339

 FROM: NUMAL PACKAGE FOR ALGOL 60 BY THE STICHTING CENTRUM WISKUNDE & INFORMATICA,
       AMSTERDAM, THE NETHERLANDS.

MATTAM :=  SCALAR  PRODUCT  OF  THE  ROW  VECTORS  GIVEN  IN  ARRAY
     A[I:I,L:U] AND ARRAY B[J:J, L:U].

 SUBSECTION: MATTAM.

 CALLING SEQUENCE:

     HEADING:
     REAL PROCEDURE MATTAM(L, U, I, J, A, B); VALUE L,U,I,J;
     INTEGER L,U,I,J; ARRAY A,B;
     CODE 34015;

     FORMAL PARAMETERS:
     L,U:    <ARITHMETIC EXPRESSION>;
             LOWER AND UPPER BOUND OF THE RUNNING SUBSCRIPT;
     I,J:    <ARITHMETIC EXPRESSION>;
             ROW-INDICES  OF  THE  ROW VECTORS  A  AND  B, RESPECTIVELY;
     A,B:    <ARRAY IDENTIFIER>;
             ARRAY A[I : I, L : U], B[J : J, L : U].

0CODE 34015;
    REAL PROCEDURE MATTAM(L, U, I, J, A, B); VALUE L,U,I,J;
    INTEGER L,U,I,J; ARRAY A,B;
    BEGIN INTEGER K; REAL S;
        S:= 0;
        FOR K:=L STEP 1 UNTIL U DO
           S:= A[I,K] * B[J,K] + S;
        MATTAM:= S
    END MATTAM;
        EOP
*/

/* l, u both starting from one, i, j are the positions of the row vectors, starting from one; idx1 & idx2 are matrices; 3.18.6 */
static FORCE_INLINE lua_Number numal_mattam (lua_State *L, int idx1, int idx2, int l, int u, int i, int j, const char *procname) {
  volatile lua_Number s, cs, ccs;
  luaL_checkstack(L, 2, "not enough stack space");
  s = cs = ccs = 0.0;
  lua_rawgeti(L, idx1, i);  /* push row vector #i in matrix idx1 */
  checkVector(L, -1, procname);
  lua_rawgeti(L, idx2, j);  /* push row vector #j in matrix idx2 */
  checkVector(L, -1, procname);
  for (; l <= u; l++) {
    s = tools_kbadd(s, agn_getinumber(L, -2, l)*agn_getinumber(L, -1, l), &cs, &ccs);
  }
  agn_poptoptwo(L);
  return s + cs + ccs;
}

/* The function computes the scalar product of the row vector i in matrix A and the row vector in matrix B. You can limit this
   to an index range p to q in the two rows by passing the optional pair p:q with p and q valid indices starting from 1.
   3.18.6 */
static int linalg_mattam (lua_State *L) {
  int m, n, p, q, i, j, l, u, nargs;
  nargs = lua_gettop(L);
  m = n = 0;  /* just to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.mattam", &m, &n);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  linalg_auxcheckmatrix(L, 2, 1, 1, "linalg.mattam", &p, &q);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  if (m != p || n != q)
    luaL_error(L, "Error in " LUA_QS ": matrix dimensions are incompatible.", "linalg.mattam");
  i = agn_checkposint(L, 3);
  j = agn_checkposint(L, 4);
  if (i > m)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.mattam", i);
  if (j > m)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.mattam", j);
  l = 1; u = m;
  la_getrange2(L, 5, nargs, &l, &u, m, "linalg.mattam");
  lua_pushnumber(L, numal_mattam(L, 1, 2, l, u, i, j, "linalg.mattam"));
  return 1;
}


/* Taken from: https://github.com/JeffBezanson/numal/blob/master/newnumal5p1.txt#L2339

     MATMAT:= SCALAR PRODUCT OF THE ROW VECTOR GIVEN IN ARRAY A[I:I,L:U]
     AND THE COLUMN VECTOR IN ARRAY B[L:U, J:J].

 SUBSECTION: MATMAT.

 CALLING SEQUENCE:

     HEADING:
     REAL PROCEDURE MATMAT(L, U, I, J, A, B); VALUE L,U,I,J;
     INTEGER L,U,I,J; ARRAY A,B;
     CODE 34013;

     FORMAL PARAMETERS:
     L,U:    <ARITHMETIC EXPRESSION>;
             LOWER AND UPPER BOUND OF THE RUNNING SUBSCRIPT;
     I,J:    <ARITHMETIC EXPRESSION>;
             ROW-INDEX  OF  THE ROW VECTOR  A  AND  COLUMN-INDEX  OF THE
             COLUMN VECTOR B;
     A,B:    <ARRAY IDENTIFIER>;
             ARRAY A[I : I, L : U], B[L : U, J : J].

 LANGUAGE: COMPASS.

0CODE 34013;
    REAL PROCEDURE MATMAT(L, U, I, J, A, B); VALUE L,U,I,J;
    INTEGER L,U,I,J; ARRAY A,B;
    BEGIN INTEGER K; REAL S;
        S:= 0;
        FOR K:=L STEP 1 UNTIL U DO
            S:= A[I,K] * B[K,J] + S;
        MATMAT:= S
    END MATMAT
*/

/* l, u both starting from zero, i is the position of the row vector, j the position of the column vector,
   both starting from one; idx1 and idx2 are the stack indices of the two matrices; 3.18.6 */
static FORCE_INLINE lua_Number numal_matmat (lua_State *L, int idx1, int idx2, int l, int u, int i, int j, const char *procname) {
  lua_Number s, cs, ccs;
  luaL_checkstack(L, 2, "not enough stack space");
  s = cs = ccs = 0.0;
  lua_rawgeti(L, idx1, i);  /* push i-th row vector in matrix at idx1 */
  checkVector(L, -1, procname);
  for (; l <= u; l++) {
    lua_rawgeti(L, idx2, l + 1);  /* push k-th row vector in matrix at idx2 and get j-th element next */
    checkVector(L, -1, procname);
    s = tools_kbadd(s, agn_getinumber(L, -2, l + 1)*agn_getinumber(L, -1, j), &cs, &ccs);
    agn_poptop(L);
  }
  agn_poptop(L);
  return s + cs + ccs;
}

/* The function computes the scalar product of the i-th row vector in matrix A and the j-th column vector in
   matrix B. You can limit this to an index range p to q in the two vectors by passing the optional pair p:q
   with p and q valid indices starting from 1. A must be an m x n matrix and B an m x n matrix, or be both
   square. 3.18.6 */
static int linalg_matmat (lua_State *L) {
  int m, n, p, q, i, j, l, u, nargs;
  nargs = lua_gettop(L);
  m = n = p = q = 0;  /* just to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.matmat", &m, &n);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  linalg_auxcheckmatrix(L, 2, 1, 0, "linalg.matmat", &p, &q);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  if (m != q || n != p)
    luaL_error(L, "Error in " LUA_QS ": matrix dimensions are incompatible.", "linalg.matmat");
  i = agn_checkposint(L, 3);
  j = agn_checkposint(L, 4);
  if (i > m)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.matmat", i);
  if (j > m)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.matmat", j);
  l = 1; u = n;
  la_getrange2(L, 5, nargs, &l, &u, m, "linalg.matmat");
  lua_pushnumber(L, numal_matmat(L, 1, 2, l - 1, u - 1, i, j, "linalg.matmat"));
  return 1;
}


/*
 ICHROW INTERCHANGES THE ELEMENTS OF THE ROW VECTORS GIVEN IN  ARRAY
 A[I:I, L:U] AND ARRAY A[J:J, L:U].

 SUBSECTION: ICHROW.

 CALLING SEQUENCE:

     HEADING:
     PROCEDURE ICHROW(L, U, I, J, A); VALUE L,U,I,J;
     INTEGER L,U,I,J; ARRAY A;
     CODE 34032;

     FORMAL PARAMETERS:
     L,U:    <ARITHMETIC EXPRESSION>;
             LOWER AND UPPER BOUND OF THE RUNNING SUBSCRIPT;
     I,J:    <ARITHMETIC EXPRESSION>;
             ROW-INDICES OF THE ROW VECTORS OF ARRAY A;
     A:      <ARRAY IDENTIFIER>;
             ARRAY A[P : Q, L : U]; P AND Q SHOULD SATISFY:  P  <=  I,
             P <= J, Q >= I AND Q >= J.

 LANGUAGE: COMPASS.

CODE 34032;
     PROCEDURE ICHROW(L, U, I, J, A); VALUE L,U,I,J;
     INTEGER L,U,I,J; ARRAY A;
     BEGIN REAL R;
         FOR L:= L STEP 1 UNTIL U DO BEGIN
             R:= A[I,L];
             A[I,L]:= A[J,L];
             A[J,L]:= R
         END
     END ICHROW
*/

/* l, u, i, j from zero */
void numal_ichrow (lua_State *L, lua_Number *a, int n, int l, int u, int i, int j) {
  lua_Number t;
  for (; l <= u; l++) {
    t = a[i*n + l];
    a[i*n + l] = a[j*n + l];
    a[j*n + l] = t;
  }
}

/* By default, swaps row p in matrix A with row q. p, q must be positive integers. The result is a new matrix.

   If the very last argument is the Boolean `true`, then the operation is done in-place, modifying A. In this
   mode, the modified matrix A is returned, as well.

   Whether in-place or not, you may limit the exchange of the matrix elements to columns s to t by passing the
   respective index range s:t as an optional fourth argument. 3.18.6 */
static int linalg_swaprow (lua_State *L) {
  int i, j, l, u, m, n, nargs, inplace, rc1, rc2;
  nargs = lua_gettop(L);
  m = n = inplace = 0;
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.swaprow", &m, &n);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  i = agn_checkposint(L, 2);
  j = agn_checkposint(L, 3);
  if (i > m)
    luaL_error(L, "Error in " LUA_QS ": row index %d out of range.", "linalg.swaprow", i);
  if (j > m)
    luaL_error(L, "Error in " LUA_QS ": row index %d out of range.", "linalg.swaprow", j);
  l = 1; u = n;
  if (lua_istrue(L, nargs)) {
    inplace = 1;
    nargs--;
  }
  la_getrange(L, 4, nargs, &l, &u, n, "linalg.swaprow");
  if (inplace) {
    luaL_checkstack(L, 2, "not enough stack space");
    if (l == 1 && u == n) {
      lua_rawgeti(L, 1, i);  /* push i-th row vector in matrix at idx */
      checkVector(L, -1, "linalg.swaprow");
      lua_rawgeti(L, 1, j);  /* push j-th row vector in matrix at idx */
      checkVector(L, -1, "linalg.swaprow");
      lua_rawseti(L, 1, i);  /* set j-th row to index i */
      lua_rawseti(L, 1, j);  /* set i-th row to index j */
    } else {
      lua_Number x, y;
      lua_rawgeti(L, 1, i);  /* push i-th row vector in matrix at idx */
      checkVector(L, -1, "linalg.swaprow");
      lua_rawgeti(L, 1, j);  /* push j-th row vector in matrix at idx */
      checkVector(L, -1, "linalg.swaprow");
      for (; l <= u; l++) {
        x = agn_rawgetinumber(L, -2, l, &rc1);
        y = agn_rawgetinumber(L, -1, l, &rc2);
        if (rc2) lua_rawsetinumber(L, -2, l, y);  /* 3.20.4 change */
        if (rc1) lua_rawsetinumber(L, -1, l, x);  /* ditto */
      }
      lua_rawseti(L, 1, j);
      lua_rawseti(L, 1, i);
    }
    lua_settop(L, 1);  /* return modified matrix */
  } else {
    lua_Number *a;
    la_createarray(L, a, m*n, "linalg.swaprow");
    if (fillmatrix(L, 1, a, m, n, 0, "linalg.swaprow")) {  /* 7.3.4 fix */
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.swaprow");
    }
    numal_ichrow(L, a,n,  l - 1, u - 1, i - 1, j - 1);
    creatematrix(L, a, m, n, 1);
    xfree(a);
  }
  return 1;
}


/*
 ICHCOL  INTERCHANGES  THE ELEMENTS  OF THE COLUMN VECTORS  GIVEN IN
 ARRAY A[L:U, I:I] AND ARRAY A[L:U, J:J].

 SUBSECTION: ICHCOL.

 CALLING SEQUENCE:

     HEADING:
     PROCEDURE ICHCOL(L, U, I, J, A); VALUE L,U,I,J;
     INTEGER L,U,I,J; ARRAY A;
     CODE 34031;

     FORMAL PARAMETERS:
     L,U:    <ARITHMETIC EXPRESSION>;
             LOWER AND UPPER BOUND OF THE RUNNING SUBSCRIPT;
     I,J:    <ARITHMETIC EXPRESSION>;
             COLUMN-INDICES   OF   THE  COLUMN  VECTORS   OF   ARRAY  A;
     A:      <ARRAY IDENTIFIER>;
             ARRAY A[L : U, P : Q]; P AND Q SHOULD SATISFY:  P  <=  I,
             P <= J, Q >= I AND Q >= J.

 LANGUAGE: COMPASS.

 CODE 34031;
     PROCEDURE ICHCOL(L, U, I, J, A); VALUE L,U,I,J;
     INTEGER L,U,I,J; ARRAY A;
     BEGIN REAL R;
         FOR L:= L STEP 1 UNTIL U DO BEGIN
             R:= A[L,I];
             A[L,I]:= A[L,J];
             A[L,J]:= R
         END
     END ICHCOL;
         EOP
*/
/* l, u, i, j from zero */
void numal_ichcol (lua_State *L, lua_Number *a, int n, int l, int u, int i, int j) {
  lua_Number t;
  for (; l <= u; l++) {
    t = a[l*n + i];
    a[l*n + i] = a[l*n + j];
    a[l*n + j] = t;
  }
}

static int linalg_swapcol (lua_State *L) {
  int i, j, l, u, m, n, nargs, inplace, rc1, rc2;
  nargs = lua_gettop(L);
  m = n = inplace = 0;
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.swapcol", &m, &n);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  i = agn_checkposint(L, 2);
  j = agn_checkposint(L, 3);
  if (i > n)
    luaL_error(L, "Error in " LUA_QS ": column index %d out of range.", "linalg.swapcol", i);
  if (j > n)
    luaL_error(L, "Error in " LUA_QS ": column index %d out of range.", "linalg.swapcol", j);
  l = 1; u = m;
  if (lua_istrue(L, nargs)) {
    inplace = 1;
    nargs--;
  }
  la_getrange(L, 4, nargs, &l, &u, m, "linalg.swapcol");
  if (inplace) {
    lua_Number x, y;
    luaL_checkstack(L, 2, "not enough stack space");
    for (; l <= u; l++) {
      lua_rawgeti(L, 1, l);  /* push l-th row vector in matrix at idx */
      checkVector(L, -1, "linalg.swapcol");
      x = agn_rawgetinumber(L, -1, i, &rc1);
      y = agn_rawgetinumber(L, -1, j, &rc2);
      if (rc2) lua_rawsetinumber(L, -1, i, y);  /* 3.20.4 change */
      if (rc1) lua_rawsetinumber(L, -1, j, x);  /* 3.20.4 change */
      lua_rawseti(L, 1, l);
    }
    lua_settop(L, 1);
  } else {
    lua_Number *a;
    la_createarray(L, a, m*n, "linalg.swapcol");
    if (fillmatrix(L, 1, a, m, n, 0, "linalg.swapcol")) {  /* 7.3.4 fix */
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.swapcol");
    }
    numal_ichcol(L, a,n,  l - 1, u - 1, i - 1, j - 1);
    creatematrix(L, a, m, n, 1);
    xfree(a);
  }
  return 1;
}


static int linalg_vector (lua_State *L) {
  int i, nops, type;
  nops = lua_gettop(L);
  if (nops == 0)
    luaL_error(L, "Error in " LUA_QS ": at least one component expected.", "linalg.vector");
  luaL_checkstack(L, nops, "too many elements");
  type = lua_type(L, 1);
  if (nops == 2 && type == LUA_TNUMBER && lua_type(L, 2) == LUA_TTABLE) {  /* Maple-like syntax */
    int c, key;
    c = 0;
    nops = agnL_checkinteger(L, 1);
    luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.2 fix */
    lua_newtable(L);
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) {
      c++;
      luaL_argcheck(L, lua_type(L, -1) == LUA_TNUMBER && lua_type(L, -2) == LUA_TNUMBER, c,
        "expected a number in " LUA_QL("linalg.vector"));
      key = (int)agn_tonumber(L, -2);
      if (key < 1 || key > nops)
        luaL_error(L, "Error in " LUA_QS ": table index is out of range.", "linalg.vector");
      lua_rawset2(L, -3);  /* deletes only the value, but not the key */
    }
    if (c > nops)
      luaL_error(L, "Error in " LUA_QS ": more entries passed than given dimension.", "linalg.vector");
  } else if (type == LUA_TNUMBER || type == LUA_TCOMPLEX || type == LUA_TUSERDATA) {  /* assume all arguments are (complex) numbers */
    lua_createtable(L, nops, 1);
    for (i=1; i <= nops; i++) {
      if (lua_isnumber(L, i) || lua_iscomplex(L, i) || isdlong(L, 3) || ismapm(L, 3) || iscmapm(L, 3) ||
          luaL_isudata(L, 3, "mpfr") || luaL_isudata(L, 3, "cmpfr") ||
          luaL_isudata(L, 3, "dd")) {
        lua_pushvalue(L, i);
        lua_rawseti(L, -2, i);
      } else {
        luaL_error(L, "Error in " LUA_QS ": vector component #%d is invalid.", "linalg.vector", i);
      }
    }
  } else if (type == LUA_TTABLE) {
    nops = luaL_getn(L, 1);
    if (nops == 0)
      luaL_error(L, "Error in " LUA_QS ": at least one table value expected.", "linalg.vector");
    luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
    lua_createtable(L, nops, 1);
    for (i=1; i <= nops; i++) {
      lua_rawgeti(L, 1, i);
      if (lua_isnumber(L, -1) || lua_iscomplex(L, -1) || isdlong(L, -1) || ismapm(L, -1) || iscmapm(L, -1) ||
          luaL_isudata(L, -1, "mpfr") || luaL_isudata(L, -1, "cmpfr") || luaL_isudata(L, -1, "dd")) {
        lua_rawseti(L, -2, i);
      } else {
        luaL_error(L, "Error in " LUA_QS ": vector component #%d is invalid.", "linalg.vector", i);
      }
    }
  } else if ((type == LUA_TSEQ && !agn_isutypeset(L, 1)) ||
             (type == LUA_TREG && !agn_isutypeset(L, 1))  ) {  /* a sequence and no user-defined type set ? */
    nops = (type == LUA_TSEQ) ? agn_seqsize(L, 1) : agn_regsize(L, 1);
    if (nops == 0)
      luaL_error(L, "Error in " LUA_QS ": at least one sequence value expected.", "linalg.vector");
    luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
    lua_createtable(L, nops, 1);
    for (i=1; i <= nops; i++) {
      if (type == LUA_TSEQ) {
        lua_seqrawgeti(L, 1, i);
      } else {
        agn_regrawgeti(L, 1, i);
      }
      if (lua_isnumber(L, -1) || lua_iscomplex(L, -1) || isdlong(L, -1) || ismapm(L, -1) || iscmapm(L, -1) ||
          luaL_isudata(L, -1, "mpfr") || luaL_isudata(L, -1, "cmpfr") || luaL_isudata(L, -1, "dd")) {
        lua_rawseti(L, -2, i);
      } else {
        luaL_error(L, "Error in " LUA_QS ": vector component #%d is invalid.", "linalg.vector", i);
      }
    }
  } else if (type == LUA_TSTRING) {  /* 3.4.8 */
    char *tofree, *token, *str;
    const char *chr;
    int i, rc;
    size_t n;
    rc = i = 0;
    chr = lua_tolstring(L, 1, &n);
    if (n == 0)
      luaL_error(L, "Error in " LUA_QS ": string is empty.", "linalg.vector");
    tofree = str = tools_strdup(chr);  /* chain it so that memory gets properly freed later on */
    if (str == NULL)
      luaL_error(L, "Error in " LUA_QS ": could not convert string.", "linalg.vector");
    luaL_checkstack(L, 2, "not enough stack space");
    lua_createtable(L, 0, 1);
    while ((token = tools_strsep(&str, " ", &n)) != NULL) {
      if (n == 0) continue;
      lua_pushlstring(L, token, n);
      if (agnL_strtonumber(L, -1))
        lua_rawseti(L, -2, ++i);
      else {
        agn_poptop(L);  /* pop non-number */
        rc = 1;
        break;
      }
    }
    xfree(tofree);
    if (rc || !i) {
      agn_poptop(L);  /* pop table */
      luaL_error(L, "Error in " LUA_QS ": string does not represent numbers.", "linalg.vector");
    }
    nops = i;
  } else
    luaL_error(L, "Error in " LUA_QS ": numbers, a table or sequence expected.", "linalg.vector");
  /* set vector attributes */
  setvattribs(L, nops);
  return 1;
}


/* Works like `linalg.vector` but returns a column vector of dimension m, that is an m x 1 matrix. */
static int linalg_colvector (lua_State *L) {
  int i, nops, type;
  nops = lua_gettop(L);
  if (nops == 0)
    luaL_error(L, "Error in " LUA_QS ": at least one component expected.", "linalg.colvector");
  luaL_checkstack(L, nops, "too many elements");
  type = lua_type(L, 1);
  if (nops == 2 && type == LUA_TNUMBER && lua_type(L, 2) == LUA_TTABLE) {  /* Maple-like syntax */
    int c, key;
    c = 0;
    nops = agnL_checkinteger(L, 1);
    luaL_checkstack(L, 3, "not enough stack space");
    createrawmatrix(L, nops, 1);
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) {
      c++;
      luaL_argcheck(L, lua_type(L, -1) == LUA_TNUMBER && lua_type(L, -2) == LUA_TNUMBER, c,
        "expected a number in " LUA_QL("linalg.colvector"));
      key = (int)agn_tonumber(L, -2);
      if (key < 1 || key > nops)
        luaL_error(L, "Error in " LUA_QS ": table index is out of range.", "linalg.colvector");
      luaL_checkstack(L, 1, "not enough stack space");
      lua_rawgeti(L, -3, key);  /* push empty row vector */
      agn_setinumber(L, -1, 1, agn_tonumber(L, -2));
      agn_poptoptwo(L);  /* pop row vector and value */
    }
    if (c > nops)
      luaL_error(L, "Error in " LUA_QS ": more entries passed than given dimension.", "linalg.colvector");
  } else if (type == LUA_TNUMBER || type == LUA_TCOMPLEX) {  /* assume all arguments are (complex) numbers */
    createrawmatrix(L, nops, 1);
    for (i=1; i <= nops; i++) {
      lua_rawgeti(L, -1, i);  /* push empty row vector */
      if (lua_iscomplex(L, i)) {
        lua_pushvalue(L, i);
        lua_rawseti(L, -2, 1);  /* set complex number into row vector at position 1 */
      } else {
        agn_setinumber(L, -1, 1, agn_checknumber(L, i));
      }
      agn_poptop(L);
    }
  } else if (type == LUA_TTABLE) {
    nops = luaL_getn(L, 1);
    if (nops == 0)
      luaL_error(L, "Error in " LUA_QS ": at least one table value expected.", "linalg.colvector");
    luaL_checkstack(L, 3, "not enough stack space");
    createrawmatrix(L, nops, 1);
    for (i=1; i <= nops; i++) {
      lua_rawgeti(L, -1, i);  /* push empty row vector */
      lua_rawgeti(L, 1, i);  /* push table element */
      if (lua_iscomplex(L, -1)) {  /* set element into row vector */
        lua_rawseti(L, -2, 1);
      } else {
        agn_setinumber(L, -2, 1, agn_checknumber(L, -1));
        agn_poptop(L);  /* pop table element */
      }
      agn_poptop(L);  /* pop row vector */
    }
  } else if (type == LUA_TSEQ && !agn_isutypeset(L, 1)) {  /* a sequence and no user-defined type set ? */
    nops = agn_seqsize(L, 1);
    if (nops == 0)
      luaL_error(L, "Error in " LUA_QS ": at least one sequence value expected.", "linalg.colvector");
    luaL_checkstack(L, 3, "not enough stack space");
    createrawmatrix(L, nops, 1);
    for (i=1; i <= nops; i++) {
      lua_rawgeti(L, -1, i);  /* push empty row vector */
      lua_seqrawgeti(L, 1, i);
      if (lua_iscomplex(L, -1)) {
        lua_rawseti(L, -2, 1);
      } else {
        agn_setinumber(L, -2, 1, agn_checknumber(L, -1));
        agn_poptop(L);  /* pop sequence element */
      }
      agn_poptop(L);  /* pop row vector */
    }
  } else if (type == LUA_TREG && !agn_isutypeset(L, 1)) {  /* a register and no user-defined type set ? */
    nops = agn_regsize(L, 1);
    if (nops == 0)
      luaL_error(L, "Error in " LUA_QS ": at least one register value expected.", "linalg.colvector");
    luaL_checkstack(L, 3, "not enough stack space");
    createrawmatrix(L, nops, 1);
    for (i=1; i <= nops; i++) {
      lua_rawgeti(L, -1, i);  /* push empty row vector */
      agn_regrawgeti(L, 1, i);
      if (lua_iscomplex(L, -1)) {
        lua_rawseti(L, -2, 1);
      } else {
        agn_setinumber(L, -2, 1, agn_checknumber(L, -1));
        agn_poptop(L);  /* pop register element */
      }
      agn_poptop(L);  /* pop row vector */
    }
  } else if (type == LUA_TSTRING) {
    char *tofree, *token, *str;
    const char *chr;
    int i, rc;
    size_t n;
    rc = i = 0;
    chr = lua_tolstring(L, 1, &n);
    if (n == 0)
      luaL_error(L, "Error in " LUA_QS ": string is empty.", "linalg.colvector");
    tofree = str = tools_strdup(chr);  /* chain it so that memory gets properly freed later on */
    if (str == NULL)
      luaL_error(L, "Error in " LUA_QS ": could not convert string.", "linalg.colvector");
    luaL_checkstack(L, 2, "not enough stack space");
    lua_createtable(L, 0, 1);
    while ((token = tools_strsep(&str, " ", &n)) != NULL) {
      if (n == 0) continue;
      lua_createtable(L, 1, 1);  /* new row vector */
      lua_pushlstring(L, token, n);
      if (agnL_strtonumber(L, -1)) {
        lua_rawseti(L, -2, 1);
        setvattribs(L, 1);
        lua_rawseti(L, -2, ++i);
      } else {
        agn_poptoptwo(L);  /* pop non-number and row vector */
        rc = 1;
        break;
      }
    }
    xfree(tofree);
    if (rc || !i) {
      agn_poptop(L);  /* pop matrix */
      luaL_error(L, "Error in " LUA_QS ": string does not represent numbers.", "linalg.colvector");
    }
    nops = i;
    setmattribs(L, nops, 1);
  } else
    luaL_error(L, "Error in " LUA_QS ": numbers, a table or sequence expected.", "linalg.colvector");
  /* set matrix attributes */
  setmattribs(L, nops, 1);
  return 1;
}


static int linalg_vzero (lua_State *L) {  /* changed 2.1.3, renamed 2.16.0, renamed 3.17.4; rewritten 4.1.1 */
  int i, n, nargs, columnv, sparse;
  nargs = lua_gettop(L);
  columnv = sparse = 0;
  n = agn_checkposint(L, 1);  /* 4.1.1 change */
  aux_checkvmoptions(L, 2, &nargs, &sparse, &columnv, "linalg.vzero");  /* 4.1.1 change */
  luaL_checkstack(L, 1 + columnv*(!sparse), "not enough stack space");
  if (columnv) {
    createrawmatrix(L, n, 1);
    for (i=1; !sparse && i <= n; i++) {
      lua_rawgeti(L, -1, i);  /* push empty row vector */
      agn_setinumber(L, -1, 1, 0);
      agn_poptop(L);  /* pop row vector */
    }
  } else {
    lua_createtable(L, n, 1);
    for (i=1; !sparse && i <= n; i++)
      agn_setinumber(L, -1, i, 0);
    /* set vector attributes */
    setvattribs(L, n);
  }
  return 1;
}


/* Creates a new matrix with m rows and n columns. If def is not undefined, explicitly sets def into each row vector.
   Leaves the new matrix on the stack top. 3.18.0 */
static void aux_newmatrix (lua_State *L, int m, int n, lua_Number def) {
  int i, j;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_createtable(L, m, 1);
  for (i=0; i < m; i++) {
    lua_createtable(L, n, 1);  /* create new row vector */
    if (!isnan(def)) {  /* explicitly set vector component */
      for (j=0; j < n; j++) {
        agn_setinumber(L, -1, j + 1, def);
      }
    }
    setvattribs(L, n);
    lua_rawseti(L, -2, i + 1);
  }
  setmattribs(L, m, n);
  /* leaves new matrix on the stack top */
}

static int linalg_newmatrix (lua_State *L) {
  int m, n;
  m = agn_checkposint(L, 1);
  n = agn_checkposint(L, 2);
  aux_newmatrix(L, m, n, agnL_optnumber(L, 3, AGN_NAN));
  return 1;
}


/* Creates an m x n matrix of ones.
   In the first form, if n is not given, it is set to m.
   In the second form, the pair m:n is denoting the row dimension m and column dimension n.
   In the third form, takes a matrix M and uses its dimensions to create the new matrix.0
   See also: `linalg.identity`, `linalg.zeros`. 3.17.4; slimmed-down 4.1.1 */
static int linalg_ones (lua_State *L) {  /* 3.17.4 */
  lua_Number *a;
  int i, m, n, nargs;
  m = n = 0;
  nargs = lua_gettop(L);
  aux_getdims(L, nargs, &m, &n, "linalg.ones");
  la_createarray(L, a, m * n, "linalg.ones");
  for (i=0; i < m * n; i++) a[i] = 1;
  creatematrix(L, a, m, n, 0);  /* a matrix with ones cannot be sparse */
  xfree(a);
  return 1;
}


/* By default, creates an m x n sparse matrix, with unset elements implicitly representing zero.
   In the first form, if n is not given, it is set to m. If the optional third argument is set to `false`,
   sets zeros explicitly into all the row vectors.
   In the second form, the pair m:n is denoting the row dimension m and column dimension n.
   In the third form, takes a matrix M and uses its dimensions to create the new matrix.
   See also: `linalg.ones`. 3.17.4 */
static int linalg_zeros (lua_State *L) {  /* 3.17.4 */
  int i, m, n, sparse, columnv, nargs, nops;
  m = n = sparse = columnv = 0;
  nargs = lua_gettop(L);
  nops = aux_getdims(L, nargs, &m, &n, "linalg.zeros");
  aux_checkvmoptions(L, nops + 1, &nargs, &sparse, &columnv, "linalg.zeros");  /* 4.1.1 change */
  if (sparse) {
    lua_createtable(L, m, 1);
    for (i=0; i < m; i++) {
      lua_createtable(L, n, 1);  /* create new vector */
      setvattribs(L, n);
      lua_rawseti(L, -2, i + 1);
    }
    setmattribs(L, m, n);
  } else {
    lua_Number *a;
    la_createarray(L, a, m*n, "linalg.zeros");
    for (i=0; i < m*n; i++) a[i] = 0.0;
    creatematrix(L, a, m, n, sparse);
    xfree(a);
  }
  return 1;
}


static int linalg_identity (lua_State *L) {  /* optimised 2.1.3, extended 3.16.5, 3.17.4, extended 4.1.1 */
  int i, j, m, n, sparse, columnv, nargs, nops;
  m = n = sparse = columnv = 0;
  nargs = lua_gettop(L);
  nops = aux_getdims(L, nargs, &m, &n, "linalg.identity");
  aux_checkvmoptions(L, nops + 1, &nargs, &sparse, &columnv, "linalg.identity");  /* 4.1.1 change */
  createrawmatrix(L, m, n);
  if (sparse) {
    for (i=1; i <= m; i++) {
      lua_rawgeti(L, -1, i);  /* get row vector */
      agn_setinumber(L, -1, i, 1);  /* set one */
      agn_poptop(L);  /* pop row vector */
    }
  } else {  /* 4.1.1 change */
    for (i=1; i <= m; i++) {
      lua_rawgeti(L, -1, i);  /* get row vector */
      for (j=1; j <= n; j++) {
        agn_setinumber(L, -1, j, i == j);  /* set one and zero otherwise */
      }
      agn_poptop(L);  /* pop row vector */
    }
  }
  return 1;
}


/* Creates a random square n x n matrix with all entries in the range [-99 .. 99]. You can change this to
   another range [-p .. p] by passing any positive integer for `p'. By default, the function generates
   pseudo-random elements. You can change this to really random integers by passing the third argument `true`.
   3.20.5 */
#define MIN(x,y)  (x < y ? x : y)
#define MAX(x,y)  (x > y ? x : y)
static int linalg_randmatrix (lua_State *L) {
  int i, j, m, n, p, q, sparse, nargs;
  const char *option;
  lua_Number *a, x;
  nargs = lua_gettop(L);
  m = agn_checkposint(L, 1);
  n = agnL_optposint(L, 2, m);
  option = agnL_optstring(L, 3, "dense");
  if (nargs == 4 && lua_ispair(L, 4)) {
    agnL_pairgetiintegers(L, "linalg.randmatrix", 4, 1, &p, &q);
  } else {
    p = -99; q = -p;
  }
  sparse = 1;
  la_createarray(L, a, n*n, "linalg.randmatrix");
  for (i=0; i < m*n; i++) a[i] = 0;
  if (tools_streq(option, "dense")) {
    for (i=0; i < m*n; i++) a[i] = tools_randomrange(p, q, 0);
    sparse = 0;
  } else if (tools_streq(option, "symmetric")) {
    if (m != n)
      luaL_error(L, "Error in " LUA_QS ": symmetric matrices must be square.", "linalg.randmatrix");
    for (i=0; i < m; i++) {
      for (j=0; j <= i; j++) {
        x = tools_randomrange(p, q, 0);
        a[i*n + j] = x;
        a[j*n + i] = x;
      }
    }
  } else if (tools_streq(option, "antisymmetric")) {
    int le;
    if (m != n)
      luaL_error(L, "Error in " LUA_QS ": antisymmetric matrices must be square.", "linalg.randmatrix");
    for (i=0; i < m; i++) {
      for (j=0; j < i; j++) {
        le = (i <= j);
        x = tools_randomrange(p, q, 0);
        a[i*n + j] = le ? x : -x;
        a[j*n + i] = le ? -x : x;
      }
    }
  } else if (tools_streq(option, "unimodular")) {
    for (i=0; i < MIN(m, n); i++) {
      a[i*n + i] = 1.0;
    }
    for (i=0; i < m; i++) {
      for (j=i + 1; j < n; j++) {
         a[i*n + j] = tools_randomrange(p, q, 0);
      }
    }
  } else if (tools_streq(option, "sparse")) {
    int k;
    for (k=0; k < m + n + MAX(m, n) - 2; k++) {
      i = tools_randomrange(1, m, 0) - 1;
      j = tools_randomrange(1, n, 0) - 1;
      a[i*n + j] = tools_randomrange(p, q, 0);
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", "linalg.randmatrix", option);
  }
  creatematrix(L, a, m, n, sparse);
  xfree(a);
  return 1;
}


static int linalg_randvector (lua_State *L) {  /* 4.0.0 RC 1 */
  int i, n, p, q, nargs;
  lua_Number *a;
  nargs = lua_gettop(L);
  n = agn_checkposint(L, 1);
  if (nargs == 2 && lua_ispair(L, 2)) {
    agnL_pairgetiintegers(L, "linalg.randvector", 2, 1, &p, &q);
  } else {
    p = -99; q = -p;
  }
  la_createarray(L, a, n*n, "linalg.randvector");
  for (i=0; i < n*n; i++) a[i] = tools_randomrange(p, q, 0);
  createvector(L, a, n);  /* creates a sparse vector */
  xfree(a);
  return 1;
}


static int linalg_checkvector (lua_State *L) {
  int i, nargs, dim, olddim;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments for " LUA_QL("linalg.checkvector"));
  olddim = 0;
  for (i=1; i <= nargs; i++) {
    if (!agn_istableutype(L, i, "vector")) {
      if (i > 1) lua_pop(L, i - 1);  /* drop dimensions accumulated so far */
      luaL_error(L, "Error in " LUA_QS ": vector expected, got %s.", "linalg.checkvector", luaL_typename(L, i));
    }
    lua_getfield(L, i, "dim");
    if (nargs != 1) {
      dim = agn_checknumber(L, -1);
      if (i == 1) olddim = dim;
      if (olddim == dim)
        olddim = dim;
      else {
        lua_pop(L, i);  /* drop dimensions accumulated so far */
        luaL_error(L, "Error in " LUA_QS ": got vectors of different dimension.", "linalg.checkvector");  /* 7.5.3 fix */
      }
    }
  }
  return nargs;  /* return all dimensions */
}


static int linalg_isvector (lua_State *L) {
  int i, nargs;
  nargs = lua_gettop(L);
  for (i=0; i < nargs; i++) {
    if (!agn_istableutype(L, i + 1, "vector")) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int linalg_checkmatrix (lua_State *L) {
  int i, nargs, retdims, p, q;
  retdims = p = q = 0;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": got no argument.", "linalg.checkmatrix");
  luaL_checkstack(L, nargs, "too many arguments for " LUA_QL("linalg.checkmatrix"));
  if (lua_istrue(L, nargs)) {  /* Agena 1.6.0, 2.36.2 optimisation */
    if (nargs > 1) nargs--;
    retdims = 1;
  }
  for (i=1; i <= nargs; i++)
    linalg_auxcheckmatrix(L, i, retdims, 0, "linalg.checkmatrix", &p, &q);
  return retdims * nargs;
}


static int linalg_ismatrix (lua_State *L) {
  int i, nargs;
  nargs = lua_gettop(L);
  for (i=0; i < nargs; i++) {
    if (agn_istableutype(L, i + 1, "matrix") == 0) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


/*
# checks whether a matrix is a square matrix and issues an error if not, 27.07.2008;
# patched & simplified 18.12.2008
linalg.checksquare := proc(A, option) is
   local dims := linalg.checkmatrix(A, true);
   if left(dims) <> right(dims) then
      error('Error in `linalg.checksquare`: square matrix expected.')
   elif option = true then
      return dims
   fi  # else just issue the error or do nothing if everything is fine
end; */
static int linalg_checksquare (lua_State *L) {  /* 3.18.7 */
  int i, nargs, p, q;
  p = q = 0;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": got no argument.", "linalg.checksquare");
  luaL_checkstack(L, nargs, "too many arguments for " LUA_QL("linalg.checksquare"));
  if (lua_istrue(L, nargs) && nargs > 1) nargs--;
  for (i=1; i <= nargs; i++) {
    linalg_auxcheckmatrix(L, i, 1, 1, "linalg.checksquare", &p, &q);
    agn_poptop(L);
    lua_pushinteger(L, p);
  }
  return nargs;
}


/*
# checks whether the matrix A is a square matrix, 27.07.2008; simplified 18.12.2008
linalg.issquare := proc(A) is
   local a := linalg.checkmatrix(A, true);
   return left(a) = right(a)
end; */
static int linalg_issquare (lua_State *L) {  /* 3.18.7 */
  int i, nargs, p, q;
  p = q = 0;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": got no argument.", "linalg.issquare");
  for (i=1; i <= nargs; i++) {
    linalg_auxcheckmatrix(L, i, 1, 0, "linalg.issquare", &p, &q);
    agn_poptop(L);
    if (p != q) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


/* Checks whether all of the given matrices are singular and returns `true` or `false`. A matrix is singular
   if it is square and its determinant equals 0.
   See also: https://www.cuemath.com/algebra/singular-matrix; 3.18.9 */
static int linalg_issingular (lua_State *L) {
  lua_Number *a, d;
  int i, nargs, p, q;
  p = q = 0;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": got no argument.", "linalg.issingular");
  for (i=1; i <= nargs; i++) {
    linalg_auxcheckmatrix(L, i, 1, 0, "linalg.issingular", &p, &q);
    agn_poptop(L);
    if (p != q) {
      lua_pushfalse(L);
      return 1;
    }
    la_createarray(L, a, p*p, "linalg.issingular");
    if (fillmatrix(L, i, a, p, p, 0, "linalg.issingular")) {  /* 7.3.4 fix */
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.issingular");
    }
    d = aux_determinant(L, i, a, p, "linalg.issingular");  /* Kahan-Babuska with singular matrices does not work well, so do not use it. */
    xfree(a);
    if (d != 0.0) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


/*
# checks whether the matrix A is a diagonal matrix. If so, it returns true and false otherwise.
# Maple version March 17, 1998, Agena version December 21, 2008.
linalg.isdiagonal := proc(A) is
   local ranges, rowdim, coldim;
   ranges := linalg.checkmatrix(A, true);
   rowdim, coldim := left(ranges), right(ranges);
   return when rowdim <> coldim with false;  # 2.12.1
   for i to rowdim do
      for j to coldim do
         skip when i = j;
         return when nonzero A[i, j] with false  # 2.12.1, 2.15.6
      od
   od;
   return true
end; */
static int linalg_isdiagonal (lua_State *L) {  /* 3.18.7 */
  int i, j, k, nargs, p, q, rc;
  lua_Number x;
  p = q = 0;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": got no argument.", "linalg.isdiagonal");
  rc = 0;
  for (i=1; i <= nargs && !rc; i++) {
    linalg_auxcheckmatrix(L, i, 1, 0, "linalg.isdiagonal", &p, &q);
    agn_poptop(L);
    for (j=0; j < p && !rc; j++) {  /* for each row vector */
      lua_rawgeti(L, i, j + 1);  /* push row vector of original matrix */
      checkVector(L, -1, "linalg.isdiagonal");
      for (k=0; k < q && !rc; k++) {
        x = agn_getinumber(L, -1, k + 1);
        rc = (j != k && x != 0.0);
      }
      agn_poptop(L);
    }
  }
  lua_pushboolean(L, !rc);
  return 1;
}


static int linalg_isantidiagonal (lua_State *L) {  /* 3.18.7 */
  int i, j, k, nargs, p, q, rc;
  lua_Number x;
  p = q = 0;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": got no argument.", "linalg.isantidiagonal");
  rc = 0;
  for (i=1; i <= nargs && !rc; i++) {
    linalg_auxcheckmatrix(L, i, 1, 0, "linalg.isantidiagonal", &p, &q);
    agn_poptop(L);
    for (j=0; j < p && !rc; j++) {  /* for each row vector */
      lua_rawgeti(L, i, j + 1);  /* push row vector of original matrix */
      checkVector(L, -1, "linalg.isantidiagonal");
      for (k=q - 1; k >= 0 && !rc; k--) {
        x = agn_getinumber(L, -1, k + 1);
        rc = (j != q - 1 - k && x != 0.0);
      }
      agn_poptop(L);
    }
  }
  lua_pushboolean(L, !rc);
  return 1;
}


/*
# checks whether the matrix A is an identity matrix. If so, it returns true and false
# otherwise. Maple version March 16, 1998, Agena version December 21, 2008.
linalg.isidentity := proc(A) is
   local rowdim, n, i;
   return when not(linalg.isdiagonal(A)) with false;  # 2.12.1
   rowdim := left(linalg.checkmatrix(A, true));
   for i to rowdim do
      return when A[i, i] <> 1 with false  # 2.12.1
   od;
   return true
end; */
static int linalg_isidentity (lua_State *L) {  /* 3.18.7 */
  int i, j, k, nargs, p, q, rc;
  lua_Number x;
  p = q = 0;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": got no argument.", "linalg.isidentity");
  rc = 0;
  for (i=1; i <= nargs && !rc; i++) {
    linalg_auxcheckmatrix(L, i, 1, 0, "linalg.isidentity", &p, &q);
    agn_poptop(L);
    if (p != q) rc = 1;
    for (j=0; j < p && !rc; j++) {  /* for each row vector */
      lua_rawgeti(L, i, j + 1);  /* push row vector of original matrix */
      checkVector(L, -1, "linalg.isidentity");
      for (k=0; k < q && !rc; k++) {
        x = agn_getinumber(L, -1, k + 1);
        rc = (j != k && x != 0.0) || (j == k && x != 1.0);
      }
      agn_poptop(L);
    }
  }
  lua_pushboolean(L, !rc);
  return 1;
}


/*
# checks whether the matrix A is a symmetric matrix. If so, it returns true and false otherwise.
# Maple version March 16, 1998, Agena version December 21, 2008
linalg.issymmetric := proc(A) is
   return when not(linalg.issquare(A)) with false;  # 2.12.1
   local s := linalg.checksquare(A);
   for m from 1 to s - 1 do
      for n from m + 1 to s do
         skip when m = n;
         return when A[m, n] <> A[n, m] with false  # 2.12.1
      od
   od;
   return true
end; */
static int linalg_issymmetric (lua_State *L) {  /* 3.18.7 */
  int i, m, n, nargs, s, t, rc;
  lua_Number x, y;
  s = t = 0;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": got no argument.", "linalg.issymmetric");
  rc = 0;
  for (i=1; i <= nargs && !rc; i++) {
    linalg_auxcheckmatrix(L, i, 1, 0, "linalg.issymmetric", &s, &t);
    agn_poptop(L);
    if (s != t) rc = 1;
    for (m=1; m <= s - 1 && !rc; m++) {  /* for each row vector */
      lua_rawgeti(L, i, m);  /* push row vector A[m] */
      checkVector(L, -1, "linalg.issymmetric");
      for (n=m + 1; n <= s && !rc; n++) {
        if (m != n) {
          lua_rawgeti(L, i, n);  /* push row vector A[n] */
          x = agn_getinumber(L, -2, n);  /* A[m, n] */
          y = agn_getinumber(L, -1, m);  /* A[n, m] */
          rc = x != y;
          agn_poptop(L);
        }
      }
      agn_poptop(L);
    }
  }
  lua_pushboolean(L, !rc);
  return 1;
}


/*
# checks whether the matrix A is an antisymmetric matrix. If so, it returns true and false otherwise.
# Maple version March 20, 1998, Agena version December 21, 2008
linalg.isantisymmetric := proc(A) is
   return when not(linalg.issquare(A)) with false;  # 2.12.1
   local s := linalg.checksquare(A);
   for m from 1 to mrange - 1 do
      for n from m + 1 to nrange do
         skip when m = n;
         return when A[m, n] <> -A[n, m] with false  # 2.12.1
      od
   od;
   return true
end; */
static int linalg_isantisymmetric (lua_State *L) {  /* 3.18.7 */
  int i, m, n, nargs, s, t, rc;
  lua_Number x, y;
  s = t = 0;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": got no argument.", "linalg.isantisymmetric");
  rc = 0;
  for (i=1; i <= nargs && !rc; i++) {
    linalg_auxcheckmatrix(L, i, 1, 0, "linalg.isantisymmetric", &s, &t);
    agn_poptop(L);
    if (s != t) rc = 1;
    for (m=1; m <= s - 1 && !rc; m++) {  /* for each row vector */
      lua_rawgeti(L, i, m);  /* push row vector A[m] */
      checkVector(L, -1, "linalg.isantisymmetric");
      for (n=m + 1; n <= s && !rc; n++) {
        if (m != n) {
          lua_rawgeti(L, i, n);  /* push row vector A[n] */
          x = agn_getinumber(L, -2, n);  /* A[m, n] */
          y = agn_getinumber(L, -1, m);  /* A[n, m] */
          rc = x != -y;
          agn_poptop(L);
        }
      }
      agn_poptop(L);
    }
  }
  lua_pushboolean(L, !rc);
  return 1;
}


/*
# Checks whether a vector or matrix contains only zeros and returns `true` or `false`.
linalg.iszero := proc(A) is  # 2.14.0
   if typeof(A) notin {'matrix', 'vector'} then  # 3.15.3 change
      error('Error in `linalg.iszero`: matrix or vector expected.')
   fi;
   return satisfy(A, << x -> zero x >>, skiphash = true)
end;
*/
static int aux_isallx (lua_State *L, lua_Number y, const char *procname)  {  /* 3.18.7, patched 3.20.1 */
  int i, j, k, nargs, p, q, rc;
  lua_Number x;
  p = q = 0;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": got no argument.", procname);
  rc = 0;
  for (i=1; i <= nargs && !rc; i++) {
    if (agn_istableutype(L, i, "matrix")) {
      linalg_auxcheckmatrixlight(L, i, &p, &q, procname);  /* p = rowdim, q = coldim */
      for (j=0; j < p && !rc; j++) {  /* for each row vector */
        lua_rawgeti(L, i, j + 1);  /* push row vector of original matrix */
        checkVector(L, -1, procname);
        for (k=0; k < q && !rc; k++) {
          x = agn_getinumber(L, -1, k + 1);
          rc = x != y;
        }
        agn_poptop(L);
      }
    } else if (agn_istableutype(L, i, "vector")) {
      lua_getfield(L, i, "dim");
      p = agn_checknumber(L, -1);
      agn_poptop(L);
      for (j=1; j <= p && !rc; j++) {
        x = agn_getinumber(L, i, j);
        rc = x != y;
      }
    } else {
      luaL_error(L, "Error in " LUA_QS ": matrix or vector expected, got %s.", procname, luaL_typename(L, i));
    }
  }
  return !rc;
}

static int linalg_iszero (lua_State *L) {
  lua_pushboolean(L, aux_isallx(L, 0.0, "linalg.iszero"));
  return 1;
}


/* Checks whether m x n matrix A is in row echelon form and returns `true` or `false`.
   A matrix is in row echelon form if it has the following properties:
   - Any row consisting entirely of zeros occurs at the bottom of the matrix.
   - For two successive (non-zero) rows, the leading non-zero in the higher row is further left than the
     leading non-zero one in the lower row.
   Quoted from, and amended: https://www.geeksforgeeks.org/row-echelon-form/
   Agena version 3.20.1; C port 4.0.1, twice as fast as the Agena implementation

linalg.isref := proc(A) is
   local _, rows, l, max, previous, flag;
   rows := linalg.rowdim(A, true);
   flag := true;
   l, max, previous -> 0;
   for i to rows do
      _, l := linalg.viszero(A[i]);
      if flag and l > max then  # we have a possible pivot
         max := l
      elif flag and l = 0 and previous > 0 then
         # we have a zero vector but the previous one was non-zero
         flag := false  # now all the following rows must be zero
      elif not flag and l = 0 and previous = 0 then
         # we have a zero vector and the previous one was zero, too
         do nothing
      else
         return false
      fi;
      previous := l
   od;
   return true
end; */
static int linalg_isref (lua_State *L) {  /* 4.0.1 */
  int i, nargs, rc;
  nargs = lua_gettop(L);
  rc = 1;
  for (i=1; i <= nargs && rc; i++) {
    rc = aux_isef(L, i, 0, "linalg.isref");
  }
  lua_pushboolean(L, rc);
  return 1;
}


/* Checks whether m x n matrix A is in reduced row echelon form and returns `true` or `false`.
   A matrix is in reduced row echelon form if it has the following properties:
   - Any row consisting entirely of zeros occurs at the bottom of the matrix.
   - The first non-zero entry of each row is equal to 1 and is the only non-zero entry of its column.
   - For two successive (non-zero) rows, the leading one in the higher row is further left than the
     leading one in the lower row.
   Quoted from: https://www.geeksforgeeks.org/row-echelon-form/ &
                https://en.wikipedia.org/wiki/Row_echelon_form
   Agena version 3.20.1; C port 4.0.1, eight times faster than the Agena implementation

linalg.isrref := proc(A) is
   local _, x, rows, l, max, previous, flag;
   rows := linalg.rowdim(A, true);
   flag := true;
   l, max, previous -> 0;
   for i to rows do
      _, l, x := linalg.viszero(A[i]);
      if flag and l > max and x = 1 then  # we have a possible pivot
         max := l;
         # the pivot is the only nonzero entry of its column
         return when linalg.countitems(0, linalg.col(A, l)) <> rows - 1 with false
      elif flag and l = 0 and previous > 0 then
         # we have a zero vector but the previous one was non-zero
         flag := false  # now all the following rows must be zero
      elif not flag and l = 0 and previous = 0 then
         # we have a zero vector and the previous one was zero, too
         do nothing
      else
         return false
      fi;
      previous := l
   od;
   return true
end; */
static int linalg_isrref (lua_State *L) {  /* 4.0.1 */
  int i, nargs, rc;
  nargs = lua_gettop(L);
  rc = 1;
  for (i=1; i <= nargs && rc; i++) {
    rc = aux_isef(L, i, 1, "linalg.isrref");
  }
  lua_pushboolean(L, rc);
  return 1;
}


/* Checks whether a vector or matrix contains ones only and returns `true` or `false`. 3.20.5, based on linalg_iszero. */
static int linalg_isone (lua_State *L) {
  lua_pushboolean(L, aux_isallx(L, 1.0, "linalg.isone"));
  return 1;
}


/* Checks whether vector v has only zero elements and returns `true` or `false`. It also returns the index of the
   first non-zero element in v - if it exists - as a second result, or 0 if v is a zero vector. The non-zero element
   is returned as a third result, too.

   By default, the check is done against strict zero. You can change this by passing a positive epsilon value as an
   optional second argument so that all elements x with |x| <= epsilon will be considered zero.

   See also: Eps, hEps, DblEps, math.epsilon, `linalg.iszero`, `tables.iszero`; 3.20.1 */
static int linalg_viszero (lua_State *L) {
  int i, n, r, rc;
  lua_Number x, eps;
  n = checkVector(L, 1, "linalg.viszero");
  eps = agnL_optnonnegative(L, 2, 0.0);
  r = 1; x = AGN_NAN;
  for (i=1; i <= n && r; i++) {  /* from left to right ! */
    x = agn_getinumber(L, 1, i);
    r = fabs(x) <= eps;
  }
  rc = r*(n != 0);
  lua_pushboolean(L, rc);
  lua_pushinteger(L, (!r)*(i - 1));
  if (!rc) lua_pushnumber(L, x);
  return 2 + !rc;
}


/* Checks whether all matrix alements are integers; 3.18.9 */
static int linalg_isintegral (lua_State *L) {
  int i, j, k, m, n, rc, nargs;
  m = n = 0;
  rc = 1;
  nargs = lua_gettop(L);
  for (k=1; rc && k <= nargs; k++) {  /* 3.19.1 extension */
    linalg_auxcheckmatrix(L, k, 1, 0, "linalg.isintegral", &m, &n);
    agn_poptop(L);  /* pop dimension pair of matrix A */
    for (i=1; rc && i <= m; i++) {
      lua_rawgeti(L, k, i);  /* push row vector of A */
      checkVector(L, -1, "linalg.isintegral");
      for (j=1; rc && j <= n; j++) {
        rc = tools_isint(agn_getinumber(L, -1, j));
      }
      agn_poptop(L);  /* pop row vector */
    }
  }
  lua_pushboolean(L, rc);
  return 1;
}


/* Checks whether at least one matrix element is a fraction and not an integer. 3.19.1 */
static int linalg_isfractional (lua_State *L) {
  int i, j, k, m, n, rc, nargs;
  m = n = rc = 0;
  nargs = lua_gettop(L);
  for (k=1; !rc && k <= nargs; k++) {
    linalg_auxcheckmatrix(L, k, 1, 0, "linalg.isfractional", &m, &n);
    agn_poptop(L);  /* pop dimension pair of matrix A */
    for (i=1; !rc && i <= m; i++) {
      lua_rawgeti(L, k, i);  /* push row vector of A */
      checkVector(L, -1, "linalg.isfractional");
      for (j=1; !rc && j <= n; j++) {
        rc = tools_isfrac(agn_getinumber(L, -1, j));
      }
      agn_poptop(L);  /* pop row vector */
    }
  }
  lua_pushboolean(L, rc);
  return 1;
}


static int linalg_vzip (lua_State *L) {  /* extended 0.30.2 */
  int i, j, sizea, sizeb, nargs, slots;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  checkvectors(L, 2, 3, "linalg.vzip");
  nargs = lua_gettop(L);
  luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.2 fix */
  lua_createtable(L, sizea, 1);
  slots = 3 + (nargs > 3)*(nargs - 3);  /* 2.30.2 */
  /* now traverse vector */
  for (i=1; i <= sizea; i++) {
    luaL_checkstack(L, slots, "too many arguments");  /* 3.15.2 fix */
    lua_pushvalue(L, 1);      /* push function; FIXME: can be optimized */
    lua_geti(L, 2, i);
    lua_geti(L, 3, i);
    for (j=4; j <= nargs; j++) {
      lua_pushvalue(L, j);
    }
    lua_call(L, nargs - 1, 1);  /* call function with nargs-1 arguments and one result */
    if (agn_isnumber(L, -1) && agn_tonumber(L, -1) == 0.0) {  /* 2.30.2 */
      agn_poptop(L);              /* sparse element, nothing to do */
    } else if (isdlong(L, -1) && getdlongvalue(L, -1) == 0.0L) {  /* 7.3.10 */
      agn_poptop(L);              /* sparse element, nothing to do */
    } else {
      lua_seti(L, -2, i);         /* store result to new vector */
    }
  }
  setvattribs(L, sizea);
  return 1;
}


static int linalg_vmap (lua_State *L) {
  int i, j, nargs, size, slots;
  nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  checkvector(L, 2, "linalg.vmap");
  luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.2 fix */
  lua_createtable(L, size, 1);
  slots = 2 + (nargs > 2)*(nargs - 2); /* 2.30.2 */
  /* now traverse vector */
  for (i=1; i <= size; i++) {
    luaL_checkstack(L, slots, "too many arguments for " LUA_QL("linalg.vmap"));  /* 3.15.2 fix */
    lua_pushvalue(L, 1);   /* push function */
    lua_geti(L, 2, i);     /* 3.15.2 change */
    for (j=3; j <= nargs; j++) lua_pushvalue(L, j);
    lua_call(L, nargs - 1, 1);  /* call function with nargs-1 arguments and one result */
    if (agn_isnumber(L, -1) && agn_tonumber(L, -1) == 0.0) {  /* 2.30.2 */
      agn_poptop(L);              /* sparse element, nothing to do */
    } else if (isdlong(L, -1) && getdlongvalue(L, -1) == 0.0L) {  /* 7.3.10 */
      agn_poptop(L);              /* sparse element, nothing to do */
    } else {
      lua_seti(L, -2, i);         /* store result to new vector */
    }
  }
  setvattribs(L, size);
  return 1;
}


static int linalg_setvelem (lua_State *L) {  /* linalg.setvelem(v, key, value); completely rewritten 3.20.2 */
  int dim, key;  /* changed 3.15.4 */
  dim = checkVector(L, 1, "linalg.setvelem");  /* 2.1.4 */
  key = agn_checkinteger(L, 2);
  key = tools_posrelat(key, dim);
  if (key < 1 || key > dim)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range 1 .. %d.", "linalg.setvelem", key, dim);
  switch (lua_type(L, 3)) {
    case LUA_TNUMBER: {
      int isnil;
      lua_Number val = agn_tonumber(L, 3);
      if (val == 0.0) {  /* check whether value at t[key] is sparse, 3.20.2 */
        lua_rawgeti(L, 1, key);
        isnil = lua_isnil(L, -1);
        agn_poptop(L);
        if (isnil) return 0;  /* do nothing, leave it sparse or set value in next statement */
      }
      lua_pushvalue(L, 3);
      break;
    }
    case LUA_TNIL: {
      lua_pushnil(L);
      break;
    }
    case LUA_TCOMPLEX: {
      lua_pushvalue(L, 3);
      break; /* 7.3.10 fix */
    }
    default: {
      if (isdlong(L, 3) || ismapm(L, 3) || iscmapm(L, 3) ||
          luaL_isudata(L, 3, "mpfr") || luaL_isudata(L, -1, "cmpfr") ||
          luaL_isudata(L, 3, "dd")) {  /* 7.3.10/7.5.1 extension */
        lua_pushvalue(L, 3);
      } else {
        luaL_error(L, "Error in " LUA_QS ": can only set numbers or " LUA_QS ", got %s.",
          "linalg.setvelem", "null", luaL_typename(L, 3));
      }
    }
  }
  lua_rawseti(L, 1, key);
  return 0;
}


static int linalg_getvelem (lua_State *L) {  /* linalg.getvelem(v, key), 3.15.4 */
  int key, dim;
  dim = checkVector(L, 1, "linalg.getvelem");  /* just checking for a table is not much faster */
  key = agn_checkinteger(L, 2);
  key = tools_posrelat(key, dim);  /* 3.20.2 */
  if (key < 1 || key > dim) {
    luaL_error(L, "Error in " LUA_QS ": index %d out of range 1:%d.", "linalg.getvelem", key, dim);
  }
  lua_rawgeti(L, 1, key);  /* prevent stack overflow with mts */
  /* returns 0 if element is not a number or has not been set */
  if (!(agn_isnumber(L, -1) || isdlong(L, -1) || lua_iscomplex(L, -1) || ismapm(L, -1) || iscmapm(L, -1) ||
        luaL_isudata(L, -1, "mpfr") || luaL_isudata(L, -1, "cmpfr") || luaL_isudata(L, -1, "dd") )) {  /* 7.5.1 extension */
    agn_poptop(L);
    lua_pushnumber(L, 0.0);
  }
  return 1;
}


/* Returns a copy of matrix A with each element in row idx multiplied by the number x. */
static int linalg_mulrow (lua_State *L) {  /* 2.1.4 */
  int i, m, n, idx;
  lua_Number x, *a;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.mulrow", &m, &n);
  agn_poptop(L);
  idx = agn_checkinteger(L, 2) - 1;
  if (idx < 0 || idx >= m)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.mulrow", idx + 1);
  x = agn_checknumber(L, 3);
  la_createarray(L, a, m * n, "linalg.mulrow");
  if (fillmatrix(L, 1, a, m, n, 0, "linalg.mulrow")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.mulrow");
  }
  for (i=0; i < n; i++) a[idx*n + i] *= x;
  creatematrix(L, a, m, n, 1);
  xfree(a);
  return 1;
}


/* Returns a copy of matrix A with each element in row idx2 exchanged by the sum of this element and the respective
   element in row idx1 multiplied by the number x. */
static int linalg_mulrowadd (lua_State *L) {  /* 2.1.4 */
  int i, m, n, idx1, idx2;
  lua_Number x, *a;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.mulrowadd", &m, &n);
  agn_poptop(L);
  idx1 = agn_checkinteger(L, 2) - 1;
  if (idx1 < 0 || idx1 >= m)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range with second argument.", "linalg.mulrowadd", idx1 + 1);
  idx2 = agn_checkinteger(L, 3) - 1;
  if (idx2 < 0 || idx2 >= m)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range with third argument.", "linalg.mulrowadd", idx2 + 1);
  x = agn_checknumber(L, 4);
  la_createarray(L, a, m * n, "linalg.mulrowadd");
  if (fillmatrix(L, 1, a, m, n, 0, "linalg.mulrowadd")) {  /* 7.3.4 fix */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.mulrowadd");
  }
  for (i=0; i < n; i++) a[idx2*n + i] += a[idx1*n + i]*x;
  creatematrix(L, a, m, n, 1);
  xfree(a);
  return 1;
}


/* taken from: http://www.mymathlib.com/matrices/linearsystems/doolittle.html, file
   provided by RLH, Copyright © 2004 RLH. All rights reserved. 3.18.6 */
static int Doolittle_LU_Decomposition (long double *A, int n) {
  int i, j, k, p;
  long double *p_k, *p_row, *p_col;
  /* For each row and column, k = 0, ..., n-1, find the upper triangular matrix elements for row k
     and if the matrix is non-singular (nonzero diagonal element).
     Find the lower triangular matrix elements for column k. */
  for (k=0, p_k=A; k < n; p_k += n, k++) {
    for (j = k; j < n; j++) {
      for (p = 0, p_col = A; p < k; p_col += n,  p++)
        *(p_k + j) -= *(p_k + p) * *(p_col + j);
    }
    if (*(p_k + k) == 0.0L) return -1;
    for (i=k + 1, p_row=p_k + n; i < n; p_row += n, i++) {
      for (p=0, p_col=A; p < k; p_col += n, p++)
        *(p_row + k) -= *(p_row + p) * *(p_col + k);
      *(p_row + k) /= *(p_k + k);
    }
  }
  return 0;
}

/* taken from: http://www.mymathlib.com/matrices/linearsystems/doolittle.html, file
   provided by RLH, Copyright © 2004 RLH. All rights reserved.

   Given the n◊n matrix A, Doolittle_LU_Decomposition_with_Pivoting uses Doolittle's algorithm
   with pivoting to decompose a row interchanged version of A into the product of a unit lower
   triangular matrix and an upper triangular matrix. The non-diagonal lower triangular part of
   the unit lower triangular matrix is returned in the lower triangular part of A and the upper
   triangular part of the upper triangular matrix is returned in the upper triangular part of A.

   Upon completion the ith element of the array pivot contains the row interchanged with row i
   when k = i, k being the k in the description of the algorithm above. The array pivot should
   be dimensioned at least n in the calling routine. Doolittle_LU_Decomposition returns -1 or 1
   if the decomposition was successful and returns 0 if the matrix is singular. */

static int Doolittle_LU_Decomposition_with_Pivoting (long double *A, long double *pivot, int n) {
  int i, j, k, d;
  long double *p_k, *p_row, *p_col, max;
  d = 1;
  p_col = NULL;
  /* for each row and column, k = 0, ..., n-1, */
  for (k=0, p_k=A; k < n; p_k += n, k++) {
    /* find the pivot row */
    pivot[k] = k;
    max = fabsl(*(p_k + k));  /* 3.18.6 fix */
    for (j=k + 1, p_row=p_k + n; j < n; j++, p_row += n) {
      if (max < fabsl(*(p_row + k))) {
        max = fabsl(*(p_row + k));
        pivot[k] = j;
        p_col = p_row;
      }
    }
    /* and if the pivot row differs from the current row, then interchange the two rows. */
    if (pivot[k] != k) {
      for (j=0; j < n; j++) {
        max = *(p_k + j);
        *(p_k + j) = *(p_col + j);
        *(p_col + j) = max;
      }
      d = -d;
    }
    /* and if the matrix is singular, return error */
    if (*(p_k + k) == 0.0L) return 0;  /* modified for Agena */
    /* otherwise find the lower triangular matrix elements for column k. */
    for (i=k + 1, p_row=p_k + n; i < n; p_row += n, i++) {
      *(p_row + k) /= *(p_k + k);
    }
    /* update remaining matrix */
    for (i=k + 1, p_row=p_k + n; i < n; p_row += n, i++) {
      for (j=k + 1; j < n; j++)
        *(p_row + j) -= *(p_row + k) * *(p_k + j);
    }
  }
  for (i=0; i < n; i++) pivot[i] += 1;  /* convert from C to Agena indices */
  return d;  /* return row interchange indicator, modified for Agena */
}


static int linalg_ludoolittle (lua_State *L) {  /* 2.1.4, fixed 2.2.0 RC 3 */
  int m, n, nn, c, nopivot;
  long double *a, *p;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.ludoolittle", &m, &n);
  agn_poptop(L);
  nn = luaL_optinteger(L, 2, m);
  if (nn < 1 || nn > m)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.ludoolittle", nn);
  nopivot = lua_isfalse(L, 3);  /* new 3.18.6 */
  createarrayld(a, nn * nn, "linalg.ludoolittle");
  createarrayld(p, nn, "linalg.ludoolittle");
  if (fillmatrixld(L, 1, a, nn, nn, 0, "linalg.ludoolittle")) {  /* 7.3.4 fix */
    xfreeall(a, p);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.ludoolittle");
  }
  if (nopivot) {  /* no pivoting, 3.18.6 */
    if ((c = Doolittle_LU_Decomposition(a, nn)) == 0) {
      xfreeall(a, p);  /* 2.2.0 RC 3, 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": singular matrix encountered.", "linalg.ludoolittle");
    }
  } else {
    if ((c = Doolittle_LU_Decomposition_with_Pivoting(a, p, nn)) == 0) {
      xfreeall(a, p);  /* 2.2.0 RC 3, 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": singular matrix encountered.", "linalg.ludoolittle");
    }
  }
  luaL_checkstack(L, 3, "not enough stack space");  /* 3.18.2 fix */
  creatematrixld(L, a, nn, nn);
  if (!nopivot) createvectorld(L, p, nn);
  lua_pushinteger(L, c);
  xfreeall(a, p);  /* 2.9.8 */
  return 3 - nopivot;
}


/* Performs LU decomposition on the m x n matrix A, Maple-style. By default, the return is an upper triangular factor. If you pass the option
  `all=true` then besides the upper triangular factor, the lower triangular factor, the pivot factor, the rank and the determinant will be
   returned, in this order.

   The function tries to prevent as many fractional elements in the resulting upper triangular factor as possible if the input matrix consists
   of integral values only. If A contains at least one fractional value, or the option `float=true' is given, then a partial row pivoting method
   is used, otherwise pivoting is done only when a leading entry is zero.

   See also: linalg.gausselim, linalg.gaussjord, linalg.ludoolittle.

   This is a port of two Maple V R4's linalg[LUdecomp] auxiliary functions fused together. 3.18.9; C port 4.0.1 */
void linalg_aux_ludecompcheckoptions (lua_State *L, int pos, int *nargs, int *isall, int *isfloat, const char *procname) {
  int checkoptions;
  *isall = 0;  /* 0 = return a new structure, do not work in-place */
  *isfloat = 0;   /* 0 = strict equality check, 1 = approximate equality check */
  checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("float", option)) {
        *isfloat = agn_checkboolean(L, -1);
      } else if (tools_streq("all", option)) {
        *isall = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    (*nargs)--;
    agn_poptoptwo(L);
  }
}

static int linalg_ludecomp (lua_State *L) {
  int m, n, i, j, k, p, isfloat, isall, nargs, col, r, nulldim, *plist;
  long double *UU, *LL, det, pval, t, rat;
  double *PP;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  nargs = lua_gettop(L);
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.ludecomp", &m, &n);
  agn_poptop(L);
  linalg_aux_ludecompcheckoptions(L, 1, &nargs, &isall, &isfloat, "linalg.ludecomp");
  createarrayld(UU, m * n, "linalg.ludecomp");
  createarrayld(LL, m * m, "linalg.ludecomp");
  if (fillmatrixld(L, 1, UU, m, n, 0, "linalg.ludecomp")) {  /* 7.3.4 fix */
    xfreeall(UU, LL);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.ludecomp");
  }
  isfloat = isfloat || !aux_isintegrall(UU, m*n);
  plist = (isall) ? malloc(MIN(m, n) * sizeof(int)) : NULL;
  if (isall && plist == NULL) {  /* 4.11.5 fix */
    xfreeall(UU, LL);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "linalg.ludecomp");
  }
  for (i=0; i < m*m; i++) LL[i] = 0.0L;
  det = 1.0L;
  col = 0;
  r = -1;
  p = 0;
  PP = NULL;
  nulldim = 0;
  while (col < n) {
    if (isfloat) {
      /* do it `Maple float style` (Maple automatically switches into float mode if at least one matrix element is fractional) */
      col = r + nulldim + 1;
      pval = 0.0L;
      while (pval == 0.0L && col < n) {
        p = r + 1;
        for (i=r + 1; i < m; i++) {
          if (pval < fabsl(UU[i*n + col])) {
            p = i;
            pval = fabsl(UU[i*n + col]);
          }
        }
        if (pval == 0.0L) nulldim++;
        col = r + nulldim + 1;
      }
      if (pval == 0.0L) break;
    } else {
      col = r + nulldim + 1;
      p = r;
      while (p <= r && col < n) {
        for (i=r + 1; i < m; i++) {
          if (UU[i*n + col] != 0.0L) {
            p = i;
            break;
          }
        }
        if (m < i + 1) nulldim++;
        col = r + nulldim + 1;
      }
      if (n < col + 1) break;
    }
    r++;
    if (isall) plist[r] = p;
    if (p != r) {
      for (j=0; j <= r - 1; j++) {
        t = LL[p*m + j];
        LL[p*m + j] = LL[r*m + j];
        LL[r*m + j] = t;
      }
      for (j=col; j < n; j++) {
        t = UU[p*n + j];
        UU[p*n + j] = UU[r*n + j];
        UU[r*n + j] = t;
      }
    }
    for (i=r + 1; i < m; i++) {
      rat = UU[i*n + col]/UU[r*n + col];
      UU[i*n + col] = 0.0L;
      LL[i*m + r] = rat;
      for (j=col + 1; j < n; j++) {
        UU[i*n + j] -= rat*UU[r*n +j];
      }
    }
    det *= UU[r*n + col];
    LL[r*m + r] = 1.0L;
  }
  for (i=r + 1; i < m; i++) LL[i*m + i] = 1.0L;
  luaL_checkstack(L, 1 + 4*isall, "not enough stack space");
  creatematrixld(L, UU, m, n);
  if (isall) {
    PP = calloc(m*m, sizeof(lua_Number));
    if (!PP) {  /* 7.3.4 fix */
      agn_poptop(L);  /* pop new matrix */
      xfreeall(plist, LL, UU);
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "linalg.ludecomp");
    }
    for (i=0; i < m; i++) PP[i*m + i] = 1.0;
    for (i=0; i < r; i++) {
      for (j=0; j < m && PP[j*m + i] == 0.0; j++) { };
      for (k=0; k < m && PP[k*m + plist[i]] == 0.0; k++) { };
      PP[j*m + i] = 0.0;
      PP[k*m + plist[i]] = 0.0;
      PP[k*m + i] = 1.0;
      PP[j*m + plist[i]] = 1.0;
    }
    creatematrixld(L, LL, m, m);
    creatematrix(L, PP, m, m, 1);
    lua_pushinteger(L, r + 1);
    lua_pushnumber(L, det);
  }
  xfreeall(plist, LL, UU, PP);
  return 1 + 4*isall;
}


static int linalg_submatrix (lua_State *L) {  /* 2.1.4 */
  int m, n, i, j, a, b, c, d, rc;
  lua_Number x;
  m = n = 0; /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.submatrix", &m, &n);
  agn_poptop(L);
  a = c = 1;
  b = n; d = m;
  /* check column number or column range */
  if (lua_ispair(L, 2))
    agnL_pairgetiposints(L, "linalg.submatrix", 2, 1, &a, &b);
  else if (agn_isinteger(L, 2))
    a = b = agn_tointeger(L, 2);
  else
    luaL_error(L, "Error in " LUA_QS ": pair or integer expected for second argument, got %s.",
      "linalg.submatrix", luaL_typename(L, 2));
  if (a > n)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.submatrix", a);
  if (b > n)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.submatrix", b);
  /* get specific row or rows */
  if (lua_gettop(L) == 3) {
    if (lua_ispair(L, 3)) {
      agnL_pairgetiposints(L, "linalg.submatrix", 3, 1, &c, &d);
    } else if (agn_isinteger(L, 3)) {
      c = d = agn_tointeger(L, 3);
    } else
      luaL_error(L, "Error in " LUA_QS ": pair or integer expected for third argument, got %s.",
        "linalg.submatrix", luaL_typename(L, 3));
  }
  if (c > m)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.submatrix", c);
  if (d > m)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "linalg.submatrix", d);
  if (a != b) {  /* more than one column ? */
    createrawmatrix(L, d - c + 1, b - a + 1);
    for (i=c; i <= d; i++) {  /* for each row vector */
      lua_rawgeti(L, -1, i - c + 1);  /* push row vector of new matrix */
      lua_rawgeti(L, 1, i);   /* push row vector of original matrix */
      checkVector(L, -1, "linalg.submatrix");  /* 3.17.4 security fix */
      for (j=a; j <= b; j++) {
        x = agn_rawgetinumber(L, -1, j, &rc);  /* 3.20.3 change */
        if (!rc) continue;  /* 3.20.4 change to preserve sparseness */
        agn_setinumber(L, -2, j - a + 1, x);  /* set component to new matrix */
      }
      agn_poptoptwo(L);
    }
  } else {  /* just one column */
    lua_createtable(L, d - c + 1, 1);
    for (i=c; i <= d; i++) {  /* for each row vector */
      lua_rawgeti(L, 1, i);   /* push row vector of original matrix */
      checkVector(L, -1, "linalg.submatrix");  /* 3.17.4 security fix */
      x = agn_rawgetinumber(L, -1, a, &rc);  /* get a-th component */
      if (!rc) continue;  /* 3.20.4 change to preserve sparseness */
      agn_setinumber(L, -2, i - c + 1, x);  /* set component to new vector */
      agn_poptop(L);
    }
    setvattribs(L, d - c + 1);
  }
  return 1;
}


/* 2.1.5, code taken from http://rosettacode.org/wiki/Reduced_row_echelon_form#C.23 and adapted for Agena. Tuned 2.21.9 */
static void rref (long double *a, int m, int n, long double eps) {
  int i, j, k, lead, r, rowi, rowiplusj, rowj, rowr;
  long double temp, div, sub;
  lead = 0;
  for (r=0; r < m; r++) {
    if (n <= lead) break;
    rowr = r*n;
    i = r;
    rowi = rowr;
    while (a[i*n + lead] == 0.0L) {
      i++;
      if (i == m) {
        i = r;
        lead++;
        if (n == lead) {
          lead--;
          break;
        }
      }
    }
    for (j = 0; j < n; j++) {
      temp = a[rowr + j];
      rowiplusj = rowi + j;
      a[rowr + j] = a[rowiplusj];
      a[rowiplusj] = temp;
    }
    div = a[r*n + lead];
    if (fabsl(div) > eps) { /* modified */
      for (j = 0; j < n; j++) a[rowr + j] /= div;
    }
    for (j = 0; j < m; j++) {
      rowj = j*n;
      if (j != r) {  /* modified */
        sub = a[rowj + lead];
        for (k = 0; k < n; k++) a[rowj + k] -= (sub * a[rowr + k]);
      }
    }
    lead++;
  }
  for (i=0; i < m; i++) {  /* modified, cancel out values very close to zero, leave these statements here ! */
    rowi = i*n;
    for (j=0; j < n; j++) {
      if (fabsl(a[rowi + j]) < eps) a[rowi + j] = 0.0L;
    }
  }
}

static int subfillobjectsld (lua_State *L, int nargs, long double *a, int m, int n, const char *procname) {
  if (nargs == 1) {
    if (fillmatrixld(L, 1, a, m, n, 0, procname)) {  /* 7.3.4 fix */
      return 1;
      /* luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", procname); */
    }
  } else {  /* augment square matrix with vector */
    int i;
    long double *b;
    createarrayld(b, m, procname);
    fillmatrixld(L, 1, a, m, n, 1, procname);
    fillvectorld(L, 2, b, m, procname);
    for (i=0; i < m; i++) a[i*n + m] = b[i];
    xfree(b);
  }
  return 0;
}

static int linalg_rref (lua_State *L) {  /* 2.1.5, 2.34.10 switch to long double */
  int m, n, nargs;
  long double *a, eps;
  m = n = 0; /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.rref", &m, &n);
  agn_poptop(L);
  nargs = lua_gettop(L);
  eps = agn_getepsilon(L);  /* 2.21.8/9 change */
  if (nargs == 2) {
    if (m != checkVector(L, 2, "linalg.rref"))
      luaL_error(L, "Error in " LUA_QS ": expected matrix and vector with equal dimensions.", "linalg.rref");
    n++;
  } else if (nargs != 1)
    luaL_error(L, "Error in " LUA_QS ": one or two arguments expected.", "linalg.rref");
  createarrayld(a, m * n, "linalg.rref");
  if (subfillobjectsld(L, nargs, a, m, n, "linalg.rref"))  { /* 7.3.4 fix */
    xfreeall(a);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.rref");
  }
  rref(a, m, n, eps);
  creatematrixld(L, a, m, n);
  xfree(a);
  return 1;
}

static void backward_substitutionld (long double *a, long double *x, int m, int n, long double eps) {
  int i, j, row;
  long double q;
  for (i=m - 1; i >= 0; i--) {
    q = 0.0L;
    row = i*n;  /* 2.21.9 tuning */
    for (j=i + 1; j < m; j++)
      q += a[row + j] * x[j];
    x[i] = (a[row + (n - 1)] - q)/a[row + i];
  }
}

/* See: https://vismor.com/documents/network_analysis/matrix_algorithms/S5.SS1.php
   by Timothy Vismor */
static void forward_substitutionld (long double *a, long double *x, int m, int n, long double eps) {
  int i, j, rowi, rowj;
  for (i=0; i < m; i++) {
    rowi = i*n;  /* 2.21.9 tuning */
    if (fabsl(a[rowi + (n - 1)]) < eps) continue;
    x[i] = a[rowi + (n - 1)]/a[rowi + i];
    for (j=i + 1; j < m; j++) {
      rowj = j*n;
      a[rowj + (n - 1)] -= x[i]*a[rowj + i];
    }
  }
}

static int isuppertriangularld (long double *a, int m, int n, long double eps) {
  int i, j, row;
  for (i=0; i < m; i++) {  /* check main diagonal */
    row = i*n;  /* 2.21.9 tuning */
    if (fabsl(a[row + i]) < eps) return 0;
    for (j=0; j < i - 1; j++) {  /* check lower half */
      if (fabsl(a[row + j]) >= eps) return 0;
    }
  }
  return 1;
}

static int islowertriangularld (long double *a, int m, int n, long double eps) {
  int i, j, row;
  for (i=0; i < m; i++) {  /* check main diagonal */
    row = i*n;  /* 2.21.9 tuning */
    if (fabsl(a[row + i]) < eps) return 0;
    for (j=i + 1; j < m; j++) {  /* check upper half */
      if (fabsl(a[row + j]) >= eps) return 0;
    }
  }
  return 1;
}

static void subcheckdims (lua_State *L, int nargs, int m, int *n, const char *procname) {
  if (nargs == 1) {
    if (m + 1 != *n)
      luaL_error(L, "Error in " LUA_QS ": matrix has wrong dimensions.", procname);
  } else if (nargs == 2) {
    if (m != *n)
      luaL_error(L, "Error in " LUA_QS ": expected a square matrix.", procname);
    if (*n != checkVector(L, 2, "linalg.backsub"))
      luaL_error(L, "Error in " LUA_QS ": expected matrix and vector with equal dimensions.", procname);
    (*n)++;
  } else
    luaL_error(L, "Error in " LUA_QS ": one or two arguments expected.", procname);
}

static int linalg_backsub (lua_State *L) {  /* 2.34.10 switch to long double */
  int m, n, nargs;
  long double *a, *x, eps;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  nargs = lua_gettop(L);
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.backsub", &m, &n);
  agn_poptop(L);  /* pop dimension pair of the matrix */
  eps = agn_getepsilon(L);  /* 2.21.8/9 change */
  subcheckdims(L, nargs, m, &n, "linalg.backsub");
  createarrayld(a, m*n, "linalg.backsub");
  createarrayld(x, m, "linalg.backsub");
  if (subfillobjectsld(L, nargs, a, m, n, "linalg.backsub"))  { /* 7.3.4 fix */
    xfreeall(a, x);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.backsub");
  }
  if (!isuppertriangularld(a, m, n, eps)) {
    xfreeall(a, x);  /* 2.9.8 */
    luaL_error(L, "Error in " LUA_QS ": matrix is not upper triangular.", "linalg.backsub");
  }
  backward_substitutionld(a, x, m, n, eps);
  createvectorld(L, x, m);
  xfreeall(a, x);  /* 2.9.8 */
  return 1;
}


static int linalg_forsub (lua_State *L) {  /* 2.34.10 switch to long double */
  int m, n, nargs;
  long double *a, *x, eps;
  m = n = 0;  /* 2.34.11 to prevent compiler warnings, MinGW GCC 9.2.0 migration patch */
  nargs = lua_gettop(L);
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.forsub", &m, &n);
  agn_poptop(L);  /* pop dimension pair of the matrix */
  eps = agn_getepsilon(L);  /* 2.21.8/9 change */
  subcheckdims(L, nargs, m, &n, "linalg.forsub");
  createarrayld(a, m*n, "linalg.forsub");
  createarrayld(x, m, "linalg.forsub");
  if (subfillobjectsld(L, nargs, a, m, n, "linalg.forsub"))  { /* 7.3.4 fix */
    xfreeall(a, x);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.forsub");
  }
  if (!islowertriangularld(a, m, n, eps)) {
    xfreeall(a, x);  /* 2.9.8 */
    luaL_error(L, "Error in " LUA_QS ": matrix is not lower triangular.", "linalg.forsub");
  }
  forward_substitutionld(a, x, m, n, eps);
  createvectorld(L, x, m);
  xfreeall(a, x);  /* 2.9.8 */
  return 1;
}


static int linalg_islower (lua_State *L) {  /* 3.10.8 */
  int i, m, n, rc, nargs;
  long double *a, eps;
  nargs = lua_gettop(L);
  eps = agn_getepsilon(L);
  m = n = 0;
  rc = 1;
  for (i=1; rc && i <= nargs; i++) {  /* 3.19.1 extension */
    linalg_auxcheckmatrix(L, i, 1, 0, "linalg.islower", &m, &n);
    agn_poptop(L);  /* pop dimension pair of the matrix */
    if (m != n) {  /* not a square matrix ? */
      rc = 0; break;
    }
    createarrayld(a, m*n, "linalg.islower");
    if (subfillobjectsld(L, i, a, m, n, "linalg.islower"))  { /* 7.3.4 fix */
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.islower");
    }
    rc = islowertriangularld(a, m, n, eps);
    xfree(a);
  }
  lua_pushboolean(L, rc);
  return 1;
}


static int linalg_isupper (lua_State *L) {  /* 3.10.8 */
  int i, m, n, rc, nargs;
  long double *a, eps;
  nargs = lua_gettop(L);
  eps = agn_getepsilon(L);
  m = n = 0;
  rc = 1;
  for (i=1; rc && i <= nargs; i++) {  /* 3.19.1 extension */
    linalg_auxcheckmatrix(L, i, 1, 0, "linalg.isupper", &m, &n);
    agn_poptop(L);  /* pop dimension pair of the matrix */
    if (m != n) {  /* not a square matrix ? */
      rc = 0; break;
    }
    createarrayld(a, m*n, "linalg.isupper");
    if (subfillobjectsld(L, i, a, m, n, "linalg.isupper"))  { /* 7.3.4 fix */
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.isupper");
    }
    rc = isuppertriangularld(a, m, n, eps);
    xfree(a);
  }
  lua_pushboolean(L, rc);
  return 1;
}


/* Eigenvalues/Eigenvectors, 3.10.6

   This source file is adapted from feigen.c that comes with the book
   Numeric Algorithm with C by Frank Uhlig et al. I cannot find the
   license of the original source codes. I release my modifications under
   the MIT license. The modified version requires C99 as it uses complex
   numbers. I may modify the code if this is a concern.

   The MIT License

   Copyright (c) 1996 Frank Uhlig et al.
                 2009 Genome Research Ltd (GRL).

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

   Contact: Heng Li <lh3lh3@gmail.com>

   Modifications to compile with GCC for OS/2 and DJGPP by a. walz, and are
   marked with the 'OS2_DOS` token. The extension to long double does not seem
   to yield better accuracy. */

/*.FE{C 7.8}
     {QR Algorithm}
     {Eigenvalues and Eigenvectors of a Matrix via the QR Algorithm}*/

#define REAL   long double  /* OS2_DOS */
#define TRUE   1
#define FALSE  0
#define ZERO   0.0L
#define ONE    1.0L
#define TWO    2.0L
#define VEKTOR 0
#define MACH_EPS DBL_EPSILON  /* DBL_EPSILON */

#define MAXIT 50  /* maximum number of iterations per eigenvalue */

#ifdef ABS  /* OS2_DOS */
#undef ABS
#define ABS fabsl
#endif
#define SQRT sqrtl          /* OS2_DOS */
#define HYPOT tools_hypotl  /* OS2_DOS */
#define SQR(x) ((x)*(x))
#define SWAPT(typ, a, b) { typ t; t = (a); (a) = (b); (b) = t; }

#define CREATEARRAY   createarrayld
#define FILLMATRIX    fillmatrixld
#define CREATEMATRIX  creatematrixld

static int BASIS = 0;

typedef struct {
	int n, max;
	REAL *mem;
} vmblock_t;

static void *vminit (void) {
	return (vmblock_t *)calloc(1, sizeof(vmblock_t));
}

static int vmcomplete (void *vmblock) {
	return 1;
}

static void vmfree (void *vmblock) {
	vmblock_t *vmb = (vmblock_t *)vmblock;
	free(vmb->mem); free(vmblock);
}

static void *vmalloc (void *vmblock, int typ, size_t zeilen, size_t spalten) {
	vmblock_t *vmb = (vmblock_t *)vmblock;
	REAL *ret = 0;
	if (typ == 0) {
		if (vmb->n + zeilen > vmb->max) {
			vmb->max = vmb->n + zeilen;
			vmb->mem = (REAL *)realloc(vmb->mem, vmb->max*sizeof(REAL));
		}
		ret = vmb->mem + vmb->n;
		vmb->n += zeilen;
	}
	return ret;
}

/*--------------------------------------------------------------------*
 * Aux functions for  eigen                                           *
 *--------------------------------------------------------------------*/

/*====================================================================*
 *                                                                    *
 *  balance balances the matrix mat so that the rows with zero entries*
 *  off the diagonal are isolated and the remaining columns and rows  *
 *  are resized to have one norm close to 1.                          *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Input parameters:                                                *
 *   =================                                                *
 *      n        int n;  ( n > 0 )                                    *
 *               Dimension of mat                                     *
 *      mat      REAL   *mat[n];                                      *
 *               n x n input matrix                                   *
 *      basis    int basis;                                           *
 *               Base of number representation in the given computer  *
 *               (see BASIS)                                          *
 *                                                                    *
 *   Output parameters:                                               *
 *   ==================                                               *
 *      mat      REAL   *mat[n];                                      *
 *               scaled matrix                                        *
 *      low      int *low;                                            *
 *      high     int *high;                                           *
 *               the rows 0 to low-1 and those from high to n-1       *
 *               contain isolated eigenvalues (only nonzero entry on  *
 *               the diagonal)                                        *
 *      scal     REAL   scal[];                                       *
 *               the vector scal contains the isolated eigenvalues in *
 *               the positions 0 to low-1 and high to n-1, its other  *
 *               components contain the scaling factors for           *
 *               transforming mat.                                    *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Macros:     SWAP, ABS                                            *
 *   =======                                                          *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Constants used:     TRUE, FALSE                                  *
 *   ===============                                                  *
 *                                                                    *
 *====================================================================*/
static int balance       /* balance a matrix .........................*/
                   (int       n,      /* size of matrix ..............*/
                    REAL *    mat[],  /* matrix ......................*/
                    REAL      scal[], /* Scaling data ................*/
                    int *     low,    /* first relevant row index ....*/
                    int *     high,   /* last relevant row index .....*/
                    int       basis   /* base of computer numbers ....*/
                   ) {
  register int i, j;
  int      iter, k, m;
  REAL     b2, r, c, f, g, s;
  b2 = (REAL)(basis*basis);
  m = 0;
  k = n - 1;
  do {
    iter = FALSE;
    for (j=k; j >= 0; j--) {
      for (r=ZERO, i = 0; i <= k; i++)
        if (i != j)  r += ABS(mat[j][i]);
      if (r == ZERO) {
        scal[k] = (REAL) j;
        if (j != k) {
          for (i=0; i <= k; i++) SWAPT(REAL, mat[i][j], mat[i][k])
          for (i=m; i < n; i++)  SWAPT(REAL, mat[j][i], mat[k][i])
        }
        k--;
        iter = TRUE;
      }
    }  /* end of j */
  }  /* end of do  */
  while (iter);
  do {
    iter = FALSE;
    for (j=m; j <= k; j++) {
      for (c=ZERO, i = m; i <= k; i++)
        if (i != j) c += ABS(mat[i][j]);
      if (c == ZERO) {
        scal[m] = (REAL) j;
        if (j != m) {
          for (i=0; i <= k; i++) SWAPT(REAL, mat[i][j], mat[i][m])
          for (i=m; i < n; i++)  SWAPT(REAL, mat[j][i], mat[m][i])
        }
        m++;
        iter = TRUE;
      }
    }  /* end of j */
  }  /* end of do  */
  while (iter);
  *low = m;
  *high = k;
  for (i=m; i <= k; i++) scal[i] = ONE;
  do {
    iter = FALSE;
    for (i=m; i <= k; i++) {
      for (c = r = ZERO, j = m; j <= k; j++)
      if (j !=i) {
        c += ABS(mat[j][i]);
        r += ABS(mat[i][j]);
      }
      g = r/basis;
      f = ONE;
      s = c + r;
      while (c < g) {
        f *= basis;
        c *= b2;
      }
      g = r*basis;
      while (c >= g) {
        f /= basis;
        c /= b2;
      }
      if ((c + r)/f < (REAL)0.95*s) {
        g = ONE/f;
        scal[i] *= f;
        iter = TRUE;
        for (j=m; j < n; j++ ) mat[i][j] *= g;
        for (j=0; j <= k; j++ ) mat[j][i] *= f;
      }
    }
  }
  while (iter);
  return 0;
}

/*====================================================================*
 *                                                                    *
 *  balback reverses the balancing of balance for the eigenvactors.   *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Input parameters:                                                *
 *   =================                                                *
 *      n        int n;  ( n > 0 )                                    *
 *               Dimension of mat                                     *
 *      low      int low;                                             *
 *      high     int high;   see balance                              *
 *      eivec    REAL   *eivec[n];                                    *
 *               Matrix of eigenvectors, as computed in  qr2          *
 *      scal     REAL   scal[];                                       *
 *               Scaling data from  balance                           *
 *                                                                    *
 *   Output parameter:                                                *
 *   =================                                                *
 *      eivec    REAL   *eivec[n];                                    *
 *               Non-normalized eigenvectors of the original matrix   *
 *                                                                    *
 *   Macros:     SWAP()                                               *
 *   =======                                                          *
 *                                                                    *
 *====================================================================*/
static int balback       /* reverse balancing ........................*/
                   (int     n,        /* Dimension of matrix .........*/
                    int     low,      /* first nonzero row ...........*/
                    int     high,     /* last nonzero row ............*/
                    REAL    scal[],   /* Scaling data ................*/
                    REAL *  eivec[]   /* Eigenvectors ................*/
                   ) {
  register int i, j, k;
  REAL s;
  for (i=low; i <= high; i++) {
    s = scal[i];
    for (j=0; j < n; j++) eivec[i][j] *= s;
  }
  for (i=low - 1; i >= 0; i--) {
    k = (int)scal[i];
    if (k != i)
      for (j=0; j < n; j++) SWAPT(REAL, eivec[i][j], eivec[k][j])
  }
  for (i=high + 1; i < n; i++) {
    k = (int)scal[i];
    if (k != i)
      for (j=0; j < n; j++) SWAPT(REAL, eivec[i][j], eivec[k][j])
  }
  return 0;
}

/*====================================================================*
 *                                                                    *
 *  elmhes transforms the matrix mat to upper Hessenberg form.        *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Input parameters:                                                *
 *   =================                                                *
 *      n        int n;  ( n > 0 )                                    *
 *               Dimension of mat                                     *
 *      low      int low;                                             *
 *      high     int high; see  balance                               *
 *      mat      REAL   *mat[n];                                      *
 *               n x n matrix                                         *
 *                                                                    *
 *   Output parameter:                                                *
 *   =================                                                *
 *      mat      REAL   *mat[n];                                      *
 *               upper Hessenberg matrix; additional information on   *
 *               the transformation is stored in the lower triangle   *
 *      perm     int perm[];                                          *
 *               Permutation vector for elmtrans                      *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Macros:   SWAP, ABS                                              *
 *   =======                                                          *
 *                                                                    *
 *====================================================================*/
static int elmhes       /* reduce matrix to upper Hessenberg form ....*/
                  (int       n,       /* Dimension of matrix .........*/
                   int       low,     /* first nonzero row ...........*/
                   int       high,    /* last nonzero row ............*/
                   REAL *    mat[],   /* input/output matrix .........*/
                   int       perm[]   /* Permutation vector ..........*/
                  ) {
  register int i, j, m;
  REAL x, y;
  for (m=low + 1; m < high; m++) {
    i = m;
    x = ZERO;
    for (j=m; j <= high; j++)
      if (ABS(mat[j][m - 1]) > ABS(x)) {
        x = mat[j][m - 1];
        i = j;
      }
    perm[m] = i;
    if (i != m) {
      for (j=m - 1; j < n; j++) SWAPT(REAL, mat[i][j], mat[m][j])
      for (j=0; j <= high; j++) SWAPT(REAL, mat[j][i], mat[j][m])
    }
    if (x != ZERO) {
      for (i=m + 1; i <= high; i++) {
        y = mat[i][m - 1];
        if (y != ZERO) {
          y = mat[i][m - 1] = y/x;
          for (j=m; j < n; j++) mat[i][j] -= y*mat[m][j];
          for (j=0; j <= high; j++) mat[j][m] += y*mat[j][i];
        }
      }  /* end i */
    }
  }  /* end m */
  return 0;
}

/*====================================================================*
 *                                                                    *
 *  elmtrans copies the Hessenberg matrix stored in mat to h.         *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Input parameters:                                                *
 *   =================                                                *
 *      n        int n;  ( n > 0 )                                    *
 *               Dimension of  mat and eivec                          *
 *      low      int low;                                             *
 *      high     int high; see  balance                               *
 *      mat      REAL   *mat[n];                                      *
 *               n x n input matrix                                   *
 *      perm     int *perm;                                           *
 *               Permutation data from  elmhes                        *
 *                                                                    *
 *   Output parameter:                                                *
 *   =================                                                *
 *      h        REAL   *h[n];                                        *
 *               Hessenberg matrix                                    *
 *                                                                    *
 *====================================================================*/
static int elmtrans       /* copy to Hessenberg form .................*/
                    (int     n,       /* Dimension of matrix .........*/
                     int     low,     /* first nonzero row ...........*/
                     int     high,    /* last nonzero row ............*/
                     REAL *  mat[],   /* input matrix ................*/
                     int     perm[],  /* row permutations ............*/
                     REAL *  h[]      /* Hessenberg matrix ...........*/
                    ) {
  register int k, i, j;
  for (i=0; i < n; i++) {
    for (k=0; k < n; k++) h[i][k] = ZERO;
    h[i][i] = ONE;
  }
  for (i=high - 1; i > low; i--) {
    j = perm[i];
    for (k=i + 1; k <= high; k++) h[k][i] = mat[k][i - 1];
    if (i != j) {
      for (k=i; k <= high; k++) {
        h[i][k] = h[j][k];
        h[j][k] = ZERO;
      }
      h[j][i] = ONE;
    }
  }
  return 0;
}

/* ------------------------------------------------------------------ */

/***********************************************************************
* This function reduces matrix mat to upper Hessenberg form by         *
* Householder transformations. All details of the transformations are  *
* stored in the remaining triangle of the Hessenberg matrix and in     *
* vector d.                                                            *
*                                                                      *
* Input parameters:                                                    *
* =================                                                    *
* n        dimension of mat                                            *
* low  \   rows 0 to low-1 and high+1 to n-1 contain isolated          *
* high  >  eigenvalues, i. e. eigenvalues corresponding to             *
*      /   eigenvectors that are multiples of unit vectors             *
* mat      [0..n-1,0..n-1] matrix to be reduced                        *
*                                                                      *
* Output parameters:                                                   *
* ==================                                                   *
* mat      the desired Hessenberg matrix together with the first part  *
*          of the reduction information below the subdiagonal          *
* d        [0..n-1] vector with the remaining reduction information    *
*                                                                      *
* Return value:                                                        *
* =============                                                        *
* Error code. This can only be the value 0 here.                       *
*                                                                      *
* global names used:                                                   *
* ==================                                                   *
* REAL, MACH_EPS, ZERO, SQRT                                           *
*                                                                      *
************************************************************************
* Literature: Numerical Mathematics 12 (1968), pages 359 and 360       *
***********************************************************************/
static int orthes (   /* reduce orthogonally to upper Hessenberg form */
                  int  n,                  /* Dimension of matrix     */
                  int  low,                /* [low,low]..[high,high]: */
                  int  high,               /* submatrix to be reduced */
                  REAL *mat[],             /* input/output matrix     */
                  REAL d[]                 /* reduction information   */
                 ) {                       /* error code              */
  int  i, j, m;    /* loop variables                                  */
  REAL s,          /* Euclidean norm sigma of the subdiagonal column  */
                   /* vector v of mat, that shall be reflected into a */
                   /* multiple of the unit vector e1 = (1,0,...,0)    */
                   /* (v = (v1,..,v(high-m+1))                        */
       x = ZERO,   /* first element of v in the beginning, then       */
                   /* summation variable in the actual Householder    */
                   /* transformation                                  */
       y,          /* sigma^2 in the beginning, then ||u||^2, with    */
                   /* u := v +- sigma * e1                            */
       eps;        /* tolerance for checking if the transformation is */
                   /* valid                                           */
  eps = (REAL)128.0*MACH_EPS;
  for (m=low + 1; m < high; m++) {
    for (y=ZERO, i = high; i >= m; i--)
      x    = mat[i][m - 1],
      d[i] = x,
      y    = y + x*x;
    if (y <= eps)
      s = ZERO;
    else {
      s = (x >= ZERO) ? -SQRT(y) : SQRT(y);
      y    -= x*s;
      d[m] =  x - s;
      for (j=m; j < n; j++) {  /* multiply mat from the left by (E-(u*uT)/y) */
        for (x=ZERO, i = high; i >= m; i--)
          x += d[i]*mat[i][j];
        for (x /= y, i = m; i <= high; i++)
          mat[i][j] -= x*d[i];
      }
      for (i=0; i <= high; i++) {  /* multiply mat from the right by (E-(u*uT)/y) */
        for (x=ZERO, j = high; j >= m; j--)
          x += d[j]*mat[i][j];
        for (x /= y, j = m; j <= high; j++)
          mat[i][j] -= x*d[j];
      }
    }
    mat[m][m - 1] = s;
  }
  return 0;

}    /* --------------------------- orthes -------------------------- */

/* ------------------------------------------------------------------ */

/***********************************************************************
* compute the matrix v of accumulated transformations from the         *
* information left by the Householder reduction of matrix mat to upper *
* Hessenberg form below the Hessenberg matrix in mat and in the        *
* vector d. The contents of the latter are destroyed.                  *
*                                                                      *
* Input parameters:                                                    *
* =================                                                    *
* n        dimension of mat                                            *
* low  \   rows 0 to low-1 and high+1 to n-1 contain isolated          *
* high  >  eigenvalues, i. e. eigenvalues corresponding to             *
*      /   eigenvectors that are multiples of unit vectors             *
* mat      [0..n-1,0..n-1] matrix produced by `orthes' giving the      *
*          upper Hessenberg matrix and part of the information on the  *
*          orthogonal reduction                                        *
* d        [0..n-1] vector with the remaining information on the       *
*          orthogonal reduction to upper Hessenberg form               *
*                                                                      *
* Output parameters:                                                   *
* ==================                                                   *
* d        input vector destroyed by this function                     *
* v        [0..n-1,0..n-1] matrix defining the similarity reduction    *
*          to upper Hessenberg form                                    *
*                                                                      *
* Return value:                                                        *
* =============                                                        *
* Error code. This can only be the value 0 here.                       *
*                                                                      *
* Global names used:                                                   *
* ==================                                                   *
* REAL, ZERO, ONE                                                      *
*                                                                      *
************************************************************************
* Literature: Numerical Mathematics 16 (1970), page 191                *
***********************************************************************/
static int orttrans       /* compute orthogonal transformation matrix */
                   (
                    int  n,      /* Dimension of matrix               */
                    int  low,    /* [low,low]..[high,high]: submatrix */
                    int  high,   /* affected by the reduction         */
                    REAL *mat[], /* Hessenberg matrix, reduction inf. */
                    REAL d[],    /* remaining reduction information   */
                    REAL *v[]    /* transformation matrix             */
                   ) {           /* error code                        */
  int  i, j, m;                        /* loop variables              */
  REAL x,                              /* summation variable in the   */
                                       /* Householder transformation  */
       y;                              /* sigma respectively          */
                                       /* sigma * (v1 +- sigma)       */
  for (i=0; i < n; i++) {            /* form the unit matrix in v     */
    for (j=0; j < n; j++)
      v[i][j] = ZERO;
    v[i][i] = ONE;
  }
  for (m=high - 1; m > low; m--) {   /* apply the transformations     */
                                       /* that reduced mat to upper   */
    y = mat[m][m - 1];                 /* Hessenberg form also to the */
                                       /* unit matrix in v. This      */
    if (y != ZERO) {                   /* produces the desired        */
                                       /* transformation matrix in v. */
      y *= d[m];
      for (i=m + 1; i <= high; i++)
        d[i] = mat[i][m - 1];
      for (j=m; j <= high; j++) {
        for (x=ZERO, i = m; i <= high; i++)
          x += d[i]*v[i][j];
        for (x /= y, i = m; i <= high; i++)
          v[i][j] += x*d[i];
      }
    }
  }
  return 0;
}    /* -------------------------- orttrans ------------------------- */

/*====================================================================*
 *                                                                    *
 *  hqrvec computes the eigenvectors for the eigenvalues found in hqr2*
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Input parameters:                                                *
 *   =================                                                *
 *      n        int n;  ( n > 0 )                                    *
 *               Dimension of  mat and eivec, number of eigenvalues.  *
 *      low      int low;                                             *
 *      high     int high; see  balance                               *
 *      h        REAL   *h[n];                                        *
 *               upper Hessenberg matrix                              *
 *      wr       REAL   wr[n];                                        *
 *               Real parts of the n eigenvalues.                     *
 *      wi       REAL   wi[n];                                        *
 *               Imaginary parts of the n eigenvalues.                *
 *                                                                    *
 *   Output parameter:                                                *
 *   =================                                                *
 *      eivec    REAL   *eivec[n];                                    *
 *               Matrix, whose columns are the eigenvectors           *
 *                                                                    *
 *   Return value:                                                    *
 *   =============                                                    *
 *      =  0     all ok                                               *
 *      =  1     h is the zero matrix.                                *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Constants used:    MACH_EPS                                      *
 *   ===============                                                  *
 *                                                                    *
 *   Macros:   SQR, ABS                                               *
 *   =======                                                          *
 *                                                                    *
 *====================================================================*/

#define agnc_divld(z,a,b,c,d) { \
  REAL (_e) = ((c)*(c)+(d)*(d)); \
  agnCmplx_create((z), ((a)*(c)+(b)*(d))/((_e)), ((b)*(c)-(a)*(d))/(_e)); \
}

static int hqrvec       /* compute eigenvectors ......................*/
                  (int     n,           /* Dimension of matrix .......*/
                   int     low,         /* first nonzero row .........*/
                   int     high,        /* last nonzero row ..........*/
                   REAL *  h[],         /* upper Hessenberg matrix ...*/
                   REAL    wr[],        /* Real parts of evalues .....*/
                   REAL    wi[],        /* Imaginary parts of evalues */
                   REAL *  eivec[]      /* Eigenvectors ..............*/
                  ) {
  int  i, j, k, l, m, en, na;
  REAL p, q, r = ZERO, s = ZERO, t, w, x, y, z = ZERO,
       ra, sa, vr, vi, norm;
  for (norm=ZERO, i = 0; i < n; i++) {      /* find norm of h         */
    for (j=i; j < n; j++) norm += ABS(h[i][j]);
  }
  if (norm == ZERO) return 1;               /* zero matrix            */
  for (en=n - 1; en >= 0; en--) {           /* transform back         */
    p = wr[en];
    q = wi[en];
    na = en - 1;
    if (q == ZERO) {
      m = en;
      h[en][en] = ONE;
      for (i=na; i >= 0; i--) {
        w = h[i][i] - p;
        r = h[i][en];
        for (j=m; j <= na; j++) r += h[i][j]*h[j][en];
        if (wi[i] < ZERO) {
          z = w;
          s = r;
        } else {
          m = i;
          if (wi[i] == ZERO)
            h[i][en] = -r/((w != ZERO) ? (w) : (MACH_EPS*norm));
          else {  /* solve the linear system: */
            /* | w   x |  | h[i][en]   |   | -r |  */
            /* |       |  |            | = |    |  */
            /* | y   z |  | h[i+1][en] |   | -s |  */
            x = h[i][i + 1];
            y = h[i + 1][i];
            q = SQR(wr[i] - p) + SQR(wi[i]);
            h[i][en] = t = (x*s - z*r)/q;
            h[i + 1][en] = ((ABS(x) > ABS(z)) ?
                           (-r - w*t)/x : (-s - y*t)/z);
          }
        }  /* wi[i] >= 0  */
      }  /*  end i     */
    }  /* end q = 0  */
    else if (q < ZERO) {
      m = na;
      if (ABS(h[en][na]) > ABS(h[na][en])) {
        h[na][na] = -(h[en][en] - p)/h[en][na];
        h[na][en] = -q/h[en][na];
      } else {  /* OS2_DOS */
        REAL c[2];
        /* c = -h[na][en]/(h[na][na] - p + q*I); */
        agnc_divld(c, -h[na][en], 0, h[na][na] - p, q);
        h[na][na] = c[0]; h[na][en] = c[1];
      }
      h[en][na] = ONE;
      h[en][en] = ZERO;
      for (i=na - 1; i >= 0; i--) {
        w = h[i][i] - p;
        ra = h[i][en];
        sa = ZERO;
        for (j=m; j <= na; j++) {
          ra += h[i][j]*h[j][na];
          sa += h[i][j]*h[j][en];
        }
        if (wi[i] < ZERO) {
          z = w;
          r = ra;
          s = sa;
        } else {
          m = i;
          if (wi[i] == ZERO) {  /* OS2_DOS */
            REAL c[2];
            agnc_divld(c, -ra, -sa, w, q);
            /* c = (-ra - sa*I)/(w + q*I); */
            h[i][na] = c[0]; h[i][en] = c[1];
          } else {
            /* solve complex linear system:                              */
            /* | w+i*q     x | | h[i][na] + i*h[i][en]  |   | -ra+i*sa | */
            /* |             | |                        | = |          | */
            /* |   y    z+i*q| | h[i+1][na]+i*h[i+1][en]|   | -r+i*s   | */
            x = h[i][i + 1];
            y = h[i + 1][i];
            vr = SQR(wr[i] - p) + SQR(wi[i]) - SQR(q);
            vi = TWO*q*(wr[i] - p);
            if (vr == ZERO && vi == ZERO)
              vr = MACH_EPS*norm *
                  (ABS(w) + ABS(q) + ABS(x) + ABS(y) + ABS(z));
            {  /* OS2_DOS */
              REAL c[2];
              /* c = (x*r - z*ra + q*sa + I*(x*s - z*sa - q*ra))/(vr + I*vi); */
              agnc_divld(c, x*r - z*ra + q*sa, x*s - z*sa - q*ra, vr, vi);
              h[i][na] = c[0]; h[i][en] = c[1];
            }
            if (ABS(x) > ABS(z) + ABS(q)) {
              h[i + 1][na] = (-ra - w*h[i][na] + q*h[i][en])/x;
              h[i + 1][en] = (-sa - w*h[i][en] - q*h[i][na])/x;
            } else {  /* OS2_DOS */
              REAL c[2];
              /* c = (-r - y*h[i][na] + I*(-s -y*h[i][en]))/(z + I*q); */
              agnc_divld(c, -r - y*h[i][na], -s -y*h[i][en], z, q);
              h[i + 1][na] = c[0]; h[i + 1][en] = c[1];
            }
          }  /* end wi[i] > 0 */
        }  /* end wi[i] >= 0 */
      }  /* end i           */
    }  /*  if q < 0        */
  }  /* end  en           */
  for (i=0; i < n; i++)         /* Eigenvectors for the evalues for */
    if (i < low || i > high)    /* rows < low  and rows  > high     */
      for (k=i + 1; k < n; k++) eivec[i][k] = h[i][k];
  for (j=n - 1; j >= low; j--) {
    m = (j <= high) ? j : high;
    if (wi[j] < ZERO) {
      for (l=j - 1, i = low; i <= high; i++) {
        for (y = z = ZERO, k = low; k <= m; k++) {
          y += eivec[i][k]*h[k][l];
          z += eivec[i][k]*h[k][j];
        }
        eivec[i][l] = y;
        eivec[i][j] = z;
      }
    } else if (wi[j] == ZERO) {
      for (i=low; i <= high; i++) {
        for (z=ZERO, k = low; k <= m; k++)
          z += eivec[i][k]*h[k][j];
        eivec[i][j] = z;
      }
    }
  }  /* end j */
  return 0;
}

/*====================================================================*
 *                                                                    *
 *  hqr2 computes the eigenvalues and (if vec != 0) the eigenvectors  *
 *  of an  n * n upper Hessenberg matrix.                             *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Control parameter:                                               *
 *   ==================                                               *
 *      vec      int vec;                                             *
 *       = 0      compute eigenvalues only                             *
 *       = 1     compute all eigenvalues and eigenvectors             *
 *                                                                    *
 *   Input parameters:                                                *
 *   =================                                                *
 *      n        int n;  ( n > 0 )                                    *
 *               Dimension of  mat and eivec,                         *
 *               length of the real parts vector  wr and of the       *
 *               imaginary parts vector  wi of the eigenvalues.       *
 *      low      int low;                                             *
 *      high     int high; see  balance                               *
 *      h        REAL   *h[n];                                        *
 *               upper  Hessenberg matrix                             *
 *                                                                    *
 *   Output parameters:                                               *
 *   ==================                                               *
 *      eivec    REAL   *eivec[n];     ( bei vec = 1 )                *
 *               Matrix, which for vec = 1 contains the eigenvectors  *
 *               as follows  :                                        *
 *               For real eigebvalues the corresponding column        *
 *               contains the corresponding eigenvactor, while for    *
 *               complex eigenvalues the corresponding column contains*
 *               the real part of the eigenvactor with its imaginary  *
 *               part is stored in the subsequent column of eivec.    *
 *               The eigenvactor for the complex conjugate eigenvactor*
 *               is given by the complex conjugate eigenvactor.       *
 *      wr       REAL   wr[n];                                        *
 *               Real part of the n eigenvalues.                      *
 *      wi       REAL   wi[n];                                        *
 *               Imaginary parts of the eigenvalues                   *
 *      cnt      int cnt[n];                                          *
 *               vector of iterations used for each eigenvalue.       *
 *               For a complex conjugate eigenvalue pair the second   *
 *               entry is negative.                                   *
 *                                                                    *
 *   Return value:                                                    *
 *   ==============                                                   *
 *      =   0    all ok                                               *
 *      = 4xx    Iteration maximum exceeded when computing evalue xx  *
 *      =  99    zero  matrix                                         *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   functions in use:                                                *
 *   =================                                                *
 *                                                                    *
 *      int hqrvec(): reverse transform for eigenvectors              *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Constants used:   MACH_EPS, MAXIT                                *
 *   ===============                                                  *
 *                                                                    *
 *   Macros:   SWAP, ABS, SQRT                                        *
 *   =======                                                          *
 *                                                                    *
 *====================================================================*/
static int hqr2         /* compute eigenvalues .......................*/
                (int     vec,         /* switch for computing evectors*/
                 int     n,           /* Dimension of matrix .........*/
                 int     low,         /* first nonzero row ...........*/
                 int     high,        /* last nonzero row ............*/
                 REAL *  h[],         /* Hessenberg matrix ...........*/
                 REAL    wr[],        /* Real parts of eigenvalues ...*/
                 REAL    wi[],        /* Imaginary parts of evalues ..*/
                 REAL *  eivec[],     /* Matrix of eigenvectors ......*/
                 int     cnt[]        /* Iteration counter ...........*/
                ) {
  int  i, j, na, en, iter, k, l, m;
  REAL p = ZERO, q = ZERO, r = ZERO, s, t, w, x, y, z;
  for (i=0; i < n; i++)
    if (i < low || i > high) {
      wr[i] = h[i][i];
      wi[i] = ZERO;
      cnt[i] = 0;
    }
  en = high;
  t = ZERO;
  while (en >= low) {
    iter = 0;
    na = en - 1;
    for (;;) {
      for (l=en; l > low; l--)  /* search for small subdiagonal element */
        if (ABS(h[l][l - 1]) <=
            MACH_EPS*(ABS(h[l - 1][l - 1]) + ABS(h[l][l]))) break;
      x = h[en][en];
      if (l == en) {  /* found one evalue */
        wr[en] = h[en][en] = x + t;
        wi[en] = ZERO;
        cnt[en] = iter;
        en--;
        break;
      }
      y = h[na][na];
      w = h[en][na]*h[na][en];
      if (l == na) {  /* found two evalues */
        p = (y - x)*0.5;
        q = p*p + w;
        z = SQRT(ABS(q));
        x = h[en][en] = x + t;
        h[na][na] = y + t;
        cnt[en] = -iter;
        cnt[na] = iter;
        if (q >= ZERO) {  /* real eigenvalues */
          z = (p < ZERO) ? (p - z) : (p + z);
          wr[na] = x + z;
          wr[en] = s = x - w/z;
          wi[na] = wi[en] = ZERO;
          x = h[en][na];
          r = HYPOT(x, z);  /* SQRT(x*x + z*z); */
          if (vec) {
            p = x/r;
            q = z/r;
            for (j=na; j < n; j++) {
              z = h[na][j];
              h[na][j] = q*z + p*h[en][j];
              h[en][j] = q*h[en][j] - p*z;
            }
            for (i=0; i <= en; i++) {
              z = h[i][na];
              h[i][na] = q*z + p*h[i][en];
              h[i][en] = q*h[i][en] - p*z;
            }
            for (i=low; i <= high; i++) {
              z = eivec[i][na];
              eivec[i][na] = q*z + p*eivec[i][en];
              eivec[i][en] = q*eivec[i][en] - p*z;
            }
          }  /* end if (vec) */
        }  /* end if (q >= ZERO) */
        else {  /* pair of complex conjugate evalues */
          wr[na] = wr[en] = x + p;
          wi[na] =   z;
          wi[en] = - z;
        }
        en -= 2;
        break;
      }  /* end if (l == na) */
      if (iter >= MAXIT) {
        cnt[en] = MAXIT + 1;
        return en;  /* MAXIT Iterations */
      }
      if ((iter != 0) && (iter % 10 == 0)) {
        t += x;
        for (i=low; i <= en; i++) h[i][i] -= x;
        s = ABS(h[en][na]) + ABS(h[na][en - 2]);
        x = y = (REAL)0.75*s;
        w = -(REAL)0.4375*s*s;
      }
      iter++;
      for (m=en - 2; m >= l; m--) {
        z = h[m][m];
        r = x - z;
        s = y - z;
        p = (r*s - w)/h[m + 1][m] + h[m][m + 1];
        q = h[m + 1][m + 1] - z - r - s;
        r = h[m + 2][m + 1];
        s = ABS(p) + ABS(q) + ABS(r);
        p /= s;
        q /= s;
        r /= s;
        if (m == l) break;
        if (ABS(h[m][m - 1])*(ABS(q) + ABS(r)) <=
            MACH_EPS*ABS(p)*(ABS(h[m - 1][m - 1]) + ABS(z) + ABS(h[m + 1][m + 1])))
          break;
      }
      for (i=m + 2; i <= en; i++) h[i][i - 2] = ZERO;
      for (i=m + 3; i <= en; i++) h[i][i - 3] = ZERO;
      for (k=m; k <= na; k++) {
        if (k != m) {  /* double  QR step, for rows l to en and columns m to en */
          p = h[k][k - 1];
          q = h[k + 1][k - 1];
          r = (k != na) ? h[k + 2][k - 1] : ZERO;
          x = ABS(p) + ABS(q) + ABS(r);
          if (x == ZERO) continue;  /*  next k */
          p /= x;
          q /= x;
          r /= x;
        }
        s = SQRT(p*p + q*q + r*r);
        if (p < ZERO) s = -s;
        if (k != m) h[k][k - 1] = -s*x;
        else if (l != m) h[k][k - 1] = -h[k][k - 1];
        p += s;
        x = p/s;
        y = q/s;
        z = r/s;
        q /= p;
        r /= p;
        for (j=k; j < n; j++) {  /* modify rows */
          p = h[k][j] + q*h[k + 1][j];
          if (k != na) {
            p += r*h[k + 2][j];
            h[k + 2][j] -= p*z;
          }
          h[k + 1][j] -= p*y;
          h[k][j]   -= p*x;
        }
        j = (k + 3 < en) ? (k + 3) : en;
        for (i=0; i <= j; i++) {  /* modify columns */
          p = x*h[i][k] + y*h[i][k + 1];
          if (k != na) {
            p += z*h[i][k + 2];
            h[i][k + 2] -= p*r;
          }
          h[i][k + 1] -= p*q;
          h[i][k]   -= p;
        }
        if (vec) {  /* if eigenvectors are needed .. */
          for (i=low; i <= high; i++) {
            p = x*eivec[i][k] + y*eivec[i][k + 1];
            if (k != na) {
              p += z*eivec[i][k + 2];
              eivec[i][k + 2] -= p*r;
            }
            eivec[i][k + 1] -= p*q;
            eivec[i][k]   -= p;
          }
        }
      }  /* end k */
    }  /* end for (;;) */
  }  /* while (en >= low)                        All evalues found    */
  if (vec)                                /* transform evectors back  */
    if (hqrvec(n, low, high, h, wr, wi, eivec)) return 99;
  return 0;
}

/*====================================================================*
 *                                                                    *
 *  norm_1 normalizes the one norm of the column vectors in v.        *
 *  (special attention to complex vectors in v  is given)             *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Input parameters:                                                *
 *   =================                                                *
 *      n        int n; ( n > 0 )                                     *
 *               Dimension of matrix v                                *
 *      v        REAL   *v[];                                         *
 *               Matrix of eigenvectors                               *
 *      wi       REAL   wi[];                                         *
 *               Imaginary parts of the eigenvalues                   *
 *                                                                    *
 *   Output parameter:                                                *
 *   =================                                                *
 *      v        REAL   *v[];                                         *
 *               Matrix with normalized eigenvectors                  *
 *                                                                    *
 *   Return value:                                                    *
 *   =============                                                    *
 *      = 0      all ok                                               *
 *      = 1      n < 1                                                *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   functions used:                                                  *
 *   ===============                                                  *
 *      REAL   comabs():  complex absolute value                      *
 *      int    comdiv():  complex division                            *
 *                                                                    *
 *   Macros:   ABS                                                    *
 *   ========                                                         *
 *                                                                    *
 *====================================================================*/
static int norm_1       /* normalize eigenvectors to have one norm 1 .*/
                  (int     n,       /* Dimension of matrix ...........*/
                   REAL *  v[],     /* Matrix with eigenvektors ......*/
                   REAL    wi[]     /* Imaginary parts of evalues ....*/
                  ) {
  int  i, j;
  REAL maxi, tr, ti;
  if (n < 1) return 1;
  for (j=0; j < n; j++) {
    if (wi[j] == ZERO) {
      maxi = v[0][j];
      for (i=1; i < n; i++)
        if (ABS(v[i][j]) > ABS(maxi)) maxi = v[i][j];
      if (maxi != ZERO) {
        maxi = ONE/maxi;
        for (i=0; i < n; i++) v[i][j] *= maxi;
      }
    } else {
      tr = v[0][j];
      ti = v[0][j + 1];
      for (i=1; i < n; i++) {
        /* if (CABS(v[i][j] + I*v[i][j + 1]) > CABS(tr + I*ti)) { */
        if (HYPOT(v[i][j], v[i][j + 1]) > HYPOT(tr, ti)) {  /* OS2_DOS */
          tr = v[i][j];
          ti = v[i][j + 1];
        }
      }
      if (tr != ZERO || ti != ZERO) {
        for (i=0; i < n; i++) {  /* OS2_DOS */
          REAL c[2];
          /* c = (v[i][j] + I*v[i][j + 1])/(tr + I*ti); */
          agnc_divld(c, v[i][j], v[i][j + 1], tr, ti);
          v[i][j] = c[0]; v[i][j + 1] = c[1];
        }
      }
      j++;  /* raise j by two */
    }
  }
  return 0;
}

/*====================================================================*
 *                                                                    *
 *  The function  eigen  determines all eigenvalues and (if desired)  *
 *  all eigenvectors of a real square  n * n  matrix via the QR method*
 *  in the version of  Martin, Parlett, Peters, Reinsch and Wilkinson.*
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Literature:                                                      *
 *   ===========                                                      *
 *      1) Peters, Wilkinson: Eigenvectors of real and complex        *
 *         matrices by LR and QR triangularisations,                  *
 *         Num. Math. 16, p.184-204, (1970); [PETE70]; contribution   *
 *         II/15, p. 372 - 395 in [WILK71].                           *
 *      2) Martin, Wilkinson: Similarity reductions of a general      *
 *         matrix to Hessenberg form, Num. Math. 12, p. 349-368,(1968)*
 *         [MART 68]; contribution II,13, p. 339 - 358 in [WILK71].   *
 *      3) Parlett, Reinsch: Balancing a matrix for calculations of   *
 *         eigenvalues and eigenvectors, Num. Math. 13, p. 293-304,   *
 *         (1969); [PARL69]; contribution II/11, p.315 - 326 in       *
 *         [WILK71].                                                  *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Control parameters:                                              *
 *   ===================                                              *
 *      vec      int vec;                                             *
 *               call for eigen :                                     *
 *       = 0     compute eigenvalues only                             *
 *       = 1     compute all eigenvalues and eigenvectors             *
 *      ortho    flag that shows if transformation of mat to          *
 *               Hessenberg form shall be done orthogonally by        *
 *               `orthes' (flag set) or elementarily by `elmhes'      *
 *               (flag cleared). The Householder matrices used in     *
 *               orthogonal transformation have the advantage of      *
 *               preserving the symmetry of input matrices.           *
 *      ev_norm  flag that shows if Eigenvectors shall be             *
 *               normalized (flag set) or not (flag cleared)          *
 *                                                                    *
 *   Input parameters:                                                *
 *   =================                                                *
 *      n        int n;  ( n > 0 )                                    *
 *               size of matrix, number of eigenvalues                *
 *      mat      REAL   *mat[n];                                      *
 *               matrix                                               *
 *                                                                    *
 *   Output parameters:                                               *
 *   ==================                                               *
 *      eivec    REAL   *eivec[n];     ( bei vec = 1 )                *
 *               matrix, if  vec = 1  this holds the eigenvectors     *
 *               thus :                                               *
 *               If the jth eigenvalue of the matrix is real then the *
 *               jth column is the corresponding real eigenvector;    *
 *               if the jth eigenvalue is complex then the jth column *
 *               of eivec contains the real part of the eigenvector   *
 *               while its imaginary part is in column j+1.           *
 *               (the j+1st eigenvector is the complex conjugate      *
 *               vector.)                                             *
 *      valre    REAL   valre[n];                                     *
 *               Real parts of the eigenvalues.                       *
 *      valim    REAL   valim[n];                                     *
 *               Imaginary parts of the eigenvalues                   *
 *      cnt      int cnt[n];                                          *
 *               vector containing the number of iterations for each  *
 *               eigenvalue. (for a complex conjugate pair the second *
 *               entry is negative.)                                  *
 *                                                                    *
 *   Return value :                                                   *
 *   =============                                                    *
 *      =   0    all ok                                               *
 *      =   1    n < 1 or other invalid input parameter               *
 *      =   2    insufficient memory                                  *
 *      = 10x    error x from balance()                               *
 *      = 20x    error x from elmh()                                  *
 *      = 30x    error x from elmtrans()   (for vec = 1 only)         *
 *      = 4xx    error xx from hqr2()                                 *
 *      = 50x    error x from balback()    (for vec = 1 only)         *
 *      = 60x    error x from norm_1()     (for vec = 1 only)         *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Functions in use  :                                              *
 *   ===================                                              *
 *                                                                    *
 *   static int balance (): Balancing of an  n x n  matrix            *
 *   static int elmh ():    Transformation to upper Hessenberg form   *
 *   static int elmtrans(): initialise eigenvectors                   *
 *   static int hqr2 ():    compute eigenvalues/eigenvectors          *
 *   static int balback (): Reverse balancing to obtain eigenvectors  *
 *   static int norm_1 ():  Normalize eigenvectors                    *
 *                                                                    *
 *   void *vmalloc():       allocate vector or matrix                 *
 *   void vmfree():         free list of vectors and matrices         *
 *                                                                    *
 *====================================================================*
 *                                                                    *
 *   Constants used :     NULL, BASIS                                 *
 *   ================                                                 *
 *                                                                    *
 *====================================================================*/
static int eigen (      /* Compute all evalues/evectors of a matrix ..*/
           int     vec,           /* switch for computing evectors ...*/
           int     ortho,         /* orthogonal Hessenberg reduction? */
           int     ev_norm,       /* normalize Eigenvectors? .........*/
           int     n,             /* size of matrix ..................*/
           int     balanceonly,   /* just balance and return .........*/
           REAL *  mat[],         /* input matrix ....................*/
           REAL *  eivec[],       /* Eigenvectors ....................*/
           REAL    valre[],       /* real parts of eigenvalues .......*/
           REAL    valim[],       /* imaginary parts of eigenvalues ..*/
           int     cnt[]          /* Iteration counter ...............*/
          ) {
  int      i, low, high, rc;
  REAL     *scale,
           *d = NULL;
  void     *vmblock;
  if (n < 1) return 1;  /*  n >= 1 .. */
  if ( (balanceonly == 0 && (valre == NULL || valim == NULL)) || mat == NULL || cnt == NULL) return 1;
  for (i=0; i < n; i++)
    if (mat[i] == NULL) return 1;
  for (i=0; i < n; i++) cnt[i] = 0;
  if (n == 1) {  /*  n = 1 .. */
    eivec[0][0] = ONE;
    valre[0]    = mat[0][0];
    valim[0]    = ZERO;
    return 0;
  }
  if (vec) {
    if (eivec == NULL) return 1;
    for (i=0; i < n; i++)
      if (eivec[i] == NULL) return 1;
  }
  vmblock = vminit();
  scale = (REAL *)vmalloc(vmblock, VEKTOR, n, 0);
  if (!vmcomplete(vmblock)) return 2;  /* memory error */
  /* if (vec && ortho) { */ /* with Eigenvectors and orthogonal Hessenberg reduction? */
  if (!balanceonly && ortho) {  /* with orthogonal Hessenberg reduction? OS2_DOS fix */
    d = (REAL *)vmalloc(vmblock, VEKTOR, n, 0);
    if (!vmcomplete(vmblock)) {
      vmfree(vmblock);
      return 1;
    }
  }
  /* balance mat for nearly equal row and column one norms */
  rc = balance(n, mat, scale, &low, &high, BASIS);
  if (rc) {
    vmfree(vmblock);
    return 100 + rc;
  }
  if (balanceonly) {
    vmfree(vmblock);
    return 0;
  }
  rc = (ortho) ? orthes(n, low, high, mat, d) : elmhes(n, low, high, mat, cnt);
  if (rc) {  /* reduce mat to upper Hessenberg form */
    vmfree(vmblock);
    return 200 + rc;
  }
  if (vec) {  /* initialize eivec */
    rc = (ortho) ? orttrans(n, low, high, mat,   d, eivec) :
                   elmtrans(n, low, high, mat, cnt, eivec);
    if (rc) {
      vmfree(vmblock);
      return 300 + rc;
    }
  }
  /* execute Francis QR algorithm to obtain eigenvalues */
  rc = hqr2(vec, n, low, high, mat, valre, valim, eivec, cnt);
  if (rc) {
    vmfree(vmblock);
    return 400 + rc;
  }
  if (vec) {  /* reverse balancing if eigenvectors are to be determined */
    rc = balback(n, low, high, scale, eivec);
    if (rc) {
      vmfree(vmblock);
      return 500 + rc;
    }
    if (ev_norm)  /* normalize eigenvectors */
      rc = norm_1(n, eivec, valim);
    if (rc) {
      vmfree(vmblock);
      return 600 + rc;
    }
  }
  vmfree(vmblock);
  return 0;
}

/* _a[0..n^2-1] is a real general matrix. On return, evalr store the
   real part of eigenvalues and evali the imgainary part. If _evec is
   not a NULL pointer, eigenvectors will be stored there. */
static int aux_eigeng (REAL *_a, int n, int balanceonly, int ortho, int ev_norm,
                       REAL *evalr, REAL *evali, REAL *_evec) {
  REAL **a = NULL; REAL **evec = NULL;
  int i, j, *cnt, rc;
  a = (REAL **)calloc(n, sizeof(void*));
  if (!a) return 2;
  if (_evec) {
    evec = (REAL **)calloc(n, sizeof(void*));
    if (!evec) return 2;
  }
  cnt = (int *)calloc(n, sizeof(int));
  if (!cnt) return 2;
  for (i=0; i < n; ++i) {
    a[i] = _a + i*n;
    if (_evec) evec[i] = _evec + i*n;
  }
  rc = eigen((_evec) ? 1 : 0, ortho, ev_norm, n, balanceonly, a, evec, evalr, evali, cnt);
  if (!rc && _evec) {
    REAL tmp;
    for (j=0; j < n; ++j) {
      tmp = 0.0;
      for (i=0; i < n; ++i) tmp += SQR(evec[i][j]);
      tmp = SQRT(tmp);
      for (i=0; i < n; ++i) evec[i][j] /= tmp;
    }
  }
  xfreeall(a, evec, cnt);
  return rc;
}

/* Check options for linalg.eigenval */
static void aux_checkeigenoptions (lua_State *L, int *nargs, int *ortho, int *ev_norm, int *eigenvonly, const char *procname) {
  int checkoptions;
  *ortho = 0;       /* 1 = orthogonal Hessenberg reduction; returns approximations at best, so do not set it to true */
  *ev_norm = 0;     /* 1 = normalise eigenvectors */
  *eigenvonly = 0;  /* 1 = return only eigenvalues, 0 = return both eigenvalues and eigenvectors. */
  if (*nargs > 1 && lua_istrue(L, *nargs)) {
    (*nargs)--; *eigenvonly = 0;
  }
  /* check for options, here `map in-place` */
  checkoptions = 3;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs > 1 && lua_ispair(L, *nargs))  /* 3.15.2 fix */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs > 1 && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streqx(option, "ortho", "orthogonal", NULL)) {
        *ortho = agn_checkboolean(L, -1);
      } else if (tools_streqx(option, "norm", "normalise", NULL)) {
        *ev_norm = agn_checkboolean(L, -1);
      } else if (tools_streqx(option, "evaluesonly", "eigenvals", NULL)) {
        *eigenvonly = agn_checkboolean(L, -1);
      } else if (tools_streq("both", option)) {
        *eigenvonly = !agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
  /* lhf, May 29, 2011 at 12:20 at
     https://stackoverflow.com/questions/6167555/how-can-i-safely-iterate-a-lua-table-while-keys-are-being-removed
     "You can safely remove entries while traversing a table but you cannot create new entries, that is, new keys.
      You can modify the values of existing entries, though. (Removing an entry being a special case of that rule.)" */
}

/* for varying signs of the eigenvectors returned, see:
   https://stats.stackexchange.com/questions/205713/does-the-sign-of-eigenvectors-matter
   Answer: No, it does not. */

/*
A := linalg.matrix([1, 2, 4], [3, 7, 2], [5, 6, 9]);
linalg.eigenval(A):
[-0.89460254283572, 13.747889058727, 4.1467134841089]
*/

static int linalg_eigen (lua_State *L) {
	REAL *mem, *evalr, *evali, *evec, *a;
  int i, m, n, rc, nargs, ortho, ev_norm, eigenvonly;
  m = n = 0;
  nargs = lua_gettop(L);
  aux_checkeigenoptions(L, &nargs, &ortho, &ev_norm, &eigenvonly, "linalg.eigen");
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.eigen", &m, &n);
  agn_poptop(L);  /* remove `dim' field */
  CREATEARRAY(a, n*n + 2*n, "linalg.eigen");
  FILLMATRIX(L, 1, a, n, n, 0, "linalg.eigen");
  for (i=n*n; i < n*n + 2*n; i++) a[i] = 0;
  mem = (REAL *)calloc(n*n + 2*n, sizeof(REAL));
  if (!mem) {
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "linalg.eigen");
  }
  evec = mem;
  evalr = evec + n*n;
  evali = evalr + n;
  rc = aux_eigeng(a, n, 0, ortho, ev_norm, evalr, evali, (eigenvonly) ? NULL : evec);
  if (rc) {
    xfreeall(a, mem);
    luaL_error(L, "Error in " LUA_QS ": code %d.", "linalg.eigen", rc);
  }
  lua_createtable(L, n, 1);
  for (i=0; i < n; i++) {
    agn_pushcomplex(L, evalr[i], evali[i]);
    lua_rawseti(L, -2, i + 1);
  }
  if (!eigenvonly) { CREATEMATRIX(L, evec, n, n); }
  xfreeall(a, mem);
  return 1 + (!eigenvonly);
}


static int linalg_eigenval (lua_State *L) {
	REAL *mem, *evalr, *evali, *evec, *a;
  int i, m, n, rc, nargs, ortho, ev_norm, eigenvonly;
  m = n = 0;
  nargs = lua_gettop(L);
  aux_checkeigenoptions(L, &nargs, &ortho, &ev_norm, &eigenvonly, "linalg.eigenval");
  eigenvonly = 1;
  linalg_auxcheckmatrix(L, 1, 1, 1, "linalg.eigenval", &m, &n);
  agn_poptop(L);  /* remove `dim' field */
  CREATEARRAY(a, n*n + 2*n, "linalg.eigenval");
  FILLMATRIX(L, 1, a, n, n, 0, "linalg.eigenval");
  for (i=n*n; i < n*n + 2*n; i++) a[i] = 0;
  mem = (REAL *)calloc(n*n + 2*n, sizeof(REAL));
  if (!mem) {
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "linalg.eigenval");
  }
  evec = mem;
  evalr = evec + n*n;
  evali = evalr + n;
  rc = aux_eigeng(a, n, 0, ortho, ev_norm, evalr, evali, NULL);
  if (rc) {
    xfreeall(a, mem);
    luaL_error(L, "Error in " LUA_QS ": code %d.", "linalg.eigenval", rc);
  }
  lua_createtable(L, n, 1);
  for (i=0; i < n; i++) {
    agn_pushcomplex(L, evalr[i], evali[i]);
    lua_rawseti(L, -2, i + 1);
  }
  xfreeall(a, mem);
  return 1;
}


/* Checks whether the given vector or matrix contains unassigned components (which default to zero) indicating
   a sparse vector or a matrix with sparse row vectors. The function returns `true` or `false`. 3.17.4 */
static int linalg_issparse (lua_State *L) {
  int i, j, m, n, rc, nonallocated;
  luaL_checkstack(L, 3, "not enough stack space");
  m = n = nonallocated = 0;  /* just to prevent compiler warnings */
  if (agn_istableutype(L, 1, "vector")) {
    m = 1;
    luaL_checkstack(L, 1, "not enough stack space");
    lua_getfield(L, 1, "dim");
    n = agn_checknumber(L, -1);
    agn_poptop(L);  /* pop field `dim' */
    for (j=0; j < n; j++) {
      agn_rawgetinumber(L, 1, j + 1, &rc);
      if (!rc) nonallocated++;
    }
  } else if (agn_istableutype(L, 1, "matrix")) {
    linalg_auxcheckmatrixlight(L, 1, &m, &n, "linalg.issparse");
    for (i=0; i < m; i++) {
      luaL_checkstack(L, 1, "not enough stack space");
      lua_rawgeti(L, 1, i + 1);  /* push row vector on stack */
      checkVector(L, -1, "linalg.issparse");
      for (j=0; j < n; j++) {
        agn_rawgetinumber(L, -1, j + 1, &rc);
        if (!rc) nonallocated++;
      }
      agn_poptop(L);  /* pop row vector */
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected a matrix or vector, got %s.", "linalg.issparse", luaL_typename(L, 1));
  }
  lua_pushboolean(L, nonallocated != 0);
  lua_pushinteger(L, m*n);
  lua_pushinteger(L, nonallocated);
  return 3;
}


static int linalg_sparse (lua_State *L) {
  int i, j, m, n, rc, flag;
  lua_Number x;
  m = n = 0;  /* just to prevent compiler warnings */
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushvalue(L, 1);
  if (agn_istableutype(L, -1, "vector")) {
    lua_getfield(L, -1, "dim");
    n = agn_checknumber(L, -1);
    agn_poptop(L);  /* pop field `dim' */
    for (i=0; i < n; i++) {
      flag = 1;
      x = agn_rawgetinumber(L, -1, i + 1, &rc);
      if (!rc || x == 0.0) {  /* 4.1.0 fix */
        luaL_checkstack(L, 1, "not enough stack space");
        lua_rawgeti(L, -1, i + 1);
        if (lua_iscomplex(L, -1)) {
          lua_Number a, b;
          agn_getcmplxparts(L, -1, &a, &b);
          flag = (a == 0 && b == 0);
        }
        agn_poptop(L);  /* pop whatever */
        if (flag) {
          lua_pushnil(L);
          lua_rawseti(L, -2, i + 1);
        }
      }
    }
  } else if (agn_istableutype(L, -1, "matrix")) {
    linalg_auxcheckmatrixlight(L, -1, &m, &n, "linalg.sparse");
    for (i=0; i < m; i++) {
      lua_rawgeti(L, -1, i + 1);  /* push row vector on stack */
      checkVector(L, -1, "linalg.sparse");
      for (j=0; j < n; j++) {
        flag = 1;
        x = agn_rawgetinumber(L, -1, j + 1, &rc);
        if (!rc || x == 0.0) {  /* 4.1.0 fix */
          luaL_checkstack(L, 1, "not enough stack space");
          lua_rawgeti(L, -1, j + 1);
          if (lua_iscomplex(L, -1)) {
            lua_Number a, b;
            agn_getcmplxparts(L, -1, &a, &b);
            flag = (a == 0 && b == 0);
          }
          agn_poptop(L);  /* pop whatever */
          if (flag) {
            lua_pushnil(L);
            lua_rawseti(L, -2, j + 1);
          }
        }
      }
      agn_poptop(L);  /* pop row vector */
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected a matrix or vector, got %s.", "linalg.sparse", luaL_typename(L, 1));
  }
  return 1;
}


/* # Returns the k-th row vector from matrix A. This is a clone of Maple's linalg[row] function. 3.17.8
linalg.row := proc(A :: matrix, k :: posint) is
   local rowdim := linalg.dim(A, true);
   k > rowdim ? error('Error in `linalg.row`: row does not exist.');
   return A[k]
end; */
static int linalg_row (lua_State *L) {  /* 3.18.7 */
  int k, m, n;
  m = n = 0;  /* just to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.row", &m, &n);
  agn_poptop(L);
  k = agn_checkposint(L, 2);
  if (k > m)
    luaL_error(L, "Error in " LUA_QS ": row %d does not exist.", "linalg.row", k);
  lua_rawgeti(L, 1, k);  /* push row vector on stack */
  checkVector(L, -1, "linalg.row");
  return 1;
}


/* # return the n-th column of a matrix or row vector o as a new vector; 18.12.2008; renamed 3.17.8
linalg.col := proc(o, n :: posint) is  # modified 2.12.1
   local R;
   case typeof(o)
      of 'matrix' then
         return linalg.submatrix(o, n);  # 3.18.5 speed-up
      of 'vector' then
         if n > o.dim then
            error('Error in `linalg.col`: column ' & n & ' does not exist.')
         fi;
         R := linalg.vector(o[n])
      else
         argerror(o, 'linalg.col', 'matrix or vector expected')  # 2.10.0 changed
      esle
   esac;
   return R
end; */
static int linalg_col (lua_State *L) {
  int k, n, rc;
  k = agn_checkposint(L, 2);
  if (agn_istableutype(L, 1, "matrix")) {
    int i, m;
    lua_Number val;
    m = n = 0;  /* just to prevent compiler warnings */
    linalg_auxcheckmatrixlight(L, 1, &m, &n, "linalg.col");  /* reserves three stack slots */
    if (k > n)
      luaL_error(L, "Error in " LUA_QS ": column %d does not exist.", "linalg.col", k);
    lua_createtable(L, m, 1);  /* create new column vector */
    for (i=0; i < m; i++) {
      lua_rawgeti(L, 1, i + 1);  /* push row vector on stack */
      checkVector(L, -1, "linalg.col");
      val = agn_rawgetinumber(L, -1, k, &rc);
      if (!rc) {  /* 4.1.0 extension for complex numbers */
        lua_rawgeti(L, -1, k);
        if (lua_iscomplex(L, -1)) {
          lua_rawseti(L, -3, i + 1);
          agn_poptop(L);  /* pop row vector */
          continue;
        }
        agn_poptoptwo(L);  /* pop whatever and row vector */
        continue;  /* 3.20.4 change to preserve sparseness */
      }
      agn_poptop(L);  /* pop row vector */
      agn_setinumber(L, -1, i + 1, val);
    }
    setvattribs(L, m);
  } else if (agn_istableutype(L, 1, "vector")) {
    lua_getfield(L, 1, "dim");
    n = agn_checknumber(L, -1);
    agn_poptop(L);  /* pop field `dim' */
    if (k > n)
      luaL_error(L, "Error in " LUA_QS ": column %d does not exist.", "linalg.col", k);
    lua_pushnumber(L, agn_getinumber(L, 1, k));
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected a matrix or vector, got %s.", "linalg.col", luaL_typename(L, 1));
  }
  return 1;
}

/* returns the column dimension of a matrix; 27.07.2008; extended 06.09.2008; simplified 18.12.2008
linalg.coldim := proc(A) is
   return right(linalg.checkmatrix(A, true))
end; */

static int linalg_coldim (lua_State *L) {  /* 3.18.7 */
  int m, n;
  m = n = 0;  /* just to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.coldim", &m, &n);
  agn_poptop(L);
  lua_pushinteger(L, n);
  return 1;
}


/* # returns the row dimension of the matrix A; 27.07.2008; extended 06.09.2008; simplified 18.12.2008
linalg.rowdim := proc(A) is
   return left(linalg.checkmatrix(A, true))
end; */

static int linalg_rowdim (lua_State *L) {  /* 3.18.7 */
  int m, n;
  m = n = 0;  /* just to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.rowdim", &m, &n);
  agn_poptop(L);
  lua_pushinteger(L, m);
  return 1;
}


/* Creates a copy of a given matrix or adds rows and columns, optionally in-place. 3.17.4/5 */
static void aux_mcopy (lua_State *L, int addrows, int addcols, lua_Number def, int setattribs, int inplace, int dense, const char *procname) {
  int i, j, k, m, n, rc;
  lua_Number val;
  m = n = 0;  /* just to prevent compiler warnings */
  linalg_auxcheckmatrix(L, 1, 1, 0, procname, &m, &n);
  agn_poptop(L);  /* pop dimensions */
  luaL_checkstack(L, 1, "not enough stack space");  /* space for new matrix; 3.17.5 fix */
  if (inplace)
    lua_pushvalue(L, 1);
  else
    lua_createtable(L, m + addrows, 1);
  for (i=0; i < m; i++) {
    luaL_checkstack(L, 2, "not enough stack space");  /* reserve space for new and existing column vector; 3.17.5 fix */
    if (!inplace) {
      lua_createtable(L, n + addcols, 1);  /* create new row vector (for writing) */
      lua_rawgeti(L, 1, i + 1);  /* push row vector on stack (for reading) */
      checkVector(L, -1, procname);
      for (j=0; j < n; j++) {
        val = agn_rawgetinumber(L, -1, j + 1, &rc);
        if (!rc) {
          luaL_checkstack(L, 1, "not enough stack space");
          lua_rawgeti(L, -1, j + 1);
          if (lua_iscomplex(L, -1)) {
            lua_rawseti(L, -3, j + 1);
          } else {  /* 3.20.4 change to preserve sparseness */
            agn_poptop(L);  /* pop whatever */
            if (dense) {
              lua_pushnumber(L, 0.0);
              lua_rawseti(L, -3, j + 1);
            }
          }
          /* both row vectors are still on stack */
        } else {  /* explicitly set row vector component from input */
          agn_setinumber(L, -2, j + 1, val);
        }
      }
    } else {  /* in-place mode, 3.18.0 fix */
      lua_rawgeti(L, 1, i + 1);  /* push row vector on stack (for writing) */
      checkVector(L, -1, procname);
      j = n;
    }
    /* add further elements to end of extended row vector (linalg.extend only), 3.17.5 */
    for (k=j; tools_isfinite(def) && k < j + addcols; k++) {
      agn_setinumber(L, -2 + inplace, k + 1, def);
    }
    if (!inplace) agn_poptop(L);  /* pop read-only row vector */
    if (setattribs) {  /* 3.17.5 security fix */
      if (!inplace) {
        setvattribs(L, n + addcols);
      } else {  /* avoid crashes */
        setvectordim(L, n + addcols)
      }
    }
    lua_rawseti(L, -2, i + 1);
  }
  for (i=0; i < addrows; i++) {  /* add further row vector(s) (linalg.extend only), 3.17.5 */
    luaL_checkstack(L, 1, "not enough stack space");  /* reserve space for new row vector */
    lua_createtable(L, n + addcols, 1);  /* push new row vector */
    for (j=0; j < n + addcols; j++) {
      if (tools_isfinite(def))
        agn_setinumber(L, -1, j + 1, def);
    }
    if (setattribs) { setvattribs(L, n + addcols); }  /* avoid crashes */
    lua_rawseti(L, -2, m + i + 1);
  }
  if (setattribs) {  /* 3.17.5 security fix */
    if (!inplace) { setmattribs(L, m + addrows, n + addcols); }
    else { setmatrixdims(L, m + addrows, n + addcols); }
  }
}

static int linalg_mcopy (lua_State *L) {
  aux_mcopy(L, 0, 0, AGN_NAN, 1, 0, 0, "linalg.mcopy");
  return 1;
}


/* Creates a new matrix which is a copy of the input matrix with addrows additional rows and addcols additional columns.
   You can also optionally initialise new entries by passing the fourth argument def. The function is a clone of Maple's
   linalg[extend] function. If addrows and addcols are both zero, a deep copy of the input matrix is returned. 3.17.5 */
static int linalg_extend (lua_State *L) {
  int addrows, addcols, inplace, approx, nargs;
  lua_Number def;
  inplace = approx = 0;
  (void)approx;
  nargs = lua_gettop(L);
  addrows = agn_checknonnegint(L, 2);
  addcols = agn_checknonnegint(L, 3);
  def = agnL_optnumber(L, 4, AGN_NAN);
  linalg_aux_fcheckoptions(L, 4, &nargs, &inplace, &approx, "linalg.extend");
  aux_mcopy(L, addrows, addcols, def, 1, inplace, 0, "linalg.extend");
  return 1;
}


/* Creates a deep copy of a given vector. 3.17.4 */
static void aux_vcopy (lua_State *L, int setattribs, int dense, const char *procname) {
  double val;
  int i, n, rc;
  n = checkVector(L, 1, procname);
  luaL_checkstack(L, 1, "not enough stack space");  /* 3.17.5 fix */
  lua_createtable(L, n, 1);
  for (i=0; i < n; i++) {
    val = agn_rawgetinumber(L, 1, i + 1, &rc);
    if (!rc) {  /* 4.1.0 extension for complex numbers */
      luaL_checkstack(L, 1, "not enough stack space");
      lua_rawgeti(L, 1, i + 1);
      if (lua_iscomplex(L, -1)) {
        lua_rawseti(L, -2, i + 1);
        continue;
      } else {  /* ignore whatever and set `null` to zero below if explicit == 1 */
        agn_poptop(L);
      }
    }
    /* explicit = 1 OR rc = 1 (number given) ? -> explicitly set vector component in input, creating a dense vector */
    if (dense || rc) {
      agn_setinumber(L, -1, i + 1, val);
    }
  }
  if (setattribs) {  /* 3.17.5 security fix */
    setvattribs(L, n);
  }
}

static int linalg_vcopy (lua_State *L) {
  aux_vcopy(L, 1, 0, "linalg.vcopy");
  return 1;
}


/* Takes a vector or matrix and converts it to a table with with no metamethods and with all sparse elements in the
   row vector(s) of the input unset, that is not set to zero. The function does not change the input structure. */
static int linalg_totable (lua_State *L) {
  int  nargs, sparse, columnv;
  nargs = lua_gettop(L);
  sparse = columnv = 0;
  aux_checkvmoptions(L, 2, &nargs, &sparse, &columnv, "linalg.totable");  /* 4.1.1 change */
  if (agn_istableutype(L, 1, "vector"))
    aux_vcopy(L, 0, !sparse, "linalg.totable");
  else if (agn_istableutype(L, 1, "matrix"))
    aux_mcopy(L, 0, 0, AGN_NAN, 0, 0, !sparse, "linalg.totable");  /* 4.1.1 extension */
  else
    luaL_error(L, "Error in " LUA_QS ": expected a matrix or vector, got %s.", "linalg.totable", luaL_typename(L, 1));
  return 1;
}


/* Determines the dimension of a matrix or a vector. If A is a matrix, the result is a pair
   with the left-hand side the number of rows and the right-hand side the number of columns.
   If A is a vector, the size of the vector is determined. 18.12.2008, 30.09.2012. C port 3.17.8

linalg.dim := proc(A, option) is
   local p;
   if typeof(A) notin {'matrix', 'vector'} then  # 3.15.3 change
      error('Error in `linalg.dim`: matrix or vector expected.')
   fi;
   p := A.dim;
   if option and p :: 'matrix' then  # 3.17.7 extension, 3.17.8 fix
      return left p, right p
   else
      return p
   fi;
end; */
static int linalg_dim (lua_State *L) {
  int isoption, rows, cols;
  rows = cols = 0;
  isoption = agnL_optboolean(L, 2, 0);
  if (agn_istableutype(L, 1, "vector")) {
    lua_getfield(L, 1, "dim");
    if (!agn_isposint(L, -1)) {
      agn_poptop(L);  /* pop whatever */
      luaL_error(L, "Error in " LUA_QS ": got malformed vector.", "linalg.dim");
    } else {
      cols = agn_tointeger(L, -1);
      agn_poptop(L);  /* pop field */
      lua_pushinteger(L, cols);
      return 1;
    }
  } else if (agn_istableutype(L, 1, "matrix")) {
    luaL_checkstack(L, 2, "not enough stack space");
    lua_getfield(L, 1, "dim");
    if (!lua_ispair(L, -1)) {
      agn_poptop(L);  /* pop whatever */
      luaL_error(L, "Error in " LUA_QS ": got malformed vector.", "linalg.dim");
    } else {
      agn_pairgeti(L, -1, 1);  /* get lhs of dimension pair */
      if (!agn_isposint(L, -1)) {
        agn_poptoptwo(L);  /* pop whatever */
        luaL_error(L, "Error in " LUA_QS ": got malformed matrix.", "linalg.dim");
      }
      if (isoption) rows = agn_tointeger(L, -1);
      agn_poptop(L);
      agn_pairgeti(L, -1, 2);  /* get rhs of dimension pair */
      if (!agn_isposint(L, -1)) {
        agn_poptoptwo(L);  /* pop whatever */
        luaL_error(L, "Error in " LUA_QS ": got malformed matrix.", "linalg.dim");
      }
      if (isoption) cols = agn_tointeger(L, -1);
      lua_pop(L, 1 + isoption);
      if (isoption) {
        lua_pushinteger(L, rows);
        lua_pushinteger(L, cols);
      }
      return 1 + isoption;  /* return either the pair pushed before or row and column dimension */
    }
  } else
    luaL_error(L, "Error in " LUA_QS ": expected a matrix or vector, got %s.", "linalg.dim", luaL_typename(L, 1));
  return 1;
}


static int linalg_countitems (lua_State *L) {  /* 3.20.2 */
  int n, i, j, inplace, approx, nargs;
  size_t c = 0;
  nargs = lua_gettop(L);
  inplace = approx = 0;
  linalg_aux_fcheckoptions(L, 3, &nargs, &inplace, &approx, "linalg.countitems");
  (void)inplace;
  n = checkVector(L, 2, "linalg.countitems");
  switch (lua_type(L, 1)) {
    case LUA_TFUNCTION:
      if (approx)
        luaL_error(L, "Error in " LUA_QS ": cannot use the " LUA_QS " option with functions.", "linalg.countitems", "approx");
      luaL_checkstack(L, nargs, "not enough stack space");
      for (i=0; i < n; i++) {
        lua_pushvalue(L, 1);           /* push function */
        lua_geti(L, 2, i + 1);         /* push vector element */
        for (j=3; j <= nargs; j++)     /* further args ... */
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);     /* call function with nargs-1 argument(s) and one result */
        if (lua_istrue(L, -1)) c++;    /* 5.0.1 fix */
        agn_poptop(L);                 /* pop result */
      }
      break;
    case LUA_TNUMBER:
      luaL_checkstack(L, 1, "not enough stack space");
      for (i=0; i < n; i++) {
        lua_geti(L, 2, i + 1);         /* push vector element */
        if ((approx) ? lua_rawaequal(L, -1, 1) : lua_equal(L, -1, 1)) c++;
        agn_poptop(L);                 /* pop vector element */
      }
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": expected a number or function, got %s.", "linalg.countitems", luaL_typename(L, 1));
  }
  lua_pushinteger(L, c);
  return 1;
}


/* Computes the Kronecker product of m x n matrix A and p x q matrix B and returns an m*p x n*q matrix.
   See also: linalg.dotprod, linalg.outprodmatrix. */
static int linalg_kronprod (lua_State *L) {  /* 4.1.0 */
  int i, j, k, l, m, n, p, q, rows, cols;
  lua_Number *a, *b, *c;
  m = n = p = q = 0;
  linalg_auxcheckmatrix(L, 1, 1, 0, "linalg.kronprod", &m, &n);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  linalg_auxcheckmatrix(L, 2, 1, 0, "linalg.kronprod", &p, &q);
  agn_poptop(L);  /* pop the dimension pair of the matrix */
  la_createarray(L, a, m * n, "linalg.kronprod");
  la_createarray(L, b, p * q, "linalg.kronprod");
  rows = m*p; cols = n*q;
  la_createarray(L, c, rows * cols, "linalg.kronprod");
  /* copy 1st matrix into a */
  if (fillmatrix(L, 1, a, m, n, 0, "linalg.kronprod")) {  /* 7.3.4 fix */
    xfreeall(a, b, c);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.kronprod");
  }
  /* copy 2nd matrix into b */
  if (fillmatrix(L, 2, b, p, q, 0, "linalg.kronprod")) {  /* 7.3.4 fix */
    xfreeall(a, b, c);
    luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "linalg.kronprod");
  }
  for (i=0; i < m; i++) {  /* for each row in A */
    for (j=0; j < n; j++) {  /* for each column in A */
      for (k=0; k < p; k++) {  /* for each row in B */
        for (l=0; l < q; l++) {  /* for each column in B */
          /* multiply each element of matrix A by whole Matrix B */
          c[(i*p + k)*cols + j*q + l] = a[i*n + j] * b[k*q + l];
        }
      }
    }
  }
  luaL_checkstack(L, 1, "not enough stack space");
  creatematrix(L, c, rows, cols, 1);  /* create sparse matrix if possible */
  xfreeall(a, b, c);
  return 1;
}


/* Creates an outer product matrix P from vectors a and b of any dimension, respectively, by multiplying
   each element i in a with each element j in b and setting P[i, j] = a[i]*b[j].

   See also: linalg.dotprod, linalg.kronprod. 4.1.0 */
static int linalg_outprodmatrix (lua_State *L) {
  int i, j, m, n;
  lua_Number x, y;
  m = checkVector(L, 1, "linalg.outprodmatrix");
  n = checkVector(L, 2, "linalg.outprodmatrix");
  luaL_checkstack(L, 1, "not enough stack space");
  createrawmatrix(L, m, n);
  for (i=0; i < m; i++) {
    luaL_checkstack(L, 1, "not enough stack space");
    lua_rawgeti(L, -1, i + 1);  /* push empty row vector */
    x = agn_getinumber(L, 1, i + 1);
    for (j=0; j < n; j++) {
      y = agn_getinumber(L, 2, j + 1);
      agn_setinumber(L, -1, j + 1, x*y);
    }
    agn_poptop(L);  /* pop row vector */
  }
  return 1;
}


/* The function returns an n-dimensional vector in which the i-th entry is one and all other entries are zero.
   By default, the vector is sparse - by passing the optional third argument `false`, all zeros are set physically
   into the vector. See also: linalg.identity, linalg.ones. 4.1.0 */
static int linalg_unitvector (lua_State *L) {
  int i, j, n, sparse, columnv, nargs;
  nargs = lua_gettop(L);
  i = agn_checkposint(L, 1);
  n = agn_checkposint(L, 2);
  aux_checkvmoptions(L, 3, &nargs, &sparse, &columnv, "linalg.unitvector");
  if (i > n)
    luaL_error(L, "Error in " LUA_QS ": %d-th element greater than dimension %d.", "linalg.unitvector", i, n);
  luaL_checkstack(L, 1 + columnv, "not enough stack space");  /* 4.1.1 change */
  if (columnv) {
    createrawmatrix(L, n, 1);
    if (sparse) {
      lua_rawgeti(L, -1, i);  /* push empty row vector */
      agn_setinumber(L, -1, 1, 1);
      agn_poptop(L);  /* pop row vector */
    } else {
      for (j=1; j <= n; j++) {
        lua_rawgeti(L, -1, j);  /* push empty row vector */
        agn_setinumber(L, -1, 1, j == i);
        agn_poptop(L);  /* pop row vector */
      }
    }
  } else {
    lua_createtable(L, n, 1);
    if (sparse) {
      agn_setinumber(L, -1, i, 1);
    } else {
      for (j=1; j <= n; j++) {
        agn_setinumber(L, -1, j, j == i);
      }
    }
    setvattribs(L, n);
  }
  return 1;
}


static const luaL_Reg linalglib[] = {
  {"add", linalg_add},                      /* added on September 04, 2008 */
  {"addcol", linalg_addcol},                /* added on July 06, 2024 */
  {"addrow", linalg_addrow},                /* added on July 06, 2024 */
  {"adjoint", linalg_adjoint},              /* added on July 10, 2024 */
  {"antidiagonal", linalg_antidiagonal},    /* added on July 19, 2024 */
  {"backsub", linalg_backsub},              /* added on March 06, 2014 */
  {"checkmatrix", linalg_checkmatrix},      /* added on September 06, 2008 */
  {"checksquare", linalg_checksquare},      /* added on July 18, 2024 */
  {"checkvector", linalg_checkvector},      /* added on September 06, 2008 */
  {"col", linalg_col},                      /* added on July 20, 2024 */
  {"coldim", linalg_coldim},                /* added on July 20, 2024 */
  {"colvector", linalg_colvector},          /* added on September 07, 2024 */
  {"countitems", linalg_countitems},        /* added on August 14, 2024 */
  {"det", linalg_det},                      /* added on February 19, 2014 */
  {"diagonal", linalg_diagonal},            /* added on February 26, 2014 */
  {"dim", linalg_dim},                      /* added on June 29, 2024 */
  {"dotprod", linalg_dotprod},              /* added on February 14, 2014 */
  {"eigen", linalg_eigen},                  /* added on February 26, 2024 */
  {"eigenval", linalg_eigenval},            /* added on February 26, 2024 */
  {"extend", linalg_extend},                /* added on June 24, 2024 */
  {"forsub", linalg_forsub},                /* added on March 07, 2014 */
  {"gausselim", linalg_gausselim},          /* added on July 06, 2024 */
  {"gaussjord", linalg_gaussjord},          /* added on July 06, 2024 */
  {"getantidiagonal", linalg_getantidiagonal},  /* added on July 19, 2024 */
  {"getdiagonal", linalg_getdiagonal},      /* added on February 26, 2014 */
  {"getvelem", linalg_getvelem},            /* added on May 07, 2024 */
  {"hilbert", linalg_hilbert},              /* added on July 06, 2024 */
  {"identity", linalg_identity},            /* added on September 06, 2008 */
  {"infcolnorm", linalg_infcolnorm},        /* added on July 09, 2024 */
  {"infnorm", linalg_infnorm},              /* added on July 08, 2024 */
  {"inverse", linalg_inverse},              /* added on February 21, 2014 */
  {"isantidiagonal", linalg_isantidiagonal},    /* added on July 19, 2024 */
  {"isantisymmetric", linalg_isantisymmetric},  /* added on July 18, 2024 */
  {"isdiagonal", linalg_isdiagonal},        /* added on July 19, 2024 */
  {"isfractional", linalg_isfractional},    /* added on July 30, 2024 */
  {"isidentity", linalg_isidentity},        /* added on July 19, 2024 */
  {"isintegral", linalg_isintegral},        /* added on July 29, 2024 */
  {"islower", linalg_islower},              /* added on March 02, 2024 */
  {"ismatrix", linalg_ismatrix},            /* added on September 06, 2008 */
  {"isone", linalg_isone},                  /* added on July 19, 2024 */
  {"isref", linalg_isref},                  /* added on August 26, 2024 */
  {"isrref", linalg_isrref},                /* added on August 26, 2024 */
  {"issingular", linalg_issingular},        /* added on July 29, 2024 */
  {"issparse", linalg_issparse},            /* added on June 22, 2024 */
  {"issquare", linalg_issquare},            /* added on July 18, 2024 */
  {"issymmetric", linalg_issymmetric},      /* added on July 19, 2024 */
  {"isupper", linalg_isupper},              /* added on March 02, 2024 */
  {"isvector", linalg_isvector},            /* added on September 06, 2008 */
  {"iszero", linalg_iszero},                /* added on July 19, 2024 */
  {"kronprod", linalg_kronprod},            /* added on September 07, 2024 */
  {"linsolve", linalg_linsolve},            /* added on February 17, 2014 */
  {"ludecomp", linalg_ludecomp},            /* added on August 27, 2024 */
  {"ludoolittle", linalg_ludoolittle},      /* added on February 27, 2014 */
  {"madd", linalg_madd},                    /* added on July 06, 2024 */
  {"maeq", linalg_maeq},                    /* added on February 17, 2014 */
  {"matinfnorm", linalg_matinfnorm},        /* added on July 09, 2024 */
  {"matmat", linalg_matmat},                /* added on July 13, 2024 */
  {"matnnorm", linalg_matnnorm},            /* added on July 09, 2024 */
  {"matonenorm", linalg_matonenorm},        /* added on July 09, 2024 */
  {"mattam", linalg_mattam},                /* added on July 13, 2024 */
  {"mcopy", linalg_mcopy},                  /* added on June 22, 2024 */
  {"meeq", linalg_meeq},                    /* added on February 27, 2014 */
  {"minor", linalg_minor},                  /* added on July 21, 2024 */
  {"mmul", linalg_mmul},                    /* added on February 14, 2014 */
  {"mpow", linalg_mpow},                    /* added on August 26, 2024 */
  {"mscalardiv", linalg_mscalardiv},        /* added on July 12, 2024 */
  {"mscalarmul", linalg_mscalarmul},        /* added on July 06, 2024 */
  {"msub", linalg_msub},                    /* added on July 06, 2024 */
  {"mulrow", linalg_mulrow},                /* added on February 26, 2014 */
  {"mulrowadd", linalg_mulrowadd},          /* added on February 26, 2014 */
  {"ncolnorm", linalg_ncolnorm},            /* added on July 09, 2024 */
  {"newmatrix", linalg_newmatrix},          /* added on June 30, 2024 */
  {"nnorm", linalg_nnorm},                  /* added on July 09, 2024 */
  {"onecolnorm", linalg_onecolnorm},        /* added on July 09, 2024 */
  {"onenorm", linalg_onenorm},              /* added on July 09, 2024 */
  {"ones", linalg_ones},                    /* added on June 22, 2024 */
  {"outprodmatrix", linalg_outprodmatrix},  /* added on September 07, 2024 */
  {"permanent", linalg_permanent},          /* added on August 19, 2024 */
  {"randmatrix", linalg_randmatrix},        /* added on August 20, 2024 */
  {"randvector", linalg_randvector},        /* added on August 22, 2024 */
  {"rank", linalg_rank},                    /* added on July 06, 2024 */
  {"rotcol", linalg_rotcol},                /* added on July 08, 2024 */
  {"rotrow", linalg_rotrow},                /* added on July 08, 2024 */
  {"row", linalg_row},                      /* added on July 20, 2024 */
  {"rowdim", linalg_rowdim},                /* added on July 20, 2024 */
  {"rref", linalg_rref},                    /* added on March 05, 2014 */
  {"scalardiv", linalg_scalardiv},          /* added on March 09, 2026 */
  {"scalarmul", linalg_scalarmul},          /* added on September 04, 2008 */
  {"scale", linalg_scale},                  /* added on July 06, 2024 */
  {"setvelem", linalg_setvelem},            /* added on December 20, 2008 */
  {"sparse", linalg_sparse},                /* added on June 25, 2024 */
  {"sub", linalg_sub},                      /* added on September 04, 2008 */
  {"submatrix", linalg_submatrix},          /* added on March 02, 2014 */
  {"swapcol", linalg_swapcol},              /* added on July 13, 2024 */
  {"swaprow", linalg_swaprow},              /* added on July 13, 2024 */
  {"totable", linalg_totable},              /* added on June 22, 2024 */
  {"trace", linalg_trace},                  /* added on February 26, 2014 */
  {"transpose", linalg_transpose},          /* added on February 21, 2014 */
  {"unitvector", linalg_unitvector},        /* added on September 07, 2024 */
  {"vaeq", linalg_vaeq},                    /* added on February 21, 2014 */
  {"vcopy", linalg_vcopy},                  /* added on June 22, 2024 */
  {"vector", linalg_vector},                /* added on September 04, 2008 */
  {"veeq", linalg_veeq},                    /* added on February 27, 2014 */
  {"viszero", linalg_viszero},              /* added on August 12, 2024 */
  {"vmap", linalg_vmap},                    /* added on December 19, 2008 */
  {"vzero", linalg_vzero},                  /* added on September 06, 2008 */
  {"vzip", linalg_vzip},                    /* added on December 19, 2008 */
  {"zeros", linalg_zeros},                  /* added on June 22, 2024 */
  {NULL, NULL}
};


/*
** Open linalg library
*/
LUALIB_API int luaopen_linalg (lua_State *L) {
  luaL_register(L, AGENA_LINALGLIBNAME, linalglib);
  BASIS = tools_doublebase();
  return 1;
}

