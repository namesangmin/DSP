#include <stddef.h>
#include "core_set.h"
#include "doppler_cfar_thread.h"
#include "timer.h"
#include <stdio.h>
void *post_thread_main(void *arg)
{
    PostArgs *a = (PostArgs *)arg;
    PostJob job;
    
    pin_thread_to_cpu(a->cpu_id);

    double total_cfar_ms = 0.0;
    
    // [핵심 1] 큐 내부에 spin-wait가 내장되어 있으므로 while(1)은 완전히 버립니다.
    // 1, 2번 코어가 번호표(idx)를 큐에 밀어 넣는 즉시 여기서 낚아챕니다.
    while (post_queue_pop(a->post_q, &job)) {
        
        // 에러가 났다면 즉시 탈출 (가벼운 relaxed 모드)
        if (atomic_load_explicit(&a->pool->error, memory_order_relaxed)) {
            break;
        }

        // 워커(1, 2번)가 완성해둔 0, 1, 2번 식판 중 하나의 번호
        int idx = job.buffer_idx;
//fprintf(stderr, "post: got buffer_idx=%d\n", job.buffer_idx);
        // =========================================================
        // 1. 도플러 처리
        // =========================================================
        // double doppler_firstTime = now_ms();
        
        // 입력: 1, 2번 코어가 막 완성해준 rd_maps[idx].data
        // 출력: a->doppler (3번 코어 혼자 순차적으로 돌기 때문에 공용 버퍼 하나만 써도 충돌 안 남)
        if (doppler_fft_processing(&a->pool->rd_maps[idx].data, 
                                      a->meta->num_pulses,
                                      &a->pool->doppler_maps[idx].data,
                                      a->doppler_timing) != 0) {
            fprintf(stderr, "post: doppler_fft_processing failed: buffer_idx=%d\n", job.buffer_idx);
            atomic_store_explicit(&a->pool->error, 1, memory_order_relaxed);
            break;
        }
        // a->doppler_timing->timing += (now_ms() - doppler_firstTime);


        // =========================================================
        // 2. CFAR 처리
        // =========================================================
        double t0 = now_ms();

        int numTrainR = 4;
        int numTrainD = 4;
        int numGuardR = 1;
        int numGuardD = 1;
        int totalWindowCells = (2 * (numTrainR + numGuardR) + 1) * (2 * (numTrainD + numGuardD) + 1);
        int guardAndCUTCells = (2 * numGuardR + 1) * (2 * numGuardD + 1);
        int rankIdx = ((totalWindowCells - guardAndCUTCells) + 1) / 2;

        int cfar_ret = cfar_detect(&a->pool->doppler_maps[idx].data,
                           a->meta,
                           numTrainR, numTrainD,
                           numGuardR, numGuardD,
                           rankIdx, 8.0,
                           a->cfar_ws,
                           a->det);

        if (cfar_ret != 0) {
            fprintf(stderr,
                    "post: cfar_detect failed: ret=%d buffer_idx=%d "
                    "dop_rows=%d dop_cols=%d ws_range=%d ws_dop=%d "
                    "powerMap=%p ii=%p detBuf=%p detCapacity=%d\n",
                    cfar_ret,
                    idx,
                    a->pool->doppler_maps[idx].data.rows,
                    a->pool->doppler_maps[idx].data.cols,
                    a->cfar_ws ? a->cfar_ws->numRange : -1,
                    a->cfar_ws ? a->cfar_ws->numDoppler : -1,
                    a->cfar_ws ? (void *)a->cfar_ws->powerMap : NULL,
                    a->cfar_ws ? (void *)a->cfar_ws->ii : NULL,
                    a->cfar_ws ? (void *)a->cfar_ws->detBuf : NULL,
                    a->cfar_ws ? a->cfar_ws->detCapacity : -1);

            a->status = -2;
            atomic_store_explicit(&a->pool->error, 1, memory_order_relaxed);
            break;
        }
        total_cfar_ms += (now_ms() - t0);


        // =========================================================
        // 3. [가장 중요] 사용 완료된 rd_map 버퍼 즉시 반납
        // =========================================================
        // 도플러가 이 식판(idx)의 데이터를 다 빨아먹었으므로, 
        // 0번 코어(로더)가 다음 데이터를 덮어쓸 수 있도록 상태를 풀어줍니다.
        // memory_order_release를 써서 "이제 이 식판 완벽히 비었음!" 이라고 파이프라인 전체에 공표합니다.
        atomic_store_explicit(&a->pool->rd_maps[idx].done_count, 0, memory_order_release);
        atomic_store_explicit(&a->pool->rd_maps[idx].state, BUF_FREE, memory_order_release);


        // =========================================================
        // 4. 탐지 결과(a->det) 후처리
        // =========================================================
        // ※ 실시간 시스템이므로 여기서 a->det를 화면에 띄우든 네트워크로 쏘든 하고
        // 다음 루프에서 탐지 결과가 누적(메모리 누수)되지 않도록 비워주는 로직이 필요합니다.
        // free_detection_list(a->det);
    }

    if (a->cfar_ms) {
        *a->cfar_ms = total_cfar_ms;
    }
    return NULL;
}