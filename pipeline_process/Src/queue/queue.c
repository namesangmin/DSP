
#if 0
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
    while (!q->closed && q->count == q->cap) { }
    if (q->closed) return -1;

    q->buf[q->tail] = job;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;

    return 0;
}

int pulse_queue_pop(PulseQueue *q, PulseJob *job)
{
    while (!q->closed && q->count == 0) { }
    if (q->count == 0 && q->closed) return 0;

    *job = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;

    return 1;
}

void pulse_queue_close(PulseQueue *q)
{
    q->closed = 1;
}
#endif