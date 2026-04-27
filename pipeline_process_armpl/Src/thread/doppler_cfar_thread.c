#include <stddef.h>
#include "core_set.h"
#include "doppler_cfar_thread.h"
#include "timer.h"
#include <stdio.h>
#include <stdlib.h> // free 사용

void *post_thread_main(void *arg)
{
    PostArgs *a = (PostArgs *)arg;
    PostJob job;
    
    pin_thread_to_cpu(a->cpu_id);
    double total_transpose_ms = 0.0;
    double total_cfar_ms = 0.0;
    
    // [최적화 2] 변하지 않는 상수 연산은 while 루프 진입 전에 단 1번만 미리 계산!
    int numTrainR = 4;
    int numTrainD = 4;
    int numGuardR = 1;
    int numGuardD = 1;
    int totalWindowCells = (2 * (numTrainR + numGuardR) + 1) * (2 * (numTrainD + numGuardD) + 1);
    int guardAndCUTCells = (2 * numGuardR + 1) * (2 * numGuardD + 1);
    int rankIdx = ((totalWindowCells - guardAndCUTCells) + 1) / 2;

    while (post_queue_pop(a->post_q, &job)) {
        
        if (atomic_load_explicit(&a->pool->error, memory_order_relaxed)) {
            break;
        }

        int idx = job.buffer_idx;

        // =========================================================
        // 0. 도플러 처리 전, 1/2번 코어가 만든 rd_map을 Transpose!
        // =========================================================
        double t_trans = now_ms(); // 타이머 시작
        if (transpose_rd_pulse_range_to_doppler_range_pulse(
                &a->pool->rd_maps[idx].data,
                &a->pool->doppler_maps[idx].data,
                a->meta) != 0) {
            fprintf(stderr, "post: transpose failed: buffer_idx=%d\n", idx);
            a->status = -1;
            atomic_store_explicit(&a->pool->error, 1, memory_order_relaxed);
            break;
        }
        total_transpose_ms += (now_ms() - t_trans); // 누적
        // =========================================================
        // 1. 도플러 처리
        // =========================================================
        if (doppler_fft_processing(&a->pool->doppler_maps[idx].data,
                                a->meta->num_pulses,
                                a->doppler_timing,
                                a->doppler_ws) != 0) {
            fprintf(stderr, "post: doppler_fft_processing failed: buffer_idx=%d\n", idx);
            a->status = -1;
            atomic_store_explicit(&a->pool->error, 1, memory_order_relaxed);
            break;
        }

        // [최적화 3] 프로파일링이 끝났다면 now_ms() 호출은 최소화하는 것이 좋습니다.
        double t0 = now_ms();

        // =========================================================
        // 2. CFAR 처리 (미리 계산해둔 상수들을 파라미터로 넘김)
        // =========================================================
        int cfar_ret = cfar_detect(&a->pool->doppler_maps[idx].data,
                           a->meta,
                           numTrainR, numTrainD,
                           numGuardR, numGuardD,
                           rankIdx, 8.0,
                           a->cfar_ws,
                           a->det);

        if (cfar_ret != 0) {
            fprintf(stderr, "post: cfar_detect failed: ret=%d buffer_idx=%d\n", cfar_ret, idx);
            a->status = -2;
            atomic_store_explicit(&a->pool->error, 1, memory_order_relaxed);
            break;
        }
        
        total_cfar_ms += (now_ms() - t0);

        // =========================================================
        // 3. 사용 완료된 rd_map 버퍼 즉시 반납
        // =========================================================
        atomic_store_explicit(&a->pool->rd_maps[idx].done_count, 0, memory_order_release);
        atomic_store_explicit(&a->pool->rd_maps[idx].state, BUF_FREE, memory_order_release);

        // =========================================================
        // 4. 탐지 결과 후처리 및 메모리 해제 [최적화 1 - 누수 방지]
        // =========================================================
        // 여기에 타겟 정보를 외부로 전송하거나 화면에 띄우는 로직 작성
        
        // 데이터 전송이 끝났다면, 다음 루프를 위해 이번에 할당된 메모리를 반드시 날려야 합니다.
        // if (a->det && a->det->items) {
        //     free(a->det->items);
        //     a->det->items = NULL;
        //     a->det->count = 0;
        // }
    }

    if (a->cfar_ms) {
        *a->cfar_ms = total_cfar_ms;
    }
    if (a->transpose_ms) {
        *a->transpose_ms = total_transpose_ms;
    }
    return NULL;
}