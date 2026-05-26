#ifndef __PULSE_COMPRESS_THREAD_H__
#define __PULSE_COMPRESS_THREAD_H__

#include "pipeline_set.h"
#include "types.h"
#include "pulse.h"
#include "common.h"
#include "layout_interface.h"


typedef struct {
    const RadarMeta *meta;
    Pipeline *pipe;
    PulseCompressCtx ctx;
    PulseQueue* q;
    Layout *layout;

    int cpu_id;
    int tid;
    float complex *tmp_buf;   /* legacy용 임시 버퍼 */

} WorkerArgs;

void *worker_thread_main(void *arg);

#endif