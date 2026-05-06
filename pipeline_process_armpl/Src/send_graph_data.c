#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <complex.h>
#include <fftw3.h>
#include <time.h>

#include "send_graph_data.h"

#define MAGIC        (0xDEADBEEFu)
#define HEADER_SIZE  (16)

/* private */
static int sock_fd = -1;
static uint8_t *tmp = NULL;
static fftwf_complex *rxsig_transpose = NULL;
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

static int send_all(int fd, const uint8_t *buf, size_t len)
{
    size_t total = 0;
    ssize_t n;

    while (total < len) {
        n = send(fd, buf + total, len - total, 0);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("send");
            return -1;
        }

        if (n == 0) {
            fprintf(stderr, "send returned 0\n");
            return -1;
        }

        total += (size_t)n;
    }

    return 0;
}

static int serialize_header(uint8_t *buf, uint32_t dwell_id)
{
    int offset = 0;

    /*
     * TCP header format
     *
     * magic       uint32
     * dwell_id    uint32
     * fasttime    uint32
     * pulses      uint32
     */
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
    uint32_t dst_ft,
    uint32_t dst_pl,
    uint32_t dst_fasttime,
    uint32_t dst_pulses,
    uint32_t src_fasttime,
    uint32_t src_pulses,
    uint32_t *ft_start,
    uint32_t *ft_end,
    uint32_t *pl_start,
    uint32_t *pl_end
)
{
    *ft_start = (uint64_t)dst_ft * src_fasttime / dst_fasttime;
    *ft_end = (uint64_t)(dst_ft + 1) * src_fasttime / dst_fasttime;
    *pl_start = (uint64_t)dst_pl * src_pulses / dst_pulses;
    *pl_end = (uint64_t)(dst_pl + 1) * src_pulses / dst_pulses;

    if (*ft_end <= *ft_start) {
        *ft_end = *ft_start + 1;
    }

    if (*pl_end <= *pl_start) {
        *pl_end = *pl_start + 1;
    }

    if (*ft_end > src_fasttime) {
        *ft_end = src_fasttime;
    }

    if (*pl_end > src_pulses) {
        *pl_end = src_pulses;
    }
}

static float pooled_rxsig_power_mean(
    uint32_t dst_ft,
    uint32_t dst_pl,
    uint32_t dst_fasttime,
    uint32_t dst_pulses,
    uint32_t src_fasttime,
    uint32_t src_pulses,
    const fftwf_complex *rxsig
)
{
    uint32_t ft_start, ft_end, pl_start, pl_end;
    uint32_t count;
    uint32_t idx;
    uint32_t sft, spl;
    double sum;

    get_src_block_range(
        dst_ft,
        dst_pl,
        dst_fasttime,
        dst_pulses,
        src_fasttime,
        src_pulses,
        &ft_start,
        &ft_end,
        &pl_start,
        &pl_end
    );

    sum = 0.0;
    count = 0;

    for (sft = ft_start; sft < ft_end; ++sft) {
        for (spl = pl_start; spl < pl_end; ++spl) {
            idx = sft * src_pulses + spl;
            sum += (double)calc_power_from_rxsig(rxsig[idx]);
            ++count;
        }
    }

    return (count > 0) ? (float)(sum / (double)count) : 0.0f;
}

static float pooled_f32_mean(
    uint32_t dst_ft,
    uint32_t dst_pl,
    uint32_t dst_fasttime,
    uint32_t dst_pulses,
    uint32_t src_fasttime,
    uint32_t src_pulses,
    const float *src
)
{
    uint32_t ft_start, ft_end, pl_start, pl_end;
    uint32_t count;
    uint32_t idx;
    uint32_t sft, spl;
    double sum;

    get_src_block_range(
        dst_ft,
        dst_pl,
        dst_fasttime,
        dst_pulses,
        src_fasttime,
        src_pulses,
        &ft_start,
        &ft_end,
        &pl_start,
        &pl_end
    );

    sum = 0.0;
    count = 0;

    for (sft = ft_start; sft < ft_end; ++sft) {
        for (spl = pl_start; spl < pl_end; ++spl) {
            idx = sft * src_pulses + spl;
            sum += (double)src[idx];
            ++count;
        }
    }

    return (count > 0) ? (float)(sum / (double)count) : 0.0f;
}

static int pooled_det_mask_value(
    uint32_t dst_ft,
    uint32_t dst_pl,
    uint32_t dst_fasttime,
    uint32_t dst_pulses,
    uint32_t src_fasttime,
    uint32_t src_pulses,
    const uint8_t *det_mask
)
{
    uint32_t ft_start, ft_end, pl_start, pl_end;
    uint32_t idx;
    uint32_t sft, spl;

    get_src_block_range(
        dst_ft,
        dst_pl,
        dst_fasttime,
        dst_pulses,
        src_fasttime,
        src_pulses,
        &ft_start,
        &ft_end,
        &pl_start,
        &pl_end
    );

    for (sft = ft_start; sft < ft_end; ++sft) {
        for (spl = pl_start; spl < pl_end; ++spl) {
            idx = sft * src_pulses + spl;

            if (det_mask[idx] != 0) {
                return 1;
            }
        }
    }

    return 0;
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
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }

    if (setsockopt(sock_fd, SOL_SOCKET, SO_PRIORITY,
                   &tcp_prio, sizeof(tcp_prio)) < 0) {
        perror("setsockopt SO_PRIORITY tcp");
    }

    setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &snd, sizeof(snd));
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dst_port);

    if (inet_pton(AF_INET, dst_ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock_fd);
        sock_fd = -1;
        return -2;
    }

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock_fd);
        sock_fd = -1;
        return -3;
    }

    /*
     * TCP payload:
     *
     * 1. rxsig power map
     * 2. pc_map power map
     * 3. power_map
     * 4. threshold_map
     * 5. det_mask
     *
     * 각 map 크기:
     * TX_FASTTIME * TX_PULSES * 4 bytes
     */
    tmp = (uint8_t *)malloc((size_t)tx_map_count * 4u * 5u);
    if (!tmp) {
        perror("malloc");
        close(sock_fd);
        sock_fd = -1;
        return -4;
    }

    rxsig_transpose = (fftwf_complex *)fftwf_malloc(
        sizeof(fftwf_complex) *
        (size_t)fasttime *
        (size_t)num_pulses
    );

    if (!rxsig_transpose) {
        perror("fftwf_malloc");
        free(tmp);
        tmp = NULL;
        close(sock_fd);
        sock_fd = -1;
        return -5;
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

    if (sock_fd < 0) {
        return -1;
    }

    if (!rxsig || !pc_map || !power_map || !threshold_map || !det_mask) {
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

    if (send_all(sock_fd, header, HEADER_SIZE) < 0) {
        return -2;
    }

    /*
     * rxsig 입력은 [pulse][fasttime] 구조.
     * Helix 그래프 송신은 [fasttime][pulse] 구조로 맞추기 위해 transpose.
     */
    for (ft = 0; ft < src_fasttime; ++ft) {
        for (pl = 0; pl < src_pulses; ++pl) {
            size_t dst = (size_t)ft * (size_t)src_pulses + (size_t)pl;
            size_t src = (size_t)pl * (size_t)src_fasttime + (size_t)ft;

            rxsig_transpose[dst] = ((fftwf_complex *)rxsig)[src];
        }
    }

    /*
     * 원본 로직 유지:
     *
     * rxsig        : transpose 후 power pooling
     * pc_map       : 그대로 power pooling
     * power_map    : 그대로 mean pooling
     * thresholdMap : 그대로 mean pooling
     * det_mask     : block 안에 탐지 있으면 1
     */
    for (ft = 0; ft < TX_FASTTIME; ++ft) {
        for (pl = 0; pl < TX_PULSES; ++pl) {
            dst_idx = ft * TX_PULSES + pl;

            write_f32(
                buf_rxsig + dst_idx * 4u,
                pooled_rxsig_power_mean(
                    ft,
                    pl,
                    TX_FASTTIME,
                    TX_PULSES,
                    src_fasttime,
                    src_pulses,
                    rxsig_transpose
                )
            );

            write_f32(
                buf_pc + dst_idx * 4u,
                pooled_rxsig_power_mean(
                    ft,
                    pl,
                    TX_FASTTIME,
                    TX_PULSES,
                    src_fasttime,
                    src_pulses,
                    (fftwf_complex *)pc_map
                )
            );

            write_f32(
                buf_power + dst_idx * 4u,
                pooled_f32_mean(
                    ft,
                    pl,
                    TX_FASTTIME,
                    TX_PULSES,
                    src_fasttime,
                    src_pulses,
                    power_map
                )
            );

            write_f32(
                buf_thresh + dst_idx * 4u,
                pooled_f32_mean(
                    ft,
                    pl,
                    TX_FASTTIME,
                    TX_PULSES,
                    src_fasttime,
                    src_pulses,
                    threshold_map
                )
            );

            write_i32(
                buf_det + dst_idx * 4u,
                pooled_det_mask_value(
                    ft,
                    pl,
                    TX_FASTTIME,
                    TX_PULSES,
                    src_fasttime,
                    src_pulses,
                    det_mask
                )
            );
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
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }

    if (tmp) {
        free(tmp);
        tmp = NULL;
    }

    if (rxsig_transpose) {
        fftwf_free(rxsig_transpose);
        rxsig_transpose = NULL;
    }
}