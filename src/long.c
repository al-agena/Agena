/*
** $Id: long.c,v 1.67 15/10/2022 roberto Exp $
** DLong Double 128-bit Arithmetic Library
** See Copyright Notice in agena.h
*/

/* A note in advance, taken from https://stackoverflow.com/questions/33076474/strtold-parsing-lower-number-than-given-number:

   The range is approximately 3.65×10^-4951 to 1.18×10^4932, to both directions.

   "DLong double don't have such large precision [of 128 bits]. If long double on your system is 80-bit Intel extended precision then it has
    only 64 bits of mantissa and can store up to ~19.2 decimal digits. If it's 64-bit double precision IEEE-754 like the case of MSVC then
    it can only be precise to ~15.95 digits – phuclv, Oct 12, 2015.

    This package does not work on 32-bit and 64-bit ARM processors as they do not support 80-bit flots. */

#include <math.h>
#include <errno.h>
#include <string.h>

#define long_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnconf.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "calc.h"
#include "cephes.h"
#include "linalg.h"
#include "long.h"
#include "sunpro.h"

#define AGENA_LIBVERSION	"long 0.5.3 for Agena as of April 19, 2026\n"


#define getdop(L,idx,a) { \
  a = checkandgetdlongnum(L, (idx)); \
}

#define getdbinops(L,a,b) { \
  if (agn_isnumber(L, 1)) { \
    a = (longdouble)agn_tonumber(L, 1); \
    b = checkandgetdlongnum(L, 2); \
  } else if (agn_isnumber(L, 2)) { \
    a = checkandgetdlongnum(L, 1); \
    b = (longdouble)agn_tonumber(L, 2); \
  } else { \
    a = checkandgetdlong(L, 1); \
    b = checkandgetdlong(L, 2); \
  } \
}

#define strictgetdbinops(L,a,b) { \
  a = checkandgetdlongnum(L, 1); \
  b = checkandgetdlongnum(L, 2); \
}

#ifndef __ARMCPU
#define FN_FABS fabsl
#else
#define FN_FABS fabs
#endif


static longdouble dlongL_optnumber (lua_State *L, int narg, longdouble def) {
  return luaL_opt(L, checkandgetdlongnum, narg, def);
}


static longdouble dlongL_optinteger (lua_State *L, int narg, longdouble def) {  /* 2.34.8 */
  return luaL_opt(L, checkandgetdlongint, narg, def);
}


#if defined(OPENSUSE)

#undef truncl
#define truncl(x)  ( ((x) > 0.0L) ? floorl((x)) : ceill((x)) )

#undef fdiml
#define fdiml(x,y) (((x) > (y)) ? (x) - (y) : 0.0L)

#undef fminl
#define fminl fMin

#undef fmaxl
#define fmaxl fMax

#undef nanl
#define nanl nan

#endif

#ifndef __ARMCPU
/* Portable strtold() implementation. This is a blend of MIT-licenced MUSL 1.2.5 code made portable
   with help of Gemini AI, November 04, 2025. 6.3.7

   On OpenSUSE and DOS, strtold with some large numbers sometimes returned a long double one off:
   DJGPP returns -1 off, OpenSUSE 10.3 +1 off.

   Input (hex)             Input (dec)             Output DJGPP
   0x23456789ABCDEF        9927935178558959        9927935178558958
   0x023456789ABCDEF       9927935178558959        9927935178558958
   0x123456789ABCDEF       81985529216486895       81985529216486896
   0x0123456789ABCDEF      81985529216486895       81985529216486896

   This implementation does not show this erroneous behaviour. */

/* 1. Define your own structure to emulate a simple read-only file stream */

#if 0 && LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024

#define LD_B1B_DIG 2
#define LD_B1B_MAX 9007199, 254740991
#define KMAX 128

#elif (LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384) || (LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024)

#define LD_B1B_DIG 3
#define LD_B1B_MAX 18, 446744073, 709551615
#define KMAX 2048

#elif LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384

#define LD_B1B_DIG 4
#define LD_B1B_MAX 10384593, 717069655, 257060992, 658440191
#define KMAX 2048

#else
#error Unsupported long double representation
#endif

#define MYMASK (KMAX-1)

typedef struct {
  const char *buffer;  /* The string to read from */
  size_t length;       /* Total length of the string */
  size_t position;     /* Current read position */
} MEM_FILE;

/* 2. Replacement for sh_fromstring(&f, s) */
static void mem_fromstring (MEM_FILE *f, const char *s) {
  f->buffer = s;
  f->length = tools_strlen(s);
  f->position = 0;  /* Start reading at the beginning */
}

/* 3. Replacement for shgetc(f) (Manual character retrieval) */
static int mem_getc (MEM_FILE *f) {
  if (f->position < f->length) {
    /* Read character and advance position */
    return (int)f->buffer[f->position++];
  }
  return EOF;  /* Indicate end of file */
}

static int mem_ungetc (int c, MEM_FILE *f) {
  if (f->position == 0) {
    /* Cannot ungetc if we are at the very start of the buffer. This is analogous to a
       standard ungetc failing if called twice without an intervening getc. */
    return EOF;
  }
  f->position--;
  /* Optional: Verify the character being "pushed back" matches the buffer content
     Standard ungetc() doesn't require this check, but it's good practice
     if the original code relied on checking the returned character. */
  if ((int)f->buffer[f->position] != c) {
    /* This indicates a severe logic error if the character doesn't match the one we just
       read. For simplicity, we typically just trust the position rewind. FIXME */
  }
  return c;  /* Return the character successfully "ungetc'd" */
}

/* 4. Replacement for shcnt(&f) */
static off_t mem_count (MEM_FILE *f) {
  /* Returns the number of characters consumed (distance from start) */
  return (off_t)f->position;
}

static long long scanexp (MEM_FILE *f, int pok) {
	int c;
	int x;
	long long y;
	int neg = 0;
	c = mem_getc(f);
	if (c == '+' || c == '-') {
		neg = (c == '-');
		c = mem_getc(f);
		if (c - '0' >= 10U && pok) mem_ungetc(c, f);
	}
	if (c - '0' >= 10U) {
		mem_ungetc(c, f);
		return LLONG_MIN;
	}
	for (x=0; c - '0' < 10U && x < INT_MAX/10; c = mem_getc(f))
		x = 10*x + c - '0';
	for (y=x; c - '0' < 10U && y < LLONG_MAX/100; c = mem_getc(f))
		y = 10*y + c - '0';
	for (; c - '0' < 10U; c = mem_getc(f));
	mem_ungetc(c, f);
	return neg ? -y : y;
}

/* just ignore any shlim statements */
#define shlim(f,g)

static long double decfloat (MEM_FILE *f, int c, int bits, int emin, int sign, int pok) {
	uint32_t x[KMAX];
	static const uint32_t th[] = { LD_B1B_MAX };
	int i, j, k, a, z;
	long long lrp = 0, dc = 0;
	long long e10 = 0;
	int lnz = 0;
	int gotdig = 0, gotrad = 0;
	int rp;
	int e2;
	int emax = -emin-bits + 3;
	int denormal = 0;
	long double y;
	long double frac = 0;
	long double bias = 0;
	static const int p10s[] = { 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000 };
	j = 0; k = 0;
	/* Don't let leading zeros consume buffer space */
	for (; c == '0'; c = mem_getc(f)) gotdig = 1;
	if (c == '.') {
		gotrad = 1;
		for (c = mem_getc(f); c == '0'; c = mem_getc(f)) gotdig = 1, lrp--;
	}
	x[0] = 0;
	for (; c - '0' < 10U || c == '.'; c = mem_getc(f)) {
		if (c == '.') {
			if (gotrad) break;
			gotrad = 1;
			lrp = dc;
		} else if (k < KMAX-3) {
			dc++;
			if (c != '0') lnz = dc;
			if (j) x[k] = x[k]*10 + c - '0';
			else x[k] = c - '0';
			if (++j == 9) {
				k++;
				j=0;
			}
			gotdig=1;
		} else {
			dc++;
			if (c!='0') {
				lnz = (KMAX - 4)*9;
				x[KMAX - 4] |= 1;
			}
		}
	}
	if (!gotrad) lrp=dc;
	if (gotdig && (c | 32) == 'e') {
		e10 = scanexp(f, pok);
		if (e10 == LLONG_MIN) {
			if (pok) {
				mem_ungetc(c, f);
			} else {
				shlim(f, 0);
				return 0;
			}
			e10 = 0;
		}
		lrp += e10;
	} else if (c >= 0) {
		mem_ungetc(c, f);
	}
	if (!gotdig) {
		errno = EINVAL;
		shlim(f, 0);
		return 0;
	}
	/* Handle zero specially to avoid nasty special cases later */
	if (!x[0]) return sign * 0.0;
	/* Optimize small integers (w/no exponent) and over/under-flow */
	if (lrp == dc && dc < 10 && (bits > 30 || x[0] >> bits == 0))
		return sign * (long double)x[0];
	if (lrp > -emin/2) {
		errno = ERANGE;
		return sign*LDBL_MAX*LDBL_MAX;
	}
	if (lrp < emin - 2*LDBL_MANT_DIG) {
		errno = ERANGE;
		return sign*LDBL_MIN*LDBL_MIN;
	}
	/* Align incomplete final B1B digit */
	if (j) {
		for (; j < 9; j++) x[k] *= 10;
		k++;
		j = 0;
	}
	a = 0;
	z = k;
	e2 = 0;
	rp = lrp;
	/* Optimize small to mid-size integers (even in exp. notation) */
	if (lnz<9 && lnz<=rp && rp < 18) {
		if (rp == 9) return sign * (long double)x[0];
		if (rp < 9) return sign * (long double)x[0]/p10s[8 - rp];
		int bitlim = bits - 3*(int)(rp - 9);
		if (bitlim > 30 || x[0] >> bitlim == 0)
			return sign*(long double)x[0] * p10s[rp - 10];
	}
	/* Drop trailing zeros */
	for (; !x[z - 1]; z--);
	/* Align radix point to B1B digit boundary */
	if (rp % 9) {
		int rpm9 = rp >= 0 ? rp % 9 : rp % 9 + 9;
		int p10 = p10s[8 - rpm9];
		uint32_t carry = 0;
		for (k=a; k!=z; k++) {
			uint32_t tmp = x[k] % p10;
			x[k] = x[k]/p10 + carry;
			carry = 1000000000/p10 * tmp;
			if (k == a && !x[k]) {
				a = ((a + 1) & MYMASK);
				rp -= 9;
			}
		}
		if (carry) x[z++] = carry;
		rp += 9 - rpm9;
	}
	/* Upscale until desired number of bits are left of radix point */
	while (rp < 9*LD_B1B_DIG || (rp == 9*LD_B1B_DIG && x[a] < th[0])) {
		uint32_t carry = 0;
		e2 -= 29;
		for (k=((z - 1) & MYMASK); ; k=((k - 1) & MYMASK)) {
			uint64_t tmp = ((uint64_t)x[k] << 29) + carry;
			if (tmp > 1000000000) {
				carry = tmp / 1000000000;
				x[k] = tmp % 1000000000;
			} else {
				carry = 0;
				x[k] = tmp;
			}
			if (k == ((z - 1) & MYMASK) && k != a && !x[k]) z = k;
			if (k == a) break;
		}
		if (carry) {
			rp += 9;
			a = ((a - 1) & MYMASK);
			if (a == z) {
				z = ((z - 1) & MYMASK);
				x[(z - 1) & MYMASK] |= x[z];
			}
			x[a] = carry;
		}
	}
	/* Downscale until exactly number of bits are left of radix point */
	for (;;) {
		uint32_t carry = 0;
		int sh = 1;
		for (i=0; i < LD_B1B_DIG; i++) {
			k = ((a + i) & MYMASK);
			if (k == z || x[k] < th[i]) {
				i = LD_B1B_DIG;
				break;
			}
			if (x[(a + i) & MYMASK] > th[i]) break;
		}
		if (i == LD_B1B_DIG && rp == 9*LD_B1B_DIG) break;
		/* FIXME: find a way to compute optimal sh */
		if (rp > 9 + 9*LD_B1B_DIG) sh = 9;
		e2 += sh;
		for (k=a; k != z; k = ((k + 1) & MYMASK)) {
			uint32_t tmp = x[k] & ((1 << sh) - 1);
			x[k] = (x[k] >> sh) + carry;
			carry = (1000000000 >> sh) * tmp;
			if (k == a && !x[k]) {
				a = ((a + 1) & MYMASK);
				i--;
				rp -= 9;
			}
		}
		if (carry) {
			if (((z + 1) & MYMASK) != a) {
				x[z] = carry;
				z = ((z + 1) & MYMASK);
			} else x[(z - 1) & MYMASK] |= 1;
		}
	}
	/* Assemble desired bits into floating point variable */
	for (y=i=0; i < LD_B1B_DIG; i++) {
		if (((a + i) & MYMASK) == z) x[(z=((z + 1) & MYMASK)) - 1] = 0;
		y = 1000000000.0L*y + x[(a + i) & MYMASK];
	}
	y *= sign;
	/* Limit precision for denormal results */
	if (bits > LDBL_MANT_DIG + e2- emin) {
		bits = LDBL_MANT_DIG + e2 - emin;
		if (bits < 0) bits = 0;
		denormal = 1;
	}
	/* Calculate bias term to force rounding, move out lower bits */
	if (bits < LDBL_MANT_DIG) {
		bias = copysignl(scalbn(1, 2*LDBL_MANT_DIG-bits - 1), y);
		frac = fmodl(y, scalbn(1, LDBL_MANT_DIG - bits));
		y -= frac;
		y += bias;
	}
	/* Process tail of decimal input so it can affect rounding */
	if (((a + i) & MYMASK) != z) {
		uint32_t t = x[(a + i) & MYMASK];
		if (t < 500000000 && (t || ((a + i + 1) & MYMASK) != z))
			frac += 0.25*sign;
		else if (t > 500000000)
			frac += 0.75*sign;
		else if (t == 500000000) {
			if (((a + i + 1) & MYMASK) == z)
				frac += 0.5*sign;
			else
				frac += 0.75*sign;
		}
		if (LDBL_MANT_DIG-bits >= 2 && !fmodl(frac, 1))
			frac++;
	}
	y += frac;
	y -= bias;
	if (((e2 + LDBL_MANT_DIG) & INT_MAX) > emax - 5) {
		if (fabsl(y) >= 2/AGN_LDBL_EPSILON) {
			if (denormal && bits == LDBL_MANT_DIG + e2 - emin)
				denormal = 0;
			y *= 0.5;
			e2++;
		}
		if (e2 + LDBL_MANT_DIG>emax || (denormal && frac))
			errno = ERANGE;
	}
	return scalbnl(y, e2);
}

static long double hexfloat (MEM_FILE *f, int bits, int emin, int sign, int pok) {
	uint32_t x = 0;
	long double y = 0;
	long double scale = 1;
	long double bias = 0;
	int gottail = 0, gotrad = 0, gotdig = 0;
	long long rp = 0;
	long long dc = 0;
	long long e2 = 0;
	int d;
	int c;
	c = mem_getc(f);
	/* Skip leading zeros */
	for (; c == '0'; c = mem_getc(f)) gotdig = 1;
	if (c == '.') {
		gotrad = 1;
		c = mem_getc(f);
		/* Count zeros after the radix point before significand */
		for (rp=0; c=='0'; c = mem_getc(f), rp--) gotdig = 1;
	}
	for (; c-'0'<10U || (c|32)-'a'<6U || c=='.'; c = mem_getc(f)) {
		if (c == '.') {
			if (gotrad) break;
			rp = dc;
			gotrad = 1;
		} else {
			gotdig = 1;
			if (c > '9') d = (c|32)+10-'a';
			else d = c-'0';
			if (dc<8) {
				x = x*16 + d;
			} else if (dc < LDBL_MANT_DIG/4+1) {
				y += d*(scale/=16);
			} else if (d && !gottail) {
				y += 0.5*scale;
				gottail = 1;
			}
			dc++;
		}
	}
	if (!gotdig) {
		mem_ungetc(c, f);
		if (pok) {
			mem_ungetc(c, f);
			if (gotrad) mem_ungetc(c, f);
		} else {
			shlim(f, 0);
		}
		return sign * 0.0;
	}
	if (!gotrad) rp = dc;
	while (dc < 8) x *= 16, dc++;
	if ((c | 32)=='p') {
		e2 = scanexp(f, pok);
		if (e2 == LLONG_MIN) {
			if (pok) {
				mem_ungetc(c, f);
			} else {
				shlim(f, 0);
				return 0;
			}
			e2 = 0;
		}
	} else {
		mem_ungetc(c, f);
	}
	e2 += 4*rp - 32;
	if (!x) return sign * 0.0;
	if (e2 > -emin) {
		errno = ERANGE;
		return sign * LDBL_MAX * LDBL_MAX;
	}
	if (e2 < emin - 2*LDBL_MANT_DIG) {
		errno = ERANGE;
		return sign * LDBL_MIN * LDBL_MIN;
	}
	while (x < 0x80000000) {
		if (y >= 0.5) {
			x += x + 1;
			y += y - 1;
		} else {
			x += x;
			y += y;
		}
		e2--;
	}
	if (bits > 32+e2-emin) {
		bits = 32+e2-emin;
		if (bits<0) bits=0;
	}
	if (bits < LDBL_MANT_DIG)
		bias = copysignl(scalbn(1, 32+LDBL_MANT_DIG-bits-1), sign);
	if (bits<32 && y && !(x&1)) x++, y=0;
	y = bias + sign*(long double)x + sign*y;
	y -= bias;
	if (!y) errno = ERANGE;
	return scalbnl(y, e2);
}

static long double __floatscan (MEM_FILE *f, int prec, int pok) {
	int sign = 1;
	size_t i;
	int bits;
	int emin;
	int c;
	switch (prec) {
	case 0:
		bits = FLT_MANT_DIG;
		emin = FLT_MIN_EXP-bits;
		break;
	case 1:
		bits = DBL_MANT_DIG;
		emin = DBL_MIN_EXP-bits;
		break;
	case 2:
		bits = LDBL_MANT_DIG;
		emin = LDBL_MIN_EXP-bits;
		break;
	default:
		return 0;
	}
	while (isspace((c = mem_getc(f))));
	if (c == '+' || c == '-') {
		sign -= 2*(c == '-');
		c = mem_getc(f);
	}
	for (i=0; i < 8 && (c|32) == "infinity"[i]; i++)
		if (i < 7) c = mem_getc(f);
	if (i == 3 || i == 8 || (i > 3 && pok)) {
		if (i != 8) {
			mem_ungetc(c, f);
			if (pok) for (; i > 3; i--) mem_ungetc(c, f);
		}
		return sign*INFINITY;
	}
	if (!i) for (i=0; i < 3 && (c | 32) == "nan"[i]; i++)
		if (i < 2) c = mem_getc(f);
	if (i == 3) {
		if (mem_getc(f) != '(') {
			mem_ungetc(c, f);
			return NAN;
		}
		for (i=1; ; i++) {
			c = mem_getc(f);
			if (c - '0' < 10U || c - 'A' < 26U || c - 'a' < 26U || c == '_')
				continue;
			if (c == ')') return NAN;
			mem_ungetc(c, f);
			if (!pok) {
				errno = EINVAL;
				shlim(f, 0);
				return 0;
			}
			while (i--) mem_ungetc(c, f);
			return NAN;
		}
		return NAN;
	}
	if (i) {
		mem_ungetc(c, f);
		errno = EINVAL;
		shlim(f, 0);
		return 0;
	}
	if (c == '0') {
		c = mem_getc(f);
		if ((c|32) == 'x')
			return hexfloat(f, bits, emin, sign, pok);
		mem_ungetc(c, f);
		c = '0';
	}
	return decfloat(f, c, bits, emin, sign, pok);
}

static long double strtox (const char *s, char **p, int prec) {
  long double y;
  /* Replace stack-allocated FILE f with MEM_FILE f */
  MEM_FILE f;
  /* Original: sh_fromstring(&f, s); */
  mem_fromstring(&f, s);
  /* Original: shlim(&f, 0); // Omitted/not needed with manual stream
     Now, __floatscan must be modified to accept a MEM_FILE */
  y = __floatscan(&f, prec, 1);
  /* Original: off_t cnt = shcnt(&f); */
  off_t cnt = mem_count(&f);
  if (p) *p = cnt ? (char *)s + cnt : (char *)s;
  return y;
}

LUALIB_API long double str_strtold (const char *s, char **p) {
	return strtox(s, p, 2);
}

#endif  /* of __ARMCPU */

/* End of portable strtold() implementation. */


static int dlong_new (lua_State *L) {
  DLong *d = (DLong *)lua_newuserdata(L, sizeof(DLong));
  lua_setmetatabletoobject(L, -1, "longdouble", 1);
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      d->value = agn_tonumber(L, 1);
      break;
    }
    case LUA_TSTRING: {
#ifndef __ARMCPU
      d->value = str_strtold(agn_tostring(L, 1), NULL);
#else
      size_t l, flag;
      flag = 0;
      char *str = (char *)lua_tolstring(L, 1, &l);
      if (l > 2) {  /* 6.3.8 */
        if ((l > 3 && str[2] == 'x' && tools_strncmp(str, "-0x", 3) == 0) ||
            (str[1] == 'x' && tools_strncmp(str, "0x", 2) == 0)) {
          int rc;
          str = str_hextodec(str, l, &rc);
          switch (rc) {
            case 1:
              luaL_error(L, "Error: memory allocation failed.");
              break;
            case 2:
              luaL_error(L, "Error: invalid character in input string.");
              break;
            default:
              flag = 1;
          }
          l = tools_strlen(str);
        }
      }
      d->value = strtold(str, NULL);
      if (flag) { xfree(str); }
#endif
      break;
    }
    case LUA_TUSERDATA: {
      if (isdlong(L, 1)) {
        d->value = getdlongvalue(L, 1);
      } else
        luaL_error(L, "Error in " LUA_QS ": wrong userdata.", "long.double");
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": expected number, a string or long userdata.", "long.double");
  }
  return 1;
}


static int dmt_unm (lua_State *L) {
  createdlong(L, -checkandgetdlong(L, 1));
  return 1;
}


static int dmt_add (lua_State *L) {
  longdouble a, b;
  getdbinops(L, a, b);
  createdlong(L, a + b);
  return 1;
}


static int dmt_sub (lua_State *L) {
  longdouble a, b;
  getdbinops(L, a, b);
  createdlong(L, a - b);
  return 1;
}


static int dmt_mul (lua_State *L) {
  longdouble a, b;
  getdbinops(L, a, b);
  createdlong(L, a * b);
  return 1;
}


static int dmt_div (lua_State *L) {
  longdouble a, b;
  getdbinops(L, a, b);
  createdlong(L, (b == 0.0L) ? AGN_NAN : a/b);
  return 1;
}

#ifndef __ARMCPU
#define long_sign(x)  ((x) < 0.0L ? -1L : (((x) == 0.0L) ? 0.0L : 1.0L))
#define long_numintdiv(a,b) (((b) != 0) ? (long_sign((a))*long_sign((b))*floorl(fabsl((a)/(b)))) : (AGN_NAN) )
#else
#define long_sign(x)  ((x) < 0.0 ? -1 : (((x) == 0.0) ? 0.0 : 1.0))
#define long_numintdiv(a,b) (((b) != 0) ? (long_sign((a))*long_sign((b))*floor(fabs((a)/(b)))) : (AGN_NAN) )
#endif
static int dmt_intdiv (lua_State *L) {
  longdouble a, b;
  getdbinops(L, a, b);
  createdlong(L, long_numintdiv(a, b));
  return 1;
}


#ifndef __ARMCPU
#define long_mod(a,b)    ( ((b) != 0) ? (a) - floorl((a)/(b))*(b) : (AGN_NAN))
#else
#define long_mod(a,b)    ( ((b) != 0) ? (a) - floor((a)/(b))*(b) : (AGN_NAN))
#endif
static int dmt_mod (lua_State *L) {
  longdouble a, b;
  getdbinops(L, a, b);
  createdlong(L, long_mod(a, b));
  return 1;
}


static int dmt_square (lua_State *L) {
#ifndef __ARMCPU
  longdouble hx, lx;
  tools_squarel(&hx, &lx, checkandgetdlong(L, 1));  /* changed 2.35.0 */
  createdlong(L, hx + lx);
#else
  createdlong(L, tools_square(checkandgetdlong(L, 1)));
#endif
  return 1;
}


static int dmt_cube (lua_State *L) {
#ifndef __ARMCPU
  longdouble hx, lx, x;
  x = checkandgetdlong(L, 1);
  tools_squarel(&hx, &lx, x);  /* changed 2.35.0 */
  createdlong(L, x*(hx + lx));
#else
  longdouble x;
  x = checkandgetdlong(L, 1);
  createdlong(L, x*x*x);
#endif
  return 1;
}


static int dmt_recip (lua_State *L) {
  longdouble x = checkandgetdlong(L, 1);
  createdlong(L, (x == 0.0L) ? AGN_NAN : 1.0L/x);
  return 1;
}


static int dmt_pow (lua_State *L) {
  longdouble a, b;
  getdbinops(L, a, b);
  createdlong(L, (a == 0.0L && b == 0.0L) ? AGN_NAN : tools_powl(a, b));  /* 2.34.6 fix, 2.41.1 tweak */
  return 1;
}


static int dmt_ipow (lua_State *L) {  /* 2.34.6 */
  longdouble a, b;
  getdbinops(L, a, b);
  createdlong(L, tools_powil(a, b));  /* 2.34.10 change */
  return 1;
}


/* Computes z^2 + c, preventing round-off errors. */
static int dmt_squareadd (lua_State *L) {  /* 2.34.6 */
  longdouble a, b;
  getdbinops(L, a, b);
  createdlong(L, fmal(a, a, b));
  return 1;
}


static int dmt_log (lua_State *L) {  /* 2.34.7 */
  longdouble x, b;
  getdbinops(L, x, b);
  createdlong(L, tools_logbasel(x, b));  /* 2.34.8 change */
  return 1;
}


static int dmt_sqrt (lua_State *L) {
  createdlong(L, sqrtl(checkandgetdlong(L, 1)));
  return 1;
}


/* root (x [, n])
   Returns the non-principal n-th root of the number or complex value x. n should be an integer and is 2 by default. */
#ifndef __ARMCPU
#define long_iseven(x)  (fmodl(x, 2) == 0.0L)
#define FN_POWL powl
#else
#define long_iseven(x)  (fmod(x, 2) == 0.0)
#define FN_POWL pow
#endif
static int dlong_root (lua_State *L) {  /* equivalent to Maple's `surd`, 2.34.6 */
  longdouble x, n;
  luaL_checkany(L, 1);
  x = checkandgetdlongnum(L, 1);
  n = dlongL_optinteger(L, 2, 2.0L);  /* 2.34.8 change */
  if (n == 0.0L || (x == 0.0L && n < 0.0L)) {
    createdlong(L, AGN_NAN);
  } else if (x >= 0) {
    createdlong(L, FN_POWL(x, 1.0L/n));
  } else if (long_iseven(n)) { /* x < 0 and n is even ? */
    createdlong(L, AGN_NAN);
  } else {
    createdlong(L, -FN_POWL(-x, 1.0L/n));
  }
  return 1;
}


static int dmt_invsqrt (lua_State *L) {  /* 2.34.6 */
  longdouble x = checkandgetdlong(L, 1);
  createdlong(L, (x == 0.0L) ? AGN_NAN : sqrtl(x)/x);
  return 1;
}


static int dmt_exp (lua_State *L) {
  createdlong(L, tools_expl(checkandgetdlong(L, 1)));  /* 2.41.1 change */
  return 1;
}


static int dmt_ln (lua_State *L) {
  longdouble x = checkandgetdlong(L, 1);
  createdlong(L, tools_logl(x));  /* 2.34.6 fix, 2.34.8 change */
  return 1;
}


static int dmt_lngamma (lua_State *L) {
  longdouble x = checkandgetdlong(L, 1);
  createdlong(L, tools_lgammal(x));  /* 2.41.1 */
  return 1;
}


static int dmt_sin (lua_State *L) {
  createdlong(L, sun_sinl(checkandgetdlong(L, 1)));  /* 2.34.9 change */
  return 1;
}


static int dmt_cos (lua_State *L) {
  createdlong(L, sun_cosl(checkandgetdlong(L, 1)));  /* 2.34.9 change */
  return 1;
}


static int dmt_tan (lua_State *L) {
  createdlong(L, sun_tanl(checkandgetdlong(L, 1)));  /* 2.34.9 change */
  return 1;
}


static int dmt_arcsin (lua_State *L) {
  createdlong(L, sun_asinl(checkandgetdlong(L, 1)));  /* 2.34.9 8% tweak */
  return 1;
}


static int dmt_arccos (lua_State *L) {
  createdlong(L, sun_acosl(checkandgetdlong(L, 1)));  /* 2.34.9 8% tweak */
  return 1;
}


static int dmt_arctan (lua_State *L) {
  createdlong(L, sun_atanl(checkandgetdlong(L, 1)));  /* 2.34.9 8% tweak */
  return 1;
}


static int dmt_arcsec (lua_State *L) {  /* 2.34.6 */
  longdouble x = checkandgetdlong(L, 1);
  createdlong(L, (x == 0.0L) ? AGN_NAN : sun_acosl(1.0L/x));  /* 2.34.9 tweak */
  return 1;
}


static int dmt_sinh (lua_State *L) {
  createdlong(L, tools_sinhl(checkandgetdlong(L, 1)));  /* 6.7.3 fix for OS/2 */
  return 1;
}


static int dmt_cosh (lua_State *L) {
  createdlong(L, tools_coshl(checkandgetdlong(L, 1)));  /* 6.7.3 fix for OS/2 */
  return 1;
}


static int dmt_tanh (lua_State *L) {
  createdlong(L, tools_tanhl(checkandgetdlong(L, 1)));  /* 6.7.3 fix for OS/2 */
  return 1;
}


#define luai_numsincl(a)     (FN_FABS(a) < 1e-4L ? 1.0L - (0.166666666666666666666667L)*a*a : sun_sinl((a))/(a))
static int dmt_sinc (lua_State *L) {  /* 2.34.7 */
  longdouble x = checkandgetdlong(L, 1);
  createdlong(L, luai_numsincl(x));
  return 1;
}


static int dmt_sign (lua_State *L) {
  longdouble x = checkandgetdlong(L, 1);
  createdlong(L, (tools_fpisnanl(x)) ? AGN_NAN : long_sign(x));  /* x == AGN_NAN will not work ! */
  return 1;
}


static int dmt_signum (lua_State *L) {  /* 2.34.7 */
  createdlong(L, tools_signuml(checkandgetdlong(L, 1)));  /* 2.34.8 fix for nan/inf */
  return 1;
}


static int dmt_antilog2 (lua_State *L) {  /* 2.34.7 */
  createdlong(L, tools_exp2l(checkandgetdlong(L, 1)));
  return 1;
}


static int dmt_antilog10 (lua_State *L) {  /* 2.34.7 */
  createdlong(L, tools_exp10l(checkandgetdlong(L, 1)));
  return 1;
}


static int dmt_abs (lua_State *L) {
  createdlong(L, FN_FABS(checkandgetdlong(L, 1)));
  return 1;
}


static int dmt_absdiff (lua_State *L) {  /* 7.5.8 */
  longdouble a, b;
  getdbinops(L, a, b);
  createdlong(L, FN_FABS(a - b));
  return 1;
}


static int dmt_zero (lua_State *L) {  /* 2.34.6 */
  lua_pushboolean(L, checkandgetdlong(L, 1) == 0.0L);
  return 1;
}


static int dmt_nonzero (lua_State *L) {  /* 2.34.6 */
  lua_pushboolean(L, checkandgetdlong(L, 1) != 0.0L);
  return 1;
}


static int dmt_finite (lua_State *L) {  /* 2.34.6 */
  lua_pushboolean(L, tools_fpisfinitel(checkandgetdlong(L, 1)));
  return 1;
}


static int dmt_nan (lua_State *L) {  /* 2.34.6 */
  lua_pushboolean(L, tools_fpisnanl(checkandgetdlong(L, 1)));
  return 1;
}


/* Checks whether a longdouble represents an even integral value. Returns `true` or `false`. 2.34.6 */
static int dmt_even (lua_State *L) {
  longdouble i, f;
  f = sun_modfl(checkandgetdlongnum(L, 1), &i);  /* 2.34.8 tweak */
  lua_pushboolean(L, f == 0.0L && long_iseven(i));
  return 1;
}


/* Checks whether a longdouble represents an odd integral value. Returns `true` or `false`. 2.34.6 */
static int dmt_odd (lua_State *L) {
  longdouble i, f;
  f = sun_modfl(checkandgetdlongnum(L, 1), &i);  /* 2.34.8 tweak */
  lua_pushboolean(L, f == 0.0L && !long_iseven(i));
  return 1;
}


static int dmt_int (lua_State *L) {  /* 2.34.6, 2.34.8 tweak */
  createdlong(L, sun_intl(checkandgetdlong(L, 1)));
  return 1;
}


static int dmt_frac (lua_State *L) {  /* 2.34.6, 2.34.8 tweak */
  createdlong(L, sun_fracl(checkandgetdlong(L, 1)));  /* preserve the sign, 2.34.7 fix */
  return 1;
}


static int dmt_integral (lua_State *L) {  /* 2.34.7, tuned 2.34.8 */
  lua_pushboolean(L, sun_isintl(checkandgetdlong(L, 1)));
  return 1;
}


static int dmt_fractional (lua_State *L) {  /* 2.34.7, tuned 2.34.8 */
  lua_pushboolean(L, sun_isfloatl(checkandgetdlong(L, 1)));
  return 1;
}


static int dmt_entier (lua_State *L) {  /* 2.34.6 */
  createdlong(L, sun_floorl(checkandgetdlong(L, 1)));  /* 3.7.1 change */
  return 1;
}


static int dlong_fma (lua_State *L) {
  longdouble a, b, c;
  getdbinops(L, a, b);
  getdop(L, 3, c);
#ifndef __ARMCPU
  createdlong(L, fmal(a, b, c));
#else
  createdlong(L, fma(a, b, c));
#endif
  return 1;
}


static int dlong_hypot (lua_State *L) {
  createdlong(L, tools_hypotl(checkandgetdlongnum(L, 1), checkandgetdlongnum(L, 2)));
  return 1;
}


/* sqrt(1 + x^2), 2.35.0 */
static int dlong_hypot2 (lua_State *L) {
  createdlong(L, tools_hypotl(1.0L, checkandgetdlongnum(L, 1)));
  return 1;
}


/* sqrt(1 - x^2), 2.35.0 */
static int dlong_hypot3 (lua_State *L) {
  createdlong(L, tools_mhypotl(1.0L, checkandgetdlongnum(L, 1)));
  return 1;
}


/* sqrt(x^2 - y^2), 2.35.0 */
static int dlong_cathet (lua_State *L) {
  createdlong(L, tools_mhypotl(checkandgetdlongnum(L, 1), checkandgetdlongnum(L, 2)));
  return 1;
}


static int dlong_pytha (lua_State *L) {  /* 2.34.9 */
  createdlong(L, tools_pythal(checkandgetdlongnum(L, 1), checkandgetdlongnum(L, 2)));
  return 1;
}


static int dlong_pytha4 (lua_State *L) {  /* 2.34.9 */
  createdlong(L, tools_mpythal(checkandgetdlongnum(L, 1), checkandgetdlongnum(L, 2)));
  return 1;
}


static int dlong_sincos (lua_State *L) {  /* 2.34.9 */
  longdouble s, c;
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.6.1 fix */
  sun_sincosl(checkandgetdlongnum(L, 1), &s, &c);
  createdlong(L, s);
  createdlong(L, c);
  return 2;
}


static int dlong_sinhcosh (lua_State *L) {  /* 6.6.6 */
  longdouble s, c;
  luaL_checkstack(L, 2, "not enough stack space");
  cephes_sinhcoshl(checkandgetdlongnum(L, 1), &s, &c);
  createdlong(L, s);
  createdlong(L, c);
  return 2;
}


/* Cotangent, i.e. -tan(Pi/2 + x); 2.34.6 */
static int dlong_cot (lua_State *L) {
#ifndef __ARMCPU
  createdlong(L, -sun_tanl(M_PIO2ld + checkandgetdlongnum(L, 1)));  /* 2.34.9 change */
#else
  createdlong(L, -sun_tan(M_PIO2 + checkandgetdlongnum(L, 1)));  /* 2.34.9 change */
#endif
  return 1;
}


/* Secant, returns the inverse cosine of number x. 2.34.6 */
static int dlong_sec (lua_State *L) {
  longdouble x, r;
  x = checkandgetdlongnum(L, 1);
  r = sun_cosl(x);  /* 2.34.9 change */
  createdlong(L, (r == 0.0L) ? AGN_NAN : 1.0L/r);
  return 1;
}


/* Cosecant, returns the inverse sine of x. 2.34.6 */
static int dlong_csc (lua_State *L) {
  longdouble t = sun_sinl(checkandgetdlongnum(L, 1));  /* 2.34.9 change */
  createdlong(L, (t == 0.0L) ? AGN_NAN : 1.0L/t);
  return 1;
}


static int dlong_csch (lua_State *L) {  /* 6.6.6 */
#ifndef __ARMCPU
  createdlong(L, 1.0L/tools_sinhl(checkandgetdlong(L, 1)));  /* 6.7.3 fix for OS/2 */
#else
  createdlong(L, 1.0/sun_sinh(checkandgetdlong(L, 1)));  /* 6.7.3 45 % boost */
#endif
  return 1;
}


static int dlong_sech (lua_State *L) {  /* 6.6.6 */
#ifndef __ARMCPU  /* tools_coshl is slightly slower */
  createdlong(L, 1.0L/tools_coshl(checkandgetdlong(L, 1)));  /* 6.7.3 fix for OS/2 */
#else
  createdlong(L, 1.0/sun_cosh(checkandgetdlong(L, 1)));  /* 6.7.3 45 % boost */
#endif
  return 1;
}


static int dlong_coth (lua_State *L) {  /* 6.6.6 */
#ifndef __ARMCPU
  createdlong(L, 1.0L/tools_tanhl(checkandgetdlong(L, 1)));
#else
  createdlong(L, 1.0/sun_tanh(checkandgetdlong(L, 1)));  /* 6.7.3 55 % boost */
#endif
  return 1;
}


/* = cos(x) + sin(x) = sqrt(2)*sin(x + Pi/4); 2.34.6 */
static int dlong_cas (lua_State *L) {
#ifndef __ARMCPU
  createdlong(L, M_SQRT2ld*sun_sinl(checkandgetdlongnum(L, 1) + M_PIO4ld));  /* 2.34.9 change */
#else
  createdlong(L, M_SQRT2*sun_sin(checkandgetdlongnum(L, 1) + M_PIO4));  /* 2.34.9 change */
#endif
  return 1;
}


static int dlong_arcsinh (lua_State *L) {
#ifndef __ARMCPU
  createdlong(L, asinhl(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, asinh(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_arccosh (lua_State *L) {
#ifndef __ARMCPU  /* the SunMath version is 2% slower */
  createdlong(L, acoshl(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, acosh(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_arctanh (lua_State *L) {
#ifndef __ARMCPU  /* the SunMath version is 21% (twenty-one) slower */
  createdlong(L, atanhl(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, atanh(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_arccsc (lua_State *L) {  /* 2.34.6 */
  longdouble x = checkandgetdlongnum(L, 1);
  createdlong(L, (x == 0.0L) ? AGN_NAN : sun_asinl(1.0L/x));  /* 2.34.9 tweak */
  return 1;
}


static int dlong_arccot (lua_State *L) {  /* 2.34.6 */
  createdlong(L, M_PIO2ld - sun_atanl(checkandgetdlongnum(L, 1)));  /* 2.34.9 tweak */
  return 1;
}


static int dlong_arccsch (lua_State *L) {  /* 6.6.6 */
  longdouble x = checkandgetdlongnum(L, 1);
  createdlong(L, (x == 0.0L) ? AGN_NAN : sun_asinhl(1.0L/x));
  return 1;
}


static int dlong_arccoth (lua_State *L) {  /* 6.6.6 */
  longdouble x = checkandgetdlongnum(L, 1);
  createdlong(L, (x == 0.0L) ? AGN_NAN : sun_atanhl(1.0L/x));
  return 1;
}


static int dlong_arcsech (lua_State *L) {  /* 6.6.6 */
  longdouble x = checkandgetdlongnum(L, 1);
  createdlong(L, (x == 0.0L) ? AGN_NAN : sun_acoshl(1.0L/x));
  return 1;
}


static int dlong_log2 (lua_State *L) {
  longdouble x = checkandgetdlongnum(L, 1);
#ifndef __ARMCPU
  createdlong(L, (x <= 0.0L) ? AGN_NAN : log2l(x));  /* 2.34.7 fix */
#else
  createdlong(L, (x <= 0.0L) ? AGN_NAN : log2(x));
#endif
  return 1;
}


static int dlong_ilog2 (lua_State *L) {  /* 2.34.7 */
  createdlong(L, tools_ilogbl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_mantissa (lua_State *L) {  /* 2.34.9 */
  int i;
#ifndef __ARMCPU
  createdlong(L, frexpl(checkandgetdlongnum(L, 1), &i));
#else
  createdlong(L, frexp(checkandgetdlongnum(L, 1), &i));
#endif
  return 1;
}


static int dlong_exponent (lua_State *L) {  /* 2.34.9 */
  int i;
#ifndef __ARMCPU
  frexpl(checkandgetdlongnum(L, 1), &i);
#else
  frexp(checkandgetdlongnum(L, 1), &i);
#endif
  createdlong(L, i);
  return 1;
}


static int dlong_significand (lua_State *L) {  /* 2.34.9 */
  longdouble x = checkandgetdlongnum(L, 1);
#ifndef __ARMCPU
  createdlong(L, scalbnl(x, -tools_ilogbl(x)));
#else
  createdlong(L, scalbn(x, -tools_ilogbl(x)));
#endif
  return 1;
}


static int dlong_log10 (lua_State *L) {
  longdouble x = checkandgetdlongnum(L, 1);
#ifndef __ARMCPU
  createdlong(L, (x == 0.0L) ? AGN_NAN : log10l(x));  /* 2.34.7 fix */
#else
  createdlong(L, (x == 0.0L) ? AGN_NAN : log10(x));
#endif
  return 1;
}


static int dlong_expm1 (lua_State *L) {
#ifndef __ARMCPU
  createdlong(L, expm1l(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, expm1(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_lnp1 (lua_State *L) {
  longdouble x = checkandgetdlongnum(L, 1);
#ifndef __ARMCPU
  createdlong(L, (x <= -1.0L) ? AGN_NAN : log1pl(x));  /* 2.34.7 fix */
#else
  createdlong(L, (x <= -1.0) ? AGN_NAN : log1p(x));
#endif
  return 1;
}


static int dlong_erf (lua_State *L) {
  createdlong(L, sun_erfl(checkandgetdlongnum(L, 1)));  /* 2.34.7 OS/2 fix; 2.34.8 tweak by 2 % */
  return 1;
}


static int dlong_erfc (lua_State *L) {
  createdlong(L, sun_erfcl(checkandgetdlongnum(L, 1)));  /* ditto */
  return 1;
}


static int dlong_erfcx (lua_State *L) {
  createdlong(L, tools_erfcxl(checkandgetdlongnum(L, 1)));  /* ditto */
  return 1;
}


static int dlong_inverf (lua_State *L) {  /* 3.3.5 */
  createdlong(L, tools_inverfl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_inverfc (lua_State *L) {  /* 6.6.13 */
  createdlong(L, tools_inverfcl(checkandgetdlongnum(L, 1)));
  return 1;
}


/* Bessel function of first kind of integer order, 6.5.0 */
static int dlong_besselj (lua_State *L) {
  int n;
  longdouble x;
  n = agn_checknonnegint(L, 1);
  x = checkandgetdlongnum(L, 2);
  createdlong(L, cephes_jnl(n, x));
  return 1;
}


/* Bessel function of second kind of integer order, 6.5.0 */
static int dlong_bessely (lua_State *L) {
  int n;
  longdouble x;
  n = agn_checknonnegint(L, 1);
  x = checkandgetdlongnum(L, 2);
  createdlong(L, cephes_ynl(n, x));
  return 1;
}


/* Binomial (probability) distribution (with no fourth argument) and
   Complemented binomial (probability) distribution (with any fourth argument), 4.6.11 */
static int dlong_binomd (lua_State *L) {
  int k, n, nargs;
  longdouble x;
  nargs = lua_gettop(L);
  k = agn_checkinteger(L, 1);
  n = agn_checkinteger(L, 2);
  x = checkandgetdlongnum(L, 3);
  createdlong(L, (nargs == 3) ? bdtrl(k, n, x) : bdtrcl(k, n, x));
  return 1;
}


/* Inverse binomial (probability) distribution, 4.6.11 */
static int dlong_invbinomd (lua_State *L) {
  int k, n;
  longdouble x;
  k = agn_checkinteger(L, 1);
  n = agn_checkinteger(L, 2);
  x = checkandgetdlongnum(L, 3);
  createdlong(L, bdtril(k, n, x));
  return 1;
}


/* Normal distribution function */
static int dlong_cdfnormald (lua_State *L) {
  createdlong(L, ndtrl(checkandgetdlongnum(L, 1)));
  return 1;
}


/* Inverse of Normal distribution function */
static int dlong_invnormald (lua_State *L) {
  createdlong(L, ndtril(checkandgetdlongnum(L, 1)));
  return 1;
}


/* Returns the nearest integer not greater than the given value. 2.34.5 */
static int dlong_floor (lua_State *L) {
#ifndef __ARMCPU
  createdlong(L, floorl(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, sun_floor(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


/* Returns the nearest integer not less than the given value. 2.34.5 */
static int dlong_ceil (lua_State *L) {
#ifndef __ARMCPU
  createdlong(L, ceill(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, sun_ceil(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


/* Returns the nearest integer not greater in magnitude than the given value. 2.34.5 */
static int dlong_trunc (lua_State *L) {
#ifndef __ARMCPU
  createdlong(L, truncl(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, sun_trunc(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_sqrt (lua_State *L) {  /* 2.35.0 */
#ifndef __ARMCPU
  createdlong(L, sqrtl(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, sqrt(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_cbrt (lua_State *L) {  /* 2.35.0 */
#ifndef __ARMCPU
  createdlong(L, cbrtl(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, cbrt(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_square (lua_State *L) {  /* 2.35.0 */
#ifndef __ARMCPU
  longdouble hx, lx;
  tools_squarel(&hx, &lx, checkandgetdlong(L, 1));
  createdlong(L, hx + lx);
#else
  longdouble x;
  x = checkandgetdlong(L, 1);
  createdlong(L, x*x);
#endif
  return 1;
}


static int dlong_cube (lua_State *L) {  /* 2.35.0, 3.7.1 change */
#ifndef __ARMCPU
  longdouble hx, lx, x;
  x = checkandgetdlong(L, 1);
  tools_squarel(&hx, &lx, x);  /* changed 2.35.0 */
  createdlong(L, x*(hx + lx));
#else
  longdouble x;
  x = checkandgetdlong(L, 1);
  createdlong(L, x*x*x);
#endif
  return 1;
}


static int dlong_recip (lua_State *L) {  /* 2.35.0 */
  longdouble x = checkandgetdlongnum(L, 1);
  createdlong(L, (x == 0.0L) ? AGN_NAN : 1.0L/x);
  return 1;
}


static int dlong_invsqrt (lua_State *L) {  /* 2.35.0 */
  longdouble x = checkandgetdlongnum(L, 1);
#ifndef __ARMCPU
  createdlong(L, (x == 0.0L) ? AGN_NAN : sqrtl(x)/x);
#else
  createdlong(L, (x == 0.0) ? AGN_NAN : sqrt(x)/x);
#endif
  return 1;
}


static int dlong_exp (lua_State *L) {  /* 2.35.0 */
  createdlong(L, tools_expl(checkandgetdlongnum(L, 1)));  /* 2.41.1 change */
  return 1;
}


static int dlong_expx2 (lua_State *L) {
  longdouble x = checkandgetdlongnum(L, 1);
  int sign = agnL_optinteger(L, 2, 1);
  createdlong(L, expx2l(x, sign));
  return 1;
}


static int dlong_ln (lua_State *L) {  /* 2.34.8 */
  createdlong(L, tools_logl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_lnabs (lua_State *L) {  /* 2.41.2 */
  longdouble x;
  x = checkandgetdlongnum(L, 1);
#ifndef __ARMCPU
  createdlong(L, (x == 0.0L) ? AGN_NAN : tools_logl(FN_FABS(x)));  /* 3.7.1 fix */
#else
  createdlong(L, (x == 0.0) ? AGN_NAN : sun_log(FN_FABS(x)));
#endif
  return 1;
}


static int dlong_gamma (lua_State *L) {  /* 2.41.1 */
  createdlong(L, tools_gammal(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_lngamma (lua_State *L) {  /* 2.41.1 */
  createdlong(L, tools_lgammal(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_beta (lua_State *L) {  /* 6.6.12 */
  long double x, y, lb;
  x = checkandgetdlongnum(L, 1);
  y = checkandgetdlongnum(L, 2);
  lb = tools_lgammal(x) + tools_lgammal(y) - tools_lgammal(x + y);
  createdlong(L, tools_expl(lb));
  return 1;
}


static int dlong_psi (lua_State *L) {  /* 6.6.11 */
  int nargs = lua_gettop(L);
  if (nargs == 1) {
    createdlong(L, tools_psil(checkandgetdlongnum(L, 1)));
  } else {  /* 6.8.2 */
    int n = agn_checknonnegint(L, 1);
    long double x = checkandgetdlongnum(L, 2);
    switch (n) {
      case 0:
        createdlong(L, tools_psil(x));
        break;
      case 1:
        createdlong(L, tools_trigammal(x));
        break;
      case 2:
        createdlong(L, tools_tetragammal(x));
        break;
      case 3:
        createdlong(L, tools_pentagammal(x));
        break;
      default:
        luaL_error(L, "Error in " LUA_QS ": first argument must be 1, 2, or 3.", "long.psi");
    }
  }
  return 1;
}


static int dlong_fact (lua_State *L) {  /* 2.41.1 */
  createdlong(L, cephes_factoriall(checkandgetdlongnum(L, 1)));  /* changed 3.16.2 */
  return 1;
}


static int dlong_lnfact (lua_State *L) {  /* 2.41.2 */
  createdlong(L, tools_lnfactoriall(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_lnbinomial (lua_State *L) {  /* 3.7.3 */
  createdlong(L, tools_lnbinomiall(checkandgetdlongnum(L, 1), checkandgetdlongnum(L, 2)));
  return 1;
}


static int dlong_sin (lua_State *L) {  /* 2.35.0 */
  createdlong(L, sun_sinl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_cos (lua_State *L) {  /* 2.35.0 */
  createdlong(L, sun_cosl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_tan (lua_State *L) {  /* 2.35.0 */
  createdlong(L, sun_tanl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_arcsin (lua_State *L) {  /* 2.35.0 */
  createdlong(L, sun_asinl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_arccos (lua_State *L) {  /* 2.35.0 */
  createdlong(L, sun_acosl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_arctan (lua_State *L) {  /* 2.35.0 */
  createdlong(L, sun_atanl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_arcsec (lua_State *L) {  /* 2.35.0 */
  longdouble x = checkandgetdlongnum(L, 1);
  createdlong(L, (x == 0.0L) ? AGN_NAN : sun_acosl(1.0L/x));
  return 1;
}


static int dlong_sinh (lua_State *L) {  /* 2.35.0 */
#ifndef __ARMCPU
  createdlong(L, tools_sinhl(checkandgetdlongnum(L, 1)));  /* 6.7.3 fix for OS/2 */
#else
  createdlong(L, sinh(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_cosh (lua_State *L) {  /* 2.35.0 */
#ifndef __ARMCPU
  createdlong(L, tools_coshl(checkandgetdlongnum(L, 1)));  /* 6.7.3 fix for OS/2 */
#else
  createdlong(L, cosh(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_tanh (lua_State *L) {  /* 2.35.0 */
#ifndef __ARMCPU
  createdlong(L, tools_tanhl(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, tanh(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_sinc (lua_State *L) {  /* 2.35.0 */
  longdouble x = checkandgetdlongnum(L, 1);
#ifndef __ARMCPU
  createdlong(L, luai_numsincl(x));
#else
  createdlong(L, luai_numsinc(x));
#endif
  return 1;
}


static int dlong_unm (lua_State *L) {  /* 2.35.1 */
  createdlong(L, -checkandgetdlongnum(L, 1));
  return 1;
}


static int dlong_signum (lua_State *L) {  /* 2.35.0 */
  createdlong(L, tools_signuml(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_antilog2 (lua_State *L) {  /* 2.35.0 */
  createdlong(L, tools_exp2l(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_antilog10 (lua_State *L) {  /* 2.35.0 */
  createdlong(L, tools_exp10l(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_abs (lua_State *L) {  /* 2.35.0 */
  createdlong(L, FN_FABS(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_sign (lua_State *L) {  /* 2.35.0 */
  longdouble x = checkandgetdlongnum(L, 1);
  createdlong(L, (tools_fpisnanl(x)) ? AGN_NAN : long_sign(x));  /* x == AGN_NAN will not work ! */
  return 1;
}


static int dlong_int (lua_State *L) {  /* 2.35.0 */
  createdlong(L, sun_intl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_frac (lua_State *L) {  /* 2.35.0 */
  createdlong(L, sun_fracl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_entier (lua_State *L) {  /* 2.35.0 */
#ifndef __ARMCPU
  createdlong(L, floorl(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, sun_floor(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


/* Decomposes a number into its integer and fractional parts.
   Returns two numbers, the integral part of the number x and its fractional part. The
   integral part is rounded towards zero. Both the integral and fractional part of the
   return have the same sign as x. The sum of the two values returned equals x. 2.34.5 */
static int dlong_modf (lua_State *L) {
  longdouble i, f;
  f = sun_modfl(checkandgetdlongnum(L, 1), &i);  /* 2.34.8 tweak */
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.6.1 fix */
  createdlong(L, i);
  createdlong(L, f);
  return 2;
}


static int dlong_ldexp (lua_State *L) {  /* 2.34.7 */
  longdouble a, b;
  getdbinops(L, a, b);
#ifndef __ARMCPU
  createdlong(L, ldexpl(a, b));
#else
  createdlong(L, ldexp(a, b));
#endif
  return 1;
}


static int dlong_frexp (lua_State *L) {  /* 2.34.7 */
  int e;
  longdouble x = checkandgetdlongnum(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.6.1 fix */
#ifndef __ARMCPU
  createdlong(L, frexpl(x, &e));
#else
  createdlong(L, frexp(x, &e));
#endif
  createdlong(L, e);
  return 2;
}


/* Computes the remainder from the division of numerator by denominator. The return value is numerator - n * denominator,
   where n is the quotient of numerator divided by denominator, rounded towards zero to an integer. 2.34.5 */
static int dlong_fmod (lua_State *L) {
  longdouble a, b;
  getdbinops(L, a, b);
#ifndef __ARMCPU
  createdlong(L, fmodl(a, b));
#else
  createdlong(L, fmod(a, b));
#endif
  return 1;
}


/* Returns the positive difference, i.e. x - y if x > y; and 0 otherwise. 2.34.5 */
static int dlong_fdim (lua_State *L) {
  longdouble a, b;
  getdbinops(L, a, b);
#ifndef __ARMCPU
  createdlong(L, fdiml(a, b));
#else
  createdlong(L, fdim(a, b));
#endif
  return 1;
}


/* Returns the next representable floating-point value after x in the direction of y. 2.34.5 */
static int dlong_nextafter (lua_State *L) {
  longdouble a, b;
  if (lua_gettop(L) == 2) {
    getdbinops(L, a, b);
  } else {
    a = checkandgetdlongnum(L, 1);
    b = HUGE_VAL;  /* HUGE_VAL & HUGE_VALL are the same and there is no difference */
  }
  createdlong(L, sun_nextafterl(a, b));
  return 1;
}


/* Returns true if the argument is negative, and false otherwise. 2.34.5 */
static int dlong_signbit (lua_State *L) {
  lua_pushboolean(L, signbit(checkandgetdlongnum(L, 1)));
  return 1;
}


/* Converts the longdouble x in the scale [a1, a2] to one in the scale [b1, b2]. The second
   and third arguments must be pairs of longdoubles. If the third argument is missing, then
   x is converted to a longdouble in [0, 1]. The return is a longdouble. See also: long.wrap.
   2.34.6 */
static int dlong_norm (lua_State *L) {
  longdouble x, a1, a2, b1, b2;
  int t3;
  x = checkandgetdlongnum(L, 1);
  agnL_pairgetilongnumbers(L, "long.(x)norm", 2, &a1, &a2);
  if (x < a1 || x > a2)
    luaL_error(L, "Error in " LUA_QS ": first argument out of range.", "long.(x)norm");
  if (a1 > a2)  /* 2.2.1 */
    luaL_error(L, "Error in " LUA_QS ": left border greater than right border in second argument.", "long.(x)norm");
  t3 = lua_type(L, 3);
  if (t3 == LUA_TNONE) {
    b1 = 0.0L; b2 = 1.0L;
  }
  else {  /* 2.10.1 */
    agnL_pairgetilongnumbers(L, "long.norm", 3, &b1, &b2);
    if (b1 > b2)  /* 2.2.1 */
      luaL_error(L, "Error in " LUA_QS ": left border greater than right border in third argument.", "long.(x)norm");
  }
  createdlong(L, b1 + (x - a1)*(b2 - b1)/(a2 - a1));
  return 1;
}


/* long.wrap(x, a, b)
   long.wrap(x, [a])

Clamping function: Conducts a range reduction of the longdouble x to the interval [a, b) and returns a longdouble.
In the second form, if a is not given, a is set to -Pi and b to +Pi. If a is given, a is set to -a and b to +a,
so a should be positive.

See also: long.norm. 2.34.6 */

#define P1 (4 * 7.8539812564849853515625e-01L)
#define P2 (4 * 3.7748947079307981766760e-08L)
#define P3 (4 * 2.6951514290790594840552e-15L)

#ifndef __ARMCPU
#define M_PIUNI   M_PIld
#define M_PI2UNI  M_PI2ld
#else
#define M_PIUNI   M_PIld
#define M_PI2UNI  M_PI2ld
#endif

static int dlong_wrap (lua_State *L) {
  longdouble theta, min, max;
  size_t nargs;
  theta = checkandgetdlongnum(L, 1);
  nargs = lua_gettop(L);
  if (nargs == 1) {
    longdouble y, r;
    int isneg, sign;
    theta = checkandgetdlongnum(L, 1);
    isneg = signbit(theta);
    sign = isneg ? -1 : 1;
    if (isneg) theta = -theta;
    y = 2*luai_numint(theta/M_PI2UNI);
    r = ((theta - y*P1) - y*P2) - y*P3;
    if (r > M_PIld) r -= M_PI2UNI;
    createdlong(L, sign*r);
  } else {
    min = dlongL_optnumber(L, 2, -M_PIUNI);
    max = dlongL_optnumber(L, 3, M_PIUNI);
    if (nargs == 2) {
      min = -min; max = -min;
    }
    if (min >= max)
      luaL_error(L, "Error in " LUA_QS ": minimum >= maximum.", "long.(x)wrap");
    createdlong(L, tools_reducerangel(theta, min, max));
  }
  return 1;
}


/* Conducts an argument reduction of x into the range |y| < Pi/2 and returns y = x - N*Pi/2. If any option is given, then the function also
   returns N, or actually the last three digits of N. The number of operations conducted are independent of the exponent of the input.

   The function is 60 percent faster than `math.wrap`, but returns a result different from x if its argument |x| is already in the range Pi/4 .. Pi/2. 2.34.7 */
static int dlong_rempio2 (lua_State *L) {
#ifndef __ARMCPU
  longdouble x, y[2];
#else
  longdouble x;
  double y[2];
#endif
  int32_t n, isopt;
  isopt = lua_gettop(L) != 1;
  x = checkandgetdlongnum(L, 1);
  n = sun_rem_pio2l(x, y);
  createdlong(L, y[0] + y[1]);
  if (isopt) createdlong(L, n);
  return 1 + isopt;
}


static int dlong_redupi (lua_State *L) {  /* 2.41.2 */
#ifndef __ARMCPU
  createdlong(L, tools_redupil(checkandgetdlongnum(L, 1)));
#else
  createdlong(L, tools_redupi(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


/* Checks whether its numeric argument x is subnormal and in this case returns the smallest normal long double number on the system, with the
   sign of x; returns x otherwise. 2.42.2 */
static int dlong_normalise (lua_State *L) {
  longdouble x = checkandgetdlongnum(L, 1);
#ifndef __ARMCPU
  if (tools_fpissubnormall(x)) {
#else
  if (sun_issubnormal(x)) {
#endif
    x = sun_copysignl(LDBL_MIN, x);
  }
  createdlong(L, x);
  return 1;
}


/* The function adds longdoubles x and y using Kahan-Ozawa round-off error prevention and returns two longdoubles: the sum of
   x and y plus the updated value of the correction variable. The correction variable q should be 0 at first invocation.
   A typical usage should look like:

   x, q -> 0;
   y := 0.1;
   while x < 1 do
      x, q := long.koadd(x, y, q)
   od;
   print(s, q);
*/
static int dlong_koadd (lua_State *L) {  /* 2.34.6, Kahan-Ozawa summation, gets argument x and correction value q, return new x and q */
  volatile longdouble q, s, sold, u, v, w, x, t;
  s = checkandgetdlongnum(L, 1);     /* cumulative sum */
  x = checkandgetdlongnum(L, 2);     /* value to be added to sum */
  q = dlongL_optnumber(L, 3, 0.0L);  /* correction value */
  v = x - q;
  sold = s;
  s += v;
  if (FN_FABS(x) < FN_FABS(q)) {
    t = x;
    x = -q;
    q = t;
  }
  u = (v - x) + q;
  if (FN_FABS(sold) < FN_FABS(v)) {
    t = sold;
    sold = v;
    v = t;
  }
  w = (s - sold) - v;
  q = u + w;
  /* s now contains sumdata */
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.6.1 fix */
  createdlong(L, s);
  createdlong(L, q);
  return 2;
}


/* The function returns the machine epsilon, the relative spacing between the longdouble |x| and its next larger longdouble
   in the machine’s floating point system. If no argument is given, x is set to 1. 2.34.6 */
static int dlong_eps (lua_State *L) {
  int nargs = lua_gettop(L);
  longdouble x = FN_FABS(dlongL_optnumber(L, 1, 1.0L));
  /* HUGE_VAL & HUGE_VALL are the same, there is no difference */
#ifndef __ARMCPU
  createdlong(L, (nargs < 2) ? nextafterl(x, HUGE_VAL) - x : tools_mathepsl(x));  /* 6.5.5 fix */
#else
  createdlong(L, (nargs < 2) ? sun_nextafter(x, HUGE_VAL) - x : tools_matheps(x));  /* 6.5.5 fix */
#endif
  return 1;
}


/* Compares the two numbers or complex values x and y and checks whether they are approximately equal. If eps is omitted,
   Eps is used. The algorithm uses a combination of simple distance measurement (|x-y| eps) [suited for values `near` 0
   and a simplified relative approximation algorithm developed by Donald H. Knuth suited for larger values
   (|x-y|eps * max(|x|,[|y|)), that checks whether the relative error is bound to a given tolerance eps.
   The function returns true if x and y are considered equal or false otherwise. If both a and b are infinity, the function
   returns true. The same applies to a and b being -infinity or undefined. 2.34.6 */
static int dlong_approx (lua_State *L) {
  longdouble a, b, eps, dist;
  eps = (lua_gettop(L) == 2) ? agn_getepsilon(L) : checkandgetdlongnum(L, 3);
  a = checkandgetdlongnum(L, 1);
  b = checkandgetdlongnum(L, 2);
  dist = FN_FABS(a - b);
  lua_pushboolean(L, dist < eps || (dist <= (eps * fMax(FN_FABS(a), FN_FABS(b)))) || (tools_fpisnanl(a)));
  return 1;
}


static int dlong_round (lua_State *L) {  /* 2.34.6 */
  longdouble x, n;
  n = agnL_optinteger(L, 2, 0);
  x = checkandgetdlongnum(L, 1);
#ifndef __ARMCPU
  createdlong(L, tools_roundfl(x, n, 4));
#else
  createdlong(L, tools_roundf(x, n, 4));
#endif
  return 1;
}


/* Shrinks a number x more or less near zero to exactly zero, using one of many methods.

   The default for eps is Eps. The standard method is 0 for hard shrinking.

   In the first form performs hard shrinking by returning
   - 0 if x < eps,
   - and x otherwise.
   In the second form performs soft shrinking by returning:
   - 0 if |x| <= eps,
   - sign(x) * (abs(x) - eps) if |x| > eps.
   eps is Eps by default.
   The function is a port of Mathematica's Chop function.
   See http://reference.wolfram.com/language/ref/Chop.html &
       https://reference.wolfram.com/legacy/v8/ref/Threshold.html */

static longdouble aux_chop (lua_State *L, longdouble x, longdouble dx, int n, int kind) {
  switch (kind) {
    case 0:  /* "Hard" */
      return (FN_FABS(x) >= dx)*x;
    case 1:  /* "Soft" */
      return tools_signuml(x)*(fMax(0.0L, FN_FABS(x) - dx));  /* 2.17.1 optimisation */
    case 2:  /* "Firm" */
      luaL_error(L, "Error in " LUA_QS ": `firm` method not yet supported.", "long.(x)chop");
      return 0;
    case 3:  /* "PiecewiseGarrote" */
      return (FN_FABS(x) >= dx) * (x - dx*dx/x);
    case 4:  /* "SmoothGarrote" */
      return tools_intpowl(x, 2*n + 1)/(tools_intpowl(x, 2.0L*n) + tools_intpowl(dx, 2.0L*n));
    case 5:  /* "Hyperbola" */
      return (FN_FABS(x) >= dx) * tools_signuml(x)*tools_mhypotl(x, dx);
    default:
      luaL_error(L, "Error in " LUA_QS ": wrong option %ls.", "long.(x)chop", kind);
      return 0;
  }
}

static int dlong_chop (lua_State *L) {  /* 2.34.8 */
  longdouble dx;
  int kind, n;
#ifndef __ARMCPU
  dx = dlongL_optnumber(L, 2, AGN_LDBL_EPSILON);  /* 2.41.2 change from DBL_EPSILON to AGN_LDBL_EPSILON */
#else
  dx = dlongL_optnumber(L, 2, DBL_EPSILON);
#endif
  n = dlongL_optinteger(L, 4, 1);
  kind = 0;
  if (lua_gettop(L) == 3) {
    if (lua_isboolean(L, 3))
      kind = lua_toboolean(L, 3);
    else if (agn_isnumber(L, 3))
      kind = lua_tointeger(L, 3);
    else
      luaL_error(L, "Error in " LUA_QS ": third argument must be a number or boolean.", "long.(x)chop");
  }
  longdouble x = checkandgetdlongnum(L, 1);
  createdlong(L, aux_chop(L, x, dx, n, kind));
  return 1;
}


static int dlong_multiple (lua_State *L) {  /* 2.34.8 */
  longdouble x, y;
  int flag;
  x = checkandgetdlongnum(L, 1);
  y = checkandgetdlongnum(L, 2);
  flag = agnL_optboolean(L, 3, 0);  /* inexactness permitted ? */
  agn_pushboolean(L, tools_ismultiplel(x, y, flag, AGN_LDBL_EPSILON));  /* 2.41.2 change from DBL_EPSILON to AGN_LDBL_EPSILON */
  return 1;
}


static int dlong_fpclassify (lua_State *L) {
  lua_pushinteger(L, tools_fpclassifyl(checkandgetdlongnum(L, 1)));
  return 1;
}


static int dlong_iszero (lua_State *L) {
#ifndef __ARMCPU
  lua_pushboolean(L, tools_fpiszerol(checkandgetdlongnum(L, 1)));
#else
  lua_pushboolean(L, FP_ZERO == tools_fpclassifyl(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


/* Determines if a longdouble is normal, i.e. whether it is neither zero, subnormal, infinite, nor undefined, and returns
   true or false. 2.34.5 */
static int dlong_isnormal (lua_State *L) {
#ifndef __ARMCPU
  lua_pushboolean(L, tools_fpisnormall(checkandgetdlongnum(L, 1)));
#else
  lua_pushboolean(L, FP_NORMAL == tools_fpclassifyl(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_issubnormal (lua_State *L) {
#ifndef __ARMCPU
  lua_pushboolean(L, tools_fpissubnormall(checkandgetdlongnum(L, 1)));
#else
  lua_pushboolean(L, FP_SUBNORMAL == tools_fpclassifyl(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


static int dlong_isirregular (lua_State *L) {  /* 6.8.2 */
#ifndef __ARMCPU
  lua_pushboolean(L, tools_isirregularl(checkandgetdlongnum(L, 1)));
#else
  lua_pushboolean(L, sun_isirregular(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


/* Determines if a longdouble has finite value i.e. whether it is normal, subnormal or zero, but not infinite or undefined,
   and returns true or false. 2.34.5 */
static int dlong_isfinite (lua_State *L) {
  lua_pushboolean(L, tools_fpisfinitel(checkandgetdlongnum(L, 1)));
  return 1;
}


/* Determines if a longdouble is +/-infinity, and returns true or false. 2.34.5 */
static int dlong_isinfinite (lua_State *L) {
#ifndef __ARMCPU
  lua_pushboolean(L, tools_fpisinfl(checkandgetdlongnum(L, 1)) != 0);
#else
  lua_pushboolean(L, isinf(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


/* Determines if a longdouble is undefined, and returns true or false. 2.34.5 */
static int dlong_isundefined (lua_State *L) {
#ifndef __ARMCPU
  lua_pushboolean(L, tools_fpisnanl(checkandgetdlongnum(L, 1)) != 0);
#else
  lua_pushboolean(L, isnan(checkandgetdlongnum(L, 1)));
#endif
  return 1;
}


/*  Composes a floating point value with the magnitude of the first argument and the sign of the second argument. 2.34.5 */
static int dlong_copysign (lua_State *L) {
  longdouble a, b;
  getdbinops(L, a, b);
#ifndef __ARMCPU
  createdlong(L, copysignl(a, b));
#else
  createdlong(L, copysign(a, b));
#endif
  return 1;
}


static int dlong_zeroin (lua_State *L) {  /* 2.41.2 */
  longdouble x, eps;
  x = checkandgetdlongnum(L, 1);
  eps = dlongL_optnumber(L, 2, AGN_LDBL_EPSILON);
  if (eps < 0.0L)
    luaL_error(L, "Error in " LUA_QS ": second argument must be nonnegative.", "long.zeroin");
  createdlong(L, FN_FABS(x) <= eps ? 0.0L : x);
  return 1;
}


static int dlong_zerosubnormal (lua_State *L) {  /* 2.41.2 */
  longdouble x = checkandgetdlongnum(L, 1);
#ifndef __ARMCPU
  createdlong(L, tools_chopl(x));
#else
  createdlong(L, tools_chop(x));
#endif
  return 1;
}


/* Signature so that `factory.reset` knows how to reset a specific iterator */
#define FN_CYTBL            100001
#define FN_CYSTR            100002
#define FN_CYSEQREG         100003
#define FN_COUNTINTSOLO     100010
#define FN_COUNTINTSTEP     100011
#define FN_COUNTDOUBLE      100012

#define aux_autocorrect(L,s0,eps) { \
  if ((L->vmsettings & 1) && (FN_FABS(s0) < eps)) { \
    s0 = 0.0L; \
  } \
}

#define aux_bailout(L,s0,stop,step) { \
  if ((step > 0.0L && s0 > stop) || (step < 0.0L && s0 < stop)) { \
    lua_pushnil(L); \
    return 1; \
  } \
}

static int sema_neumaier (lua_State *L) {  /* based on factory.c/accu_neumaier */
  volatile longdouble s, c, t, x, s0;
  longdouble stop, eps;
  eps = agn_gethepsilon(L);
  s = getdlongvalue(L, lua_upvalueindex(2));   /* accumulator */
  stop = getdlongvalue(L, lua_upvalueindex(3));
  x = getdlongvalue(L, lua_upvalueindex(4));   /* step */
  c = getdlongvalue(L, lua_upvalueindex(5));   /* correction value */
  t = s + x;
  c += (FN_FABS(s) >= FN_FABS(x)) ? (s - t) + x : (x - t) + s;
  s = t;
  s0 = s + c;
  aux_bailout(L, s0, stop, x);
  aux_autocorrect(L, s0, eps);
  createdlong(L, s);
  lua_replace(L, lua_upvalueindex(2));  /* update accumulator */
  createdlong(L, c);
  lua_replace(L, lua_upvalueindex(5));  /* update correction value */
  /* the correction value is added to the final sum, not the intermediate sum. */
  createdlong(L, s0);  /* new `external` index to be returned */
  return 1;
}

static int sema_int (lua_State *L) {
  int64_t val = getdlongvalue(L, lua_upvalueindex(2));
  if (val > getdlongvalue(L, lua_upvalueindex(3)))
    lua_pushnil(L);
  else {
    createdlong(L, ++val);  /* new value */
    lua_pushvalue(L, -1);  /* duplicate it */
    lua_replace(L, lua_upvalueindex(2));  /* update count */
  }
  return 1;  /* return new value */
}

static int sema_intstep (lua_State *L) {
  int64_t val = getdlongvalue(L, lua_upvalueindex(2)) + getdlongvalue(L, lua_upvalueindex(4));
  if (val > getdlongvalue(L, lua_upvalueindex(3)))
    lua_pushnil(L);
  else {
    createdlong(L, val);  /* new value */
    lua_pushvalue(L, -1);  /* duplicate it */
    lua_replace(L, lua_upvalueindex(2));  /* update count */
  }
  return 1;  /* return new value */
}

#define KAHANOZAWA    0
#define BABUSKA       1
#define NEUMAIER      2
#define KBN           3
static int dlong_count (lua_State *L) {  /* based on factory.count, 2.34.7 */
  int alg;
  longdouble count, step, stop, eps;
  alg = NEUMAIER;  /* default is Neumaier */
  eps = agn_gethepsilon(L);
  count = dlongL_optnumber(L, 1, 0);  /* start value */
  step = dlongL_optnumber(L, 2, 1);
  if (step == 0.0L)
    luaL_error(L, "Error in " LUA_QS ": step size is zero.", "long.count");
  stop = dlongL_optnumber(L, 3, HUGE_VAL) + ((step > 0.0L) ? eps : -eps);
  if (FN_FABS(step) <= eps)
    luaL_error(L, "Error in " LUA_QS ": step size |%f| <= %f threshold.", "long.count", (lua_Number)step, (lua_Number)eps);
  if (sun_isintl(count) && sun_isintl(step)) {  /* 2.34.8 tweak */
    luaL_checkstack(L, 3 + (step != 1.0), "not enough stack space");  /* 3.18.4 fix */
    lua_pushinteger(L, (step == 1.0L) ? FN_COUNTINTSOLO : FN_COUNTINTSTEP);  /* 1 push signature */
    createdlong(L, count - step);        /* 2 start value (minus step) */
    createdlong(L, stop);                /* 3 stop value */
    if (step == 1.0L)
      lua_pushcclosure(L, &sema_int, 3);
    else {
      createdlong(L, step);              /* 4 step */
      lua_pushcclosure(L, &sema_intstep, 4);
    }
    return 1;
  }
  switch (alg) {
    case NEUMAIER: {  /* apply Neumaier summation */
      volatile longdouble s, c, t, x;
      x = step;
      s = count - x;  /* accumulator */
      c = 0.0L;       /* correction value */
      t = s + x;
      c += (FN_FABS(s) >= FN_FABS(x)) ? (s - t) + x : (x - t) + s;
      luaL_checkstack(L, 5, "not enough stack space");  /* 3.18.4 fix */
      lua_pushinteger(L, FN_COUNTDOUBLE);  /* 1 - signature */
      createdlong(L, count - step);        /* 2 - count - step */
      createdlong(L, stop);                /* 3 - stop */
      createdlong(L, step);                /* 4 - step */
      createdlong(L, c);                   /* 5 - c */
      lua_pushcclosure(L, &sema_neumaier, 5); /* pops these five values from the stack */
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": should not happen.", "long.count");
  }
  return 1;
}


static int dlong_isequal (lua_State *L) {  /* 2.34.6 */
  longdouble a, b;
  getdbinops(L, a, b);
  lua_pushboolean(L, a == b || (tools_fpisnanl(a) && tools_fpisnanl(b)));  /* 2.34.7 nan fix */
  return 1;
}


static int dlong_isunequal (lua_State *L) {  /* 2.34.5 */
  longdouble a, b;
  getdbinops(L, a, b);
  lua_pushboolean(L, !(a == b || (tools_fpisnanl(a) && tools_fpisnanl(b))));  /* 2.34.7 nan fix */
  return 1;
}


static int dlong_isless (lua_State *L) {  /* 2.34.6 */
  longdouble a, b;
  getdbinops(L, a, b);
  lua_pushboolean(L, a < b);
  return 1;
}


static int dlong_islessequal (lua_State *L) {  /* 2.34.6 */
  longdouble a, b;
  getdbinops(L, a, b);
  lua_pushboolean(L, a <= b);
  return 1;
}


static int dmt_eeq (lua_State *L) {
  longdouble a, b;
  strictgetdbinops(L, a, b);
  lua_pushboolean(L, a == b || (tools_fpisnanl(a) && tools_fpisnanl(b)));  /* 2.34.7 undefined fix */
  return 1;
}


static int dmt_aeq (lua_State *L) {
  int rc;
  longdouble a, b;
  strictgetdbinops(L, a, b);
  if (l_unlikely((a == b) || (tools_fpisnanl(a) && tools_fpisnanl(b)))) {
    rc = 1;
  } else {
    longdouble eps = (longdouble)(L->Eps);
    longdouble dist = FN_FABS(a - b);
    /* HUGE_VAL & HUGE_VALL are the same and there is no difference */
    rc = (dist != HUGE_VAL) && (dist <= eps || dist <= (eps * fMax(FN_FABS(a), FN_FABS(b))));
  }
  lua_pushboolean(L, rc);
  return 1;
}


static int dmt_le (lua_State *L) {
  longdouble a, b;
  strictgetdbinops(L, a, b);
  lua_pushboolean(L, a <= b);
  return 1;
}


static int dmt_lt (lua_State *L) {
  longdouble a, b;
  strictgetdbinops(L, a, b);
  lua_pushboolean(L, a < b);
  return 1;
}


static int dmt_gc (lua_State *L) {
  (void)checkdlong(L, 1);
  lua_setmetatabletoobject(L, 1, NULL, 1);  /* 5.2.0 fix */
  return 0;
}


#define MAX_ITEM   512
static int dmt_tostring (lua_State *L) {  /* at the console, the data is formatted as follows: */
  DLong *d = checkdlong(L, 1);
  if (agn_getutype(L, 1)) {  /* pushes the user-defined type onto the stack */
    if (tools_fpisinfl(d->value)) {
      lua_pushstring(L, signbit(d->value) ? "(-infinity)" : "(infinity)");
    } else if (tools_fpisnanl(d->value)) {
      lua_pushstring(L, "(undefined)");
    } else if (d->value == 0) {  /* 6.5.1 extension */
      lua_pushstring(L, signbit(d->value) ? "(-0)" : "(0)");
    } else {  /* 2.35.0 improvement */
      longdouble absx;
      int i;
      char buf[MAX_ITEM];  /* 2.41.1 big number fix */
      for (i=0; i < MAX_ITEM; i++) buf[i] = 'x';
      absx = FN_FABS(d->value);
#ifndef __ARMCPU
      xsprintf(buf, absx < 1e-20L || absx > 1e20 ? "%.19Le" : "%.19LF", d->value);  /* 19 fractional digits instead of just 13; 2.34.10 fix */
#else
      xsprintf(buf, absx < 1e-20L || absx > 1e20 ? "%.19e" : "%.19F", d->value);
#endif
      /* now the formatted number is in buf, not the format string */
      for (i=tools_strlen(buf) - 1; i > 0; i--) {  /* delete trailing zeros */
        if (buf[i] == '0')
          buf[i] = '\0';
        else
          break;
      }
      if (buf[tools_strlen(buf) - 1] == '.') buf[tools_strlen(buf) - 1] = '\0';
      lua_pushfstring(L, "(%s)", buf);
    }
    lua_concat(L, 2);
  } else {
    luaL_error(L, "Error in " LUA_QS ": invalid long union.", "long.__tostring");
  }
  return 1;
}


static int dlong_tostring (lua_State *L) {  /* 2.34.10 */
  DLong *d = checkdlong(L, 1);
  const char *format = agnL_optstring(L, 2, NULL);
  if (!format) {
    longdouble absx = FN_FABS(d->value);
    lua_pushfstring(L, absx < 1e-10L || absx > 1e20 ? "%le" : "%lf", d->value);  /* 19 fractional digits */
  } else {  /* 2.34.10 extension */
    if (agnL_gettablefield(L, "strings", "format", "long.tostring", 1) != LUA_TFUNCTION) {
      luaL_error(L, "Error in " LUA_QS ": could not find `strings.format`.", "long.tostring");
    }
    lua_pushstring(L, format);
    createdlong(L, d->value);
    lua_call(L, 2, 1);
    if (!lua_isstring(L, -1)) {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": `strings.format` did not return a string.", "long.tostring");
    }
  }
  return 1;
}


static int dlong_tonumber (lua_State *L) {
  DLong *d = checkdlong(L, 1);
  int option = lua_gettop(L) == 2;
  long double x = d->value;
  lua_pushnumber(L, x);
  if (option) {
    x = fabsl(x);
    lua_pushboolean(L, x < DBL_MIN || x > DBL_MAX);  /* 3.7.4 extension */
  }
  return 1 + option;
}


static int dlong_overflow (lua_State *L) {  /* 3.7.4 */
  DLong *d;
  long double x;
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "long.overflow");
  }
  for (i=0; i < nargs && r; i++) {  /* 6.5.1 change */
    d = checkdlong(L, i + 1);
    x = fabsl(d->value);
    r = x < DBL_MIN || x > DBL_MAX;
  }
  lua_pushboolean(L, r);
  return 1;
}


static int dlong_islong (lua_State *L) {  /* 6.4.10 */
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "long.islong");
  }
  for (i=0; i < nargs && r; i++) {  /* 6.5.0 change */
    r = isdlong(L, i + 1);
  }
  lua_pushboolean(L, r);
  return 1;
}


/* Determines if a longdouble is positive, and returns true or false. 6.4.10 */
static int dlong_ispositive (lua_State *L) {
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "long.ispositive");
  }
  for (i=0; i < nargs && r; i++) {  /* 6.5.0 extension */
    r = long_sign(checkandgetdlongnum(L, i + 1)) > 0;
  }
  lua_pushboolean(L, r);
  return 1;
}


/* Determines if a longdouble is non-negative, and returns true or false. 6.4.10 */
static int dlong_isnonnegative (lua_State *L) {
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "long.isnonnegative");
  }
  for (i=0; i < nargs && r; i++) {  /* 6.5.0 extension */
    r = long_sign(checkandgetdlongnum(L, 1)) >= 0;
  }
  lua_pushboolean(L, r);
  return 1;
}


static int dlong_isnegative (lua_State *L) {  /* 6.5.1 */
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "long.isnegative");
  }
  for (i=0; i < nargs && r; i++) {  /* 6.5.0 extension */
    r = long_sign(checkandgetdlongnum(L, 1)) < 0;
  }
  lua_pushboolean(L, r);
  return 1;
}


static int dlong_isposint (lua_State *L) {  /* 6.5.1 */
  long double x;
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "long.isposint");
  }
  for (i=0; i < nargs && r; i++) {
    x = checkandgetdlongnum(L, i + 1);
    r = sun_isintl(x) && long_sign(x) > 0;
  }
  lua_pushboolean(L, r);
  return 1;
}


static int dlong_isnonnegint (lua_State *L) {  /* 6.5.1 */
  long double x;
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "long.isnonnegint");
  }
  for (i=0; i < nargs && r; i++) {
    x = checkandgetdlongnum(L, i + 1);
    r = sun_isintl(x) && long_sign(x) >= 0;
  }
  lua_pushboolean(L, r);
  return 1;
}


static int dlong_isnegint (lua_State *L) {  /* 6.5.1 */
  long double x;
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "long.isnegint");
  }
  for (i=0; i < nargs && r; i++) {
    x = checkandgetdlongnum(L, i + 1);
    r = sun_isintl(x) && long_sign(x) < 0;
  }
  lua_pushboolean(L, r);
  return 1;
}


static int dlong_checklong (lua_State *L) {  /* 6.4.10 */
  int i, nargs = lua_gettop(L);
  for (i=0; i < nargs; i++) {
    if (!isdlong(L, i + 1)) {
      luaL_error(L, "Error: value #%d must be a longdouble, got %s.", i + 1, luaL_typename(L, i + 1));
    }
  }
  return 0;
}


static int dlong_fmin (lua_State *L) {  /* rewritten and tuned, 6.5.6 */
  longdouble a, b;
  a = checkandgetdlongnum(L, 1);
  b = checkandgetdlongnum(L, 2);
  if (a < b) {
    lua_settop(L, 1);
  }
  return 1;
}


static int dlong_fmax (lua_State *L) {  /* rewritten and tuned, 6.5.6 */
  longdouble a, b;
  a = checkandgetdlongnum(L, 1);
  b = checkandgetdlongnum(L, 2);
  if (a > b) {
    lua_settop(L, 1);
  }
  return 1;
}


/*** Various functions *********************************************************************************/

#define lua_rawsetilongnumber(L, idx, n, num) { \
  createdlong(L, num); \
  lua_rawseti(L, (idx) - 1, n); \
}

/* creates an array a with size n, FREE it ! */
#define createarray(a, n, procname) { \
  if ((n) < 1) \
    luaL_error(L, "Error in " LUA_QS ": table or sequence with at least one entry expected.", (procname)); \
  (a) = malloc((n)*sizeof(longdouble)); \
  if ((a) == NULL) \
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", (procname)); \
}

/* expects a vector of dimension n and puts an Agena vector at the top of the stack. */
static int createvector (lua_State *L, longdouble *a, int n) {
  /* create new vector with the elements of C array a */
  int i;
  lua_createtable(L, n, 1);
  for (i=0; i < n; i++) {
    /* if (a[i] != 0) */  /* FIXME: we do not create a sparse vector or matrix */
    lua_rawsetilongnumber(L, -1, i + 1, a[i]);
  }
  /* setvattribs(L, n); */
  return 1;
}

/* Checks whether an object at stack index idx is an Agena matrix.

   If retdims is not 0, assigns its dimensions to p and q, and also puts the dimension pair onto the stack. It returns an error
   if issquare is not 0 and the dimensions are not the same.

   If retdims is 0, the the function does not change the stack. */
static void linalg_auxcheckmatrix (lua_State *L, int idx, int retdims, int issquare, const char *procname, int *p, int *q) {
  if (!agn_istableutype(L, idx, "matrix"))
    luaL_error(L, "Error in " LUA_QS ": matrix expected, got %s.", procname, luaL_typename(L, idx));
  else if (retdims) {
    lua_getfield(L, idx, "dim");  /* pushes dimensions onto the stack */
    if (!lua_ispair(L, -1))
      luaL_error(L, "Error in " LUA_QS ": invalid matrix received, missing dimensions.", procname);
    agn_pairgeti(L, -1, 1);
    *p = agn_checkinteger(L, -1);
    agn_pairgeti(L, -2, 2);
    *q = agn_checkinteger(L, -1);
    agn_poptoptwo(L);  /* pop left and right value */
    if (issquare && ( *p != *q ))
      luaL_error(L, "Error in " LUA_QS ": square matrix expected.", procname);  /* Agena 1.8.1 */
  }
}

/* checks whether the argument is a vector */
#define checkvector(L,a,p) { \
  if (!(agn_istableutype(L, (a), "vector")) ) \
    luaL_error(L, "Error in " LUA_QS ": vector expected, got %s.", (p), lua_typename(L, lua_type((L), (a)))); \
  lua_getfield(L, (a), "dim"); \
  size = agn_checknumber(L, -1); \
  agn_poptop(L); \
}

static int checkVector (lua_State *L, int idx, const char *procname) {
  int size;
  checkvector(L, idx, procname);
  return size;
}

/* expects an n-dimensional Agena row vector at index idx and creates a C array a. The function leaves
   the stack untouched. */
static void fillvectorld (lua_State *L, int idx, longdouble *a, int n, const char *procname) {
  int i;
  if (checkVector(L, idx, procname) != n)
    luaL_error(L, "Error in " LUA_QS ": vector has wrong dimension.", procname);
  for (i=0; i < n; i++) {  /* extended 7.3.10 to assign long double */
    lua_rawgeti(L, -1, i + 1);  /* do not call lua_geti */
    if (agn_isnumber(L, -1)) {
      a[i] = agn_tonumber(L, -1);
    } else if (isdlong(L, -1)) {
      a[i] = getdlongvalue(L, -1);
    } else {  /* with non-numbers, sets zero */
      a[i] = 0.0L;
    }
    agn_poptop(L);  /* pop row vector element */
  }
}

/* Gauß Elimination with Pivoting

   Merge of code taken from:
   - http://www.dailyfreecode.com/code/basic-gauss-elimination-method-gauss-2949.aspx posted by Alexander Evans, and
   - http://paulbourke.net/miscellaneous/gausselim published by Paul Bourke;
   modified for Agena.

   At completion, a will hold the upper triangular matrix, and x the solution vector. */
static int gsolve (longdouble *a, int n, longdouble *x, longdouble eps) {
  int i, j, k, maxrow, nn, issingular;
  longdouble tmp, max;
  nn = n + 1;
  issingular = 0;
  for (j=0; j < n; j++) {  /* modified, for (j=0; j < n-1; j++) { */
    max = FN_FABS(a[j*nn + j]);
    maxrow = j;
    for (i=j + 1; i < n; i++)  /* find the row with the largest first value */
      if (FN_FABS(a[i*nn + j]) > max) {
        max = a[i*nn + j];
        maxrow = i;
      }
    /*  if (maxrow != j) { */
    for (k=j; k < n + 1; k++) {  /* swap the maxrow and jth row */
      tmp = a[j*nn + k];
      a[j*nn + k] = a[maxrow*nn + k];
      a[maxrow*nn + k] = tmp;
    }
    /* } */
    if (issingular == 0 && FN_FABS(a[j*nn + j]) < eps) {  /* 7.5.1 improvement */
      issingular = 1; /* statement added to detect singular matrices */
    }
    for (i=j + 1; i < n; i++) {  /* eliminate the ith element of the jth row */
      tmp = a[i*nn + j] / a[j*nn + j];
      for (k=n; k >= j; k--) {
        a[i*nn + k] -=  a[j*nn + k]*tmp;
      }
    }
  }
  for (j=n - 1; j >= 0; j--) {  /* conduct back substitution */
    tmp = 0;
    for (k=j + 1; k < n; k++) {
      tmp += a[j*nn + k] * x[k];
    }
    x[j] = (a[j*nn + n] - tmp)/a[j*nn + j];
  }
  return issingular;
}

static int dlong_gsolve (lua_State *L) {  /* taken from long.gsolve, 2.34.10 */
  int i, m, n, nargs, iszero, isundefined;
  longdouble *a, *x, eps;
  nargs = lua_gettop(L);
  m = n = 0;
  linalg_auxcheckmatrix(L, 1, 1, 0, "long.gsolve", &m, &n);
  agn_poptop(L);  /* pop dimension pair of the matrix */
  eps = agn_getdblepsilon(L);
  if (nargs == 1) {
    if (m + 1 != n)
      luaL_error(L, "Error in " LUA_QS ": matrix has wrong dimensions.", "long.gsolve");
  } else if (nargs == 2) {
    if (m != n)
      luaL_error(L, "Error in " LUA_QS ": expected a square matrix.", "long.gsolve");
    if (n != checkVector(L, 2, "long.gsolve"))
      luaL_error(L, "Error in " LUA_QS ": expected matrix and vector with equal dimensions.", "long.gsolve");
    n++;
  } else
    luaL_error(L, "Error in " LUA_QS ": one or two arguments expected.", "long.gsolve");
  createarray(a, m*n, "long.gsolve");
  createarray(x, m, "long.gsolve");
  if (nargs == 1) {
    if (fillmatrixld(L, 1, a, m, n, 0, "long.gsolve")) {  /* 7.3.4 fix */
      xfreeall(a, x);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "long.gsolve");
    }
  } else {  /* augment square matrix with vector */
    int i;
    longdouble *b;
    createarray(b, m, "long.gsolve");
    if (fillmatrixld(L, 1, a, m, n, 1, "long.gsolve")) {  /* 7.3.4 fix */
      xfreeall(a, x, b);
      luaL_error(L, "Error in " LUA_QS ": row vector has wrong dimension.", "long.gsolve");
    }
    fillvectorld(L, 2, b, m, "long.gsolve");
    for (i=0; i < m; i++) a[i*n + m] = b[i];
    xfree(b);
  }
  if (gsolve(a, m, x, eps)) {  /* singular matrix found */
    lua_pushfail(L);
    return 1;
  }
  iszero = 1; isundefined = 0;
  /* now inspect augmented row reduced echelon matrix */
  for (i=0; i < n - 1; i++) {
    if (!tools_approxl(a[(m - 1)*n + i], 0, eps)) {
      iszero = 0;
      break;
    }
  }
  for (i=0; i < n; i++) {
    if (tools_fpisnanl(a[(m - 1)*n + i])) {
      isundefined = 1;
      break;
    }
  }
  if (isundefined) {  /* example: m := matrix([1, 1, 1], [2, 2, 5], [4, 4, 8]), b := vector(-1, -8, -14) */
    lua_pushfail(L);
  } else if (iszero) {  /* infinite number of solutions or no solution */
    lua_pushnumber(L, tools_approxl(a[(m - 1)*n + (n - 1)], 0, eps) ? HUGE_VAL : AGN_NAN);
  } else {
    createvector(L, x, m);
  }
  xfreeall(a, x);
  return 1;
}


static int polygen (lua_State *L) {  /* 6.6.5 */
  longdouble x;
  LongDoubleArray *a = NULL;
  x = checkandgetdlongnum(L, 1);
  a = (LongDoubleArray *)lua_touserdata(L, lua_upvalueindex(1));
  createdlong(L, tools_polyevalfma(x, a, a->size));
  return 1;
}

static int dlong_polygen (lua_State *L) {
  int nops;
  LongDoubleArray *a = NULL;
  nops = agnL_gettop(L, "expected at least one argument", "long.polygen");
  luaL_checkstack(L, nops, "too many arguments");
  agnL_pushcoeffs(L, a, nops, "long.polygen");
  lua_pushcclosure(L, &polygen, 1);
  return 1;
}


static const struct luaL_Reg dlong_lib [] = {  /* metamethods for userdata `n` */
  {"__eq",         dmt_eeq},       /* equality mt */
  {"__eeq",        dmt_eeq},       /* strict equality mt */
  {"__aeq",        dmt_aeq},       /* approximate equality mt */
  {"__lt",         dmt_lt},        /* less-than mt */
  {"__le",         dmt_le},        /* less-or-equal mt */
  {"__unm",        dmt_unm},       /* unary minus */
  {"__abs",        dmt_abs},
  {"__absdiff",    dmt_absdiff},
  {"__sign",       dmt_sign},
  {"__signum",     dmt_signum},
  {"__add",        dmt_add},
  {"__sub",        dmt_sub},
  {"__mul",        dmt_mul},
  {"__div",        dmt_div},
  {"__mod",        dmt_mod},
  {"__sin",        dmt_sin},
  {"__cos",        dmt_cos},
  {"__tan",        dmt_tan},
  {"__arcsin",     dmt_arcsin},
  {"__arccos",     dmt_arccos},
  {"__arctan",     dmt_arctan},
  {"__arcsec",     dmt_arcsec},
  {"__sinh",       dmt_sinh},
  {"__cosh",       dmt_cosh},
  {"__tanh",       dmt_tanh},
  {"__sinc",       dmt_sinc},
  {"__square",     dmt_square},
  {"__cube",       dmt_cube},
  {"__recip",      dmt_recip},
  {"__pow",        dmt_pow},
  {"__ipow",       dmt_ipow},
  {"__squareadd",  dmt_squareadd},
  {"__antilog2",   dmt_antilog2},
  {"__antilog10",  dmt_antilog10},
  {"__sqrt",       dmt_sqrt},
  {"__invsqrt",    dmt_invsqrt},
  {"__ln",         dmt_ln},
  {"__lngamma",    dmt_lngamma},
  {"__log",        dmt_log},
  {"__exp",        dmt_exp},
  {"__intdiv",     dmt_intdiv},
  {"__zero",       dmt_zero},
  {"__nonzero",    dmt_nonzero},
  {"__entier",     dmt_entier},
  {"__int",        dmt_int},
  {"__frac",       dmt_frac},
  {"__even",       dmt_even},
  {"__odd",        dmt_odd},
  {"__nan",        dmt_nan},
  {"__integral",   dmt_integral},
  {"__fractional", dmt_fractional},
  {"__finite",     dmt_finite},
  {"__gc",         dmt_gc},
  {"__tostring",   dmt_tostring},  /* for output at the console, e.g. print(n) */
  {NULL, NULL}
};


static const luaL_Reg dlonglib[] = {
  {"approx",       dlong_approx},
  {"arccosh",      dlong_arccosh},
  {"arccot",       dlong_arccot},
  {"arccoth",      dlong_arccoth},
  {"arccsc",       dlong_arccsc},
  {"arccsch",      dlong_arccsch},
  {"arcsech",      dlong_arcsech},
  {"arcsinh",      dlong_arcsinh},
  {"arctanh",      dlong_arctanh},
  {"besselj",      dlong_besselj},
  {"bessely",      dlong_bessely},
  {"beta",         dlong_beta},
  {"binomd",       dlong_binomd},
  {"cas",          dlong_cas},
  {"cathet",       dlong_cathet},
  {"cbrt",         dlong_cbrt},
  {"ceil",         dlong_ceil},
  {"checklong",    dlong_checklong},
  {"chop",         dlong_chop},
  {"copysign",     dlong_copysign},
  {"cot",          dlong_cot},
  {"coth",         dlong_coth},
  {"count",        dlong_count},
  {"csc",          dlong_csc},
  {"csch",         dlong_csch},
  {"double",       dlong_new},  /* alias */
  {"eps",          dlong_eps},
  {"erf",          dlong_erf},
  {"erfc",         dlong_erfc},
  {"erfcx",        dlong_erfcx},
  {"expx2",        dlong_expx2},
  {"expm1",        dlong_expm1},
  {"exponent",     dlong_exponent},
  {"fact",         dlong_fact},
  {"fdim",         dlong_fdim},
  {"floor",        dlong_floor},
  {"fma",          dlong_fma},
  {"fmax",         dlong_fmax},
  {"fmin",         dlong_fmin},
  {"fmod",         dlong_fmod},
  {"frexp",        dlong_frexp},
  {"gamma",        dlong_gamma},
  {"gsolve",       dlong_gsolve},
  {"hypot",        dlong_hypot},
  {"hypot2",       dlong_hypot2},
  {"hypot3",       dlong_hypot3},
  {"ilog2",        dlong_ilog2},
  {"invbinomd",    dlong_invbinomd},
  {"inverf",       dlong_inverf},
  {"inverfc",      dlong_inverfc},
  {"invnormald",   dlong_invnormald},
  {"isequal",      dlong_isequal},
  {"isless",       dlong_isless},
  {"islessequal",  dlong_islessequal},
  {"islong",       dlong_islong},
  {"isnegative",   dlong_isnegative},
  {"isnegint",     dlong_isnegint},
  {"isnonnegative",dlong_isnonnegative},
  {"isnonnegint",  dlong_isnonnegint},
  {"isposint",     dlong_isposint},
  {"ispositive",   dlong_ispositive},
  {"isunequal",    dlong_isunequal},
  {"koadd",        dlong_koadd},
  {"ldexp",        dlong_ldexp},
  {"lnabs",        dlong_lnabs},
  {"lnbinomial",   dlong_lnbinomial},
  {"lnfact",       dlong_lnfact},
  {"lnp1",         dlong_lnp1},
  {"log10",        dlong_log10},
  {"log2",         dlong_log2},
  {"mantissa",     dlong_mantissa},
  {"modf",         dlong_modf},
  {"multiple",     dlong_multiple},
  {"new",          dlong_new},
  {"nextafter",    dlong_nextafter},
  {"norm",         dlong_norm},
  {"cdfnormald",   dlong_cdfnormald},
  {"normalise",    dlong_normalise},
  {"overflow",     dlong_overflow},
  {"polygen",      dlong_polygen},
  {"psi",          dlong_psi},
  {"pytha4",       dlong_pytha4},
  {"pytha",        dlong_pytha},
  {"redupi",       dlong_redupi},
  {"rempio2",      dlong_rempio2},
  {"root",         dlong_root},
  {"round",        dlong_round},
  {"sec",          dlong_sec},
  {"sech",         dlong_sech},
  {"signbit",      dlong_signbit},
  {"significand",  dlong_significand},
  {"sincos",       dlong_sincos},
  {"sinhcosh",     dlong_sinhcosh},
  {"tonumber",     dlong_tonumber},
  {"tostring",     dlong_tostring},
  {"trunc",        dlong_trunc},
  {"unm",          dlong_unm},
  {"wrap",         dlong_wrap},
  {"zeroin",       dlong_zeroin},
  {"zerosubnormal", dlong_zerosubnormal},
  {"fpclassify",   dlong_fpclassify},
  {"isnormal",     dlong_isnormal},
  {"isirregular",  dlong_isirregular},
  {"issubnormal",  dlong_issubnormal},
  {"iszero",       dlong_iszero},
  {"isfinite",     dlong_isfinite},
  {"isinfinite",   dlong_isinfinite},
  {"isundefined",  dlong_isundefined},
  {"xabs",         dlong_abs},
  {"xantilog10",   dlong_antilog10},
  {"xantilog2",    dlong_antilog2},
  {"xarccos",      dlong_arccos},
  {"xarcsec",      dlong_arcsec},
  {"xarcsin",      dlong_arcsin},
  {"xarctan",      dlong_arctan},
  {"xcos",         dlong_cos},
  {"xcosh",        dlong_cosh},
  {"xcube",        dlong_cube},
  {"xexp",         dlong_exp},
  {"xinvsqrt",     dlong_invsqrt},
  {"xln",          dlong_ln},
  {"xlngamma",     dlong_lngamma},
  {"xrecip",       dlong_recip},
  {"xsign",        dlong_sign},
  {"xsignum",      dlong_signum},
  {"xsin",         dlong_sin},
  {"xsinc",        dlong_sinc},
  {"xsinh",        dlong_sinh},
  {"xsqrt",        dlong_sqrt},
  {"xsquare",      dlong_square},
  {"xtan",         dlong_tan},
  {"xtanh",        dlong_tanh},
  {"xint",         dlong_int},
  {"xfrac",        dlong_frac},
  {"xentier",      dlong_entier},
  {NULL, NULL}
};


/*
** Open long library
*/
LUALIB_API int luaopen_long (lua_State *L) {
  luaL_newmetatable(L, "longdouble");    /* metatable for long double userdata, adds it to the registry with key 'long' */
  luaL_register(L, NULL, dlong_lib);     /* assign C metamethods to this metatable */
  luaL_register(L, AGENA_LONGLIBNAME, dlonglib);
  lua_pushinteger(L, FP_NAN);
  lua_setfield(L, -2, "fp_nan");         /* for FP_NAN */
  lua_pushinteger(L, FP_INFINITE);
  lua_setfield(L, -2, "fp_infinite");    /* for FP_INFINITE */
  lua_pushinteger(L, FP_SUBNORMAL);
  lua_setfield(L, -2, "fp_subnormal");   /* for FP_SUBNORMAL */
  lua_pushinteger(L, FP_ZERO);
  lua_setfield(L, -2, "fp_zero");        /* for FP_ZERO */
  lua_pushinteger(L, FP_NORMAL);
  lua_setfield(L, -2, "fp_normal");      /* for FP_NORMAL */
#ifndef __ARMCPU
  createdlong(L, LDBL_MIN);              /* minimum positive _normalised_ value */
#else
  createdlong(L, DBL_MIN);               /* minimum positive _normalised_ value */
#endif
  lua_setfield(L, -2, "ldblmin");
#ifndef __ARMCPU
  createdlong(L, LDBL_MAX);              /* maximum positive _normalised_ value */
#else
  createdlong(L, DBL_MAX);               /* maximum positive _normalised_ value */
#endif
  lua_setfield(L, -2, "ldblmax");
  createdlong(L, AGN_LDBL_EPSILON);
  lua_setfield(L, -2, "ldblepsilon");
  return 1;
}

