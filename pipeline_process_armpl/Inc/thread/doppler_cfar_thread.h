#ifndef __DOPPLER_CFAR_THREAD_H__
#define __DOPPLER_CFAR_THREAD_H__

#include "pipeline_set.h"  // RdMapBuffer, DopplerBuffer, Pipeline, 큐 전부 여기서
#include "types.h"
#include "doppler_fft.h"
#include "cfar.h"
#include "cluster.h"
#include "transpose_interface.h"
#include "common.h"

typedef struct {
    const RadarMeta *meta;
    Pipeline *pipe;    

    DopplerWorkspace *doppler_ws;
    CfarWorkspace * cfar_ws;
    
    DetectionList *det;
    PipelineTiming *timing;  // cfar_ms, transpose_ms 대신

    ClusterWorkspace *cluster_ws;   // 추가
    ClusterParams    *cluster_params; // 추가
    ClusterList      *clusters;     // 추가
    
    Transpose *transpose;

    int cpu_id;
    int status;   
    
    Accumulator      *total_acc;   // 추가
    int              *valid_files; // 추가
} PostArgs;

void *post_thread_main(void *arg);

#endif