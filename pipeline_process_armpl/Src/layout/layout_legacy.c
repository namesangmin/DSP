#include "layout_legacy.h"
#include <stdlib.h>
#include <complex.h>
#include <time.h>   /* 추가 */

void layout_legacy_store_raw(float complex *raw_data,
                              const float *buffer,
                              int num_pulses, int num_ranges)
{
    /* [range][pulse] 1001×512로 전치해서 저장 */
    const float complex *src = (const float complex *)buffer;
    for (int p = 0; p < num_pulses; p++)
        for (int r = 0; r < num_ranges; r++)
            raw_data[r * num_pulses + p] = src[p * num_ranges + r];
}

int layout_legacy_run_pc(PulseCompressCtx *ctx,
                          const float complex *raw_data,
                          float complex *rd_map,
                          int pulse_idx, int num_pulses, int num_ranges,
                          float complex *tmp_buf,
                          double *time_ms)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int r = 0; r < num_ranges; r++)
        tmp_buf[r] = raw_data[r * num_pulses + pulse_idx];

    double pc_time = 0.0;
    int ret = pulse_compress_one(ctx, tmp_buf, tmp_buf, &pc_time);

    for (int r = 0; r < num_ranges; r++)
        rd_map[r * num_pulses + pulse_idx] = tmp_buf[r];

    clock_gettime(CLOCK_MONOTONIC, &end);
    long sec  = end.tv_sec  - start.tv_sec;
    long nsec = end.tv_nsec - start.tv_nsec;
    *time_ms  = (double)sec * 1000.0 + (double)nsec / 1e6;

    return ret;
}