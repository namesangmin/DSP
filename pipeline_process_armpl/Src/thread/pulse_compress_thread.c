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
    int tid = a->tid;
    while (1) 
    {
        if (queue_pop_pulse(a->q, &job) != 0) break;
        if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) break;

        // const float complex *pulse_raw_ptr = &a->pipe->raw_data[job.raw_idx][(size_t)job.pulse_idx * a->meta->num_fast_time_samples];
        // float complex *rd_row_ptr = &CMAT_AT(&a->pipe->pulse_compress_map[job.raw_idx].data, job.pulse_idx, 0);

        double execute_time = 0.0;
       int pc_ret = layout_run_pc(a->layout, &a->ctx,
                            a->pipe->raw_data[job.raw_idx],
                            a->pipe->pulse_compress_map[job.raw_idx].data.data,
                            job.pulse_idx,
                            a->meta->num_pulses,
                            a->meta->num_fast_time_samples,
                            a->tmp_buf,        /* 추가 */
                            &execute_time);

        // int pc_ret = pulse_compress_one(&a->ctx, pulse_raw_ptr, rd_row_ptr, &execute_time); 
        if(pc_ret != 0)
        {
            fprintf(stderr, "pulse_compress_one failed: ret=%d pulse_idx=%d\n", pc_ret, job.pulse_idx);
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }
        a->pipe->compress_times[job.raw_idx][tid] += execute_time;
        
        int done = atomic_fetch_add_explicit(&a->pipe->pulse_compress_map[job.raw_idx].done_count,
                                             1, memory_order_release) + 1;
        if (done == a->meta->num_pulses) 
        {
            atomic_thread_fence(memory_order_acquire);
            atomic_store_explicit(&a->pipe->pulse_compress_map[job.raw_idx].state, BUF_READY, memory_order_release);

            PostJob p_job = { .buffer_idx = job.raw_idx };
            if (queue_push_post(a->pipe->post_q, p_job) != 0)
            {
                fprintf(stderr, "post_queue_push failed\n");
                atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
                break;
            }
        }
    }
    int remain = atomic_fetch_sub_explicit(&a->pipe->active_workers, 1, memory_order_acq_rel) - 1;
    if (remain == 0) 
    {
        queue_close_post(a->pipe->post_q);
    }

    return NULL;
}
