#ifndef __DOPPLER_CFAR_THREAD_H__
#define __DOPPLER_CFAR_THREAD_H__

#include "common.h"
#include "pipeline_set.h"
#include "doppler_fft.h"
#include "pulse_compress_thread.h"

typedef struct {
    const RadarMeta *meta;
    PipelineFile *file;
    ComplexMatrix *doppler;
    DetectionList *det;
    DopplerFftTiming *doppler_timing;
    double *cfar_ms;
    int cpu_id;
    int status;    
} PostArgs;

void *post_thread_main(void *arg);

#endif