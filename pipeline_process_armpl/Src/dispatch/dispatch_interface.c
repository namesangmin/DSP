#include "dispatch_interface.h"
#include "dispatch_half.h"
#include "dispatch_evenodd.h"
#include <stdlib.h>
#include <string.h>

static const DispatchOps half_ops    = { dispatch_half    };
static const DispatchOps evenodd_ops = { dispatch_evenodd };

Dispatch *dispatch_create(DispatchType type)
{
    Dispatch *d = malloc(sizeof(Dispatch));
    if (!d) return NULL;
    d->ops = (type == DISPATCH_EVENODD) ? &evenodd_ops : &half_ops;
    return d;
}

void dispatch_destroy(Dispatch *d)
{
    free(d);
}

void dispatch_select_queue(Dispatch *d, int pulse_idx, int half,
                           PulseQueue *q0, PulseQueue *q1,
                           PulseQueue **out_q)
{
    d->ops->dispatch(pulse_idx, half, q0, q1, out_q);
}

DispatchType dispatch_type_from_str(const char *s)
{
    if (!strcmp(s, "evenodd")) return DISPATCH_EVENODD;
    return DISPATCH_HALF;
}