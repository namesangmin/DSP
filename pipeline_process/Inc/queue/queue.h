#if 0
#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

//#include "common.h"
//#include "pipeline_set.h"
//#include "pulse.h"

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

#endif