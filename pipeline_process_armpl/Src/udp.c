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
             const udp_target_t *targets)
{
    if (sock_fd < 0) return -1;
    if (!targets && target_num > 0) return -1;
    if (target_num > MAX_TARGETS) target_num = MAX_TARGETS;

    int offset = 0;

    // Header
    write_u32(pkt_buf + offset, dwell_id);    offset += 4;
    write_u32(pkt_buf + offset, target_num);  offset += 4;

    // Per target
    for (uint32_t i = 0; i < target_num; i++) {
        write_f32(pkt_buf + offset, targets[i].range_m);      offset += 4;
        write_f32(pkt_buf + offset, targets[i].velocity_mps); offset += 4;
        write_f32(pkt_buf + offset, targets[i].peak_power);   offset += 4;
    }

    ssize_t sent = sendto(sock_fd, pkt_buf, (size_t)offset, 0,
                          (struct sockaddr *)&dst_addr, sizeof(dst_addr));
    if (sent < 0) {
        perror("udp: sendto");
        return -2;
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