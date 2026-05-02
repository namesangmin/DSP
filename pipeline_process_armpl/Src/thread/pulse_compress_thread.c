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
    
    double local_compress_ms = 0.0; // 타이머 변수 복구

    while (pulse_queue_pop(a->q, &job)) {
        if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) {
            break;
        }

        double t0 = now_ms(); // 시간 측정 시작

        const float complex *pulse_raw_ptr = &CMAT_AT(&a->pipe->raw_data, job.pulse_idx, 0);
        int curr_idx = atomic_load_explicit(&a->pipe->current_write_idx, memory_order_acquire); 

        float complex *rd_row_ptr = &CMAT_AT(&a->pipe->rd_maps[curr_idx].data, job.pulse_idx, 0);
        
        if (pulse_compress_one(&a->ctx, pulse_raw_ptr, rd_row_ptr) != 0) {
            fprintf(stderr, "pulse_compress_one failed: pulse_idx=%d\n", job.pulse_idx);
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            return NULL;
        }
        local_compress_ms += now_ms() - t0; // 시간 측정 완료 및 누적
        // 데이터 쓰기가 완료되었음을 보장하기 위해 release 사용
        int done = atomic_fetch_add_explicit(&a->pipe->rd_maps[curr_idx].done_count, 1, memory_order_release) + 1;

        // printf("🛠️ [Worker %d] curr_idx: %d | 꺼낸 pulse_idx: %d | 누적 done: %d\n", 
        //        a->cpu_id, curr_idx, job.pulse_idx, done);


        if (done == a->meta->num_pulses) {
            // 내가 마지막 512번째 펄스를 끝냈다면, 이전 스레드들의 쓰기 결과를 동기화
            atomic_thread_fence(memory_order_acquire);
            
            // Transpose 없이 3번 코어(포스트)에게 번호표만 전달
            PostJob p_job = { .buffer_idx = curr_idx }; 

            if (post_queue_push(&a->pipe->post_q, p_job) != 0) {
                fprintf(stderr, "post_queue_push failed: buffer_idx=%d\n", curr_idx);
                atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
                return NULL;
            }
            post_queue_close(&a->pipe->post_q);
        }
    }

    printf("🔥 [Worker %d] 내가 압축에 쓴 시간: %f ms\n", a->cpu_id, local_compress_ms);
    if (a->timing) {
            if (a->timing->compress_ms < local_compress_ms) {
                a->timing->compress_ms = local_compress_ms;
            }
    }

    return NULL;
}