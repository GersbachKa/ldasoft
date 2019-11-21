
struct lisa_orbit{
  int N;
  
  double L;
  double fstar;
  
  double *t;
  double **x;
  double **y;
  double **z;
  double **dx;
  double **dy;
  double **dz;
};
struct lisa_orbit orbit;

void printProgress (double percentage);

void FAST_LISA(struct lisa_orbit *orbit, double TOBS, double *params, long N, long M, double *XLS, double *ALS, double *ELS);
void XYZ(double L, double fstar, double TOBS, double ***d, double f0, long q, long M, double *XLS, double *ALS, double *ELS);

double M_fdot(double f, double fdot);

void instrument_noise(double f, double fstar, double L, double *SAE, double *SXYZ);

void spacecraft(struct lisa_orbit *orbit, double tint, double *xint, double *yint, double *zint);
void initialize_orbit(char OrbitFile[], struct lisa_orbit *orbit);


double Sum(double *AA, double *EE, long M, double SN, double TOBS);


/*****************************************************/
/*                                                   */
/*        Median-based Confusion Noise Fitting       */
/*                                                   */
/*****************************************************/

double quickselect(double *arr, int n, int k);
void medianX(long imin, long imax, double fstar, double L, double *XP, double *Xnoise, double *Xconf,double TOBS);
void medianAE(long imin, long imax, double fstar, double L, double *AEP, double *AEnoise, double *AEconf, double TOBS);
double ran2(long *idum);
double gasdev2(long *idum);
void KILL(char*);

/*****************************************************/
/*                                                   */
/*        Spline-based Confusion Noise Fitting       */
/*                                                   */
/*****************************************************/

void spline_fit(int flag, int divs, long imin, long imax, double *XP, double *Xnoise, double *Xconf, double T, double fstar, double L);
void splineMCMC(int imin, int imax, int ND, double *datax, double *datay, double *sigma, double *Xnoise, double T);
void spline(double *x, double *y, int n, double yp1, double ypn, double *y2);
void splint(double *xa, double *ya, double *y2a, int n, double x, double *y);


double confusion_fit(double f, double logA, double alpha, double beta, double kappa, double gamma, double fk);
void confusion_mcmc(double *data, double *noise, double *conf, int imin, int imax, double T);

