#include "queue_pulse.h"

int pulse_queue_init(PulseQueue *q, int cap) {
    memset(q, 0, sizeof(*q));
    q->cap = cap + 1; 
    q->buf = (PulseJob *)calloc((size_t)q->cap, sizeof(PulseJob));
    if (!q->buf) return -1;

    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    atomic_init(&q->closed, 0);
    return 0;
}

void pulse_queue_destroy(PulseQueue *q) {
    if (!q) return;
    free(q->buf);
    memset(q, 0, sizeof(*q));
}

int pulse_queue_push(PulseQueue *q, PulseJob job) {
    int tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    int next_tail = (tail + 1) % q->cap;

    // 꽉 찼으면 빈 자리가 날 때까지 대기 (Pure Spin-wait)
    while (next_tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
        if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;
        
        usleep(1);
    }

    if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;

    q->buf[tail] = job;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return 0; 
}

int pulse_queue_pop(PulseQueue *q, PulseJob *job)
{
    if (!q || !job) {
        return 0;
    }

    while (1) {
        int head = atomic_load_explicit(&q->head, memory_order_relaxed);
        int tail = atomic_load_explicit(&q->tail, memory_order_acquire);

        if (head != tail) {
            *job = q->buf[head];

            atomic_store_explicit(&q->head,
                                  (head + 1) % q->cap,
                                  memory_order_release);

            return 1;
        }

        if (atomic_load_explicit(&q->closed, memory_order_acquire)) {
            return 0;
        }

        usleep(1);
    }
}

void pulse_queue_close(PulseQueue *q) {
    atomic_store_explicit(&q->closed, 1, memory_order_release);
}