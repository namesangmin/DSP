#include <stdio.h>

#include "common.h"
#include "queue.h"
#include "core_set.h"
#include "thread_func.h"
#include "timer.h" 
<<<<<<< Updated upstream

void pipeline_signal_post(PipelineFile *file, int error_flag)
{
    pthread_mutex_lock(&file->post_mtx);

    if (error_flag) {
        file->error = 1;
    }
    file->post_ready = 1;

    pthread_cond_signal(&file->post_cv);
    pthread_mutex_unlock(&file->post_mtx);
=======
#include "loader_thread.h"
#include "loader.h" // 고속 로드 함수 헤더 추가
#include "core_set.h"

int loader_thread_init(const char *dat_path, const RadarMeta *meta,  LoaderArgs *ld, Pipeline* pool) {
    ld->pulse_buffer = (RawIQSample *)malloc((size_t)meta->num_fast_time_samples * sizeof(RawIQSample));
   
    if (!ld->pulse_buffer) {
        free_complex_matrix(&pool->raw_data);
        return -1;
    }

    // 기본 정보 세팅
    ld->dat_path = dat_path;
    ld->meta = meta;
    ld->pipe = pool;

    return 0;
}

int loader_thread_destroy(LoaderArgs *ld) {
    if (ld == NULL || ld->pulse_buffer == NULL) {
        return 0;
    }

    free(ld->pulse_buffer);
    ld->pulse_buffer = NULL;
    
    return 0;
>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
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
=======
    int num_pulses = a->meta->num_pulses;
    int fast = a->meta->num_fast_time_samples;
    int half = num_pulses / 2;

    FILE *fp = fopen(a->dat_path, "rb");

    if (!fp) {
        atomic_store(&a->pipe->error, 1);
        pulse_queue_close(&a->pipe->even_q);
        pulse_queue_close(&a->pipe->odd_q);
        return NULL;
    }

    fseek(fp, 232, SEEK_SET);

    if (!a->pulse_buffer) {
        atomic_store(&a->pipe->error, 1);
        fclose(fp);
        pulse_queue_close(&a->pipe->even_q);
        pulse_queue_close(&a->pipe->odd_q);
        return NULL;
    }

    for (int p = 0; p < num_pulses; ++p) {

        if (fread(a->pulse_buffer, sizeof(RawIQSample), (size_t)fast, fp) != (size_t)fast) {
            atomic_store(&a->pipe->error, 1);
            break;
        }

        float complex *dst = &CMAT_AT(&a->pipe->raw_data, p, 0);
        
        for (int c = 0; c < fast; ++c) {
            dst[c] = (float)a->pulse_buffer[c].i + (float)a->pulse_buffer[c].q * I;
        }

        PulseJob job = { .pulse_idx = p };
        PulseQueue *q = (p < half) ? &a->pipe->even_q : &a->pipe->odd_q;
        //printf("📦 [Loader] 큐 Push -> pulse_idx: %d (대상: %s 큐)\n", p, (p < half) ? "EVEN" : "ODD");

        if (pulse_queue_push(q, job) != 0) {
            printf("🚨 [Loader ERROR] 큐 Push 실패! pulse_idx: %d\n", p);
            atomic_store(&a->pipe->error, 1);
            break;
        }
    }
    
    fclose(fp);
    pulse_queue_close(&a->pipe->even_q);
    pulse_queue_close(&a->pipe->odd_q);

    if (a->timing){
        a->timing->loader_ms = now_ms() - t0;
>>>>>>> Stashed changes
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