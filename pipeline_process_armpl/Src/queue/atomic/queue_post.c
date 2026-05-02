#include <unistd.h>
#include "queue_post.h"
int post_queue_init(PostQueue *q, int cap) {
    memset(q, 0, sizeof(*q));

    q->cap = cap + 1; 
    q->buf = (PostJob *)calloc((size_t)q->cap, sizeof(PostJob));
    if (!q->buf) return -1;

    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    atomic_init(&q->closed, 0);
    return 0;
}

void post_queue_destroy(PostQueue *q) {
    if (!q) return;
    free(q->buf);
    memset(q, 0, sizeof(*q));
}

int post_queue_push(PostQueue *q, PostJob job) {
    int tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    int next_tail = (tail + 1) % q->cap;

    // 꽉 찼으면 빈 자리가 날 때까지 대기 (Pure Spin-wait)
    while (next_tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
        if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;
        usleep(3);
    }

    if (atomic_load_explicit(&q->closed, memory_order_acquire)) return -1;

    q->buf[tail] = job;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return  0;
}

int post_queue_pop(PostQueue *q, PostJob *job) {
    int head = atomic_load_explicit(&q->head, memory_order_relaxed);

    while (head == atomic_load_explicit(&q->tail, memory_order_acquire)) {
        if (atomic_load_explicit(&q->closed, memory_order_acquire)) {
            if (head == atomic_load_explicit(&q->tail, memory_order_acquire)) return 0;
            break;
        }

        usleep(3);
    }

    *job = q->buf[head];
    atomic_store_explicit(&q->head, (head + 1) % q->cap, memory_order_release);
    return 1;
}

// queue_post.c에 추가
void post_queue_close(PostQueue *q) {
    // atomic_store_explicit(&q->head,   1, memory_order_relaxed);
    // atomic_store_explicit(&q->tail,   1, memory_order_relaxed);
    atomic_store_explicit(&q->closed, 1, memory_order_release);
}
void post_queue_open(PostQueue *q) {
    atomic_store_explicit(&q->closed, 0, memory_order_release);
}
