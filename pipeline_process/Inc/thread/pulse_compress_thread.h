#ifndef __PULSE_COMPRESS_THREAD_H__
#define __PULSE_COMPRESS_THREAD_H__

#include <complex.h>
#include "loader.h"
#include "common.h"
#include "queue.h"

typedef struct {
    int pulse_idx;
} PulseJob;

typedef struct {
    const RadarMeta *meta;
    int total_pulses;
    PipelinePool *pool;
    PulseQueue *q;
    PulseQueue *even_q;
    PulseQueue *odd_q;
    int cpu_id;
    PulseCompressCtx ctx;
    
    double compress_ms; 
} WorkerArgs;

void *worker_thread_main(void *arg);

#endif