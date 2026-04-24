#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include "pulse.h"
#include "timer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int next_power_of_two(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

static void bit_reverse_permute(double complex *x, int n) {
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

static void fft_inplace(double complex *x, int n, int inverse) {
    int len, i, j;
    bit_reverse_permute(x, n);

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

int apply_pulse_compression_fft(const ComplexMatrix *x, const ComplexMatrix *h, ComplexMatrix *y, int *mf_delay) {
    int rows = x->rows;
    int cols = x->cols;
    int L = h->rows;
    int conv_len = rows + L - 1;
    int nfft = next_power_of_two(conv_len);

    double complex *H = NULL;
    double complex *X = NULL;
    double complex *Y = NULL;

    if (alloc_complex_matrix(rows, cols, y) != 0) return -1;

    H = (double complex *)calloc((size_t)nfft, sizeof(double complex));
    X = (double complex *)calloc((size_t)nfft, sizeof(double complex));
    Y = (double complex *)calloc((size_t)nfft, sizeof(double complex));
    if (!H || !X || !Y) {
        free(H);
        free(X);
        free(Y);
        free_complex_matrix(y);
        return -1;
    }

    for (int i = 0; i < L; ++i) H[i] = CMAT_AT(h, i, 0);
    fft_inplace(H, nfft, 0);

    if (mf_delay) *mf_delay = L - 1;

    for (int c = 0; c < cols; ++c) {
        for (int i = 0; i < nfft; ++i) X[i] = 0.0 + I * 0.0;
        for (int r = 0; r < rows; ++r) X[r] = CMAT_AT(x, r, c);

        fft_inplace(X, nfft, 0);

        for (int i = 0; i < nfft; ++i) Y[i] = X[i] * H[i];

        fft_inplace(Y, nfft, 1);

        for (int r = 0; r < rows; ++r) {
            int src = r + (L - 1);
            if (src < conv_len) {
                CMAT_AT(y, r, c) = Y[src];
            } else {
                CMAT_AT(y, r, c) = 0.0 + I * 0.0;
            }
        }
    }

    free(H);
    free(X);
    free(Y);
    return 0;
}

int pulse_compression_ex(const ComplexMatrix *x, const RadarMeta *meta, ComplexMatrix *y, PulseTiming *timing) {
    ComplexMatrix h = {0};
    int mf_delay = 0;
    int ret;
    double t0, t1;

    if (timing) {
        timing->filter_ready_ms = 0.0;
        timing->compression_ms = 0.0;
    }

    t0 = now_ms();
    ret = make_pulse_compression_filter(meta, 1, &h);
    t1 = now_ms();
    if (timing) timing->filter_ready_ms = t1 - t0;
    if (ret != 0) return ret;

    t0 = now_ms();
    ret = apply_pulse_compression_fft(x, &h, y, &mf_delay);
    t1 = now_ms();
    if (timing) timing->compression_ms = t1 - t0;

    free_complex_matrix(&h);
    return ret;
}

int pulse_compression(const ComplexMatrix *x, const RadarMeta *meta, ComplexMatrix *y) {
    return pulse_compression_ex(x, meta, y, NULL);
}