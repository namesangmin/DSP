#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include "doppler.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
                    CMAT_AT(x, r, c) -
                    2.0 * CMAT_AT(x, r, c - 1) +
                    CMAT_AT(x, r, c - 2);
            }
        }
    } else {
        free_complex_matrix(y);
        return -1;
    }

    return 0;
}

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
    double fd = ((double)doppler_bin - floor((double)nfft / 2.0)) * (prf_hz / (double)nfft);
    return fd * lambda / 2.0;
}

double get_range_from_bin(int range_bin, double fs_hz) {
    const double c = 299792458.0;
    return ((double)range_bin) * c / (2.0 * fs_hz);
}

int doppler_processing(const ComplexMatrix *rxsig_pc, const RadarMeta *meta, int nfft, ComplexMatrix *doppler_map) {
    ComplexMatrix mti = {0};
    double *win = NULL;
    int rows = rxsig_pc->rows;
    int pulses = rxsig_pc->cols;

    if (nfft <= 0) nfft = pulses;
    if (apply_mti(rxsig_pc, 1, &mti) != 0) return -1;
    if (alloc_complex_matrix(rows, nfft, doppler_map) != 0) {
        free_complex_matrix(&mti);
        return -1;
    }

    win = (double *)calloc((size_t)pulses, sizeof(double));
    if (!win) {
        free_complex_matrix(&mti);
        free_complex_matrix(doppler_map);
        return -1;
    }
    make_hamming_window(pulses, win);

    for (int r = 0; r < rows; ++r) {
        for (int k = 0; k < nfft; ++k) {
            int k_shift = k - (nfft / 2);
            double complex acc = 0.0 + I * 0.0;

            for (int n = 0; n < pulses; ++n) {
                double angle = -2.0 * M_PI * (double)k_shift * (double)n / (double)nfft;
                double complex ex = cos(angle) + I * sin(angle);
                acc += (CMAT_AT(&mti, r, n) * win[n]) * ex;
            }

            CMAT_AT(doppler_map, r, k) = acc;
        }
    }

    free(win);
    free_complex_matrix(&mti);
    (void)meta;
    return 0;
}