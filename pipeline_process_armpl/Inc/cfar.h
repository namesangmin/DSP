#ifndef CFAR_H
#define CFAR_H

<<<<<<< Updated upstream
#include "loader.h"

=======
#include "types.h"
#include <stdint.h> // uint8_t 사용을 위해 필요
>>>>>>> Stashed changes
typedef struct {
    int range_bin;
    int doppler_bin;
    double power;
    double threshold;
    double range_m;
    double velocity_mps;
} Detection;

typedef struct {
<<<<<<< Updated upstream
=======
    char filename[256];
>>>>>>> Stashed changes
    int count;
    Detection *items;
} DetectionList;

<<<<<<< Updated upstream
=======
typedef struct {
    int numRange;
    int numDoppler;
    int detCapacity;
    
    float *powerMap;
    float *col_sum_outer;
    float *col_sum_guard;
    
    uint8_t *det_mask; 
    Detection *detBuf;
} CfarWorkspace;

int init_cfar_workspace(CfarWorkspace *ws, int numRange, int numDoppler);
void cleanup_cfar_workspace(CfarWorkspace *ws);

>>>>>>> Stashed changes
int cfar_detect(const ComplexMatrix *doppler_map,
                const RadarMeta *meta,
                int numTrainR, int numTrainD,
                int numGuardR, int numGuardD,
                int rankIdx, double scale,
                DetectionList *out);

void free_detection_list(DetectionList *list);

#endif