/* Src/queue/mutex/post_queue_mu.c */
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "post_queue_mu.h"

typedef struct {
    PostJob         *buf;
    int              cap, head, tail, count, closed;
    pthread_mutex_t  mtx;
    pthread_cond_t   not_empty, not_full;
} PostQueue_Mu;

int post_queue_init_mu(void **out, int cap) {
    PostQueue_Mu *q = malloc(sizeof(PostQueue_Mu));
    if (!q) return -1;
    memset(q, 0, sizeof(*q));
    q->buf = calloc((size_t)cap, sizeof(PostJob));
    if (!q->buf) { free(q); return -1; }
    q->cap = cap;
    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    *out = q;
    return 0;
}

int post_queue_push_mu(void *impl, PostJob job) {
    PostQueue_Mu *q = (PostQueue_Mu *)impl;
    pthread_mutex_lock(&q->mtx);
    while (!q->closed && q->count == q->cap)
        pthread_cond_wait(&q->not_full, &q->mtx);
    if (q->closed) { pthread_mutex_unlock(&q->mtx); return -1; }
    q->buf[q->tail] = job;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mtx);
    return 0;
}

int post_queue_pop_mu(void *impl, PostJob *job) {
    PostQueue_Mu *q = (PostQueue_Mu *)impl;
    pthread_mutex_lock(&q->mtx);
    while (!q->closed && q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->mtx);
    if (q->count == 0 && q->closed) { pthread_mutex_unlock(&q->mtx); return 1; }
    *job = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mtx);
    return 0;
}

void post_queue_close_mu(void *impl) {
    PostQueue_Mu *q = (PostQueue_Mu *)impl;
    pthread_mutex_lock(&q->mtx);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mtx);
}

void post_queue_open_mu(void *impl) {
    PostQueue_Mu *q = (PostQueue_Mu *)impl;
    pthread_mutex_lock(&q->mtx);
    q->closed = 0;
    pthread_mutex_unlock(&q->mtx);
}

void post_queue_destroy_mu(void *impl) {
    PostQueue_Mu *q = (PostQueue_Mu *)impl;
    if (!q) return;
    free(q->buf);
    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    free(q);
}