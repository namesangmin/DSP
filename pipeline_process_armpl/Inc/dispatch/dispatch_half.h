#ifndef DISPATCH_HALF_H
#define DISPATCH_HALF_H

#include "queue_interface.h"

void dispatch_half(int pulse_idx, int half,
                   PulseQueue *q0, PulseQueue *q1,
                   PulseQueue **out_q);

#endif