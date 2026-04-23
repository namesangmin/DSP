#ifndef CFAR_H
#define CFAR_H

#include "loader.h"

typedef struct {
    int range_bin;
    int doppler_bin;
    double power;
    double threshold;
    double range_m;
    double velocity_mps;
} Detection;

typedef struct {
    int count;
    Detection *items;
} DetectionList;

int cfar_detect(const ComplexMatrix *doppler_map,
                const RadarMeta *meta,
                int numTrainR, int numTrainD,
                int numGuardR, int numGuardD,
                int rankIdx, double scale,
                DetectionList *out);

void free_detection_list(DetectionList *list);

#endif