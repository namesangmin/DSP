#include <stdio.h>
#include <complex.h>
#include <stddef.h>
#include <blas.h>

#include "pulse_compress_thread.h"
#include "core_set.h"
#include "timer.h"

void *worker_thread_main(void *arg)
{
    WorkerArgs *a = (WorkerArgs *)arg;
    PulseJob job;

    pin_thread_to_cpu(a->cpu_id);
    
    while (pulse_queue_pop(a->q, &job)) {
        if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) {
            break;
        }

        const float complex *pulse_raw_ptr = &a->pipe->raw_data[job.raw_idx][(size_t)job.pulse_idx * a->meta->num_fast_time_samples];
        float complex *rd_row_ptr = &CMAT_AT(&a->pipe->rd_maps[job.raw_idx].data, job.pulse_idx, 0);

        double execute_time = 0.0f;
        if (pulse_compress_one(&a->ctx, pulse_raw_ptr, rd_row_ptr, &execute_time) != 0) {
           
            fprintf(stderr, "pulse_compress_one failed: pulse_idx=%d ctx=%p raw=%p rd=%p\n",
                job.pulse_idx, &a->ctx, pulse_raw_ptr, rd_row_ptr);            
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }
        int w_idx = (a->cpu_id == 1) ? 0 : 1;
        a->pipe->compress_times[job.raw_idx][w_idx] += execute_time;

        int done = atomic_fetch_add_explicit(&a->pipe->rd_maps[job.raw_idx].done_count, 1, memory_order_release) + 1;
        if (done == a->meta->num_pulses) {
            //printf("cpu id: %d, done count: %d\n", a->cpu_id, done);

            // 내가 마지막 512번째 펄스를 끝냈다면, 이전 스레드들의 쓰기 결과를 동기화
            atomic_thread_fence(memory_order_acquire);
            atomic_store_explicit(&a->pipe->rd_maps[job.raw_idx].state, BUF_READY, memory_order_release);
           
            PostJob p_job = { .buffer_idx = job.raw_idx }; 
            if (post_queue_push(&a->pipe->post_q, p_job) != 0) {
                fprintf(stderr, "post_queue_push failed: buffer_idx=%d\n", job.raw_idx);
                atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
                return NULL;
            }
        }
    }

    //printf("🔥 [Worker %d] 내가 압축에 쓴 시간: %f ms\n", a->cpu_id, a->timing->compress_ms);
    
    int remain = atomic_fetch_sub_explicit(&a->pipe->active_workers, 1, memory_order_acq_rel) - 1;
    //printf("remain: %d\n", remain);
    if (remain == 0) {
        // 내가 마지막 퇴근자니까 post_q의 문을 잠근다!
        post_queue_close(&a->pipe->post_q);
    }
    return NULL;
}