#ifndef __PIPELINE_SET_H__
#define __PIPELINE_SET_H__

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>      // uint32_t 추가
#include <fftw3.h>
#include "types.h"
#include "queue_post.h"
#include "queue_pulse.h"

#define NUM_BUFFERS 11
typedef enum {
    BUF_FREE = 0,       // 비어 있음
    BUF_FILLING = 1,    // 짝/홀 코어가 열심히 쓰는 중
    BUF_READY = 2,      // 압축 완료! (도플러로 넘길 준비)
    BUF_PROCESSING = 3  // 도플러/CFAR 코어가 처리 중
} BufferState;

typedef struct {
    ComplexMatrix data;
    atomic_int    state;
    atomic_int    done_count; // worker들이 이 버퍼에 다 썼는지 카운팅
} RdMapBuffer;

typedef struct {
    ComplexMatrix data;
    atomic_int    state;
} DopplerBuffer;

typedef struct {
    atomic_int    current_write_idx;
    atomic_int    error;
    atomic_int active_workers; // 워커 스레드 종료 체크용
    double compress_times[NUM_BUFFERS][2];
    
    PostQueue post_q;
    PulseQueue even_q;   // pulse 0~255
    PulseQueue odd_q;    // pulse 256~511

    float complex *raw_data[NUM_BUFFERS];  // 단일 → 배열
    RdMapBuffer    rd_maps[NUM_BUFFERS];
    DopplerBuffer  doppler_maps[NUM_BUFFERS];
    char filenames[NUM_BUFFERS][256];

    uint32_t dwell_ids[NUM_BUFFERS];
    float    phi[NUM_BUFFERS];  
} Pipeline;

struct RadarMeta;
int init_pipeline_pool(const RadarMeta *meta, Pipeline *pipe);
void cleanup_pipeline_pool(Pipeline *pool);

#endif