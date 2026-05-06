#ifndef SEND_GRAPH_DATA_H
#define SEND_GRAPH_DATA_H

#include <stdint.h>

// =========================================================
// 다운샘플 해상도
// =========================================================
#define TX_FASTTIME  (256u)
#define TX_PULSES    (128u)

int  send_graph_data_init(const char *dst_ip, uint16_t dst_port,
                        int num_fasttime, int num_pulses);

int send_graph_data(uint32_t dwell_id,
                    uint32_t num_fasttime,
                    uint32_t num_pulses,
                    const void *rxsig,        // float complex [pulse][range]
                    const void *pc_map,       // float complex [pulse][range]
                    const float *power_map,    // [range][doppler]
                    const float *threshold_map,// [range][doppler]
                    const uint8_t *det_mask);

void send_graph_data_destroy(void);

#endif