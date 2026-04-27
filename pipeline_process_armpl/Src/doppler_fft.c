//#define _GNU_SOURCE
/*
rd_maps[idx].data
    ↓
apply_mti()
    rd_map에서 값 읽음
    MTI 계산
    doppler_maps[idx].data에 저장
    ↓
apply_mtd()
    doppler_maps[idx].data에 window 적용
    doppler_maps[idx].data에서 FFT in-place
    doppler_maps[idx].data에서 fftshift
    ↓
cfar_detect()
    doppler_maps[idx].data 사용
*/

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <string.h>
#include "doppler_fft.h"
#include "timer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void make_hamming_window(int N, float *w)
{
    if (!w || N <= 0) {
        return;
    }

    if (N == 1) {
        w[0] = 1.0;
        return;
    }

    for (int n = 0; n < N; ++n) {
        w[n] = 0.54 - 0.46 * cos((2.0 * M_PI * (float)n) / (float)(N - 1));
    }
}
int init_doppler_workspace(DopplerWorkspace *ws, int pulses, int nfft)
{
    if (!ws || pulses <= 0 || nfft < pulses) {
        return -1;
    }

    memset(ws, 0, sizeof(*ws));

    ws->pulses = pulses;
    ws->nfft = nfft;

    ws->hamming_win = (float *)malloc((size_t)pulses * sizeof(float));
    if (!ws->hamming_win) {
        cleanup_doppler_workspace(ws);
        return -1;
    }

    make_hamming_window(pulses, ws->hamming_win);

    ws->plan_buf = (fftwf_complex *)fftwf_malloc((size_t)nfft * sizeof(fftwf_complex));
    if (!ws->plan_buf) {
        cleanup_doppler_workspace(ws);
        return -1;
    }

    ws->mtd_plan = fftwf_plan_dft_1d(nfft,
                                     ws->plan_buf,
                                     ws->plan_buf,
                                     FFTW_FORWARD,
                                     FFTW_ESTIMATE);

    if (!ws->mtd_plan) {
        cleanup_doppler_workspace(ws);
        return -1;
    }

    return 0;
}
void cleanup_doppler_workspace(DopplerWorkspace *ws)
{
    if (!ws) {
        return;
    }

    if (ws->mtd_plan) {
        fftwf_destroy_plan(ws->mtd_plan);
    }

    if (ws->plan_buf) {
        fftwf_free(ws->plan_buf);
    }

    free(ws->hamming_win);

    memset(ws, 0, sizeof(*ws));
}

static int is_power_of_two(int n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

static void bit_reverse_permute(float complex *x, int n) {
    int i, j, bit;
    for (i = 1, j = 0; i < n; ++i) {
        bit = n >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            float complex tmp = x[i];
            x[i] = x[j];
            x[j] = tmp;
        }
    }
}

static void fft_inplace(float complex *x, int n) {
    int len, i, j;

    bit_reverse_permute(x, n);

    for (len = 2; len <= n; len <<= 1) {
        float angle = -2.0 * M_PI / (float)len;
        float complex wlen = cos(angle) + I * sin(angle);

        for (i = 0; i < n; i += len) {
            float complex w = 1.0 + I * 0.0;
            for (j = 0; j < len / 2; ++j) {
                float complex u = x[i + j];
                float complex v = x[i + j + len / 2] * w;
                x[i + j] = u + v;
                x[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

static void fftshift_1d(float complex *x, int n) {
    int half = n / 2;
    for (int i = 0; i < half; ++i) {
        float complex tmp = x[i];
        x[i] = x[i + half];
        x[i + half] = tmp;
    }
}

static int apply_mti(const ComplexMatrix *x, int order, ComplexMatrix *y)
{
    if (!x || !x->data || !y || !y->data) {
        return -1;
    }

    int rows = x->rows;
    int cols = x->cols;

    if (y->rows < rows || y->cols < cols) {
        return -1;
    }

    if (order == 1) {
        for (int r = 0; r < rows; r++) {
            const float complex *in_row = &CMAT_AT(x, r, 0);
            float complex *out_row = &CMAT_AT(y, r, 0);

            out_row[0] = 0.0 + I * 0.0;

            for (int c = 1; c < cols; ++c) {
                out_row[c] = in_row[c] - in_row[c - 1];
            }
        }
    }
    else if (order == 2) {
        for (int r = 0; r < rows; r++) {
            const float complex *in_row = &CMAT_AT(x, r, 0);
            float complex *out_row = &CMAT_AT(y, r, 0);

            out_row[0] = 0.0 + I * 0.0;

            if (cols > 1) {
                out_row[1] = 0.0 + I * 0.0;
            }

            for (int c = 2; c < cols; c++) {
                out_row[c] = in_row[c] - 2.0 * in_row[c - 1] + in_row[c - 2];
            }
        }
    }
    else {
        return -1;
    }

    return 0;
}
static int apply_mtd(ComplexMatrix *doppler_map,
                     int pulses,
                     int nfft,
                     DopplerWorkspace *ws)
{
    if (!doppler_map || !doppler_map->data || !ws ||
        !ws->hamming_win || !ws->plan_buf || !ws->mtd_plan) {
        return -1;
    }

    if (pulses <= 0 || nfft <= 0 || nfft < pulses) {
        return -1;
    }

    if (ws->pulses != pulses || ws->nfft != nfft) {
        fprintf(stderr,
                "apply_mtd: workspace mismatch ws_pulses=%d ws_nfft=%d pulses=%d nfft=%d\n",
                ws->pulses, ws->nfft, pulses, nfft);
        return -1;
    }

    if (doppler_map->cols < nfft) {
        fprintf(stderr,
                "apply_mtd: doppler cols too small cols=%d nfft=%d\n",
                doppler_map->cols, nfft);
        return -1;
    }

    int rows = doppler_map->rows;
    float *win = ws->hamming_win;
    float complex *buf = (float complex *)ws->plan_buf;

    for (int r = 0; r < rows; ++r) {
        float complex *row = &CMAT_AT(doppler_map, r, 0);

        /*
         * row -> plan_buf
         * 여기서 hamming window도 같이 적용
         */
        for (int p = 0; p < pulses; ++p) {
            buf[p] = row[p] * win[p];
        }

        for (int p = pulses; p < nfft; ++p) {
            buf[p] = 0.0f + 0.0f * I;
        }

        /*
         * ARMPL FFTW-compatible FFT 실행
         */
        fftwf_execute(ws->mtd_plan);

        /*
         * FFT 결과를 다시 row에 복사
         */
        memcpy(row, buf, (size_t)nfft * sizeof(float complex));

        fftshift_1d(row, nfft);
    }

    return 0;
}

int doppler_fft_processing(const ComplexMatrix *rd_map,
                           int nfft,
                           ComplexMatrix *doppler_map,
                           DopplerFftTiming *timing,
                           DopplerWorkspace *ws)
{
  if (!rd_map || !rd_map->data || !doppler_map || !doppler_map->data || !timing || !ws) {
        return -1;
    }

    int rows = rd_map->rows;
    int pulses = rd_map->cols;

    // if (!is_power_of_two(nfft)) {
    //     return -1;
    // }

    if (nfft < pulses) {
        return -1;
    }
    if (doppler_map->rows != rows || doppler_map->cols < nfft) {
            fprintf(stderr,
                    "doppler_fft_processing: shape mismatch rd=%dx%d dop=%dx%d nfft=%d\n",
                    rd_map->rows, rd_map->cols,
                    doppler_map->rows, doppler_map->cols,
                    nfft);
            return -1;
        }
    // if (doppler_map->rows != rows || doppler_map->cols < nfft) {
    //     return -1;
    // }

    if (timing) {
        timing->mti_ms = 0.0;
        timing->mtd_ms = 0.0;
    }

    double t0 = now_ms();

    /*
     * MTI:
     * rxsig_pc == rd_map
     * doppler_map == MTI 결과 저장 위치
     */
    if (apply_mti(rd_map, 1, doppler_map) != 0) {
        return -1;
    }

    if (timing) {
        timing->mti_ms += now_ms() - t0;
    }

    t0 = now_ms();

    /*
     * MTD:
     * doppler_map에 이미 MTI 결과가 들어 있음.
     * 여기서 window + FFT + fftshift를 doppler_map에 바로 수행.
     */
    if (apply_mtd(doppler_map, pulses, nfft, ws) != 0) {
        return -1;
    }

    if (timing) {
        timing->mtd_ms += now_ms() - t0;
    }

    return 0;
}

// int doppler_fft_processing(const ComplexMatrix *rxsig_pc, const RadarMeta *meta, int nfft, ComplexMatrix *doppler_map) {
//     return doppler_fft_processing_ex(rxsig_pc, meta, nfft, doppler_map, NULL);
// }