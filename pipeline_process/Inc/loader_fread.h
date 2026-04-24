#ifndef __LOADER_FREAD_H__
#define __LOADER_FREAD_H__

#include <stddef.h>
#include <complex.h>
#include "loader.h"

typedef struct {
    double i;
    double q;
} RawIQSample;

// mmap 없이 fread를 사용하여 파일 전체를 읽어 2차원 ComplexMatrix로 반환합니다.
int load_complex_bin_all_fread(const char *path, 
                               int num_pulses, 
                               int num_fast_time_samples, 
                               size_t header_offset, 
                               ComplexMatrix *out);

#endif