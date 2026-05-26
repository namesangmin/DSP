/* Inc/queue/lock_free/pulse_queue_lf.h */
#ifndef PULSE_QUEUE_LF_H
#define PULSE_QUEUE_LF_H

#include "queue_interface.h"

int  pulse_queue_init_lf   (void **out, int cap);
int  pulse_queue_push_lf   (void *impl, PulseJob job);
int  pulse_queue_pop_lf    (void *impl, PulseJob *job);
void pulse_queue_close_lf  (void *impl);
void pulse_queue_open_lf   (void *impl);
void pulse_queue_destroy_lf(void *impl);

#endif