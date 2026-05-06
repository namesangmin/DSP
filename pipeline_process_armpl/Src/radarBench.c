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
#include <signal.h>  

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
#include "cluster.h"
#include "udp.h"
#include "send_graph_data.h"

// =========================================================
// 전역 상태 - init()에서 한 번만 초기화
// =========================================================
typedef struct {
    int valid_files;
    Pipeline        pipe;

    LoaderArgs      ld;
    WorkerArgs      wk_even;
    WorkerArgs      wk_odd;
    PostArgs        post;

    CfarWorkspace   cfar_ws;
    DopplerWorkspace doppler_ws;
    DetectionList   det;

    PipelineTiming  timing;
    Accumulator total_acc;
    DetectionList *history;

    ClusterWorkspace cluster_ws;   // 추가
    ClusterParams    cluster_params; // 추가
    ClusterList      clusters;     // 추가
    ClusterList *cluster_history;  // 추가
    
    pthread_t th_loader;
    pthread_t th_even;
    pthread_t th_odd;
    pthread_t th_post;

    const RadarMeta *meta;
} AppState;

static AppState g_state;

// =========================================================
// init - 프로그램 시작 시 한 번만
// =========================================================
static int app_init(const char *dir_path, const RadarMeta *meta,
                    struct dirent **namelist, int num_files) {
    AppState *s = &g_state;
    memset(s, 0, sizeof(*s));

    s->meta = meta;
    
    // 파이프라인 초기화
    if (init_pipeline_pool(meta, &s->pipe) != 0) {
        fprintf(stderr, "init_pipeline failed\n");
        return -1;
    }
    size_t q_size = (meta->num_pulses / 2 + 1) * NUM_BUFFERS;
    // 큐 초기화
    if (pulse_queue_init(&s->pipe.even_q, q_size) != 0 ||
        pulse_queue_init(&s->pipe.odd_q,  q_size) != 0) {
        fprintf(stderr, "pulse_queue_init failed\n");
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (post_queue_init(&s->pipe.post_q, NUM_BUFFERS) != 0) {
        fprintf(stderr, "post_queue_init failed\n");
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    // loader 초기화
    if (loader_thread_init(meta, &s->ld, &s->pipe, 5555) != 0) {
        fprintf(stderr, "loader_thread_init failed\n");
        post_queue_destroy(&s->pipe.post_q);
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    // 펄스 압축 컨텍스트
    if (pulse_compress_ctx_init(meta, &s->wk_even.ctx) != 0 ||
        pulse_compress_ctx_init(meta, &s->wk_odd.ctx)  != 0) {
        fprintf(stderr, "pulse_compress_ctx_init failed\n");
        pulse_compress_ctx_destroy(&s->wk_even.ctx);
        pulse_compress_ctx_destroy(&s->wk_odd.ctx);
        loader_thread_destroy(&s->ld);
        post_queue_destroy(&s->pipe.post_q);
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    // CFAR, Doppler 워크스페이스
    if (init_cfar_workspace(&s->cfar_ws,
                            meta->num_fast_time_samples,
                            meta->num_pulses) != 0) {
        fprintf(stderr, "init_cfar_workspace failed\n");
        pulse_compress_ctx_destroy(&s->wk_even.ctx);
        pulse_compress_ctx_destroy(&s->wk_odd.ctx);
        loader_thread_destroy(&s->ld);
        post_queue_destroy(&s->pipe.post_q);
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (init_doppler_workspace(&s->doppler_ws,
                               meta->num_pulses,
                               meta->num_pulses) != 0) {
        fprintf(stderr, "init_doppler_workspace failed\n");
        cleanup_cfar_workspace(&s->cfar_ws);
        pulse_compress_ctx_destroy(&s->wk_even.ctx);
        pulse_compress_ctx_destroy(&s->wk_odd.ctx);
        loader_thread_destroy(&s->ld);
        post_queue_destroy(&s->pipe.post_q);
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (init_cluster_workspace(&s->cluster_ws,
                            meta->num_fast_time_samples,
                            meta->num_pulses,
                            (float)meta->fs_hz,
                            299792458.0f,
                            (float)meta->prf_hz,
                            (float)meta->fc_hz,
                            meta->num_pulses) != 0) {
        fprintf(stderr, "init_cluster_workspace failed\n");
        cleanup_doppler_workspace(&s->doppler_ws);
        cleanup_cfar_workspace(&s->cfar_ws);
        pulse_compress_ctx_destroy(&s->wk_even.ctx);
        pulse_compress_ctx_destroy(&s->wk_odd.ctx);
        loader_thread_destroy(&s->ld);
        post_queue_destroy(&s->pipe.post_q);
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    s->cluster_history = calloc(num_files, sizeof(ClusterList));
    s->history = calloc(num_files, sizeof(DetectionList));
    s->valid_files = 0;
    s->total_acc = (Accumulator){0};
  
    s->ld.meta      = meta;
    s->ld.pipe      = &s->pipe;
    s->ld.cpu_id    = 0;
    s->ld.timing    = &s->timing;
    s->ld.file_list = namelist;
    s->ld.num_files = num_files;
    s->ld.dir_path  = dir_path;

    s->wk_even.meta    = meta;
    s->wk_even.pipe    = &s->pipe;
    s->wk_even.q       = &s->pipe.even_q;
    s->wk_even.cpu_id  = 1;
    s->wk_even.timing  = &s->timing;

    s->wk_odd.meta     = meta;
    s->wk_odd.pipe     = &s->pipe;
    s->wk_odd.q        = &s->pipe.odd_q;
    s->wk_odd.cpu_id   = 2;
    s->wk_odd.timing   = &s->timing;

    s->cluster_params = (ClusterParams){
        .range_radius   = 2,
        .doppler_radius = 2,
        .min_pts        = 3,
        .max_targets    = 5,
        .power_ratio_min = 0.1f,  // 1위의 10% 미만이면 사이드로브로 제거
    };
    s->post.meta       = meta;
    s->post.pipe       = &s->pipe;
    s->post.det        = &s->det;
    s->post.cfar_ws    = &s->cfar_ws;
    s->post.doppler_ws = &s->doppler_ws;
    s->post.cluster_ws     = &s->cluster_ws;
    s->post.clusters       = &s->clusters;
    s->post.cluster_params = &s->cluster_params;
    s->post.cpu_id     = 3;
    s->post.timing     = &s->timing;
    s->post.total_acc   = &s->total_acc;
    s->post.history     = s->history;
    s->post.valid_files = &s->valid_files;
    s->post.cluster_history = s->cluster_history;

    // UDP
    udp_init("127.0.0.1", 7777);

    // TCP
    send_graph_data_init("127.0.0.1", 9999, meta->num_fast_time_samples, meta->num_pulses);
        
    return 0;
}

// UDP 모드용 init (dir_path, namelist, num_files 없음)
static int app_init_udp(const RadarMeta *meta)
{
    AppState *s = &g_state;
    memset(s, 0, sizeof(*s));
    s->meta = meta;

    if (init_pipeline_pool(meta, &s->pipe) != 0) {
        fprintf(stderr, "init_pipeline failed\n");
        return -1;
    }

    size_t q_size = (meta->num_pulses / 2 + 1) * NUM_BUFFERS;
    if (pulse_queue_init(&s->pipe.even_q, q_size) != 0 ||
        pulse_queue_init(&s->pipe.odd_q,  q_size) != 0) {
        fprintf(stderr, "pulse_queue_init failed\n");
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (post_queue_init(&s->pipe.post_q, NUM_BUFFERS) != 0) {
        fprintf(stderr, "post_queue_init failed\n");
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (loader_thread_init(meta, &s->ld, &s->pipe, 5555) != 0) {
        fprintf(stderr, "loader_thread_init failed\n");
        post_queue_destroy(&s->pipe.post_q);
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (pulse_compress_ctx_init(meta, &s->wk_even.ctx) != 0 ||
        pulse_compress_ctx_init(meta, &s->wk_odd.ctx)  != 0) {
        fprintf(stderr, "pulse_compress_ctx_init failed\n");
        pulse_compress_ctx_destroy(&s->wk_even.ctx);
        pulse_compress_ctx_destroy(&s->wk_odd.ctx);
        loader_thread_destroy(&s->ld);
        post_queue_destroy(&s->pipe.post_q);
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (init_cfar_workspace(&s->cfar_ws,
                            meta->num_fast_time_samples,
                            meta->num_pulses) != 0) {
        fprintf(stderr, "init_cfar_workspace failed\n");
        pulse_compress_ctx_destroy(&s->wk_even.ctx);
        pulse_compress_ctx_destroy(&s->wk_odd.ctx);
        loader_thread_destroy(&s->ld);
        post_queue_destroy(&s->pipe.post_q);
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (init_doppler_workspace(&s->doppler_ws,
                               meta->num_pulses,
                               meta->num_pulses) != 0) {
        fprintf(stderr, "init_doppler_workspace failed\n");
        cleanup_cfar_workspace(&s->cfar_ws);
        pulse_compress_ctx_destroy(&s->wk_even.ctx);
        pulse_compress_ctx_destroy(&s->wk_odd.ctx);
        loader_thread_destroy(&s->ld);
        post_queue_destroy(&s->pipe.post_q);
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (init_cluster_workspace(&s->cluster_ws,
                               meta->num_fast_time_samples,
                               meta->num_pulses,
                               (float)meta->fs_hz,
                               299792458.0f,
                               (float)meta->prf_hz,
                               (float)meta->fc_hz,
                               meta->num_pulses) != 0) {
        fprintf(stderr, "init_cluster_workspace failed\n");
        cleanup_doppler_workspace(&s->doppler_ws);
        cleanup_cfar_workspace(&s->cfar_ws);
        pulse_compress_ctx_destroy(&s->wk_even.ctx);
        pulse_compress_ctx_destroy(&s->wk_odd.ctx);
        loader_thread_destroy(&s->ld);
        post_queue_destroy(&s->pipe.post_q);
        pulse_queue_destroy(&s->pipe.even_q);
        pulse_queue_destroy(&s->pipe.odd_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    // UDP 모드는 프레임 수를 모르므로 최대값으로 잡음
#define MAX_UDP_FRAMES 100000
    s->history         = calloc(MAX_UDP_FRAMES, sizeof(DetectionList));
    s->cluster_history = calloc(MAX_UDP_FRAMES, sizeof(ClusterList));
    s->valid_files = 0;
    s->total_acc   = (Accumulator){0};

    s->ld.meta      = meta;
    s->ld.pipe      = &s->pipe;
    s->ld.cpu_id    = 0;
    s->ld.timing    = &s->timing;
    s->ld.file_list = NULL;
    s->ld.num_files = 0;
    s->ld.dir_path  = NULL;

    s->wk_even.meta   = meta;
    s->wk_even.pipe   = &s->pipe;
    s->wk_even.q      = &s->pipe.even_q;
    s->wk_even.cpu_id = 1;
    s->wk_even.timing = &s->timing;

    s->wk_odd.meta    = meta;
    s->wk_odd.pipe    = &s->pipe;
    s->wk_odd.q       = &s->pipe.odd_q;
    s->wk_odd.cpu_id  = 2;
    s->wk_odd.timing  = &s->timing;

    s->cluster_params = (ClusterParams){
        .range_radius    = 2,
        .doppler_radius  = 2,
        .min_pts         = 3,
        .max_targets     = 5,
        .power_ratio_min = 0.1f,
    };

    s->post.meta            = meta;
    s->post.pipe            = &s->pipe;
    s->post.det             = &s->det;
    s->post.cfar_ws         = &s->cfar_ws;
    s->post.doppler_ws      = &s->doppler_ws;
    s->post.cluster_ws      = &s->cluster_ws;
    s->post.clusters        = &s->clusters;
    s->post.cluster_params  = &s->cluster_params;
    s->post.cpu_id          = 3;
    s->post.timing          = &s->timing;
    s->post.total_acc       = &s->total_acc;
    s->post.history         = s->history;
    s->post.valid_files     = &s->valid_files;
    s->post.cluster_history = s->cluster_history;

    udp_init("192.168.1.22", 7777);
    send_graph_data_init("192.168.1.22", 9999,
        meta->num_fast_time_samples, meta->num_pulses);

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
    cleanup_cluster_workspace(&g_state.cluster_ws);
    free_detection_list(&g_state.det);
}

static int process_udp(const RadarMeta *meta)
{
    AppState *s = &g_state;

    if (app_init_udp(meta) != 0) return -1;

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

    app_cleanup();
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

    struct dirent **namelist;
    int num_files = scandir(dir_path, &namelist, NULL, versionsort);
    if (num_files < 0) {
        perror("scandir failed");
        return -1;
    }
    
    if (app_init(dir_path, &meta, namelist, num_files) != 0) {
        return -1;
    }
    AppState *s = &g_state;

 // 큐 열기
    pulse_queue_open(&s->pipe.even_q);
    pulse_queue_open(&s->pipe.odd_q);
    post_queue_open(&s->pipe.post_q);

    // 스레드 한 번만 생성
    pthread_create(&s->th_loader, NULL, loader_thread_main, &s->ld);
    pthread_create(&s->th_even,   NULL, worker_thread_main, &s->wk_even);
    pthread_create(&s->th_odd,    NULL, worker_thread_main, &s->wk_odd);
    pthread_create(&s->th_post,   NULL, post_thread_main,   &s->post);

    pthread_join(s->th_loader, NULL);
    pthread_join(s->th_even,   NULL);
    pthread_join(s->th_odd,    NULL);
    pthread_join(s->th_post,   NULL);
   
    if (s->valid_files > 0) {
        print_trajectory_summary(s->history, s->cluster_history, s->valid_files);
        print_global_average(&s->total_acc,
                             s->valid_files);
    }

    for (int i = 0; i < s->valid_files; i++)
        free_detection_list(&s->history[i]);
    free(s->history);
    s->history = NULL;

    for (int i = 0; i < num_files; i++)
        free(namelist[i]);
    free(namelist);

    app_cleanup();
    return 0;
}

// =========================================================
// main
// =========================================================
int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    RadarMeta meta = {0};
    if (load_metadata(argv[1], &meta) != 0) {
        fprintf(stderr, "failed to read metadata\n");
        return 1;
    }
    print_metadata(&meta);

    if (argc == 2) {
        printf("UDP 모드: 포트 5555에서 수신 대기\n");
        return process_udp(&meta);
    }

    // 파일 모드
    struct stat st;
    if (stat(argv[2], &st) != 0) {
        perror("stat failed");
        return 1;
    }

    if (S_ISDIR(st.st_mode)) {
        printf("Target is a DIRECTORY. Batch processing...\n");
        return process_directory(argv[2], argv[1]);
    }

    fprintf(stderr, "target must be a directory\n");
    return 1;
}