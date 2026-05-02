<<<<<<< Updated upstream
/*
* Copyright (C) 2013 - 2016  Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without restriction,
* including without limitation the rights to use, copy, modify, merge,
* publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in this
* Software without prior written authorization from Xilinx.
*
*/

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
=======
>>>>>>> Stashed changes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <complex.h>

#include "timer.h"
#include "loader.h"
#include "pulse.h"
#include "doppler_fft.h"
#include "cfar.h"
<<<<<<< Updated upstream
#include "writer.h"

#include "loader_mmap.h"
#include "pulse_mmap.h"

#include "common.h"
#include "queue.h"
#include "core_set.h"
#include "thread_func.h"


/* mmap + 4-thread pipeline:
   core0 = loader
   core1 = even pulse compression
   core2 = odd pulse compression
   core3 = Doppler + CFAR
*/
static int run_mmap_pipeline_single_file(const char *dat_path,
                                         const RadarMeta *meta,
                                         double *load_ms,
                                         PulseTiming *pulse_timing,
                                         ComplexMatrix *doppler,
                                         DopplerFftTiming *doppler_timing,
                                         double *cfar_ms,
                                         DetectionList *det)
{
    PipelineFile file;
    PulseQueue even_q, odd_q;
    //PulseChunkQueue even_q, odd_q;

    LoaderArgs ld;
    WorkerArgs wk_even, wk_odd;
    PostArgs post;
    pthread_t th_loader, th_even, th_odd, th_post;
    double t0, t1;

    memset(&file, 0, sizeof(file));
    memset(&even_q, 0, sizeof(even_q));
    memset(&odd_q, 0, sizeof(odd_q));
    memset(&ld, 0, sizeof(ld));
    memset(&wk_even, 0, sizeof(wk_even));
    memset(&wk_odd, 0, sizeof(wk_odd));
    memset(&post, 0, sizeof(post));

    if (!dat_path || !meta || !load_ms || !pulse_timing ||
        !doppler || !doppler_timing || !cfar_ms || !det) {
        return -1;
    }

    *load_ms = 0.0;
    *cfar_ms = 0.0;
    pulse_timing->filter_ready_ms = 0.0;
    pulse_timing->compression_ms = 0.0;
    doppler_timing->mti_ms = 0.0;
    doppler_timing->mtd_ms = 0.0;

    atomic_init(&file.done_count, 0);
    pthread_mutex_init(&file.post_mtx, NULL);
    pthread_cond_init(&file.post_cv, NULL);

    t0 = now_ms();

    if (pulse_compress_ctx_init(meta, &wk_even.ctx) != 0 ||
        pulse_compress_ctx_init(meta, &wk_odd.ctx) != 0) {
        pulse_compress_ctx_destroy(&wk_even.ctx);
        pulse_compress_ctx_destroy(&wk_odd.ctx);
        pulse_queue_destroy(&even_q);
        pulse_queue_destroy(&odd_q);
        free_complex_matrix(&file.pc);
        dat_mmap_close(&file.mm);
        pthread_cond_destroy(&file.post_cv);
        pthread_mutex_destroy(&file.post_mtx);
        return -1;
    }

    t1 = now_ms();
    pulse_timing->filter_ready_ms = t1 - t0;

    t0 = now_ms();
    if (dat_mmap_open(dat_path,
                      meta->num_pulses,
                      meta->num_fast_time_samples,
                      232,
                      &file.mm) != 0) {
        pthread_cond_destroy(&file.post_cv);
        pthread_mutex_destroy(&file.post_mtx);
        return -1;
    }
    t1 = now_ms();
    *load_ms = t1 - t0;

    if (alloc_complex_matrix(meta->num_fast_time_samples,
                             meta->num_pulses,
                             &file.pc) != 0) {
        dat_mmap_close(&file.mm);
        pthread_cond_destroy(&file.post_cv);
        pthread_mutex_destroy(&file.post_mtx);
        return -1;
    }

    if (pulse_queue_init(&even_q, 64) != 0 || pulse_queue_init(&odd_q, 64) != 0) {
        pulse_queue_destroy(&even_q);
        pulse_queue_destroy(&odd_q);
        free_complex_matrix(&file.pc);
        dat_mmap_close(&file.mm);
        pthread_cond_destroy(&file.post_cv);
        pthread_mutex_destroy(&file.post_mtx);
        return -1;
    }

    /* loader */
    ld.meta = meta;
    ld.file = &file;
    ld.even_q = &even_q;
    ld.odd_q = &odd_q;
    ld.cpu_id = 0;

    //짝수 큐
    wk_even.meta = meta;
    wk_even.total_pulses = meta->num_pulses;
    wk_even.file = &file;
    wk_even.q = &even_q;
    wk_even.even_q = &even_q;
    wk_even.odd_q = &odd_q;
    wk_even.cpu_id = 1;

    wk_odd.meta = meta;
    wk_odd.total_pulses = meta->num_pulses;
    wk_odd.file = &file;
    wk_odd.q = &odd_q;
    wk_odd.even_q = &even_q;
    wk_odd.odd_q = &odd_q;
    wk_odd.cpu_id = 2;

    /* post worker */
    post.meta = meta;
    post.file = &file;
    post.doppler = doppler;
    post.det = det;
    post.doppler_timing = doppler_timing;
    post.cfar_ms = cfar_ms;
    post.cpu_id = 3;
    post.status = 0;

    pthread_create(&th_post,   NULL, post_thread_main,   &post);
    t0 = now_ms();

    pthread_create(&th_loader, NULL, loader_thread_main, &ld);
    pthread_create(&th_even,   NULL, worker_thread_main, &wk_even);
    pthread_create(&th_odd,    NULL, worker_thread_main, &wk_odd);

    pthread_join(th_loader, NULL);
    pthread_join(th_even,   NULL);
    pthread_join(th_odd,    NULL);
    t1 = now_ms();
    pulse_timing->compression_ms = t1 - t0;

    if (file.error && !file.post_ready) {
        pipeline_signal_post(&file, 1);
    }

    pthread_join(th_post, NULL);

    pulse_queue_destroy(&even_q);
    pulse_queue_destroy(&odd_q);
    free_complex_matrix(&file.pc);
    dat_mmap_close(&file.mm);
    pthread_cond_destroy(&file.post_cv);
    pthread_mutex_destroy(&file.post_mtx);

    if (file.error || post.status != 0) {
        return -1;
    }

    if (atomic_load(&file.done_count) != meta->num_pulses) {
        fprintf(stderr, "done_count mismatch: got=%d expected=%d\n",
                atomic_load(&file.done_count), meta->num_pulses);
        return -1;
    }

    return 0;
}

// --------------------------------------------------------------------------------
// CPU 사용 틱 읽기 (/proc/self/stat - utime + stime)
// --------------------------------------------------------------------------------
static long read_cpu_ticks(void) {
    FILE *f = fopen("/proc/self/stat", "r");
    if (!f) return 0;

    unsigned long utime = 0, stime = 0;
    int   f1;
    char  f2[256], f3;
    int   f4, f5, f6, f7, f8;
    unsigned long f9, f10, f11, f12, f13;

    fscanf(f,
        "%d %s %c %d %d %d %d %d "
        "%lu %lu %lu %lu %lu "
        "%lu %lu",
        &f1, f2, &f3, &f4, &f5, &f6, &f7, &f8,
        &f9, &f10, &f11, &f12, &f13,
        &utime, &stime);

    fclose(f);
    return (long)(utime + stime);
}

static int ends_with(const char *s, const char *suffix) {
    size_t ls, lt;
    if (!s || !suffix) return 0;
    ls = strlen(s);
    lt = strlen(suffix);
    if (ls < lt) return 0;
    return strcmp(s + ls - lt, suffix) == 0;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage (File/Dir): %s <metadata.csv> <target_path> <imag_path_or_DUMMY> <version> [runs]\n", prog);
    
    fprintf(stderr, "version: matlab, mmap, single\n");
}
=======
#include "common.h"
#include "queue_post.h"
#include "queue_pulse.h"
#include "core_set.h"
#include "pipeline_set.h"
#include "doppler_cfar_thread.h"
#include "loader_thread.h"
#include "pulse_compress_thread.h"
#include "print.h"

// =========================================================
// 전역 상태 - init()에서 한 번만 초기화
// =========================================================
typedef struct {
    Pipeline        pipe;

    LoaderArgs      ld;
    WorkerArgs      wk_even;
    WorkerArgs      wk_odd;
    PostArgs        post;

    CfarWorkspace   cfar_ws;
    DopplerWorkspace doppler_ws;
    DetectionList   det;

    PipelineTiming  timing;

    pthread_t th_loader;
    pthread_t th_even;
    pthread_t th_odd;
    pthread_t th_post;

    const RadarMeta *meta;
    const char      *dat_path;
} AppState;

static AppState g_state;

// =========================================================
// init - 프로그램 시작 시 한 번만
// =========================================================
static int app_init(const char *dat_path, const RadarMeta *meta) {
    memset(&g_state, 0, sizeof(g_state));

    g_state.meta     = meta;
    g_state.dat_path = dat_path;

    // 파이프라인 초기화 (raw_data, rd_maps, doppler_maps 할당)
    if (init_pipeline_pool(dat_path, meta, &g_state.pipe) != 0) {
        fprintf(stderr, "init_pipeline failed\n");
        return -1;
    }

    // 큐 초기화
    if (pulse_queue_init(&g_state.pipe.even_q, meta->num_pulses / 2 + 1) != 0 ||
        pulse_queue_init(&g_state.pipe.odd_q,  meta->num_pulses / 2 + 1) != 0) {
        fprintf(stderr, "pulse_queue_init failed\n");
        cleanup_pipeline_pool (&g_state.pipe);
        return -1;
    }

    if (post_queue_init(&g_state.pipe.post_q, NUM_BUFFERS) != 0) {
        fprintf(stderr, "post_queue_init failed\n");
        pulse_queue_destroy(&g_state.pipe.even_q);
        pulse_queue_destroy(&g_state.pipe.odd_q);
        cleanup_pipeline_pool (&g_state.pipe);
        return -1;
    }

    // loader 초기화 (pulse_buffer만)
    if (loader_thread_init(dat_path, meta, &g_state.ld, &g_state.pipe) != 0) {
        fprintf(stderr, "loader_thread_init failed\n");
        post_queue_destroy(&g_state.pipe.post_q);
        pulse_queue_destroy(&g_state.pipe.even_q);
        pulse_queue_destroy(&g_state.pipe.odd_q);
        cleanup_pipeline_pool (&g_state.pipe);
        return -1;
    }

    // 펄스 압축 컨텍스트
    if (pulse_compress_ctx_init(meta, &g_state.wk_even.ctx) != 0 ||
        pulse_compress_ctx_init(meta, &g_state.wk_odd.ctx)  != 0) {
        fprintf(stderr, "pulse_compress_ctx_init failed\n");
        pulse_compress_ctx_destroy(&g_state.wk_even.ctx);
        pulse_compress_ctx_destroy(&g_state.wk_odd.ctx);
        loader_thread_destroy(&g_state.ld);
        post_queue_destroy(&g_state.pipe.post_q);
        pulse_queue_destroy(&g_state.pipe.even_q);
        pulse_queue_destroy(&g_state.pipe.odd_q);
        cleanup_pipeline_pool (&g_state.pipe);
        return -1;
    }

    // CFAR, Doppler 워크스페이스
    if (init_cfar_workspace(&g_state.cfar_ws,
                            meta->num_fast_time_samples,
                            meta->num_pulses) != 0) {
        fprintf(stderr, "init_cfar_workspace failed\n");
        pulse_compress_ctx_destroy(&g_state.wk_even.ctx);
        pulse_compress_ctx_destroy(&g_state.wk_odd.ctx);
        loader_thread_destroy(&g_state.ld);
        post_queue_destroy(&g_state.pipe.post_q);
        pulse_queue_destroy(&g_state.pipe.even_q);
        pulse_queue_destroy(&g_state.pipe.odd_q);
        cleanup_pipeline_pool (&g_state.pipe);
        return -1;
    }

    if (init_doppler_workspace(&g_state.doppler_ws,
                               meta->num_pulses,
                               meta->num_pulses) != 0) {
        fprintf(stderr, "init_doppler_workspace failed\n");
        cleanup_cfar_workspace(&g_state.cfar_ws);
        pulse_compress_ctx_destroy(&g_state.wk_even.ctx);
        pulse_compress_ctx_destroy(&g_state.wk_odd.ctx);
        loader_thread_destroy(&g_state.ld);
        post_queue_destroy(&g_state.pipe.post_q);
        pulse_queue_destroy(&g_state.pipe.even_q);
        pulse_queue_destroy(&g_state.pipe.odd_q);
        cleanup_pipeline_pool (&g_state.pipe);
        return -1;
    }

    return 0;
}

// =========================================================
// cleanup - 프로그램 종료 시 한 번만
// =========================================================
static void app_cleanup(void) {
    cleanup_pipeline_pool (&g_state.pipe);
    pulse_queue_destroy(&g_state.pipe.even_q);
    pulse_queue_destroy(&g_state.pipe.odd_q);
    post_queue_destroy(&g_state.pipe.post_q);
    loader_thread_destroy(&g_state.ld);
    pulse_compress_ctx_destroy(&g_state.wk_even.ctx);
    pulse_compress_ctx_destroy(&g_state.wk_odd.ctx);
    cleanup_cfar_workspace(&g_state.cfar_ws);
    cleanup_doppler_workspace(&g_state.doppler_ws);
    free_detection_list(&g_state.det);
}

// =========================================================
// 파일 하나 처리 - 스레드 실행 + join + 결과 수집
// =========================================================
static int process_single_file(const char *dat_path,
                               Detection  *out_best,
                               Accumulator *global_acc) {
    AppState *s = &g_state;
>>>>>>> Stashed changes

    // 파일마다 상태 리셋
    memset(&s->timing, 0, sizeof(s->timing));

<<<<<<< Updated upstream
    printf("Loaded config:\n");
    printf("  fc = %.3f Hz\n", meta->fc_hz);
    printf("  fs = %.3f Hz\n", meta->fs_hz);
    printf("  PRF = %.3f Hz\n", meta->prf_hz);
    printf("  pulse width = %.9f s\n", meta->pulse_width_s);
    printf("  sweep BW = %.3f Hz\n", meta->sweep_bandwidth_hz);
    printf("  pulses = %d\n", meta->num_pulses);
    printf("  fast-time samples = %d\n", meta->num_fast_time_samples);
    printf("  lambda = %.9f m\n", lambda);
    printf("  pulse samples = %d\n", pulse_samples);
}

static void print_average_line(const char *name, double avg_ms) {
    printf("  %-18s = %.3f ms (%.9f sec)\n", name, avg_ms, avg_ms / 1000.0);
}


// --------------------------------------------------------------------------------
// 단일 파일 처리
// [수정됨] Accumulator *global_acc 매개변수 추가
// --------------------------------------------------------------------------------
int process_single_file(const char *metadata_path,
                        const char *real_path,
                        const char *imag_path,
                        const char *version,
                        int runs,
                        Detection *out_best,
                        Accumulator *global_acc) 
{
    int config_printed = 0;
    DetectionList final_det = {0};
    
    double sum_load_ms = 0.0, sum_pulse_ready_ms = 0.0, sum_pulse_apply_ms = 0.0, sum_pulse_total_ms = 0.0;
    double sum_mti_ms = 0.0, sum_mtd_ms = 0.0, sum_doppler_total_ms = 0.0;
    double sum_cfar_ms = 0.0, sum_total_ms = 0.0, sum_detections = 0.0;

    long   ticks_before = read_cpu_ticks();
    double wall_before  = now_ms();

    for (int run = 1; run <= runs; ++run) {
        RadarMeta meta = {0};
        ComplexMatrix rx = {0}, pc = {0}, doppler = {0};
        DetectionList det = {0};
        PulseTiming pulse_timing = {0};
        DopplerFftTiming doppler_timing = {0};

        double total_t0 = now_ms();
        double t0;
        double load_ms = 0.0;
        double pulse_total_ms = 0.0;
        double doppler_total_ms = 0.0;
        double cfar_ms = 0.0;
        double total_ms = 0.0;

        int use_rx_path = 0;

        if (load_metadata(metadata_path, &meta) != 0) return 1;

        if (!config_printed && run == 1) {
            print_metadata(&meta);
            config_printed = 1;
        }

        /* ---------- 입력 경로 선택 ---------- */
        if (ends_with(real_path, ".bin") &&
            ends_with(imag_path, ".bin") &&
            ends_with(version, "matlab")) {

            t0 = now_ms();
            if (load_complex_bin_pair_matlab(real_path,
                                             imag_path,
                                             meta.num_fast_time_samples,
                                             meta.num_pulses,
                                             &rx) != 0) {
                return 1;
            }
            load_ms = now_ms() - t0;
            use_rx_path = 1;
        }
        else if (ends_with(real_path, ".dat") && ends_with(version, "single")) {

            t0 = now_ms();
            if (load_complex_bin_single(real_path,
                                        meta.num_pulses,
                                        meta.num_fast_time_samples,
                                        &rx) != 0) {
                return 1;
            }
            load_ms = now_ms() - t0;
            use_rx_path = 1;
        }
        else if (ends_with(real_path, ".dat") && ends_with(version, "mmap")) {

            if (run_mmap_pipeline_single_file(real_path,
                                              &meta,
                                              &load_ms,
                                              &pulse_timing,
                                              &doppler,
                                              &doppler_timing,
                                              &cfar_ms,
                                              &det) != 0) {
                return 1;
            }

        }
        else {
            t0 = now_ms();
            if (load_complex_csv_pair(real_path,
                                      imag_path,
                                      meta.num_fast_time_samples,
                                      meta.num_pulses,
                                      &rx) != 0) {
                return 1;
            }
            load_ms = now_ms() - t0;
            use_rx_path = 1;
        }

        /* ---------- 일반 경로: rx -> pc -> doppler -> cfar ---------- */
        if (use_rx_path) {
            if (pulse_compression_ex(&rx, &meta, &pc, &pulse_timing) != 0) {
                free_complex_matrix(&rx);
                return 1;
            }

            if (doppler_fft_processing_ex(&pc,
                                          &meta,
                                          meta.num_pulses,
                                          &doppler,
                                          &doppler_timing) != 0) {
                free_complex_matrix(&rx);
                free_complex_matrix(&pc);
                return 1;
            }

            t0 = now_ms();
            int numTrainR = 4, numTrainD = 4, numGuardR = 1, numGuardD = 1;
            int totalWindowCells = (2 * (numTrainR + numGuardR) + 1) * (2 * (numTrainD + numGuardD) + 1);
            int guardAndCUTCells = (2 * numGuardR + 1) * (2 * numGuardD + 1);
            int rankIdx = ((totalWindowCells - guardAndCUTCells) + 1) / 2;

            if (cfar_detect(&doppler,
                            &meta,
                            numTrainR,
                            numTrainD,
                            numGuardR,
                            numGuardD,
                            rankIdx,
                            9.0,
                            &det) != 0) {
                free_complex_matrix(&rx);
                free_complex_matrix(&pc);
                free_complex_matrix(&doppler);
                return 1;
            }
            cfar_ms = now_ms() - t0;
        }

        /* ---------- timing 정리 ---------- */
        pulse_total_ms = pulse_timing.filter_ready_ms + pulse_timing.compression_ms;
        doppler_total_ms = doppler_timing.mti_ms + doppler_timing.mtd_ms;
        total_ms = now_ms() - total_t0;

        printf("  [run %d] load=%.3fms total=%.3fms, detections=%d\n",
               run, load_ms, total_ms, det.count);

        sum_load_ms          += load_ms;
        sum_pulse_ready_ms   += pulse_timing.filter_ready_ms;
        sum_pulse_apply_ms   += pulse_timing.compression_ms;
        sum_pulse_total_ms   += pulse_total_ms;
        sum_mti_ms           += doppler_timing.mti_ms;
        sum_mtd_ms           += doppler_timing.mtd_ms;
        sum_doppler_total_ms += doppler_total_ms;
        sum_cfar_ms          += cfar_ms;
        sum_total_ms         += total_ms;
        sum_detections       += (double)det.count;

        if (run == runs) {
            free_detection_list(&final_det);
            final_det = det;
            det.items = NULL;
            det.count = 0;
        }

        free_detection_list(&det);
        free_complex_matrix(&rx);
        free_complex_matrix(&pc);
        free_complex_matrix(&doppler);
=======
    atomic_store(&s->pipe.error, 0);
    atomic_store(&s->pipe.current_write_idx, 0);
    
    for (int i = 0; i < NUM_BUFFERS; i++) {
        atomic_store(&s->pipe.rd_maps[i].state,      BUF_FREE);
        atomic_store(&s->pipe.rd_maps[i].done_count, 0);
        atomic_store(&s->pipe.doppler_maps[i].state, BUF_FREE);
>>>>>>> Stashed changes
    }

    free_detection_list(&s->det);
    memset(&s->det, 0, sizeof(s->det));

    // 큐 리셋
    pulse_queue_close(&s->pipe.even_q);
    pulse_queue_close(&s->pipe.odd_q);
    post_queue_close(&s->pipe.post_q);

    // loader args
    s->ld.dat_path = dat_path;
    s->ld.meta     = s->meta;
    s->ld.pipe     = &s->pipe;   // even_q, odd_q, raw_data 다 pipe 안에
    s->ld.cpu_id   = 0;
    s->ld.timing   = &s->timing;

    // worker even (코어1, pulse 0~255)
    s->wk_even.meta    = s->meta;
    s->wk_even.pipe    = &s->pipe;  // rd_maps, doppler_maps, post_q 다 pipe 안에
    s->wk_even.q       = &s->pipe.even_q;  // 자기 큐만 명시
    s->wk_even.cpu_id  = 1;
    s->wk_even.timing  = &s->timing;

    // worker odd (코어2, pulse 256~511)
    s->wk_odd.meta     = s->meta;
    s->wk_odd.pipe     = &s->pipe;
    s->wk_odd.q        = &s->pipe.odd_q;   // 자기 큐만 명시
    s->wk_odd.cpu_id   = 2;
    s->wk_odd.timing   = &s->timing;

    // post args
    s->post.meta       = s->meta;
    s->post.pipe       = &s->pipe;  // post_q, rd_maps, doppler_maps 다 pipe 안에
    s->post.det        = &s->det;
    s->post.cfar_ws    = &s->cfar_ws;
    s->post.doppler_ws = &s->doppler_ws;
    s->post.cpu_id     = 3;
    s->post.status     = 0;
    s->post.timing     = &s->timing;

    double total_t0 = now_ms();
    long ticks_before = read_cpu_ticks();

    pulse_queue_open(&s->pipe.even_q);
    pulse_queue_open(&s->pipe.odd_q);
    post_queue_open(&s->pipe.post_q);
    
    pthread_create(&s->th_loader, NULL, loader_thread_main, &s->ld);
    pthread_create(&s->th_even,   NULL, worker_thread_main, &s->wk_even);
    pthread_create(&s->th_odd,    NULL, worker_thread_main, &s->wk_odd);
    pthread_create(&s->th_post,   NULL, post_thread_main,   &s->post);
    
    pthread_join(s->th_loader, NULL);
    pthread_join(s->th_even,   NULL);
    pthread_join(s->th_odd,    NULL);
    pthread_join(s->th_post,   NULL);
                                    
    long ticks_after = read_cpu_ticks();
    double total_ms = now_ms() - total_t0;

    if (atomic_load(&s->pipe.error) != 0 || s->post.status != 0) {
        fprintf(stderr, "Pipeline error detected!\n");
        return -1;
    }

    // 타이밍 출력
    double doppler_total_ms = s->timing.mtd_ms + s->timing.mti_ms;
    double pulse_total_ms   = s->timing.compress_ms;
    double algo_only_ms     = pulse_total_ms + doppler_total_ms
                            + s->timing.cfar_ms + s->timing.transpose_ms;

    printf("\n--- Timing ---\n");
    print_average_line("load",          s->timing.loader_ms);
    print_average_line("compress",      pulse_total_ms);
    print_average_line("transpose",     s->timing.transpose_ms);
    print_average_line("mti",       s->timing.mti_ms);
    print_average_line("mtd",       s->timing.mtd_ms);
    print_average_line("cfar",          s->timing.cfar_ms);
    print_average_line("total",         total_ms);
    print_average_line("algo_only",     algo_only_ms);
    printf("  %-18s = %d\n", "detections", s->det.count);

    // CPU 사용률
    long   hz        = sysconf(_SC_CLK_TCK);
    double cpu_sec   = (double)(ticks_after - ticks_before) / (double)hz;
    double wall_sec  = total_ms / 1000.0;
    double cores_used = (wall_sec > 0.0) ? (cpu_sec / wall_sec) : 0.0;
<<<<<<< Updated upstream

    // [추가됨] 파일 하나의 처리가 완료된 후, global_acc 에 이 파일의 평균 수행 시간을 누적
    if (global_acc != NULL && runs > 0) {
        global_acc->load_ms          += (sum_load_ms / runs);
        global_acc->pulse_ready_ms   += (sum_pulse_ready_ms / runs);
        global_acc->pulse_apply_ms   += (sum_pulse_apply_ms / runs);
        global_acc->pulse_total_ms   += (sum_pulse_total_ms / runs);
        global_acc->mti_ms           += (sum_mti_ms / runs);
        global_acc->mtd_ms           += (sum_mtd_ms / runs);
        global_acc->doppler_total_ms += (sum_doppler_total_ms / runs);
        global_acc->cfar_ms          += (sum_cfar_ms / runs);
        global_acc->total_time_ms    += (sum_total_ms / runs);
        global_acc->algo_only_ms     += ((sum_pulse_total_ms + sum_doppler_total_ms + sum_cfar_ms) / runs);
        global_acc->detections       += (sum_detections / runs);
    }

    if (final_det.count > 0) {
        Detection best_det = final_det.items[0];
        for (int i = 1; i < final_det.count; i++) {
            if (final_det.items[i].power > best_det.power)
                best_det = final_det.items[i];
        }
        if (out_best) *out_best = best_det;

        printf("\nStrongest detection after CA-CFAR:\n");
        printf("  Range bin    = %d\n  Doppler bin  = %d\n",
               best_det.range_bin, best_det.doppler_bin);
        printf("  Range        = %.2f m\n  Radial vel.  = %.2f m/s\n",
               best_det.range_m, best_det.velocity_mps);
        printf("  Power        = %.6e\n  Threshold    = %.6e\n",
               best_det.power, best_det.threshold);
    } else {
        if (out_best) out_best->range_bin = -1;
        printf("\nNo CFAR detection found.\n");
    }

    printf("\n--- Average over %d runs ---\n", runs);
    print_average_line("load",          sum_load_ms          / (double)runs);
    print_average_line("pulse_ready",   sum_pulse_ready_ms   / (double)runs);
    print_average_line("pulse_apply",   sum_pulse_apply_ms   / (double)runs);
    print_average_line("pulse_total",   sum_pulse_total_ms   / (double)runs);
    print_average_line("mti",           sum_mti_ms           / (double)runs);
    print_average_line("mtd",           sum_mtd_ms           / (double)runs);
    print_average_line("doppler_total", sum_doppler_total_ms / (double)runs);
    print_average_line("cfar",          sum_cfar_ms          / (double)runs);
    print_average_line("total time",    sum_total_ms         / (double)runs);
    print_average_line("algo_only",     (sum_pulse_total_ms + sum_doppler_total_ms + sum_cfar_ms) / (double)runs);
    printf("  %-18s = %.2f\n", "detections", sum_detections / (double)runs);

    printf("\n--- CPU Core Usage (all %d runs) ---\n", runs);
=======
    printf("\n--- CPU Core Usage ---\n");
>>>>>>> Stashed changes
    printf("  Wall time   = %.3f sec\n", wall_sec);
    printf("  CPU time    = %.3f sec\n", cpu_sec);
    printf("  Cores used  = %.2f\n", cores_used);

    // 최강 탐지 결과
    if (s->det.count > 0) {
        Detection best = s->det.items[0];
        for (int i = 1; i < s->det.count; i++) {
            if (s->det.items[i].power > best.power)
                best = s->det.items[i];
        }
        if (out_best) *out_best = best;

        printf("\nStrongest detection:\n");
        printf("  Range bin   = %d\n  Doppler bin = %d\n",
               best.range_bin, best.doppler_bin);
        printf("  Range       = %.2f m\n  Velocity    = %.2f m/s\n",
               best.range_m, best.velocity_mps);
        printf("  Power       = %.6e\n  Threshold   = %.6e\n",
               best.power, best.threshold);
    } else {
        if (out_best) out_best->range_bin = -1;
        printf("\nNo CFAR detection found.\n");
    }

    // 글로벌 누적
    if (global_acc) {
        global_acc->load_ms          += s->timing.loader_ms;
        global_acc->pulse_total_ms   += pulse_total_ms;
        global_acc->mti_ms += s->timing.mti_ms;
        global_acc->mtd_ms += s->timing.mtd_ms;
        global_acc->doppler_total_ms += doppler_total_ms;
        global_acc->cfar_ms          += s->timing.cfar_ms;
        global_acc->transpose_ms     += s->timing.transpose_ms;
        global_acc->total_time_ms    += total_ms;
        global_acc->algo_only_ms     += algo_only_ms;
        global_acc->detections       += s->det.count;
    }

    return 0;
}

<<<<<<< Updated upstream
// --------------------------------------------------------------------------------
// 폴더 순회 및 궤적 요약
// --------------------------------------------------------------------------------
void process_directory(const char *dir_path, const char *metadata_path, const char *version, int runs) {
    struct dirent **namelist;

    // [추가됨] 전체 디렉토리에 대한 결과를 누적할 구조체
    Accumulator total_acc;
    memset(&total_acc, 0, sizeof(Accumulator));

=======
// =========================================================
// 디렉토리 순회
// =========================================================
static int process_directory(const char *dir_path, const char *metadata_path) {
    RadarMeta meta = {0};
    if (load_metadata(metadata_path, &meta) != 0) {
        fprintf(stderr, "failed to read metadata\n");
        return -1;
    }
    print_metadata(&meta);

    // 첫 번째 dat 파일 경로로 init (경로는 루프에서 매번 바뀜)
    // init은 메모리 할당만 하므로 dat_path는 나중에 ld.dat_path로 덮어씀
    if (app_init(dir_path, &meta) != 0) {
        return -1;
    }

    struct dirent **namelist;
>>>>>>> Stashed changes
    int num_files = scandir(dir_path, &namelist, NULL, versionsort);
    if (num_files < 0) {
        perror("scandir failed");
        app_cleanup();
        return -1;
    }

    DetectionList *history = calloc(num_files, sizeof(DetectionList));
    Accumulator    total_acc = {0};
    int            valid_files = 0;

    printf("\nScanning directory: %s\n", dir_path);

    for (int i = 0; i < num_files; i++) {
        const char *filename = namelist[i]->d_name;

        if (filename[0] == '.') {
            free(namelist[i]);
            continue;
        }

<<<<<<< Updated upstream
        char filepath[1024];
        Detection best_det;
        int processed = 0;

        if (ends_with(version, "single") || ends_with(version, "mmap")) {
            snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, filename);
            struct stat st;

            if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
                printf("\n=========================================================\n");
                printf("[FILE %d] Processing Integrated DAT: %s\n", valid_files + 1, filename);
                printf("=========================================================\n");
                // [수정됨] 매개변수 맨 끝에 &total_acc 전달
                process_single_file(metadata_path, filepath, "DUMMY", version, runs, &best_det, &total_acc);
                processed = 1;
            }
        } 
        else if (strstr(filename, "real") != NULL &&
                   (ends_with(filename, ".csv") || ends_with(filename, ".bin"))) {

            char real_file[1024], imag_file[1024], imag_name[512];
            snprintf(real_file, sizeof(real_file), "%s/%s", dir_path, filename);

            char *real_ptr = strstr(filename, "real");
            strncpy(imag_name, filename, (size_t)(real_ptr - filename));
            imag_name[real_ptr - filename] = '\0';
            strcat(imag_name, "imag");
            strcat(imag_name, real_ptr + 4);

            snprintf(imag_file, sizeof(imag_file), "%s/%s", dir_path, imag_name);

            struct stat st;
            if (stat(real_file, &st) == 0 && S_ISREG(st.st_mode)) {
                printf("\n=========================================================\n");
                printf("[FILE %d] Processing Paired Data: \n  Real: %s\n  Imag: %s\n", valid_files + 1, filename, imag_name);
                printf("=========================================================\n");
                // [수정됨] 매개변수 맨 끝에 &total_acc 전달
                process_single_file(metadata_path, real_file, imag_file, version, runs, &best_det, &total_acc);
                processed = 1;
            }
=======
        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, filename);

        struct stat st;
        if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
            free(namelist[i]);
            continue;
>>>>>>> Stashed changes
        }

        printf("\n=========================================================\n");
        printf("[FILE %d] %s\n", valid_files + 1, filename);
        printf("=========================================================\n");

        Detection best_det = {0};
        if (process_single_file(filepath, &best_det, &total_acc) != 0) {
            fprintf(stderr, "process_single_file failed: %s\n", filename);
            free(namelist[i]);
            continue;
        }

        strncpy(history[valid_files].filename, filename, 255);
        history[valid_files].filename[255] = '\0';
        history[valid_files].count = (best_det.range_bin != -1) ? 1 : 0;
        if (history[valid_files].count > 0) {
            history[valid_files].items = malloc(sizeof(Detection));
            if (history[valid_files].items)
                history[valid_files].items[0] = best_det;
        }
        valid_files++;
        free(namelist[i]);
    }
    free(namelist);

    if (valid_files > 0) {
<<<<<<< Updated upstream
        printf("\n\n#########################################################\n");
        printf("                TARGET TRAJECTORY SUMMARY                \n");
        printf("#########################################################\n");
        printf("Total Files Processed: %d\n\n", valid_files);

        printf("%-20s | %-12s | %-12s | %-15s\n", "Filename", "Range (m)", "Velocity (m/s)", "Power");
        printf("-----------------------------------------------------------------\n");

        int first_idx = -1, last_idx = -1;

        for (int i = 0; i < valid_files; i++) {
            if (history[i].detected) {
                if (first_idx == -1) first_idx = i;
                last_idx = i;
                printf("%-20s | %-12.2f | %-12.2f | %-15.2e\n",
                       history[i].filename,
                       history[i].det.range_m,
                       history[i].det.velocity_mps,
                       history[i].det.power);
            } else {
                printf("%-20s | %-12s | %-12s | %-15s\n", history[i].filename, "N/A", "N/A", "N/A");
            }
        }

        if (first_idx != -1 && last_idx != -1 && first_idx != last_idx) {
            double start_range = history[first_idx].det.range_m;
            double end_range   = history[last_idx].det.range_m;
            double diff        = end_range - start_range;

            printf("\n--- Trajectory Analysis ---\n");
            printf("Initial Position : %.2f m (File: %s)\n", start_range, history[first_idx].filename);
            printf("Final Position   : %.2f m (File: %s)\n", end_range,   history[last_idx].filename);
            printf("Displacement     : %.2f m %s\n", diff, diff < 0 ? "(Moving Toward)" : "(Moving Away)");
        }
        printf("#########################################################\n\n");

        // [추가됨] 모든 파일이 처리된 후, 전체 디렉토리의 평균 출력
        printf("\n\n#########################################################\n");
        printf("         GLOBAL DIRECTORY AVERAGE (%d Files)        \n", valid_files);
        printf("#########################################################\n");
        print_average_line("load",          total_acc.load_ms          / valid_files);
        print_average_line("pulse_ready",   total_acc.pulse_ready_ms   / valid_files);
        print_average_line("pulse_apply",   total_acc.pulse_apply_ms   / valid_files);
        print_average_line("pulse_total",   total_acc.pulse_total_ms   / valid_files);
        print_average_line("mti",           total_acc.mti_ms           / valid_files);
        print_average_line("mtd",           total_acc.mtd_ms           / valid_files);
        print_average_line("doppler_total", total_acc.doppler_total_ms / valid_files);
        print_average_line("cfar",          total_acc.cfar_ms          / valid_files);
        print_average_line("total time",    total_acc.total_time_ms    / valid_files);
        print_average_line("algo_only",     total_acc.algo_only_ms     / valid_files);
        printf("  %-18s = %.2f\n", "detections", (double)total_acc.detections / valid_files);
        printf("#########################################################\n\n");
=======
        print_trajectory_summary(history, valid_files);
        print_global_average(&total_acc, valid_files > 1 ? valid_files - 1 : 1);
>>>>>>> Stashed changes
    }

    for (int i = 0; i < valid_files; i++)
        free_detection_list(&history[i]);
    free(history);

    app_cleanup();
    return 0;
}

<<<<<<< Updated upstream
// --------------------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------------------
int main(int argc, char **argv) {
    pin_to_cpu0();

    if (argc < 4) {
=======
// =========================================================
// main
// =========================================================
int main(int argc, char **argv) {
    if (argc < 3) {
>>>>>>> Stashed changes
        print_usage(argv[0]);
        return 1;
    }

    const char *metadata_path = argv[1];
    const char *target_path   = argv[2];
<<<<<<< Updated upstream
    const char *imag_path     = argv[3];
    const char *version = argv[4];
    int runs = (argc >= 6) ? atoi(argv[5]) : 1;
    if (runs <= 0) runs = 1;
=======
>>>>>>> Stashed changes

    struct stat st;
    if (stat(target_path, &st) != 0) {
        perror("stat failed");
        return 1;
    }

    if (S_ISDIR(st.st_mode)) {
<<<<<<< Updated upstream
        printf("Target is a DIRECTORY. Batch sequential processing initiated...\n");
        process_directory(target_path, metadata_path, version, runs);
    } else if (S_ISREG(st.st_mode)) {
        printf("Target is a FILE.\n");
        // [수정됨] 단일 파일 모드에서는 global_acc 에 NULL 전달
        process_single_file(metadata_path, target_path, imag_path, version, runs, NULL, NULL);
=======
        printf("Target is a DIRECTORY. Batch processing...\n");
        return process_directory(target_path, metadata_path);
>>>>>>> Stashed changes
    }

    fprintf(stderr, "target must be a directory\n");
    return 1;
}