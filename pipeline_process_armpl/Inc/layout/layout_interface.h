#ifndef LAYOUT_INTERFACE_H
#define LAYOUT_INTERFACE_H

#include <complex.h>
#include "types.h"
#include "pulse.h"

typedef enum {
    LAYOUT_DEFAULT,
    LAYOUT_LEGACY,
} LayoutType;

typedef struct {
    /* raw_data에 수신 데이터 저장 */
    void (*store_raw)(float complex *raw_data,
                      const float *buffer,
                      int num_pulses, int num_ranges);

    /* PC 입력 추출 + 결과 저장 */
    int (*run_pc)(PulseCompressCtx *ctx,
                   const float complex *raw_data,
                   float complex *rd_map,
                   int pulse_idx, int num_pulses, int num_ranges,
                   float complex *tmp_buf,
                   double *time_ms);
} LayoutOps;

typedef struct {
    const LayoutOps *ops;
} Layout;

Layout    *layout_create       (LayoutType type);
void       layout_destroy      (Layout *l);
LayoutType layout_type_from_str(const char *s);

void layout_store_raw(Layout *l, float complex *raw_data,
                      const float *buffer,
                      int num_pulses, int num_ranges);

int layout_run_pc(Layout *l, PulseCompressCtx *ctx,
                  const float complex *raw_data,
                  float complex *rd_map,
                  int pulse_idx, int num_pulses, int num_ranges,
                  float complex *tmp_buf,
                  double *time_ms);

#endif