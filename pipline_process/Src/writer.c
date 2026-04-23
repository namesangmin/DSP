#include <stdio.h>
#include "writer.h"

int init_timing_csv(const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp,
            "run,load_ms,pulse_ready_ms,pulse_apply_ms,pulse_total_ms,"
            "mti_ms,mtd_ms,doppler_total_ms,cfar_ms,total_ms,detections\n");

    fclose(fp);
    return 0;
}

int append_timing_csv(const char *path, const TimingRecord *rec) {
    FILE *fp = fopen(path, "a");
    if (!fp) return -1;

    fprintf(fp,
            "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d\n",
            rec->run,
            rec->load_ms,
            rec->pulse_ready_ms,
            rec->pulse_apply_ms,
            rec->pulse_total_ms,
            rec->mti_ms,
            rec->mtd_ms,
            rec->doppler_total_ms,
            rec->cfar_ms,
            rec->total_ms,
            rec->detections);

    fclose(fp);
    return 0;
}

int write_detections_csv(const char *path, const DetectionList *list) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "range_bin,doppler_bin,power,threshold,range_m,velocity_mps\n");
    for (int i = 0; i < list->count; ++i) {
        const Detection *d = &list->items[i];
        fprintf(fp, "%d,%d,%.12e,%.12e,%.10f,%.10f\n",
                d->range_bin,
                d->doppler_bin,
                d->power,
                d->threshold,
                d->range_m,
                d->velocity_mps);
    }

    fclose(fp);
    return 0;
}