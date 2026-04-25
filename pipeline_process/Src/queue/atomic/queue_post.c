#include "queue_post.h"

int post_queue_init(PostQueue *q, int cap) {
    q->buf = (PostJob *)calloc(cap, sizeof(PostJob));
    if (!q->buf) return -1;
    
    q->cap = cap;
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    atomic_init(&q->closed, 0);
    
    return 0; // 성공
}

void post_queue_destroy(PostQueue *q) {
    if (q->buf) {
        free(q->buf);
        q->buf = NULL;
    }
}

void post_queue_close(PostQueue *q) {
    atomic_store(&q->closed, 1);
}

// 1 성공, 0 실패
int post_queue_push(PostQueue *q, PostJob job) {
    int current_tail = atomic_load(&q->tail);
    int next_tail = (current_tail + 1) % q->cap;

    // 큐가 꽉 찼는지 확인
    if (next_tail == atomic_load(&q->head)) {
        return 0; 
    }

    q->buf[current_tail] = job;
    atomic_store(&q->tail, next_tail); // 꼬리 이동

    return 1;
}

// 1 성공, 0 실패
int post_queue_pop(PostQueue *q, PostJob *job) {
    int current_head = atomic_load(&q->head);

    // 큐가 비어있는지 확인
    if (current_head == atomic_load(&q->tail)) {
        if (atomic_load(&q->closed)) return 0; // 문 닫힘
        return 0; // 비어있음
    }

    *job = q->buf[current_head];
    atomic_store(&q->head, (current_head + 1) % q->cap); // 머리 이동

    return 1;
}