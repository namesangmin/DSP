#ifndef __LOADER_THREAD_H__
#define __LOADER_THREAD_H__

#include "pipeline_set.h"
#include "types.h"
#include "common.h"
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