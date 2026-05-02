#ifndef PIPELINE_TYPES_H
#define PIPELINE_TYPES_H

typedef struct {
    double load_ms;
    double pulse_apply_ms;
    double pulse_total_ms;
    double mti_ms;
    double mtd_ms;
    double doppler_total_ms;
    double cfar_ms;
    double transpose_ms;
    double total_time_ms;
    double algo_only_ms;
    int detections;
} Accumulator;

// common.h에 추가
typedef struct {
    double loader_ms;
    double compress_ms;
    double transpose_ms;
    double mti_ms;
    double mtd_ms;
    double cfar_ms;
} PipelineTiming;

#define CMAT_AT(m, r, c) ((m)->data[(size_t)(r) * (size_t)((m)->cols) + (size_t)(c)])

#endif