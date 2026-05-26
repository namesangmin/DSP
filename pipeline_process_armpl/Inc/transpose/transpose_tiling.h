#ifndef TRANSPOSE_TILING_H
#define TRANSPOSE_TILING_H

#include "types.h"

int transpose_tiling(const ComplexMatrix *src,
                     ComplexMatrix       *dst,
                     const RadarMeta     *meta);

#endif