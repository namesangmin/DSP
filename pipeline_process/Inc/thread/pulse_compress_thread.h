#ifndef __PULSE_COMPRESS_THREAD_H__
#define __PULSE_COMPRESS_THREAD_H__

#include <complex.h>
#include "loader.h"
#include "loader_fread.h"
#include "common.h"
#include "queue.h"

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

typedef struct {
    int pulse_idx;
} PulseJob;

int pulse_compress_ctx_init(const RadarMeta *meta, PulseCompressCtx *ctx);
void pulse_compress_ctx_destroy(PulseCompressCtx *ctx);

int pulse_compress_one(PulseCompressCtx *ctx,
                       const RawIQSample *raw_pulse,
                       double complex *out_range_bins);

void *worker_thread_main(void *arg);

#endif