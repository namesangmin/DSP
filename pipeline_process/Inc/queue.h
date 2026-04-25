#ifndef QUEUE_H
#define QUEUE_H

#include <stdatomic.h>

#include "common.h"
#include "pipeline_set.h"
#include "pulse_compress_thread.h"
#include "pulse.h"

#if 0
typedef struct {
    PulseJob *buf;
    int cap;
    int head;
    int tail;
    int count;
    int closed;
} PulseQueue;

int pulse_queue_init(PulseQueue *q, int cap);
void pulse_queue_destroy(PulseQueue *q);
int pulse_queue_push(PulseQueue *q, PulseJob job);
int pulse_queue_pop(PulseQueue *q, PulseJob *job);
void pulse_queue_close(PulseQueue *q);
#endif

#if 1
typedef struct {
    PulseJob *buf;
    int cap;
    atomic_int head;   // 빼는 쪽 (Consumer)
    atomic_int tail;   // 넣는 쪽 (Producer)
    atomic_int closed; // 종료 플래그
} PulseQueue;

int pulse_queue_init(PulseQueue *q, int cap);
void pulse_queue_destroy(PulseQueue *q);
int pulse_queue_push(PulseQueue *q, PulseJob job);
int pulse_queue_pop(PulseQueue *q, PulseJob *job);
void pulse_queue_close(PulseQueue *q);
#endif

#endif