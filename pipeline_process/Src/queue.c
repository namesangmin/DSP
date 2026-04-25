#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

int pulse_queue_init(PulseQueue *q, int cap)
{
    memset(q, 0, sizeof(*q));

    q->buf = (PulseJob *)calloc((size_t)cap, sizeof(PulseJob));
    if (!q->buf) return -1;

    q->cap = cap;
    return 0;
}

void pulse_queue_destroy(PulseQueue *q)
{
    if (!q) return;

    free(q->buf);
    memset(q, 0, sizeof(*q));
}

int pulse_queue_push(PulseQueue *q, PulseJob job)
{

    while (!q->closed && q->count == q->cap) {
        //pthread_cond_wait(&q->not_full, &q->mtx);
    }

    if (q->closed) {
        //pthread_mutex_unlock(&q->mtx);
        return -1;
    }

    q->buf[q->tail] = job;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;

    return 0;
}

int pulse_queue_pop(PulseQueue *q, PulseJob *job)
{

    while (!q->closed && q->count == 0) {
        //pthread_cond_wait(&q->not_empty, &q->mtx);
    }

    if (q->count == 0 && q->closed) {
        //pthread_mutex_unlock(&q->mtx);
        return 0;
    }

    *job = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;

    return 1;
}