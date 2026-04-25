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

} PulseQueue;

int pulse_queue_init(PulseQueue *q, int cap);
void pulse_queue_destroy(PulseQueue *q);
int pulse_queue_push(PulseQueue *q, PulseJob job);
int pulse_queue_pop(PulseQueue *q, PulseJob *job);
// void pulse_queue_close(PulseQueue *q);

#endif