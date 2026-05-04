#include <stdio.h>
#include <stdatomic.h> // atomic 함수 사용을 위해 추가
#include <fftw3.h>
#include <complex.h>

#include "timer.h" 
#include "loader_thread.h"
#include "loader.h" // 고속 로드 함수 헤더 추가
#include "core_set.h"

int loader_thread_init(const char *dat_path, const RadarMeta *meta,  LoaderArgs *ld, Pipeline* pool) {
    size_t total_doubles = (size_t)meta->num_pulses * meta->num_fast_time_samples *2u;
   
    ld->buffer = (double*)malloc(total_doubles * sizeof(double));
    if (!ld->buffer) {
        return -1;
    }

    // 기본 정보 세팅
    ld->dat_path = dat_path;
    ld->meta = meta;
    ld->pipe = pool;

    return 0;
}

int loader_thread_destroy(LoaderArgs *ld) {
    if (ld == NULL || ld->buffer == NULL) {
        return 0;
    }

    fftwf_free(ld->buffer);
    ld->buffer = NULL;
    
    return 0;
}

void *loader_thread_main(void *arg)
{
    LoaderArgs *a = (LoaderArgs *)arg;
    pin_thread_to_cpu(a->cpu_id);
    double t0 = now_ms();

    FILE *fp = fopen(a->dat_path, "rb");

    if (!fp) {
        atomic_store(&a->pipe->error, 1);
        pulse_queue_close(&a->pipe->even_q);
        pulse_queue_close(&a->pipe->odd_q);
        return NULL;
    }

    fseek(fp, 232, SEEK_SET);

    size_t total_doubles = (size_t)a->meta->num_pulses * a->meta->num_fast_time_samples * 2u;

    if (fread(a->buffer, sizeof(double), total_doubles, fp) != total_doubles)
    {
        atomic_store(&a->pipe->error, 1);
        fclose(fp);
        pulse_queue_close(&a->pipe->even_q);
        pulse_queue_close(&a->pipe->odd_q);
        return NULL;
    }
    fclose(fp);

    int cols = a->meta->num_pulses;
    int rows = a->meta->num_fast_time_samples;
    int half = cols / 2;

    for (int p = 0; p < cols; p++) {
        for (int s = 0; s < rows; s++) {
            size_t idx = (size_t)p * (size_t)rows + (size_t)s;
            size_t bidx    = 2u * idx;

            a->pipe->raw_data[idx][0] = (float)a->buffer[bidx];
            a->pipe->raw_data[idx][1] = (float)a->buffer[bidx + 1];
        }

        PulseJob job = { .pulse_idx = p };
        PulseQueue *q = (p < half) ? &a->pipe->even_q : &a->pipe->odd_q;

        if (pulse_queue_push(q, job) != 0) 
        {
            printf("[Loader ERROR] 큐 Push 실패! pulse_idx: %d\n", p);
            atomic_store(&a->pipe->error, 1);
            break;
        }
    }

    pulse_queue_close(&a->pipe->even_q);
    pulse_queue_close(&a->pipe->odd_q);

    if (a->timing){
        a->timing->loader_ms = now_ms() - t0;
    }

    return NULL;
}