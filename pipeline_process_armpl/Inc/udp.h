#ifndef UDP_H
#define UDP_H

#include <stdint.h>
#include "common.h"
#include "cluster.h"

#define MAX_TARGETS (32)   // 추가

// =========================================================
// 패킷 구조
// =========================================================
// [Header]
//   dwell_id     uint32  4
//   target_num   uint32  4
// [Per target * target_num]
//   range_m      float   4
//   velocity_mps float   4
//   peak_power   float   4
// =========================================================

typedef struct {
    uint32_t dwell_id;
    uint32_t target_num;
    float theta;
    float phi;
    double compress_ms;
    double transpose_ms;
    double mti_ms;
    double mtd_ms;
    double cfar_ms;
    double cluster_ms;
} udp_header_t;

typedef struct {
    float distance;
    float speed;
    //float peak_power;
} udp_target_t;

// =========================================================
// API
// =========================================================
int  udp_init(const char *dst_ip, uint16_t dst_port);
int  udp_loop(uint32_t dwell_id,
            uint32_t target_num,
            const ClusterResult *targets,
            float angle,    
            PipelineTiming *timing);
void udp_destroy(void);

#endif /* UDP_H */