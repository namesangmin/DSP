#ifndef CFAR_H
#define CFAR_H

#include "loader.h"
#include "common.h"
#include <stdint.h> // uint8_t 사용을 위해 필요
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
    int detCapacity;
    
    float *powerMap;
    float *col_sum_outer;
    float *col_sum_guard;
    
    // 상대방의 비기: 탐지 여부만 0과 1로 기록할 1D 마스크 배열 추가!
    uint8_t *det_mask; 
    
    Detection *detBuf;
} CfarWorkspace;

int init_cfar_workspace(CfarWorkspace *ws, int numRange, int numDoppler);
void cleanup_cfar_workspace(CfarWorkspace *ws);

int cfar_detect(const ComplexMatrix *doppler_map,
                const RadarMeta *meta,
                CfarWorkspace *ws,
                DetectionList *out);

void free_detection_list(DetectionList *list);

#endif