#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

int pulse_queue_init(PulseQueue *q, int cap);
void pulse_queue_destroy(PulseQueue *q);
int pulse_queue_push(PulseQueue *q, PulseJob job);
int pulse_queue_pop(PulseQueue *q, PulseJob *job);
void pulse_queue_close(PulseQueue *q);

int pulse_chunk_queue_init(PulseChunkQueue *q, int cap);
void pulse_chunk_queue_destroy(PulseChunkQueue *q);
int pulse_chunk_queue_push(PulseChunkQueue *q, PulseChunkJob job);
int pulse_chunk_queue_pop(PulseChunkQueue *q, PulseChunkJob *job);
void pulse_chunk_queue_close(PulseChunkQueue *q);
#endif