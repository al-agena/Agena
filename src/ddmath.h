/* Double-Double Arithmetic library. */

#ifndef ddmath_h
#define ddmath_h

#include <math.h>
#include <stdio.h>

#ifndef INLINE
  #if defined(__GNUC__) || defined(__DJGPP__)
    #define INLINE static __inline__
  #elif defined(_MSC_VER)
    #define INLINE __inline
  #else
    #define INLINE static
  #endif
#endif

/* Represents a high-precision number as the sum of two doubles */
typedef struct {
  double hi;
  double lo;
} dd_pair;

/* Constants accurate to ~31 decimal digits;
   In double-double arithmetic, the "shortness" of the high part isn't a lack of precision—it
   is a requirement of the format. To maintain the Double-Double invariant, the high part must
   be the nearest 64-bit double to the true value. If you provide a high part with "too many"
   digits, the compiler will simply round it to the nearest 53-bit mantissa anyway. */
static const dd_pair DD_PI   =        {3.141592653589793116e+00, 1.224646799147353207e-16};
static const dd_pair DD_PI_2 =        {1.570796326794896558e+00, 6.123233995736766036e-17};
static const dd_pair DD_PI_4 =        {7.853981633974482790e-01, 3.061616997868383018e-17};
static const dd_pair DD_2PI =         {6.2831853071795864769, 2.4492935982947063544e-16};
static const dd_pair DD_INV_PI_2 =    {0.636619772367581343, 1.5047466351052244e-17};
static const dd_pair DD_LN2 =         {0.693147180559945309, 2.31904681384629956e-17};  /* 7.4.1 improvement */
static const dd_pair DD_LN10 =        {2.30258509299404568, 4.68167643773037595e-17};
static const dd_pair DD_E =           {2.718281828459045, 1.4456468917292502e-16};
static const dd_pair DD_INV_LN2 =     {1.4426950408889634, 2.0355273740931033e-17};
static const dd_pair DD_ZETA2 =       {1.6449340668482264, 3.040672350398476e-17};  /* Zeta(2) = Pi^2/6 */

static const dd_pair DD_LN_SQRT_2PI = {0.9189385332046727, 4.020521404118058e-17};  /* ln(sqrt(2*Pi)) */
static const dd_pair DD_EULER_GAMMA = {0.5772156649015329, -4.942915152430645e-18};
static const dd_pair DD_SQRT2 =       {1.4142135623730951, -9.667293313452913e-17};
static const dd_pair DD_SQRT3 =       {1.7320508075688772, 1.0035084221806903e-16};
static const dd_pair DD_PI_180 =      {0.017453292519943295, 2.9486522708701687e-19};  /* Pi/180.0 */
/* The maximum and minimum values a dd_pair can represent (same as DBL_MAX, DBL_MIN) */
static const dd_pair DD_MAX =         {1.7976931348623157e+308, 0.0};
static const dd_pair DD_MIN =         {2.2250738585072014e-308, 0.0};
static const dd_pair DD_APPROX_EPS =  {1.0e-30, 0.0};  /* 10^-30 as a double-double */

static const dd_pair DD_NOUGHT =      {0.0, 0.0};
static const dd_pair DD_NAUGHT =      {0.0, 0.0};  /* do not set to DD_NOUGHT, Mac OS X does not like it */
static const dd_pair DD_ONE =         {1.0, 0.0};
static const dd_pair DD_TWO =         {2.0, 0.0};
static const dd_pair DD_THREE =       {3.0, 0.0};
static const dd_pair DD_FOUR =        {4.0, 0.0};
static const dd_pair DD_FIVE =        {5.0, 0.0};
static const dd_pair DD_SIX =         {6.0, 0.0};
static const dd_pair DD_SEVEN =       {7.0, 0.0};
static const dd_pair DD_EIGHT =       {8.0, 0.0};
static const dd_pair DD_NINE =        {9.0, 0.0};
static const dd_pair DD_TEN =         {10.0, 0.0};
static const dd_pair DD_ELEVEN =      {11.0, 0.0};
static const dd_pair DD_TWELVE =      {12.0, 0.0};
static const dd_pair DD_HALF =        {0.5, 0.0};
static const dd_pair DD_360 =         {360.0, 0.0};
/* sqrt(2*pi) in double-double */
static const dd_pair DD_SQRT_2PI =    {2.5066282746310005, 1.025195137823553e-16};
/* The first Lanczos coefficient (c0) */
static const dd_pair DD_LY0 =         {1.0000000001900149, 8.23232337621301e-17};

static const dd_pair DD_NAN =         {0.0/0.0, 0.0};
static const dd_pair DD_INF =         {1.0/0.0, 0.0};

#define DD_ZERO DD_NAUGHT
#define dd_neg dd_unm

#define dd2double(dd)   ((double)(dd.hi + dd.lo))

INLINE dd_pair dd_to_radians (dd_pair degrees);
INLINE dd_pair two_sum (double a, double b);
INLINE dd_pair fast_two_sum (double a, double b);
INLINE dd_pair two_diff (double a, double b);
INLINE dd_pair two_prod (double a, double b);
INLINE dd_pair dd_add (dd_pair x, dd_pair y);
INLINE dd_pair dd_sub (dd_pair a, dd_pair b);
INLINE dd_pair dd_mul_d (dd_pair a, double b);
INLINE dd_pair dd_mul (dd_pair x, dd_pair y);
INLINE dd_pair dd_mul_d_d (double a, double b);
dd_pair dd_fma (dd_pair a, dd_pair b, dd_pair c);
INLINE dd_pair dd_div (dd_pair a, dd_pair b);
INLINE dd_pair dd_div_d_d (double a, double b);
INLINE dd_pair dd_inv (dd_pair a);
INLINE dd_pair dd_inv_d (double a);
INLINE dd_pair dd_pow (dd_pair a, dd_pair b);
INLINE dd_pair dd_pow_n (dd_pair a, int n);
INLINE dd_pair dd_pow_d_i (double a, int n);
INLINE dd_pair dd_unm (dd_pair a);
INLINE dd_pair dd_abs (dd_pair a);
INLINE dd_pair dd_absdiff (dd_pair a, dd_pair b);
INLINE int dd_sign (dd_pair a);
INLINE int dd_signbit (dd_pair a);
INLINE dd_pair dd_copysign (dd_pair mag, dd_pair sgn);
INLINE dd_pair dd_square (dd_pair a);
INLINE dd_pair dd_cube (dd_pair a);
INLINE dd_pair dd_sqrt (dd_pair a);
INLINE dd_pair dd_cbrt (dd_pair a);
INLINE dd_pair dd_root_n (dd_pair a, int n);
INLINE dd_pair dd_hypot (dd_pair a, dd_pair b);
INLINE dd_pair dd_hypot4 (dd_pair a, dd_pair b);
INLINE dd_pair dd_erf (dd_pair x);
INLINE dd_pair dd_erfc (dd_pair x);
INLINE dd_pair dd_exp (dd_pair a);
INLINE dd_pair dd_exp2 (dd_pair a);
INLINE dd_pair dd_exp10 (dd_pair a);
INLINE dd_pair dd_log (dd_pair a);
INLINE dd_pair dd_log2 (dd_pair a);
INLINE dd_pair dd_log10 (dd_pair a);
INLINE dd_pair dd_sin (dd_pair a);
INLINE dd_pair dd_cos (dd_pair a);
INLINE dd_pair dd_tan (dd_pair a);
INLINE dd_pair dd_sec (dd_pair a);
INLINE dd_pair dd_csc (dd_pair a);
INLINE dd_pair dd_cot (dd_pair a);
INLINE dd_pair dd_sinc (dd_pair x);
INLINE dd_pair dd_sinh (dd_pair x);
INLINE dd_pair dd_cosh (dd_pair x);
INLINE dd_pair dd_tanh (dd_pair x);
INLINE dd_pair dd_sech (dd_pair a);
INLINE dd_pair dd_csch (dd_pair a);
INLINE dd_pair dd_coth (dd_pair a);
INLINE dd_pair dd_atan2 (dd_pair y, dd_pair x);
INLINE dd_pair dd_atan (dd_pair x);
INLINE dd_pair dd_asin (dd_pair x);
INLINE dd_pair dd_acos (dd_pair x);
INLINE void dd_sincos (dd_pair theta, dd_pair *s, dd_pair *c);
INLINE dd_pair dd_atan2 (dd_pair y, dd_pair x);
INLINE dd_pair dd_asec (dd_pair a);
INLINE dd_pair dd_acsc (dd_pair a);
INLINE dd_pair dd_acot (dd_pair a);
INLINE dd_pair dd_pair_from_d (double d);
INLINE dd_pair dd_lgamma (dd_pair x);
INLINE dd_pair dd_fac (dd_pair a);
INLINE dd_pair dd_floor (dd_pair a);
INLINE dd_pair dd_ceil (dd_pair a);
INLINE dd_pair dd_round (dd_pair a);
INLINE dd_pair dd_trunc (dd_pair a);
INLINE dd_pair dd_frac (dd_pair a);
INLINE dd_pair dd_modf (dd_pair a, dd_pair *iptr);
INLINE dd_pair dd_renorm (double hi, double lo);
INLINE dd_pair dd_ldexp (dd_pair a, int e);
INLINE dd_pair dd_frexp (dd_pair a, int *e);
const char *dd_to_str (dd_pair a);
INLINE int dd_eq (dd_pair a, dd_pair b);
INLINE int dd_lt (dd_pair a, dd_pair b);
INLINE int dd_gt (dd_pair a, dd_pair b);
INLINE int dd_le (dd_pair a, dd_pair b);
INLINE int dd_ge (dd_pair a, dd_pair b);
int dd_approx (dd_pair x, dd_pair y, dd_pair eps);
INLINE int dd_zero (dd_pair a);
INLINE int dd_nonzero (dd_pair a);
INLINE int dd_isnan (dd_pair a);
INLINE int dd_isinf (dd_pair a);
INLINE int dd_isfinite (dd_pair a);

#endif


