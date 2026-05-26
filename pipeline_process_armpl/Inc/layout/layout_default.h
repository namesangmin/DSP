#ifndef LAYOUT_DEFAULT_H
#define LAYOUT_DEFAULT_H

#include <complex.h>
#include "types.h"
#include "pulse.h"

void layout_default_store_raw(float complex *raw_data,
                               const float *buffer,
                               int num_pulses, int num_ranges);

int layout_default_run_pc(PulseCompressCtx *ctx,
                           const float complex *raw_data,
                           float complex *rd_map,
                           int pulse_idx, int num_pulses, int num_ranges,
                           float complex *tmp_buf,   /* 무시 */
                           double *time_ms);

#endif