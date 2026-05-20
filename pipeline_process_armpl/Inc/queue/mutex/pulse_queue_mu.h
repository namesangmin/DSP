/* Inc/queue/mutex/pulse_queue_mu.h */
#ifndef PULSE_QUEUE_MU_H
#define PULSE_QUEUE_MU_H

#include "queue_interface.h"

int  pulse_queue_init_mu   (void **out, int cap);
int  pulse_queue_push_mu   (void *impl, PulseJob job);
int  pulse_queue_pop_mu    (void *impl, PulseJob *job);
void pulse_queue_close_mu  (void *impl);
void pulse_queue_open_mu   (void *impl);
void pulse_queue_destroy_mu(void *impl);

#endif