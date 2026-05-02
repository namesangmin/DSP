#ifndef __DOPPLER_CFAR_THREAD_H__
#define __DOPPLER_CFAR_THREAD_H__

// doppler_cfar_thread.h
#include "pipeline_set.h"  // RdMapBuffer, DopplerBuffer, Pipeline, 큐 전부 여기서
#include "types.h"
#include "doppler_fft.h"
#include "cfar.h"
#include "common.h"
// ← cfar.h 중복 제거, queue_*.h 제거, pulse_compress_thread.h 제거, pulse.h 제거
// ← ComplexMatrix *doppler 제거 (pipe->doppler_maps[idx].data로 접근)


typedef struct {
    const RadarMeta *meta;
    Pipeline *pipe;    
    DopplerWorkspace *doppler_ws;
    CfarWorkspace * cfar_ws;
    DetectionList *det;
    PipelineTiming *timing;  // cfar_ms, transpose_ms 대신

    int cpu_id;
    int status;    
} PostArgs;

void *post_thread_main(void *arg);

#endif