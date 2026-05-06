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

long read_cpu_ticks(void) {
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

void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage (File/Dir): %s <metadata.csv> <target_path> <imag_path_or_DUMMY> [runs]\n", prog);
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

    for (int i = 0; i < valid_files; i++) {
        if (cluster_history[i].count > 0) {
            if (first_idx == -1) first_idx = i;
            last_idx = i;
            ClusterResult *best = &cluster_history[i].items[0]; // 이미 power 내림차순 정렬됨
            printf("%-20s | %-12.2f | %-14.2f | %-12.6e | %-6d\n",
                   history[i].filename,   // 파일명은 DetectionList에 있으니 유지
                   best->range_m,
                   best->velocity_mps,
                   best->peak_power,
                   cluster_history[i].count);
        } else {
            printf("%-20s | %-12s | %-14s | %-12s | %-6d\n",
                   history[i].filename, "N/A", "N/A", "N/A", 0);
        }
    }

    if (first_idx != -1 && last_idx != -1 && first_idx != last_idx) {
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

// print.c에 구현
void print_file_result(
    const PipelineTiming *timing,
    const DetectionList  *det,
    const ClusterList    *clusters,   // 추가
    int                   file_num)
{
    double doppler_total_ms = timing->mtd_ms + timing->mti_ms;
    double pulse_total_ms   = timing->compress_ms;
    double algo_only_ms     = pulse_total_ms + doppler_total_ms
                            + timing->cfar_ms + timing->transpose_ms + timing->cluster_ms;
    double total_ms = algo_only_ms + timing->loader_ms;

    printf("\n--- Timing ---\n");
    print_average_line("load",      timing->loader_ms);
    
    print_average_line("compress (Core 1)",  timing->compress_core1_ms);
    print_average_line("compress (Core 2)",  timing->compress_core2_ms);

    print_average_line("compress",  pulse_total_ms);
    print_average_line("transpose", timing->transpose_ms);
    print_average_line("mti",       timing->mti_ms);
    print_average_line("mtd",       timing->mtd_ms);
    print_average_line("cfar",      timing->cfar_ms);
    print_average_line("cluster",      timing->cluster_ms);
    print_average_line("total",     total_ms);
    print_average_line("algo_only", algo_only_ms);
    printf("  %-18s = %d\n", "detections", det->count);

    if (det->count > 0) {
        Detection best = det->items[0];
        for (int i = 1; i < det->count; i++) {
            if (det->items[i].power > best.power)
                best = det->items[i];
        }
        printf("\nStrongest detection:\n");
        printf("  Range bin   = %d\n  Doppler bin = %d\n",
               best.range_bin, best.doppler_bin);
        printf("  Range       = %.2f m\n  Velocity    = %.2f m/s\n",
               best.range_m, best.velocity_mps);
        printf("  Power       = %.6e\n  Threshold   = %.6e\n",
               best.power, best.threshold);
    } 
    else {
        printf("\nNo CFAR detection found.\n");
    }

    // cfar detection 출력 아래에
    printf("\n--- Clustering Result ---\n");
    if (clusters->count == 0) {
        printf("  No clusters found.\n");
    } 
    else {
        printf("  %-4s %-10s %-12s %-14s %-14s %-12s %-6s\n",
            "Rank", "R_bin", "D_bin", "Range (m)", "Velocity (m/s)", "Power", "Size");
        for (int i = 0; i < clusters->count; i++) {
            printf("  %-4d %-10d %-12d %-14.2f %-14.2f %-12.6e %-6d\n",
                i + 1,
                clusters->items[i].peak_r,      // range bin
                clusters->items[i].peak_d,      // doppler bin
                clusters->items[i].range_m,
                clusters->items[i].velocity_mps,
                clusters->items[i].peak_power,
                clusters->items[i].size);
        }
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

    if (out_best) {
        out_best->range_bin = -1;
        if (det->count > 0) {
            *out_best = det->items[0];
            for (int i = 1; i < det->count; i++) {
                if (det->items[i].power > out_best->power)
                    *out_best = det->items[i];
            }
        }
    }
}