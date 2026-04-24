#include <stdio.h>

#include "common.h"
#include "queue.h"
#include "core_set.h"
#include "timer.h" 
#include "loader_thread.h"

void pipeline_signal_post(PipelineFile *file, int error_flag)
{
    pthread_mutex_lock(&file->post_mtx);

    if (error_flag) {
        file->error = 1;
    }
    file->post_ready = 1;

    pthread_cond_signal(&file->post_cv);
    pthread_mutex_unlock(&file->post_mtx);
}

#if 1
void *loader_thread_main(void *arg)
{
    LoaderArgs *a = (LoaderArgs *)arg;

    pin_thread_to_cpu(a->cpu_id);

    for (int pulse_idx = 0; pulse_idx < a->meta->num_pulses; ++pulse_idx) {
        PulseJob job;
        job.pulse_idx = pulse_idx;
        job.raw = dat_mmap_get_pulse(&a->file->mm, pulse_idx);

        if (!job.raw) {
            fprintf(stderr, "dat_mmap_get_pulse failed: pulse_idx=%d\n", pulse_idx);
            pulse_queue_close(a->even_q);
            pulse_queue_close(a->odd_q);
            pipeline_signal_post(a->file, 1);
            return NULL;
        }

        if ((pulse_idx & 1) == 0) {
            if (pulse_queue_push(a->even_q, job) != 0) {
                pulse_queue_close(a->even_q);
                pulse_queue_close(a->odd_q);
                pipeline_signal_post(a->file, 1);
                return NULL;
            }
        } 
        else {
            if (pulse_queue_push(a->odd_q, job) != 0) {
                pulse_queue_close(a->even_q);
                pulse_queue_close(a->odd_q);
                pipeline_signal_post(a->file, 1);
                return NULL;
            }
        }
    }

    pulse_queue_close(a->even_q);
    pulse_queue_close(a->odd_q);
    return NULL;
}
#endif