#ifndef PULSE_MMAP_H
#define PULSE_MMAP_H

#include <complex.h>
#include "loader.h"
#include "loader_mmap.h"

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
} PulseCompressCtx;

int pulse_compress_ctx_init(const RadarMeta *meta, PulseCompressCtx *ctx);
void pulse_compress_ctx_destroy(PulseCompressCtx *ctx);

int pulse_compress_one(PulseCompressCtx *ctx,
                       const RawIQSample *raw_pulse,
                       double complex *out_range_bins);

#endif