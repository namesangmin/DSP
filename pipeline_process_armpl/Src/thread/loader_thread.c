#include <stdio.h>
#include <stdatomic.h> // atomic 함수 사용을 위해 추가
#include <fftw3.h>
#include <complex.h>
#include <dirent.h>
#include <sys/stat.h>    // struct stat
#include <limits.h>      // PATH_MAX
#include <unistd.h>

#include "timer.h" 
#include "loader_thread.h"
#include "loader.h" // 고속 로드 함수 헤더 추가
#include "core_set.h"

int loader_thread_init(const RadarMeta *meta,  LoaderArgs *ld, Pipeline* pool) {
    size_t total_doubles = (size_t)meta->num_pulses * meta->num_fast_time_samples *2u;
   
    ld->buffer = (double*)malloc(total_doubles * sizeof(double));
    if (!ld->buffer) {
        return -1;
    }

    return 0;
}

int loader_thread_destroy(LoaderArgs *ld) {
    if (ld == NULL || ld->buffer == NULL) {
        return 0;
    }

    free(ld->buffer);
    ld->buffer = NULL;
    
    return 0;
}

void *loader_thread_main(void *arg)
{
    LoaderArgs *a = (LoaderArgs *)arg;
    pin_thread_to_cpu(a->cpu_id);

    int cols = a->meta->num_pulses;
    int rows = a->meta->num_fast_time_samples;
    int half = cols / 2;
    size_t total_doubles = (size_t)cols * rows * 2u;

    for (int fi = 0; fi < a->num_files; fi++) {
        const char *fname = a->file_list[fi]->d_name;
        if (fname[0] == '.') continue;

        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", a->dir_path, fname);

        struct stat st;
        if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        int raw_idx = fi % NUM_BUFFERS;

        while (atomic_load_explicit(&a->pipe->rd_maps[raw_idx].state, memory_order_acquire) != BUF_FREE) 
        {
            if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) {
                break; // 파이프라인 에러 시 탈출
            }
            usleep(100); // CPU 과부하 방지 (0.1ms 대기)
        }

        if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) {
            break;
        }

        atomic_store_explicit(&a->pipe->rd_maps[raw_idx].state, BUF_FILLING, memory_order_release);
        // =================================================================

        double t0 = now_ms();

        FILE *fp = fopen(filepath, "rb");
        if (!fp) {
            fprintf(stderr, "[Loader ERROR] fopen 실패: %s\n", filepath);
            atomic_store(&a->pipe->error, 1);
            break;
        }

        fseek(fp, 232, SEEK_SET);

        if (fread(a->buffer, sizeof(double), total_doubles, fp) != total_doubles) {
            fprintf(stderr, "[Loader ERROR] fread 실패: %s\n", filepath);
            fclose(fp);
            atomic_store(&a->pipe->error, 1);
            break;
        }
        fclose(fp);
        
        snprintf(a->pipe->filenames[raw_idx], 256, "%s", fname);

        double t1 = now_ms();

        int push_err = 0;
        for (int p = 0; p < cols; p++) {
            for (int s = 0; s < rows; s++) {
                size_t idx  = (size_t)p * rows + s;
                size_t bidx = 2u * idx;
                a->pipe->raw_data[raw_idx][idx] =
                    (float)a->buffer[bidx] + (float)a->buffer[bidx + 1] * I;
            }

            PulseJob job = { .pulse_idx = p, .raw_idx = raw_idx };
            PulseQueue *q = (p < half) ? &a->pipe->even_q : &a->pipe->odd_q;

            if (pulse_queue_push(q, job) != 0) {
                fprintf(stderr, "[Loader ERROR] 큐 Push 실패: pulse_idx=%d\n", p);
                atomic_store(&a->pipe->error, 1);
                push_err = 1;
                break;
            }
        }
        if (push_err) break;

        if (a->timing) {
            a->timing->loader_ms = t1 - t0;
        }
    }

    pulse_queue_close(&a->pipe->even_q);
    pulse_queue_close(&a->pipe->odd_q);
    
    return NULL;
}