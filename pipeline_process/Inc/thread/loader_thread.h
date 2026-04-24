#ifndef __LOADER_THREAD_H__
#define __LOADER_THREAD_H__

#include "common.h"
#include "core_set.h"

#include "pipeline_set.h"

#include "queue.h"
#include "pulse_compress_thread.h"

typedef struct {
    const RadarMeta *meta;
    PipelineFile *file;
    PulseQueue *even_q;
    PulseQueue *odd_q;
    int cpu_id;
} LoaderArgs;

void *loader_thread_main(void *arg);

#endif