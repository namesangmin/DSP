#ifndef LOADER_MMAP_H
#define LOADER_MMAP_H

#include <stddef.h>
#include <complex.h>
#include "loader.h"

typedef struct {
    double i;
    double q;
} RawIQSample;

typedef struct {
    int fd;
    size_t map_len;
    void *mapped;

    /* 232B header 제거 후 실제 IQ 시작 주소 */
    const unsigned char *data_base;

    int num_pulses;
    int num_fast_time_samples;
    size_t pulse_bytes;
    size_t header_offset;
} DatMmapLoader;

int dat_mmap_open(const char *path,
                  int num_pulses,
                  int num_fast_time_samples,
                  size_t header_offset,
                  DatMmapLoader *ctx);

/* pulse 시작 포인터 반환 */
const RawIQSample *dat_mmap_get_pulse(const DatMmapLoader *ctx, int pulse_idx);


void dat_mmap_close(DatMmapLoader *ctx);

#endif