#include <stdio.h>
#include <stdatomic.h> // atomic 함수 사용을 위해 추가

#include "timer.h" 
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
}

void *loader_thread_main(void *arg)
{
    LoaderArgs *a = (LoaderArgs *)arg;
    pin_thread_to_cpu(a->cpu_id);

    double t0 = now_ms();

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
    }

    return NULL;
}