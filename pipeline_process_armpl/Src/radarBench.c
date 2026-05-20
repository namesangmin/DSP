#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <dirent.h>
#include <pthread.h>
#include <complex.h>
#include <signal.h>  
#include <sys/types.h>
#include <sys/stat.h>

#include "timer.h"
#include "loader.h"
#include "pulse.h"
#include "doppler_fft.h"
#include "cfar.h"
#include "common.h"
#include "queue_interface.h"
#include "core_set.h"
#include "pipeline_set.h"
#include "doppler_cfar_thread.h"
#include "loader_thread.h"
#include "pulse_compress_thread.h"
#include "print.h"
#include "cluster.h"
#include "udp.h"
#include "send_graph_data.h"
#include "transpose_interface.h"

#define MAX_UDP_FRAMES 100000

typedef struct {
    int valid_files;
    Pipeline        pipe;

    LoaderArgs      ld;
    WorkerArgs      wk[2];      
    PostArgs        post;

    CfarWorkspace    cfar_ws;
    DopplerWorkspace doppler_ws;
    DetectionList    det;

    PipelineTiming  timing;
    Accumulator     total_acc;
    DetectionList   *history;

    ClusterWorkspace cluster_ws;
    ClusterParams    cluster_params;
    ClusterList      clusters;
    ClusterList     *cluster_history;

    pthread_t th_loader;
    pthread_t th_worker[2];
    pthread_t th_post;

    const RadarMeta *meta;
} AppState;

static AppState g_state;

void handle_sigint(int sig) {
    print_global_average(&g_state.total_acc, g_state.valid_files);
    exit(0);
}

static int app_init_udp(const RadarMeta *meta, QueueType qt, TransposeType tt)
{
    AppState *s = &g_state;
    memset(s, 0, sizeof(*s));
    s->meta = meta;

    if (init_pipeline_pool(meta, &s->pipe) != 0)
    {
        fprintf(stderr, "init_pipeline failed\n");
        return -1;
    }

    size_t q_size = (meta->num_pulses / 2 + 1) * NUM_BUFFERS;

    s->pipe.worker_q[0] = pulse_queue_create(qt, q_size);
    s->pipe.worker_q[1] = pulse_queue_create(qt, q_size);
    s->pipe.post_q      = post_queue_create (qt, NUM_BUFFERS);

    if (!s->pipe.worker_q[0] || !s->pipe.worker_q[1] || !s->pipe.post_q) {
        fprintf(stderr, "queue_create failed\n");
        pulse_queue_destroy(s->pipe.worker_q[0]);
        pulse_queue_destroy(s->pipe.worker_q[1]);
        post_queue_destroy (s->pipe.post_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (loader_thread_init(meta, &s->ld, &s->pipe, 9000) != 0)
    {
        fprintf(stderr, "loader_thread_init failed\n");
        pulse_queue_destroy(s->pipe.worker_q[0]);
        pulse_queue_destroy(s->pipe.worker_q[1]);
        post_queue_destroy (s->pipe.post_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (pulse_compress_ctx_init(meta, &s->wk[0].ctx) != 0 ||
        pulse_compress_ctx_init(meta, &s->wk[1].ctx) != 0)
    {
        fprintf(stderr, "pulse_compress_ctx_init failed\n");
        pulse_compress_ctx_destroy(&s->wk[0].ctx);
        pulse_compress_ctx_destroy(&s->wk[1].ctx);
        loader_thread_destroy(&s->ld);
        pulse_queue_destroy(s->pipe.worker_q[0]);
        pulse_queue_destroy(s->pipe.worker_q[1]);
        post_queue_destroy (s->pipe.post_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (init_cfar_workspace(&s->cfar_ws,
        meta->num_fast_time_samples,
        meta->num_pulses) != 0)
    {
        fprintf(stderr, "init_cfar_workspace failed\n");
        pulse_compress_ctx_destroy(&s->wk[0].ctx);
        pulse_compress_ctx_destroy(&s->wk[1].ctx);
        loader_thread_destroy(&s->ld);
        pulse_queue_destroy(s->pipe.worker_q[0]);
        pulse_queue_destroy(s->pipe.worker_q[1]);
        post_queue_destroy (s->pipe.post_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    if (init_doppler_workspace(&s->doppler_ws,
        meta->num_pulses,
        meta->num_pulses) != 0)
    {
        fprintf(stderr, "init_doppler_workspace failed\n");
        cleanup_cfar_workspace(&s->cfar_ws);
        pulse_compress_ctx_destroy(&s->wk[0].ctx);
        pulse_compress_ctx_destroy(&s->wk[1].ctx);
        loader_thread_destroy(&s->ld);
        pulse_queue_destroy(s->pipe.worker_q[0]);
        pulse_queue_destroy(s->pipe.worker_q[1]);
        post_queue_destroy (s->pipe.post_q);
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
        meta->num_pulses) != 0)
    {
        fprintf(stderr, "init_cluster_workspace failed\n");
        cleanup_doppler_workspace(&s->doppler_ws);
        cleanup_cfar_workspace(&s->cfar_ws);
        pulse_compress_ctx_destroy(&s->wk[0].ctx);
        pulse_compress_ctx_destroy(&s->wk[1].ctx);
        loader_thread_destroy(&s->ld);
        pulse_queue_destroy(s->pipe.worker_q[0]);
        pulse_queue_destroy(s->pipe.worker_q[1]);
        post_queue_destroy (s->pipe.post_q);
        cleanup_pipeline_pool(&s->pipe);
        return -1;
    }

    s->history         = calloc(MAX_UDP_FRAMES, sizeof(DetectionList));
    s->cluster_history = calloc(MAX_UDP_FRAMES, sizeof(ClusterList));
    s->valid_files = 0;
    s->total_acc   = (Accumulator){0};

    s->ld.meta      = meta;
    s->ld.pipe      = &s->pipe;
    s->ld.cpu_id    = 0;
    s->ld.timing    = &s->timing;

    s->wk[0].meta   = meta;
    s->wk[0].pipe   = &s->pipe;
    s->wk[0].q      = s->pipe.worker_q[0];
    s->wk[0].cpu_id = 1;
    s->wk[0].tid    = 0;

    s->wk[1].meta   = meta;
    s->wk[1].pipe   = &s->pipe;
    s->wk[1].q      = s->pipe.worker_q[1];
    s->wk[1].cpu_id = 2;
    s->wk[1].tid    = 1;

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
    s->post.valid_files     = &s->valid_files;
    s->post.transpose = transpose_create(tt);

    udp_init("192.168.10.4", 7777);
    send_graph_data_init("192.168.10.4", 9999, meta->num_fast_time_samples, meta->num_pulses);

    return 0;
}

static void app_cleanup(void)
{
    cleanup_pipeline_pool(&g_state.pipe);
    pulse_queue_destroy(g_state.pipe.worker_q[0]);
    pulse_queue_destroy(g_state.pipe.worker_q[1]);
    post_queue_destroy (g_state.pipe.post_q);
    loader_thread_destroy(&g_state.ld);
    pulse_compress_ctx_destroy(&g_state.wk[0].ctx);
    pulse_compress_ctx_destroy(&g_state.wk[1].ctx);
    cleanup_cfar_workspace(&g_state.cfar_ws);
    cleanup_doppler_workspace(&g_state.doppler_ws);
    cleanup_cluster_workspace(&g_state.cluster_ws);
    free_detection_list(&g_state.det);

    transpose_destroy(g_state.post.transpose);

}

static int process_udp(const RadarMeta *meta, QueueType qt, TransposeType tt)
{
    AppState *s = &g_state;

    if (app_init_udp(meta, qt, tt) != 0) return -1;

    queue_open_pulse(s->pipe.worker_q[0]);
    queue_open_pulse(s->pipe.worker_q[1]);
    queue_open_post (s->pipe.post_q);

    pthread_create(&s->th_loader,    NULL, loader_thread_main, &s->ld);
    pthread_create(&s->th_worker[0], NULL, worker_thread_main, &s->wk[0]);
    pthread_create(&s->th_worker[1], NULL, worker_thread_main, &s->wk[1]);
    pthread_create(&s->th_post,      NULL, post_thread_main,   &s->post);

    pthread_join(s->th_loader,    NULL);
    pthread_join(s->th_worker[0], NULL);
    pthread_join(s->th_worker[1], NULL);
    pthread_join(s->th_post,      NULL);

    app_cleanup();
    return 0;
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_sigint);

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    QueueType qt = QUEUE_FUTEX; 
    TransposeType tt = TRANSPOSE_TILING;

    for (int i = 2; i < argc; i++) 
    {
        if (!strcmp(argv[i], "--queue") && i+1 < argc)
            qt = queue_type_from_str(argv[++i]);
        else if (!strcmp(argv[i], "--transpose") && i+1 < argc)
            tt = transpose_type_from_str(argv[++i]);
    }

    RadarMeta meta = {0};
    if (load_metadata(argv[1], &meta) != 0) {
        fprintf(stderr, "failed to read metadata\n");
        return 1;
    }
    print_metadata(&meta);

    if (argc >= 2) {
        printf("UDP 모드: 포트 9000에서 수신 대기\n");
        return process_udp(&meta, qt, tt);
    }

    fprintf(stderr, "target must be a directory\n");
    return 1;
}