/* Cephes mathematical functions

   taken from various C files written by Stephen L. Moshier.

   Cephes Math Library, Copyright by Stephen L. Moshier
   Direct inquiries to 30 Frost Street, Cambridge, MA 02140 */

#include <math.h>

#ifndef PROPCMPLX
#include <complex.h>
#endif

#define cephes_c
#define LUA_LIB

#include "agncmpt.h"
#include "agnconf.h"
#include "cephes.h"

/*							polevl.c
 *							p1evl.c
 *
 *	Evaluate polynomial
 *
 * SYNOPSIS:
 *
 * int N;
 * double x, y, coef[N+1], polevl[];
 *
 * y = polevl( x, coef, N );
 *
 * DESCRIPTION:
 *
 * Evaluates polynomial of degree N:
 *
 *                     2          N
 * y  =  C  + C x + C x  +...+ C x
 *        0    1     2          N
 *
 * Coefficients are stored in reverse order:
 *
 * coef[0] = C  , ..., coef[N] = C  .
 *            N                   0
 *
 *  The function p1evl() assumes that coef[N] = 1.0 and is
 * omitted from the array.  Its calling arguments are
 * otherwise the same as polevl().
 *
 * SPEED:
 *
 * In the interest of speed, there are no checks for out
 * of bounds arithmetic.  This routine is used by most of
 * the functions in the library.  Depending on available
 * equipment features, the user may wish to rewrite the
 * program in microcode or assembly language.
 */

/*
 * Cephes Math Library Release 2.1: December, 1988
 * Copyright 1984, 1987, 1988 by Stephen L. Moshier
 * Direct inquiries to 30 Frost Street, Cambridge, MA 02140
 */

double polevl (double x, double coef[], int N) {
  double ans;
  int i;
  double *p;
  p = coef;
  ans = *p++;
  i = N;
  do
    ans = fma(ans, x, *p++);  /* 6.4.9 improvement */
  while (--i);
  return ans;
}

/*							p1evl()	*/
/*                                          N
 * Evaluate polynomial when coefficient of x  is 1.0.
 * Otherwise same as polevl.
 */

double p1evl (double x, double coef[], int N) {
  double ans;
  double *p;
  int i;
  p = coef;
  ans = x + *p++;
  i = N - 1;
  do
    ans = fma(ans, x, *p++);  /* 6.4.9 improvement */
  while (--i);
  return ans;
}


/*							ei.c
 *
 *	Exponential integral
 *
 * SYNOPSIS:
 *
 * double x, y, ei();
 *
 * y = ei( x );
 *
 * DESCRIPTION:
 *
 *               x
 *                -     t
 *               | |   e
 *    Ei(x) =   -|-   ---  dt .
 *             | |     t
 *              -
 *             -inf
 *
 * Not defined for x <= 0.
 * See also expn.c.
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE       0,100       50000      8.6e-16     1.3e-16
 *
 * Cephes Math Library Release 2.8:  May, 1999
 * Copyright 1999 by Stephen L. Moshier */

/* 0 < x <= 2
   Ei(x) - EUL - ln(x) = x A(x)/B(x)
   Theoretical peak relative error 9.73e-18  */
static double A[6] = {
  -5.350447357812542947283E0,
   2.185049168816613393830E2,
  -4.176572384826693777058E3,
   5.541176756393557601232E4,
  -3.313381331178144034309E5,
   1.592627163384945414220E6,
};

static double B[6] = {
  /*  1.000000000000000000000E0, */
  -5.250547959112862969197E1,
   1.259616186786790571525E3,
  -1.756549581973534652631E4,
   1.493062117002725991967E5,
  -7.294949239640527645655E5,
   1.592627163384945429726E6,
};


/* 8 <= x <= 20
   x exp(-x) Ei(x) - 1 = 1/x R(1/x)
   Theoretical peak absolute error = 1.07e-17  */
static double A2[10] = {
  -2.106934601691916512584E0,
   1.732733869664688041885E0,
  -2.423619178935841904839E-1,
   2.322724180937565842585E-2,
   2.372880440493179832059E-4,
  -8.343219561192552752335E-5,
   1.363408795605250394881E-5,
  -3.655412321999253963714E-7,
   1.464941733975961318456E-8,
   6.176407863710360207074E-10,
};

static double B2[9] = {
  -2.298062239901678075778E-1,
   1.105077041474037862347E-1,
  -1.566542966630792353556E-2,
   2.761106850817352773874E-3,
  -2.089148012284048449115E-4,
   1.708528938807675304186E-5,
  -4.459311796356686423199E-7,
   1.394634930353847498145E-8,
   6.150865933977338354138E-10,
};

/* x > 20
   x exp(-x) Ei(x) - 1  =  1/x A3(1/x)/B3(1/x)
   Theoretical absolute error = 6.15e-17  */
static double A3[9] = {
  -7.657847078286127362028E-1,
   6.886192415566705051750E-1,
  -2.132598113545206124553E-1,
   3.346107552384193813594E-2,
  -3.076541477344756050249E-3,
   1.747119316454907477380E-4,
  -6.103711682274170530369E-6,
   1.218032765428652199087E-7,
  -1.086076102793290233007E-9,
};

static double B3[9] = {
  -1.888802868662308731041E0,
   1.066691687211408896850E0,
  -2.751915982306380647738E-1,
   3.930852688233823569726E-2,
  -3.414684558602365085394E-3,
   1.866844370703555398195E-4,
  -6.345146083130515357861E-6,
   1.239754287483206878024E-7,
  -1.086076102793126632978E-9,
};

/* 16 <= x <= 32
   x exp(-x) Ei(x) - 1  =  1/x A4(1/x) / B4(1/x)
   Theoretical absolute error = 1.22e-17  */
static double A4[8] = {
  -2.458119367674020323359E-1,
  -1.483382253322077687183E-1,
   7.248291795735551591813E-2,
  -1.348315687380940523823E-2,
   1.342775069788636972294E-3,
  -7.942465637159712264564E-5,
   2.644179518984235952241E-6,
  -4.239473659313765177195E-8,
};

static double B4[8] = {
  -1.044225908443871106315E-1,
  -2.676453128101402655055E-1,
   9.695000254621984627876E-2,
  -1.601745692712991078208E-2,
   1.496414899205908021882E-3,
  -8.462452563778485013756E-5,
   2.728938403476726394024E-6,
  -4.239462431819542051337E-8,
};

/* 4 <= x <= 8
   x exp(-x) Ei(x) - 1  =  1/x A5(1/x) / B5(1/x)
   Theoretical absolute error = 2.20e-17  */
static double A5[8] = {
  -1.373215375871208729803E0,
  -7.084559133740838761406E-1,
   1.580806855547941010501E0,
  -2.601500427425622944234E-1,
   2.994674694113713763365E-2,
  -1.038086040188744005513E-3,
   4.371064420753005429514E-5,
   2.141783679522602903795E-6,
};

static double B5[8] = {
   8.585231423622028380768E-1,
   4.483285822873995129957E-1,
   7.687932158124475434091E-2,
   2.449868241021887685904E-2,
   8.832165941927796567926E-4,
   4.590952299511353531215E-4,
  -4.729848351866523044863E-6,
   2.665195537390710170105E-6,
};
/* 2 <= x <= 4
   x exp(-x) Ei(x) - 1  =  1/x A6(1/x) / B6(1/x)
   Theoretical absolute error = 4.89e-17  */
static double A6[8] = {
   1.981808503259689673238E-2,
  -1.271645625984917501326E0,
  -2.088160335681228318920E0,
   2.755544509187936721172E0,
  -4.409507048701600257171E-1,
   4.665623805935891391017E-2,
  -1.545042679673485262580E-3,
   7.059980605299617478514E-5,
};

static double B6[7] = {
   1.476498670914921440652E0,
   5.629177174822436244827E-1,
   1.699017897879307263248E-1,
   2.291647179034212017463E-2,
   4.450150439728752875043E-3,
   1.727439612206521482874E-4,
   3.953167195549672482304E-5,
};
/* 32 <= x <= 64
   x exp(-x) Ei(x) - 1  =  1/x A7(1/x) / B7(1/x)
   Theoretical absolute error = 7.71e-18  */
static double A7[6] = {
   1.212561118105456670844E-1,
  -5.823133179043894485122E-1,
   2.348887314557016779211E-1,
  -3.040034318113248237280E-2,
   1.510082146865190661777E-3,
  -2.523137095499571377122E-5,
};

static double B7[5] = {
  -1.002252150365854016662E0,
   2.928709694872224144953E-1,
  -3.337004338674007801307E-2,
   1.560544881127388842819E-3,
  -2.523137093603234562648E-5,
};

double ei (double x) {
  double f, w;
  if (x <= 0.0) return AGN_NAN;  /* tip: use jbmath.c' r8_ei for negative x */
  else if (x < 2.0) {
    /* Power series.
                            inf    n
                             -    x
     Ei(x) = EUL + ln x  +   >   ----
                             -   n n!
                            n=1
    */
    f = polevl(x, A, 5)/p1evl(x, B, 6);
    /*      f = polevl(x, A, 6)/p1evl(x, B, 7); */
    /*      f = polevl(x, A, 8)/p1evl(x, B, 9); */
    return EUL + sun_log(x) + x * f;  /* 2.16.6 change */
  }
  else if (x < 4.0) {
    /* Asymptotic expansion.
                            1       2       6
    x exp(-x) Ei(x) =  1 + ---  +  ---  +  ---- + ...
                            x        2       3
                                    x       x
    */
    w = 1.0/x;
    f = polevl(w, A6, 7)/p1evl(w, B6, 7);
    return sun_exp(x)*w*(1.0 + w*f);
  }
  else if (x < 8.0) {
    w = 1.0/x;
    f = polevl(w, A5, 7)/p1evl(w, B5, 8);
    return sun_exp(x)*w*(1.0 + w*f);
  }
  else if (x < 16.0) {
    w = 1.0/x;
    f = polevl(w, A2, 9)/p1evl(w, B2, 9);
    return sun_exp(x)*w*(1.0 + w*f);
  }
  else if (x < 32.0) {
    w = 1.0/x;
    f = polevl(w, A4, 7)/p1evl(w, B4, 8);
    return sun_exp(x)*w*(1.0 + w*f);
  }
  else if (x < 64.0) {
    w = 1.0/x;
    f = polevl(w,A7,5)/p1evl(w,B7,5);
    return sun_exp(x)*w*(1.0 + w*f);
  } else {
    w = 1.0/x;
    f = polevl(w,A3,8)/p1evl(w,B3,9);
    return sun_exp(x)*w*(1.0 + w*f);
  }
}


/*							sici.c
 *
 *	Sine and cosine integrals
 *
 * SYNOPSIS:
 *
 * double x, Ci, Si, sici();
 *
 * sici( x, &Si, &Ci );
 *
 * DESCRIPTION:
 *
 * Evaluates the integrals
 *
 *                          x
 *                          -
 *                         |  cos t - 1
 *   Ci(x) = eul + ln x +  |  --------- dt,
 *                         |      t
 *                        -
 *                         0
 *             x
 *             -
 *            |  sin t
 *   Si(x) =  |  ----- dt
 *            |    t
 *           -
 *            0
 *
 * where eul = 0.57721566490153286061 is Euler's constant.
 * The integrals are approximated by rational functions.
 * For x > 8 auxiliary functions f(x) and g(x) are employed
 * such that
 *
 * Ci(x) = f(x) sin(x) - g(x) cos(x)
 * Si(x) = pi/2 - f(x) cos(x) - g(x) sin(x)
 *
 * ACCURACY:
 *    Test interval = [0,50].
 * Absolute error, except relative when > 1:
 * arithmetic   function   # trials      peak         rms
 *    IEEE        Si        30000       4.4e-16     7.3e-17
 *    IEEE        Ci        30000       6.9e-16     5.1e-17
 *    DEC         Si         5000       4.4e-17     9.0e-18
 *    DEC         Ci         5300       7.9e-17     5.2e-18
 *
 * Cephes Math Library Release 2.1:  January, 1989
 * Copyright 1984, 1987, 1989 by Stephen L. Moshier
 * Direct inquiries to 30 Frost Street, Cambridge, MA 02140 */

static double SN[] = {
  -8.39167827910303881427E-11,
   4.62591714427012837309E-8,
  -9.75759303843632795789E-6,
   9.76945438170435310816E-4,
  -4.13470316229406538752E-2,
   1.00000000000000000302E0,
};

static double SD[] = {
   2.03269266195951942049E-12,
   1.27997891179943299903E-9,
   4.41827842801218905784E-7,
   9.96412122043875552487E-5,
   1.42085239326149893930E-2,
   9.99999999999999996984E-1,
};

static double CN[] = {
   2.02524002389102268789E-11,
  -1.35249504915790756375E-8,
   3.59325051419993077021E-6,
  -4.74007206873407909465E-4,
   2.89159652607555242092E-2,
  -1.00000000000000000080E0,
};

static double CD[] = {
   4.07746040061880559506E-12,
   3.06780997581887812692E-9,
   1.23210355685883423679E-6,
   3.17442024775032769882E-4,
   5.10028056236446052392E-2,
   4.00000000000000000080E0,
};

static double FN4[] = {
   4.23612862892216586994E0,
   5.45937717161812843388E0,
   1.62083287701538329132E0,
   1.67006611831323023771E-1,
   6.81020132472518137426E-3,
   1.08936580650328664411E-4,
   5.48900223421373614008E-7,
};

static double FD4[] = {
   8.16496634205391016773E0,
   7.30828822505564552187E0,
   1.86792257950184183883E0,
   1.78792052963149907262E-1,
   7.01710668322789753610E-3,
   1.10034357153915731354E-4,
   5.48900252756255700982E-7,
};

static double FN8[] = {
   4.55880873470465315206E-1,
   7.13715274100146711374E-1,
   1.60300158222319456320E-1,
   1.16064229408124407915E-2,
   3.49556442447859055605E-4,
   4.86215430826454749482E-6,
   3.20092790091004902806E-8,
   9.41779576128512936592E-11,
   9.70507110881952024631E-14,
};

static double FD8[] = {
   9.17463611873684053703E-1,
   1.78685545332074536321E-1,
   1.22253594771971293032E-2,
   3.58696481881851580297E-4,
   4.92435064317881464393E-6,
   3.21956939101046018377E-8,
   9.43720590350276732376E-11,
   9.70507110881952025725E-14,
};

static double GN4[] = {
   8.71001698973114191777E-2,
   6.11379109952219284151E-1,
   3.97180296392337498885E-1,
   7.48527737628469092119E-2,
   5.38868681462177273157E-3,
   1.61999794598934024525E-4,
   1.97963874140963632189E-6,
   7.82579040744090311069E-9,
};

static double GD4[] = {
   1.64402202413355338886E0,
   6.66296701268987968381E-1,
   9.88771761277688796203E-2,
   6.22396345441768420760E-3,
   1.73221081474177119497E-4,
   2.02659182086343991969E-6,
   7.82579218933534490868E-9,
};

static double GN8[] = {
   6.97359953443276214934E-1,
   3.30410979305632063225E-1,
   3.84878767649974295920E-2,
   1.71718239052347903558E-3,
   3.48941165502279436777E-5,
   3.47131167084116673800E-7,
   1.70404452782044526189E-9,
   3.85945925430276600453E-12,
   3.14040098946363334640E-15,
};

static double GD8[] = {
   1.68548898811011640017E0,
   4.87852258695304967486E-1,
   4.67913194259625806320E-2,
   1.90284426674399523638E-3,
   3.68475504442561108162E-5,
   3.57043223443740838771E-7,
   1.72693748966316146736E-9,
   3.87830166023954706752E-12,
   3.14040098946363335242E-15,
};

void sici (double x, double *si, double *ci) {
  double z, c, s, f, g;
  short sign;
  if (x < 0.0) {
    sign = -1;
    x = -x;
  } else
    sign = 0;
  if (x == 0.0) {
    *si = 0.0;
    *ci = AGN_NAN;
    return;
  }
  if (x > 1.0e9) {
	*si = PIO2 - sun_cos(x)/x;
	*ci = sun_sin(x)/x;
	return;
  }
  if (x > 4.0) goto asympt;
  z = x * x;
  s = x * polevl(z, SN, 5)/polevl(z, SD, 5);
  c = z * polevl(z, CN, 5)/polevl(z, CD, 5);
  if (sign) s = -s;
  *si = s;
  *ci = EUL + sun_log(x) + c;	/* real part if x < 0 */ /* 2.16.6 change */
  return;

/* The auxiliary functions are:
 *
 *
 * *si = *si - PIO2;
 * c = cos(x);
 * s = sin(x);
 *
 * t = *ci * s - *si * c;
 * a = *ci * c + *si * s;
 *
 * *si = t;
 * *ci = -a;
 */
asympt:
  s = sun_sin(x);
  c = sun_cos(x);
  z = 1.0/(x*x);
  if (x < 8.0) {
    f = polevl(z, FN4, 6)/(x * p1evl(z, FD4, 7));
    g = z * polevl(z, GN4, 7)/p1evl(z, GD4, 7);
  } else {
    f = polevl(z, FN8, 8)/(x * p1evl(z, FD8, 8));
    g = z * polevl(z, GN8, 8)/p1evl(z, GD8, 9);
  }
  *si = PIO2 - f * c - g * s;
  if (sign) *si = -(*si);
  *ci = f * s - g * c;
  return;
}

/*							chbevl.c
 *
 *	Evaluate Chebyshev series
 *
 * SYNOPSIS:
 *
 * int N;
 * double x, y, coef[N], chebevl();
 *
 * y = chbevl( x, coef, N );
 *
 * DESCRIPTION:
 *
 * Evaluates the series
 *
 *        N-1
 *         - '
 *  y  =   >   coef[i] T (x/2)
 *         -            i
 *        i=0
 *
 * of Chebyshev polynomials Ti at argument x/2.
 *
 * Coefficients are stored in reverse order, i.e. the zero
 * order term is last in the array.  Note N is the number of
 * coefficients, not the order.
 *
 * If coefficients are for the interval a to b, x must
 * have been transformed to x -> 2(2x - b - a)/(b-a) before
 * entering the routine.  This maps x from (a, b) to (-1, 1),
 * over which the Chebyshev polynomials are defined.
 *
 * If the coefficients are for the inverted interval, in
 * which (a, b) is mapped to (1/b, 1/a), the transformation
 * required is x -> 2(2ab/x - b - a)/(b-a).  If b is infinity,
 * this becomes x -> 4a/x - 1.
 *
 * SPEED:
 *
 * Taking advantage of the recurrence properties of the
 * Chebyshev polynomials, the routine requires one more
 * addition per loop than evaluating a nested polynomial of
 * the same degree.
 *
 * Cephes Math Library Release 2.0:  April, 1987
 * Copyright 1985, 1987 by Stephen L. Moshier
 * Direct inquiries to 30 Frost Street, Cambridge, MA 02140 */

double chbevl (double x, double array[], int n) {
  double b0, b1, b2, *p;
  int i;
  p = array;
  b0 = *p++;
  b1 = 0.0;
  i = n - 1;
  do {
    b2 = b1;
    b1 = b0;
    /* b0 = x*b1 - b2 + *p++; */
    b0 = fma(x, b1, - b2 + *p++);  /* 6.4.9 improvement */
  } while (--i);
  return 0.5*(b0 - b2);
}

/*							shichi.c
 *
 *	Hyperbolic sine and cosine integrals
 *
 * SYNOPSIS:
 *
 * double x, Chi, Shi, shichi();
 *
 * shichi( x, &Chi, &Shi );
 *
 * DESCRIPTION:
 *
 * Approximates the integrals
 *
 *                            x
 *                            -
 *                           | |   cosh t - 1
 *   Chi(x) = eul + ln x +   |    -----------  dt,
 *                         | |          t
 *                          -
 *                          0
 *
 *               x
 *               -
 *              | |  sinh t
 *   Shi(x) =   |    ------  dt
 *            | |       t
 *             -
 *             0
 *
 * where eul = 0.57721566490153286061 is Euler's constant.
 * The integrals are evaluated by power series for x < 8
 * and by Chebyshev expansions for x between 8 and 88.
 * For large x, both functions approach exp(x)/2x.
 * Arguments greater than 88 in magnitude return MAXNUM.
 *
 * ACCURACY:
 *
 * Test interval 0 to 88.
 *                      Relative error:
 * arithmetic   function  # trials      peak         rms
 *    DEC          Shi       3000       9.1e-17
 *    IEEE         Shi      30000       6.9e-16     1.6e-16
 *        Absolute error, except relative when |Chi| > 1:
 *    DEC          Chi       2500       9.3e-17
 *    IEEE         Chi      30000       8.4e-16     1.4e-16
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 2000 by Stephen L. Moshier */

/* x exp(-x) shi(x), inverted interval 8 to 18 */
static double S1[] = {
   1.83889230173399459482E-17,
  -9.55485532279655569575E-17,
   2.04326105980879882648E-16,
   1.09896949074905343022E-15,
  -1.31313534344092599234E-14,
   5.93976226264314278932E-14,
  -3.47197010497749154755E-14,
  -1.40059764613117131000E-12,
   9.49044626224223543299E-12,
  -1.61596181145435454033E-11,
  -1.77899784436430310321E-10,
   1.35455469767246947469E-9,
  -1.03257121792819495123E-9,
  -3.56699611114982536845E-8,
   1.44818877384267342057E-7,
   7.82018215184051295296E-7,
  -5.39919118403805073710E-6,
  -3.12458202168959833422E-5,
   8.90136741950727517826E-5,
   2.02558474743846862168E-3,
   2.96064440855633256972E-2,
   1.11847751047257036625E0
};

/* x exp(-x) shi(x), inverted interval 18 to 88 */
static double S2[] = {
  -1.05311574154850938805E-17,
   2.62446095596355225821E-17,
   8.82090135625368160657E-17,
  -3.38459811878103047136E-16,
  -8.30608026366935789136E-16,
   3.93397875437050071776E-15,
   1.01765565969729044505E-14,
  -4.21128170307640802703E-14,
  -1.60818204519802480035E-13,
   3.34714954175994481761E-13,
   2.72600352129153073807E-12,
   1.66894954752839083608E-12,
  -3.49278141024730899554E-11,
  -1.58580661666482709598E-10,
  -1.79289437183355633342E-10,
   1.76281629144264523277E-9,
   1.69050228879421288846E-8,
   1.25391771228487041649E-7,
   1.16229947068677338732E-6,
   1.61038260117376323993E-5,
   3.49810375601053973070E-4,
   1.28478065259647610779E-2,
   1.03665722588798326712E0
};

/* x exp(-x) chin(x), inverted interval 8 to 18 */
static double C1[] = {
  -8.12435385225864036372E-18,
   2.17586413290339214377E-17,
   5.22624394924072204667E-17,
  -9.48812110591690559363E-16,
   5.35546311647465209166E-15,
  -1.21009970113732918701E-14,
  -6.00865178553447437951E-14,
   7.16339649156028587775E-13,
  -2.93496072607599856104E-12,
  -1.40359438136491256904E-12,
   8.76302288609054966081E-11,
  -4.40092476213282340617E-10,
  -1.87992075640569295479E-10,
   1.31458150989474594064E-8,
  -4.75513930924765465590E-8,
  -2.21775018801848880741E-7,
   1.94635531373272490962E-6,
   4.33505889257316408893E-6,
  -6.13387001076494349496E-5,
  -3.13085477492997465138E-4,
   4.97164789823116062801E-4,
   2.64347496031374526641E-2,
   1.11446150876699213025E0
};

/* x exp(-x) chin(x), inverted interval 18 to 88 */
static double C2[] = {
   8.06913408255155572081E-18,
  -2.08074168180148170312E-17,
  -5.98111329658272336816E-17,
   2.68533951085945765591E-16,
   4.52313941698904694774E-16,
  -3.10734917335299464535E-15,
  -4.42823207332531972288E-15,
   3.49639695410806959872E-14,
   6.63406731718911586609E-14,
  -3.71902448093119218395E-13,
  -1.27135418132338309016E-12,
   2.74851141935315395333E-12,
   2.33781843985453438400E-11,
   2.71436006377612442764E-11,
  -2.56600180000355990529E-10,
  -1.61021375163803438552E-9,
  -4.72543064876271773512E-9,
  -3.00095178028681682282E-9,
   7.79387474390914922337E-8,
   1.06942765566401507066E-6,
   1.59503164802313196374E-5,
   3.49592575153777996871E-4,
   1.28475387530065247392E-2,
   1.03665693917934275131E0
  };

/* Sine and cosine integrals */

void shichi (double x, double *si, double *ci) {
  double k, z, c, s, a;
  short sign;
  if (x < 0.0) {
    sign = -1;
    x = -x;
  }
  else
    sign = 0;
  if (x == 0.0) {
    *si = 0.0;
    *ci = AGN_NAN;
	return;
  }
  if (x >= 8.0) goto chb;
  z = x * x;
  /* Direct power series expansion */
  a = 1.0;
  s = 1.0;
  c = 0.0;
  k = 2.0;
  do {
    a *= z/k;
    c += a/k;
    k += 1.0;
    a /= k;
    s += a/k;
    k += 1.0;
  } while (fabs(a/s) > MACHEP);
  s *= x;
  goto done;

chb:
  if (x < 18.0) {
    a = (576.0/x - 52.0)/10.0;
    k = sun_exp(x)/x;
    s = k * chbevl(a, S1, 22);
    c = k * chbevl(a, C1, 23);
    goto done;
  }
  if (x <= 88.0) {
    a = (6336.0/x - 212.0)/70.0;
    k = sun_exp(x)/x;
    s = k * chbevl(a, S2, 23);
    c = k * chbevl(a, C2, 24);
    goto done;
  } else {
    if (sign) *si = -HUGE_VAL;
	else *si = HUGE_VAL;
    *ci = HUGE_VAL;
    return;
  }

done:
  if (sign) s = -s;
  *si = s;
  *ci = EUL + sun_log(x) + c;  /* 2.16.6 change */
}

/*							dawsn.c
 *
 *	Dawson's Integral
 *
 * SYNOPSIS:
 *
 * double x, y, dawsn();
 *
 * y = dawsn( x );
 *
 * DESCRIPTION:
 *
 * Approximates the integral
 *
 *                             x
 *                             -
 *                      2     | |        2
 *  dawsn(x)  =  exp( -x  )   |    exp( t  ) dt
 *                          | |
 *                           -
 *                           0
 *
 * Three different rational approximations are employed, for
 * the intervals 0 to 3.25; 3.25 to 6.25; and 6.25 up.
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      0,10        10000       6.9e-16     1.0e-16
 *    DEC       0,10         6000       7.4e-17     1.4e-17
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 1989, 2000 by Stephen L. Moshier */

/* Dawson's integral, interval 0 to 3.25 */
static double An[10] = {
   1.13681498971755972054E-11,
   8.49262267667473811108E-10,
   1.94434204175553054283E-8,
   9.53151741254484363489E-7,
   3.07828309874913200438E-6,
   3.52513368520288738649E-4,
  -8.50149846724410912031E-4,
   4.22618223005546594270E-2,
  -9.17480371773452345351E-2,
   9.99999999999999994612E-1,
};

static double Ad[11] = {
   2.40372073066762605484E-11,
   1.48864681368493396752E-9,
   5.21265281010541664570E-8,
   1.27258478273186970203E-6,
   2.32490249820789513991E-5,
   3.25524741826057911661E-4,
   3.48805814657162590916E-3,
   2.79448531198828973716E-2,
   1.58874241960120565368E-1,
   5.74918629489320327824E-1,
   1.00000000000000000539E0,
};

/* interval 3.25 to 6.25 */
static double Bn[11] = {
   5.08955156417900903354E-1,
  -2.44754418142697847934E-1,
   9.41512335303534411857E-2,
  -2.18711255142039025206E-2,
   3.66207612329569181322E-3,
  -4.23209114460388756528E-4,
   3.59641304793896631888E-5,
  -2.14640351719968974225E-6,
   9.10010780076391431042E-8,
  -2.40274520828250956942E-9,
   3.59233385440928410398E-11,
};

static double Bd[10] = {
  /*  1.00000000000000000000E0,*/
  -6.31839869873368190192E-1,
   2.36706788228248691528E-1,
  -5.31806367003223277662E-2,
   8.48041718586295374409E-3,
  -9.47996768486665330168E-4,
   7.81025592944552338085E-5,
  -4.55875153252442634831E-6,
   1.89100358111421846170E-7,
  -4.91324691331920606875E-9,
   7.18466403235734541950E-11,
};

/* 6.25 to infinity */
static double Cn[5] = {
  -5.90592860534773254987E-1,
   6.29235242724368800674E-1,
  -1.72858975380388136411E-1,
   1.64837047825189632310E-2,
  -4.86827613020462700845E-4,
};

static double Cd[5] = {
  /* 1.00000000000000000000E0,*/
  -2.69820057197544900361E0,
   1.73270799045947845857E0,
  -3.93708582281939493482E-1,
   3.44278924041233391079E-2,
  -9.73655226040941223894E-4,
};

double dawsn (double xx) {
  double x, y;
  int sign;
  sign = 1;
  if (xx < 0.0) {
    sign = -1;
    xx = -xx;
  }
  if (xx < 3.25) {
    x = xx*xx;
    y = xx*polevl(x, An, 9)/polevl(x, Ad, 10);
    return sign*y;
  }
  x = 1.0/(xx*xx);
  if (xx < 6.25) {
    y = 1.0/xx + x*polevl(x, Bn, 10)/(p1evl(x, Bd, 10)*xx);
    return sign*0.5*y;
  }
  if (xx > 1.0e9) return (sign * 0.5)/xx;
  /* 6.25 to infinity */
  y = 1.0/xx + x*polevl(x, Cn, 4)/(p1evl(x, Cd, 5)*xx);
  return sign*0.5*y;
}

/*							psi.c
 *
 *	Psi (digamma) function
 *
 * SYNOPSIS:
 *
 * double x, y, psi();
 *
 * y = psi( x );
 *
 * DESCRIPTION:
 *
 *              d      -
 *   psi(x)  =  -- ln | (x)
 *              dx
 *
 * is the logarithmic derivative of the gamma function.
 * For integer x,
 *                   n-1
 *                    -
 * psi(n) = -EUL  +   >  1/k.
 *                    -
 *                   k=1
 *
 * This formula is used for 0 < n <= 10.  If x is negative, it
 * is transformed to a positive argument by the reflection
 * formula  psi(1-x) = psi(x) + pi cot(pi x).
 * For general positive x, the argument is made greater than 10
 * using the recurrence  psi(x+1) = psi(x) + 1/x.
 * Then the following asymptotic expansion is applied:
 *
 *                           inf.   B
 *                            -      2k
 * psi(x) = log(x) - 1/2x -   >   -------
 *                            -        2k
 *                           k=1   2k x
 *
 * where the B2k are Bernoulli numbers.
 *
 * ACCURACY:
 *    Relative error (except absolute when |psi| < 1):
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0,30         2500       1.7e-16     2.0e-17
 *    IEEE      0,30        30000       1.3e-15     1.4e-16
 *    IEEE      -30,0       40000       1.5e-15     2.2e-16
 *
 * ERROR MESSAGES:
 *     message         condition      value returned
 * psi singularity    x integer <=0      MAXNUM
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 1992, 2000 by Stephen L. Moshier
 *
 * NOTE: This psi() implementation is more accuarate than
 * jbmath's r8_digamma().
 */
static double Apsi[] = {
   8.33333333333333333333E-2,
  -2.10927960927960927961E-2,
   7.57575757575757575758E-3,
  -4.16666666666666666667E-3,
   3.96825396825396825397E-3,
  -8.33333333333333333333E-3,
   8.33333333333333333333E-2
};

double psi (double x) {
  double p, q, nz, s, w, y, z;
  int i, n, negative;
  negative = 0;
  nz = 0.0;
  if (x <= 0.0) {
	  negative = 1;
    q = x;
    p = sun_floor(q);
    if (p == q) return AGN_NAN;
    /* Remove the zeros of tan(PI x) by subtracting the nearest integer from x */
    nz = q - p;
    if (nz != 0.5) {
		  if (nz > 0.5)	{
        p += 1.0;
        nz = q - p;
      }
		  nz = PI/sun_tan(PI*nz);  /* 3.7.2 change */
    } else
      nz = 0.0;
    x = 1.0 - x;
  }
  /* check for positive integer up to 10 */
  if ((x <= 10.0) && (x == sun_floor(x))) {
    y = 0.0;
    n = x;
    for (i=1; i < n; i++) {
      w = i;
      y += 1.0/w;
    }
    y -= EUL;
    goto done;
  }
  s = x;
  w = 0.0;
  while (s < 10.0) {
    w += 1.0/s;
    s += 1.0;
  }
  if (s < 1.0e17) {
    z = 1.0/(s*s);
    y = z*polevl(z, Apsi, 6);
  }
  else
    y = 0.0;
  y = sun_log(s) - (0.5/s) - y - w;  /* 2.16.6 change */
done:
  if (negative) y -= nz;
  return y;
}


/*							spence.c
 *
 * Dilogarithm Function
 *
 * SYNOPSIS:
 *
 * double x, y, spence();
 *
 * y = spence( x );
 *
 * DESCRIPTION:
 *
 * Computes the integral
 *
 *                    x
 *                    -
 *                   | | log t
 * spence(x)  =  -   |   ----- dt
 *                 | |   t - 1
 *                  -
 *                  1
 *
 * for x >= 0.  A rational approximation gives the integral in
 * the interval (0.5, 1.5). Transformation formulas for 1/x
 * and 1-x are employed outside the basic expansion range.
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      0,4         30000       3.9e-15     5.4e-16
 *    DEC       0,4          3000       2.5e-16     4.5e-17
 *
 * Cephes Math Library Release 2.8: June, 2000
 * Copyright 1985, 1987, 1989, 2000 by Stephen L. Moshier */

static double Asp[8] = {
  4.65128586073990045278E-5,
  7.31589045238094711071E-3,
  1.33847639578309018650E-1,
  8.79691311754530315341E-1,
  2.71149851196553469920E0,
  4.25697156008121755724E0,
  3.29771340985225106936E0,
  1.00000000000000000126E0,
};

static double Bsp[8] = {
  6.90990488912553276999E-4,
  2.54043763932544379113E-2,
  2.82974860602568089943E-1,
  1.41172597751831069617E0,
  3.63800533345137075418E0,
  5.03278880143316990390E0,
  3.54771340985225096217E0,
  9.99999999999999998740E-1,
};

double spence (double x) {
  double w, y, z;
  int flag;
  if (x < 0.0) return AGN_NAN;
  if (x == 1.0) return 0.0;
  if (x == 0.0) return ZETA2;
  if (x == HUGE_VAL) return -HUGE_VAL;  /* 6.4.10 extension */
  flag = 0;
  if (x > 2.0) {
    x = 1.0/x;
    flag |= 2;
  }
  if (x > 1.5) {
    w = (1.0/x) - 1.0;
    flag |= 2;
  } else if (x < 0.5) {
    w = -x;
    flag |= 1;
  } else {
    w = x - 1.0;
  }
  y = -w*polevl(w, Asp, 7)/polevl(w, Bsp, 7);
  if (flag & 1)
    y = ZETA2 - sun_log(x)*sun_log(1.0 - x) - y;  /* 2.16.6 change */
  if (flag & 2) {
    z = sun_log(x);  /* 2.16.6 change */
    y = -0.5*z*z - y;
  }
  return y;
}


/*							fresnl.c
 *
 *	Fresnel integral
 *
 * SYNOPSIS:
 *
 * double x, S, C;
 * void fresnl();
 *
 * fresnl( x, _&S, _&C );
 *
 * DESCRIPTION:
 *
 * Evaluates the Fresnel integrals
 *
 *           x
 *           -
 *          | |
 * C(x) =   |   cos(pi/2 t**2) dt,
 *        | |
 *         -
 *          0
 *
 *           x
 *           -
 *          | |
 * S(x) =   |   sin(pi/2 t**2) dt.
 *        | |
 *         -
 *          0
 *
 * The integrals are evaluated by a power series for x < 1.
 * For x >= 1 auxiliary functions f(x) and g(x) are employed
 * such that
 *
 * C(x) = 0.5 + f(x) sin( pi/2 x**2 ) - g(x) cos( pi/2 x**2 )
 * S(x) = 0.5 - f(x) cos( pi/2 x**2 ) - g(x) sin( pi/2 x**2 )
 *
 * ACCURACY:
 *
 *  Relative error.
 *
 * Arithmetic  function   domain     # trials      peak         rms
 *   IEEE       S(x)      0, 10       10000       2.0e-15     3.2e-16
 *   IEEE       C(x)      0, 10       10000       1.8e-15     3.3e-16
 *   DEC        S(x)      0, 10        6000       2.2e-16     3.9e-17
 *   DEC        C(x)      0, 10        5000       2.3e-16     3.9e-17
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 1989, 2000 by Stephen L. Moshier */

/* S(x) for small x */
static double sn[6] = {
  -2.99181919401019853726E3,
   7.08840045257738576863E5,
  -6.29741486205862506537E7,
   2.54890880573376359104E9,
  -4.42979518059697779103E10,
   3.18016297876567817986E11,
};

static double sd[6] = {
   2.81376268889994315696E2,
   4.55847810806532581675E4,
   5.17343888770096400730E6,
   4.19320245898111231129E8,
   2.24411795645340920940E10,
   6.07366389490084639049E11,
};

/* C(x) for small x */
static double cn[6] = {
  -4.98843114573573548651E-8,
   9.50428062829859605134E-6,
  -6.45191435683965050962E-4,
   1.88843319396703850064E-2,
  -2.05525900955013891793E-1,
   9.99999999999999998822E-1,
};

static double cd[7] = {
   3.99982968972495980367E-12,
   9.15439215774657478799E-10,
   1.25001862479598821474E-7,
   1.22262789024179030997E-5,
   8.68029542941784300606E-4,
   4.12142090722199792936E-2,
   1.00000000000000000118E0,
};

/* Auxiliary function f(x) */
static double fn[10] = {
    4.21543555043677546506E-1,
    1.43407919780758885261E-1,
    1.15220955073585758835E-2,
    3.45017939782574027900E-4,
    4.63613749287867322088E-6,
    3.05568983790257605827E-8,
    1.02304514164907233465E-10,
    1.72010743268161828879E-13,
    1.34283276233062758925E-16,
    3.76329711269987889006E-20,
};

static double fd[10] = {
    7.51586398353378947175E-1,
    1.16888925859191382142E-1,
    6.44051526508858611005E-3,
    1.55934409164153020873E-4,
    1.84627567348930545870E-6,
    1.12699224763999035261E-8,
    3.60140029589371370404E-11,
    5.88754533621578410010E-14,
    4.52001434074129701496E-17,
    1.25443237090011264384E-20,
};

/* Auxiliary function g(x) */
static double gn[11] = {
    5.04442073643383265887E-1,
    1.97102833525523411709E-1,
    1.87648584092575249293E-2,
    6.84079380915393090172E-4,
    1.15138826111884280931E-5,
    9.82852443688422223854E-8,
    4.45344415861750144738E-10,
    1.08268041139020870318E-12,
    1.37555460633261799868E-15,
    8.36354435630677421531E-19,
    1.86958710162783235106E-22,
};

static double gd[11] = {
    1.47495759925128324529E0,
    3.37748989120019970451E-1,
    2.53603741420338795122E-2,
    8.14679107184306179049E-4,
    1.27545075667729118702E-5,
    1.04314589657571990585E-7,
    4.60680728146520428211E-10,
    1.10273215066240270757E-12,
    1.38796531259578871258E-15,
    8.39158816283118707363E-19,
    1.86958710162783236342E-22,
};


void fresnl (double xxa, double *ssa, double *cca) {
  double f, g, cc, ss, c, s, t, u, x, x2;
  x = fabs(xxa);
  x2 = x*x;
  if (x2 < 2.5625) {
    t = x2*x2;
    ss = x*x2*polevl(t, sn, 5)/p1evl(t, sd, 6);
    cc = x*polevl(t, cn, 5)/polevl(t, cd, 6);
    goto done;
  }
  if (x > 36974.0) {
    cc = 0.5;
    ss = 0.5;
    goto done;
  }
  /*		Asymptotic power series auxiliary functions
   *		for large argument
   */
  x2 = x*x;
  t = PI*x2;
  u = 1.0/(t*t);
  t = 1.0/t;
  f = 1.0 - u*polevl(u, fn, 9)/p1evl(u, fd, 10);
  g = t*polevl( u, gn, 10)/p1evl(u, gd, 11);
  t = PIO2 * x2;
  c = sun_cos(t);
  s = sun_sin(t);
  t = PI*x;
  cc = 0.5 + (f*s - g*c)/t;
  ss = 0.5 - (f*c + g*s)/t;
done:
  if (xxa < 0.0) {
    cc = -cc;
    ss = -ss;
  }
  *cca = cc;
  *ssa = ss;
}

/*							cgamma
 *
 *	Complex gamma function
 *
 * SYNOPSIS:
 *
 * #include <complex.h>
 * agn_Complex x, y, cgamma();
 *
 * y = cgamma( x );
 *
 * DESCRIPTION:
 *
 * Returns complex-valued gamma function of the complex argument.
 * This variable is also filled in by the logarithmic gamma
 * function clgam().
 *
 * Arguments |x| < 18 are increased by recurrence.
 * Large arguments are handled by Stirling's formula. Large negative
 * arguments are made positive using the reflection formula.
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      -20,20      500000      2.0e-14     2.7e-15
 *    IEEE     -100,100     100000      1.4e-13     1.5e-14
 *
 * Error for arguments outside the test range will be larger
 * owing to error amplification by the exponential function.
 */
/*							clgam
 *
 *	Natural logarithm of complex gamma function
 *
 * SYNOPSIS:
 *
 * #include <complex.h>
 * agn_Complex x, y, clgam();
 *
 * y = clgam( x );
 *
 * DESCRIPTION:
 *
 * Returns the base e (2.718...) logarithm of the complex gamma
 * function of the argument.
 *
 * The logarithm of the gamma function is approximated by the
 * logarithmic version of Stirling's asymptotic formula.
 * Arguments of real part less than 14 are increased by recurrence.
 * The cosecant reflection formula is employed for arguments
 * having real part less than -14.
 *
 * Arguments greater than MAXLGM return MAXNUM and an error
 * message.  MAXLGM = 2.556348e305 for IEEE arithmetic.
 *
 * ACCURACY:
 *
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      -20,20      500000      1.4e-14     4.5e-16
 *    IEEE     -100,100     100000                  1.6e-16
 * The error criterion was relative when the function magnitude
 * was greater than one but absolute when it was less than one.
 *
 * Cephes Math Library Release 2.7:  March, 1998
 * Copyright 1984, 1998 Stephen L. Moshier */

/* Asymptotic expansion of log gamma  */
static double Alg[] = {
  -1.9175269175269175269175269175269175269175E-3,
   8.4175084175084175084175084175084175084175E-4,
  -5.9523809523809523809523809523809523809524E-4,
   7.9365079365079365079365079365079365079365E-4,
  -2.7777777777777777777777777777777777777778E-3,
   8.3333333333333333333333333333333333333333E-2
};

/* Logarithm of gamma function */

void cephes_clgam_reim (double re, double im, double *rre, double *rim) {
  double p, q, a, t1, t2, t3, t4, t5, t7, old, rrre, rrim, c[2], w[2], u[2], v[2];
  int i, cj;
  cj = 0;
  if (im < 0) {
    cj = 1;
    im = -im;
  } else if (im == 0.0) {  /* added A. Walz, Agena 0.32.4 */
    if (tools_isint(re) && re <= 0.0) {  /* 2.3.0 RC2 eCS, 2.16.6 change */
      *rre = AGN_NAN;
      *rim = AGN_NAN;
      return;
    }
    else if (re == 1.0 || re == 2.0) {
      *rre = 0.0;
      *rim = 0.0;  /* 2.14.10a fix */
      return;
    }
  }
  /* Reflection formula -z gamma(-z) gamma(z) = pi/sin(pi z) */
  if ((re < -14.0) || (im < -14.0)) {
    q = re;
    p = sun_floor(q);
    if (p == q) goto loverf;
    if (fabs(im) > 36.7) {
      /* sin z grows exponentially with Im(z).  Find ln sin(pi z)
      from |sin z| = sqrt( sin^2 x + sinh^2 y),
      arg sin z = arctan(tanh y/tan x).  */
      c[0] = PI * im - 0.6931471805599453094;
      c[1] = PI*(0.5 - q);
      cephes_clgam_reim(1.0 - re, im, &rrre, &rrim);
      c[0] = LOGPI - c[0] - rrre;
      c[1] = -c[1] + rrim;
    } else {
      /* Reduce sine arg mod pi.  */
      double t3, t5, si, co, sih, coh;
      t3 = PI*re - PI*p;
      t5 = PI*im;
      sun_sincos(t3, &si, &co);      /* 4.5.7 overflow tweak */
      sun_sinhcosh(t5, &sih, &coh);  /* 4.5.7 overflow tweak */
      u[0] = si*coh;
      u[1] = co*sih;
      if (u[0] == 0.0 && u[1] == 0) goto loverf;
      cephes_clgam_reim(1 - re, im, &w[0], &w[1]);
      t3 = sun_pow(u[0], 2.0, 1);
      t5 = sun_pow(u[1], 2.0, 1);
      c[0] = LOGPI - sun_log(t3 + t5)*0.5 - w[0];  /* 2.16.6 change, 2.17.7 tweak */
      c[1] = -sun_atan2(u[1], u[0]) - w[1];  /* 2.16.6/3.7.2 change */
      c[0] = c[0];
      c[1] = c[1] + PI*p;
    }
    goto ldone;
  }
  w[0] = 0.0; w[1] = 0.0;
  if (re < 14.0) {
    /* To satisfy Im {clgam(z)} = arg cgamma(z), accumulate
	   arg u during the recurrence.  */
    double t1, t2, t4;
    a = 0.0;
    w[0] = 1.0; w[1] = 0;
    p = 0.0;
    u[0] = re; u[1] = im;
    while (u[0] < 14.0) {
      if (u[0] == 0.0 && u[1] == 0.0) goto loverf;
      old = w[0];
      w[0] = w[0]*u[0] - w[1]*u[1];
      w[1] = old*u[1]  + w[1]*u[0];
      a = a + sun_atan2(u[1], u[0]);  /* 2.16.6/3.7.2 change */
      p += 1.0;
      u[0] = re + p;
      u[1] = im;
    }
    re = u[0]; im = u[1];
    t1 = w[0]*w[0];
    t2 = w[1]*w[1];
    t4 = sqrt(t1 + t2);
    w[0] = -sun_log(t4);  /* 2.16.6 change */
    w[1] = -a;
  }
  if (re > MAXLGM) {
loverf:
    c[0] = HUGE_VAL;
    c[1] = HUGE_VAL;
    goto ldone;
  }
  t1 = re - 0.5;
  t2 = re*re;
  t3 = im*im;
  t5 = sun_log(t2 + t3);  /* 2.16.6 change */
  t7 = sun_atan2(im, re);  /* 2.16.6/3.7.2 change */
  c[0] = 0.5*(t1*t5) - im*t7 - re + 0.91893853320467274178 + w[0];  /* 2.14.9 change, subs divs with recip muls */
  c[1] = 0.5*(im*t5) + t1*t7 - im + w[1];
  if (sun_hypot(re, im) > 1.0e8) goto ldone;  /* 2.9.8 */
  t1 = re*re;
  t2 = im*im;
  t3 = t1 - t2;
  t4 = t3*t3;
  t7 = 1/(t4 + 4.0*t1*t2);
  v[0] = t3*t7;
  v[1] = -2.0*re*im*t7;
  u[0] = Alg[0]; u[1] = 0;
  for (i=1; i < 6; i++) {
    old = u[0];
    u[0] = u[0]*v[0] - u[1]*v[1] + Alg[i];
    u[1] = old*v[1]  + u[1]*v[0];
  }
  t2 = re*re;
  t3 = im*im;
  t5 = 1/(t2 + t3);
  c[0] = c[0] + u[0]*re*t5 + u[1]*im*t5;
  c[1] = c[1] + u[1]*re*t5 - u[0]*im*t5;
ldone:
  if (cj) c[1] = -c[1];
  *rre = c[0];
  *rim = c[1];
}

void cephes_cgam_reim (double re, double im, double *rre, double *rim) {
  double t1, si, co;
  cephes_clgam_reim(re, im, rre, rim);
  t1 = sun_exp(*rre);
  sun_sincos(*rim, &si, &co);
  *rre = t1*co;
  *rim = t1*si;
}

#ifndef PROPCMPLX
complex double cephes_clgam (complex double x) {
  double p, q, a;
  complex double c, w, u, v;
  int i, cj;
  cj = 0;
  if (cimag(x) < 0) {
    cj = 1;
    x = conj(x);
  } else if (cimag(x) == 0.0) {  /* added A. Walz, Agena 0.32.4 */
    if (tools_isint(creal(x)) && creal(x) <= 0.0)  /* 2.3.0 RC2 eCS, 2.16.6 change */
      return AGN_NAN;
    else if (creal(x) == 1 || creal(x) == 2)
      return 0;
  }
  /* Reflection formula -z gamma(-z) gamma(z) = pi/sin(pi z) */
  if ((creal(x) < -14.0) || (cimag(x) < -14.0)) {
    q = creal(x);
    p = sun_floor(q);
    if (p == q) goto loverf;
    if (fabs(cimag(x)) > 36.7) {
      /* sin z grows exponentially with Im(z).  Find ln sin(pi z)
      from |sin z| = sqrt( sin^2 x + sinh^2 y),
      arg sin z = arctan(tanh y/tan x).  */
      c = PI * cimag(x) - 0.6931471805599453094 + I*PI*(0.5 - q);
      c = LOGPI - c - cephes_clgam(1.0 - x);
    } else {
      /* Reduce sine arg mod pi.  */
      u = csin(PI*(x - p));
      if (u == 0.0) goto loverf;
      w = cephes_clgam(1.0 - x);
      c = LOGPI - clog(u) - w;
      c = c + PI*p*I;
    }
    goto ldone;
  }
  w = 0.0;
  if (creal(x) < 14.0) {
    /* To satisfy Im {clgam(z)} = arg cgamma(z), accumulate
	   arg u during the recurrence.  */
    a = 0.0;
    w = 1.0;
    p = 0.0;
    u = x;
    while (creal(u) < 14.0) {
      if (u == 0.0) goto loverf;
      w *= u;
      a += carg(u);
      p += 1.0;
      u = x + p;
    }
    x = u;
    w = -sun_log(cabs(w)) - I * a;  /* 2.16.6 change */
  }
  if (creal(x) > MAXLGM) {
loverf:
    c = MAXNUM + MAXNUM * I;
    goto ldone;
  }
  c = (x - 0.5)*clog(x) - x + 0.91893853320467274178 + w;  /* 0.9189... = log( sqrt( 2*pi ) ) */
  if (cabs(x) > 1.0e8) goto ldone;
  v = 1.0/(x*x);
  u = Alg[0];
  for (i=1; i < 6; i++) {
    u = u * v + Alg[i];
  }
  c = c + u/x;
ldone:
  if (cj) c = conj(c);
  return c;
}
#endif


/*							gamma.c
 *
 *	Gamma function
 *
 * SYNOPSIS:
 *
 * double x, y, gamma();
 * extern int sgngam;
 *
 * y = gamma( x );
 *
 * DESCRIPTION:
 *
 * Returns gamma function of the argument.  The result is
 * correctly signed, and the sign (+1 or -1) is also
 * returned in a global (extern) variable named sgngam.
 * This variable is also filled in by the logarithmic gamma
 * function lgam().
 *
 * Arguments |x| <= 34 are reduced by recurrence and the function
 * approximated by a rational function of degree 6/7 in the
 * interval (2,3).  Large arguments are handled by Stirling's
 * formula. Large negative arguments are made positive using
 * a reflection formula.
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC      -34, 34      10000       1.3e-16     2.5e-17
 *    IEEE    -170,-33      20000       2.3e-15     3.3e-16
 *    IEEE     -33,  33     20000       9.4e-16     2.2e-16
 *    IEEE      33, 171.6   20000       2.3e-15     3.2e-16
 *
 * Error for arguments outside the test range will be larger
 * owing to error amplification by the exponential function.
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 1989, 1992, 2000 by Stephen L. Moshier */

static double P[] = {
  1.60119522476751861407E-4,
  1.19135147006586384913E-3,
  1.04213797561761569935E-2,
  4.76367800457137231464E-2,
  2.07448227648435975150E-1,
  4.94214826801497100753E-1,
  9.99999999999999996796E-1
};

static double Q[] = {
 -2.31581873324120129819E-5,
  5.39605580493303397842E-4,
 -4.45641913851797240494E-3,
  1.18139785222060435552E-2,
  3.58236398605498653373E-2,
 -2.34591795718243348568E-1,
  7.14304917030273074085E-2,
  1.00000000000000000320E0
};

/* Stirling's formula for the gamma function */
static double STIR[5] = {
  7.87311395793093628397E-4,
 -2.29549961613378126380E-4,
 -2.68132617805781232825E-3,
  3.47222221605458667310E-3,
  8.33333333333482257126E-2,
};


/* Gamma function computed by Stirling's formula.
 * The polynomial STIR is valid for 33 <= x <= 172.
 */
static double stirf (double x) {
  double y, w, v;
  w = 1.0/x;
  w = 1.0 + w*polevl(w, STIR, 4);
  y = sun_exp(x);
  if (x > MAXSTIR) { /* Avoid overflow in pow() */
    v = sun_pow(x, 0.5*x - 0.25, 1);
    y = v*(v/y);
  } else
    y = sun_pow(x, x - 0.5, 1)/y;
  y = 2.50662827463100050242E0*y*w;  /* SQTPI = 2.50662827463100050242E0 */
  return y;
}

double cephes_gamma (double x) {
  double p, q, z;
  int i, sgngam;
  sgngam = 1;
  if (isnan((double)x)) return x;
#ifdef __linux__
  if (isnan((double)x)) return x;  /* 2.12.0 RC 3, Raspbian won't compile otherwise */
#else
  if (isnan(x)) return x;
#endif
  if (x == HUGE_VAL) return x;
  if (x == -HUGE_VAL) return AGN_NAN;
  q = fabs(x);
  if (q > 33.0) {
    if (x < 0.0) {
      p = sun_floor(q);
      if (p == q) return AGN_NAN;
      i = p;
      if ((i & 1) == 0) sgngam = -1;
      z = q - p;
      if (z > 0.5) {
        p += 1.0;
        z = q - p;
      }
      z = q * sun_sin(PI*z);
      if (z == 0.0) return sgngam*HUGE_VAL;
      z = fabs(z);
      z = PI/(z * stirf(q));
    } else {
      z = stirf(x);
      if (isnan(z)) z = HUGE_VAL;  /* overflow with arguments > 0: added 0.33.2 */
    }
    return sgngam*z;
  }
  z = 1.0;
  while (x >= 3.0) {
    x -= 1.0;
    z *= x;
  }
  while (x < 0.0) {
	 if (x > -1.E-9) goto small;
    z /= x;
    x += 1.0;
  }
  while (x < 2.0) {
    if (x < 1.e-9) goto small;
    z /= x;
    x += 1.0;
  }
  if (x == 2.0) return z;
  x -= 2.0;
  p = polevl(x, P, 6);
  q = polevl(x, Q, 7);
  return z*p/q;
small:
  if (x == 0.0)
    return AGN_NAN;
  else
    return z/((1.0 + EULERGAMMA*x)*x);
}


/*							cgamma
 *
 *	Complex gamma function
 *
 * SYNOPSIS:
 *
 * #include <complex.h>
 * agn_Complex x, y, cgamma();
 *
 * y = cgamma( x );
 *
 * DESCRIPTION:
 *
 * Returns complex-valued gamma function of the complex argument.
 * This variable is also filled in by the logarithmic gamma
 * function clgam().
 *
 * Arguments |x| < 18 are increased by recurrence.
 * Large arguments are handled by Stirling's formula. Large negative
 * arguments are made positive using the reflection formula.
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      -20,20      500000      2.0e-14     2.7e-15
 *    IEEE     -100,100     100000      1.4e-13     1.5e-14
 *
 * Error for arguments outside the test range will be larger
 * owing to error amplification by the exponential function.
 *
 * Cephes Math Library Release 2.7:  March, 1998
 * Copyright 1984, 1998 Stephen L. Moshier */

#ifndef PROPCMPLX

#define NSTIRCG   7

/* Stirling's formula for the gamma function */
static double STIRCG[NSTIRCG] = {
 -5.92166437353693882865E-4,
  6.97281375836585777429E-5,
  7.84039221720066627474E-4,
 -2.29472093621399176955E-4,
 -2.68132716049382716049E-3,
  3.47222222222222222222E-3,
  8.33333333333333333333E-2
};

/* Gamma function computed by Stirling's formula.  */

/* static agn_Complex cstirf(x) */
complex double cstirf (complex double x) {
  complex double y, w;
  int i;
  w = 1.0/x;
  y = STIRCG[0];
  for (i=1; i < NSTIRCG; i++)
    y = y*w + STIRCG[i];
  w = 1.0 + w*y;
  y = cpow(x, x - 0.5)*cexp(-x);
  y = SQTPI*y*w;
  return y;
}


complex double cephes_cgamma (complex double x) {
  double p, q, re, im;
  complex double c, u;
  int k;
  re = creal(x);
  im = cimag(x);
  if (im == 0 && re < 0 && TRUNC(re) == re) return AGN_NAN;  /* added 0.33.2 */
  if (fabs(re) > 18.0) {
    if (re < 0.0) {
      double si, co, sih, coh;
      q = re;
      p = sun_floor(q);
      if (p == q && im == 0.0) return HUGE_VAL;
      /* c = csin( PI * x ); */
      /* Compute sin(pi x) */
      k = q - 2.0*sun_floor(0.5*q);
      q = PI*(q - p);
      p = PI*im;
      sun_sincos(q, &si, &co);      /* 4.5.7 overflow tweak */
      sun_sinhcosh(p, &sih, &coh);  /* 4.5.7 overflow tweak */
      c = si*coh + co*sih*I;        /* 3.7.2 change */
      if (k & 1) c = -c;
		/* Reflection formula. */
      c = PI/(c*cephes_cgamma(1.0 - x));
    } else {
      c = cstirf(x);
      if (tools_isnanc(c)) c = HUGE_VAL;  /* changed 2.12.0 RC 3/7.1.7 */
    }
    return c;
  }
  c = 1.0;
  p = 0.0;
  u = x;
  while (creal(u) < 18.0) {
    if ((fabs(creal(u)) < 1.0e-9) && (fabs(cimag(u)) < 1.0e-9)) goto small;
    c *= u;
    p += 1.0;
    u = x + p;
  }
  u = cstirf(u);
  return u/c;
small:
  if ((re == 0.0) && (im == 0.0))
    return AGN_NAN;
  else
    return 1.0/(((1.0 + EULERGAMMA*u)*u)*c);
}

#endif


/*							airy.c
 *
 *	Airy function
 *
 * SYNOPSIS:
 *
 * double x, ai, aip, bi, bip;
 * int airy();
 *
 * airy( x, _&ai, _&aip, _&bi, _&bip );
 *
 * DESCRIPTION:
 *
 * Solution of the differential equation
 *
 *	y"(x) = xy.
 *
 * The function returns the two independent solutions Ai, Bi
 * and their first derivatives Ai'(x), Bi'(x).
 *
 * Evaluation is by power series summation for small x,
 * by rational minimax approximations for large x.
 *
 * ACCURACY:
 *
 * Error criterion is absolute when function <= 1, relative
 * when function > 1, except * denotes relative error criterion.
 * For large negative x, the absolute error increases as x^1.5.
 * For large positive x, the relative error increases as x^1.5.
 *
 * Arithmetic  domain   function  # trials      peak         rms
 * IEEE        -10, 0     Ai        10000       1.6e-15     2.7e-16
 * IEEE          0, 10    Ai        10000       2.3e-14*    1.8e-15*
 * IEEE        -10, 0     Ai'       10000       4.6e-15     7.6e-16
 * IEEE          0, 10    Ai'       10000       1.8e-14*    1.5e-15*
 * IEEE        -10, 10    Bi        30000       4.2e-15     5.3e-16
 * IEEE        -10, 10    Bi'       30000       4.9e-15     7.3e-16
 * DEC         -10, 0     Ai         5000       1.7e-16     2.8e-17
 * DEC           0, 10    Ai         5000       2.1e-15*    1.7e-16*
 * DEC         -10, 0     Ai'        5000       4.7e-16     7.8e-17
 * DEC           0, 10    Ai'       12000       1.8e-15*    1.5e-16*
 * DEC         -10, 10    Bi        10000       5.5e-16     6.8e-17
 * DEC         -10, 10    Bi'        7000       5.3e-16     8.7e-17
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 1989, 2000 by Stephen L. Moshier */

static double c1 = 0.35502805388781723926;
static double c2 = 0.258819403792806798405;
static double sqrt3 = 1.732050807568877293527;
static double sqpii = 5.64189583547756286948E-1;

static double AN[8] = {
  3.46538101525629032477E-1,
  1.20075952739645805542E1,
  7.62796053615234516538E1,
  1.68089224934630576269E2,
  1.59756391350164413639E2,
  7.05360906840444183113E1,
  1.40264691163389668864E1,
  9.99999999999999995305E-1,
};

static double AD[8] = {
  5.67594532638770212846E-1,
  1.47562562584847203173E1,
  8.45138970141474626562E1,
  1.77318088145400459522E2,
  1.64234692871529701831E2,
  7.14778400825575695274E1,
  1.40959135607834029598E1,
  1.00000000000000000470E0,
};

static double APN[8] = {
  6.13759184814035759225E-1,
  1.47454670787755323881E1,
  8.20584123476060982430E1,
  1.71184781360976385540E2,
  1.59317847137141783523E2,
  6.99778599330103016170E1,
  1.39470856980481566958E1,
  1.00000000000000000550E0,
};

static double APD[8] = {
  3.34203677749736953049E-1,
  1.11810297306158156705E1,
  7.11727352147859965283E1,
  1.58778084372838313640E2,
  1.53206427475809220834E2,
  6.86752304592780337944E1,
  1.38498634758259442477E1,
  9.99999999999999994502E-1,
};

static double BN16[5] = {
  -2.53240795869364152689E-1,
  5.75285167332467384228E-1,
  -3.29907036873225371650E-1,
  6.44404068948199951727E-2,
  -3.82519546641336734394E-3,
};

static double BD16[5] = {
  -7.15685095054035237902E0,
  1.06039580715664694291E1,
  -5.23246636471251500874E0,
  9.57395864378383833152E-1,
  -5.50828147163549611107E-2,
};

static double BPPN[5] = {
  4.65461162774651610328E-1,
  -1.08992173800493920734E0,
  6.38800117371827987759E-1,
  -1.26844349553102907034E-1,
  7.62487844342109852105E-3,
};

static double BPPD[5] = {
  -8.70622787633159124240E0,
  1.38993162704553213172E1,
  -7.14116144616431159572E0,
  1.34008595960680518666E0,
  -7.84273211323341930448E-2,
};

static double AFN[9] = {
  -1.31696323418331795333E-1,
  -6.26456544431912369773E-1,
  -6.93158036036933542233E-1,
  -2.79779981545119124951E-1,
  -4.91900132609500318020E-2,
  -4.06265923594885404393E-3,
  -1.59276496239262096340E-4,
  -2.77649108155232920844E-6,
  -1.67787698489114633780E-8,
};

static double AFD[9] = {
  1.33560420706553243746E1,
  3.26825032795224613948E1,
  2.67367040941499554804E1,
  9.18707402907259625840E0,
  1.47529146771666414581E0,
  1.15687173795188044134E-1,
  4.40291641615211203805E-3,
  7.54720348287414296618E-5,
  4.51850092970580378464E-7,
};

static double AGN[11] = {
  1.97339932091685679179E-2,
  3.91103029615688277255E-1,
  1.06579897599595591108E0,
  9.39169229816650230044E-1,
  3.51465656105547619242E-1,
  6.33888919628925490927E-2,
  5.85804113048388458567E-3,
  2.82851600836737019778E-4,
  6.98793669997260967291E-6,
  8.11789239554389293311E-8,
  3.41551784765923618484E-10,
};

static double AGD[10] = {
  9.30892908077441974853E0,
  1.98352928718312140417E1,
  1.55646628932864612953E1,
  5.47686069422975497931E0,
  9.54293611618961883998E-1,
  8.64580826352392193095E-2,
  4.12656523824222607191E-3,
  1.01259085116509135510E-4,
  1.17166733214413521882E-6,
  4.91834570062930015649E-9,
};

static double APFN[9] = {
  1.85365624022535566142E-1,
  8.86712188052584095637E-1,
  9.87391981747398547272E-1,
  4.01241082318003734092E-1,
  7.10304926289631174579E-2,
  5.90618657995661810071E-3,
  2.33051409401776799569E-4,
  4.08718778289035454598E-6,
  2.48379932900442457853E-8,
};

static double APFD[9] = {
  1.47345854687502542552E1,
  3.75423933435489594466E1,
  3.14657751203046424330E1,
  1.09969125207298778536E1,
  1.78885054766999417817E0,
  1.41733275753662636873E-1,
  5.44066067017226003627E-3,
  9.39421290654511171663E-5,
  5.65978713036027009243E-7,
};

static double APGN[11] = {
  -3.55615429033082288335E-2,
  -6.37311518129435504426E-1,
  -1.70856738884312371053E0,
  -1.50221872117316635393E0,
  -5.63606665822102676611E-1,
  -1.02101031120216891789E-1,
  -9.48396695961445269093E-3,
  -4.60325307486780994357E-4,
  -1.14300836484517375919E-5,
  -1.33415518685547420648E-7,
  -5.63803833958893494476E-10,
};

static double APGD[11] = {
  9.85865801696130355144E0,
  2.16401867356585941885E1,
  1.73130776389749389525E1,
  6.17872175280828766327E0,
  1.08848694396321495475E0,
  9.95005543440888479402E-2,
  4.78468199683886610842E-3,
  1.18159633322838625562E-4,
  1.37480673554219441465E-6,
  5.79912514929147598821E-9,
};

int airy (double x, double *ai, double *aip, double *bi, double *bip) {
  double z, zz, t, f, g, uf, ug, k, zeta, theta;
  int domflg;
  domflg = 0;
  if (x > MAXAIRY) {
    *ai = 0;
    *aip = 0;
    *bi = MAXNUM;
    *bip = MAXNUM;
    return -1;
  }
  if (x < -2.09) {
    domflg = 15;
    t = sqrt(-x);
    zeta = -2.0*x*t/3.0;
    t = sqrt(t);
    k = sqpii/t;
    z = 1.0/zeta;
    zz = z*z;
    uf = 1.0 + zz * polevl(zz, AFN, 8)/p1evl(zz, AFD, 9);
    ug = z * polevl(zz, AGN, 10)/p1evl(zz, AGD, 10);
    theta = zeta + 0.25 * PI;
    f = sun_sin(theta);
    g = sun_cos(theta);
    *ai = k*(f*uf - g*ug);
    *bi = k*(g*uf + f*ug);
    uf = 1.0 + zz*polevl(zz, APFN, 8)/p1evl(zz, APFD, 9);
    ug = z*polevl(zz, APGN, 10)/p1evl(zz, APGD, 10);
    k = sqpii*t;
    *aip = -k * (g * uf + f * ug);
    *bip = k * (f * uf - g * ug);
    return 0;
  }
  if (x >= 2.09) {	/* cbrt(9) */
    domflg = 5;
    t = sqrt(x);
    zeta = 2.0*x*t/3.0;
    g = sun_exp(zeta);
    t = sqrt(t);
    k = 2.0*t*g;
    z = 1.0/zeta;
    f = polevl(z, AN, 7)/polevl(z, AD, 7);
    *ai = sqpii*f/k;
    k = -0.5*sqpii*t/g;
    f = polevl(z, APN, 7)/polevl(z, APD, 7);
    *aip = f*k;
    if (x > 8.3203353) {  /* zeta > 16 */
      f = z*polevl(z, BN16, 4)/p1evl(z, BD16, 5);
      k = sqpii*g;
      *bi = k*(1.0 + f)/t;
      f = z*polevl(z, BPPN, 4)/p1evl(z, BPPD, 5);
      *bip = k*t*(1.0 + f);
      return 0;
    }
  }
  f = 1.0;
  g = x;
  t = 1.0;
  uf = 1.0;
  ug = x;
  k = 1.0;
  z = x*x*x;
  while (t > MACHEP) {
    uf *= z;
    k += 1.0;
    uf /=k;
    ug *= z;
    k += 1.0;
    ug /=k;
    uf /=k;
    f += uf;
    k += 1.0;
    ug /=k;
    g += ug;
    t = fabs(uf/f);
  }
  uf = c1*f;
  ug = c2*g;
  if ((domflg & 1) == 0) *ai = uf - ug;
  if ((domflg & 2) == 0) *bi = sqrt3 * (uf + ug);
  /* the deriviative of ai */
  k = 4.0;
  uf = 0.5*x*x;  /* 2.17.7 tweak */
  ug = z/3.0;
  f = uf;
  g = 1.0 + ug;
  uf /= 3.0;
  t = 1.0;
  while (t > MACHEP) {
    uf *= z;
    ug /=k;
    k += 1.0;
    ug *= z;
    uf /=k;
    f += uf;
    k += 1.0;
    ug /=k;
    uf /=k;
    g += ug;
    k += 1.0;
    t = fabs(ug/g);
  }
  uf = c1*f;
  ug = c2*g;
  if ((domflg & 4) == 0) *aip = uf - ug;
  if ((domflg & 8) == 0) *bip = sqrt3 * (uf + ug);
  return 0;
}


 /*							zetac.c
 *
 *	Riemann zeta function
 *
 * SYNOPSIS:
 *
 * double x, y, zetac();
 *
 * y = zetac( x );
 *
 * DESCRIPTION:
 *
 *                inf.
 *                 -    -x
 *   zetac(x)  =   >   k   ,   x > 1,
 *                 -
 *                k=2
 *
 * is related to the Riemann zeta function by
 *
 *	Riemann zeta(x) = zetac(x) + 1.
 *
 * Extension of the function definition for x < 1 is implemented.
 * Zero is returned for x > log2(MAXNUM).
 *
 * An overflow error may occur for large negative x, due to the
 * gamma function in the reflection formula.
 *
 * ACCURACY:
 *
 * Tabulated values have full machine accuracy.
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      1,50        10000       9.8e-16	    1.3e-16
 *    DEC       1,50         2000       1.1e-16     1.9e-17
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 1989, 2000 by Stephen L. Moshier */

/* Riemann zeta(x) - 1 for integer arguments between 0 and 30. */
static double azetac[] = {
-1.50000000000000000000E0,
 1.70141183460469231730E38,  /* infinity */
 6.44934066848226436472E-1,
 2.02056903159594285400E-1,
 8.23232337111381915160E-2,
 3.69277551433699263314E-2,
 1.73430619844491397145E-2,
 8.34927738192282683980E-3,
 4.07735619794433937869E-3,
 2.00839282608221441785E-3,
 9.94575127818085337146E-4,
 4.94188604119464558702E-4,
 2.46086553308048298638E-4,
 1.22713347578489146752E-4,
 6.12481350587048292585E-5,
 3.05882363070204935517E-5,
 1.52822594086518717326E-5,
 7.63719763789976227360E-6,
 3.81729326499983985646E-6,
 1.90821271655393892566E-6,
 9.53962033872796113152E-7,
 4.76932986787806463117E-7,
 2.38450502727732990004E-7,
 1.19219925965311073068E-7,
 5.96081890512594796124E-8,
 2.98035035146522801861E-8,
 1.49015548283650412347E-8,
 7.45071178983542949198E-9,
 3.72533402478845705482E-9,
 1.86265972351304900640E-9,
 9.31327432419668182872E-10
};

/* 2**x (1 - 1/x) (zeta(x) - 1) = P(1/x)/Q(1/x), 1 <= x <= 10 */
static double zP[9] = {
  5.85746514569725319540E11,
  2.57534127756102572888E11,
  4.87781159567948256438E10,
  5.15399538023885770696E9,
  3.41646073514754094281E8,
  1.60837006880656492731E7,
  5.92785467342109522998E5,
  1.51129169964938823117E4,
  2.01822444485997955865E2,
};
static double zQ[8] = {
  3.90497676373371157516E11,
  5.22858235368272161797E10,
  5.64451517271280543351E9,
  3.39006746015350418834E8,
  1.79410371500126453702E7,
  5.66666825131384797029E5,
  1.60382976810944131506E4,
  1.96436237223387314144E2,
};

/* log(zeta(x) - 1 - 2**-x), 10 <= x <= 50 */
static double zA[11] = {
  8.70728567484590192539E6,
  1.76506865670346462757E8,
  2.60889506707483264896E10,
  5.29806374009894791647E11,
  2.26888156119238241487E13,
  3.31884402932705083599E14,
  5.13778997975868230192E15,
 -1.98123688133907171455E15,
 -9.92763810039983572356E16,
  7.82905376180870586444E16,
  9.26786275768927717187E16,
};

static double zB[10] = {
 -7.92625410563741062861E6,
 -1.60529969932920229676E8,
 -2.37669260975543221788E10,
 -4.80319584350455169857E11,
 -2.07820961754173320170E13,
 -2.96075404507272223680E14,
 -4.86299103694609136686E15,
  5.34589509675789930199E15,
  5.71464111092297631292E16,
 -1.79915597658676556828E16,
};

/* (1-x) (zeta(x) - 1), 0 <= x <= 1 */

static double zR[6] = {
 -3.28717474506562731748E-1,
  1.55162528742623950834E1,
 -2.48762831680821954401E2,
  1.01050368053237678329E3,
  1.26726061410235149405E4,
 -1.11578094770515181334E5,
};

static double zS[5] = {
  1.95107674914060531512E1,
  3.17710311750646984099E2,
  3.03835500874445748734E3,
  2.03665876435770579345E4,
  7.43853965136767874343E4,
};

#define MAXXL2 127

/* Riemann zeta function, minus one */

double zetac (double x) {
  int i;
  double a, b, s, w;
  if (x < 0.0) {
	  if (x < -170.6243) return AGN_NAN;  /* domain error */
    s = 1.0 - x;
    w = zetac(s);
    b = sun_sin(0.5*PI*x)*sun_pow(2.0*PI, x, 1)*cephes_gamma(s)*(1.0 + w)/PI;
    return b - 1.0;
  }
  if (x >= MAXXL2) return 0.0;	/* because first term is 2**-x */
  /* Tabulated values for integer argument */
  w = sun_floor(x);
  if (w == x) {
    i = x;
    if (i < 31) {
      return (i == 1) ? AGN_NAN : azetac[i];
    }
  }
  if (x < 1.0) {
    w = 1.0 - x;
    a = polevl(x, zR, 5)/(w*p1evl(x, zS, 5));
    return a;
  }
  if (x == 1.0) return AGN_NAN;
  if (x <= 10.0) {
    b = sun_pow(2.0, x, 1)*(x - 1.0);
    w = 1.0/x;
    s = (x * polevl(w, zP, 8))/(b*p1evl(w, zQ, 8));
    return s;
  }
  if (x <= 50.0) {
    b = sun_pow(2.0, -x, 1);
    w = polevl(x, zA, 10)/p1evl(x, zB, 10);
    w = sun_exp(w) + b;
    return w;
  }
  /* Basic sum of inverse powers */
  s = 0.0;
  a = 1.0;
  do {
    a += 2.0;
    b = sun_pow(a, -x, 1);
    s += b;
  } while (b/s > MACHEP);
  b = sun_pow(2.0, -x, 1);
  s = (s + b)/(1.0 - b);
  return s;
}


/* Taken from Stephen L. Moshier's Cephes 2.8 package, double/powi.c */

double cepowi (double x, int nn, int denormal) {
  int n, e, sign, asign, lx;
  double w, y, s;
  if (x == 0.0) {
    if (nn == 0) return 1.0;
    else if (nn < 0) return HUGE_VAL;
    else {
      if (nn & 1) return x;
      else return 0.0;
    }
  }
  if (nn == 0) return 1.0;
  if (nn == -1) return 1.0/x;
  if (x < 0.0) {
    asign = -1;
    x = -x;
  } else
    asign = 0;
  if (nn < 0) {
    sign = -1;
    n = -nn;
  } else {
    sign = 1;
    n = nn;
  }
  /* Even power will be positive. */
  if ((n & 1) == 0) asign = 0;
  /* Overflow detection, calculate approximate logarithm of answer */
  s = sun_frexp(x, &lx);
  e = (lx - 1)*n;
  if ((e == 0) || (e > 64) || (e < -64)) {
    s = (s - 7.0710678118654752e-1)/(s +  7.0710678118654752e-1);
    s = (2.9142135623730950 * s - 0.5 + lx) * nn * LOGE2;
  } else
    s = LOGE2 * e;
  if (s > CeMAXLOG) {
    y = HUGE_VAL;  /* overflow */
    goto done;
  }
  if (denormal) {
    if (s < CeMINLOG) {
      y = 0.0;
      goto done;
	  }
    /* Handle tiny denormal answer, but with less accuracy
     * since roundoff error in 1.0/x will be amplified.
     * The precise demarcation should be the gradual underflow threshold.
     */
    if ((s < (-CeMAXLOG + 2.0)) && (sign < 0)) {
      x = 1.0/x;
      sign = -sign;
    }
  } else {  /* do not produce denormal answer */
    if (s < -CeMAXLOG) return 0.0;
  }
  /* First bit of the power */
  if (n & 1) y = x;
  else y = 1.0;
  w = x;
  n >>= 1;
  while (n) {
    w *= w;	/* arg to the 2-to-the-kth power */
  	if (n & 1) y *= w;  /* if that bit is set, then include in product */
    n >>= 1;
  }
  if (sign < 0) y = 1.0/y;
done:
  if (asign) {
  /* odd power of negative number */
    if (y == 0.0) y = NEGZERO;
  else
    y = -y;
  }
  return y;
}


/*							fac.c
 *
 *	Factorial function
 *
 * SYNOPSIS:
 *
 * double y, fac();
 * int i;
 *
 * y = cephes_fac( i );
 *
 * DESCRIPTION:
 *
 * Returns factorial of i  =  1 * 2 * 3 * ... * i.
 * cephes_fac(0) = 1.0.
 *
 * Due to machine arithmetic bounds the largest value of
 * i accepted is 33 in DEC arithmetic or 170 in IEEE
 * arithmetic.  Greater values, or negative ones,
 * produce an error message and return MAXNUM.
 *
 * ACCURACY:
 *
 * For i < 34 the values are simply tabulated, and have
 * full machine accuracy.  If i > 55, cephes_fac(i) = gamma(i+1);
 * see gamma.c.
 *
 *                      Relative error:
 * arithmetic   domain      peak
 *    IEEE      0, 170    1.4e-15
 *    DEC       0, 33      1.4e-17
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 2000 by Stephen L. Moshier */

/* Factorials of integers from 0 through 33 */
static double factbl[] = {
  1.00000000000000000000E0,           /* 0 */
  1.00000000000000000000E0,           /* 1 */
  2.00000000000000000000E0,           /* 2 */
  6.00000000000000000000E0,           /* 3 */
  2.40000000000000000000E1,           /* 4 */
  1.20000000000000000000E2,           /* 5 */
  7.20000000000000000000E2,           /* 6 */
  5.04000000000000000000E3,           /* 7 */
  4.03200000000000000000E4,           /* 8 */
  3.62880000000000000000E5,           /* 9 */
  3.62880000000000000000E6,           /* 10 */
  3.99168000000000000000E7,           /* 11 */
  4.79001600000000000000E8,           /* 12 */
  6.22702080000000000000E9,           /* 13 */
  8.71782912000000000000E10,          /* 14 */
  1.30767436800000000000E12,          /* 15 */
  2.09227898880000000000E13,          /* 16 */
  3.55687428096000000000E14,          /* 17 */
  6.40237370572800000000E15,          /* 18 */
  1.21645100408832000000E17,          /* 19 */
  2.43290200817664000000E18,          /* 20 */
  5.10909421717094400000E19,          /* 21 */
  1.12400072777760768000E21,          /* 22 */
  2.58520167388849766400E22,          /* 23 */
  6.20448401733239439360E23,          /* 24 */
  1.55112100433309859840E25,          /* 25 */
  4.03291461126605635584E26,          /* 26 */
  1.0888869450418352160768E28,        /* 27 */
  3.04888344611713860501504E29,       /* 28 */
  8.841761993739701954543616E30,      /* 29 */
  2.6525285981219105863630848E32,     /* 30 */
  8.22283865417792281772556288E33,    /* 31 */
  2.6313083693369353016721801216E35,  /* 32 */
  8.68331761881188649551819440128E36  /* 33 */
};

#define MAXFAC 170  /* changed from 33 */
#define MTHRESH 34

double cephes_fac (int i) {
  double f, n;
  int j;
  if (i < 0)       return AGN_NAN;   /* Singularity */
  if (i > MAXFAC)  return HUGE_VAL;  /* overflow */
  /* Get answer from table for small i. */
  if (i < MTHRESH) return factbl[i];
  /* Use gamma function for large i. */
  if (i > 55)      return cephes_gamma(i + 1);
  /* Compute directly for intermediate i. */
  n = (double)MTHRESH;
  f = (double)MTHRESH;
  for (j=35; j <= i; j++) {
    n += 1.0;
    f *= n;
  }
  f *= factbl[MTHRESH - 1];
  return f;
}


/*							polylog.c
 *
 *	Polylogarithms
 *
 * SYNOPSIS:
 *
 * double x, y, polylog();
 * int n;
 *
 * y = polylog( n, x );
 *
 *
 * The polylogarithm of order n is defined by the series
 *
 *              inf   k
 *               -   x
 *  Li (x)  =    >   ---  .
 *    n          -     n
 *              k=1   k
 *
 *  For x = 1,
 *
 *               inf
 *                -    1
 *   Li (1)  =    >   ---   =  Riemann zeta function (n)  .
 *     n          -     n
 *               k=1   k
 *
 *  When n = 2, the function is the dilogarithm, related to Spence's integral:
 *
 *                 x                      1-x
 *                 -                        -
 *                | |  -ln(1-t)            | |  ln t
 *   Li (x)  =    |    -------- dt    =    |    ------ dt    =   spence(1-x) .
 *     2        | |       t              | |    1 - t
 *               -                        -
 *                0                        1
 *
 *  See also the program cpolylog.c for the complex polylogarithm,
 *  whose definition is extended to x > 1.
 *
 *  References:
 *
 *  Lewin, L., _Polylogarithms and Associated Functions_,
 *  North Holland, 1981.
 *
 *  Lewin, L., ed., _Structural Properties of Polylogarithms_,
 *  American Mathematical Society, 1991.
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain   n   # trials      peak         rms
 *    IEEE      0, 1     2     50000      6.2e-16     8.0e-17
 *    IEEE      0, 1     3    100000      2.5e-16     6.6e-17
 *    IEEE      0, 1     4     30000      1.7e-16     4.9e-17
 *    IEEE      0, 1     5     30000      5.1e-16     7.8e-17
 *
 * Cephes Math Library Release 2.8:  July, 1999
 * Copyright 1999 by Stephen L. Moshier */

static double polylogA4[13] = {
 3.056144922089490701751E-2,
 3.243086484162581557457E-1,
 2.877847281461875922565E-1,
 7.091267785886180663385E-2,
 6.466460072456621248630E-3,
 2.450233019296542883275E-4,
 4.031655364627704957049E-6,
 2.884169163909467997099E-8,
 8.680067002466594858347E-11,
 1.025983405866370985438E-13,
 4.233468313538272640380E-17,
 4.959422035066206902317E-21,
 1.059365867585275714599E-25,
};
static double polylogB4[12] = {
  /* 1.000000000000000000000E0, */
 2.821262403600310974875E0,
 1.780221124881327022033E0,
 3.778888211867875721773E-1,
 3.193887040074337940323E-2,
 1.161252418498096498304E-3,
 1.867362374829870620091E-5,
 1.319022779715294371091E-7,
 3.942755256555603046095E-10,
 4.644326968986396928092E-13,
 1.913336021014307074861E-16,
 2.240041814626069927477E-20,
 4.784036597230791011855E-25,
};

double polylog (int n, double x, int *flag) {
  double h, k, p, s, t, u, z;  /* xc, */
  int i, j;
  *flag = (n < -1) ? 1 : 0;  /* not implemented for n < -1 */
/*  This recurrence provides formulas for n < 2.

    d                 1
    --   Li (x)  =   ---  Li   (x)  .
    dx     n          x     n-1
*/
  if (n == -1) {
    p  = 1.0 - x;
    u = x/p;
    return (x == 1.0) ? AGN_NAN : u*u + u;
  }
  if (n == 0) {
    /* 6.4.5 fix: behave like Maple does */
    return (x == 1.0) ? AGN_NAN : x/(1.0 - x);
  }
  /* Not implemented for n < -1.
     Not defined for x > 1.  Use cpolylog if you need that.  */
  if (x > 1.0 || n < -1) return AGN_NAN;  /* domain error */
  if (n == 1) {
    return -sun_log(1.0 - x);
  }
  /* Argument +1 */
  if (x == 1.0 && n > 1) return zetac((double)n) + 1.0;
  /* Argument -1.
                        1-n
     Li (-z)  = - (1 - 2   ) Li (z)
       n                       n
   */
  if (x == -1.0 && n > 1) {
    /* Li_n(1) = zeta(n) */
    s = zetac((double) n) + 1.0;
    s *= (cepowi(2.0, 1 - n, 0) - 1.0);
    return s;
  }
/*  Inversion formula:
 *                                                   [n/2]   n-2r
 *                n                  1     n           -  log    (z)
 *  Li (-z) + (-1)  Li (-1/z)  =  - --- log (z)  +  2  >  ----------- Li  (-1)
 *    n               n              n!                -   (n - 2r)!    2r
 *                                                    r=1
 */
  if (x < -1.0 && n > 1) {
    double q, w;
    int r;
    w = sun_log(-x);  /* 3.7.2 change */
    s = 0.0;
    for (r=1; r <= n/2; r++) {
      j = 2*r;
      p = polylog(j, -1.0, flag);
      j = n - j;
      if (j == 0) {
        s += p;
        break;
      }
      q = (double) j;
      q = sun_pow(w, q, 1)*p/cephes_fac(j);
      s += q;
    }
    s *= 2.0;
    q = polylog(n, 1.0/x, flag);
    if (n & 1) q = -q;
    s -= q;
    s -= sun_pow(w, (double)n, 1)/cephes_fac(n);
    return s;
  }
  if (n == 2 && (x < 0.0 || x > 1.0)) return (spence(1.0 - x));
  /*  The power series converges slowly when x is near 1.  For n = 3, this
      identity helps:

      Li (-x/(1-x)) + Li (1-x) + Li (x)
        3               3          3
                     2                               2                 3
       = Li (1) + (pi /6) log(1-x) - (1/2) log(x) log (1-x) + (1/6) log (1-x)
           3
  */
  if (n == 3) {
    p = x*x*x;
    /* if (x > 0.8) {
      // this is a highly inaccurate branch, so we switch it off. Note that it handles 0.8 < x <= 1 only
      // and the power series below handles this case very well.
      u = sun_log(x);
      s = p/6.0;
      xc = 1.0 - x;
      s -= 0.5*u*u*sun_log(xc);
      s += ZETA2*u;
      s -= polylog(3, -xc/x, flag);
      s -= polylog(3, xc, flag);
      s += zetac(3.0);
      s += 1.0;
      return s;
    } */
    /* Power series  */
    t = p/27.0;
    t += 0.125*x*x;
    t += x;
    s = 0.0;
    k = 4.0;
    do {
      p *= x;
      h = p/(k*k*k);
      s += h;
      k += 1.0;
    } while (fabs(h/s) > 1.1e-16);
    return s + t;
  }
  if (n == 4) {
    if (x >= 0.875) {
      u = 1.0 - x;
      s = polevl(u, polylogA4, 12)/p1evl(u, polylogB4, 12);  /* 6.4.5 fix */
      s =  s*u*u - 1.202056903159594285400*u;
      s +=  1.0823232337111381915160;
      return s;
    }
    goto pseries;
  }
  if (x < 0.75) goto pseries;
/*  This expansion in powers of log(x) is especially useful when
    x is near 1.

    See also the pari gp calculator.

                      inf                  j
                       -    z(n-j) (log(x))
    polylog(n,x)  =    >   -----------------
                       -           j!
                      j=0

      where

      z(j) = Riemann zeta function (j), j != 1

                              n-1
                               -
      z(1) =  -log(-log(x)) +  >  1/k
                               -
                              k=1
  */
  z = sun_log(x);  /* 2.16.6 change */
  h = -sun_log(-z);  /* 2.16.6 change */
  for (i=1; i < n; i++) h += 1.0/i;
  p = 1.0;
  s = zetac((double)n) + 1.0;
  for (j=1; j <= n + 1; j++) {
    p *= z/j;
    if (j == n - 1)
      s += h*p;
    else
      s += p*(zetac((double)(n - j)) + 1.0);
  }
  j = n + 3;
  z = z*z;
  for (;;) {
    p = p*z/((j - 1)*j);
    h = zetac((double)(n - j)) + 1.0;
    h *= p;
    s += h;
    if (fabs(h/s) < MACHEP) break;
    j += 2;
  }
  return s;
pseries:
  p = x*x*x;
  k = 3.0;
  s = 0.0;
  do {
    p *= x;
    k += 1.0;
    h = p/cepowi(k, n, 0);
    s += h;
  } while (fabs(h/s) > MACHEP);
  s += x*x*x/cepowi(3.0, n, 0);
  s += x*x/cepowi(2.0, n, 0);
  s += x;
  return s;
}


/*							zeta.c
 *
 *	Riemann zeta function of two arguments
 *
 * SYNOPSIS:
 *
 * double x, q, y, zeta();
 *
 * y = zeta( x, q );
 *
 * DESCRIPTION:
 *
 *                 inf.
 *                  -        -x
 *   zeta(x,q)  =   >   (k+q)
 *                  -
 *                 k=0
 *
 * where x > 1 and q is not a negative integer or zero.
 * The Euler-Maclaurin summation formula is used to obtain
 * the expansion
 *
 *                n
 *                -       -x
 * zeta(x,q)  =   >  (k+q)
 *                -
 *               k=1
 *
 *           1-x                 inf.  B   x(x+1)...(x+2j)
 *      (n+q)           1         -     2j
 *  +  ---------  -  -------  +   >    --------------------
 *        x-1              x      -                   x+2j+1
 *                   2(n+q)      j=1       (2j)! (n+q)
 *
 * where the B2j are Bernoulli numbers.  Note that (see zetac.c)
 * zeta(x,1) = zetac(x) + 1.
 *
 * ACCURACY:
 *
 * REFERENCE:
 *
 * Gradshteyn, I. S., and I. M. Ryzhik, Tables of Integrals,
 * Series, and Products, p. 1073; Academic Press, 1980.
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 2000 by Stephen L. Moshier */

/* Expansion coefficients
 * for Euler-Maclaurin summation formula
 * (2k)!/B2k
 * where B2k are Bernoulli numbers
 */
static double ZETAA[] = {
   12.0,
  -720.0,
   30240.0,
  -1209600.0,
   47900160.0,
  -1.8924375803183791606e9,   /*1.307674368e12/691*/
   7.47242496e10,
  -2.950130727918164224e12,   /*1.067062284288e16/3617*/
   1.1646782814350067249e14,  /*5.109094217170944e18/43867*/
  -4.5979787224074726105e15,  /*8.028576626982912e20/174611*/
   1.8152105401943546773e17,  /*1.5511210043330985984e23/854513*/
  -7.1661652561756670113e18   /*1.6938241367317436694528e27/236364091*/
};
/* 30 Nov 86 -- error in third coefficient fixed */

double zeta (double x, double q) {
  int i;
  double a, b, k, s, t, w;
  if (x == 1.0) return HUGE_VAL;
  if (x < 1.0) return AGN_NAN;
  if (q <= 0.0) {
    if (q == sun_floor(q)) return HUGE_VAL;
    if (x != sun_floor(x)) return AGN_NAN;  /* because q^-x not defined */
  }
  /* Euler-Maclaurin summation formula
   * Permit negative q but continue sum until n+q > +9. This case should be handled by a reflection formula.
   * If q < 0 and x is an integer, there is a relation to the polygamma function. */
  s = sun_pow(q, -x, 1);
  a = q;
  i = 0;
  b = 0.0;
  while (i < 9 || a <= 9.0) {
    i += 1;
    a += 1.0;
    b = sun_pow(a, -x, 1);
    s += b;
    if (fabs(b/s) < MACHEP) goto done;
  }
  w = a;
  s += b*w/(x - 1.0);
  s -= 0.5 * b;
  a = 1.0;
  k = 0.0;
  for (i=0; i < 12; i++) {
    a *= x + k;
    b /= w;
    t = a*b/ZETAA[i];
    s = s + t;
    t = fabs(t/s);
    if (t < MACHEP) break;
    k += 1.0;
    a *= x + k;
    b /= w;
    k += 1.0;
  }
done:
  return s;
}


/* Basic sum of inverse powers */
/*
pseres:
  s = pow( q, -x );
  a = q;
  do {
    a += 2.0;
    b = pow( a, -x );
    s += b;
  } while (b/s > MACHEP);
  b = pow( 2.0, -x );
  s = (s + b)/(1.0-b);
  return s;
*/

/*							expn.c
 *
 *		Exponential integral En
 *
 * SYNOPSIS:
 *
 * int n;
 * double x, y, expn();
 *
 * y = expn( n, x );
 *
 * DESCRIPTION:
 *
 * Evaluates the exponential integral
 *
 *                 inf.
 *                   -
 *                  | |   -xt
 *                  |    e
 *      E (x)  =    |    ----  dt.
 *       n          |      n
 *                | |     t
 *                 -
 *                  1
 *
 * Both n and x must be nonnegative.
 *
 * The routine employs either a power series, a continued
 * fraction, or an asymptotic formula depending on the
 * relative values of n and x.
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0, 30        5000       2.0e-16     4.6e-17
 *    IEEE      0, 30       10000       1.7e-15     3.6e-16
 */
/*							expn.c	*/
/* Cephes Math Library Release 2.8:  June, 2000
   Copyright 1985, 2000 by Stephen L. Moshier */

double expn (int n, double x) {
  double ans, r, t, yk, xk;
  double pk, pkm1, pkm2, qk, qkm1, qkm2;
  double psi, z;
  int i, k;
  static double big = 1.44115188075855872E+17;
  if (x < 0 || n < 0) return AGN_NAN;  /* domain error */
  if (x > CeMAXLOG) return 0.0;
  if (x == 0.0) {
    if (n < 2) return AGN_NAN;  /* singularity */
  	else return 1.0/(n - 1.0);
  }
  if (n == 0) return sun_exp(-x)/x;
  /*							expn.c	*/
  /*		Expansion for large n		*/
  if (n > 5000) {
    xk = x + n;
    yk = 1.0/(xk*xk);
    t = n;
    ans = yk*t*(6.0*x*x - 8.0*t*x + t*t);
    ans = yk*(ans + t * (t - 2.0*x));
    ans = yk*(ans + t);
    ans = (ans + 1.0) * sun_exp(-x)/xk;
    goto done;
  }
  if (x > 1.0) goto cfrac;
  /*							expn.c	*/
  /*		Power series expansion		*/
  psi = -EUL - sun_log(x);
  for (i=1; i<n; i++) psi = psi + 1.0/i;
  z = -x;
  xk = 0.0;
  yk = 1.0;
  pk = 1.0 - n;
  if (n == 1) ans = 0.0;
  else ans = 1.0/pk;
  do {
    xk += 1.0;
    yk *= z/xk;
    pk += 1.0;
    if (pk != 0.0) ans += yk/pk;
    if (ans != 0.0) t = fabs(yk/ans);
    else t = 1.0;
  } while (t > MACHEP);
  k = xk;
  t = n;
  r = n - 1;
  ans = (sun_pow(z, r, 1) * psi/cephes_gamma(t)) - ans;
  goto done;
  /*							expn.c	*/
  /*		continued fraction		*/
cfrac:
  k = 1;
  pkm2 = 1.0;
  qkm2 = x;
  pkm1 = 1.0;
  qkm1 = x + n;
  ans = pkm1/qkm1;
  do {
    k += 1;
    if (k & 1) {
      yk = 1.0;
      xk = n + (k-1)*0.5;  /* 2.17.7 tweak */
    } else {
      yk = x;
      xk = k*0.5;  /* 2.17.7 tweak */
    }
    pk = pkm1 * yk  +  pkm2 * xk;
    qk = qkm1 * yk  +  qkm2 * xk;
    if (qk != 0) {
      r = pk/qk;
      t = fabs((ans - r)/r);
      ans = r;
    } else
      t = 1.0;
    pkm2 = pkm1;
    pkm1 = pk;
    qkm2 = qkm1;
    qkm1 = qk;
    if (fabs(pk) > big) {
      pkm2 /= big;
      pkm1 /= big;
      qkm2 /= big;
      qkm1 /= big;
    }
  } while (t > MACHEP);
  ans *= sun_exp(-x);
done:
  return ans;
}


/*							igam.c
 *
 *	Incomplete gamma integral
 *
 * SYNOPSIS:
 *
 * double a, x, y, igam();
 *
 * y = igam( a, x );
 *
 * DESCRIPTION:
 *
 * The function is defined by
 *
 *                           x
 *                            -
 *                   1       | |  -t  a-1
 *  igam(a,x)  =   -----     |   e   t   dt.
 *                  -      | |
 *                 | (a)    -
 *                           0
 *
 * In this implementation a must be positive and x non-negative.
 * The integral is evaluated by either a power series or
 * continued fraction expansion, depending on the relative
 * values of a and x.
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      0,30       200000       3.6e-14     2.9e-15
 *    IEEE      0,100      300000       9.9e-14     1.5e-14
 */

double igam (double a, double x) {  /* upper incomplete gamma function */
  double ans, ax, c, r;
  if (x <= 0 || a <= 0) return AGN_NAN;
  if (x > 1.0 && x > a) return 1.0 - igamc(a,x);
  /* Compute  x**a * exp(-x)/gamma(a)  */
  ax = a*sun_log(x) - x - sun_lgamma(a);  /* 2.16.6 change */
  if (ax < -CeMAXLOG) return AGN_NAN;  /* underflow */
  ax = sun_exp(ax);
  /* power series */
  r = a;
  c = 1.0;
  ans = 1.0;
  do {
    r += 1.0;
    c *= x/r;
    ans += c;
  } while (c/ans > MACHEP);
  return ans*ax/a;
}

/*							igamc()
 *
 *	Complemented incomplete gamma integral
 *
 * SYNOPSIS:
 *
 * double a, x, y, igamc();
 *
 * y = igamc( a, x );
 *
 * DESCRIPTION:
 *
 * The function is defined by
 *
 *  igamc(a,x)   =   1 - igam(a,x)
 *
 *                            inf.
 *                              -
 *                     1       | |  -t  a-1
 *               =   -----     |   e   t   dt.
 *                    -      | |
 *                   | (a)    -
 *                             x
 *
 * In this implementation both arguments must be positive.
 * The integral is evaluated by either a power series or
 * continued fraction expansion, depending on the relative
 * values of a and x.
 *
 * ACCURACY:
 *
 * Tested at random a, x.
 *                a         x                      Relative error:
 * arithmetic   domain   domain     # trials      peak         rms
 *    IEEE     0.5,100   0,100      200000       1.9e-14     1.7e-15
 *    IEEE     0.01,0.5  0,100      200000       1.4e-13     1.6e-15
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1985, 1987, 2000 by Stephen L. Moshier */

double igamc (double a, double x) {
  double ans, ax, c, yc, r, t, y, z;
  double pk, pkm1, pkm2, qk, qkm1, qkm2, big, biginv;
  big = 4.503599627370496e15;
  biginv =  2.22044604925031308085e-16;
  if ((x <= 0) || ( a <= 0))
    return AGN_NAN;
  if ((x < 1.0) || (x < a))
    return 1.0 - igam(a, x);
  ax = a*sun_log(x) - x - sun_lgamma(a);  /* 2.16.6 change */
  if (ax < -CeMAXLOG) return AGN_NAN;  /* underflow */
  ax = sun_exp(ax);
  /* continued fraction */
  y = 1.0 - a;
  z = x + y + 1.0;
  c = 0.0;
  pkm2 = 1.0;
  qkm2 = x;
  pkm1 = x + 1.0;
  qkm1 = z * x;
  ans = pkm1/qkm1;
  do {
    c += 1.0;
    y += 1.0;
    z += 2.0;
    yc = y * c;
    pk = pkm1 * z  -  pkm2 * yc;
    qk = qkm1 * z  -  qkm2 * yc;
    if (qk != 0) {
      r = pk/qk;
      t = fabs((ans - r)/r);
      ans = r;
    } else
      t = 1.0;
    pkm2 = pkm1;
    pkm1 = pk;
    qkm2 = qkm1;
    qkm1 = qk;
    if (fabs(pk) > big) {
      pkm2 *= biginv;
      pkm1 *= biginv;
      qkm2 *= biginv;
      qkm1 *= biginv;
    }
  } while (t > MACHEP);
  return ans*ax;
}


/*							incbet.c
 *
 *	Incomplete beta integral
 *
 * SYNOPSIS:
 *
 * double a, b, x, y, incbet();
 *
 * y = incbet( a, b, x );
 *
 * DESCRIPTION:
 *
 * Returns incomplete beta integral of the arguments, evaluated
 * from zero to x.  The function is defined as
 *
 *                  x
 *     -            -
 *    | (a+b)      | |  a-1     b-1
 *  -----------    |   t   (1-t)   dt.
 *   -     -     | |
 *  | (a) | (b)   -
 *                 0
 *
 * The domain of definition is 0 <= x <= 1.  In this
 * implementation a and b are restricted to positive values.
 * The integral from x to 1 may be obtained by the symmetry
 * relation
 *
 *    1 - incbet( a, b, x )  =  incbet( b, a, 1-x ).
 *
 * The integral is evaluated by a continued fraction expansion
 * or, when b*x is small, by a power series.
 *
 * ACCURACY:
 *
 * Tested at uniformly distributed random points (a,b,x) with a and b
 * in "domain" and x between 0 and 1.
 *                                        Relative error
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      0,5         10000       6.9e-15     4.5e-16
 *    IEEE      0,85       250000       2.2e-13     1.7e-14
 *    IEEE      0,1000      30000       5.3e-12     6.3e-13
 *    IEEE      0,10000    250000       9.3e-11     7.1e-12
 *    IEEE      0,100000    10000       8.7e-10     4.8e-11
 * Outputs smaller than the IEEE gradual underflow threshold
 * were excluded from these statistics.
 *
 * ERROR MESSAGES:
 *   message         condition      value returned
 * incbet domain      x<0, x>1          0.0
 * incbet underflow                     0.0
 *
 * Cephes Math Library, Release 2.8:  June, 2000
 * Copyright 1984, 1995, 2000 by Stephen L. Moshier */

/* Power series for incomplete beta integral. Use when b*x is small and x not too close to 1. */
static double pseries (double a, double b, double x) {
  double t, u, v, n, t1, z, ai;
  volatile double s, cs, ccs;
  ai = 1.0/a;
  u = (1.0 - b)*x;
  v = u/(a + 1.0);
  t1 = v;
  t = u;
  n = 2.0;
  s = 0.0;
  z = MACHEP * ai;
  cs = ccs = 0.0;
  while (fabs(v) > z) {
    u = (n - b)*x/n;
    t *= u;
    v = t/(a + n);
    s = tools_kbadd(s, v, &cs, &ccs);  /* s += v; 6.4.9 change to Kahan-Babuska summation */
    n += 1.0;
  }
  s += cs + ccs;
  s += t1;
  s += ai;
  u = a*sun_log(x);  /* 2.16.6 change */
  if ((a + b) < MAXGAM && fabs(u) < CeMAXLOG) {
    t = cephes_gamma(a + b)/(cephes_gamma(a)*cephes_gamma(b));
    s *= t*sun_pow(x, a, 1);
  } else {
    t = sun_lgamma(a + b) - sun_lgamma(a) - sun_lgamma(b) + u + sun_log(s);  /* 2.16.6 change */
    if (t < CeMINLOG) s = 0.0;
    else s = sun_exp(t);
	}
  return s;
}

/* Continued fraction expansion #1
 * for incomplete beta integral
 */
static double incbcf (double a, double b, double x) {
  double xk, pk, pkm1, pkm2, qk, qkm1, qkm2;
  double k1, k2, k3, k4, k5, k6, k7, k8;
  double r, t, ans, thresh, big, biginv;
  int n;
  big = 4.503599627370496e15;
  biginv =  2.22044604925031308085e-16;
  k1 = a;
  k2 = a + b;
  k3 = a;
  k4 = a + 1.0;
  k5 = 1.0;
  k6 = b - 1.0;
  k7 = k4;
  k8 = a + 2.0;
  pkm2 = 0.0;
  qkm2 = 1.0;
  pkm1 = 1.0;
  qkm1 = 1.0;
  ans = 1.0;
  r = 1.0;
  n = 0;
  thresh = 3.0*MACHEP;
  do {
    xk = -(x*k1*k2)/(k3*k4);
    pk = pkm1 +  pkm2*xk;
    qk = qkm1 +  qkm2*xk;
    pkm2 = pkm1;
    pkm1 = pk;
    qkm2 = qkm1;
    qkm1 = qk;
    xk = (x*k5*k6)/(k7*k8);
    pk = pkm1 +  pkm2*xk;
    qk = qkm1 +  qkm2*xk;
    pkm2 = pkm1;
    pkm1 = pk;
    qkm2 = qkm1;
    qkm1 = qk;
    if (qk != 0) r = pk/qk;
    if (r != 0) {
      t = fabs((ans - r)/r);
      ans = r;
    } else
      t = 1.0;
    if (t < thresh) goto cdone;
    k1 += 1.0;
    k2 += 1.0;
    k3 += 2.0;
    k4 += 2.0;
    k5 += 1.0;
    k6 -= 1.0;
    k7 += 2.0;
    k8 += 2.0;
    if ((fabs(qk) + fabs(pk)) > big) {
      pkm2 *= biginv;
      pkm1 *= biginv;
      qkm2 *= biginv;
      qkm1 *= biginv;
    }
    if ((fabs(qk) < biginv) || (fabs(pk) < biginv)) {
      pkm2 *= big;
      pkm1 *= big;
      qkm2 *= big;
      qkm1 *= big;
    }
  } while (++n < 300);
cdone:
  return ans;
}

/* Continued fraction expansion #2
 * for incomplete beta integral */
static double incbd (double a, double b, double x) {
  double xk, pk, pkm1, pkm2, qk, qkm1, qkm2;
  double k1, k2, k3, k4, k5, k6, k7, k8;
  double r, t, ans, z, thresh, big, biginv;
  int n;
  big = 4.503599627370496e15;
  biginv =  2.22044604925031308085e-16;
  k1 = a;
  k2 = b - 1.0;
  k3 = a;
  k4 = a + 1.0;
  k5 = 1.0;
  k6 = a + b;
  k7 = a + 1.0;;
  k8 = a + 2.0;
  pkm2 = 0.0;
  qkm2 = 1.0;
  pkm1 = 1.0;
  qkm1 = 1.0;
  z = x/(1.0 - x);
  ans = 1.0;
  r = 1.0;
  n = 0;
  thresh = 3.0 * MACHEP;
  do {
    xk = -(z*k1*k2)/(k3*k4);
    pk = pkm1 + pkm2*xk;
    qk = qkm1 + qkm2*xk;
    pkm2 = pkm1;
    pkm1 = pk;
    qkm2 = qkm1;
    qkm1 = qk;
    xk = (z*k5*k6)/(k7*k8);
    pk = pkm1 + pkm2*xk;
    qk = qkm1 + qkm2*xk;
    pkm2 = pkm1;
    pkm1 = pk;
    qkm2 = qkm1;
    qkm1 = qk;
    if (qk != 0) r = pk/qk;
    if (r != 0) {
      t = fabs((ans - r)/r);
      ans = r;
    } else
      t = 1.0;
    if (t < thresh) goto cdone;
    k1 += 1.0;
    k2 -= 1.0;
    k3 += 2.0;
    k4 += 2.0;
    k5 += 1.0;
    k6 += 1.0;
    k7 += 2.0;
    k8 += 2.0;
    if ((fabs(qk) + fabs(pk)) > big) {
      pkm2 *= biginv;
      pkm1 *= biginv;
      qkm2 *= biginv;
      qkm1 *= biginv;
    }
    if (fabs(qk) < biginv || fabs(pk) < biginv)	{
      pkm2 *= big;
      pkm1 *= big;
      qkm2 *= big;
      qkm1 *= big;
    }
  } while (++n < 300);
cdone:
  return ans;
}

double incbet (double aa, double bb, double xx) {
  double a, b, t, x, xc, w, y;
  int flag;
  if (aa <= 0.0 || bb <= 0.0)
	  goto domerr;
  if (xx <= 0.0 || xx >= 1.0) {
    if (xx == 0.0) return 0.0;
    if (xx == 1.0) return 1.0;
domerr:
    return AGN_NAN;  /* domain error */
  }
  flag = 0;
  if ((bb*xx) <= 1.0 && xx <= 0.95) {
    t = pseries(aa, bb, xx);
    goto done;
  }
  w = 1.0 - xx;
  /* Reverse a and b if x is greater than the mean. */
  if (xx > (aa/(aa + bb))) {
    flag = 1;
    a = bb;
    b = aa;
    xc = xx;
    x = w;
  } else {
    a = aa;
    b = bb;
    xc = w;
    x = xx;
  }
  if (flag == 1 && (b*x) <= 1.0 && x <= 0.95) {
    t = pseries(a, b, x);
    goto done;
  }
  /* Choose expansion for better convergence. */
  y = x*(a + b - 2.0) - (a - 1.0);
  if (y < 0.0) w = incbcf(a, b, x);
  else w = incbd(a, b, x)/xc;
  /* Multiply w by the factor
       a      b   _             _     _
      x  (1-x)   | (a+b) / ( a | (a) | (b) ) .   */
  y = a*sun_log(x);  /* 2.16.6 change */
  t = b*sun_log(xc);  /* 2.16.6 change */
  if ((a + b) < MAXGAM && fabs(y) < CeMAXLOG && fabs(t) < CeMAXLOG) {
    t = sun_pow(xc, b, 1);
    t *= sun_pow(x, a, 1);
    t /= a;
    t *= w;
    t *= cephes_gamma(a + b)/(cephes_gamma(a)*cephes_gamma(b));
    goto done;
  }
  /* Resort to logarithms.  */
  y += t + sun_lgamma(a + b) - sun_lgamma(a) - sun_lgamma(b);
  y += sun_log(w/a);  /* 2.16.6 change */
  if (y < CeMINLOG) t = 0.0;
  else t = sun_exp(y);
done:
  if (flag == 1) {
    if (t <= MACHEP) t = 1.0 - MACHEP;
    else t = 1.0 - t;
  }
  return t;
}


/*							fdtr.c
 *
 *	F distribution
 *
 * SYNOPSIS:
 *
 * int df1, df2;
 * double x, y, fdtr();
 *
 * y = fdtr( df1, df2, x );
 *
 * DESCRIPTION:
 *
 * Returns the area from zero to x under the F density
 * function (also known as Snedcor's density or the
 * variance ratio density).  This is the density
 * of x = (u1/df1)/(u2/df2), where u1 and u2 are random
 * variables having Chi square distributions with df1
 * and df2 degrees of freedom, respectively.
 *
 * The incomplete beta integral is used, according to the
 * formula
 *
 *	P(x) = incbet( df1/2, df2/2, (df1*x/(df2 + df1*x) ).
 *
 * The arguments a and b are greater than zero, and x is
 * nonnegative.
 *
 * ACCURACY:
 *
 * Tested at random points (a,b,x).
 *
 *                x     a,b                     Relative error:
 * arithmetic  domain  domain     # trials      peak         rms
 *    IEEE      0,1    0,100       100000      9.8e-15     1.7e-15
 *    IEEE      1,5    0,100       100000      6.5e-15     3.5e-16
 *    IEEE      0,1    1,10000     100000      2.2e-11     3.3e-12
 *    IEEE      1,5    1,10000     100000      1.1e-11     1.7e-13
 * See also incbet.c.
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * fdtr domain     a<0, b<0, x<0         0.0
 */
/*							fdtrc()
 *
 *	Complemented F distribution
 *
 * SYNOPSIS:
 *
 * int df1, df2;
 * double x, y, fdtrc();
 *
 * y = fdtrc( df1, df2, x );
 *
 * DESCRIPTION:
 *
 * Returns the area from x to infinity under the F density
 * function (also known as Snedcor's density or the
 * variance ratio density).
 *
 *                      inf.
 *                       -
 *              1       | |  a-1      b-1
 * 1-P(x)  =  ------    |   t    (1-t)    dt
 *            B(a,b)  | |
 *                     -
 *                      x
 *
 * The incomplete beta integral is used, according to the
 * formula
 *
 *	P(x) = incbet( df2/2, df1/2, (df2/(df2 + df1*x) ).
 *
 * ACCURACY:
 *
 * Tested at random points (a,b,x) in the indicated intervals.
 *                x     a,b                     Relative error:
 * arithmetic  domain  domain     # trials      peak         rms
 *    IEEE      0,1    1,100       100000      3.7e-14     5.9e-16
 *    IEEE      1,5    1,100       100000      8.0e-15     1.6e-15
 *    IEEE      0,1    1,10000     100000      1.8e-11     3.5e-13
 *    IEEE      1,5    1,10000     100000      2.0e-11     3.0e-12
 * See also incbet.c.
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * fdtrc domain    a<0, b<0, x<0         0.0
 */
/*							fdtri()
 *
 *	Inverse of complemented F distribution
 *
 * SYNOPSIS:
 *
 * int df1, df2;
 * double x, p, fdtri();
 *
 * x = fdtri( df1, df2, p );
 *
 * DESCRIPTION:
 *
 * Finds the F density argument x such that the integral
 * from x to infinity of the F density is equal to the
 * given probability p.
 *
 * This is accomplished using the inverse beta integral
 * function and the relations
 *
 *      z = incbi( df2/2, df1/2, p )
 *      x = df2 (1-z) / (df1 z).
 *
 * Note: the following relations hold for the inverse of
 * the uncomplemented F distribution:
 *
 *      z = incbi( df1/2, df2/2, p )
 *      x = df2 z / (df1 (1-z)).
 *
 * ACCURACY:
 *
 * Tested at random points (a,b,p).
 *
 *              a,b                     Relative error:
 * arithmetic  domain     # trials      peak         rms
 *  For p between .001 and 1:
 *    IEEE     1,100       100000      8.3e-15     4.7e-16
 *    IEEE     1,10000     100000      2.1e-11     1.4e-13
 *  For p between 10^-6 and 10^-3:
 *    IEEE     1,100        50000      1.3e-12     8.4e-15
 *    IEEE     1,10000      50000      3.0e-12     4.8e-14
 * See also fdtrc.c.
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * fdtri domain   p <= 0 or p > 1       0.0
 *                     v < 1
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 1995, 2000 by Stephen L. Moshier */

double fdtrc (int ia, int ib, double x) {
  double a, b, w;
  if (ia < 1 || ib < 1 || x < 0.0) return AGN_NAN;
  a = ia;
  b = ib;
  w = b/(b + a*x);
  return incbet(0.5*b, 0.5*a, w);
}


double fdtr (int ia, int ib, double x) {
  double a, b, w;
  if (ia < 1 || ib < 1 || x < 0.0) return AGN_NAN;
  a = ia;
  b = ib;
  w = a*x;
  w = w/(b + w);
  return incbet(0.5*a, 0.5*b, w);
}


double fdtri (int ia, int ib, double y) {
  double a, b, w, x;
  if (ia < 1 || ib < 1 || y <= 0.0 || y > 1.0) return AGN_NAN;
  a = ia;
  b = ib;
  /* Compute probability for x = 0.5.  */
  w = incbet(0.5*b, 0.5*a, 0.5);
  /* If that is greater than y, then the solution w < .5.
     Otherwise, solve at 1-y to remove cancellation in (b - b*w).  */
  if (w > y || y < 0.001)	{
    w = incbi(0.5*b, 0.5*a, y);
    x = (b - b*w)/(a*w);
  } else {
    w = incbi(0.5*a, 0.5*b, 1.0 - y);
    x = b*w/(a*(1.0 - w));
  }
  return x;
}


/*							bdtri()
 *
 * Inverse binomial distribution
 *
 * SYNOPSIS:
 *
 * int k, n;
 * double p, y, bdtri();
 *
 * p = bdtr( k, n, y );
 *
 * DESCRIPTION:
 *
 * Finds the event probability p such that the sum of the
 * terms 0 through k of the Binomial probability density
 * is equal to the given cumulative probability y.
 *
 * This is accomplished using the inverse beta integral
 * function and the relation
 *
 * 1 - p = incbi( n-k, k+1, y ).
 *
 * ACCURACY:
 *
 * Tested at random points (a,b,p).
 *
 *               a,b                     Relative error:
 * arithmetic  domain     # trials      peak         rms
 *  For p between 0.001 and 1:
 *    IEEE     0,100       100000      2.3e-14     6.4e-16
 *    IEEE     0,10000     100000      6.6e-12     1.2e-13
 *  For p between 10^-6 and 0.001:
 *    IEEE     0,100       100000      2.0e-12     1.3e-14
 *    IEEE     0,10000     100000      1.5e-12     3.2e-14
 * See also incbi.c.
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * bdtri domain     k < 0, n <= k         0.0
 *                  x < 0, x > 1
 */
double bdtri (int k, int n, double y) {
  double dk, dn, p;
  if (y < 0.0 || y > 1.0 || k < 0 || n <= k) {
    /* mtherr( "bdtri", DOMAIN ); */
    return 0.0;
  }
  dn = n - k;
  if (k == 0) {
    if (y > 0.8)
      p = -sun_expm1(sun_log1p(y - 1.0)/dn);
    else
      p = 1.0 - sun_pow(y, 1.0/dn, 1);
  } else {
  dk = k + 1;
  p = incbet(dn, dk, 0.5);
  if (p > 0.5)
    p = incbi(dk, dn, 1.0 - y);
  else
    p = 1.0 - incbi(dn, dk, y);
  }
  return p;
}


/*							ndtri.c
 *
 * Inverse of Normal distribution function
 *
 * SYNOPSIS:
 *
 * double x, y, ndtri();
 *
 * x = ndtri( y );
 *
 * DESCRIPTION:
 *
 * Returns the argument, x, for which the area under the
 * Gaussian probability density function (integrated from
 * minus infinity to x) is equal to y.
 *
 * For small arguments 0 < y < exp(-2), the program computes
 * z = sqrt( -2.0 * log(y) );  then the approximation is
 * x = z - log(z)/z  - (1/z) P(1/z) / Q(1/z).
 * There are two rational functions P/Q, one for 0 < y < exp(-32)
 * and the other for y up to exp(-2).  For larger arguments,
 * w = y - 0.5, and  x/sqrt(2pi) = w + w**3 R(w**2)/S(w**2)).
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain        # trials      peak         rms
 *    DEC      0.125, 1         5500       9.5e-17     2.1e-17
 *    DEC      6e-39, 0.135     3500       5.7e-17     1.3e-17
 *    IEEE     0.125, 1        20000       7.2e-16     1.3e-16
 *    IEEE     3e-308, 0.135   50000       4.6e-16     9.8e-17
 *
 * ERROR MESSAGES:
 *
 *   message         condition    value returned
 * ndtri domain       x <= 0        -MAXNUM
 * ndtri domain       x >= 1         MAXNUM
 *
 */

/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1989, 2000 by Stephen L. Moshier
*/

/* approximation for 0 <= |y - 0.5| <= 3/8 */
static double P0[5] = {
  -5.99633501014107895267E1,
   9.80010754185999661536E1,
  -5.66762857469070293439E1,
   1.39312609387279679503E1,
  -1.23916583867381258016E0,
};
static double Q0[8] = {
   1.95448858338141759834E0,
   4.67627912898881538453E0,
   8.63602421390890590575E1,
  -2.25462687854119370527E2,
   2.00260212380060660359E2,
  -8.20372256168333339912E1,
   1.59056225126211695515E1,
  -1.18331621121330003142E0,
};

/* Approximation for interval z = sqrt(-2 log y ) between 2 and 8
 * i.e., y between exp(-2) = .135 and exp(-32) = 1.27e-14. */
static double P1[9] = {
   4.05544892305962419923E0,
   3.15251094599893866154E1,
   5.71628192246421288162E1,
   4.40805073893200834700E1,
   1.46849561928858024014E1,
   2.18663306850790267539E0,
  -1.40256079171354495875E-1,
  -3.50424626827848203418E-2,
  -8.57456785154685413611E-4,
};

static double Q1[8] = {
   1.57799883256466749731E1,
   4.53907635128879210584E1,
   4.13172038254672030440E1,
   1.50425385692907503408E1,
   2.50464946208309415979E0,
  -1.42182922854787788574E-1,
  -3.80806407691578277194E-2,
  -9.33259480895457427372E-4,
};

/* Approximation for interval z = sqrt(-2 log y ) between 8 and 64
 * i.e., y between exp(-32) = 1.27e-14 and exp(-2048) = 3.67e-890. */
static double P2[9] = {
   3.23774891776946035970E0,
   6.91522889068984211695E0,
   3.93881025292474443415E0,
   1.33303460815807542389E0,
   2.01485389549179081538E-1,
   1.23716634817820021358E-2,
   3.01581553508235416007E-4,
   2.65806974686737550832E-6,
   6.23974539184983293730E-9,
};

static double Q2[8] = {
   6.02427039364742014255E0,
   3.67983563856160859403E0,
   1.37702099489081330271E0,
   2.16236993594496635890E-1,
   1.34204006088543189037E-2,
   3.28014464682127739104E-4,
   2.89247864745380683936E-6,
   6.79019408009981274425E-9,
};

double ndtri (double y0) {
  double x, y, z, y2, x0, x1, s2pi;
  int code;
  s2pi = 2.50662827463100050242E0;  /* sqrt(2pi) */
  if (y0 <= 0.0) return AGN_NAN;  /* domain error */
  if (y0 >= 1.0) return AGN_NAN;  /* domain error */
  code = 1;
  y = y0;
  if (y > (1.0 - 0.13533528323661269189)) { /* 0.135... = exp(-2) */
    y = 1.0 - y;
    code = 0;
  }
  if (y > 0.13533528323661269189) {
    y = y - 0.5;
    y2 = y*y;
    x = y + y*(y2*polevl(y2, P0, 4)/p1evl(y2, Q0, 8));
    x = x*s2pi;
    return x;
  }
  x = sqrt(-2.0*sun_log(y));  /* 2.16.6 change */
  x0 = x - sun_log(x)/x;  /* 2.16.6 change */
  z = 1.0/x;
  if (x < 8.0) /* y > exp(-32) = 1.2664165549e-14 */
	  x1 = z*polevl(z, P1, 8)/p1evl(z, Q1, 8);
  else
	  x1 = z*polevl(z, P2, 8)/p1evl(z, Q2, 8);
  x = x0 - x1;
  if (code != 0) x = -x;
  return x;
}


/*							incbi()
 *
 * Inverse of incomplete beta integral
 *
 * SYNOPSIS:
 *
 * double a, b, x, y, incbi();
 *
 * x = incbi( a, b, y );
 *
 * DESCRIPTION:
 *
 * Given y, the function finds x such that
 *
 *  incbet( a, b, x ) = y .
 *
 * The routine performs interval halving or Newton iterations to find the
 * root of incbet(a,b,x) - y = 0.
 *
 * ACCURACY:
 *                      Relative error:
 *                x     a,b
 * arithmetic   domain  domain  # trials    peak       rms
 *    IEEE      0,1    .5,10000   50000    5.8e-12   1.3e-13
 *    IEEE      0,1   .25,100    100000    1.8e-13   3.9e-15
 *    IEEE      0,1     0,5       50000    1.1e-12   5.5e-15
 *    VAX       0,1    .5,100     25000    3.5e-14   1.1e-15
 * With a and b constrained to half-integer or integer values:
 *    IEEE      0,1    .5,10000   50000    5.8e-12   1.1e-13
 *    IEEE      0,1    .5,100    100000    1.7e-14   7.9e-16
 * With a = .5, b constrained to half-integer or integer values:
 *    IEEE      0,1    .5,10000   10000    8.3e-11   1.0e-11
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1996, 2000 by Stephen L. Moshier */

double incbi (double aa, double bb, double yy0) {
  double a, b, y0, d, y, x, x0, x1, lgm, yp, di, dithresh, yl, yh, xt;
  int i, rflg, dir, nflg;
  i = 0;
  if (yy0 <= 0.0) return 0.0;
  if (yy0 >= 1.0) return 1.0;
  x0 = 0.0;
  yl = 0.0;
  x1 = 1.0;
  yh = 1.0;
  nflg = 0;
  if (aa <= 1.0 || bb <= 1.0) {
    dithresh = 1.0e-6;
    rflg = 0;
    a = aa;
    b = bb;
    y0 = yy0;
    x = a/(a + b);
    y = incbet(a, b, x);
    goto ihalve;
  } else {
    dithresh = 1.0e-4;
  }
  /* approximation to inverse function */
  yp = -ndtri(yy0);
  if (yy0 > 0.5) {
    rflg = 1;
    a = bb;
    b = aa;
    y0 = 1.0 - yy0;
    yp = -yp;
	} else {
    rflg = 0;
    a = aa;
    b = bb;
    y0 = yy0;
  }
  lgm = (yp*yp - 3.0)/6.0;
  x = 2.0/(1.0/(2.0*a - 1.0) + 1.0/(2.0*b - 1.0));
  d = yp*sqrt(x + lgm)/x
  	- (1.0/(2.0*b - 1.0) - 1.0/(2.0*a - 1.0))
	  * (lgm + 5.0/6.0 - 2.0/(3.0*x));
  d = 2.0 * d;
  if (d < CeMINLOG) {
    x = 1.0;
	  goto under;
  }
  /* x = a/(a + b*sun_exp(d)); */
  x = a/fma(b, sun_exp(d), a);  /* 6.4.10 improvement */
  y = incbet(a, b, x);
  yp = (y - y0)/y0;
  if (fabs(yp) < 0.2) goto newt;
  /* Resort to interval halving if not close enough. */
ihalve:
  dir = 0;
  di = 0.5;
  for (i=0; i < 100; i++) {
    if (i != 0) {
      /* x = x0 + di*(x1 - x0); */
      x = fma(di, x1 - x0, x0);  /* 6.4.10 improvement */
      if (x == 1.0) x = 1.0 - MACHEP;
      if (x == 0.0) {
        di = 0.5;
        /* x = x0 + di*(x1 - x0); */
        x = fma(di, x1 - x0, x0);  /* 6.4.10 improvement */
        if (x == 0.0) goto under;
      }
      y = incbet(a, b, x);
      yp = (x1 - x0)/(x1 + x0);
      if (fabs(yp) < dithresh) goto newt;
      yp = (y - y0)/y0;
      if (fabs(yp) < dithresh) goto newt;
    }
    if (y < y0) {
      x0 = x;
      yl = y;
      if (dir < 0) {
        dir = 0;
        di = 0.5;
      } else if (dir > 3)
        /* di = 1.0 - (1.0 - di)*(1.0 - di); */
        di = fma(-(1.0 - di), 1.0 - di, 1.0);  /* 6.4.10 improvement */
      else if (dir > 1)
        /* di = 0.5 * di + 0.5; */
        di = fma(0.5, di, 0.5);  /* 6.4.10 improvement */
      else
        di = (y0 - y)/(yh - yl);
      dir += 1;
      if (x0 > 0.75) {
        if (rflg == 1) {
          rflg = 0;
          a = aa;
          b = bb;
          y0 = yy0;
        } else {
          rflg = 1;
          a = bb;
          b = aa;
          y0 = 1.0 - yy0;
        }
        x = 1.0 - x;
        y = incbet(a, b, x);
        x0 = 0.0;
        yl = 0.0;
        x1 = 1.0;
        yh = 1.0;
        goto ihalve;
      }
    } else {
      x1 = x;
      if (rflg == 1 && x1 < MACHEP) {
        x = 0.0;
        goto done;
      }
      yh = y;
      if (dir > 0) {
        dir = 0;
        di = 0.5;
      } else if (dir < -3)
        di = di*di;
      else if (dir < -1)
        di = 0.5*di;
      else
        di = (y - y0)/(yh - yl);
      dir -= 1;
    }
	}
  /* mtherr( "incbi", PLOSS ); */
  if (x0 >= 1.0) {
  	x = 1.0 - MACHEP;
	  goto done;
  }
  if (x <= 0.0) {
under:
    x = 0.0;  /* underflow */
    goto done;
  }
newt:
  if (nflg) goto done;
  nflg = 1;
  lgm = sun_lgamma(a+b) - sun_lgamma(a) - sun_lgamma(b);
  for (i=0; i < 8; i++) {
    /* Compute the function at this point. */
    if (i != 0)
      y = incbet(a, b, x);
    if (y < yl) {
      x = x0;
      y = yl;
    } else if (y > yh) {
      x = x1;
      y = yh;
    } else if (y < y0) {
      x0 = x;
      yl = y;
    } else {
      x1 = x;
      yh = y;
    }
    if (x == 1.0 || x == 0.0) break;
    /* Compute the derivative of the function at this point. */
    /* d = (a - 1.0) * sun_log(x) + (b - 1.0) * sun_log(1.0 - x) + lgm; */  /* 2.16.6 change */
    d = fma(a - 1.0, sun_log(x), fma(b - 1.0, sun_log(1.0 - x), lgm));  /* 6.4.10 improvement */
    if (d < CeMINLOG) goto done;
    if (d > CeMAXLOG) break;
    d = sun_exp(d);
    /* Compute the step to the next approximation of x. */
    d = (y - y0)/d;
    xt = x - d;
    if (xt <= x0) {
      y = (x - x0)/(x1 - x0);
      /* xt = x0 + 0.5*y*(x - x0); */
      xt = fma(0.5*y, x - x0, x0);  /* 6.4.10 improvement */
      if (xt <= 0.0) break;
    }
    if (xt >= x1) {
      y = (x1 - x)/(x1 - x0);
      /* xt = x1 - 0.5*y*(x1 - x); */
      xt = fma(-0.5*y, x1 - x, x1);  /* 6.4.10 improvement */
      if (xt >= 1.0) break;
    }
    x = xt;
    if (fabs(d/x) < 128.0*MACHEP) goto done;
  }
  /* Did not converge.  */
  dithresh = 256.0*MACHEP;
  goto ihalve;
done:
  if (rflg) {
    if (x <= MACHEP) x = 1.0 - MACHEP;
    else x = 1.0 - x;
  }
  return x;
}


/*							exp2.c
 *
 *	Base 2 exponential function
 *
 * SYNOPSIS:
 *
 * double x, y, exp2();
 *
 * y = exp2( x );
 *
 * DESCRIPTION:
 *
 * Returns 2 raised to the x power.
 *
 * Range reduction is accomplished by separating the argument
 * into an integer k and fraction f such that
 *     x    k  f
 *    2  = 2  2.
 *
 * A Pade' form
 *
 *   1 + 2x P(x**2) / (Q(x**2) - x P(x**2) )
 *
 * approximates 2**x in the basic range [-0.5, 0.5].
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE    -1022,+1024   30000       1.8e-16     5.4e-17
 *
 * See exp.c for comments on error amplification.
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * exp underflow    x < -MAXL2        0.0
 * exp overflow     x > MAXL2         MAXNUM
 *
 * For DEC arithmetic, MAXL2 = 127.
 * For IEEE arithmetic, MAXL2 = 1024.
 */
/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1995, 2000 by Stephen L. Moshier
*/

static double PPPP[] = {
  2.30933477057345225087E-2,
  2.02020656693165307700E1,
  1.51390680115615096133E3,
};

static double QQQQ[] = {
  2.33184211722314911771E2,
  4.36821166879210612817E3,
};

#define MAXL2 1024.0
#define MINL2 -1024.0

double cephes_exp2 (double x) {
  double px, xx;
  short n;
  if (isnan(x)) return x;
  if (x > MAXL2 || x < MINL2)
    return sun_pow(2, x, 1); /* 2.8.5 change: overflow or underflow ? -> fallback */
  xx = x;	/* save x */
  /* separate into integer and fractional parts */
  px = sun_floor(x + 0.5);
  n = px;
  x = x - px;
  /* rational approximation exp2(x) = 1 +  2xP(xx)/(Q(xx) - P(xx)) where xx = x**2 */
  xx = x*x;
  px = x*polevl(xx, PPPP, 2);
  x =  px/(p1evl(xx, QQQQ, 2) - px);
  x = 1.0 + sun_ldexp(x, 1);
  /* scale by power of 2 */
  x = sun_ldexp(x, n);
  return x;
}


/*							exp10.c
 *
 *	Base 10 exponential function
 *      (Common antilogarithm)
 *
 * SYNOPSIS:
 *
 * double x, y, exp10();
 *
 * y = exp10( x );
 *
 * DESCRIPTION:
 *
 * Returns 10 raised to the x power.
 *
 * Range reduction is accomplished by expressing the argument
 * as 10**x = 2**n 10**f, with |f| < 0.5 log10(2).
 * The Pade' form
 *
 *    1 + 2x P(x**2)/( Q(x**2) - P(x**2) )
 *
 * is used to approximate 10**f.
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE     -307,+307    30000       2.2e-16     5.5e-17
 * Test result from an earlier version (2.1):
 *    DEC       -38,+38     70000       3.1e-17     7.0e-18
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * exp10 underflow    x < -MAXL10        0.0
 * exp10 overflow     x > MAXL10       MAXNUM
 *
 * DEC arithmetic: MAXL10 = 38.230809449325611792.
 * IEEE arithmetic: MAXL10 = 308.2547155599167.
 */
/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1991, 2000 by Stephen L. Moshier
Direct inquiries to 30 Frost Street, Cambridge, MA 02140
*/

static double PPP[] = {
  4.09962519798587023075E-2,
  1.17452732554344059015E1,
  4.06717289936872725516E2,
  2.39423741207388267439E3,
};

static double QQQ[] = {
  8.50936160849306532625E1,
  1.27209271178345121210E3,
  2.07960819286001865907E3,
};

static double LOG210 = 3.32192809488736234787e0;
static double LG102A = 3.01025390625000000000E-1;
static double LG102B = 4.60503898119521373889E-6;
static double MAXL10 = 308.2547155599167;

double cephes_exp10 (double x) {
  double px, xx;
  short n;
  if (isnan(x)) return x;
  if (x > MAXL10 || x < -MAXL10)
    return sun_pow(10, x, 1);  /* 2.8.5 change: overflow or underflow ? -> fallback */
  /* Express 10**x = 10**g 2**n
   *   = 10**g 10**( n log10(2) )
   *   = 10**( g + n log10(2) )
   */
  px = sun_floor(LOG210*x + 0.5);
  n = px;
  x -= px*LG102A;
  x -= px*LG102B;
  /* rational approximation for exponential of the fractional part:
   * 10**x = 1 + 2x P(x**2)/( Q(x**2) - P(x**2) ) */
  xx = x*x;
  px = x*polevl(xx, PPP, 3);
  x = px/(p1evl(xx, QQQ, 3) - px);
  x = 1.0 + sun_ldexp(x, 1);
  /* multiply by power of 2 */
  x = sun_ldexp(x, n);
  return x;
}


/*							hyp2f1.c
 *
 *	Gauss hypergeometric function   F
 *	                               2 1
 *
 * SYNOPSIS:
 *
 * double a, b, c, x, y, hyp2f1();
 *
 * y = hyp2f1( a, b, c, x );
 *
 * DESCRIPTION:
 *
 *  hyp2f1( a, b, c, x )  =   F ( a, b; c; x )
 *                           2 1
 *
 *           inf.
 *            -   a(a+1)...(a+k) b(b+1)...(b+k)   k+1
 *   =  1 +   >   -----------------------------  x   .
 *            -         c(c+1)...(c+k) (k+1)!
 *          k = 0
 *
 *  Cases addressed are
 *	Tests and escapes for negative integer a, b, or c
 *	Linear transformation if c - a or c - b negative integer
 *	Special case c = a or c = b
 *	Linear transformation for  x near +1
 *	Transformation for x < -0.5
 *	Psi function expansion if x > 0.5 and c - a - b integer
 *      Conditionally, a recurrence on c to make c-a-b > 0
 *
 * |x| > 1 is rejected.
 *
 * The parameters a, b, c are considered to be integer
 * valued if they are within 1.0e-14 of the nearest integer
 * (1.0e-13 for IEEE arithmetic).
 *
 * ACCURACY:
 *
 *               Relative error (-1 < x < 1):
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      -1,7        230000      1.2e-11     5.2e-14
 *
 * Several special cases also tested with a, b, c in
 * the range -7 to 7.
 *
 * ERROR MESSAGES:
 *
 * A "partial loss of precision" message is printed if
 * the internally estimated relative error exceeds 1^-12.
 * A "singularity" message is printed on overflow or
 * in cases not addressed (such as x < -1).
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 1992, 2000 by Stephen L. Moshier */

#define EPS 1.0e-13
#define EPS2 1.0e-10
#define ETHRESH 1.0e-12

/* Defining power series expansion of Gauss hypergeometric function */

/* double *loss estimates loss of significance */
static double hys2f1 (double a, double b, double c, double x, double *loss) {
  double f, g, h, k, m, s, u, umax;
  int i;
  i = 0;
  umax = 0.0;
  f = a;
  g = b;
  h = c;
  s = 1.0;
  u = 1.0;
  k = 0.0;
  do {
    if (fabs(h) < EPS)	{
      *loss = 1.0;
      return HUGE_VAL;
    }
    m = k + 1.0;
    u = u*((f + k)*(g + k)*x/((h + k)*m));
    s += u;
    k = fabs(u);  /* remember largest term summed */
    if (k > umax) umax = k;
    k = m;
    if (++i > 10000) { /* should never happen */
      *loss = 1.0;
      return s;
    }
  } while (fabs(u/s) > MACHEP);
  /* return estimated relative error */
  *loss = (MACHEP*umax)/fabs(s) + (MACHEP*i);
  return s;
}

/* Apply transformations for |x| near 1
 * then call the power series */
static double hyt2f1 (double a, double b, double c, double x, double *loss) {
  double p, q, r, s, t, y, d, err, err1, ax, id, d1, d2, e, y1;
  int i, aid;
  err = 0.0;
  s = 1.0 - x;
  if (x < -0.5) {
    if (b > a)
      y = sun_pow(s, -a, 1)*hys2f1(a, c - b, c, -x/s, &err);
    else
      y = sun_pow(s, -b, 1)*hys2f1(c - a, b, c, -x/s, &err);
    goto done;
  }
  d = c - a - b;
  id = sun_round(d);	/* nearest integer to d */
  if (x > 0.9) {
    if (fabs(d - id) > EPS) { /* test for integer c-a-b */
      /* Try the power series first */
      y = hys2f1(a, b, c, x, &err);
      if (err < ETHRESH) goto done;
      /* If power series fails, then apply AMS55 #15.3.6 */
      q = hys2f1(a, b, 1.0 - d, s, &err);
      q *= cephes_gamma(d)/(cephes_gamma(c - a)*cephes_gamma(c - b));
      r = sun_pow(s, d, 1)*hys2f1(c - a, c - b, d + 1.0, s, &err1);
      r *= cephes_gamma(-d)/(cephes_gamma(a)*cephes_gamma(b));
      y = q + r;
      q = fabs(q); /* estimate cancellation error */
      r = fabs(r);
      if (q > r) r = q;
      err += err1 + (MACHEP*r)/y;
      y *= cephes_gamma(c);
      goto done;
    } else {
      /* Psi function expansion, AMS55 #15.3.10, #15.3.11, #15.3.12 */
      if (id >= 0.0) {
        e = d;
        d1 = d;
        d2 = 0.0;
        aid = id;
      } else {
        e = -d;
        d1 = 0.0;
        d2 = d;
        aid = -id;
      }
      ax = sun_log(s);
      /* sum for t = 0 */
      y = psi(1.0) + psi(1.0 + e) - psi(a + d1) - psi(b + d1) - ax;
      y /= cephes_gamma(e + 1.0);
      p = (a + d1)*(b + d1)*s/cephes_gamma(e + 2.0);	/* Poch for t=1 */
      t = 1.0;
      do {
        r = psi(1.0 + t) + psi(1.0 + t + e) - psi(a + t + d1)
          - psi(b + t + d1) - ax;
        q = p*r;
        y += q;
        p *= s*(a + t + d1)/(t + 1.0);
        p *= (b + t + d1)/(t + 1.0 + e);
        t += 1.0;
      } while (fabs(q/y) > EPS);
      if (id == 0.0) {
        y *= cephes_gamma(c)/(cephes_gamma(a)*cephes_gamma(b));
        goto psidon;
      }
      y1 = 1.0;
      if (aid == 1) goto nosum;
      t = 0.0;
      p = 1.0;
      for (i=1; i < aid; i++) {
        r = 1.0 - e + t;
        p *= s*(a + t + d2)*(b + t + d2)/r;
        t += 1.0;
        p /= t;
        y1 += p;
      }
nosum:
      p = cephes_gamma(c);
      y1 *= cephes_gamma(e)*p/(cephes_gamma(a + d1)*cephes_gamma(b + d1));
      y *= p/(cephes_gamma(a + d2)*cephes_gamma(b + d2));
      if ((aid & 1) != 0) y = -y;
      q = sun_pow(s, id, 1);	/* s to the id power */
      if (id > 0.0) y *= q;
      else y1 *= q;
      y += y1;
psidon:
      goto done;
    }
  }
  /* Use defining power series if no special cases */
  y = hys2f1(a, b, c, x, &err);
done:
  *loss = err;
  return y;
}

double hyp2f1 (double a, double b, double c, double x) {
  double d, d1, d2, e, p, q, r, s, y, ax, ia, ib, ic, id, err;
  int flag, i, aid;
  err = 0.0;
  ax = fabs(x);
  s = 1.0 - x;
  d = 0;  /* to prevent compiler warnings */
  flag = 0;
  ia = sun_round(a); /* nearest integer to a */
  ib = sun_round(b);
  if (a <= 0) {
    if (fabs(a - ia) < EPS) flag |= 1;		/* a is a negative integer */
  }
  if (b <= 0) {
    if (fabs(b - ib) < EPS) flag |= 2;		/* b is a negative integer */
  }
  if (ax < 1.0) {
    if (fabs(b - c) < EPS) {		/* b = c */
      y = sun_pow(s, -a, 1);	/* s to the -a power */
      goto hypdon;
    }
    if (fabs(a - c) < EPS) {		/* a = c */
      y = sun_pow(s, -b, 1);	/* s to the -b power */
      goto hypdon;
    }
  }
  if (c <= 0.0) {
    ic = sun_round(c); 	/* nearest integer to c */
    if (fabs(c - ic) < EPS) {		/* c is a negative integer */
      /* check if termination before explosion */
      if ((flag & 1) && (ia > ic)) goto hypok;
      if ((flag & 2) && (ib > ic)) goto hypok;
      goto hypdiv;
		}
	}
  if (flag) goto hypok;			/* function is a polynomial */
  if (ax > 1.0) goto hypdiv;			/* series diverges	*/
  p = c - a;
  ia = sun_round(p); /* nearest integer to c-a */
  if ((ia <= 0.0) && (fabs(p - ia) < EPS)) flag |= 4;	/* negative int c - a */
  r = c - b;
  ib = sun_round(r); /* nearest integer to c-b */
  if ((ib <= 0.0) && (fabs(r - ib) < EPS)) flag |= 8;	/* negative int c - b */
  d = c - a - b;
  id = sun_round(d); /* nearest integer to d */
  q = fabs(d - id);
  /* Thanks to Christian Burger <BURGER@DMRHRZ11.HRZ.Uni-Marburg.DE>
   * for reporting a bug here.  */
  if (fabs(ax - 1.0) < EPS) {			/* |x| == 1.0	*/
    if (x > 0.0) {
      if (flag & 12) { /* negative int c-a or c-b */
        if (d >= 0.0) goto hypf;
        else goto hypdiv;
      }
      if (d <= 0.0) goto hypdiv;
      y = cephes_gamma(c)*cephes_gamma(d)/(cephes_gamma(p)*cephes_gamma(r));
      goto hypdon;
    }
    if (d <= -1.0) goto hypdiv;
  }
  /* Conditionally make d > 0 by recurrence on c
   * AMS55 #15.2.27 */
  if (d < 0.0) {
    /* Try the power series first */
    y = hyt2f1(a, b, c, x, &err);
    if (err < ETHRESH) goto hypdon;
    /* Apply the recurrence if power series fails */
    err = 0.0;
    aid = 2 - id;
    e = c + aid;
    d2 = hyp2f1(a, b, e, x);
    d1 = hyp2f1(a, b, e + 1.0, x);
    q = a + b + 1.0;
    for (i=0; i < aid; i++) {
      r = e - 1.0;
      y = (e*(r - (2.0*e - q)*x)*d2 + (e - a)*(e - b)*x*d1)/(e*r*s);
      e = r;
      d1 = d2;
      d2 = y;
    }
    goto hypdon;
  }
  if (flag & 12) goto hypf; /* negative integer c-a or c-b */
hypok:
  y = hyt2f1(a, b, c, x, &err);
hypdon:
  return y;
  /*	mtherr( "hyp2f1", PLOSS ); */
  /*	printf( "Estimated err = %.2e\n", err ); */
  /* The transformation for c-a or c-b negative integer
   * AMS55 #15.3.3 */
hypf:
  y = sun_pow(s, d, 1)*hys2f1(c - a, c - b, c, x, &err);
  goto hypdon;
  /* The alarm exit */
hypdiv:
  /* mtherr( "hyp2f1", OVERFLOW ); */
  return HUGE_VAL;
}


/*							hyperg.c
 *
 *	Confluent hypergeometric function
 *
 * SYNOPSIS:
 *
 * double a, b, x, y, hyp1f1();
 *
 * y = hyp1f1( a, b, x );
 *
 * DESCRIPTION:
 *
 * Computes the confluent hypergeometric function
 *
 *                          1           2
 *                       a x    a(a+1) x
 *   F ( a,b;x )  =  1 + ---- + --------- + ...
 *  1 1                  b 1!   b(b+1) 2!
 *
 * Many higher transcendental functions are special cases of
 * this power series.
 *
 * As is evident from the formula, b must not be a negative
 * integer or zero unless a is an integer with 0 >= a > b.
 *
 * The routine attempts both a direct summation of the series
 * and an asymptotic expansion.  In each case error due to
 * roundoff, cancellation, and nonconvergence is estimated.
 * The result with smaller estimated error is returned.
 *
 * ACCURACY:
 *
 * Tested at random points (a, b, x), all three variables
 * ranging from 0 to 30.
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0,30         2000       1.2e-15     1.3e-16
 qtst1:
 21800   max =  1.4200E-14   rms =  1.0841E-15  ave = -5.3640E-17
 ltstd:
 25500   max = 1.2759e-14   rms = 3.7155e-16  ave = 1.5384e-18
 *    IEEE      0,30        30000       1.8e-14     1.1e-15
 *
 * Larger errors can be observed when b is near a negative
 * integer or zero.  Certain combinations of arguments yield
 * serious cancellation error in the power series summation
 * and also are not in the region of near convergence of the
 * asymptotic series.  An error message is printed if the
 * self-estimated relative error is greater than 1.0e-12.
 */
/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1988, 2000 by Stephen L. Moshier
*/
/*							hy1f1a()	*/
/* asymptotic formula for hypergeometric function:
 *
 *        (    -a
 *  --    ( |z|
 * |  (b) ( -------- 2f0( a, 1+a-b, -1/x )
 *        (  --
 *        ( |  (b-a)
 *
 *
 *                                x    a-b                     )
 *                               e  |x|                        )
 *                             + -------- 2f0( b-a, 1-a, 1/x ) )
 *                                --                           )
 *                               |  (a)                        )
 */

/* hyp2f0(); int type; determines what converging factor to use */
static double hyp2f0 (double a, double b, double x, int type, double *err) {
  double a0, alast, t, tlast, maxt, n, an, bn, u, sum, temp;
  an = a;
  bn = b;
  a0 = 1.0e0;
  alast = 1.0e0;
  sum = 0.0;
  n = 1.0e0;
  t = 1.0e0;
  tlast = 1.0e9;
  maxt = 0.0;
  do {
    if (an == 0) goto pdone;
    if (bn == 0) goto pdone;
    u = an*(bn*x/n);
    /* check for blowup */
    temp = fabs(u);
    if ((temp > 1.0) && (maxt > (MAXNUM/temp))) goto error;
    a0 *= u;
    t = fabs(a0);
    /* terminating condition for asymptotic series */
    if (t > tlast) goto ndone;
    tlast = t;
    sum += alast;	/* the sum is one term behind */
    alast = a0;
    if (n > 200) goto ndone;
    an += 1.0e0;
    bn += 1.0e0;
    n += 1.0e0;
    if (t > maxt) maxt = t;
  } while (t > MACHEP);
pdone:	/* series converged! */
  /* estimate error due to roundoff and cancellation */
  *err = fabs(MACHEP*(n + maxt));
  alast = a0;
  goto done;
ndone:	/* series did not converge */
  /* The following "Converging factors" are supposed to improve accuracy,
   * but do not actually seem to accomplish very much. */
  n -= 1.0;
  x = 1.0/x;
  switch (type) {	/* "type" given as subroutine argument */
    case 1:
      alast *= (0.5 + (0.125 + 0.25*b - 0.5*a + 0.25*x - 0.25*n)/x);
      break;
    case 2:
      alast *= 2.0/3.0 - b + 2.0*a + x - n;
      break;
    default:
      ;
  }
  /* estimate error due to roundoff, cancellation, and nonconvergence */
  *err = MACHEP*(n + maxt) + fabs(a0);
done:
  sum += alast;
  return sum;
  /* series blew up: */
error:
  *err = HUGE_VAL;
  /* mtherr( "hyperg", TLOSS ); */
  return sum;
}

double hy1f1a (double a, double b, double x, double *err) {
  double h1, h2, t, u, temp, acanc, asum, err1, err2;
  if (x == 0) {
    acanc = 1.0;
    asum = MAXNUM;
    goto adone;
  }
  temp = sun_log(fabs(x));
  t = x + temp*(a - b);
  u = -temp*a;
  if (b > 0) {
    temp = sun_lgamma(b);
    t += temp;
    u += temp;
  }
  h1 = hyp2f0(a, a - b + 1, -1.0/x, 1, &err1);
  temp = sun_exp(u)/tools_gamma(b - a);  /* sun_exp(u)/cephes_gamma(b - a); */
  h1 *= temp;
  err1 *= temp;
  h2 = hyp2f0(b - a, 1.0 - a, 1.0/x, 2, &err2);
  temp = (a < 0) ? sun_exp(t)/cephes_gamma(a) : sun_exp(t - sun_lgamma(a));
  h2 *= temp;
  err2 *= temp;
  asum = (x < 0.0) ? h1 : h2;
  acanc = fabs(err1) + fabs(err2);
  if (b < 0) {
    temp = cephes_gamma(b);
    asum *= temp;
    acanc *= fabs(temp);
  }
  if (asum != 0.0) acanc /= fabs(asum);
  acanc *= 30.0;	/* fudge factor, since error of asymptotic formula
                   * often seems this much larger than advertised */
adone:
  *err = acanc;
  return asum;
}

/* Power series summation for confluent hypergeometric function		*/
double hy1f1p (double a, double b, double x, double *err) {
  double n, a0, t, u, temp, an, bn, maxt, pcanc;
  volatile double sum, cs, ccs;
  /* set up for power series summation */
  an = a;
  bn = b;
  a0 = 1.0;
  sum = 1.0;
  n = 1.0;
  t = 1.0;
  maxt = 0.0;
  cs = ccs = 0.0;
  while (t > MACHEP) {
    if (bn == 0) {  /* check bn first since if both	*/
      /* mtherr( "hyperg", SING ); */
      return AGN_NAN;	/* an and bn are zero it is	*/
    }
    if (an == 0) return sum + cs + ccs;	/* a singularity */
    if (n > 200) goto pdone;
    u = x*(an/(bn*n));
    /* check for blowup */
    temp = fabs(u);
    if ((temp > 1.0) && (maxt > (MAXNUM/temp))) {
      pcanc = 1.0;	/* estimate 100% error */
      goto blowup;
    }
    a0 *= u;
    sum = tools_kbadd(sum, a0, &cs, &ccs);  /* sum += a0; 6.4.9 change to Kahan-Babuska summation */
    t = fabs(a0);
    if (t > maxt) maxt = t;
    /* if ((maxt/fabs(sum)) > 1.0e17 ) {
      pcanc = 1.0;
      goto blowup;
    } */
    an += 1.0;
    bn += 1.0;
    n += 1.0;
  }
pdone:
  /* estimate error due to roundoff and cancellation */
  sum += cs + ccs;
  if (sum != 0.0) maxt /= fabs(sum);
  maxt *= MACHEP; 	/* this way avoids multiply overflow */
  pcanc = fabs(MACHEP*n + maxt);
blowup:
  *err = pcanc;
  return sum;
}

double hyp1f1 (double a, double b, double x) {
  double asum, psum, acanc, pcanc, temp;
  if (0 <= a && a < b) return hy1f1p(a, b, x, &temp);  /* 6.4.8 change, behave as Maple does */
  if (a == 0.0 && b == 0.0) return sun_exp(x);  /* 6.4.8 change, behave as Maple does */
  if (b == 0.0 && x == 0.0) return 1.0;  /* 6.4.8 change, behave as Maple does */
  pcanc = 0;
  /* See if a Kummer transformation will help */
  temp = b - a;
  if (fabs(temp) < 0.001*fabs(a))
	  return sun_exp(x)*hyp1f1(temp, b, -x);
  psum = hy1f1p(a, b, x, &pcanc);
  if (pcanc < 1.0e-15) goto done;
  /* try asymptotic series */
  asum = hy1f1a(a, b, x, &acanc);
  /* Pick the result with less estimated error */
  if (acanc < pcanc) {
	  pcanc = acanc;
  	psum = asum;
 }
done:
  if (pcanc > 1.0e-12) {  /* 6.4.8 change, behave as Maple does */
    psum = hy1f1p(a, b, x, &temp);
    if (tools_isinf(psum)) psum = AGN_NAN;
  }
  /*	mtherr( "hyperg", PLOSS ); */
  return psum;
}


/*							ellpk.c
 *
 * Complete elliptic integral of the first kind
 *
 * SYNOPSIS:
 *
 * double m1, y, ellpk();
 *
 * y = ellpk( m1 );
 *
 * DESCRIPTION:
 *
 * Approximates the integral
 *
 *            pi/2
 *             -
 *            | |
 *            |           dt
 * K(m)  =    |    ------------------
 *            |                   2
 *          | |    sqrt( 1 - m sin t )
 *           -
 *            0
 *
 * !!! where m = 1 - m1 !!!, using the approximation
 *
 *     P(x)  -  log x Q(x).
 *
 * The argument m1 is used rather than m so that the logarithmic
 * singularity at m = 1 will be shifted to the origin; this
 * preserves maximum accuracy.
 *
 * K(0) = pi/2.
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC        0,1        16000       3.5e-17     1.1e-17
 *    IEEE       0,1        30000       2.5e-16     6.8e-17
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * ellpk domain       x<0, x>1           0.0
 *
 * Cephes Math Library, Release 2.8:  June, 2000
 * Copyright 1984, 1987, 2000 by Stephen L. Moshier */

static double EllipticK_P[] = {
 1.37982864606273237150E-4,
 2.28025724005875567385E-3,
 7.97404013220415179367E-3,
 9.85821379021226008714E-3,
 6.87489687449949877925E-3,
 6.18901033637687613229E-3,
 8.79078273952743772254E-3,
 1.49380448916805252718E-2,
 3.08851465246711995998E-2,
 9.65735902811690126535E-2,
 1.38629436111989062502E0
};

static double EllipticK_Q[] = {
 2.94078955048598507511E-5,
 9.14184723865917226571E-4,
 5.94058303753167793257E-3,
 1.54850516649762399335E-2,
 2.39089602715924892727E-2,
 3.01204715227604046988E-2,
 3.73774314173823228969E-2,
 4.88280347570998239232E-2,
 7.03124996963957469739E-2,
 1.24999999999870820058E-1,
 4.99999999999999999821E-1
};

static double EllipticK_C1 = 1.3862943611198906188E0; /* log(4) */

double ellpk (double x) {
  if (x < 0.0 || x > 1.0) return AGN_NAN;
  if (x > MACHEP)
	  return (polevl(x, EllipticK_P, 10) - sun_log(x)*polevl(x, EllipticK_Q, 10));
  else
    return (x == 0.0) ? AGN_NAN : (EllipticK_C1 - 0.5*sun_log(x));
}


/*							ellik.c
 *
 * Incomplete elliptic integral of the first kind
 *
 * SYNOPSIS:
 *
 * double phi, m, y, ellik();
 *
 * y = ellik( phi, m );
 *
 * DESCRIPTION:
 *
 * Approximates the integral
 *
 *                phi
 *                 -
 *                | |
 *                |           dt
 * F(phi_\m)  =   |    ------------------
 *                |                   2
 *              | |    sqrt( 1 - m sin t )
 *               -
 *                0
 *
 * of amplitude phi and modulus m, using the arithmetic -
 * geometric mean algorithm.
 *
 * ACCURACY:
 *
 * Tested at random points with m in [0, 1] and phi as indicated.
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE     -10,10       200000      7.4e-16     1.0e-16
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 2000 by Stephen L. Moshier */

double ellik (double phi, double m) {
  double a, b, c, e, temp, t, K;
  int d, mod, sign, npio2;
  if (m == 0.0) return phi;
  a = 1.0 - m;
  if (a == 0.0) {
    if (fabs(phi) >= PIO2) return AGN_NAN;
	  return sun_log(sun_tan(0.5*(PIO2 + phi)));
	}
  npio2 = sun_floor(phi/PIO2);
  if (npio2 & 1)
    npio2 += 1;
  if (npio2) {
    K = ellpk(a);
    phi = phi - npio2*PIO2;
  } else
    K = 0.0;
  if (phi < 0.0) {
    phi = -phi;
    sign = -1;
  } else
    sign = 0;
  b = sqrt(a);
  t = sun_tan(phi);
  if (fabs(t) > 10.0) {
    /* Transform the amplitude */
    e = 1.0/(b*t);
    /* ... but avoid multiple recursions.  */
    if (fabs(e) < 10.0) {
      e = sun_atan(e);
      if (npio2 == 0) K = ellpk(a);
      temp = K - ellik(e, m);
      goto done;
    }
  }
  a = 1.0;
  c = sqrt(m);
  d = 1;
  mod = 0;
  while (fabs(c/a) > MACHEP) {
    temp = b/a;
    phi = phi + sun_atan(t*temp) + mod*PI;
    mod = (phi + PIO2)/PI;
    t = t*(1.0 + temp)/(1.0 - temp*t*t);
    c = 0.5*(a - b);
    temp = sqrt(a*b);
    a = 0.5*(a + b);
    b = temp;
    d += d;
  }
  temp = (sun_atan(t) + mod*PI)/(d*a);
done:
  if (sign < 0) temp = -temp;
  temp += npio2*K;
  return temp;
}


/*							ellpe.c
 *
 * Complete elliptic integral of the second kind
 *
 * SYNOPSIS:
 *
 * double m1, y, ellpe();
 *
 * y = ellpe( m1 );
 *
 * DESCRIPTION:
 *
 * Approximates the integral
 *
 *            pi/2
 *             -
 *            | |                 2
 * E(m)  =    |    sqrt( 1 - m sin t ) dt
 *          | |
 *           -
 *            0
 *
 * !!! Where m = 1 - m1 !!!, using the approximation
 *
 *      P(x)  -  x log x Q(x).
 *
 * Though there are no singularities, the argument m1 is used
 * rather than m for compatibility with ellpk().
 *
 * E(1) = 1; E(0) = pi/2.
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC        0, 1       13000       3.1e-17     9.4e-18
 *    IEEE       0, 1       10000       2.1e-16     7.3e-17
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * ellpe domain      x<0, x>1            0.0
 *
 * Cephes Math Library, Release 2.8: June, 2000
 * Copyright 1984, 1987, 1989, 2000 by Stephen L. Moshier */

static double ellpe_P[] = {
  1.53552577301013293365E-4,
  2.50888492163602060990E-3,
  8.68786816565889628429E-3,
  1.07350949056076193403E-2,
  7.77395492516787092951E-3,
  7.58395289413514708519E-3,
  1.15688436810574127319E-2,
  2.18317996015557253103E-2,
  5.68051945617860553470E-2,
  4.43147180560990850618E-1,
  1.00000000000000000299E0
};

static double ellpe_Q[] = {
  3.27954898576485872656E-5,
  1.00962792679356715133E-3,
  6.50609489976927491433E-3,
  1.68862163993311317300E-2,
  2.61769742454493659583E-2,
  3.34833904888224918614E-2,
  4.27180926518931511717E-2,
  5.85936634471101055642E-2,
  9.37499997197644278445E-2,
  2.49999999999888314361E-1
};

double ellpe (double x) {
  if ((x <= 0.0) || (x > 1.0)) {
    return (x == 0.0) ? 1.0 : AGN_NAN;
    /* mtherr( "ellpe", DOMAIN ); */
  }
  return polevl(x, ellpe_P, 10) - sun_log(x)*(x*polevl(x, ellpe_Q, 9));
}


/*							ellie.c
 *
 * Incomplete elliptic integral of the second kind
 *
 * SYNOPSIS:
 *
 * double phi, m, y, ellie();
 *
 * y = ellie( phi, m );
 *
 * DESCRIPTION:
 *
 * Approximates the integral
 *
 *                phi
 *                 -
 *                | |
 *                |                   2
 * E(phi_\m)  =   |    sqrt( 1 - m sin t ) dt
 *                |
 *              | |
 *               -
 *                0
 *
 * of amplitude phi and modulus m, using the arithmetic -
 * geometric mean algorithm.
 *
 * ACCURACY:
 *
 * Tested at random arguments with phi in [-10, 10] and m in
 * [0, 1].
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC        0,2         2000       1.9e-16     3.4e-17
 *    IEEE     -10,10      150000       3.3e-15     1.4e-16
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 1993, 2000 by Stephen L. Moshier */

double ellie (double phi, double m) {
  double a, b, c, e, temp, lphi, t, E;
  int d, mod, npio2, sign;
  if (m == 0.0) return phi;
  lphi = phi;
  npio2 = sun_floor(lphi/PIO2);
  if (npio2 & 1) npio2 += 1;
  lphi = lphi - npio2*PIO2;
  if (lphi < 0.0) {
    lphi = -lphi;
    sign = -1;
  } else {
    sign = 1;
  }
  a = 1.0 - m;
  E = ellpe(a);
  if (a == 0.0) {
    temp = sun_sin(lphi);
    goto done;
  }
  t = sun_tan(lphi);
  b = sqrt(a);
  /* Thanks to Brian Fitzgerald <fitzgb@mml0.meche.rpi.edu>
     for pointing out an instability near odd multiples of pi/2.  */
  if (fabs(t) > 10.0) {
  	/* Transform the amplitude */
	  e = 1.0/(b*t);
    /* ... but avoid multiple recursions.  */
	  if (fabs(e) < 10.0) {
      e = sun_atan(e);
      temp = E + m*sun_sin(lphi)*sun_sin(e) - ellie(e, m);
      goto done;
    }
  }
  c = sqrt(m);
  a = 1.0;
  d = 1;
  e = 0.0;
  mod = 0;
  while (fabs(c/a) > MACHEP) {
    temp = b/a;
    lphi = lphi + sun_atan(t*temp) + mod*PI;
    mod = (lphi + PIO2)/PI;
    t = t*(1.0 + temp)/(1.0 - temp*t*t);
    c = 0.5*(a - b);
    temp = sqrt(a*b);
    a = 0.5*(a + b);
    b = temp;
    d += d;
    e += c*sun_sin(lphi);
  }
  temp = E/ellpk(1.0 - m);
  temp *= (sun_atan(t) + mod*PI)/(d*a);
  temp += e;
done:
  if (sign < 0) temp = -temp;
  temp += npio2*E;
  return temp;
}


/*							ellpj.c
 *
 *	Jacobian Elliptic Functions
 *
 * SYNOPSIS:
 *
 * double u, m, sn, cn, dn, phi;
 * int ellpj();
 *
 * ellpj( u, m, _&sn, _&cn, _&dn, _&phi );
 *
 * DESCRIPTION:
 *
 * Evaluates the Jacobian elliptic functions sn(u|m), cn(u|m),
 * and dn(u|m) of parameter m between 0 and 1, and real
 * argument u.
 *
 * These functions are periodic, with quarter-period on the
 * real axis equal to the complete elliptic integral
 * ellpk(1.0-m).
 *
 * Relation to incomplete elliptic integral:
 * If u = ellik(phi,m), then sn(u|m) = sin(phi),
 * and cn(u|m) = cos(phi).  Phi is called the amplitude of u.
 *
 * Computation is by means of the arithmetic-geometric mean
 * algorithm, except when m is within 1e-9 of 0 or 1.  In the
 * latter case with m close to 1, the approximation applies
 * only for phi < pi/2.
 *
 * ACCURACY:
 *
 * Tested at random points with u between 0 and 10, m between
 * 0 and 1.
 *
 *            Absolute error (* = relative error):
 * arithmetic   function   # trials      peak         rms
 *    DEC       sn           1800       4.5e-16     8.7e-17
 *    IEEE      phi         10000       9.2e-16*    1.4e-16*
 *    IEEE      sn          50000       4.1e-15     4.6e-16
 *    IEEE      cn          40000       3.6e-15     4.4e-16
 *    IEEE      dn          10000       1.3e-12     1.8e-14
 *
 *  Peak error observed in consistency check using addition
 * theorem for sn(u+v) was 4e-16 (absolute).  Also tested by
 * the above relation to the incomplete elliptic integral.
 * Accuracy deteriorates when u is large.
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 2000 by Stephen L. Moshier */

int ellpj (double u, double m, double *sn, double *cn, double *dn, double *ph) {
  double ai, b, phi, t, twon, sih;
  double a[9], c[9];
  int i;
  /* Check for special cases */
  if (m < 0.0 || m > 1.0) {
    /* mtherr( "ellpj", DOMAIN ); */
    *sn = AGN_NAN;
    *cn = AGN_NAN;
    *ph = AGN_NAN;
    *dn = AGN_NAN;
    return -1;
  }
  if (m < 1.0e-9) {
    sun_sincos(u, &t, &b);  /* 4.5.7 overflow tweak */
    ai = 0.25*m*(u - t*b);
    *sn = t - ai*b;
    *cn = b + ai*t;
    *ph = u - ai;
    *dn = 1.0 - 0.5*m*t*t;
    return 0;
  }
  if (m >= 0.9999999999) {
    ai = 0.25*(1.0 - m);
    sun_sinhcosh(u, &sih, &b);  /* 4.5.7 overflow tweak */
    /* b = sun_cosh(u); */
    t = sun_tanh(u);
    phi = 1.0/b;
    twon = b*sih;
    *sn = t + ai*(twon - u)/(b*b);
    *ph = 2.0*sun_atan(sun_exp(u)) - PIO2 + ai*(twon - u)/b;
    ai *= t*phi;
    *cn = phi - ai*(twon - u);
    *dn = phi + ai*(twon + u);
    return 0;
  }
  /* A. G. M. scale */
  a[0] = 1.0;
  b = sqrt(1.0 - m);
  c[0] = sqrt(m);
  twon = 1.0;
  i = 0;
  while (fabs(c[i]/a[i]) > MACHEP) {
    if (i > 7) {
      /* mtherr( "ellpj", OVERFLOW ); */
      goto done;
    }
    ai = a[i];
    ++i;
    c[i] = 0.5*(ai - b);
    t = sqrt(ai*b);
    a[i] = 0.5*(ai + b);
    b = t;
    twon *= 2.0;
  }
done:
  /* backward recurrence */
  phi = twon * a[i] * u;
  do {
    t = c[i]*sun_sin(phi)/a[i];
    b = phi;
    phi = (sun_asin(t) + phi)/2.0;
	} while (--i);
  *sn = sun_sin(phi);
  t = sun_cos(phi);
  *cn = t;
  *dn = t/sun_cos(phi - b);
  *ph = phi;
  return 0;
}


/*							sindg.c
 *
 * Circular sine of angle in degrees
 *
 * SYNOPSIS:
 *
 * double x, y, sindg();
 *
 * y = sindg( x );
 *
 * DESCRIPTION:
 *
 * Range reduction is into intervals of 45 degrees.
 *
 * Two polynomial approximating functions are employed.
 * Between 0 and pi/4 the sine is approximated by
 *      x  +  x**3 P(x**2).
 * Between pi/4 and pi/2 the cosine is represented as
 *      1  -  x**2 P(x**2).
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain      # trials      peak         rms
 *    DEC       +-1000        3100      3.3e-17      9.0e-18
 *    IEEE      +-1000       30000      2.3e-16      5.6e-17
 *
 * ERROR MESSAGES:
 *
 *   message           condition        value returned
 * sindg total loss   x > 8.0e14 (DEC)      0.0
 *                    x > 1.0e14 (IEEE)
 */
/*							cosdg.c
 *
 * Circular cosine of angle in degrees
 *
 * SYNOPSIS:
 *
 * double x, y, cosdg();
 *
 * y = cosdg( x );
 *
 * DESCRIPTION:
 *
 * Range reduction is into intervals of 45 degrees.
 *
 * Two polynomial approximating functions are employed.
 * Between 0 and pi/4 the cosine is approximated by
 *      1  -  x**2 P(x**2).
 * Between pi/4 and pi/2 the sine is represented as
 *      x  +  x**3 P(x**2).
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain      # trials      peak         rms
 *    DEC      +-1000         3400       3.5e-17     9.1e-18
 *    IEEE     +-1000        30000       2.1e-16     5.7e-17
 *  See also sin().
 *
 *
 * Cephes Math Library Release 2.0:  April, 1987
 * Copyright 1985, 1987 by Stephen L. Moshier
 * Direct inquiries to 30 Frost Street, Cambridge, MA 02140 */

static double sincof[] = {
   1.58962301572218447952E-10,
  -2.50507477628503540135E-8,
   2.75573136213856773549E-6,
  -1.98412698295895384658E-4,
   8.33333333332211858862E-3,
  -1.66666666666666307295E-1
};

static double coscof[] = {
   1.13678171382044553091E-11,
  -2.08758833757683644217E-9,
   2.75573155429816611547E-7,
  -2.48015872936186303776E-5,
   1.38888888888806666760E-3,
  -4.16666666666666348141E-2,
   4.99999999999999999798E-1
};

static double PI180 = 1.74532925199432957692E-2; /* pi/180 */
static double lossth = 1.0e14;

double sindg (double x) {
  double y, z, zz;
  int j, sign;
  /* make argument positive but save the sign */
  sign = 1;
  if (x < 0) {
    x = -x;
    sign = -1;
  }
  if (x > lossth) {
    /* mtherr( "sindg", TLOSS ); */
    return AGN_NAN;
  }
  y = sun_floor(x/45.0); /* integer part of x/PIO4 */
  /* strip high bits of integer part to prevent integer overflow */
  z = sun_ldexp(y, -4);
  z = sun_floor(z);           /* integer part of y/8 */
  z = y - sun_ldexp(z, 4);  /* y - 16 * (y/16) */
  j = z; /* convert to integer for tests on the phase angle */
  /* map zeros to origin */
  if (j & 1) {
    j += 1;
    y += 1.0;
  }
  j = j & 07; /* octant modulo 360 degrees */
  /* reflect in x axis */
  if (j > 3) {
    sign = -sign;
    j -= 4;
  }
  z = x - y*45.0; /* x mod 45 degrees */
  z *= PI180;	/* multiply by pi/180 to convert to radians */
  zz = z*z;
  if (j == 1 || j == 2) {
    y = 1.0 - zz*polevl(zz, coscof, 6);
  } else {
    y = z + z*(zz*polevl(zz, sincof, 5));
  }
  if (sign < 0) y = -y;
  return y;
}

double cosdg (double x) {
  double y, z, zz;
  int j, sign;
  /* make argument positive */
  sign = 1;
  if (x < 0) x = -x;
  if (x > lossth) {
  	/* mtherr( "cosdg", TLOSS ); */
	  return AGN_NAN;
  }
  y = sun_floor(x/45.0);
  z = sun_ldexp(y, -4);
  z = sun_floor(z);		/* integer part of y/8 */
  z = y - sun_ldexp(z, 4);  /* y - 16 * (y/16) */
  /* integer and fractional part modulo one octant */
  j = z;
  if (j & 1) {	/* map zeros to origin */
    j += 1;
    y += 1.0;
  }
  j = j & 07;
  if (j > 3) {
    j -=4;
    sign = -sign;
  }
  if (j > 1) sign = -sign;
  z = x - y*45.0; /* x mod 45 degrees */
  z *= PI180;	/* multiply by pi/180 to convert to radians */
  zz = z*z;
  if (j == 1 || j == 2) {
    y = z + z*(zz*polevl(zz, sincof, 5));
  } else {
    y = 1.0 - zz*polevl(zz, coscof, 6);
  }
  if (sign < 0) y = -y;
  return y;
}


/*							tandg.c
 *
 *	Circular tangent of argument in degrees
 *
 * SYNOPSIS:
 *
 * double x, y, tandg();
 *
 * y = tandg( x );
 *
 * DESCRIPTION:
 *
 * Returns the circular tangent of the argument x in degrees.
 *
 * Range reduction is modulo pi/4.  A rational function
 *       x + x**3 P(x**2)/Q(x**2)
 * is employed in the basic interval [0, pi/4].
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC      0,10          8000      3.4e-17      1.2e-17
 *    IEEE     0,10         30000      3.2e-16      8.4e-17
 *
 * ERROR MESSAGES:
 *
 *   message         condition          value returned
 * tandg total loss   x > 8.0e14 (DEC)      0.0
 *                    x > 1.0e14 (IEEE)
 * tandg singularity  x = 180 k  +  90     MAXNUM
 */
/*							cotdg.c
 *
 *	Circular cotangent of argument in degrees
 *
 * SYNOPSIS:
 *
 * double x, y, cotdg();
 *
 * y = cotdg( x );
 *
 * DESCRIPTION:
 *
 * Returns the circular cotangent of the argument x in degrees.
 *
 * Range reduction is modulo pi/4.  A rational function
 *       x + x**3 P(x**2)/Q(x**2)
 * is employed in the basic interval [0, pi/4].
 *
 * ERROR MESSAGES:
 *
 *   message         condition          value returned
 * cotdg total loss   x > 8.0e14 (DEC)      0.0
 *                    x > 1.0e14 (IEEE)
 * cotdg singularity  x = 180 k            MAXNUM
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 2000 by Stephen L. Moshier */

static double tandgP[] = {
  -1.30936939181383777646E4,
   1.15351664838587416140E6,
  -1.79565251976484877988E7
};

static double tandgQ[] = {
  /* 1.00000000000000000000E0,*/
   1.36812963470692954678E4,
  -1.32089234440210967447E6,
   2.50083801823357915839E7,
  -5.38695755929454629881E7
};

static double tancot (double xx, int cotflg) {
  double x, y, z, zz;
  int j, sign;
  /* make argument positive but save the sign */
  if (xx < 0) {
    x = -xx;
    sign = -1;
  } else {
    x = xx;
    sign = 1;
	}
  if (x > lossth) {
    /* mtherr( "tandg", TLOSS ); */
    return AGN_NAN;
	}
  /* compute x mod PIO4 */
  y = sun_floor(x/45.0);
  /* strip high bits of integer part */
  z = sun_ldexp(y, -3);
  z = sun_floor(z);		/* integer part of y/8 */
  z = y - sun_ldexp(z, 3);  /* y - 16 * (y/16) */
  /* integer and fractional part modulo one octant */
  j = z;
  /* map zeros and singularities to origin */
  if (j & 1) {
    j += 1;
    y += 1.0;
  }
  z = x - y * 45.0;
  z *= PI180;
  zz = z * z;
  if (zz > 1.0e-14)
    y = z + z*(zz*polevl(zz, tandgP, 2)/p1evl(zz, tandgQ, 4));
  else
    y = z;
  if (j & 2) {
	  if (cotflg) y = -y;
    else {
      if (y != 0.0) {
        y = -1.0/y;
      } else {
			  /* mtherr( "tandg", SING ); */
			  y = AGN_NAN;
      }
    }
  } else {
    if (cotflg) {
      if (y != 0.0) y = 1.0/y;
      else {
        /* mtherr( "cotdg", SING ); */
        y = AGN_NAN;
      }
    }
  }
  if (sign < 0) y = -y;
  return y;
}

double tandg (double x) {
  return tancot(x, 0);
}

double cotdg (double x) {
  return tancot(x, 1);
}


/*							i0.c
 *
 *	Modified Bessel function of order zero
 *
 * SYNOPSIS:
 *
 * double x, y, i0();
 *
 * y = i0( x );
 *
 * DESCRIPTION:
 *
 * Returns modified Bessel function of order zero of the
 * argument.
 *
 * The function is defined as i0(x) = j0( ix ).
 *
 * The range is partitioned into the two intervals [0,8] and
 * (8, infinity).  Chebyshev polynomial expansions are employed
 * in each interval.
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0,30         6000       8.2e-17     1.9e-17
 *    IEEE      0,30        30000       5.8e-16     1.4e-16
 */
/*							i0e.c
 *
 *	Modified Bessel function of order zero,
 *	exponentially scaled
 *
 * SYNOPSIS:
 *
 * double x, y, i0e();
 *
 * y = i0e( x );
 *
 * DESCRIPTION:
 *
 * Returns exponentially scaled modified Bessel function
 * of order zero of the argument.
 *
 * The function is defined as i0e(x) = exp(-|x|) j0( ix ).
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      0,30        30000       5.4e-16     1.2e-16
 * See i0().
 *
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1987, 2000 by Stephen L. Moshier */

/* Chebyshev coefficients for exp(-x) I0(x)
 * in the interval [0,8].
 *
 * lim(x->0){ exp(-x) I0(x) } = 1.
 */

static double i0A[] = {
-4.41534164647933937950E-18,
 3.33079451882223809783E-17,
-2.43127984654795469359E-16,
 1.71539128555513303061E-15,
-1.16853328779934516808E-14,
 7.67618549860493561688E-14,
-4.85644678311192946090E-13,
 2.95505266312963983461E-12,
-1.72682629144155570723E-11,
 9.67580903537323691224E-11,
-5.18979560163526290666E-10,
 2.65982372468238665035E-9,
-1.30002500998624804212E-8,
 6.04699502254191894932E-8,
-2.67079385394061173391E-7,
 1.11738753912010371815E-6,
-4.41673835845875056359E-6,
 1.64484480707288970893E-5,
-5.75419501008210370398E-5,
 1.88502885095841655729E-4,
-5.76375574538582365885E-4,
 1.63947561694133579842E-3,
-4.32430999505057594430E-3,
 1.05464603945949983183E-2,
-2.37374148058994688156E-2,
 4.93052842396707084878E-2,
-9.49010970480476444210E-2,
 1.71620901522208775349E-1,
-3.04682672343198398683E-1,
 6.76795274409476084995E-1
};

/* Chebyshev coefficients for exp(-x) sqrt(x) I0(x)
 * in the inverted interval [8,infinity].
 *
 * lim(x->inf){ exp(-x) sqrt(x) I0(x) } = 1/sqrt(2pi).
 */

static double i0B[] = {
-7.23318048787475395456E-18,
-4.83050448594418207126E-18,
 4.46562142029675999901E-17,
 3.46122286769746109310E-17,
-2.82762398051658348494E-16,
-3.42548561967721913462E-16,
 1.77256013305652638360E-15,
 3.81168066935262242075E-15,
-9.55484669882830764870E-15,
-4.15056934728722208663E-14,
 1.54008621752140982691E-14,
 3.85277838274214270114E-13,
 7.18012445138366623367E-13,
-1.79417853150680611778E-12,
-1.32158118404477131188E-11,
-3.14991652796324136454E-11,
 1.18891471078464383424E-11,
 4.94060238822496958910E-10,
 3.39623202570838634515E-9,
 2.26666899049817806459E-8,
 2.04891858946906374183E-7,
 2.89137052083475648297E-6,
 6.88975834691682398426E-5,
 3.36911647825569408990E-3,
 8.04490411014108831608E-1
};

double cephes_i0 (double x) {
  double y;
  if (x < 0) x = -x;
  if (x <= 8.0) {
    y = 0.5*x - 2.0;
    return sun_exp(x)*chbevl(y, i0A, 30);
	}
  return sun_exp(x)*chbevl(32.0/x - 2.0, i0B, 25)/sqrt(x);
}


double cephes_i0e (double x) {
  double y;
  if (x < 0) x = -x;
  if (x <= 8.0) {
    y = 0.5*x - 2.0;
    return chbevl(y, i0A, 30);
  }
  return chbevl(32.0/x - 2.0, i0B, 25)/sqrt(x);
}


/*							i1.c
 *
 *	Modified Bessel function of order one
 *
 * SYNOPSIS:
 *
 * double x, y, i1();
 *
 * y = i1( x );
 *
 * DESCRIPTION:
 *
 * Returns modified Bessel function of order one of the
 * argument.
 *
 * The function is defined as i1(x) = -i j1( ix ).
 *
 * The range is partitioned into the two intervals [0,8] and
 * (8, infinity).  Chebyshev polynomial expansions are employed
 * in each interval.
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0, 30        3400       1.2e-16     2.3e-17
 *    IEEE      0, 30       30000       1.9e-15     2.1e-16
 */
/*							i1e.c
 *
 *	Modified Bessel function of order one,
 *	exponentially scaled
 *
 * SYNOPSIS:
 *
 * double x, y, i1e();
 *
 * y = i1e( x );
 *
 * DESCRIPTION:
 *
 * Returns exponentially scaled modified Bessel function
 * of order one of the argument.
 *
 * The function is defined as i1(x) = -i exp(-|x|) j1( ix ).
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      0, 30       30000       2.0e-15     2.0e-16
 * See i1().
 */

/*							i1.c 2		*/

/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1985, 1987, 2000 by Stephen L. Moshier
*/
/* Chebyshev coefficients for exp(-x) I1(x) / x
 * in the interval [0,8].
 *
 * lim(x->0){ exp(-x) I1(x) / x } = 1/2.
 */

static double i1A[] = {
 2.77791411276104639959E-18,
-2.11142121435816608115E-17,
 1.55363195773620046921E-16,
-1.10559694773538630805E-15,
 7.60068429473540693410E-15,
-5.04218550472791168711E-14,
 3.22379336594557470981E-13,
-1.98397439776494371520E-12,
 1.17361862988909016308E-11,
-6.66348972350202774223E-11,
 3.62559028155211703701E-10,
-1.88724975172282928790E-9,
 9.38153738649577178388E-9,
-4.44505912879632808065E-8,
 2.00329475355213526229E-7,
-8.56872026469545474066E-7,
 3.47025130813767847674E-6,
-1.32731636560394358279E-5,
 4.78156510755005422638E-5,
-1.61760815825896745588E-4,
 5.12285956168575772895E-4,
-1.51357245063125314899E-3,
 4.15642294431288815669E-3,
-1.05640848946261981558E-2,
 2.47264490306265168283E-2,
-5.29459812080949914269E-2,
 1.02643658689847095384E-1,
-1.76416518357834055153E-1,
 2.52587186443633654823E-1
};

/*							i1.c	*/

/* Chebyshev coefficients for exp(-x) sqrt(x) I1(x)
 * in the inverted interval [8,infinity].
 *
 * lim(x->inf){ exp(-x) sqrt(x) I1(x) } = 1/sqrt(2pi).
 */

static double i1B[] = {
 7.51729631084210481353E-18,
 4.41434832307170791151E-18,
-4.65030536848935832153E-17,
-3.20952592199342395980E-17,
 2.96262899764595013876E-16,
 3.30820231092092828324E-16,
-1.88035477551078244854E-15,
-3.81440307243700780478E-15,
 1.04202769841288027642E-14,
 4.27244001671195135429E-14,
-2.10154184277266431302E-14,
-4.08355111109219731823E-13,
-7.19855177624590851209E-13,
 2.03562854414708950722E-12,
 1.41258074366137813316E-11,
 3.25260358301548823856E-11,
-1.89749581235054123450E-11,
-5.58974346219658380687E-10,
-3.83538038596423702205E-9,
-2.63146884688951950684E-8,
-2.51223623787020892529E-7,
-3.88256480887769039346E-6,
-1.10588938762623716291E-4,
-9.76109749136146840777E-3,
 7.78576235018280120474E-1
};

/*							i1.c	*/

double cephes_i1 (double x) {
  double y, z;
  z = fabs(x);
  if (z <= 8.0) {
    y = 0.5*z - 2.0;
    z = chbevl(y, i1A, 29)*z*sun_exp(z);
  } else {
    z = sun_exp(z)*chbevl(32.0/z - 2.0, i1B, 25)/sqrt(z);
	}
  if (x < 0.0) z = -z;
  return z;
}

/*							i1e()	*/

double cephes_i1e (double x) {
  double y, z;
  z = fabs(x);
  if (z <= 8.0) {
    y = 0.5*z - 2.0;
    z = chbevl(y, i1A, 29)*z;
  } else {
    z = chbevl(32.0/z - 2.0, i1B, 25)/sqrt(z);
  }
  if (x < 0.0) z = -z;
  return z;
}


/*							j0.c
 *
 *	Bessel function of order zero
 *
 * SYNOPSIS:
 *
 * double x, y, j0();
 *
 * y = j0(x);
 *
 * DESCRIPTION:
 *
 * Returns Bessel function of order zero of the argument.
 *
 * The domain is divided into the intervals [0, 5] and
 * (5, infinity). In the first interval the following rational
 * approximation is used:
 *
 *        2         2
 * (w - r  ) (w - r  ) P (w) / Q (w)
 *       1         2    3       8
 *
 *            2
 * where w = x  and the two r's are zeros of the function.
 *
 * In the second interval, the Hankel asymptotic expansion
 * is employed with two rational functions of degree 6/6
 * and 7/7.
 *
 * ACCURACY:
 *                      Absolute error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0, 30       10000       4.4e-17     6.3e-18
 *    IEEE      0, 30       60000       4.2e-16     1.1e-16
 */
/*							y0.c
 *
 *	Bessel function of the second kind, order zero
 *
 * SYNOPSIS:
 *
 * double x, y, y0();
 *
 * y = y0( x);
 *
 * DESCRIPTION:
 *
 * Returns Bessel function of the second kind, of order
 * zero, of the argument.
 *
 * The domain is divided into the intervals [0, 5] and
 * (5, infinity). In the first interval a rational approximation
 * R(x) is employed to compute
 *   y0(x)  = R(x)  +   2 * log(x) * j0(x) / PI.
 * Thus a call to j0() is required.
 *
 * In the second interval, the Hankel asymptotic expansion
 * is employed with two rational functions of degree 6/6
 * and 7/7.
 *
 * ACCURACY:
 *
 *  Absolute error, when y0(x) < 1; else relative error:
 *
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0, 30        9400       7.0e-17     7.9e-18
 *    IEEE      0, 30       30000       1.3e-15     1.6e-16
 *
 */
/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1989, 2000 by Stephen L. Moshier
*/

/* Note: all coefficients satisfy the relative error criterion
 * except YP, YQ which are designed for absolute error. */

static double jPP[7] = {
  7.96936729297347051624E-4,
  8.28352392107440799803E-2,
  1.23953371646414299388E0,
  5.44725003058768775090E0,
  8.74716500199817011941E0,
  5.30324038235394892183E0,
  9.99999999999999997821E-1,
};
static double jPQ[7] = {
  9.24408810558863637013E-4,
  8.56288474354474431428E-2,
  1.25352743901058953537E0,
  5.47097740330417105182E0,
  8.76190883237069594232E0,
  5.30605288235394617618E0,
  1.00000000000000000218E0,
};

static double jQP[8] = {
-1.13663838898469149931E-2,
-1.28252718670509318512E0,
-1.95539544257735972385E1,
-9.32060152123768231369E1,
-1.77681167980488050595E2,
-1.47077505154951170175E2,
-5.14105326766599330220E1,
-6.05014350600728481186E0,
};

static double jQQ[7] = {
/*  1.00000000000000000000E0,*/
  6.43178256118178023184E1,
  8.56430025976980587198E2,
  3.88240183605401609683E3,
  7.24046774195652478189E3,
  5.93072701187316984827E3,
  2.06209331660327847417E3,
  2.42005740240291393179E2,
};

static double jYP[8] = {
 1.55924367855235737965E4,
-1.46639295903971606143E7,
 5.43526477051876500413E9,
-9.82136065717911466409E11,
 8.75906394395366999549E13,
-3.46628303384729719441E15,
 4.42733268572569800351E16,
-1.84950800436986690637E16,
};

static double jYQ[7] = {
/* 1.00000000000000000000E0,*/
 1.04128353664259848412E3,
 6.26107330137134956842E5,
 2.68919633393814121987E8,
 8.64002487103935000337E10,
 2.02979612750105546709E13,
 3.17157752842975028269E15,
 2.50596256172653059228E17,
};

/*  5.783185962946784521175995758455807035071 */
static double jDR1 = 5.78318596294678452118E0;
/* 30.47126234366208639907816317502275584842 */
static double jDR2 = 3.04712623436620863991E1;

static double jRP[4] = {
-4.79443220978201773821E9,
 1.95617491946556577543E12,
-2.49248344360967716204E14,
 9.70862251047306323952E15,
};

static double jRQ[8] = {
/* 1.00000000000000000000E0,*/
 4.99563147152651017219E2,
 1.73785401676374683123E5,
 4.84409658339962045305E7,
 1.11855537045356834862E10,
 2.11277520115489217587E12,
 3.10518229857422583814E14,
 3.18121955943204943306E16,
 1.71086294081043136091E18,
};

double cephes_j0 (double x) {
  double w, z, p, q, xn;
  if (x < 0 ) x = -x;
  if (x <= 5.0) {
    z = x * x;
    if (x < 1.0e-5) return 1.0 - z/4.0;
    p = (z - jDR1) * (z - jDR2);
    p = p * polevl(z, jRP, 3)/p1evl(z, jRQ, 8);
    return p;
  }
  w = 5.0/x;
  q = 25.0/(x*x);
  p = polevl(q, jPP, 6)/polevl(q, jPQ, 6);
  q = polevl(q, jQP, 7)/p1evl( q, jQQ, 7);
  xn = x - PIO4;
  p = p * sun_cos(xn) - w * q * sun_sin(xn);
  return p * SQ2OPI / sqrt(x);
}

/*							y0() 2	*/
/* Bessel function of second kind, order zero	*/

/* Rational approximation coefficients YP[], YQ[] are used here.
 * The function computed is  y0(x)  -  2 * log(x) * j0(x) / PI,
 * whose value at x = 0 is  2 * ( log(0.5) + EUL ) / PI
 * = 0.073804295108687225.
 */

double cephes_y0 (double x) {
  double w, z, p, q, xn;
  if (x <= 5.0) {
    if (x <= 0.0 ) {
      /* mtherr( "y0", DOMAIN); */
      return( -MAXNUM);
    }
    z = x * x;
    w = polevl(z, jYP, 7) / p1evl( z, jYQ, 7);
    w += TWOOPI * sun_log(x) * j0(x);
    return w;
  }
  w = 5.0/x;
  z = 25.0/(x * x);
  p = polevl(z, jPP, 6)/polevl(z, jPQ, 6);
  q = polevl(z, jQP, 7)/p1evl(z, jQQ, 7);
  xn = x - PIO4;
  p = p * sun_sin(xn) + w * q * sun_cos(xn);
  return p * SQ2OPI / sqrt(x);
}


/*							j1.c
 *
 *	Bessel function of order one
 *
 * SYNOPSIS:
 *
 * double x, y, j1();
 *
 * y = j1(x);
 *
 * DESCRIPTION:
 *
 * Returns Bessel function of order one of the argument.
 *
 * The domain is divided into the intervals [0, 8] and
 * (8, infinity). In the first interval a 24 term Chebyshev
 * expansion is used. In the second, the asymptotic
 * trigonometric representation is employed using two
 * rational functions of degree 5/5.
 *
 * ACCURACY:
 *                      Absolute error:
 * arithmetic   domain      # trials      peak         rms
 *    DEC       0, 30       10000       4.0e-17     1.1e-17
 *    IEEE      0, 30       30000       2.6e-16     1.1e-16
 */
/*							y1.c
 *
 *	Bessel function of second kind of order one
 *
 * SYNOPSIS:
 *
 * double x, y, y1();
 *
 * y = y1( x);
 *
 * DESCRIPTION:
 *
 * Returns Bessel function of the second kind of order one
 * of the argument.
 *
 * The domain is divided into the intervals [0, 8] and
 * (8, infinity). In the first interval a 25 term Chebyshev
 * expansion is used, and a call to j1() is required.
 * In the second, the asymptotic trigonometric representation
 * is employed using two rational functions of degree 5/5.
 *
 * ACCURACY:
 *                      Absolute error:
 * arithmetic   domain      # trials      peak         rms
 *    DEC       0, 30       10000       8.6e-17     1.3e-17
 *    IEEE      0, 30       30000       1.0e-15     1.3e-16
 *
 * (error criterion relative when |y1| > 1).
 */

/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1989, 2000 by Stephen L. Moshier
*/

/*
#define PIO4 .78539816339744830962
#define THPIO4 2.35619449019234492885
#define SQ2OPI .79788456080286535588
*/

static double j1RP[4] = {
-8.99971225705559398224E8,
 4.52228297998194034323E11,
-7.27494245221818276015E13,
 3.68295732863852883286E15,
};

static double j1RQ[8] = {
/* 1.00000000000000000000E0,*/
 6.20836478118054335476E2,
 2.56987256757748830383E5,
 8.35146791431949253037E7,
 2.21511595479792499675E10,
 4.74914122079991414898E12,
 7.84369607876235854894E14,
 8.95222336184627338078E16,
 5.32278620332680085395E18,
};

static double j1PP[7] = {
 7.62125616208173112003E-4,
 7.31397056940917570436E-2,
 1.12719608129684925192E0,
 5.11207951146807644818E0,
 8.42404590141772420927E0,
 5.21451598682361504063E0,
 1.00000000000000000254E0,
};

static double j1PQ[7] = {
 5.71323128072548699714E-4,
 6.88455908754495404082E-2,
 1.10514232634061696926E0,
 5.07386386128601488557E0,
 8.39985554327604159757E0,
 5.20982848682361821619E0,
 9.99999999999999997461E-1,
};

static double j1QP[8] = {
 5.10862594750176621635E-2,
 4.98213872951233449420E0,
 7.58238284132545283818E1,
 3.66779609360150777800E2,
 7.10856304998926107277E2,
 5.97489612400613639965E2,
 2.11688757100572135698E2,
 2.52070205858023719784E1,
};

static double j1QQ[7] = {
/* 1.00000000000000000000E0,*/
 7.42373277035675149943E1,
 1.05644886038262816351E3,
 4.98641058337653607651E3,
 9.56231892404756170795E3,
 7.99704160447350683650E3,
 2.82619278517639096600E3,
 3.36093607810698293419E2,
};

static double j1YP[6] = {
 1.26320474790178026440E9,
-6.47355876379160291031E11,
 1.14509511541823727583E14,
-8.12770255501325109621E15,
 2.02439475713594898196E17,
-7.78877196265950026825E17,
};

static double j1YQ[8] = {
/* 1.00000000000000000000E0,*/
 5.94301592346128195359E2,
 2.35564092943068577943E5,
 7.34811944459721705660E7,
 1.87601316108706159478E10,
 3.88231277496238566008E12,
 6.20557727146953693363E14,
 6.87141087355300489866E16,
 3.97270608116560655612E18,
};

static double j1Z1 = 1.46819706421238932572E1;
static double j1Z2 = 4.92184563216946036703E1;

double cephes_j1 (double x) {
  double w, z, p, q, xn;
  w = x;
  if (x < 0) w = -x;
  if (w <= 5.0) {
    z = x*x;
    w = polevl(z, j1RP, 3)/p1evl(z, j1RQ, 8);
    w = w*x*(z - j1Z1)*(z - j1Z2);
    return w ;
  }
  w = 5.0/x;
  z = w*w;
  p = polevl(z, j1PP, 6)/polevl(z, j1PQ, 6);
  q = polevl(z, j1QP, 7)/p1evl(z, j1QQ, 7);
  xn = x - THPIO4;
  p = p*sun_cos(xn) - w*q*sun_sin(xn);
  return p * SQ2OPI/sqrt(x);
}


double cephes_y1 (double x) {
  double w, z, p, q, xn;
  if (x <= 5.0) {
    if (x <= 0.0) {
      /* mtherr( "y1", DOMAIN); */
      return -MAXNUM;
    }
    z = x * x;
    w = x * (polevl(z, j1YP, 5 )/p1evl(z, j1YQ, 8));
    w += TWOOPI*(j1(x)*sun_log(x) - 1.0/x);
    return( w);
  }
  w = 5.0/x;
  z = w*w;
  p = polevl(z, j1PP, 6)/polevl(z, j1PQ, 6);
  q = polevl(z, j1QP, 7)/p1evl(z, j1QQ, 7);
  xn = x - THPIO4;
  p = p*sun_sin(xn) + w*q*sun_cos(xn);
  return p*SQ2OPI/sqrt(x);
}


/*							jn.c
 *
 *	Bessel function of integer order
 *
 * SYNOPSIS:
 *
 * int n;
 * double x, y, jn();
 *
 * y = jn( n, x );
 *
 * DESCRIPTION:
 *
 * Returns Bessel function of order n, where n is a
 * (possibly negative) integer.
 *
 * The ratio of jn(x) to j0(x) is computed by backward
 * recurrence.  First the ratio jn/jn-1 is found by a
 * continued fraction expansion.  Then the recurrence
 * relating successive orders is applied until j0 or j1 is
 * reached.
 *
 * If n = 0 or 1 the routine for j0 or j1 is called
 * directly.
 *
 * ACCURACY:
 *                      Absolute error:
 * arithmetic   range      # trials      peak         rms
 *    DEC       0, 30        5500       6.9e-17     9.3e-18
 *    IEEE      0, 30        5000       4.4e-16     7.9e-17
 *
 * Not suitable for large n or x. Use jv() instead.
 *
 * See also: agnhlps.c/sun_jn, agnhlps.c/sun_yn.
 */
/*							jn.c
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 2000 by Stephen L. Moshier
*/

double cephes_jn (int n, double x) {
  double pkm2, pkm1, pk, xk, r, ans;
  int k, sign;
  if (n < 0) {
    n = -n;
    if ((n & 1) == 0)  /* -1**n */
      sign = 1;
    else
      sign = -1;
  } else
    sign = 1;
  if (x < 0.0) {
    if (n & 1) sign = -sign;
    x = -x;
  }
  if (n == 0) return sign*j0(x);
  if (n == 1) return sign*j1(x);
  if (n == 2) return sign*(2.0*j1(x)/x - j0(x));
  if (x < MACHEP) return 0.0;
  /* continued fraction */
  k = 53;
  pk = 2*(n + k);
  ans = pk;
  xk = x*x;
  do {
    pk -= 2.0;
    ans = pk - (xk/ans);
  } while (--k > 0);
  ans = x/ans;
  /* backward recurrence */
  pk = 1.0;
  pkm1 = 1.0/ans;
  k = n - 1;
  r = 2*k;
  do {
    pkm2 = (pkm1*r - pk*x)/x;
    pk = pkm1;
    pkm1 = pkm2;
    r -= 2.0;
  } while (--k > 0);
  if (fabs(pk) > fabs(pkm1))
    ans = j1(x)/pk;
  else
    ans = j0(x)/pkm1;
  return sign*ans;
}


static long double MACHEPL = 5.42101086242752217003726400434970855712890625E-20L;

/* Polynomial evaluator: P[0] x^n  +  P[1] x^(n-1)  +  ...  +  P[n] */
static long double polevll (long double x, void *p, int n) {
  register long double y;
  register long double *P = (long double *)p;
  y = *P++;
  do {
    y = fmal(y, x, *P++);
  }
  while (--n);
  return(y);
}

/* Polynomial evaluator: x^n  +  P[0] x^(n-1)  +  P[1] x^(n-2)  +  ...  +  P[n] */
static long double p1evll (long double x, void *p, int n) {
  register long double y;
  register long double *P = (long double *)p;
  n -= 1;
  y = x + *P++;
  do {
    y = fmal(y, x, *P++);
  }
  while (--n);
  return y;
}

/*							j0l.c
 *
 *	Bessel function of order zero
 *
 * SYNOPSIS:
 *
 * long double x, y, j0l();
 *
 * y = j0l( x );
 *
 * DESCRIPTION:
 *
 * Returns Bessel function of first kind, order zero of the argument.
 *
 * The domain is divided into the intervals [0, 9] and
 * (9, infinity). In the first interval the rational approximation
 * is (x^2 - r^2) (x^2 - s^2) (x^2 - t^2) P7(x^2) / Q8(x^2),
 * where r, s, t are the first three zeros of the function.
 * In the second interval the expansion is in terms of the
 * modulus M0(x) = sqrt(J0(x)^2 + Y0(x)^2) and phase  P0(x)
 * = atan(Y0(x)/J0(x)).  M0 is approximated by sqrt(1/x)P7(1/x)/Q7(1/x).
 * The approximation to J0 is M0 * cos(x -  pi/4 + 1/x P5(1/x^2)/Q6(1/x^2)).
 *
 * ACCURACY:
 *
 *                      Absolute error:
 * arithmetic   domain      # trials      peak         rms
 *    IEEE      0, 30       100000      2.8e-19      7.4e-20
 */
/*							y0l.c
 *
 *	Bessel function of the second kind, order zero
 *
 * SYNOPSIS:
 *
 * double x, y, y0l();
 *
 * y = y0l( x );
 *
 * DESCRIPTION:
 *
 * Returns Bessel function of the second kind, of order
 * zero, of the argument.
 *
 * The domain is divided into the intervals [0, 5>, [5,9> and
 * [9, infinity). In the first interval a rational approximation
 * R(x) is employed to compute y0(x)  = R(x) + 2/pi * log(x) * j0(x).
 *
 * In the second interval, the approximation is
 *     (x - p)(x - q)(x - r)(x - s)P7(x)/Q7(x)
 * where p, q, r, s are zeros of y0(x).
 *
 * The third interval uses the same approximations to modulus
 * and phase as j0(x), whence y0(x) = modulus * sin(phase).
 *
 * ACCURACY:
 *
 *  Absolute error, when y0(x) < 1; else relative error:
 *
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      0, 30       100000      3.4e-19     7.6e-20
 */

/* Copyright 1994 by Stephen L. Moshier (moshier@world.std.com).  */

static long double j0n[8] = {
 1.296848754518641770562E0L,
-3.239201943301299801018E3L,
 3.490002040733471400107E6L,
-2.076797068740966813173E9L,
 7.283696461857171054941E11L,
-1.487926133645291056388E14L,
 1.620335009643150402368E16L,
-7.173386747526788067407E17L,
};

static long double j0d[8] = {
/* 1.000000000000000000000E0L,*/
 2.281959869176887763845E3L,
 2.910386840401647706984E6L,
 2.608400226578100610991E9L,
 1.752689035792859338860E12L,
 8.879132373286001289461E14L,
 3.265560832845194013669E17L,
 7.881340554308432241892E19L,
 9.466475654163919450528E21L,
};

static long double modulusn[8] = {
 3.947542376069224461532E-1L,
 6.864682945702134624126E0L,
 1.021369773577974343844E1L,
 7.626141421290849630523E0L,
 2.842537511425216145635E0L,
 7.162842530423205720962E-1L,
 9.036664453160200052296E-2L,
 8.461833426898867839659E-3L,
};

static long double modulusd[7] = {
/* 1.000000000000000000000E0L,*/
 9.117176038171821115904E0L,
 1.301235226061478261481E1L,
 9.613002539386213788182E0L,
 3.569671060989910901903E0L,
 8.983920141407590632423E-1L,
 1.132577931332212304986E-1L,
 1.060533546154121770442E-2L,
};

static long double phasen[6] = {
-7.356766355393571519038E-1L,
-5.001635199922493694706E-1L,
-7.737323518141516881715E-2L,
-3.998893155826990642730E-3L,
-7.496317036829964150970E-5L,
-4.290885090773112963542E-7L,
};

static long double phased[6] = {
/* 1.000000000000000000000E0L,*/
 7.377856408614376072745E0L,
 4.285043297797736118069E0L,
 6.348446472935245102890E-1L,
 3.229866782185025048457E-2L,
 6.014932317342190404134E-4L,
 3.432708072618490390095E-6L,
};

#define JZ1 5.783185962946784521176L
#define JZ2 30.47126234366208639908L
#define JZ3 7.488700679069518344489e1L

long double cephes_j0l (long double x) {
  long double xx, y, z, modulus, phase;
  xx = x*x;
  if (xx < 81.0L) {
    y = (xx - JZ1)*(xx - JZ2)*(xx - JZ3);
    y *= polevll(xx, j0n, 7)/p1evll(xx, j0d, 8);
    return y;
  }
  y = fabsl(x);
  xx = 1.0/xx;
  phase = polevll(xx, phasen, 5)/p1evll(xx, phased, 6);
  z = 1.0/y;
  modulus = polevll(z, modulusn, 7)/p1evll(z, modulusd, 7);
  y = modulus*sun_cosl(y - PIO4L + z*phase)/sqrtl(y);
  return y;
}

static long double y0n[8] = {
 1.556909814120445353691E4L,
-1.464324149797947303151E7L,
 5.427926320587133391307E9L,
-9.808510181632626683952E11L,
 8.747842804834934784972E13L,
-3.461898868011666236539E15L,
 4.421767595991969611983E16L,
-1.847183690384811186958E16L,
};

static long double y0d[7] = {
/* 1.000000000000000000000E0L,*/
 1.040792201755841697889E3L,
 6.256391154086099882302E5L,
 2.686702051957904669677E8L,
 8.630939306572281881328E10L,
 2.027480766502742538763E13L,
 3.167750475899536301562E15L,
 2.502813268068711844040E17L,
};

static long double y059n[10] = {
 2.368715538373384869796E-2L,
-1.472923738545276751402E0L,
 2.525993724177105060507E1L,
 7.727403527387097461580E1L,
-4.578271827238477598563E3L,
 7.051411242092171161986E3L,
 1.951120419910720443331E5L,
 6.515211089266670755622E5L,
-1.164760792144532266855E5L,
-5.566567444353735925323E5L,
};

static long double y059d[9] = {
/* 1.000000000000000000000E0L,*/
-6.235501989189125881723E1L,
 2.224790285641017194158E3L,
-5.103881883748705381186E4L,
 8.772616606054526158657E5L,
-1.096162986826467060921E7L,
 1.083335477747278958468E8L,
-7.045635226159434678833E8L,
 3.518324187204647941098E9L,
 1.173085288957116938494E9L,
};

#define Y0Z1 3.957678419314857868376e0L
#define Y0Z2 7.086051060301772697624e0L
#define Y0Z3 1.022234504349641701900e1L
#define Y0Z4 1.336109747387276347827e1L

long double cephes_y0l (long double x) {
  long double xx, y, z, modulus, phase;
  if (x < 0.0) return (-MAXNUML);
  xx = x * x;
  if (xx < 81.0L) {
    if (xx < 20.25L) {
      y = TWOOPIL*tools_logl(x)*cephes_j0l(x);
      y += polevll(xx, y0n, 7)/p1evll(xx, y0d, 7);
    } else {
      y = (x - Y0Z1)*(x - Y0Z2)*(x - Y0Z3)*(x - Y0Z4);
      y *= polevll(x, y059n, 9)/p1evll(x, y059d, 9);
    }
    return y;
  }
  y = fabsl(x);
  xx = 1.0/xx;
  phase = polevll(xx, phasen, 5)/p1evll(xx, phased, 6);
  z = 1.0/y;
  modulus = polevll(z, modulusn, 7)/p1evll(z, modulusd, 7);
  y = modulus*sun_sinl(y -  PIO4L + z*phase)/sqrtl(y);
  return y;
}

/*							j1l.c
 *
 *	Bessel function of order one
 *
 * SYNOPSIS:
 *
 * long double x, y, j1l();
 *
 * y = j1l( x );
 *
 * DESCRIPTION:
 *
 * Returns Bessel function of order one of the argument.
 *
 * The domain is divided into the intervals [0, 9] and
 * (9, infinity). In the first interval the rational approximation
 * is (x^2 - r^2) (x^2 - s^2) (x^2 - t^2) x P8(x^2) / Q8(x^2),
 * where r, s, t are the first three zeros of the function.
 * In the second interval the expansion is in terms of the
 * modulus M1(x) = sqrt(J1(x)^2 + Y1(x)^2) and phase  P1(x)
 * = atan(Y1(x)/J1(x)).  M1 is approximated by sqrt(1/x)P7(1/x)/Q8(1/x).
 * The approximation to j1 is M1 * cos(x -  3 pi/4 + 1/x P5(1/x^2)/Q6(1/x^2)).
 *
 * ACCURACY:
 *                      Absolute error:
 * arithmetic   domain      # trials      peak         rms
 *    IEEE      0, 30        40000      1.8e-19      5.0e-20
 */
/*							y1l.c
 *
 *	Bessel function of the second kind, order zero
 *
 * SYNOPSIS:
 *
 * double x, y, y1l();
 *
 * y = y1l( x );
 *
 * DESCRIPTION:
 *
 * Returns Bessel function of the second kind, of order
 * zero, of the argument.
 *
 * The domain is divided into the intervals [0, 4.5>, [4.5,9> and
 * [9, infinity). In the first interval a rational approximation
 * R(x) is employed to compute y0(x)  = R(x) + 2/pi * log(x) * j0(x).
 *
 * In the second interval, the approximation is
 *     (x - p)(x - q)(x - r)(x - s)P9(x)/Q10(x)
 * where p, q, r, s are zeros of y1(x).
 *
 * The third interval uses the same approximations to modulus
 * and phase as j1(x), whence y1(x) = modulus * sin(phase).
 *
 * ACCURACY:
 *
 *  Absolute error, when y0(x) < 1; else relative error:
 *
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      0, 30       36000       2.7e-19     5.3e-20
 */

/* Copyright 1994 by Stephen L. Moshier (moshier@world.std.com).  */

static long double j1n[9] = {
-2.63469779622127762897E-4L,
 9.31329762279632791262E-1L,
-1.46280142797793933909E3L,
 1.32000129539331214495E6L,
-7.41183271195454042842E8L,
 2.626500686552841932403E11L,
-5.68263073022183470933E13L,
 6.80006297997263446982E15L,
-3.41470097444474566748E17L,
};

static long double j1d[8] = {
/* 1.00000000000000000000E0L,*/
 2.95267951972943745733E3L,
 4.78723926343829674773E6L,
 5.37544732957807543920E9L,
 4.46866213886267829490E12L,
 2.76959756375961607085E15L,
 1.23367806884831151194E18L,
 3.57325874689695599524E20L,
 5.10779045516141578461E22L,
};

static long double modulusnj1l[8] = {
-5.041742205078442098874E0L,
 3.918474430130242177355E-1L,
 2.527521168680500659056E0L,
 7.172146812845906480743E0L,
 2.859499532295180940060E0L,
 1.014671139779858141347E0L,
 1.255798064266130869132E-1L,
 1.596507617085714650238E-2L,
};

static long double modulusdj1l[8] = {
/* 1.000000000000000000000E0L,*/
-6.233092094568239317498E0L,
-9.214128701852838347002E-1L,
 2.531772200570435289832E0L,
 8.755081357265851765640E0L,
 3.554340386955608261463E0L,
 1.267949948774331531237E0L,
 1.573909467558180942219E-1L,
 2.000925566825407466160E-2L,
};

static long double phasenj1l[6] = {
 2.010456367705144783933E0L,
 1.587378144541918176658E0L,
 2.682837461073751055565E-1L,
 1.472572645054468815027E-2L,
 2.884976126715926258586E-4L,
 1.708502235134706284899E-6L,
};

static long double phasedj1l[6] = {
/* 1.000000000000000000000E0L,*/
 6.809332495854873089362E0L,
 4.518597941618813112665E0L,
 7.320149039410806471101E-1L,
 3.960155028960712309814E-2L,
 7.713202197319040439861E-4L,
 4.556005960359216767984E-6L,
};

#define JZ1j1l 1.46819706421238932572e1L
#define JZ2j1l 4.92184563216946036703e1L
#define JZ3j1l 1.03499453895136580332e2L

#define THPIO4L  2.35619449019234492885L

long double cephes_j1l (long double x) {
  long double xx, y, z, modulus, phase;
  xx = x*x;
  if (xx < 81.0L) {
    y = (xx - JZ1j1l)*(xx - JZ2j1l)*(xx - JZ3j1l);
    y *= x*polevll(xx, j1n, 8)/p1evll(xx, j1d, 8);
    return y;
  }
  y = fabsl(x);
  xx = 1.0/xx;
  phase = polevll(xx, phasenj1l, 5)/p1evll(xx, phasedj1l, 6);
  z = 1.0/y;
  modulus = polevll(z, modulusnj1l, 7)/p1evll(z, modulusdj1l, 8);
  y = modulus*sun_cosl(y - THPIO4L + z*phase)/sqrtl(y);
  if (x < 0) y = -y;
  return y;
}

static long double y1n[7] = {
-1.288901054372751879531E5L,
 9.914315981558815369372E7L,
-2.906793378120403577274E10L,
 3.954354656937677136266E12L,
-2.445982226888344140154E14L,
 5.685362960165615942886E15L,
-2.158855258453711703120E16L,
};

static long double y1d[7] = {
/* 1.000000000000000000000E0L,*/
 8.926354644853231136073E2L,
 4.679841933793707979659E5L,
 1.775133253792677466651E8L,
 5.089532584184822833416E10L,
 1.076474894829072923244E13L,
 1.525917240904692387994E15L,
 1.101136026928555260168E17L,
};

static long double y159n[10] = {
-6.806634906054210550896E-1L,
 4.306669585790359450532E1L,
-9.230477746767243316014E2L,
 6.171186628598134035237E3L,
 2.096869860275353982829E4L,
-1.238961670382216747944E5L,
-1.781314136808997406109E6L,
-1.803400156074242435454E6L,
-1.155761550219364178627E6L,
 3.112221202330688509818E5L,
};

static long double y159d[10] = {
/* 1.000000000000000000000E0L,*/
-6.181482377814679766978E1L,
 2.238187927382180589099E3L,
-5.225317824142187494326E4L,
 9.217235006983512475118E5L,
-1.183757638771741974521E7L,
 1.208072488974110742912E8L,
-8.193431077523942651173E8L,
 4.282669747880013349981E9L,
-1.171523459555524458808E9L,
 1.078445545755236785692E8L,
};

#define THPIO4L 2.35619449019234492885L
#define Y1Z1 2.19714132603101703515e0L
#define Y1Z2 5.42968104079413513277e0L
#define Y1Z3 8.59600586833116892643e0L
#define Y1Z4 1.17491548308398812434e1L

long double cephes_y1l (long double x) {
  long double xx, y, z, modulus, phase;
  if (x < 0.0) return -MAXNUML;
  z = 1.0/x;
  xx = x*x;
  if (xx < 81.0L ) {
    if (xx < 20.25L) {
      y = TWOOPIL*(tools_logl(x)*cephes_j1l(x) - z);
      y += x*polevll(xx, y1n, 6)/p1evll(xx, y1d, 7);
    } else {
      y = (x - Y1Z1)*(x - Y1Z2)*(x - Y1Z3)*(x - Y1Z4);
      y *= polevll(x, y159n, 9)/p1evll(x, y159d, 10);
    }
    return y;
  }
  xx = 1.0/xx;
  phase = polevll(xx, phasen, 5)/p1evll( xx, phased, 6);
  modulus = polevll(z, modulusn, 7)/p1evll(z, modulusd, 8);
  z = modulus*sun_sinl( x - THPIO4L + z*phase)/sqrtl(x);
  return z;
}

/*							jnl.c
 *
 *	Bessel function of integer order
 *
 * SYNOPSIS:
 *
 * int n;
 * long double x, y, jnl();
 *
 * y = jnl( n, x );
 *
 * DESCRIPTION:
 *
 * Returns Bessel function of order n, where n is a
 * (possibly negative) integer.
 *
 * The ratio of jn(x) to j0(x) is computed by backward
 * recurrence.  First the ratio jn/jn-1 is found by a
 * continued fraction expansion.  Then the recurrence
 * relating successive orders is applied until j0 or j1 is
 * reached.
 *
 * If n = 0 or 1 the routine for j0 or j1 is called
 * directly.
 *
 * ACCURACY:
 *                      Absolute error:
 * arithmetic   domain      # trials      peak         rms
 *    IEEE     -30, 30        5000       3.3e-19     4.7e-20
 *
 * Not suitable for large n or x.
 */
/*							jn.c
Cephes Math Library Release 2.0:  April, 1987
Copyright 1984, 1987 by Stephen L. Moshier
Direct inquiries to 30 Frost Street, Cambridge, MA 02140
*/

long double cephes_jnl (int n, long double x) {
  long double pkm2, pkm1, pk, xk, r, ans;
  int k, sign;
  if (n < 0) {
    n = -n;
    if( (n & 1) == 0 )	/* -1**n */
      sign = 1;
    else
      sign = -1;
  } else
    sign = 1;
  if (x < 0.0L) {
    if (n & 1) sign = -sign;
  }
  if (n == 0) return sign*cephes_j0l(x);
  if (n == 1) return sign*cephes_j1l(x);
  if (n == 2) return sign*(2.0L*cephes_j1l(x)/x - cephes_j0l(x));
  if (x < MACHEPL) return 0.0L;
  /* continued fraction */
  k = 53;
  pk = 2*(n + k);
  ans = pk;
  xk = x * x;
  do {
    pk -= 2.0L;
    ans = pk - (xk/ans);
  } while (--k > 0);
  ans = x/ans;
  /* backward recurrence */
  pk = 1.0L;
  pkm1 = 1.0L/ans;
  k = n - 1;
  r = 2*k;
  do {
    pkm2 = (pkm1*r - pk*x)/x;
    pk = pkm1;
    pkm1 = pkm2;
    r -= 2.0L;
  } while (--k > 0);
  if (fabsl(pk) > fabsl(pkm1))
    ans = cephes_j1l(x)/pk;
  else
    ans = cephes_j0l(x)/pkm1;
  return sign*ans;
}


/*							ynl.c
 *
 *	Bessel function of second kind of integer order
 *
 * SYNOPSIS:
 *
 * long double x, y, ynl();
 * int n;
 *
 * y = ynl( n, x );
 *
 * DESCRIPTION:
 *
 * Returns Bessel function of order n, where n is a
 * (possibly negative) integer.
 *
 * The function is evaluated by forward recurrence on
 * n, starting with values computed by the routines
 * y0l() and y1l().
 *
 * If n = 0 or 1 the routine for y0l or y1l is called
 * directly.
 *
 * ACCURACY:
 *
 *       Absolute error, except relative error when y > 1.
 *       x >= 0,  -30 <= n <= +30.
 * arithmetic   domain     # trials      peak         rms
 *    IEEE     -30, 30       10000       1.3e-18     1.8e-19
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * ynl singularity   x = 0              MAXNUML
 * ynl overflow                         MAXNUML
 *
 * Spot checked against tables for x, n between 0 and 100.
 */
/*
Cephes Math Library Release 2.1:  December, 1988
Copyright 1984, 1987 by Stephen L. Moshier
Direct inquiries to 30 Frost Street, Cambridge, MA 02140
*/

long double cephes_ynl (int n, long double x) {
  long double an, anm1, anm2, r;
  int k, sign;
  if (n < 0) {
    n = -n;
    if ((n & 1) == 0)	/* -1**n */
      sign = 1;
    else
      sign = -1;
  } else
    sign = 1;
  if (n == 0) return sign*cephes_y0l(x);
  if (n == 1) return sign*cephes_y1l(x);
  /* test for overflow */
  if (x <= 0.0L) {
	  /* mtherr( "ynl", SING ); */
    return AGN_NAN;
	}
  /* forward recurrence on n */
  anm2 = cephes_y0l(x);
  anm1 = cephes_y1l(x);
  k = 1;
  r = 2*k;
  do {
    an = r*anm1/x - anm2;
    anm2 = anm1;
    anm1 = an;
    r += 2.0L;
    ++k;
	} while (k < n);
  return sign*an;
}


/*							bdtrl.c
 *
 * Binomial (probability) distribution
 *
 * SYNOPSIS:
 *
 * int k, n;
 * long double p, y, bdtrl();
 *
 * y = bdtrl( k, n, p ) = stats.binomd (k, n, p)
 *
 * DESCRIPTION:
 *
 * Returns the sum of the terms 0 through k of the Binomial
 * probability density:
 *
 *   k
 *   --  ( n )   j      n-j
 *   >   (   )  p  (1-p)
 *   --  ( j )
 *  j=0
 *
 * The terms are not summed directly; instead the incomplete
 * beta integral is employed, according to the formula
 *
 * y = bdtr( k, n, p ) = incbet( n-k, k+1, 1-p ).
 *
 * The arguments must be positive, with p ranging from 0 to 1.
 *
 * ACCURACY:
 *
 * Tested at random points (k,n,p) with a and b between 0
 * and 10000 and p between 0 and 1.
 *    Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE      0,10000      3000       1.6e-14     2.2e-15
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * bdtrl domain        k < 0            0.0
 *                     n < k
 *                     x < 0, x > 1
 */
/*							bdtrcl()
 *
 *	Complemented binomial distribution
 *
 * SYNOPSIS:
 *
 * int k, n;
 * long double p, y, bdtrcl();
 *
 * y = bdtrcl( k, n, p );
 *
 * DESCRIPTION:
 *
 * Returns the sum of the terms k+1 through n of the Binomial
 * probability density:
 *
 *   n
 *   --  ( n )   j      n-j
 *   >   (   )  p  (1-p)
 *   --  ( j )
 *  j=k+1
 *
 * The terms are not summed directly; instead the incomplete
 * beta integral is employed, according to the formula
 *
 * y = bdtrc( k, n, p ) = incbet( k+1, n-k, p ).
 *
 * The arguments must be positive, with p ranging from 0 to 1.
 *
 * ACCURACY:
 *
 * See incbet.c.
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * bdtrcl domain     x<0, x>1, n<k       0.0
 */
/*							bdtril()
 *
 * Inverse binomial distribution
 *
 * SYNOPSIS:
 *
 * int k, n;
 * long double p, y, bdtril();
 *
 * p = bdtril( k, n, y );
 *
 * DESCRIPTION:
 *
 * Finds the event probability p such that the sum of the
 * terms 0 through k of the Binomial probability density
 * is equal to the given cumulative probability y.
 *
 * This is accomplished using the inverse beta integral
 * function and the relation
 *
 * 1 - p = calc.invibeta(y, n-k, k+1).
 *
 * ACCURACY:
 *
 * See incbi.c.
 * Tested at random k, n between 1 and 10000.  The "domain" refers to p:
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE       0,1        3500       2.0e-15     8.2e-17
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * bdtril domain     k < 0, n <= k         0.0
 *                  x < 0, x > 1
 */
/*								bdtr() */
/*
Cephes Math Library Release 2.3:  March, 1995
Copyright 1984, 1995 by Stephen L. Moshier
*/

/*							ndtrl.c
 *
 * Normal distribution function
 *
 * SYNOPSIS:
 *
 * long double x, y, ndtrl();
 *
 * y = ndtrl( x );
 *
 * DESCRIPTION:
 *
 * Returns the area under the Gaussian probability density
 * function, integrated from minus infinity to x:
 *
 *                            x
 *                             -
 *                   1        | |          2
 *    ndtr(x)  = ---------    |    exp( - t /2 ) dt
 *               sqrt(2pi)  | |
 *                           -
 *                          -inf.
 *
 *             =  ( 1 + erf(z) ) / 2
 *             =  erfc(z) / 2
 *
 * where z = x/sqrt(2). Computation is via the functions
 * erf and erfc with care to avoid error amplification in computing exp(-x^2).
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    IEEE     -13,0        30000       7.7e-19     1.0e-19
 *    IEEE     -106.5,-2    30000       4.2e-19     7.2e-20
 *    IEEE       0,3        30000       1.0e-19     2.4e-20
 *
 * ERROR MESSAGES:
 *
 *   message         condition           value returned
 * erfcl underflow    x^2 / 2 > MAXLOGL        0.0
 */
/*
Cephes Math Library Release 2.3:  January, 1995
Copyright 1984, 1995 by Stephen L. Moshier
*/

static long double SQRTHL = 7.071067811865475244008e-1L;

static long double ndtrlP[10] = {
 1.130609921802431462353E9L,
 2.290171954844785638925E9L,
 2.295563412811856278515E9L,
 1.448651275892911637208E9L,
 6.234814405521647580919E8L,
 1.870095071120436715930E8L,
 3.833161455208142870198E7L,
 4.964439504376477951135E6L,
 3.198859502299390825278E5L,
-9.085943037416544232472E-6L,
};

static long double ndtrlQ[10] = {
/* 1.000000000000000000000E0L, */
 1.130609910594093747762E9L,
 3.565928696567031388910E9L,
 5.188672873106859049556E9L,
 4.588018188918609726890E9L,
 2.729005809811924550999E9L,
 1.138778654945478547049E9L,
 3.358653716579278063988E8L,
 6.822450775590265689648E7L,
 8.799239977351261077610E6L,
 5.669830829076399819566E5L,
};

static long double ndtrlR[5] = {
 3.621349282255624026891E0L,
 7.173690522797138522298E0L,
 3.445028155383625172464E0L,
 5.537445669807799246891E-1L,
 2.697535671015506686136E-2L,
};

static long double ndtrlS[5] = {
/* 1.000000000000000000000E0L, */
 1.072884067182663823072E1L,
 1.533713447609627196926E1L,
 6.572990478128949439509E0L,
 1.005392977603322982436E0L,
 4.781257488046430019872E-2L,
};

/* Exponentially scaled erfc function exp(x^2) erfc(x) valid for x > 1. Use with ndtrl and expx2l. */
static long double erfcel (long double x) {
  long double p, q, y;
  y = 1.0L/x;
  if (x < 8.0L) {
    p = polevll(y, ndtrlP, 9);
    q = p1evll(y, ndtrlQ, 10);
  } else {
    q = y*y;
    p = y*polevll(q, ndtrlR, 4);
    q = p1evll(q, ndtrlS, 5);
  }
  y = p/q;
  return y;
}

/*							expx2l.c
 *
 * Exponential of squared argument
 *
 * SYNOPSIS:
 *
 * long double x, y, expx2l();
 * int sign;
 *
 * y = expx2l( x, sign );
 *
 * DESCRIPTION:
 *
 * Computes y = exp(x*x) while suppressing error amplification
 * that would ordinarily arise from the inexactness of the
 * exponential argument x*x.
 *
 * If sign < 0, the result is inverted; i.e., y = exp(-x*x) .
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic      domain        # trials      peak         rms
 *   IEEE     -106.566, 106.566    10^5       1.6e-19     4.4e-20
 */
/*
Cephes Math Library Release 2.9:  June, 2000
Copyright 2000 by Stephen L. Moshier
*/

#define expx2lM      32768.0
#define expx2lMINV   3.0517578125e-5

long double MAXLOGL =  1.1356523406294143949491931077970764891253E4L;
long double MINLOGL = -1.143276959615573793352782661133116431383730E4L;

/* exp(x*x) or exp(-x*x) */
long double expx2l (long double x, int sign) {
  long double u, u1, m, f;
  x = fabsl(x);
  if (sign < 0) x = -x;
  /* Represent x as an exact multiple of M plus a residual.
     M is a power of 2 chosen so that exp(m * m) does not overflow
     or underflow and so that |x - m| is small.  */
  m = expx2lMINV * sun_floorl(fmal(expx2lM, x, 0.5L));
  f = x - m;
  /* x^2 = m^2 + 2mf + f^2 */
  u = m*m;
  u1 = fmal(2*m, f, f*f);
  if (sign < 0) {
    u = -u;
    u1 = -u1;
  }
  if ((u + u1) > MAXLOGL) return HUGE_VALL;
  /* u is exact, u1 is small. */
  u = tools_expl(u)*tools_expl(u1);
  return(u);
}

/* Normal distribution function */
long double ndtrl (long double a) {
  long double x, y, z;
  x = a*SQRTHL;
  z = fabsl(x);
  if (z < 1.0)
    y = 0.5L + 0.5L*sun_erfl(x);
  else {
    /* See below for erfcel. */
    y = 0.5L * erfcel(z);
    /* Multiply by exp(-x^2 / 2)  */
    z = expx2l(a, -1);
    y = y * sqrtl(z);
    if (x > 0.0L) y = 1.0L - y;
  }
  return y;
}


/*							ndtril.c
 *
 * Inverse of Normal distribution function
 *
 * SYNOPSIS:
 *
 * long double x, y, ndtril();
 *
 * x = ndtril( y );
 *
 * DESCRIPTION:
 *
 * Returns the argument, x, for which the area under the
 * Gaussian probability density function (integrated from
 * minus infinity to x) is equal to y.
 *
 * For small arguments 0 < y < exp(-2), the program computes
 * z = sqrt( -2 log(y) );  then the approximation is
 * x = z - log(z)/z  - (1/z) P(1/z) / Q(1/z) .
 * For larger arguments,  x/sqrt(2 pi) = w + w^3 R(w^2)/S(w^2)) ,
 * where w = y - 0.5 .
 *
 * ACCURACY:
 *                      Relative error:
 * arithmetic   domain        # trials      peak         rms
 *  Arguments uniformly distributed:
 *    IEEE       0, 1           5000       7.8e-19     9.9e-20
 *  Arguments exponentially distributed:
 *    IEEE     exp(-11355),-1  30000       1.7e-19     4.3e-20
 *
 * ERROR MESSAGES:
 *
 *   message         condition    value returned
 * ndtril domain      x <= 0        -MAXNUML
 * ndtril domain      x >= 1         MAXNUML
 */
/*
Cephes Math Library Release 2.3:  January, 1995
Copyright 1984, 1995 by Stephen L. Moshier
*/

static long double s2pi = 2.506628274631000502416E0L;

static long double ndtrilP0[8] = {
 8.779679420055069160496E-3L,
-7.649544967784380691785E-1L,
 2.971493676711545292135E0L,
-4.144980036933753828858E0L,
 2.765359913000830285937E0L,
-9.570456817794268907847E-1L,
 1.659219375097958322098E-1L,
-1.140013969885358273307E-2L,
};

static long double ndtrilQ0[7] = {
/* 1.000000000000000000000E0L, */
-5.303846964603721860329E0L,
 9.908875375256718220854E0L,
-9.031318655459381388888E0L,
 4.496118508523213950686E0L,
-1.250016921424819972516E0L,
 1.823840725000038842075E-1L,
-1.088633151006419263153E-2L,
};

/* Approximation for interval z = sqrt(-2 log y ) between 2 and 8
 */
/*  ndtri(p) = z - ln(z)/z - 1/z P1(1/z)/Q1(1/z)
    z = sqrt(-2 ln(p))
    2 <= z <= 8, i.e., y between exp(-2) = .135 and exp(-32) = 1.27e-14.
    Peak relative error 5.3e-21  */
static long double ndtrilP1[10] = {
 4.302849750435552180717E0L,
 4.360209451837096682600E1L,
 9.454613328844768318162E1L,
 9.336735653151873871756E1L,
 5.305046472191852391737E1L,
 1.775851836288460008093E1L,
 3.640308340137013109859E0L,
 3.691354900171224122390E-1L,
 1.403530274998072987187E-2L,
 1.377145111380960566197E-4L,
};

static long double ndtrilQ1[9] = {
/* 1.000000000000000000000E0L, */
 2.001425109170530136741E1L,
 7.079893963891488254284E1L,
 8.033277265194672063478E1L,
 5.034715121553662712917E1L,
 1.779820137342627204153E1L,
 3.845554944954699547539E0L,
 3.993627390181238962857E-1L,
 1.526870689522191191380E-2L,
 1.498700676286675466900E-4L,
};

/* ndtri(x) = z - ln(z)/z - 1/z P2(1/z)/Q2(1/z)
   z = sqrt(-2 ln(y))
   8 <= z <= 32
   i.e., y between exp(-32) = 1.27e-14 and exp(-512) = 4.38e-223
   Peak relative error 1.0e-21  */
static long double ndtrilP2[8] = {
 3.244525725312906932464E0L,
 6.856256488128415760904E0L,
 3.765479340423144482796E0L,
 1.240893301734538935324E0L,
 1.740282292791367834724E-1L,
 9.082834200993107441750E-3L,
 1.617870121822776093899E-4L,
 7.377405643054504178605E-7L,
};

static long double ndtrilQ2[7] = {
/* 1.000000000000000000000E0L, */
 6.021509481727510630722E0L,
 3.528463857156936773982E0L,
 1.289185315656302878699E0L,
 1.874290142615703609510E-1L,
 9.867655920899636109122E-3L,
 1.760452434084258930442E-4L,
 8.028288500688538331773E-7L,
};

/*  ndtri(x) = z - ln(z)/z - 1/z P3(1/z)/Q3(1/z)
    32 < z < 2048/13
    Peak relative error 1.4e-20  */
static long double ndtrilP3[8] = {
 2.020331091302772535752E0L,
 2.133020661587413053144E0L,
 2.114822217898707063183E-1L,
-6.500909615246067985872E-3L,
-7.279315200737344309241E-4L,
-1.275404675610280787619E-5L,
-6.433966387613344714022E-8L,
-7.772828380948163386917E-11L,
};

static long double ndtrilQ3[7] = {
/* 1.000000000000000000000E0L, */
 2.278210997153449199574E0L,
 2.345321838870438196534E-1L,
-6.916708899719964982855E-3L,
-7.908542088737858288849E-4L,
-1.387652389480217178984E-5L,
-7.001476867559193780666E-8L,
-8.458494263787680376729E-11L,
};

/* Inverse of Normal distribution function */
long double ndtril (long double y0) {
  long double x, y, z, y2, x0, x1;
  int code;
  if (y0 <= 0.0L) {
    /* mtherr( "ndtril", DOMAIN ); */
    return -MAXNUML;
  }
  if (y0 >= 1.0L) {
    /* mtherr( "ndtri", DOMAIN ); */
    return MAXNUML;
	}
  code = 1;
  y = y0;
  if (y > (1.0L - 0.13533528323661269189L)) {  /* 0.135... = exp(-2) */
    y = 1.0L - y;
    code = 0;
  }
  if (y > 0.13533528323661269189L) {
    y = y - 0.5L;
    y2 = y*y;
    /* x = y + y*(y2*polevll(y2, ndtrilP0, 7)/p1evll(y2, ndtrilQ0, 7)); */
    x = fmal(y, y2*polevll(y2, ndtrilP0, 7)/p1evll(y2, ndtrilQ0, 7), y);
    return x*s2pi;
  }
  x = sqrtl(-2.0L * tools_logl(y));
  x0 = x - tools_logl(x)/x;
  z = 1.0L/x;
  if (x < 8.0L)
    x1 = z*polevll(z, ndtrilP1, 9)/p1evll(z, ndtrilQ1, 9);
  else if (x < 32.0L)
    x1 = z*polevll(z, ndtrilP2, 7)/p1evll(z, ndtrilQ2, 7);
  else
    x1 = z*polevll(z, ndtrilP3, 7)/p1evll(z, ndtrilQ3, 7);
  x = x0 - x1;
  if (code != 0) x = -x;
  return x;
}


/*							incbil()
 *
 * Inverse of incomplete beta integral
 *
 * SYNOPSIS:
 *
 * long double a, b, x, y, incbil();
 *
 * x = incbil( a, b, y );
 *
 * DESCRIPTION:
 *
 * Given y, the function finds x such that
 *
 *  incbet( a, b, x ) = y.
 *
 * the routine performs up to 10 Newton iterations to find the
 * root of incbet(a,b,x) - y = 0.
 *
 * ACCURACY:
 *                      Relative error:
 *                x       a,b
 * arithmetic   domain   domain   # trials    peak       rms
 *    IEEE      0,1    .5,10000    10000    1.1e-14   1.4e-16
 */
/*
Cephes Math Library Release 2.3:  March, 1995
Copyright 1984, 1995 by Stephen L. Moshier
*/

/* Inverse of incomplete beta integral */
long double incbil (long double aa, long double bb, long double yy0) {
  long double a, b, y0, d, y, x, x0, x1, lgm, yp, di, dithresh, yl, yh, xt;
  int i, rflg, dir, nflg;
  if (yy0 <= 0.0L) return 0.0L;
  if (yy0 >= 1.0L) return 1.0L;
  x0 = 0.0L;
  yl = 0.0L;
  x1 = 1.0L;
  yh = 1.0L;
  if( aa <= 1.0L || bb <= 1.0L ) {
    dithresh = 1.0e-7L;
    rflg = 0;
    a = aa;
    b = bb;
    y0 = yy0;
    x = a/(a + b);
    y = incbetl(a, b, x);
    nflg = 0;
    goto ihalve;
  } else {
    nflg = 0;
    dithresh = 1.0e-4L;
  }
  /* approximation to inverse function */
  yp = -ndtril( yy0 );
  if (yy0 > 0.5L) {
    rflg = 1;
    a = bb;
    b = aa;
    y0 = 1.0L - yy0;
    yp = -yp;
  } else {
    rflg = 0;
    a = aa;
    b = bb;
    y0 = yy0;
  }
  lgm = (yp*yp - 3.0L)/6.0L;
  x = 2.0L/( 1.0L/(2.0L*a - 1.0L) + 1.0L/(2.0L*b - 1.0L) );
  d = yp * sqrtl(x + lgm)/x
    - (1.0L/(2.0L*b - 1.0L) - 1.0L/(2.0L*a - 1.0L))
    * (lgm + (5.0L/6.0L) - 2.0L/(3.0L*x));
  d = 2.0L*d;
  if (d < MINLOGL) {
    x = 1.0L;
    goto under;
  }
  x = a/(a + b*tools_expl(d));
  y = incbetl(a, b, x);
  yp = (y - y0)/y0;
  if (fabsl(yp) < 0.2) goto newt;
  /* Resort to interval halving if not close enough. */
ihalve:
  dir = 0;
  di = 0.5L;
  for (i=0; i < 400; i++) {
    if (i != 0) {
      x = x0 + di*(x1 - x0);
      if (x == 1.0L) x = 1.0L - MACHEPL;
      if (x == 0.0L) {
        di = 0.5;
        /* x = x0 + di*(x1 - x0); */
        x = fmal(di, x1 - x0, x0);
        if (x == 0.0) goto under;
      }
      y = incbetl(a, b, x);
      yp = (x1 - x0)/(x1 + x0);
      if (fabsl(yp) < dithresh) goto newt;
      yp = (y - y0)/y0;
      if (fabsl(yp) < dithresh) goto newt;
    }
    if (y < y0) {
      x0 = x;
      yl = y;
      if (dir < 0) {
        dir = 0;
        di = 0.5L;
      }
      else if (dir > 3) {
        long double t = 1.0L - di;
        di = 1.0L - t*t;
      }
      else if (dir > 1)
        di = 0.5L*(di + 1.0L);
      else
        di = (y0 - y)/(yh - yl);
      dir += 1;
      if (x0 > 0.95L) {
        if (rflg == 1) {
          rflg = 0;
          a = aa;
          b = bb;
          y0 = yy0;
        } else {
          rflg = 1;
          a = bb;
          b = aa;
          y0 = 1.0 - yy0;
        }
        x = 1.0L - x;
        y = incbetl(a, b, x);
        x0 = 0.0;
        yl = 0.0;
        x1 = 1.0;
        yh = 1.0;
        goto ihalve;
      }
    } else {
      x1 = x;
      if (rflg == 1 && x1 < MACHEPL) {
        x = 0.0L;
        goto done;
      }
      yh = y;
      if (dir > 0) {
        dir = 0;
        di = 0.5L;
      }
      else if (dir < -3)
        di = di*di;
      else if (dir < -1)
        di = 0.5L*di;
      else
        di = (y - y0)/(yh - yl);
      dir -= 1;
    }
  }
  /* mtherr( "incbil", PLOSS ); */
  if (x0 >= 1.0L) {
    x = 1.0L - MACHEPL;
    goto done;
  }
  if (x <= 0.0L) {
under:
    /* mtherr( "incbil", UNDERFLOW ); */
    x = 0.0L;
    goto done;
  }
newt:
  if (nflg) goto done;
  nflg = 1;
  lgm = tools_lgammal(a + b) - tools_lgammal(a) - tools_lgammal(b);
  for (i=0; i < 15; i++) {
    /* Compute the function at this point. */
    if (i != 0) y = incbetl(a, b, x);
    if (y < yl) {
      x = x0;
      y = yl;
    } else if (y > yh) {
      x = x1;
      y = yh;
    } else if (y < y0) {
      x0 = x;
      yl = y;
    } else {
      x1 = x;
      yh = y;
    }
    if (x == 1.0L || x == 0.0L) break;
    /* Compute the derivative of the function at this point. */
    d = (a - 1.0L)*tools_logl(x) + (b - 1.0L)*tools_logl(1.0L - x) + lgm;
    if (d < MINLOGL) goto done;
    if (d > MAXLOGL) break;
    d = expl(d);
    /* Compute the step to the next approximation of x. */
    d = (y - y0)/d;
    xt = x - d;
    if (xt <= x0) {
      y = (x - x0)/(x1 - x0);
      /* xt = x0 + 0.5L*y*(x - x0); */
      xt = fmal(0.5L*y, x - x0, x0);
      if (xt <= 0.0L) break;
    }
    if (xt >= x1) {
      y = (x1 - x)/(x1 - x0);
      xt = x1 - 0.5L*y*(x1 - x);
      if (xt >= 1.0L) break;
    }
    x = xt;
    if (fabsl(d/x) < (128.0L*MACHEPL)) goto done;
  }
  /* Did not converge.  */
  dithresh = 256.0L*MACHEPL;
  goto ihalve;
done:
  if (rflg) {
  if (x <= MACHEPL)
    x = 1.0L - MACHEPL;
  else
    x = 1.0L - x;
  }
  return x;
}


/* Binomial distribution */
long double bdtrl (int k, int n, long double p) {
  long double dk, dn, q;
  if (p < 0.0L || p > 1.0L || k < 0 || n < k) {
    /* mtherr( "bdtrl", DOMAIN ); */
    return AGN_NAN;  /* 0.0L */
	}
  if (k == n) return 1.0L;
  q = 1.0L - p;
  dn = n - k;
  if (k == 0) {
    dk = tools_powl(q, dn);
  } else {
    dk = k + 1;
    dk = incbetl(dn, dk, q);
  }
  return dk;
}


/* Complemented binomial distribution */
long double bdtrcl (int k, int n, long double p) {
  long double dk, dn;
  if (p < 0.0L || p > 1.0L) return AGN_NAN;  /* 0.0L */
  if (k < 0) return 1.0L;
  if (n < k) {
    /* mtherr( "bdtrcl", DOMAIN ); */
    return AGN_NAN;  /* 0.0L; */
  }
  if (k == n) return 0.0L;
  dn = n - k;
  if (k == 0) {
    if (p < 0.01L)
      dk = -tools_expm1l(dn*tools_log1pl(-p));
    else
		  dk = 1.0L - tools_powl(1.0L - p, dn);
  } else {
    dk = k + 1;
    dk = incbetl(dk, dn, p);
  }
  return dk;
}


/* Inverse binomial distribution */
long double bdtril (int k, int n, long double y) {
  long double dk, dn, p;
  if (y < 0.0L || y > 1.0L || k < 0 || n <= k) {
    /* mtherr( "bdtril", DOMAIN ); */
    return AGN_NAN;  /* 0.0L */
  }
  dn = n - k;
  if (k == 0) {
    if (y > 0.8L)
      p = -tools_expm1l(tools_log1pl(y - 1.0L)/dn);
    else
      p = 1.0L - tools_powl(y, 1.0L/dn);
  } else {
    dk = k + 1;
    p = incbetl(dn, dk, y);
    if (p > 0.5)
      p = incbil(dk, dn, 1.0L - y);
    else
      p = 1.0 - incbil(dn, dk, y);
  }
  return p;
}


/*							incbetl.c
 *
 *	Incomplete beta integral
 *
 * SYNOPSIS:
 *
 * long double a, b, x, y, incbetl();
 *
 * y = incbetl( a, b, x );
 *
 * DESCRIPTION:
 *
 * Returns incomplete beta integral of the arguments, evaluated
 * from zero to x.  The function is defined as
 *
 *                  x
 *     -            -
 *    | (a+b)      | |  a-1     b-1
 *  -----------    |   t   (1-t)   dt.
 *   -     -     | |
 *  | (a) | (b)   -
 *                 0
 *
 * The domain of definition is 0 <= x <= 1.  In this
 * implementation a and b are restricted to positive values.
 * The integral from x to 1 may be obtained by the symmetry
 * relation
 *
 *    1 - incbet( a, b, x )  =  incbet( b, a, 1-x ).
 *
 * The integral is evaluated by a continued fraction expansion
 * or, when b*x is small, by a power series.
 *
 * ACCURACY:
 *
 * Tested at random points (a,b,x) with x between 0 and 1.
 * arithmetic   domain     # trials      peak         rms
 *    IEEE       0,5       20000        4.5e-18     2.4e-19
 *    IEEE       0,100    100000        3.9e-17     1.0e-17
 * Half-integer a, b:
 *    IEEE      .5,10000  100000        3.9e-14     4.4e-15
 * Outputs smaller than the IEEE gradual underflow threshold
 * were excluded from these statistics.
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * incbetl domain     x<0, x>1          0.0
 */
/*
Cephes Math Library, Release 2.3:  January, 1995
Copyright 1984, 1995 by Stephen L. Moshier
*/

#define MAXGAML 1755.455L
static long double big = 9.223372036854775808e18L;
static long double biginv = 1.084202172485504434007e-19L;

/* Power series for incomplete gamma integral. Use when b*x is small. */
static long double pseriesl (long double a, long double b, long double x) {
  long double s, t, u, v, n, t1, z, ai;
  ai = 1.0L/a;
  u = (1.0L - b)*x;
  v = u/(a + 1.0L);
  t1 = v;
  t = u;
  n = 2.0L;
  s = 0.0L;
  z = MACHEPL*ai;
  while (fabsl(v) > z) {
    u = (n - b)*x/n;
    t *= u;
    v = t/(a + n);
    s += v;
    n += 1.0L;
  }
  s += t1;
  s += ai;
  u = a * tools_logl(x);
  if ((a + b) < MAXGAML && fabsl(u) < MAXLOGL) {
    t = tools_gammal(a + b)/(tools_gammal(a)*tools_gammal(b));
    s = s*t*tools_powl(x, a);
  } else {
    t = tools_lgammal(a + b) - tools_lgammal(a) - tools_lgammal(b) + u + tools_logl(s);
    if (t < MINLOGL)
      s = 0.0L;
    else
      s = expl(t);
  }
  return s;
}

/* Continued fraction expansion #1 for incomplete beta integral */
static long double incbcfl (long double a, long double b, long double x) {
  long double xk, pk, pkm1, pkm2, qk, qkm1, qkm2, k1, k2, k3, k4, k5, k6, k7, k8, r, t, ans, thresh;
  int n;
  k1 = a;
  k2 = a + b;
  k3 = a;
  k4 = a + 1.0L;
  k5 = 1.0L;
  k6 = b - 1.0L;
  k7 = k4;
  k8 = a + 2.0L;
  pkm2 = 0.0L;
  qkm2 = 1.0L;
  pkm1 = 1.0L;
  qkm1 = 1.0L;
  ans = 1.0L;
  r = 1.0L;
  n = 0;
  thresh = 3.0L*MACHEPL;
  do {
    xk = -(x*k1*k2)/(k3*k4);
    pk = pkm1 + pkm2*xk;
    qk = qkm1 + qkm2*xk;
    pkm2 = pkm1;
    pkm1 = pk;
    qkm2 = qkm1;
    qkm1 = qk;
    xk = (x*k5*k6)/(k7*k8);
    pk = pkm1 + pkm2*xk;
    qk = qkm1 + qkm2*xk;
    pkm2 = pkm1;
    pkm1 = pk;
    qkm2 = qkm1;
    qkm1 = qk;
    if (qk != 0.0L)
		  r = pk/qk;
    if (r != 0.0L) {
      t = fabsl((ans - r)/r);
      ans = r;
    } else
      t = 1.0L;
    if (t < thresh) goto cdone;
    k1 += 1.0L;
    k2 += 1.0L;
    k3 += 2.0L;
    k4 += 2.0L;
    k5 += 1.0L;
    k6 -= 1.0L;
    k7 += 2.0L;
    k8 += 2.0L;
    if ((fabsl(qk) + fabsl(pk)) > big) {
      pkm2 *= biginv;
      pkm1 *= biginv;
      qkm2 *= biginv;
      qkm1 *= biginv;
    }
	  if ((fabsl(qk) < biginv) || (fabsl(pk) < biginv)) {
      pkm2 *= big;
      pkm1 *= big;
      qkm2 *= big;
      qkm1 *= big;
    }
  } while (++n < 400);
  /* mtherr( "incbetl", PLOSS ); */
cdone:
  return ans;
}

/* Continued fraction expansion #2 for incomplete beta integral */
static long double incbdl (long double a, long double b, long double x) {
  long double xk, pk, pkm1, pkm2, qk, qkm1, qkm2, k1, k2, k3, k4, k5, k6, k7, k8, r, t, ans, z, thresh;
  int n;
  k1 = a;
  k2 = b - 1.0L;
  k3 = a;
  k4 = a + 1.0L;
  k5 = 1.0L;
  k6 = a + b;
  k7 = a + 1.0L;
  k8 = a + 2.0L;
  pkm2 = 0.0L;
  qkm2 = 1.0L;
  pkm1 = 1.0L;
  qkm1 = 1.0L;
  z = x/(1.0L - x);
  ans = 1.0L;
  r = 1.0L;
  n = 0;
  thresh = 3.0L * MACHEPL;
  do {
    xk = -(z*k1*k2)/(k3*k4);
    pk = pkm1 + pkm2*xk;
    qk = qkm1 + qkm2*xk;
    pkm2 = pkm1;
    pkm1 = pk;
    qkm2 = qkm1;
    qkm1 = qk;
    xk = (z*k5*k6)/(k7*k8);
    pk = pkm1 + pkm2*xk;
    qk = qkm1 + qkm2*xk;
    pkm2 = pkm1;
    pkm1 = pk;
    qkm2 = qkm1;
    qkm1 = qk;
    if (qk != 0.0L) r = pk/qk;
    if (r != 0.0L) {
      t = fabsl((ans - r)/r);
      ans = r;
    } else
      t = 1.0L;
    if (t < thresh) goto cdone;
    k1 += 1.0L;
    k2 -= 1.0L;
    k3 += 2.0L;
    k4 += 2.0L;
    k5 += 1.0L;
    k6 += 1.0L;
    k7 += 2.0L;
    k8 += 2.0L;
    if ((fabsl(qk) + fabsl(pk)) > big) {
      pkm2 *= biginv;
      pkm1 *= biginv;
      qkm2 *= biginv;
      qkm1 *= biginv;
    }
    if ((fabsl(qk) < biginv) || (fabsl(pk) < biginv)) {
      pkm2 *= big;
      pkm1 *= big;
      qkm2 *= big;
      qkm1 *= big;
    }
  } while (++n < 400);
  /* mtherr( "incbetl", PLOSS ); */
cdone:
  return ans;
}

/* Incomplete beta integral */
long double incbetl (long double aa, long double bb, long double xx) {
  long double a, b, t, x, w, xc, y;
  int flag;
  if (aa <= 0.0L || bb <= 0.0L || xx <= 0.0L || xx >= 1.0L) {
    if (xx == 0.0L) return 0.0L;
    if (xx == 1.0L) return 1.0L;
      /* mtherr( "incbetl", DOMAIN ); */
    return AGN_NAN;  /* 0.0L */
  }
  flag = 0;
  if ((bb * xx) <= 1.0L && xx <= 0.95L) {
    t = pseriesl(aa, bb, xx);
    goto done;
  }
   w = 1.0L - xx;
  /* Reverse a and b if x is greater than the mean. */
  if (xx > (aa/(aa + bb))) {
    flag = 1;
    a = bb;
    b = aa;
    xc = xx;
    x = w;
  } else {
    a = aa;
    b = bb;
    xc = w;
    x = xx;
  }
  if (flag == 1 && (b*x) <= 1.0L && x <= 0.95L) {
    t = pseriesl(a, b, x);
    goto done;
  }
  /* Choose expansion for optimal convergence */
  y = x * (a + b - 2.0L) - (a - 1.0L);
  if (y < 0.0L)
    w = incbcfl(a, b, x);
  else
    w = incbdl(a, b, x)/xc;
  /* Multiply w by the factor
       a      b   _             _     _
      x  (1-x)   | (a+b) / ( a | (a) | (b) ) .   */
  y = a*tools_logl(x);
  t = b*tools_logl(xc);
  if ((a+b) < MAXGAML && fabsl(y) < MAXLOGL && fabsl(t) < MAXLOGL) {
    t = tools_powl(xc, b);
    t *= tools_powl(x, a);
    t /= a;
    t *= w;
    t *= tools_gammal(a + b)/(tools_gammal(a) * tools_gammal(b));
    goto done;
  } else {
    /* Resort to logarithms.  */
    y += t + tools_lgammal(a + b) - tools_lgammal(a) - tools_lgammal(b);
    y += tools_logl(w/a);
    if (y < MINLOGL)
      t = 0.0L;
   	else
   	  t = tools_expl(y);
  }
done:
  if (flag == 1) {
    if (t <= MACHEPL)
      t = 1.0L - MACHEPL;
    else
      t = 1.0L - t;
  }
  return t;
}


/* Merge of Cephes' sinh() and cosh() but without polynomial approximation around the origin, 6.6.6 */

static long double LOGE2L  = 6.9314718055994530941723E-1L;

void cephes_sinhcoshl (long double x, long double *sih, long double *coh) {
  long double a;
  a = fabsl(x);
  if (x > (MAXLOGL + LOGE2L) || x > -(MINLOGL-LOGE2L)) {
    *sih = (x > 0.0L) ? HUGE_VALL : -HUGE_VALL;
    *coh = HUGE_VALL;
  } else {
    if (a >= (MAXLOGL - LOGE2L)) {
      a = tools_expl(0.5L*a);
      a = (0.5L*a)*a;
      *coh = a;
      *sih = sun_copysignl(a, x);
    } else {
      long double y;
      a = tools_expl(a);
      y = 0.5L*a - (0.5L/a);
      *sih = sun_copysignl(y, x);
      *coh = 0.5L*(a + 1.0L/a);
    }
  }
}

