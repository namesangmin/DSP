//#define _POSIX_C_SOURCE 199309L
//#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#include "udp.h"

// 최대 타겟 수 기준 최대 패킷 크기
// header(8) + target * 12 * MAX_TARGETS
#define UDP_PACKET_SIZE  (8 + MAX_TARGETS * 12)

// =========================================================
// private
// =========================================================
static int               sock_fd  = -1;
static struct sockaddr_in dst_addr;
static uint8_t           pkt_buf[UDP_PACKET_SIZE];

static void write_u32(uint8_t *buf, uint32_t val)
{
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >>  8) & 0xFF;
    buf[3] = (val      ) & 0xFF;
}

static void write_f32(uint8_t *buf, float val)
{
    uint32_t u;
    memcpy(&u, &val, sizeof(uint32_t));
    write_u32(buf, u);
}

/* uint64_t를 big-endian 버퍼에 씀 */
static void write_u64(uint8_t *buf, uint64_t val)
{
    buf[0] = (val >> 56) & 0xFF;
    buf[1] = (val >> 48) & 0xFF;
    buf[2] = (val >> 40) & 0xFF;
    buf[3] = (val >> 32) & 0xFF;
    buf[4] = (val >> 24) & 0xFF;
    buf[5] = (val >> 16) & 0xFF;
    buf[6] = (val >>  8) & 0xFF;
    buf[7] = (val      ) & 0xFF;
}

/* double을 big-endian 버퍼에 씀 */
static void write_f64(uint8_t *buf, double val)
{
    uint64_t tmp;
    memcpy(&tmp, &val, sizeof(uint64_t));
    write_u64(buf, tmp);
}


static int serialize_meta(const udp_header_t *meta, uint8_t *buf, int start_offset)
{
    int offset = start_offset;
    write_u32(buf + offset, meta->dwell_id);   offset += 4;
    write_u32(buf + offset, meta->target_num); offset += 4;
    write_f32(buf + offset, meta->theta);      offset += 4;
    write_f32(buf + offset, meta->phi);        offset += 4;
    write_f64(buf + offset, meta->compress_ms + meta->transpose_ms);   offset += 8;
    write_f64(buf + offset, meta->mti_ms); offset += 8;
    write_f64(buf + offset, meta->mtd_ms);   offset += 8;
    write_f64(buf + offset, meta->cfar_ms); offset += 8;
    write_f64(buf + offset, meta->cluster_ms);   offset += 8;
    return offset;
}

static int serialize_data(const udp_target_t *data, uint8_t *buf, int offset)
{
    write_f32(buf + offset, data->distance);    offset += 4;
    write_f32(buf + offset, data->speed);       offset += 4;

    return offset;
}
// =========================================================
// public
// =========================================================
int udp_init(const char *dst_ip, uint16_t dst_port)
{
    if (!dst_ip) return -1;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("udp: socket");
        return -1;
    }

    int prio = 6;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)) < 0) {
        perror("udp: setsockopt SO_PRIORITY");
    }

    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port   = htons(dst_port);

    if (inet_pton(AF_INET, dst_ip, &dst_addr.sin_addr) <= 0) {
        perror("udp: inet_pton");
        close(sock_fd);
        sock_fd = -1;
        return -2;
    }

    return 0;
}

int udp_loop(uint32_t dwell_id,
            uint32_t target_num,
            const udp_target_t *targets,
            float angle,
            PipelineTiming *timing)
{
    if (sock_fd < 0) return -1;
    if (!targets && target_num > 0) return -2;
    if (target_num > MAX_TARGETS) target_num = MAX_TARGETS;

    int len = 0;
    udp_header_t meta;

    meta.dwell_id = (uint32_t)dwell_id;
    meta.target_num = (uint32_t)target_num;
    meta.theta = 45.0f;
    meta.phi = angle;
    meta.compress_ms = timing->compress_ms / 1000;
    meta.transpose_ms = timing->transpose_ms/ 1000;
    meta.mti_ms = timing->mti_ms/ 1000;
    meta.mtd_ms = timing->mtd_ms/ 1000;
    meta.cfar_ms = timing->cfar_ms/ 1000;
    meta.cluster_ms = timing->cluster_ms/ 1000;

    len = serialize_meta(&meta, pkt_buf, len);
    for (int i = 0; i < target_num; i++) {
        len = serialize_data(targets + i, pkt_buf, len);
    }

    ssize_t sent = sendto(sock_fd, pkt_buf, (size_t)len, 0,
                          (struct sockaddr *)&dst_addr, sizeof(dst_addr));
    if (sent < 0) {
        perror("udp: sendto");
        return -3;
    }

    if(sent != len){
        return -4;
    }

    return 0;
}

void udp_destroy(void)
{
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }
}