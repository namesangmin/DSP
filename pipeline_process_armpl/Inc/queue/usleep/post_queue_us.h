/* Inc/queue/usleep/post_queue_us.h */
#ifndef POST_QUEUE_US_H
#define POST_QUEUE_US_H

#include "queue_interface.h"

int  post_queue_init_us   (void **out, int cap);
int  post_queue_push_us   (void *impl, PostJob job);
int  post_queue_pop_us    (void *impl, PostJob *job);
void post_queue_close_us  (void *impl);
void post_queue_open_us   (void *impl);
void post_queue_destroy_us(void *impl);

#endif