#ifndef __PIPELINE_SET_H__
#define __PIPELINE_SET_H__

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <fftw3.h>
#include "types.h"
#include "queue_interface.h"   /* queue_post.h, queue_pulse.h 대신 */
#include "layout_interface.h"
#define NUM_BUFFERS 2

typedef enum {
    BUF_FREE       = 0,
    BUF_FILLING    = 1,
    BUF_READY      = 2,
    BUF_PROCESSING = 3
} BufferState;

typedef struct {
    ComplexMatrix data;
    atomic_int    state;
    atomic_int    done_count;
} RdMapBuffer;

typedef struct {
    ComplexMatrix data;
    atomic_int    state;
} DopplerBuffer;

typedef struct {
    atomic_int    error;
    atomic_int    active_workers;
    double        compress_times[NUM_BUFFERS][2];

    PulseQueue   *worker_q[2];   /* even_q, odd_q → worker_q[0], worker_q[1] */
    PostQueue    *post_q;

    float complex  *raw_data[NUM_BUFFERS];
    RdMapBuffer     pulse_compress_map[NUM_BUFFERS];
    DopplerBuffer   doppler_map[NUM_BUFFERS];

    uint32_t        dwell_ids[NUM_BUFFERS];
    float           phi[NUM_BUFFERS];
} Pipeline;

struct RadarMeta;

int init_pipeline_pool(const RadarMeta *meta, Pipeline *pool, int num_workers, LayoutType lt);
void cleanup_pipeline_pool (Pipeline *pool);

#endif