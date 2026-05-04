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
    if (!w || N <= 0) return;
    if (N == 1) { w[0] = 1.0f; return; }
    for (int n = 0; n < N; ++n)
        w[n] = 0.54f - 0.46f * cosf((2.0f * M_PI * (float)n) / (float)(N - 1));
}

int init_doppler_workspace(DopplerWorkspace *ws, int pulses, int nfft)
{
    if (!ws || pulses <= 0 || nfft < pulses) return -1;

    memset(ws, 0, sizeof(*ws));
    ws->pulses = pulses;
    ws->nfft   = nfft;

    ws->hamming_win = (float *)malloc((size_t)pulses * sizeof(float));
    if (!ws->hamming_win) { cleanup_doppler_workspace(ws); return -1; }
    
    make_hamming_window(pulses, ws->hamming_win);

    ws->plan_buf = (float complex *)fftwf_malloc((size_t)nfft * sizeof(float complex));
    if (!ws->plan_buf) 
    { 
        cleanup_doppler_workspace(ws); 
        return -1;
    }

    ws->local_buf = (float complex *)fftwf_malloc((size_t)nfft * sizeof(float complex));
     if (!ws->local_buf) 
    { 
        cleanup_doppler_workspace(ws); 
        return -1;
    }   
    ws->mtd_plan = fftwf_plan_dft_1d(nfft, ws->plan_buf, ws->plan_buf,
                                     FFTW_FORWARD, FFTW_ESTIMATE);
    if (!ws->mtd_plan)
    {
        cleanup_doppler_workspace(ws); 
        return -1; 
    }

    return 0;
}

void cleanup_doppler_workspace(DopplerWorkspace *ws)
{
    if (!ws) return;
    if (ws->mtd_plan) fftwf_destroy_plan(ws->mtd_plan);
    if (ws->plan_buf) fftwf_free(ws->plan_buf);
    if(ws->local_buf) fftwf_free(ws->local_buf);
    free(ws->hamming_win);
    memset(ws, 0, sizeof(*ws));
}

/* in-place MTI: 뒤에서 앞으로 순회해서 임시 버퍼 없이 처리 */
static int apply_mti(ComplexMatrix *map, int order)
{
    if (!map || !map->data) return -1;

    int rows = map->rows;
    int cols = map->cols;

    if (order == 1) {
        for (int r = 0; r < rows; r++) {
            float complex *row = &CMAT_AT(map, r, 0);
            for (int c = cols - 1; c >= 1; --c)
                row[c] = row[c] - row[c - 1];
            row[0] = 0.0f + 0.0f * I;
        }
    }
    else if (order == 2) {
        for (int r = 0; r < rows; r++) {
            float complex *row = &CMAT_AT(map, r, 0);
            for (int c = cols - 1; c >= 2; --c)
                row[c] = row[c] - 2.0f * row[c - 1] + row[c - 2];
            if (cols > 1) row[1] = 0.0f + 0.0f * I;
            row[0] = 0.0f + 0.0f * I;
        }
    }
    else {
        return -1;
    }

    return 0;
}

static int apply_mtd(ComplexMatrix *doppler_map, int pulses, int nfft, DopplerWorkspace *ws)
{
    if (!doppler_map || !doppler_map->data || !ws ||
        !ws->hamming_win || !ws->plan_buf || !ws->mtd_plan)
        return -1;

    if (pulses <= 0 || nfft <= 0 || nfft < pulses) return -1;

    if (ws->pulses != pulses || ws->nfft != nfft) {
        fprintf(stderr,
                "apply_mtd: workspace mismatch ws_pulses=%d ws_nfft=%d pulses=%d nfft=%d\n",
                ws->pulses, ws->nfft, pulses, nfft);
        return -1;
    }

    if (doppler_map->cols < nfft) {
        fprintf(stderr, "apply_mtd: doppler cols too small cols=%d nfft=%d\n",
                doppler_map->cols, nfft);
        return -1;
    }

    int rows        = doppler_map->rows;
    float *win      = ws->hamming_win;

    //#pragma omp parallel num_threads(2)
    {
        //#pragma omp for schedule(static)
        for (int r = 0; r < rows; ++r) {
            float complex *row = &CMAT_AT(doppler_map, r, 0);

            // 1. 윈도우 적용
            for (int p = 0; p < pulses; ++p) {
                ws->local_buf[p] = row[p] * win[p];
            }

            // 2. Zero-padding
            if (nfft > pulses) {
                memset(&ws->local_buf[pulses], 0, (size_t)(nfft - pulses) * sizeof(float complex));
            }

            // 3. FFT 실행 (ws->mtd_plan 대신 개별 plan 또는 전용 실행 함수 필요)
            // fftwf_execute_dft를 쓰면 버퍼를 지정해서 실행 가능합니다.
            fftwf_execute_dft(ws->mtd_plan, ws->local_buf, ws->local_buf);

            // 4. Shift 및 복사
            int half = nfft / 2;
            for (int i = 0; i < half; ++i) {
                row[i] = ws->local_buf[i + half];
                row[i + half] = ws->local_buf[i];
            }
        }
    }

    return 0;
}

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

    t0 = now_ms();
    if (apply_mtd(doppler_map, pulses, nfft, ws) != 0) return -1;
    timing->mtd_ms = now_ms() - t0;

    return 0;
}