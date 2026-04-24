#ifndef WRITER_H
#define WRITER_H

#include "cfar.h"

typedef struct {
    int run;
    double load_ms;
    double pulse_ready_ms;
    double pulse_apply_ms;
    double pulse_total_ms;
    double mti_ms;
    double mtd_ms;
    double doppler_total_ms;
    double cfar_ms;
    double total_ms;
    int detections;
} TimingRecord;

int init_timing_csv(const char *path);
int append_timing_csv(const char *path, const TimingRecord *rec);
int write_detections_csv(const char *path, const DetectionList *list);

#endif