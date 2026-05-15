#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <complex.h>
#include <fftw3.h>
#include <time.h>

#include "send_graph_data.h"

#define MAGIC        (0xDEADBEEFu)
#define HEADER_SIZE  (16)

static int sock_fd = -1;
static uint8_t *tmp = NULL;
static uint8_t header[HEADER_SIZE];

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

static void write_i32(uint8_t *buf, int val)
{
    uint32_t u;

    memcpy(&u, &val, sizeof(uint32_t));
    write_u32(buf, u);
}

static int serialize_header(uint8_t *buf, uint32_t dwell_id)
{
    int offset = 0;

    write_u32(buf + offset, MAGIC);        offset += 4;
    write_u32(buf + offset, dwell_id);     offset += 4;
    write_u32(buf + offset, TX_FASTTIME);  offset += 4;
    write_u32(buf + offset, TX_PULSES);    offset += 4;

    return offset;
}

static inline float calc_power_from_rxsig(const fftwf_complex x)
{
    const float re = creal(x);
    const float im = cimag(x);

    return re * re + im * im;
}

static inline void get_src_block_range(
    uint32_t dst_ft, uint32_t dst_pl, uint32_t dst_fasttime,
    uint32_t dst_pulses, uint32_t src_fasttime, uint32_t src_pulses,
    uint32_t *ft_start, uint32_t *ft_end, uint32_t *pl_start,
    uint32_t *pl_end)
{
    *ft_start = (uint64_t)dst_ft * src_fasttime / dst_fasttime;
    *ft_end = (uint64_t)(dst_ft + 1) * src_fasttime / dst_fasttime;
    *pl_start = (uint64_t)dst_pl * src_pulses / dst_pulses;
    *pl_end = (uint64_t)(dst_pl + 1) * src_pulses / dst_pulses;

    if (*ft_end <= *ft_start) 
    {
        *ft_end = *ft_start + 1;
    }

    if (*pl_end <= *pl_start) 
    {
        *pl_end = *pl_start + 1;
    }

    if (*ft_end > src_fasttime) 
    {
        *ft_end = src_fasttime;
    }

    if (*pl_end > src_pulses) 
    {
        *pl_end = src_pulses;
    }
}

/* public */
int send_graph_data_init(
    const char *dst_ip,
    uint16_t dst_port,
    int fasttime,
    int num_pulses
)
{
    struct sockaddr_in addr;
    uint32_t tx_map_count;
    int tcp_prio;
    int snd;
    int rcv;

    tcp_prio = 0;
    snd = 1073741824;
    rcv = 1073741824;
    tx_map_count = TX_FASTTIME * TX_PULSES;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) 
    {
        perror("socket");
        return -1;
    }

    if (setsockopt(sock_fd, SOL_SOCKET, SO_PRIORITY,
                   &tcp_prio, sizeof(tcp_prio)) < 0) 
    {
        perror("setsockopt SO_PRIORITY tcp");
    }

    setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &snd, sizeof(snd));
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dst_port);

    if (inet_pton(AF_INET, dst_ip, &addr.sin_addr) <= 0) 
    {
        perror("inet_pton");
        close(sock_fd);
        sock_fd = -1;
        return -2;
    }

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
    {
        perror("connect");
        close(sock_fd);
        sock_fd = -1;
        return -3;
    }

    tmp = (uint8_t *)malloc((size_t)tx_map_count * 4u * 5u);
    if (!tmp) 
    {
        perror("malloc");
        close(sock_fd);
        sock_fd = -1;
        return -4;
    }

    return 0;
}

static int send_all(int fd, const uint8_t *buf, size_t len)
{
    size_t total = 0;
    ssize_t n;

    while (total < len) 
    {
        n = send(fd, buf + total, len - total, 0);

        if (n < 0) {
            if (errno == EINTR) 
            {
                continue;
            }

            perror("send");
            return -1;
        }

        if (n == 0) 
        {
            fprintf(stderr, "send returned 0\n");
            return -1;
        }

        total += (size_t)n;
    }

    return 0;
}

int send_graph_data(
    uint32_t dwell_id,
    uint32_t num_fasttime,
    uint32_t num_pulses,
    const void *rxsig,
    const void *pc_map,
    const float *power_map,
    const float *threshold_map,
    const uint8_t *det_mask)
{
#ifdef DEBUG
    struct timespec start, end;
    long sec, nsec;
    double elapsed;

    clock_gettime(CLOCK_MONOTONIC, &start);
#endif

    uint32_t src_fasttime;
    uint32_t src_pulses;
    uint32_t tx_map_count;
    uint32_t ft;
    uint32_t pl;
    uint32_t dst_idx;

    uint8_t *buf_rxsig;
    uint8_t *buf_pc;
    uint8_t *buf_power;
    uint8_t *buf_thresh;
    uint8_t *buf_det;

    if (sock_fd < 0) 
    {
        return -1;
    }

    if (!rxsig || !pc_map || !power_map || !threshold_map || !det_mask) 
    {
        return -1;
    }

    src_fasttime = num_fasttime;
    src_pulses = num_pulses;
    tx_map_count = TX_FASTTIME * TX_PULSES;

    buf_rxsig = tmp + tx_map_count * 4u * 0u;
    buf_pc = tmp + tx_map_count * 4u * 1u;
    buf_power = tmp + tx_map_count * 4u * 2u;
    buf_thresh = tmp + tx_map_count * 4u * 3u;
    buf_det = tmp + tx_map_count * 4u * 4u;

    serialize_header(header, dwell_id);

    if (send_all(sock_fd, header, HEADER_SIZE) < 0) 
    {
        return -2;
    }

    for (ft = 0; ft < TX_FASTTIME; ++ft) {
        for (pl = 0; pl < TX_PULSES; ++pl) {
            dst_idx = ft * TX_PULSES + pl;

            // 블록 범위 한 번만 계산
            uint32_t ft_start, ft_end, pl_start, pl_end;
            get_src_block_range(ft, pl, TX_FASTTIME, TX_PULSES,
                                src_fasttime, src_pulses,
                                &ft_start, &ft_end, &pl_start, &pl_end);

            // 5개 값을 한 루프에서 계산
            float sum_rxsig = 0, sum_pc = 0, sum_power = 0, sum_thresh = 0;
            int det_hit = 0;
            uint32_t count = 0;

            for (uint32_t spl = pl_start; spl < pl_end; ++spl) {
                for (uint32_t sft = ft_start; sft < ft_end; ++sft) {
                    uint32_t i_pulse  = spl * src_fasttime + sft;  // [pulse][fasttime]
                    uint32_t i_range  = sft * src_pulses   + spl;  // [fasttime][pulse]

                    sum_rxsig  += calc_power_from_rxsig(((fftwf_complex *)rxsig)[i_pulse]);
                    sum_pc     += calc_power_from_rxsig(((fftwf_complex *)pc_map)[i_pulse]);
                    sum_power  += power_map[i_range];
                    sum_thresh += threshold_map[i_range];
                    if (det_mask[i_range]) det_hit = 1;
                    ++count;
                }
            }

            float inv = (count > 0) ? 1.0f / count : 0.0f;
            write_f32(buf_rxsig  + dst_idx * 4u, sum_rxsig  * inv);
            write_f32(buf_pc     + dst_idx * 4u, sum_pc     * inv);
            write_f32(buf_power  + dst_idx * 4u, sum_power  * inv);
            write_f32(buf_thresh + dst_idx * 4u, sum_thresh * inv);
            write_i32(buf_det    + dst_idx * 4u, det_hit);
        }
    }

    if (send_all(sock_fd, tmp, (size_t)tx_map_count * 4u * 5u) < 0) {
        return -3;
    }

#ifdef DEBUG
    clock_gettime(CLOCK_MONOTONIC, &end);

    sec = end.tv_sec - start.tv_sec;
    nsec = end.tv_nsec - start.tv_nsec;
    elapsed = (double)sec + (double)nsec * 1e-9;

    printf("TCP Send Time: %.9f sec\n", elapsed);
#endif

    return 0;
}

void send_graph_data_destroy(void)
{
    if (sock_fd >= 0) 
    {
        close(sock_fd);
        sock_fd = -1;
    }

    if (tmp) 
    {
        free(tmp);
        tmp = NULL;
    }
     
}