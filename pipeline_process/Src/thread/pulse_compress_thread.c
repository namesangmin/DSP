#include <stdio.h>
#include <complex.h>
#include <stddef.h>

#include "pulse_compress_thread.h"
#include "core_set.h"
#include "timer.h"

#if 1
void *worker_thread_main(void *arg)
{
    WorkerArgs *a = (WorkerArgs *)arg;
    PulseJob job;

    pin_thread_to_cpu(a->cpu_id);
    
    int num_range_bins = a->pool->raw_data.rows;
    int num_pulses = a->pool->raw_data.cols;
    double local_compress_ms = 0.0;

    PulseQueue *my_q = a->q;

    while (pulse_queue_pop(my_q, &job)) {
        // 가장 가벼운 relaxed 모드로 에러 체크
        if (atomic_load_explicit(&a->pool->error, memory_order_relaxed)) {
            break;
        }

        double t0 = now_ms();

        double complex *pulse_raw_ptr = &a->pool->raw_data.data[job.pulse_idx * num_range_bins];

        if (pulse_compress_one(&a->ctx, pulse_raw_ptr, a->ctx.out_buf) != 0) {
            fprintf(stderr, "pulse_compress_one failed: pulse_idx=%d\n", job.pulse_idx);
            atomic_store_explicit(&a->pool->error, 1, memory_order_relaxed);
            return NULL;
        }
        local_compress_ms += now_ms() - t0;

        // [핵심]: 쓰레기 하드코딩 삭제. 
        // 0번 코어(로더)가 지금 "몇 번 식판에 담아라"라고 지정한 인덱스를 실시간으로 가져옴.
        int curr_idx = atomic_load_explicit(&a->pool->current_write_idx, memory_order_acquire); 

        // 결과 저장 (세로 방향)
        for (int r = 0; r < num_range_bins; ++r) {
            size_t idx = (size_t)r * (size_t)num_pulses + (size_t)job.pulse_idx;
            a->pool->rd_maps[curr_idx].data.data[idx] = a->ctx.out_buf[r];
        }

        // 해당 인덱스(curr_idx)의 카운터를 올림
        int done = atomic_fetch_add_explicit(&a->pool->rd_maps[curr_idx].done_count, 1, memory_order_release) + 1;
        
        // 내가 이 버퍼(curr_idx)의 마지막 512번째 펄스를 처리한 놈이라면?
        if (done == a->meta->num_pulses) {
            PostJob p_job = { .buffer_idx = curr_idx }; // 현재 완성한 식판 번호를 담음
            
            // 3번 코어(포스트)에게 번호표 전달
            while (!post_queue_push(a->post_q, p_job)) {
                // 3중 버퍼이므로 꽉 찰 일은 거의 없지만, 실시간 안전장치로 유지
            }
        }
    }

    a->compress_ms = local_compress_ms;
    return NULL;
}

#endif
