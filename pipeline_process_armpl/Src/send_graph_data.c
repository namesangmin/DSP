//#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <complex.h>
#include <math.h>

#include "send_graph_data.h"

#define MAGIC        (0xDEADBEEFu)
#define HEADER_SIZE  (16)   // magic + dwell_id + num_fasttime + num_pulses
#define MAP_COUNT    (5)    // rxsig, pc_map, power_map, threshold, det_mask
#define TIMING_COUNT (6)    // compress, transpose, mti, mtd, cfar, cluster

// 전체 패킷 크기 계산
// float map 4개 + uint8 map 1개 + timing 6 * double
#define TX_MAP_CELLS  (TX_FASTTIME * TX_PULSES)
#define PAYLOAD_SIZE  (TX_MAP_CELLS * 4u * 4u   \
                     + TX_MAP_CELLS * 1u         \
                     + TIMING_COUNT * 8u)

// =========================================================
// private
// =========================================================
static int               sock_fd    = -1;
static uint8_t          *pkt_buf    = NULL;  // header + payload
static float complex    *transpose_buf = NULL; // transpose 작업용

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

static void write_f64(uint8_t *buf, double val)
{
    uint64_t u;
    memcpy(&u, &val, sizeof(uint64_t));
    buf[0] = (u >> 56) & 0xFF;
    buf[1] = (u >> 48) & 0xFF;
    buf[2] = (u >> 40) & 0xFF;
    buf[3] = (u >> 32) & 0xFF;
    buf[4] = (u >> 24) & 0xFF;
    buf[5] = (u >> 16) & 0xFF;
    buf[6] = (u >>  8) & 0xFF;
    buf[7] = (u      ) & 0xFF;
}

static int send_all(int fd, const uint8_t *buf, size_t len)
{
    size_t  total = 0;
    ssize_t n;

    while (total < len) {
        n = send(fd, buf + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("tcp: send");
            return -1;
        }
        if (n == 0) {
            fprintf(stderr, "tcp: send returned 0\n");
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

// =========================================================
// 다운샘플 헬퍼
// =========================================================

// [pulse][range] 배치 복소수 → transpose → power 평균
static float pooled_complex_power(uint32_t dst_ft, uint32_t dst_pl,
                                   uint32_t dst_fasttime, uint32_t dst_pulses,
                                   uint32_t src_fasttime, uint32_t src_pulses,
                                   const float complex *src_range_pulse)
{
    // src: [range][pulse] 배치 (transpose 완료 상태)
    uint32_t ft_s = (uint64_t)dst_ft * src_fasttime / dst_fasttime;
    uint32_t ft_e = (uint64_t)(dst_ft + 1) * src_fasttime / dst_fasttime;
    uint32_t pl_s = (uint64_t)dst_pl * src_pulses / dst_pulses;
    uint32_t pl_e = (uint64_t)(dst_pl + 1) * src_pulses / dst_pulses;

    if (ft_e <= ft_s) ft_e = ft_s + 1;
    if (pl_e <= pl_s) pl_e = pl_s + 1;
    if (ft_e > src_fasttime) ft_e = src_fasttime;
    if (pl_e > src_pulses)   pl_e = src_pulses;

    double sum   = 0.0;
    uint32_t cnt = 0;

    for (uint32_t ft = ft_s; ft < ft_e; ft++) {
        for (uint32_t pl = pl_s; pl < pl_e; pl++) {
            float complex v = src_range_pulse[ft * src_pulses + pl];
            float re = crealf(v), im = cimagf(v);
            sum += (double)(re * re + im * im);
            cnt++;
        }
    }
    return (cnt > 0) ? (float)(sum / cnt) : 0.0f;
}

// float map 평균 ([range][doppler] 배치)
static float pooled_float(uint32_t dst_ft, uint32_t dst_pl,
                           uint32_t dst_fasttime, uint32_t dst_pulses,
                           uint32_t src_fasttime, uint32_t src_pulses,
                           const float *src)
{
    uint32_t ft_s = (uint64_t)dst_ft * src_fasttime / dst_fasttime;
    uint32_t ft_e = (uint64_t)(dst_ft + 1) * src_fasttime / dst_fasttime;
    uint32_t pl_s = (uint64_t)dst_pl * src_pulses / dst_pulses;
    uint32_t pl_e = (uint64_t)(dst_pl + 1) * src_pulses / dst_pulses;

    if (ft_e <= ft_s) ft_e = ft_s + 1;
    if (pl_e <= pl_s) pl_e = pl_s + 1;
    if (ft_e > src_fasttime) ft_e = src_fasttime;
    if (pl_e > src_pulses)   pl_e = src_pulses;

    double   sum = 0.0;
    uint32_t cnt = 0;

    for (uint32_t ft = ft_s; ft < ft_e; ft++) {
        for (uint32_t pl = pl_s; pl < pl_e; pl++) {
            sum += (double)src[ft * src_pulses + pl];
            cnt++;
        }
    }
    return (cnt > 0) ? (float)(sum / cnt) : 0.0f;
}

// uint8 OR ([range][doppler] 배치)
static uint8_t pooled_mask(uint32_t dst_ft, uint32_t dst_pl,
                            uint32_t dst_fasttime, uint32_t dst_pulses,
                            uint32_t src_fasttime, uint32_t src_pulses,
                            const uint8_t *src)
{
    uint32_t ft_s = (uint64_t)dst_ft * src_fasttime / dst_fasttime;
    uint32_t ft_e = (uint64_t)(dst_ft + 1) * src_fasttime / dst_fasttime;
    uint32_t pl_s = (uint64_t)dst_pl * src_pulses / dst_pulses;
    uint32_t pl_e = (uint64_t)(dst_pl + 1) * src_pulses / dst_pulses;

    if (ft_e <= ft_s) ft_e = ft_s + 1;
    if (pl_e <= pl_s) pl_e = pl_s + 1;
    if (ft_e > src_fasttime) ft_e = src_fasttime;
    if (pl_e > src_pulses)   pl_e = src_pulses;

    for (uint32_t ft = ft_s; ft < ft_e; ft++) {
        for (uint32_t pl = pl_s; pl < pl_e; pl++) {
            if (src[ft * src_pulses + pl]) return 1;
        }
    }
    return 0;
}

// =========================================================
// public
// =========================================================
int send_graph_data_init(const char *dst_ip, uint16_t dst_port,
                                 int num_fasttime, int num_pulses)
{
    struct sockaddr_in addr;

    if (!dst_ip) return -1;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("tcp: socket");
        return -1;
    }

    int snd = 1 << 20; // 1MB send buffer
    setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &snd, sizeof(snd));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(dst_port);

    if (inet_pton(AF_INET, dst_ip, &addr.sin_addr) <= 0) {
        perror("tcp: inet_pton");
        close(sock_fd);
        sock_fd = -1;
        return -2;
    }

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("tcp: connect");
        close(sock_fd);
        sock_fd = -1;
        return -3;
    }

    // 패킷 버퍼 할당
    pkt_buf = (uint8_t *)malloc(HEADER_SIZE + PAYLOAD_SIZE);
    if (!pkt_buf) {
        perror("tcp: malloc pkt_buf");
        close(sock_fd);
        sock_fd = -1;
        return -4;
    }

    // transpose 작업 버퍼: [range][pulse]
    transpose_buf = (float complex *)malloc(
        (size_t)num_fasttime * (size_t)num_pulses * sizeof(float complex));
    if (!transpose_buf) {
        perror("tcp: malloc transpose_buf");
        free(pkt_buf);
        pkt_buf = NULL;
        close(sock_fd);
        sock_fd = -1;
        return -5;
    }

    return 0;
}

int send_graph_data_loop(uint32_t              dwell_id,
                          uint32_t              num_fasttime,
                          uint32_t              num_pulses,
                          const void           *rxsig,
                          const void           *pc_map,
                          const float          *power_map,
                          const float          *threshold_map,
                          const uint8_t        *det_mask,
                          const graph_timing_t *timing)
{
    if (sock_fd < 0)                                         return -1;
    if (!rxsig || !pc_map || !power_map ||
        !threshold_map || !det_mask || !timing)              return -1;

    const float complex *rxsig_fc  = (const float complex *)rxsig;
    const float complex *pc_map_fc = (const float complex *)pc_map;

    int offset = 0;

    // =========================================================
    // Header
    // =========================================================
    write_u32(pkt_buf + offset, MAGIC);        offset += 4;
    write_u32(pkt_buf + offset, dwell_id);     offset += 4;
    write_u32(pkt_buf + offset, TX_FASTTIME);  offset += 4;
    write_u32(pkt_buf + offset, TX_PULSES);    offset += 4;

    // =========================================================
    // rxsig: [pulse][range] → transpose → [range][pulse] → power 다운샘플
    // =========================================================
    for (uint32_t r = 0; r < num_fasttime; r++) {
        for (uint32_t p = 0; p < num_pulses; p++) {
            transpose_buf[r * num_pulses + p] = rxsig_fc[p * num_fasttime + r];
        }
    }
    for (uint32_t ft = 0; ft < TX_FASTTIME; ft++) {
        for (uint32_t pl = 0; pl < TX_PULSES; pl++) {
            float v = pooled_complex_power(ft, pl,
                          TX_FASTTIME, TX_PULSES,
                          num_fasttime, num_pulses,
                          transpose_buf);
            write_f32(pkt_buf + offset, v);
            offset += 4;
        }
    }

    // =========================================================
    // pc_map: 동일하게 [pulse][range] → transpose → power 다운샘플
    // =========================================================
    for (uint32_t r = 0; r < num_fasttime; r++) {
        for (uint32_t p = 0; p < num_pulses; p++) {
            transpose_buf[r * num_pulses + p] = pc_map_fc[p * num_fasttime + r];
        }
    }
    for (uint32_t ft = 0; ft < TX_FASTTIME; ft++) {
        for (uint32_t pl = 0; pl < TX_PULSES; pl++) {
            float v = pooled_complex_power(ft, pl,
                          TX_FASTTIME, TX_PULSES,
                          num_fasttime, num_pulses,
                          transpose_buf);
            write_f32(pkt_buf + offset, v);
            offset += 4;
        }
    }

    // =========================================================
    // power_map: [range][doppler] → 다운샘플
    // =========================================================
    for (uint32_t ft = 0; ft < TX_FASTTIME; ft++) {
        for (uint32_t pl = 0; pl < TX_PULSES; pl++) {
            float v = pooled_float(ft, pl,
                          TX_FASTTIME, TX_PULSES,
                          num_fasttime, num_pulses,
                          power_map);
            write_f32(pkt_buf + offset, v);
            offset += 4;
        }
    }

    // =========================================================
    // threshold_map: [range][doppler] → 다운샘플
    // =========================================================
    for (uint32_t ft = 0; ft < TX_FASTTIME; ft++) {
        for (uint32_t pl = 0; pl < TX_PULSES; pl++) {
            float v = pooled_float(ft, pl,
                          TX_FASTTIME, TX_PULSES,
                          num_fasttime, num_pulses,
                          threshold_map);
            write_f32(pkt_buf + offset, v);
            offset += 4;
        }
    }

    // =========================================================
    // det_mask: [range][doppler] → OR 다운샘플 (bool)
    // =========================================================
    for (uint32_t ft = 0; ft < TX_FASTTIME; ft++) {
        for (uint32_t pl = 0; pl < TX_PULSES; pl++) {
            pkt_buf[offset++] = pooled_mask(ft, pl,
                                    TX_FASTTIME, TX_PULSES,
                                    num_fasttime, num_pulses,
                                    det_mask);
        }
    }

    // =========================================================
    // Timing
    // =========================================================
    write_f64(pkt_buf + offset, timing->compress_ms);  offset += 8;
    write_f64(pkt_buf + offset, timing->transpose_ms); offset += 8;
    write_f64(pkt_buf + offset, timing->mti_ms);       offset += 8;
    write_f64(pkt_buf + offset, timing->mtd_ms);       offset += 8;
    write_f64(pkt_buf + offset, timing->cfar_ms);      offset += 8;
    write_f64(pkt_buf + offset, timing->cluster_ms);   offset += 8;

    // =========================================================
    // 전송
    // =========================================================
    if (send_all(sock_fd, pkt_buf, (size_t)offset) < 0) {
        return -2;
    }

    return 0;
}

void send_graph_data_destroy(void)
{
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }
    free(pkt_buf);
    pkt_buf = NULL;
    free(transpose_buf);
    transpose_buf = NULL;
}