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

    int tid = (a->cpu_id == 1) ? 0 : 1;  // pc_timing 인덱스

    while (1) {
        double wait_start = now_ms();

        int got = pulse_queue_pop(a->q, &job);  // 내부 usleep 포함

        pc_timing[tid].wait_ms += now_ms() - wait_start;

        if (!got) break;
        if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) break;

        double work_start = now_ms();

        const float complex *pulse_raw_ptr = &a->pipe->raw_data[job.raw_idx]
                                              [(size_t)job.pulse_idx * a->meta->num_fast_time_samples];
        float complex *rd_row_ptr = &CMAT_AT(&a->pipe->rd_maps[job.raw_idx].data, job.pulse_idx, 0);

        double execute_time = 0.0;
        if (pulse_compress_one(&a->ctx, pulse_raw_ptr, rd_row_ptr, &execute_time) != 0) {
            fprintf(stderr, "pulse_compress_one failed: pulse_idx=%d\n", job.pulse_idx);
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }

        pc_timing[tid].work_ms += now_ms() - work_start;

        int w_idx = (a->cpu_id == 1) ? 0 : 1;
        a->pipe->compress_times[job.raw_idx][w_idx] += execute_time;

        int done = atomic_fetch_add_explicit(&a->pipe->rd_maps[job.raw_idx].done_count,
                                             1, memory_order_release) + 1;
        if (done == a->meta->num_pulses) {
            atomic_thread_fence(memory_order_acquire);
            atomic_store_explicit(&a->pipe->rd_maps[job.raw_idx].state,
                                  BUF_READY, memory_order_release);

            PostJob p_job = { .buffer_idx = job.raw_idx };
            if (post_queue_push(&a->pipe->post_q, p_job) != 0) {
                fprintf(stderr, "post_queue_push failed\n");
                atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
                break;
            }
        }
    }

    int remain = atomic_fetch_sub_explicit(&a->pipe->active_workers, 1, memory_order_acq_rel) - 1;
    if (remain == 0) {
        post_queue_close(&a->pipe->post_q);
    }

    return NULL;
}