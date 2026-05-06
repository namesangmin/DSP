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
    
    struct dirent **file_list;   // 추가
    const char *dir_path;
    int num_files;      // 추가
    int valid_files;
} LoaderArgs;


int loader_thread_init(const RadarMeta *meta, LoaderArgs *ld, Pipeline *pool);
int loader_thread_destroy(LoaderArgs *ld);
void *loader_thread_main(void *arg);

#endif