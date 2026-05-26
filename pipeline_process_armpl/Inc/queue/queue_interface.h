/* Inc/queue/queue_interface.h */
#ifndef QUEUE_INTERFACE_H
#define QUEUE_INTERFACE_H

#include <stddef.h>

/* Job 타입 — 모든 구현체가 이 헤더만 include하면 됨 */
typedef struct { int pulse_idx; int raw_idx;  } PulseJob;
typedef struct { int buffer_idx;              } PostJob;

typedef enum {
    QUEUE_FUTEX    = 0,
    QUEUE_LOCKFREE = 1,
    QUEUE_MUTEX    = 2,
    QUEUE_USLEEP   = 3,
} QueueType;

/* pulse 전용 vtable */
typedef struct {
    int  (*push)   (void *impl, PulseJob  job);
    int  (*pop)    (void *impl, PulseJob *job);
    void (*close)  (void *impl);
    void (*open)   (void *impl);
    void (*flush)  (void *impl);   /* 배치 후 wake — futex만 구현, 나머지는 NULL 가능 */
    void (*destroy)(void *impl);
} PulseQueueOps;

/* post 전용 vtable */
typedef struct {
    int  (*push)   (void *impl, PostJob  job);
    int  (*pop)    (void *impl, PostJob *job);
    void (*close)  (void *impl);
    void (*open)   (void *impl);
    void (*destroy)(void *impl);
} PostQueueOps;

/* 핸들 */
typedef struct {
    void                *impl;
    const PulseQueueOps *ops;
} PulseQueue;

typedef struct {
    void               *impl;
    const PostQueueOps *ops;
} PostQueue;

/* 팩토리 */
PulseQueue *pulse_queue_create  (QueueType type, int cap);
PostQueue  *post_queue_create   (QueueType type, int cap);
void        pulse_queue_destroy (PulseQueue *q);
void        post_queue_destroy  (PostQueue  *q);
QueueType   queue_type_from_str (const char *s);

/* 매크로 제거하고 함수 선언으로 교체 */

/* pulse queue */
int  queue_push_pulse (PulseQueue *q, PulseJob job);
int  queue_pop_pulse  (PulseQueue *q, PulseJob *job);
void queue_close_pulse(PulseQueue *q);
void queue_open_pulse (PulseQueue *q);
void queue_flush_pulse(PulseQueue *q);

/* post queue */
int  queue_push_post  (PostQueue *q, PostJob job);
int  queue_pop_post   (PostQueue *q, PostJob *job);
void queue_close_post (PostQueue *q);
void queue_open_post  (PostQueue *q);

#endif