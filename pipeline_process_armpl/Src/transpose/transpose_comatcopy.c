#include "transpose_comatcopy.h"
#include <blas.h>
#include <complex.h>

int transpose_comatcopy(const ComplexMatrix *src,
                        ComplexMatrix       *dst,
                        const RadarMeta     *meta)
{
    const int pulses = meta->num_pulses;
    const int ranges = meta->num_fast_time_samples;

    float complex alpha = 1.0f + 0.0f * I;

    comatcopy(
        'R',                                    /* row-major */
        'T',                                    /* transpose */
        pulses,                                 /* rows of src */
        ranges,                                 /* cols of src */
        alpha,
        (const float complex *)src->data, ranges,  /* src, lda */
        (float complex *)dst->data,       pulses    /* dst, ldb */
    );

    return 0;
}