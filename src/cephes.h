/* Cephes mathematical functions

   taken from various C files written by Stephen L. Moshier.

   Cephes Math Library, Copyright by Stephen L. Moshier
   Direct inquiries to 30 Frost Street, Cambridge, MA 02140 */

#ifndef cephes_h
#define cephes_h

#ifndef PROPCMPLX
#include <complex.h>
#endif

/* some constants */
#define EUL      0.57721566490153286061
#define MACHEP   1.38777878078144567553E-17
#define MAXNUM   1.0e308
#define MAXLGM   2.556348e305
#define MAXSTIR  143.01608
#define MAXGAM   171.624376956302725
#define MAXAIRY  25.77
#define DP1	     3.14159265160560607910E0
#define DP2      1.98418714791870343106E-9
#define DP3	     1.14423774522196636802E-17
#define SQTPI    2.50662827463100050242E0      /* 2*sqrt(Pi/2) */
#define SQ2OPI   0.79788456080286535588E0      /* sqrt(2/Pi) */
#define THPIO4   2.35619449019234492885E0      /* 3*Pi/4 */
#define LOGE2    6.93147180559945309417E-1     /* ln(2) */
#define LOGPI    1.14472988584940017414E0      /* ln(Pi) */
#define CeMAXLOG 8.8029691931113054295988E1    /* ln(2**127) */
#define CeMINLOG -8.872283911167299960540E1    /* ln(2**-128) */
#define NEGZERO  -0.0

#ifndef __ARMCPU
#define MAXNUML  1.189731495357231765021e4932L
#define TWOOPIL  6.36619772367581343075535E-1L
#define PIO4L    0.78539816339744830961566L
#else
#define MAXNUML  MAXNUM
#define TWOOPIL  6.36619772367581382433e-01
#define PIO4L    PIO4
#endif

double polevl (double x, double coef[], int N);
double p1evl  (double x, double coef[], int N);
double ei     (double x);
void   sici   (double x, double *si, double *ci);
double chbevl (double x, double array[], int n);
void   shichi (double x, double *si, double *ci);
double dawsn  (double xx);
double psi    (double x);
double spence (double x);
void   fresnl (double xxa, double *ssa, double *cca);
double cephes_gamma (double x);
double zeta   (double x, double q);
double zetac  (double x);
#define Zeta(x) (zetac(x) + 1)
double cepowi (double x, int nn, int denormal);
double fac    (int i);
int    airy   (double x, double *ai, double *aip, double *bi, double *bip);
double polylog (int n, double x, int *flag);
double expn   (int n, double x);
double igam   (double a, double x);
double igamc  (double a, double x);
double incbet (double aa, double bb, double xx);
double bdtri  (int k, int n, double y);
double fdtrc  (int ia, int ib, double x);
double fdtr   (int ia, int ib, double x);
double fdtri  (int ia, int ib, double y);
double ndtri  (double y0);
double incbi  (double aa, double bb, double yy);
double cephes_exp2 (double x);
double cephes_exp10 (double x);
long double expx2l (long double x, int sign);
void cephes_sinhcoshl (long double x, long double *sih, long double *coh);

double cephes_i0     (double x);
double cephes_i0e    (double x);
double cephes_i1     (double x);
double cephes_i1e    (double x);
double cephes_j0     (double x);
double cephes_j1     (double x);
double cephes_jn     (int n, double x);
double cephes_y0     (double x);
double cephes_y1     (double x);

double sindg  (double x);
double cosdg  (double x);
double tandg  (double x);
double cotdg  (double x);
#define cscdg(x)  (1/sindg((x)))
#define secdg(x)  (1/cosdg((x)))

long double cephes_jnl (int n, long double x);
long double cephes_j0l (long double x);
long double cephes_j1l (long double x);
long double cephes_ynl (int n, long double x);
long double cephes_y0l (long double x);
long double cephes_y1l (long double x);

long double bdtrcl  (int k, int n, long double p);
long double bdtrl   (int k, int n, long double p);
long double bdtril  (int k, int n, long double y);
long double incbetl (long double aa, long double bb, long double xx);
long double ndtrl   (long double a);
long double ndtril  (long double y0);
long double incbil  (long double aa, long double bb, long double yy0);

void cephes_cgam_reim (double re, double im, double *rre, double *rim);
void cephes_clgam_reim (double re, double im, double *rre, double *rim);
#ifndef PROPCMPLX
complex double cephes_clgam (complex double);
complex double cephes_cgamma (complex double x);
#else
#define cephes_clgam cephes_clgam_reim
#define cephes_cgamma cephes_cgam_reim
#endif

double cephes_fac (int i);

double hyp1f1 (double a, double b, double x);
double hy1f1a (double a, double b, double x, double *err);
double hy1f1p (double a, double b, double x, double *err);
double hyp2f1 (double a, double b, double c, double x);
double ellpk  (double x);              /* Complete elliptic integral of the first kind */
double ellik  (double phi, double m);  /* Incomplete elliptic integral of the first kind */
double ellpe  (double x);              /* Complete elliptic integral of the second kind */
double ellie  (double phi, double m);  /* Incomplete elliptic integral of the second kind */
int    ellpj  (double u, double m,     /* Jacobian Elliptic Functions */
               double *sn, double *cn, double *dn, double *ph);
#endif


