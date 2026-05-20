#include "transpose_interface.h"
#include "transpose_tiling.h"
#include "transpose_comatcopy.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const TransposeOps tiling_ops    = { transpose_tiling    };
static const TransposeOps comatcopy_ops = { transpose_comatcopy };

Transpose *transpose_create(TransposeType type)
{
    Transpose *t = malloc(sizeof(Transpose));
    if (!t) return NULL;

    t->ops = (type == TRANSPOSE_COMATCOPY) ? &comatcopy_ops
                                           : &tiling_ops;
    return t;
}

void transpose_destroy(Transpose *t)
{
    free(t);
}

int transpose_exec(Transpose *t,
                   const ComplexMatrix *src,
                   ComplexMatrix       *dst,
                   const RadarMeta     *meta,
                   double              *time_ms)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int ret = t->ops->transpose(src, dst, meta);

    clock_gettime(CLOCK_MONOTONIC, &end);
    long sec  = end.tv_sec  - start.tv_sec;
    long nsec = end.tv_nsec - start.tv_nsec;
    *time_ms  = (double)sec * 1000.0 + (double)nsec / 1e6;

    return ret;
}

TransposeType transpose_type_from_str(const char *s)
{
    if (!strcmp(s, "comatcopy")) return TRANSPOSE_COMATCOPY;
    return TRANSPOSE_TILING;
}