#include <stdlib.h>
#include <stdint.h> // uint8_t 사용
#include <math.h>
#include <complex.h>
#include <string.h>
#include <time.h>

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
    ws->detCapacity = numRange * numDoppler;
    ws->powerMap = (float *)malloc((size_t)numRange * (size_t)numDoppler * sizeof(float));
    ws->col_sum_outer = (float *)calloc((size_t)numDoppler, sizeof(float));
    ws->col_sum_guard = (float *)calloc((size_t)numDoppler, sizeof(float));
    ws->det_mask = (uint8_t *)calloc((size_t)numRange * (size_t)numDoppler, sizeof(uint8_t));
    ws->threshold_map = (float *)malloc((size_t)numRange * numDoppler * sizeof(float));

    if (!ws->powerMap || !ws->col_sum_outer || !ws->col_sum_guard || !ws->det_mask) {
        if (ws->powerMap) free(ws->powerMap);
        if (ws->col_sum_outer) free(ws->col_sum_outer);
        if (ws->col_sum_guard) free(ws->col_sum_guard);
        if (ws->det_mask) free(ws->det_mask);
        memset(ws, 0, sizeof(*ws));
        return -1;
    }

    ws->detBuf = (Detection *)malloc((size_t)ws->detCapacity * sizeof(Detection));
    if (!ws->detBuf) {
        free(ws->col_sum_outer);
        free(ws->col_sum_guard);
        free(ws->powerMap);
        free(ws->det_mask);
        memset(ws, 0, sizeof(*ws));
        return -1;
    }

    return 0;
}

void cleanup_cfar_workspace(CfarWorkspace *ws)
{
    if (!ws) 
    {
        return;
    }

    if (ws->powerMap) free(ws->powerMap);
    if (ws->col_sum_outer) free(ws->col_sum_outer);
    if (ws->col_sum_guard) free(ws->col_sum_guard);
    if (ws->det_mask) free(ws->det_mask);
    if (ws->detBuf) free(ws->detBuf);

    memset(ws, 0, sizeof(*ws));
}

void free_detection_list(DetectionList *list)
{
    if (!list) return;

    list->items = NULL;
    list->count = 0;
}

float get_range_from_bin(int range_bin, float fs_hz) 
{
    const float c = 299792458.0f;
    return ((float)range_bin) * c / (2.0f * (float)fs_hz);
}

float get_velocity_from_bin(int doppler_bin, int nfft, float prf_hz, float fc_hz) 
{
    const float c = 299792458.0f;
    float lambda = c / (float)fc_hz;
    float fd = ((float)doppler_bin - (float)(nfft / 2)) * (prf_hz / (float)nfft);
    return fd * lambda / 2.0f;
}

int cfar_detect(const ComplexMatrix *doppler_map,
                const RadarMeta *meta,
                CfarWorkspace *ws,
                DetectionList *out,
                double *time)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    long sec, nsec;

    if (!doppler_map || !doppler_map->data || !meta || !ws || !out) 
    {
        return -1;
    }

    int numRange = doppler_map->rows;
    int numDoppler = doppler_map->cols;

    if (ws->numRange != numRange || ws->numDoppler != numDoppler ||
        !ws->powerMap || !ws->col_sum_outer || !ws->col_sum_guard ||
        !ws->det_mask || !ws->detBuf) 
    {
        return -1;
    }

    out->count = 0;
    int detCount = 0;

    float *powerMap = ws->powerMap;
    float *col_sum_outer = ws->col_sum_outer;
    float *col_sum_guard = ws->col_sum_guard;
    uint8_t *det_mask = ws->det_mask;
    Detection *detBuf = ws->detBuf;

    const int numTrainR = 16;
    const int numTrainD = 8;
    const int numGuardR = 4;
    const int numGuardD = 2;

    int winR = numTrainR + numGuardR;
    int winD = numTrainD + numGuardD;
        
    int outer_cells = (2 * winR + 1) * (2 * winD + 1);
    int inner_cells = (2 * numGuardR + 1) * (2 * numGuardD + 1);
    int training_cells = outer_cells - inner_cells;

    if (training_cells <= 0)
    {
        return -1;
    }

    const float pfa = 1e-11f;
    const float scale = (float)training_cells * (powf(pfa, -1.0f / (float)training_cells) - 1.0f);
    float final_scale = scale / (float)training_cells;

    for (int r = 0; r < numRange; ++r) 
    {
        size_t pwr_base = (size_t)r * (size_t)numDoppler;
        #pragma GCC ivdep
        for (int d = 0; d < numDoppler; ++d) 
        {
            float complex z = CMAT_AT(doppler_map, r, d);
            powerMap[pwr_base + d] = crealf(z) * crealf(z) + cimagf(z) * cimagf(z);
        }
    }

    int or1 = 0, or2 = 2 * winR;
    int gr1 = winR - numGuardR, gr2 = winR + numGuardR;

    for (int d = 0; d < numDoppler; ++d) 
    {
        float sum_o = 0.0f, sum_g = 0.0f;
        for (int rr = or1; rr <= or2; ++rr) sum_o += powerMap[rr * numDoppler + d];
        for (int rr = gr1; rr <= gr2; ++rr) sum_g += powerMap[rr * numDoppler + d];
        col_sum_outer[d] = sum_o;
        col_sum_guard[d] = sum_g;
    }

    for (int r = winR; r < numRange - winR; ++r) 
    {  
        if (r > winR) 
        {
            int add_o = r + winR, sub_o = r - winR - 1;
            int add_g = r + numGuardR, sub_g = r - numGuardR - 1;

            #pragma GCC ivdep
            for (int d = 0; d < numDoppler; ++d) 
            {
                col_sum_outer[d] += powerMap[add_o * numDoppler + d] - powerMap[sub_o * numDoppler + d];
                col_sum_guard[d] += powerMap[add_g * numDoppler + d] - powerMap[sub_g * numDoppler + d];
            }
        }

        float noise_outer = 0.0f;
        float noise_guard = 0.0f;
        for (int d = 0; d <= 2 * winD; ++d) noise_outer += col_sum_outer[d];
        for (int d = winD - numGuardD; d <= winD + numGuardD; ++d) noise_guard += col_sum_guard[d];

        size_t row_base = (size_t)r * numDoppler;
        for (int d = winD; d < numDoppler - winD; ++d) 
        {    
            float noise_sum = noise_outer - noise_guard;
            float threshold = final_scale * noise_sum;
            int idx = row_base + d;

            ws->threshold_map[idx] = threshold;
            det_mask[idx] = (powerMap[idx] > threshold) ? 1 : 0;

            if (d < numDoppler - winD - 1) 
            {
                noise_outer += col_sum_outer[d + winD + 1] - col_sum_outer[d - winD];
                noise_guard += col_sum_guard[d + numGuardD + 1] - col_sum_guard[d - numGuardD];
            }
        }
    }

    for (int r = winR; r < numRange - winR; ++r) 
    {
        size_t row_base = (size_t)r * numDoppler;
        for (int d = winD; d < numDoppler - winD; ++d) 
        {
            int idx = row_base + d;
            
            if (det_mask[idx]) 
            {
                if (detCount >= ws->detCapacity) return -2;

                Detection det;
                det.range_bin = r;
                det.doppler_bin = d;
                det.range_m = get_range_from_bin(r, meta->fs_hz);
                det.velocity_mps = get_velocity_from_bin(d, numDoppler, meta->prf_hz, meta->fc_hz);
                det.power = powerMap[idx];
                det.threshold = powerMap[idx]; 
                detBuf[detCount++] = det;
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    sec = end.tv_sec - start.tv_sec;
    nsec = end.tv_nsec - start.tv_nsec;
    *time = (double)sec * 1000.0 + (double)nsec / 1000000.0;
    
    if (detCount == 0) 
    {
        out->count = 0;
        out->items = NULL;
        return 0;
    }

    out->items = ws->detBuf;
    out->count = detCount;

    return 0;
}