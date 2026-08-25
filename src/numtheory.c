/*
** $Id: numtheory.c, v 1.0.0 by Alexander Walz - initiated April 06, 2025
** Number Theory library
** See Copyright Notice in agena.h
*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define numtheory_c
#define LUA_LIB

#include "numtheory.h"

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnconf.h"
#include "agnhlps.h"
#include "agnxlib.h"

/* Checks if a given integer n is a perfect square, i.e. if sqrt(n)^2 = n. Any power of two is a perfect square, for example
   n = 1, 4, 9, 16, 25, 36, 49, etc.  See also: `sqrt`, `square`, `math.ispow2`. INLINE function is not faster, 2.16.13 */
#define issquare(x) ({ \
  lua_Number __issquares = luai_numint(sqrt(x)); \
  (__issquares*__issquares == x); \
})


/* greatest common divisor, 0.9.0 as of Jan 09, 2008; patched 0.26.1, 11.08.2009, improved 2.10.0
   math.gcd := proc(x :: number, y :: number) is
   return when float(x) or float(y) with 1;
   while y <> 0 do
      x, y := y, x % y
   od;
   return x
end; */

/* Taken from:
   https://stackoverflow.com/questions/1231733/euclidean-greatest-common-divisor-for-more-than-two-numbers
   Fastest algorithm I could find on the web. */
static long int aux_gcdmulti (lua_State *L, int nargs, const char *procname) {
  lua_Number x;
  int i;
  long int r, *terms;
  r = 0L;
  terms = calloc(nargs, sizeof(long int));
  if (terms == NULL)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
  for (i=0; i < nargs; i++) {
    if (!agn_isnumber(L, i + 1)) {
      xfree(terms);
      luaL_error(L, "Error in " LUA_QS ": expected a number, got %s.", procname, luaL_typename(L, i + 1));
    }
    x = agn_tonumber(L, i + 1);
    if (tools_isfrac(x)) {
      xfree(terms);
      return 1;
    }
    terms[i] = (long int)labs(x);
  }
  r = terms[0];
  for (i=1; i < nargs; i++) {
    r = tools_gcd(terms[i], r);
    if (r == 1L) break;  /* 6.3.6 fix */
  }
  xfree(terms);
  return r;
}

static int numtheory_gcd (lua_State *L) {  /* 2.12.3, 85 % faster than the Agena implementation */
  lua_Number x, y;
  int nargs = lua_gettop(L);
  if (nargs == 2) {
    x = agn_checknumber(L, 1);
    y = agn_checknumber(L, 2);
    lua_pushnumber(L, tools_gcd(x, y));  /* 2.38.2 tuned by 15 % 2.38.2 */
  } else if (nargs > 2) {  /* 3.20.5 */
    lua_pushnumber(L, aux_gcdmulti(L, nargs, "numtheory.gcd"));
  } else
    luaL_error(L, "Error in " LUA_QS ": expected two or more arguments, got %d.", "numtheory.gcd", nargs);
  return 1;
}


/* least common multiple, 0.9.0 as of Jan 09, 2008; patched 0.26.1, 11.08.2009, changed 2.10.0
   math.lcm := proc(x :: number, y :: number) is
   return if y = 0 then 0 else abs(x/math.gcd(x, y)*y) fi
end; */

/* Taken from:
   https://www.geeksforgeeks.org/lcm-of-given-array-elements/
   Fastest algorithm I could find on the web. */
static lua_Number aux_lcmmulti (lua_State *L, int nargs, const char *procname) {
  int i;
  lua_Number r, *terms;
  terms = calloc(nargs, sizeof(lua_Number));
  if (terms == NULL)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
  for (i=0; i < nargs; i++) {
    if (!agn_isnumber(L, i + 1)) {
      xfree(terms);
      luaL_error(L, "Error in " LUA_QS ": expected a number, got %s.", procname, luaL_typename(L, i + 1));
    }
    terms[i] = fabs(agn_tonumber(L, i + 1));
  }
  r = terms[0];
  for (i=1; i < nargs; i++) {
    r *= terms[i]/tools_gcd(terms[i], r);
  }
  xfree(terms);
  return r;
}

static int numtheory_lcm (lua_State *L) {  /* 2.12.3, 165 % faster than the Agena implementation */
  lua_Number x, y;
  int nargs = lua_gettop(L);
  if (nargs == 2) {
    x = agn_checknumber(L, 1);
    y = agn_checknumber(L, 2);
    lua_pushnumber(L, y == 0.0 ? 0 : fabs(x/tools_gcd(x, y)*y));  /* 2.38.2 tuned by 15 % 2.38.2 */
  } else if (nargs > 2) {  /* 3.20.5 */
    lua_pushnumber(L, aux_lcmmulti(L, nargs, "numtheory.lcm"));
  } else
    luaL_error(L, "Error in " LUA_QS ": expected two or more arguments, got %d.", "numtheory.lcm", nargs);
  return 1;
}


/*
math.fib := proc(n :: nonnegint) is  # modified 2.12.1
   feature reminisce; # creates a read-write remember table
   if n < 2 then
      return n  # exit conditions, changed to Maple V R4's combinat.fibonacci standard with 2.10.2
   elif n < 71 then  # use Golden angle, values precomputed with Maple V Release 4 are:
      # see:  http://stackoverflow.com/questions/6037472/can-a-fibonacci-function-be-written-to-execute-in-o1-time
      # 1.6180339887498948482  = (1 + sqrt(5))/2
      # -0.6180339887498948482 = (1 - sqrt(5))/2
      # 2.2360679774997896964  = sqrt(5)
      # do not use int for in Linux, this causes wrong results
      return math.rint( (1.6180339887498948482**n - (-0.6180339887498948482)**n)/2.2360679774997896964 )  # 2.10.3 change
   elif n < 79 then  # avoid round-off errors in the formula above with n >= 71
      # with remember tables, recursion is much faster than an iterative approach
      return procname(n - 1) + procname(n - 2)
   else  # result is larger than math.lastcontint, thus is not accurate
      return fail
   fi
end; */
static unsigned long long int aux_fib[] = {  /* 0 to 78 (fib(78) is last <= lastcontint = 2^53); added two values 7.5.8s */
                 0ULL,                1ULL,                1ULL,                2ULL,                3ULL,
                 5ULL,                8ULL,               13ULL,               21ULL,               34ULL,
                55ULL,               89ULL,              144ULL,              233ULL,              377ULL,
               610ULL,              987ULL,             1597ULL,             2584ULL,             4181ULL,
              6765ULL,            10946ULL,            17711ULL,            28657ULL,            46368ULL,
             75025ULL,           121393ULL,           196418ULL,           317811ULL,           514229ULL,
            832040ULL,          1346269ULL,          2178309ULL,          3524578ULL,          5702887ULL,
           9227465ULL,         14930352ULL,         24157817ULL,         39088169ULL,         63245986ULL,
         102334155ULL,        165580141ULL,        267914296ULL,        433494437ULL,        701408733ULL,
        1134903170ULL,       1836311903ULL,       2971215073ULL,       4807526976ULL,       7778742049ULL,
       12586269025ULL,      20365011074ULL,      32951280099ULL,      53316291173ULL,      86267571272ULL,
      139583862445ULL,     225851433717ULL,     365435296162ULL,     591286729879ULL,     956722026041ULL,
     1548008755920ULL,    2504730781961ULL,    4052739537881ULL,    6557470319842ULL,   10610209857723ULL,
    17167680177565ULL,   27777890035288ULL,   44945570212853ULL,   72723460248141ULL,  117669030460994ULL,
   190392490709135ULL,  308061521170129ULL,  498454011879264ULL,  806515533049393ULL, 1304969544928657ULL,
  2111485077978050ULL, 3416454622906707ULL, 5527939700884757ULL, 8944394323791464ULL };

/* See also: `Complex Fibonacci` at https://jaketae.github.io/study/complex-fibonacci/ by Jake Tae

readlib(C):
# with(codegen,C):  # Maple 9
Digits := 30;
PHI := (1+sqrt(5))/2;
PHIINV := (1-sqrt(5))/2;
assume(re, real, im, real);
n := re+I*im;
e := 1/sqrt(5)*(PHI^n - PHIINV^n);
f := evalc(e);
C(f, optimized);
*/
static int numtheory_fib (lua_State *L) {  /* 2.12.5, 40 percent faster than the Agena implementation; extended 3.16.5 */
  long double heps, re, im;
  heps = (long double)agnL_optpositive(L, 2, agn_gethepsilon(L));  /* 3.16.7 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number n = agn_tonumber(L, 1);
      if (n < 79.0 && tools_isnonnegint(n)) {
        lua_pushnumber(L, aux_fib[(int)n]);
      } else {
        long double t1, t3, t7, t8;
        t1 = 2.23606797749978969640917366873L;  /* = sqrt(5.0); */
        t3 = tools_powl(0.5L + 0.5L*t1, n);
        t7 = tools_expl(n*tools_logl(-0.5L + 0.5L*t1));
        t8 = n*M_PIld;
        re = tools_zeroinl(t1*(t3 - t7*sun_cosl(t8))*0.2L, heps);  /* 3.16.7 change */
        im = tools_zeroinl(-0.2L*t1*t7*sun_sinl(t8), heps);
        if (im == 0.0L)
          lua_pushnumber(L, re);
        else
          agn_pushcomplex(L, (double)re, (double)im);
      }
      break;
    }
    case LUA_TCOMPLEX: {
      long double t1, t3, t5, t6, t10, t14, t17;
      re = agn_complexreal(L, 1); im = agn_compleximag(L, 1);
      if (im == 0 && re < 79.0 && tools_isnonnegint(re)) {  /* 3.16.7 extension */
        re = aux_fib[(int)re];
      } else {
        t1 = 2.23606797749978969640917366873L;  /* = sqrt(5.0); */
        t3 = tools_logl(0.5L + 0.5L*t1);
        t5 = tools_expl(re*t3);
        t6 = im*t3;
        t10 = tools_logl(-0.5L + 0.5L*t1);
        t14 = tools_expl(re*t10 - M_PIld*im);
        t17 = im*t10 + M_PIld*re;
        re = tools_zeroinl(0.2L*t1*(t5*sun_cosl(t6) - t14*sun_cosl(t17)), heps);  /* 3.16.7 change */
        im = tools_zeroinl(0.2L*t1*(t5*sun_sinl(t6) - t14*sun_sinl(t17)), heps);
      }
      agn_pushcomplex(L, (double)re, (double)im);
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "numtheory.fib");
  }
  return 1;
}


/* For any non-negative integer n returns the index i of the Fibonacci number with fib(i) <= n < fib(i + 1)
   math.fibinv := << x :: nonnegint -> if x < 2 then x else entier(log(x * sqrt 5 + 1/2, math.Phi)) fi >>;  # 2.12.5 */
static int numtheory_fibinv (lua_State *L) {  /* 2.12.6, twice as fast as the Agena implementation, activated 2.16.12 */
  int n = agn_checknonnegint(L, 1);
  if (n < 2)
    lua_pushinteger(L, n);
  else
    lua_pushinteger(L, sun_floor(sun_log(n * SQRT5 + 0.5)*INVPHILN));  /* 2.16.12 optimisation */
  return 1;
}


/* Checks whether a non-negative integer is a Fibonacci number. See: https://www.geeksforgeeks.org/check-number-fibonacci-number,
   by Abhay Rathi. See also: `math.fib`, `math.fibinv`.  2.16.13 */
static int numtheory_isfib (lua_State *L) {
  lua_Number n = agn_checknonnegint(L, 1);
  n *= n;  /* 2.30.3 tweak */
  lua_pushboolean(L, issquare(5*n + 4) || issquare(5*n - 4));  /* or both ! */
  return 1;
}


/* binet := proc(k :: nonnegint, n :: {number, complex}) is
   local metallicmean, invmetallicmean, f;
   f := hypot(k, 2);  # = sqrt(k^2 + 4);
   metallicmean := 0.5*(k + f);
   invmetallicmean := 0.5*(k - f);
   if n :: complex or float(n) then
      invmetallicmean := invmetallicmean!0;
   fi;
   return (metallicmean^n - invmetallicmean^n)/f
end; */

/* math.binet(k, n [, heps])
   For integral n > 2 , computes the n-th term in the sequence of Metallic numbers of order k which is equal to k times the
   (n - 1)st term plus the (n - 2)nd term:

                                    math.binet(k, n) = k*math.binet(k, n - 1) + math.binet(k, n - 2)

   or in other words determines the n-th Metallic number of order k by multiplying k with the predecessor and adding the term
   before that predecessor.

   The defaults for integral n < 2 are: math.binet(k, 0) = 0 and math.binet(k, 1) = 1.

   n may also be a fractional number or a complex number, in these cases computing the generalised Binet formula:

                                            /            2     \n    /            2     \n
                                            | k    sqrt(k  + 4)|     | k    sqrt(k  + 4)|
                                            |--- + ------------|  -  |--- - ------------|
                                            \ 2         2      /     \ 2          2     /
                                            ----------------------------------------------
                                                                   2
                                                             sqrt(k  + 4)

   Specifically, with k = 1, the function computes Fibonacci numbers, with k = 2 Pell numbers and with k = 3 Bronze Fibonacci
   numbers.

   In general, the result always is a complex number, even for integral n. Very small values close to zero in the real and imaginary
   parts of the result are set to zero. You can change the threshold for zeroing by passing the third argument `heps` which is the
   constant hEps = 1.4901161193848e-12 by default. */

/* Pell numbers from n=0 .. 42 */
static unsigned long long int aux_pell[] = {
                 0ULL,               1ULL,                2ULL,
                 5ULL,              12ULL,               29ULL,
                70ULL,             169ULL,              408ULL,
               985ULL,            2378ULL,             5741ULL,
             13860ULL,           33461ULL,            80782ULL,
            195025ULL,          470832ULL,          1136689ULL,
           2744210ULL,         6625109ULL,         15994428ULL,
          38613965ULL,        93222358ULL,        225058681ULL,
         543339720ULL,      1311738121ULL,       3166815962ULL,
        7645370045ULL,     18457556052ULL,      44560482149ULL,
      107578520350ULL,    259717522849ULL,     627013566048ULL,
     1513744654945ULL,   3654502875938ULL,    8822750406821ULL,
    21300003689580ULL,  51422757785981ULL,  124145519261542ULL,
   299713796309065ULL, 723573111879672ULL, 1746860020068409ULL,
  4217293152016490ULL
};

/* Pell numbers from n=0 .. 31 */
static unsigned long long int aux_bronze[] = {
                 0ULL,               1ULL,               3ULL,
                10ULL,              33ULL,             109ULL,
               360ULL,            1189ULL,            3927ULL,
             12970ULL,           42837ULL,          141481ULL,
            467280ULL,         1543321ULL,         5097243ULL,
          16835050ULL,        55602393ULL,       183642229ULL,
         606529080ULL,      2003229469ULL,      6616217487ULL,
       21851881930ULL,     72171863277ULL,    238367471761ULL,
      787274278560ULL,   2600190307441ULL,   8587845200883ULL,
    28363725910090ULL,  93679022931153ULL, 309400794703549ULL,
  1021881407041800ULL, 3375045015828949ULL
};

/*
restart;
Digits := 30;
interface(prettyprint=1);
binet := proc(k, n)
   local metallicmean, invmetallicmean, fac;
   metallicmean := (k + sqrt(k^2 + 4))/2;
   invmetallicmean := (k - sqrt(k^2 + 4))/2;
   fac := sqrt(k^2 + 4);
   round((metallicmean^n - invmetallicmean^n)/fac);
end:
for n from 0 to 76 do
   i := evalf(binet(3, n)):
   if i > 2^53 then lprint(`toobig`, n); break fi;
   lprint(`  `, i, `ULL,`)
od:
*/

static int numtheory_binet (lua_State *L) {  /* 3.16.6 */
  long double re, im, heps, t3, t4, t6, t9, t13, t17, t20, t22, t25, t29, t33, t37;
  int k = agn_checknonnegint(L, 1);
  heps = (long double)agnL_optpositive(L, 3, agn_gethepsilon(L));
  if (agn_isnumber(L, 2)) {
    re = agn_tonumber(L, 2); im = 0;
    switch (k) {
      case 1:  /* Fibonacci sequence */
        if (re < 77 && tools_isnonnegint(re)) {  /* avoid round-off errors & reduce runtime */
          agn_pushcomplex(L, aux_fib[(int)re], 0);
          return 1;
        }
        break;
      case 2:  /* Pell sequence */
        if (re < 43 && tools_isnonnegint(re)) {  /* avoid round-off errors & reduce runtime */
          agn_pushcomplex(L, aux_pell[(int)re], 0);
          return 1;
        }
        break;
      case 3:  /* Bronze Fibonacci sequence */
        if (re < 32 && tools_isnonnegint(re)) {  /* avoid round-off errors & reduce runtime */
          agn_pushcomplex(L, aux_bronze[(int)re], 0);
          return 1;
        }
        break;
      default:
        break;
    }
  } else if (lua_iscomplex(L, 2)) {
    re = agn_complexreal(L, 2); im = agn_compleximag(L, 2);
  } else {
    re = im = 0.0L;
    luaL_nonumorcmplx(L, 2, "numtheory.binet");
  }
  t3 = sqrtl(k*k + 4.0L);
  t4 = 0.5L*(k + t3);
  t6 = tools_logl(fabsl(t4));
  t9 = 0.5L - 0.5L*tools_signuml(t4);
  t13 = tools_expl(re*t6 - im*t9*M_PIld);
  t17 = im*t6 + re*t9*M_PIld;
  t20 = 0.5L*(k - t3);
  t22 = tools_logl(fabsl(t20));
  t25 = 0.5L - 0.5L*tools_signuml(t20);
  t29 = tools_expl(re*t22 - im*t25*M_PIld);
  t33 = im*t22 + re*t25*M_PIld;
  t37 = 1.0L/t3;
  re = tools_zeroinl((t13*sun_cosl(t17) - t29*sun_cosl(t33))*t37, heps);
  im = tools_zeroinl((t13*sun_sinl(t17) - t29*sun_sinl(t33))*t37, heps);
  agn_pushcomplex(L, (double)(re), (double)(im));
  return 1;
}


/* Computes the modular multiplicative inverse of an integer a modulo b and returns an integer x such that:
        a x = 1 ( mod b )
   b is 257 by default. If b is not prime, then not every nonzero integer a has a modular inverse - in this
   case the function returns `undefined`.
   Taken from: https://rosettacode.org/wiki/Modular_inverse#C */
static lua_Integer invmod (lua_Integer a, lua_Integer b) {
  lua_Integer t, nt, r, nr, q, tmp;
  if (b == 0) return -1;
  if (b < 0) b = -b;
  if (a < 0) a = b - (-a % b);
  t = 0;  nt = 1;  r = b;  nr = a % b;
  while (nr != 0) {
    q = r/nr;
    tmp = nt;  nt = t - q*nt;  t = tmp;
    tmp = nr;  nr = r - q*nr;  r = tmp;
  }
  if (r > 1) return -1;  /* no inverse */
  if (t < 0) t += b;
  return t;
}

#define PRIME  257

static int numtheory_invmod (lua_State *L) {  /* 2.38.1 */
  lua_Integer x, n, r;
  x = agn_checkinteger(L, 1);
  n = agnL_optnonnegint(L, 2, PRIME);
  r = invmod(x, n);
  lua_pushnumber(L, (r == -1) ? AGN_NAN : r);  /* behave like Maple does */
  return 1;
}


/* Performs modular multiplication: a*b % n = ((a % n)*(b % n)) % n
   taken from: https://stackoverflow.com/questions/20971888/modular-multiplication-of-large-numbers-in-c
   written by user Kevin Hopps, adapted, and patched for negative modulus (c)
   12 to 22 % faster than mulmod(x,y,n) ( (n == 0) ? AGN_NAN : luai_nummod(luai_nummod(x, n)*luai_nummod(y, n), n))
   for a wider range of integers a, b, c */
#define add(a,b,c) ({ \
  lua_Integer room = (c - 1) - a; \
  ((b <= room) ? a + b : b - room - 1); \
})

/* first statement: q may be negative; second statement: now -c < a && a < c */
#define mod(a,c) ({ \
  lua_Integer q = a/c; \
  a -= q*c; \
  (a + (a < 0)*c); \
})

static double mulmod (lua_Integer a, lua_Integer b, lua_Integer c) {
  lua_Integer t, sc;
  double result = 0;
  if (c == 0) return AGN_NAN;
  sc = (c < 0);
  if (sc) c = -c;  /* extension of original algorithm */
  a = mod(a, c);
  b = mod(b, c);
  if (b > a) { SWAP(a, b, t); }
  while (b) {
    if (b & 0x1) result = add(result, a, c);
    a = add(a, a, c);
    b >>= 1;
  }
  if (sc) {  /* extension of original algorithm */
    result -= c*(result != 0);
  }
  return result;
}

static int numtheory_mulmod (lua_State *L) {  /* 2.38.1 */
  lua_Integer x, y, n;
  x = agn_checkinteger(L, 1);
  y = agn_checkinteger(L, 2);
  n = agn_checkinteger(L, 3);
  lua_pushnumber(L, mulmod(x, y, n));
  return 1;
}


/*
for a from -5 to 5 do
   for b from -5 to 5 do
      for n from -5 to 5 do
         x := math.powmod(a, b, n)
         y := a^b % n
         if x <> y then
            print(a, b, n, x, y)
         fi
      od
   od
od;
*/

/* The function performs modular exponentiation and computes computes x^p % m.
   This is a port of the corresponding function powermod available in Julia 0.5. 2.10.0 */
double nt_powermodint (lua_Integer x, lua_Integer e, lua_Integer m) {
  lua_Integer f, p;  /* p = power */
  if ((x == 0 && e <= 0) || m == 0) return AGN_NAN;  /* 2.38.1 patch, do not return 0 */
  if (m == 1) return 0;
  if (e == 0) return 1;
  if (e < 0) {  /* |x^e| < 1, invert x modulo m with Euclidean algorithm, 2.38.1 patch */
    lua_Integer rc;
    double r = tools_intpow(x, -e);  /* 6.3.6 enhancement */
    if (r == 0) return AGN_NAN;      /* ditto */
    rc = invmod(r, m);  /* round to nearest integer */
    return (rc == -1) ? AGN_NAN : rc;
  }
  x %= m;  /* reduce r initially */
  /* use repeated squaring method (right-to-left) */
  p = 1;
  f = tools_intpow(2, sun_ilogb(e) + 1);
  while (f > 1) {
    p = (p * p) % m;
    f >>= 1;
    if (f <= e) {
      p = (p * x) % m;
      e -= f;
    }
  }
  while (p < 0 && m > 0) p += m;  /* 2.38.1 patch */
  return p;
}

static int numtheory_powmod (lua_State *L) {
  int isintp;
  lua_Number x, p, m, r;
  x = agn_checknumber(L, 1);
  p = agn_checknumber(L, 2);
  m = agn_checknumber(L, 3);
  isintp = tools_isint(p);  /* 2.14.13 change */
  if (tools_isint(x) && isintp && tools_isint(m)) {
    r = nt_powermodint(x, p, m);
  } else {
    if (m == 0) {
      r = AGN_NAN;
    } else {
      r = (isintp) ? luai_numipow(x, p) : luai_numpow(x, p);
      r = (r < m) ? r : luai_nummod(r, m);
    }
  }
  lua_pushnumber(L, r);  /* 2.38.1 change */
  return 1;
}


static int numtheory_isprime (lua_State *L) {
  int64_t n;
  lua_Number x = agn_checknumber(L, 1);
  if (tools_isfrac(x) || fabs(x) > AGN_LASTCONTINT) {  /* 3.20.5 */
    luaL_error(L, "Error in " LUA_QS ": argument is too big or fractional.", "numtheory.isprime");
  }
  n = (int64_t)x;
  lua_pushboolean(L, tools_isprime(n));  /* tuned by 20%, 2.37.9, tuned 6.3.2 */
  return 1;
}


/* Determines if two integers a, b are relatively prime (or co-prime). Two numbers are co-prime if they share no common
   positive factors other than 1. 6.3.5 */
static int numtheory_iscoprime (lua_State *L) {
  lua_Number x, y;
  x = agn_checknumber(L, 1);
  if (tools_isfrac(x) || fabs(x) > AGN_LASTCONTINT) {
    luaL_error(L, "Error in " LUA_QS ": argument is too big or fractional.", "numtheory.iscoprime");
  }
  y = agn_checknumber(L, 2);
  if (tools_isfrac(y) || fabs(y) > AGN_LASTCONTINT) {
    luaL_error(L, "Error in " LUA_QS ": argument is too big or fractional.", "numtheory.iscoprime");
  }
  lua_pushboolean(L, tools_gcd(x, y) == 1);
  return 1;
}


static int numtheory_prevprime (lua_State *L) {  /* 0.33.2, fixed and extended 1.0.6; changed 2.37.9 */
  long long int n;
  lua_Number x = agn_checknumber(L, 1);  /* 2.13.0; 2.38.1; 3.20.5 */
  if (tools_isfrac(x) || fabs(x) > AGN_LASTCONTINT) {
    luaL_error(L, "Error in " LUA_QS ": argument is too big or fractional.", "numtheory.prevprime");
  }
  n = (long long int)x;
  if (n < 3)
    lua_pushfail(L);
  else
    lua_pushnumber(L, tools_prevprime(n));  /* tuned by 15%, 2.37.9 */
  return 1;
}


static int numtheory_nextprime (lua_State *L) {  /* 0.33.2, extended 1.0.6; changed 2.37.9 */
  lua_Number x = agn_checknumber(L, 1);
  if (tools_isfrac(x) || fabs(x) > 9007199254740881.0) {  /* 9007199254740881.0 is previous prime to 2^53 */
    luaL_error(L, "Error in " LUA_QS ": argument is too big or fractional.", "numtheory.prevprime");
  }
  /* 2.13.0; 2.38.1 removed range check (n > 2147483647 || n < -2147483647) as it strains performance */
  lua_pushnumber(L, tools_nextprime((long long int)x));
  return 1;
}


/* Performs prime factorisation for positive integer n and returns the factors in a table. The function
   will issue an error if n > 9,007,199,254,740,992.
   Taken from: https://www.geeksforgeeks.org/print-all-prime-factors-of-a-given-number/ 3.20.5 */
static int numtheory_primes (lua_State *L) {
  long int i, c, n;
  lua_Number x = agn_checknumber(L, 1);
  if (x < 1.0 || tools_isfrac(x) || fabs(x) > AGN_LASTCONTINT) {
    luaL_error(L, "Error in " LUA_QS ": argument is too small, too big or fractional.", "numtheory.primes");
  }
  n = (long int)x;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_createtable(L, agn_log2(L, n), 0);  /* more than enough */
  c = 0;
  while (n % 2 == 0) {  /* number of 2s that divide n */
    lua_rawsetinumber(L, -1, ++c, 2);
    n >>= 1;
  }
  /* n must be odd at this point. So we can skip  one element (Note i = i +2) */
  for (i=3; i*i <= n; i += 2) {
    /* while i divides n, insert i and divide n */
    while (n % i == 0) {
      lua_rawsetinumber(L, -1, ++c, i);
      n /= i;
    }
  }
  /* This condition is to handle the case when n is a prime number greater than 2 */
  if (n > 2) {
    lua_rawsetinumber(L, -1, ++c, n);
  }
  return 1;
}


/* Returns all the integers that divide n without remainder. The result is a table.
   With |n| < 2, the result will only include n. With |n| > 1, the result will never
   include 1 and n, and it is also sorted in ascending order.

   By default, all the results are non-negative. If you pass the option true, however,
   the first number in the resulting table will be negative if n is negative.

   The function is based on https://rosettacode.org/wiki/Factors_of_an_integer#Pascal,
   and was modified to include primes and to avoid insertion of duplicates. 4.11.0 */
static int numtheory_factors (lua_State *L) {
  long int i, c, n, last;
  int isneg, option;
  lua_Number x = agn_checknumber(L, 1);
  if (tools_isfrac(x) || fabs(x) > AGN_LASTCONTINT)
    luaL_error(L, "Error in " LUA_QS ": argument is too big or fractional.", "numtheory.factors");
  isneg = signbit(x);  /* some GCC compilers want a floating-point argument ... */
  n = (long int)x;
  option = agnL_optboolean(L, 2, 0);
  last = -1;
  n = abs(n);
  if (n >= MAX_INT)
    luaL_error(L, "Error in " LUA_QS ": argument is big.", "numtheory.factors");
  luaL_checkstack(L, 2, "not enough stack space");
  lua_createtable(L, 16, 0);
  c = 0;
  for (i=1; i*i < n; i++) {
    if (i != 1 && n % i == 0) {
      lua_rawsetiinteger(L, -1, ++c, c == 0 && option && isneg ? -i : i);
      last = n/i;
      lua_rawsetiinteger(L, -1, ++c, last);
    }
  }
  /* Check to see if number is a square; |x| in {0, 1} will also be dealt with here */
  i = tools_isqrt(n);
  /* printf("n/i == last: %d, n=%d i=%d\n", i != 0 && n/i == last, n, i); */
  if (i*i == n) {
    lua_rawsetiinteger(L, -1, ++c, (option && c == 0 && isneg) ? -i : i);
  } else if (n % i == 0 && n/i != last) {  /* n/i == last with i = {6, 8, 12, ...} */
    lua_rawsetiinteger(L, -1, ++c, (option && c == 0 && isneg) ? -n/i : n/i);
  } else if (c == 0) {  /* we have a prime */
    lua_rawsetiinteger(L, -1, ++c, (option && isneg) ? -n : n);
  }
  /* Change sign of first entry with negative n */
  luaL_checkstack(L, 3, "not enough stack space");
  lua_getglobal(L, "sort");
  if (!lua_isfunction(L, -1))
    luaL_error(L, "Error in " LUA_QS ": could not fetch internal sorting function.", "numtheory.factors");
  lua_pushvalue(L, -2);
  lua_pushstring(L, "number");  /* tune-up */
  lua_call(L, 2, 0);
  return 1;
}


static int numtheory_issquare (lua_State *L) {  /* 2.16.13 */
  lua_Number n = agn_checknumber(L, 1);  /* 4.11.1 extension */
  lua_pushboolean(L, tools_isint(n) ? issquare((lua_Integer)n) : 0);
  return 1;
}


/* Checks if a given integer n is a perfect cube, i.e. if cbrt(n)^3 = n. See also: `cbrt`, `cube`. 2.16.13 */
static int numtheory_iscube (lua_State *L) {  /* 2.16.13 */
  lua_Number n, c;
  n = agn_checknumber(L, 1);  /* 4.11.1 extension */
  if (tools_isfrac(n)) {
    lua_pushfalse(L);
  } else {
    c = luai_numint(cbrt(n));
    lua_pushboolean(L, c*c*c == n);
  }
  return 1;
}


/* Taken from: https://alt.math.recreational.narkive.com/N1aOOtjR/extended-kronecker-s-symbol-for-m-and-n,
   written by Harry J. Smith, ca. 2004.

   KroM1(b) == Kronecker(-1, b)
   got help from Benoit Cloitre abcloitre(AT)wanadoo.fr. He pointed me to
   http://www.research.att.com/cgi-bin/access.cgi/as/njas/sequences/eisA.cgi?Anum=A014577
   and suggested the sequences are the same if you change their 0 to -1.
   See my sequence A097402. */

static FORCE_INLINE int KroM1 (int b) {
  int f = 1;  /* f = final factor */
  if (b < 0) { b = -b; f = -1; }
  if (b < 3) return f;
  while (1) {
    if ((b & 1) == 1) return ((b & 2) == 0) ? f : -f;
    b >>= 1;
  }
}

/* cf. Cohen p.29
   return Kronecker symbol (a|b)
   which, if b is an odd prime, == legendre symbol (a|b) */
static int Kronecker (int a, int b) {
  int f, k, r, v;
  int tab2[] = { 0, 1, 0, -1, 0, -1, 0, 1 };
  if (a == -1) return KroM1(b);
  f = 1;  /* final factor */
  if (a < 0) {
    f = KroM1(b);
    a = -a;
  }
  if (b < 0) b = -b;
  /* (-1)^((a^2-1)/8) == tab2[a & 7] */
  if (b == 0) return (a != 1) ? 0 : f;
  if (a == 0) return (b != 1) ? 0 : f;
  /* (if a and b are both even:) if ( ((a & 1) == 0) && ((b & 1) == 0) ) return 0; */
  /* from here on, a and b are both positive; checked against original query */
  if (a == b) return a + b == 2;  /* change by a_walz */
  v = 0;
  while ((b & 1) == 0) {
    v++;
    b >>= 1;
  }
  k = ((v & 1) == 0) ? 1 : tab2[a & 7];
  while (1) {
    if (a == 0) return (b > 1) ? 0 : f * k;
    v = 0;
    while ((a & 1) == 0) {
      v++;
      a >>= 1;
    }
    if ((v & 1) == 1) k *= tab2[b & 7];  /* k *= (-1)**((b*b-1)/8) */
    if ((a & b & 2) != 0) k = -k;  /* k = k*(-1)**((a-1)*(b-1)/4) */
    r = a;
    a = b % r;
    b = r;
  }
}

static int numtheory_kronecker (lua_State *L) {  /* 4.1.0 */
  lua_pushinteger(L, Kronecker(agn_checkinteger(L, 1), agn_checkinteger(L, 2)));
  return 1;
}


/* Computes the Jacobi symbol J(a, b). a is an integer relatively prime to b, a positive (odd) number.
   Returns either -1, 0 or 1.

   The Jacobi symbol is a generalization of the Legendre symbol.

   Hint: In Maple, numtheory[legendre] just calls numtheory[jacobi].

   Taken from: https://en.wikipedia.org/wiki/Jacobi_symbol, 4.11.0 */
static int jacobi_iter (int a, int n) {
  int r, t;
  if (n == 0) return 0;
  /* step 1 */
  a = (a % n + n) % n;  /* handle (a < 0) */
  /* step 3 */
  t = 0;  /* XOR of bits 1 and 2 determines sign of return value */
  while (a != 0) {
    /* step 2 */
    while (a % 4 == 0) a /= 4;
    if (a % 2 == 0) {
      t ^= n;  /* could be "^= n & 6"; we only care about bits 1 and 2 */
      a >>= 1;
    }
    /* step 4 */
    t ^= a & n & 2;  /* flip sign if a % 4 == n % 4 == 3 */
    r = n % a;
    n = a;
    a = r;
  }
  if (n != 1) return 0;
  else if ((t ^ (t >> 1)) & 2) return -1;
  else return 1;
}

static int jac (int a, int n) {
  int q, t, r;
  r = 1;
  while (a != 0) {
    while (sun_remquo(a, 2.0, &q) == 0) {
      a = q;
      t = luai_nummod(n, 8.0);
      if (t == 3.0 || t == 5.0) r = -r;
    }
    SWAP(a, n, t);
    if (luai_nummod(a, 4.0) == 3.0 && luai_nummod(n, 4.0) == 3.0) r = -r;
    a = luai_nummod(a, n);
  }
  return (n == 1)*r;
}

static int jacobi_rec (int a, int b) {
  int b2, s, q;
  if (((a & 1) == 0 && (b & 1) == 0) || (b == 0 && (a & 1))) return 0;  /* with n = 0 & odd a, the function would recurse indefinitely */
  b2 = b;
  while (sun_remquo(b2, 4.0, &q) == 0.0) {
    b2 = q;
  }
  if (sun_remquo(b2, 2.0, &q) == 0.0) {
    b2 = q; s = jac(2, fabs(a));
  } else
    s = 1;
  if (a < 0 && luai_nummod(b2, 4.0) == 3.0) s = -s;
  return s*jac(fabs(a), b2);
}

static int jacobi (int a, int b) {
  /* if n is even, do it Maple-style, that is recursively */
  return (b & 1) ? jacobi_iter(a, b) : jacobi_rec(a, b);
}

static int numtheory_jacobi (lua_State* L) {
  lua_pushinteger(L, jacobi(agn_checkinteger(L, 1), agn_checkinteger(L, 2)));
  return 1;
}


static const luaL_Reg numtheorylib[] = {
  {"binet",     numtheory_binet},      /* added on May 28, 2024 */
  {"factors",   numtheory_factors},    /* added on April 05, 2025 */
  {"fib",       numtheory_fib},        /* added on July 26, 2018 */
  {"fibinv",    numtheory_fibinv},     /* added on July 31, 2018 */
  {"gcd",       numtheory_gcd},        /* added on July 22, 2018 */
  {"invmod",    numtheory_invmod},     /* added on March 28, 2023 */
  {"iscoprime", numtheory_iscoprime},  /* added on October 31, 2025 */
  {"iscube",    numtheory_iscube},     /* added on January 17, 2020 */
  {"isfib",     numtheory_isfib},      /* added on January 17, 2020 */
  {"isprime",   numtheory_isprime},    /* added on August 19, 2007 */
  {"issquare",  numtheory_issquare},   /* added on January 17, 2020 */
  {"jacobi",    numtheory_jacobi},     /* added on April 04, 2025 */
  {"kronecker", numtheory_kronecker},  /* added on September 05, 2024 */
  {"lcm",       numtheory_lcm},        /* added on July 22, 2018 */
  {"mulmod",    numtheory_mulmod},     /* added on March 28, 2023 */
  {"nextprime", numtheory_nextprime},  /* added on June 24, 2010 */
  {"powmod",    numtheory_powmod},     /* added on October 19, 2016 */
  {"prevprime", numtheory_prevprime},  /* added on June 24, 2010 */
  {"primes",    numtheory_primes},     /* added on August 20, 2024 */
  {NULL, NULL}
};


/*
** Open numtheory library
*/
LUALIB_API int luaopen_numtheory (lua_State *L) {
  luaL_register(L, AGENA_NUMTHEORYLIBNAME, numtheorylib);
  return 1;
}

/* ====================================================================== */

