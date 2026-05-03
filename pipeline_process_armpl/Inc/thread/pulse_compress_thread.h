#ifndef __PULSE_COMPRESS_THREAD_H__
#define __PULSE_COMPRESS_THREAD_H__

// pulse_compress_thread.h
#include "pipeline_set.h"
#include "types.h"
#include "pulse.h"
#include "common.h"
// ← queue_*.h 제거, common.h는 types.h로 대체

typedef struct {
    const RadarMeta *meta;
    Pipeline *pipe;
    PulseCompressCtx ctx;
    PulseQueue* q;
    PipelineTiming *timing;  // cfar_ms, transpose_ms 대신

    int cpu_id;
    double compress_ms; 
} WorkerArgs;

void *worker_thread_main(void *arg);

#endif