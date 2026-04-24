#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"
#include "pipeline_set.h"
#include "pulse_compress_thread.h"
#include "pulse.h"

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
    const RadarMeta *meta;
    int total_pulses;
    PipelinePool *file;
    PulseQueue *q;
    PulseQueue *even_q;
    PulseQueue *odd_q;
    int cpu_id;
    PulseCompressCtx ctx;
    
    double compress_ms;  // ← 추가
} WorkerArgs;


int pulse_queue_init(PulseQueue *q, int cap);
void pulse_queue_destroy(PulseQueue *q);
int pulse_queue_push(PulseQueue *q, PulseJob job);
int pulse_queue_pop(PulseQueue *q, PulseJob *job);
void pulse_queue_close(PulseQueue *q);

#endif