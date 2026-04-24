#ifndef __PIPELINE_SET_H__
#define __PIPELINE_SET_H__

#include <pthread.h>
#include <stdatomic.h>
#include "common.h"
#include "loader.h"
#include "loader_fread.h"

// 1. 버퍼 상태 플래그 정의
typedef enum {
    BUF_FREE = 0,       // 비어 있음 (압축 코어가 쓸 수 있음)
    BUF_FILLING = 1,    // 짝/홀 코어가 열심히 쓰는 중
    BUF_READY = 2,      // 압축 완료! (도플러가 읽어갈 수 있음)
    BUF_PROCESSING = 3  // 도플러/CFAR 코어가 읽고 연산 중
} BufferState;

#define NUM_BUFFERS 3

// 2. 개별 버퍼 구조체 (배열 + 상태 + 완료카운트)
typedef struct {
    ComplexMatrix data;
    atomic_int state;       // 현재 버퍼의 상태 (BUF_FREE 등)
    atomic_int done_count;  // 1001개 펄스가 다 찼는지 확인용
} RdMapBuffer;

typedef struct {
    RealMatrix data;
    atomic_int state;
} DetectionBuffer;

// 3. 메인 파이프라인 관리자 (기존 PipelineFile 완벽 대체)
typedef struct {
    ComplexMatrix raw_data; // 파일에서 딱 한 번 읽을 원본 (1개)

    RdMapBuffer rd_maps[NUM_BUFFERS];      // 3중 버퍼
    DetectionBuffer det_maps[NUM_BUFFERS]; // 3중 버퍼

    atomic_int current_write_idx; // 짝/홀 코어가 현재 채우고 있는 인덱스 (0, 1, 2)
    
    int error;
} PipelinePool; // 이름도 멋지게 Pool로 변경!

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
