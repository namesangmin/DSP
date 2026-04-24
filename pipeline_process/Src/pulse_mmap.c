#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>

#include "pulse_compress_thread.h"
#include "pulse.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern int make_pulse_compression_filter(const RadarMeta *meta,
                                         int use_window,
                                         ComplexMatrix *h);

static int next_power_of_two_local(int n)
{
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

static void bit_reverse_permute_local(double complex *x, int n)
{
    int i, j, bit;

    for (i = 1, j = 0; i < n; ++i) {
        bit = n >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;

        if (i < j) {
            double complex tmp = x[i];
            x[i] = x[j];
            x[j] = tmp;
        }
    }
}

static void fft_inplace_local(double complex *x, int n, int inverse)
{
    int len, i, j;

    bit_reverse_permute_local(x, n);

    for (len = 2; len <= n; len <<= 1) {
        double angle = (inverse ? 2.0 : -2.0) * M_PI / (double)len;
        double complex wlen = cos(angle) + I * sin(angle);

        for (i = 0; i < n; i += len) {
            double complex w = 1.0 + I * 0.0;

            for (j = 0; j < len / 2; ++j) {
                double complex u = x[i + j];
                double complex v = x[i + j + len / 2] * w;

                x[i + j] = u + v;
                x[i + j + len / 2] = u - v;

                w *= wlen;
            }
        }
    }

    if (inverse) {
        for (i = 0; i < n; ++i) {
            x[i] /= (double)n;
        }
    }
}

int pulse_compress_ctx_init(const RadarMeta *meta, PulseCompressCtx *ctx)
{
    if (!meta || !ctx) {
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->input_len = meta->num_fast_time_samples;

    if (make_pulse_compression_filter(meta, 1, &ctx->h) != 0) {
        return -1;
    }

    ctx->filter_len = ctx->h.rows;
    ctx->conv_len   = ctx->input_len + ctx->filter_len - 1;
    ctx->nfft       = next_power_of_two_local(ctx->conv_len);
    ctx->mf_delay   = ctx->filter_len - 1;

    ctx->H = (double complex *)calloc((size_t)ctx->nfft, sizeof(double complex));
    ctx->X = (double complex *)calloc((size_t)ctx->nfft, sizeof(double complex));
    ctx->Y = (double complex *)calloc((size_t)ctx->nfft, sizeof(double complex));
    
    // [추가된 부분] 임시 출력 버퍼 할당
    ctx->out_buf = (double complex *)calloc((size_t)ctx->input_len, sizeof(double complex));

    // 할당 실패 검증 로직에도 out_buf 추가
    if (!ctx->H || !ctx->X || !ctx->Y || !ctx->out_buf) {
        pulse_compress_ctx_destroy(ctx);
        return -1;
    }    
    if (!ctx->H || !ctx->X || !ctx->Y) {
        pulse_compress_ctx_destroy(ctx);
        return -1;
    }

    for (int i = 0; i < ctx->filter_len; ++i) {
        ctx->H[i] = CMAT_AT(&ctx->h, i, 0);
    }

    fft_inplace_local(ctx->H, ctx->nfft, 0);

    return 0;
}

void pulse_compress_ctx_destroy(PulseCompressCtx *ctx)
{
    if (!ctx) {
        return;
    }

    free(ctx->H);
    free(ctx->X);
    free(ctx->Y);
    free_complex_matrix(&ctx->h);

    memset(ctx, 0, sizeof(*ctx));
}

int pulse_compress_one(PulseCompressCtx *ctx,
                       const RawIQSample *raw_pulse,
                       double complex *out_range_bins)
{
    if (!ctx || !raw_pulse || !out_range_bins) {
        return -1;
    }

    if (ctx->input_len <= 0 || ctx->nfft <= 0 ||
        !ctx->H || !ctx->X || !ctx->Y) {
        fprintf(stderr, "pulse_compress_one: ctx not initialized\n");
        return -1;
    }

    for (int i = 0; i < ctx->nfft; ++i) {
        ctx->X[i] = 0.0 + I * 0.0;
    }

    for (int i = 0; i < ctx->input_len; ++i) {
        ctx->X[i] = raw_pulse[i].i + raw_pulse[i].q * I;
    }

    fft_inplace_local(ctx->X, ctx->nfft, 0);

    for (int i = 0; i < ctx->nfft; ++i) {
        ctx->Y[i] = ctx->X[i] * ctx->H[i];
    }

    fft_inplace_local(ctx->Y, ctx->nfft, 1);

    for (int r = 0; r < ctx->input_len; ++r) {
        int src = r + ctx->mf_delay;
        out_range_bins[r] = (src < ctx->conv_len) ? ctx->Y[src] : (0.0 + I * 0.0);
    }

    return 0;
}