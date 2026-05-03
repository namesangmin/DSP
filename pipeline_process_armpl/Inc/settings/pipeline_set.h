#ifndef __PIPELINE_SET_H__
#define __PIPELINE_SET_H__

#include <pthread.h>
#include <stdatomic.h>
#include <fftw3.h>
#include "types.h"
#include "queue_post.h"
#include "queue_pulse.h"

#define NUM_BUFFERS 3
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

    PostQueue post_q;
    PulseQueue even_q;   // pulse 0~255
    PulseQueue odd_q;    // pulse 256~511

    fftwf_complex *raw_data;
    RdMapBuffer    rd_maps[NUM_BUFFERS];
    DopplerBuffer  doppler_maps[NUM_BUFFERS];
} Pipeline;

struct RadarMeta;
int init_pipeline_pool(const char *dat_path, const RadarMeta *meta, Pipeline *pipe);
void cleanup_pipeline_pool(Pipeline *pool);

#endif