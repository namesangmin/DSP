#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>

#include "pulse_compress_thread.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int make_lfm_pulse(const RadarMeta *meta, ComplexMatrix *pls) {
    int n;
    double fs = meta->fs_hz;
    double pw = meta->pulse_width_s;
    double bw = meta->sweep_bandwidth_hz;
    double k = bw / pw;
    int N = (int)llround(fs * pw);

    if (N <= 0) return -1;
    if (alloc_complex_matrix(N, 1, pls) != 0) return -1;

    for (n = 0; n < N; ++n) {
        double t = (double)n / fs;
        double phase = M_PI * k * t * t;
        CMAT_AT(pls, n, 0) = cos(phase) + I * sin(phase);
    }
    return 0;
}

static int taylor_window(int N, int nbar, double sll_db, double *w) {
    double A, sp2;
    double *Fm = NULL;
    double maxv = 0.0;

    if (N <= 0 || !w) return -1;
    if (nbar < 2) {
        for (int i = 0; i < N; ++i) w[i] = 1.0;
        return 0;
    }

    Fm = (double *)calloc((size_t)nbar, sizeof(double));
    if (!Fm) return -1;

    A = acosh(pow(10.0, sll_db / 20.0)) / M_PI;
    sp2 = ((double)nbar * (double)nbar) /
          (A * A + ((double)nbar - 0.5) * ((double)nbar - 0.5));

    for (int m = 1; m <= nbar - 1; ++m) {
        double numer = 1.0;
        double denom = 1.0;
        for (int n = 1; n <= nbar - 1; ++n) {
            if (n == m) continue;
            numer *= 1.0 - ((double)(m * m)) /
                     (sp2 * (A * A + ((double)n - 0.5) * ((double)n - 0.5)));
            denom *= 1.0 - ((double)(m * m)) / ((double)(n * n));
        }
        Fm[m] = (((m + 1) % 2) ? -1.0 : 1.0);
        Fm[m] *= numer / (2.0 * denom);
    }

    for (int n = 0; n < N; ++n) {
        double xi = ((double)n - ((double)N - 1.0) / 2.0) / (double)N;
        double sum = 1.0;
        for (int m = 1; m <= nbar - 1; ++m) {
            sum += 2.0 * Fm[m] * cos(2.0 * M_PI * m * xi);
        }
        w[n] = sum;
        if (w[n] > maxv) maxv = w[n];
    }

    if (maxv > 0.0) {
        for (int n = 0; n < N; ++n) w[n] /= maxv;
    }

    free(Fm);
    return 0;
}

int make_pulse_compression_filter(const RadarMeta *meta, int use_window, ComplexMatrix *h) {
    ComplexMatrix pls = {0};
    double *win = NULL;
    int N;

    if (make_lfm_pulse(meta, &pls) != 0) return -1;

    N = pls.rows;
    if (alloc_complex_matrix(N, 1, h) != 0) {
        free_complex_matrix(&pls);
        return -1;
    }

    if (use_window) {
        win = (double *)calloc((size_t)N, sizeof(double));
        if (!win) {
            free_complex_matrix(&pls);
            free_complex_matrix(h);
            return -1;
        }
        if (taylor_window(N, 4, 30.0, win) != 0) {
            free(win);
            free_complex_matrix(&pls);
            free_complex_matrix(h);
            return -1;
        }
    }

    for (int i = 0; i < N; ++i) {
        int src = N - 1 - i;
        double complex v = CMAT_AT(&pls, src, 0);
        if (use_window) v *= win[src];
        CMAT_AT(h, i, 0) = conj(v);
    }

    free(win);
    free_complex_matrix(&pls);
    return 0;
}

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
    free(ctx->out_buf);

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