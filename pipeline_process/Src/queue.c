#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

#if 0
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

#if 1
int pulse_queue_init(PulseQueue *q, int cap) {
    memset(q, 0, sizeof(*q));
    // 락-프리 큐는 꽉 찬 상태와 빈 상태를 구분하기 위해 1칸을 비워둡니다.
    // 따라서 요청받은 cap보다 1칸 더 크게 할당합니다.
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

    // 꽉 찼으면 빈 자리가 날 때까지 대기 (Spin-wait)
    while (next_tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
        if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;
        usleep(1); // CPU 과부하 방지
    }

    if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;

    q->buf[tail] = job;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return 0;
}

int pulse_queue_pop(PulseQueue *q, PulseJob *job) {
    int head = atomic_load_explicit(&q->head, memory_order_relaxed);

    // 비어있으면 데이터가 들어올 때까지 대기 (Spin-wait)
    while (head == atomic_load_explicit(&q->tail, memory_order_acquire)) {
        if (atomic_load_explicit(&q->closed, memory_order_acquire)) {
            // 닫혔고, 남은 데이터도 확인
            if (head == atomic_load_explicit(&q->tail, memory_order_acquire)) return 0;
            break; // 닫혔지만 뺄 데이터가 남아있으면 루프 탈출
        }
        usleep(1); // CPU 과부하 방지
    }

    *job = q->buf[head];
    atomic_store_explicit(&q->head, (head + 1) % q->cap, memory_order_release);
    return 1;
}

void pulse_queue_close(PulseQueue *q) {
    atomic_store_explicit(&q->closed, 1, memory_order_release);
}
#endif