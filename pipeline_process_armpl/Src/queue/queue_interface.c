#include "queue_interface.h"
#include "pulse_queue_fu.h"
#include "post_queue_fu.h"
#include "pulse_queue_lf.h"
#include "post_queue_lf.h"
#include "pulse_queue_mu.h"
#include "post_queue_mu.h"
#include "pulse_queue_us.h"
#include "post_queue_us.h"
#include <stdlib.h>
#include <string.h>

static const PulseQueueOps pulse_ops_table[] = {
    [QUEUE_FUTEX]    = { pulse_queue_push_fu, pulse_queue_pop_fu,
                         pulse_queue_close_fu, pulse_queue_open_fu,
                         pulse_queue_flush_fu,       /* futex만 구현 */
                         pulse_queue_destroy_fu },
    [QUEUE_LOCKFREE] = { pulse_queue_push_lf, pulse_queue_pop_lf,
                         pulse_queue_close_lf, pulse_queue_open_lf,
                         NULL,                        /* 스핀이라 불필요 */
                         pulse_queue_destroy_lf },
    [QUEUE_MUTEX]    = { pulse_queue_push_mu, pulse_queue_pop_mu,
                         pulse_queue_close_mu, pulse_queue_open_mu,
                         NULL,                        /* cond_signal이 push 안에서 처리 */
                         pulse_queue_destroy_mu },
    [QUEUE_USLEEP]   = { pulse_queue_push_us, pulse_queue_pop_us,
                         pulse_queue_close_us, pulse_queue_open_us,
                         NULL,
                         pulse_queue_destroy_us },
};

static const PostQueueOps post_ops_table[] = {
    [QUEUE_FUTEX]    = { post_queue_push_fu, post_queue_pop_fu, 
                         post_queue_close_fu, post_queue_open_fu, 
                         post_queue_destroy_fu },
    [QUEUE_LOCKFREE] = { post_queue_push_lf, post_queue_pop_lf, 
                         post_queue_close_lf, post_queue_open_lf, 
                         post_queue_destroy_lf },
    [QUEUE_MUTEX]    = { post_queue_push_mu, post_queue_pop_mu, 
                         post_queue_close_mu, post_queue_open_mu, 
                         post_queue_destroy_mu },
    [QUEUE_USLEEP]   = { post_queue_push_us, post_queue_pop_us, 
                         post_queue_close_us, post_queue_open_us, 
                         post_queue_destroy_us },
};

typedef int (*init_fn)(void **out, int cap);

static const init_fn pulse_init_table[] = {
    [QUEUE_FUTEX]    = pulse_queue_init_fu,
    [QUEUE_LOCKFREE] = pulse_queue_init_lf,
    [QUEUE_MUTEX]    = pulse_queue_init_mu,
    [QUEUE_USLEEP]   = pulse_queue_init_us,
};

static const init_fn post_init_table[] = {
    [QUEUE_FUTEX]    = post_queue_init_fu,
    [QUEUE_LOCKFREE] = post_queue_init_lf,
    [QUEUE_MUTEX]    = post_queue_init_mu,
    [QUEUE_USLEEP]   = post_queue_init_us,
};

PulseQueue *pulse_queue_create(QueueType type, int cap) {
    PulseQueue *q = malloc(sizeof(PulseQueue));
    if (!q) return NULL;
    if (pulse_init_table[type](&q->impl, cap) != 0) { free(q); return NULL; }
    q->ops = &pulse_ops_table[type];
    return q;
}

PostQueue *post_queue_create(QueueType type, int cap) {
    PostQueue *q = malloc(sizeof(PostQueue));
    if (!q) return NULL;
    if (post_init_table[type](&q->impl, cap) != 0) { free(q); return NULL; }
    q->ops = &post_ops_table[type];
    return q;
}

void pulse_queue_destroy(PulseQueue *q) {
    if (!q) return;
    q->ops->destroy(q->impl);
    free(q);
}

void post_queue_destroy(PostQueue *q) {
    if (!q) return;
    q->ops->destroy(q->impl);
    free(q);
}

QueueType queue_type_from_str(const char *s) {
    if (!strcmp(s, "futex"))    return QUEUE_FUTEX;
    if (!strcmp(s, "lockfree")) return QUEUE_LOCKFREE;
    if (!strcmp(s, "mutex"))    return QUEUE_MUTEX;
    if (!strcmp(s, "usleep"))   return QUEUE_USLEEP;
    return QUEUE_FUTEX;
}

/* pulse queue */
int queue_push_pulse(PulseQueue *q, PulseJob job)
{
    return q->ops->push(q->impl, job);
}

int queue_pop_pulse(PulseQueue *q, PulseJob *job)
{
    return q->ops->pop(q->impl, job);
}

void queue_close_pulse(PulseQueue *q)
{
    q->ops->close(q->impl);
}

void queue_open_pulse(PulseQueue *q)
{
    q->ops->open(q->impl);
}

void queue_flush_pulse(PulseQueue *q)
{
    if (q->ops->flush) q->ops->flush(q->impl);
}

/* post queue */
int queue_push_post(PostQueue *q, PostJob job)
{
    return q->ops->push(q->impl, job);
}

int queue_pop_post(PostQueue *q, PostJob *job)
{
    return q->ops->pop(q->impl, job);
}

void queue_close_post(PostQueue *q)
{
    q->ops->close(q->impl);
}

void queue_open_post(PostQueue *q)
{
    q->ops->open(q->impl);
}