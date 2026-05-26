#include <string.h>
#include "pipeline_set.h"
#include "loader.h"
int init_pipeline_pool(const RadarMeta *meta, Pipeline *pool, int num_workers, LayoutType lt)
{
    memset(pool, 0, sizeof(*pool));

    for (int i = 0; i < NUM_BUFFERS; i++) 
    {
        pool->raw_data[i] = (float complex *)fftwf_malloc((size_t)meta->num_pulses * meta->num_fast_time_samples * sizeof(float complex));
        if(!pool->raw_data[i])
        {
            return -1;
        }

        /* init_pipeline_pool에서 */
        if (lt == LAYOUT_LEGACY) {
            if (alloc_complex_matrix(meta->num_fast_time_samples, meta->num_pulses,
                                    &pool->pulse_compress_map[i].data) != 0)
                return -1;
        } 
        else {
            if (alloc_complex_matrix(meta->num_pulses, meta->num_fast_time_samples,
                                    &pool->pulse_compress_map[i].data) != 0)
                return -1;
        }

        atomic_init(&pool->pulse_compress_map[i].state, BUF_FREE);
        atomic_init(&pool->pulse_compress_map[i].done_count, 0);

        if (alloc_complex_matrix(meta->num_fast_time_samples, meta->num_pulses, &pool->doppler_map[i].data) != 0) 
        {
            return -1;
        }

        atomic_init(&pool->doppler_map[i].state, BUF_FREE);
    }

    atomic_init(&pool->error, 0);
    atomic_store(&pool->active_workers, num_workers);
    return 0;
}

void cleanup_pipeline_pool(Pipeline *pool) 
{
    if (!pool) return;
    
    for (int i = 0; i < NUM_BUFFERS; i++) 
    {
        if (pool->raw_data[i]) 
        {
            fftwf_free(pool->raw_data[i]);
            pool->raw_data[i] = NULL;
        }
        
        free_complex_matrix(&pool->pulse_compress_map[i].data);
        free_complex_matrix(&pool->doppler_map[i].data);
    }
}