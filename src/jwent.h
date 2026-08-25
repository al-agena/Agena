#ifndef jwent_h
#define jwent_h

void rt_init (int binmode);
void rt_updatecount (int oc, int fold, int binary);
void rt_add (void *buf, int bufl);
void rt_end (double *r_ent, double *r_chisq, double *r_mean, double *r_montepicalc, double *r_scc, long *r_totalc);

#endif