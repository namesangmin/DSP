#ifndef __PULSE_COMPRESS_THREAD_H__
#define __PULSE_COMPRESS_THREAD_H__

#include <complex.h>
#include "loader.h"
#include "loader_fread.h"
#include "common.h"
#include "queue.h"

typedef struct {
    int pulse_idx;
} PulseJob;

void *worker_thread_main(void *arg);

#endif