#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <string.h>

#include "cfar.h"
#include "doppler_fft.h"

int init_cfar_workspace(CfarWorkspace *ws, int numRange, int numDoppler)
{
    if (!ws || numRange <= 0 || numDoppler <= 0) {
        return -1;
    }

    memset(ws, 0, sizeof(*ws));

    ws->numRange = numRange;
    ws->numDoppler = numDoppler;
    ws->ii_rows = numRange + 1;
    ws->ii_cols = numDoppler + 1;
    ws->detCapacity = numRange * numDoppler;

    ws->powerMap = (double *)malloc((size_t)numRange *
                                    (size_t)numDoppler *
                                    sizeof(double));
    if (!ws->powerMap) {
        return -1;
    }

    ws->ii = (double *)malloc((size_t)ws->ii_rows *
                              (size_t)ws->ii_cols *
                              sizeof(double));
    if (!ws->ii) {
        free(ws->powerMap);
        memset(ws, 0, sizeof(*ws));
        return -1;
    }

    ws->detBuf = (Detection *)malloc((size_t)ws->detCapacity *
                                     sizeof(Detection));
    if (!ws->detBuf) {
        free(ws->ii);
        free(ws->powerMap);
        memset(ws, 0, sizeof(*ws));
        return -1;
    }

    return 0;
}

void cleanup_cfar_workspace(CfarWorkspace *ws)
{
    if (!ws) {
        return;
    }

    free(ws->powerMap);
    free(ws->ii);
    free(ws->detBuf);

    memset(ws, 0, sizeof(*ws));
}

static int cmp_detection_power_desc(const void *a, const void *b) 
{
    const Detection *da = (const Detection *)a;
    const Detection *db = (const Detection *)b;
    if (da->power > db->power) return -1;
    if (da->power < db->power) return 1;
    return 0;
}

void free_detection_list(DetectionList *list)
 {
    if (!list) return;

    free(list->items);
    list->items = NULL;
    list->count = 0;
}

static inline double rect_sum(const double *ii, int stride,
                              int r1, int c1, int r2, int c2)
{
    int rr1 = r1;
    int cc1 = c1;
    int rr2 = r2 + 1;
    int cc2 = c2 + 1;

    return ii[(size_t)rr2 * (size_t)stride + (size_t)cc2]
         - ii[(size_t)rr1 * (size_t)stride + (size_t)cc2]
         - ii[(size_t)rr2 * (size_t)stride + (size_t)cc1]
         + ii[(size_t)rr1 * (size_t)stride + (size_t)cc1];
}

double get_range_from_bin(int range_bin, double fs_hz) {
    const double c = 299792458.0;
    return ((double)range_bin) * c / (2.0 * fs_hz);
}

double get_velocity_from_bin(int doppler_bin, int nfft, double prf_hz, double fc_hz) {
    const double c = 299792458.0;
    double lambda = c / fc_hz;
    double fd = ((double)doppler_bin - (double)(nfft / 2)) * (prf_hz / (double)nfft);
    return fd * lambda / 2.0;
}

int cfar_detect(const ComplexMatrix *doppler_map,
                const RadarMeta *meta,
                int numTrainR, int numTrainD,
                int numGuardR, int numGuardD,
                int rankIdx, double scale,
                CfarWorkspace *ws,
                DetectionList *out)
{
    int numRange;
    int numDoppler;
    int winR;
    int winD;
    int detCount = 0;

    (void)rankIdx;

    if (!doppler_map || !doppler_map->data || !meta || !ws || !out) {
        return -1;
    }

    numRange = doppler_map->rows;
    numDoppler = doppler_map->cols;

    if (ws->numRange != numRange ||
        ws->numDoppler != numDoppler ||
        !ws->powerMap ||
        !ws->ii ||
        !ws->detBuf) {
        return -1;
    }

    free_detection_list(out);

    double *powerMap = ws->powerMap;
    double *ii = ws->ii;
    Detection *detBuf = ws->detBuf;

   /*
 * ii는 전체 memset 필요 없음.
 * 0번째 row와 0번째 col만 0이면 됨.
 */
memset(ii, 0, (size_t)ws->ii_cols * sizeof(double));

for (int r = 1; r < ws->ii_rows; ++r) {
    ii[(size_t)r * (size_t)ws->ii_cols] = 0.0;
}

/*
 * powerMap 생성 + integral image 생성 한 번에 처리
 */
for (int r = 0; r < numRange; ++r) {
    double row_sum = 0.0;

    size_t pwr_base = (size_t)r * (size_t)numDoppler;
    size_t ii_prev  = (size_t)r * (size_t)ws->ii_cols;
    size_t ii_cur   = (size_t)(r + 1) * (size_t)ws->ii_cols;

    for (int d = 0; d < numDoppler; ++d) {
        double complex z = CMAT_AT(doppler_map, r, d);
        double re = creal(z);
        double im = cimag(z);
        double pwr = re * re + im * im;

        powerMap[pwr_base + (size_t)d] = pwr;

        row_sum += pwr;

        ii[ii_cur + (size_t)(d + 1)] =
            ii[ii_prev + (size_t)(d + 1)] + row_sum;
    }
}
    winR = numTrainR + numGuardR;
    winD = numTrainD + numGuardD;
        
    int outer_cells = (2 * winR + 1) * (2 * winD + 1);
    int inner_cells = (2 * numGuardR + 1) * (2 * numGuardD + 1);
    int training_cells = outer_cells - inner_cells;
    
    if (training_cells <= 0) {
    return -1;
    }

    double scale_over_training = scale / (double)training_cells;

    for (int r = winR; r < numRange - winR; ++r) {
        size_t pwr_base = (size_t)r * (size_t)numDoppler;

        for (int d = winD; d < numDoppler - winD; ++d) {
            int outer_r1 = r - winR;
            int outer_r2 = r + winR;
            int outer_d1 = d - winD;
            int outer_d2 = d + winD;

            int guard_r1 = r - numGuardR;
            int guard_r2 = r + numGuardR;
            int guard_d1 = d - numGuardD;
            int guard_d2 = d + numGuardD;

            double outer_sum = rect_sum(ii, ws->ii_cols,
                                        outer_r1, outer_d1,
                                        outer_r2, outer_d2);

            double inner_sum = rect_sum(ii, ws->ii_cols,
                                        guard_r1, guard_d1,
                                        guard_r2, guard_d2);

            double noise_sum = outer_sum - inner_sum;
            double threshold = noise_sum * scale_over_training;

            double cut_power = powerMap[pwr_base + (size_t)d];

            if (cut_power > threshold) {
                if (detCount >= ws->detCapacity) {
                    return -2;
                }

                Detection det;
                det.range_bin = r;
                det.doppler_bin = d;
                det.range_m = get_range_from_bin(r, meta->fs_hz);
                det.velocity_mps = get_velocity_from_bin(d, numDoppler, meta->prf_hz, meta->fc_hz);
                det.power = cut_power;
                det.threshold = threshold;

                detBuf[detCount++] = det;
            }
        }
    }

    if (detCount == 0) {
        out->count = 0;
        out->items = NULL;
        return 0;
    }

    out->items = (Detection *)malloc((size_t)detCount * sizeof(Detection));
    if (!out->items) {
        out->count = 0;
        return -3;
    }

    memcpy(out->items, detBuf, (size_t)detCount * sizeof(Detection));
    out->count = detCount;

    return 0;
}
