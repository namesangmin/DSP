#ifndef __LOADER_THREAD_H__
#define __LOADER_THREAD_H__

#include "common.h"
#include "core_set.h"
#include "pipeline_set.h" 
#include "queue.h"
#include "pulse_compress_thread.h"

typedef struct {
    const RadarMeta *meta;
    
    PipelinePool *pool;     // [수정] PipelineFile을 PipelinePool로 교체!
    
    PulseQueue *even_q;
    PulseQueue *odd_q;
    
    int cpu_id;
    
    // ================= [추가된 부분] =================
    const char *dat_path;   // 0번 코어가 한 방에 읽을 원본 데이터 파일 경로
    double *out_loader_ms;  // 파일 읽는 데 걸린 시간을 메인으로 반환할 포인터
    // ===============================================

} LoaderArgs;

void *loader_thread_main(void *arg);

#endif