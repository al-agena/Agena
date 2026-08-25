/*        Apply various randomness tests to a stream of bytes

                  by John Walker  --  September 1996
                       https://www.fourmilab.ch/

Source file: https://github.com/Fourmilab/ent_random_sequence_tester/blob/master/src/ent.c & randtest.c

This product (software, documents, and data files) is licensed under a Creative Commons Attribution-ShareAlike 4.0 International License (legal text). You are free to copy and redistribute this material in any medium or format, and to remix, transform, and build upon the material for any purpose, including commercially. You must give credit, provide a link to the license, and indicate if changes were made. If you remix, transform, or build upon this material, you must distribute your contributions under the same license as the original.

This product is provided with no warranty, either expressed or implied, including but not limited to any implied warranties of merchantability or fitness for a particular purpose, regarding these materials and is made available available solely on an “as-is” basis.

In no event shall John Walker be liable to anyone for special, collateral, incidental, or consequential damages in connection with or arising out of distribution or use of these materials. The sole and exclusive liability of John Walker, regardless of the form of action, shall not exceed the compensation received by the author for the product.

John Walker reserves the right to revise and improve this products as he sees fit. This publication describes the state of this product at the time of its publication, and may not reflect the product at all times in the future.
*/

#define jwent_c
#define LUA_LIB

#include <math.h>
#include <string.h>
#include "agnhlps.h"

static int binary = 0;    /* treat input as a bitstream */
static long ccount[256],  /* bins to count occurrences of values */
            totalc = 0;   /* total bytes counted */
static double prob[256];  /* probabilities per bin for entropy */

/* RT_LOG2 -- Calculate log to the base 2  */
#define log2of10 3.32192809488736234787
#define rt_log2(x) (log2of10*log10(x))

/* Bytes used as Monte Carlo co-ordinates. This should be no more bits than the mantissa of your "double" floating point type. */
#define MONTEN  6

static int mp, sccfirst;
static unsigned int monte[MONTEN];
static long inmont, mcount;
static double incirc, sccun, sccu0, scclast, scct1, scct2, scct3, ent, chisq, datasum;

/* RT_INIT -- Initialise random test counters. */
void rt_init (int binmode) {
  binary = binmode;             /* set binary / byte mode */
  /* initialise for calculations */
  ent = 0.0;                    /* clear entropy accumulator */
  chisq = 0.0;                  /* clear Chi-Square */
  datasum = 0.0;                /* clear sum of bytes for arithmetic mean */
  mp = 0;                       /* reset Monte Carlo accumulator pointer */
  mcount = 0;                   /* clear Monte Carlo tries */
  inmont = 0;                   /* clear Monte Carlo inside count */
  incirc = 65535.0*65535.0;     /* in-circle distance for Monte Carlo */
  sccfirst = 1;                 /* Mark first time for serial correlation */
  scct1 = scct2 = scct3 = 0.0;  /* clear serial correlation terms */
  incirc = pow(pow(256.0, (double)(MONTEN/2)) - 1, 2.0);
  memset(ccount, 0, sizeof ccount);
  /* for (i=0; i < 256; i++) ccount[i] = 0; */
  totalc = 0;
}


/* RT_ADD -- Add one or more bytes to accumulation. */
void rt_add (void *buf, int bufl) {
  unsigned char *bp = buf;
  int oc, c, bean;
  double montex, montey;
  while (bean=0, (bufl-- > 0)) {
    oc = *bp++;
    do {
      c = (binary) ? (!!(oc & 0x80)) : oc;
      ccount[c]++;  /* update counter for this bin */
      totalc++;
      /* update inside / outside circle counts for Monte Carlo computation of PI */
      if (bean == 0) {
        monte[mp++] = oc;    /* save character for Monte Carlo */
        if (mp >= MONTEN) {  /* calculate every MONTEN character */
          int mj;
          mp = 0;
          mcount++;
          montex = montey = 0;
          for (mj=0; mj < MONTEN/2; mj++) {
            montex = montex*256.0 + monte[mj];
            montey = montey*256.0 + monte[(MONTEN/2) + mj];
          }
          if ((montex*montex + montey*montey) <= incirc) inmont++;
        }
      }
      /* update calculation of serial correlation coefficient */
      sccun = c;
      if (sccfirst) {
        sccfirst = 0;
        scclast = 0;
        sccu0 = sccun;
      } else {
        scct1 = scct1 + scclast*sccun;
      }
      scct2 = scct2 + sccun;
      scct3 = scct3 + sccun*sccun;
      scclast = sccun;
      oc <<= 1;
    } while (binary && (++bean < 8));
  }
}

/* RT_END -- Complete calculation and return results. */
void rt_end (double *r_ent, double *r_chisq, double *r_mean, double *r_montepicalc, double *r_scc, long *r_totalc) {
  int i;
  double ent, scc, ccexp, montepi;
  /* Complete calculation of serial correlation coefficient */
  scct1 = scct1 + scclast*sccu0;
  scct2 = scct2*scct2;
  scc = totalc*scct3 - scct2;
  scc = (scc == 0.0) ? -100000 : (totalc*scct1 - scct2)/scc;
  /* Scan bins and calculate probability for each bin and Chi-Square distribution. The probability will be reused in the entropy
     calculation below. While we're at it, we sum of all the data which will be used to compute the mean. */
  ccexp = totalc/(binary ? 2.0 : 256.0);  /* expected count per bin */
  for (i=0; i < (binary ? 2 : 256); i++) {
    double a = ccount[i] - ccexp;
    prob[i] = ((double)ccount[i])/totalc;
    chisq += (a*a)/ccexp;
    datasum += ((double) i)*ccount[i];
  }
  /* Calculate entropy */
  ent = 0;
  for (i=0; i < (binary ? 2 : 256); i++) {
    if (prob[i] > 0.0) {
      ent += prob[i]*rt_log2(1/prob[i]);
    }
  }
  /* Calculate Monte Carlo value for PI from percentage of hits
     within the circle */
  montepi = 4.0*(((double)inmont)/mcount);
  /* Return results through arguments */
  *r_ent = ent;
  *r_chisq = chisq;
  *r_mean = datasum/totalc;
  *r_montepicalc = montepi;
  *r_scc = scc;
  *r_totalc = totalc;
}