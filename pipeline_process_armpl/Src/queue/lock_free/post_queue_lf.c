/* Src/queue/lock_free/post_queue_lf.c */
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "post_queue_lf.h"

typedef struct {
    PostJob    *buf;
    int         cap;
    char pad0[64 - sizeof(PostJob*) - sizeof(int)];
    atomic_int  head;
    char pad1[64 - sizeof(atomic_int)];
    atomic_int  tail;
    char pad2[64 - sizeof(atomic_int)];
    atomic_int  closed;
    char pad3[64 - sizeof(atomic_int)];
} __attribute__((aligned(64))) PostQueue_Lf;

int post_queue_init_lf(void **out, int cap) {
    PostQueue_Lf *q = aligned_alloc(64, sizeof(PostQueue_Lf));
    if (!q) return -1;
    memset(q, 0, sizeof(*q));
    q->cap = cap + 1;
    q->buf = calloc((size_t)q->cap, sizeof(PostJob));
    if (!q->buf) { free(q); return -1; }
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    atomic_init(&q->closed, 0);
    *out = q;
    return 0;
}

int post_queue_push_lf(void *impl, PostJob job) {
    PostQueue_Lf *q = (PostQueue_Lf *)impl;
    int tail      = atomic_load_explicit(&q->tail, memory_order_relaxed);
    int next_tail = (tail + 1) % q->cap;

    while (next_tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
        if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;
    }
    if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;

    q->buf[tail] = job;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return 0;
}

int post_queue_pop_lf(void *impl, PostJob *job) {
    PostQueue_Lf *q = (PostQueue_Lf *)impl;

    while (1) {
        int head = atomic_load_explicit(&q->head, memory_order_relaxed);
        int tail = atomic_load_explicit(&q->tail, memory_order_acquire);

        if (head != tail) {
            *job = q->buf[head];
            atomic_store_explicit(&q->head, (head + 1) % q->cap, memory_order_release);
            return 0;
        }
        if (atomic_load_explicit(&q->closed, memory_order_acquire)) return 1;
    }
}

void post_queue_close_lf(void *impl) {
    PostQueue_Lf *q = (PostQueue_Lf *)impl;
    atomic_store_explicit(&q->closed, 1, memory_order_release);
}

void post_queue_open_lf(void *impl) {
    PostQueue_Lf *q = (PostQueue_Lf *)impl;
    atomic_store_explicit(&q->closed, 0, memory_order_release);
}

void post_queue_destroy_lf(void *impl) {
    PostQueue_Lf *q = (PostQueue_Lf *)impl;
    if (!q) return;
    free(q->buf);
    free(q);
}