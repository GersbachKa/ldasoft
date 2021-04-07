////  noise_mcmc.c//////  Created by Tyson Littenberg on 4/06/21.//
#include <stdio.h>#include <stdlib.h>#include <string.h>#include <math.h>#include <time.h>#include <gsl/gsl_rng.h>#include <gsl/gsl_randist.h>#include <omp.h>#include <LISA.h>#include <GalacticBinary.h>#include <GalacticBinaryIO.h>#include <GalacticBinaryMath.h>#include <GalacticBinaryData.h>#include <GalacticBinaryModel.h>#include "Noise.h"int main(int argc, char *argv[]){    time_t start, stop;    start = time(NULL);    char filename[128];        print_LISA_ASCII_art(stdout);        struct Data *data   = malloc(sizeof(struct Data));    struct Flags *flags = malloc(sizeof(struct Flags));    struct Orbit *orbit = malloc(sizeof(struct Orbit));    struct Chain *chain = malloc(sizeof(struct Chain));    struct SplineModel *model = malloc(sizeof(struct SplineModel));    struct SplineModel *trial = malloc(sizeof(struct SplineModel));        parse(argc,argv,data,orbit,flags,chain,1,1,0);        /*     * Get Data     */        /* Initialize data structures */    alloc_data(data, flags);        /* Initialize LISA orbit model */    initialize_orbit(data, orbit, flags);        /* Initialize chain structure and files */    initialize_chain(chain, flags, &data->cseed, "a");        /* read data */    GalacticBinaryReadData(data,orbit,flags);        /*     * Initialize Spline Model     */    int Nspline = 32+1;    initialize_spline_model(orbit, data, model, Nspline);    initialize_spline_model(orbit, data, trial, Nspline);    sprintf(filename,"%s/data/initial_spline_points.dat",flags->runDir);    print_noise_model(model->spline, filename);        sprintf(filename,"%s/data/interpolated_spline_points.dat",flags->runDir);    print_noise_model(model->psd, filename);            //MCMC    FILE *chainFile = fopen("chains/chain_file.dat","w");    for(int step=0; step<1000000; step++)    {        noise_spline_model_mcmc(orbit, data, model, trial, chain, flags, 0);        if(step%10==0)fprintf(chainFile,"%i %.12g\n",step,model->logL);    }        fclose(chainFile);        sprintf(filename,"%s/data/final_spline_points.dat",flags->runDir);    print_noise_model(model->spline, filename);        sprintf(filename,"%s/data/final_interpolated_spline_points.dat",flags->runDir);    print_noise_model(model->psd, filename);        free_spline_model(model);    free_spline_model(trial);        //print total run time    stop = time(NULL);        printf(" ELAPSED TIME = %g seconds\n",(double)(stop-start));            return 0;}