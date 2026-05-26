#ifndef DISPATCH_INTERFACE_H
#define DISPATCH_INTERFACE_H

#include "types.h"
#include "queue_interface.h"

typedef enum {
    DISPATCH_HALF,
    DISPATCH_EVENODD,
} DispatchType;

typedef struct {
    void (*dispatch)(int pulse_idx, int half,
                     PulseQueue *q0, PulseQueue *q1,
                     PulseQueue **out_q);
} DispatchOps;

typedef struct {
    const DispatchOps *ops;
} Dispatch;

Dispatch    *dispatch_create       (DispatchType type);
void         dispatch_destroy      (Dispatch *d);
void         dispatch_select_queue (Dispatch *d, int pulse_idx, int half,
                                    PulseQueue *q0, PulseQueue *q1,
                                    PulseQueue **out_q);
DispatchType dispatch_type_from_str(const char *s);

#endif