/*
** $Id: units.c, v 1.0.0 by Alexander Walz - Initiated: March 05, 2024
** See Copyright Notice in agena.h
*/

#include <stdio.h>

#define units_c
#define LUA_LIB

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"


#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_UNITSLIBNAME "units"
LUALIB_API int (luaopen_units) (lua_State *L);
#endif


static int units_fahren (lua_State *L) {  /* 3.11.1 */
  lua_pushnumber(L, (agn_checknumber(L, 1)*9.0/5.0) + 32.0);
  return 1;
}


static int units_celsius (lua_State *L) {  /* 3.11.1 */
  lua_pushnumber(L, (agn_checknumber(L, 1) - 32.0)*5.0/9.0);
  return 1;
}


static int units_mile (lua_State *L) {  /* the statute mile is `exactly` 1,609.344[0006] metres. */
  lua_Number x = agn_checknumber(L, 1);
  /* for definition of the standard mile see https://www.ginifab.com/feeds/miles_to_km/
     for definition of the nautical mile see https://de.wikipedia.org/wiki/Seemeile
     3.11.3 corrections */
  lua_pushnumber(L, (lua_gettop(L) == 1) ? x/1.6093440006 : x/1.852216);
  return 1;  /* 4.10.6 fix */
}


static int units_km (lua_State *L) {  /* 3.11.1 */
  lua_Number x = agn_checknumber(L, 1);
  /* see note in units_mile */
  lua_pushnumber(L, (lua_gettop(L) == 1) ? x*1.6093440006 : x*1.852216);
  return 1;
}


/* Example: 1 Indian survey foot = 0.3047996 metres */
static const lua_Number feet2metres[] = {
  1.0,         /* 0, standard case */
  0.3048,      /* 1, International foot */
  0.30480061,  /* 2, US survey foot */
  0.3047990,   /* 3, pre-1959 UK foot */
  0.3047996,   /* 4, Indian survey foot, ditto */
  0.3137,      /* 5, Rhineland foot */
  /* from here on, historical feet follow; the selection is random at best and does not represent any preference of a country, UNDOC */
  0.178,       /* 6, Russian handspan, piad, Russian piad = 4 vershoks = 17.8 cm, see: https://en.wikipedia.org/wiki/Span_(unit) */
  0.316102,    /* 7, Vienna foot, ditto */
  0.33412,     /* 8, Tyrol foot, ditto */
  0.3,         /* 9, Zurich foot, ditto */
  0.297896,    /* 10, piede romano, ditto */
  0.3231,      /* 11, Turin foot, ditto */
  0.34773,     /* 12, Venice or Lombardian foot, ditto */
  0.32484,     /* 13, pied du roi, ditto */
  0.283133,    /* 14, Amsterdam voet, ditto */
  0.288,       /* 15, post-1818 Warsaw stopa, ditto */
  0.279,       /* 16, Toledo pie, ditto */
  0.7112,      /* 17, Russian ell, arshin */
  0.2286005,   /* 18, US span */
  0.9144       /* YARD2METREPOS, yards, 1 yard = 0.9144 metres */
};

#define YARD2METREPOS (sizeof(feet2metres)/sizeof(lua_Number) - 1)

static const char *unitnames[] = {
  "standard", "international", "US", "UK", "India",     /*  0 to 4  */
  "Rhineland", "piad", "Vienna", "Tyrol", "Zurich",     /*  5 to 9  */
  "Rome", "Turin", "Venice", "France", "Amsterdam",     /* 10 to 14 */
  "Warsaw", "Toledo", "arshin", "span", "yard", NULL};  /* 15 to 18 */

static int units_foot (lua_State *L) {  /* 3.11.1 */
  lua_Number x = agn_checknumber(L, 1);  /* x is in metres */
  int mode = luaL_checkoption(L, 2, "international", unitnames);
  if (mode == YARD2METREPOS)
    luaL_error(L, "Error in " LUA_QS ": option " LUA_QL("%s") " is not supported.", "utils.foot", unitnames[mode]);
  lua_pushnumber(L, x/feet2metres[mode]);
  return 1;
}


static int units_metre (lua_State *L) {  /* 3.11.1 */
  lua_Number x = agn_checknumber(L, 1);  /* x is in feet, or Canadian yard or a Russian handspan if 2nd arg is 'yard' or 'piad', respectively. */
  int mode = luaL_checkoption(L, 2, "international", unitnames);
  lua_pushnumber(L, x*feet2metres[mode]);
  return 1;
}


static int units_yard (lua_State *L) {  /* 3.11.1 */
  lua_Number x = agn_checknumber(L, 1);  /* x is in metres */
  lua_pushnumber(L, x/feet2metres[YARD2METREPOS]);
  return 1;
}


/* Example: 1 Indian survey foot = 0.3047996 metres */
static const lua_Number inches2cm[] = {
  2.54000508,  /* 0, inch, 3.11.3 fix, see: Smithonian Physical Tables */
  17.8,        /* 1, Russian handspan, piad, Russian piad = 4 vershoks = 17.8 cm, see: https://en.wikipedia.org/wiki/Span_(unit) */
  22.86005,    /* 2, US span */
  10.16002,    /* 3, hand, 1 hand = 4 inches */
  20.11684     /* 4, link = 2/3 feet */
};

static const char *unitnamescm[] = {
  "inch", "piad", "span", "hand", "link", NULL /* 0 to 4 */
};


static int units_inch (lua_State *L) {  /* 3.11.2 */
  lua_Number x = agn_checknumber(L, 1);  /* x is in centimetres */
  int mode = luaL_checkoption(L, 2, "inch", unitnamescm);  /* here 'inch' means cm actually */
  switch (mode) {
    case 0: lua_pushnumber(L, x/inches2cm[0]); break;     /* cm to inches */
    case 2: lua_pushnumber(L, 9*x); break;                /* US span to inches */
    case 3: lua_pushnumber(L, 4*x); break;                /* hand to inches */
    default:
      lua_pushnumber(L, x*inches2cm[mode]/inches2cm[0]);  /* first calculate cm, then convert to inches */
  }
  return 1;
}


static int units_cm (lua_State *L) {  /* 3.11.2 */
  lua_Number x = agn_checknumber(L, 1);  /* x is in inches or in Russian handspans if any option is given */
  int mode = luaL_checkoption(L, 2, "inch", unitnamescm);
  lua_pushnumber(L, x*inches2cm[mode]);  /* 3.11.3 fix & extension */
  return 1;
}


static int units_gram (lua_State *L) {  /* 3.11.2 */
  lua_Number x = agn_checknumber(L, 1);  /* x is in ounces */
  lua_pushnumber(L, x*28.349523125);
  return 1;
}


/* Wikipedia: "The [International] avoirdupois ounce (exactly 28.349523125 g) is 1/16 avoirdupois pound;
   this is the United States customary and British imperial ounce." 3.11.2 */
static int units_ounce (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);  /* x is in grams */
  lua_pushnumber(L, x/28.349523125);
  return 1;
}


static int units_floz (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);  /* x is in litres */
  lua_pushnumber(L, (lua_gettop(L) == 1) ?
    x*33.8140227018429971686 :  /* US fluid ounce */
    x*35.1950797278540460044);  /* imperial fluid ounce */
  return 1;
}


static const lua_Number tolitres[] = {
  1.0,              /* 0, unity */
  0.0295735295625,  /* 1, US fluid ounce */
  0.0284130625,     /* 2, imperial fluid ounce */
  3.785411784,      /* 3, US liquid gallon */
  4.40488377086,    /* 4, US dry gallon */
  4.54609           /* 5, British imperial gallon */
};

static const char *fluidnames[] = {
  "standard", "US floz", "imp floz", "gallon", "dry gallon", "imp gallon", NULL};  /* 0 to 5 */

static int units_litre (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);  /* x is in fl oz. or gallons */
  int mode = luaL_checkoption(L, 2, "US floz", fluidnames);
  lua_pushnumber(L, x*tolitres[mode]);  /* US fluid ounce */
  return 1;
}


static int units_gallon (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);  /* x is in litres */
  int mode = luaL_checkoption(L, 2, "gallon", fluidnames);  /* convert to US liquid gallons by default */
  if (mode < 3)
    luaL_error(L, "Error in " LUA_QS ": option " LUA_QL("%s") " is not supported.", "utils.gallon", fluidnames[mode]);
  lua_pushnumber(L, x/tolitres[mode]);  /* US fluid ounce */
  return 1;
}


static const luaL_Reg units[] = {
  {"celsius", units_celsius},  /* added on March 05, 2024 */
  {"cm", units_cm},            /* added on March 10, 2024 */
  {"fahren", units_fahren},    /* added on March 05, 2024 */
  {"floz", units_floz},        /* added on March 10, 2024 */
  {"foot", units_foot},        /* added on March 05, 2024 */
  {"gallon", units_gallon},    /* added on March 10, 2024 */
  {"gram", units_gram},        /* added on March 10, 2024 */
  {"inch", units_inch},        /* added on March 10, 2024 */
  {"km", units_km},            /* added on March 05, 2024 */
  {"liter", units_litre},      /* added on March 10, 2024 */
  {"litre", units_litre},      /* added on March 10, 2024 */
  {"meter", units_metre},      /* added on March 05, 2024 */
  {"metre", units_metre},      /* added on March 05, 2024 */
  {"mile", units_mile},        /* added on March 05, 2024 */
  {"ounce", units_ounce},      /* added on March 10, 2024 */
  {"yard", units_yard},        /* added on March 05, 2024 */
  {NULL, NULL}
};


/*
** Open units library
*/
LUALIB_API int luaopen_units (lua_State *L) {
  luaL_register(L, AGENA_UNITSLIBNAME, units);
  return 1;
}

/* ====================================================================== */

