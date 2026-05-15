#include "queue_pulse.h"
#include <unistd.h>
#include <sys/syscall.h>    
#include <linux/futex.h>

// 임시 디버그용
// atomic_int pop_sleep_count = 0;
// atomic_int push_sleep_count = 0;
int pulse_queue_init(PulseQueue *q, int cap) 
{
    memset(q, 0, sizeof(*q));
    q->cap = cap + 1; 
    q->buf = (PulseJob *)calloc((size_t)q->cap, sizeof(PulseJob));
    if (!q->buf) return -1;

    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    atomic_init(&q->closed, 0);
    return 0;
}

void pulse_queue_destroy(PulseQueue *q) 
{
    if (!q) return;
    free(q->buf);
    memset(q, 0, sizeof(*q));
}

// int pulse_queue_push(PulseQueue *q, PulseJob job) 
// {
//     int tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
//     int next_tail = (tail + 1) % q->cap;

//     // 꽉 찼으면 빈 자리가 날 때까지 대기 (Pure Spin-wait)
//     while (next_tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
//         if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;
        
//         atomic_fetch_add(&push_sleep_count, 1);
//         usleep(100);
//     }

//     if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -2;

//     q->buf[tail] = job;
//     atomic_store_explicit(&q->tail, next_tail, memory_order_release);
//     return 0; 
// }

// int pulse_queue_pop(PulseQueue *q, PulseJob *job)
// {
//     if (!q || !job) 
//     {
//         return 1;
//     }

//     int head = atomic_load_explicit(&q->head, memory_order_relaxed);
//     while (1) 
//     {
//         int tail = atomic_load_explicit(&q->tail, memory_order_acquire);

//         if (head != tail) 
//         {
//             *job = q->buf[head];
//             atomic_store_explicit(&q->head, (head + 1) % q->cap, memory_order_release);

//             return 0;
//         }

//         if (atomic_load_explicit(&q->closed, memory_order_acquire)) 
//         {
//             return 1;
//         }

//         atomic_fetch_add_explicit(&pop_sleep_count, 1, memory_order_relaxed);
//         usleep(5000);
//     }
// }

int pulse_queue_push(PulseQueue *q, PulseJob job)
{
    int tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    int next_tail = (tail + 1) % q->cap;

    while (next_tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
        if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;
        //atomic_fetch_add_explicit(&push_sleep_count, 1, memory_order_relaxed);
        usleep(100);  // push 풀은 그대로 (꽉 찬 경우라 드묾)
    }

    if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;

    q->buf[tail] = job;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    //syscall(SYS_futex, (int *)&q->tail, FUTEX_WAKE, 1, NULL, NULL, 0);  // consumer 깨움
    return 0;
}

int pulse_queue_pop(PulseQueue *q, PulseJob *job)
{
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

        //atomic_fetch_add_explicit(&pop_sleep_count, 1, memory_order_relaxed);
        syscall(SYS_futex, (int *)&q->tail, FUTEX_WAIT, tail, NULL, NULL, 0);  // tail 바뀔 때까지 sleep
        // EAGAIN(이미 바뀜), EINTR(시그널) 둘 다 그냥 루프 재시도
    }
}

void pulse_queue_close(PulseQueue *q) 
{
    atomic_store_explicit(&q->closed, 1, memory_order_release);
}

void pulse_queue_open(PulseQueue * q)
{
    atomic_store_explicit(&q->closed, 0, memory_order_release);
}