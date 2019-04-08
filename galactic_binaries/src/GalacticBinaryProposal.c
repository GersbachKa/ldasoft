//
//  GalacticBinaryProposal.c
//  
//
//  Created by Littenberg, Tyson B. (MSFC-ZP12) on 2/6/17.
//
//

#include <math.h>

#include <gsl/gsl_blas.h>

#include <sys/stat.h>

#include <gsl/gsl_rng.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_eigen.h>

#include "LISA.h"
#include "Constants.h"
#include "GalacticBinary.h"
#include "GalacticBinaryIO.h"
#include "GalacticBinaryPrior.h"
#include "GalacticBinaryModel.h"
#include "GalacticBinaryWaveform.h"
#include "GalacticBinaryFStatistic.h"
#include "GalacticBinaryProposal.h"
#include "GalacticBinaryMath.h"


#define FIXME 0
#define SNRCAP 10000.0 /* SNR cap on logL */


static void write_Fstat_animation(double fmin, double T, struct Proposal *proposal)
{
  FILE *fptr = fopen("fstat/Fstat.gpi","w");
  
  fprintf(fptr,"!rm Fstat.mp4\n");
  fprintf(fptr,"set terminal pngcairo size 1024,512 enhanced font 'Verdana,12'\n");
  
  fprintf(fptr,"\n");
  
  fprintf(fptr,"# perula palette\n");
  fprintf(fptr,"set palette defined (0 '#352a87', 1 '#0363e1', 2 '#1485d4', 3 '#06a7c6', 4 '#38b99e', 5 '#92bf73', 6 '#d9ba56', 7 '#fcce2e', 8 '#f9fb0e')\n");
  
  fprintf(fptr,"\n");
  
  fprintf(fptr,"\n");

  fprintf(fptr,"unset colorbox\n");
  fprintf(fptr,"unset key\n");
  fprintf(fptr,"unset label\n");
  fprintf(fptr,"set tics scale 2 font ',8'\n");
  fprintf(fptr,"set xlabel '{/Symbol f}\n");
  fprintf(fptr,"set ylabel 'cos({/Symbol q})' offset 1\n");
  
  fprintf(fptr,"\n");
  
  fprintf(fptr,"\n");
  
  fprintf(fptr,"df0 = %g\n",proposal->matrix[0][1]/T);
  fprintf(fptr,"dph = 0.5*2.*pi/%i.\n",(int)proposal->matrix[1][0]);
  fprintf(fptr,"dth = 0.5*2./%i.\n"   ,(int)proposal->matrix[2][0]);
  
  fprintf(fptr,"\n");
  
  fprintf(fptr,"set pm3d map\n");
  
  fprintf(fptr,"\n");
  
  fprintf(fptr,"do for [ii=0:%i-1:+1]{\n",(int)proposal->matrix[0][0]);
  fprintf(fptr,"  set output sprintf('fstat_frame%%05.0f.png',ii)\n");
  fprintf(fptr,"  set size 1,1\n");
//  fprintf(fptr,"  set multiplot title sprintf('fstat-frame%%05.0f.png',ii)\n");
  fprintf(fptr,"  set multiplot title sprintf('f = %%f ',(ii*df0+%g)*1000)\n",fmin);
  fprintf(fptr,"  set size 0.5,1\n");

  fprintf(fptr,"\n");

  fprintf(fptr,"  set origin 0,0\n");
  fprintf(fptr,"  set cbrange [0.001:%lg]\n",proposal->maxp);
  fprintf(fptr,"  input = sprintf('skymap_%%05.0f.dat',ii)\n");
  fprintf(fptr,"  splot [0:2*pi] [-1:1] input u ($2+dph):($1+dth):3\n");
  
  fprintf(fptr,"\n");
  
  fprintf(fptr,"  set origin 0.5,0\n");
  fprintf(fptr,"  set cbrange [log(0.001):log(%lg)]\n",proposal->maxp);
  fprintf(fptr,"  input = sprintf('skymap_%%05.0f.dat',ii)\n");
  fprintf(fptr,"  splot [0:2*pi] [-1:1] input u ($2+dph):($1+dth):(log($3))\n");
  
  fprintf(fptr,"  unset multiplot\n");
  
  fprintf(fptr,"}\n");

  fprintf(fptr,"\n");

  fprintf(fptr,"# Create animation\n");
  fprintf(fptr,"CMD = 'ffmpeg -r 256 -f image2 -s 3840x1080 -start_number 0 -i fstat_frame%%05d.png -vframes %i -vcodec libx264 -crf 25  -pix_fmt yuv420p Fstat.mp4'\n",(int)proposal->matrix[0][0]);
  fprintf(fptr,"system(CMD)\n");

  fprintf(fptr,"\n");

  fprintf(fptr,"!rm *.png\n");
  fclose(fptr);
}

void setup_frequency_proposal(struct Data *data)
{
  int BW = 20;
  double *power = data->p;
  double total  = 0.0;
  FILE *temp = fopen("data/frequency_proposal.dat","w");
  for(int i=0; i<data->N-BW; i++)
  {
    power[i]=0.0;
    for(int n=i; n<i+BW; n++)
    {
      double SnA = data->noise[FIXME]->SnA[n];
      double SnE = data->noise[FIXME]->SnE[n];
      
      double AA = data->tdi[FIXME]->A[2*n]*data->tdi[FIXME]->A[2*n]+data->tdi[FIXME]->A[2*n+1]*data->tdi[FIXME]->A[2*n+1];
      double EE = data->tdi[FIXME]->E[2*n]*data->tdi[FIXME]->E[2*n]+data->tdi[FIXME]->E[2*n+1]*data->tdi[FIXME]->E[2*n+1];
      
      power[i] += AA/SnA + EE/SnE;
      total += power[i];
    }
  }
  for(int i=data->N-BW; i<data->N; i++)
  {
    power[i] = power[data->N-BW-1];
    total += power[i];
  }
  
  data->pmax = 0.0;
  for(int i=0; i<data->N; i++)
  {
    fprintf(temp,"%i %lg\n",i,power[i]);
    if(power[i]>data->pmax) data->pmax = power[i];
  }
  fclose(temp);
  
  
  //also get SNR^2 of data
  total = 0.0;
  for(int n=0; n<data->N; n++)
  {
      double SnA = data->noise[FIXME]->SnA[n];
      double SnE = data->noise[FIXME]->SnE[n];
      
      double AA = data->tdi[FIXME]->A[2*n]*data->tdi[FIXME]->A[2*n]+data->tdi[FIXME]->A[2*n+1]*data->tdi[FIXME]->A[2*n+1];
      double EE = data->tdi[FIXME]->E[2*n]*data->tdi[FIXME]->E[2*n]+data->tdi[FIXME]->E[2*n+1]*data->tdi[FIXME]->E[2*n+1];
      
      total += AA/SnA + EE/SnE;
  }

  data->SNR2 = total - data->N;
  printf("total=%g, N=%i\n",total, data->N);
  if(data->SNR2<0.0)data->SNR2=0.0;
  data->SNR2*=4.0;//why the factor of 4?
  printf("data-based SNR^2:  %g (%g)\n", data->SNR2, sqrt(data->SNR2));

}

void print_acceptance_rates(struct Proposal **proposal, int NP, int ic, FILE *fptr)
{
  fprintf(fptr,"Acceptance rates for chain %i:\n", ic);
  
  for(int n=0; n<NP+1; n++)
  {

    fprintf(fptr,"   %.1e  [%s]\n", (double)proposal[n]->accept[ic]/(double)proposal[n]->trial[ic],proposal[n]->name);
  }
}

double draw_from_spectrum(struct Data *data, struct Model *model, struct Source *source, UNUSED struct Proposal *proposal, double *params, gsl_rng *seed)
{
  //TODO: Work in amplitude
  
  //rejections ample for f
  int check = 1;
  double alpha;
  int q;
  int count=0;
  while(check)
  {
    params[0] = (double)model->prior[0][0] + gsl_rng_uniform(seed)*(double)(model->prior[0][1]-model->prior[0][0]);
    alpha     = gsl_rng_uniform(seed)*data->pmax;
    q = (int)(params[0]-data->qmin);
    if(alpha<data->p[q]) check = 0;
    count++;
//    printf("params[0]=%.12g, q=%i, alpha=%g, p=%g, pmax=%g\n",params[0],q,alpha,data->p[q],data->pmax);
  }
//  printf("============>params[0]=%.12g, q=%i, alpha=%g, p=%g, pmax=%g\n",params[0],q,alpha,data->p[q],data->pmax);
  //random draws for other parameters
  for(int n=1; n<source->NP; n++) params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);

  return 0;
}

double draw_from_prior(UNUSED struct Data *data, struct Model *model, UNUSED struct Source *source, UNUSED struct Proposal *proposal, double *params, gsl_rng *seed)
{
  
  double logQ = 0.0;
  int n;
  
  //frequency
  n = 0;
  params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
  logQ += model->logPriorVolume[n];
  
  //sky location
  n = 1;
  params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
  logQ += model->logPriorVolume[n];
  
  n = 2;
  params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
  logQ += model->logPriorVolume[n];
  
  //amplitude
  n = 3;
//  logQ += draw_signal_amplitude(data, model, source, proposal, params, seed);
  double q=-INFINITY;
  while(q==-INFINITY)
  {
    params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
    q = evaluate_snr_prior(data, model, params);
  }
  logQ += model->logPriorVolume[n];
  logQ += q;

  
  //inclination
  n = 4;
  params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
  logQ += model->logPriorVolume[n];
  
  //polarization
  n = 5;
  params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
  logQ += model->logPriorVolume[n];
  
  //phase
  n = 6;
  params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
  logQ += model->logPriorVolume[n];
  
  //fdot
  if(source->NP>7)
  {
    n = 7;
    params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
    logQ += model->logPriorVolume[n];
  }
  
  //f-double-dot
  if(source->NP>8)
  {
    n = 8;
    params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
    logQ += model->logPriorVolume[n];
  }
  
  
  return logQ;
}

double draw_from_extrinsic_prior(UNUSED struct Data *data, struct Model *model, UNUSED struct Source *source, UNUSED struct Proposal *proposal, double *params, gsl_rng *seed)
{
  for(int n=1; n<3; n++) params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
  for(int n=4; n<7; n++) params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
  
  for(int j=0; j<source->NP; j++)
  {
    if(params[j]!=params[j]) fprintf(stderr,"draw_from_prior: params[%i]=%g, U[%g,%g]\n",j,params[j],model->prior[j][0],model->prior[j][1]);
  }
  
  double logP = 0.0;
  for(int j=0; j<source->NP; j++) logP += model->logPriorVolume[j];
  
  return logP;
}

double draw_from_galaxy_prior(struct Model *model, struct Prior *prior, double *params, gsl_rng *seed)
{
  double **uniform_prior = model->prior;
  double logP=-INFINITY;
  double alpha=prior->skymaxp;
  
  while(alpha>logP)
  {
    //sky location
    params[1] = model->prior[1][0] + gsl_rng_uniform(seed)*(model->prior[1][1]-model->prior[1][0]);
    params[2] = model->prior[2][0] + gsl_rng_uniform(seed)*(model->prior[2][1]-model->prior[2][0]);
    
    //map costheta and phi to index of skyhist array
    int i = (int)floor((params[1]-uniform_prior[1][0])/prior->dcostheta);
    int j = (int)floor((params[2]-uniform_prior[2][0])/prior->dphi);
    
    int k = i*prior->nphi + j;
    
    logP = prior->skyhist[k];
    alpha = log(gsl_rng_uniform(seed)*exp(prior->skymaxp));
  }
  return logP;
}

double draw_calibration_parameters(struct Data *data, struct Model *model, gsl_rng *seed)
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
        dA = gsl_ran_gaussian(seed,CAL_SIGMA_AMP);
        model->calibration[m]->dampX = dA;
        logP += log(gsl_ran_gaussian_pdf(dA,CAL_SIGMA_AMP));
        
        //phase
        dphi = 0.0;//gsl_ran_gaussian(seed,CAL_SIGMA_PHASE);
        model->calibration[m]->dphiX = dphi;
        logP += 0.0;//log(gsl_ran_gaussian_pdf(dphi,CAL_SIGMA_PHASE));
        
        break;
      case 2:
        
        //amplitude
        dA = gsl_ran_gaussian(seed,CAL_SIGMA_AMP);
        model->calibration[m]->dampA = dA;
        logP += log(gsl_ran_gaussian_pdf(dA,CAL_SIGMA_AMP));
        
        //phase
        dphi = 0.0;//gsl_ran_gaussian(seed,CAL_SIGMA_PHASE);
        model->calibration[m]->dphiA = dphi;
        logP += 0.0;//log(gsl_ran_gaussian_pdf(dphi,CAL_SIGMA_PHASE));

        //amplitude
        dA = gsl_ran_gaussian(seed,CAL_SIGMA_AMP);
        model->calibration[m]->dampE = dA;
        logP += log(gsl_ran_gaussian_pdf(dA,CAL_SIGMA_AMP));
        
        //phase
        dphi = 0.0;//gsl_ran_gaussian(seed,CAL_SIGMA_PHASE);
        model->calibration[m]->dphiE = dphi;
        logP += 0.0;//log(gsl_ran_gaussian_pdf(dphi,CAL_SIGMA_PHASE));

        break;
      default:
        break;
    }//end switch
  }//end loop over segments

  return logP;
}

double draw_signal_amplitude(struct Data *data, struct Model *model, UNUSED struct Source *source, UNUSED struct Proposal *proposal, double *params, gsl_rng *seed)
{
  int k;
  double SNR, den=-1.0, alpha=1.0;
  double max;
    
  double dfac, dfac5;
  
  //SNRPEAK defined in Constants.h
    
  double SNR4 = 4.0*SNRPEAK;
  double SNRsq = 4.0*SNRPEAK*SNRPEAK;
  
  dfac = 1.+SNRPEAK/(SNR4);
  dfac5 = dfac*dfac*dfac*dfac*dfac;
  max = (3.*SNRPEAK)/(SNRsq*dfac5);
  
  int n = (int)floor(params[0] - model->prior[0][0]);
  double sf = 1.0;//sin(f/fstar); //sin(f/f*)
  double sn = model->noise[0]->SnA[n]*model->noise[0]->etaA;
  double sqT = sqrt(data->T);
  
  //Estimate SNR
  double SNm  = sn/(4.*sf*sf);   //Michelson noise
  double SNR1 = sqT/sqrt(SNm); //Michelson SNR (w/ no spread)
  
  double SNRmin = model->prior[3][0]*SNR1;
  double SNRmax = model->prior[3][1]*SNR1;
  
  
  //Draw new SNR
  k = 0;
  SNR = SNRmin + (SNRmax-SNRmin)*gsl_rng_uniform(seed);
  
  dfac = 1.+SNR/(SNR4);
  dfac5 = dfac*dfac*dfac*dfac*dfac;
  
  den = (3.*SNR)/(SNRsq*dfac5);
  
  alpha = max*gsl_rng_uniform(seed);
  
  while(alpha > den)
  {
    
    SNR = SNRmin + (SNRmax-SNRmin)*gsl_rng_uniform(seed);
    
    dfac = 1.+SNR/(SNR4);
    dfac5 = dfac*dfac*dfac*dfac*dfac;
    
    den = (3.*SNR)/(SNRsq*dfac5);
    
    alpha = max*gsl_rng_uniform(seed);
    
    k++;
    
    //you had your chance
    if(k>10000)
    {
      SNR=0.0;
      den=0.0;
      return -INFINITY;
    }
    
    
  }
  
  //SNR defined with Sn(f) but Snf array holdes <n_i^2>
  params[3] = SNR/SNR1;//log(SNR/SNR1);
  
  return log(den);
  
  //  FILE *temp=fopen("prior.dat","a");
  //  fprintf(temp,"%lg\n",params[3]);
  //  fclose(temp);
  
}

double draw_from_fisher(UNUSED struct Data *data, struct Model *model, struct Source *source, UNUSED struct Proposal *proposal, double *params, gsl_rng *seed)
{
  int i,j;
  int NP=source->NP;
  //double sqNP = sqrt((double)source->NP);
  double Amps[NP];
  double jump[NP];
  
  //draw the eigen-jump amplitudes from N[0,1] scaled by evalue & dimension
  for(i=0; i<NP; i++)
  {
    //Amps[i] = gsl_ran_gaussian(seed,1)/sqrt(source->fisher_evalue[i])/sqNP;
    Amps[i] = gsl_ran_gaussian(seed,1)/sqrt(source->fisher_evalue[i]);
    jump[i] = 0.0;
  }
  
  //decompose eigenjumps into paramter directions
  /*for(i=0; i<NP; i++) for (j=0; j<NP; j++)
  {
    jump[j] += Amps[i]*source->fisher_evectr[j][i];
    if(jump[j]!=jump[j])jump[j]=0.0;
  }*/
  
  //choose one eigenvector to jump along
  i = (int)(gsl_rng_uniform(seed)*(double)NP);
  for (j=0; j<NP; j++) jump[j] += Amps[i]*source->fisher_evectr[j][i];
  
  //check jump value, set to small value if singular
  for(i=0; i<NP; i++) if(jump[i]!=jump[i]) jump[i] = 0.01*source->params[i];
  
  //jump from current position
  for(i=0; i<NP; i++) params[i] = source->params[i] + jump[i];
  
  for(int j=0; j<NP; j++)
  {
    if(params[j]!=params[j]) fprintf(stderr,"draw_from_fisher: params[%i]=%g, N[%g,%g]\n",j,params[j],source->params[j],jump[j]);
  }

  //not updating Fisher between moves, proposal is symmetric
  return 0.0;
}

double draw_from_cdf(UNUSED struct Data *data, struct Model *model, struct Source *source, struct Proposal *proposal, double *params, gsl_rng *seed)
{
  int N = proposal->size;
  int NP = source->NP;
  double **cdf = proposal->matrix;

  for(int n=0; n<NP; n++)
  {

    //draw n from U[0,N]
    double c_prime = gsl_rng_uniform(seed)*(double)(N-1);
    
    //map to nearest sample
    int c_minus = (int)floor(c_prime);
    int c_plus  = c_minus+1;
    
    //get value of pdf at cprime
    double p = 1./(cdf[n][c_plus] - cdf[n][c_minus]);
    
    //interpolate to get new parameter
    params[n] = (c_prime - c_minus)*(1./p) + cdf[n][c_minus];
    
    if(params[n]<model->prior[n][0] || params[n]>=model->prior[n][1]) return -INFINITY;
    
  }

  return cdf_density(model, source, proposal);
}
double cdf_density(struct Model *model, struct Source *source, struct Proposal *proposal)
{
  int N = proposal->size;
  int NP = source->NP;
  double **cdf = proposal->matrix;
  double logP=0.0;
  double *params = source->params;
  
  int i,j;
  
  for(int n=0; n<NP; n++)
  {
    if(params[n]<model->prior[n][0] || params[n]>=model->prior[n][1])
      {
       return -INFINITY;

      }

    //find samples either end of p-value
    if(params[n]<cdf[n][0] || params[n]>= cdf[n][N-1])
      {
          return -INFINITY;
          
      }
    else
    {
      i=0;
      while(params[n]>cdf[n][i]) i++;
      j=i+1;
      while(cdf[n][j]==cdf[n][i]) j++;
      logP += log(  ((double)(j-i)/N) /  (cdf[n][j]-cdf[n][i])  );
    }
  }
  return logP;
}
double draw_from_cov(UNUSED struct Data *data, struct Model *model, struct Source *source, struct Proposal *proposal, double *params, gsl_rng *seed)
{
    double ran_no[8],x,choose_dist;
    
    
    x=0.0;
    choose_dist=gsl_rng_uniform(seed);
    
    
    
    for(int n=0; n<8; n++)
    {
        ran_no[n] = gsl_rng_uniform(seed);
    }
    
    
    if (choose_dist > (1.0-proposal->vector[0]))
    {
        for(int n=0; n<8; n++)
        {
            for(int k=0; k<8; k++)
            {
                x += ran_no[k]*proposal->tensor[0][n][k];
            }
            params[n]= x + proposal->matrix[0][n];
            if(params[n]<model->prior[n][0] || params[n]>=model->prior[n][1]) return -INFINITY;
            x=0.0;
        }
    }
    else
    {
        for(int n=0; n<8; n++)
        {
            for(int k=0; k<8; k++)
            {
                x += ran_no[k]*proposal->tensor[1][n][k];
            }
            params[n]= x + proposal->matrix[1][n];
            if(params[n]<model->prior[n][0] || params[n]>=model->prior[n][1]) return -INFINITY;
            x=0.0;
        }
    }

  return cov_density(model, source, proposal);
}
double cov_density(struct Model *model, struct Source *source, struct Proposal *proposal)
{
//  double logP=0.0;
   
    double logP=0.0;
    double x[2][8],scalar1,scalar2,c1[8],c2[8];
    double *params = source->params,tmp_x_min;
    int NP=8;
    long double det1 = proposal->vector[1];
    long double det2 = proposal->vector[3];
    long double b1,b2,p;

    if(params[2]>2*acos(-1))
    {
        params[2]-=2*acos(-1);
    }
    if(params[2]<0.0)
    {
        params[2]+=2*acos(-1);
    }
    if(params[6]>2*acos(-1))
    {
        params[6]-=2*acos(-1);
    }
    if(params[6]<0.0)
    {
        params[6]+=2*acos(-1);
    }
    if(params[5]>acos(-1))
    {
        params[5]-=acos(-1);
    }
    if(params[5]<0)
    {
        params[5]+=acos(-1);
    }

    for(int n=0; n<NP; n++)
    {
        if(params[n]<model->prior[n][0] || params[n]>=model->prior[n][1])
        {
            printf(" \n n=%d and params[n]=%g\n",n,params[n]);
            return -INFINITY;
        }
        
        //////////////////////
        
        
            if (n == 2 || n == 5 || n == 6)
            {
                if (n == 2)
                {
                    double tmp_x[3]={fabs(params[n]-proposal->matrix[0][n]),fabs(params[n]-2*acos(-1)-proposal->matrix[0][n]),fabs(params[n]+2*acos(-1)-proposal->matrix[0][n])};
                    tmp_x_min=tmp_x[0];
                    for(int h=1; h<3; h++)
                    {
                        if (tmp_x[h]<tmp_x_min)tmp_x_min=tmp_x[h];
                    }
                    if(tmp_x_min == fabs(params[n]-proposal->matrix[0][n]))x[0][n]=params[n]-proposal->matrix[0][n];
                    
                    else if(tmp_x_min == fabs(params[n]-2*acos(-1)-proposal->matrix[0][n]))
                    {
                        x[0][n]=params[n]-2*acos(-1)-proposal->matrix[0][n];
                    }
                    
                    else
                    {
                        x[0][n]=params[n]+2*acos(-1)-proposal->matrix[0][n];
                    }
                    
                }
                if (n == 5)
                {
                    double tmp_x[3]={fabs(params[n]-proposal->matrix[0][n]),fabs(params[n]-acos(-1)-proposal->matrix[0][n]),fabs(params[n]+acos(-1)-proposal->matrix[0][n])};
                    tmp_x_min=tmp_x[0];
                    for(int h=1; h<3; h++)
                    {
                        if (tmp_x[h]<tmp_x_min)tmp_x_min=tmp_x[h];
                    }
                    if(tmp_x_min == fabs(params[n]-proposal->matrix[0][n]))x[0][n]=params[n]-proposal->matrix[0][n];
                    
                    else if(tmp_x_min == fabs(params[n]-acos(-1)-proposal->matrix[0][n]))
                    {
                        x[0][n]=params[n]-acos(-1)-proposal->matrix[0][n];
                    }
                    
                    else
                    {
                        x[0][n]=params[n]+acos(-1)-proposal->matrix[0][n];
                    }
                    
                }
                if (n == 6)
                {
                    double tmp_x[3]={fabs(params[n]-proposal->matrix[0][n]),fabs(params[n]-2*acos(-1)-proposal->matrix[0][n]),fabs(params[n]+2*acos(-1)-proposal->matrix[0][n])};
                    tmp_x_min=tmp_x[0];
                    for(int h=1; h<3; h++)
                    {
                        if (tmp_x[h]<tmp_x_min)tmp_x_min=tmp_x[h];
                    }
                    if(tmp_x_min == fabs(params[n]-proposal->matrix[0][n]))x[0][n]=params[n]-proposal->matrix[0][n];
                    
                    else if(tmp_x_min == fabs(params[n]-2*acos(-1)-proposal->matrix[0][n]))
                    {
                        x[0][n]=params[n]-2*acos(-1)-proposal->matrix[0][n];
                    }
                    
                    else
                    {
                        x[0][n]=params[n]+2*acos(-1)-proposal->matrix[0][n];
                    }
                    
                }
            }
            else
            {
                x[0][n]=params[n]-proposal->matrix[0][n];
            }
        

        
        
        //////////////////////
        
        if(proposal->vector[0] != 1.0)
        {
            
            
            
            if (n == 2 || n == 5 || n == 6)
            {
                if (n == 2)
                {
                    double tmp_x[3]={fabs(params[n]-proposal->matrix[1][n]),fabs(params[n]-2*acos(-1)-proposal->matrix[1][n]),fabs(params[n]+2*acos(-1)-proposal->matrix[1][n])};
                    tmp_x_min=tmp_x[0];
                    for(int h=1; h<3; h++)
                    {
                        if (tmp_x[h]<tmp_x_min)tmp_x_min=tmp_x[h];
                    }
                    if(tmp_x_min == fabs(params[n]-proposal->matrix[1][n]))x[1][n]=params[n]-proposal->matrix[1][n];
                    
                    else if(tmp_x_min == fabs(params[n]-2*acos(-1)-proposal->matrix[1][n]))
                    {
                        x[1][n]=params[n]-2*acos(-1)-proposal->matrix[1][n];
                    }
                    
                    else
                    {
                        x[1][n]=params[n]+2*acos(-1)-proposal->matrix[1][n];
                    }
                    
                }
                if (n == 5)
                {
                    double tmp_x[3]={fabs(params[n]-proposal->matrix[1][n]),fabs(params[n]-acos(-1)-proposal->matrix[1][n]),fabs(params[n]+acos(-1)-proposal->matrix[1][n])};
                    tmp_x_min=tmp_x[0];
                    for(int h=1; h<3; h++)
                    {
                        if (tmp_x[h]<tmp_x_min)tmp_x_min=tmp_x[h];
                    }
                    if(tmp_x_min == fabs(params[n]-proposal->matrix[1][n]))x[1][n]=params[n]-proposal->matrix[1][n];
                    
                    else if(tmp_x_min == fabs(params[n]-acos(-1)-proposal->matrix[1][n]))
                    {
                        x[1][n]=params[n]-acos(-1)-proposal->matrix[1][n];
                    }
                    
                    else
                    {
                        x[1][n]=params[n]+acos(-1)-proposal->matrix[1][n];
                    }
                    
                }
                if (n == 6)
                {
                    double tmp_x[3]={fabs(params[n]-proposal->matrix[1][n]),fabs(params[n]-2*acos(-1)-proposal->matrix[1][n]),fabs(params[n]+2*acos(-1)-proposal->matrix[1][n])};
                    tmp_x_min=tmp_x[0];
                    for(int h=1; h<3; h++)
                    {
                        if (tmp_x[h]<tmp_x_min)tmp_x_min=tmp_x[h];
                    }
                    if(tmp_x_min == fabs(params[n]-proposal->matrix[1][n]))x[1][n]=params[n]-proposal->matrix[1][n];
                    
                    else if(tmp_x_min == fabs(params[n]-2*acos(-1)-proposal->matrix[1][n]))
                    {
                        x[1][n]=params[n]-2*acos(-1)-proposal->matrix[1][n];
                    }
                    
                    else
                    {
                        x[1][n]=params[n]+2*acos(-1)-proposal->matrix[1][n];
                    }
                    
                }
            }
            else
            {
                x[1][n]=params[n]-proposal->matrix[1][n];
            }
        }
    }
    

    
    for(int n=0; n<NP; n++)
    {
        c1[n]=0;
        for(int k=0; k<NP; k++)
        {
            c1[n]+=x[0][k]*proposal->tensor[2][k][n];
        }
    }
    scalar1=0.0;
    
    
    if(proposal->vector[0] != 1.0)
    {
        for(int n=0; n<NP; n++)
        {
            c2[n]=0;
            for(int k=0; k<NP; k++)
            {
                c2[n]+=x[1][k]*proposal->tensor[3][k][n];
            }
        }
        scalar2=0.0;
    }


    
    
    for(int n=0; n<NP; n++)
    {
        scalar1+=x[0][n]*c1[n];
        if(proposal->vector[0] != 1.0)scalar2+=x[1][n]*c2[n];
    }



    b1=exp(-0.5*scalar1);
    if(proposal->vector[0] != 1.0)b2=exp(-0.5*scalar2);
    p=(proposal->vector[0])*b1/(sqrt((2*acos(-1.0))*(2*acos(-1.0))*det1));
    if(proposal->vector[0] != 1.0)
    {
        p+=(1-proposal->vector[0])*b2/(sqrt((2*acos(-1.0))*(2*acos(-1.0))*det2));
    }
    logP += log(p);

    
  return logP;
}

double fm_shift(struct Data *data, struct Model *model, struct Source *source, struct Proposal *proposal, double *params, gsl_rng *seed)
{
  //doppler modulation frequency (in bins)
  double fm = data->T/YEAR;
  
  //update all parameters with a draw from the fisher
  if(gsl_rng_uniform(seed)<0.5) draw_from_fisher(data, model, source, proposal, params, seed);
  else for(int n=1; n<source->NP; n++) params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);

  //perturb frequency by 1 fm
  double scale = floor(6*gsl_ran_gaussian(seed,1));
  
  params[0] += scale*fm;
  //params[7] += scale*fm*fm;
  
  //fm shift is symmetric
  return 0.0;
}

double t0_shift(UNUSED struct Data *data, struct Model *model, UNUSED struct Source *source, UNUSED struct Proposal *proposal, UNUSED double *params, gsl_rng *seed)
{
  for(int i=1; i<model->NT; i++)
  {
    //uniform draw
    if(gsl_rng_uniform(seed) < 0.5 )
      model->t0[i] = model->t0_min[i] + gsl_rng_uniform(seed)*(model->t0_max[i] - model->t0_min[i]);
    
    //gaussian draw
    else if (gsl_rng_uniform(seed) < 0.5 )
      model->t0[i] += 1.0*gsl_ran_gaussian(seed,1);
    else
      model->t0[i] += 0.1*gsl_ran_gaussian(seed,1);

    
    //t0 shift is symmetric
    if(model->t0[i] < model->t0_min[i] || model->t0[i] >= model->t0_max[i]) return -INFINITY;
  }
  
  return 0.0;
}

void initialize_proposal(struct Orbit *orbit, struct Data *data, struct Chain *chain, struct Flags *flags, struct Proposal **proposal, int NMAX)
{
  int NC = chain->NC;
  int nl=0;
  int Ncov=2;
  double check=0.0;
  double junk, ten[2][8][8],params[8],*ptr_params,(**ptr_ten_out);
  double f1,f2,f3,f4,f5,f6,f7,f8;
  FILE *fptr;
  struct Model *temp;
    
  for(int i=0; i<chain->NP+1; i++)
  {
    
    proposal[i]->trial  = malloc(NC*sizeof(int));
    proposal[i]->accept = malloc(NC*sizeof(int));
    
    for(int ic=0; ic<NC; ic++)
    {
      proposal[i]->trial[ic]  = 1;
      proposal[i]->accept[ic] = 0;
    }
    
    switch(i)
    {
      case 0:
        /*
         delayed rejection proposal does not fit in with others' protocal
         -must have zero weight
         -must be last in the list
         */
        sprintf(proposal[i]->name,"delayed rejection");
        proposal[i]->weight = 0.0;
        break;
        
      case 1:
        sprintf(proposal[i]->name,"prior");
        proposal[i]->function = &draw_from_prior;
        proposal[i]->weight = 0.0;
        check+=proposal[i]->weight;
        break;
      case 2:
        sprintf(proposal[i]->name,"fstat");
        
        
        setup_fstatistic_proposal(orbit, data, flags, proposal[i]);

        proposal[i]->function = &jump_from_fstatistic;
        proposal[i]->weight = 0.2;
//        proposal[i]->weight = 0.0;
        check+=proposal[i]->weight;
        break;
      case 3:
        sprintf(proposal[i]->name,"extrinsic prior");
        proposal[i]->function = &draw_from_extrinsic_prior;
        proposal[i]->weight = 0.0;
        check+=proposal[i]->weight;
        break;
      case 4:
        sprintf(proposal[i]->name,"fisher");
        proposal[i]->function = &draw_from_fisher;
        proposal[i]->weight = 1.0; //that's a 1 all right.  don't panic
        break;
      case 5:
        sprintf(proposal[i]->name,"fm shift");
        proposal[i]->function = &fm_shift;
        proposal[i]->weight = 0.2;
//        proposal[i]->weight = 0.0;
        check+=proposal[i]->weight;
        break;
      case 6:
        sprintf(proposal[i]->name,"cov draw");
        proposal[i]->function = &draw_from_cov;
        proposal[i]->weight = 0.2;
//        proposal[i]->weight = 1.0;
        check+=proposal[i]->weight;
        //parse covariance file
        
        
        //        printf("\ndata->NP is %d \n",data->NP);
        proposal[i]->vector = malloc(4*sizeof(double));
        
        proposal[i]->matrix = malloc(Ncov*sizeof(double*));
        for(int j=0; j<Ncov; j++) proposal[i]->matrix[j] = malloc(data->NP * sizeof(double));
        
        proposal[i]->tensor = malloc((Ncov+2)*sizeof(double**));
        for(int j=0; j<Ncov+2; j++) proposal[i]->tensor[j] = malloc(data->NP * sizeof(double**));
        for(int k=0; k<data->NP; k++) proposal[i]->tensor[0][k] = malloc(data->NP * sizeof(double*));
        for(int k=0; k<data->NP; k++) proposal[i]->tensor[1][k] = malloc(data->NP * sizeof(double*));
        
        for(int k=0; k<data->NP; k++) proposal[i]->tensor[2][k] = malloc(data->NP * sizeof(double*));
        for(int k=0; k<data->NP; k++) proposal[i]->tensor[3][k] = malloc(data->NP * sizeof(double*));
        
        
        
        fptr = fopen(flags->covFile,"r");
        printf("reading covariance.txt...\n");
        
        
        while(!feof(fptr))
        {
            for(int j=0; j<8; j++) fscanf(fptr,"%lg",&junk);
            nl++;
        }
        rewind(fptr);
        nl--;
        
        for(int n=0; n<nl; n++)
        {
            fscanf(fptr, "%lg%lg%lg%lg%lg%lg%lg%lg\n", &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8);
            if(n == 0)
            {
                //alpha
                proposal[i]->vector[0]=f1;
                //det(cov(Sigma1))
                proposal[i]->vector[1]=f2;
            }
            if(n == 1)
            {
                proposal[i]->matrix[0][0]=f1;
                proposal[i]->matrix[0][1]=f2;
                proposal[i]->matrix[0][2]=f3;
                proposal[i]->matrix[0][3]=f4;
                proposal[i]->matrix[0][4]=f5;
                proposal[i]->matrix[0][5]=f6;
                proposal[i]->matrix[0][6]=f7;
                proposal[i]->matrix[0][7]=f8;
            }
            if(n>1 && n<=9)
            {
                ten[0][n-2][0]=f1;
                ten[0][n-2][1]=f2;
                ten[0][n-2][2]=f3;
                ten[0][n-2][3]=f4;
                ten[0][n-2][4]=f5;
                ten[0][n-2][5]=f6;
                ten[0][n-2][6]=f7;
                ten[0][n-2][7]=f8;
            }
            if(n == 10 && proposal[i]->vector[0] != 1.0)
            {
                //(1-alpha)
                proposal[i]->vector[2]=f1;
                //det(cov(Sigma2))
                proposal[i]->vector[3]=f2;
            }
            if(n == 11 && proposal[i]->vector[0] != 1.0)
            {
                proposal[i]->matrix[1][0]=f1;
                proposal[i]->matrix[1][1]=f2;
                proposal[i]->matrix[1][2]=f3;
                proposal[i]->matrix[1][3]=f4;
                proposal[i]->matrix[1][4]=f5;
                proposal[i]->matrix[1][5]=f6;
                proposal[i]->matrix[1][6]=f7;
                proposal[i]->matrix[1][7]=f8;
            }
            if(n>11 && n<=19 && proposal[i]->vector[0] != 1.0)
            {
                ten[1][n-12][0]=f1;
                ten[1][n-12][1]=f2;
                ten[1][n-12][2]=f3;
                ten[1][n-12][3]=f4;
                ten[1][n-12][4]=f5;
                ten[1][n-12][5]=f6;
                ten[1][n-12][6]=f7;
                ten[1][n-12][7]=f8;
            }
        }
        
        ptr_params=params;
        ptr_ten_out=proposal[i]->tensor[0];
        cholesky_decomp(ten[0],ptr_ten_out,data->NP);
        ptr_ten_out=proposal[i]->tensor[2];
        invert_matrix2(ten[0],ptr_ten_out,data->NP);

        if(proposal[i]->vector[0] != 1.0)
        {
            ptr_ten_out=proposal[i]->tensor[1];
            cholesky_decomp(ten[1],ptr_ten_out,data->NP);
            ptr_ten_out=proposal[i]->tensor[3];
            invert_matrix2(ten[1],ptr_ten_out,data->NP);
        }
        fclose(fptr);
        break;
        
      case 7:
        sprintf(proposal[i]->name,"cdf draw");
        proposal[i]->function = &draw_from_cdf;
        proposal[i]->weight = 0.0;
//        proposal[i]->weight = 0.0;
        check+=proposal[i]->weight;
        //parse chain file
        fptr = fopen(flags->cdfFile,"r");
        proposal[i]->size=0;
        while(!feof(fptr))
        {
            //fscanf(fptr,"%lg",&junk);
            for(int j=0; j<data->NP; j++) fscanf(fptr,"%lg",&junk);
            proposal[i]->size++;
        }
        rewind(fptr);
        proposal[i]->size--;
        proposal[i]->vector = malloc(proposal[i]->size * sizeof(double));
        proposal[i]->matrix = malloc(data->NP * sizeof(double*));
        for(int j=0; j<data->NP; j++) proposal[i]->matrix[j] = malloc(proposal[i]->size * sizeof(double));
        
        temp = malloc(sizeof(struct Model));
        alloc_model(temp,NMAX,data->N,data->Nchannel, data->NP, data->NT);
        
        for(int n=0; n<proposal[i]->size; n++)
        {
            //fscanf(fptr,"%lg",&junk);
            scan_source_params(data, temp->source[0], fptr);
            for(int j=0; j<data->NP; j++) proposal[i]->matrix[j][n] = temp->source[0]->params[j];
        }
        free_model(temp);
        
        //now sort each row of the matrix
        for(int j=0; j<data->NP; j++)
        {
            //fill up proposal vector
            for(int n=0; n<proposal[i]->size; n++)
                proposal[i]->vector[n] = proposal[i]->matrix[j][n];
            
            //sort it
            gsl_sort(proposal[i]->vector,1, proposal[i]->size);
            
            //replace that row of the matrix
            for(int n=0; n<proposal[i]->size; n++)
                proposal[i]->matrix[j][n] = proposal[i]->vector[n];
        }
        
        free(proposal[i]->vector);
        fclose(fptr);
        break;
            
      default:
        break;
    }
  }
  //Fisher proposal fills in the cracks
  proposal[4]->weight -= check;
  
  if(proposal[4]->weight<0.0)
  {
    fprintf(stderr,"Proposal weights not normalized (line %d of file %s)\n",__LINE__,__FILE__);
    exit(1);
  }
}



void setup_fstatistic_proposal(struct Orbit *orbit, struct Data *data, struct Flags *flags, struct Proposal *proposal)
{
  /*
   -Step through 3D grid in frequency and sky location.
   - frequency resolution hard-coded to 1/4 of a bin
   - sky location resolution hard-coded to 30x30 bins
   -Compute F-statistic in each cell of the grid
   - cap F-statistic at SNRmax=20
   - normalize to make it a proper proposal (this part is a pain to get right...)
   */
  
  //matrix to hold maximized extrinsic parameters
  double *Fparams = malloc(4*sizeof(double));
  
  //grid sizes
  int n_f     = 4*data->N;
  int n_theta = 30;
  int n_phi   = 30;
  if(flags->debug)
  {
    n_f/=4;
    n_theta/=3;
    n_phi/=3;
  }
  
  double d_f     = (double)data->N/(double)n_f;
  double d_theta = 2./(double)n_theta;
  double d_phi   = PI2/(double)n_phi;

  fprintf(stdout,"\n============ F-Statistic sky proposal ============\n");
  fprintf(stdout,"   n_f     = %i\n",n_f);
  fprintf(stdout,"   n_theta = %i\n",n_theta);
  fprintf(stdout,"   n_phi   = %i\n",n_phi);
  fprintf(stdout,"   cap     = %g\n",SNRCAP);

  double fdot = 0.0; //TODO: what to do about fdot...
  
  //F-statistic for TDI variabls
  double logL_X,logL_AE;
  
  //allocate memory in proposal structure and pack up metadata
  /*
   proposal->matrix is 3x2 matrix.
   -rows are parameters {f,theta,phi}
   -columns are bin number and width {n,d}
   */
  proposal->matrix = malloc(3*sizeof(double *));
  for(int i=0; i<3; i++)
    proposal->matrix[i] = malloc(2*sizeof(double));

  proposal->matrix[0][0] = (double)n_f;
  proposal->matrix[0][1] = d_f;
  
  proposal->matrix[1][0] = (double)n_theta;
  proposal->matrix[1][1] = d_theta;
  
  proposal->matrix[2][0] = (double)n_phi;
  proposal->matrix[2][1] = d_phi;

  /*
   proposal->tensor holds the proposal density
   - n_f x n_theta x n_phi "tensor"
   */
  proposal->tensor = malloc(n_f*sizeof(double **));
  for(int i=0; i<n_f; i++)
  {
    proposal->tensor[i] = malloc(n_theta*sizeof(double *));
    for(int j=0; j<n_theta; j++)
    {
      proposal->tensor[i][j] = malloc(n_phi*sizeof(double));
      for(int k=0; k<n_phi; k++)
      {
        proposal->tensor[i][j][k] = 1.0;
      }
    }
  }
  
  double norm = 0.0;
  double maxLogL = -1e60;
  
  //loop over sub-bins
  for(int i=0; i<n_f; i++)
  {
    if(i%(n_f/100)==0)printProgress((double)i/(double)n_f);

    double q = (double)(data->qmin) + (double)(i)*d_f;
    double f = q/data->T;
    
    
    //loop over colatitude bins
    for (int j=0; j<n_theta; j++)
    {
      double theta = acos(-1*(-1. + (double)j*d_theta));
      
      //loop over longitude bins
      for(int k=0; k<n_phi; k++)
      {
        double phi = PI2 - (double)k*d_phi;
        
        if(i>0 && i<n_f-1)
        {
          get_Fstat_logL(orbit, data, f, fdot, theta, phi, &logL_X, &logL_AE, Fparams);
                    
          if(logL_AE > maxLogL) maxLogL = logL_AE;
          //if(logL_AE > SNRCAP)  logL_AE = SNRCAP;
          
          proposal->tensor[i][j][k] = logL_AE;//sqrt(2*logL_AE);
        }

        norm += proposal->tensor[i][j][k];
        
      }//end loop over longitude bins
    }//end loop over colatitude bins
  }//end loop over sub-bins
  
  //normalize
  proposal->norm = (n_f*n_theta*n_phi)/norm;
  proposal->maxp = maxLogL*proposal->norm;//sqrt(2.*maxLogL)*proposal->norm;
  
  for(int i=0; i<n_f; i++) for(int j=0; j<n_theta; j++) for(int k=0; k<n_phi; k++) proposal->tensor[i][j][k] *= proposal->norm;
  
  if(flags->verbose)
  {
    mkdir("fstat",S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    char filename[128];
    
    for(int i=0; i<n_f; i++)
    {
      sprintf(filename,"fstat/skymap_%05d.dat",i);
      FILE *fptr=fopen(filename,"w");
      for (int j=0; j<n_theta; j++)
      {
        double theta = acos(-1. + (double)j*d_theta);

        for(int k=0; k<n_phi; k++)
        {
          double phi = (double)k*d_phi;

          fprintf(fptr,"%.12g %.12g %.12g\n", cos(theta), phi, proposal->tensor[i][j][k]);
        }
        fprintf(fptr,"\n");
      }
      fclose(fptr);
    }
    write_Fstat_animation(data->qmin/data->T, data->T,proposal);
  }
  
  free(Fparams);
  fprintf(stdout,"\n================================================\n\n");
  fflush(stdout);
}

double draw_from_fstatistic(struct Data *data, UNUSED struct Model *model, UNUSED struct Source *source, struct Proposal *proposal, double *params, gsl_rng *seed)
{
  double logP = 0.0;
  
  int n_f     = (int)proposal->matrix[0][0];
  int n_theta = (int)proposal->matrix[1][0];
  int n_phi   = (int)proposal->matrix[2][0];
  
  double d_f     = proposal->matrix[0][1];
  double d_theta = proposal->matrix[1][1];
  double d_phi   = proposal->matrix[2][1];

  double q,costheta,phi,p,alpha;

  double i,j,k;
  
  //first draw from prior
  for(int n=0; n<source->NP; n++)
  {
    params[n] = model->prior[n][0] + gsl_rng_uniform(seed)*(model->prior[n][1]-model->prior[n][0]);
    logP += model->logPriorVolume[n];
  }
  //put back 
  
  
  //now rejection sample on f,theta,phi
  int check=1;
  while(check)
  {
    i = gsl_rng_uniform(seed)*n_f;
    j = gsl_rng_uniform(seed)*n_theta;
    k = gsl_rng_uniform(seed)*n_phi;
    
    q        = (double)(data->qmin) + i*d_f;
    costheta = -1. + j*d_theta;
    phi      = k*d_phi;
    
    //printf("q=%g, costheta=%g, phi=%g --> i=%i, j=%i, k=%i\n",q,costheta,phi,(int)i,(int)j,(int)k);
    
    p = proposal->tensor[(int)i][(int)j][(int)k];
    alpha = gsl_rng_uniform(seed)*proposal->maxp;
    
    if(p>alpha)check=0;

  }
  
  params[0] = q;
  params[1] = costheta;
  params[2] = phi;
  
  logP += log(p);
  
  return logP;
}

double jump_from_fstatistic(struct Data *data, struct Model *model, struct Source *source, struct Proposal *proposal, double *params, gsl_rng *seed)
{
  double logP = 0.0;
  
  int n_f     = (int)proposal->matrix[0][0];
  int n_theta = (int)proposal->matrix[1][0];
  int n_phi   = (int)proposal->matrix[2][0];
  
  double d_f     = proposal->matrix[0][1];
  double d_theta = proposal->matrix[1][1];
  double d_phi   = proposal->matrix[2][1];
  
  double q,costheta,phi,p,alpha;
  
  double i=0,j,k;
  
  /* half the time do an fm shift, half the time completely rebott frequency */
  int fmFlag = 0;
  if(gsl_rng_uniform(seed)<-0.5) fmFlag=1;
  
  if(fmFlag)
  {
    fm_shift(data, model, source, proposal, params, seed);
    
    q = params[0];
    i = floor((q-data->qmin)/d_f);
    
    if(i<0.0 || i>n_f-1) return -INFINITY;
  }
  
  //now rejection sample on f,theta,phi
  int check=1;
  while(check)
  {
    if(!fmFlag)i = gsl_rng_uniform(seed)*n_f;
    j = gsl_rng_uniform(seed)*n_theta;
    k = gsl_rng_uniform(seed)*n_phi;
    
    q        = (double)(data->qmin) + i*d_f;
    costheta = -1. + j*d_theta;
    phi      = k*d_phi;
    
    //printf("q=%g, costheta=%g, phi=%g --> i=%i, j=%i, k=%i\n",q,costheta,phi,(int)i,(int)j,(int)k);
    if(
       ((int)i < 0 || (int)i > n_f-1)     ||
       ((int)j < 0 || (int)j > n_theta-1) ||
       ((int)k < 0 || (int)k > n_phi-1)
       )
    {
      fprintf(stdout,"%i (%i) %i (%i) %i (%i)\n",(int)i,n_f,(int)j,n_theta,(int)k,n_phi);
      fflush(stdout);
      return -INFINITY;
    }
    else
      p = proposal->tensor[(int)i][(int)j][(int)k];
    alpha = gsl_rng_uniform(seed)*proposal->maxp;
    
    if(p>alpha)check=0;
    
  }
  
  params[0] = q;
  params[1] = costheta;
  params[2] = phi;
  
  logP = log(p);
  
  return logP;
}


double evaluate_fstatistic_proposal(struct Data *data, struct Proposal *proposal, double *params)
{  
  double d_f     = proposal->matrix[0][1];
  double d_theta = proposal->matrix[1][1];
  double d_phi   = proposal->matrix[2][1];
  
  int n_f     = (int)proposal->matrix[0][0];
  int n_theta = (int)proposal->matrix[1][0];
  int n_phi   = (int)proposal->matrix[2][0];

  int i = (int)floor((params[0] - data->qmin)/d_f);
  int j = (int)floor((params[1] - -1)/d_theta);
  int k = (int)floor((params[2])/d_phi);
  
  if      (i<0 || i>=n_f    ) return -INFINITY;
  else if (j<0 || j>=n_theta) return -INFINITY;
  else if (k<0 || k>=n_phi  ) return -INFINITY;
  else return log(proposal->tensor[i][j][k]);
}



