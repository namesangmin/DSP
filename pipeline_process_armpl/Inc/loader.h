#ifndef LOADER_H
#define LOADER_H

#include <complex.h>
#include <stddef.h>
#include "types.h"

int alloc_complex_matrix(int rows, int cols, ComplexMatrix *m);
void free_complex_matrix(ComplexMatrix *m);

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
