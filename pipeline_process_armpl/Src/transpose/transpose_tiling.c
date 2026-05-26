#include "transpose_tiling.h"
#include "types.h"

#define TILE 16

int transpose_tiling(const ComplexMatrix *src,
                     ComplexMatrix       *dst,
                     const RadarMeta     *meta)
{
    const int pulses = meta->num_pulses;
    const int ranges = meta->num_fast_time_samples;

    const float complex *restrict s = (const float complex *)src->data;
    float complex       *restrict d = (float complex *)dst->data;

    for (int c = 0; c < ranges; c += TILE) {
        for (int r = 0; r < pulses; r += TILE) {
            int c_end = (c + TILE > ranges) ? ranges : c + TILE;
            int r_end = (r + TILE > pulses) ? pulses : r + TILE;
            for (int j = c; j < c_end; j++) {
                float complex *restrict d_ptr = &d[j * pulses];
                for (int i = r; i < r_end; i++)
                    d_ptr[i] = s[i * ranges + j];
            }
        }
    }
    return 0;
}