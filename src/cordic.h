#ifndef cordic_h
#define cordic_h

double sqrt_cordic     (double x, int n);
double cbrt_cordic     (double x, int n);
double exp_cordic      (double x, int n);
double ln_cordic       (double x, int n);
void   cossin_cordic   (double x, int n, double *c, double *s);
double tan_cordic      (double x, int n);
double arccos_cordic   (double x, int n);
double arcsin_cordic   (double x, int n);
double arctan_cordic   (double x, double y, int n, double *hypotenuse);
double multiply_cordic (double x, double y);

double angle_shift     (double alpha, double beta);

#endif

