#include <stdio.h>
#include <math.h>
#include "print.h"

void print_metadata(const RadarMeta *meta) {
    const double c = 299792458.0;
    double lambda = c / meta->fc_hz;
    int pulse_samples = (int)llround(meta->fs_hz * meta->pulse_width_s);

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

void print_average_line(const char *name, double avg_ms) {
    printf("  %-18s = %.3f ms (%.9f sec)\n", name, avg_ms, avg_ms / 1000.0);
}

void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <metadata.csv> [options]\n"
        "\n"
        "Options:\n"
        "  --queue <type>      큐 구현체 선택        (기본값: futex)\n"
        "                      futex | mutex | lockfree | usleep\n"
        "\n"
        "  --transpose <type>  전치 방식 선택        (기본값: tiling)\n"
        "                      tiling | comatcopy\n"
        "\n"
        "  --dispatch <type>   펄스 분배 방식 선택   (기본값: half)\n"
        "                      half | evenodd\n"
        "\n"
        "  --workers <n>       worker 스레드 수      (기본값: 2)\n"
        "                      1 | 2\n"
        "\n"
        "  --layout <type>     메모리 레이아웃 선택  (기본값: default)\n"
        "                      default | legacy\n"
        "\n"
        "Example:\n"
        "  %s meta.csv\n"
        "  %s meta.csv --queue mutex\n"
        "  %s meta.csv --queue futex --transpose comatcopy\n"
        "  %s meta.csv --queue futex --dispatch evenodd --workers 1\n"
        "  %s meta.csv --layout legacy\n",
        prog, prog, prog, prog, prog, prog);
}

void print_trajectory_summary(DetectionList *history,
                               ClusterList   *cluster_history,
                               int            valid_files)
{
    printf("\n\n#########################################################\n");
    printf("                TARGET TRAJECTORY SUMMARY                \n");
    printf("#########################################################\n");
    printf("Total Files Processed: %d\n\n", valid_files);

    // CFAR raw detection 수 요약
    printf("%-20s | %-12s | %-14s | %-12s | %-6s\n",
           "Filename", "Range (m)", "Velocity (m/s)", "Power", "Cluster");
    printf("--------------------------------------------------------------------\n");

    int first_idx = -1, last_idx = -1;

    for (int i = 0; i < valid_files; i++) 
    {
        if (cluster_history[i].count > 0) 
        {
            if (first_idx == -1) first_idx = i;
            last_idx = i;
            ClusterResult *best = &cluster_history[i].items[0]; // 이미 power 내림차순 정렬됨
            printf("%-20s | %-12.2f | %-14.2f | %-12.6e | %-6d\n",
                   history[i].filename,   // 파일명은 DetectionList에 있으니 유지
                   best->range_m,
                   best->velocity_mps,
                   best->peak_power,
                   cluster_history[i].count);
        } 
        else
        {
            printf("%-20s | %-12s | %-14s | %-12s | %-6d\n", history[i].filename, "N/A", "N/A", "N/A", 0);
        }
    }

    if (first_idx != -1 && last_idx != -1 && first_idx != last_idx) 
    {
        double start_range = cluster_history[first_idx].items[0].range_m;
        double end_range   = cluster_history[last_idx].items[0].range_m;
        double diff        = end_range - start_range;

        printf("\n--- Trajectory Analysis ---\n");
        printf("Initial Position : %.2f m (File: %s)\n",
               start_range, history[first_idx].filename);
        printf("Final Position   : %.2f m (File: %s)\n",
               end_range,   history[last_idx].filename);
        printf("Displacement     : %.2f m %s\n",
               diff, diff < 0 ? "(Moving Toward)" : "(Moving Away)");
    }
}

// 타이밍 평균
void print_global_average(const Accumulator *acc, int timing_files){
        if (timing_files > 0) {
            printf("\n\n#########################################################\n");
            printf("         GLOBAL DIRECTORY AVERAGE (%d Files)        \n", timing_files);
            printf("#########################################################\n");
            print_average_line("load",          acc->load_ms          / timing_files);
            print_average_line("pulse_total",   acc->pulse_total_ms   / timing_files);
            print_average_line("transpose",     acc->transpose_ms          / timing_files);
            print_average_line("mti",           acc->mti_ms           / timing_files);
            print_average_line("mtd",           acc->mtd_ms           / timing_files);
            print_average_line("cfar",          acc->cfar_ms          / timing_files);
            print_average_line("cluster",          acc->cluster_ms    / timing_files);
            print_average_line("total time",    acc->total_time_ms    / timing_files);
            print_average_line("algo_only",     acc->algo_only_ms     / timing_files);
            printf("  %-18s = %.2f\n", "detections", (double)acc->detections / timing_files);
            printf("#########################################################\n\n");
        }
}

void accumulate_result(
    Accumulator          *acc,
    const PipelineTiming *timing,
    const DetectionList  *det,
    Detection            *out_best)
{
    double doppler_total_ms = timing->mtd_ms + timing->mti_ms;
    double pulse_total_ms   = timing->compress_ms;
    double algo_only_ms     = pulse_total_ms + doppler_total_ms
                            + timing->cfar_ms + timing->transpose_ms + timing->cluster_ms;
    double total_ms = algo_only_ms + timing->loader_ms;

    acc->load_ms          += timing->loader_ms;
    acc->pulse_total_ms   += pulse_total_ms;
    acc->mti_ms           += timing->mti_ms;
    acc->mtd_ms           += timing->mtd_ms;
    acc->doppler_total_ms += doppler_total_ms;
    acc->cfar_ms          += timing->cfar_ms;
    acc->transpose_ms     += timing->transpose_ms;
    acc->total_time_ms    += total_ms;
    acc->algo_only_ms     += algo_only_ms;
    acc->detections       += det->count;
    acc->cluster_ms += timing->cluster_ms;

    if (out_best) 
    {
        out_best->range_bin = -1;
        if (det->count > 0) 
        {
            *out_best = det->items[0];
            for (int i = 1; i < det->count; i++) 
            {
                if (det->items[i].power > out_best->power)
                    *out_best = det->items[i];
            }
        }
    }
}