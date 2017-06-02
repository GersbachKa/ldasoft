/*
 gcc gb_mcmc_chirpmass.c -lm -lgsl -o gb_mcmc_chirpmass
 */

/***************************  REQUIRED LIBRARIES  ***************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>

/* Mass of the Sun (s) */
#define TSUN  4.9169e-6


/* ============================  MAIN PROGRAM  ============================ */

double M_fdot(double f, double fdot)
{
  return pow( fdot*pow(f,-11./3.)*(5./96.)*pow(M_PI,-8./3.)  ,  3./5.)/TSUN;
}

double M_fddot(double f, double fddot)
{
  return pow( fddot*pow(f,-19./3.)*(25./33792.)*pow(M_PI,-16./3.)  ,  3./10.)/TSUN;
}

int main(int argc, char* argv[])
{

  if(argc!=2)
  {
    fprintf(stdout,"Usage: gb_mcmc_chirpmass chainfile\n");
    return 1;
  }
  
  FILE *ifile = fopen(argv[1],"r");
  
  
  char filename[128];
  sprintf(filename,"mchirp_%s",argv[1]);
  FILE *ofile = fopen(filename,"w");
  
  sprintf(filename,"frequencies_%s",argv[1]);
  FILE *ofile2 = fopen(filename,"w");


  //parse file
  
  /*
   0)  index
   1)  logL
   2)  t0
   3)  tgap
   4)  f
   5)  fdot
   6)  A
   7)  phi
   8)  theta
   9)  cosi
   10) psi
   11) phase
   12) fddot
   */
  
  int i;
  double logL,t0,f,fdot,A,phi,theta,cosi,psi,phase,fddot;
  
  while(!feof(ifile))
  {
    fscanf(ifile,"%i%lg%lg%lg%lg%lg%lg%lg%lg%lg%lg%lg",&i,&logL,&t0,&f,&fdot,&A,&phi,&theta,&cosi,&psi,&phase,&fddot);
    if(fdot>0.0&&fddot>0.0)
    {
    fprintf(ofile2,"%.12g %.12g %.12g\n",f,fdot,fddot);
    fprintf(ofile,"%.12g %.12g\n",M_fdot(f,fdot),M_fddot(f,fddot));
    }
  }
  
  return 0;
}

