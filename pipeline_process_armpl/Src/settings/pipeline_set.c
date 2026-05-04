#include <string.h>
#include "pipeline_set.h"
#include "loader.h"
int init_pipeline_pool(const char *dat_path, const RadarMeta *meta, Pipeline *pool) 
{
    memset(pool, 0, sizeof(*pool));
   
    size_t total_doubles = (size_t)meta->num_pulses * meta->num_fast_time_samples;

    pool->raw_data = (float complex *)fftwf_malloc(total_doubles * sizeof(float complex));
    if(!pool->raw_data)
    {
        return -1;
    }

    for (int i = 0; i < NUM_BUFFERS; i++) 
    {
        alloc_complex_matrix(meta->num_pulses, meta->num_fast_time_samples, &pool->rd_maps[i].data);
        atomic_init(&pool->rd_maps[i].state, BUF_FREE);
        atomic_init(&pool->rd_maps[i].done_count, 0);

        alloc_complex_matrix(meta->num_fast_time_samples, meta->num_pulses, &pool->doppler_maps[i].data);
        atomic_init(&pool->doppler_maps[i].state, BUF_FREE);
    }

    atomic_init(&pool->current_write_idx, 0);
    atomic_init(&pool->error, 0);
    return 0;
}

void cleanup_pipeline_pool(Pipeline *pool) 
{
    if (!pool) return;

    if (pool->raw_data) {
        fftwf_free(pool->raw_data);
        pool->raw_data = NULL;
    }

    for (int i = 0; i < NUM_BUFFERS; i++) 
    {
        free_complex_matrix(&pool->rd_maps[i].data);
        free_complex_matrix(&pool->doppler_maps[i].data);
    }
}