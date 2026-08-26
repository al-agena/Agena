/* ZX Spectrum mathematical functions

   The functions use the same algorithms and Chebyshev polynomials of degree 6, 8, or 12 as implemented in the
   ZX Spectrum ROM, with similar accuracy.

   All functions are based on the book `The Complete Spectrum ROM Disassembly`, written by Dr. Ian Logan &
   Dr. Frank O’Hara, pp. 217, available - among other sites - at ftp://ftp.worldofspectrum.org/pub/sinclair/books.

   In general, the procedures are mostly slower and also less precise than their Agena pendants. By default, the fully
   expanded and simplified polynomials are hard-wired into the C code. By passing the optional last argument `true`,
   however, the polynomials are processed iteratively in real-time, using the `zx.genseries` function which imitates
   the Z80 assembler subprocedure `series generator`. You may query the respective Chebyshev coefficient vectors by
   calling `zx.getcoeffs`, and globally change them with `zx.setcoeffs`.

   Initiated November 11, 2015; Alexander Walz.

   For licence and copyright, see agena.h file.

   See also article/excerpt `Polynomial Approximations to Elementary Functions` by C. W. Clenshaw,
   National Physical Laboratory, Teddington, Middlesex, England.

   Remark from https://worldofspectrum.org/ZXSpectrum128%2B3Manual/chapter8pt31.html:
   "Numbers are stored to an accuracy of 9 or 10 digits. The largest number you can get is about 10^38, and the smallest
   (positive) number is about 4 x 10^-39."

   "A [floating-point number] is stored in floating point binary with one exponent byte e (1 <= e <= 255), and
   four mantissa bytes m (0.5 <= m <= 1). This represents the number m * 2^(e-128)."

   "Since 0.5 <= m <= 1, the most significant bit of the mantissa m is always 1. Therefore in actual fact we can replace it
   with a bit to show the sign - 0 for positive numbers, 1 for negative.

   Zero has a special representation in which all 5 bytes are 0."

   #Stats:
   import stats;
   import zx alias;
   s := seq();
   for x from -100 to 100 by 0.001 do
      insert SIN(x) |- sin(x) into s
   od;
   stats.median(stats.sorted(s)):
   stats.amean(s):
   stats.minmax(s)[2]:
*/

#include <stdlib.h>
#include <string.h>

#define zx_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnconf.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "prepdefs.h"  /* FORCE_INLINE */

#define AGENA_LIBVERSION	"zx 1.0.0 for Agena as of April 21, 2024\n"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_ZXLIBNAME "zx"
LUALIB_API int (luaopen_zx) (lua_State *L);
#endif


/* C floats do not work here as basic arithmetic implemented in the ZX Spectrum seems to be much more elaborate.
   So use doubles and then round half up (MDFDIR) to 10 digital places (DIGPLACES). Samples checked with the
   EightyOne emulator where `*` is CTRL+B. */
#define MDFDIR     4
#define DIGPLACES  10
#define CASTNUM    lua_Number


/* Chebyshev coefficients for zx.ATN (and indirectly zx.ACS, zx.ASN) */
static size_t ATNA_N = 12;

static lua_Number ATNA[256] = {
  -0.0000000002, 0.0000000010, -0.0000000066, 0.0000000432, -0.0000002850, 0.0000019105,
  -0.0000131076, 0.0000928715, -0.0006905975, 0.0055679210, -0.0529464623, 0.8813735870,
  /* defaults */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/* Chebyshev coefficients for zx.SIN (and zx.COS); for coefficients check the paper `Polynomial Approximations
   to Elementary Functions` m,entioned above with all but the first coefficient taken half. */
static size_t SINA_N = 6;

static lua_Number SINA[256] = {
  -0.000000003, 0.000000592, -0.000068294, 0.004559008, -0.142630785, 1.276278962,
  /* defaults */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/* Chebyshev coefficients for zx.LN (also used by zx.SQR and zx.POW) */
static size_t LNA_N = 12;

static lua_Number LNA[256] = {
  -0.0000000003, 0.0000000020, -0.0000000127, -0.0000000823, -0.0000005389, 0.0000035828,
  -0.0000243013, 0.0001693953, -0.0012282837, 0.0094766116,  -0.0818414567, 0.9302292213,
  /* defaults */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/* Chebyshev coefficients for zx.EXP (also used by zx.SQR and zx.POW) */
static size_t EXPA_N = 8;

static lua_Number EXPA[256] = {
  0.000000001, 0.000000053, 0.000001851, 0.000053453, 0.001235714, 0.021446556, 0.248762434, 1.456999875,
  /* defaults */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/* Receives a number z in the range [-1, 1] and a sequence of coefficients and returns the value of the corresponding
   Chebyshev polynomial. If z is out of range, _no_ error is returned. */
static lua_Number FORCE_INLINE genseries (lua_Number z, lua_Number *a, int breg) {
  lua_Number m0, m1, m2, t, u;
  m0 = 2*z;
  m1 = m2 = t = 0;
  while (breg--) {  /* simplified 3.3.2 */
    m1 = m2;
    u = t*m0 - m2 + *a++;  /* fma significantly slows down the computation */
    m2 = t;
    t = u;
  }
  return t - m1;
}


static int zx_genseries (lua_State *L) {
  lua_Number z, *a;
  int breg, i;
  z = agn_checknumber(L, 1);
  luaL_argcheck(L, lua_isseq(L, 2), 2, "sequence expected");
  breg = agn_seqsize(L, 2);
  if (breg < 1)
    luaL_error(L, "Error in " LUA_QS ": at least one coefficient expected.", "zx.genseries");
  a = agn_malloc(L, breg * sizeof(lua_Number), "zx.genseries", NULL);  /* 2.9.8 */
  for (i=0; i < breg; i++) a[i] = agn_seqgetinumber(L, 2, i + 1);  /* 2.9.8 fix */
  lua_pushnumber(L, genseries(z, a, breg));
  xfree(a);
  return 1;
}


/* ZX Spectrum sine function

   Deviations with respect to C's sin:
   median:    1.5157375354846e-010
   mean:      2.4453803557139e-010
   maximum:   9.1993013207059e-010

   Reference: for arg from -100 to 100 by 0.01.

   12 % faster than C's sin function.

   Also check fastmath.zxchebysin. */

/* From the Chebyshev coeefficients, we generate a polynom with Maple, 6 times more accurate than this one (error 1e-16 instead of 1e-10):
   #define PSIN(z) (1.267162131 - 0.284851843*z + 0.18226552e-1*z*z - 0.546208e-3*z*z*z + 0.9480e-5*z*z*z*z - 0.112e-6*z*z*z*z*z)

Digits := 30:

A := [-0.000000003, 0.000000592, -0.000068294, 0.004559008, -0.142630785, 1.276278962]:

(32*Z*Z*Z*Z*Z-40*Z*Z*Z+10*Z)*A[1]
+(16*Z*Z*Z*Z-16*Z*Z+2)*A[2]
+(8*Z*Z*Z-6*Z)*A[3]
+(4*Z*Z-2)*A[4]
+2*Z *A[5]
+A[6]:

convert(", horner, Z); */

#define PSIN(z) ({ \
  (1.267162130 + (-0.284851836 + (0.18226560e-1 + (-0.546232e-3 + (0.9472e-5 - 0.96e-7*z)*z)*z)*z)*z); \
})

static FORCE_INLINE lua_Number zxsin (lua_Number x, int gen) {
  CASTNUM w, z;
  x = INVPI2*x;  /* 0.1591549... = 0.5/Pi */
  x -= sun_floor(x + 0.5);
  w = 4.0*x;
  if (w > 1) w = 2 - w;
  else if (w < -1) w = -w - 2;
  z = 2.0*w*w - 1.0;
  return w*( (gen) ? genseries(z, SINA, SINA_N) : PSIN(z) );
}


static int zx_sin (lua_State *L) {
  lua_pushnumber(L, zxsin(agn_checknumber(L, 1), agnL_optboolean(L, 2, 0)));
  return 1;
}


/* ZX Spectrum cosine function

   Deviations with respect to C's cos:
   median:    1.5130774411176e-010
   mean:      2.4266713755513e-010
   maximum:   9.1993102024901e-010

   Reference: for arg from -100 to 100 by 0.01

   8 % faster than C's cos function.

   Also check fastmath.zxchebycos. */

static FORCE_INLINE lua_Number zxcos (lua_Number x, int gen) {
  CASTNUM w, z;
  x = INVPI2*x;  /* 0.15915494309189533577 = 0.5/Pi */
  x -= sun_floor(x + 0.5);
  w = 4.0*x;
  /* this is how the ZX Spectrum 48 ROM assembler routine at offset 20 works: preparation to call sine */
  w = (w > 0) ? 1 - w : -w + 1;
  /* argument transformed, prepare to calculate `sine`, same as in zxsin */
  if (w > 1) w = 2 - w;
  else if (w < -1) w = -w - 2;
  z = 2.0*w*w - 1.0;
  return w*( (gen) ? genseries(z, SINA, SINA_N) : PSIN(z) );
}

static int zx_cos (lua_State *L) {
  lua_pushnumber(L, zxcos(agn_checknumber(L, 1), agnL_optboolean(L, 2, 0)));
  return 1;
}


/* ZX Spectrum tangent function

   Deviations with respect to C's tan:
   median:    2.5561464056523e-010
   mean:      3.2139040809957e-009
   maximum:   5.4963111324469e-006

   Reference: for arg from -100 to 100 by 0.01

   7 % faster than C's tan function.

   Also check fastmath.zxchebytan. */

static FORCE_INLINE lua_Number zxtan (lua_Number x, int gen) {
  CASTNUM w, z, a[2];
  int i;
  x = INVPI2*x;  /* 0.15915494309189533577 = 0.5/Pi */
  x -= sun_floor(x + 0.5);
  /* this is how the ZX Spectrum 48 ROM assembler routine at offset 20 works: preparation to call sine */
  for (i=0; i < 2; i++) {
    w = 4.0*x;
    if (i == 1) w = (w > 0) ? 1 - w : -w + 1;  /* cosine to be computed ? */
    /* argument transformed, now calculate sine (i == 0) or cosine (i == 1) */
    if (w > 1) w = 2 - w;
    else if (w < -1) w = -w - 2;
    z = 2.0*w*w - 1.0;
    a[i] = w*((gen) ? genseries(z, SINA, SINA_N) : PSIN(z));
  }
  return a[0]/a[1];
}

static int zx_tan (lua_State *L) {
  lua_pushnumber(L, zxtan(agn_checknumber(L, 1), agnL_optboolean(L, 2, 0)));
  return 1;
}


/* ZX Spectrum ln function (natural logarithm)

   Deviations with respect to C's log:
   median:    3.3824310019526e-008
   mean:      5.0304746414217e-008
   maximum:   1.9750573576616e-007

   Reference: for arg from 0.0001 to 100 by 0.0001

   3 % slower than C's log function. */

static FORCE_INLINE lua_Number zxln (lua_Number d, int gen) {
  CASTNUM s, z, t1, t2, t4, t6;
  int n;
  if (d <= 0) return AGN_NAN;
  d = sun_frexp(d, &n);  /* n holds e', the base-2 exponent, and d the mantissa, 2.14.13 change from frexp to sun_frexp */
  if (d > 0.8) {  /* (D holds X'). */
    s = d - 1;
    z = 2.5*d - 3;
  } else {
    n--;
    s = 2.0*d - 1;
    z = 5.0*d - 3;
  }
  if (gen)
    z = genseries(z, LNA, LNA_N);
  else {
    t1 = z*z;
    t2 = t1*t1;
    t4 = t2*t2;
    t6 = t1*z;
    z = -0.6671616E-3*t2*z - 0.1565489016*z - 0.48128E-5*t4*z - 0.6144E-6*t4*t6
        - 0.560384E-4*t2*t6 - 0.89116584E-2*t6 + 0.2048E-5*t4*t1 - 0.261888E-4*t4
        + 0.2759168E-3*t2*t1 + 0.233844E-2*t2 + 0.353305696E-1*t1 + 0.9116074545;
  }
  return s*z + n*LN2;
}

static int zx_ln (lua_State *L) {
  lua_pushnumber(L, zxln(agn_checknumber(L, 1), agnL_optboolean(L, 2, 0)));
  return 1;
}


/* ZX Spectrum exponential function to the base e

   Deviations with respect to C's exp:
   median:    1.2643013879704e-029
   mean:      1.2496836518746e-007
   maximum:   2.6684032491175e-005

   Reference: for arg from -100 to 10 by 0.0001

   The function is 15 % slower than C's exp function. It looses precision if its
   argument is greater than the constant `E`. */

static FORCE_INLINE lua_Number zxexp (lua_Number d, int gen) {
  CASTNUM z, n, t1, t2, t3, t7;
  d *= INVLN2;  /* (D=C*(1/LN 2);EXP C=2**D). */  /* 2.12.1 improvements */
  n = sun_floor(d);
  z = d - n;  /* (2**(N+Z) is now required). */
  z = 2.0*z - 1;
  if (gen)
    t7 = genseries(z, EXPA, EXPA_N);
  else {
    t1 = z*z;
    t2 = t1*z;
    t3 = t1*t1;
    t7 = 0.128E-6*t3*t2 + 0.59008E-4*t3*z + 0.9811784E-2*t2 + 0.49012908*z +
         0.3392E-5*t3*t1 + 0.85016E-3*t3 + 0.84932884E-1*t1 + 0.1414213563E1;
  }
  return t7 * tools_intpow(2, n);
}

static int zx_exp (lua_State *L) {
  lua_pushnumber(L, zxexp(agn_checknumber(L, 1), agnL_optboolean(L, 2, 0)));
  return 1;
}


/* Returns the ZX Spectrum exponentiation x ^ y, with x and y numbers, and returns a number.

   Internally, the ZX Spectrum and this function treats x^y like exp(ln(x)*y). If x < 0 then
   `undefined` is returned.

   Deviations with respect to C's pow:
   median:    0.00064980709794327
   mean:      1.2496836518746e-007
   maximum:   4437.2052326202

   Reference: for arg from 0 to 10 by 0.0001

   Large deviations with C's pow function. At least twice as slow as pow. */

static FORCE_INLINE lua_Number zxpow (lua_Number x, lua_Number y, int gen) {
  CASTNUM r;
  if (x == 0) {
    if (y == 0) r = 1;
    else if (y > 0) r = 0;
    else r = AGN_NAN;
  } else
    r = zxexp(zxln(x, gen) * y, gen);  /* EXP (Y*LN X) */
  return r;
}

static int zx_pow (lua_State *L) {
  lua_pushnumber(L, zxpow(agn_checknumber(L, 1), agn_checknumber(L, 2), agnL_optboolean(L, 3, 0)));
  return 1;
}


/* Returns the ZX Spectrum square root of its numeric argument x and returns a number. If x < 0, `undefined`
   is returned.

   Internally, the ZX Spectrum treats sqr(x) = pow(x, 0.5).

   Deviations with respect to C's sqrt:
   median:    1.0059902155746e-007
   mean:      1.7540440159232e-007
   maximum:   8.5615920397686e-007

   Reference: for arg from 0 to 100 by 0.0001

   60 percent slower than C's sqrt function. */

#define zxsqr(x, gen)   zxpow((x), 0.5, (gen))
/* einfach und ganz schlecht gemacht is the following: */
#define zxadd(x,y) (tools_roundf((x) + (y), DIGPLACES, MDFDIR))
#define zxsub(x,y) (tools_roundf((x) - (y), DIGPLACES, MDFDIR))
#define zxmul(x,y) (tools_roundf((x) * (y), DIGPLACES, MDFDIR))
#define zxdiv(x,y) (tools_roundf((x) / (y), DIGPLACES, MDFDIR))

static int zx_sqr (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  lua_pushnumber(L, zxsqr(x, agnL_optboolean(L, 2, 0)));
  return 1;
}


static int zx_hyp (lua_State *L) {  /* 3.13.5 */
  lua_Number x = agn_checknumber(L, 1);
  lua_Number y = agn_checknumber(L, 2);
  int gen = agnL_optboolean(L, 3, 0);
  lua_pushnumber(L, zxsqr(zxadd(zxmul(x, x), zxmul(y, y)), gen));
  return 1;
}


/* Computes the ZX Spectrum inverse tangent of its numeric argument x and returns a number.

   Deviations with respect to C's atan:
   median:    3.4858782527181e-012
   mean:      6.647332575213e-012
   maximum:   3.9744829649635e-010

   Reference: for arg from -100 to 100 by 0.0001

   20 percent faster than C's atan function. */

static FORCE_INLINE lua_Number zxatn (lua_Number b, int gen) {
  CASTNUM d, z, t1, t2, t4, t6, t;
  d = (fabs(b) >= 1) ? -1/b : b;
  z = 2*d*d - 1;
  if (gen)
    t = genseries(z, ATNA, ATNA_N);
  else {
    t1 = z*z;
    t2 = t1*t1;
    t4 = t2*t2;
    t6 = t1*z;
    t = -0.3608128E-3*t2*z - 0.10187654*z - 0.22528E-5*t4*z - 0.4096E-6*t4*t6
        - 0.300032E-4*t2*t6 - 0.50309E-2*t6 + 0.1024E-5*t4*t1 + 0.84992E-5*t4
        + 0.1023936E-3*t2*t1 + 0.131556E-2*t2 + 0.208518532E-1*t1 + 0.8704197514;
  };
  t = d*t;
  if (b >= 1) t += PIO2;
  else if (b <= -1) t -= PIO2;
  return t;
}

static int zx_atn (lua_State *L) {
  lua_pushnumber(L, zxatn(agn_checknumber(L, 1), agnL_optboolean(L, 2, 0)));
  return 1;
}


/* Computes the ZX Spectrum inverse sine of its numeric argument x and returns a number.
   If x nE [-1, 1], `undefined` is returned.

   Deviations with respect to C's asin:
   median:    2.1370449854174e-009
   mean:      4.4287855517797e-009
   maximum:   3.0062257683205e-008

   Reference: for arg from -1 to 1 by 0.00001

   VERY slow with respect to C's asin */

static FORCE_INLINE lua_Number zxasn (lua_Number x, int gen) {
  CASTNUM t, atn;
  t = x/(1 + zxsqr(1 - x*x, gen));  /* = tan(y/2)  */
  atn = zxatn(t, gen);
  return atn + atn;
}

static int zx_asn (lua_State *L) {
  lua_pushnumber(L, zxasn(agn_checknumber(L, 1), agnL_optboolean(L, 2, 0)));
  return 1;
}


/* Computes the ZX Spectrum inverse cosine of its numeric argument x and returns a number.
   If x nE [-1, 1], `undefined` is returned.

   Deviations with respect to C's acos:
   median:    2.1370449854174e-009
   mean:      4.4287855521004e-009
   maximum:   3.0062257683205e-008

   Reference: from arg from -1 to 1 by 0.00001 */

static int zx_acs (lua_State *L) {
  lua_pushnumber(L, PI/2 - zxasn(agn_checknumber(L, 1), agnL_optboolean(L, 2, 0)));
  return 1;
}


/* Just for completeness ... */


/* Returns the absolute magnitude of the number x. The function does not use Chebyshev polynomials. */

static int zx_abs (lua_State *L) {
  lua_pushnumber(L, fabs(agn_checknumber(L, 1)));
  return 1;
}


/* Returns -1 if the number x is negative, 0 if x is zero, and 1 if x is positive. If x is `undefined`,
   `undefined` is returned. The function does not use Chebyshev polynomials. */
static int zx_sgn (lua_State *L) {
  lua_pushnumber(L, tools_sign(agn_checknumber(L, 1)));  /* 2.17.1 optimisation */
  return 1;
}


/* Returns -1 if the number x is negative, and +1 otherwise. If x is `undefined`, `undefined` is returned.
   The function does not use Chebyshev polynomials. 3.13.5 */
static int zx_sig (lua_State *L) {
  lua_pushnumber(L, tools_signum(agn_checknumber(L, 1)));
  return 1;
}


/* Rounds its numeric argument x downwards to the nearest integer. The function does not use Chebyshev polynomials. */

static int zx_int (lua_State *L) {
  lua_pushnumber(L, sun_floor(agn_checknumber(L, 1)));
  return 1;
}


/* Returns one if its numeric argument is zero, and zero otherwise. The function does not use Chebyshev polynomials. */

static int zx_not (lua_State *L) {
  lua_pushnumber(L, agn_checknumber(L, 1) == 0);
  return 1;
}


/* Performs the binary operation 'x OR y' and returns the number x if the number y is zero,
   and the value 1 otherwise. Strings are not supported. The function does not use Chebyshev polynomials. */

static int zx_or (lua_State *L) {
  lua_Number x, y;
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  lua_pushnumber(L,  (y == 0) ? x : 1);
  return 1;
}


/* Returns x if y is non-zero and the value zero otherwise. Strings are not supported.
   The function does not use Chebyshev polynomials. */

static int zx_and (lua_State *L) {
  lua_Number x, y;
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  lua_pushnumber(L,  (y != 0) ? x : 0);
  return 1;
}


/* The function returns the Chebyshev coefficient vectors for various package functions. The return is a dictionary of four
   numeric sequences:

   'SIN': coefficient vector for zx.SIN and indirectly zx.COS, 6 numbers by default.
   'ATN': coefficient vector for zx.ATN and indirectly zx.ACS and zx.ASN, 12 numbers by default.
   'LN':  coefficient vector for zx.LN and indirectly zx.SQR and zx.POW, 12 numbers by default.
   'EXP': coefficient vector for zx.EXP and indirectly zx.SQR and zx.POW, 8 numbers by default. */

static int zx_getcoeffs (lua_State *L) {
  int i;
  lua_createtable(L, 0, 4);
  lua_pushstring(L, "SIN");
  agn_createseq(L, SINA_N);
  for (i=0; i < SINA_N; i++)
    agn_seqsetinumber(L, -1, i + 1, SINA[i]);
  lua_rawset(L, -3);
  lua_pushstring(L, "LN");
  agn_createseq(L, LNA_N);
  for (i=0; i < LNA_N; i++)
    agn_seqsetinumber(L, -1, i + 1, LNA[i]);
  lua_rawset(L, -3);
  lua_pushstring(L, "EXP");
  agn_createseq(L, EXPA_N);
  for (i=0; i < EXPA_N; i++)
    agn_seqsetinumber(L, -1, i + 1, EXPA[i]);
  lua_rawset(L, -3);
  lua_pushstring(L, "ATN");
  agn_createseq(L, ATNA_N);
  for (i=0; i < ATNA_N; i++)
    agn_seqsetinumber(L, -1, i + 1, ATNA[i]);
  lua_rawset(L, -3);
  return 1;
}


/* Globally sets Chebyshev coefficients to the package's environment. You can change existing coefficients, reduce or
   enlarge their respective number down to one or up to 256 values. Internally, the coefficients are treated
   as C doubles, the built-in defaults have the precision of C floats.

   The first argument must be the string 'SIN', 'ATN', 'LN', or 'EXP'. The second argument must be a sequence
   of one to 256 numbers.

   For the meanings of the first argument, see `zx.getcoeffs`.

   Please note that the respective zx functions must be called with the last argument `true` in order to revert to
   the (changed) coefficient vectors as they use hard-wired expanded polynomials by default. */

static int zx_setcoeffs (lua_State *L) {
  int i;
  size_t n;
  const char *coeff = agn_checkstring(L, 1);
  if (!lua_isseq(L, 2))
    luaL_error(L, "Error in " LUA_QS ": second argument must be a sequence, got %s.", "zx.setcoeffs", luaL_typename(L, 2));
  n = agn_seqsize(L, 2);
  if (n < 1 || n > 256)
    luaL_error(L, "Error in " LUA_QS ": one to 256 coefficients needed, got %d.", "zx.setcoeffs", n);
  if (tools_streq(coeff, "SIN")) {  /* 2.16.12 tweak */
    for (i=0; i < n; i++)
      SINA[i] = agn_seqgetinumber(L, 2, i + 1);
    SINA_N = n;
  }
  else if (tools_streq(coeff, "LN")) {  /* 2.16.12 tweak */
    for (i=0; i < n; i++)
      LNA[i] = agn_seqgetinumber(L, 2, i + 1);
    LNA_N = n;
  }
  else if (tools_streq(coeff, "EXP")) {  /* 2.16.12 tweak */
    for (i=0; i < n; i++)
      EXPA[i] = agn_seqgetinumber(L, 2, i + 1);
    EXPA_N = n;
  }
  else if (tools_streq(coeff, "ATN")) {  /* 2.16.12 tweak */
    for (i=0; i < n; i++)
      ATNA[i] = agn_seqgetinumber(L, 2, i + 1);
    ATNA_N = n;
  }
  else
    luaL_error(L, "Error in " LUA_QS ": unknown setting %s.", "zx.setcoeffs", coeff);
  return 0;
}


/* Reduces a number x to another number v in the range [-1, 1] where sin(v) = sin(Pi*v/2), and returns v. The function
   imitates the ZX Spectrum ROM 'reduce argument' Z80 assembler subroutine which is used to prepare calls to ZX Spectrum's
   sine and cosine subroutines. 3.2.2 eight percent tweak. Note that using GET_HIGH_WORD gives only a 1.7 % plus, the main
   boost comes from fastFloor. */
/* See: http://www.java2s.com/example/java/java.lang/a-faster-floor-implementation-that-mathfloor.html */
static FORCE_INLINE int fastFloor (double x) {
  int xi = (int)x;
  return x < xi ? xi - 1 : xi;
}

static int zx_reduce (lua_State *L) {
  lua_Number x;
  int32_t hx, ix;
  x = agn_checknumber(L, 1)/PI2;          /* 2.9.8 improvement */
  /* x = 4*(x - sun_floor(x + 0.5)) ; */  /* where x now is greater than, or equal to, -2 but less than +2. */
  x = 4*(x - fastFloor(x + 0.5)) ;        /* where x now is greater than, or equal to, -2 but less than +2. */
  hx = __HI(x);
  ix = hx & 0x7fffffff;                   /* get |x|'s high word */
  if (hx > 0x3FF00000                     /* V = 2-4*Y   if 1 < 4*Y < 2      - case ii. */
    && hx < 0x40000000)      x = 2 - x;
  else if (ix > 0x3FF00000)  x = -x - 2;  /* V = -4*Y-2  if -2 <= 4*Y < -1.  - case iii. */
  /* else (ix <= 0x3FF00000) x = x;          V = 4*Y     if -1 <= 4*Y <= 1   - case i. */
  lua_pushnumber(L, x);
  return 1;
}


static int zx_add (lua_State *L) {  /* 2.33.2 */
  CASTNUM x, y;
  x = (CASTNUM)agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  lua_pushnumber(L, zxadd(x, y));
  return 1;
}


static int zx_sub (lua_State *L) {  /* 2.33.2 */
  CASTNUM x, y;
  x = (CASTNUM)agn_checknumber(L, 1);
  y = (CASTNUM)agn_checknumber(L, 2);
  lua_pushnumber(L, zxsub(x, y));
  return 1;
}


static int zx_mul (lua_State *L) {  /* 2.33.2 */
  CASTNUM x, y;
  x = (CASTNUM)agn_checknumber(L, 1);
  y = (CASTNUM)agn_checknumber(L, 2);
  lua_pushnumber(L, zxmul(x, y));
  return 1;
}


static int zx_div (lua_State *L) {  /* 2.33.2 */
  CASTNUM x, y;
  x = (CASTNUM)agn_checknumber(L, 1);
  y = (CASTNUM)agn_checknumber(L, 2);
  /* if (y == 0)
    luaL_error(L, "6 Number too big, 0:1."); */
printf("%.21f %.21f\n", x/y, zxdiv(x, y));
  lua_pushnumber(L, y == 0 ? AGN_NAN : zxdiv(x, y));
  return 1;
}


/* Modulo, equals zx.SUB(x, zx.MUL(y, zx.INT(zx.DIV(x, y)))), 3.13.5 */
static int zx_mod (lua_State *L) {
  CASTNUM x, y;
  x = (CASTNUM)agn_checknumber(L, 1);
  y = (CASTNUM)agn_checknumber(L, 2);
  /* if (y == 0)
    luaL_error(L, "6 Number too big, 0:1."); */
  if (y == 0) {
    lua_pushundefined(L);
  } else {
    int gen = agnL_optboolean(L, 2, 0);
    CASTNUM t = sun_floor( (gen) ? x/y : zxdiv(x, y) );
    t = (gen) ? y*t : zxmul(y, t);
    lua_pushnumber(L, (gen) ? x - t : zxsub(x, t) );
  }
  return 1;
}


/* Functions that do not exist on the ZX Spectrum ***********************************************/

/* For the stats:
import stats;
import zx alias
import fastmath alias
s := seq();
for x from -5 to 5 by 0.01 do
   insert SINH(x) |- sinh(x) into s
od;
stats.median(stats.sorted(s)):
stats.amean(s):
stats.minmax(s)[2]:

watch();
for i from -5 to 5 by 0.0000001 do
   x := SINH(i)
od;
t1 := watch():

watch();
for i from -5 to 5 by 0.0000001 do
   x := sunsinh(i)
od;
t2 := watch():

if t1 > t2 then print(t1/t2*100 - 100, 'slower') else print(t2/t1*100 - 100, 'faster') fi
*/

/* Non-ZX Spectrum hyperbolic sine function, 2.41.3

   Deviations with respect to C's sun_sinh:
   median:    2.8382753924916e-009
   mean:      8.3764566048346e-009
   maximum:   6.1184167066131e-008

   Reference: for x from -5 to 5 by 0.01 do.

   4 % slower than sun_sinh. */

static int zx_sinh (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  int gen = agnL_optboolean(L, 2, 0);
  lua_pushnumber(L, 0.5*(zxexp(x, gen) - zxexp(-x, gen)));
  return 1;
}


/* Non-ZX Spectrum hyperbolic cosine function, 2.41.3

   Deviations with respect to C's sun_cosh:
   median:    2.8023912079789e-009
   mean:      8.3503459090197e-009
   maximum:   6.118594342297e-008

   Reference: for x from -5 to 5 by 0.01 do.

   21 % slower than sun_cosh. */

static int zx_cosh (lua_State *L) {  /* hyperbolic cosine, 2.41.3 */
  lua_Number x = agn_checknumber(L, 1);
  int gen = agnL_optboolean(L, 2, 0);
  lua_pushnumber(L, 0.5*(zxexp(x, gen) + zxexp(-x, gen)));
  return 1;
}


/* Non-ZX Spectrum hyperbolic tangent function, 2.41.3

   Deviations with respect to C's sun_tanh:
   median:    2.8023912079789e-009
   mean:      8.3503459090197e-009
   maximum:   6.118594342297e-008

   Reference: for x from -5 to 5 by 0.01 do.

   8 % slower than sun_tanh. */

static int zx_tanh (lua_State *L) {  /* hyperbolic tangent, 2.41.3 */
  lua_Number x, ep, em;
  int gen;
  x = agn_checknumber(L, 1);
  gen = agnL_optboolean(L, 2, 0);
  ep = zxexp(x, gen);
  em = zxexp(-x, gen);
  lua_pushnumber(L, (ep - em)/(ep + em));
  return 1;
}


/* Non-ZX Spectrum hyperbolic secant function, 2.41.3

   Deviations with respect to fastmath's sun_sech emu:
   median:    7.1726069528211e-011
   mean:      1.5153870366859e-010
   maximum:   1.0000000827404e-009

   Reference: for x from -5 to 5 by 0.01 do.

   16 % slower than sun_sech. */

static int zx_sech (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  int gen = agnL_optboolean(L, 2, 0);
  lua_pushnumber(L, 2/(zxexp(x, gen) + zxexp(-x, gen)));
  return 1;
}


/* Non-ZX Spectrum hyperbolic cotangent function, 2.41.3

   Deviations with respect to fastmath's sun_coth emu:
   median:    8.7458928987871e-012
   mean:      9.2974888594721e-009
   maximum:   3.0429307571467e-006

   Reference: for x from -5 to 5 by 0.01 do.

   10 % FASTER than sun_coth. */

static int zx_coth (lua_State *L) {  /* hyperbolic cotangent, 2.41.3 */
  lua_Number x, ep, em;
  int gen;
  x = agn_checknumber(L, 1);
  gen = agnL_optboolean(L, 2, 0);
  ep = zxexp(x, gen);
  em = zxexp(-x, gen);
  lua_pushnumber(L, x == 0 ? AGN_NAN : (ep + em)/(ep - em));
  return 1;
}


/* Non-ZX Spectrum hyperbolic cosecant function, 2.41.3

   Deviations with respect to fastmath's sun_csch emu:
   median:    7.8436916683966e-011
   mean:      9.4841847068476e-009
   maximum:   3.0632409959708e-006

   Reference: for x from -5 to 5 by 0.01 do.

   3 % slower than sun_csch. */

static int zx_csch (lua_State *L) {  /* hyperbolic cosecant, 2.41.3 */
  lua_Number x = agn_checknumber(L, 1);
  int gen = agnL_optboolean(L, 2, 0);
  lua_pushnumber(L, 2/(zxexp(x, gen) - zxexp(-x, gen)));
  return 1;
}


/* Non-ZX Spectrum inverse hyperbolic tangent function, 2.41.3

   Deviations with respect to fastmath's sun_atanh:
   median:    1.5212805265508e-008
   mean:      2.3623674021908e-008
   maximum:   9.354782576354e-008

   Reference: for x from -5 to 5 by 0.01 do.

   2 % slower than sun_atanh. */

static int zx_atnh (lua_State *L) {  /* hyperbolic arcus tangent, 2.41.3 */
  lua_Number x = agn_checknumber(L, 1);
  int gen = agnL_optboolean(L, 2, 0);
  lua_pushnumber(L, (fabs(x) >= 1) ? AGN_NAN : 0.5*(zxln((1 + x)/(1 - x), gen)));
  return 1;
}


/* Non-ZX Spectrum inverse hyperbolic sine function, 2.41.3

   Deviations with respect to fastmath's sun_asinh:
   median:    6.8245281870105e-008
   mean:      2.7446085715057e-007
   maximum:   4.5992325072852e-006

   Reference: for x from -5 to 5 by 0.01 do.

   43 % slower than sun_asinh. */

static int zx_asnh (lua_State *L) {  /* hyperbolic arcus sine, 2.41.3 */
  lua_Number x = agn_checknumber(L, 1);
  int gen = agnL_optboolean(L, 2, 0);
  lua_pushnumber(L, zxln(x + zxsqr(x*x + 1, gen), gen));
  return 1;
}


static int zx_acsh (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  int gen = agnL_optboolean(L, 2, 0);
  lua_pushnumber(L, (x < 1) ? AGN_NAN: zxln(x + zxsqr(x*x - 1, gen), gen));
  return 1;
}


static int zx_cot (lua_State *L) {  /* cotangent, 2.41.3 */
  lua_Number x = agn_checknumber(L, 1);
  int gen = agnL_optboolean(L, 2, 0);
  lua_pushnumber(L, x == 0 ? AGN_NAN : 1/zxtan(x, gen));
  return 1;
}


static int zx_csc (lua_State *L) {  /* cosecant, 2.41.3 */
  lua_Number x = agn_checknumber(L, 1);
  int gen = agnL_optboolean(L, 2, 0);
  lua_pushnumber(L, x == 0 ? AGN_NAN : 1/zxsin(x, gen));
  return 1;
}


static int zx_sec (lua_State *L) {  /* secant, 2.41.3 */
  lua_Number x = agn_checknumber(L, 1);
  int gen = agnL_optboolean(L, 2, 0);
  lua_pushnumber(L, 1/zxcos(x, gen));
  return 1;
}


#define ERRA1 0.254829592
#define ERRA2 -0.284496736
#define ERRA3 1.421413741
#define ERRA4 -1.453152027
#define ERRA5 1.061405429
#define ERRP  0.3275911

/* error function, 2.41.3, Abramowitz and Stegun, p. 299, formula 7.1.26 */

static FORCE_INLINE lua_Number zxerf (lua_Number x, int gen) {
  lua_Number t, t2, sgn;
  sgn = 1;
  if (x < 0) {  /* erf is symmetric about the origin, but the polynomial is useless with x < -1, so ... */
    x = -x;
    sgn = -1;
  }
  t = 1/(1 + (ERRP*x));
  t2 = t*t;
  return sgn*(1 - t*(ERRA1 + ERRA2*t + ERRA3*t2 + ERRA4*t2*t + ERRA5*t2*t2)*zxexp(-x*x, gen));  /* error is  |eps| <= 1.5*10^(-7) */
}

static int zx_erf (lua_State *L) {
  lua_pushnumber(L, zxerf(agn_checknumber(L, 1), agnL_optboolean(L, 2, 0)));
  return 1;
}

static int zx_erfc (lua_State *L) {  /* 3.13.5 */
  lua_pushnumber(L, tools_roundf(1 - zxerf(agn_checknumber(L, 1), agnL_optboolean(L, 2, 0)), DIGPLACES, MDFDIR));
  return 1;
}


/* static lua_Number GAMMA[26] = {
   1.0000000000000000,
   0.5772156649015329,
  -0.6558780715202538,
  -0.0420026350340952,
   0.1665386113822915,
  -0.0421977345555443,
  -0.0096219715278770,
   0.0072189432466630,
  -0.0011651675918591,
  -0.0002152416741149,
   0.0001280502823882,
  -0.0000201348547807,
  -0.0000012504934821,
   0.0000011330272320,
  -0.0000002056338417,
   0.0000000061160950,
   0.0000000050020075,
  -0.0000000011812746,
   0.0000000001043427,
   0.0000000000077823,
  -0.0000000000036968,
   0.0000000000005100,
  -0.0000000000000206,
  -0.0000000000000054,
   0.0000000000000014,
   0.0000000000000001
}; */

static lua_Number _GAMMA[27] = {  /* GAMMA reversed, plus additional zero */
   0.0000000000000001,
   0.0000000000000014,
  -0.0000000000000054,
  -0.0000000000000206,
   0.0000000000005100,
  -0.0000000000036968,
   0.0000000000077823,
   0.0000000001043427,
  -0.0000000011812746,
   0.0000000050020075,
   0.0000000061160950,
  -0.0000002056338417,
   0.0000011330272320,
  -0.0000012504934821,
  -0.0000201348547807,
   0.0001280502823882,
  -0.0002152416741149,
  -0.0011651675918591,
   0.0072189432466630,
  -0.0096219715278770,
  -0.0421977345555443,
   0.1665386113822915,
  -0.0420026350340952,
  -0.6558780715202538,
   0.5772156649015329,
   1.0000000000000000,
   0.0000000000000000
};

/* Gamma function, taken from document file `P620_Lec_06_Mod1_ML_06_SpecialFcns.pdf`,
   `Petroleum Engineering 620 Fluid Flow in Petroleum Reservoirs,
   Math Lecture 6 - Introduction to Special Functions` p.15,
   FORTRAN function A_GAMMA(x), Texas A&M University, 2.41.3 */
static int zx_gam (lua_State *L) {
  lua_Number x, r;
  int gen;
  x = agn_checknumber(L, 1);
  gen = agnL_optboolean(L, 2, 0);
  if (x < 2) {  /* Abramowitz and Stegun, p. 256, formula 6.1.34 */
    /* ! the polynomial computes wrong results for x >= 2 ! */
    /* this works too: volatile double xx, *p;
    int n = 26; p = GAMMA; r = 0; xx = x;
    while (n--) {
      r = r + (*p++)*xx; xx *= x;
    } */
    r = tools_polyevals(x, _GAMMA, 27);
    r = (x*r == 0.0) ? AGN_NAN : 1/r;  /* x = 0 or r = 0 3.3.4 fix */
    /* should return undefined for negative integral x, but we don't */
  } else {
    lua_Number t1, t2, t3, t4, x2, x3, x5;
    t1 = -x;
    t2 = 0.91893853320467274178;  /* = 0.5*zxln(2.0*PI, gen), 3.3.4 optimisation */
    x2 = x*x;
    x3 = x2*x;
    t3 = 1.0/(12.0*x) - 1.0/(360.0*x3);
    x5 = x3*x2;
    t4 = 1.0/(1260.0*x5) - 1.0/(1680.0*x5*x2);
    /* exp((x - 0.5)*ln(x) + (t1 + t2 + t3 + t4)) = x^(x-.5000000000)*exp(t1+t2+t3+t4) =
       simplify(exp((x - 0.5)*ln(x) + (t1 + t2 + t3 + t4))); */
    r = zxexp((x - 0.5)*zxln(x, gen) + (t1 + t2 + t3 + t4), gen);
  }
  lua_pushnumber(L, r);
  return 1;
}


/* Logarithmic Gamma function, adapted from document file `P620_Lec_06_Mod1_ML_06_SpecialFcns.pdf`,
   `Petroleum Engineering 620 Fluid Flow in Petroleum Reservoirs,
   Math Lecture 6 - Introduction to Special Functions` p.15,
   FORTRAN function A_GAMMA(x), Texas A&M University, 2.41.3 */
static int zx_lgam (lua_State *L) {
  lua_Number x, r;
  int gen;
  x = agn_checknumber(L, 1);
  gen = agnL_optboolean(L, 2, 0);
  if (x < 2) {  /* Abramowitz and Stegun, p. 256, formula 6.1.34 */
    r = tools_polyevals(x, _GAMMA, 27);
    r = (x == 0.0 || r <= 0) ? AGN_NAN : zxln(1/r, gen);
    /* should return undefined for negative integral x, but we don't */
  } else {  /* this is just an approximation with error ~ 1e-9 ! */
    lua_Number t1, t2, t3, t4, x2, x3, x5;
    t1 = -x;
    t2 = 0.91893853320467274178;  /* = 0.5*zxln(2.0*PI, gen) */
    x2 = x*x;
    x3 = x2*x;
    t3 = 1.0/(12.0*x) - 1.0/(360.0*x3);
    x5 = x3*x2;
    t4 = 1.0/(1260.0*x5) - 1.0/(1680.0*x5*x2);
    /* ln(exp((x - 0.5)*ln(x) + (t1 + t2 + t3 + t4))) = ln(x^(x-0.5000000000)*exp(t1+t2+t3+t4)) =
       simplify(ln(exp((x - 0.5)*ln(x) + (t1 + t2 + t3 + t4)))); */
    r = (x - 0.5)*zxln(x, gen) + (t1 + t2 + t3 + t4);
  }
  lua_pushnumber(L, r);
  return 1;
}


static const luaL_Reg zxlib[] = {
  {"ABS", zx_abs},                  /* added on November 15, 2015 */
  {"ACS", zx_acs},                  /* added on November 15, 2015 */
  {"ACSH", zx_acsh},                /* added on July 02, 2023 */
  {"ADD", zx_add},                  /* added on October 20, 2022 */
  {"AND", zx_and},                  /* added on November 15, 2015 */
  {"ASN", zx_asn},                  /* added on November 15, 2015 */
  {"ASNH", zx_asnh},                /* added on July 02, 2023 */
  {"ATN", zx_atn},                  /* added on November 15, 2015 */
  {"ATNH", zx_atnh},                /* added on July 02, 2023 */
  {"COS", zx_cos},                  /* added on November 12, 2015 */
  {"COSH", zx_cosh},                /* added on July 02, 2023 */
  {"COT", zx_cot},                  /* added on July 02, 2023 */
  {"COTH", zx_coth},                /* added on July 02, 2023 */
  {"CSC", zx_csc},                  /* added on July 02, 2023 */
  {"CSCH", zx_csch},                /* added on July 02, 2023 */
  {"DIV", zx_div},                  /* added on October 20, 2022 */
  {"ERF", zx_erf},                  /* added on July 02, 2023 */
  {"ERFC", zx_erfc},                /* added on April 21, 2024 */
  {"EXP", zx_exp},                  /* added on November 15, 2015 */
  {"GAM", zx_gam},                  /* added on July 02, 2023 */
  {"HYP", zx_hyp},                  /* added on April 21, 2024 */
  {"INT", zx_int},                  /* added on November 16, 2015 */
  {"LGAM", zx_lgam},                /* added on August 16, 2023 */
  {"LN",  zx_ln},                   /* added on November 12, 2015 */
  {"MOD", zx_mod},                  /* added on April 21,, 2024 */
  {"MUL", zx_mul},                  /* added on October 20, 2022 */
  {"NOT", zx_not},                  /* added on November 15, 2015 */
  {"OR",  zx_or},                   /* added on November 15, 2015 */
  {"POW", zx_pow},                  /* added on November 15, 2015 */
  {"SEC", zx_sec},                  /* added on July 02, 2023 */
  {"SECH", zx_sech},                /* added on July 02, 2023 */
  {"SGN", zx_sgn},                  /* added on November 15, 2015 */
  {"SIG", zx_sig},                  /* added on April 21, 2024 */
  {"SIN", zx_sin},                  /* added on November 11, 2015 */
  {"SINH", zx_sinh},                /* added on July 02, 2023 */
  {"SQR", zx_sqr},                  /* added on November 15, 2015 */
  {"SUB", zx_sub},                  /* added on October 20, 2022 */
  {"TAN", zx_tan},                  /* added on November 15, 2015 */
  {"TANH", zx_tanh},                /* added on July 02, 2023 */
  {"genseries", zx_genseries},      /* added on November 12, 2015 */
  {"getcoeffs", zx_getcoeffs},      /* added on November 16, 2015 */
  {"reduce", zx_reduce},            /* added on November 16, 2015 */
  {"setcoeffs", zx_setcoeffs},      /* added on November 16, 2015 */
  {NULL, NULL}
};


/*
** Open ZX library
*/
LUALIB_API int luaopen_zx (lua_State *L) {
  luaL_register(L, AGENA_ZXLIBNAME, zxlib);
  lua_rawsetstringstring(L, -1, "initstring", AGENA_LIBVERSION);
  lua_pushnumber(L, 4*zxatn(1.0, 1));  /* formerly (float)PI, before 2.41.3 */
  lua_setfield(L, -2, "PI");
  lua_pushnumber(L, zxexp(1.0, 1));    /* 2.41.3 */
  lua_setfield(L, -2, "E");
  return 1;
}

