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
        int transpose_ret = transpose_exec(a->transpose,
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
        if (doppler_ret != 0) 
        {
            fprintf(stderr, "post: doppler_fft_processing failed: ret=%d buffer_idx=%d\n", doppler_ret, idx);
            a->status = -2;
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            break;
        }

        execute_time = 0.0;
        int cfar_ret = cfar_detect(&a->pipe->doppler_map[idx].data, a->meta, a->cfar_ws, a->det, &execute_time);
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
         
        accumulate_result(a->total_acc, a->timing, a->det);   
        (*a->valid_files)++;
        memset(a->timing, 0, sizeof(*a->timing));

        a->det->items = NULL;
        a->det->count = 0;
    }

    return NULL;
}