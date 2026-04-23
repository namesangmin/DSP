#ifndef LOADER_H
#define LOADER_H

#include <complex.h>
#include <stddef.h>

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
    int rows;
    int cols;
    double *data;
} RealMatrix;

int alloc_complex_matrix(int rows, int cols, ComplexMatrix *m);
int alloc_real_matrix(int rows, int cols, RealMatrix *m);
void free_complex_matrix(ComplexMatrix *m);
void free_real_matrix(RealMatrix *m);

int load_metadata(const char *path, RadarMeta *meta);

int load_complex_csv_pair(const char *real_path,
                          const char *imag_path,
                          int rows, int cols,
                          ComplexMatrix *out);

int load_complex_bin_pair_matlab(const char *real_path,
                                 const char *imag_path,
                                 int rows, int cols,
                                 ComplexMatrix *out);

// dat 파일
int load_complex_bin_single(const char *path, int rows, int cols, ComplexMatrix *out);

#define CMAT_AT(m, r, c) ((m)->data[(size_t)(r) * (size_t)((m)->cols) + (size_t)(c)])
#define RMAT_AT(m, r, c) ((m)->data[(size_t)(r) * (size_t)((m)->cols) + (size_t)(c)])

#endif
