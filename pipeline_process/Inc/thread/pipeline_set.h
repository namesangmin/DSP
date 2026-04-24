#ifndef __PIPELINE_SET_H__
#define __PIPELINE_SET_H__

#include "common.h"

typedef struct {
    ComplexMatrix raw_data; // [수정] mmap 구조체 대신, fread로 읽어온 전체 데이터를 담을 공간!
    ComplexMatrix pc;       // 펄스 압축 결과
    
    atomic_int done_count;

    pthread_mutex_t post_mtx;
    pthread_cond_t post_cv;

    int post_ready;
    int error;
} PipelineFile;

void pipeline_signal_post(PipelineFile *file, int error_flag);

#endif
