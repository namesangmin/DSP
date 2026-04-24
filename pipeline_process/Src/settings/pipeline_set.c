#include "pipeline_set.h"

int init_pipeline_pool(const char *dat_path, const RadarMeta *meta, PipelinePool *pool) {
    memset(pool, 0, sizeof(*pool));

    // 3중 버퍼 메모리 할당 및 초기화
    for (int i = 0; i < NUM_BUFFERS; i++) {
        alloc_complex_matrix(meta->num_fast_time_samples, meta->num_pulses, &pool->rd_maps[i].data);
        atomic_init(&pool->rd_maps[i].state, BUF_FREE);
        atomic_init(&pool->rd_maps[i].done_count, 0);

        alloc_real_matrix(meta->num_fast_time_samples, meta->num_pulses, &pool->det_maps[i].data);
        atomic_init(&pool->det_maps[i].state, BUF_FREE);
    }

    atomic_init(&pool->current_write_idx, 0); // 0번 버퍼부터 시작
    pool->error = 0;

    return 0;
}