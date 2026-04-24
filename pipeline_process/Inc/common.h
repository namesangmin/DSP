#ifndef PIPELINE_TYPES_H
#define PIPELINE_TYPES_H

#include <pthread.h>
#include <stdatomic.h>

#include "loader.h"
#include "loader_mmap.h"
#include "pulse_mmap.h"
#include "doppler_fft.h"
#include "cfar.h"

// =========================================================
// [수정됨] C언어의 위에서 아래로 읽는 규칙에 따라, 
// LoaderArgs 등에서 사용하기 전에 청크 구조체를 먼저 선언합니다.
// =========================================================

// 1. 청크 단위의 작업 명세서 (기존 PulseJob 대체)
typedef struct {
    int start_idx;          /* 이 청크의 시작 펄스 인덱스 */
    int count;              /* 이 청크에 포함된 펄스 개수 */
    const RawIQSample *raw; /* 청크의 첫 번째 펄스 시작 주소 (mmap) */
} PulseChunkJob;

// 2. 청크 전용 큐 (기존 PulseQueue 대체)
typedef struct {
    PulseChunkJob *buf;     /* 버퍼 타입이 ChunkJob으로 변경됨 */
    int cap;
    int head;
    int tail;
    int count;
    int closed;

    pthread_mutex_t mtx;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} PulseChunkQueue;

// =========================================================

// 예전 단일 펄스용 구조체 (참고용 유지)
typedef struct {
    int pulse_idx;
    const RawIQSample *raw;   /* mmap된 파일 안의 pulse 시작 주소 */
} PulseJob;

typedef struct {
    PulseJob *buf;
    int cap;
    int head;
    int tail;
    int count;
    int closed;

    pthread_mutex_t mtx;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} PulseQueue;

typedef struct {
    DatMmapLoader mm;
    ComplexMatrix pc;
    atomic_int done_count;

    pthread_mutex_t post_mtx;
    pthread_cond_t post_cv;
    int post_ready;
    int error;
} PipelineFile;

typedef struct {
    const RadarMeta *meta;
    PipelineFile *file;
    PulseQueue *even_q;
    PulseQueue *odd_q;
    // PulseChunkQueue *even_q;  /* <--- 타입 변경됨 */
    // PulseChunkQueue *odd_q;   /* <--- 타입 변경됨 */
    int cpu_id;
    // [추가] 로더의 순수 작업 시간을 저장할 결과 포인터
    double *out_loader_ms; 
    double load_time_sum;
} LoaderArgs;

typedef struct {
    const RadarMeta *meta;
    int total_pulses;
    PipelineFile *file;
    PulseQueue *q;
    PulseQueue *even_q;
    PulseQueue *odd_q;
    // PulseChunkQueue *q;       /* <--- 타입 변경됨 */
    // PulseChunkQueue *even_q;  /* <--- 타입 변경됨 */
    // PulseChunkQueue *odd_q;   /* <--- 타입 변경됨 */
    int cpu_id;
    PulseCompressCtx ctx;
    
    double compress_ms;  // ← 추가
} WorkerArgs;

typedef struct {
    const RadarMeta *meta;
    PipelineFile *file;
    ComplexMatrix *doppler;
    DetectionList *det;
    DopplerFftTiming *doppler_timing;
    double *cfar_ms;
    int cpu_id;
    int status;    
} PostArgs;

typedef struct {
    char filename[256];
    int detected;
    Detection det;
} TrackPoint;

// 1. main 파일 상단에 누적용 구조체 선언
typedef struct {
    double load_ms;
    double pulse_ready_ms;
    double pulse_apply_ms;
    double pulse_total_ms;
    double mti_ms;
    double mtd_ms;
    double doppler_total_ms;
    double cfar_ms;
    double total_time_ms;
    double algo_only_ms;
    int detections;
} Accumulator;

#endif