#include <stdio.h>
#include <stdatomic.h> // atomic 함수 사용을 위해 추가

#include "core_set.h"
#include "timer.h" 
#include "loader_thread.h"
#include "loader.h" // 고속 로드 함수 헤더 추가

void *loader_thread_main(void *arg)
{
    LoaderArgs *a = (LoaderArgs *)arg;

    // 0번 코어에 스레드 고정
    pin_thread_to_cpu(a->cpu_id);

    double t0 = now_ms();

    // =================================================================
    // 1. 파일 전체를 메모리(pool->raw_data)로 단 한 번에 고속 로드!
    // =================================================================
    if (load_complex_bin_all_fread(a->dat_path, a->meta->num_pulses, a->meta->num_fast_time_samples, 232, &a->pool->raw_data) != 0) {
        fprintf(stderr, "loader_thread: file load failed\n");
        // 에러 발생 시 알람시계 대신 원자적 변수로 에러 상태 표시
        atomic_store(&a->pool->error, 1);
        // pulse_queue_close(a->even_q);
        // pulse_queue_close(a->odd_q);
        return NULL;
    }

    // 로드 시간 기록
    if (a->out_loader_ms) {
        *a->out_loader_ms = now_ms() - t0;
    }

    // =================================================================
    // 2. 데이터가 메모리에 다 올라갔으니, 워커들에게 번호표(인덱스)만 배분
    // =================================================================
    for (int pulse_idx = 0; pulse_idx < a->meta->num_pulses; ++pulse_idx) {
        PulseJob job;
        job.pulse_idx = pulse_idx;

        // 짝수면 짝수 큐에, 홀수면 홀수 큐에 푸시
        PulseQueue *target_q = ((pulse_idx & 1) == 0) ? a->even_q : a->odd_q;
        
        if (pulse_queue_push(target_q, job) != 0) {
            atomic_store(&a->pool->error, 1);
            // pulse_queue_close(a->even_q);
            // pulse_queue_close(a->odd_q);
            return NULL;
        }
    }

    // 3. 작업 할당 끝 (워커 스레드들에게 더 이상 일거리가 없음을 알림)
    // pulse_queue_close(a->even_q);
    // pulse_queue_close(a->odd_q);
    
    return NULL;
}