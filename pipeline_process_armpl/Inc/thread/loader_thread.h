#ifndef __LOADER_THREAD_H__
#define __LOADER_THREAD_H__

#include "common.h"
#include "core_set.h"
#include "pipeline_set.h" 
#include "queue_pulse.h"
#include "pulse_compress_thread.h"

typedef struct {
    const RadarMeta *meta;
    PipelinePool *pool;   
    PulseQueue *even_q;
    PulseQueue *odd_q;
    
    int cpu_id;
    
    const char *dat_path;
    double *out_loader_ms;

} LoaderArgs;

void *loader_thread_main(void *arg);

#endif