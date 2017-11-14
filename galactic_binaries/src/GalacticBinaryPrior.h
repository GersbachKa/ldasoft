//
//  GalacticBinaryPrior.h
//  
//
//  Created by Littenberg, Tyson B. (MSFC-ZP12) on 2/6/17.
//
//

#ifndef GalacticBinaryPrior_h
#define GalacticBinaryPrior_h

#include <stdio.h>

/* ----------------  MILKY WAY MODEL  ------------------- */

/* distance from solar BC to GC (kpc) */
#define GALAXY_RGC 7.2

/* bulge fraction */
#define GALAXY_A  0.25

/* bulge radius (kpc) */
#define GALAXY_Rb 0.8

/* disk radius (kpc) */
#define GALAXY_Rd 2.5

/* disk height (kpc) */
#define GALAXY_Zd 0.4

struct Prior
{
  //Source parameter priors
  double **prior;
  double logPriorVolume;

  //galaxy prior
  double *skyhist;
  double dcostheta;
  double dphi;
  double skymaxp;
  int ncostheta;
  int nphi;
};

void set_galaxy_prior(struct Flags *flags, struct Prior *prior);
void set_uniform_prior(struct Flags *flags, struct Model *model, struct Data *data, int verbose);
double evaluate_prior(struct Flags *flags, struct Data *data, struct Model *model, struct Prior *prior, double *params);
double evaluate_snr_prior(struct Prior *prior, struct Data *data, struct Model *model, double *params);


#endif /* GalacticBinaryPrior_h */
