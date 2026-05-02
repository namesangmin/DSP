#ifndef __QUEUE_POST_H__
#define __QUEUE_POST_H__

#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int buffer_idx;
} PostJob;

typedef struct {
    PostJob *buf;
    int cap;
    atomic_int head;   // 빼는 쪽 (Consumer - Core 3)
    atomic_int tail;   // 넣는 쪽 (Producer - Core 1, 2)
    atomic_int closed; // 종료 플래그 (int로 통일)
} PostQueue;

int post_queue_init(PostQueue *q, int cap);
void post_queue_destroy(PostQueue *q);
int post_queue_push(PostQueue *q, PostJob job);
int post_queue_pop(PostQueue *q, PostJob *job);
void post_queue_close(PostQueue *q);
void post_queue_open(PostQueue *q);

#endif