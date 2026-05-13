#ifndef SEND_GRAPH_DATA_H
#define SEND_GRAPH_DATA_H

#include <stdint.h>

#define TX_FASTTIME         (256u)
#define TX_PULSES           (128u)
#define PACKET_PAYLOAD      (1400u)
#define TX_MAP_BYTES        (TX_FASTTIME * TX_PULSES * 4u * 5u)
#define MAX_CHUNKS          ((TX_MAP_BYTES + PACKET_PAYLOAD - 1u) / PACKET_PAYLOAD)
#define UDP_GRAPH_HDR_SIZE  (16u)
#define CHUNK_ID_META       (0xFFFFu)

typedef struct {
    uint32_t frame_id;
    uint32_t total_payload_size;
    uint16_t chunk_id;
    uint16_t total_chunks;
    uint16_t payload_size;
    uint16_t reserved;
} udp_graph_hdr_t;

typedef struct {
    uint32_t frame_id;
    uint32_t num_fasttime;
    uint32_t num_pulses;
    uint32_t num_maps;
    uint32_t total_bytes;
    uint32_t reserved;
} graph_data_hdr_t;

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