#ifndef SEND_GRAPH_DATA_H
#define SEND_GRAPH_DATA_H

#include <stdint.h>

// =========================================================
// 다운샘플 해상도
// =========================================================
#define TX_FASTTIME  (256u)
#define TX_PULSES    (128u)

// =========================================================
// 패킷 구조
// =========================================================
// [Header]
//   magic        uint32  4
//   dwell_id     uint32  4
//   num_fasttime uint32  4
//   num_pulses   uint32  4
// [Maps - 다운샘플 TX_FASTTIME * TX_PULSES]
//   rxsig        float[] TX_FASTTIME * TX_PULSES * 4
//   pc_map       float[] TX_FASTTIME * TX_PULSES * 4
//   power_map    float[] TX_FASTTIME * TX_PULSES * 4
//   threshold    float[] TX_FASTTIME * TX_PULSES * 4
//   det_mask     uint8[] TX_FASTTIME * TX_PULSES * 1  (bool)
// [Timing]
//   compress_ms  double  8
//   transpose_ms double  8
//   mti_ms       double  8
//   mtd_ms       double  8
//   cfar_ms      double  8
//   cluster_ms   double  8
// =========================================================

typedef struct {
    double compress_ms;
    double transpose_ms;
    double mti_ms;
    double mtd_ms;
    double cfar_ms;
    double cluster_ms;
} graph_timing_t;

// =========================================================
// API
// =========================================================
int  send_graph_data_init(const char *dst_ip, uint16_t dst_port,
                                  int num_fasttime, int num_pulses);

int  send_graph_data_loop(uint32_t              dwell_id,
                           uint32_t              num_fasttime,
                           uint32_t              num_pulses,
                           const void           *rxsig,        // float complex [pulse][range]
                           const void           *pc_map,       // float complex [pulse][range]
                           const float          *power_map,    // [range][doppler]
                           const float          *threshold_map,// [range][doppler]
                           const uint8_t        *det_mask,     // [range][doppler]
                           const graph_timing_t *timing);

void send_graph_data_destroy(void);

#endif /* SEND_GRAPH_DATA_H */