#ifndef TRANSPOSE_COMATCOPY_H
#define TRANSPOSE_COMATCOPY_H

#include "types.h"

int transpose_comatcopy(const ComplexMatrix *src,
                        ComplexMatrix       *dst,
                        const RadarMeta     *meta);

#endif