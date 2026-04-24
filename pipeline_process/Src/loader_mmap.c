#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "loader_mmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

int dat_mmap_open(const char *path,
                  int num_pulses,
                  int num_fast_time_samples,
                  size_t header_offset,
                  DatMmapLoader *ctx)
{
    struct stat st;
    size_t expected_size;

    if (!path || !ctx || num_pulses <= 0 || num_fast_time_samples <= 0) {
        return -1;
    }

    if (sizeof(RawIQSample) != sizeof(double) * 2) {
        fprintf(stderr, "dat_mmap_open: RawIQSample size mismatch\n");
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->fd = -1;
    ctx->mapped = NULL;

    ctx->fd = open(path, O_RDONLY);
    if (ctx->fd < 0) {
        perror("open");
        return -1;
    }

    if (fstat(ctx->fd, &st) != 0) {
        perror("fstat");
        close(ctx->fd);
        ctx->fd = -1;
        return -1;
    }

    ctx->header_offset = header_offset;
    ctx->num_pulses = num_pulses;
    ctx->num_fast_time_samples = num_fast_time_samples;
    ctx->pulse_bytes = (size_t)num_fast_time_samples * sizeof(RawIQSample);

    expected_size = header_offset +
                    (size_t)num_pulses * (size_t)num_fast_time_samples * sizeof(RawIQSample);

    if ((size_t)st.st_size != expected_size) {
        fprintf(stderr,
                "dat_mmap_open: file size mismatch (actual=%zu, expected=%zu)\n",
                (size_t)st.st_size, expected_size);
        close(ctx->fd);
        ctx->fd = -1;
        return -1;
    }

    ctx->map_len = (size_t)st.st_size;
    ctx->mapped = mmap(NULL, ctx->map_len, PROT_READ, MAP_PRIVATE, ctx->fd, 0); // MAP_PRIVATE | MAP_POPULATE
    if (ctx->mapped == MAP_FAILED) {
        perror("mmap");
        close(ctx->fd);
        ctx->fd = -1;
        ctx->mapped = NULL;
        return -1;
    }

#ifdef MADV_SEQUENTIAL
    madvise(ctx->mapped, ctx->map_len, MADV_SEQUENTIAL);
#endif

    ctx->data_base = (const unsigned char *)ctx->mapped + header_offset;

    return 0;
}

const RawIQSample *dat_mmap_get_pulse(const DatMmapLoader *ctx, int pulse_idx)
{
    if (!ctx || !ctx->data_base) {
        return NULL;
    }

    if (pulse_idx < 0 || pulse_idx >= ctx->num_pulses) {
        return NULL;
    }

    return (const RawIQSample *)(ctx->data_base +
                                 (size_t)pulse_idx * ctx->pulse_bytes);
}

void dat_mmap_close(DatMmapLoader *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->mapped && ctx->mapped != MAP_FAILED) {
        munmap(ctx->mapped, ctx->map_len);
    }

    if (ctx->fd >= 0) {
        close(ctx->fd);
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->fd = -1;
}