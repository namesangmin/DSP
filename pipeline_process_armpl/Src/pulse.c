//#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
 #include <blas.h>
#include "pulse_compress_thread.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
int transpose_rd_pulse_range_to_doppler_range_pulse(
    const ComplexMatrix *rd_map,
    ComplexMatrix *doppler_map,
    const RadarMeta *meta)
{
    if (!rd_map || !rd_map->data || !doppler_map || !doppler_map->data || !meta) {
        return -1;
    }

    int pulses = meta->num_pulses;                 // 512
    int ranges = meta->num_fast_time_samples;      // 1001

    if (rd_map->rows != pulses || rd_map->cols != ranges) {
        fprintf(stderr,
                "transpose: rd_map shape mismatch rows=%d cols=%d expected=%dx%d\n",
                rd_map->rows, rd_map->cols, pulses, ranges);
        return -1;
    }

    if (doppler_map->rows != ranges || doppler_map->cols != pulses) {
        fprintf(stderr,
                "transpose: doppler_map shape mismatch rows=%d cols=%d expected=%dx%d\n",
                doppler_map->rows, doppler_map->cols, ranges, pulses);
        return -1;
    }

    armpl_singlecomplex_t alpha = 1.0f + 0.0f * I;

    comatcopy('R',                 // row-major
              'T',                 // transpose
              (armpl_int_t)pulses, // source rows = 512
              (armpl_int_t)ranges, // source cols = 1001
              alpha,
              (const armpl_singlecomplex_t *)rd_map->data,
              (armpl_int_t)ranges, // lda = source row length = 1001
              (armpl_singlecomplex_t *)doppler_map->data,
              (armpl_int_t)pulses);// ldb = dest row length = 512

    return 0;
}
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
        float t = (float)n / fs;
        float phase = M_PI * k * t * t;
        CMAT_AT(pls, n, 0) = cos(phase) + I * sin(phase);
    }
    return 0;
}
static float safe_acoshf(float x)
{
    if (x < 1.0f) {
        x = 1.0f;
    }

    return logf(x + sqrtf(x * x - 1.0f));
}
static int taylor_window(int N, int nbar, float sll_db, float *w) {
    float A, sp2;
    float *Fm = NULL;
    float maxv = 0.0;

    if (N <= 0 || !w) return -1;
    if (nbar < 2) {
        for (int i = 0; i < N; ++i) w[i] = 1.0;
        return 0;
    }

    Fm = (float *)calloc((size_t)nbar, sizeof(float));
    if (!Fm) return -1;

    A = safe_acoshf(pow(10.0, sll_db / 20.0)) / M_PI;
    sp2 = ((float)nbar * (float)nbar) /
          (A * A + ((float)nbar - 0.5) * ((float)nbar - 0.5));

    for (int m = 1; m <= nbar - 1; ++m) {
        float numer = 1.0;
        float denom = 1.0;
        for (int n = 1; n <= nbar - 1; ++n) {
            if (n == m) continue;
            numer *= 1.0 - ((float)(m * m)) /
                     (sp2 * (A * A + ((float)n - 0.5) * ((float)n - 0.5)));
            denom *= 1.0 - ((float)(m * m)) / ((float)(n * n));
        }
        Fm[m] = (((m + 1) % 2) ? -1.0 : 1.0);
        Fm[m] *= numer / (2.0 * denom);
    }

    for (int n = 0; n < N; ++n) {
        float xi = ((float)n - ((float)N - 1.0) / 2.0) / (float)N;
        float sum = 1.0;
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
    float *win = NULL;
    int N;

    if (make_lfm_pulse(meta, &pls) != 0) return -1;

    N = pls.rows;
    if (alloc_complex_matrix(N, 1, h) != 0) {
        free_complex_matrix(&pls);
        return -1;
    }

    if (use_window) {
        win = (float *)calloc((size_t)N, sizeof(float));
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
        float complex v = CMAT_AT(&pls, src, 0);
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

    ctx->H = (float complex *)fftwf_malloc((size_t)ctx->nfft * sizeof(float complex));
    ctx->X = (float complex *)fftwf_malloc((size_t)ctx->nfft * sizeof(float complex));
    ctx->Y = (float complex *)fftwf_malloc((size_t)ctx->nfft * sizeof(float complex));
    ctx->out_buf = (float complex *)fftwf_malloc((size_t)ctx->input_len * sizeof(float complex));
    if (!ctx->H || !ctx->X || !ctx->Y || !ctx->out_buf) {
        pulse_compress_ctx_destroy(ctx);
        return -1;
    }

    memset(ctx->H, 0, (size_t)ctx->nfft * sizeof(float complex));
    memset(ctx->X, 0, (size_t)ctx->nfft * sizeof(float complex));
    memset(ctx->Y, 0, (size_t)ctx->nfft * sizeof(float complex));
    memset(ctx->out_buf, 0, (size_t)ctx->input_len * sizeof(float complex));    

    ctx->forward_plan = fftwf_plan_dft_1d(ctx->nfft,
                                        (fftwf_complex *)ctx->X,
                                        (fftwf_complex *)ctx->X,
                                        FFTW_FORWARD,
                                        FFTW_ESTIMATE);

    ctx->inverse_plan = fftwf_plan_dft_1d(ctx->nfft,
                                        (fftwf_complex *)ctx->Y,
                                        (fftwf_complex *)ctx->Y,
                                        FFTW_BACKWARD,
                                        FFTW_ESTIMATE);

    if (!ctx->forward_plan || !ctx->inverse_plan) {
        pulse_compress_ctx_destroy(ctx);
        return -1;
    }

    for (int i = 0; i < ctx->filter_len; ++i) {
        ctx->H[i] = CMAT_AT(&ctx->h, i, 0);
    }

    fftwf_plan h_plan = fftwf_plan_dft_1d(ctx->nfft,
                                        (fftwf_complex *)ctx->H,
                                        (fftwf_complex *)ctx->H,
                                        FFTW_FORWARD,
                                        FFTW_ESTIMATE);

    if (!h_plan) {
        pulse_compress_ctx_destroy(ctx);
        return -1;
    }

    fftwf_execute(h_plan);
    fftwf_destroy_plan(h_plan);

    // [최적화 1] H에 1/NFFT 미리 곱해두기 (실시간 곱셈 51만번 제거)
    float inv_n = 1.0f / (float)ctx->nfft;
    for (int i = 0; i < ctx->nfft; ++i) {
        ctx->H[i] *= inv_n;
    }
    return 0;
}

void pulse_compress_ctx_destroy(PulseCompressCtx *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->forward_plan) fftwf_destroy_plan(ctx->forward_plan);
    if (ctx->inverse_plan) fftwf_destroy_plan(ctx->inverse_plan);

    if (ctx->H) fftwf_free(ctx->H);
    if (ctx->X) fftwf_free(ctx->X);
    if (ctx->Y) fftwf_free(ctx->Y);
    if (ctx->out_buf) fftwf_free(ctx->out_buf);

    free_complex_matrix(&ctx->h);
    memset(ctx, 0, sizeof(*ctx));
}

int pulse_compress_one(PulseCompressCtx *ctx,
                       const float complex *raw_pulse,
                       float complex *out_range_bins)
{
    if (!ctx) {
        fprintf(stderr, "pulse_compress_one: ctx is NULL\n");
        return -1;
    }

    if (!raw_pulse) {
        fprintf(stderr, "pulse_compress_one: raw_pulse is NULL\n");
        return -1;
    }

    if (!out_range_bins) {
        fprintf(stderr, "pulse_compress_one: out_range_bins is NULL\n");
        return -1;
    }

    if (ctx->input_len <= 0 || ctx->nfft <= 0 ||
        !ctx->H || !ctx->X || !ctx->Y ||
        !ctx->forward_plan || !ctx->inverse_plan) {
        fprintf(stderr, "pulse_compress_one: ctx not initialized\n");
        return -1;
    }

    armpl_int_t n = (armpl_int_t)ctx->input_len;
    armpl_int_t inc = 1;

    memcpy(ctx->X, raw_pulse, (size_t)ctx->input_len * sizeof(float complex));

    memset(ctx->X + ctx->input_len, 0,
        (size_t)(ctx->nfft - ctx->input_len) * sizeof(float complex));

    fftwf_execute(ctx->forward_plan);

    #pragma GCC ivdep
    for (int i = 0; i < ctx->nfft; ++i) {
        ctx->Y[i] = ctx->X[i] * ctx->H[i];
    }

    fftwf_execute(ctx->inverse_plan);
memcpy(out_range_bins, &ctx->Y[ctx->mf_delay], (size_t)ctx->input_len * sizeof(float complex));
    return 0;
}