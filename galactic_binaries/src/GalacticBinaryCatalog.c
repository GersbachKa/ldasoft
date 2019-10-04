//
//  GalacticBinaryCatalog.c
//  
//
//  Created by Littenberg, Tyson B. (MSFC-ST12) on 9/17/19.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

/* ===============  PROTOTYPE DECLARATIONS FOR INTERNAL FUNCTIONS ========= */

#include "LISA.h"
#include "Constants.h"
#include "GalacticBinary.h"
#include "GalacticBinaryIO.h"
#include "GalacticBinaryMath.h"
#include "GalacticBinaryModel.h"
#include "GalacticBinaryWaveform.h"
#include "GalacticBinaryCatalog.h"

/* ============================  MAIN PROGRAM  ============================ */

int main(int argc, char *argv[])
{

    
    FILE *chain_file1;
    int NMAXa = 1;   //max number of frequency & time segments

    
    /* Allocate data structures */
    struct Flags *flags = malloc(sizeof(struct Flags));
    struct Orbit *orbit = malloc(sizeof(struct Orbit));
    struct Chain *chain = malloc(sizeof(struct Chain));
    struct Data  **data_tmp = malloc(sizeof(struct Data*)*NMAXa); //data[NF]
    
    
//   Parse command line and set defaults/flags
    for(int i=0; i<NMAXa; i++)
    {
        data_tmp[i] = malloc(sizeof(struct Data));
        data_tmp[i]->t0   = malloc( NMAXa * sizeof(double) );
    }
    parse(argc,argv,data_tmp,orbit,flags,chain,NMAXa);
    alloc_data(data_tmp, flags);
    struct Data *data  = data_tmp[0];
    
    //Orbits
    /* Load spacecraft ephemerides */
    switch(flags->orbit)
    {
        case 0:
            initialize_analytic_orbit(orbit);
            break;
        case 1:
            initialize_numeric_orbit(orbit);
            break;
        default:
            fprintf(stderr,"unsupported orbit type\n");
            return(1);
            break;
    }
    
    chain_file1 = fopen(flags->matchInfile1,"r");

    if ( chain_file1 == NULL )
    {
        printf("match-in1 is null\n");
    }
    

    double f0,dfdt,costheta,phi,amp,cosi,phi0,psi;
    double junk;

    int N=0;
    while(!feof(chain_file1))
    {
        fscanf(chain_file1,"%lg %lg %lg %lg %lg %lg %lg %lg",&junk,&junk,&junk,&junk,&junk,&junk,&junk,&junk);
        N++;
    }
    
    rewind(chain_file1);
    
    N--;
  
  //selection criteria for catalog entries
    int matchFlag;          //track if sample matches any entries
    double Match;     //match between pairs of waveforms
    double tolerance = 0.900; //tolerance on match to be considered associated
    double dqmax = 1e-6;      //maximum frequency separation to try match calculation (in frequency bins)
  

  //Book-keeping
  int DMAX = data->DMAX; //maximum number of sources per chain sample (needs to be read in from above)
  int IMAX = N/DMAX;     //maximum number of chain samples (needs to be read in from above)
  int NMAX = N; //(absurd) upper limit on total number of catalog entries
  

    
    struct Catalog *catalog = NULL;
    catalog = malloc(sizeof(struct Catalog));
    catalog->N = 0; //start with 0 sources in catalog
    catalog->entry = malloc(NMAX*sizeof(struct Entry*));
    struct Entry *entry = NULL;
    entry = malloc(sizeof(struct Entry));
    alloc_entry(entry,IMAX);
    entry->source[0] = malloc(sizeof(struct Source));
    alloc_source(entry->source[0], data->N,2,data->NP);

    
    //shortcut to entry pointer in catalog structure
//    struct Entry *entry = NULL;
//    entry = malloc(sizeof(struct Entry));
//    alloc_entry(entry,IMAX);
//    entry->source[0] = malloc(sizeof(struct Source));
//    alloc_source(entry->source[0], data->N,2,data->NP);

    
    struct Source *sample = NULL;
    sample = malloc(sizeof *sample);
    alloc_source(sample, data->N,2,data->NP);

//  /******************************************************************/
//  /*             Allocate & Initialize Instrument Model             */
//  /******************************************************************/
    
    struct Noise *noise = NULL;
    noise = malloc(flags->NT*sizeof(struct Noise));
    alloc_noise(noise, data->N);
    
    struct TDI *tdi = NULL;
    tdi = malloc(data->NP*sizeof(struct TDI));
    alloc_tdi(tdi, data->N, data->Nchannel);
    
    //Noise model
    //Get noise spectrum for data segment
    for(int n=0; n<data->N; n++)
    {
        double f = data->fmin + (double)(n)/data->T;
        if(strcmp(data->format,"phase")==0)
        {
            noise->SnA[n] = AEnoise(orbit->L, orbit->fstar, f);
            noise->SnE[n] = AEnoise(orbit->L, orbit->fstar, f);
        }
        else if(strcmp(data->format,"frequency")==0)
        {
            noise->SnA[n] = AEnoise_FF(orbit->L, orbit->fstar, f);
            noise->SnE[n] = AEnoise_FF(orbit->L, orbit->fstar, f);
        }
        else
        {
            fprintf(stderr,"Unsupported data format %s",data->format);
            exit(1);
        }
    }

    
//  /******************************************************************/
//  /*        First sample of the chain initializes entry list        */
//  /******************************************************************/
  for(int d=0; d<DMAX; d++)
  {
      
      //parse source in first sample of chain file
      fscanf(chain_file1,"%lg %lg %lg %lg %lg %lg %lg %lg",&f0,&dfdt,&amp,&phi,&costheta,&cosi,&psi,&phi0);
//        //map polarization angle into [0:pi], preserving relation to phi0//DON'T DO AGAIN, already done
//        if(psi>M_PI) psi  -= M_PI;
//        if(phi0>PI2) phi0 -= PI2;//DON'T DO AGAIN, already done
      
        //map parameters to vector
        sample->f0       = f0;
        sample->dfdt     = dfdt;
        sample->costheta = costheta;
        sample->phi      = phi;
        sample->amp      = amp;
        sample->cosi     = cosi;
        sample->phi0     = phi0;
        sample->psi      = psi;
        if(sample->NP>8)sample->d2fdt2 = 11.0/3.0*dfdt*dfdt/f0;
//        //sample->d2fdt2 = fddot;
        map_params_to_array(sample, sample->params, data->T);

        //Book-keeping of injection time-frequency volume
        galactic_binary_alignment(orbit, data, sample);

       //calculate waveform model of sample
        galactic_binary(orbit, data->format, data->T, data->t0[0], sample->params, data->NP, sample->tdi->X, sample->tdi->A, sample->tdi->E, sample->BW, 2);
        


//  add new source to catalog
    create_new_source(catalog, sample, IMAX, data->N, 2, data->NP);
  }


//  /******************************************************************/
//  /*            Now loop over the rest of the chain file            */
//  /******************************************************************/
  for(int i=1; i<IMAX; i++)
  {
    //check each source in chain sample
    for(int d=0; d<DMAX; d++)
    {
        //parse source parameters
        fscanf(chain_file1,"%lg %lg %lg %lg %lg %lg %lg %lg",&f0,&dfdt,&amp,&phi,&costheta,&cosi,&psi,&phi0);
        //populate sample structure with current parameters
        sample->f0       = f0;
        sample->dfdt     = dfdt;
        sample->costheta = costheta;
        sample->phi      = phi;
        sample->amp      = amp;
        sample->cosi     = cosi;
        sample->phi0     = phi0;
        sample->psi      = psi;
        if(sample->NP>8)sample->d2fdt2 = 11.0/3.0*dfdt*dfdt/f0;
        //sample->d2fdt2 = fddot;
        map_params_to_array(sample, sample->params, data->T);

        //calculate waveform model of sample
        //Book-keeping of injection time-frequency volume
        galactic_binary_alignment(orbit, data, sample);

        //calculate waveform model of sample
        galactic_binary(orbit, data->format, data->T, data->t0[0], sample->params, data->NP, sample->tdi->X, sample->tdi->A, sample->tdi->E, sample->BW, 2);
        
      //calculate match of sample and all entries
      matchFlag = 0;
      for(int n=0; n<catalog->N; n++)
      {
        entry = catalog->entry[n];
        
        //check frequency separation
        double q_entry  = entry->source[0]->f0;
        double q_sample = sample->f0;
        if( fabs(q_entry-q_sample) > dqmax ) continue;

        //calculate match
        Match = waveform_match(sample, entry->source[0], noise);


        if(Match > tolerance)
        {
          matchFlag = 1;
          //append sample to entry
          append_sample_to_entry(entry, sample, IMAX, data->N, 2, data->NP);

          //stop looping over entries in catalog
          break;
        }
//
      }//end loop over catalog entries
//
      //if the match tolerence is never met, add as new source
      if(!matchFlag)create_new_source(catalog, sample, IMAX, data->N, 2, data->NP);
//
    }//end loop over sources in chain sample

  }//end loop over chain
  fclose(chain_file1);

//  /******************************************************************/
//  /*             Select entries that have enough weight             */
//  /******************************************************************/
  double weight;
  double weight_threshold = .2; //how many samples must an entry have to any hope of counting?

  int detections = 0;           //number of entries that meet the weight threshold
  int detection_index[NMAX];    //list of entry indicies for detected sources

  for(int n=0; n<catalog->N; n++)
  {

    weight = (double)catalog->entry[n]->I/(double)IMAX;
    if(weight > weight_threshold)
    {
      //record index of catalog entry for detected source
      detection_index[detections] = n;

      //increment the number of detections
      detections++;
    }
  }
  fprintf(stdout,"\nNumber of detections is %d.\n",detections);
//
//
//  /******************************************************************/
//  /*           Format selected entries as L4 data products          */
//  /******************************************************************/
//  for(int d=0; d<detections; d++)
//  {
//    int n = detection_index[d];
//    entry = catalog->entry[n];
//  }
  
  if(flags->orbit)free_orbit(orbit);
  
  return 0;
}


/* ============================  SUBROUTINES  ============================ */

void alloc_entry(struct Entry *entry, int IMAX)
{
  entry->I = 0;
  entry->source = malloc(IMAX*sizeof(struct Source*));
}

void create_new_source(struct Catalog *catalog, struct Source *sample, int IMAX, int NFFT, int Nchannel, int NP)
{
  int N = catalog->N;
  //allocate memory for new entry in catalog
  struct Entry *entry = NULL;
  entry = malloc(sizeof(struct Entry));
  alloc_entry(entry,IMAX);
  entry->source[entry->I] = malloc(sizeof(struct Source));
  alloc_source(entry->source[entry->I], NFFT, Nchannel, NP);
  //add sample to the catalog as the new entry
  copy_source(sample, entry->source[entry->I]);
  catalog->entry[N] = entry;
  entry->I++; //increment number of samples for entry
  catalog->N++;//increment number of entries for catalog
  fprintf(stdout,"\nNew source created. Total number of sources is now %d\n",catalog->N);
    
}

void append_sample_to_entry(struct Entry *entry, struct Source *sample, int IMAX, int NFFT, int Nchannel, int NP)
{
  //malloc source structure for this sample's entry
  entry->source[entry->I] = malloc(sizeof(struct Source));
  alloc_source(entry->source[entry->I], NFFT, Nchannel, NP);
  
  //copy chain sample into catalog entry
  copy_source(sample, entry->source[entry->I]);
  
  //increment number of stored samples for this entry
  entry->I++;
}
