#include "queue_pulse.h"
// #include <unistd.h> <-- usleep을 쓸 거면 이게 필요하지만, 우린 안 쓸 겁니다.
#include <sched.h>     // usleep 대신 CPU를 똑똑하게 양보할 때 씀

int pulse_queue_init(PulseQueue *q, int cap) {
    memset(q, 0, sizeof(*q));
    // 락-프리 큐는 꽉 찬 상태와 빈 상태를 구분하기 위해 1칸을 비워둡니다.
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
        
        // usleep(1) 삭제! 대신 아래처럼 깡으로 돌거나 yield를 씁니다.
        // 아무것도 안 적으면 CPU 100% 점유하며 최고 속도 반응
        // sched_yield(); // 만약 다른 스레드가 급하면 양보 (선택 사항)
    }

    if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;

    q->buf[tail] = job;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return 0;
}

int pulse_queue_pop(PulseQueue *q, PulseJob *job) {
    int head = atomic_load_explicit(&q->head, memory_order_relaxed);

    // 비어있으면 데이터가 들어올 때까지 대기 (Pure Spin-wait)
    while (head == atomic_load_explicit(&q->tail, memory_order_acquire)) {
        if (atomic_load_explicit(&q->closed, memory_order_acquire)) {
            if (head == atomic_load_explicit(&q->tail, memory_order_acquire)) return 0;
            break; 
        }
    }

    *job = q->buf[head];
    atomic_store_explicit(&q->head, (head + 1) % q->cap, memory_order_release);
    return 1;
}

void pulse_queue_close(PulseQueue *q) {
    atomic_store_explicit(&q->closed, 1, memory_order_release);
}