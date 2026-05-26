#include "dispatch_evenodd.h"

void dispatch_evenodd(int pulse_idx, int half,
                      PulseQueue *q0, PulseQueue *q1,
                      PulseQueue **out_q)
{
    *out_q = (pulse_idx % 2 == 0) ? q0 : q1;
}