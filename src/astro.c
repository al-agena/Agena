/*
** $Id: astro.c, initiated January 06, 2013 $
** Astronomy library
** See Copyright Notice in agena.h
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define astro_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnhlps.h"
#include "agnt64.h"
#include "agnxlib.h"
#include "dcastro.h"
#include "moon.h"
#include "sdncal.h"
#include "sofa.h"
#include "sunriset.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_ASTROLIBNAME "astro"
LUALIB_API int (luaopen_astro) (lua_State *L);
#endif


static const char *const bodies[] = {  /* from index 0 */
  "Mercury", "Venus", "Earth", "Mars", "Jupiter",
  "Saturn", "Uranus", "Neptune", "Pluto",
  "Sun", "Moon", "EMB", "SSB", NULL
};

static int ndays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static void aux_checkdatetime (lua_State *L,
    int yy, int mm, int dd, int h, int m, double s, int ms, const char *procname) {
  if (h == 24 && m == 0 && s == 0.0) return;
  if (mm < 1 || h < 0 || m < 0 || s < 0 || ms < 0)
    luaL_error(L, "Error in " LUA_QS ": invalid date.", procname);
  if (mm > 12 || h > 23 || m > 59 || s >= 60 || ms > 999)
    luaL_error(L, "Error in " LUA_QS ": invalid date.", procname);
  if (dd > ndays[mm - 1] + (mm == 2) * tools_isleapyear(yy))  /* 1.9.1 */
    luaL_error(L, "in `%s`: day must be in the range [1, %d].", procname, ndays[mm - 1]);
}

static void aux_fetchdate (lua_State *L, int idx, int *yy, int *mm, int *dd,
  int *h, int *m, double *s, int *ms, int issueerror, const char *procname) {  /* 5.5.6 */
  int r = 0;
  if (lua_istable(L, idx)) {
    int rc;
    r = luaL_getn(L, idx);
    *yy = agn_rawgetiinteger(L, idx, 1, &rc);  /* defaults to 0 */
    *mm = agn_rawgetiinteger(L, idx, 2, &rc);  /* ditto */
    *dd = agn_rawgetiinteger(L, idx, 3, &rc);  /* ditto */
    *h =  (r > 3) ? agn_rawgetiinteger(L, idx, 4, &rc) : 0;
    *m =  (r > 4) ? agn_rawgetiinteger(L, idx, 5, &rc) : 0;
    *s =  (r > 5) ? agn_rawgetinumber(L,  idx, 6, &rc) : 0;
    *ms = (r > 6) ? agn_rawgetinumber(L,  idx, 7, &rc) : 0;  /* 7.8.5 extension */
  } else {
    int nargs = lua_gettop(L);
    *yy = agn_checkinteger(L, idx);
    *mm = agn_checkinteger(L, idx + 1);
    *dd = agn_checkinteger(L, idx + 2);
    *h =  (nargs > 3) ? luaL_optint(L, idx + 3, 0) : 0;
    *m =  (nargs > 4) ? luaL_optint(L, idx + 4, 0) : 0;
    *s =  (nargs > 5) ? luaL_optnumber(L, idx + 5, 0) : 0;
    *ms = (nargs > 6) ? luaL_optnumber(L, idx + 6, 0) : 0;  /* 7.8.5 extension */
  }
  if (issueerror)
    aux_checkdatetime(L, *yy, *mm, *dd, *h, *m, *s, *ms, procname);
}

static void aux_fetchtime (lua_State *L, int idx, int *h, int *m, double *s) {  /* 5.5.6 */
  int rc;
  if (lua_istable(L, idx)) {
    *h = agn_rawgetiinteger(L, idx, 1, &rc);  /* defaults to 0 */
    *m = agn_rawgetiinteger(L, idx, 2, &rc);  /* ditto */
    *s = agn_rawgetinumber(L,  idx, 3, &rc);  /* ditto */
  } else {
    *h = agnL_optinteger(L, idx, 0);
    *m = agnL_optinteger(L, idx + 1, 0);
    *s = agnL_optnumber(L,  idx + 2, 0);
  }
}

static int aux_fetchdatetime (lua_State *L, int idx, int nargs, astro_time_t *atime, const char *procname) {
  int y, d, m, hh, mm, ms, rc;
  double ss, jd;
  /* Count the actual remaining arguments available for date parsing */
  int newargs = nargs - idx + 1;
  if (newargs <= 0) {  /* No time arguments provided -> get current time */
    jd = tools_getcurrentjd(&rc);
    if (rc) return 0;
    *atime = Astronomy_TimeFromDays(jd - JD_J2000_EPOCH);
  } else if (newargs == 1 && agn_isnumber(L, idx) && agn_tonumber(L, idx) > 100000.0) {
    jd = agn_tonumber(L, idx);
    *atime = Astronomy_TimeFromDays(jd - JD_J2000_EPOCH);
  } else {  /* Calendar fields provided */
    aux_fetchdate(L, idx, &y, &m, &d, &hh, &mm, &ss, &ms, 1, procname);
    *atime = Astronomy_MakeTime(y, m, d, hh, mm, ss + ms/1000.0);
  }
  return 1;
}

static int aux_getdatetimefields (lua_State *L, int nargs,   /* 7.8.8 proposed by Gemini AI */
    int *iy, int *im, int *id, int *ihh, int *imm, double *isec,
    const char *procname) {
  int y, m, d, hh, mm, ms;
  double ss;
  /* Calculate the number of arguments available for date parsing */
  int newargs = nargs - 1 + 1;
  if (newargs > 1 || (newargs == 1 && !agn_isnumber(L, 1))) {
    /* individual time components or table of time components: do not convert back and forth to/from jd  */
    aux_fetchdate(L, 1, &y, &m, &d, &hh, &mm, &ss, &ms, 1, procname);
    *isec = ss + (ms/1000.0);
  } else {  /* current system time or Julian Date */
    double whole_sec, frac_sec;
    astro_time_t atime = { 0 };
    astro_utc_t utc = { 0 };
    if (!aux_fetchdatetime(L, 1, nargs, &atime, procname)) return 0;
    utc = Astronomy_UtcFromTime(atime);
    /* CHRONOLOGICAL TRUNCATION: Clean up hardware binary drift. If seconds are fractions of
       a microsecond past or before a whole second, snap them perfectly back onto the grid. */
    frac_sec = sun_modf(utc.second, &whole_sec);
    if (frac_sec > 0.999) {
      utc.second = whole_sec + 1.0;
      if (utc.second >= 60.0) {
        utc.second = 0.0;
        utc.minute += 1;
        if (utc.minute >= 60) {
          utc.minute = 0;
          utc.hour += 1;
        }
      }
    } else if (frac_sec < 0.001) {
      utc.second = whole_sec; /* Drops trailing microsecond noise blocks */
    }
    y = utc.year; m = utc.month; d = utc.day;
    hh = utc.hour; mm = utc.minute; *isec = utc.second;
  }
  *iy = y; *im = m; *id = d; *ihh = hh; *imm = mm;
  return (y == 0 && m == 0 && d == 0) ? 0 : 1;
}

#define aux_fractionalday(hh,mm,ss)  ((hh + mm/60.0 + (ss/3600.0))/24.0)


/* Returns the sunrise/sunset times in UTC for years starting with 1800 A.D. to 2099 A.D. It is a workhorse function,
   maybe you would like to use `astro.sunriset` for a more convenient interface.

   year, month and day, all integers, are the values of the day to evaluate. lon is the longitude (west/east),
   and lat the latitude (west/east), both in decimal degrees of type float of the location that is of interest.
   Use astro.todec to convert coordinates containing degrees (integer), minutes (integer), and seconds (integer
   or float), and the orientation to decimal degrees.

   Example: astro.sunriseset(2013, 1, 7, astro.dmstodec(6, 46, 58, 'E'), astro.dmstodec(51, 13, 32, 'N'))

   The first and second returns are the sunrise/sunset times which are considered to occur when the Sun's upper
   limb is 35 arc minutes below the horizon (this accounts for the refraction of the Earth's atmosphere).

   The third return is 0, if the rises and sun sets in a day; +1 if the Sun is above the specified "horizon" 24 hours,
   -1 if the Sun is below the specified "horizon" 24 hours.

   The fourth and fifth returns are start and end times of civil twilight. Civil twilight starts/ends when the
   Sun's centre is 6 degrees below the horizon.

   The sixth return is 0, if the rises and sun sets in a day; +1 if the Sun is above the specified "civil twilight
   horizon" 24 hours, -1 if the Sun is below the specified "horizon" 24 hours.

   The seventh and eighth returns are the start and end times of nautical twilight. Nautical twilight starts/ends
   when the Sun's centre is 12 degrees below the horizon.

   The ninth return is 0, if the rises and sun sets in a day; +1 if the Sun is above the specified "nautical twilight
   horizon" 24 hours, -1 if the Sun is below the specified "horizon" 24 hours.

   The tenth and eleventh returns are the start and end times of astronomical twilight. Astronomical twilight starts/ends
   when the Sun's centre is 18 degrees below the horizon.

   The twelfth return is 0, if the rises and sun sets in a day; +1 if the Sun is above the specified "nautical twilight
   horizon" 24 hours, -1 if the Sun is below the specified "astronomical twilight horizon" 24 hours.

   The thirteenth return is the time when the Sun is at south (in decimal UTC).

   All times returned are given in decimal hours of type number. Use `astro.dectotm` to convert them into `tm` notation.
*/

static void checkdateloc (lua_State *L, int year, int month, int day, lua_Number lon, lua_Number lat, const char *procname) {
  if (year < 1801 || year > 2099)
    luaL_error(L, "in `%s`: year must be in the range [1801, 2099].", procname);
  if (month < 1 || month > 12)
    luaL_error(L, "in `%s`: month must be in the range [1, 12].", procname);
  if (day < 1 || day > ndays[month - 1] + (month == 2) * tools_isleapyear(year))  /* 1.9.1 */
    luaL_error(L, "in `%s`: day must be in the range [1, %d].", ndays[month - 1], procname);
  if (lon < -180 || lon > 180)  /* longitude = Längengrad (E/W) */
    luaL_error(L, "in `%s`: longitude must be in the range [-180, 180].", procname);
  if (lat < -90 || lat > 90)  /* latitude = Breitengrad (N/S) */
    luaL_error(L, "in `%s`: latitude must be in the range [-90, 90].", procname);
}

static int astro_sunriseset (lua_State *L) {
  int year, month, day, r[4], nargs;
  lua_Number lon, lat, rise, set, civilrise, civilset, nauticalrise, nauticalset, astrorise, astroset, south;
  nargs = lua_gettop(L);
  year = agn_checknumber(L, 1);
  month = agn_checknumber(L, 2);
  day = agn_checknumber(L, 3);
  lon = agn_checknumber(L, 4);
  lat = agn_checknumber(L, 5);
  checkdateloc(L, year, month, day, lon, lat, "astro.sunriseset");
  r[0] = sun_rise_set(year, month, day, lon, lat, &rise, &set, &south);
  r[1] = civil_twilight(year, month, day, lon, lat, &civilrise, &civilset, &south);
  r[2] = nautical_twilight(year, month, day, lon, lat, &nauticalrise, &nauticalset, &south);
  r[3] = astronomical_twilight(year, month, day, lon, lat, &astrorise, &astroset, &south);
  if (nargs > 5) {  /* 5.5.4 */
    lua_createtable(L, 13, 0);
    lua_rawsetinumber(L, -1,  1, rise);
    lua_rawsetinumber(L, -1,  2, set);
    lua_rawsetinumber(L, -1,  3, r[0]);
    /* 0 if Sun is not above/below the limb, +1 if sun is above the limb for 24 hours, -1 if it is below the limb for 24 hours. */
    lua_rawsetinumber(L, -1,  4, civilrise);
    lua_rawsetinumber(L, -1,  5, civilset);
    lua_rawsetinumber(L, -1,  6, r[1]);
    lua_rawsetinumber(L, -1,  7, nauticalrise);
    lua_rawsetinumber(L, -1,  8, nauticalset);
    lua_rawsetinumber(L, -1,  9, r[2]);
    lua_rawsetinumber(L, -1, 10, astrorise);
    lua_rawsetinumber(L, -1, 11, astroset);
    lua_rawsetinumber(L, -1, 12, r[3]);
    lua_rawsetinumber(L, -1, 13, south);
    return 1;
  } else {
    luaL_checkstack(L, 13, "not enough stack space");  /* 3.15.5 fix */
    lua_pushnumber(L, rise);
    lua_pushnumber(L, set);
    lua_pushnumber(L, r[0]);
    /* 0 if Sun is not above/below the limb, +1 if sun is above the limb for 24 hours, -1 if it is below the limb for 24 hours. */
    lua_pushnumber(L, civilrise);
    lua_pushnumber(L, civilset);
    lua_pushnumber(L, r[1]);
    lua_pushnumber(L, nauticalrise);
    lua_pushnumber(L, nauticalset);
    lua_pushnumber(L, r[2]);
    lua_pushnumber(L, astrorise);
    lua_pushnumber(L, astroset);
    lua_pushnumber(L, r[3]);
    lua_pushnumber(L, south);
    return 13;
  }
}


static int checkcoords (lua_State *L, lua_Number x, lua_Number y, lua_Number z, const char *d, const char *procname) {
  if (!tools_streqx(d, "N", "S", "W", "E", NULL))  /* 2.25.1 tweak */
    luaL_error(L, "Error in " LUA_QS ": fourth argument, unknown orientation `%s`.", procname, d);
  if (tools_streqx(d, "N", "S", NULL) && (x < 0 || x >= 90))  /* 2.25.1 tweak */
    luaL_error(L, "Error in " LUA_QS ": latitude must be in the range [0, 90[.", procname);
  else if ((tools_streqx(d, "E", "W", NULL)) && (x < 0 || x >= 180))  /* 2.25.1 tweak */
    luaL_error(L, "Error in " LUA_QS "`: longitude must be in the range [0, 180[.", procname);
  if (y < 0 || y >= 60)
    luaL_error(L, "Error in " LUA_QS ": second argument must be in the range [0, 60[.", procname);
  if (z < 0 || z >= 60)
    luaL_error(L, "Error in " LUA_QS ": third argument must be in the range [0, 60[.", procname);
  return (tools_streq(d, "S") || tools_streq(d, "W")) ? -x : x;
}

/* Converts coordinates in the form degree, minute, second, and their orientation 'N', 'S', 'W', or 'E' (DMS format)
   to their corresponding decimal degree representation (DegDec format). The return is a number. */
static int astro_dmstodec (lua_State *L) {
  lua_Number x, y, z, fraction;
  const char *d;
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  z = agn_checknumber(L, 3);
  d = agn_checkstring(L, 4);
  x = checkcoords(L, x, y, z, d, "astro.dmstodec");
  /* Calculate the total number of seconds of the fraction */
  fraction = (y * 60 + z)/3600;
  lua_pushnumber(L, x + (x < 0 ? -fraction : fraction));
  return 1;
}


/* Converts coordinates x in decimal degrees (a number) to the form degree, minute, second, and their orientation
   'N', 'S', 'W', or 'E' (DMS format). You must also specify whether to compute latitude or longitude values,
   by passing the strings "lat" or "lon", respectively. The return are three numbers and the orientation, a string. */
static int astro_dectodms (lua_State *L) {
  lua_Number x, tx, minute, second;
  const char *o;
  x = agn_checknumber(L, 1);
  o = agn_checkstring(L, 2);
  if (tools_strneq(o, "lat") && tools_strneq(o, "lon"))  /* 2.25.1 tweak */
    luaL_error(L, "Error in " LUA_QS ": second argument must be either 'lon' or 'lat'.", "astro.dectodms");
  tx = sun_trunc(x);
  minute = fabs(tx - x)*60;
  second = fabs(sun_trunc(minute) - minute)*60;
  luaL_checkstack(L, 4, "not enough stack space");  /* 3.15.5 fix */
  lua_pushnumber(L, fabs(tx));
  lua_pushnumber(L, sun_trunc(minute));
  lua_pushnumber(L, second);
  lua_pushstring(L, (tools_streq(o, "lat")) ? (x < 0 ? "S" : "N") : (x < 0 ? "W" : "E"));  /* 2.25.1 tweak */
  return 4;
}


/* Converts a Julian date (a number) to its Gregorian representation: the year, the month, and the day
   (all integers except the sonds which can be fractional). */
/* Test case: astro.cdate(2456299) -> 2013 1 6; rewritten 2.2.0 RC 5 */
static int astro_cdate (lua_State *L) {
  lua_Number fd, sec;
  int iy, im, id, deg, min, ms, nargs;
  nargs = lua_gettop(L);
  if (nargs == 0) {  /* 7.8.9 extension */
    int ss;
    if (tools_now(&iy, &im, &id, &deg, &min, &ss, &ms) == 1) {
      luaL_error(L, "Error in " LUA_QS ": could not get current system date.", "astro.cdate");
      return 1;
    }
    sec = ss + ms/1000.0;
    fd = aux_fractionalday(deg, min, sec);
    nargs = 1;  /* assume a Julian Date has been given */
  } else if (tools_jd2cdate(agn_checknumber(L, 1), &iy, &im, &id, &fd, &deg, &min, &sec) == -1) {
    luaL_error(L, "Error in " LUA_QS ": invalid Julian date.", "astro.cdate");
    return 1;
  }
  if (nargs == 1) {
    luaL_checkstack(L, 7, "not enough stack space");  /* 3.15.5 fix */
    lua_pushnumber(L, iy);
    lua_pushnumber(L, im);
    lua_pushnumber(L, id);
    lua_pushnumber(L, fd);  /* fractional day (decimal) */
    lua_pushnumber(L, deg);
    lua_pushnumber(L, min);
    lua_pushnumber(L, sec);
    return 7;
  } else {  /* new 5.5.4 */
    luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.5 fix */
    lua_createtable(L, 6, 0);
    lua_rawsetinumber(L, -1, 1, iy);
    lua_rawsetinumber(L, -1, 2, im);
    lua_rawsetinumber(L, -1, 3, id);
    lua_rawsetinumber(L, -1, 4, deg);
    lua_rawsetinumber(L, -1, 5, min);
    lua_rawsetinumber(L, -1, 6, sec);
    lua_pushnumber(L, fd);  /* fractional day (decimal) */
    return 2;
  }
}


/* Converts a Gregorian date represented by year, month, day and optionally hour, minute, and second (all numbers) to the
   corresponding Julian date. The return is a number, or `fail` if the date or time is of a wrong format.

   Test cases checked with http://aa.usno.navy.mil/data/docs/JulianDate.php: all results correct:
   astro.jdate(2016,6,8,12,30) -> 2457548.0208333
   astro.jdate(2016,1,1,23,59) -> 2457389.4993056; */

static void aux_checkoptions (lua_State *L, int pos, int *nargs, int *full, const char *procname) {
  int checkoptions;
  *full = 0;
  /* check for options, here `map in-place` */
  checkoptions = 1;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "full")) {
        *full = agn_checkboolean(L, -1);
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

static int astro_jdate (lua_State *L) {  /* patched 7.8.6 */
  double jd, intjd, s;
  int nargs, full;
  int yy, mm, dd, h, m, ms;
  nargs = lua_gettop(L);
  aux_checkoptions(L, 1, &nargs, &full, "astro.jdate");
  lua_settop(L, nargs);
  if (nargs == 0) {  /* 7.3.0 extension: get current time as jd */
    int ss;
    if (tools_now(&yy, &mm, &dd, &h, &m, &ss, &ms)) goto err;
    s = (double)ss;
  } else {
    aux_fetchdate(L, 1, &yy, &mm, &dd, &h, &m, &s, &ms, 1, "astro.jdate");
  }
  if (tools_isint(s) && ms != 0) s += ms/1000.0;
  jd = iauJuliandate(yy, mm, dd, h, m, s);
  if (jd == HUGE_VAL) goto err;
  intjd = TRUNC(jd - 0.5);
  luaL_checkstack(L, 1 + full*2, "not enough stack space");
  lua_pushnumber(L, jd);  /* 8-byte floating-point Julian Date incl. milliseconds-since-noon in the fractional part. */
  if (full) {
    double totalms = (double)h*3600000.0 + (double)m*60000.0 + (double)s*1000.0 + (double)ms;
    lua_pushnumber(L, intjd);  /* 4-byte integral Julian Date */
    lua_pushnumber(L, (uint32_t)totalms);  /* 4-byte millisecond-since-midnight integer */
  }
  return 1 + full*2;
err:
  lua_pushfail(L);
  return 1;
}


static int is_persian_leap (int persian_year);

static int astro_isleapyear (lua_State *L) {
  int y = agn_checkinteger(L, 1);  /* the year */
  if (lua_gettop(L) > 1) {
    lua_pushboolean(L, is_persian_leap(y));
  } else {
    lua_pushboolean(L, tools_isleapyear(y));
  }
  return 1;
}


/* Checks whether a date and optionally time represents a valid calendar date/a valid time
   and returns `true` or `false`.

   The date is given by the integers year, month day, the optional time by integral hour and
   minute and the number second which can be fractional.

   Alternatively, in the second form, you may pass the input data in the table obj.

   24:00:00 hours is not considered a valid date.

   tools_checkdatetime() is a tiny bit faster than iauCal2jd() combined with iauTf2d() if
   needed. 5.5.6 */
static int astro_isvaliddate (lua_State *L) {
  int yy, mm, dd, h, m, ms;
  double s;
  (void)ms;
  aux_fetchdate(L, 1, &yy, &mm, &dd, &h, &m, &s, &ms, 0, "astro.isvaliddate");
  lua_pushboolean(L, (h < 24)*tools_checkdatetime(yy, mm, dd, h, m, s, 0));
  return 1;
}


/* Checks for a valid time and returns `true` or `false`.

   The time is given by integral hour and minute and the number second which can be fractional.

   Alternatively, in the second form, you may pass the input data in the table obj.

   24:00:00 hours is not considered a valid date. 5.5.6 */
static int astro_isvalidtime (lua_State *L) {
  int h, m;
  lua_Number s, djt;
  aux_fetchtime(L, 1, &h, &m, &s);
  lua_pushboolean(L, iauTf2d('+', h, m, s, &djt) == 0);
  return 1;
}


/* astro.isdst: determines whether Daylight Saving Time is (or was, or will be) in effect at the given date
   represented by the integers year, month, day, hour, minute, second, and returns true or false. In case of
   an invalid date or an internal time conversion failure, the function issues an error. */

static int astro_isdst (lua_State *L) {  /* 2.9.8 */
  int yy, mm, dd, h, m, ms, rc;
  Time64_T t;
  double s;
  struct TM *stm;
  (void)ms;
  aux_fetchdate(L, 1, &yy, &mm, &dd, &h, &m, &s, &ms, 0, "astro.isdst");
  t = tools_maketime(yy, mm, dd, h, m, s, &rc);
  if (rc == -2)
    luaL_error(L, "Error in " LUA_QS ": time component out of range.", "astro.isdst");
  else if (rc == -1)
    luaL_error(L, "Error in " LUA_QS ": could not determine time.", "astro.isdst");
  stm = localtime64(&t);
  if (stm == NULL)  /* invalid date? */
    luaL_error(L, "Error in " LUA_QS ": time could not be converted.", "astro.isdst");
  lua_pushboolean(L, stm->tm_isdst == 1);
  return 1;
}


/* Takes a year, a month, a day, and an hour (all numbers) and returns the moon phase as a real number in the
   range [0, 1], where 0 is new moon and 1 is full Moon; and an integer in the range [0, 7], where 0 indicates
   new moon and 4 indicates full moon. */
static int astro_moonphase (lua_State *L) {
  int ip;
  lua_Number illuminated;
  astro_utc_t utc = { 0 };
  astro_time_t atime = { 0 };
  astro_angle_result_t phase;
  if (!aux_fetchdatetime(L, 1, lua_gettop(L), &atime, "astro.moonphase")) goto err;
  /* compute Moon's ecliptic phase angle, in degrees:
     0 = new moon, 90 = first quarter, 180 = full moon, 270 = third quarter */
  phase = Astronomy_MoonPhase(atime);
  if (phase.status != ASTRO_SUCCESS) goto err;
  utc = Astronomy_UtcFromTime(atime);
  /* compute percentage of the Moon's disc that is illuminated from the Earth's point of view. */
  illuminated = moon_phase(utc.year, utc.month, utc.day, utc.hour, &ip);
  /* Calculate the percentage of the Moon's disc that is illuminated from the Earth's point of view.
     Almost equal to `illuminated`. */
  luaL_checkstack(L, 3, "not enough stack space");  /* 3.15.5 fix */
  lua_pushnumber(L, illuminated);
  lua_pushnumber(L, ip);
  lua_pushnumber(L, phase.angle);
  return 3;
err:
  lua_pushfail(L);
  return 1;
}


/* Searches from a given starting point to calculate the exact time a specific lunar phase occurs.
   A moon phase is defined by the geocentric ecliptic longitude angle separation between the Sun and the Moon.
   It allows you to precisely predict milestones such as New Moons, Full Moons, or any custom
   crescent/gibbous angle threshold.

   By default the function searches for the next Full Moon, for the next 40 days, starting from the
   date given, if any.

   You can give the longitude x of interest with the 'longitude = <x>' option, or 'lon = <x>' for short.

   longitude = 0 represents the New Moon (the Moon and Sun are aligned at the same longitude).
   longitude = 90 represents the First Quarter (the Moon is 90 degrees ahead of the Sun).
   longitude = 180 represents the Full Moon (the Moon and Sun are directly opposite each other).
   longitude = 270 represents the Third Quarter (the Moon is 270 degrees ahead of the Sun).

   You can specify another search range y in (fractional) days with the 'limit = <y>' option, or
   alternatively the 'days = <y>' option with equal meaning. If y is positive you search forward,
   if y is negative it is backward.

   You can specify a start date for the search, by either passing the Julian Date jd of interest
   or the Gregorian Date (year, month, day, hour, minute, second - the latter might be fractional
   to include microseconds), with either six separate arguments or a table of these arguments.

   If no start date is given, the current date and time are taken. Input and output time is UTC.

   If successful, the function returns two values:

   - The precise Julian Date timestamp of the predicted lunar phase event.
   - The equivalent Gregorian date in a table with year, month, day, hour, minute and second
     including sub-second decimal places.

   In case of an internal error or invalid input date, the function returns `fail`.

   Example:

   > import astro;
   > astro.searchmoonphase(limit=-40, longitude=180):  # search backward
   2461251.1094261 [2026, 7, 29, 14, 36, 19.011207818985]
   > astro.searchmoonphase(2461252, limit=40, longitude=180):  # search forward
   2461280.6808049 [2026, 8, 28, 4, 19, 6.085461974144]
   > astro.searchmoonphase([2026, 7, 30, 12, 0, 0], limit=40, longitude=180):
   2461280.6808049, [2026, 8, 28, 4, 19, 6.085461974144]

   7.8.6. The function has been created with the help of Gemini AI. */

static void aux_checkphaseoptions (lua_State *L, int pos, int *nargs,
  lua_Number *longitude, lua_Number *limitdays, const char *procname) {
  int checkoptions;
  *longitude = 180.0;  /* full moon */
  *limitdays = 40.0;   /* for the next 40 days */
  /* check for options */
  checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "longitude") || tools_streq(option, "lon")) {
        *longitude = agn_checknumber(L, -1);
      } else if (tools_streq(option, "limit") || tools_streq(option, "days")) {
        *limitdays = agn_checknumber(L, -1);
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

static int astro_searchmoonphase (lua_State *L) {
  astro_utc_t utc = { 0 };
  astro_time_t atime = { 0 };
  lua_Number longitude, limitdays;
  int nargs = lua_gettop(L);
  aux_checkphaseoptions(L, 1, &nargs, &longitude, &limitdays, "astro.searchmoonphase");
  lua_settop(L, nargs);
  if (!aux_fetchdatetime(L, 1, nargs, &atime, "astro.searchmoonphase")) goto err;
  astro_search_result_t result = Astronomy_SearchMoonPhase(longitude, atime, limitdays);
  if (result.status != ASTRO_SUCCESS) goto err;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushnumber(L, result.time.ut + JD_J2000_EPOCH);  /* Julian Date, 7.8.7 fix */
  lua_createtable(L, 6, 0);
  utc = Astronomy_UtcFromTime(result.time);
  lua_rawsetinumber(L, -1, 1, utc.year);
  lua_rawsetinumber(L, -1, 2, utc.month);
  lua_rawsetinumber(L, -1, 3, utc.day);
  lua_rawsetinumber(L, -1, 4, utc.hour);
  lua_rawsetinumber(L, -1, 5, utc.minute);
  lua_rawsetinumber(L, -1, 6, utc.second);
  return 2;
err:
  lua_pushfail(L);
  return 1;
}


/* Calculates the apparent brightness, illuminated surface area percentage, and phase geometry of a celestial body
   as seen from Earth at a specific moment. For Saturn, it also returns the tilt angle of its ring system.

   The function requires a celestial body name string as the first argument, followed by flexible time tracking
   options:

   body_name (string): The name of the target celestial body to evaluate. Supported options include: "Mercury",
   "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto", "Sun", and "Moon".

   time_parameter (optional variables):

   - Omitted (1 argument total): Automatically uses the current system date and time (UTC) as the tracking point.
   - Julian Date (2 arguments total): A single number representing the absolute Julian Date to evaluate.
   - Calendar Parameters (7 arguments total): Pass separate numeric arguments matching (year, month, day, hour,
   min, sec) to specify a custom UTC timeline frame.

   If successful, the function returns a single Lua table containing the following key-value pairs (or fail if
   the calculations fail):

   - fraction (number): The percentage of the body's visible disk illuminated by sunlight, expressed as a fraction
     between 0.0 (0%, totally dark) and 1.0 (100%, fully lit).
   - magnitude (number): The apparent visual brightness of the body as seen from Earth on the logarithmic magnitude
     scale. Lower or more negative numbers indicate brighter objects (e.g., the Sun is -26.71, Venus peaks around
     -4.8, and dim outer planets have positive values like Pluto at +14.0).
   - phaseangle (number): The phase angle in degrees, measured from the center of the target body between the Sun
     and the Earth. 0° indicates a fully illuminated phase, while 180° indicates a completely dark phase.
   - ringtilt (number, Saturn only): The tilt angle of Saturn's rings in degrees relative to Earth. This value
   dictates how open or closed the rings appear from our perspective, heavily influencing Saturn's overall visual
   magnitude.

   > astro.illumination('Moon', [2026, 8, 28, 4, 19, 6.085461974144]):
   [fraction ~ 0.99998342840944, magnitude ~ -12.647142713872, phaseangle ~ 0.46648311109478]

   7.8.6, generated with the help of Gemini AI. */
static int astro_illumination (lua_State *L) {
  astro_time_t atime;
  astro_body_t celbody;
  astro_illum_t illum;
  int nargs;
  nargs = lua_gettop(L);
  int index = luaL_checkoption(L, 1, NULL, bodies);
  celbody = (astro_body_t)index;
  if (celbody == BODY_EMB || celbody == BODY_SSB) {
    luaL_error(L, "Error in " LUA_QS ": target body must orbit the Sun.", "astro.illumination");
  }
  if (!aux_fetchdatetime(L, 2, nargs, &atime, "astro.illumination")) goto err;
  illum = Astronomy_Illumination(celbody, atime);
  if (illum.status != ASTRO_SUCCESS) goto err;
  lua_createtable(L, 0, 3 + (celbody == BODY_SATURN));
  /* The phase angle of the body in degrees, measured from the center of the body between the Sun and the Earth.
     (0° = Fully illuminated/Full, 180° = Completely dark/New). */
  lua_rawsetstringnumber(L, -1, "phaseangle", illum.phase_angle);
  /* The percentage of the body's visible disk that is illuminated by sunlight, expressed as a fraction
     between 0.0 (0%) and 1.0 (100%). */
  lua_rawsetstringnumber(L, -1, "fraction", illum.phase_fraction);
  /* The apparent visual magnitude (brightness) of the body as seen from Earth. Lower/more negative values
     represent brighter bodies (e.g., Venus peaks near -4.8, whereas Pluto sits around +14.0). */
  lua_rawsetstringnumber(L, -1, "magnitude", illum.mag);
  /* The tilt angle of Saturn's rings in degrees relative to the Earth. It dictates how open or closed the rings
     appear, which directly influences Saturn's total brightness. This value will only be returned if body is "Saturn". */
  if (celbody == BODY_SATURN) {
    lua_rawsetstringnumber(L, -1, "ringtilt", illum.ring_tilt);
  }
  return 1;
err:
  lua_pushfail(L);
  return 1;
}


/* Returns the times of Lunar rise and set in GMT. Receives the year, month day, and the longitude (all of type number)
   and returns two numbers: the GMT rise time in a decimal, and the GMT set time also in a decimal. Use `clock.dectotm`
   to convert the rise and set times to sexagesimal format, or try `astro.moon`.

   Test case: astro.moonriseset(2013, 1, 8, astro.dmstodec(7, 6, 0, 'E'), astro.dmstodec(50, 43, 48, 'N')) ->
   3.7666666666667 12.566666666667 */
static int astro_moonriseset (lua_State *L) {
  double year, month, day, lon, lat, rise, set;
  year = agn_checknumber(L, 1);
  month = agn_checknumber(L, 2);
  day = agn_checknumber(L, 3);
  lon = agn_checknumber(L, 4);
  lat = agn_checknumber(L, 5);
  checkdateloc(L, year, month, day, lon, lat, "astro.moonriseset");
  riseset(lat, lon, day, month, year, 0, &rise, &set);
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.5 fix */
  lua_pushnumber(L, rise);
  lua_pushnumber(L, set);
  return 2;
}


/* Converts a Julian date to the corresponding year, month and day in the Jewish calendar, in this order. The
   fraction of day, the hour, minute and second are also returned. See also: astro.jdate, os.date with the
   '*j' format. If no argument is given, the current system time is returned. 2.10.0 */
static int astro_hdate (lua_State *L) {
  lua_Number fd, ss;
  int nargs, nrets, intable, y, m, d, hh, mm;
  nargs = lua_gettop(L);
  nrets = 1;
  if ( (intable = (nargs > 0 && lua_istrue(L, nargs))) ) {  /* 7.8.5 change */
    lua_settop(L, --nargs);
  }
  hh = mm = ss = 0;
  if (!aux_getdatetimefields(L, nargs, &y, &m, &d, &hh, &mm, &ss, "astro.hdate")) {
    luaL_error(L, "Error in " LUA_QS ": could not convert date.", "astro.hdate");
  }
  fd = aux_fractionalday(hh, mm, ss);
  SdnToJewish(GregorianToSdn(y, m, d), &y, &m, &d);
  if (!intable) {
    luaL_checkstack(L, 7, "not enough stack space");
    lua_pushnumber(L, y);
    lua_pushnumber(L, m);
    lua_pushnumber(L, d);
    lua_pushnumber(L, fd);
    lua_pushnumber(L, hh);
    lua_pushnumber(L, mm);
    lua_pushnumber(L, ss);
    nrets = 7;
  } else {
    lua_createtable(L, 7, 0);
    lua_rawsetinumber(L, -1, 1, y);
    lua_rawsetinumber(L, -1, 2, m);
    lua_rawsetinumber(L, -1, 3, d);
    lua_rawsetinumber(L, -1, 4, fd);
    lua_rawsetinumber(L, -1, 5, hh);
    lua_rawsetinumber(L, -1, 6, mm);
    lua_rawsetinumber(L, -1, 7, ss);
  }
  return nrets;
}


/* Converts a Jewish Date to a Julian Date (UTC).
 * y: Jewish Year (e.g., 5786)
 * m: Month (using your system: 2=Tishrei, 8=Nisan)
 * d: Day (1-30)
 * fraction: Time of day as a decimal (e.g., 0.614189...)
 * To convert a Hebrew data to a Julian Date (JD), we will use an optimized "Absolute Date" algorithm.
 * In astronomy, we first calculate the number of days since a fixed epoch and then shift it to the Julian period.
 * Created by Gemini AI, 7.3.0 */
static double hebrew_to_jd (int y, int m, int d, double fraction) {
  /* 1. Calculate the 'Molad' (New Moon) of Rosh Hashanah */
  long months_elapsed = (235L * y - 234L) / 19L;
  long total_parts = 12084L + 13753L * months_elapsed;
  long day = 29L * months_elapsed + total_parts / 25920L;
  total_parts %= 25920L;
  /* 2. Full Postponement Rules (Dechiyot) */
  long rosh_hashanah = day;
  long day_of_week = rosh_hashanah % 7L;
  /* Rule 1: Lo ADU Rosh (No Rosh Hashanah on Sun, Wed, Fri) */
  if (day_of_week == 0 || day_of_week == 3 || day_of_week == 5) rosh_hashanah++;
  /* Rule 2: Molad Zaken (If Molad is at or after Noon, postpone) */
  /* This is often where the 1-2 day shift happens! */
  else if (total_parts >= 19440L) {
    rosh_hashanah++;
    /* Re-check Rule 1 after bumping for Rule 2 */
    day_of_week = rosh_hashanah % 7L;
    if (day_of_week == 0 || day_of_week == 3 || day_of_week == 5) rosh_hashanah++;
  }
  /* 3. Calculate Days from 1 Tishrei to the target month */
  int month_days = 0;
  int current_m = 2; /* Tishrei */
  /* Simplified month lengths for 5786 (Standard Year) */
  while (current_m != m) {
    /* Tishrei, Shevat, Nisan, Sivan, Av are 30 days */
    if (current_m == 2 || current_m == 6 || current_m == 8 || current_m == 10 || current_m == 12)
      month_days += 30;
    else
      month_days += 29;
    current_m = (current_m % 12) + 1;
  }
  /* 4. The Correct Epoch Offset */
  /* 347997 is the number of days between JD 0 and the Hebrew Epoch */
  double jd = (double)(rosh_hashanah + month_days + d) + 347997.5;
  return jd + fraction;
}

/* * Converts Hours, Minutes, and Seconds to a decimal fraction of a day.
 * h: 0-23
 * m: 0-59
 * s: 0-59 (can be double if you have fractional seconds)
 * Returns: A value between 0.0 and 1.0
 * Created by Gemini AI, 7.3.0 */
static double hms_to_fraction (int h, int m, double s) {
  /* * 3600 seconds per hour
   * 60 seconds per minute
   * 86400.0 total seconds in a 24-hour day */
  double total_seconds = (h*3600.0) + (m*60.0) + s;
  return total_seconds/86400.0;
}

/* Converts a Hebrew Date, optionally with hours, minutes and seconds, to a Julian Date (JD 2000.0). */
static int astro_h2jd (lua_State *L) {
  int y, d, m, hh, mm, ms;
  double ss;
  (void)ms;
  aux_fetchdate(L, 1, &y, &m, &d, &hh, &mm, &ss, &ms, 1, "astro.h2jd");  /* 7.8.5 extension */
  lua_pushnumber(L, hebrew_to_jd(y, m, d, hms_to_fraction(hh, mm, ss)));
  return 1;
}


/* Converts a Gregorian Calendar date given in year, month, day to Julian Date and returns four values:

   1) the Modified Julian Date (MJD) zero-point, always the number 2400000.5,
   2) the MJD for midnight, a number,
   3) a numerical status code: 0 for okay, 1 for bad year, 2 for bad month, 3 for bad day, else any other integer,
   4) the corresponding status text: 'okay', 'bad year', 'bad month', 'bad day', 'unknown exception'.

   In case of invalid input, the first two results are both `undefined`.

   You can alternatively pass year, month and day, in this order, in a table obj.

   To get the Julian Date for midnight, sum up the first and second result.

   The function provides a 1:1 interface to SOFA's iauCal2jd() function. 5.5.4

   See also: astro.tf2d.

   Return checked with https://aa.usno.navy.mil/data/JulianDate. */
static int astro_cal2jd (lua_State *L) {
  int y, m, d, hh, mm, ms, rc;
  lua_Number ss, djm0, djm;
  (void)ms;
  aux_fetchdate(L, 1, &y, &m, &d, &hh, &mm, &ss, &ms, 1, "astro.cal2j");
  rc = iauCal2jd(y, m, d, &djm0, &djm);
  luaL_checkstack(L, 4, "not enough stack space");
  if (rc >= 0) {
    lua_pushnumber(L, djm0);
    lua_pushnumber(L, djm);
  } else {
    lua_pushundefined(L);
    lua_pushundefined(L);
  }
  lua_pushinteger(L, rc < 0 ? -rc : rc);
  switch (rc) {
    case 0:   lua_pushliteral(L, "okay"); break;
    case -1:  lua_pushliteral(L, "bad year"); break;
    case -2:  lua_pushliteral(L, "bad month"); break;
    case -3:  lua_pushliteral(L, "bad day"); break;
    default:  lua_pushliteral(L, "unknown exception"); break;
  }
  return 4;
}


/* Converts hours, minutes, seconds to fractional, decimal days. If not given, minutes and seconds
   default to zero. You may also pass the time in a table in the order mentioned before.

   `sign` usually is the single-character string '+', but can also be '-'. With the empty string,
   the default is '+'.

   The function returns three values:

   1) the fractional day(s),
   2) a numerical status code: 0 for okay, 1 for hours out-of-range, 2 for minutes out-of-range, 3 for seconds out-of-range,
   3) the corresponding status text.

   The function provides a 1:1 interface to SOFA's iauTf2d() function.

   See also: astro.cal2jd. 5.5.4 */
static int astro_tf2d (lua_State *L) {
  int h, m, rc;
  size_t l;
  char signchar;
  lua_Number s, djt;
  const char *sign = agn_checklstring(L, 1, &l);
  signchar = (l == 0) ? '+' : sign[0];
  aux_fetchtime(L, 2, &h, &m, &s);
  rc = iauTf2d(signchar, h, m, s, &djt);
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushnumber(L, djt);
  lua_pushinteger(L, rc);
  switch (rc) {
    case 0:  lua_pushliteral(L, "okay"); break;
    case 1:  lua_pushliteral(L, "hour outside range [0, 23]"); break;
    case 2:  lua_pushliteral(L, "minute outside range [0, 59]"); break;
    case 3:  lua_pushliteral(L, "second outside range [0, 60)"); break;
    default: lua_pushliteral(L, "unknown exception"); break;
  }
  return 3;
}


/* The function normalizes time components hours, minute, second, so 0:61 becomes 1:01 and returns
   the new values as three results. See also: `clock.adjust`.
   Created by Gemini AI, put to the public domain. 7.3.1 UNDOC */
static void aux_normhms (int *h, int *m, double *s, int cycle) {
  /* 1. Handle seconds into minutes */
  while (*s >= 60.0) { *s -= 60.0; (*m)++; }
  while (*s < 0.0)   { *s += 60.0; (*m)--; }
  /* 2. Handle minutes into hours */
  while (*m >= 60)   { *m -= 60; (*h)++; }
  while (*m < 0)     { *m += 60; (*h)--; }
  /* 3. Optional: Wrap hours into a 24-hour cycle */
  if (cycle) *h = (*h % 24 + 24) % 24;
}

static int astro_normhms (lua_State *L) {
  double s;
  int h, m, cycle;
  cycle = lua_gettop(L) == 4;
  aux_fetchtime(L, 1, &h, &m, &s);
  aux_normhms(&h, &m, &s, cycle);
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushinteger(L, h);
  lua_pushinteger(L, m);
  lua_pushnumber(L, s);
  return 3;
}


/* Decomposes radians into degrees, arcminutes, arcseconds and fraction. angle represents the angle
   in radians, and prec the precision which defaults to -5:

   prec        resolution
      :      ...0000 00 00
     -7         1000 00 00
     -6          100 00 00
     -5           10 00 00
     -4            1 00 00
     -3            0 10 00
     -2            0 01 00
     -1            0 00 10
      0            0 00 01
      1            0 00 00.1
      2            0 00 00.01
      3            0 00 00.001
      :            0 00 00.000...

   The largest positive useful value for prec is determined by the size of angle, and is
   typically 9. The function provides a 1:1 interface to SOFA's iauA2af() function. 7.3.1 */
static int astro_a2af (lua_State *L) {
  int i, ndp, idmsf[4];
  double angle;
  char sign[2] = { 0 };
  angle = agn_checknumber(L, 1);
  ndp = agnL_optinteger(L, 2, -5);
  iauA2af(ndp, angle, sign, idmsf);
  luaL_checkstack(L, 5, "not enough stack space");
  for (i=0; i < 4; i++) {
    lua_pushinteger(L, idmsf[i]);
  }
  lua_pushlstring(L, sign, 1);
  return 5;
}


/* Decomposes radians into hours, minutes, seconds and fraction. rad represents the value
   in radians, and prec the precision which defaults to -5, see `astro.a2af` for the
   meaning. The function provides a 1:1 interface to SOFA's iauA2tf() function. 7.3.1 */
static int astro_a2tf (lua_State *L) {
  int i, ndp, idmsf[4];
  double angle;
  char sign[2] = { 0 };
  angle = agn_checknumber(L, 1);
  ndp = agnL_optinteger(L, 2, -5);  /* 7.3.6 fix */
  iauA2tf(ndp, angle, sign, idmsf);
  luaL_checkstack(L, 5, "not enough stack space");
  for (i=0; i < 4; i++) {
    lua_pushinteger(L, idmsf[i]);
  }
  lua_pushlstring(L, sign, 1);
  return 5;
}


/* Decomposes days to hours, minutes, seconds, fraction. day represents the day and may be fractional,
   and prec the precision which defaults to -5, see `astro.a2af` for the meaning. The function provides
   a 1:1 interface to SOFA's iauD2tf() function. 7.3.1 */
static int astro_d2tf (lua_State *L) {
  int i, ndp, idmsf[4];
  double angle;
  char sign[2] = { 0 };
  angle = agn_checknumber(L, 1);
  ndp = agn_checkinteger(L, 2);
  iauD2tf(ndp, angle, sign, idmsf);
  luaL_checkstack(L, 5, "not enough stack space");
  for (i=0; i < 4; i++) {
    lua_pushinteger(L, idmsf[i]);
  }
  lua_pushlstring(L, sign, 1);
  return 5;
}


/* Returns 1 if the Gregorian date (y, m, d) is ON or AFTER the calculated Equinox moment.
 * Scientifically, Tahvil-e Saal (Transition of the Year) refers to the exact moment
 * the Sun "transfers" from the zodiac sign of Pisces into the sign of Aries. */
static int is_after_tahvil (int g_year, int g_month, int g_day, double equinox_jde) {
  int a, y, m;
  double current_jde;
  /* 1. Convert current Gregorian date to JDE at 00:00 UTC */
  /* Formula for Gregorian to Julian Day Number */
  a = (14 - g_month)/12;
  y = g_year + 4800 - a;
  m = g_month + 12*a - 3;
  current_jde = g_day + (153*m + 2)/5 + 365*y + y/4 - y/100 + y/400 - 32045.5;
  /* 2. Compare.
     If current_jde is 2461120.5 (March 21 midnight)
     and equinox is 2461120.115 (March 20 afternoon),
     then we have definitely passed Tahvil. */
  if (current_jde > equinox_jde) return 1;
  /* 3. If we are ON the day of the equinox, we check the fraction (time) */
  if (floor(current_jde + 0.5) == floor(equinox_jde + 0.5)) {
    /* here you could add hour/minute logic if your current_jde includes the current system time. */
    return 0;  /* simplified for "Start of day" checks */
  }
  return 0;
}

static int get_persian_year (int greg_year, int month, int day) {
  /* Check if we have reached the 'Tahvil' (Equinox) yet */
  astro_seasons_t seasons = Astronomy_Seasons(greg_year);
  if (is_after_tahvil(greg_year, month, day, seasons.mar_equinox.ut + JD_J2000_EPOCH)) {
    return greg_year - 621;
  } else {
    return greg_year - 622;
  }
}

static int astro_persianyear (lua_State *L) {  /* 7.3.0 UNDOC */
  int y, m, d;
  y = agn_checkinteger(L, 1);      /* Gregorian year */
  m = agnL_optnonnegint(L, 2, 1);  /* Gregorian month */
  d = agnL_optnonnegint(L, 3, 1);  /* Gregorian day */
  lua_pushinteger(L, get_persian_year(y, m, d));
  return 1;
}


/* The following Persian Calendar functions have been generated by Gemini AI and thus are put to the
   public domain (_no_ MIT licence).

   1. The Persian Calendar: Following the Sun's Speed

   The Persian calendar is arguably the most accurate solar calendar in use because it is tied to
   the True Solar Year.

   Earth does not move at a constant speed around the Sun. Because our orbit is an ellipse
   (Kepler's Second Law), we move faster when we are closer to the Sun (Northern Hemisphere Winter)
   and slower when we are further away (Northern Hemisphere Summer).

   Spring/Summer: Earth takes about 186 days to travel from the Vernal Equinox to the Autumnal Equinox.
   Autumn/Winter: Earth takes about 179 days to travel the remaining half of the orbit.

   The Persian calendar designers (including Omar Khayyam) realized this. They gave the first six months
   31 days to match the slow part of the orbit, and the last six months 30/29 days to match the fast
   part. It is a mathematical "fit" to the actual physics of the solar system.

   The Standard Year (365 days):

   - Months 1–6: All are 31 days (Total: 186 days).
   - Months 7–11: All are 30 days (Total: 150 days).
   - Month 12 (Esfand): Is 29 days.

   The Leap Year (366 days):

   - Months 1–6: All are 31 days.
   - Months 7–11: All are 30 days.
   - Month 12 (Esfand): Grows by one day to 30 days.

   2. The Gregorian Calendar: A History of Politics

   The Western calendar is not based on orbital speed; it is an evolution of the Roman Calendar,
     which was heavily influenced by politics and superstition:
   - Lunar Roots: Early Romans used a lunar calendar. They considered even numbers unlucky, so they tried
     to make months 29 or 31 days long.
   - February's "Leftover" Status: February was the last month added to the Roman calendar. After all the
     "lucky" months were filled, February was left with the remaining 28 days.
   - The Emperors' Egos: Legend suggests that July was named after Julius Caesar and August after
     Augustus Caesar. Both emperors allegedly wanted their months to have 31 days to show equal importance,
     which further broke any "natural" alternating 30/31 pattern.

   3. Comparison of Precision

   The Gregorian system uses a fixed mathematical rule for leap years ( day every 4 years, except
   for years divisible by 100 but not 400). This results in an error of one day every 3,226 years.

   The Persian calendar, however, uses an observational approach or a complex 33-year cycle. This
   makes it accurate to one day every 141,000 years.

   4. The Modern Reform (1925)

   The specific version coded here was officially adopted by the Iranian Parliament on March 31, 1925
   (11 Farvardin 1304).

   Before 1925, Iran used a mix of calendars, including the Jalali calendar (for agriculture) and the
   Islamic Lunar calendar (for religious and official government business). The 1925 reform did three
   critical things:

   - Fixed the Month Lengths: It set the first six months to 31 days, the next five to 30 days, and the
     last month to 29 (or 30 in leap years).

   - Standardized the Names: It brought back the ancient Zoroastrian month names (Farvardin, Ordibehesht, etc.).

   - Scientific Accuracy: It mandated that the year must always begin at the mathematical moment of the
     Vernal Equinox (Spring Equinox) as observed in the Shiraz/Iran timezone.

   5. The Ancestor: The Jalali Calendar (1079)

   While the "legal" version is from 1925, the logic we are using here (basing the year on the Equinox)
   is much older. It was designed by the famous polymath Omar Khayyam in 1079 AD. See `3. Comparison of
   Precision` for its remarkable and outstanding precision. */

typedef struct {
  int day;
  int month;
  int year;
  int hour;
  int minute;
  double second;
  const char* name;
} PersianDate;

static const char *PERSIAN_MONTHS[] = {
  "Farvardin", "Ordibehesht", "Khordad",
  "Tir", "Mordad", "Shahrivar",
  "Mehr", "Aban", "Azar",
  "Dey", "Bahman", "Esfand"
};

/* Astronomical names for the days of a month */
static const char *PERSIAN_DAY_NAMES[] = {
  "Ohrmazd", "Vahman", "Ardwahisht", "Shahrewar", "Spandarmad", "Hordad",
  "Amurdad", "Day-pa-Adar", "Adar", "Aban", "Khwarshed", "Mah",
  "Tishtar", "Gosh", "Day-pa-Mihr", "Mihr", "Srosh", "Rashn",
  "Farwardin", "Warahran", "Ram", "Gwad", "Day-pa-Den", "Den",
  "Ard", "Ashtad", "Asman", "Zam", "Mahraspand", "Anaram",
  "Ruz-e-Zayed" /* The 31st "intercalary" day in the modern solar model */
};

/* The Persian week starts on Saturday. */
static const char *PERSIAN_WEEKDAYS[] = {
  "Shanbeh",        /* Saturday */
  "Yekshanbeh",     /* Sunday */
  "Doshanbeh",      /* Monday */
  "Seshanbeh",      /* Tuesday */
  "Chaharshanbeh",  /* Wednesday */
  "Panjshanbeh",    /* Thursday */
  "Jomeh"           /* Friday (The Weekend/Rest Day) */
};

/* Determines if the year is a leap year (Kabiseh). In the 33-year cycle, leap years follow a specific pattern. */
static int is_persian_leap (int persian_year) {
  int remain = (persian_year + 38) % 33;
  /* If the remainder is divisible by 4, it's a leap year. The '!= 32' handles the special 5-year gap at the end
     of the 33-year sequence. */
  return (remain % 4 == 0 && remain != 32);
}

/* 1. This function calculates the JDE of Nowruz for any Persian Year */
static double get_nowruz_jde (int persian_year) {
  double equinox, jde_midnight_utc, tehran_noon_jde;
  astro_seasons_t seasons = Astronomy_Seasons(persian_year + 621);
  /* A Persian Year + 621 = Gregorian Year. We use the equinox calculation for that Gregorian Year. */
  equinox = seasons.mar_equinox.ut + JD_J2000_EPOCH;
  /* Apply the Tehran Noon Rule (The 'Tahvil' check). Tehran Noon is approx JDE .354 */
  jde_midnight_utc = sun_floor(equinox + 0.5) - 0.5;
  tehran_noon_jde = jde_midnight_utc + 8.5/24.0;
  return jde_midnight_utc + (equinox >= tehran_noon_jde);
}

/* 2. Now we check for a leap year by comparing this year to next year */
static int get_days_in_year_astro (int persian_year) {
  if (persian_year == 0 || persian_year < -1000 || persian_year > 3000) return -1;
  double this_year = get_nowruz_jde(persian_year);
  double next_year = get_nowruz_jde(persian_year + 1);
  /* If the difference is 366, it's a leap year! */
  return (int)(next_year - this_year + 0.5);
}

static PersianDate calculate_persian_date (int persian_year, int day_of_year) {
  int total_days;
  PersianDate pd;
  pd.year = persian_year;
  total_days = get_days_in_year_astro(persian_year);
  if (total_days == -1 || day_of_year < 1 || day_of_year > total_days) {
    pd.month = pd.day = pd.hour = pd.minute = pd.second = 0;
    pd.name = "Invalid";
    return pd;
  }
  if (day_of_year <= 186) {
    pd.month = (day_of_year - 1)/31 + 1;
    pd.day = (day_of_year - 1) % 31 + 1;
  } else {
    int d = day_of_year - 186; /* Use a temp variable to be safe */
    pd.month = (d - 1)/30 + 7;
    pd.day = (d - 1) % 30 + 1;
  }
  pd.hour = pd.minute = pd.second = 0;
  /* SAFE Name Assignment: We check pd.month to ensure we never access index -1 */
  if (pd.month >= 1 && pd.month <= 12) {
    pd.name = PERSIAN_MONTHS[pd.month - 1];
  } else {
    pd.month = pd.day = 0;
    pd.name = "Invalid";
  }
  return pd;
}

static int astro_persiandate (lua_State *L) {
  PersianDate pd;
  int persian_year, day_of_year;
  persian_year = agn_checkinteger(L, 1);  /* Persian (!) year */
  day_of_year = agn_checkinteger(L, 2);
  pd = calculate_persian_date(persian_year, day_of_year);
  if (pd.month == 0) {
    luaL_error(L, "Error in " LUA_QS ": invalid day of year given.", "astro.persiandate");
  }
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushstring(L, pd.name);
  lua_pushinteger(L, pd.month);
  lua_pushinteger(L, pd.day);
  return 3;
}

/* Returns the Farsi name of integer month. 7.3.0 */
static int astro_pmonth (lua_State *L) {
  int month = agn_checkinteger(L, 1);
  if (month < 1 || month > 12) {
    luaL_error(L, "Error in " LUA_QS ": invalid month given.", "astro.pmonth");
  }
  lua_pushstring(L, PERSIAN_MONTHS[month - 1]);
  return 1;
}


/* By default returns the name of the given day of week, an integer in the range [1, 7]. The Persian
   week starts on Saturday.
   If any second argument is given, the Zoroastrian name is returned for the given day of month, an
   integer in the range [1, 31]. 7.3.0 */
static int astro_pweekday (lua_State *L) {
  int nargs = lua_gettop(L);
  int weekday = agn_checkinteger(L, 1);
  if (nargs == 1) {
    if (weekday < 1 || weekday > 7)
      luaL_error(L, "Error in " LUA_QS ": invalid day of week given.", "astro.pweekday");
    lua_pushstring(L, PERSIAN_WEEKDAYS[weekday - 1]);
    return 1;
  } else if (weekday < 1 || weekday > 31) {
    luaL_error(L, "Error in " LUA_QS ": invalid day of week given.", "astro.pweekday");
  }
  lua_pushstring(L, PERSIAN_DAY_NAMES[weekday - 1]);
  return 1;
}


/* Shiraz is UTC + 3.5 hours. 3.5/24.0 = 0.14583333333333334 */
#define SHIRAZ_OFFSET_JD 0.145833333333333333333
static double persian_to_jd (int py, int pm, int pd, int h, int m, double s) {
  /* 1. Get the starting JD for this Persian year (Midnight UTC of Nowruz) */
  double nowruz, time_fraction;
  nowruz = get_nowruz_jde(py);
  /* 2. Calculate days elapsed within the year */
  int days_elapsed;
  if (pm <= 6) {
    days_elapsed = (pm - 1)*31;
  } else {
    days_elapsed = 186 + (pm - 7)*30;
  }
  days_elapsed += (pd - 1);
  /* 3. Convert Time to a fraction of a day (0.0 to 1.0) */
  time_fraction = (h/24.0) + (m/1440.0) + (s/86400.0);
  /* 4. Shiraz Offset (UTC+3.5)
     To get UTC JD, we subtract the offset from the local time fraction. */
  /* 5. Final Result */
  return nowruz + (double)days_elapsed + time_fraction - SHIRAZ_OFFSET_JD;
}

#ifdef DONOTCOMPILE
static PersianDate jd_to_persian (double jd) {
  PersianDate pd;
  lua_Number fd, sec;
  int greg_year, imm, idd, deg, min;
  jd += SHIRAZ_OFFSET_JD;
  /* 1. Estimate the Persian year (approx Gregorian - 621) */
  if ((tools_jd2cdate(jd, &greg_year, &imm, &idd, &fd, &deg, &min, &sec)) == -1) {
    PersianDate pd;
    pd.year = pd.month = pd.day = 0;
    pd.hour = pd.minute = pd.second = 0;
    pd.name = "Invalid";
    return pd;
  }
  int py = greg_year - 621;
  /* 2. Check if we are still in the previous Persian year */
  /* If the target date is BEFORE this year's Nowruz, it belongs to the previous py */
  double nowruz_this_year = get_nowruz_jde(py);
  if (jd < nowruz_this_year) {
    py--;
    nowruz_this_year = get_nowruz_jde(py);
  }
  /* 3. The 'Day of Year' is the difference between target and Nowruz + 1 */
  int day_of_year = (int)(jd - nowruz_this_year + 1.05);
  /* 4. Reuse your existing robust calculation logic */
  pd = calculate_persian_date(py, day_of_year);
  pd.hour = deg;
  pd.minute = min;
  pd.second = sec;
  return pd;
}
#endif

/* There are peculiar roundoff errors with Julian Dates und fractional seconds. This function tries
   to minimise them. 7.8.8 */
static PersianDate jd_to_persian (double jd) {
  PersianDate pd;
  lua_Number fd, sec, nowruz_this_year, local_jd, rem_seconds, local_fd, secsperday;
  int greg_year, day_of_year, imm, idd, deg, min, py;
  /* 1. Extract pure UTC calendar fields first.
     Do NOT add SHIRAZ_OFFSET_JD to the raw 'jd' variable here! */
  if ((tools_jd2cdate(jd, &greg_year, &imm, &idd, &fd, &deg, &min, &sec)) == -1) {
    pd.year = pd.month = pd.day = 0;
    pd.hour = pd.minute = pd.second = 0;
    pd.name = "Invalid";
    return pd;
  }
  /* 2. Apply the Shiraz offset directly to the isolated fractional components */
  local_fd = fd + SHIRAZ_OFFSET_JD;
  if (local_fd >= 1.0) {
    local_fd -= 1.0;
    jd += 1.0; /* Shift calendar day forward if offset crosses midnight */
  }
  /* 3. Re-calculate the local hours, minutes, and precise seconds from local_fd */
  secsperday = 24.0*3600.0*local_fd;
  deg = (int)sun_floor(secsperday/3600.0);
  rem_seconds = sun_fmod(secsperday, 3600.0);
  min = (int)sun_floor(rem_seconds/60.0);
  sec = sun_fmod(rem_seconds, 60.0);
  /* 4. Estimate the Persian year (approx Gregorian - 621) using adjusted day boundary */
  py = greg_year - 621;
  nowruz_this_year = get_nowruz_jde(py);
  /* Use the adjusted jd + offset to compare against Nowruz transit time */
  local_jd = jd + SHIRAZ_OFFSET_JD;
  if (local_jd < nowruz_this_year) {
    py--;
    nowruz_this_year = get_nowruz_jde(py);
  }
  /* 5. Calculate day of the year using aligned local time */
  day_of_year = (int)(local_jd - nowruz_this_year + 1.05);
  pd = calculate_persian_date(py, day_of_year);
  pd.hour = deg;
  pd.minute = min;
  pd.second = sec;  /* adjusted fractional seconds */
  return pd;
}


/* Converts a Julian Date (JD 2000.0) or a Gregorian Date, both considered UTC, to the corresponding
   Persian Date.
   If no argument is given, the current system time is returned.
   With all flavours, the function returns seven values: the Persian year, month, and day,
   followed by the time of day expressed as a decimal fraction, and finally the hours, minutes,
   and seconds. Note that the latter four (the time-related values) are specifically calculated
   in Local Shiraz Time. 7.3.0 */
static int astro_pdate (lua_State *L) {
  PersianDate pd;
  int nargs;
  double jd, fd;
  astro_time_t atime = { 0 };
  nargs = lua_gettop(L);
  aux_fetchdatetime(L, 1, nargs, &atime, "astro.pdate");
  jd = atime.ut + JD_J2000_EPOCH;
  pd = jd_to_persian(jd);
  if (pd.month == 0)
    luaL_error(L, "Error in " LUA_QS ": invalid Julian date encountered.", "astro.pdate");
  /* Shiraz Time, not UTC ! */
  luaL_checkstack(L, 7, "not enough stack space");
  lua_pushinteger(L, pd.year);
  lua_pushinteger(L, pd.month);
  lua_pushinteger(L, pd.day);
  fd = iauJdfd(jd, 0);
  lua_pushnumber(L, (fd == -1.0) ? AGN_NAN : fd + SHIRAZ_OFFSET_JD);
  lua_pushinteger(L, pd.hour);
  lua_pushinteger(L, pd.minute);
  lua_pushnumber(L, pd.second);  /* tools_roundf(pd.second, 4, 9)); */
  return 7;
}


/* Converts a Persian year, month and day, and optionally hours, minute and second to the corresponding
   Julian Date (UTC, JD 2000.0). Hours, minute and second default to zero and are considered to be in local
   Shiraz Time. 7.3.0/7.8.5 */
static int astro_p2jd (lua_State *L) {
  int y, d, m, hh, mm, ms;
  double ss;
  (void)ms;
  /* y stands for Persian year, m for Persian month, d for Perdian day */
  aux_fetchdate(L, 1, &y, &m, &d, &hh, &mm, &ss, &ms, 1, "astro.p2jd");
  lua_pushnumber(L, persian_to_jd(y, m, d, hh, mm, ss));  /* default is midnight ! */
  return 1;
}


/* Returns the time as a Julian Date, that is in UTC. When given no argument, determines the current system time.
   Otherwise y, m, d, hh, mm, ss denote the time, considered in UTC. UNDOC 7.3.1 */
static int astro_time (lua_State *L) {
  int y, m, d, hh, mm, ms, nargs;
  lua_Number ss;
  astro_time_t atime;
  nargs = lua_gettop(L);
  if (nargs == 0) {
    atime = Astronomy_CurrentTime();  /* same as tools_getcurrentjd() but 13 % faster (at least in Windows) */
  } else {
    astro_utc_t utc = { 0 };
    (void)ms;
    aux_fetchdate(L, 1, &y, &m, &d, &hh, &mm, &ss, &ms, 1, "astro.time");
    utc.year = y; utc.month = m; utc.day = d; utc.hour = hh; utc.minute = mm; utc.second = ss;
    atime = Astronomy_TimeFromUtc(utc);
  }
  lua_pushnumber(L, atime.ut + JD_J2000_EPOCH);
  return 1;
}


/* Searches for the date and time Venus will next appear brightest as seen from the Earth. 3.7.1/2 */
static int astro_peak (lua_State *L) {
  astro_illum_t illum;
  astro_time_t atime = { 0 };
  int nargs = lua_gettop(L);
  if (!aux_fetchdatetime(L, 1, nargs, &atime, "astro.peak")) goto err;
  illum = Astronomy_SearchPeakMagnitude(BODY_VENUS, atime);
  if (illum.status != ASTRO_SUCCESS) goto err;
  lua_createtable(L, 0, 5);
  /* The date and time of the observation. */
  lua_rawsetstringnumber(L, -1, "time", illum.time.ut + JD_J2000_EPOCH);
  /* The visual magnitude of the body. Smaller values are brighter. */
  lua_rawsetstringnumber(L, -1, "magnitude", illum.mag);
  /* The angle in degrees between the Sun and the Earth, as seen from the body.
     Indicates the body's phase as seen from the Earth. */
  lua_rawsetstringnumber(L, -1, "phaseangle", illum.phase_angle);
  /* A value in the range [0.0, 1.0] indicating what fraction of the body's
     apparent disc is illuminated, as seen from the Earth. */
  lua_rawsetstringnumber(L, -1, "phasefraction", illum.phase_fraction);
  /* The distance between the Sun and the body at the observation time. */
  lua_rawsetstringnumber(L, -1, "heliodist", illum.helio_dist);
  return 1;
err:
  lua_pushfail(L);
  return 1;
}


/* Computes the next lunar eclipse following the given Julian date jd, an Agena number. If jd is
   not given, the function takes the current system time to compute the result.

   If the last argument is `true` then the lunar eclipse following the next eclipse is being determined.

   The result is a table with the following fields:
   - 'time', the time of the eclipse as a Julian Date,
   - 'obscuration', the peak fraction of the Moon's apparent disc that is covered by the Earth's umbra,
   - 'kind', the type of lunar eclipse found, either 'penumbral', 'partial' or 'total',
   - 'penumbral', the semi-duration of the penumbral phase in minutes,
   - 'partial', the semi-duration of the partial phase in minutes, or 0.0 if none,
   - 'total', the semi-duration of the total phase in minutes, or 0.0 if none. 7.3.1. */
static int astro_lunareclipse (lua_State *L) {
  int nargs;
  astro_lunar_eclipse_t e;
  astro_time_t atime = { 0 };
  nargs = lua_gettop(L);
  if (nargs == 0) {
    atime = Astronomy_CurrentTime();  /* same as tools_getcurrentjd() */
  } else {
    atime.ut = agn_checknumber(L, 1) - JD_J2000_EPOCH;
  }
  if (!(nargs > 0 && lua_istrue(L, nargs)))
    e = Astronomy_SearchLunarEclipse(atime);
  else
    e = Astronomy_NextLunarEclipse(atime);
  if (e.status != ASTRO_SUCCESS) {
    luaL_error(L, "Error in " LUA_QS ": could not compute data.", "astro.lunareclipse");
  }
  lua_createtable(L, 0, 6);
  /* The date and time of the eclipse at its peak, UTC */
  lua_rawsetstringnumber(L, -1, "time", e.peak.ut + JD_J2000_EPOCH);
  /* The peak fraction of the Moon's apparent disc that is covered by the Earth's umbra. */
  lua_rawsetstringnumber(L, -1, "obscuration", e.obscuration);
  /* The type of lunar eclipse found. */
  switch (e.kind) {
    case ECLIPSE_PENUMBRAL:
      /* Gemini AI: The Moon passes through Earth's faint outer shadow. It’s hard to see with the naked eye. */
      lua_rawsetstringstring(L, -1, "kind", "penumbral");
      break;
    case ECLIPSE_PARTIAL:
      /* Gemini AI: Only a portion of the Moon enters Earth's dark inner shadow (the umbra). */
      lua_rawsetstringstring(L, -1, "kind", "partial");
      break;
    case ECLIPSE_TOTAL:
      /* Gemini AI: The entire Moon is obscured by the Earth's umbra. */
      lua_rawsetstringstring(L, -1, "kind", "total");
      break;
    default:
      lua_rawsetstringstring(L, -1, "kind", "unknown");
  }
  /* The semi-duration of the penumbral phase in minutes. */
  lua_rawsetstringnumber(L, -1, "penumbral", e.sd_penum);
  /* The semi-duration of the partial phase in minutes, or 0.0 if none. */
  lua_rawsetstringnumber(L, -1, "partial", e.sd_partial);
  /* The semi-duration of the total phase in minutes, or 0.0 if none. */
  lua_rawsetstringnumber(L, -1, "total", e.sd_total);
  return 1;
}


/* Computes the next solar eclipse following the given Julian date jd, an Agena number. If jd is
   not given, the function takes the current system time to compute the result.

   If the last argument is `true` then the solar eclipse following the next eclipse is being determined.

   The result is a table with the following fields:
   - 'time', the date and time as a Julian Date when the solar eclipse is darkest. This is the instant
      when the axis of the Moon's shadow cone passes closest to the Earth's center.
   - 'obscuration', the peak fraction of the Sun's apparent disc area obscured by the Moon (total and
     annular eclipses only).
   - 'kind', the type of lunar eclipse found, either 'annular', 'partial' or 'total',
   - 'distance', the distance between the Sun/Moon shadow axis and the center of the Earth, in kilometers.
   - 'latitude', the geographic latitude at the center of the peak eclipse shadow.
   - 'longitude', the geographic longitude at the center of the peak eclipse shadow. 7.3.1. */
static int astro_solareclipse (lua_State *L) {
  int nargs;
  astro_global_solar_eclipse_t e;
  astro_time_t atime = { 0 };
  nargs = lua_gettop(L);
  if (nargs == 0) {
    atime = Astronomy_CurrentTime();  /* same as tools_getcurrentjd() */
  } else {
    atime.ut = agn_checknumber(L, 1) - JD_J2000_EPOCH;
  }
  if (!(nargs > 0 && lua_istrue(L, nargs)))
    e = Astronomy_SearchGlobalSolarEclipse(atime);
  else
    e = Astronomy_NextGlobalSolarEclipse(atime);
  if (e.status != ASTRO_SUCCESS) {
    luaL_error(L, "Error in " LUA_QS ": could not compute data.", "astro.solareclipse");
  }
  lua_createtable(L, 0, 6);
  /* The date and time when the solar eclipse is darkest. This is the instant when the axis of the
     Moon's shadow cone passes closest to the Earth's center. */
  lua_rawsetstringnumber(L, -1, "time", e.peak.ut + JD_J2000_EPOCH);
  /* The peak fraction of the Sun's apparent disc area obscured by the Moon (total and annular
     eclipses only). */
  lua_rawsetstringnumber(L, -1, "obscuration", e.obscuration);
  /* The type of solar eclipse found. */
  switch (e.kind) {
    case ECLIPSE_ANNULAR:
      lua_rawsetstringstring(L, -1, "kind", "annular");
      break;
    case ECLIPSE_PARTIAL:
      lua_rawsetstringstring(L, -1, "kind", "partial");
      break;
    case ECLIPSE_TOTAL:
      lua_rawsetstringstring(L, -1, "kind", "total");
      break;
    default:
      lua_rawsetstringstring(L, -1, "kind", "unknown");
  }
  /* The distance between the Sun/Moon shadow axis and the center of the Earth, in kilometers. */
  lua_rawsetstringnumber(L, -1, "distance", e.distance);
  /* The geographic latitude at the center of the peak eclipse shadow. */
  lua_rawsetstringnumber(L, -1, "latitude", e.latitude);
  /* The geographic longitude at the center of the peak eclipse shadow. */
  lua_rawsetstringnumber(L, -1, "longitude", e.longitude);
  return 1;
}


static void aux_getaltitudeparams (lua_State *L, int mode, int nargs,
    lua_Number *lat, lua_Number *lon, lua_Number *elev,
    int *yy, int *mm, int *dd, int *h, int *m, lua_Number *s,
    lua_Number *angle, int *dir, lua_Number *limit, const char *procname) {
  int shift = 0;
  *lat   = agn_checknumber(L, 2);
  *lon   = agn_checknumber(L, 3);
  *elev  = agn_checknumber(L, 4);  /* in meters */
  if ((mode == 1 && nargs > 11) || (mode == 2 && nargs == 10) || (mode == 3 && nargs > 10)) {
    *yy    = agn_checkinteger(L, 5);
    *mm    = agn_checkposint(L, 6);
    *dd    = agn_checkposint(L, 7);
    *h     = agn_checknonnegint(L, 8);
    *m     = agn_checknonnegint(L, 9);
    *s     = agn_checknonnegative(L, 10);
    shift = 5;
  } else {
    double fd;  /* fractional date, unused */
    (void)fd;
    if (tools_jd2cdate(agn_checknumber(L, 5), yy, mm, dd, &fd, h, m, s) == -1) {
      luaL_error(L, "Error in " LUA_QS ": invalid Julian date.", procname);
      return;
    }
  }
  if (mode != 3) {
    *angle = agnL_optnumber(L,  6 + shift, 0.0);  /* in degrees */
    *dir   = agnL_optinteger(L, 7 + shift, 0);    /* see below */
    *limit = agnL_optnumber(L,  8 + shift, 1.0);  /* search window limit in days */
  } else {  /* astro.searchriseset */
    *angle = 0.0;
    *dir   = agnL_optinteger(L, 6 + shift, 0);    /* see below */
    *limit = agnL_optnumber(L,  7 + shift, 1.0);  /* search window limit in days */
  }
  if (*mm > 12 || *h > 23 || *m > 59 || *s >= 60)
    luaL_error(L, "Error in " LUA_QS ": invalid Julian date.", procname);
  if (*dd > ndays[*mm - 1] + (*mm == 2) * tools_isleapyear(*yy))  /* 1.9.1 */
    luaL_error(L, "in `%s`: day must be in the range [1, %d].", ndays[*mm - 1], procname);
  if (*lon < -180 || *lon > 180)  /* longitude = Längengrad (E/W) */
    luaL_error(L, "in `%s`: longitude must be in the range [-180, 180].", procname);
  if (*lat < -90 || *lat > 90)  /* latitude = Breitengrad (N/S) */
    luaL_error(L, "in `%s`: latitude must be in the range [-90, 90].", procname);
  checkdateloc(L, 2000, 1, 1, *lon, *lat, procname);
}

static int astro_searchaltitude (lua_State *L) {  /* 7.8.5, created with the help of Gemini AI */
  astro_utc_t utc = { 0 };
  astro_time_t atime;
  astro_search_result_t result;
  astro_body_t celbody;
  int yy, mm, dd, h, m, dir, nargs;
  lua_Number s, angle, lat, lon, elev, limit;
  nargs = lua_gettop(L);
  celbody = (astro_body_t)luaL_checkoption(L, 1, NULL, bodies);  /* 7.8.6 change */
  aux_getaltitudeparams(L, 1, nargs, &lat, &lon, &elev, &yy, &mm, &dd, &h, &m, &s, &angle, &dir, &limit, "astro.searchaltitude");
  atime = Astronomy_MakeTime(yy, mm, dd, h, m, s);
  astro_observer_t observer = { lat, lon, elev };
  dir = (dir < 0) ? -1 : 1;  /* -1: searching for the downward/setting crossing, +1 upward/rising */
  result = Astronomy_SearchAltitude(celbody, observer, dir, atime, limit, angle);
  if (result.status != ASTRO_SUCCESS) {
    lua_pushfail(L);
    return 1;
  }
  utc = Astronomy_UtcFromTime(result.time);
  luaL_checkstack(L, 2, "not enough stack space");  /* result changed 7.8.6 */
  lua_pushnumber(L, result.time.ut + JD_J2000_EPOCH);  /* Julian Date */
  lua_createtable(L, 6, 0);
  lua_rawsetinumber(L, -1, 1, utc.year);
  lua_rawsetinumber(L, -1, 2, utc.month);
  lua_rawsetinumber(L, -1, 3, utc.day);
  lua_rawsetinumber(L, -1, 4, utc.hour);
  lua_rawsetinumber(L, -1, 5, utc.minute);
  lua_rawsetinumber(L, -1, 6, utc.second);
  return 2;
}


static int astro_searchsolarnoon (lua_State *L) {  /* 7.8.7 */
  astro_hour_angle_t result;
  astro_utc_t utc = { 0 };
  int yy     = agn_checkinteger(L, 1);
  int mm     = agn_checkposint(L, 2);
  int dd     = agn_checkposint(L, 3);
  double lat = agn_checknumber(L, 4);
  double lon = agn_checknumber(L, 5);
  astro_observer_t observer = { lat, lon, 0.0 };
  astro_time_t atime = Astronomy_MakeTime(yy, mm, dd, 0, 0, 0);
  /* Find the next instance where the Sun crosses the local meridian (Hour Angle = 0) */
  result = Astronomy_SearchHourAngleEx(BODY_SUN, observer, 0.0, atime, 1.0);
  if (result.status != 0) {
    lua_pushfail(L);
    return 1;
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushnumber(L, result.time.ut + JD_J2000_EPOCH);  /* Julian Date */
  lua_createtable(L, 6, 0);
  utc = Astronomy_UtcFromTime(result.time);
  lua_rawsetinumber(L, -1, 1, utc.year);
  lua_rawsetinumber(L, -1, 2, utc.month);
  lua_rawsetinumber(L, -1, 3, utc.day);
  lua_rawsetinumber(L, -1, 4, utc.hour);
  lua_rawsetinumber(L, -1, 5, utc.minute);
  lua_rawsetinumber(L, -1, 6, utc.second);
  return 2;
}

/*
   astro.searchhourangle(body, latitude, longitude, elevation,
     y, m, d, hh, mm, ss, angle, direction)
   astro.searchhourangle(body, latitude, longitude, elevation,
     jd, angle, direction)

   Searches for the time when the center of a body reaches a specified hour angle as seen by an observer
   on the Earth.

   `body` may be 'Mercury', 'Venus', 'Earth', 'Mars', 'Jupiter', 'Saturn', 'Uranus', 'Neptune', 'Pluto',
   'Sun' or 'Moon'.

   `latitude` in degrees, `longitude` in degrees and `elevation` in meters indicate a location on or near
   the surface of the Earth where the observer is located.

   The starting date and time is given by either the Julian Date jd or the given year y, month m, day d,
   hour hh, minute mm, (fractional) second ss.

   The hour `angle` of a celestial body indicates its position in the sky with respect to the Earth's rotation.
   The hour angle depends on the location of the observer on the Earth. `angle` is in the range [0, 24).

   To find when a body culminates, that is reaches the highest point in the sky pass 0 for hour `angle`.
   To find when a body reaches its lowest point in the sky, pass 12 for `angle`.

   The hour angle increases by 1 unit for every sidereal hour that passes after that point, up to 24 sidereal
   hours when it reaches the highest point again. So the hour angle indicates the number of hours that have
   passed since the most recent time that the body has culminated, or reached its highest point.

   `direction` indicates the direction in time to perform the search: a positive value searches forward in time,
   a negative value searches backward in time. `direction` should not be zero.

   Note that, especially close to the Earth's poles, a body as seen on a given day may always be above the
   horizon or always below the horizon, so the caller cannot assume that a culminating object is visible nor
   that an object is below the horizon at its minimum altitude.

   On success, the function reports the date and time as a Julian Date, along with the horizontal coordinates
   of the body at that time, as seen by the given observer, as a dictionary with the following fields:

   'azimuth': compass direction around the horizon in degrees. 0=North, 90=East, 180=South, 270=West;
   'altitude': angle in degrees above (positive) or below (negative) the observer's horizon;
   'ra': right ascension in sidereal hours;
   'declination': declination in degrees.

   Otherwise, the function returns `fail`.

   The function provides a 1:1 interface to function Astronomy_SearchHourAngleEx() of the C library
   `Astronomy Engine for C/C++`. The description has been taken from the accompanying documentation. 7.8.7

   For the solar noon, with the starting date August 13, 2026 00:00:00 UTC, at location Chattanooga, TN, enter:

   import astro;
   lat, lon, angle, dir := 35.0456, -85.3097, 0.0, 1.0;
   astro.searchhourangle('Sun', lat, lon, 0, 2026, 8, 13, 0, 0, 0, angle, dir):
   2461266.2403325 [altitude ~ 69.456447100381, azimuth ~ 179.99998731165, declination ~ 14.502047100381, ra ~ 9.5592033742828] */
static int astro_searchhourangle (lua_State *L) {
  astro_time_t atime;
  astro_hour_angle_t result;
  astro_body_t celbody;
  int yy, mm, dd, h, m, dir, nargs;
  lua_Number s, angle, lat, lon, elev, limit;
  nargs = lua_gettop(L);
  celbody = (astro_body_t)luaL_checkoption(L, 1, NULL, bodies);
  aux_getaltitudeparams(L, 1, nargs, &lat, &lon, &elev, &yy, &mm, &dd, &h, &m, &s, &angle, &dir, &limit, "astro.searchhourangle");
  atime = Astronomy_MakeTime(yy, mm, dd, h, m, s);
  astro_observer_t observer = { lat, lon, elev };
  result = Astronomy_SearchHourAngleEx(celbody, observer, angle, atime, dir);
  if (result.status != 0) {
    lua_pushfail(L);
    return 1;
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushnumber(L, result.time.ut + JD_J2000_EPOCH);
  lua_createtable(L, 0, 4);
  /* Compass direction around the horizon in degrees. 0=North, 90=East, 180=South, 270=West. */
  lua_rawsetstringnumber(L, -1, "azimuth", result.hor.azimuth);
  /* Angle in degrees above (positive) or below (negative) the observer's horizon. */
  lua_rawsetstringnumber(L, -1, "altitude", result.hor.altitude);
  /* Right ascension in sidereal hours. */
  lua_rawsetstringnumber(L, -1, "ra", result.hor.ra);
  /* Declination in degrees. */
  lua_rawsetstringnumber(L, -1, "declination", result.hor.dec);
  return 2;
}


/*
   astro.gethourangle(body, latitude, longitude, elevation, y, m, d, hh, mm, ss)
   astro.searchhourangle(body, latitude, longitude, elevation, jd)

   Finds the hour angle of a body for a given observer and time.

   `body` may be 'Mercury', 'Venus', 'Earth', 'Mars', 'Jupiter', 'Saturn', 'Uranus', 'Neptune', 'Pluto',
   'Sun' or 'Moon'.

   `latitude` in degrees, `longitude` in degrees and `elevation` in meters indicate a location on or near
   the surface of the Earth where the observer is located.

   The date and time of interest is given by either the Julian Date jd or the given year y, month m,
   day d, hour hh, minute mm, (fractional) second ss.

   The hour angle of a celestial body indicates its position in the sky with respect to the Earth's rotation.
   The hour angle depends on the location of the observer on the Earth. The hour angle is 0 when the body's
   center reaches its highest angle above the horizon in a given day. The hour angle increases by 1 unit
   for every sidereal hour that passes after that point, up to 24 sidereal hours when it reaches the highest
   point again. So the hour angle indicates the number of hours that have passed since the most recent time
   that the body has culminated, or reached its highest point.

   If successful, the function will return the hour angle in the half-open range [0, 24). Otherwise, it
   returns `fail`.

   Multiply the result by 15 to get degrees (360° / 24 hours = 15° per hour).

   > import astro;
   > lat, lon := 35.0456, -85.3097;
   > astro.gethourangle('Sun', lat, lon, 0, 2026, 8, 13, 13, 10, 0):
   19.397981766124

   The function provides a 1:1 interface to function Astronomy_HourAngle() of the C library
   `Astronomy Engine for C/C++`. The description has been taken from the accompanying documentation. 7.8.7 */
static int astro_gethourangle (lua_State *L) {
  astro_time_t atime;
  astro_func_result_t result;
  astro_body_t celbody;
  int yy, mm, dd, h, m, dir, nargs;
  lua_Number s, angle, lat, lon, elev, limit;
  nargs = lua_gettop(L);
  celbody = (astro_body_t)luaL_checkoption(L, 1, NULL, bodies);
  aux_getaltitudeparams(L, 2, nargs, &lat, &lon, &elev, &yy, &mm, &dd, &h, &m, &s, &angle, &dir, &limit, "astro.gethourangle");
  atime = Astronomy_MakeTime(yy, mm, dd, h, m, s);
  astro_observer_t observer = { lat, lon, elev };
  result = Astronomy_HourAngle(celbody, &atime, observer);
  if (result.status != 0) {
    lua_pushfail(L);
  } else {
    lua_pushnumber(L, result.value);  /* the hour angle in the half-open range [0, 24) */
  }
  return 1;
}


/*
   astro.searchriseset(body, latitude, longitude, elevation, y, m, d, hh, mm, ss, direction, limit)
   astro.searchriseset(body, latitude, longitude, elevation, jd, direction, limit)

   Searches for the next time a celestial body rises or sets as seen by an observer on the Earth.

   `body` may be 'Mercury', 'Venus', 'Earth', 'Mars', 'Jupiter', 'Saturn', 'Uranus', 'Neptune', 'Pluto',
   'Sun' or 'Moon'.

   `latitude` in degrees, `longitude` in degrees and `elevation` in meters indicate a location on or near
   the surface of the Earth where the observer is located.

   The date and time of interest is given by either the Julian Date jd or the given year y, month m,
   day d, hour hh, minute mm, (fractional) second ss.

   Pass +1 for `direction` to search for the time a body begins to rise above the horizon.
   Pass -1 for `direction` to search for the time a body finishes sinking below the horizon.

   `limit` depicts how many days to search for a rise or set time, and defines the direction in time to search. When `limit`
   is positive, the search is performed into the future, after the given start time. When negative, the search is performed
   into the past, before start time. To limit a rise or set time to the same day, you can use a value of 1 day. In cases
   where you want to find the next rise or set time no matter how far in the future (for example, for an observer near the
   south pole), you can pass in a larger value like 365.

   This function finds the next rise or set time of the Sun, Moon, or planet other than the Earth. Rise time is when the body
   first starts to be visible above the horizon. For example, sunrise is the moment that the top of the Sun first appears
   to peek above the horizon. Set time is the moment when the body appears to vanish below the horizon. Therefore, this
   function adjusts for the apparent angular radius of the observed body (significant only for the Sun and Moon).

   This function corrects for a typical value of atmospheric refraction, which causes celestial bodies to appear higher above
   the horizon than they would if the Earth had no atmosphere. Astronomy Engine uses a correction of 34 arcminutes. Real-world
   refraction varies based on air temperature, pressure, and humidity; such weather-based conditions are outside the scope
   of Astronomy Engine.

   Note that rise or set may not occur in every 24 hour period. For example, near the Earth's poles, there are long periods
   of time where the Sun stays below the horizon, never rising. Also, it is possible for the Moon to rise just before midnight
   but not set during the subsequent 24-hour day. This is because the Moon sets nearly an hour later each day due to orbiting
   the Earth a significant amount during each rotation of the Earth. Therefore callers must not assume that the function will always succeed.

   On success, the function returns the date and time of the rise or set time as requested, as a Julian Date, and `fail` in
   case of an error.

   The function provides a 1:1 interface to function Astronomy_SearchRiseSetEx() of the C library
   `Astronomy Engine for C/C++`. The description has been taken from the accompanying documentation. 7.8.7
*/

static int astro_searchriseset (lua_State *L) {
  astro_time_t atime;
  astro_search_result_t result;
  astro_body_t celbody;
  astro_utc_t utc = { 0 };
  int yy, mm, dd, h, m, dir, nargs;
  lua_Number s, angle, lat, lon, elev, limit;
  nargs = lua_gettop(L);
  celbody = (astro_body_t)luaL_checkoption(L, 1, NULL, bodies);
  aux_getaltitudeparams(L, 3, nargs, &lat, &lon, &elev, &yy, &mm, &dd, &h, &m, &s, &angle, &dir, &limit, "astro.searchriseset");
  atime = Astronomy_MakeTime(yy, mm, dd, h, m, s);
  astro_observer_t observer = { lat, lon, elev };
  result = Astronomy_SearchRiseSetEx(celbody, observer, (dir >= 0) ? +1 : -1, atime, limit, 0.0);  /* 7.8.8 change */
  if (result.status != 0) {
    lua_pushfail(L);
    return 1;
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushnumber(L, result.time.ut + JD_J2000_EPOCH);  /* Julian Date */
  lua_createtable(L, 6, 0);
  utc = Astronomy_UtcFromTime(result.time);
  lua_rawsetinumber(L, -1, 1, utc.year);
  lua_rawsetinumber(L, -1, 2, utc.month);
  lua_rawsetinumber(L, -1, 3, utc.day);
  lua_rawsetinumber(L, -1, 4, utc.hour);
  lua_rawsetinumber(L, -1, 5, utc.minute);
  lua_rawsetinumber(L, -1, 6, utc.second);
  return 2;
}


/* astro.getposition(body, latitude, longitude, elevation, y, m, d, hh, mm, ss)
   astro.getposition(body, latitude, longitude, elevation, jd)

Calculates the exact position of a celestial body for a specific observer location and time.

The input parameters are:

    body (string): `body` may be 'Mercury', 'Venus', 'Earth', 'Mars', 'Jupiter', 'Saturn', 'Uranus', 'Neptune',
    'Pluto', 'Sun' or 'Moon'.
    lat (number): Geographic latitude of the observer in degrees (positive for North, negative for South).
    lon (number): Geographic longitude of the observer in degrees (positive for East, negative for West).
    elev (number): Observer's elevation above sea level in meters.
    yy, mm, dd (integer): The date of the observation (Year, Month, Day).
    h, m, s (integer/number): The time of the observation in UTC (Hour, Minute, Second).
    Alternatively: The Julian Date jd.

The function returns two distinct values:

    Julian Date (number): The precise moment of calculation formatted as a Julian Date (adjusted to the J2000 epoch).
    Coordinate Table (table): A key-value dictionary containing the positional mapping:

    azimuth: The compass heading in degrees, measured clockwise starting from North (0° = North, 90° = East,
    180° = South, 270° = West).
    altitude: The angle in degrees above (+) or below (-) the observer's local horizon. This value automatically
    compensates for standard atmospheric refraction on Earth.
    ra: The Right Ascension in sidereal hours (ranging from 0 to 24). This represents the space-fixed longitude on
    the celestial sphere.
    declination: The Declination in degrees (ranging from -90° to +90°), representing celestial latitude relative
    to Earth's projected equator.

    Example for Chattanooga, TN, for August 13, 2026, noon local time:

    > import astro;

    > lat, lon, elev := 35.0456, -85.3097, 206;

    > astro.getposition('Sun', lat, lon, elev, 2026, 8, 13, 17, 46, 6):
    2461266.2403472 [altitude ~ 69.454648517606, azimuth ~ 179.99942552291, declination ~ 14.493921704599, ra ~ 9.559571062088]

    7.8.7, created by Gemini AI */

#define ABERRATION_CORRECTED   1
#define EQUATOR_TOPOCENTRIC    1
static int astro_getposition (lua_State *L) {
  astro_time_t atime;
  astro_horizon_t hor;
  astro_equatorial_t equ;
  astro_body_t celbody;
  int yy, mm, dd, h, m, dir, nargs;
  lua_Number s, angle, lat, lon, elev, limit;
  nargs = lua_gettop(L);
  celbody = (astro_body_t)luaL_checkoption(L, 1, NULL, bodies);
  /* Extract coordinates and time from arguments starting at index 2 */
  aux_getaltitudeparams(L, 2, nargs, &lat, &lon, &elev, &yy, &mm, &dd, &h, &m, &s, &angle, &dir, &limit, "astro.getposition");
  atime = Astronomy_MakeTime(yy, mm, dd, h, m, s);
  astro_observer_t observer = { lat, lon, elev };
  /* Step 1: Get equatorial coordinates first (RA/Dec) using raw enum values (1, 1) */
  equ = Astronomy_Equator(celbody, &atime, observer, ABERRATION_CORRECTED, EQUATOR_TOPOCENTRIC);
  /* Step 2: Feed the resulting RA and Dec directly into the Horizon converter.
     We pass '1' as the final parameter for standard atmospheric refraction. */
  hor = Astronomy_Horizon(&atime, observer, equ.ra, equ.dec, 1);
  lua_createtable(L, 0, 4);
  /* for the azimut: 0° = North, 90° = East, South = 180°, West = 270° */
  lua_rawsetstringnumber(L, -1, "azimuth",     hor.azimuth);
  lua_rawsetstringnumber(L, -1, "altitude",    hor.altitude);
  lua_rawsetstringnumber(L, -1, "ra",          equ.ra);
  lua_rawsetstringnumber(L, -1, "declination", equ.dec);
  return 1;
}


/* Calculates the lunar libration (apparent physical "wobble") and sub-Earth tracking coordinates
of the Moon for a specific moment.

Because the Moon's orbit is elliptical and tilted, it appears to rock back and forth and nod up and down
over the course of a month as seen from Earth. This function computes that exact physical orientation.

The function accepts either zero arguments, a single Julian Date, or six calendar parameters
passed as numbers:

- Omitted (0 arguments): Automatically calculates the libration coordinates for the current system
  date and time (UTC).
- Julian Date (1 argument): A single number representing the absolute Julian Date to evaluate.
- Calendar Parameters (6 arguments): Pass separate numeric arguments matching (year, month,
  day, hour, min, sec) to specify a custom UTC timeline frame.

The function returns a dictionary representing the Moon's perspective orientation relative
to an observer on Earth:

- field 'latitude': The selenographic latitude of the point on the Moon's surface directly
  beneath the Earth, measured in degrees.
    Positive (+): The Moon is nodding forward, tilting its North Pole terrain toward Earth.
    Negative (-): The Moon is nodding backward, tilting its South Pole terrain toward Earth.
- field 'longitude': The selenographic longitude of the point on the Moon's surface directly
  beneath the Earth, measured in degrees.
    Positive (+): The Moon is twisted east, exposing more features along its eastern edge.
    Negative (-): The Moon is twisted west, exposing more features along its western edge.
- field 'limbangle': The position angle of the Moon's illuminated edge (or its apparent
  angular diameter frame metric), measured in degrees.

Created by Gemini AI, 7.8.6 */
static int astro_lunarlibration (lua_State *L) {
  astro_time_t atime = { 0 };
  astro_libration_t result = { 0 };
  if (!aux_fetchdatetime(L, 1, lua_gettop(L), &atime, "astro.lunarlibration")) goto err;
  result = Astronomy_Libration(atime);
  lua_createtable(L, 0, 3);
  lua_rawsetstringnumber(L, -1, "latitude", result.elat);
  lua_rawsetstringnumber(L, -1, "longitude", result.elon);
  lua_rawsetstringnumber(L, -1, "limbangle", result.diam_deg);
  return 1;
err:
  lua_pushfail(L);
  return 1;
}


/* Finds the very next occurring Lunar Apogee (the point in the Moon's orbit furthest from Earth)
   or Lunar Perigee (the point closest to Earth) relative to a given starting date and time.

   Input Parameters: The function accepts either zero arguments, a Julian Date or six date-time arguments
   passed as integers/numbers:

   No arguments: Automatically uses the current system date and time (UTC) as the search starting point.
   Six arguments (yy, mm, dd, h, m, s) or date and optionally time passed in a table: Specifies a custom
   UTC date and time to begin searching forward from.
   One argument: The Julian Date where to search from.

   Return Values: If successful, the function returns exactly 9 values:

   The calendar year, month, day, hour, minute, second of the event (UTC) plus the kind "apogee" if
   the Moon is furthest, or "perigee" if it is closest, the distance between the centers of the Earth
   and the Moon in kilometers and the exact Julian Date timestamp of the event.

   In case of an internal error or invalid input date, the function returns `fail`. 7.8.6

   Examples:

   > import astro

   # Search for the next event from the current date:
   > L := [astro.searchlunarapsis()]:
   [2026, 8, 22, 8, 20, 46.747706830502, apogee, 404632.01214081, 2461274.8477633]

   # Search for the event after that:
   > astro.searchlunarapsis(L[9]):
   2026    9       6       20      44      8.9950999617577 perigee 368248.59424929 2461290.363993 */

static int astro_searchlunarapsis (lua_State *L) {
  astro_time_t atime = { 0 };
  astro_apsis_t result = { 0 };
  if (!aux_fetchdatetime(L, 1, lua_gettop(L), &atime, "astro.searchlunarapsis")) goto err;
  result = Astronomy_SearchLunarApsis(atime);
  if (result.status != ASTRO_SUCCESS) goto err;
  astro_utc_t utc = Astronomy_UtcFromTime(result.time);
  luaL_checkstack(L, 9, "not enough stack space");
  lua_pushinteger(L, utc.year);
  lua_pushinteger(L, utc.month);
  lua_pushinteger(L, utc.day);
  lua_pushinteger(L, utc.hour);
  lua_pushinteger(L, utc.minute);
  lua_pushnumber(L,  utc.second);
  /* In Astronomy Engine, 1 (APSIS_APOCENTER) is apogee, 0 (APSIS_PERICENTER) is perigee */
  lua_pushstring(L, (result.kind == 1) ? "apogee" : "perigee");
  lua_pushnumber(L, result.dist_km);
  lua_pushnumber(L, result.time.ut + JD_J2000_EPOCH);
  return 9;
err:
  lua_pushfail(L);
  return 1;
}


/* Finds the very next occurring Perihelion (the point in a planet's orbit closest to the Sun) or Aphelion (the point
   furthest from the Sun) relative to a given starting date and time.

   The function requires a target body string as the first argument, followed by flexible time tracking options:

   - body (string): The name of the body orbiting the sun: 'Mercury', 'Venus', 'Earth', 'Mars', 'Jupiter', 'Saturn',
     'Uranus', 'Neptune' or 'Pluto'.

   - atime (optional variables):
     - Omitted (1 argument total): Automatically uses the current system date and time (UTC) as the search baseline.
     - Julian Date (2 arguments total): A single number representing the absolute Julian Date to begin searching forward from.
     - Calendar parameters (7 arguments total): Pass separate numeric parameters matching (body_name, year, month, day,
       hour, min, sec) to specify a custom date framework.

   If successful, the function returns exactly 9 values:

   The calendar year, month, day, hour, minute, second of the event (UTC) plus the kind 'perihelion' if closest to the Sun,
   or 'aphelion' if furthest, distance between the centers of the planet and the Sun measured in Astronomical Units (AU)
   and the exact Julian Date timestamp of the computed event.

   In case of an internal error or invalid input date, the function returns `fail`.

   > import astro;

   Search for the next event for the ninth planet:
   > astro.searchplanetapsis('Pluto', [2026, 8, 11, 12, 0, 0]):
   2114  2  20  10  45  39.279797673225  aphelion  49.32025774638  2493232.9483713

   And the event after that:
   > astro.searchplanetapsis('Pluto', 2493233):
   2237  9  16  17  1  52.103931605816  perihelion  29.645264444715  2538366.2096308

   Created by Gemini AI, 7.8.6 */
static int astro_searchplanetapsis (lua_State *L) {
  astro_time_t atime;
  astro_apsis_t result;
  astro_body_t celbody;
  int nargs;
  nargs = lua_gettop(L);
  /* 1. Parse the celestial body string using your custom check options */
  int index = luaL_checkoption(L, 1, NULL, bodies);
  celbody = (astro_body_t)index;
  /* Verify the user didn't request the Sun, Earth/Moon Barycenter, or an invalid body */
  if (celbody == BODY_SUN || celbody == BODY_MOON || celbody == BODY_EMB || celbody == BODY_SSB) {
    luaL_error(L, "Error in " LUA_QS ": target body must orbit the Sun.", "astro.searchplanetapsis");
  }
  if (!aux_fetchdatetime(L, 2, nargs, &atime, "astro.searchplanetapsis")) goto err;
  /* 3. Core Engine Call */
  result = Astronomy_SearchPlanetApsis(celbody, atime);
  if (result.status != ASTRO_SUCCESS) goto err;
  astro_utc_t utc = Astronomy_UtcFromTime(result.time);
  luaL_checkstack(L, 9, "not enough stack space");
  /* 4. Push Return Values */
  lua_pushinteger(L, utc.year);
  lua_pushinteger(L, utc.month);
  lua_pushinteger(L, utc.day);
  lua_pushinteger(L, utc.hour);
  lua_pushinteger(L, utc.minute);
  lua_pushnumber(L,  utc.second);
  /* Map result.kind:
     1 (APSIS_APOCENTER) means Aphelion for planets.
     0 (APSIS_PERICENTER) means Perihelion. */
  lua_pushstring(L, (result.kind == 1) ? "aphelion" : "perihelion");
  /* Distance returned from this specific function is in AU (Astronomical Units) */
  lua_pushnumber(L, result.dist_au);
  /* Return the absolute Julian Date */
  lua_pushnumber(L, result.time.ut + JD_J2000_EPOCH);
  return 9;
err:
  lua_pushfail(L);
  return 1;
}


static int aux_getyearforseason (lua_State *L, const char *procname) {
  int year;
  if (lua_gettop(L) == 0) {
    int im, id;
    double x, fd;
    astro_time_t atime;
    atime = Astronomy_CurrentTime();
    x = atime.ut + JD_J2000_EPOCH;
    if (iauJd2cal(x, 0, &year, &im, &id, &fd) == -1) {
      luaL_error(L, "Error in " LUA_QS ": time could not be converted.", procname);
    }
  } else {
    year = agn_checkinteger(L, 1);  /* Gregorian year */
  }
  return year;
}

static int astro_seasons (lua_State *L) {  /* 7.8.7 */
  int year = aux_getyearforseason(L, "astro.seasons");
  astro_seasons_t seasons = Astronomy_Seasons(year);
  if (seasons.status != 0) {
    lua_pushfail(L);
  } else {
    lua_createtable(L, 0, 4);
    lua_rawsetstringnumber(L, -1, "spring", seasons.mar_equinox.ut + JD_J2000_EPOCH);
    lua_rawsetstringnumber(L, -1, "summer", seasons.jun_solstice.ut + JD_J2000_EPOCH);
    lua_rawsetstringnumber(L, -1, "autumn", seasons.sep_equinox.ut + JD_J2000_EPOCH);
    lua_rawsetstringnumber(L, -1, "winter", seasons.dec_solstice.ut + JD_J2000_EPOCH);
  }
  return 1;
}


static int astro_spring (lua_State *L) {  /* 7.8.7 */
  int year = aux_getyearforseason(L, "astro.spring");
  astro_seasons_t seasons = Astronomy_Seasons(year);
  if (seasons.status != 0) {
    lua_pushfail(L);
  } else {
    lua_pushnumber(L, seasons.mar_equinox.ut + JD_J2000_EPOCH);
  }
  return 1;
}


static int astro_summer (lua_State *L) {  /* 7.8.7 */
  int year = aux_getyearforseason(L, "astro.summer");
  astro_seasons_t seasons = Astronomy_Seasons(year);
  if (seasons.status != 0) {
    lua_pushfail(L);
  } else {
    lua_pushnumber(L, seasons.jun_solstice.ut + JD_J2000_EPOCH);
  }
  return 1;
}


static int astro_autumn (lua_State *L) {  /* 7.8.7 */
  int year = aux_getyearforseason(L, "astro.autumn");
  astro_seasons_t seasons = Astronomy_Seasons(year);
  if (seasons.status != 0) {
    lua_pushfail(L);
  } else {
    lua_pushnumber(L, seasons.sep_equinox.ut + JD_J2000_EPOCH);
  }
  return 1;
}


static int astro_winter (lua_State *L) {  /* 7.8.7 */
  int year = aux_getyearforseason(L, "astro.winter");
  astro_seasons_t seasons = Astronomy_Seasons(year);
  if (seasons.status != 0) {
    lua_pushfail(L);
  } else {
    lua_pushnumber(L, seasons.dec_solstice.ut + JD_J2000_EPOCH);
  }
  return 1;
}


static void aux_getcoordsparams (lua_State *L, int nargs,
    lua_Number *p, lua_Number *q,
    lua_Number *lat, lua_Number *lon,
    int *yy, int *mm, int *dd, int *h, int *m, lua_Number *s,
    int *dir) {
  int shift = 0;
  *p   = agn_checknumber(L, 1);
  *q   = agn_checknumber(L, 2);
  *lat = agn_checknumber(L, 3);
  *lon = agn_checknumber(L, 4);
  if (nargs > 6) {
    *yy = agn_checkinteger(L, 5);
    *mm = agn_checkposint(L, 6);
    *dd = agn_checkposint(L, 7);
    *h  = agn_checknonnegint(L, 8);
    *m  = agn_checknonnegint(L, 9);
    *s  = agn_checknonnegative(L, 10);
    shift = 5;
  } else {
    double fd; (void)fd;  /* fractional date, unused */
    if (tools_jd2cdate(agn_checknumber(L, 5), yy, mm, dd, &fd, h, m, s) == -1) {
      luaL_error(L, "Error in " LUA_QS ": invalid Julian date.", "astro.coords");
    }
  }
  if (*mm > 12 || *dd > 31 || *h > 23 || *m > 59 || *s >= 60)
    luaL_error(L, "Error in " LUA_QS ": invalid Gregorian date.", "astro.coords");
  if (*lon < -180 || *lon > 180)  /* longitude = Längengrad (E/W), 7.8.9 extension */
    luaL_error(L, "in `%s`: longitude must be in the range [-180, 180].", "astro.coords");
  if (*lat < -90 || *lat > 90)  /* latitude = Breitengrad (N/S), 7.8.9 extension */
    luaL_error(L, "in `%s`: latitude must be in the range [-90, 90].", "astro.coords");
  *dir = agnL_optinteger(L, 6 + shift, 1);
}


/* Converts between Local Horizontal Coordinates (altitude/azimuth) to Equatorial
   (right ascension/declination) and vice versa for the given location, date and time. */
static int astro_coords (lua_State *L) {  /* 7.8.8 */
  astro_time_t atime;
  int yy, mm, dd, h, m, dir, nargs;
  double lat, lon, p, q, s;
  /* long double precision does not improve the result much ... */
  long double x, y, ha_rad, ha_hours, lst_hours, dec_rad;
  /* 1. Read input Alt/Az from the stack */
  /* 2. Extract observer location and time data starting from index 3 */
  nargs = lua_gettop(L);
  dir = 1;
  aux_getcoordsparams(L, nargs, &p, &q, &lat, &lon, &yy, &mm, &dd, &h, &m, &s, &dir);
  atime = Astronomy_MakeTime(yy, mm, dd, h, m, s);
  /* Fetch Greenwich Sidereal Time (GST) */
  lst_hours = Astronomy_SiderealTime(&atime);
  if (dir == 1) {
    /* Convert Local Horizontal Coordinates (Alt/Az) to Equatorial (RA/Dec) */
    long double azimuth, altitude, az_rad, alt_rad, lat_rad, sin_dec, ra_hours, cos_dec;
    /* 3. Convert input degrees to radians for trigonometry */
    altitude = p; azimuth = q;
    az_rad  = DEG2RADL*azimuth;
    alt_rad = DEG2RADL*altitude;
    lat_rad = DEG2RADL*lat;
    /* 4. Calculate Declination via Spherical Trigonometry */
    sin_dec = sun_sinl(alt_rad)*sun_sinl(lat_rad) + sun_cosl(alt_rad)*sun_cosl(lat_rad)*sun_cosl(az_rad);
    /* Safe boundaries check for asin */
    if (sin_dec > 1.0)  sin_dec = 1.0L;
    if (sin_dec < -1.0) sin_dec = -1.0L;
    dec_rad = sun_asinl(sin_dec);
    cos_dec = sun_cosl(dec_rad);
    /* 5. Calculate Hour Angle (HA) with singularity protection */
    if (tools_fabsl(cos_dec) < 1e-7L) {
        /* Object is exactly at celestial pole; HA is mathematically undefined */
        ha_hours = 0.0L;
    } else {
        y = -sun_sinl(az_rad)*sun_cosl(alt_rad)/cos_dec;
        x = (sun_sinl(alt_rad) - sun_sinl(lat_rad)*sin_dec)/(sun_cosl(lat_rad)*cos_dec);
        ha_rad = sun_atan2l(y, x);
        /* Correct unit conversion: radians -> degrees -> hours */
        ha_hours = (ha_rad*RAD2DEGL)/15.0L;
        if (ha_hours < 0.0L) ha_hours += 24.0L;
    }
    /* Adjust Greenwich Sidereal Time for local longitude */
    lst_hours += lon/15.0L;
    if (lst_hours < 0.0L) lst_hours += 24.0L;
    if (lst_hours >= 24.0L) lst_hours = sun_fmodl(lst_hours, 24.0L);
    /* 7. Right Ascension (RA) = Local Sidereal Time - Hour Angle */
    ra_hours = lst_hours - ha_hours;
    if (ra_hours < 0.0L) ra_hours += 24.0L;
    if (ra_hours >= 24.0L) ra_hours = sun_fmodl(ra_hours, 24.0L);
    /* 8. Return the results in a Lua table */
    lua_createtable(L, 0, 2);
    lua_rawsetstringnumber(L, -1, "ra",          ra_hours);
    lua_rawsetstringnumber(L, -1, "declination", RAD2DEGL*dec_rad);
  } else {
    /* Convert Equatorial (RA/Dec) to Local Horizontal Coordinates (Alt/Az) */
    /* 3. Fetch Greenwich Sidereal Time (GST) and adjust for local longitude */
    long double ra_hours, dec_degs, lat_rad, sin_alt, alt_rad, az_rad, az_degs, cos_alt;
    ra_hours  = p;
    dec_degs  = q;
    lst_hours += lon/15.0L;
    if (lst_hours < 0.0L) lst_hours += 24.0L;
    if (lst_hours >= 24.0L) lst_hours = sun_fmodl(lst_hours, 24.0L);
    /* 4. Calculate local Hour Angle (HA) */
    ha_hours = lst_hours - ra_hours;
    if (ha_hours < 0.0L) ha_hours += 24.0L;  /* Keep HA positive for consistency */
    if (ha_hours >= 24.0L) ha_hours = sun_fmodl(ha_hours, 24.0L);
    ha_rad = DEG2RADL*ha_hours*15.0L;
    /* 5. Convert inputs to radians */
    dec_rad = DEG2RADL*dec_degs;
    lat_rad = DEG2RADL*lat;
    /* 6. Calculate Altitude using Spherical Trigonometry */
    sin_alt = sun_sinl(dec_rad)*sun_sinl(lat_rad) + sun_cosl(dec_rad)*sun_cosl(lat_rad)*sun_cosl(ha_rad);
    if (sin_alt > 1.0L)  sin_alt = 1.0L;
    if (sin_alt < -1.0L) sin_alt = -1.0L;
    alt_rad = sun_asinl(sin_alt); /* Swapped asin to sun_asin */
    /* 7. Calculate Azimuth using Spherical Trigonometry with Zenith Protection */
    cos_alt = sun_cosl(alt_rad);
    if (tools_fabsl(cos_alt) < 1e-7L) {
      /* Handle zenith singularity gracefully */
      az_degs = 0.0L;
    } else {
      y = -sun_sinl(ha_rad)*sun_cosl(dec_rad)/cos_alt;
      x = (sun_sinl(dec_rad)*sun_cosl(lat_rad) - sun_cosl(dec_rad)*sun_sinl(lat_rad)*sun_cos(ha_rad))/cos_alt;
      az_rad = sun_atan2l(y, x);
      az_degs = RAD2DEGL*az_rad;
      if (az_degs < 0.0L) az_degs += 360.0L;
      if (az_degs >= 360.0L) az_degs = sun_fmodl(az_degs, 360.0L);
    }
    /* 8. Return the results in a Lua table */
    lua_createtable(L, 0, 2);
    lua_rawsetstringnumber(L, -1, "azimuth",  az_degs);
    lua_rawsetstringnumber(L, -1, "altitude", RAD2DEGL*alt_rad);
  }
  return 1;
}


/* Calculates Greenwich Apparent Sidereal Time (GAST) for the given date and time in UTC.

Given a date and time, this function calculates the rotation of the Earth, represented by the equatorial angle of
the Greenwich prime meridian with respect to distant stars (not the Sun, which moves relative to background stars
by almost one degree per day). This angle is called Greenwich Apparent Sidereal Time (GAST). GAST is measured in
sidereal hours in the half-open range [0, 24). When GAST = 0, it means the prime meridian is aligned with the
of-date equinox, corrected at that time for precession and nutation of the Earth's axis. In this context, the
"equinox" is the direction in space where the Earth's orbital plane (the ecliptic) intersects with the plane of
the Earth's equator, at the location on the Earth's orbit of the (seasonal) March equinox. As the Earth rotates,
GAST increases from 0 up to 24 sidereal hours, then starts over at 0. To convert to degrees, multiply the
return value by 15.

For Local Siderial Time, add the longitude of interest (in decimal degrees) divided by 15 to the result r of
astro.siderial. Add 24 if r is negative. If r is greater than or equal to 24, compute r % 24. 7.8.8 */

static int astro_siderial (lua_State *L) {
  lua_Number gast;
  int lastgiven, nargs = lua_gettop(L);
  astro_time_t atime = { 0 };
  lastgiven = (nargs == 7) ||  /* local siderial time requested ? */
              (nargs == 2 && agn_isnumber(L, 1) && lua_istable(L, 2)) ||
              (nargs == 2 && agn_isnumber(L, 1) && lua_isnumber(L, 2));
  if (!aux_fetchdatetime(L, 1 + lastgiven, nargs, &atime, "astro.siderial")) goto err;
  gast = Astronomy_SiderealTime(&atime);
  if (lastgiven) {  /* Local Apparent Sidereal Time */
    /* longitude [sic !] and time (calendar components or Julian Date given */
    lua_Number lon = lua_tonumber(L, 1);
    gast += lon/15.0;
    if (gast < 0.0)   gast += 24.0;
    if (gast >= 24.0) gast = sun_fmod(gast, 24.0);
  } else if (!(nargs == 1 || nargs == 6)) {
    goto err;
  }
  lua_pushnumber(L, gast);
  return 1;
err:
  lua_pushfail(L);
  return 1;
}


static const luaL_Reg astrolib[] = {
  {"a2af", astro_a2af},                            /* added on March 23, 2026 */
  {"a2tf", astro_a2tf},                            /* added on March 23, 2026 */
  {"autumn", astro_autumn},                        /* added on March 22, 2026 */
  {"cal2jd", astro_cal2jd},                        /* added on August 26, 2025 */
  {"cdate", astro_cdate},                          /* added on January 07, 2013 */
  {"coords", astro_coords},                        /* added on August 14, 2026 */
  {"d2tf", astro_d2tf},                            /* added on March 23, 2026 */
  {"dectodms", astro_dectodms},                    /* added on January 08, 2013 */
  {"dmstodec", astro_dmstodec},                    /* added on January 06, 2013 */
  {"gethourangle", astro_gethourangle},            /* added on August 13, 2026 */
  {"getposition", astro_getposition},              /* added on August 13, 2026 */
  {"hdate", astro_hdate},                          /* added on January 17, 2016 */
  {"h2jd", astro_h2jd},                            /* added on March 21, 2026 */
  {"illumination", astro_illumination},            /* added on August 11, 2026 */
  {"isdst", astro_isdst},                          /* added on June 08, 2016 */
  {"isleapyear", astro_isleapyear},                /* added on January 07, 2013 */
  {"isvaliddate", astro_isvaliddate},              /* added on August 27, 2025 */
  {"isvalidtime", astro_isvalidtime},              /* added on August 27, 2025 */
  {"jdate", astro_jdate},                          /* added on January 07, 2013 */
  {"lunareclipse", astro_lunareclipse},            /* added on March 24, 2026 */
  {"lunarlibration", astro_lunarlibration},        /* added on August 11, 2026 */
  {"moonphase", astro_moonphase},                  /* added on January 08, 2013 */
  {"moonriseset", astro_moonriseset},              /* added on January 08, 2013 */
  {"normhms", astro_normhms},                      /* added on March 23, 2026 */
  {"pdate", astro_pdate},                          /* added on March 21, 2026 */
  {"peak", astro_peak},                            /* added on March 24, 2026 */
  {"p2jd", astro_p2jd},                            /* added on March 21, 2026 */
  {"persiandate", astro_persiandate},              /* added on March 21, 2026 */
  {"persianyear", astro_persianyear},              /* added on March 21, 2026 */
  {"pmonth", astro_pmonth},                        /* added on March 21, 2026 */
  {"pweekday", astro_pweekday},                    /* added on March 21, 2026 */
  {"solareclipse", astro_solareclipse},            /* added on March 24, 2026 */
  {"spring", astro_spring},                        /* added on March 21, 2026 */
  {"summer", astro_summer},                        /* added on March 22, 2026 */
  {"searchaltitude", astro_searchaltitude},        /* added on August 10, 2026 */
  {"searchhourangle", astro_searchhourangle},      /* added on August 13, 2026 */
  {"searchlunarapsis", astro_searchlunarapsis},    /* added on August 11, 2026 */
  {"searchmoonphase", astro_searchmoonphase},      /* added on August 11, 2026 */
  {"searchplanetapsis", astro_searchplanetapsis},  /* added on August 11, 2026 */
  {"searchriseset", astro_searchriseset},          /* added on August 13, 2026 */
  {"searchsolarnoon", astro_searchsolarnoon},      /* added on August 13, 2026 */
  {"seasons", astro_seasons},                      /* added on August 13, 2026 */
  {"siderial", astro_siderial},                    /* added on August 15, 2026 */
  {"sunriseset", astro_sunriseset},                /* added on January 06, 2013 */
  {"tf2d", astro_tf2d},                            /* added on August 26, 2025 */
  {"time", astro_time},                            /* added on March 24, 2026 */
  {"winter", astro_winter},                        /* added on March 22, 2026 */
  {NULL, NULL}
};


/*
** Open astro library
*/
LUALIB_API int luaopen_astro (lua_State *L) {
  luaL_register(L, AGENA_ASTROLIBNAME, astrolib);
  return 1;
}

