/* Inc/queue/lock_free/post_queue_lf.h */
#ifndef POST_QUEUE_LF_H
#define POST_QUEUE_LF_H

#include "queue_interface.h"

int  post_queue_init_lf   (void **out, int cap);
int  post_queue_push_lf   (void *impl, PostJob job);
int  post_queue_pop_lf    (void *impl, PostJob *job);
void post_queue_close_lf  (void *impl);
void post_queue_open_lf   (void *impl);
void post_queue_destroy_lf(void *impl);

#endif