#ifndef __PIPELINE_SET_H__
#define __PIPELINE_SET_H__

#include <pthread.h>
#include <stdatomic.h>
#include "common.h"
#include "loader.h"

#define NUM_BUFFERS 3

// 1. 버퍼 상태 플래그 정의
typedef enum {
    BUF_FREE = 0,       // 비어 있음
    BUF_FILLING = 1,    // 짝/홀 코어가 열심히 쓰는 중
    BUF_READY = 2,      // 압축 완료! (도플러로 넘길 준비)
    BUF_PROCESSING = 3  // 도플러/CFAR 코어가 처리 중
} BufferState;

// 2. 개별 버퍼 구조체
typedef struct {
    ComplexMatrix data;
    atomic_int state;       
    atomic_int done_count;  // "이 버퍼"에 펄스가 다 찼는지 확인 (이게 핵심!)
} RdMapBuffer;

typedef struct {
    RealMatrix data;
    atomic_int state;
} DetectionBuffer;

// 3. 메인 파이프라인 관리자
typedef struct {
    ComplexMatrix raw_data; // 파일에서 딱 한 번 읽을 원본 (1개)

    RdMapBuffer rd_maps[NUM_BUFFERS];      // 3중 버퍼
    DetectionBuffer det_maps[NUM_BUFFERS]; // 3중 버퍼

    atomic_int current_write_idx; // 짝/홀 코어가 현재 채우고 있는 인덱스 (0, 1, 2)
    atomic_int error;             // 에러 플래그 (0 정상, 1 에러)
    
    // atomic_int total_done_count; <-- 과거의 쓰레기, 삭제됨!
} PipelinePool; 

int init_pipeline_pool(const char *dat_path, const RadarMeta *meta, PipelinePool *pool);
void cleanup_pipeline_pool(PipelinePool *pool);

#endif