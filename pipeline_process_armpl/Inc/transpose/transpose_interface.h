#ifndef TRANSPOSE_INTERFACE_H
#define TRANSPOSE_INTERFACE_H

#include "types.h"

typedef enum {
    TRANSPOSE_TILING,
    TRANSPOSE_COMATCOPY,
} TransposeType;

typedef struct {
    int (*transpose)(const ComplexMatrix *src,
                     ComplexMatrix       *dst,
                     const RadarMeta     *meta);
} TransposeOps;

typedef struct {
    const TransposeOps *ops;
} Transpose;

Transpose    *transpose_create      (TransposeType type);
void          transpose_destroy     (Transpose *t);
int           transpose_exec        (Transpose *t,
                                     const ComplexMatrix *src,
                                     ComplexMatrix       *dst,
                                     const RadarMeta     *meta,
                                     double              *time_ms);
TransposeType transpose_type_from_str(const char *s);

#endif