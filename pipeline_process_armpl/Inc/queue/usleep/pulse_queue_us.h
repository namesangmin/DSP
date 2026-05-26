/* Inc/queue/usleep/pulse_queue_us.h */
#ifndef PULSE_QUEUE_US_H
#define PULSE_QUEUE_US_H

#include "queue_interface.h"

int  pulse_queue_init_us   (void **out, int cap);
int  pulse_queue_push_us   (void *impl, PulseJob job);
int  pulse_queue_pop_us    (void *impl, PulseJob *job);
void pulse_queue_close_us  (void *impl);
void pulse_queue_open_us   (void *impl);
void pulse_queue_destroy_us(void *impl);

#endif