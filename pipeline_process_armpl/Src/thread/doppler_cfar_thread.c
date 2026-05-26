#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    while (1) 
    {
        if (queue_pop_post(a->pipe->post_q, &job) != 0) break;
        if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) break;

        int idx = job.buffer_idx;
        
        a->timing->compress_core1_ms = a->pipe->compress_times[idx][0];
        a->timing->compress_core2_ms = a->pipe->compress_times[idx][1];
        //printf(" a->pipe->compress_times[idx][0] :%f,  a->pipe->compress_times[idx][1]: %f\n",  a->pipe->compress_times[idx][0],  a->pipe->compress_times[idx][1]);
        a->pipe->compress_times[idx][0] = 0.0;
        a->pipe->compress_times[idx][1] = 0.0;

        if (a->timing->compress_core1_ms > a->timing->compress_core2_ms) 
        {
            a->timing->compress_ms = a->timing->compress_core1_ms;
        } 
        else 
        {
            a->timing->compress_ms = a->timing->compress_core2_ms;
        }

        atomic_store_explicit(&a->pipe->pulse_compress_map[idx].state, BUF_PROCESSING, memory_order_release);
        double execute_time = 0.0;
        ComplexMatrix *fft_input;

        if (a->layout_type == LAYOUT_DEFAULT) {
            if (transpose_exec(a->transpose,
                               &a->pipe->pulse_compress_map[idx].data,
                               &a->pipe->doppler_map[idx].data,
                               a->meta, &execute_time) != 0) {
                fprintf(stderr, "[Post] transpose 실패: idx=%d\n", idx);
                a->status = -1;
                atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
                break;
            }
            a->timing->transpose_ms = execute_time;
            fft_input = &a->pipe->doppler_map[idx].data;
        } 
        else {
            /* legacy: transpose 없이 pulse_compress_map 바로 사용 */
            a->timing->transpose_ms = 0.0;
            fft_input = &a->pipe->pulse_compress_map[idx].data;
        }

        int doppler_ret = doppler_fft_processing(fft_input, a->meta->num_pulses, a->timing, a->doppler_ws);
        if (doppler_ret != 0) 
        {
            fprintf(stderr, "post: doppler_fft_processing failed: ret=%d buffer_idx=%d\n", doppler_ret, idx);
            a->status = -2;
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }

        execute_time = 0.0;
        int cfar_ret = cfar_detect(fft_input, a->meta, a->cfar_ws, a->det, &execute_time);
        if (cfar_ret != 0) 
        {
            fprintf(stderr, "post: cfar_detect failed: ret=%d buffer_idx=%d\n", cfar_ret, idx);
            a->status = -3;
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }
        a->timing->cfar_ms = execute_time;

        execute_time = 0.0;
        cluster_detections(a->cfar_ws->det_mask, a->cfar_ws->powerMap,
                        a->cluster_params, a->cluster_ws,
                        a->clusters, &execute_time);
        a->timing->cluster_ms = execute_time;

        udp_loop(a->pipe->dwell_ids[idx], (uint32_t)a->clusters->count, a->clusters->items, a->pipe->phi[idx], a->timing);
        send_graph_data(a->pipe->dwell_ids[idx],
            a->meta->num_fast_time_samples, a->meta->num_pulses,
            a->pipe->raw_data[idx], a->pipe->pulse_compress_map[idx].data.data,
            a->cfar_ws->powerMap, a->cfar_ws->threshold_map, a->cfar_ws->det_mask);

        atomic_store_explicit(&a->pipe->pulse_compress_map[idx].done_count, 0, memory_order_release);
        atomic_store_explicit(&a->pipe->pulse_compress_map[idx].state, BUF_FREE, memory_order_release);
         
      // cluster history 저장
        int fi = *a->valid_files;
                
        // if (a->cluster_history && a->clusters->count > 0) {
        //     printf("a->clusters->count: %d\n", a->clusters->count);
        //     a->cluster_history[fi].count = a->clusters->count;
        //     a->cluster_history[fi].items = malloc(a->clusters->count * sizeof(ClusterResult));
        //     if (a->cluster_history[fi].items) {
        //         memcpy(a->cluster_history[fi].items, a->clusters->items,
        //             a->clusters->count * sizeof(ClusterResult));
        //     }
        // }
        Detection best = {0};
        best.range_bin = -1;
        accumulate_result(a->total_acc, a->timing, a->det, &best);

        // if (a->history && fi >= 0) {
        //     a->history[fi].count = (best.range_bin != -1) ? 1 : 0;
        //     if (a->history[fi].count > 0) {
        //         a->history[fi].items = malloc(sizeof(Detection));
        //         if (a->history[fi].items)
        //             a->history[fi].items[0] = best;
        //     }
        // }

        (*a->valid_files)++;
        memset(a->timing, 0, sizeof(*a->timing));

        a->det->items = NULL;
        a->det->count = 0;
    }

    return NULL;
}