#include "layout_interface.h"
#include "layout_default.h"
#include "layout_legacy.h"
#include <stdlib.h>
#include <string.h>

static const LayoutOps default_ops = {
    layout_default_store_raw,
    layout_default_run_pc,
};

static const LayoutOps legacy_ops = {
    layout_legacy_store_raw,
    layout_legacy_run_pc,
};

Layout *layout_create(LayoutType type)
{
    Layout *l = malloc(sizeof(Layout));
    if (!l) return NULL;
    l->ops = (type == LAYOUT_LEGACY) ? &legacy_ops : &default_ops;
    return l;
}

void layout_destroy(Layout *l)
{
    free(l);
}

LayoutType layout_type_from_str(const char *s)
{
    if (!strcmp(s, "legacy")) return LAYOUT_LEGACY;
    return LAYOUT_DEFAULT;
}

void layout_store_raw(Layout *l, float complex *raw_data,
                      const float *buffer,
                      int num_pulses, int num_ranges)
{
    l->ops->store_raw(raw_data, buffer, num_pulses, num_ranges);
}

int layout_run_pc(Layout *l, PulseCompressCtx *ctx,
                  const float complex *raw_data,
                  float complex *rd_map,
                  int pulse_idx, int num_pulses, int num_ranges,
                  float complex *tmp_buf,
                  double *time_ms)
{
    return l->ops->run_pc(ctx, raw_data, rd_map,
                          pulse_idx, num_pulses, num_ranges,
                          tmp_buf, time_ms);
}
