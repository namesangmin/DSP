#include <string.h>
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
    atomic_init(&pool->error, 0); // 에러 플래그 초기화
    return 0;
}

void cleanup_pipeline_pool(PipelinePool *pool) {
    if (!pool) return;

    // 1. 가장 먼저 읽어왔던 8MB짜리 원본 데이터 해제
    free_complex_matrix(&pool->raw_data);

    // 2. 3중 버퍼(Triple Buffer)에 할당했던 작업용 메모리 싹 다 해제
    for (int i = 0; i < NUM_BUFFERS; i++) {
        free_complex_matrix(&pool->rd_maps[i].data);
        free_real_matrix(&pool->det_maps[i].data);
    }
}