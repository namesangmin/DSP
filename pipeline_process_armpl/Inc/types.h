// types.h - 의존성 없는 순수 타입 정의
#ifndef __RADAR_TYPES_H__
#define __RADAR_TYPES_H__

#include <complex.h>
#include <stddef.h>
#include <stdatomic.h>

typedef struct {
    double fc_hz;
    double fs_hz;
    double prf_hz;
    double pulse_width_s;
    double sweep_bandwidth_hz;
    int num_pulses;
    int num_fast_time_samples;
} RadarMeta;

typedef struct {
    int rows;
    int cols;
    float complex *data;
} ComplexMatrix;

// RawIQSample은 아래에서 따로 설명
typedef struct {
    double i;
    double q;
} RawIQSample;

#endif