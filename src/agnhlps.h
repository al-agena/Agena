/*
** $Id: agnhlps.h $
** C Helper routines
** See Copyright Notice in agena.h
** initiated Alexander Walz, July 20, 2007
*/

#ifndef agnhlps_h
#define agnhlps_h

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>     /* div_t */

#if defined(__DJGPP__) || defined(OPENSUSE)
#include <stdint.h>     /* for int32_t */
#include <sys/types.h>  /* for off_t & off64_t */
#endif

#include "prepdefs.h"

/* stime, ftime deprecated with GLIBC 2.31. */
#if LINUX_NOFSTIME
#include <sys/timeb.h>
#include <sys/times.h>
#endif

#include "agena.h"
#include "agnconf.h"
#include "agncmpt.h"
#include "agnt64.h"
#include "cephes.h"
#include "sunpro.h"  /* EXTRACT_WORDS */
#include "calc.h"
/* We must define LUAI_USER_ALIGNMENT_T here again (see agnconf.h) in order to compile */
#ifndef LUAI_USER_ALIGNMENT_T
#define LUAI_USER_ALIGNMENT_T  union { double u; void *s; long l; }
#endif
#include "llimits.h"  /* for lua_number2int */

/* TYPEDEF 2.14.4 */

#ifndef off64_t
#define off64_t long long int
#endif


#define SWAP(a,b,t) { (t) = (a); (a) = (b); (b) = (t); }
#define tools_bzero(to,count) memset(to, 0, count)

/* TOOLS_SWAP is a GCC extension */
#define TOOLS_SWAP(x, y) do { typeof(x) TOOLS_SWAP = x; x = y; y = TOOLS_SWAP; } while (0)

/* some constants */

#undef SQRT2
#define SQRT2        1.41421356237309504880168872421

#undef PI
#define PI           3.14159265358979323846264338328

#undef PI2
#define PI2          6.28318530717958647692528676656  /* Pi*2 */

#undef PIO2
#define PIO2         1.57079632679489661923132169164  /* Pi/2 = 90 degrees */

#undef PIO4
#define PIO4         0.78539816339744830961566084582

#undef INVPI2
#define INVPI2       0.15915494309189533576888376337  /* 1/(Pi*2) */

#undef INVPIO4
#define INVPIO4      1.27323954473516268615107010698  /* 4/Pi, 2.14.6 */

#undef INVPISQO4
#define INVPISQO4    0.40528473456935108577551785284  /* 4/Pi^2, 2.14.6 */

#undef ONEOPI
#define ONEOPI       0.31830988618379067153776752674  /* 1/Pi, 3.15.1 */

#undef ONEOPIld
#define ONEOPIld     0.318309886183790671537767526745L  /* = 1/Pi */

#undef TWOOPI
#define TWOOPI       0.63661977236758134307553505349  /* 2/Pi, 4.11.6 */

#undef ZETA2
#define ZETA2        1.64493406684822643647  /* Zeta(2) = Pi^2/6, 21 Digits */

#undef TWOOPIld
#define TWOOPIld     0.636619772367581343075535053490L  /* = 2/Pi */

#undef EULERGAMMAld
#define EULERGAMMAld 0.577215664901532860606512090082L  /* Maple V Release 4: Digits := 30; evalf(gamma) */

#undef ZETA2ld
#define ZETA2ld      1.64493406684822643647241516665L  /* Zeta(2) = Pi^2/6 */

#undef RAD2DEG
#define RAD2DEG      57.2957795130823208767981548141   /* 180/Pi, 3.16.1 */

#undef RAD2DEGL
#define RAD2DEGL     57.2957795130823208767981548141L  /* 180/Pi, 3.16.1 */

#undef DEG2RAD
#define DEG2RAD      0.0174532925199432957692369076849

#undef DEG2RADL
#define DEG2RADL     0.0174532925199432957692369076849L

#undef EXP1
#define EXP1         2.71828182845904523536028747135

#undef AGN_EPSILON
#define AGN_EPSILON  1.4901161193847656e-08  /* suited for doubles */

#undef AGN_HEPSILON  /* 2.12.1 */
#define AGN_HEPSILON 1.4901161193847656e-12

/* Constant taken from: https://stackoverflow.com/questions/34924598/why-cant-i-add-ldbl-epsilon-to-1 */
#ifndef __ARMCPU
#define AGN_LDBL_EPSILON 1.084202172485504434E-19  /* without the L suffix */
#else
#define AGN_LDBL_EPSILON DBL_EPSILON  /* = 1.084202172485504434007452e-19L */
#endif

#undef RADIANS_PER_DEGREE
#define RADIANS_PER_DEGREE (PI/180.0)

#undef EULERGAMMA
#define EULERGAMMA   0.577215664901532860606512090082  /* Maple V Release 4: Digits := 30; evalf(gamma) */

#ifndef __ARMCPU
#undef LN2
#define LN2          0.693147180559945309417232121458  /* = ln(2) */

#undef INVLN2
#define INVLN2       1.44269504088896340735992468100   /* = 1/ln(2) */

#undef LN10
#define LN10         2.30258509299404568401799145468   /* = ln(10) */

#undef INVLN10
#define INVLN10      0.434294481903251827651128918917  /* = 1/ln(10) */

#else  /* I really do not understand why the defines above do no longer compile on Raspberry Pi */

static const long double LN2     = 0.693147180559945309417232121458L;
static const long double INVLN2  = 1.44269504088896340735992468100L;
static const long double LN10    = 2.30258509299404568401799145468L;
static const long double INVLN10 = 0.434294481903251827651128918917L;
#endif

#undef LOG2_10
#define LOG2_10      3.32192809488736234787031942949  /* log[2](10) */

#undef INVLOG2_10
#define INVLOG2_10   0.301029995663981195213738894724  /* 1/log[2](10) */

#undef M_SQRT1_2
#define M_SQRT1_2    0.707106781186547524400844362105  /* = 1/sqrt(2) */

#undef SQRTPI
#define SQRTPI       1.77245385090551602729816748334   /* = sqrt(Pi) */

#undef SQRTPIld
#define SQRTPIld     1.7724538509055160272981674833411451827975494561224L   /* = sqrt(Pi) */

#undef SQRTPI2
#define SQRTPI2      2.50662827463100050241576528481   /* = sqrt(2*Pi) */

#undef SQRT5
#define SQRT5        2.23606797749978969640917366873   /* = sqrt(5) */

#undef INVSQRTPI
#define INVSQRTPI    0.56418958354775628694807945156   /* 1/sqrt(Pi) */

#undef INVSQRTPIld
#define INVSQRTPIld  0.56418958354775628694807945156077258584405062932899L   /* 1/sqrt(Pi) */

#undef LNSQRTPI
#define LNSQRTPI     0.57236494292470008707171367568   /* ln(sqrt(Pi)) */

#undef PHI
#define PHI          1.61803398874989484820458683437   /* = ((1.0 + sqrt(5.0))/2.0), Golden ratio, changed 2.9.8 */

#undef PHIld
#define PHIld        1.6180339887498948482045868343656381177203091798058L

#undef PHIINV
#define PHIINV       0.618033988749894848204586834364  /* = 1/((1.0 + sqrt(5.0))/2.0), inverse Golden ratio */

#undef PHIINVld
#define PHIINVld     0.61803398874989484820458683436563811772030917980575L

#undef PHIINVSQ
#define PHIINVSQ     0.381966011250105151795413165632  /* = (1/((1.0 + sqrt(5.0))/2.0))**2, squared inverse Golden ratio */

#undef PHIINVSQld
#define PHIINVSQld   0.3819660112501051517954131656343618822796908201942L

#undef PHILN
#define PHILN        0.481211825059603447497758913427  /* = ln(math.Phi) */

#undef INVPHILN
#define INVPHILN     2.07808692123502753760132260611   /* = 1/ln(math.Phi) */

#undef SQRTH
#define SQRTH        0.70710678118654752440L

/* 0x43500000, 0x00000000 = 2^54, for normalisising a subnormal number x by multiplication, i.e. x *= TWO54 */
#undef TWO54
#define TWO54        1.80143985094819840000e+16

/* Just for documentation, as the following constants seem to be compiled into libm:
OpenBSD/sys/arch/i386/include/_float.h:#define __DBL_EPSILON            2.2204460492503131E-16
OpenBSD/sys/arch/i386/include/_float.h:#define __LDBL_EPSILON           1.08420217248550443401e-19L */
#ifndef DBL_EPSILON
#define DBL_EPSILON                    2.2204460492503131e-16
#endif

#define DBL_EPSILON_SQUARED            0.493038065763132386887854044803e-31  /* 2.17.8, Maple: (2.2204460492503131E-16)^2; */
#define TWO_OVER_DBL_EPSILON           0.900719925474099192230726999071e16   /* 2.17.8, Maple: 2/(2.2204460492503131E-16) */
#define FOUR_OVER_DBL_EPSILON_SQUARED  0.811296384146066802962012054672e32   /* 2.17.8, Maple: 4/((2.2204460492503131E-16)^2) */

#define tools_b64     "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define tools_cb64    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"


/* Long double constants */
#define M_Eld         2.718281828459045235360287471352662498L  /* = exp(1) */
#define M_LOG2Eld     1.442695040888963407359924681001892137L  /* = log_2 e */
#define M_LOG10Eld    0.434294481903251827651128918916605082L  /* = log_10 e */
#define M_LN2ld       0.693147180559945309417232121458176568L  /* = log_e 2 */
#define M_LN10ld      2.302585092994045684017991454684364208L  /* = log_e 10 */
#define M_PI2ld       6.283185307179586476925286766559005768L  /* = 2*Pi */
#define M_PIld        3.141592653589793238462643383279502884L  /* = Pi */
#define M_PIO2ld      1.570796326794896619231321691639751442L  /* = Pi/2 */
#define M_PIO4ld      0.785398163397448309615660845819875721L  /* = Pi/4 */
#define M_1OPIld      0.318309886183790671537767526745028724L  /* = 1/Pi */
#define M_2OPIld      0.636619772367581343075535053490057448L  /* = 2/Pi */
#define M_2OSQRTPIld  1.128379167095512573896158903121545172L  /* = 2/sqrt(Pi) */
#define M_SQRT2ld     1.414213562373095048801688724209698079L  /* = sqrt(2) */
#define M_SQRT1_2ld   0.707106781186547524400844362104849039L  /* = 1/sqrt(2) */
#define M_SQRTPI1_2ld 0.886226925452758013649083741670572590L  /* = 0.5*sqrt(Pi) */
#ifndef M_PIO2
#define M_PIO2        1.570796326794896619231321691639751442  /* = Pi/2 */
#endif
#ifndef M_PIO4
#define M_PIO4        0.785398163397448309615660845819875721  /* = Pi/4 */
#endif

/* see: https://stackoverflow.com/questions/48737056/bit-cast-uint64-t-to-double-and-back-in-a-macro */
#define DoubleAsUb8(x) ((union { double d; uint64_t u; }) { x } .u)
#define Ub8AsDouble(x) ((union { uint64_t u; double d; }) { x } .d)

/* misc functions */

/* used in packages `ads` */

LUALIB_API void tools_quicksort (off64_t v[], int32_t left, int32_t right);
LUALIB_API int tools_binsearch (off64_t *a, off64_t cnt, off64_t z);

/* used in packages `stats` */

LUALIB_API void tools_dquicksort (double v[], int32_t left, int32_t right);
LUALIB_API int tools_dnonrecursivequicksort (double *a, size_t n);
LUALIB_API int pixel_qsort (double *pix_arr, int npix);
LUALIB_API int tools_sort (double *a, size_t ii, int mode);

LUALIB_API INLINE void tools_dheapsort (double *a, int32_t l, int32_t r);
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API INLINE void tools_dheapsortl (long double *a, int32_t l, int32_t r);
#endif
LUALIB_API void tools_dintrosort (double *v, int32_t left, int32_t right, size_t depth, size_t threshold);
LUALIB_API void klib_dintrosort (double *a, int32_t size);
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API void tools_dintrosortl (long double *v, int32_t left, int32_t right, size_t depth, size_t threshold);
#endif
LUALIB_API void tools_dheapsort_uchar (unsigned char *a, int32_t l, int32_t r);
LUALIB_API void tools_dintrosort_uchar (unsigned char *v, int32_t left, int32_t right, size_t depth, size_t threshold);

LUALIB_API void tools_dheapsort_ushort (uint16_t *a, int32_t l, int32_t r);
LUALIB_API void tools_dintrosort_ushort (uint16_t *v, int32_t left, int32_t right, size_t depth, size_t threshold);

LUALIB_API void tools_dheapsort_int32 (int32_t *a, int32_t l, int32_t r);
LUALIB_API void tools_dintrosort_int32 (int32_t *v, int32_t left, int32_t right, size_t depth, size_t threshold);

LUALIB_API void tools_dheapsort_uint32 (uint32_t *a, int32_t l, int32_t r);
LUALIB_API void tools_dintrosort_uint32 (uint32_t *v, int32_t left, int32_t right, size_t depth, size_t threshold);

LUALIB_API void tools_dintrosort_uint64 (uint64_t *v, int32_t left, int32_t right, size_t depth, size_t threshold);
LUALIB_API void tools_dheapsort_uint64 (uint64_t *a, int32_t l, int32_t r);

LUALIB_API void tools_dintrosort_int64 (int64_t *v, int32_t left, int32_t right, size_t depth, size_t threshold);
LUALIB_API void tools_dheapsort_int64 (int64_t *a, int32_t l, int32_t r);

LUALIB_API double tools_kth_smallest (double a[], long int n, long int k);

LUALIB_API uint64_t tools_isqrtu64 (uint64_t n);
LUALIB_API uint64_t tools_mulmodu64 (uint64_t a, uint64_t b, uint64_t mod);
LUALIB_API int64_t  tools_mulmodi64 (int64_t a, int64_t b, int64_t mod);
LUALIB_API uint64_t tools_powmodu64 (uint64_t base, uint64_t expo, uint64_t mod);
LUALIB_API int64_t  tools_powmodi64 (int64_t base, int64_t expo, int64_t mod);
LUALIB_API uint64_t tools_gcdu64 (uint64_t a, uint64_t b);
LUALIB_API int64_t  tools_gcdi64 (int64_t a, int64_t b);
LUALIB_API int64_t  tools_lcmi64 (int64_t a, int64_t b);
LUALIB_API uint64_t tools_lcmu64 (uint64_t a, uint64_t b);

LUALIB_API int tools_isprime (uint64_t x);
LUALIB_API uint64_t tools_nextprime (uint64_t n);
LUALIB_API uint64_t tools_prevprime (uint64_t x);

/* Dr. F.H.Toor's and my database C functions used in packages `base` and `phonetiqs` */

LUALIB_API int     my_open (const char *file);
LUALIB_API FILE    *my_fopen (const char *file, int append);
LUALIB_API int     my_roopen (const char *file);
LUALIB_API FILE    *my_froopen (const char *file);
LUALIB_API int     my_create (const char *file);
LUALIB_API FILE    *my_fcreate (const char *file);
LUALIB_API int     my_close (int hnd);
LUALIB_API off64_t my_seek (int hnd, off64_t pos);
LUALIB_API off64_t my_lof (int hnd);
LUALIB_API ssize_t my_read (int hnd, void *data, size_t size);
LUALIB_API int32_t my_readl (int hnd);
LUALIB_API char    my_readc (int hnd);
LUALIB_API void    my_write (int hnd, void *data, size_t size);
LUALIB_API void    my_writel (int hnd, int32_t data);
LUALIB_API void    my_writec (int hnd, char data);
LUALIB_API int     my_lock (int hnd, off64_t start, off64_t size);
LUALIB_API int     my_unlock (int hnd, off64_t start, off64_t size);
LUALIB_API void    my_move (int hnd, off64_t fpos, off64_t tpos, off64_t size);
LUALIB_API void    my_expand (int hnd, int mrc, int cnt, int count, int *error);
LUALIB_API off64_t my_fpos (int hnd);
LUALIB_API const char *my_ioerror (int en);

LUALIB_API int      sec_read (int hnd, void *data, size_t size);
LUALIB_API char     sec_readc (int hnd, int *success);
LUALIB_API int32_t  sec_readl (int hnd, int *success);
LUALIB_API off64_t  sec_lof (int hnd, int *success);

LUALIB_API int tools_fsync (int hnd);
LUALIB_API int tools_eof (FILE *f);
LUALIB_API int tools_writable (FILE *fp);  /* 7.3.3 */
LUALIB_API int tools_iswinpath (const char *path);  /* 5.2.3 */
LUALIB_API int tools_isunixpath (const char *path);  /* 5.2.3 */

/* memory allocation */
LUALIB_API INLINE void   *tools_malloc (size_t size, int zero);
LUALIB_API INLINE void   *tools_realloc (void *buf, size_t oldsize, size_t newsize, int zero);
LUALIB_API INLINE void   *tools_memalign (size_t align, size_t size);
LUALIB_API INLINE void   tools_memaligned_free (void *ptr);

/* string functions */
LUALIB_API char          *str_charreplace (char *s, char from, char to, int flag);
LUALIB_API char          *str_concat (const char *s1, ...);
LUALIB_API char          *str_insert (const char *str, const char *what, int pos);
LUALIB_API int           str_glob (const char *pat, const char *str);
LUALIB_API char          *str_substr (const char *str, const size_t begin, const size_t end, int *error);
LUALIB_API char          *str_reverse (const char *s, size_t len);
LUALIB_API char          *latin9_to_utf8 (const char *string, size_t *len);
LUALIB_API char          *utf8_to_latin9 (const char *string, size_t *len);
LUALIB_API size_t        utf8_to_latin9_len (const char *string, size_t *len);
LUALIB_API char          *str_hextodec (const char *str, size_t l, int *rc);
LUALIB_API int           is_utf8 (const char *string, size_t *pos);
LUALIB_API size_t        size_utf8 (const char *str);
LUALIB_API void          cp850_to_latin1 (unsigned char *s);
LUALIB_API size_t        cp850_to_utf8 (const unsigned char *s, unsigned char *d, int *rc);
LUALIB_API void          cp852_to_latin2 (unsigned char *s);
LUALIB_API size_t        cp852_to_utf8 (const unsigned char *s, unsigned char *d, int *rc);
LUALIB_API void          cp860_to_latin1 (unsigned char *s);
LUALIB_API size_t        cp860_to_utf8 (const unsigned char *s, unsigned char *d, int *rc);
LUALIB_API void          cp775_to_latin4 (unsigned char *s);
LUALIB_API size_t        cp775_to_utf8 (const unsigned char *s, unsigned char *d, int *rc);
LUALIB_API size_t        win1258_to_utf8 (const unsigned char *s, unsigned char *d, int *rc);
LUALIB_API void          cp857_to_latin5 (unsigned char *s);
LUALIB_API size_t        cp857_to_utf8 (const unsigned char *s, unsigned char *d, int *rc);
LUALIB_API int           tools_isnumericstring (const char *str, int checksign);
LUALIB_API int           tools_isnumericlstring (const char *str, size_t l, int checksign);
LUALIB_API int           tools_stricmp (const char* p, const char* q);
LUALIB_API int           tools_strverscmp (const char *l0, const char *r0);
LUALIB_API size_t        tools_strlcpy (char *d, const char *s, size_t n);
LUALIB_API char          *tools_itoa (int64_t val, int base);
LUALIB_API INLINE ptrdiff_t tools_posrelat (ptrdiff_t pos, size_t len);
LUALIB_API INLINE size_t tools_optstrlen (size_t l, size_t *chunks);
LUALIB_API INLINE char   *tools_stralloc (size_t len);
LUALIB_API INLINE char   *tools_strdup (const char *s);
LUALIB_API INLINE char   *tools_strndup (const char *s, size_t l);
LUALIB_API char          *tools_strnstr (const char *s, const char *p, size_t slen);
LUALIB_API int           tools_strinalphabet (const char *p, const char *alpha, size_t plen, size_t alphalen);
LUALIB_API char          *tools_strsep (char **str, const char *delim, size_t *l);
LUALIB_API INLINE int    tools_isvowel (int c, int withy);
LUALIB_API INLINE int    tools_isconsonant (int c);
LUALIB_API INLINE int    tools_isalphadia (int c);

#define tools_strlen(s)         ((s) == NULL ? 0 : strlen(s))  /* 2.39.1 security fix */
#define tools_streq(p,q)        (strcmp((p),(q)) == 0)
#define tools_strcmp            strcmp
#define tools_strncmp           strncmp
#define tools_memchr            memchr
#define tools_memcpy            memcpy
#define tools_memcmp            memcmp
#define tools_hasstrchr(s,p,l)  ((void)l, strpbrk((s), (p)) != NULL)

LUALIB_API size_t tools_strnlen (const char *str, size_t maxlen);
LUALIB_API size_t str_strftime  (char *s, size_t maxsize, const char *format, const struct tm *timeptr, int msecs);
LUALIB_API char *str_strptime   (const char *s, const char *f, struct tm *tm, int *msecs);
LUALIB_API char *str_randname   (char *str, size_t strl, const char *alphabet, size_t alphalen, size_t l, size_t u);

LUALIB_API unsigned long tools_getthreadid (void);
LUALIB_API unsigned long tools_getposixthreadid (void);
LUALIB_API uint32_t      tools_getthreadseed (void);
LUALIB_API size_t        tools_randint (size_t l, size_t u, int genseed);

LUALIB_API double   tools_uptime (void);
LUALIB_API uint32_t tools_getcyclesperusec (void);
LUALIB_API int64_t  tools_getArmFrequencyInKHz (void);
LUALIB_API uint64_t wallclock_ns (void);

LUALIB_API char *tools_mkdtemp  (char *templ);

/* Returns non-zero if x is aligned on a "long" boundary. */
/* #define AGN_BLOCKSIZE    (sizeof(unsigned long)) */ /* string "long" word-boundary */

/* NONULL masks with the exception of ISNULL derived from:
   https://codereview.stackexchange.com/questions/231845/fast-strlen-in-c-using-scalar-bithacks,
   user S.S. Anne; the MUSL versions are not faster */
#ifdef IS32BIT
  #define NOT_HIGH_MASK   0x80808080
  #define HIGH_MASK       0x7f7f7f7f
  #define LOW_MASK        0x01010101
  #define BLOCK_T         uint32_t
#elif defined(IS64BIT)
  #define NOT_HIGH_MASK   0x8080808080808080
  #define HIGH_MASK       0x7f7f7f7f7f7f7f7f
  #define LOW_MASK        0x0101010101010101
  #define BLOCK_T         uint64_t
#else
  #error long int is not a 32bit or 64bit type
  #define ISNULL(x)       (0)
  #define NONULL(x)       (1)
#endif

#define ISNULL(x)         (((x) - LOW_MASK) & ~(x) & NOT_HIGH_MASK)
#define NONULL(x)         (!((((x) & HIGH_MASK) - LOW_MASK) & NOT_HIGH_MASK))

#define ONES              ((size_t)-1/UCHAR_MAX)
#define SIZEST            (sizeof(size_t))
#define AGN_BLOCKSIZE     (sizeof(BLOCK_T))
#define AGN_ALIGNMASK     (AGN_BLOCKSIZE - 1)

#define tools_strisaligned(x)           (((BLOCK_T)(x) & (AGN_ALIGNMASK)) == 0)  /* 2.25.5 fix */
#define tools_strisunaligned(x)         (((BLOCK_T)(x) & (AGN_ALIGNMASK)) != 0)  /* 2.25.5 fix */
#define tools_stringsarealigned(x, y)   (tools_strisaligned(x) && tools_strisaligned(y))
#define tools_largeenough(l)            ((l) >= AGN_BLOCKSIZE)
#define tools_alignablesize(l)          (((l) & AGN_ALIGNMASK) == 0)
#define tools_strisnull(x)              ISNULL(x)
#define tools_strisnonnull(x)           NONULL(x)

#if defined(__GNUC__) && defined(__INTEL)
LUALIB_API void *asm_memset (void *d, int s, size_t c);
#else
#define asm_meset   memset
#endif

/* 4-byte-alignment computation does not work without knowing the sizes of the strings, so we define a macro: */
#define tools_strneq(s,t) (!tools_streq((s),(t))) /* 2.17.8: we use the new 4-byte word aligned tools_streq C function; changed 2.25.1 */
LUALIB_API int  tools_streqx (const char *s, ...);
LUALIB_API void tools_strnrev (char *s, int n);
LUALIB_API INLINE int tools_isdigit (int x, int base);
LUALIB_API INLINE const char *tools_lmemfind (const char *s1, size_t l1, const char *s2, size_t l2);
/* for pattern-matching facility see tools_strmatch in lstrlib.h */

LUALIB_API uint32_t *tools_strtouint32s (const char *src, size_t l, size_t *chunks, int tobigendian);
#ifdef IS32BIT
LUALIB_API uint32_t tools_strtouint32 (const char *src, size_t l, int *rc, int tobigendian);
#else
LUALIB_API uint32_t tools_strtouint32 (const char *src, size_t l, int *rc, uint32_t *low, int tobigendian);
#endif

LUALIB_API LUA_INTEGER lua_unpackint (const char *str, int islittle, int size, int issigned, int *rc);

/* miscellaneous */
LUALIB_API char *tools_getdirname (char *path);
LUALIB_API char *tools_getline (FILE *fp, char *buf, char *line, size_t *linesize, int buffersize, size_t *maxbufsize, int *r) ;

/* ISO 8859/1 Latin-1 alphabetic and upper and lower case bit vector tables.

   Taken from the entropy utility ENT written by John Walker, January 28th, 2008,
   Fourmilab, http://www.fourmilab.ch.

   Sources available are at https://github.com/Fourmilab/ent_random_sequence_tester

   This software is in the public domain. Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose and without fee is hereby granted, without any conditions or
   restrictions. This software is provided as is without express or implied warranty. */

extern unsigned char isoalpha[32], isoupper[32], isolower[32];  /* 2.25.0 extern declaration to work on MinGW/GCC 10.x */

#define isISOspace(x)  ((isascii((uchar (x))) && isspace((uchar (x)))) || ((x) == 0xA0))
#define isISOalpha(x)  ((isoalpha[((uchar (x))) / 8] & (0x80 >> (((uchar (x))) % 8))) != 0)
#define isISOupper(x)  ((isoupper[((uchar (x))) / 8] & (0x80 >> (((uchar (x))) % 8))) != 0)
#define isISOlower(x)  ((isolower[((uchar (x))) / 8] & (0x80 >> (((uchar (x))) % 8))) != 0)
#define isISOprint(x)  ((((x) >= ' ') && ((x) <= '~')) || ((x) >= 0xA0))
#define toISOupper(x)  (isISOlower(x) ? (isascii((uchar (x))) ?  \
                            toupper(x) : ((((uchar (x)) != 0xDF) && \
                            ((uchar (x)) != 0xFF)) ? \
                       ((uchar (x)) - 0x20) : (x))) : (x))
#define toISOlower(x)  (isISOupper(x) ? (isascii((uchar (x))) ?  \
                            tolower(x) : ((uchar (x)) + 0x20)) \
                      : (x))

/* miscellaneous and OS functions */
LUALIB_API char *tools_getenv (const char *name);
LUALIB_API int tools_cwd (char *buffer);
/* get keystroke function, does not work in Haiku */
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__OS2__)
LUALIB_API int kbhit (void);
#ifndef __OS2__  /* getch works in eCS */
LUALIB_API int getch (void);
#endif
#endif
LUALIB_API int tools_anykey (void);  /* 2.12.4 */

/* 7.3.5 change to prevent potential syntax errors in more complex logical structures: */
#define xfree(v) do { free(v); (v) = NULL; } while (0)

/* Taken from `21st Century C`, 2nd edition, O'Reilly, p. 182 of German translation, vectorise.c source file,
   by Ben Klemens; modified; needs stdlib.h for free function */
#define Fn_freeall(type, fn, ...) {                                   \
  int i;                                                              \
  void *stopper_for_apply = (int[]){0};                               \
  type **list_for_apply = (type*[]){__VA_ARGS__, stopper_for_apply};  \
  for (i=0; list_for_apply[i] != stopper_for_apply; i++) {            \
    fn(list_for_apply[i]);                                            \
    list_for_apply[i] = NULL;                                         \
  }                                                                   \
}

#define xfreeall(...) Fn_freeall(void, free, __VA_ARGS__);

/* https://en.wikipedia.org/wiki/Binary_search_algorithm#Implementation_issues */
#define tools_midpoint(l,r) (l + ((r - l) >> 1))

LUALIB_API int tools_isintenum (int x, const int *a, size_t n);
LUALIB_API int tools_isinushortarray (unsigned short int x, const unsigned short int *a, size_t n);

#ifdef _WIN32
#define xsprintf(buf,fmt,x)  __mingw_sprintf((buf), (fmt), (x))
#else
#define xsprintf(buf,fmt,x)  sprintf((buf), (fmt), (x))
#endif

/* Endian functions */

LUALIB_API char     tools_endian (void);
LUALIB_API double   tools_tobigendian (double x);

#if BYTE_ORDER != BIG_ENDIAN
/* on Little Endian, apply tools_swapdouble only on the uint64_t representation of a double, not a double itself ! */
#if __GNUC_PREREQ(4, 3)
#define tools_swapu32   __builtin_bswap32
#define tools_swapu64   __builtin_bswap64   /* requires a uint64_t argument and returns a uint64_t, not a double; 2.17.1 change */
#else
#define tools_swapu32(x) \
  (  (((x) & 0xff000000ul) >> 24) \
   | (((x) & 0x00ff0000ul) >>  8) \
   | (((x) & 0x0000ff00ul) <<  8) \
   | (((x) & 0x000000fful) << 24))

#define tools_swapu64(x)                 \
  (  (((x) & 0xff00000000000000ull) >> 56) \
   | (((x) & 0x00ff000000000000ull) >> 40) \
   | (((x) & 0x0000ff0000000000ull) >> 24) \
   | (((x) & 0x000000ff00000000ull) >> 8)  \
   | (((x) & 0x00000000ff000000ull) << 8)  \
   | (((x) & 0x0000000000ff0000ull) << 24) \
   | (((x) & 0x000000000000ff00ull) << 40) \
   | (((x) & 0x00000000000000ffull) << 56))
#endif
#define tools_unswapu32  tools_swapu32
#define tools_unswapu64  tools_swapu64  /* 2.17.1 change */
#define tools_tolittleendian(d) (d)     /* Little Endian, do nothing; 2.17.1 change */
#define tools_ftok(inode,device,id) ((inode & 0xffff) | ((device & 0xff) << 16) | ((id & 0xffu) << 24))  /* System V Inter Process Communications key */

#else  /* big endian */
LUALIB_API double   tools_tolittleendian (double x);
LUALIB_API uint64_t tools_swapdouble (double n);
LUALIB_API double   tools_unswapdouble (uint64_t n);
LUALIB_API float    tools_unswapfloat (uint32_t a);
#endif  /* of BYTE_ORDER != BIG_ENDIAN */

#if BYTE_ORDER != BIG_ENDIAN
#define tools_swapint32(d) ((int32_t)(d))  /* used by xbase.DBFWriteAttribute, DBFReadAttribute (binary I type) */
#define tools_swapfloat(d) ((float)(d))
#else
LUALIB_API int32_t  tools_swapint32 (int32_t d);
LUALIB_API float    tools_swapfloat (float a);
#endif

LUALIB_API uint16_t tools_swapuint16 (uint16_t d);
LUALIB_API int16_t  tools_swapint16 (int16_t d);
LUALIB_API uint32_t tools_swapuint32 (uint32_t d);
LUALIB_API void     tools_swapint32_t (int32_t *n);
LUALIB_API void     tools_swapuint32_t (uint32_t *n);
LUALIB_API void     tools_swapint16_t (int16_t *n);
LUALIB_API void     tools_swapuint16_t (uint16_t *n);
LUALIB_API void     tools_swapint64_t (int64_t *n);
LUALIB_API void     tools_swapuint64_t (uint64_t *n);

LUALIB_API uint32_t tools_swaplower32 (uint32_t x, unsigned int n);
LUALIB_API uint64_t tools_swaplower64 (uint64_t x, unsigned int n);
LUALIB_API uint32_t tools_swapupper32 (uint32_t x, unsigned int n);
LUALIB_API uint64_t tools_swapupper64 (uint64_t x, unsigned int n);


/* taken from: https://stackoverflow.com/questions/48737056/bit-cast-uint64-t-to-double-and-back-in-a-macro
   3.5 % faster than the original iterative casting functions; as fast as "*((uint64_t*)(void*)&x" or
   uint64_t u;
   double x = ...;
   memcpy(&u, &x, sizeof(u)); */
#define tools_doubletouint64(x) ((union { double d; uint64_t u; }) { x } .u)
#define tools_uint64todouble(x) ((union { uint64_t u; double d; }) { x } .d)
#define tools_doubletouint32(x) ((union { float d; uint32_t u; }) { x } .u)
#define tools_uint32todouble(x) ((union { uint32_t u; float d; }) { x } .d)
/* 2.16.6 fix */
LUALIB_API uint64_t tools_doubletouint64andswap (double a);
LUALIB_API uint64_t tools_twouint32touint64 (uint32_t d, uint32_t e);
LUALIB_API double   tools_uint64todoubleandswap (uint64_t a);
LUALIB_API uint64_t tools_twoint32touint64 (int32_t d, int32_t e);
LUALIB_API int32_t  tools_uint64toint32 (uint64_t d, char k);
LUALIB_API uint32_t tools_uint64touint32 (uint64_t d, uint32_t *low);  /* 2.25.5 */
#if BYTE_ORDER != BIG_ENDIAN
#define tools_sint2double(a)   ((double)((int32_t)(a)))   /* used by xbase.DBFReadAttribute */
#define tools_ushort2double(a) ((double)((uint16_t)(a)))  /* dito */
#define tools_short2double(a)  ((double)((int16_t)(a)))   /* dito */
#else
LUALIB_API double   tools_sint2double (int32_t a);
LUALIB_API double   tools_ushort2double (uint16_t a);
LUALIB_API double   tools_short2double (int16_t a);
#endif

/* IEEE simulators */

LUALIB_API INLINE int tools_isfinite (double x);
LUALIB_API INLINE int tools_isinf (double x);
LUALIB_API INLINE int tools_isinfx (double x, int *isnegative);
LUALIB_API INLINE int tools_isnan (double x);
LUALIB_API INLINE int tools_isnanorinf (double x);
LUALIB_API     double tools_nan (void);
/* Modern GCC compilers crash with complex doubles given to isnan(). So check the real and imaginary parts individually: */
#ifndef PROPCMPLX  /* 7.1.7 */
#define tools_isnanc(z) (( luai_numisnan(creal(z)) || luai_numisnan(cimag(z)) ))
#endif

#ifndef __ARMCPU
LUALIB_API INLINE int tools_fpiszerol (long double e);
LUALIB_API INLINE int tools_fpissubnormall (long double e);
LUALIB_API INLINE int tools_fpisinfl (long double e);
LUALIB_API INLINE int tools_fpisnanl (long double e);
LUALIB_API INLINE int tools_fpisnormall (long double e);
LUALIB_API INLINE int tools_fpisfinitel (long double e);
#else
#define tools_fpiszerol(e)       (e == 0.0L)
#define tools_fpissubnormall(e)  (fpclassify(e) == FP_SUBNORMAL)
#define tools_fpisinfl           isinf
#define tools_fpisnanl           isnan
#define tools_fpisnormall        isnormal
#define tools_fpisfinitel        isfinite
#endif

/* numerical functions */

LUALIB_API double   cephes_powi (double x, int nn);
#define cephes_intpow cephes_powi
LUALIB_API double   tools_intpow (double x, int n);  /* use cephes_powi instead, it is eight percent faster */
LUALIB_API double   tools_intpowmod (int x, int n, int m);
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API long double tools_intpowl (long double x, int n);
#endif
LUALIB_API float    tools_pow2fast (float x);
LUALIB_API double   tools_nextpower (double x, uint32_t b, int orequalx);
LUALIB_API double   tools_nextpow2 (uint32_t x);
LUALIB_API uint32_t tools_nextpow2u32 (uint32_t x);
LUALIB_API uint64_t tools_nextpow2u64 (uint64_t n);
LUALIB_API double   tools_prevpower (double x, uint32_t b, int orequalx);
LUALIB_API double   tools_prevpow2 (uint32_t x);
LUALIB_API int      tools_approx (double x, double y, double eps);
#define tools_capprox(xr,xi,yr,yi,eps) (tools_approx(xr, xi, eps) && tools_approx(yr, yi, eps))
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API int      tools_approxl (long double x, long double y, long double eps);
#endif
LUALIB_API INLINE double tools_csgn (double a, double b);
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API INLINE long double tools_csgnl (long double a, long double b);
#endif
LUALIB_API double   tools_roundf (double x, int d, int t);
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API long double sun_ceill (long double x);
LUALIB_API long double sun_floorl (long double x);
LUALIB_API long double tools_roundfl (long double x, int d, int t);
#endif
LUALIB_API double   tools_fdim (double x, double y);
LUALIB_API int      tools_signbit (double x);
LUALIB_API INLINE double tools_sign (double x);

/* returns 1 if x = 1, and -1 if x = 0, without branching;
   taken from: https://stackoverflow.com/questions/44066748/fastest-way-to-compute-the-opposite-of-the-sign-of-a-number-in-c */
#define tools_bool2pm(x)  (((x) >> (sizeof(x) * CHAR_BIT - 1)) | 1)

LUALIB_API INLINE double tools_signum (double x);
LUALIB_API long double tools_powl (long double x, long double y);
LUALIB_API long double tools_powil (long double x, int nn);
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API long double tools_signl (long double x);
LUALIB_API long double sun_frexpl (long double x, int *e);
LUALIB_API long double sun_fmodl (long double x, long double y);
LUALIB_API long double tools_expl (long double x);
LUALIB_API long double tools_expm1l (long double x);
LUALIB_API long double sun_scalbnl (long double x, int n);
LUALIB_API INLINE long double tools_signuml (long double x);
LUALIB_API long double sun_nextafterl (long double x, long double y);
LUALIB_API long double sun_copysignl (long double x, long double y);
#endif
LUALIB_API INLINE int tools_gammasign (double x);
LUALIB_API INLINE int tools_gammasignl (long double x);
LUALIB_API double tools_mulsign (double x, double y);
LUALIB_API double tools_fma (double a, double b, double c);
LUALIB_API double tools_mhypot (double a, double b);
LUALIB_API double tools_mpytha (double x, double y);
LUALIB_API long double tools_hypotl (long double x, long double y);
LUALIB_API long double tools_mhypotl (long double a, long double b);
LUALIB_API INLINE double sun_hypot (double x, double y);
LUALIB_API double   sun_hypot2 (double y);
LUALIB_API double   sun_hypot3 (double y);
LUALIB_API double   sun_pytha (double x, double y);
LUALIB_API void tools_squarel (long double *hi, long double *lo, long double x);
LUALIB_API long double tools_pythal (long double x, long double y);
LUALIB_API long double tools_mpythal (long double x, long double y);
LUALIB_API int      tools_uintlog2 (uint32_t v);
LUALIB_API int      tools_uintlog2_64 (uint64_t v);
LUALIB_API int      tools_uintlog10_64 (uint64_t x);

LUALIB_API float    tools_lbfast (float x);
LUALIB_API double   sun_nextafter (double x, double y);
LUALIB_API int      sun_frexp_exp (double x);
LUALIB_API double   sun_frexp_man (double x);
LUALIB_API double   sun_xfrexp (double x, int *eptr, int *isneg);
LUALIB_API INLINE int32_t sun_exponent (double x);
LUALIB_API INLINE int32_t sun_uexponent (double x);
LUALIB_API INLINE int sun_isirregular (double x);
LUALIB_API int tools_isirregularl (long double e);
LUALIB_API INLINE int tools_isregint (double x);
LUALIB_API INLINE int tools_isregintx (double x, int32_t *ux);
LUALIB_API INLINE int tools_isregnonnegint (double x);
LUALIB_API INLINE int tools_isregnonnegintx (double x, int32_t *ux);
LUALIB_API int        sun_isnormal (double x);
LUALIB_API INLINE int sun_issubnormal (double x);
LUALIB_API INLINE int sun_isexceptional (double x);
LUALIB_API INLINE int tools_ispow2 (double x);
#define tools_ispow2_uint32(x) ({ \
  int64_t y = x; \
  (!(!y) & !(y & (y - 1))); \
})
LUALIB_API double   sun_normalise (double x, uint32_t *high);
LUALIB_API double   tools_zeroin (double x, double eps);
LUALIB_API long double tools_zeroinl (long double x, long double eps);
LUALIB_API double   tools_redupi (double x);
LUALIB_API long double tools_redupil (long double x);
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API long double tools_fabsl (long double x);
LUALIB_API long double tools_fmal (long double x, long double y, long double z);
#endif
#define tools_negatewhen(x,c) ((!(c)^(!(c)-1))*(x))  /* negates x if c = 0 and returns x unchanged if c = 1; a lttle bit faster than a double_cast union solution */
#define tools_chop(x)  (sun_issubnormal(x) ? ((signbit(x)) ? -0.0 : 0.0) : x)
#define tools_chopl(x) ((tools_fpissubnormall(x)) ? ((signbit(x)) ? -0.0L : 0.0L) : x)
#ifndef __ARMCPU  /* 2.37.1 */
#define sun_roundl(x)  (tools_roundfl(x, 4, 0))
#endif
LUALIB_API int tools_ndigplaces (double x, int *overflow);
/* LUALIB_API double   sun_fmod (double x, double y); */  /* 3 percent slower than GCC's implementation */
LUALIB_API double      sun_modf (double x, double *iptr);  /* 20 percent faster than GCC's implementation */
#ifndef __ARMCPU
LUALIB_API long double sun_modfl (long double x, long double *iptr);
LUALIB_API long double sun_intl (long double x);
LUALIB_API long double sun_fracl (long double x);
LUALIB_API int         sun_isintl (long double x);
LUALIB_API int         sun_isfloatl (long double x);
#endif
LUALIB_API double   sun_frac (double x);            /* 3 percent faster than x - trunc(x) macro */
LUALIB_API double   sun_cbrt (double x);            /* 15 percent faster than GCC's implementation */
LUALIB_API long double sun_sinl (long double x);
LUALIB_API long double sun_cosl (long double x);
LUALIB_API long double sun_tanl (long double x);
LUALIB_API void sun_sincosl (long double x, long double *sin, long double *cos);
LUALIB_API long double tools_sinhl (long double x);  /* MUSL version is slightly slower than the one in GCC */
LUALIB_API long double tools_coshl (long double x);  /* MUSL version is slightly slower than the one in GCC */
LUALIB_API long double tools_tanhl (long double x);  /* MUSL version is slightly slower than the one in GCC */
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API long double tools_sqrtl (long double x);
LUALIB_API long double tools_cbrtl (long double x);
LUALIB_API long double sun_asinl (long double x);
LUALIB_API long double sun_acosl (long double x);
LUALIB_API long double sun_atanl (long double x);
LUALIB_API long double sun_atan2l (long double y, long double x);
#endif
LUALIB_API double   sun_sin (double x);             /* 30 percent faster than GCC's implementation */
LUALIB_API double   sun_cos (double x);             /* 30 percent faster than GCC's implementation */
LUALIB_API double   sun_tan (double x);             /* eight percent faster than GCC's implementation */
LUALIB_API double   sun_asin (double x);            /* as fast as GCC's asin implementation */
LUALIB_API double   sun_acos (double x);            /* as fast as GCC's acos implementation */
LUALIB_API double   sun_atan (double x);            /* 45 percent faster than GCC's implementation */
LUALIB_API double   sun_atan2 (double y, double x); /* 20 percent faster than GCC's implementation */
LUALIB_API double   sun_sinh (double x);            /* 45 percent faster than GCC's implementation */
LUALIB_API double   sun_cosh (double x);            /* 45 percent faster than GCC's implementation */
LUALIB_API void     sun_sinhcosh (double x, double *sih, double *coh);  /* 12 % slower than tools_sinhcosh */
LUALIB_API double   sun_tanh (double x);            /* 55 percent faster than GCC's implementation */
LUALIB_API int      sun_rem_pio2 (double x, double *y);
LUALIB_API double   sun_asinh (double x);           /* 15 % faster than GCC's asinh */
LUALIB_API double   sun_acosh (double x);           /* 8 % slower than GCC's acosh */
LUALIB_API double   sun_atanh (double x);           /* 15 % faster than GCC's implementation */
LUALIB_API long double sun_asinhl (long double x);  /* 2 % faster than GCC's asinhl */
LUALIB_API long double sun_acoshl (long double x);  /* 2 % slower [sic !] than GCC's asinhl */
LUALIB_API long double sun_atanhl (long double x);  /* 21 percent slower [sic !] than GCC's atanhl */
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API int      sun_rem_pio2l (long double x, long double *y);
#endif
LUALIB_API INLINE double sun_pow (double x, double y, int optimised);   /* 1.3 times faster than GCC's implementation, in some cases it is slower */
LUALIB_API INLINE long double safe_sqrtl (long double x);
LUALIB_API INLINE double sun_log (double x);             /* 20 percent faster than GCC's implementation */
LUALIB_API INLINE double sun_log2 (double x);            /* 2.5 percent slower than GCC's log2 */
LUALIB_API long double tools_logl (long double x);
LUALIB_API long double tools_log2l (long double x);
LUALIB_API long double tools_log10l (long double x);
LUALIB_API INLINE long double tools_logbasel (long double x, long double b);
LUALIB_API long double tools_log1pl (long double xm1);   /* MUSL's version is 4 % slower than the one available in GCC */
LUALIB_API double   tools_logbase (double x, double b);  /* 2.14.2 */
LUALIB_API double   sun_log1p (double x);           /* DON'T USE IT: On AMD Ryzen 5 5600H, the function is 18 % slower than GCC's log1p. */
LUALIB_API double   sun_exp (double x);             /* 60 percent faster than GCC's implementation */
/* LUALIB_API double   sun_exp2 (double x); */      /* x percent faster than GCC's implementation */
/* LUALIB_API double   sun_exp10 (double x); */     /* x percent faster than GCC's implementation */

#ifndef __ARMCPU  /* 2.37.1 */
#define sun_exp2(x)  (sun_exp(LN2*x))
#define sun_exp10(x) (sun_exp(LN10*x))
#else  /* I do not understand why the defines to not compile any longermon Raspberry Pi */
/* #define sun_exp2(x)  (sun_exp(LN2*x)) */
/* #define sun_exp10(x) (sun_exp(LN10*x)) */
LUALIB_API double sun_exp2 (double x);
LUALIB_API double sun_exp10 (double x);
#endif

LUALIB_API double   tools_exp2 (double x);
LUALIB_API double   tools_exp10 (double x);

LUALIB_API long double tools_exp2l (long double x);
LUALIB_API long double tools_exp10l (long double x);
LUALIB_API double   tools_exp2_9 (double x);
LUALIB_API double   tools_exp10_9 (double x);
LUALIB_API double   sun_expm1 (double x);           /* twice as fast as GCC's implementation */
LUALIB_API double   sun_log2 (double x);            /* seven percent slower than GCC's log2 implementation */
LUALIB_API double   sun_log10 (double x);           /* four percent faster than GCC's log10 implementation */
LUALIB_API double   sun_ilogb (double x);           /* three percent faster than GCC's ilogb implementation */
LUALIB_API double   sun_ilog10 (double x);
LUALIB_API long double   tools_ilogbl (long double x);
LUALIB_API double   sun_fabs (double x);                /* as fast as GCC's fabs */
LUALIB_API double   sun_ldexp (double value, int exp);  /* 15 to 45 percent faster than GCC's ldexp */
#define sun_scalbn  sun_ldexp
LUALIB_API double   sun_frexp (double x, int *eptr);    /* 16 percent faster than GCC's ldexp */
LUALIB_API double   sun_lgamma (double x);
LUALIB_API double   sun_lgamma_r (double x, int *signgamp);
LUALIB_API double   tools_gamma (double x);
LUALIB_API double   tools_upperincompletegamma (double a, double x);
LUALIB_API double   tools_lowerincompletegamma (double a, double x);
LUALIB_API double   slm_rgamma (double x);
LUALIB_API long double tools_gammal (long double x);
LUALIB_API long double tools_lgammal (long double x);
LUALIB_API long double tools_lgammal_r (long double x, int *sg);
LUALIB_API double tools_digamma (double x);  /* equals Psi function */
LUALIB_API long double tools_psil (long double x);  /* equals Digamma function */
LUALIB_API double tools_trigamma (double x);
LUALIB_API long double tools_trigammal (long double x);
LUALIB_API double tools_tetragamma (double x);
LUALIB_API long double tools_tetragammal (long double x);
LUALIB_API long double tools_pentagammal (long double x);
LUALIB_API double   tools_beta (double z, double w);
LUALIB_API double   tools_lnbeta (double z, double w);
LUALIB_API INLINE double tools_ei (double x);
LUALIB_API double   tools_ei_taylor (int n, double x);
LUALIB_API double   cephes_factorial (int i);
LUALIB_API long double cephes_factoriall (int i);
LUALIB_API INLINE double tools_lnfactorial (unsigned int n);
LUALIB_API INLINE long double tools_lnfactoriall (unsigned int n);
LUALIB_API double tools_lnbinomial (double n, double k);
LUALIB_API long double tools_binomiall (long double n, long double k);
LUALIB_API long double tools_lnbinomiall (long double n, long double k);
#define TOOLS_NFACTORIALL 171
LUALIB_API double   sun_j0 (double x);              /* 15 % slower */
LUALIB_API double   sun_y0 (double x);
LUALIB_API double   sun_j1 (double x);
LUALIB_API double   sun_y1 (double x);
LUALIB_API double   sun_jn (int n, double x);
LUALIB_API double   sun_yn (int n, double x);
LUALIB_API double   sun_erf (double x);             /* as fast as GCC's erf implementation */
LUALIB_API double   sun_erfc (double x);            /* 13% faster than GCC's erfc implementation */
LUALIB_API double   sun_sqrt (double x);            /* DO NOT USE: seven times slower than GCC's sqrt implementation which may just directly call the CPU's built-in sqrt */
LUALIB_API double tools_isqrt (double x);
LUALIB_API double tools_invsqrt (double x);
LUALIB_API double tools_invhypot (double x, double y);
LUALIB_API double tools_invpytha (double x, double y);
LUALIB_API long double sun_erfl (long double x);    /* GCC for OS/2 does not have erfl */
LUALIB_API long double sun_erfcl (long double x);   /* GCC for OS/2 does not have erfcl */
LUALIB_API double   sun_ceil (double x);            /* 14 percent faster than GCC's ceil implementation */
LUALIB_API double   sun_floor (double x);           /* 12 percent faster than GCC's floor implementation */
LUALIB_API double   sun_trunc (double x);
LUALIB_API double   sun_round (double x);           /* 65 percent faster when rounding to an integer */
/* LUALIB_API double   sun_rint (double x); */      /* 9 % slower than GCC's rint */
LUALIB_API double   sun_intdiv (double x, double y);
LUALIB_API double   sun_iqr (double x, double y, double *remainder);
LUALIB_API double   sun_remquo (double x, double y, int *quo);  /* not faster than sun_iqr, but used for testlib ref tests */
LUALIB_API double   sun_quotient (double x, double y);   /* 10 % slower than sun_intdiv */
LUALIB_API double   sun_remainder (double x, double p);  /* 33 % slower */
LUALIB_API double   sun_fmod (double x, double y);
LUALIB_API int      tools_fpclassify (double x);
LUALIB_API int      tools_fpclassifyl (long double e);
#ifndef __ARMCPU
LUALIB_API long double tools_mathepsl (long double x);
LUALIB_API long double tools_cbrtepsl (long double x);
#else
#define tools_mathepsl    tools_matheps
#define tools_cbrtepsl    tools_cbrteps
#endif
LUALIB_API long double tools_reducerangel (long double x, long double min, long double max);
LUALIB_API INLINE double tools_reducerange (double x, double min, double max);
LUALIB_API double   tools_branch (double x, int d);
LUALIB_API double   tools_matheps (double x);
LUALIB_API double   tools_cbrteps (double x);
LUALIB_API double   tools_binomial (double n, double k);
LUALIB_API double   tools_expx2 (double x, int sign);
LUALIB_API double   tools_cexpx2 (double a, double b, double sign, double *im);
LUALIB_API void     sun_sincos (double x, double *s, double *c);  /* 35 % faster than calling sin and cos individually */
LUALIB_API void     tools_sincospi (double a, double *sp, double *cp, double *tp, int gettangent);
LUALIB_API double   tools_relerr (double a, double b);
LUALIB_API double   tools_erfcx (double x);
LUALIB_API long double tools_erfcxl (long double x);
LUALIB_API double   tools_inverf (double x);
#ifndef __ARMCPU
LUALIB_API long double tools_inverfl (long double x);
LUALIB_API long double tools_inverfcl (long double x);
#else
#define tools_inverfl(x)  (-tools_inverfc(x + 1))
#define tools_inverfcl    tools_inverfc
#endif
LUALIB_API double   tools_inverfc (double y);
LUALIB_API double   tools_w_im (double x);

#define tools_clip(x,a,b) (fMin(fMax((a),(x)),(b)))  /* too lazy to write special functions for each C type, XXX can be optimised */

LUALIB_API double tools_envj (double n, double x);
LUALIB_API int    tools_msta1 (double x, double mp);
LUALIB_API int    tools_msta2 (double x, double n, double mp);
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API long double tools_envjl (long double n, long double x);
LUALIB_API int    tools_msta1l (long double x, long double mp);
LUALIB_API int    tools_msta2l (long double x, long double n, long double mp);
#endif

#ifndef PROPCMPLX
LUALIB_API double tools_cabs (agn_Complex z);
LUALIB_API agn_Complex tools_cdiv (agn_Complex x, agn_Complex y);
LUALIB_API agn_Complex tools_crecip (agn_Complex x);
LUALIB_API agn_Complex tools_csqrt (agn_Complex z);
LUALIB_API agn_Complex slm_csqrt (agn_Complex z);
LUALIB_API agn_Complex slm_crsqrt (agn_Complex z);
LUALIB_API agn_Complex tools_cpow (agn_Complex a, agn_Complex z);  /* 13 percent faster than GCC's cpow implementation */
LUALIB_API agn_Complex tools_cexp (agn_Complex z);
LUALIB_API agn_Complex tools_clog (agn_Complex z);   /* 25 percent faster than GCC's clog */
#define tools_csin(z) (agnc_sin(z))  /* GCC's csin and ccos are buggy */
#define tools_ccos(z) (agnc_cos(z))  /* ditto */
LUALIB_API agn_Complex tools_casin (agn_Complex z);  /* twice as fast as the GCC implementation */
LUALIB_API agn_Complex tools_cacos (agn_Complex z);  /* twice as fast as the GCC implementation */
LUALIB_API agn_Complex tools_catan (agn_Complex z);  /* 15 percent faster than GCC's catan */
#define tools_catan2(y,x) ( ( x == 0+0*I && y == 0+0*I ) ? 0+0*I : -I*tools_clog(tools_cdiv(x + I*y, tools_csqrt(x*x + y*y))))
LUALIB_API agn_Complex tools_catanh (agn_Complex z);
LUALIB_API agn_Complex tools_ctan (agn_Complex z);   /* 25 percent faster than GCC's implementation */
LUALIB_API agn_Complex tools_ctanh (agn_Complex z);  /* 30 percent faster than GCC's ctanh */
LUALIB_API agn_Complex tools_cerfc (agn_Complex x);
LUALIB_API agn_Complex tools_cerf (agn_Complex x);
LUALIB_API agn_Complex tools_w (agn_Complex z, double relerr);
#else  /* PROPCMPLX */
LUALIB_API void   tools_crecip (double x, double y, double *re, double *im);
#define slm_csqrt slm_csqrt_reim
LUALIB_API void   slm_crsqrt (double x, double y, double *re, double *im);
LUALIB_API double tools_cabs (double a, double b);
LUALIB_API void   tools_clog (double a, double b, double *re, double *im);
LUALIB_API void   tools_casinh (double x, double y, double *outx, double *outy);
LUALIB_API void   tools_casin (double x, double y, double *outx, double *outy);
LUALIB_API void   tools_cacos (double x, double y, double *outx, double *outy);
LUALIB_API void   tools_casec (double x, double y, double *outx, double *outy);
LUALIB_API void   tools_catan2 (double a, double b, double c, double d, double *real, double *imag);
LUALIB_API void   tools_cerfc (double z_re, double z_im, double *rre, double *rim);
LUALIB_API void   tools_cerf (double z_re, double z_im, double *rre, double *rim);
typedef struct { double cmplx[2]; } PROPCMPLX_CMPLX;
LUALIB_API PROPCMPLX_CMPLX tools_w (PROPCMPLX_CMPLX z, double relerr);
#endif  /* PROPCMPLX */

LUALIB_API void slm_csqrt_reim (double x, double y, double *re, double *im);
#define tools_cgamma  cephes_cgam_reim   /* cephes_cgam_reim (double re, double im, double *rre, double *rim) */
#define tools_clgamma cephes_clgam_reim  /* cephes_clgam_reim (double re, double im, double *rre, double *rim) */
LUALIB_API void tools_cpsi (double x, double y, double *re, double *im);

LUALIB_API void            tools_sincosfast (float x, float *pS, float *pC);
LUALIB_API double          tools_sinfast (double x);
LUALIB_API double          tools_cosfast (double x);
LUALIB_API float           tools_sqrtfast (float x);
LUALIB_API float           tools_sqrtapx (float x);
LUALIB_API double          tools_numbpartapx (int n);
LUALIB_API double          tools_nextmultiple (int32_t n, int32_t b);
LUALIB_API double          tools_prevmultiple (int32_t n, int32_t b, int orequal);
LUALIB_API uint32_t        tools_adjustmultiple (uint32_t n, uint32_t b);
LUALIB_API int             tools_ismultiple (double x, double y, int inexact, double eps);
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API int             tools_ismultiplel (long double x, long double y, int inexact, long double eps);
#endif
LUALIB_API unsigned int    tools_modulo (unsigned int num, unsigned int div);  /* 2.12.0 RC 4 */
LUALIB_API long int        tools_gcd (double x, double y);  /* 2.12.3 */
LUALIB_API double          tools_koadd (volatile double s, volatile double x, volatile double *q);  /* 2.13.0 */
LUALIB_API double          tools_kbadd (volatile double s, volatile double x, volatile double *cs, volatile double *ccs);
#ifndef __ARMCPU  /* 2.37.1 */
LUALIB_API long double     tools_koaddl (volatile long double s, volatile long double x, volatile long double *q);
#endif
LUALIB_API double          tools_examul (const double a, const double b, double *q);
LUALIB_API INLINE double   tools_polyeval (double x, double coef[], int n);  /* 2.13.0, with Kahan-Ozawa round-off error prevention */
LUALIB_API INLINE double   tools_polyevals (double x, double *coeffs, int n);  /* 2.41.3, w/o any round-off error prevention */
LUALIB_API INLINE double   tools_polyevalfmas (double x, LongDoubleArray *coeffs, int n);  /* 2.35.1 */
LUALIB_API INLINE double   tools_polyevalfma (double x, LongDoubleArray *coeffs, int n);  /* 3.1.2 */

LUALIB_API INLINE int      tools_isint (double x);
LUALIB_API INLINE int      tools_isfrac (double x);
LUALIB_API INLINE int      tools_isnegint (double x);
LUALIB_API INLINE int      tools_isposint (double x);
LUALIB_API INLINE int      tools_isposintwords (int32_t hx, uint32_t lx, int32_t *ux);
LUALIB_API INLINE int      tools_isnonnegint (double x);
LUALIB_API INLINE int      tools_isnonzeroint (double x);
LUALIB_API INLINE int      tools_isnonposint (double x);
LUALIB_API INLINE int      tools_isuint (double x, int bits);
LUALIB_API INLINE int      tools_ispositive (double x);
LUALIB_API INLINE int      tools_isnonnegative (double x);
LUALIB_API INLINE int      tools_fracthalf (double x);
LUALIB_API INLINE int      tools_fracthalfwords (int32_t hx, int32_t ix, uint32_t lx);
LUALIB_API        int      tools_fracthalfx (double x, double *dptr);
LUALIB_API        int      tools_fracthalfwordsx (int32_t hx, int32_t ix, uint32_t lx, double *dptr);
LUALIB_API INLINE double   tools_random (int mode);
LUALIB_API INLINE int      tools_randomrange (int l, int u, int mode);
LUALIB_API INLINE int      tools_isevenorodd (double x);
#define tools_iseven(x)    (tools_isevenorodd(x) == 2)
#define tools_isodd(x)     (tools_isevenorodd(x) == 1)
#define tools_intiseven(x) (!(n & 1))  /* x should be of type int, tuned 2.31.5 */
#define tools_intisodd(x)  (n & 1)     /* x should be of type int, tuned 2.31.5 */

/* tools_square is 10 percent faster then sun_pow. There is no need to check for subnormal values, NaN, or +/-inf !  2.17.7 */
#define tools_square(x) ({ \
  lua_Number __squ; \
  __squ = (x); \
  (__squ*__squ); \
})

#define tools_squareld(x) ({ \
  long double __squ; \
  __squ = (x); \
  (__squ*__squ); \
})


/* See: math.adjust;
   onesided = -1: adjust x `to the left` and to the `right` of |y|.
   onesided = +1: adjust x `to the right` and to the `left` of |y|.
   onesided = 0:  adjust x if y != 0, on both sides, else leave it unchanged. */
#define tools_adjust(x, y, eps, onesided) { \
  if (tools_approx(fabs(x), (y), (eps))) { \
    if ((onesided) != 0) { \
      if (((onesided) == -1) && (((x) < -(y)) || ((x) > (y)))) { (x) = copysign((y), (x)); } \
      else                   if (((x) > -(y)) || ((x) < (y)))  { (x) = copysign((y), (x)); } \
    } else (x) = ((y) == 0) ? (y) : copysign((y), (x)); \
  } \
}

#define tools_adjustl(x, y, eps, onesided) { \
  if (tools_approxl(fabs(x), (y), (eps))) { \
    if ((onesided) != 0) { \
      if (((onesided) == -1) && (((x) < -(y)) || ((x) > (y)))) { (x) = sun_copysignl((y), (x)); } \
      else                   if (((x) > -(y)) || ((x) < (y)))  { (x) = sun_copysignl((y), (x)); } \
    } else (x) = ((y) == 0.0L) ? (y) : sun_copysignl((y), (x)); \
  } \
}


#ifdef __ARMCPU
#define tools_fabsl        sun_fabs
#define tools_fmal         fma
#define sun_copysignl      copysign
#define sun_frexpl         frexp
#define sun_modfl          modfl  /* do not use sun_modf, it won't compile */
#define sun_fmodl          sun_fmod
#define tools_expm1l       sun_expm1
#define tools_signl        tools_sign
#define tools_csgnl        tools_csgn
#define tools_approxl      tools_approx
#define tools_expl         sun_exp
#define tools_sqrtl        sqrt
#define tools_cbrtl        sun_cbrt
#define truncl             sun_trunc
#define tools_roundfl      tools_roundf
#define sun_ceill          sun_ceil
#define sun_floorl         sun_floor
#define tools_koaddl       tools_koadd
#define sun_atan2l         atan2
#define tools_signuml      tools_signum
#define sun_scalbnl        sun_scalbn
#define sun_intl           luai_numint
#define sun_fracl          sun_frac
#define sun_isintl         tools_isint
#define sun_isfloatl       tools_isfrac
#define sun_nextafterl     sun_nextafter
#define sun_sinl           sun_sin
#define sun_cosl           sun_cos
#define sun_tanl           sun_tan
#define sun_asinl          sun_asin
#define sun_acosl          sun_acos
#define sun_atanl          sun_atan
#define tools_msta1l       tools_msta1
#define tools_msta2l       tools_msta2
#define sun_roundl         sun_round
#define tools_ilogb        sun_ilogb
#define sun_rem_pio2l      sun_rem_pio2
#define tools_intpowl      tools_intpow
#define tools_ismultiplel  tools_ismultiple
#define tools_signuml      tools_signum
#endif  /* of __ARMCPU */

#ifndef PROPCMPLX
LUALIB_API agn_Complex tools_centier (agn_Complex a);
#else
LUALIB_API void        tools_centier (double a, double b, double *x, double *y);
#endif

/* setting and getting bits in an integer */

LUALIB_API INLINE void     tools_setbit (int *x, int pos, int bit);
LUALIB_API INLINE int      tools_getbit (int x, int pos);
LUALIB_API INLINE int      tools_getuint32bit (uint32_t x, int pos);
LUALIB_API INLINE int      tools_getint32bit (int32_t x, int pos);
LUALIB_API INLINE int      tools_getuint64bit (uint64_t x, int pos);
LUALIB_API INLINE int      tools_setuint64bit (uint64_t *n, int pos, int value);
LUALIB_API INLINE int      tools_getint64bit (int64_t x, int pos);
LUALIB_API INLINE int      tools_setint64bit (int64_t *n, int pos, int value);
LUALIB_API INLINE uint32_t tools_onebits32 (register uint32_t x);
LUALIB_API INLINE uint64_t tools_onebitsu64 (register uint64_t x);
LUALIB_API INLINE uint32_t tools_clz32 (register uint32_t x, uint32_t *y);
LUALIB_API INLINE uint64_t tools_clzu64 (register uint64_t x, uint64_t *y);
LUALIB_API INLINE uint32_t tools_ctz32 (register uint32_t x, uint32_t *y);
LUALIB_API INLINE uint64_t tools_ctzu64 (register uint64_t x, uint64_t *y);

#ifndef __GNUC__
LUALIB_API INLINE int      tools_ctz (register uint32_t x);
#else
#define tools_ctz   __builtin_ctz
#endif
LUALIB_API INLINE uint8_t  tools_msb32 (register uint32_t x);
LUALIB_API INLINE uint8_t  tools_msb64 (register uint64_t x);
LUALIB_API INLINE uint8_t  tools_lsb32 (register uint32_t x);
LUALIB_API INLINE uint8_t  tools_lsb64 (register uint64_t x);
LUALIB_API INLINE uint32_t tools_uarshift32 (uint32_t x, int32_t n);
LUALIB_API INLINE uint32_t tools_rotl32 (uint32_t n, unsigned int c);
LUALIB_API INLINE uint32_t tools_rotr32 (uint32_t n, unsigned int c);
LUALIB_API INLINE int32_t  tools_slo32 (int32_t m, char n);
LUALIB_API INLINE int32_t  tools_sar32 (int32_t m, char n);

/* computes the number of iterations from [start, stop], with a step size which might be fractional, 3.7.7 */
#define tools_numiters(start,stop,step) (sun_trunc(fabs(stop - start)/step) + 1)

#if defined(_WIN32) && !(defined(ffs))
#define ffs tools_lsb  /* 2.17.8 extension, MinGW does not have ffs, find first bit set, http://man7.org/linux/man-pages/man3/ffs.3.html */
#endif
LUALIB_API INLINE uint32_t tools_fibmod (uint32_t k, uint32_t m, int exact);  /* 2.14.8 */
LUALIB_API INLINE uint32_t tools_murmurhash3 (const void *key, uint32_t key_length_in_bytes, uint32_t seed);  /* 3.13.2 */
LUALIB_API uint32_t tools_crc32 (const char *buf, size_t len, uint32_t crc);  /* 7.7.9 */
/* In tools_md5(), `len` is the size of the input string. `output` should be "unsigned char str[32 + 1]". */
LUALIB_API void tools_md5 (char *input, size_t len, unsigned char *output);  /* 7.7.10 */
LUALIB_API unsigned int lua_hashnum (double x);  /* 7.7.11 */
LUALIB_API INLINE uint32_t tools_ntohl (uint32_t net32);
LUALIB_API INLINE float tools_ntohf (uint32_t net32);
LUALIB_API INLINE uint16_t tools_htons (uint16_t net16);
LUALIB_API INLINE uint32_t tools_htonl (uint32_t net32);
LUALIB_API INLINE uint32_t tools_htonf (float f);
LUALIB_API INLINE uint16_t tools_ntohs (uint16_t net16);
LUALIB_API INLINE uint16_t tools_rfc1071 (void *buf, size_t size);  /* 3.17.3 */
LUALIB_API unsigned long tools_cksum (const unsigned char *b, size_t n);  /* 2.22.1 */

/* for INTS only ! 2.17.8
   Idea taken from https://codereview.stackexchange.com/questions/231845/fast-strlen-in-c-using-scalar-bithacks, user S.S. Anne */
#define mul2(x)  ((x) << 1)
#define mul4(x)  ((x) << 2)
#define mul8(x)  ((x) << 3)
#define mul16(x) ((x) << 4)
#define div2(x)  ((x) >> 1)
#define div4(x)  ((x) >> 2)
#define div8(x)  ((x) >> 3)
#define div16(x) ((x) >> 4)

#define dsign(x)      ( ((x) == 0) ? 0 : (((x) < 0) ? -1 : 1) )

#define Xor(x,y)      ( !(x) != !(y) )  /* logical XOR operation, makes sure that both operands will always be evaluated */

#define tools_isleapyear(y) ( ((y) % 4 == 0 && !((y) % 100 == 0)) || ((y) % 400 == 0) )

#define tools_exists(x) (access((x), 00|04) != -1)  /* 00 = file/dir exists, 04 = is readable */

LUALIB_API int tools_checkdatetime (int year, int month, int day, int hour, int minute, int second, int msecond);
LUALIB_API Time64_T tools_maketime (int year, int month, int day, int hour, int minute, int second, int *rc);

/* 2.25.5, ftime & stime have been deprecated with GLIBC 2.31. (GLIBC, not GCC !) */

#if LINUX_NOFSTIME
LUALIB_API int tools_stime (const time_t *t);
LUALIB_API int tools_ftime (struct timeb *tb);
#else
#define tools_stime stime
#define tools_ftime ftime
#endif


LUALIB_API double tools_esd (int y, int m, int d, int hh, int mm, int ss);
LUALIB_API char   *tools_dtoa (double x);
LUA_API    int    tools_jd2cdate (double x, int *iy, int *im, int *id, double *fd, int *deg, int *min, double *sec);
LUALIB_API double tools_getcurrentjd (int *rc);
LUALIB_API int    tools_now (int *yy, int *mm, int *dd, int *h, int *m, int *s, int *ms);

LUALIB_API void   tools_commatodot (char *str, int *result);
LUALIB_API char   *tools_between (const char *str, size_t s_len, const char *p, size_t p_len, const char *q, size_t q_len, size_t *r_len);

LUALIB_API int SDL_GetCPUCacheLineSize (void);
LUALIB_API const char *SDL_GetCPUName (void);
LUALIB_API const char *SDL_GetCPUType (void);

#define MS_WIN32S      1
#define MS_WIN95       2
#define MS_WINNT4      3
#define MS_WIN98       4
#define MS_WINME       5
#define MS_WIN2K       6
#define MS_WINXP       7
#define MS_WIN2003     8
#define MS_WINVISTA    9
#define MS_WINS2008    10
#define MS_WIN7        11
#define MS_WIN80       12
#define MS_WINS2012    13
#define MS_WIN81       14
#define MS_WINS2012R2  15
#define MS_WIN10       16
#define MS_WIN10S      17
#define MS_WIN10POST   18

#if defined( _WIN32) || defined(__OS2__)
#define szCSDVersionLENGTH 128
typedef struct WinVer {
  int BuildNumber;
  int MajorVersion;
  int MinorVersion;
  int PlatformId;
  char ProductType;
  char Maintenance[szCSDVersionLENGTH];
} WinVer;

LUALIB_API int getWindowsVersion (struct WinVer *winver);
LUALIB_API int getSysOpType (int *minorVersion, int *BuildNumber, int *PlatformId, int *ProductType, double *winver, uint8_t *SPmaj, uint8_t *SPmin);

#define tools_getwinver() ({ \
  struct WinVer windowsver = {0}; \
  int rc = getWindowsVersion(&windowsver); \
  (rc); \
})
#endif

LUALIB_API char *getModuleFileName (void);
LUALIB_API char *tools_computername (void);
LUALIB_API int tools_shellgeom (int *width, int *height);
LUALIB_API int tools_doublebase (void);

#if defined(_WIN32) || defined(__SOLARIS)
int tools_vasprintf (char **sptr, char *fmt, va_list argv);
#endif

LUALIB_API int tools_wait (double x);
LUALIB_API double tools_time (void);

/* Defines for tools_charmap */
#define __0____  0x000
#define __UPPER  0x001
#define __LOWER  0x002
#define __DIGIT  0x004
#define __BLANK  0x008 /* HT  LF  VT  FF  CR  SP */
#define __PUNCT  0x010
#define __CNTRL  0x020
#define __HEXAD  0x040
#define __PRINT  0x080

/* Defines for tools_alphadia */
#define __ALPHA  0x001
#define __DIACR  0x002
#define __VOWEL  0x004
#define __UNKNO  0x008 /* not yet set or used */


/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/*
 * from: @(#)fdlibm.h 5.1 93/09/24
 * $FreeBSD: src/lib/msun/src/openlibm.h,v 1.82 2011/11/12 19:55:48 theraven Exp $
 */
/*
 * C99 specifies that complex numbers have the same representation as
 * an array of two elements, where the first element is the real part
 * and the second element is the imaginary part.
 */
#ifndef PROPCMPLX
typedef union {
  float complex f;
  float a[2];
} float_complex;

typedef union {
  agn_Complex f;
  double a[2];
} double_complex;

typedef union {
  long agn_Complex f;
  long double a[2];
} long_double_complex;

#define  REALPART(z)  ((z).a[0])
#define  IMAGPART(z)  ((z).a[1])

/*
 * Inline functions that can be used to construct complex values.
 *
 * The C99 standard intends x+I*y to be used for this, but x+I*y is
 * currently unusable in general since gcc introduces many overflow,
 * underflow, sign and efficiency bugs by rewriting I*y as
 * (0.0+I)*(y+0.0*I) and laboriously computing the full complex product.
 * In particular, I*Inf is corrupted to NaN+I*Inf, and I*-0 is corrupted
 * to -0.0+I*0.0.
 *
 * In C11, a CMPLX(x,y) macro was added to circumvent this limitation,
 * and gcc 4.7 added a __builtin_complex feature to simplify implementation
 * of CMPLX in libc, so we can take advantage of these features if they
 * are available.
 */
#if defined(CMPLXF) && defined(CMPLX) && defined(CMPLXL) /* C11 */
#  define cpackf(x,y) CMPLXF(x,y)
#  define cpack(x,y)  CMPLX(x,y)
#  define cpackl(x,y) CMPLXL(x,y)
#elif __GNUC_PREREQ(4, 7) && !defined(__INTEL_COMPILER)
#  define cpackf(x,y) __builtin_complex ((float) (x), (float) (y))
#  define cpack(x,y)  __builtin_complex ((double) (x), (double) (y))
#  define cpackl(x,y) __builtin_complex ((long double) (x), (long double) (y))
#else /* define our own cpack functions */

/* Do NOT use INLINE macro as this causes trouble in OpenSUSE 10.3 and Mac OS X */
static __inline float complex cpackf (float x, float y) {
  float_complex z;
  REALPART(z) = x;
  IMAGPART(z) = y;
  return z.f;
}

static __inline agn_Complex cpack (double x, double y) {  /* equals Sun's CMPLX alias */
  double_complex z;
  REALPART(z) = x;
  IMAGPART(z) = y;
  return z.f;
}

static __inline long agn_Complex cpackl (long double x, long double y) {  /* equals Sun's CMPLXL alias */
  long_double_complex z;
  REALPART(z) = x;
  IMAGPART(z) = y;
  return z.f;
}
#endif  /* define our own cpack functions */
#endif  /* of PROPCMPLX */
#endif

