/* Inc/queue/mutex/post_queue_mu.h */
#ifndef POST_QUEUE_MU_H
#define POST_QUEUE_MU_H

#include "queue_interface.h"

int  post_queue_init_mu   (void **out, int cap);
int  post_queue_push_mu   (void *impl, PostJob job);
int  post_queue_pop_mu    (void *impl, PostJob *job);
void post_queue_close_mu  (void *impl);
void post_queue_open_mu   (void *impl);
void post_queue_destroy_mu(void *impl);

#endif