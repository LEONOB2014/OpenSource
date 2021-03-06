#include "par.h"
#include "segy.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <genfft.h>

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif
#define NINT(x) ((int)((x)>0.0?(x)+0.5:(x)-0.5))

#ifndef COMPLEX
typedef struct _complexStruct { /* complex number */
    float r,i;
} complex;
#endif/* complex */

int getFileInfo(char *filename, int *n1, int *n2, int *ngath, float *d1, float *d2, float *f1, float *f2, float *xmin, float *xmax, float *sclsxgx, int *nxm);
int readData(FILE *fp, float *data, segy *hdrs, int n1);
int writeData(FILE *fp, float *data, segy *hdrs, int n1, int n2);
void complex_sqrt(complex *z);
void scl_data(float *data, int nsam, int nrec, float scl, float *datout, int nsamout);
void pad2d_data(float *data, int nsam, int nrec, int nsamout, int nrecout, float *datout);
void pad_data(float *data, int nsam, int nrec, int nsamout, float *datout);

/*********************** self documentation **********************/
char *sdoc[] = {
" ",
" ampdet - Determine amplitude",
" ",
" author  : Jan Thorbecke : 19-04-1995 (janth@xs4all.nl)",
" product : Originates from DELPHI software",
"                         : revision 2010",
" ",
NULL};
/**************** end self doc ***********************************/

int main (int argc, char **argv)
{
    FILE    *fp;
	char    *file_gp, *file_fp, *file_wav;
    int     nx, nt, ngath, ntraces, ret, size, nxwav;
    int     ntfft, nfreq, nxfft, nkx, i, j, n;
    float   dx, dt, fx, ft, xmin, xmax, scl, *den, dentmp;
    float   df, dw, dkx, eps, reps, leps, sclfk;
    float   *Gpd, *f1pd, *G_pad, *f_pad, *wav, *wav_pad, *outdata;
    complex *G_w, *f_w, *Gf, *amp, *wav_w, *S, *ZS, *SS;
    segy    *hdr_gp, *hdr_fp, *hdr_wav, *hdr_out;

	initargs(argc, argv);
	requestdoc(1);

	if(!getparstring("file_gp", &file_gp)) file_gp=NULL;
    if (file_gp==NULL) verr("file %s does not exist",file_gp);
    if(!getparstring("file_fp", &file_fp)) file_fp=NULL;
    if (file_fp==NULL) verr("file %s does not exist",file_fp);
    if(!getparstring("file_wav", &file_wav)) file_wav=NULL;
    if (file_wav==NULL) verr("file %s does not exist",file_wav);
	if(!getparfloat("eps", &eps)) eps=0.00;
	if(!getparfloat("reps", &reps)) reps=0.01;

    ngath = 1;
    ret = getFileInfo(file_gp, &nt, &nx, &ngath, &dt, &dx, &ft, &fx, &xmin, &xmax, &scl, &ntraces);

    size    = nt*nx;

	Gpd     = (float *)malloc(size*sizeof(float));
	hdr_gp  = (segy *) calloc(nx,sizeof(segy));
    fp      = fopen(file_gp, "r");
	if (fp == NULL) verr("error on opening input file_in1=%s", file_gp);
    nx      = readData(fp, Gpd, hdr_gp, nt);
    fclose(fp);

	f1pd    = (float *)malloc(size*sizeof(float));
	hdr_fp  = (segy *) calloc(nx,sizeof(segy));
    fp      = fopen(file_fp, "r");
	if (fp == NULL) verr("error on opening input file_in1=%s", file_fp);
    nx      = readData(fp, f1pd, hdr_fp, nt);
    fclose(fp);

    wav     = (float *)malloc(nt*sizeof(float));
	hdr_wav = (segy *) calloc(1,sizeof(segy));
    fp      = fopen(file_wav, "r");
	if (fp == NULL) verr("error on opening input file_in1=%s", file_fp);
    nxwav   = readData(fp, wav, hdr_wav, nt);
    fclose(fp);

    /* Start the scaling */
    ntfft   = optncr(nt);
	nfreq   = ntfft/2+1;
	df      = 1.0/(ntfft*dt);
    dw      = 2.0*PI*df;
	nkx     = optncc(nx);
	dkx     = 2.0*PI/(nkx*dx);
	sclfk   = dt*(dt*dx)*(dt*dx);

    vmess("ntfft:%d, nfreq:%d, nkx:%d dx:%.3f dt:%.3f",ntfft,nfreq,nkx,dx,dt);

    /* Allocate the arrays */
    G_pad = (float *)calloc(ntfft*nkx,sizeof(float));
	if (G_pad == NULL) verr("memory allocation error for G_pad");
    f_pad = (float *)calloc(ntfft*nkx,sizeof(float));
	if (f_pad == NULL) verr("memory allocation error for f_pad");
    wav_pad = (float *)calloc(ntfft,sizeof(float));
	if (wav_pad == NULL) verr("memory allocation error for wav_pad");
    G_w   = (complex *)calloc(nfreq*nkx,sizeof(complex));
	if (G_w == NULL) verr("memory allocation error for G_w");
    f_w   = (complex *)calloc(nfreq*nkx,sizeof(complex));
	if (f_w == NULL) verr("memory allocation error for f_w");
    Gf    = (complex *)calloc(nfreq*nkx,sizeof(complex));
	if (Gf == NULL) verr("memory allocation error for Gf");
    wav_w = (complex *)calloc(nfreq*nkx,sizeof(complex));
	if (wav_w == NULL) verr("memory allocation error for wav_w");
    amp   = (complex *)calloc(nfreq*nkx,sizeof(complex));
	if (amp == NULL) verr("memory allocation error for amp");
    S   = (complex *)calloc(nfreq*nkx,sizeof(complex));
	if (S == NULL) verr("memory allocation error for S");
    ZS   = (complex *)calloc(nfreq*nkx,sizeof(complex));
	if (ZS == NULL) verr("memory allocation error for ZS");
    SS   = (complex *)calloc(nfreq*nkx,sizeof(complex));
	if (SS == NULL) verr("memory allocation error for SS");
    den   = (float *)calloc(nfreq*nkx,sizeof(float));
	if (den == NULL) verr("memory allocation error for den");

    /* pad zeroes in 2 directions to reach FFT lengths */
	pad2d_data(Gpd, nt,nx,ntfft,nkx,G_pad);
	pad2d_data(f1pd,nt,nx,ntfft,nkx,f_pad);
    pad_data(  wav, nt, 1,ntfft,  wav_pad);

    /* double forward FFT */
	xt2wkx(&G_pad[0], &G_w[0], ntfft, nkx, ntfft, nkx, 0);
	xt2wkx(&f_pad[0], &f_w[0], ntfft, nkx, ntfft, nkx, 0);
    rcmfft(&wav_pad[0], &Gf[0], ntfft, 1, ntfft, nfreq, -1);

    for (i=0; i<nkx; i++) {
        for (j=0; j<nfreq; j++) {
            wav_w[j*nkx+i].r = Gf[j].r;
            wav_w[j*nkx+i].i = Gf[j].i;	
		}
    }

	for (i = 0; i < nkx*nfreq; i++) {
		Gf[i].r = (G_w[i].r*f_w[i].r - G_w[i].i*f_w[i].i);
		Gf[i].i = (G_w[i].r*f_w[i].i + G_w[i].i*f_w[i].r);

		S[i].r = (wav_w[i].r*wav_w[i].r + wav_w[i].i*wav_w[i].i);
		S[i].i = (wav_w[i].r*wav_w[i].i - wav_w[i].i*wav_w[i].r);

		ZS[i].r = (Gf[i].r*S[i].r + Gf[i].i*S[i].i);
		ZS[i].i = (Gf[i].r*S[i].i - Gf[i].i*S[i].r);

		SS[i].r = (S[i].r*S[i].r + S[i].i*S[i].i);
		SS[i].i = (S[i].r*S[i].i - S[i].i*S[i].r);

		if (i==0) dentmp=SS[i].r;
		else dentmp=MAX(dentmp,SS[i].r);
	}

	leps = reps*dentmp+eps;
	vmess("dentmp:%.4e leps:%.4e",dentmp,leps);

	for (i = 0; i < nkx*nfreq; i++) {
		S[i].r = (ZS[i].r*SS[i].r+ZS[i].i*SS[i].i)/(SS[i].r*SS[i].r+SS[i].i*SS[i].i+leps);
		S[i].i = (ZS[i].i*SS[i].r-ZS[i].r*SS[i].i)/(SS[i].r*SS[i].r+SS[i].i*SS[i].i+leps);

		amp[i].r = sqrtf(S[i].r*S[i].r+S[i].i*S[i].i);
		amp[i].i = 0.0;
		
		// complex_sqrt(&amp[i]);
		if (isnan(amp[i].r)) amp[i].r = 0;
		if (isnan(amp[i].i)) amp[i].i = 0;
		if (isinf(amp[i].r)) amp[i].r = 0;
		if (isinf(amp[i].i)) amp[i].i = 0;

		Gf[i].r = (G_w[i].r*amp[i].r - G_w[i].i*amp[i].i);
		Gf[i].i = (G_w[i].r*amp[i].i + G_w[i].i*amp[i].r);
	}

	// for (i=0; i<nfreq; i++) {
	// 	for (j=0; j<nkx; j++) {
	// 		Gpd[j*nfreq+i] = sqrtf(amp[i*nkx+j].r*amp[i*nkx+j].r+amp[i*nkx+j].i*amp[i*nkx+j].i);
	// 	}
	// }
    
    // conv_small(G_w, amp, Gf, nkx, nfreq); // Scaled data

    /* inverse double FFT */
	wkx2xt(&Gf[0], &G_pad[0], ntfft, nkx, nkx, ntfft, 0);
	/* select original samples and traces */
	scl = (1.0)/(nkx*ntfft);
	scl_data(G_pad,ntfft,nx,scl,Gpd ,nt);

    fp      = fopen("out.su", "w+");
    ret = writeData(fp, Gpd, hdr_gp, nt, nx);
	if (ret < 0 ) verr("error on writing output file.");
    fclose(fp);

	// fp      = fopen("wav.su", "w+");
	// for (j=0; j<nkx; j++) {
	// 	hdr_gp[j].ns = nfreq;
	// }
    // ret = writeData(fp, Gpd, hdr_gp, nfreq, nkx);
	// if (ret < 0 ) verr("error on writing output file.");
    // fclose(fp);

    free(f1pd);free(Gpd);free(hdr_gp);free(hdr_fp);

	return 0;
}

void complex_sqrt(complex *z)
{
    float zmod, zmodr, zzmr, zzmi, zzm;

    zmod  = sqrtf(z[0].r*z[0].r+z[0].i*z[0].i);
    zmodr = sqrtf(zmod);
    zzmr  = z[0].r + zmod;
    zzmi  = z[0].i;
    zzm   = sqrtf(zzmr*zzmr+zzmi*zzmi);

    z[0].r = (zmodr*zzmr)/zzm;
    z[0].i = (zmodr*zzmi)/zzm;
}

void pad_data(float *data, int nsam, int nrec, int nsamout, float *datout)
{
	int it,ix;
	for (ix=0;ix<nrec;ix++) {
		for (it=0;it<nsam;it++)
			datout[ix*nsamout+it]=data[ix*nsam+it];
		for (it=nsam;it<nsamout;it++)
			datout[ix*nsamout+it]=0.0;
	}
}

void pad2d_data(float *data, int nsam, int nrec, int nsamout, int nrecout, float *datout)
{
	int it,ix;
	for (ix=0;ix<nrec;ix++) {
		for (it=0;it<nsam;it++)
			datout[ix*nsam+it]=data[ix*nsam+it];
		for (it=nsam;it<nsamout;it++)
			datout[ix*nsam+it]=0.0;
	}
	for (ix=nrec;ix<nrecout;ix++) {
		for (it=0;it<nsamout;it++)
			datout[ix*nsam+it]=0.0;
	}
}

void scl_data(float *data, int nsam, int nrec, float scl, float *datout, int nsamout)
{
	int it,ix;
	for (ix = 0; ix < nrec; ix++) {
		for (it = 0 ; it < nsamout ; it++)
			datout[ix*nsamout+it] = scl*data[ix*nsam+it];
	}
}