#ifndef PIPELINE_TYPES_H
#define PIPELINE_TYPES_H

#include <pthread.h>
#include <stdatomic.h>

#include "cfar.h"

typedef struct {
    char filename[256];
    int detected;
    Detection det;
} TrackPoint;

typedef struct {
    double load_ms;
    double pulse_ready_ms;
    double pulse_apply_ms;
    double pulse_total_ms;
    double mti_ms;
    double mtd_ms;
    double doppler_total_ms;
    double cfar_ms;
    double total_time_ms;
    double algo_only_ms;
    int detections;
} Accumulator;

#endif