#include <stdio.h>
#include <stdatomic.h> // atomic 함수 사용을 위해 추가

#include "timer.h" 
#include "loader_thread.h"
#include "loader.h" // 고속 로드 함수 헤더 추가

#include <stdio.h>
#include <stdatomic.h> // atomic 함수 사용을 위해 추가

#include "timer.h" 
#include "loader_thread.h"
#include "loader.h" // 고속 로드 함수 헤더 추가

void *loader_thread_main(void *arg)
{
    LoaderArgs *a = (LoaderArgs *)arg;
    //pin_thread_to_cpu(a->cpu_id);

    double t0 = now_ms();

    int num_pulses = a->meta->num_pulses;
    int fast = a->meta->num_fast_time_samples;
    int half = num_pulses / 2;

    // 메모리 먼저 할당
    if (alloc_complex_matrix(num_pulses, fast, &a->pool->raw_data) != 0) {
        atomic_store(&a->pool->error, 1);
        pulse_queue_close(a->even_q);
        pulse_queue_close(a->odd_q);
        return NULL;
    }

    FILE *fp = fopen(a->dat_path, "rb");
    if (!fp) {
        atomic_store(&a->pool->error, 1);
        pulse_queue_close(a->even_q);
        pulse_queue_close(a->odd_q);
        return NULL;
    }

    fseek(fp, 232, SEEK_SET);

    RawIQSample *pulse_buffer = (RawIQSample *)malloc((size_t)fast * sizeof(RawIQSample));
   
    if (!pulse_buffer) {
        atomic_store(&a->pool->error, 1);
        fclose(fp);
        pulse_queue_close(a->even_q);
        pulse_queue_close(a->odd_q);
        return NULL;
    }

    for (int p = 0; p < num_pulses; ++p) {
        // 펄스 1개 읽기
        if (fread(pulse_buffer, sizeof(RawIQSample), (size_t)fast, fp)
                != (size_t)fast) {
            atomic_store(&a->pool->error, 1);
            break;
        }

        // float 변환해서 raw_data에 저장
        float complex *dst = &CMAT_AT(&a->pool->raw_data, p, 0);
        for (int c = 0; c < fast; ++c) {
            dst[c] = (float)pulse_buffer[c].i + (float)pulse_buffer[c].q * I;
        }

        // 읽자마자 바로 큐에 넣음 → 압축 스레드가 즉시 처리 가능
        PulseJob job = { .pulse_idx = p };
        PulseQueue *q = (p < half) ? a->even_q : a->odd_q;

        if (pulse_queue_push(q, job) != 0) {
            atomic_store(&a->pool->error, 1);
            break;
        }
    }

    free(pulse_buffer);
    fclose(fp);
    pulse_queue_close(a->even_q);
    pulse_queue_close(a->odd_q);

    if (a->out_loader_ms)
        *a->out_loader_ms = now_ms() - t0;

    return NULL;
}