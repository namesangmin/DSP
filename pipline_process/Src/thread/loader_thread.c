#include <stdio.h>

#include "common.h"
#include "queue.h"
#include "core_set.h"
#include "thread_func.h"
#include "timer.h" 

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

// common.h 등에 정의되어 있어야 할 청크 구조체
/*
typedef struct {
    int start_idx;
    int count;
    const RawIQSample *raw;
} PulseChunkJob;
*/
#if 0
void *loader_chunk_thread_main(void *arg)
{
    LoaderArgs *a = (LoaderArgs *)arg;

    pin_thread_to_cpu(a->cpu_id);

    int chunk_size =1; // 청크 크기 설정 (원하는 2^n 단위로 조절)

    for (int pulse_idx = 0; pulse_idx < a->meta->num_pulses; pulse_idx += chunk_size) {
        
        PulseChunkJob job;
        job.start_idx = pulse_idx;
        job.count = (pulse_idx + chunk_size > a->meta->num_pulses) ? 
                    (a->meta->num_pulses - pulse_idx) : chunk_size;
        
        // 청크의 맨 첫 번째 주소만 던집니다.
        job.raw = dat_mmap_get_pulse(&a->file->mm, pulse_idx);

        if (!job.raw) {
            pulse_chunk_queue_close(a->even_q);
            pulse_chunk_queue_close(a->odd_q);
            pipeline_signal_post(a->file, 1);
            return NULL;
        }

        // 청크 인덱스(0, 1, 2...)를 기준으로 짝/홀수 큐 분배
        int chunk_index = pulse_idx / chunk_size;

        if ((chunk_index & 1) == 0) {
            if (pulse_chunk_queue_push(a->even_q, job) != 0) goto error_exit;
        } else {
            if (pulse_chunk_queue_push(a->odd_q, job) != 0) goto error_exit;
        }
    }

    pulse_chunk_queue_close(a->even_q);
    pulse_chunk_queue_close(a->odd_q);
    
    return NULL;

error_exit:
    pulse_chunk_queue_close(a->even_q);
    pulse_chunk_queue_close(a->odd_q);
    pipeline_signal_post(a->file, 1);
    return NULL;
}
#endif