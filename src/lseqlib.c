/*
** $Id: lseqlib.c,v 1.38 2005/10/23 17:38:15 roberto Exp $
** Library for Sequence Manipulation
** See Copyright Notice in agena.h
*/


#include <stddef.h>

#define lseqlib_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"   /* for LUA_OPEQ */
#include "agnxlib.h"
#include "linalg.h"
#include "lobject.h"   /* for agnO_aligntowordboundary */
#include "lopcodes.h"  /* for OP_EQ */
#include "ltablib.h"

/* creates a sequence with values a, a+step ..., b-step, b; based on calc.fseq
   uses a modified version of Kahan's summation algorithm to avoid roundoff errors
   0.30.0, December 30, 2009; merged with calc.fseq 1.3.3, February 01, 2011

   calc.fseq comments: creates a sequence with values f(a), ..., f(b); June 30, 2008;
   20 % faster than an Agena implementation; extended August 20, 2009, 0.27.0
   extended 0.30.0, December 29, 2009 with Kahan summation algorithm to avoid roundoff errors;
   extended 1.8.8, November 07, 2012, to also return non-numeric sequences; extended 2.4.1,
   January 15, 2015, to process the number of elements to be returned.

   For the idea on the Matlab version please see:
   http://de.mathworks.com/help/matlab/ref/linspace.html
   http://www.che.utah.edu/~sutherland/wiki/index.php/Matlab_Arrays#Linspace_and_Logspace */

static int seq_new (lua_State *L) {
  /* use `volatile` so that the compiler does not render the Kahan code effectless */
  /* volatile lua_Number idx, step, c, y, t; formerly nseq, 2.28.1; created October 15, 2014 */
  int isfunc, isint, isdefault;
  size_t counter, i, nargs, total, offset;
  lua_Number a, b, eps;
  volatile lua_Number s, c, cs, ccs, cc, t, idx, step, x;
  luaL_aux_nstructure(L, "sequences.new", &nargs, &offset, &a, &b, &step, &eps, &total, &isfunc, &isdefault, &isint);
  luaL_checkstack(L, 1 + isdefault, "not enough stack space");  /* 3.15.4 fix */
  agn_createseq(L, total);
  counter = 0;
  cs = ccs = 0;
  s = idx = a;
  if (isdefault) {  /* create a sequence of n slots and fill it with one and the same default of any type */
    agn_pairgeti(L, 2, 2);
  }
  /* total > counter: prevents that the last element is inserted even if a roundoff error occurred. */
  if (isfunc) {  /* function passed ? */
    int slots = 2 + (nargs >= 4 + offset)*(nargs - 4 - (int)offset + 1);
    while (idx <= b || tools_approx(idx, b, eps)) {
      luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.5/3.15.4 fix */
      lua_pushvalue(L, offset);  /* push function */
      lua_pushnumber(L, (fabs(idx) < AGN_HEPSILON) ? 0 : idx);  /* quite dirty hack to avoid roundoff errors with 0 */
      for (i=4 + offset; i <= nargs; i++) lua_pushvalue(L, i);
      lua_call(L, slots - 1, 1);
      lua_seqrawseti(L, -2, ++counter);
      if (isint) {
        idx += step;
      } else {
        x = step;  /* Kahan-Babuska, 2.12.1 */
        t = s + x;
        c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
        s = t;
        t = cs + c;
        cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
        cs = t;
        ccs += cc;
        idx = s + cs + ccs;
      }
    }
  } else {
    while (idx <= b || tools_approx(idx, b, eps)) {
      if (isdefault) {  /* fill with default value  */
        lua_pushvalue(L, -1);
        lua_seqrawseti(L, -3, idx);
      } else
        agn_seqsetinumber(L, -1, ++counter, (fabs(idx) < AGN_HEPSILON) ? 0 : idx);
      if (isint) {  /* 2.12.2 */
        idx += step;
      } else {
        x = step;  /* Kahan-Babuska */
        t = s + x;
        c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
        s = t;
        t = cs + c;
        cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
        cs = t;
        ccs += cc;
        idx = s + cs + ccs;
      }
    }
  }
  if (isdefault) agn_poptop(L);
  return 1;
}


/* In sequence s, the function replaces every occurrence of the value old with value new, in-place, and
   returns the number of substitutions done. Note that if new is `null`, the function automatically
   closes the space by shifting down the other elements as sequences cannot store `null`.
   The function is four three faster than `subs`, avoiding most of all checks and conversions that
   `subs` is performing. 5.1.3 */
static int seq_subs (lua_State *L) {  /* 5.1.3 */
  luaL_checktype(L, 1, LUA_TSEQ);
  luaL_checkany(L, 3);
  lua_pushinteger(L, agn_seqsubs(L, 1, 2, 3));
  return 1;
}


/* Resizes a sequence to the given number of pre-allocated slots. If you actually shrink a sequence, then it discards any surplus elements.
   The function returns the number of allocated elements and the number of pre-allocated slots, which may be vacant.

   If newsize is 0, then the function deletes all values from the sequence.

   If the optional third argument is `true` and newsize is non-zero, then the function sets the optimum number of pre-allocated slots to the
   smallest power of 2 greater than or equal to newsize.

   Note that Agena automatically enlarges and shrinks a sequence if necessary when adding new or purging existing values, see environ.kernel/
   seqautoshrink.

   See also: math.nextpower, size, environ.attrib maxsize and size values. */
static int seq_resize (lua_State *L) {  /* 2.29.0 */
  int newsize, optimal;
  size_t size, a[4];
  luaL_argcheck(L, lua_type(L, 1) == LUA_TSEQ, 1, "argument is not a sequence");
  agn_seqstate(L, 1, a);
  if (lua_gettop(L) == 1) {  /* enlarge to the optimum number of pre-allocated slots */
    agn_seqresize(L, 1, agnO_aligntowordboundary(a[0]));  /* 4.2.2 change */
  } else {
    newsize = agn_checknonnegint(L, 2);
    optimal = agnL_optboolean(L, 3, 0);
    size = a[0];
    if (optimal) {
      newsize = (newsize != 0) ? agnO_aligntowordboundary(newsize) : 0;  /* 4.2.2 change */
    }
    agn_seqresize(L, 1, newsize);
    /* agn_seqresize never actually shrinks a sequence to zero to avoid crashes, so now purge the last element if requested. */
    if (size > 0 && newsize == 0) {
      lua_pushnil(L);
      lua_seqrawseti(L, 1, 1);
    }
  }
  agn_seqstate(L, 1, a);
  lua_pushinteger(L, a[0]);  /* t->size */
  lua_pushinteger(L, a[1]);  /* t->maxsize */
  return 2;
}


static int seq_numunion (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TSEQ, 1, "argument is not a sequence");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TSEQ, 2, "argument is not a sequence");
  lua_pushinteger(L, agn_numunion(L, 1, 2));
  return 1;
}


static int seq_numintersect (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TSEQ, 1, "argument is not a sequence");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TSEQ, 2, "argument is not a sequence");
  lua_pushinteger(L, agn_numintersect(L, 1, 2));
  return 1;
}


static int seq_numminus (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TSEQ, 1, "argument is not a sequence");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TSEQ, 2, "argument is not a sequence");
  lua_pushinteger(L, agn_numminus(L, 1, 2));
  return 1;
}


static int seq_isall (lua_State *L) {  /* 3.10.2 */
  agn_seqisall(L, 1, "sequences.isall");
  return 1;
}


/* Creates a sequence of n subsequences, each with narray preallocated slots. You can fill the subsequences
   with a default, narray times, by passing the `init=default` option. 3.17.8 */
static int seq_seqofseqs (lua_State *L) {
  int i, j, narray, nops, isdefault;
  luaL_checkstack(L, 2, "not enough stack space");
  nops = agn_checknonnegint(L, 1);
  narray = agn_checknonnegint(L, 2);  /* 3.18.1 fix */
  isdefault = lua_ispair(L, 3);
  agn_createseq(L, nops);
  if (isdefault) {  /* create a sequence of n slots and fill it with one and the same default of any type */
    agn_pairgeti(L, 3, 2);
  }
  for (i=0; i < nops; i++) {
    agn_createseq(L, narray);
    if (isdefault) {
      for (j=0; j < narray; j++) {
        lua_pushvalue(L, -2);
        lua_seqrawseti(L, -2, j + 1);
      }
    }
    lua_seqrawseti(L, -2 - isdefault, i + 1);
  }
  if (isdefault) agn_poptop(L);
  return 1;
}


/* Checks sequence A for subsequences of the same size. Returns both the number of rows and the number of columns found
   if all subsequences have the same size or issues an error otherwise. 3.17.8 */
static int aux_getdim (lua_State *L, int idx, int *rows, int *cols) {
  int i, c, rc;
  luaL_checktype(L, idx, LUA_TSEQ);
  *cols = c = 0;
  *rows = agn_seqsize(L, idx);
  rc = 0;
  for (i=0; i < *rows; i++) {
    lua_seqrawgeti(L, idx, i + 1);  /* push item */
    if (lua_isseq(L, -1)) {  /* a row */
      *cols = agn_seqsize(L, -1);
      if (i == 0) c = *cols;
      rc = (c != *cols);
    } else {  /* not a row */
      rc = 1;
    }
    agn_poptop(L);
    if (rc) break;
  }
  return rc;
}

static int seq_getdim (lua_State *L) {  /* 3.17.8 */
  int rows, cols;
  if (aux_getdim(L, 1, &rows, &cols)) {
    luaL_error(L, "Error in " LUA_QS ": sequence is not 2-dimensional.", "sequences.getdim");
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushinteger(L, rows);
  lua_pushinteger(L, cols);
  return 2;
}


/* Checks whether sequence A has the same number of rows as there are elements in each of its subsequences (`columns`).
   The subsequences must be of the same size. Returns `true` or `false` and in case of `true`, also returns the number
   of rows and the number of columns found. 3.17.8 */
static int seq_issquare (lua_State *L) {
  int rows, cols, rc, nrets;
  rc = aux_getdim(L, 1, &rows, &cols);
  nrets = 1 + 2*(rc == 0);
  luaL_checkstack(L, nrets, "not enough stack space");
  lua_pushboolean(L, rc == 0 && rows == cols);
  if (!rc) {
    lua_pushinteger(L, rows);
    lua_pushinteger(L, cols);
  }
  return nrets;
}


/* Checks sequence A for subsequences of the same size. Returns `true` or `false` and in case of `true`, also returns
   the number of rows and the number of columns found. With square sequences, also returns `true`. 3.17.8 */
static int seq_isrectangular (lua_State *L) {
  int rows, cols, rc, nrets;
  rc = aux_getdim(L, 1, &rows, &cols);
  nrets = 1 + 2*(rc == 0);
  luaL_checkstack(L, nrets, "not enough stack space");
  lua_pushboolean(L, rc == 0);
  if (!rc) {
    lua_pushinteger(L, rows);
    lua_pushinteger(L, cols);
  }
  return nrets;
}


/* Based on linalg.extend, 3.17.9 */
static void aux_extend (lua_State *L, int addrows, int addcols, int inplace, int idxdef, const char *procname) {
  int i, j, k, m, n;
  if (addrows == 0 && addcols == 0) return;
  m = n = 0;  /* just to prevent compiler warnings */
  if (aux_getdim(L, 1, &m, &n))
    luaL_error(L, "Error in " LUA_QS ": got malformed sequence.", procname);
  luaL_checkstack(L, 1, "not enough stack space");
  if (idxdef) {
    lua_pushvalue(L, idxdef);
    if (!lua_ispair(L, -1))  /* better be sure than sorry */
      luaL_error(L, "Error in " LUA_QS ": stack corruption with default value." LUA_QS ".", procname);
    agn_pairgeti(L, -1, 2);
    lua_replace(L, idxdef);
    agn_poptop(L);
  }
  if (inplace)
    lua_pushvalue(L, 1);
  else
    agn_createseq(L, m + addrows);
  for (i=0; i < m; i++) {
    luaL_checkstack(L, 2 + (inplace == 0), "not enough stack space");  /* reserve space for new and existing column vector */
    if (!inplace) {
      agn_createseq(L, n + addcols);  /* create new row vector */
      lua_seqrawgeti(L, 1, i + 1);  /* push row vector on stack (for reading) */
      luaL_checktype(L, -1, LUA_TSEQ);
      for (j=0; j < n; j++) {
        lua_seqrawgeti(L, -1, j + 1);
        lua_seqrawseti(L, -3, j + 1);
      }
    } else {
      lua_seqrawgeti(L, 1, i + 1);  /* push row vector on stack (for writing) */
      luaL_checktype(L, -1, LUA_TSEQ);
      j = n;
    }
    /* add further elements to end of extended row vector (linalg.extend only) */
    for (k=j; idxdef && k < j + addcols; k++) {
      lua_pushvalue(L, idxdef);
      lua_seqrawseti(L, -2 - !inplace, k + 1);
    }
    if (!inplace) agn_poptop(L);  /* pop read-only row vector */
    lua_seqrawseti(L, -2, i + 1);  /* set row vector into resulting table */
  }
  for (i=0; i < addrows; i++) {  /* add further row vector(s) */
    /* we still have enough reserved stack space */
    agn_createseq(L, n + addcols);  /* push new row vector */
    for (j=0; idxdef && j < n + addcols; j++) {
      lua_pushvalue(L, idxdef);
      lua_seqrawseti(L, -2, j + 1);
    }
    lua_seqrawseti(L, -2, m + i + 1);  /* drop new row vector */
  }
  /* resulting table is on the stack top */
}

/* Creates a new 2-dimension sequence which is a copy of the input 2D sequence with addrows additional rows and addcols
   additional columns. You can also optionally initialise new entries by passing the 'init = <def>' option where `def`
   may be of any type. Also, the 'inplace = true'  option allows to work in-place, altering the input sequence and
   saving memory. If addrows and addcols are both zero, a deep copy of the input sequence is returned.
   3.18.1, based on linalg.extend. */
static int seq_extend (lua_State *L) {
  int addrows, addcols, inplace, init, sparse, nargs;
  inplace = init = sparse = 0;
  nargs = lua_gettop(L);
  addrows = agn_checknonnegint(L, 2);
  addcols = agn_checknonnegint(L, 3);
  tables_checkoptions(L, 4, &nargs, &inplace, &init, &sparse, "sequences.extend");
  aux_extend(L, addrows, addcols, inplace, init, "sequences.extend");
  return 1;
}


static int seq_move (lua_State *L) {  /* based on tbl_move, 3.18.1 */
  lua_Integer f = luaL_checkinteger(L, 2);  /* source start position */
  lua_Integer e = luaL_checkinteger(L, 3);  /* source end position */
  lua_Integer t = agn_checkposint(L, 4);    /* destination start position, 6.5.4 fix */
  int tt = !lua_isnoneornil(L, 5) ? 5 : 1;  /* index of destination structure */
  luaL_checktype(L, 1, LUA_TSEQ);
  luaL_checktype(L, tt, LUA_TSEQ);
  luaL_argcheck(L, agn_seqsize(L, tt) + 1 >= t, 4, "destination wrap around");
  if (e >= f) {  /* otherwise, nothing to move */
    lua_Integer n, i;
    luaL_argcheck(L, f > 0 || e < LUA_MAXINTEGER + f, 3, "too many elements to move");
    n = e - f + 1;  /* number of elements to move */
    luaL_argcheck(L, t <= LUA_MAXINTEGER - n + 1, 4, "destination wrap around");
    /* if APPEND: source end pos < dest start pos ||
          SHIFT LEFT: source start pos >= dest start pos ||
          source structure different from destination structure */
    if (t > e || t <= f || (tt != 1 && !lua_compare(L, 1, tt, LUA_OPEQ))) {
      for (i=0; i < n; i++) {  /* from left to right, optionally using metamethods */
        lua_seqrawgeti(L, 1, f + i);
        lua_seqrawseti(L, tt, t + i);
      }
    } else {  /* SHIFT RIGHT in same structure */
      for (i=n - 1; i >= 0; i--) {  /* from right to left, optionally using metamethods */
        lua_seqrawgeti(L, 1, f + i);
        lua_seqrawseti(L, tt, t + i);
      }
    }
  }
  lua_pushvalue(L, tt);  /* return destination sequence */
  return 1;
}


/* create new 2-dimensional sequence with the elements of C `matrix` a, based on linalg/creatematrix, 3.18.9 */
static int creatematrix (lua_State *L, lua_Number *a, int m, int n, int sparse) {
  int i, j;
  lua_Number item;
  luaL_checkstack(L, 2, "not enough stack space");
  agn_createseq(L, m);
  for (i=0; i < m; i++) {
    agn_createseq(L, n);  /* create new vector */
    for (j=0; j < n; j++) {
      item = a[i*n + j];
      if (!sparse || (sparse && item != 0))
        agn_seqsetinumber(L, -1, j + 1, item);  /* create sparse matrix if requested */
    }
    lua_seqrawseti(L, -2, i + 1);
  }
  return 1;
}

static FORCE_INLINE int checkvector (lua_State *L, int idx, const char *procname) {
  if (!(lua_isseq(L, idx)))
    luaL_error(L, "Error in " LUA_QS ": sequence expected, got %s.", procname, luaL_typename(L, idx));
  return agn_seqsize(L, idx);
}

static void fillmatrix (lua_State *L, int idx, lua_Number *a, int m, int n, const char *procname) {
  int i, j;
  for (i=0; i < m; i++) {
    lua_seqrawgeti(L, idx, i + 1);  /* push row vector on stack */
    if (checkvector(L, -1, procname) != n)
      luaL_error(L, "Error in " LUA_QS ": row sequence has wrong dimension.", procname);
    for (j=0; j < n; j++)
      a[i*n + j] = agn_seqgetinumber(L, -1, j + 1);  /* with non-numbers, sets zero */
    agn_poptop(L);  /* pop row vector */
  }
}

static int FORCE_INLINE aux_checkrectangular (lua_State *L, int idx, int from,
  int *m, int *n, int *inplace, int *init, int *sparse, const char *procname) {
  int nargs;
  nargs = lua_gettop(L);
  *m = *n = *inplace = *init = *sparse = 0;
  if (aux_getdim(L, idx, m, n))
    luaL_error(L, "Error in " LUA_QS ": sequence is not two-dimensional.", procname);
  tables_checkoptions(L, from, &nargs, inplace, init, sparse, procname);
  return nargs;
}

static int seq_transpose (lua_State *L) {  /* 3.18.9 */
  lua_Number *a;
  int m, n, inplace, init, sparse;
  m = n = inplace = init = sparse = 0;
  aux_checkrectangular(L, 1, 2, &m, &n, &inplace, &init, &sparse, "sequences.transpose");
  la_createarray(L, a, m*n, "sequences.transpose");
  fillmatrix(L, 1, a, m, n, "sequences.transpose");
  if (m == n) {
    la_transpose(a, n);
    creatematrix(L, a, n, n, 0);
  } else {
    lua_Number *b;
    int i, j, c;
    la_createarray(L, b, m*n, "sequences.transpose");
    c = 0;
    for (j=0; j < n; j++) {
      for (i=0; i < m; i++) {
        b[c++] = a[i*n + j];
      }
    }
    creatematrix(L, b, n, m, 0);
    xfree(b);
  }
  xfree(a);
  return 1;
}


/* By default, swaps row p in matrix A with row q. p, q must be positive integers. The result is a new matrix.

   If the very last argument is the Boolean `true`, then the operation is done in-place, modifying A. In this
   mode, the modified matrix A is returned, as well.

   Whether in-place or not, you may limit the exchange of the matrix elements to columns s to t by passing the
   respective index range s:t as an optional fourth argument. Bases on linalg_swaprow, 3.18.9 */
static int seq_swaprow (lua_State *L) {
  int i, j, l, u, m, n, nargs, inplace, init, sparse;
  luaL_checkany(L, 3);
  nargs = lua_gettop(L);
  i = agn_checkposint(L, 2);
  j = agn_checkposint(L, 3);
  m = n = inplace = init = sparse = 0;
  nargs = aux_checkrectangular(L, 1, 4, &m, &n, &inplace, &init, &sparse, "sequences.swaprow");
  if (i > m)
    luaL_error(L, "Error in " LUA_QS ": row index %d out of range.", "sequences.swaprow", i);
  if (j > m)
    luaL_error(L, "Error in " LUA_QS ": row index %d out of range.", "sequences.swaprow", j);
  l = 1; u = n;
  if (lua_istrue(L, nargs)) {
    inplace = 1;
    nargs--;
  }
  la_getrange(L, 4, nargs, &l, &u, n, "sequences.swaprow");
  if (inplace) {
    luaL_checkstack(L, 2, "not enough stack space");
    if (l == 1 && u == n) {
      lua_seqrawgeti(L, 1, i);  /* push i-th row vector in matrix at idx */
      checkvector(L, -1, "sequences.swaprow");
      lua_seqrawgeti(L, 1, j);  /* push j-th row vector in matrix at idx */
      checkvector(L, -1, "sequences.swaprow");
      lua_seqrawseti(L, 1, i);  /* set j-th row to index i */
      lua_seqrawseti(L, 1, j);  /* set i-th row to index j */
    } else {
      lua_Number x, y;
      lua_seqrawgeti(L, 1, i);  /* push i-th row vector in matrix at idx */
      checkvector(L, -1, "sequences.swaprow");
      lua_seqrawgeti(L, 1, j);  /* push j-th row vector in matrix at idx */
      checkvector(L, -1, "sequences.swaprow");
      for (; l <= u; l++) {
        x = agn_seqgetinumber(L, -2, l);
        y = agn_seqgetinumber(L, -1, l);
        agn_seqsetinumber(L, -2, l, y);
        agn_seqsetinumber(L, -1, l, x);
      }
      lua_seqrawseti(L, 1, j);
      lua_seqrawseti(L, 1, i);
    }
    lua_settop(L, 1);  /* return modified matrix */
  } else {
    lua_Number *a;
    la_createarray(L, a, m*n, "sequences.swaprow");
    fillmatrix(L, 1, a, m, n, "sequences.swaprow");
    numal_ichrow(L, a, n,  l - 1, u - 1, i - 1, j - 1);
    creatematrix(L, a, m, n, 0);
    xfree(a);
  }
  return 1;
}


/* based on linalg.swapcol, 3.18.9 */
static int seq_swapcol (lua_State *L) {
  int i, j, l, u, m, n, nargs, inplace, init, sparse;
  luaL_checkany(L, 3);
  nargs = lua_gettop(L);
  m = n = inplace = init = sparse = 0;
  i = agn_checkposint(L, 2);
  j = agn_checkposint(L, 3);
  nargs = aux_checkrectangular(L, 1, 4, &m, &n, &inplace, &init, &sparse, "sequences.swapcol");
  if (i > n)
    luaL_error(L, "Error in " LUA_QS ": column index %d out of range.", "sequences.swapcol", i);
  if (j > n)
    luaL_error(L, "Error in " LUA_QS ": column index %d out of range.", "sequences.swapcol", j);
  l = 1; u = m;
  if (lua_istrue(L, nargs)) {
    inplace = 1;
    nargs--;
  }
  la_getrange(L, 4, nargs, &l, &u, m, "sequences.swapcol");
  if (inplace) {
    lua_Number x, y;
    luaL_checkstack(L, 2, "not enough stack space");
    for (; l <= u; l++) {
      lua_seqrawgeti(L, 1, l);  /* push l-th row vector in matrix at idx */
      checkvector(L, -1, "sequences.swapcol");
      x = agn_seqgetinumber(L, -1, i);
      y = agn_seqgetinumber(L, -1, j);
      agn_seqsetinumber(L, -1, i, y);
      agn_seqsetinumber(L, -1, j, x);
      lua_seqrawseti(L, 1, l);
    }
    lua_settop(L, 1);
  } else {
    lua_Number *a;
    la_createarray(L, a, m*n, "sequences.swapcol");
    fillmatrix(L, 1, a, m, n, "sequences.swapcol");
    numal_ichcol(L, a, n,  l - 1, u - 1, i - 1, j - 1);
    creatematrix(L, a, m, n, 0);
    xfree(a);
  }
  return 1;
}


static int seq_col (lua_State *L) {  /* 7.3.7 */
  int i, j, c, l, u, m, n, idx, nargs, ncols;
  luaL_checkany(L, 2);
  nargs = lua_gettop(L);
  m = n = ncols = 0;
  if (aux_getdim(L, 1, &m, &n))  /* checks input structure for correct type, too */
    luaL_error(L, "Error in " LUA_QS ": sequence is not two-dimensional.", "sequences.col");
  for (i=2; i <= nargs && agn_isinteger(L, i); i++, ncols++) {
    idx = tools_posrelat(agn_tointeger(L, i), n);
    if (idx < 1 || idx > n)
      luaL_error(L, "Error in " LUA_QS ": column %d does not exist.", "sequences.col", agn_tointeger(L, i));
  }
  if (ncols == 0)
    luaL_error(L, "Error in " LUA_QS ": need at least one index.", "sequences.col");
  l = 1; u = m;
  la_getrange(L, 2 + ncols, nargs, &l, &u, m, "sequences.col");
  c = 0;
  luaL_checkstack(L, 3 + (ncols != 1), "not enough stack space");
  agn_createseq(L, u - l + 1);
  if (ncols == 1) {
    for (; l <= u; l++) {
      lua_seqrawgeti(L, 1, l);  /* push l-th row vector in matrix at idx */
      checkvector(L, -1, "sequences.col");
      lua_seqrawgeti(L, -1, tools_posrelat(agn_tointeger(L, 2), n));
      lua_seqrawseti(L, -3, ++c);
      agn_poptop(L);
    }
  } else {
    for (i=l; i <= u; i++) {   /* for each row vector i in range */
      agn_createseq(L, ncols);
      lua_seqrawgeti(L, 1, i);  /* push l-th row vector in matrix at idx */
      checkvector(L, -1, "sequences.col");
      for (j=2; j < 2 + ncols; j++) {  /* for each column j */
        lua_seqrawgeti(L, -1, tools_posrelat(agn_tointeger(L, j), n));  /* get value at column j */
        lua_seqrawseti(L, -3, j - 1);  /* set it into substructure */
      }
      agn_poptop(L);  /* drop row vector, result substructure is on top */
      lua_seqrawseti(L, -2, ++c);
    }
  }
  return 1;
}


/* Taken from Lua 5.4.6, Agena 3.18.9

   sequences.concat (s [, sep [, i [, j]]])

   Given a sequence where all elements are strings or numbers, returns the string s[i] & sep & s[i+1] ··· sep & s[j].
   The default value for sep is the empty string, the default for i is 1, and the default for j is size(s). If i is greater
   than j, returns the empty string. */

static void addfield (lua_State *L, luaL_Buffer *b, lua_Integer i) {
  lua_seqrawgeti(L, 1, i);
  if (l_unlikely(!lua_isstring(L, -1)))
    luaL_error(L, "invalid value (%s) at index %I in sequence for 'concat'",
                  luaL_typename(L, -1), (LUAI_UACINT)i);
  luaL_addvalue(b);
}

static int seq_concat (lua_State *L) {
  luaL_Buffer b;
  luaL_checktype(L, 1, LUA_TSEQ);  /* 3.19.1 fix */
  lua_Integer last = agn_seqsize(L, 1);
  size_t lsep;
  const char *sep = luaL_optlstring(L, 2, "", &lsep);
  lua_Integer i = luaL_optinteger(L, 3, 1);
  last = luaL_optinteger(L, 4, last);
  luaL_buffinit(L, &b);
  for (; i < last; i++) {
    addfield(L, &b, i);
    luaL_addlstring(&b, sep, lsep);
  }
  if (i == last)  /* add last value (if interval was not empty) */
    addfield(L, &b, i);
  luaL_pushresult(&b);
  return 1;
}


/* Checks whether sequence s consists of zeros only and returns `true` or `false`. It returns the index of the
   first non-zero element in s as a second result, or 0 if there is none. The non-zero element is returned as a third
   result, too.

   The function considers any non-numeric values in s as non-zero. An empty sequence is non-zero, too.

   By default, the check is done against strict zero. You can change this by passing a positive epsilon value as an
   optional second argument so that all elements x with |x| <= epsilon will be considered zero.

   See also: `tables.iszero`. 3.20.1 */
static int seq_iszero (lua_State *L) {
  int i, n, r, rc;
  lua_Number x, eps;
  luaL_checktype(L, 1, LUA_TSEQ);
  eps = agnL_optnonnegative(L, 2, 0.0);
  r = 1; x = AGN_NAN;
  n = agn_seqsize(L, 1);
  for (i=1; i <= n && r; i++) {  /* from left to right ! */
    x = agn_seqrawgetinumber(L, 1, i, &rc);
    r = rc && (fabs(x) <= eps);
  }
  rc = r*(n != 0);
  lua_pushboolean(L, rc);
  lua_pushinteger(L, (!r)*(i - 1));
  if (!rc) lua_pushnumber(L, x);
  return 2 + !rc;
}


static int seq_isone (lua_State *L) {  /* 6.5.4 */
  int i, n, r, rc;
  lua_Number x, eps;
  luaL_checktype(L, 1, LUA_TSEQ);
  eps = agnL_optnonnegative(L, 2, 0.0);
  r = 1; x = AGN_NAN;
  n = agn_seqsize(L, 1);
  for (i=1; i <= n && r; i++) {  /* from left to right ! */
    x = agn_seqrawgetinumber(L, 1, i, &rc);
    r = rc && (fabs(x - 1.0) <= eps);
  }
  rc = r*(n != 0);
  lua_pushboolean(L, rc);
  lua_pushinteger(L, (!r)*(i - 1));
  if (!rc) lua_pushnumber(L, x);
  return 2 + !rc;
}


/* }====================================================== */

static const luaL_Reg seq_funcs[] = {
  {"col", seq_col},                      /* added 7.3.7, April 05, 2026 */
  {"concat", seq_concat},                /* added 3.18.9, July 28, 2024 */
  {"extend", seq_extend},                /* added 3.18.1, July 01, 2024 */
  {"getdim", seq_getdim},                /* added 3.17.8, June 29, 2024 */
  {"isall", seq_isall},                  /* added February 04, 2024 */
  {"isone", seq_isone},                  /* added 6.5.4 December 19, 2025 */
  {"isrectangular", seq_isrectangular},  /* added 3.17.8, June 29, 2024 */
  {"issquare", seq_issquare},            /* added 3.17.8, June 29, 2024 */
  {"iszero", seq_iszero},                /* added 3.20.1, August 12, 2024 */
  {"move", seq_move},                    /* added 3.18.1, July 01, 2024 */
  {"new", seq_new},                      /* added May 30, 2022 */
  {"numunion", seq_numunion},            /* added January 26, 2024 */
  {"numintersect", seq_numintersect},    /* added January 26, 2024 */
  {"numminus", seq_numminus},            /* added January 26, 2024 */
  {"resize", seq_resize},                /* added June 24, 2022 */
  {"seqofseqs", seq_seqofseqs},          /* added 3.17.8, June 29, 2024 */
  {"subs", seq_subs},                    /* added July 11, 2025 */
  {"swapcol", seq_swapcol},              /* added 3.18.9, July 28, 2024 */
  {"swaprow", seq_swaprow},              /* added 3.18.9, July 28, 2024 */
  {"transpose", seq_transpose},          /* added 3.18.9, July 28, 2024 */
  {NULL, NULL}
};


LUALIB_API int luaopen_sequences (lua_State *L) {
  luaL_register(L, AGENA_SEQLIBNAME, seq_funcs);
  return 1;
}

