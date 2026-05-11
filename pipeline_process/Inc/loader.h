#ifndef LOADER_H
#define LOADER_H

#include <complex.h>
#include <stddef.h>
#include "common.h"

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
    double complex *data;
} ComplexMatrix;

typedef struct {
    double i;
    double q;
} RawIQSample;

int alloc_complex_matrix(int rows, int cols, ComplexMatrix *m);
//int alloc_real_matrix(int rows, int cols, RealMatrix *m);
void free_complex_matrix(ComplexMatrix *m);
//void free_real_matrix(RealMatrix *m);

int load_metadata(const char *path, RadarMeta *meta);

// dat 파일
int load_complex_bin_single(const char *path, int rows, int cols, ComplexMatrix *out);

// mmap 없이 fread를 사용하여 파일 전체를 읽어 2차원 ComplexMatrix로 반환합니다.
int load_complex_bin_all_fread(const char *path, 
                               int num_pulses, 
                               int num_fast_time_samples, 
                               size_t header_offset, 
                               ComplexMatrix *out);

#endif
