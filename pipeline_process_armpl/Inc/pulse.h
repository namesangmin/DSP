#ifndef PULSE_H
#define PULSE_H

#include <complex.h>
#include <stddef.h>
#include <fftw3.h>
#include "types.h"
#include "common.h"
typedef struct {
    ComplexMatrix h;
    int input_len;
    int filter_len;
    int conv_len;
    int nfft;
    int mf_delay;

    float complex *H;
    float complex *X;
    float complex *Y;
    PipelineTiming *timing;  // cfar_ms, transpose_ms 대신
    fftwf_plan forward_plan;
    fftwf_plan inverse_plan;
} PulseCompressCtx;

int make_pulse_compression_filter(const RadarMeta *meta, int use_window, ComplexMatrix *h);
int pulse_compress_ctx_init(const RadarMeta *meta, PulseCompressCtx *ctx);
void pulse_compress_ctx_destroy(PulseCompressCtx *ctx);
int pulse_compress_one(PulseCompressCtx *ctx, const fftwf_complex *raw_pulse, float complex *out_range_bins);
int transpose_rd_pulse_range_to_doppler_range_pulse(const ComplexMatrix *rd_map, ComplexMatrix *doppler_map, const RadarMeta *meta);
#endif  