#ifndef __QUEUE_PULSE_H__
#define __QUEUE_PULSE_H__

#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    int pulse_idx;
    int raw_idx;

} PulseJob;

typedef struct {
    PulseJob *buf;
    int cap;

    char pad0[64];

    atomic_int head;

    char pad1[64];

    atomic_int tail;

    char pad2[64];

    atomic_int closed;

    char pad3[64];
} PulseQueue;

int pulse_queue_init(PulseQueue *q, int cap);
void pulse_queue_destroy(PulseQueue *q);
int pulse_queue_push(PulseQueue *q, PulseJob job);
int pulse_queue_pop(PulseQueue *q, PulseJob *job);
void pulse_queue_close(PulseQueue *q);
void pulse_queue_open(PulseQueue * q);

#endif