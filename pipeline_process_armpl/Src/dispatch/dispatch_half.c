#include "dispatch_half.h"

void dispatch_half(int pulse_idx, int half,
                   PulseQueue *q0, PulseQueue *q1,
                   PulseQueue **out_q)
{
    *out_q = (pulse_idx < half) ? q0 : q1;
}