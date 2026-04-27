#ifndef __DOPPLER_FFT_H__
#define __DOPPLER_FFT_H__

#include <fftw3.h>
#include "common.h"
#include "loader.h"

typedef struct {
    double mti_ms;
    double mtd_ms;
} DopplerFftTiming;

typedef struct {
    int pulses;             // 512
    int nfft;               // 512

    float *hamming_win;     // length = pulses

    fftwf_complex *plan_buf;
    fftwf_plan mtd_plan;
} DopplerWorkspace;

// int init_doppler_workspace(DopplerWorkspace *ws, int pulses);
int init_doppler_workspace(DopplerWorkspace *ws, int pulses, int nfft);

void cleanup_doppler_workspace(DopplerWorkspace *ws);

int doppler_fft_processing(const ComplexMatrix *rd_map,
                           int nfft,
                           ComplexMatrix *doppler_map,
                           DopplerFftTiming *timing,
                           DopplerWorkspace *ws);

#endif