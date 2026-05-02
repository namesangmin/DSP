<<<<<<< Updated upstream
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
=======
>>>>>>> Stashed changes
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include "doppler_fft.h"
#include "timer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int is_power_of_two(int n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

<<<<<<< Updated upstream
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
=======
int init_doppler_workspace(DopplerWorkspace *ws, int pulses, int nfft)
{
    if (!ws || pulses <= 0 || nfft < pulses) return -1;

    memset(ws, 0, sizeof(*ws));
    ws->pulses = pulses;
    ws->nfft   = nfft;

    ws->hamming_win = (float *)malloc((size_t)pulses * sizeof(float));
    if (!ws->hamming_win) { cleanup_doppler_workspace(ws); return -1; }
    
    make_hamming_window(pulses, ws->hamming_win);

    ws->plan_buf = (fftwf_complex *)fftwf_malloc((size_t)nfft * sizeof(fftwf_complex));
    if (!ws->plan_buf) { cleanup_doppler_workspace(ws); return -1; }

    ws->mtd_plan = fftwf_plan_dft_1d(nfft, ws->plan_buf, ws->plan_buf,
                                     FFTW_FORWARD, FFTW_ESTIMATE);
    if (!ws->mtd_plan) { cleanup_doppler_workspace(ws); return -1; }

    return 0;
>>>>>>> Stashed changes
}

static void fft_inplace(double complex *x, int n) {
    int len, i, j;

    bit_reverse_permute(x, n);

    for (len = 2; len <= n; len <<= 1) {
        double angle = -2.0 * M_PI / (double)len;
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
}

static void fftshift_1d(double complex *x, int n) {
    int half = n / 2;
    for (int i = 0; i < half; ++i) {
        double complex tmp = x[i];
        x[i] = x[i + half];
        x[i + half] = tmp;
    }
}

static int apply_mti(const ComplexMatrix *x, int order, ComplexMatrix *y) {
    int rows = x->rows;
    int cols = x->cols;

    if (alloc_complex_matrix(rows, cols, y) != 0) return -1;

    if (order == 1) {
        for (int r = 0; r < rows; ++r) {
            CMAT_AT(y, r, 0) = 0.0 + I * 0.0;
            for (int c = 1; c < cols; ++c) {
                CMAT_AT(y, r, c) = CMAT_AT(x, r, c) - CMAT_AT(x, r, c - 1);
            }
        }
    } else if (order == 2) {
        for (int r = 0; r < rows; ++r) {
            CMAT_AT(y, r, 0) = 0.0 + I * 0.0;
            if (cols > 1) CMAT_AT(y, r, 1) = 0.0 + I * 0.0;
            for (int c = 2; c < cols; ++c) {
                CMAT_AT(y, r, c) =
                    CMAT_AT(x, r, c)
                    - 2.0 * CMAT_AT(x, r, c - 1)
                    + CMAT_AT(x, r, c - 2);
            }
        }
    } else {
        free_complex_matrix(y);
        return -1;
    }

    return 0;
}

<<<<<<< Updated upstream
static void make_hamming_window(int N, double *w) {
    if (N <= 1) {
        if (N == 1) w[0] = 1.0;
        return;
    }

    for (int n = 0; n < N; ++n) {
        w[n] = 0.54 - 0.46 * cos(2.0 * M_PI * n / (double)(N - 1));
    }
}

double get_velocity_from_bin(int doppler_bin, int nfft, double prf_hz, double fc_hz) {
    const double c = 299792458.0;
    double lambda = c / fc_hz;
    double fd = ((double)doppler_bin - (double)(nfft / 2)) * (prf_hz / (double)nfft);
    return fd * lambda / 2.0;
}

double get_range_from_bin(int range_bin, double fs_hz) {
    const double c = 299792458.0;
    return ((double)range_bin) * c / (2.0 * fs_hz);
}

int doppler_fft_processing_ex(const ComplexMatrix *rxsig_pc, const RadarMeta *meta, int nfft,
                              ComplexMatrix *doppler_map, DopplerFftTiming *timing) {
    ComplexMatrix mti = {0};
    double *win = NULL;
    double complex *buf = NULL;
    int rows = rxsig_pc->rows;
    int pulses = rxsig_pc->cols;
    double t0, t1;

    if (timing) {
        timing->mti_ms = 0.0;
        timing->mtd_ms = 0.0;
    }

    if (nfft <= 0) nfft = pulses;
    if (!is_power_of_two(nfft)) return -1;
=======
int doppler_fft_processing(ComplexMatrix *doppler_map,
                           int nfft,
                           PipelineTiming *timing,
                           DopplerWorkspace *ws)
{
    if (!doppler_map || !doppler_map->data || !timing || !ws)
        return -1;

    int pulses = doppler_map->cols;

    if (nfft < pulses) return -1;

    if (doppler_map->cols < nfft) {
        fprintf(stderr,
                "doppler_fft_processing: shape mismatch dop=%dx%d nfft=%d\n",
                doppler_map->rows, doppler_map->cols, nfft);
        return -1;
    }
    
    double t0 = now_ms();
    if (apply_mti(doppler_map, 1) != 0) return -1;
    timing->mti_ms = now_ms() - t0;
>>>>>>> Stashed changes

    t0 = now_ms();
    if (apply_mti(rxsig_pc, 1, &mti) != 0) return -1;
    t1 = now_ms();
    if (timing) timing->mti_ms = t1 - t0;

    if (alloc_complex_matrix(rows, nfft, doppler_map) != 0) {
        free_complex_matrix(&mti);
        return -1;
    }

    win = (double *)calloc((size_t)pulses, sizeof(double));
    buf = (double complex *)calloc((size_t)nfft, sizeof(double complex));
    if (!win || !buf) {
        free(win);
        free(buf);
        free_complex_matrix(&mti);
        free_complex_matrix(doppler_map);
        return -1;
    }

    t0 = now_ms();

    make_hamming_window(pulses, win);

    for (int r = 0; r < rows; ++r) {
        for (int i = 0; i < nfft; ++i) {
            buf[i] = 0.0 + I * 0.0;
        }

        for (int p = 0; p < pulses && p < nfft; ++p) {
            buf[p] = CMAT_AT(&mti, r, p) * win[p];
        }

        fft_inplace(buf, nfft);
        fftshift_1d(buf, nfft);

        for (int k = 0; k < nfft; ++k) {
            CMAT_AT(doppler_map, r, k) = buf[k];
        }
    }

    t1 = now_ms();
    if (timing) timing->mtd_ms = t1 - t0;

    free(win);
    free(buf);
    free_complex_matrix(&mti);
    (void)meta;
    return 0;
}

int doppler_fft_processing(const ComplexMatrix *rxsig_pc, const RadarMeta *meta, int nfft, ComplexMatrix *doppler_map) {
    return doppler_fft_processing_ex(rxsig_pc, meta, nfft, doppler_map, NULL);
}