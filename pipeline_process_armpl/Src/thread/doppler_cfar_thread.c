#include <stddef.h>
#include <stdio.h>
#include <stdlib.h> // free 사용
#include "core_set.h"
#include "doppler_cfar_thread.h"
#include "pulse.h"
#include "timer.h"
#include "print.h"

void *post_thread_main(void *arg)
{
    PostArgs *a = (PostArgs *)arg;
    PostJob job;
    
    pin_thread_to_cpu(a->cpu_id);

    while (post_queue_pop(&a->pipe->post_q, &job)) 
    {
        if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) 
        {
            break;
        }
        int idx = job.buffer_idx;
        double execute_time = 0.0;
        
a->timing->compress_core1_ms = a->pipe->compress_times[idx][0];
a->timing->compress_core2_ms = a->pipe->compress_times[idx][1];
a->pipe->compress_times[idx][0] = 0.0;
a->pipe->compress_times[idx][1] = 0.0;

// 실제 파이프라인 딜레이는 두 코어 중 "더 오래 걸린 놈"의 시간입니다.
if (a->timing->compress_core1_ms > a->timing->compress_core2_ms) {
    a->timing->compress_ms = a->timing->compress_core1_ms;
} else {
    a->timing->compress_ms = a->timing->compress_core2_ms;
}

        atomic_store_explicit(&a->pipe->rd_maps[idx].state, BUF_PROCESSING, memory_order_release);
        // =========================================================
        // 0. Transpose
        // =========================================================
        if (transpose_rd_pulse_range_to_doppler_range_pulse(
                &a->pipe->rd_maps[idx].data,
                &a->pipe->doppler_maps[idx].data,
                a->meta, &execute_time) != 0) 
        {
            fprintf(stderr, "post: transpose failed: buffer_idx=%d\n", idx);
            a->status = -1;
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }
        a->timing->transpose_ms = execute_time;

        // =========================================================
        // 1. 도플러 처리
        // =========================================================
        if (doppler_fft_processing(&a->pipe->doppler_maps[idx].data,
                                a->meta->num_pulses,
                                a->timing,
                                a->doppler_ws) != 0) 
        {
            fprintf(stderr, "post: doppler_fft_processing failed: buffer_idx=%d\n", idx);
            a->status = -1;
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }

        // =========================================================
        // 2. CFAR
        // =========================================================
        execute_time = 0.0;
        int cfar_ret = cfar_detect(&a->pipe->doppler_maps[idx].data,
                           a->meta,
                           a->cfar_ws,
                           a->det, &execute_time);

        if (cfar_ret != 0) 
        {
            fprintf(stderr, "post: cfar_detect failed: ret=%d buffer_idx=%d\n", cfar_ret, idx);
            a->status = -2;
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }
        a->timing->cfar_ms = execute_time;

        // =========================================================
        // 3. 버퍼 반납 + 인덱스 증가
        // =========================================================
        atomic_store_explicit(&a->pipe->rd_maps[idx].done_count, 0, memory_order_release);
        atomic_store_explicit(&a->pipe->rd_maps[idx].state, BUF_FREE, memory_order_release);
      
        // =========================================================
        // 4. 결과 출력 + 누적
        // =========================================================
        int fi = *a->valid_files;

        // printf("\n=========================================================\n");
        // printf("[FILE %d]\n", fi + 1);
        // printf("=========================================================\n");

       // print_file_result(a->timing, a->det, fi + 1);

        Detection best = {0};
        best.range_bin = -1;
        accumulate_result(a->total_acc, a->timing, a->det, &best);

        if (a->history && fi >= 0) {
            a->history[fi].count = (best.range_bin != -1) ? 1 : 0;
            if (a->history[fi].count > 0) {
                a->history[fi].items = malloc(sizeof(Detection));
                if (a->history[fi].items)
                    a->history[fi].items[0] = best;
            }
        }

        (*a->valid_files)++;
       // printf("valid files: %d\n", *a->valid_files);
        // =========================================================
        // 5. 다음 파일 위한 리셋
        // =========================================================
        memset(a->timing, 0, sizeof(*a->timing));

        a->det->items = NULL;
        a->det->count = 0;
    }

    return NULL;
}