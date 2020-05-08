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


#include <math.h>
#include <string.h>
#include <stdio.h>

#include "LISA.h"
#include "Constants.h"
#include "GalacticBinary.h"
#include "GalacticBinaryMath.h"
#include "GalacticBinaryIO.h"
#include "GalacticBinaryWaveform.h"
#include "GalacticBinaryPrior.h"

static double loglike(double *x, int D)
{
    double u, rsq, z, s, ll;
    
    z = x[2];
    rsq = x[0]*x[0]+x[1]*x[1]+x[2]*x[2];
    u = sqrt(x[0]*x[0]+x[1]*x[1]);
    
    s = 1.0/cosh(z/GALAXY_Zd);
    
    // Note that overall rho0 in density is irrelevant since we are working with ratios of likelihoods in the MCMC
    
    ll = log(GALAXY_A*exp(-rsq/(GALAXY_Rb*GALAXY_Rb))+(1.0-GALAXY_A)*exp(-u/GALAXY_Rd)*s*s);
    
    return(ll);
    
}


static void rotate_galtoeclip(double *xg, double *xe)
{
    xe[0] = -0.05487556043*xg[0] + 0.4941094278*xg[1] - 0.8676661492*xg[2];
    
    xe[1] = -0.99382137890*xg[0] - 0.1109907351*xg[1] - 0.00035159077*xg[2];
    
    xe[2] = -0.09647662818*xg[0] + 0.8622858751*xg[1] + 0.4971471918*xg[2];
}

void set_galaxy_prior(struct Flags *flags, struct Prior *prior)
{
    fprintf(stdout,"\n============ Galaxy model sky prior ============\n");
    fprintf(stdout,"Monte carlo over galaxy model\n");
    fprintf(stdout,"   Distance to GC = %g kpc\n",GALAXY_RGC);
    fprintf(stdout,"   Disk Radius    = %g kpc\n",GALAXY_Rd);
    fprintf(stdout,"   Disk Height    = %g kpc\n",GALAXY_Zd);
    fprintf(stdout,"   Bulge Radius   = %g kpc\n",GALAXY_Rb);
    fprintf(stdout,"   Bulge Fraction = %g\n",    GALAXY_A);
    
    double *x, *y;  // current and proposed parameters
    int D = 3;  // number of parameters
    int Nth = 200;  // bins in cos theta
    int Nph = 200;  // bins in phi
    int MCMC=100000000;
    int j;
    int ith, iph, cnt;
    double H, dOmega;
    double logLx, logLy;
    double alpha, beta, xx, yy, zz;
    double *xe, *xg;
    double r_ec, theta, phi;
    int mc;
    //  FILE *chain = NULL;
    
    if(flags->debug)
    {
        Nth /= 10;
        Nph /= 10;
        MCMC/=10;
    }
    
    const gsl_rng_type * T;
    gsl_rng * r;
    gsl_rng_env_setup();
    
    T = gsl_rng_default;
    r = gsl_rng_alloc (T);
    
    x =  (double*)calloc(D,sizeof(double));
    xe = (double*)calloc(D,sizeof(double));
    xg = (double*)calloc(D,sizeof(double));
    y =  (double*)calloc(D,sizeof(double));
    
    prior->skyhist = (double*)calloc((Nth*Nph),sizeof(double));
    
    prior->dcostheta = 2./(double)Nth;
    prior->dphi      = 2.*M_PI/(double)Nph;
    
    prior->ncostheta = Nth;
    prior->nphi      = Nph;
    
    prior->skymaxp = 0.0;
    
    for(ith=0; ith< Nth; ith++)
    {
        for(iph=0; iph< Nph; iph++)
        {
            prior->skyhist[ith*Nph+iph] = 0.0;
        }
    }
    
    // starting values for parameters
    x[0] = 0.5;
    x[1] = 0.4;
    x[2] = 0.1;
    
    logLx = loglike(x, D);
    
    cnt = 0;
    
    //  if(flags->verbose)chain = fopen("chain.dat", "w");
    
    //int MCMC=1000000000;
    for(mc=0; mc<MCMC; mc++)
    {
        if(mc%(MCMC/100)==0)printProgress((double)mc/(double)MCMC);
        
        alpha = gsl_rng_uniform(r);
        
        if(alpha > 0.7)  // uniform draw from a big box
        {
            
            y[0] = 20.0*GALAXY_Rd*(-1.0+2.0*gsl_rng_uniform(r));
            y[1] = 20.0*GALAXY_Rd*(-1.0+2.0*gsl_rng_uniform(r));
            y[2] = 40.0*GALAXY_Zd*(-1.0+2.0*gsl_rng_uniform(r));
            
        }
        else
        {
            
            beta = gsl_rng_uniform(r);
            
            if(beta > 0.8)
            {
                xx = 0.1;
            }
            else if (beta > 0.4)
            {
                xx = 0.01;
            }
            else
            {
                xx = 0.001;
            }
            for(j=0; j< 2; j++) y[j] = x[j] + gsl_ran_gaussian(r,xx);
            y[2] = x[2] + 0.1*gsl_ran_gaussian(r,xx);
            
        }
        
        
        logLy = loglike(y, D);
        
        H = logLy - logLx;
        beta = gsl_rng_uniform(r);
        beta = log(beta);
        
        if(H > beta)
        {
            for(j=0; j< D; j++) x[j] = y[j];
            logLx = logLy;
        }
        
        if(mc%100 == 0)
        {
            
            /* rotate from galactic to ecliptic coordinates */
            
            xg[0] = x[0] - GALAXY_RGC;   // solar barycenter is offset from galactic center along x-axis (by convention)
            xg[1] = x[1];
            xg[2] = x[2];
            
            rotate_galtoeclip(xg, xe);
            
            r_ec = sqrt(xe[0]*xe[0]+xe[1]*xe[1]+xe[2]*xe[2]);
            
            theta = M_PI/2.0-acos(xe[2]/r_ec);
            
            phi = atan2(xe[1],xe[0]);
            
            if(phi<0.0) phi += 2.0*M_PI;
            
            //      if(mc%1000 == 0 && flags->verbose) fprintf(chain,"%d %e %e %e %e %e %e %e\n", mc/1000, logLx, x[0], x[1], x[2], theta, phi, r_ec);
            
            ith = (int)(0.5*(1.0+sin(theta))*(double)(Nth));
            //iph = (int)(phi/(2.0*M_PI)*(double)(Nph));
            //ith = (int)(0.5*(1.0-sin(theta))*(double)(Nth));
            iph = (int)((2*M_PI-phi)/(2.0*M_PI)*(double)(Nph));
            
            //ith = (int)floor(Nth*gsl_rng_uniform(r));
            //iph = (int)floor(Nph*gsl_rng_uniform(r));
            
            cnt++;
            
            if(ith < 0 || ith > Nth -1) printf("%d %d\n", ith, iph);
            if(iph < 0 || iph > Nph -1) printf("%d %d\n", ith, iph);
            
            prior->skyhist[ith*Nph+iph] += 1.0;
            
        }
        
    }
    
    //  if(flags->verbose)fclose(chain);
    
    dOmega = 4.0*M_PI/(double)(Nth*Nph);
    
    double uni = 0.1;
    //fprintf(stderr,"\n   HACK:  setup_galaxy_prior() uni=%g\n",uni);
    yy = (1.0-uni)/(double)(cnt);
    zz = uni/(double)(Nth*Nph);
    
    yy /= dOmega;
    zz /= dOmega;
    
    for(ith=0; ith< Nth; ith++)
    {
        for(iph=0; iph< Nph; iph++)
        {
            xx = yy*prior->skyhist[ith*Nph+iph];
            //if(fabs(xx)  > 0.0) printf("%e %e %e\n", xx, zz, yy);
            prior->skyhist[ith*Nph+iph] = log(xx + zz);
            if(prior->skyhist[ith*Nph+iph]>prior->skymaxp) prior->skymaxp = prior->skyhist[ith*Nph+iph];
        }
    }
    
    if(flags->verbose)
    {
        FILE *fptr = fopen("skyprior.dat", "w");
        for(ith=0; ith< Nth; ith++)
        {
            xx = -1.0+2.0*((double)(ith)+0.5)/(double)(Nth);
            //xx *= -1.0;    // plotting flip?
            for(iph=0; iph< Nph; iph++)
            {
                yy = 2.0*M_PI*((double)(iph)+0.5)/(double)(Nph);
                //yy = 2.0*M_PI - yy;  // plotting flip?
                fprintf(fptr,"%e %e %e\n", yy, xx, prior->skyhist[ith*Nph+iph]);
            }
            fprintf(fptr,"\n");
        }
        fclose(fptr);
    }
    
    free(x);
    free(y);
    free(xe);
    free(xg);
    gsl_rng_free (r);
    fprintf(stdout,"\n================================================\n\n");
    fflush(stdout);
    
}

void set_uniform_prior(struct Flags *flags, struct Model *model, struct Data *data, int verbose)
{
    /*
     params[0] = source->f0*T;
     params[1] = source->costheta;
     params[2] = source->phi;
     params[3] = log(source->amp);
     params[4] = source->cosi;
     params[5] = source->psi;
     params[6] = source->phi0;
     params[7] = source->dfdt*T*T;
     */
    
    
    //TODO:  make t0 a parameter
    for(int i=0; i<model->NT; i++)
    {
        model->t0[i] = data->t0[i];
        model->t0_min[i] = data->t0[i] - 20;
        model->t0_max[i] = data->t0[i] + 20;
    }
    
    //TODO: assign priors by parameter name, use mapper to get into vector (more robust to changes)
    
    //frequency bin
    //double qpad = 10;
    model->prior[0][0] = data->qmin;// + data->qpad;
    model->prior[0][1] = data->qmax;// - data->qpad;
    
    //colatitude
    model->prior[1][0] = -1.0;
    model->prior[1][1] =  1.0;
    
    //longitude
    model->prior[2][0] = 0.0;
    model->prior[2][1] = PI2;
    
    //log amplitude
    model->prior[3][0] = -60.0;//-54
    model->prior[3][1] = -48.0;
    
    //cos inclination
    model->prior[4][0] = -1.0;
    model->prior[4][1] =  1.0;
    
    //polarization
    model->prior[5][0] = 0.0;
    model->prior[5][1] = M_PI;
    
    //phase
    model->prior[6][0] = 0.0;
    model->prior[6][1] = PI2;
    
    //fdot (bins/Tobs)
    
    /* frequency derivative priors are a little trickier...*/
    double fmin = model->prior[0][0]/data->T;
    double fmax = model->prior[0][1]/data->T;
    
    /* emprical envolope functions from Gijs' MLDC catalog */
    double fdotmin = -0.000005*pow(fmin,(13./3.));
    double fdotmax = 0.0000008*pow(fmax,(11./3.));
    
    /* use prior on chirp mass to convert to priors on frequency evolution */
    if(flags->detached)
    {
        double Mcmin = 0.15;
        double Mcmax = 1.00;
        
        fdotmin = galactic_binary_fdot(Mcmin, fmin, data->T);
        fdotmax = galactic_binary_fdot(Mcmax, fmax, data->T);
    }
    
    double fddotmin = 11.0/3.0*fdotmin*fdotmin/fmax;
    double fddotmax = 11.0/3.0*fdotmax*fdotmax/fmin;
    
    if(!flags->detached)
    {
        fddotmin = -fddotmax;
    }
    if(verbose)
    {
        fprintf(stdout,"\n============== PRIORS ==============\n");
        if(flags->detached)fprintf(stdout,"  Assuming detached binary, Mchirp = [0.15,1]\n");
        fprintf(stdout,"  p(fdot)  = U[%g,%g]\n",fdotmin,fdotmax);
        fprintf(stdout,"  p(fddot) = U[%g,%g]\n",fddotmin,fddotmax);
        fprintf(stdout,"  p(lnA)   = U[%g,%g]\n",model->prior[3][0],model->prior[3][1]);
        fprintf(stdout,"====================================\n\n");
    }
    
    if(data->NP>7)
    {
        model->prior[7][0] = fdotmin*data->T*data->T;
        model->prior[7][1] = fdotmax*data->T*data->T;
    }
    if(data->NP>8)
    {
        model->prior[8][0] = fddotmin*data->T*data->T*data->T;
        model->prior[8][1] = fddotmax*data->T*data->T*data->T;
    }
    
    /******************************************************
     
     Learn to parse prior files with flexible format and
     reset uniform priors accordingly (doing the naive thing
     of setting a uniform distribution over the ~90% credible
     interval for exammple.  Will need to expand to at least
     gaussian priors.
     
     What to do about intervals so small that they are
     effectively delta functions?  Might anger some proposals...
     
     GAIA distance accuracy < 20%
     
     ******************************************************/
    if(flags->emPrior)
    {
        FILE *priorFile = fopen(flags->pdfFile,"r");
        char name[16];
        double min,max;
        
        while(fscanf(priorFile,"%s %lg %lg",name,&min,&max) != EOF)
        {
            
            if(strcmp("f0",name) == 0)
            {
                model->prior[0][0] = min*data->T;
                model->prior[0][1] = max*data->T;
            }
            
            else if(strcmp("dfdt",name) == 0)
            {
                model->prior[7][0] = min*data->T*data->T;
                model->prior[7][1] = max*data->T*data->T;
            }
            
            else if(strcmp("amp",name) == 0)
            {
                model->prior[3][0] = log(min);
                model->prior[3][1] = log(max);
            }
            
            else if(strcmp("phi",name) == 0)
            {
                model->prior[2][0] = min;
                model->prior[2][1] = max;
            }
            
            else if(strcmp("costheta",name) == 0)
            {
                model->prior[1][0] = min;
                model->prior[1][1] = max;
            }
            
            else if(strcmp("cosi",name) == 0)
            {
                model->prior[4][0] = min;
                model->prior[4][1] = max;
            }
            
            else if(strcmp("psi",name) == 0)
            {
                model->prior[5][0] = min;
                model->prior[5][1] = max;
            }
            
            else if(strcmp("phi0",name) == 0)
            {
                model->prior[6][0] = min;
                model->prior[6][1] = max;
            }
            
            else
            {
                fprintf(stdout, "unrecognized parameter in prior file: %s\n",name);
            }      
        }
        
        fclose(priorFile);
    }
    
    //set prior volume
    for(int n=0; n<data->NP; n++) model->logPriorVolume[n] = log(model->prior[n][1]-model->prior[n][0]);
    
}

double evaluate_prior(struct Flags *flags, struct Data *data, struct Model *model, struct Prior *prior, double *params)
{
    double logP=0.0;
    double **uniform_prior = model->prior;
    
    //guard against nan's, but do so loudly
    for(int i=0; i<model->NP; i++)
    {
        if(params[i]!=params[i])
        {
            fprintf(stderr,"parameter %i not a number\n",i);
            return -INFINITY;
        }
    }
    
    //sky location prior
    logP += evalaute_sky_location_prior(params, uniform_prior, model->logPriorVolume, flags->galaxyPrior, prior->skyhist, prior->dcostheta, prior->dphi, prior->nphi);
    
    //amplitude prior
    logP += evaluate_snr_prior(data, model, params);
    
    //everything else uses simple uniform priors
    logP += evaluate_uniform_priors(params, uniform_prior, model->logPriorVolume, model->NP);
    
    
    return logP;
}

double evaluate_uniform_priors(double *params, double **uniform_prior, double *logPriorVolume, int NP)
{
    //nan check
    for(int n=0; n<NP; n++) if(params[n]!=params[n]) return -INFINITY;
    
    double logP = 0.0;
    //frequency bin (uniform)
    if(params[0]<uniform_prior[0][0] || params[0]>uniform_prior[0][1]) return -INFINITY;
    //TODO: is frequency logPriorVolume up to date?
    else logP -= log(uniform_prior[0][1]-uniform_prior[0][0]);
    
    //cosine inclination
    if(params[4]<uniform_prior[4][0] || params[4]>uniform_prior[4][1]) return -INFINITY;
    else logP -= logPriorVolume[4];
    
    //polarization
    while(params[5]<uniform_prior[5][0]) params[5] += M_PI;
    while(params[5]>uniform_prior[5][1]) params[5] -= M_PI;
    logP -= logPriorVolume[5];
    
    //phase
    while(params[6]<uniform_prior[6][0]) params[6] += PI2;
    while(params[6]>uniform_prior[6][1]) params[6] -= PI2;
    logP -= logPriorVolume[6];
    
    //fdot (bins/Tobs)
    if(NP>7)
    {
        if(params[7]<uniform_prior[7][0] || params[7]>uniform_prior[7][1]) return -INFINITY;
        else logP -= logPriorVolume[7];
    }
    
    //fddot
    if(NP>8)
    {
        if(params[8]<uniform_prior[8][0] || params[8]>uniform_prior[8][1]) return -INFINITY;
        else logP -= logPriorVolume[8];
    }
    return logP;
}

double evalaute_sky_location_prior(double *params, double **uniform_prior, double *logPriorVolume, int galaxyFlag, double *skyhist, double dcostheta, double dphi, int nphi)
{
    double logP = 0.0;
    if(galaxyFlag)
    {
        if(params[1]<uniform_prior[1][0] || params[1]>uniform_prior[1][1]) return -INFINITY;
        
        if(uniform_prior[2][0] > 0.0 || uniform_prior[2][1] < PI2)
        {
            //rejection sample on reduced prior range
            if(params[2]<uniform_prior[2][0] || params[2]>uniform_prior[2][1]) return -INFINITY;
        }
        else
        {
            //periodic boundary conditions for full range
            while(params[2] < 0  ) params[2] += PI2;
            while(params[2] > PI2) params[2] -= PI2;
        }
        //map costheta and phi to index of skyhist array
        int i = (int)floor((params[1]-uniform_prior[1][0])/dcostheta);
        int j = (int)floor((params[2]-uniform_prior[2][0])/dphi);
        
        int k = i*nphi + j;
        
        logP += skyhist[k];
        
        //    FILE *fptr = fopen("prior.dat","a");
        //    fprintf(fptr,"%i %i %i %g\n",i,j,k,prior->skyhist[k]);
        //    fclose(fptr);
    }
    else
    {
        //colatitude (reflective)
        if(params[1]<uniform_prior[1][0] || params[1]>uniform_prior[1][1]) return -INFINITY;
        else logP -= logPriorVolume[1];
        
        //longitude (periodic)
        if(uniform_prior[2][0] > 0.0 || uniform_prior[2][1] < PI2)
        {
            //rejection sample on reduced prior range
            if(params[2]<uniform_prior[2][0] || params[2]>uniform_prior[2][1]) return -INFINITY;
        }
        else
        {
            //while(params[2] < uniform_prior[2][0]) params[2] += uniform_prior[2][1]-uniform_prior[2][0];
            //while(params[2] > uniform_prior[2][1]) params[2] -= uniform_prior[2][1]-uniform_prior[2][0];
            if(params[2]<uniform_prior[2][0] || params[2]>=uniform_prior[2][1])
            {
                params[2] = atan2(sin(params[2]),cos(params[2]));
                if(params[2] < 0.0) params[2] += PI2;
            }
        }
        logP -= logPriorVolume[2];
    }
    return logP;
}



double evaluate_snr_prior(struct Data *data, struct Model *model, double *params)
{
    //check that frequency is in range
    int n = (int)floor(params[0] - model->prior[0][0]);
    if(n<0 || n>=data->N) return -INFINITY;
    
    //calculate noise model estimate
    double sf = data->sine_f_on_fstar;
    double sn = model->noise[0]->SnA[n]*model->noise[0]->etaA;
    
    //get GW amplitude
    double amp = exp(params[3]);
    
    double snr = analytic_snr(amp,sn,sf,data->sqT);
    
    return log(snr_prior(snr));// * snr/amp);
}



double evaluate_calibration_prior(struct Data *data, struct Model *model)
{
    
    double dA,dphi;
    double logP = 0.0;
    
    //apply calibration error to full signal model
    //loop over time segments
    for(int m=0; m<model->NT; m++)
    {
        switch(data->Nchannel)
        {
            case 1:
                
                //amplitude
                dA   = model->calibration[m]->dampX;
                logP += log(gsl_ran_gaussian_pdf(dA,CAL_SIGMA_AMP));
                
                //phase
                dphi = model->calibration[m]->dphiX;
                logP += log(gsl_ran_gaussian_pdf(dphi,CAL_SIGMA_PHASE));
                
                break;
            case 2:
                
                //amplitude
                dA   = model->calibration[m]->dampA;
                logP += log(gsl_ran_gaussian_pdf(dA,CAL_SIGMA_AMP));
                
                //phase
                dphi = model->calibration[m]->dphiA;
                logP += log(gsl_ran_gaussian_pdf(dphi,CAL_SIGMA_PHASE));
                
                //amplitude
                dA   = model->calibration[m]->dampE;
                logP += log(gsl_ran_gaussian_pdf(dA,CAL_SIGMA_AMP));
                
                //phase
                dphi = model->calibration[m]->dphiE;
                logP += log(gsl_ran_gaussian_pdf(dphi,CAL_SIGMA_PHASE));
                
            default:
                break;
        }//end switch
    }//end loop over segments
    
    return logP;
}



