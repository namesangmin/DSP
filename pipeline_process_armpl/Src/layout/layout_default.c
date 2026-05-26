#include "layout_default.h"
#include <string.h>
#include <complex.h>

void layout_default_store_raw(float complex *raw_data,
                               const float *buffer,
                               int num_pulses, int num_ranges)
{
    /* [pulse][range] 512×1001 그대로 복사 */
    memcpy(raw_data, buffer,
           (size_t)num_pulses * num_ranges * sizeof(float complex));
}

int layout_default_run_pc(PulseCompressCtx *ctx,
                           const float complex *raw_data,
                           float complex *rd_map,
                           int pulse_idx, int num_pulses, int num_ranges,
                           float complex *tmp_buf,   /* 무시 */
                           double *time_ms)
{
    const float complex *src = &raw_data[(size_t)pulse_idx * num_ranges];
    float complex       *dst = &rd_map  [(size_t)pulse_idx * num_ranges];
    return pulse_compress_one(ctx, src, dst, time_ms);
}