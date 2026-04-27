#ifndef CFAR_H
#define CFAR_H

#include "loader.h"
#include "common.h"

typedef struct {
    int range_bin;
    int doppler_bin;
    double power;
    double threshold;
    double range_m;
    double velocity_mps;
} Detection;

typedef struct {
    char filename[256];
    int detected;
    Detection det;
} TrackPoint;

typedef struct {
    int count;
    Detection *items;
} DetectionList;

typedef struct {
    int numRange;
    int numDoppler;

    int ii_rows;
    int ii_cols;
    float *powerMap;
    float *ii;

    Detection *detBuf;
    int detCapacity;
} CfarWorkspace;
int init_cfar_workspace(CfarWorkspace *ws, int numRange, int numDoppler);
void cleanup_cfar_workspace(CfarWorkspace *ws);

int cfar_detect(const ComplexMatrix *doppler_map,
                const RadarMeta *meta,
                int numTrainR, int numTrainD,
                int numGuardR, int numGuardD,
                int rankIdx, float scale,
                CfarWorkspace *ws,
                DetectionList *out);

void free_detection_list(DetectionList *list);

#endif