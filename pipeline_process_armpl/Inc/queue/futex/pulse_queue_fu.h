/* Inc/queue/futex/pulse_queue_fu.h */
#ifndef PULSE_QUEUE_FU_H
#define PULSE_QUEUE_FU_H

#include "queue_interface.h"

int  pulse_queue_init_fu   (void **out, int cap);
int  pulse_queue_push_fu   (void *impl, PulseJob job);
int  pulse_queue_pop_fu    (void *impl, PulseJob *job);
void pulse_queue_close_fu  (void *impl);
void pulse_queue_open_fu   (void *impl);
void pulse_queue_destroy_fu(void *impl);
void pulse_queue_flush_fu(void *impl);

#endif