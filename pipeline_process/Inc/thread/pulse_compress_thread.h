#ifndef __PULSE_COMPRESS_THREAD_H__
#define __PULSE_COMPRESS_THREAD_H__

// #include "loader.h"
// #include "common.h"

#include "queue_pulse.h"
#include "pipeline_set.h"
#include "pulse.h"

typedef struct {
    const RadarMeta *meta;
    int cpu_id;
    double compress_ms; 

    PipelinePool *pool;
    PulseQueue *q;
    
    PulseCompressCtx ctx;
} WorkerArgs;

void *worker_thread_main(void *arg);

#endif