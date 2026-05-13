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
    char pad0[64 - sizeof(PostJob*) - sizeof(int)];

    atomic_int head;   // 빼는 쪽 (Consumer - Core 3)
    char pad1[64 - sizeof(atomic_int)];
    
    atomic_int tail;   // 넣는 쪽 (Producer - Core 1, 2)
    char pad2[64 - sizeof(atomic_int)];

    atomic_int closed; // 종료 플래그 (int로 통일)
    char pad3[64 - sizeof(atomic_int)];
}__attribute__((aligned(64))) PostQueue;

int post_queue_init(PostQueue *q, int cap);
void post_queue_destroy(PostQueue *q);
int post_queue_push(PostQueue *q, PostJob job);
int post_queue_pop(PostQueue *q, PostJob *job);
void post_queue_close(PostQueue *q);
void post_queue_open(PostQueue *q);

#endif