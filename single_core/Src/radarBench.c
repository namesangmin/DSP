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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "timer.h"
#include "loader.h"
#include "pulse.h"
#include "doppler_fft.h"
#include "cfar.h"
#include "writer.h"

// 궤적 추적을 위한 구조체 선언
typedef struct {
    char filename[256];
    int detected;
    Detection det;
} TrackPoint;

// --------------------------------------------------------------------------------
// CPU0 고정
// --------------------------------------------------------------------------------
static void pin_to_cpu0(void) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    CPU_SET(1, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("sched_setaffinity");
    }
}

// --------------------------------------------------------------------------------
// CPU 사용 틱 읽기 (/proc/self/stat - utime + stime)
// --------------------------------------------------------------------------------
static long read_cpu_ticks(void) {
    FILE *f = fopen("/proc/self/stat", "r");
    if (!f) return 0;

    // /proc/self/stat 필드: pid(1) comm(2) state(3) ppid(4) pgrp(5)
    //   session(6) tty_nr(7) tpgid(8) flags(9) minflt(10) cminflt(11)
    //   majflt(12) cmajflt(13) utime(14) stime(15)
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
        "Usage (File/Dir): %s <metadata.csv> <target_path> <imag_path_or_DUMMY> [runs]\n", prog);
}

static void print_metadata(const RadarMeta *meta) {
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

static void print_average_line(const char *name, double avg_ms) {
    printf("  %-18s = %.3f ms (%.9f sec)\n", name, avg_ms, avg_ms / 1000.0);
}

// --------------------------------------------------------------------------------
// 단일 파일 처리
// --------------------------------------------------------------------------------
int process_single_file(const char *metadata_path, const char *real_path, const char *imag_path, int runs, Detection *out_best) {
    int config_printed = 0;
    DetectionList final_det = {0};

    double sum_load_ms = 0.0, sum_pulse_ready_ms = 0.0, sum_pulse_apply_ms = 0.0, sum_pulse_total_ms = 0.0;
    double sum_mti_ms = 0.0, sum_mtd_ms = 0.0, sum_doppler_total_ms = 0.0;
    double sum_cfar_ms = 0.0, sum_total_ms = 0.0, sum_detections = 0.0;

    // --- CPU 사용량 측정 시작 ---
    long   ticks_before = read_cpu_ticks();
    double wall_before  = now_ms();

    for (int run = 1; run <= runs; ++run) {
        RadarMeta meta = {0};
        ComplexMatrix rx = {0}, pc = {0}, doppler = {0};
        DetectionList det = {0};
        PulseTiming pulse_timing = {0};
        DopplerFftTiming doppler_timing = {0};
        double total_t0 = now_ms(), t0;
        double load_ms = 0.0, pulse_total_ms = 0.0, doppler_total_ms = 0.0, cfar_ms = 0.0, total_ms = 0.0;

        t0 = now_ms();
        if (load_metadata(metadata_path, &meta) != 0) return 1;

        if (ends_with(real_path, ".bin") && ends_with(imag_path, ".bin")) {
            if (load_complex_bin_pair_matlab(real_path, imag_path, meta.num_fast_time_samples, meta.num_pulses, &rx) != 0) return 1;
        } else if (ends_with(real_path, ".dat")) {
            if (load_complex_bin_single(real_path, meta.num_pulses, meta.num_fast_time_samples, &rx) != 0) return 1;
        } else {
            if (load_complex_csv_pair(real_path, imag_path, meta.num_fast_time_samples, meta.num_pulses, &rx) != 0) return 1;
        }
        load_ms = now_ms() - t0;

        if (!config_printed && run == 1) {
            print_metadata(&meta);
            config_printed = 1;
        }

        if (pulse_compression_ex(&rx, &meta, &pc, &pulse_timing) != 0) {
            free_complex_matrix(&rx); return 1;
        }
        pulse_total_ms = pulse_timing.filter_ready_ms + pulse_timing.compression_ms;

        if (doppler_fft_processing_ex(&pc, &meta, meta.num_pulses, &doppler, &doppler_timing) != 0) {
            free_complex_matrix(&rx); free_complex_matrix(&pc); return 1;
        }
        doppler_total_ms = doppler_timing.mti_ms + doppler_timing.mtd_ms;

        t0 = now_ms();
        int numTrainR = 4, numTrainD = 4, numGuardR = 1, numGuardD = 1;
        int totalWindowCells = (2 * (numTrainR + numGuardR) + 1) * (2 * (numTrainD + numGuardD) + 1);
        int guardAndCUTCells = (2 * numGuardR + 1) * (2 * numGuardD + 1);
        int rankIdx = ((totalWindowCells - guardAndCUTCells) + 1) / 2;

        if (cfar_detect(&doppler, &meta, numTrainR, numTrainD, numGuardR, numGuardD, rankIdx, 9.0, &det) != 0) {
            free_complex_matrix(&rx); free_complex_matrix(&pc); free_complex_matrix(&doppler); return 1;
        }
        cfar_ms = now_ms() - t0;
        total_ms = now_ms() - total_t0;

        printf("  [run %d] load=%.3fms total=%.3fms, detections=%d\n", run, load_ms, total_ms, det.count);

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
        free_complex_matrix(&rx); free_complex_matrix(&pc); free_complex_matrix(&doppler);
    }

    // --- CPU 사용량 측정 종료 ---
    long   ticks_after = read_cpu_ticks();
    double wall_after  = now_ms();

    long   hz        = sysconf(_SC_CLK_TCK);           // 보통 100 ticks/sec
    double cpu_sec   = (double)(ticks_after - ticks_before) / (double)hz;
    double wall_sec  = (wall_after - wall_before) / 1000.0;
    double cores_used = (wall_sec > 0.0) ? (cpu_sec / wall_sec) : 0.0;

    // 파일 1개당 가장 강한 표적 찾아서 반환
    if (final_det.count > 0) {
        Detection best_det = final_det.items[0];
        for (int i = 1; i < final_det.count; i++) {
            if (final_det.items[i].power > best_det.power)
                best_det = final_det.items[i];
        }
        if (out_best) *out_best = best_det;

        printf("\nStrongest detection after CA-CFAR:\n");
        printf("  Range bin    = %d\n  Doppler bin  = %d\n", best_det.range_bin, best_det.doppler_bin);
        printf("  Range        = %.2f m\n  Radial vel.  = %.2f m/s\n", best_det.range_m, best_det.velocity_mps);
        printf("  Power        = %.6e\n  Threshold    = %.6e\n", best_det.power, best_det.threshold);
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

    // --- CPU 사용량 출력 ---
    printf("\n--- CPU Core Usage (all %d runs) ---\n", runs);
    printf("  Wall time   = %.3f sec\n", wall_sec);
    printf("  CPU time    = %.3f sec\n", cpu_sec);
    printf("  Cores used  = %.2f\n", cores_used);

    free_detection_list(&final_det);
    return 0;
}

// --------------------------------------------------------------------------------
// 폴더 순회 및 궤적 요약
// --------------------------------------------------------------------------------
void process_directory(const char *dir_path, const char *metadata_path, int runs) {
    struct dirent **namelist;

    int num_files = scandir(dir_path, &namelist, NULL, versionsort);
    if (num_files < 0) {
        perror("scandir failed");
        return;
    }

    TrackPoint *history = calloc(num_files, sizeof(TrackPoint));
    int valid_files = 0;

    printf("\nScanning directory sequentially: %s\n", dir_path);

    for (int i = 0; i < num_files; i++) {
        char *filename = namelist[i]->d_name;

        if (filename[0] == '.') {
            free(namelist[i]);
            continue;
        }

        char filepath[1024];
        Detection best_det;
        int processed = 0;

        if (ends_with(filename, ".dat")) {
            snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, filename);
            struct stat st;
            if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
                printf("\n=========================================================\n");
                printf("[FILE %d] Processing Integrated DAT: %s\n", valid_files + 1, filename);
                printf("=========================================================\n");
                process_single_file(metadata_path, filepath, "DUMMY", runs, &best_det);
                processed = 1;
            }
        } else if (strstr(filename, "real") != NULL &&
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
                process_single_file(metadata_path, real_file, imag_file, runs, &best_det);
                processed = 1;
            }
        }

        if (processed) {
            strncpy(history[valid_files].filename, filename, 255);
            history[valid_files].filename[255] = '\0';
            history[valid_files].detected = (best_det.range_bin != -1);
            history[valid_files].det = best_det;
            valid_files++;
        }
        free(namelist[i]);
    }
    free(namelist);

    if (valid_files > 0) {
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
    }

    free(history);
}

// --------------------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------------------
int main(int argc, char **argv) {
    pin_to_cpu0();

    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    const char *metadata_path = argv[1];
    const char *target_path   = argv[2];
    const char *imag_path     = argv[3];
    int runs = (argc >= 5) ? atoi(argv[4]) : 1;
    if (runs <= 0) runs = 1;

    struct stat st;
    if (stat(target_path, &st) != 0) {
        perror("stat failed (check if path exists)");
        return 1;
    }

    if (S_ISDIR(st.st_mode)) {
        printf("Target is a DIRECTORY. Batch sequential processing initiated...\n");
        process_directory(target_path, metadata_path, runs);
    } else if (S_ISREG(st.st_mode)) {
        printf("Target is a FILE.\n");
        process_single_file(metadata_path, target_path, imag_path, runs, NULL);
    }

    return 0;
}