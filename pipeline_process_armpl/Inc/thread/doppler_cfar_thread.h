#ifndef __DOPPLER_CFAR_THREAD_H__
#define __DOPPLER_CFAR_THREAD_H__

#include "pipeline_set.h"
#include "common.h"

#include "doppler_fft.h"
#include "cfar.h"

#include "queue_post.h"
#include "queue_pulse.h"

#include "pulse_compress_thread.h"
#include "cfar.h"
#include "pulse.h"

typedef struct {
    const RadarMeta *meta;
    PipelinePool *pool;
    ComplexMatrix *doppler;
    DetectionList *det;
    DopplerFftTiming *doppler_timing;
    
    CfarWorkspace * cfar_ws;
    DopplerWorkspace *doppler_ws;
    double *cfar_ms;
    double *transpose_ms;
    int cpu_id;
    int status;    
    
    PostQueue *post_q;
} PostArgs;

void *post_thread_main(void *arg);

#endif