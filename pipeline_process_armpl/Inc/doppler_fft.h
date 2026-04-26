#ifndef __DOPPLER_FFT_H__
#define __DOPPLER_FFT_H__

#include "loader.h"

typedef struct {
    double mti_ms;
    double mtd_ms;
} DopplerFftTiming;

typedef struct {
    int pulses;
    double *hamming_win;
} DopplerWorkspace;

int init_doppler_workspace(DopplerWorkspace *ws, int pulses);
void cleanup_doppler_workspace(DopplerWorkspace *ws);

int doppler_fft_processing(const ComplexMatrix *rd_map,
                           int nfft,
                           ComplexMatrix *doppler_map,
                           DopplerFftTiming *timing,
                           DopplerWorkspace *ws);

#endif