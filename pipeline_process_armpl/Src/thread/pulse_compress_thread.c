#include <stdio.h>
#include <complex.h>
#include <stddef.h>
#include <blas.h>

#include "pulse_compress_thread.h"
#include "core_set.h"
#include "timer.h"

#if 1
void *worker_thread_main(void *arg)
{
    WorkerArgs *a = (WorkerArgs *)arg;
    PulseJob job;

    pin_thread_to_cpu(a->cpu_id);
    
    //int num_pulses = a->meta->num_pulses;                    // 512
    int num_range_bins = a->meta->num_fast_time_samples;      // 1001
    double local_compress_ms = 0.0;

    PulseQueue *my_q = a->q;

    while (pulse_queue_pop(my_q, &job)) {
        // 가장 가벼운 relaxed 모드로 에러 체크
        if (atomic_load_explicit(&a->pool->error, memory_order_relaxed)) {
            break;
        }

        double t0 = now_ms();

        const float complex *pulse_raw_ptr = &CMAT_AT(&a->pool->raw_data, job.pulse_idx, 0);
        int curr_idx = atomic_load_explicit(&a->pool->current_write_idx, memory_order_acquire); 

        float complex *rd_row_ptr =
            &CMAT_AT(&a->pool->rd_maps[curr_idx].data, job.pulse_idx, 0);

        if (pulse_compress_one(&a->ctx, pulse_raw_ptr, rd_row_ptr) != 0) {
            fprintf(stderr, "pulse_compress_one failed: pulse_idx=%d\n", job.pulse_idx);
            atomic_store_explicit(&a->pool->error, 1, memory_order_relaxed);
            return NULL;
        }

        //int curr_idx = atomic_load_explicit(&a->pool->current_write_idx, memory_order_acquire); 

        // 결과 저장 (세로 방향)
        // for (int r = 0; r < num_range_bins; ++r) {
        //     CMAT_AT(&a->pool->rd_maps[curr_idx].data, r, job.pulse_idx) = a->ctx.out_buf[r];
        // }

        // 해당 인덱스(curr_idx)의 카운터를 올림
        int done = atomic_fetch_add_explicit(&a->pool->rd_maps[curr_idx].done_count, 1, memory_order_release) + 1;
        
        // 내가 이 버퍼(curr_idx)의 마지막 512번째 펄스를 처리한 놈이라면?
        if (done == a->meta->num_pulses) {
            if (transpose_rd_pulse_range_to_doppler_range_pulse(
                    &a->pool->rd_maps[curr_idx].data,
                    &a->pool->doppler_maps[curr_idx].data,
                    a->meta) != 0) {
                fprintf(stderr, "transpose failed: buffer_idx=%d\n", curr_idx);
                atomic_store_explicit(&a->pool->error, 1, memory_order_relaxed);
                return NULL;
            }
            
            PostJob p_job = { .buffer_idx = curr_idx }; // 현재 완성한 식판 번호를 담음
            
            // 3번 코어(포스트)에게 번호표 전달
            if (post_queue_push(a->post_q, p_job) != 0) {
                fprintf(stderr, "post_queue_push failed: buffer_idx=%d\n", curr_idx);
                atomic_store_explicit(&a->pool->error, 1, memory_order_relaxed);
                return NULL;
            }
        }
        local_compress_ms += now_ms() - t0;
    }

    a->compress_ms = local_compress_ms;
    return NULL;
}

#endif
