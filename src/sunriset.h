#ifndef sunriset_h
#define sunriset_h

#include "agnhlps.h"

extern const char*  timezone_name;
extern long int     timezone_offset;

#define TMOD(x)     ((x) < 0 ? (x) + 24:((x) >= 24 ? (x) - 24:(x)))
#define DAYSOFF(x)  ((x) < 0 ? "(-1) ":((x) >= 24 ? "(+1) ":""))

#define HOURS(h)    ((int)(sun_floor(h)))
#define MINUTES(h)  ((int)(60*(h - sun_floor(h))))

/* A macro to compute the number of days elapsed since 2000 Jan 0.0 */
/* (which is equal to 1999 Dec 31, 0h UT)                           */
/* Dan R sez: This is some pretty fucking high magic. */
#define days_since_2000_Jan_0(y,m,d) \
    (367L*(y) - ((7*((y) + (((m)+9)/12)))/4) + ((275*(m))/9) + (d) - 730530L)

/* Some conversion factors between radians and degrees */

#define RADEG       (180.0/PI)
#define DEGRAD      (PI/180.0)

/* The trigonometric functions in degrees */

#define sind(x)     sun_sin((x)*DEGRAD)
#define cosd(x)     sun_cos((x)*DEGRAD)
#define tand(x)     sun_tan((x)*DEGRAD)

#define atand(x)    (RADEG*sun_atan(x))
#define asind(x)    (RADEG*sun_asin(x))
#define acosd(x)    (RADEG*sun_acos(x))
#define atan2d(y,x) (RADEG*sun_atan2((y),(x)))

/* Following are some macros around the "workhorse" function __daylen__ */
/* They mainly fill in the desired values for the reference altitude    */
/* below the horizon, and also selects whether this altitude should     */
/* refer to the Sun's center or its upper limb.                         */


/* This macro computes the length of the day, from sunrise to sunset. */
/* Sunrise/set is considered to occur when the Sun's upper limb is    */
/* 50 arc minutes below the horizon (this accounts for the refraction */
/* of the Earth's atmosphere).                                        */
/* The original version of the program used the value of 35 arc mins, */
/* which is the accepted value in Sweden.                             */
#define day_length(year,month,day,lon,lat) \
        __daylen__(year, month, day, lon, lat, -50.0/60.0, 1)

/* This macro computes the length of the day, including civil twilight. */
/* Civil twilight starts/ends when the Sun's center is 6 degrees below  */
/* the horizon.                                                         */
#define day_civil_twilight_length(year,month,day,lon,lat) \
        __daylen__(year, month, day, lon, lat, -6.0, 0)

/* This macro computes the length of the day, incl. nautical twilight.  */
/* Nautical twilight starts/ends when the Sun's center is 12 degrees    */
/* below the horizon.                                                   */
#define day_nautical_twilight_length(year,month,day,lon,lat) \
        __daylen__(year, month, day, lon, lat, -12.0, 0)

/* This macro computes the length of the day, incl. astronomical twilight. */
/* Astronomical twilight starts/ends when the Sun's center is 18 degrees   */
/* below the horizon.                                                      */
#define day_astronomical_twilight_length(year,month,day,lon,lat) \
        __daylen__(year, month, day, lon, lat, -18.0, 0)


/* This macro computes times for sunrise/sunset.                      */
/* Sunrise/set is considered to occur when the Sun's upper limb is    */
/* 35 arc minutes below the horizon (this accounts for the refraction */
/* of the Earth's atmosphere).                                        */
#define sun_rise_set(year,month,day,lon,lat,rise,set,south) \
        __sunriset__(year, month, day, lon, lat, -35.0/60.0, 1, rise, set, south)

/* This macro computes the start and end times of civil twilight.       */
/* Civil twilight starts/ends when the Sun's center is 6 degrees below  */
/* the horizon.                                                         */
#define civil_twilight(year,month,day,lon,lat,start,end,south) \
        __sunriset__(year, month, day, lon, lat, -6.0, 0, start, end, south)

/* This macro computes the start and end times of nautical twilight.    */
/* Nautical twilight starts/ends when the Sun's center is 12 degrees    */
/* below the horizon.                                                   */
#define nautical_twilight(year,month,day,lon,lat,start,end,south) \
        __sunriset__(year, month, day, lon, lat, -12.0, 0, start, end, south)

/* This macro computes the start and end times of astronomical twilight.   */
/* Astronomical twilight starts/ends when the Sun's center is 18 degrees   */
/* below the horizon.                                                      */
#define astronomical_twilight(year,month,day,lon,lat,start,end,south) \
        __sunriset__(year, month, day, lon, lat, -18.0, 0, start, end, south)


/* Function prototypes */

double __daylen__ (int year, int month, int day, double lon, double lat,
                   double altit, int upper_limb);

int    __sunriset__ (int year, int month, int day, double lon, double lat,
                     double altit, int upper_limb, double *rise, double *set,
                     double *tsouth);

void   sunpos (double d, double *lon, double *r);

void   sun_RA_dec (double d, double *RA, double *dec, double *r);

double revolution (double x);

double rev180 (double x);

double GMST0 (double d);

#endif
