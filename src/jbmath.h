#ifndef jbmath_h
#define jbmath_h

int i4_max (int i1, int i2);
int i4_min (int i1, int i2);
int i4_modp (int i, int j, int *rc);
int i4_wrap (int ival, int ilo, int ihi);
int r8_inits (double dos[], int nos, double eta);

double r8_besi1 (double x);
double r8_besi1e (double x);
double r8_besj0 (double x);
double r8_besk (double nu, double x);
double r8_besk1 (double x);
double r8_besk1e (double x);
double r8_lgmc (double x);
double r8_mach (int i);
double r8_sign (double x);
double r8_uniform_01 (int *seed);
double r8mat_is_symmetric (int m, int n, double a[]);
double r8mat_max (int m, int n, double a[]);
double r8mat_min (int m, int n, double a[]);
double r8vec_min (int n, double r8vec[]);
double *sample_paths_cholesky (int n, int n2, double rhomax, double rho0,
  double *correlation (int n, double rho_vec[], double rho0), int *seed);
double *sample_paths_eigen (int n, int n2, double rhomax, double rho0,
  double *correlation (int n, double rho_vec[], double rho0), int *seed);
double *sample_paths2_cholesky (int n, int n2, double rhomin, double rhomax,
  double rho0, double *correlation2 (int m, int n, double s[], double t[],
  double rho0), int *seed);
double *sample_paths2_eigen ( int n, int n2, double rhomin, double rhomax,
  double rho0, double *correlation2 ( int m, int n, double s[], double t[],
  double rho0), int *seed);

double r8_lambert_w (double x, int nb, int l);
double r8_digamma (double x);
double r8_rc (double x, double y, double errtol);
double r8_e1 (double x);
double r8_ei (double x);
#define r8_ei(x) (-r8_e1(-x))

#endif
