#ifndef __QUEUE_PULSE_H__
#define __QUEUE_PULSE_H__

#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if 1
typedef struct {
    int pulse_idx;
} PulseJob;

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