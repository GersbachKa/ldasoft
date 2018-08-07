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
int galactic_binary_bandwidth(double L, double fstar, double f, double fdot, double costheta, double A, double T, int N);

double M_fdot(double f, double fdot);
double galactic_binary_dL(double f0, double dfdt, double A);

void instrument_noise(double f, double fstar, double L, double *SAE, double *SXYZ);

void spacecraft(struct lisa_orbit *orbit, double tint, double *xint, double *yint, double *zint);
void initialize_orbit(char OrbitFile[], struct lisa_orbit *orbit);
void LISA_spline(double *x, double *y, int n, double yp1, double ypn, double *y2);
void LISA_splint(double *xa, double *ya, double *y2a, int n, double x, double *y);


void dfour1(double data[], unsigned long nn, int isign);

double Sum(double *AA, double *EE, long M, double SN, double TOBS);

double quickselect(double *arr, int n, int k);
void medianX(long imin, long imax, double fstar, double L, double *XP, double *Xnoise, double *Xconf,double TOBS);
void medianAE(long imin, long imax, double fstar, double L, double *AEP, double *AEnoise, double *AEconf, double TOBS);
double ran2(long *idum);
double gasdev2(long *idum);
void KILL(char*);

