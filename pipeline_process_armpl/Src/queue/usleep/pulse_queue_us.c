/* Src/queue/usleep/pulse_queue_us.c */
#include <unistd.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "pulse_queue_us.h"

typedef struct {
    PulseJob   *buf;
    int         cap;
    char pad0[64 - sizeof(PulseJob*) - sizeof(int)];
    atomic_int  head;
    char pad1[64 - sizeof(atomic_int)];
    atomic_int  tail;
    char pad2[64 - sizeof(atomic_int)];
    atomic_int  closed;
    char pad3[64 - sizeof(atomic_int)];
} __attribute__((aligned(64))) PulseQueue_Us;

int pulse_queue_init_us(void **out, int cap) {
    PulseQueue_Us *q = aligned_alloc(64, sizeof(PulseQueue_Us));
    if (!q) return -1;
    memset(q, 0, sizeof(*q));
    q->cap = cap + 1;
    q->buf = calloc((size_t)q->cap, sizeof(PulseJob));
    if (!q->buf) { free(q); return -1; }
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    atomic_init(&q->closed, 0);
    *out = q;
    return 0;
}

int pulse_queue_push_us(void *impl, PulseJob job) {
    PulseQueue_Us *q = (PulseQueue_Us *)impl;
    int tail      = atomic_load_explicit(&q->tail, memory_order_relaxed);
    int next_tail = (tail + 1) % q->cap;

    while (next_tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
        if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;
        usleep(100);
    }
    if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;

    q->buf[tail] = job;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return 0;
}

int pulse_queue_pop_us(void *impl, PulseJob *job) {
    PulseQueue_Us *q = (PulseQueue_Us *)impl;
    if (!q || !job) return 1;

    int head = atomic_load_explicit(&q->head, memory_order_relaxed);
    while (1) {
        int tail = atomic_load_explicit(&q->tail, memory_order_acquire);
        if (head != tail) {
            *job = q->buf[head];
            atomic_store_explicit(&q->head, (head + 1) % q->cap, memory_order_release);
            return 0;
        }
        if (atomic_load_explicit(&q->closed, memory_order_acquire)) return 1;
        usleep(5000);
    }
}

void pulse_queue_close_us(void *impl) {
    PulseQueue_Us *q = (PulseQueue_Us *)impl;
    atomic_store_explicit(&q->closed, 1, memory_order_release);
}

void pulse_queue_open_us(void *impl) {
    PulseQueue_Us *q = (PulseQueue_Us *)impl;
    atomic_store_explicit(&q->closed, 0, memory_order_release);
}

void pulse_queue_destroy_us(void *impl) {
    PulseQueue_Us *q = (PulseQueue_Us *)impl;
    if (!q) return;
    free(q->buf);
    free(q);
}