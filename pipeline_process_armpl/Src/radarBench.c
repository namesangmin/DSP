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

    // 파일마다 상태 리셋
    memset(&s->timing, 0, sizeof(s->timing));

    atomic_store(&s->pipe.error, 0);
    atomic_store(&s->pipe.current_write_idx, 0);
    
    for (int i = 0; i < NUM_BUFFERS; i++) {
        atomic_store(&s->pipe.rd_maps[i].state,      BUF_FREE);
        atomic_store(&s->pipe.rd_maps[i].done_count, 0);
        atomic_store(&s->pipe.doppler_maps[i].state, BUF_FREE);
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
    printf("\n--- CPU Core Usage ---\n");
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

        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, filename);

        struct stat st;
        if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
            free(namelist[i]);
            continue;
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
        print_trajectory_summary(history, valid_files);
        print_global_average(&total_acc, valid_files > 1 ? valid_files - 1 : 1);
    }

    for (int i = 0; i < valid_files; i++)
        free_detection_list(&history[i]);
    free(history);

    app_cleanup();
    return 0;
}

// =========================================================
// main
// =========================================================
int main(int argc, char **argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *metadata_path = argv[1];
    const char *target_path   = argv[2];

    struct stat st;
    if (stat(target_path, &st) != 0) {
        perror("stat failed");
        return 1;
    }

    if (S_ISDIR(st.st_mode)) {
        printf("Target is a DIRECTORY. Batch processing...\n");
        return process_directory(target_path, metadata_path);
    }

    fprintf(stderr, "target must be a directory\n");
    return 1;
}