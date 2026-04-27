#ifndef PULSE_H
#define PULSE_H

#include <complex.h>
#include <stddef.h>
#include <fftw3.h>

#include "loader.h"
#include "common.h"

typedef struct {
    double filter_ready_ms;
    double compression_ms;
} PulseTiming;

typedef struct {
    ComplexMatrix h;
    int input_len;
    int filter_len;
    int conv_len;
    int nfft;
    int mf_delay;
    
    double complex *out_buf;
    double complex *H;
    double complex *X;
    double complex *Y;
    fftw_plan forward_plan;
    fftw_plan inverse_plan;
} PulseCompressCtx;


int make_pulse_compression_filter(const RadarMeta *meta, int use_window, ComplexMatrix *h);
int pulse_compress_ctx_init(const RadarMeta *meta, PulseCompressCtx *ctx);
void pulse_compress_ctx_destroy(PulseCompressCtx *ctx);
int pulse_compress_one(PulseCompressCtx *ctx,
                       const double complex *raw_pulse,
                       double complex *out_range_bins);
#endif  