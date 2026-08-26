/* Sources found at http://www.rosettacalendar.com/
   Copyright 1993-2015, Scott E. Lee */

/* $selId: gregor.c,v 2.0 1995/10/24 01:13:06 lees Exp $
 * Copyright 1993-1995, Scott E. Lee, all rights reserved.
 * Permission granted to use, copy, modify, distribute and sell so long as
 * the above copyright and this permission statement are retained in all
 * copies.  THERE IS NO WARRANTY - USE AT YOUR OWN RISK.
 */

/**************************************************************************
 *
 * These are the externally visible components of this file:
 *
 *     void
 *     SdnToGregorian(
 *         long int  sdn,
 *         int      *pYear,
 *         int      *pMonth,
 *         int      *pDay);
 *
 * Convert a SDN to a Gregorian calendar date.  If the input SDN is less
 * than 1, the three output values will all be set to zero, otherwise
 * *pYear will be >= -4714 and != 0; *pMonth will be in the range 1 to 12
 * inclusive; *pDay will be in the range 1 to 31 inclusive.
 *
 *     long int
 *     GregorianToSdn(
 *         int inputYear,
 *         int inputMonth,
 *         int inputDay);
 *
 * Convert a Gregorian calendar date to a SDN.  Zero is returned when the
 * input date is detected as invalid or out of the supported range.  The
 * return value will be > 0 for all valid, supported dates, but there are
 * some invalid dates that will return a positive value.  To verify that a
 * date is valid, convert it to SDN and then back and compare with the
 * original.
 *
 *     char *MonthNameShort[13];
 *
 * Convert a Gregorian month number (1 to 12) to the abbreviated (three
 * character) name of the Gregorian month (null terminated).  An index of
 * zero will return a zero length string.
 *
 *     char *MonthNameLong[13];
 *
 * Convert a Gregorian month number (1 to 12) to the name of the Gregorian
 * month (null terminated).  An index of zero will return a zero length
 * string.
 *
 * VALID RANGE
 *
 *     4714 B.C. to at least 10000 A.D.
 *
 *     Although this software can handle dates all the way back to 4714
 *     B.C., such use may not be meaningful.  The Gregorian calendar was
 *     not instituted until October 15, 1582 (or October 5, 1582 in the
 *     Julian calendar).  Some countries did not accept it until much
 *     later.  For example, Britain converted in 1752, The USSR in 1918 and
 *     Greece in 1923.  Most European countries used the Julian calendar
 *     prior to the Gregorian.
 *
 * CALENDAR OVERVIEW
 *
 *     The Gregorian calendar is a modified version of the Julian calendar.
 *     The only difference being the specification of leap years.  The
 *     Julian calendar specifies that every year that is a multiple of 4
 *     will be a leap year.  This leads to a year that is 365.25 days long,
 *     but the current accepted value for the tropical year is 365.242199
 *     days.
 *
 *     To correct this error in the length of the year and to bring the
 *     vernal equinox back to March 21, Pope Gregory XIII issued a papal
 *     bull declaring that Thursday October 4, 1582 would be followed by
 *     Friday October 15, 1582 and that centennial years would only be a
 *     leap year if they were a multiple of 400.  This shortened the year
 *     by 3 days per 400 years, giving a year of 365.2425 days.
 *
 *     Another recently proposed change in the leap year rule is to make
 *     years that are multiples of 4000 not a leap year, but this has never
 *     been officially accepted and this rule is not implemented in these
 *     algorithms.
 *
 * ALGORITHMS
 *
 *     The calculations are based on three different cycles: a 400 year
 *     cycle of leap years, a 4 year cycle of leap years and a 5 month
 *     cycle of month lengths.
 *
 *     The 5 month cycle is used to account for the varying lengths of
 *     months.  You will notice that the lengths alternate between 30
 *     and 31 days, except for three anomalies: both July and August
 *     have 31 days, both December and January have 31, and February
 *     is less than 30.  Starting with March, the lengths are in a
 *     cycle of 5 months (31, 30, 31, 30, 31):
 *
 *         Mar   31 days  \
 *         Apr   30 days   |
 *         May   31 days    > First cycle
 *         Jun   30 days   |
 *         Jul   31 days  /
 *
 *         Aug   31 days  \
 *         Sep   30 days   |
 *         Oct   31 days    > Second cycle
 *         Nov   30 days   |
 *         Dec   31 days  /
 *
 *         Jan   31 days  \
 *         Feb 28/9 days   |
 *                          > Third cycle (incomplete)
 *
 *     For this reason the calculations (internally) assume that the
 *     year starts with March 1.
 *
 * TESTING
 *
 *     This algorithm has been tested from the year 4714 B.C. to 10000
 *     A.D.  The source code of the verification program is included in
 *     this package.
 *
 * REFERENCES
 *
 *     Conversions Between Calendar Date and Julian Day Number by Robert J.
 *     Tantzen, Communications of the Association for Computing Machinery
 *     August 1963.  (Also published in Collected Algorithms from CACM,
 *     algorithm number 199).
 *
 **************************************************************************/

#define sdncal_c
#define LUA_LIB


#define SDN_OFFSET         32045
#define DAYS_PER_5_MONTHS  153
#define DAYS_PER_4_YEARS   1461
#define DAYS_PER_400_YEARS 146097

void
SdnToGregorian(
    long int  sdn,
    int      *pYear,
    int      *pMonth,
    int      *pDay)
{
    int       century;
    int       year;
    int       month;
    int       day;
    long int  temp;
    int       dayOfYear;

    if (sdn <= 0) {
	*pYear = 0;
	*pMonth = 0;
	*pDay = 0;
	return;
    }

    temp = (sdn + SDN_OFFSET) * 4 - 1;

    /* Calculate the century (year/100). */
    century = temp / DAYS_PER_400_YEARS;

    /* Calculate the year and day of year (1 <= dayOfYear <= 366). */
    temp = ((temp % DAYS_PER_400_YEARS) / 4) * 4 + 3;
    year = (century * 100) + (temp / DAYS_PER_4_YEARS);
    dayOfYear = (temp % DAYS_PER_4_YEARS) / 4 + 1;

    /* Calculate the month and day of month. */
    temp = dayOfYear * 5 - 3;
    month = temp / DAYS_PER_5_MONTHS;
    day = (temp % DAYS_PER_5_MONTHS) / 5 + 1;

    /* Convert to the normal beginning of the year. */
    if (month < 10) {
	month += 3;
    } else {
	year += 1;
	month -= 9;
    }

    /* Adjust to the B.C./A.D. type numbering. */
    year -= 4800;
    if (year <= 0) year--;

    *pYear = year;
    *pMonth = month;
    *pDay = day;
}

long int
GregorianToSdn(
    int inputYear,
    int inputMonth,
    int inputDay)
{
    int year;
    int month;

    /* check for invalid dates */
    if (inputYear == 0 || inputYear < -4714 ||
	inputMonth <= 0 || inputMonth > 12 ||
	inputDay <= 0 || inputDay > 31)
    {
	return(0);
    }

    /* check for dates before SDN 1 (Nov 25, 4714 B.C.) */
    if (inputYear == -4714) {
	if (inputMonth < 11) {
	    return(0);
	}
	if (inputMonth == 11 && inputDay < 25) {
	    return(0);
	}
    }

    /* Make year always a positive number. */
    if (inputYear < 0) {
	year = inputYear + 4801;
    } else {
	year = inputYear + 4800;
    }

    /* Adjust the start of the year. */
    if (inputMonth > 2) {
	month = inputMonth - 3;
    } else {
	month = inputMonth + 9;
	year--;
    }

    return( ((year / 100) * DAYS_PER_400_YEARS) / 4
	    + ((year % 100) * DAYS_PER_4_YEARS) / 4
	    + (month * DAYS_PER_5_MONTHS + 2) / 5
	    + inputDay
	    - SDN_OFFSET );
}

char *MonthNameShort[13] = {
    "",
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

char *MonthNameLong[13] = {
    "",
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

/* $selId: julian.c,v 2.0 1995/10/24 01:13:06 lees Exp $
 * Copyright 1993-1995, Scott E. Lee, all rights reserved.
 * Permission granted to use, copy, modify, distribute and sell so long as
 * the above copyright and this permission statement are retained in all
 * copies.  THERE IS NO WARRANTY - USE AT YOUR OWN RISK.
 */

/**************************************************************************
 *
 * These are the externally visible components of this file:
 *
 *     void
 *     SdnToJulian(
 *         long int  sdn,
 *         int      *pYear,
 *         int      *pMonth,
 *         int      *pDay);
 *
 * Convert a SDN to a Julian calendar date.  If the input SDN is less than
 * 1, the three output values will all be set to zero, otherwise *pYear
 * will be >= -4713 and != 0; *pMonth will be in the range 1 to 12
 * inclusive; *pDay will be in the range 1 to 31 inclusive.
 *
 *     long int
 *     JulianToSdn(
 *         int inputYear,
 *         int inputMonth,
 *         int inputDay);
 *
 * Convert a Julian calendar date to a SDN.  Zero is returned when the
 * input date is detected as invalid or out of the supported range.  The
 * return value will be > 0 for all valid, supported dates, but there are
 * some invalid dates that will return a positive value.  To verify that a
 * date is valid, convert it to SDN and then back and compare with the
 * original.
 *
 * VALID RANGE
 *
 *     4713 B.C. to at least 10000 A.D.
 *
 *     Although this software can handle dates all the way back to 4713
 *     B.C., such use may not be meaningful.  The calendar was created in
 *     46 B.C., but the details did not stabilize until at least 8 A.D.,
 *     and perhaps as late at the 4th century.  Also, the beginning of a
 *     year varied from one culture to another - not all accepted January
 *     as the first month.
 *
 * CALENDAR OVERVIEW
 *
 *     Julias Caesar created the calendar in 46 B.C. as a modified form of
 *     the old Roman republican calendar which was based on lunar cycles.
 *     The new Julian calendar set fixed lengths for the months, abandoning
 *     the lunar cycle.  It also specified that there would be exactly 12
 *     months per year and 365.25 days per year with every 4th year being a
 *     leap year.
 *
 *     Note that the current accepted value for the tropical year is
 *     365.242199 days, not 365.25.  This lead to an 11 day shift in the
 *     calendar with respect to the seasons by the 16th century when the
 *     Gregorian calendar was created to replace the Julian calendar.
 *
 *     The difference between the Julian and today's Gregorian calendar is
 *     that the Gregorian does not make centennial years leap years unless
 *     they are a multiple of 400, which leads to a year of 365.2425 days.
 *     In other words, in the Gregorian calendar, 1700, 1800 and 1900 are
 *     not leap years, but 2000 is.  All centennial years are leap years in
 *     the Julian calendar.
 *
 *     The details are unknown, but the lengths of the months were adjusted
 *     until they finally stabilized in 8 A.D. with their current lengths:
 *
 *         January          31
 *         February         28/29
 *         March            31
 *         April            30
 *         May              31
 *         June             30
 *         Quintilis/July   31
 *         Sextilis/August  31
 *         September        30
 *         October          31
 *         November         30
 *         December         31
 *
 *     In the early days of the calendar, the days of the month were not
 *     numbered as we do today.  The numbers ran backwards (decreasing) and
 *     were counted from the Ides (15th of the month - which in the old
 *     Roman republican lunar calendar would have been the full moon) or
 *     from the Nonae (9th day before the Ides) or from the beginning of
 *     the next month.
 *
 *     In the early years, the beginning of the year varied, sometimes
 *     based on the ascension of rulers.  It was not always the first of
 *     January.
 *
 *     Also, today's epoch, 1 A.D. or the birth of Jesus Christ, did not
 *     come into use until several centuries later when Christianity became
 *     a dominant religion.
 *
 * ALGORITHMS
 *
 *     The calculations are based on two different cycles: a 4 year cycle
 *     of leap years and a 5 month cycle of month lengths.
 *
 *     The 5 month cycle is used to account for the varying lengths of
 *     months.  You will notice that the lengths alternate between 30 and
 *     31 days, except for three anomalies: both July and August have 31
 *     days, both December and January have 31, and February is less than
 *     30.  Starting with March, the lengths are in a cycle of 5 months
 *     (31, 30, 31, 30, 31):
 *
 *         Mar   31 days  \
 *         Apr   30 days   |
 *         May   31 days    > First cycle
 *         Jun   30 days   |
 *         Jul   31 days  /
 *
 *         Aug   31 days  \
 *         Sep   30 days   |
 *         Oct   31 days    > Second cycle
 *         Nov   30 days   |
 *         Dec   31 days  /
 *
 *         Jan   31 days  \
 *         Feb 28/9 days   |
 *                          > Third cycle (incomplete)
 *
 *     For this reason the calculations (internally) assume that the year
 *     starts with March 1.
 *
 * TESTING
 *
 *     This algorithm has been tested from the year 4713 B.C. to 10000 A.D.
 *     The source code of the verification program is included in this
 *     package.
 *
 * REFERENCES
 *
 *     Conversions Between Calendar Date and Julian Day Number by Robert J.
 *     Tantzen, Communications of the Association for Computing Machinery
 *     August 1963.  (Also published in Collected Algorithms from CACM,
 *     algorithm number 199).  [Note: the published algorithm is for the
 *     Gregorian calendar, but was adjusted to use the Julian calendar's
 *     simpler leap year rule.]
 *
 **************************************************************************/

#include "sdncal.h"

#define SDN_OFFSET1        32083
#define DAYS_PER_5_MONTHS  153
#define DAYS_PER_4_YEARS   1461


void
SdnToJulian(
    long int  sdn,
    int      *pYear,
    int      *pMonth,
    int      *pDay)
{
    int       year;
    int       month;
    int       day;
    long int  temp;
    int       dayOfYear;

    if (sdn <= 0) {
	*pYear = 0;
	*pMonth = 0;
	*pDay = 0;
	return;
    }

    temp = (sdn + SDN_OFFSET1) * 4 - 1;

    /* Calculate the year and day of year (1 <= dayOfYear <= 366). */
    year = temp / DAYS_PER_4_YEARS;
    dayOfYear = (temp % DAYS_PER_4_YEARS) / 4 + 1;

    /* Calculate the month and day of month. */
    temp = dayOfYear * 5 - 3;
    month = temp / DAYS_PER_5_MONTHS;
    day = (temp % DAYS_PER_5_MONTHS) / 5 + 1;

    /* Convert to the normal beginning of the year. */
    if (month < 10) {
	month += 3;
    } else {
	year += 1;
	month -= 9;
    }

    /* Adjust to the B.C./A.D. type numbering. */
    year -= 4800;
    if (year <= 0) year--;

    *pYear = year;
    *pMonth = month;
    *pDay = day;
}


/* this function calculates the Julian date using the Julian calendar (+13 days offset) */
long int
JulianToSdn(
    int inputYear,
    int inputMonth,
    int inputDay)
{
    int year;
    int month;

    /* check for invalid dates */
    if (inputYear == 0 || inputYear < -4713 ||
	inputMonth <= 0 || inputMonth > 12 ||
	inputDay <= 0 || inputDay > 31)
    {
	return(0);
    }

    /* check for dates before SDN 1 (Jan 2, 4713 B.C.) */
    if (inputYear == -4713) {
	if (inputMonth == 1 && inputDay == 1) {
	    return(0);
	}
    }

    /* Make year always a positive number. */
    if (inputYear < 0) {
	year = inputYear + 4801;
    } else {
	year = inputYear + 4800;
    }

    /* Adjust the start of the year. */
    if (inputMonth > 2) {
	month = inputMonth - 3;
    } else {
	month = inputMonth + 9;
	year--;
    }

    return( (year * DAYS_PER_4_YEARS) / 4
	    + (month * DAYS_PER_5_MONTHS + 2) / 5
	    + inputDay
	    - SDN_OFFSET1 );
}


/**************************************************************************
 *
 * These are the externally visible components of this file:
 *
 *     void
 *     SdnToJewish(
 *         long int sdn,
 *         int *pYear,
 *         int *pMonth,
 *         int *pDay);
 *
 * Convert a SDN to a Jewish calendar date.  If the input SDN is before the
 * first day of year 1, the three output values will all be set to zero,
 * otherwise *pYear will be > 0; *pMonth will be in the range 1 to 13
 * inclusive; *pDay will be in the range 1 to 30 inclusive.  Note that Adar
 * II is assigned the month number 7 and Elul is always 13.
 *
 *     long int
 *     JewishToSdn(
 *         int year,
 *         int month,
 *         int day);
 *
 * Convert a Jewish calendar date to a SDN.  Zero is returned when the
 * input date is detected as invalid or out of the supported range.  The
 * return value will be > 0 for all valid, supported dates, but there are
 * some invalid dates that will return a positive value.  To verify that a
 * date is valid, convert it to SDN and then back and compare with the
 * original.
 *
 *     char *JewishMonthName[14];
 *
 * Convert a Jewish month number (1 to 13) to the name of the Jewish month
 * (null terminated).  An index of zero will return a zero length string.
 *
 * VALID RANGE
 *
 *     Although this software can handle dates all the way back to the year
 *     1 (3761 B.C.), such use may not be meaningful.
 *
 *     The Jewish calendar has been in use for several thousand years, but
 *     in the early days there was no formula to determine the start of a
 *     month.  A new month was started when the new moon was first
 *     observed.
 *
 *     It is not clear when the current rule based calendar replaced the
 *     observation based calendar.  According to the book "Jewish Calendar
 *     Mystery Dispelled" by George Zinberg, the patriarch Hillel II
 *     published these rules in 358 A.D.  But, according to The
 *     Encyclopedia Judaica, Hillel II may have only published the 19 year
 *     rule for determining the occurrence of leap years.
 *
 *     I have yet to find a specific date when the current set of rules
 *     were known to be in use.
 *
 * CALENDAR OVERVIEW
 *
 *     The Jewish calendar is based on lunar as well as solar cycles.  A
 *     month always starts on or near a new moon and has either 29 or 30
 *     days (a lunar cycle is about 29 1/2 days).  Twelve of these
 *     alternating 29-30 day months gives a year of 354 days, which is
 *     about 11 1/4 days short of a solar year.
 *
 *     Since a month is defined to be a lunar cycle (new moon to new moon),
 *     this 11 1/4 day difference cannot be overcome by adding days to a
 *     month as with the Gregorian calendar, so an entire month is
 *     periodically added to the year, making some years 13 months long.
 *
 *     For astronomical as well as ceremonial reasons, the start of a new
 *     year may be delayed until a day or two after the new moon causing
 *     years to vary in length.  Leap years can be from 383 to 385 days and
 *     common years can be from 353 to 355 days.  These are the months of
 *     the year and their possible lengths:
 *
 *                       COMMON YEAR          LEAP YEAR
 *          1 Tishri    30   30   30         30   30   30
 *          2 Heshvan   29   29   30         29   29   30 (variable)
 *          3 Kislev    29   30   30         29   30   30 (variable)
 *          4 Tevet     29   29   29         29   29   29
 *          5 Shevat    30   30   30         30   30   30
 *          6 Adar I    29   29   29         30   30   30 (variable)
 *          7 Adar II   --   --   --         29   29   29 (optional)
 *          8 Nisan     30   30   30         30   30   30
 *          9 Iyyar     29   29   29         29   29   29
 *         10 Sivan     30   30   30         30   30   30
 *         11 Tammuz    29   29   29         29   29   29
 *         12 Av        30   30   30         30   30   30
 *         13 Elul      29   29   29         29   29   29
 *                     ---  ---  ---        ---  ---  ---
 *                     353  354  355        383  384  385
 *
 *     Note that the month names and other words that appear in this file
 *     have multiple possible spellings in the Roman character set.  I have
 *     chosen to use the spellings found in the Encyclopedia Judaica.
 *
 *     Adar II, the month added for leap years, is sometimes referred to as
 *     the 13th month, but I have chosen to assign it the number 7 to keep
 *     the months in chronological order.  This may not be consistent with
 *     other numbering schemes.
 *
 *     Leap years occur in a fixed pattern of 19 years called the metonic
 *     cycle.  The 3rd, 6th, 8th, 11th, 14th, 17th and 19th years of this
 *     cycle are leap years.  The first metonic cycle starts with Jewish
 *     year 1, or 3761/60 B.C.  This is believed to be the year of
 *     creation.
 *
 *     To construct the calendar for a year, you must first find the length
 *     of the year by determining the first day of the year (Tishri 1, or
 *     Rosh Ha-Shanah) and the first day of the following year.  This
 *     selects one of the six possible month length configurations listed
 *     above.
 *
 *     Finding the first day of the year is the most difficult part.
 *     Finding the date and time of the new moon (or molad) is the first
 *     step.  For this purpose, the lunar cycle is assumed to be 29 days 12
 *     hours and 793 halakim.  A halakim is 1/1080th of an hour or 3 1/3
 *     seconds.  (This assumed value is only about 1/2 second less than the
 *     value used by modern astronomers -- not bad for a number that was
 *     determined so long ago.)  The first molad of year 1 occurred on
 *     Sunday at 11:20:11 P.M.  This would actually be Monday, because the
 *     Jewish day is considered to begin at sunset.
 *
 *     Since sunset varies, the day is assumed to begin at 6:00 P.M.  for
 *     calendar calculation purposes.  So, the first molad was 5 hours 793
 *     halakim after the start of Tishri 1, 0001 (which was Monday
 *     September 7, 4761 B.C. by the Gregorian calendar).  All subsequent
 *     molads can be calculated from this starting point by adding the
 *     length of a lunar cycle.
 *
 *     Once the molad that starts a year is determined the actual start of
 *     the year (Tishri 1) can be determined.  Tishri 1 will be the day of
 *     the molad unless it is delayed by one of the following four rules
 *     (called dehiyyot).  Each rule can delay the start of the year by one
 *     day, and since rule #1 can combine with one of the other rules, it
 *     can be delayed as much as two days.
 *
 *         1.  Tishri 1 must never be Sunday, Wednesday or Friday.  (This
 *             is largely to prevent certain holidays from occurring on the
 *             day before or after the Sabbath.)
 *
 *         2.  If the molad occurs on or after noon, Tishri 1 must be
 *             delayed.
 *
 *         3.  If it is a common (not leap) year and the molad occurs on
 *             Tuesday at or after 3:11:20 A.M., Tishri 1 must be delayed.
 *
 *         4.  If it is the year following a leap year and the molad occurs
 *             on Monday at or after 9:32:43 and 1/3 sec, Tishri 1 must be
 *             delayed.
 *
 * GLOSSARY
 *
 *     dehiyyot         The set of 4 rules that determine when the new year
 *                      starts relative to the molad.
 *
 *     halakim          1/1080th of an hour or 3 1/3 seconds.
 *
 *     lunar cycle      The period of time between mean conjunctions of the
 *                      sun and moon (new moon to new moon).  This is
 *                      assumed to be 29 days 12 hours and 793 halakim for
 *                      calendar purposes.
 *
 *     metonic cycle    A 19 year cycle which determines which years are
 *                      leap years and which are common years.  The 3rd,
 *                      6th, 8th, 11th, 14th, 17th and 19th years of this
 *                      cycle are leap years.
 *
 *     molad            The date and time of the mean conjunction of the
 *                      sun and moon (new moon).  This is the approximate
 *                      beginning of a month.
 *
 *     Rosh Ha-Shanah   The first day of the Jewish year (Tishri 1).
 *
 *     Tishri           The first month of the Jewish year.
 *
 * ALGORITHMS
 *
 *     SERIAL DAY NUMBER TO JEWISH DATE
 *
 *     The simplest approach would be to use the rules stated above to find
 *     the molad of Tishri before and after the given day number.  Then use
 *     the molads to find Tishri 1 of the current and following years.
 *     From this the length of the year can be determined and thus the
 *     length of each month.  But this method is used as a last resort.
 *
 *     The first 59 days of the year are the same regardless of the length
 *     of the year.  As a result, only the day number of the start of the
 *     year is required.
 *
 *     Similarly, the last 6 months do not change from year to year.  And
 *     since it can be determined whether the year is a leap year by simple
 *     division, the lengths of Adar I and II can be easily calculated.  In
 *     fact, all dates after the 3rd month are consistent from year to year
 *     (once it is known whether it is a leap year).
 *
 *     This means that if the given day number falls in the 3rd month or on
 *     the 30th day of the 2nd month the length of the year must be found,
 *     but in no other case.
 *
 *     So, the approach used is to take the given day number and round it
 *     to the closest molad of Tishri (first new moon of the year).  The
 *     rounding is not really to the *closest* molad, but is such that if
 *     the day number is before the middle of the 3rd month the molad at
 *     the start of the year is found, otherwise the molad at the end of
 *     the year is found.
 *
 *     Only if the day number is actually found to be in the ambiguous
 *     period of 29 to 31 days is the other molad calculated.
 *
 *     JEWISH DATE TO SERIAL DAY NUMBER
 *
 *     The year number is used to find which 19 year metonic cycle contains
 *     the date and which year within the cycle (this is a division and
 *     modulus).  This also determines whether it is a leap year.
 *
 *     If the month is 1 or 2, the calculation is simple addition to the
 *     first of the year.
 *
 *     If the month is 8 (Nisan) or greater, the calculation is simple
 *     subtraction from beginning of the following year.
 *
 *     If the month is 4 to 7, it is considered whether it is a leap year
 *     and then simple subtraction from the beginning of the following year
 *     is used.
 *
 *     Only if it is the 3rd month is both the start and end of the year
 *     required.
 *
 * TESTING
 *
 *     This algorithm has been tested in two ways.  First, 510 dates from a
 *     table in "Jewish Calendar Mystery Dispelled" were calculated and
 *     compared to the table.  Second, the calculation algorithm described
 *     in "Jewish Calendar Mystery Dispelled" was coded and used to verify
 *     all dates from the year 1 (3761 B.C.) to the year 13760 (10000
 *     A.D.).
 *
 *     The source code of the verification program is included in this
 *     package.
 *
 * REFERENCES
 *
 *     The Encyclopedia Judaica, the entry for "Calendar"
 *
 *     The Jewish Encyclopedia
 *
 *     Jewish Calendar Mystery Dispelled by George Zinberg, Vantage Press,
 *     1963
 *
 *     The Comprehensive Hebrew Calendar by Arthur Spier, Behrman House
 *
 *     The Book of Calendars [note that this work contains many typos]
 *
 **************************************************************************/

#define HALAKIM_PER_HOUR 1080
#define HALAKIM_PER_DAY 25920
#define HALAKIM_PER_LUNAR_CYCLE ((29 * HALAKIM_PER_DAY) + 13753)
#define HALAKIM_PER_METONIC_CYCLE (HALAKIM_PER_LUNAR_CYCLE * (12 * 19 + 7))

#define SDN_OFFSET2 347997
#define NEW_MOON_OF_CREATION 31524

#define SUNDAY    0
#define MONDAY    1
#define TUESDAY   2
#define WEDNESDAY 3
#define THURSDAY  4
#define FRIDAY    5
#define SATURDAY  6

#define NOON (18 * HALAKIM_PER_HOUR)
#define AM3_11_20 ((9 * HALAKIM_PER_HOUR) + 204)
#define AM9_32_43 ((15 * HALAKIM_PER_HOUR) + 589)

static int monthsPerYear[19] = {
    12, 12, 13, 12, 12, 13, 12, 13, 12, 12, 13, 12, 12, 13, 12, 12, 13, 12, 13
};

static int yearOffset[19] = {
    0, 12, 24, 37, 49, 61, 74, 86, 99, 111, 123,
    136, 148, 160, 173, 185, 197, 210, 222
};

char *JewishMonthName[14] = {
    "",
    "Tishri",
    "Heshvan",
    "Kislev",
    "Tevet",
    "Shevat",
    "AdarI",
    "AdarII",
    "Nisan",
    "Iyyar",
    "Sivan",
    "Tammuz",
    "Av",
    "Elul"
};

/************************************************************************
 * Given the year within the 19 year metonic cycle and the time of a molad
 * (new moon) which starts that year, this routine will calculate what day
 * will be the actual start of the year (Tishri 1 or Rosh Ha-Shanah).  This
 * first day of the year will be the day of the molad unless one of 4 rules
 * (called dehiyyot) delays it.  These 4 rules can delay the start of the
 * year by as much as 2 days.
 */
static long int Tishri1(
    int      metonicYear,
    long int moladDay,
    long int moladHalakim)
{
    long int tishri1;
    int dow;
    int leapYear;
    int lastWasLeapYear;

    tishri1 = moladDay;
    dow = tishri1 % 7;
    leapYear = metonicYear == 2 || metonicYear == 5 || metonicYear == 7
	|| metonicYear == 10 || metonicYear == 13 || metonicYear == 16
	|| metonicYear == 18;
    lastWasLeapYear = metonicYear == 3 || metonicYear == 6
	|| metonicYear == 8 || metonicYear == 11 || metonicYear == 14
	|| metonicYear == 17 || metonicYear == 0;

    /* Apply rules 2, 3 and 4. */
    if ((moladHalakim >= NOON) ||
	((!leapYear) && dow == TUESDAY && moladHalakim >= AM3_11_20) ||
	(lastWasLeapYear && dow == MONDAY && moladHalakim >= AM9_32_43))
    {
	tishri1++;
	dow++;
	if (dow == 7) {
	    dow = 0;
	}
    }

    /* Apply rule 1 after the others because it can cause an additional
     * delay of one day. */
    if (dow == WEDNESDAY || dow == FRIDAY || dow == SUNDAY) {
	tishri1++;
    }

    return(tishri1);
}

/************************************************************************
 * Given a metonic cycle number, calculate the date and time of the molad
 * (new moon) that starts that cycle.  Since the length of a metonic cycle
 * is a constant, this is a simple calculation, except that it requires an
 * intermediate value which is bigger that 32 bits.  Because this
 * intermediate value only needs 36 to 37 bits and the other numbers are
 * constants, the process has been reduced to just a few steps.
 */
static void MoladOfMetonicCycle(
    int       metonicCycle,
    long int *pMoladDay,
    long int *pMoladHalakim)
{
    register unsigned long int r1, r2, d1, d2;

    /* Start with the time of the first molad after creation. */
    r1 = NEW_MOON_OF_CREATION;

    /* Calculate metonicCycle * HALAKIM_PER_METONIC_CYCLE.  The upper 32
     * bits of the result will be in r2 and the lower 16 bits will be
     * in r1. */
    r1 += metonicCycle * (HALAKIM_PER_METONIC_CYCLE & 0xFFFF);
    r2 = r1 >> 16;
    r2 += metonicCycle * ((HALAKIM_PER_METONIC_CYCLE >> 16) & 0xFFFF);

    /* Calculate r2r1 / HALAKIM_PER_DAY.  The remainder will be in r1, the
     * upper 16 bits of the quotient will be in d2 and the lower 16 bits
     * will be in d1. */
    d2 = r2 / HALAKIM_PER_DAY;
    r2 -= d2 * HALAKIM_PER_DAY;
    r1 = (r2 << 16) | (r1 & 0xFFFF);
    d1 = r1 / HALAKIM_PER_DAY;
    r1 -= d1 * HALAKIM_PER_DAY;

    *pMoladDay = (d2 << 16) | d1;
    *pMoladHalakim = r1;
}

/************************************************************************
 * Given a day number, find the molad of Tishri (the new moon at the start
 * of a year) which is closest to that day number.  It's not really the
 * *closest* molad that we want here.  If the input day is in the first two
 * months, we want the molad at the start of the year.  If the input day is
 * in the fourth to last months, we want the molad at the end of the year.
 * If the input day is in the third month, it doesn't matter which molad is
 * returned, because both will be required.  This type of "rounding" allows
 * us to avoid calculating the length of the year in most cases.
 */
static void FindTishriMolad(
    long int inputDay,
    int *pMetonicCycle,
    int *pMetonicYear,
    long int *pMoladDay,
    long int *pMoladHalakim)
{
    long int moladDay;
    long int moladHalakim;
    int metonicCycle;
    int metonicYear;

    /* Estimate the metonic cycle number.  Note that this may be an under
     * estimate because there are 6939.6896 days in a metonic cycle not
     * 6940, but it will never be an over estimate.  The loop below will
     * correct for any error in this estimate. */
    metonicCycle = (inputDay + 310) / 6940;

    /* Calculate the time of the starting molad for this metonic cycle. */
    MoladOfMetonicCycle(metonicCycle, &moladDay, &moladHalakim);

    /* If the above was an under estimate, increment the cycle number until
     * the correct one is found.  For modern dates this loop is about 98.6%
     * likely to not execute, even once, because the above estimate is
     * really quite close. */
    while (moladDay < inputDay - 6940 + 310) {
	metonicCycle++;
	moladHalakim += HALAKIM_PER_METONIC_CYCLE;
	moladDay += moladHalakim / HALAKIM_PER_DAY;
	moladHalakim = moladHalakim % HALAKIM_PER_DAY;
    }

    /* Find the molad of Tishri closest to this date. */
    for (metonicYear = 0; metonicYear < 18; metonicYear++) {
	if (moladDay > inputDay - 74) {
	    break;
	}
	moladHalakim += HALAKIM_PER_LUNAR_CYCLE * monthsPerYear[metonicYear];
	moladDay += moladHalakim / HALAKIM_PER_DAY;
	moladHalakim = moladHalakim % HALAKIM_PER_DAY;
    }

    *pMetonicCycle = metonicCycle;
    *pMetonicYear = metonicYear;
    *pMoladDay = moladDay;
    *pMoladHalakim = moladHalakim;
}

/************************************************************************
 * Given a year, find the number of the first day of that year and the date
 * and time of the starting molad.
 */
static void FindStartOfYear(
    int       year,
    int      *pMetonicCycle,
    int      *pMetonicYear,
    long int *pMoladDay,
    long int *pMoladHalakim,
    int      *pTishri1)
{
    *pMetonicCycle = (year - 1) / 19;
    *pMetonicYear = (year - 1) % 19;
    MoladOfMetonicCycle(*pMetonicCycle, pMoladDay, pMoladHalakim);

    *pMoladHalakim += HALAKIM_PER_LUNAR_CYCLE * yearOffset[*pMetonicYear];
    *pMoladDay += *pMoladHalakim / HALAKIM_PER_DAY;
    *pMoladHalakim = *pMoladHalakim % HALAKIM_PER_DAY;

    *pTishri1 = Tishri1(*pMetonicYear, *pMoladDay, *pMoladHalakim);
}

/************************************************************************
 * Given a serial day number (SDN), find the corresponding year, month and
 * day in the Jewish calendar.  The three output values will always be
 * modified.  If the input SDN is before the first day of year 1, they will
 * all be set to zero, otherwise *pYear will be > 0; *pMonth will be in the
 * range 1 to 13 inclusive; *pDay will be in the range 1 to 30 inclusive.
 */
#ifdef DONOTCOMPILEME
void SdnToJewish (long int sdn, int *pYear, int *pMonth, int *pDay) {
    long int inputDay;
    long int day;
    long int halakim;
    int metonicCycle;
    int metonicYear;
    int tishri1;
    int tishri1After;
    int yearLength;

    if (sdn <= SDN_OFFSET2) {
	*pYear = 0;
	*pMonth = 0;
	*pDay = 0;
	return;
    }
    inputDay = sdn - SDN_OFFSET2;

    FindTishriMolad(inputDay, &metonicCycle, &metonicYear, &day, &halakim);
    tishri1 = Tishri1(metonicYear, day, halakim);

    if (inputDay >= tishri1) {
	/* It found Tishri 1 at the start of the year. */
	*pYear = metonicCycle * 19 + metonicYear + 1;
	if (inputDay < tishri1 + 59) {
	    if (inputDay < tishri1 + 30) {
		*pMonth = 1;
		*pDay = inputDay - tishri1 + 1;
	    } else {
		*pMonth = 2;
		*pDay = inputDay - tishri1 - 29;
	    }
	    return;
	}

	/* We need the length of the year to figure this out, so find
	 * Tishri 1 of the next year. */
	halakim += HALAKIM_PER_LUNAR_CYCLE * monthsPerYear[metonicYear];
	day += halakim / HALAKIM_PER_DAY;
	halakim = halakim % HALAKIM_PER_DAY;
	tishri1After = Tishri1((metonicYear + 1) % 19, day, halakim);
    } else {
	/* It found Tishri 1 at the end of the year. */
    *pYear = metonicCycle * 19 + metonicYear;
   	if (inputDay >= tishri1 - 177) {
	    /* 1. Check if the current year is actually a leap year */
	    int isLeap = (monthsPerYear[(*pYear - 1) % 19] == 13);  /* 7.8.8 fix */
	    /* It is one of the last months of the year. */
	    if (inputDay > tishri1 - 30) {
		*pMonth = isLeap ? 13 : 12;  /* Fall back to Month 12 (Elul) in common years! */
		*pDay = inputDay - tishri1 + 30;
	    } else if (inputDay > tishri1 - 60) {
		*pMonth = isLeap ? 12 : 11;  /* Av */
		*pDay = inputDay - tishri1 + 60;
	    } else if (inputDay > tishri1 - 89) {
		*pMonth = isLeap ? 11 : 10;  /* Tammuz */
		*pDay = inputDay - tishri1 + 89;
	    } else if (inputDay > tishri1 - 119) {
		*pMonth = isLeap ? 10 : 9;   /* Sivan */
		*pDay = inputDay - tishri1 + 119;
	    } else if (inputDay > tishri1 - 148) {
		*pMonth = isLeap ? 9 : 8;    /* Iyar */
		*pDay = inputDay - tishri1 + 148;
	    } else {
		*pMonth = isLeap ? 8 : 7;    /* Nisan */
		*pDay = inputDay - tishri1 + 178;
	    }
	    return;
	} else {
	    if (monthsPerYear[(*pYear - 1) % 19] == 13) {
		*pMonth = 7;
		*pDay = inputDay - tishri1 + 207;
		if (*pDay > 0) return;
		(*pMonth)--;
		(*pDay) += 30;
		if (*pDay > 0) return;
		(*pMonth)--;
		(*pDay) += 30;
	    } else {
		*pMonth = 6;
		*pDay = inputDay - tishri1 + 207;
		if (*pDay > 0) return;
		(*pMonth)--;
		(*pDay) += 30;
	    }
	    if (*pDay > 0) return;
	    (*pMonth)--;
	    (*pDay) += 29;
	    if (*pDay > 0) return;

	    /* We need the length of the year to figure this out, so find
	     * Tishri 1 of this year. */
	    tishri1After = tishri1;
	    FindTishriMolad(day - 365,
		&metonicCycle, &metonicYear, &day, &halakim);
	    tishri1 = Tishri1(metonicYear, day, halakim);
	}
    }

    yearLength = tishri1After - tishri1;
    day = inputDay - tishri1 - 29;
    if (yearLength == 355 || yearLength == 385) {
	/* Heshvan has 30 days */
	if (day <= 30) {
	    *pMonth = 2;
	    *pDay = day;
	    return;
	}
	day -= 30;
    } else {
	/* Heshvan has 29 days */
	if (day <= 29) {
	    *pMonth = 2;
	    *pDay = day;
	    return;
	}
	day -= 29;
    }

    /* It has to be Kislev. */
    *pMonth = 3;
    *pDay = day;
}
#endif

void SdnToJewish (long int sdn, int *pYear, int *pMonth, int *pDay) {
  long int inputDay;
  long int day;
  long int halakim;
  int metonicCycle;
  int metonicYear;
  int tishri1;
  int tishri1After;
  int yearLength;
  int calculatedMonth = 0; /* Temporary holder to process normalization safely */

  if (sdn <= SDN_OFFSET2) {
    *pYear = 0;
    *pMonth = 0;
    *pDay = 0;
    return;
  }
  inputDay = sdn - SDN_OFFSET2;

  FindTishriMolad(inputDay, &metonicCycle, &metonicYear, &day, &halakim);
  tishri1 = Tishri1(metonicYear, day, halakim);

  if (inputDay >= tishri1) {
    /* It found Tishri 1 at the start of the year. */
    *pYear = metonicCycle * 19 + metonicYear + 1;
    if (inputDay < tishri1 + 59) {
      if (inputDay < tishri1 + 30) {
        calculatedMonth = 1;
        *pDay = inputDay - tishri1 + 1;
      } else {
        calculatedMonth = 2;
        *pDay = inputDay - tishri1 - 29;
      }
      goto normalize_and_exit;
    }

    /* We need the length of the year to figure this out, so find
     * Tishri 1 of the next year. */
    halakim += HALAKIM_PER_LUNAR_CYCLE * monthsPerYear[metonicYear];
    day += halakim / HALAKIM_PER_DAY;
    halakim = halakim % HALAKIM_PER_DAY;
    tishri1After = Tishri1((metonicYear + 1) % 19, day, halakim);
  } else {
    /* It found Tishri 1 at the end of the year. */
    *pYear = metonicCycle * 19 + metonicYear;
    if (inputDay >= tishri1 - 177) {
      /* Check if the current year is a leap year */
      int isLeap = (monthsPerYear[(*pYear - 1) % 19] == 13);
      /* It is one of the last months of the year. */
      if (inputDay > tishri1 - 30) {
        calculatedMonth = isLeap ? 13 : 12;
        *pDay = inputDay - tishri1 + 30;
      } else if (inputDay > tishri1 - 60) {
        calculatedMonth = isLeap ? 12 : 11;
        *pDay = inputDay - tishri1 + 60;
      } else if (inputDay > tishri1 - 89) {
        calculatedMonth = isLeap ? 11 : 10;
        *pDay = inputDay - tishri1 + 89;
      } else if (inputDay > tishri1 - 119) {
        calculatedMonth = isLeap ? 10 : 9;
        *pDay = inputDay - tishri1 + 119;
      } else if (inputDay > tishri1 - 148) {
        calculatedMonth = isLeap ? 9 : 8;
        *pDay = inputDay - tishri1 + 148;
      } else {
        calculatedMonth = isLeap ? 8 : 7;
        *pDay = inputDay - tishri1 + 178;
      }
      goto normalize_and_exit;
    } else {
      if (monthsPerYear[(*pYear - 1) % 19] == 13) {
        calculatedMonth = 7;
        *pDay = inputDay - tishri1 + 207;
        if (*pDay > 0) goto normalize_and_exit;
        calculatedMonth--;
        *pDay += 30;
        if (*pDay > 0) goto normalize_and_exit;
        calculatedMonth--;
        *pDay += 30;
      } else {
        calculatedMonth = 6;
        *pDay = inputDay - tishri1 + 207;
        if (*pDay > 0) goto normalize_and_exit;
        calculatedMonth--;
        *pDay += 30;
      }
      if (*pDay > 0) goto normalize_and_exit;
      calculatedMonth--;
      *pDay += 29;
      if (*pDay > 0) goto normalize_and_exit;

      /* We need the length of the year to figure this out, so find
       * Tishri 1 of this year. */
      tishri1After = tishri1;
      FindTishriMolad(day - 365, &metonicCycle, &metonicYear, &day, &halakim);
      tishri1 = Tishri1(metonicYear, day, halakim);
    }
  }

  yearLength = tishri1After - tishri1;
  day = inputDay - tishri1 - 29;
  if (yearLength == 355 || yearLength == 385) {
    /* Heshvan has 30 days */
    if (day <= 30) {
      calculatedMonth = 2;
      *pDay = day;
      goto normalize_and_exit;
    }
    day -= 30;
  } else {
    /* Heshvan has 29 days */
    if (day <= 29) {
      calculatedMonth = 2;
      *pDay = day;
      goto normalize_and_exit;
    }
    day -= 29;
  }

  /* It has to be Kislev. */
  calculatedMonth = 3;
  *pDay = day;

normalize_and_exit:
  /* UNIFIED CHRONOLOGICAL MAPPING LOGIC
     This translates the internal backward indices into absolute, 
     uniform standard months: Elul is ALWAYS 12, Adar II is ALWAYS 13. */
  if (monthsPerYear[(*pYear - 1) % 19] == 13) {
    /* Only apply shifting logic inside true 13-month leap years */
    if (calculatedMonth == 13)    *pMonth = 12; /* Elul maps back to 12 */
    else if (calculatedMonth == 12) *pMonth = 11; /* Av maps back to 11 */
    else if (calculatedMonth == 11) *pMonth = 10; /* Tammuz maps back to 10 */
    else if (calculatedMonth == 10) *pMonth = 9;  /* Sivan maps back to 9 */
    else if (calculatedMonth == 9)  *pMonth = 8;  /* Iyar maps back to 8 */
    else if (calculatedMonth == 8)  *pMonth = 7;  /* Nisan maps back to 7 */
    else if (calculatedMonth == 7)  *pMonth = 13; /* Adar II maps uniquely to 13 */
    else *pMonth = calculatedMonth;         /* Months 1-6 stay exactly identical */
  } else {
    /* Regular years remain untouched because they match standard indexing */
    *pMonth = calculatedMonth;
  }
  return;
}

/************************************************************************
 * Given a year, month and day in the Jewish calendar, find the
 * corresponding serial day number (SDN).  Zero is returned when the input
 * date is detected as invalid.  The return value will be > 0 for all valid
 * dates, but there are some invalid dates that will return a positive
 * value.  To verify that a date is valid, convert it to SDN and then back
 * and compare with the original.
 */
long int JewishToSdn(int year, int month, int day) {
    long int sdn;
    int      metonicCycle;
    int      metonicYear;
    int      tishri1;
    int      tishri1After;
    long int moladDay;
    long int moladHalakim;
    int      yearLength;
    int      lengthOfAdarIAndII;

    if (year <= 0 || day <= 0 || day > 30) {
	return(0);
    }

    switch (month) {
    case 1:
    case 2:
	/* It is Tishri or Heshvan - don't need the year length. */
	FindStartOfYear(year, &metonicCycle, &metonicYear,
	    &moladDay, &moladHalakim, &tishri1);
	if (month == 1) {
	    sdn = tishri1 + day - 1;
	} else {
	    sdn = tishri1 + day + 29;
	}
	break;

    case 3:
	/* It is Kislev - must find the year length. */

	/* Find the start of the year. */
	FindStartOfYear(year, &metonicCycle, &metonicYear,
	    &moladDay, &moladHalakim, &tishri1);

	/* Find the end of the year. */
	moladHalakim += HALAKIM_PER_LUNAR_CYCLE * monthsPerYear[metonicYear];
	moladDay += moladHalakim / HALAKIM_PER_DAY;
	moladHalakim = moladHalakim % HALAKIM_PER_DAY;
	tishri1After = Tishri1((metonicYear + 1) % 19, moladDay, moladHalakim);

	yearLength = tishri1After - tishri1;

	if (yearLength == 355 || yearLength == 385) {
	    sdn = tishri1 + day + 59;
	} else {
	    sdn = tishri1 + day + 58;
	}
	break;

    case 4:
    case 5:
    case 6:
	/* It is Tevet, Shevat or Adar I - don't need the year length. */

	FindStartOfYear(year + 1, &metonicCycle, &metonicYear,
	    &moladDay, &moladHalakim, &tishri1After);

	if (monthsPerYear[(year - 1) % 19] == 12) {
	    lengthOfAdarIAndII = 29;
	} else {
	    lengthOfAdarIAndII = 59;
	}

	if (month == 4) {
	    sdn = tishri1After + day - lengthOfAdarIAndII - 237;
	} else if (month == 5) {
	    sdn = tishri1After + day - lengthOfAdarIAndII - 208;
	} else {
	    sdn = tishri1After + day - lengthOfAdarIAndII - 178;
	}
	break;

    default:
	/* It is Adar II or later - don't need the year length. */
	FindStartOfYear(year + 1, &metonicCycle, &metonicYear,
	    &moladDay, &moladHalakim, &tishri1After);

	switch (month) {
	case  7:
	    sdn = tishri1After + day - 207;
	    break;
	case  8:
	    sdn = tishri1After + day - 178;
	    break;
	case  9:
	    sdn = tishri1After + day - 148;
	    break;
	case 10:
	    sdn = tishri1After + day - 119;
	    break;
	case 11:
	    sdn = tishri1After + day - 89;
	    break;
	case 12:
	    sdn = tishri1After + day - 60;
	    break;
	case 13:
	    sdn = tishri1After + day - 30;
	    break;
	default:
	    return(0);
	}
    }
    return(sdn + SDN_OFFSET2);
}