#ifndef DOPPLER_FFT_H
#define DOPPLER_FFT_H

#include "loader.h"

typedef struct {
    double mti_ms;
    double mtd_ms;
    double timing;
} DopplerFftTiming;

int doppler_fft_processing(const ComplexMatrix *rxsig_pc, int nfft,
                              ComplexMatrix *doppler_map, DopplerFftTiming *timing);
// int doppler_fft_processing(const ComplexMatrix *rxsig_pc, const RadarMeta *meta, int nfft, ComplexMatrix *doppler_map);

#endif