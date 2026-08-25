#ifndef numarray_h
#define numarray_h

#define NAMAXDIMS 8

typedef struct NumArray {
  size_t size;         /* number of 1D-slots */
  union {
    lua_Number *n;     /* pointer to lua_Number values */
    unsigned char *c;  /* pointer to char values */
    int32_t *i;        /* pointer to signed 32-bit integers */
    uint32_t *ui;      /* pointer to unsigned 32-bit integers */
    uint16_t *us;      /* pointer to unsigned 16-bit integers */
    long double *ld;   /* pointer to 80-bit floating-point numbers */
    agn_Complex *z;    /* pointer to agn_Complex values */
    int64_t *i64;      /* pointer to signed 32-bit integers */
    uint64_t *ui64;    /* pointer to unsigned 32-bit integers */
  } data;
  int64_t dims[NAMAXDIMS]; /* the individual dimensions */
  int ndims;           /* total number of dimensions */
  int registry;        /* registry, for attribute information, stored to index 0; 2.15.1 */
  char datatype;       /* is array a double (==1), int32_t (==2), uint16_t (==3), uint32_t (==4), long double (==5), complex double (==6) por uchar (==0) array ? */
  char padding[7];     /* 7 bytes - keeps the next NumArray in an array aligned */
} NumArray;

/* GREP "{"uchar", "double"," in numarray.c */
/*                0         1        2        3         4         5         6         7          8    */
typedef enum { NAUCHAR, NADOUBLE, NAINT32, NAUINT16, NAUINT32, NAUINT64, NAINT64, NALDOUBLE, NACDOUBLE } natype;

#define checknumarray(L,n) (NumArray *)luaL_checkudata(L, n, "numarray")
#define isnumarray(L,n)    (luaL_isudata(L, n, "numarray") && agn_isutypeset(L, n))  /* 4.11.4 extension */

NumArray *numarray_createarray (lua_State *L, natype what, int64_t nops, const char *procname);  /* 4.10.6 */

/* pos starts from zero */
#define numarray_setdouble(L,a,pos,val,procname) { \
  if (pos < 0 || pos >= a->size) \
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", procname, pos + 1); \
  a->data.n[pos] = val; \
}

#define numarray_getdouble(L,a,pos,procname) { \
  lua_Number r; \
  if (pos < 0 || pos >= a->size) \
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", procname, pos + 1); \
  r = a->data.n[pos]); \
  (r); \
}

#define numarray_getsize(L,a) (a->size)
#define numarray_gettype(L,a) (a->datatype)
#define numarray_getdoublearray(L,a) (a->data.n)
#define numarray_isdoublearray(L,a) (a->data.n == NADOUBLE)

#define numarray_freedoublearray(L, a) { \
  xfree(a->data.n); \
}

#define numarray_createdouble(L, a, ii) { \
  a = (NumArray *)lua_newuserdata(L, sizeof(NumArray)); \
  a->size = (ii); \
  a->datatype = NADOUBLE; \
  a->data.n = calloc(ii, sizeof(lua_Number)); \
  if (a->data.n == NULL) \
    luaL_error(L, "Error: memory allocation failed."); \
  lua_getmetatable(L, 1); \
  lua_setmetatable(L, -2); \
  agn_setutypestring(L, -1, "double"); \
  lua_createtable(L, 0, 0); \
  a->registry = luaL_ref(L, LUA_REGISTRYINDEX); \
}

#endif

