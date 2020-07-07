/*
 *  Copyright (C) 2019 Tyson B. Littenberg (MSFC-ST12), Neil J. Cornish
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with with program; see the file COPYING. If not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *  MA  02111-1307  USA
 */


#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

/**
@file GalacticBinary.h
\brief Definition of data structures.
*/

#define MAXSTRINGSIZE 1024 //!<maximum number of characters for `path+filename` strings

/*!
 * \brief Analaysis segment and meta data about size of segment, location in full data stream, and LISA observation parameters.
 *
 * The Data structure stores information about the data being analyzed,
 * including size of data, where it is located in the full LISA band,
 * information about gaps, etc.
 *
 * The structure stores the Fourier-domain TDI data channels, and a
 * fiducial model for the instrument noise. It also has memory allocated
 * for storing the reconstructed waveforms, residuals, and noise models.
 */
struct Data
{
    int N;        //!<number of frequency bins
    int NT;       //!<number of time segments
    int Nchannel; //!<number of data channels
    int DMAX;     //!<max dimension of signal model
    
    long cseed; //!<seed for MCMC
    long nseed; //!<seed for noise realization
    long iseed; //!<seed for injection parameters
    
    double T; //!<observation time
    double sqT; //!<\f$\sqrt{T}\f$
    
    int qmin; //!<minimum frequency bin of segment
    int qmax; //!<maximum frequency bin of segment
    int qpad; //!<number of frequency bins padding ends of segment
    
    double fmin; //!<minimum frequency of segment
    double fmax; //!<maximum frequency of segment
    double sine_f_on_fstar; //!<\f$sin(f * 2\pi L/c)\f$
    
    //some manipulations of f,fmin for likelihood calculation
    double sum_log_f; //!<\f$\sum \log(f)\f$ appears in some normalizations
    double logfmin; //!<\f$\log(f_{\rm min})\f$ appears in some normalizations
    
    double *t0;   //!<start times of segments
    double *tgap; //!<time between segments
    
    //Response
    struct TDI **tdi; //!<TDI data channels
    struct Noise **noise; //!<Reference noise model
    
    //Reconstructed signal
    int Nwave; //!<Number of samples for computing posterior reconstructions
    int downsample; //!<Downsample factor for getting the desired number of samples

    double ****h_rec; //!<Store waveform reconstruction samples \f$ 2N \times N_\rm{channel} \times NT \times NMCMC \f$
    double ****h_res; //!<Store data residual samples \f$ 2N \times N_\rm{channel} \times NT \times NMCMC \f$
    double ****r_pow; //!<Store residual power samples \f$ N \times N_\rm{channel} \times NT \times NMCMC \f$
    double ****h_pow; //!<Store waveform power samples \f$ N \times N_\rm{channel} \times NT \times NMCMC \f$
    double ****S_pow; //!<Store noise power samples \f$ N \times N_\rm{channel} \times NT \times NMCMC \f$
    
    //Injection
    int NP; //!<number of parameters of injection
    struct Source *inj; //!<injected source structure
    
    //Spectrum proposal
    double *p; //!<power spectral density of data
    double pmax; //!<maximum power spectrial density
    double SNR2; //!<estimated \f${\rm SNR}^2\f$ of data
    
    //
    char fileName[128]; //!<place holder for filnames
    
    /**
     \brief Convention for data format
     
     format = "phase" for phase difference (distance)
     format = "frequency" for fractional frequency (velocity) **Use for matching LDC**
     */
    char format[16];
    
};

/*!
 *\brief Run flags set or changed from default values when parsing command line for `gb_mcmc` **and** `gb_catalog`.
 *
 *Descriptions of each flag includes default settings and command line arguments to change/set flags.
 *Boolean flags are defined as integers with `0==FALSE` and `1==TRUE`.
 *
 */
struct Flags
{
    int verbose;    //!<`[--verbose; default=FALSE]`: increases file output (all chains, less downsampling, etc.)
    int quiet;      //!<`[--quiet; default=FALSE]`: decreases file output (less diagnostic data, only what is needed for post processing/recovery)
    int NMCMC;      //!<`[--steps=INT; default=100000]`: number of MCMC samples
    int NBURN;      //!<number of burn in samples, equals Flags::NMCMC.
    int NINJ;       //!<`[--inj=FILENAME]`: number of injections = number of `--inj` instances in command line
    int NDATA;      //!<`[default=1]`: number of frequency segments, equal to Flags::NINJ.
    int NT;         //!<`[--segments=INT; default=1]`: number of time segments
    int DMAX;       //!<`[--sources=INT; default=10]`: max number of sources
    int simNoise;   //!<`[--sim-noise; default=FALSE]`: simulate random noise realization and add to data
    int fixSky;     //!<`[--fix-sky; default=FALSE]`: hold sky location fixed to injection parameters.  Set to `TRUE` if Flags::knownSource=`TRUE`.
    int fixFreq;    //!<`[--fix-freq; default=FALSE]`: hold GW frequency fixed to injection parameters
    int galaxyPrior;//!<`[--galaxy-prior; default=FALSE]`: use model of galaxy for sky location prior
    int snrPrior;   //!<`[--snr-prior; default=FALSE]`: use SNR prior for amplitude
    int emPrior;    //!<`[--em-prior=FILENAME]`: use input data file with EM-derived parameters for priors.
    int knownSource;//!<`[--known-source; default=FALSE]`: injection is known binary, will need polarization and phase to be internally generated. Sets Flags::fixSky = `TRUE`.
    int detached;   //!<`[--detached; default=FALSE]`: assume binary is detached, fdot prior becomes \f$U[\dot{f}(\mathcal{M}_c=0.15),\dot{f}(\mathcal{M}_c=1.00)]\f$
    int strainData; //!<`[--data=FILENAME; default=FALSE]`: read data from file instead of simulate internally.
    int orbit;      //!<`[--orbit=FILENAME; default=FALSE]`: use numerical spacecraft ephemerides supplied in `FILENAME`. `--orbit` argument sets flag to `TRUE`.
    int prior;      //!<`[--prior; default=FALSE]`: set log-likelihood to constant for testing detailed balance.
    int debug;      //!<`[--debug; default=FALSE]`: coarser settings for proposals  and verbose output for debugging
    int cheat;      //!<start sampler at injection values
    int burnin;     //!<`[--no-burnin; default=TRUE]`: chain is in the burn in phase
    int update;     //!<`[--update=FILENAME; default=FALSE]`: updating fit from previous chain samples passed as `FILENAME`, used in draw_from_cdf().
    int updateCov;  //!<`[--update-cov=FILENAME; default=FALSE]`: updating fit from covariance matrix files built from chain samples, passed as `FILENAME`, used in draw_from_cov().
    int match;      //!<[--match=FLOAT; default=0.8]`: match threshold for chain sample clustering in post processing.
    int rj;         //!<--no-rj; default=TRUE]`: flag for determining if trans dimensional MCMC moves (RJMCMC) are enabled.
    int gap;        //!<`[--fit-gap; default=FALSE]`: flag for determining if model includes fit to time-gap in the data.
    int calibration;//!<`[--calibration; default=FALSE]`: flag for determining if model is marginalizing over calibration  uncertainty.
    int confNoise;  //!<`[--conf-noise; default=FALSE]`: include model of confusion noise in \f$S_n(f)\f$, either for simulating noise or as starting value for parameterized noise model.
    int resume;     //!<`[--resume; default=FALSE]`: restart sampler from run state saved during checkpointing. Starts from scratch if no checkpointing files are found.
    int catalog;    //!<`[--catalog=FILENAME; default=FALSE]`: use list of previously detected sources supplied in `FILENAME` to clean bandwidth padding (`gb_mcmc`) or for building family tree (`gb_catalog`).
    char **injFile;                   //!<`[--inj=FILENAME]`: list of injection files. Can support up to `NINJ=10` separate injections.
    char noiseFile[MAXSTRINGSIZE];    //!<file containing reconstructed noise model for `gb_catalog` to compute SNRs against.
    char cdfFile[MAXSTRINGSIZE];      //!<store `FILENAME` of input chain file from Flags::update.
    char covFile[MAXSTRINGSIZE];      //!<store `FILENAME` of input covariance matrix file from Flags::updateCov.
    char matchInfile1[MAXSTRINGSIZE]; //!<input waveform \f$A\f$ for computing match \f$(h_A|h_B)\f$
    char matchInfile2[MAXSTRINGSIZE]; //!<input waveform \f$B\f$ for computing match \f$(h_A|h_B)\f$
    char pdfFile[MAXSTRINGSIZE];      //!<store `FILENAME` of input priors for Flags:knownSource.
    char catalogFile[MAXSTRINGSIZE];  //!<store `FILENAME` containing previously identified detections from Flags::catalog for cleaning padding regions
};

struct Chain
{
    //Number of chains
    int NC;
    int NP;
    int NS;
    int *index;
    int **dimension;
    double *acceptance;
    double *temperature;
    double *avgLogL;
    double annealing;
    double logLmax;
    
    //thread-safe RNG
    const gsl_rng_type **T;
    gsl_rng **r;
    
    //chain files
    FILE **noiseFile;
    FILE **chainFile;
    FILE **calibrationFile;
    FILE **dimensionFile;
    FILE **parameterFile;
    FILE *likelihoodFile;
    FILE *temperatureFile;
};

struct Source
{
    //Intrinsic
    double m1;
    double m2;
    double f0;
    
    //Extrinisic
    double psi;
    double cosi;
    double phi0;
    
    double D;
    double phi;
    double costheta;
    
    //Derived
    double amp;
    double dfdt;
    double d2fdt2;
    double Mc;
    
    //Book-keeping
    int BW;
    int qmin;
    int qmax;
    int imin;
    int imax;
    
    //Response
    struct TDI *tdi;
    
    //Fisher matrix
    double **fisher_matrix;
    double **fisher_evectr;
    double *fisher_evalue;
    
    //Package parameters for waveform generator
    int NP;
    double *params;
    
};

struct Noise
{
    int N;
    
    //multiplyers of analytic inst. noise model
    double etaA;
    double etaE;
    double etaX;
    
    //composite noise model
    double *SnA;
    double *SnE;
    double *SnX;
    
    //NEW! noise parameters for power-law fit
    double SnA_0;
    double SnE_0;
    double SnX_0;
    double alpha_A;
    double alpha_E;
    double alpha_X;
    
};

struct Calibration
{
    double dampA;
    double dampE;
    double dampX;
    double dphiA;
    double dphiE;
    double dphiX;
    double real_dphiA;
    double real_dphiE;
    double real_dphiX;
    double imag_dphiA;
    double imag_dphiE;
    double imag_dphiX;
};

struct Model
{
    //Source parameters
    int NT;     //number of time segments
    int NP;     //maximum number of signal parameters
    int Nmax;   //maximum number of signals in model
    int Nlive;  //current number of signals in model
    struct Source **source;
    
    //Noise parameters
    struct Noise **noise;
    
    //Calibration parameters
    struct Calibration **calibration;
    
    //TDI
    struct TDI **tdi;
    struct TDI **residual;
    
    //Start time for segment for model
    double *t0;
    double *t0_min;
    double *t0_max;
    
    //Source parameter priors
    double **prior;
    double *logPriorVolume;
    
    //Model likelihood
    double logL;
    double logLnorm;
};


