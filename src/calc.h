#ifndef calc_h
#define calc_h

typedef struct DoubleArray {  /* 4.12.0a */
  int size;
  double *v;
} DoubleArray;

typedef struct LongDoubleArray {  /* 2.34.10 */
  int size;
  long double *v;
} LongDoubleArray;

/* just to avoid confusion about data types, we set up a pair of defines; 7.3.9 */
#define getdblarray(x, n)     ((x)->v[(n)])
#define getldblarray          getdblarray
#define setdblarray(x, n, y)  (x)->v[(n)] = (y)
#define setldblarray          setdblarray
#endif