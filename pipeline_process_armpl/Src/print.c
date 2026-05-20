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
        "Usage: %s <metadata.csv> [--queue <type>]\n"
        "\n"
        "  --queue <type>   큐 구현체 선택 (기본값: futex)\n"
        "                   type: futex | lockfree | mutex | usleep\n"
        "\n"
        "Example:\n"
        "  %s meta.csv\n"
        "  %s meta.csv --queue mutex\n"
        "  %s meta.csv --queue lockfree\n",
        prog, prog, prog, prog);
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
    const DetectionList  *det)
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
}