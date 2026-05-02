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

// 궤적 요약 테이블 + trajectory analysis
void print_trajectory_summary(DetectionList *history, int valid_files) {
    printf("\n\n#########################################################\n");
    printf("                TARGET TRAJECTORY SUMMARY                \n");
    printf("#########################################################\n");
    printf("Total Files Processed: %d\n\n", valid_files);

    printf("%-20s | %-12s | %-12s | %-15s\n",
           "Filename", "Range (m)", "Velocity (m/s)", "Power");
    printf("-----------------------------------------------------------------\n");

    int first_idx = -1, last_idx = -1;

    for (int i = 0; i < valid_files; i++) {
        if (history[i].count > 0) {
            if (first_idx == -1) first_idx = i;
            last_idx = i;
            printf("%-20s | %-12.2f | %-12.2f | %-15.2e\n",
                   history[i].filename,
                   history[i].items[0].range_m,
                   history[i].items[0].velocity_mps,
                   history[i].items[0].power);
        } else {
            printf("%-20s | %-12s | %-12s | %-15s\n",
                   history[i].filename, "N/A", "N/A", "N/A");
        }
    }

    if (first_idx != -1 && last_idx != -1 && first_idx != last_idx) {
        double start_range = history[first_idx].items[0].range_m;
        double end_range   = history[last_idx].items[0].range_m;
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
            printf("         GLOBAL DIRECTORY AVERAGE (%d Files, excluding FILE 1)        \n", timing_files);
            printf("#########################################################\n");
            print_average_line("load",          acc->load_ms          / timing_files);
            print_average_line("pulse_total",   acc->pulse_total_ms   / timing_files);
            print_average_line("transpose",     acc->transpose_ms          / timing_files);
            print_average_line("mti",           acc->mti_ms           / timing_files);
            print_average_line("mtd",           acc->mtd_ms           / timing_files);
            //print_average_line("doppler_total", acc->doppler_total_ms / timing_files);
            print_average_line("cfar",          acc->cfar_ms          / timing_files);

            print_average_line("total time",    acc->total_time_ms    / timing_files);
            print_average_line("algo_only",     acc->algo_only_ms     / timing_files);
            printf("  %-18s = %.2f\n", "detections", (double)acc->detections / timing_files);
            printf("#########################################################\n\n");
        }
}

