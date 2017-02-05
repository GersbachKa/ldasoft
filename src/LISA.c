//
//  LISA.c
//  
//
//  Created by Littenberg, Tyson B. (MSFC-ZP12) on 1/15/17.
//
//
#include <stdlib.h>
#include <math.h>

#include "LISA.h"
#include "Constants.h"
#include "GalacticBinaryMath.h"

void spacecraft(struct Orbit *orbit, double tint, double *xint, double *yint, double *zint)
{
  int i;
  
  for(i=0; i<3; i++)
  {
    LISA_splint(orbit->t, orbit->x[i], orbit->dx[i], orbit->Norb, tint, &xint[i+1]);
    LISA_splint(orbit->t, orbit->y[i], orbit->dy[i], orbit->Norb, tint, &yint[i+1]);
    LISA_splint(orbit->t, orbit->z[i], orbit->dz[i], orbit->Norb, tint, &zint[i+1]);
  }
}

void initialize_orbit(struct Orbit *orbit)
{
  fprintf(stdout,"==== Initialize LISA Orbit Structure ====\n\n");

  int n,i;
  double junk;
  
  FILE *infile = fopen(orbit->OrbitFileName,"r");
  
  //how big is the file
  n=0;
  while(!feof(infile))
  {
    /*columns of orbit file:
     t sc1x sc1y sc1z sc2x sc2y sc2z sc3x sc3y sc3z
     */
    n++;
    fscanf(infile,"%lg %lg %lg %lg %lg %lg %lg %lg %lg %lg",&junk,&junk,&junk,&junk,&junk,&junk,&junk,&junk,&junk,&junk);
  }
  n--;
  rewind(infile);
  
  //allocate memory for local workspace
  orbit->Norb = n;
  double *t   = malloc(sizeof(double)*orbit->Norb);
  double **x  = malloc(sizeof(double *)*3);
  double **y  = malloc(sizeof(double *)*3);
  double **z  = malloc(sizeof(double *)*3);
  double **dx = malloc(sizeof(double *)*3);
  double **dy = malloc(sizeof(double *)*3);
  double **dz = malloc(sizeof(double *)*3);
  for(i=0; i<3; i++)
  {
    x[i]  = malloc(sizeof(double)*orbit->Norb);
    y[i]  = malloc(sizeof(double)*orbit->Norb);
    z[i]  = malloc(sizeof(double)*orbit->Norb);
    dx[i] = malloc(sizeof(double)*orbit->Norb);
    dy[i] = malloc(sizeof(double)*orbit->Norb);
    dz[i] = malloc(sizeof(double)*orbit->Norb);
  }
  
  //allocate memory for orbit structure
  orbit->t  = malloc(sizeof(double)*orbit->Norb);
  orbit->x  = malloc(sizeof(double *)*3);
  orbit->y  = malloc(sizeof(double *)*3);
  orbit->z  = malloc(sizeof(double *)*3);
  orbit->dx = malloc(sizeof(double *)*3);
  orbit->dy = malloc(sizeof(double *)*3);
  orbit->dz = malloc(sizeof(double *)*3);
  for(i=0; i<3; i++)
  {
    orbit->x[i]  = malloc(sizeof(double)*orbit->Norb);
    orbit->y[i]  = malloc(sizeof(double)*orbit->Norb);
    orbit->z[i]  = malloc(sizeof(double)*orbit->Norb);
    orbit->dx[i] = malloc(sizeof(double)*orbit->Norb);
    orbit->dy[i] = malloc(sizeof(double)*orbit->Norb);
    orbit->dz[i] = malloc(sizeof(double)*orbit->Norb);
  }
  
  //read in orbits
  for(n=0; n<orbit->Norb; n++)
  {
    //First time sample must be at t=0 for phasing
    fscanf(infile,"%lg",&t[n]);
    for(i=0; i<3; i++) fscanf(infile,"%lg %lg %lg",&x[i][n],&y[i][n],&z[i][n]);
    
    orbit->t[n] = t[n];
    
    //Repackage orbit positions into arrays for interpolation
    for(i=0; i<3; i++)
    {
      orbit->x[i][n] = x[i][n];
      orbit->y[i][n] = y[i][n];
      orbit->z[i][n] = z[i][n];
    }
  }
  fclose(infile);
  
  //calculate derivatives for cubic spline
  for(i=0; i<3; i++)
  {
    LISA_spline(t, orbit->x[i], orbit->Norb, 1.e30, 1.e30, orbit->dx[i]);
    LISA_spline(t, orbit->y[i], orbit->Norb, 1.e30, 1.e30, orbit->dy[i]);
    LISA_spline(t, orbit->z[i], orbit->Norb, 1.e30, 1.e30, orbit->dz[i]);
  }
  
  //calculate average arm length
  printf("Estimating average armlengths -- assumes evenly sampled orbits\n\n");
  double L12=0.0;
  double L23=0.0;
  double L31=0.0;
  double x1,x2,x3,y1,y2,y3,z1,z2,z3;
  for(n=0; n<orbit->Norb; n++)
  {
    x1 = orbit->x[0][n];
    x2 = orbit->x[1][n];
    x3 = orbit->x[2][n];
    
    y1 = orbit->y[0][n];
    y2 = orbit->y[1][n];
    y3 = orbit->y[2][n];
    
    z1 = orbit->z[0][n];
    z2 = orbit->z[1][n];
    z3 = orbit->z[2][n];
    
    
    //L12
    L12 += sqrt( (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2) + (z1-z2)*(z1-z2) );
    
    //L23
    L23 += sqrt( (x3-x2)*(x3-x2) + (y3-y2)*(y3-y2) + (z3-z2)*(z3-z2) );
    
    //L31
    L31 += sqrt( (x1-x3)*(x1-x3) + (y1-y3)*(y1-y3) + (z1-z3)*(z1-z3) );
  }
  L12 /= (double)orbit->Norb;
  L31 /= (double)orbit->Norb;
  L23 /= (double)orbit->Norb;
  
  printf("Average arm lengths for the constellation:\n");
  printf("  L12 = %g\n",L12);
  printf("  L31 = %g\n",L31);
  printf("  L23 = %g\n",L23);
  printf("\n");
  
  //are the armlenghts consistent?
  double L = (L12+L31+L23)/3.;
  printf("Fractional deviation from average armlength for each side:\n");
  printf("  L12 = %g\n",fabs(L12-L)/L);
  printf("  L31 = %g\n",fabs(L31-L)/L);
  printf("  L23 = %g\n",fabs(L23-L)/L);
  printf("\n");
  
  //store armlenght & transfer frequency in orbit structure.
  orbit->L     = L;
  orbit->fstar = C/(2.0*M_PI*L);
  
  //free local memory
  for(i=0; i<3; i++)
  {
    free(x[i]);
    free(y[i]);
    free(z[i]);
    free(dx[i]);
    free(dy[i]);
    free(dz[i]);
  }
  free(t);
  free(x);
  free(y);
  free(z);
  free(dx);
  free(dy);
  free(dz);
  fprintf(stdout,"=========================================\n\n");

}
/*************************************************************************/



void LISA_spline(double *x, double *y, int n, double yp1, double ypn, double *y2)
// Unlike NR version, assumes zero-offset arrays.  CHECK THAT THIS IS CORRECT.
{
  int i, k;
  double p, qn, sig, un, *u;
  u = malloc(sizeof(double)*(n-1));
  // Boundary conditions: Check which is best.
  if (yp1 > 0.99e30)
    y2[0] = u[0] = 0.0;
  else {
    y2[0] = -0.5;
    u[0] = (3.0/(x[1]-x[0]))*((y[1]-y[0])/(x[1]-x[0])-yp1);
  }
  
  
  for(i = 1; i < n-1; i++) {
    sig = (x[i]-x[i-1])/(x[i+1]-x[i-1]);
    p = sig*y2[i-1] + 2.0;
    y2[i] = (sig-1.0)/p;
    u[i] = (y[i+1]-y[i])/(x[i+1]-x[i]) - (y[i]-y[i-1])/(x[i]-x[i-1]);
    u[i] = (6.0*u[i]/(x[i+1]-x[i-1])-sig*u[i-1])/p;
  }
  // More boundary conditions.
  if (ypn > 0.99e30)
    qn = un = 0.0;
  else {
    qn = 0.5;
    un = (3.0/(x[n-1]-x[n-2]))*(ypn-(y[n-1]-y[n-2])/(x[n-1]-x[n-2]));
  }
  y2[n-1] = (un-qn*u[n-2])/(qn*y2[n-2]+1.0);
  for (k = n-2; k >= 0; k--)
    y2[k] = y2[k]*y2[k+1]+u[k];
  free(u);
}

void LISA_splint(double *xa, double *ya, double *y2a, int n, double x, double *y)
{
  // Unlike NR version, assumes zero-offset arrays.  CHECK THAT THIS IS CORRECT.
  int klo,khi,k;
  double h,b,a;
  
  klo=0;
  khi=n-1;
  while (khi-klo > 1) {
    k=(khi+klo) >> 1;
    if (xa[k] > x) khi=k;
    else klo=k;
  }
  h=xa[khi]-xa[klo];
  
  a=(xa[khi]-x)/h;
  b=(x-xa[klo])/h;
  *y=a*ya[klo]+b*ya[khi]+((a*a*a-a)*y2a[klo]+(b*b*b-b)*y2a[khi])*(h*h)/6.0;
}



void LISA_tdi(double L, double fstar, double T, double ***d, double f0, long q, double *M, double *A, double *E, int BW, int NI)
{
  int i,j,k;
  int BW2   = BW*2;
  int BWon2 = BW/2;
  double fonfs;
  double c3, s3, c2, s2, c1, s1;
  double f;
  double X[BW2+1],Y[BW2+1],Z[BW2+1];
  double phiLS, cLS, sLS;
  double sqT=sqrt(T);
  
  //phiLS = PI2*f0*(0.5-LonC);//1 s sampling rate
  //TODO: sampling rate is hard-coded into tdi function
  phiLS = PI2*f0*(7.5-L/C);//15 s sampling rate
  //phiLS = PI2*f0*(dt/2.0-LonC);//arbitrary sampling rate
  cLS = cos(phiLS);
  sLS = sin(phiLS);
  
  for(i=1; i<=BW; i++)
  {
    k = 2*i;
    j = k-1;
    
    f = ((double)(q + i-1 - BWon2))/T;
    fonfs = f/fstar;
    c3 = cos(3.*fonfs);  c2 = cos(2.*fonfs);  c1 = cos(fonfs);
    s3 = sin(3.*fonfs);  s2 = sin(2.*fonfs);  s1 = sin(fonfs);
    
    X[j] =	(d[1][2][j]-d[1][3][j])*c3 + (d[1][2][k]-d[1][3][k])*s3 +
    (d[2][1][j]-d[3][1][j])*c2 + (d[2][1][k]-d[3][1][k])*s2 +
    (d[1][3][j]-d[1][2][j])*c1 + (d[1][3][k]-d[1][2][k])*s1 +
    (d[3][1][j]-d[2][1][j]);
    
    X[k] =	(d[1][2][k]-d[1][3][k])*c3 - (d[1][2][j]-d[1][3][j])*s3 +
    (d[2][1][k]-d[3][1][k])*c2 - (d[2][1][j]-d[3][1][j])*s2 +
    (d[1][3][k]-d[1][2][k])*c1 - (d[1][3][j]-d[1][2][j])*s1 +
    (d[3][1][k]-d[2][1][k]);
    
    M[j] = sqT*(X[j]*cLS - X[k]*sLS);
    M[k] =-sqT*(X[j]*sLS + X[k]*cLS);
    
    //save some CPU time when only X-channel is needed
    if(NI>1)
    {
      Y[j] =	(d[2][3][j]-d[2][1][j])*c3 + (d[2][3][k]-d[2][1][k])*s3 +
      (d[3][2][j]-d[1][2][j])*c2 + (d[3][2][k]-d[1][2][k])*s2+
      (d[2][1][j]-d[2][3][j])*c1 + (d[2][1][k]-d[2][3][k])*s1+
      (d[1][2][j]-d[3][2][j]);
      
      Y[k] =	(d[2][3][k]-d[2][1][k])*c3 - (d[2][3][j]-d[2][1][j])*s3+
      (d[3][2][k]-d[1][2][k])*c2 - (d[3][2][j]-d[1][2][j])*s2+
      (d[2][1][k]-d[2][3][k])*c1 - (d[2][1][j]-d[2][3][j])*s1+
      (d[1][2][k]-d[3][2][k]);
      
      Z[j] =	(d[3][1][j]-d[3][2][j])*c3 + (d[3][1][k]-d[3][2][k])*s3+
      (d[1][3][j]-d[2][3][j])*c2 + (d[1][3][k]-d[2][3][k])*s2+
      (d[3][2][j]-d[3][1][j])*c1 + (d[3][2][k]-d[3][1][k])*s1+
      (d[2][3][j]-d[1][3][j]);
      
      Z[k] =	(d[3][1][k]-d[3][2][k])*c3 - (d[3][1][j]-d[3][2][j])*s3+
      (d[1][3][k]-d[2][3][k])*c2 - (d[1][3][j]-d[2][3][j])*s2+
      (d[3][2][k]-d[3][1][k])*c1 - (d[3][2][j]-d[3][1][j])*s1+
      (d[2][3][k]-d[1][3][k]);
      
      
      A[j] =  sqT*((2.0*X[j]-Y[j]-Z[j])*cLS-(2.0*X[k]-Y[k]-Z[k])*sLS)/3.0;
      A[k] = -sqT*((2.0*X[j]-Y[j]-Z[j])*sLS+(2.0*X[k]-Y[k]-Z[k])*cLS)/3.0;
      
      E[j] =  sqT*((Z[j]-Y[j])*cLS-(Z[k]-Y[k])*sLS)/SQ3;
      E[k] = -sqT*((Z[j]-Y[j])*sLS+(Z[k]-Y[k])*cLS)/SQ3;
    }
  }
}

double AEnoise(double L, double fstar, double f)
{
  //Power spectral density of the detector noise and transfer frequency
  double Sn;
  
  
  // Calculate the power spectral density of the detector noise at the given frequency
  Sn = 16.0/3.0*ipow(sin(f/fstar),2.0)*( ( (2.0+cos(f/fstar))*Sps + 2.0*(3.0+2.0*cos(f/fstar)+cos(2.0*f/fstar))*Sacc*(1.0/ipow(2.0*M_PI*f,4))) / ipow(2.0*L,2.0));
  
  return Sn;
}
