#ifndef __DOPPLER_FFT_H__
#define __DOPPLER_FFT_H__

#include <fftw3.h>
#include "types.h"
#include "common.h"

typedef struct {
    int pulses;             // 512
    int nfft;               // 512
    float *hamming_win;     // length = pulses

    fftwf_complex *plan_buf;
    fftwf_plan mtd_plan;
} DopplerWorkspace;

int init_doppler_workspace(DopplerWorkspace *ws, int pulses, int nfft);

void cleanup_doppler_workspace(DopplerWorkspace *ws);

int doppler_fft_processing(ComplexMatrix *doppler_map,
                           int nfft,
                           PipelineTiming *timing,
                           DopplerWorkspace *ws);

#endif