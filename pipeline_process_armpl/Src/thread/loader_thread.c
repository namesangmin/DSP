#include <stdio.h>
#include <stdint.h>      // uint32_t 추가
#include <stdatomic.h> // atomic 함수 사용을 위해 추가
#include <fftw3.h>
#include <complex.h>
#include <dirent.h>
#include <sys/stat.h>    // struct stat
#include <limits.h>      // PATH_MAX
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/syscall.h>    
#include <linux/futex.h>

#include "timer.h" 
#include "loader_thread.h"
#include "loader.h" // 고속 로드 함수 헤더 추가
#include "core_set.h"
#define PACKET_PAYLOAD 1400

static uint32_t offset[6];
static int sock_fd = -1;
static const uint32_t header_size = sizeof(packet_header_t);
static ICDHeader_t *icd_data;
static double t0;

int loader_thread_init(const RadarMeta *meta,  LoaderArgs *ld, Pipeline* pool, uint16_t rx_port) {
    struct sockaddr_in rxaddr;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }
    
    // 소켓 재사용 설정 (프로그램 재시작 시 포트 바인딩 에러 방지)
    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int bfs = 256 * 1024 * 1024;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &bfs, sizeof(bfs));

    memset(&rxaddr, 0, sizeof(rxaddr));
    rxaddr.sin_family = AF_INET;
    rxaddr.sin_addr.s_addr = INADDR_ANY; // 모든 IP로부터 수신
    rxaddr.sin_port = htons(rx_port);       // 사용할 포트 번호

    // 2. 바인딩 (전화기에 번호 부여)
    if (bind(sock_fd, (const struct sockaddr *)&rxaddr, sizeof(rxaddr)) < 0) {
        perror("bind failed");
        return -2;
    }

    icd_data = (ICDHeader_t *)malloc(sizeof(ICDHeader_t));
    if(!icd_data){
        return -3;
    }

    size_t total_floats = (size_t)meta->num_pulses * meta->num_fast_time_samples *2u;
    ld->buffer = (float*)malloc(total_floats * sizeof(float));
    if (!ld->buffer) {
        return -4;
    }

    printf("UDP 수신 초기화 완료 (Port: %d)\n", rx_port);

    return 0;
}

int loader_thread_destroy(LoaderArgs *ld) {
    if (ld == NULL || ld->buffer == NULL) {
        return 0;
    }

    free(ld->buffer);
    ld->buffer = NULL;

    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }

    return 0;
}

static int load_bin_to_float_array(float *out_buf,
                                    size_t count,        
                                    size_t offset_count)  // 232 (파일 헤더 크기)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    uint32_t received_packet_count = 0;
    uint32_t total_packets = 0;
    uint8_t packet_raw[1500]; 

    size_t total_expected_bytes = count * sizeof(float); // 512 * 1001 * 8

    printf("\n[DEBUG] 수신 시작: 기대 데이터 총량: %zu bytes\n", total_expected_bytes);
    
    int isFirstGetData = 0;

    while (1) {
        ssize_t n = recvfrom(sock_fd, packet_raw, sizeof(packet_raw), 0, 
                             (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("[ERROR] recvfrom failed");
            return -1;
        } 
        
        if(!isFirstGetData){
            isFirstGetData = 1;
            t0 = now_ms();
        }

        // 1. 헤더 파싱 (헤더는 송신측에서 htonl로 보냈으므로 ntohl 필수)
        uint32_t *header_ptr = (uint32_t *)packet_raw;

        uint32_t curr_dwell_id = ntohl(header_ptr[0]); 
        uint32_t curr_id      = ntohl(header_ptr[1]);
        uint32_t curr_count   = ntohl(header_ptr[2]);
        uint32_t curr_payload = ntohl(header_ptr[3]);
        uint32_t curr_fsize   = ntohl(header_ptr[4]);
        //uint32_t curr_reserved   = ntohl(header_ptr[5]);
        
        offset[0] = curr_dwell_id;
        offset[1] = curr_id;
        offset[2] = curr_count;
        offset[3] = curr_payload;
        offset[4] = curr_fsize;

        if (curr_id % 500 == 0 || curr_id == 0xFFFFFFFF) {
            printf("[RECV] DWELL ID: %u | curr ID: %u | total packets %u | Payload Size: %ld\t", curr_dwell_id, curr_id, total_packets, n-24);
            printf("receive data size: %ld\n", n -header_size);
        }
        
        uint8_t *payload_ptr = packet_raw + header_size;
        
        // 데이터 헤더 정보
        if (curr_id == 0xFFFFFFFF) {
            memcpy(icd_data, payload_ptr, sizeof(ICDHeader_t));
            continue;
        }
        else {
            if(total_packets == 0) 
                total_packets = curr_count;

            // [일반 패킷] 절대 위치를 찾아 복사
            // 송신 측이 파일 기준 curr_id * 1400 지점의 데이터를 보냈음.
            size_t write_pos_bytes = (size_t)curr_id * PACKET_PAYLOAD;
            size_t data_len = (size_t)(n - header_size);

            // 버퍼 오버플로우 안전장치
            if (write_pos_bytes + data_len <= total_expected_bytes) {
                memcpy((uint8_t*)out_buf + write_pos_bytes, payload_ptr, data_len);
            }
            else {
                printf("[ERROR] Memory Overflow! ID:%u\n", curr_id);
            }
            received_packet_count++;
        }

        // 모든 패킷을 다 받았는지 확인하여 루프 탈출
        if (total_packets > 0 && received_packet_count >= total_packets) {
            printf("[SUCCESS] 모든 패킷 수신 완료 (%u/%u)\n", received_packet_count, total_packets);
            break; 
        }
    }

    printf("\n");
    printf("====================ICD_HEADER 정보====================\n");
    printf("dwell id: %u\n", icd_data->DwellId);
    printf("Phi: %f\n", icd_data->Phi);
    printf("fc_Hz: %f\n", icd_data->fc_Hz);
    printf("fs_Hz: %f\n", icd_data->fs_Hz);
    printf("PRF_Hz: %f\n", icd_data->PRF_Hz);
    printf("PulseWidth: %f\n", icd_data->PulseWidth);
    printf("SweepBandwidth: %f\n", icd_data->SweepBandwidth);
    printf("NumPulse: %u\n", icd_data->NumPulse);
    printf("NumSample: %u\n", icd_data->NumSample);
    printf("=======================================================\n");
    printf("\n");
    return 0;
}

void *loader_thread_main(void *arg)
{
    LoaderArgs *a = (LoaderArgs *)arg;
    pin_thread_to_cpu(a->cpu_id);

    int cols = a->meta->num_pulses;
    int rows = a->meta->num_fast_time_samples;
    int half = cols / 2;
    size_t total_count = (size_t)a->meta->num_pulses * (size_t)a->meta->num_fast_time_samples * 2u;
    int frame_idx = 0;
   
    while(1){
        int raw_idx = frame_idx % NUM_BUFFERS;
        int push_err = 0;

        while (atomic_load_explicit(&a->pipe->pulse_compress_map[raw_idx].state, memory_order_acquire) != BUF_FREE) 
        {
            if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) {
                push_err = 1;
                break; // 파이프라인 에러 시 탈출
            }
            usleep(100); // CPU 과부하 방지 (0.1ms 대기)
        }

        if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) {
            break;
        }

        atomic_store_explicit(&a->pipe->pulse_compress_map[raw_idx].state, BUF_FILLING, memory_order_release);
        atomic_fetch_add(&a->pipe->buf_use_count[raw_idx], 1);  // ← 여기

        // =================================================================
        // UDP로 받는 시간 측정
        if(load_bin_to_float_array(a->buffer, total_count, 232) < 0){
            atomic_store(&a->pipe->error, 1);
            break;
        }
        
        a->pipe->dwell_ids[raw_idx] = icd_data->DwellId;
        a->pipe->phi[raw_idx] = icd_data->Phi; 

        for (int p = 0; p < cols; p++) {
            for (int s = 0; s < rows; s++) {
                size_t idx  = (size_t)p * rows + s;
                size_t bidx = 2u * idx;
                a->pipe->raw_data[raw_idx][idx] =
                    a->buffer[bidx] + a->buffer[bidx + 1] * I;
            }
        }
        
        for (int p = 0; p < cols; p++) {
            PulseJob job = { .pulse_idx = p, .raw_idx = raw_idx };
            PulseQueue *q = (p < half) ? &a->pipe->even_q : &a->pipe->odd_q;
            
            int pulse_ret = pulse_queue_push(q, job); 
            if (pulse_ret != 0) {
                fprintf(stderr, "[Loader ERROR] 큐 Push 실패: ret=%d pulse_idx=%d\n", pulse_ret, p);
                atomic_store(&a->pipe->error, 1);
                push_err = 1;
                break;
            }
        }
        
        if (push_err) break;
        // 512번 push 완료 후 큐당 1번씩
        syscall(SYS_futex, (int *)&a->pipe->even_q.tail, FUTEX_WAKE, 1, NULL, NULL, 0);
        syscall(SYS_futex, (int *)&a->pipe->odd_q.tail,  FUTEX_WAKE, 1, NULL, NULL, 0);

        frame_idx++;  // ← 여기 추가
        double t1 = now_ms();

        if (a->timing) {
            a->timing->loader_ms = t1 - t0;
        }
    }

    pulse_queue_close(&a->pipe->even_q);
    pulse_queue_close(&a->pipe->odd_q);
    
    return NULL;
}