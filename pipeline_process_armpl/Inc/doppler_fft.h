#ifndef DOPPLER_FFT_H
#define DOPPLER_FFT_H

<<<<<<< Updated upstream
#include "loader.h"

typedef struct {
    double mti_ms;
    double mtd_ms;
} DopplerFftTiming;

int doppler_fft_processing_ex(const ComplexMatrix *rxsig_pc, const RadarMeta *meta, int nfft,
                              ComplexMatrix *doppler_map, DopplerFftTiming *timing);
int doppler_fft_processing(const ComplexMatrix *rxsig_pc, const RadarMeta *meta, int nfft, ComplexMatrix *doppler_map);

double get_velocity_from_bin(int doppler_bin, int nfft, double prf_hz, double fc_hz);
double get_range_from_bin(int range_bin, double fs_hz);
=======
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
>>>>>>> Stashed changes

#endif