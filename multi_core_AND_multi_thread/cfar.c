#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <omp.h>
#include "cfar.h"
#include "doppler_fft.h"

static int cmp_detection_power_desc(const void *a, const void *b) {
    const Detection *da = (const Detection *)a;
    const Detection *db = (const Detection *)b;
    if (da->power > db->power) return -1;
    if (da->power < db->power) return 1;
    return 0;
}

void free_detection_list(DetectionList *list) {
    if (!list) return;
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

static double rect_sum(const double *ii, int cols,
                       int r1, int c1, int r2, int c2) {
    int stride = cols + 1;
    int rr1 = r1;
    int cc1 = c1;
    int rr2 = r2 + 1;
    int cc2 = c2 + 1;

    return ii[(size_t)rr2 * stride + cc2]
         - ii[(size_t)rr1 * stride + cc2]
         - ii[(size_t)rr2 * stride + cc1]
         + ii[(size_t)rr1 * stride + cc1];
}

static double get_range_from_bin_matlab(int range_idx_1based, double fs_hz) {
    const double c = 299792458.0;
    return ((double)(range_idx_1based - 1)) * c / (2.0 * fs_hz);
}

static double get_velocity_from_bin_matlab(int doppler_idx_1based,
                                           int nfft,
                                           double prf_hz,
                                           double fc_hz) {
    const double c = 299792458.0;
    double lambda = c / fc_hz;
    double fd = ((double)(doppler_idx_1based - 1) - floor((double)nfft / 2.0))
              * (prf_hz / (double)nfft);
    return fd * lambda / 2.0;
}

int cfar_detect(const ComplexMatrix *doppler_map,
                const RadarMeta *meta,
                int numTrainR, int numTrainD,
                int numGuardR, int numGuardD,
                int rankIdx, double scale,
                DetectionList *out) {
    int numRange = doppler_map->rows;
    int numDoppler = doppler_map->cols;
    int winR = numTrainR + numGuardR;
    int winD = numTrainD + numGuardD;
    int ii_rows = numRange + 1;
    int ii_cols = numDoppler + 1;

    double *powerMap = NULL;
    double *ii = NULL;
    Detection *detBuf = NULL;
    int *row_counts = NULL;
    int *row_offsets = NULL;
    int detCount = 0;

    (void)rankIdx; /* CA-CFAR에서는 rankIdx 사용 안 함 */

    out->count = 0;
    out->items = NULL;

    powerMap = (double *)calloc((size_t)numRange * (size_t)numDoppler, sizeof(double));
    ii = (double *)calloc((size_t)ii_rows * (size_t)ii_cols, sizeof(double));
    detBuf = (Detection *)calloc((size_t)numRange * (size_t)numDoppler, sizeof(Detection));
    row_counts = (int *)calloc((size_t)numRange, sizeof(int));
    row_offsets = (int *)calloc((size_t)numRange, sizeof(int));
    if (!powerMap || !ii || !detBuf || !row_counts || !row_offsets) {
        free(powerMap);
        free(ii);
        free(detBuf);
        free(row_counts);
        free(row_offsets);
        return -1;
    }

    #pragma omp parallel for collapse(2) schedule(static)
    for (int r = 0; r < numRange; ++r) {
        for (int d = 0; d < numDoppler; ++d) {
            double complex v = CMAT_AT(doppler_map, r, d);
            powerMap[(size_t)r * (size_t)numDoppler + (size_t)d] =
                creal(v) * creal(v) + cimag(v) * cimag(v);
        }
    }

    for (int r = 1; r <= numRange; ++r) {
        double row_sum = 0.0;
        for (int d = 1; d <= numDoppler; ++d) {
            row_sum += powerMap[(size_t)(r - 1) * (size_t)numDoppler + (size_t)(d - 1)];
            ii[(size_t)r * (size_t)ii_cols + (size_t)d] =
                ii[(size_t)(r - 1) * (size_t)ii_cols + (size_t)d] + row_sum;
        }
    }

    #pragma omp parallel for schedule(static)
    for (int r = winR; r < numRange - winR; ++r) {
        int local_count = 0;

        for (int d = winD; d < numDoppler - winD; ++d) {
            int outer_r1 = r - winR;
            int outer_r2 = r + winR;
            int outer_d1 = d - winD;
            int outer_d2 = d + winD;

            int guard_r1 = r - numGuardR;
            int guard_r2 = r + numGuardR;
            int guard_d1 = d - numGuardD;
            int guard_d2 = d + numGuardD;

            double outer_sum = rect_sum(ii, numDoppler, outer_r1, outer_d1, outer_r2, outer_d2);
            double inner_sum = rect_sum(ii, numDoppler, guard_r1, guard_d1, guard_r2, guard_d2);

            int outer_cells = (outer_r2 - outer_r1 + 1) * (outer_d2 - outer_d1 + 1);
            int inner_cells = (guard_r2 - guard_r1 + 1) * (guard_d2 - guard_d1 + 1);
            int training_cells = outer_cells - inner_cells;

            double noise_sum = outer_sum - inner_sum;
            double noise_avg = noise_sum / (double)training_cells;
            double threshold = scale * noise_avg;
            double cut = powerMap[(size_t)r * (size_t)numDoppler + (size_t)d];

            if (cut > threshold) {
                local_count++;
            }
        }

        row_counts[r] = local_count;
    }

    for (int r = 0; r < numRange; ++r) {
        row_offsets[r] = detCount;
        detCount += row_counts[r];
    }

    #pragma omp parallel for schedule(static)
    for (int r = winR; r < numRange - winR; ++r) {
        int write_idx = row_offsets[r];

        for (int d = winD; d < numDoppler - winD; ++d) {
            int outer_r1 = r - winR;
            int outer_r2 = r + winR;
            int outer_d1 = d - winD;
            int outer_d2 = d + winD;

            int guard_r1 = r - numGuardR;
            int guard_r2 = r + numGuardR;
            int guard_d1 = d - numGuardD;
            int guard_d2 = d + numGuardD;

            double outer_sum = rect_sum(ii, numDoppler, outer_r1, outer_d1, outer_r2, outer_d2);
            double inner_sum = rect_sum(ii, numDoppler, guard_r1, guard_d1, guard_r2, guard_d2);

            int outer_cells = (outer_r2 - outer_r1 + 1) * (outer_d2 - outer_d1 + 1);
            int inner_cells = (guard_r2 - guard_r1 + 1) * (guard_d2 - guard_d1 + 1);
            int training_cells = outer_cells - inner_cells;

            double noise_sum = outer_sum - inner_sum;
            double noise_avg = noise_sum / (double)training_cells;
            double threshold = scale * noise_avg;
            double cut = powerMap[(size_t)r * (size_t)numDoppler + (size_t)d];

            if (cut > threshold) {
                Detection det;
                /*
                 * MATLAB 출력 기준으로 1-based bin 저장.
                 * 내부 연산은 0-based로 하고, 출력/거리/속도 계산만 MATLAB 기준으로 맞춘다.
                 */
                det.range_bin = r + 1;
                det.doppler_bin = d + 1;
                det.power = cut;
                det.threshold = threshold;
                det.range_m = get_range_from_bin_matlab(det.range_bin, meta->fs_hz);
                det.velocity_mps = get_velocity_from_bin_matlab(det.doppler_bin,
                                                                numDoppler,
                                                                meta->prf_hz,
                                                                meta->fc_hz);
                detBuf[write_idx++] = det;
            }
        }
    }

    if (detCount > 0) {
        out->items = (Detection *)malloc((size_t)detCount * sizeof(Detection));
        if (!out->items) {
            free(powerMap);
            free(ii);
            free(detBuf);
            free(row_counts);
            free(row_offsets);
            return -1;
        }
        for (int i = 0; i < detCount; ++i) out->items[i] = detBuf[i];
        out->count = detCount;
        qsort(out->items, (size_t)out->count, sizeof(Detection), cmp_detection_power_desc);
    }

    free(powerMap);
    free(ii);
    free(detBuf);
    free(row_counts);
    free(row_offsets);
    return 0;
}
