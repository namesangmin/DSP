/* Inc/queue/futex/post_queue_fu.h */
#ifndef POST_QUEUE_FU_H
#define POST_QUEUE_FU_H

#include "queue_interface.h"

int  post_queue_init_fu   (void **out, int cap);
int  post_queue_push_fu   (void *impl, PostJob job);
int  post_queue_pop_fu    (void *impl, PostJob *job);
void post_queue_close_fu  (void *impl);
void post_queue_open_fu   (void *impl);
void post_queue_destroy_fu(void *impl);

#endif