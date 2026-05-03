#ifndef __LOADER_THREAD_H__
#define __LOADER_THREAD_H__

// loader_thread.h
#include "pipeline_set.h"
#include "types.h"
#include "common.h"
// ← core_set.h, queue_pulse.h 제거 (pipeline_set.h로 들어옴)
// loader_thread_main 반환타입 void*로 수정
typedef struct {
    const RadarMeta *meta;
    Pipeline *pipe;  
    PipelineTiming *timing;  // cfar_ms, transpose_ms 대신
    
    double *buffer;
    int cpu_id;
    const char *dat_path;
} LoaderArgs;


int loader_thread_init(const char *dat_path, const RadarMeta *meta, LoaderArgs *ld, Pipeline *pool);
int loader_thread_destroy(LoaderArgs *ld);
void *loader_thread_main(void *arg);

#endif