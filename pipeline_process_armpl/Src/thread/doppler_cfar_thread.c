#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "core_set.h"
#include "doppler_cfar_thread.h"
#include "pulse.h"
#include "timer.h"
#include "print.h"
#include "udp.h"
#include "send_graph_data.h"

void *post_thread_main(void *arg)
{
    PostArgs *a = (PostArgs *)arg;
    PostJob job; 
    
    pin_thread_to_cpu(a->cpu_id);

    while (1) {
        double wait_start = now_ms();
        int got = post_queue_pop(&a->pipe->post_q, &job);

        post_timing.wait_ms += now_ms() - wait_start;

        if (got) break;
        if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) break;

        // 실제 처리 시간 측정 시작
        double work_start = now_ms();

        int idx = job.buffer_idx;
        
        a->timing->compress_core1_ms = a->pipe->compress_times[idx][0];
        a->timing->compress_core2_ms = a->pipe->compress_times[idx][1];
        a->pipe->compress_times[idx][0] = 0.0;
        a->pipe->compress_times[idx][1] = 0.0;

        if (a->timing->compress_core1_ms > a->timing->compress_core2_ms) {
            a->timing->compress_ms = a->timing->compress_core1_ms;
        } 
        else {
            a->timing->compress_ms = a->timing->compress_core2_ms;
        }
 
        atomic_store_explicit(&a->pipe->pulse_compress_map[idx].state, BUF_PROCESSING, memory_order_release);

        double execute_time = 0.0;
        int transpose_ret = transpose_rd_pulse_range_to_doppler_range_pulse(
                &a->pipe->pulse_compress_map[idx].data,
                &a->pipe->doppler_map[idx].data,
                a->meta, &execute_time); 
        if (transpose_ret != 0) 
        {
            fprintf(stderr, "post: transpose failed: ret=%d buffer_idx=%d\n", transpose_ret, idx);
            a->status = -1;
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }
        a->timing->transpose_ms = execute_time;

        int doppler_ret = doppler_fft_processing(&a->pipe->doppler_map[idx].data, a->meta->num_pulses, a->timing, a->doppler_ws);
        if ( doppler_ret != 0) 
        {
            fprintf(stderr, "post: doppler_fft_processing failed: ret=%d buffer_idx=%d\n", doppler_ret, idx);
            a->status = -1;
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }

        execute_time = 0.0;
        int cfar_ret = cfar_detect(&a->pipe->doppler_map[idx].data, a->meta, a->cfar_ws, a->det, &execute_time);
        if (cfar_ret != 0) 
        {
            fprintf(stderr, "post: cfar_detect failed: ret=%d buffer_idx=%d\n", cfar_ret, idx);
            a->status = -2;
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }
        a->timing->cfar_ms = execute_time;

        execute_time = 0.0;
        cluster_detections(a->cfar_ws->det_mask,
                        a->cfar_ws->powerMap,
                        a->cluster_params,
                        a->cluster_ws,
                        a->clusters,
                        &execute_time);
        a->timing->cluster_ms = execute_time;

        // cluster history 저장
        int fi = *a->valid_files;

        if (a->cluster_history && a->clusters->count > 0) {
            a->cluster_history[fi].count = a->clusters->count;
            a->cluster_history[fi].items = malloc(a->clusters->count * sizeof(ClusterResult));
            if (a->cluster_history[fi].items) {
                memcpy(a->cluster_history[fi].items, a->clusters->items,
                    a->clusters->count * sizeof(ClusterResult));
            }
        }
        
        // =========================================================
        // 4. 그래프 + 표적 정보 보냄
        // =========================================================
        uint32_t dwell_id = a->pipe->dwell_ids[idx];
        float phi = a->pipe->phi[idx];

        udp_target_t targets[MAX_TARGETS];
        for (int i = 0; i < a->clusters->count; i++) {
            targets[i].distance = a->clusters->items[i].range_m;
            targets[i].speed = a->clusters->items[i].velocity_mps;
        }

        udp_loop(dwell_id, (uint32_t)a->clusters->count, targets, phi, a->timing);
        send_graph_data(dwell_id,
            a->meta->num_fast_time_samples, a->meta->num_pulses,
            a->pipe->raw_data[idx],
            a->pipe->pulse_compress_map[idx].data.data,
            a->cfar_ws->powerMap,
            a->cfar_ws->threshold_map,
            a->cfar_ws->det_mask);

        // 처리 시간 측정 종료
        post_timing.work_ms += now_ms() - work_start;

        // =========================================================
        // 5. 버퍼 반납 + 인덱스 증가
        // =========================================================
        atomic_store_explicit(&a->pipe->pulse_compress_map[idx].done_count, 0, memory_order_release);
        atomic_store_explicit(&a->pipe->pulse_compress_map[idx].state, BUF_FREE, memory_order_release);
      
        // =========================================================
        // 6. 결과 출력 + 누적
        // =========================================================
        snprintf(a->history[fi].filename, 256, "%s", a->pipe->filenames[idx]);
       
        print_file_result(a->timing, a->det, a->clusters, fi);

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
        
        // =========================================================
        // 7. 다음 파일 위한 리셋
        // =========================================================
        memset(a->timing, 0, sizeof(*a->timing));

        a->det->items = NULL;
        a->det->count = 0;
    }

    return NULL;
}