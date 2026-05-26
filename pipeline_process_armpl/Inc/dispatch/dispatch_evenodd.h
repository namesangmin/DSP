#ifndef DISPATCH_EVENODD_H
#define DISPATCH_EVENODD_H

#include "queue_interface.h"

void dispatch_evenodd(int pulse_idx, int half,
                      PulseQueue *q0, PulseQueue *q1,
                      PulseQueue **out_q);

#endif