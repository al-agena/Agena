#include <stdlib.h>
#include <math.h>

#define sofa_c
#define LUA_LIB

#include "sofam.h"

/*----------------------------------------------------------------------
**
**  Copyright (C) 2017
**  Standards Of Fundamental Astronomy Board
**  of the International Astronomical Union.
**
**  =====================
**  SOFA Software License
**  =====================
**
**  NOTICE TO USER:
**
**  BY USING THIS SOFTWARE YOU ACCEPT THE FOLLOWING SIX TERMS AND
**  CONDITIONS WHICH APPLY TO ITS USE.
**
**  1. The Software is owned by the IAU SOFA Board ("SOFA").
**
**  2. Permission is granted to anyone to use the SOFA software for any
**     purpose, including commercial applications, free of charge and
**     without payment of royalties, subject to the conditions and
**     restrictions listed below.
**
**  3. You (the user) may copy and distribute SOFA source code to others,
**     and use and adapt its code and algorithms in your own software,
**     on a world-wide, royalty-free basis.  That portion of your
**     distribution that does not consist of intact and unchanged copies
**     of SOFA source code files is a "derived work" that must comply
**     with the following requirements:
**
**     a) Your work shall be marked or carry a statement that it
**        (i) uses routines and computations derived by you from
**        software provided by SOFA under license to you; and
**        (ii) does not itself constitute software provided by and/or
**        endorsed by SOFA.
**
**     b) The source code of your derived work must contain descriptions
**        of how the derived work is based upon, contains and/or differs
**        from the original SOFA software.
**
**     c) The names of all routines in your derived work shall not
**        include the prefix "iau" or "sofa" or trivial modifications
**        thereof such as changes of case.
**
**     d) The origin of the SOFA components of your derived work must
**        not be misrepresented;  you must not claim that you wrote the
**        original software, nor file a patent application for SOFA
**        software or algorithms embedded in the SOFA software.
**
**     e) These requirements must be reproduced intact in any source
**        distribution and shall apply to anyone to whom you have
**        granted a further right to modify the source code of your
**        derived work.
**
**     Note that, as originally distributed, the SOFA software is
**     intended to be a definitive implementation of the IAU standards,
**     and consequently third-party modifications are discouraged.  All
**     variations, no matter how minor, must be explicitly marked as
**     such, as explained above.
**
**  4. You shall not cause the SOFA software to be brought into
**     disrepute, either by misuse, or use for inappropriate tasks, or
**     by inappropriate modification.
**
**  5. The SOFA software is provided "as is" and SOFA makes no warranty
**     as to its use or performance.   SOFA does not and cannot warrant
**     the performance or results which the user may obtain by using the
**     SOFA software.  SOFA makes no warranties, express or implied, as
**     to non-infringement of third party rights, merchantability, or
**     fitness for any particular purpose.  In no event will SOFA be
**     liable to the user for any consequential, incidental, or special
**     damages, including any lost profits or lost savings, even if a
**     SOFA representative has been advised of such damages, or for any
**     claim by any third party.
**
**  6. The provision of any version of the SOFA software under the terms
**     and conditions specified herein does not imply that future
**     versions will also be made available under the same terms and
**     conditions.
*
**  In any published work or commercial product which uses the SOFA
**  software directly, acknowledgement (see www.iausofa.org) is
**  appreciated.
**
**  Correspondence concerning SOFA software should be addressed as
**  follows:
**
**      By email:  sofa@ukho.gov.uk
**      By post:   IAU SOFA Center
**                 HM Nautical Almanac Office
**                 UK Hydrographic Office
**                 Admiralty Way, Taunton
**                 Somerset, TA1 2DN
**                 United Kingdom
**
**--------------------------------------------------------------------*/

extern int iauCal2jd (int iy, int im, int id, double *djm0, double *djm) {
/*
**  - - - - - - - - - -
**   i a u C a l 2 j d
**  - - - - - - - - - -
**
**  Gregorian Calendar to Julian Date.
**
**  This function is part of the International Astronomical Union's
**  SOFA (Standards Of Fundamental Astronomy) software collection.
**
**  Status:  support function.
**
**  Given:
**     iy,im,id  int     year, month, day in Gregorian calendar (Note 1)
**
**  Returned:
**     djm0      double  MJD zero-point: always 2400000.5
**     djm       double  Modified Julian Date for 0 hrs
**
**  Returned (function value):
**               int     status:
**                           0 = OK
**                          -1 = bad year   (Note 3: JD not computed)
**                          -2 = bad month  (JD not computed)
**                          -3 = bad day    (JD computed)
**
**  Notes:
**
**  1) The algorithm used is valid from -4800 March 1, but this
**     implementation rejects dates before -4799 January 1.
**
**  2) The Julian Date is returned in two pieces, in the usual SOFA
**     manner, which is designed to preserve time resolution.  The
**     Julian Date is available as a single number by adding djm0 and
**     djm.
**
**  3) In early eras the conversion is from the "Proleptic Gregorian
**     Calendar";  no account is taken of the date(s) of adoption of
**     the Gregorian Calendar, nor is the AD/BC numbering convention
**     observed.
**
**  Reference:
**
**     Explanatory Supplement to the Astronomical Almanac,
**     P. Kenneth Seidelmann (ed), University Science Books (1992),
**     Section 12.92 (p604).
**
**  This revision:  2013 August 7
**
**  SOFA release 2017-04-20
**
**  Copyright (C) 2017 IAU SOFA Board.  See notes at end.
*/
  int j, ly, my;
  long iypmy;
  /* Earliest year allowed (4800BC) */
  const int IYMIN = -4799;
  /* Month lengths in days */
  static const int mtab[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  /* Preset status. */
  j = 0;
  /* Validate year and month. */
  if (iy < IYMIN) return -1;
  if (im < 1 || im > 12) return -2;
  /* If February in a leap year, 1, otherwise 0. */
  ly = ((im == 2) && !(iy % 4) && (iy % 100 || !(iy % 400)));
  /* Validate day, taking into account leap years. */
  if ( (id < 1) || (id > (mtab[im - 1] + ly))) j = -3;
  /* Return result. */
  my = (im - 14)/12;
  iypmy = (long) (iy + my);
  *djm0 = DJM0;  /* 2.10.3 */
  *djm = (double)((1461L*(iypmy + 4800L))/4L
                + (367L*(long) (im - 2 - 12*my))/12L
                - (3L*((iypmy + 4900L)/100L))/4L
                + (long) id - 2432076L);
/* Return status. */
  return j;
}


/*
**  - - - - - - - - - -
**   i a u J d 2 c a l
**  - - - - - - - - - -
**
**  Julian Date to Gregorian year, month, day, and fraction of a day.
**
**  This function is part of the International Astronomical Union's
**  SOFA (Standards Of Fundamental Astronomy) software collection.
**
**  Status:  support function.
**
**  Given:
**     dj1,dj2   double   Julian Date (Notes 1, 2)
**
**  Returned (arguments):
**     iy        int      year
**     im        int      month
**     id        int      day
**     fd        double   fraction of day
**
**  Returned (function value):
**               int      status:
**                           0 = OK
**                          -1 = unacceptable date (Note 3)
**
**  Notes:
**
**  1) The earliest valid date is -68569.5 (-4900 March 1).  The
**     largest value accepted is 10^9.
**
**  2) The Julian Date is apportioned in any convenient way between
**     the arguments dj1 and dj2.  For example, JD=2450123.7 could
**     be expressed in any of these ways, among others:
**
**            dj1             dj2
**
**         2450123.7           0.0       (JD method)
**         2451545.0       -1421.3       (J2000 method)
**         2400000.5       50123.2       (MJD method)
**         2450123.5           0.2       (date & time method)
**
**  3) In early eras the conversion is from the "proleptic Gregorian
**     calendar";  no account is taken of the date(s) of adoption of
**     the Gregorian calendar, nor is the AD/BC numbering convention
**     observed.
**
**  Reference:
**
**     Explanatory Supplement to the Astronomical Almanac,
**     P. Kenneth Seidelmann (ed), University Science Books (1992),
**     Section 12.92 (p604).
**
**  This revision:  2017 January 12
**
**  SOFA release 2017-04-20
**
**  Copyright (C) 2017 IAU SOFA Board.  See notes at end.
**
** Concerning leap years, this implementation does not work well with
** the Julian Calendar which has a leap year every 4 years (the year
** divisible by 4).
*/
/* Minimum and maximum allowed JD */
extern int iauJd2cal (double dj1, double dj2,
             int *iy, int *im, int *id, double *fd) {
  static const double djmin = -68569.5;
  static const double djmax = 1e9;
  long jd, l, n, i, k;
  double dj, d1, d2, f1, f2, f, d;
  /* Verify date is acceptable. */
  dj = dj1 + dj2;
  if (dj < djmin || dj > djmax) return -1;
  /* Copy the date, big then small, and re-align to midnight. */
  if (dj1 >= dj2) {
    d1 = dj1;
    d2 = dj2;
  } else {
    d1 = dj2;
    d2 = dj1;
  }
  d2 -= 0.5;
  /* Separate day and fraction. */
  f1 = fmod(d1, 1.0);
  f2 = fmod(d2, 1.0);
  f = fmod(f1 + f2, 1.0);
  if (f < 0.0) f += 1.0;
  d = dnint(d1 - f1) + dnint(d2 - f2) + dnint(f1 + f2 - f);  /* 2.10.3 changed from floor */
  jd = (long)dnint(d) + 1L;  /* ditto */
  /* Express day in Gregorian calendar. */
  l = jd + 68569L;
  n = (4L*l)/146097L;
  l -= (146097L*n + 3L)/4L;
  i = (4000L*(l + 1L))/1461001L;
  l -= (1461L*i)/4L - 31L;
  k = (80L*l)/2447L;
  *id = (int)(l - (2447L*k)/80L);
  l = k/11L;
  *im = (int)(k + 2L - 12L*l);
  *iy = (int)(100L*(n - 49L) + i + l);
  *fd = f;  /* this value may be too imprecise ! */
  return 0;
}

extern double iauJdfd (double dj1, double dj2) {  /* added 7.3.0 */
  static const double djmin = -68569.5;
  static const double djmax = 1e9;
  double dj, d1, d2, f1, f2, f;
  /* Verify date is acceptable. */
  dj = dj1 + dj2;
  if (dj < djmin || dj > djmax) return -1.0;
  /* Copy the date, big then small, and re-align to midnight. */
  if (dj1 >= dj2) {
    d1 = dj1;
    d2 = dj2;
  } else {
    d1 = dj2;
    d2 = dj1;
  }
  d2 -= 0.5;
  /* Separate day and fraction. */
  f1 = fmod(d1, 1.0);
  f2 = fmod(d2, 1.0);
  f = fmod(f1 + f2, 1.0);
  if (f < 0.0) f += 1.0;
  return f;
}

extern int iauTf2d (char s, int ihour, int imin, double sec, double *days) {
/*
**  - - - - - - - -
**   i a u T f 2 d
**  - - - - - - - -
**
**  Convert hours, minutes, seconds to days.
**
**  This function is part of the International Astronomical Union's
**  SOFA (Standards of Fundamental Astronomy) software collection.
**
**  Status:  support function.
**
**  Given:
**     s         char    sign:  '-' = negative, otherwise positive
**     ihour     int     hours
**     imin      int     minutes
**     sec       double  seconds
**
**  Returned:
**     days      double  interval in days
**
**  Returned (function value):
**               int     status:  0 = OK
**                                1 = ihour outside range 0-23
**                                2 = imin outside range 0-59
**                                3 = sec outside range 0-59.999...
**
**  Notes:
**
**  1)  The result is computed even if any of the range checks fail.
**
**  2)  Negative ihour, imin and/or sec produce a warning status, but
**      the absolute value is used in the conversion.
**
**  3)  If there are multiple errors, the status value reflects only the
**      first, the smallest taking precedence.
**
**  This revision:  2013 June 18
**
**  SOFA release 2017-04-20
**
**  Copyright (C) 2017 IAU SOFA Board.  See notes at end.
*/
/* Compute the interval. */
  *days = ( s == '-' ? -1.0 : 1.0 ) *
          ( 60.0*( 60.0*( (double)abs(ihour) ) +
                            ( (double)abs(imin) ) ) +
                                      fabs(sec) )/DAYSEC;
/* Validate arguments and return status. */
  if (ihour < 0 || ihour > 23) return 1;
  if (imin < 0 || imin > 59) return 2;
  if (sec < 0.0 || sec >= 60.0) return 3;
  return 0;
}


/* The following functions are wrappers to SOFA procedures, written by Alexander Walz, and
  subject to the SOFA Software License. */

extern double iauJuliandate (int yy, int mm, int dd, int h, int m, double s) {  /* January 07, 2013/7.8.6 double fix */
  double djm0, djm, djt;
  if (iauCal2jd(yy, mm, dd, &djm0, &djm) >= 0 && iauTf2d('+', h, m, s, &djt) == 0)  /* 5.5.6 change */
    return djm0 + djm + djt;
  else
    return HUGE_VAL;
}


/*
**  - - - - - - - -
**   i a u A 2 a f
**  - - - - - - - -
**
**  Decompose radians into degrees, arcminutes, arcseconds, fraction.
**
**  This function is part of the International Astronomical Union's
**  SOFA (Standards of Fundamental Astronomy) software collection.
**
**  Status:  vector/matrix support function.
**
**  Given:
**     ndp     int     resolution (Note 1)
**     angle   double  angle in radians
**
**  Returned:
**     sign    char*   '+' or '-'
**     idmsf   int[4]  degrees, arcminutes, arcseconds, fraction
**
**  Notes:
**
**  1) The argument ndp is interpreted as follows:
**
**     ndp         resolution
**      :      ...0000 00 00
**     -7         1000 00 00
**     -6          100 00 00
**     -5           10 00 00
**     -4            1 00 00
**     -3            0 10 00
**     -2            0 01 00
**     -1            0 00 10
**      0            0 00 01
**      1            0 00 00.1
**      2            0 00 00.01
**      3            0 00 00.001
**      :            0 00 00.000...
**
**  2) The largest positive useful value for ndp is determined by the
**     size of angle, the format of doubles on the target platform, and
**     the risk of overflowing idmsf[3].  On a typical platform, for
**     angle up to 2pi, the available floating-point precision might
**     correspond to ndp=12.  However, the practical limit is typically
**     ndp=9, set by the capacity of a 32-bit int, or ndp=4 if int is
**     only 16 bits.
**
**  3) The absolute value of angle may exceed 2pi.  In cases where it
**     does not, it is up to the caller to test for and handle the
**     case where angle is very nearly 2pi and rounds up to 360 degrees,
**     by testing for idmsf[0]=360 and setting idmsf[0-3] to zero.
**
**  Called:
**     iauD2tf      decompose days to hms
**
**  This revision:  2021 May 11
**
**  SOFA release 2023-10-11
**
**  Copyright (C) 2023 IAU SOFA Board.  See notes at end.
*/

extern void iauA2af (int ndp, double angle, char *sign, int idmsf[4]) {
  /* Hours to degrees*radians to turns */
  const double F = 15.0/D2PI;
  /* Scale then use days to h,m,s function. */
  iauD2tf(ndp, angle*F, sign, idmsf);
}

/*
**  - - - - - - - -
**   i a u D 2 t f
**  - - - - - - - -
**
**  Decompose days to hours, minutes, seconds, fraction.
**
**  This function is part of the International Astronomical Union's
**  SOFA (Standards of Fundamental Astronomy) software collection.
**
**  Status:  vector/matrix support function.
**
**  Given:
**     ndp     int     resolution (Note 1)
**     days    double  interval in days
**
**  Returned:
**     sign    char*   '+' or '-'
**     ihmsf   int[4]  hours, minutes, seconds, fraction
**
**  Notes:
**
**  1) The argument ndp is interpreted as follows:
**
**     ndp         resolution
**      :      ...0000 00 00
**     -7         1000 00 00
**     -6          100 00 00
**     -5           10 00 00
**     -4            1 00 00
**     -3            0 10 00
**     -2            0 01 00
**     -1            0 00 10
**      0            0 00 01
**      1            0 00 00.1
**      2            0 00 00.01
**      3            0 00 00.001
**      :            0 00 00.000...
**
**  2) The largest positive useful value for ndp is determined by the
**     size of days, the format of double on the target platform, and
**     the risk of overflowing ihmsf[3].  On a typical platform, for
**     days up to 1.0, the available floating-point precision might
**     correspond to ndp=12.  However, the practical limit is typically
**     ndp=9, set by the capacity of a 32-bit int, or ndp=4 if int is
**     only 16 bits.
**
**  3) The absolute value of days may exceed 1.0.  In cases where it
**     does not, it is up to the caller to test for and handle the
**     case where days is very nearly 1.0 and rounds up to 24 hours,
**     by testing for ihmsf[0]=24 and setting ihmsf[0-3] to zero.
**
**  This revision: 2021 May 11
**
**  SOFA release 2023-10-11
**
**  Copyright (C) 2023 IAU SOFA Board. See notes at end.
*/
extern void iauD2tf (int ndp, double days, char *sign, int ihmsf[4]) {
  int nrs, n;
  double rs, rm, rh, a, w, ah, am, as, af;
  /* Handle sign. */
  *sign = (char)(( days >= 0.0 ) ? '+' : '-');
  /* Interval in seconds. */
  a = DAYSEC*fabs(days);
  /* Pre-round if resolution coarser than 1s (then pretend ndp=1). */
  if (ndp < 0) {
    nrs = 1;
    for (n=1; n <= -ndp; n++) {
      nrs *= (n == 2 || n == 4) ? 6 : 10;
    }
    rs = (double)nrs;
    w = a/rs;
    a = rs*dnint(w);
  }
  /* Express the unit of each field in resolution units. */
  nrs = 1;
  for (n = 1; n <= ndp; n++) {
    nrs *= 10;
  }
  rs = (double)nrs;
  rm = rs*60.0;
  rh = rm*60.0;
  /* Round the interval and express in resolution units. */
  a = dnint(rs*a);
  /* Break into fields. */
  ah = a/rh;
  ah = dint(ah);
  a -= ah*rh;
  am = a/rm;
  am = dint(am);
  a -= am*rm;
  as = a/rs;
  as = dint(as);
  af = a - as*rs;
  /* Return results. */
  ihmsf[0] = (int)ah;
  ihmsf[1] = (int)am;
  ihmsf[2] = (int)as;
  ihmsf[3] = (int)af;
}


/*
**  - - - - - - - -
**   i a u A 2 t f
**  - - - - - - - -
**
**  Decompose radians into hours, minutes, seconds, fraction.
**
**  This function is part of the International Astronomical Union's
**  SOFA (Standards of Fundamental Astronomy) software collection.
**
**  Status:  vector/matrix support function.
**
**  Given:
**     ndp     int     resolution (Note 1)
**     angle   double  angle in radians
**
**  Returned:
**     sign    char*   '+' or '-'
**     ihmsf   int[4]  hours, minutes, seconds, fraction
**
**  Notes:
**
**  1) The argument ndp is interpreted as follows:
**
**     ndp         resolution
**      :      ...0000 00 00
**     -7         1000 00 00
**     -6          100 00 00
**     -5           10 00 00
**     -4            1 00 00
**     -3            0 10 00
**     -2            0 01 00
**     -1            0 00 10
**      0            0 00 01
**      1            0 00 00.1
**      2            0 00 00.01
**      3            0 00 00.001
**      :            0 00 00.000...
**
**  2) The largest positive useful value for ndp is determined by the
**     size of angle, the format of doubles on the target platform, and
**     the risk of overflowing ihmsf[3].  On a typical platform, for
**     angle up to 2pi, the available floating-point precision might
**     correspond to ndp=12.  However, the practical limit is typically
**     ndp=9, set by the capacity of a 32-bit int, or ndp=4 if int is
**     only 16 bits.
**
**  3) The absolute value of angle may exceed 2pi.  In cases where it
**     does not, it is up to the caller to test for and handle the
**     case where angle is very nearly 2pi and rounds up to 24 hours,
**     by testing for ihmsf[0]=24 and setting ihmsf[0-3] to zero.
**
**  Called:
**     iauD2tf      decompose days to hms
**
**  This revision:  2021 May 11
**
**  SOFA release 2023-10-11
**
**  Copyright (C) 2023 IAU SOFA Board.  See notes at end.
*/
extern void iauA2tf (int ndp, double angle, char *sign, int ihmsf[4]) {
  /* Scale then use days to h,m,s function. */
   iauD2tf(ndp, angle/D2PI, sign, ihmsf);
}



/*
**  - - - - - - - -
**   i a u A f 2 a
**  - - - - - - - -
**
**  Convert degrees, arcminutes, arcseconds to radians.
**
**  This function is part of the International Astronomical Union's
**  SOFA (Standards of Fundamental Astronomy) software collection.
**
**  Status:  support function.
**
**  Given:
**     s         char    sign:  '-' = negative, otherwise positive
**     ideg      int     degrees
**     iamin     int     arcminutes
**     asec      double  arcseconds
**
**  Returned:
**     rad       double  angle in radians
**
**  Returned (function value):
**               int     status:  0 = OK
**                                1 = ideg outside range 0-359
**                                2 = iamin outside range 0-59
**                                3 = asec outside range 0-59.999...
**
**  Notes:
**
**  1)  The result is computed even if any of the range checks fail.
**
**  2)  Negative ideg, iamin and/or asec produce a warning status, but
**      the absolute value is used in the conversion.
**
**  3)  If there are multiple errors, the status value reflects only the
**      first, the smallest taking precedence.
**
**  This revision:  2021 May 11
**
**  SOFA release 2023-10-11
**
**  Copyright (C) 2023 IAU SOFA Board.  See notes at end.
*/
extern int iauAf2a (char s, int ideg, int iamin, double asec, double *rad) {
/* Compute the interval. */
  *rad  = ( s == '-' ? -1.0 : 1.0 ) *
          ( 60.0 * ( 60.0 * ( (double) abs(ideg) ) +
                            ( (double) abs(iamin) ) ) +
                                       fabs(asec) ) * DAS2R;
  /* Validate arguments and return status. */
  if ( ideg < 0 || ideg > 359 ) return 1;
  if ( iamin < 0 || iamin > 59 ) return 2;
  if ( asec < 0.0 || asec >= 60.0 ) return 3;
  return 0;
}



