/*
 * Copyright © 2010 Guido Trentalancia IZ6RDB
 * This program is freeware, however it is provided as is, without any warranty.
 *
 * Calculate Moon rise and set time
 *
 * This program is the translation in C of the Javascript algorithm by Stephen R. Schmitt.
 * For the original program, see:
 *   http://mysite.verizon.net/res148h4j/javascript/script_moon_rise_set.html
 *
 * Before compiling, please edit the longitude and latitude variables in main() according
 * to the desired location.
 */

#include <math.h>

#include "agnhlps.h"
#include "sofa.h"

typedef enum { false, true } bool;

static double Sky[3] = { 0.0, 0.0, 0.0 };
static double Dec[3] = { 0.0, 0.0, 0.0 };
static double VHz[3] = { 0.0, 0.0, 0.0 };
static double RAn[3] = { 0.0, 0.0, 0.0 };

const static double DR = M_PI/180;
const static double K1 = 15*M_PI*1.0027379/180;
static double Rise_az = 0.0, Set_az = 0.0;
static int Rise_time[2] = { 0.0, 0.0 }, Set_time[2] = { 0.0, 0.0 };

static bool Moonrise = false, Moonset = false;

/* moon's position using fundamental arguments (Van Flandern & Pulkkinen, 1979) */
static void moon (double jd) {
  double d, f, g, h, m, n, s, u, v, w;
  h = 0.606434 + 0.03660110129*jd;
  m = 0.374897 + 0.03629164709*jd;
  f = 0.259091 + 0.03674819520*jd;
  d = 0.827362 + 0.03386319198*jd;
  n = 0.347343 - 0.00014709391*jd;
  g = 0.993126 + 0.00273777850*jd;
  h = h - sun_floor(h);
  m = m - sun_floor(m);
  f = f - sun_floor(f);
  d = d - sun_floor(d);
  n = n - sun_floor(n);
  g = g - sun_floor(g);
  h *= 2*M_PI;
  m *= 2*M_PI;
  f *= 2*M_PI;
  d *= 2*M_PI;
  n *= 2*M_PI;
  g *= 2*M_PI;
  v = 0.39558*sun_sin(f + n);
  v = v + 0.08200*sun_sin(f);
  v = v + 0.03257*sun_sin(m - f - n);
  v = v + 0.01092*sun_sin(m + f + n);
  v = v + 0.00666*sun_sin(m - f);
  v = v - 0.00644*sun_sin(m + f - 2*d + n);
  v = v - 0.00331*sun_sin(f - 2 * d + n);
  v = v - 0.00304*sun_sin(f - 2 * d);
  v = v - 0.00240*sun_sin(m - f - 2*d - n);
  v = v + 0.00226*sun_sin(m + f);
  v = v - 0.00108*sun_sin(m + f - 2*d);
  v = v - 0.00079*sun_sin(f - n);
  v = v + 0.00078*sun_sin(f + 2*d + n);
  u = 1 - 0.10828*sun_cos(m);
  u = u - 0.01880*sun_cos(m - 2*d);
  u = u - 0.01479*sun_cos(2*d);
  u = u + 0.00181*sun_cos(2*m - 2*d);
  u = u - 0.00147*sun_cos(2*m);
  u = u - 0.00105*sun_cos(2*d - g);
  u = u - 0.00075*sun_cos(m - 2*d + g);
  w = 0.10478*sun_sin(m);
  w = w - 0.04105*sun_sin(2*f + 2*n);
  w = w - 0.0213 *sun_sin(m - 2*d);
  w = w - 0.01779*sun_sin(2*f + n);
  w = w + 0.01774*sun_sin(n);
  w = w + 0.00987*sun_sin(2*d);
  w = w - 0.00338*sun_sin(m - 2*f - 2*n);
  w = w - 0.00309*sun_sin(g);
  w = w - 0.0019 *sun_sin(2*f);
  w = w - 0.00144*sun_sin(m + n);
  w = w - 0.00144*sun_sin(m - 2*f - n);
  w = w - 0.00113*sun_sin(m + 2*f + 2*n);
  w = w - 0.00094*sun_sin(m - 2*d + g);
  w = w - 0.00092*sun_sin(2*m - 2*d);
  s = w/sqrt(u - v*v);	/* compute moon's right ascension ...  */
  Sky[0] = h + sun_atan(s/sqrt(1 - s*s));
  s = v/sqrt(u);		/* declination ... */
  Sky[1] = sun_atan(s/sqrt(1 - s*s));
  Sky[2] = 60.40974*sqrt(u);	/* and parallax */
}

/* test an hour for an event */
double test_moon (int k, double t0, double lat, double plx) {
  double ha[3] = { 0.0, 0.0, 0.0 };
  double a, b, c, d, e, s, z, hr, min, time, az, hz, nz, dz;
  if (RAn[2] < RAn[0]) RAn[2] = RAn[2] + 2*M_PI;
  ha[0] = t0 - RAn[0] + k*K1;
  ha[2] = t0 - RAn[2] + k*K1 + K1;
  ha[1] = 0.5*(ha[2] + ha[0]);	   /* hour angle at half hour */
  Dec[1] = 0.5*(Dec[2] + Dec[0]);	 /* declination at half hour */
  s = sun_sin(DR*lat);
  c = sun_cos(DR*lat);
  /* refraction + sun semidiameter at horizon + parallax correction */
  z = sun_cos(DR*(90.567 - 41.685/plx));
  if (k <= 0)			/* first call of function */
	VHz[0] = s*sun_sin(Dec[0]) + c*sun_cos(Dec[0])*sun_cos(ha[0]) - z;
  VHz[2] = s*sun_sin(Dec[2]) + c*sun_cos(Dec[2])*sun_cos(ha[2]) - z;
  if (dsign(VHz[0]) == dsign(VHz[2]))
	return VHz[2];		/* no event this hour */
  VHz[1] = s*sun_sin(Dec[1]) + c*sun_cos(Dec[1])*sun_cos(ha[1]) - z;
  a = 2*VHz[2] - 4*VHz[1] + 2*VHz[0];
  b = 4*VHz[1] - 3*VHz[0] - VHz[2];
  d = b*b - 4*a*VHz[0];
  if (d < 0) return VHz[2];  /*  no event this hour */
  d = sqrt(d);
  e = (-b + d)/(2*a);
  if ((e > 1) || (e < 0))
	e = (-b - d)/(2*a);
  time = (double)k + e + 1/120;  /* time of an event + round up */
  hr = sun_floor(time);
  min = sun_floor((time - hr)*60);
  hz = ha[0] + e*(ha[2] - ha[0]);  /* azimuth of the moon at the event */
  nz = -sun_cos(Dec[1])*sun_sin(hz);
  dz = c*sun_sin(Dec[1]) - s*sun_cos(Dec[1])*sun_cos(hz);
  az = sun_atan2(nz, dz)/DR;
  if (az < 0)
	az += 360;
  if ((VHz[0] < 0) && (VHz[2] > 0)) {
    Rise_time[0] = (int)hr;
    Rise_time[1] = (int)min;
    Rise_az = az;
    Moonrise = true;
  }
  if ((VHz[0] > 0) && (VHz[2] < 0)) {
    Set_time[0] = (int)hr;
    Set_time[1] = (int)min;
    Set_az = az;
    Moonset = true;
  }
  return VHz[2];
}

/* Local Sidereal Time for zone */
extern double lst (const double lon, const double jd, const double z) {
  double s = 24110.5 + 8640184.812999999*jd/36525 + 86636.6 * z + 86400*lon;
  s /= 86400;
  s = s - sun_floor(s);
  return s*360*DR;
}

/* 3-point interpolation */
double interpolate (const double f0, const double f1, const double f2, const double p) {
  double a = f1 - f0;
  double b = f2 - f1 - a;
  double f = f0 + p*(2*a + b*(2*p - 1));
  return f;
}

/* calculate moonrise and moonset times */
extern void riseset (const double lat, const double lon, const int day,
	     const int month, const int year, const int TimezoneOffset, double *rise, double *set) {
  int i, j, k;
  int zone = rint(TimezoneOffset/60);
  double ph, jd, tz, t0, mp[3][3], lon_local;
  /* guido: julian day has been converted to int from double */
  jd = (iauJuliandate(year, month, day, 0, 0, 0)) - 2451545;	/* Julian day relative to Jan 1.5, 2000 */
  lon_local = lon;
  for (i=0; i < 3; i++) {
    for (j = 0; j < 3; j++) mp[i][j] = 0.0;
  }
  lon_local = lon/360;
  tz = ((double)zone)/24;
  t0 = lst(lon_local, jd, tz);	/* local sidereal time */
  jd = jd + tz;  /* get moon position at start of day */
  for (k=0; k < 3; k++) {
    moon(jd);
    mp[k][0] = Sky[0];
    mp[k][1] = Sky[1];
    mp[k][2] = Sky[2];
    jd += 0.5;
  }
  if (mp[1][0] <= mp[0][0]) mp[1][0] = mp[1][0] + 2*M_PI;
  if (mp[2][0] <= mp[1][0]) mp[2][0] = mp[2][0] + 2*M_PI;
  RAn[0] = mp[0][0];
  Dec[0] = mp[0][1];
  Moonrise = false;		/* initialize */
  Moonset = false;
  for (k=0; k < 24; k++) {  /* check each hour of this day */
    ph = ((double)(k + 1))/24;
    RAn[2] = interpolate(mp[0][0], mp[1][0], mp[2][0], ph);
    Dec[2] = interpolate(mp[0][1], mp[1][1], mp[2][1], ph);
    VHz[2] = test_moon(k, t0, lat, mp[1][2]);
    RAn[0] = RAn[2];	/* advance to next hour */
    Dec[0] = Dec[2];
    VHz[0] = VHz[2];
  }
  *rise = (double)Rise_time[0] + (double)Rise_time[1]/60;
  *set = (double)Set_time[0] + (double)Set_time[1]/60;
  /* reset so that when called another time, former results might not be used, 7.3.6 */
  Rise_time[0] = 0.0;
  Rise_time[1] = 0.0;
  Set_time[0] = 0.0;
  Set_time[1] = 0.0;
}


/* The following moon phase functions have been taken from: http://www.voidware.com/moon_phase.htm */

#define		RAD	(PI/180.0)
#define     SMALL_FLOAT	(1e-12)

static double sun_position (double j) {
  double n, x, e, l, dl, v;
  int i;
  n = 360/365.2422*j;
  i = n/360;
  n = n - i*360.0;
  x = n - 3.762863;
  if (x < 0) x += 360;
  x *= RAD;
  e = x;
  do {
    dl = e - 0.016718*sun_sin(e) - x;
    e = e - dl/(1 - 0.016718*sun_cos(e));  /* 2.40.2 tweak */
  } while (fabs(dl) >= SMALL_FLOAT);
  v = 360/PI*sun_atan(1.01686011182*sun_tan(e/2));  /* 2.40.2 tweak */
  l = v + 282.596403;
  i = l/360;
  l = l - i*360.0;
  return l;
}

static double moon_position (double j, double ls) {
  double ms, l, mm, n, ev, sms, ae, ec;
  int i;
  /* ls = sun_position(j) */
  ms = 0.985647332099*j - 3.762863;
  if (ms < 0) ms += 360.0;
  l = 13.176396*j + 64.975464;
  i = l/360;
  l = l - i*360.0;
  if (l < 0) l += 360.0;
  mm = l-0.1114041*j - 349.383063;
  i = mm/360;
  mm -= i*360.0;
  n = 151.950429 - 0.0529539*j;
  i = n/360;
  n -= i*360.0;
  ev = 1.2739*sun_sin((2*(l - ls) - mm)*RAD);  /* 2.40.2 tweak */
  sms = sun_sin(ms*RAD);  /* 2.40.2 tweak */
  ae = 0.1858*sms;
  mm += ev - ae - 0.37*sms;
  ec = 6.2886*sun_sin(mm*RAD);  /* 2.40.2 tweak */
  l += ev + ec - ae + 0.214*sun_sin(2*mm*RAD);  /* 2.40.2 tweak */
  l= 0.6583*sun_sin(2*(l - ls)*RAD) + l;  /* 2.40.2 tweak */
  return l;
}

extern double moon_phase (int year, int month, int day, double hour, int *ip) {
  /*
    Calculates more accurately than Moon_phase , the phase of the moon at
    the given epoch.
  */
  double j, ls, lm, t;
  j = iauJuliandate(year, month, day, hour, 0, 0) - 2444238.5;
  ls = sun_position(j);
  lm = moon_position(j, ls);
  t = lm - ls;
  if (t < 0) t += 360;
  *ip = (int)((t + 22.5)/45) & 0x7;
  return (1.0 - sun_cos((lm - ls)*RAD))/2;  /* 2.40.2 tweak */
}

