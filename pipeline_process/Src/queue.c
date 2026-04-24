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
    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    return 0;
}

void pulse_queue_destroy(PulseQueue *q)
{
    if (!q) return;

    free(q->buf);
    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    memset(q, 0, sizeof(*q));
}

int pulse_queue_push(PulseQueue *q, PulseJob job)
{
    pthread_mutex_lock(&q->mtx);

    while (!q->closed && q->count == q->cap) {
        pthread_cond_wait(&q->not_full, &q->mtx);
    }

    if (q->closed) {
        pthread_mutex_unlock(&q->mtx);
        return -1;
    }

    q->buf[q->tail] = job;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mtx);
    return 0;
}

int pulse_queue_pop(PulseQueue *q, PulseJob *job)
{
    pthread_mutex_lock(&q->mtx);

    while (!q->closed && q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mtx);
    }

    if (q->count == 0 && q->closed) {
        pthread_mutex_unlock(&q->mtx);
        return 0;
    }

    *job = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mtx);
    return 1;
}

void pulse_queue_close(PulseQueue *q)
{
    pthread_mutex_lock(&q->mtx);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mtx);
}
