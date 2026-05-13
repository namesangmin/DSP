// types.h - 의존성 없는 순수 타입 정의
#ifndef __RADAR_TYPES_H__
#define __RADAR_TYPES_H__

//#include <complex.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>
#include <complex.h>
typedef struct {
    float fc_hz;
    float fs_hz;
    float prf_hz;
    float pulse_width_s;
    float sweep_bandwidth_hz;
    uint32_t num_pulses;
    uint32_t num_fast_time_samples;
} RadarMeta;

typedef struct {
    int rows;
    int cols;
    float complex *data;
} ComplexMatrix;

#endif