#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <fftw3.h>
#include "loader.h"
#include "common.h"

static void trim(char *s) 
{
    char *p = s;
    char *q;

    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    q = s + strlen(s);
    while (q > s && isspace((unsigned char)q[-1])) {
        q--;
    }
    *q = '\0';
}

int alloc_complex_matrix(int rows, int cols, ComplexMatrix *m) 
{
    if (!m || rows <= 0 || cols <= 0) return -1;

    m->rows = rows;
    m->cols = cols;
    m->data = (float complex *)fftwf_malloc((size_t)rows * (size_t)cols * sizeof(float complex));
    if(!m->data)
    {
        return -1;
    }
    return 0;
}

void free_complex_matrix(ComplexMatrix *m) 
{
    if (!m) return;
    fftwf_free(m->data);
    m->data = NULL;
    m->rows = 0;
    m->cols = 0;
}

int load_metadata(const char *path, RadarMeta *meta) 
{
    FILE *fp;
    char line[512];

    if (!path || !meta) return -1;

    memset(meta, 0, sizeof(*meta));

    fp = fopen(path, "r");
    if (!fp) return -1;

    while (fgets(line, sizeof(line), fp)) {
        char *comma;
        char key[256];
        char val[256];

        if (line[0] == '\0' || line[0] == '\n') continue;

        comma = strchr(line, ',');
        if (!comma) continue;

        *comma = '\0';

        strncpy(key, line, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        strncpy(val, comma + 1, sizeof(val) - 1);
        val[sizeof(val) - 1] = '\0';

        trim(key);
        trim(val);

        if (strcmp(key, "fc_Hz") == 0) meta->fc_hz = atof(val);
        else if (strcmp(key, "fs_Hz") == 0) meta->fs_hz = atof(val);
        else if (strcmp(key, "PRF_Hz") == 0) meta->prf_hz = atof(val);
        else if (strcmp(key, "PulseWidth_s") == 0) meta->pulse_width_s = atof(val);
        else if (strcmp(key, "SweepBandwidth_Hz") == 0) meta->sweep_bandwidth_hz = atof(val);
        else if (strcmp(key, "NumPulses") == 0) meta->num_pulses = atoi(val);
        else if (strcmp(key, "NumFastTimeSamples") == 0) meta->num_fast_time_samples = atoi(val);
    }

    fclose(fp);

    if (meta->fc_hz <= 0.0 ||
        meta->fs_hz <= 0.0 ||
        meta->prf_hz <= 0.0 ||
        meta->pulse_width_s <= 0.0 ||
        meta->sweep_bandwidth_hz <= 0.0 ||
        meta->num_pulses <= 0 ||
        meta->num_fast_time_samples <= 0) {
        return -1;
    }

    return 0;
}

// int load_bin_to_float_array(float *out_buf,
//                                     size_t count,        
//                                     size_t offset_count)
// {
//     struct sockaddr_in client_addr;
//     socklen_t addr_len = sizeof(client_addr);
    
//     uint32_t received_packet_count = 0;
//     uint32_t total_packets = 0;
//     uint8_t packet_raw[1500]; 

//     size_t total_expected_bytes = count * sizeof(float);

//     printf("\n[DEBUG] 수신 시작: 기대 데이터 총량: %zu bytes\n", total_expected_bytes);
    
//     int isFirstGetData = 0;

//     while (1) 
//     {
//         ssize_t n = recvfrom(sock_fd, packet_raw, sizeof(packet_raw), 0, 
//                              (struct sockaddr *)&client_addr, &addr_len);
//         if (n < 0) {
//             if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
//             perror("[ERROR] recvfrom failed");
//             return -1;
//         } 
        
//         if(!isFirstGetData)
//         {
//             isFirstGetData = 1;
//             t0 = now_ms();
//         }

//         // 1. 헤더 파싱 (헤더는 송신측에서 htonl로 보냈으므로 ntohl 필수)
//         uint32_t *header_ptr = (uint32_t *)packet_raw;

//         uint32_t curr_dwell_id = ntohl(header_ptr[0]); 
//         uint32_t curr_id      = ntohl(header_ptr[1]);
//         uint32_t curr_count   = ntohl(header_ptr[2]);
//         uint32_t curr_payload = ntohl(header_ptr[3]);
//         uint32_t curr_fsize   = ntohl(header_ptr[4]);
//         //uint32_t curr_reserved   = ntohl(header_ptr[5]);
        
//         offset[0] = curr_dwell_id;
//         offset[1] = curr_id;
//         offset[2] = curr_count;
//         offset[3] = curr_payload;
//         offset[4] = curr_fsize;

//         if (curr_id % 500 == 0 || curr_id == 0xFFFFFFFF) 
//         {
//             printf("[RECV] DWELL ID: %u | curr ID: %u | total packets %u | Payload Size: %ld\t", curr_dwell_id, curr_id, total_packets, n-24);
//             printf("receive data size: %ld\n", n -header_size);
//         }
        
//         uint8_t *payload_ptr = packet_raw + header_size;
        
//         // 데이터 헤더 정보
//         if (curr_id == 0xFFFFFFFF) 
//         {
//             memcpy(icd_data, payload_ptr, sizeof(ICDHeader_t));
//             continue;
//         }
//         else 
//         {
//             if(total_packets == 0) 
//                 total_packets = curr_count;

//             // [일반 패킷] 절대 위치를 찾아 복사
//             // 송신 측이 파일 기준 curr_id * 1400 지점의 데이터를 보냈음.
//             size_t write_pos_bytes = (size_t)curr_id * PACKET_PAYLOAD;
//             size_t data_len = (size_t)(n - header_size);

//             // 버퍼 오버플로우 안전장치
//             if (write_pos_bytes + data_len <= total_expected_bytes) 
//             {
//                 memcpy((uint8_t*)out_buf + write_pos_bytes, payload_ptr, data_len);
//             }
//             else 
//             {
//                 printf("[ERROR] Memory Overflow! ID:%u\n", curr_id);
//             }
//             received_packet_count++;
//         }

//         // 모든 패킷을 다 받았는지 확인하여 루프 탈출
//         if (total_packets > 0 && received_packet_count >= total_packets) 
//         {
//             printf("[SUCCESS] 모든 패킷 수신 완료 (%u/%u)\n", received_packet_count, total_packets);
//             break; 
//         }
//     }

//     printf("\n");
//     printf("====================ICD_HEADER 정보====================\n");
//     printf("dwell id: %u\n", icd_data->DwellId);
//     printf("Phi: %f\n", icd_data->Phi);
//     printf("fc_Hz: %f\n", icd_data->fc_Hz);
//     printf("fs_Hz: %f\n", icd_data->fs_Hz);
//     printf("PRF_Hz: %f\n", icd_data->PRF_Hz);
//     printf("PulseWidth: %f\n", icd_data->PulseWidth);
//     printf("SweepBandwidth: %f\n", icd_data->SweepBandwidth);
//     printf("NumPulse: %u\n", icd_data->NumPulse);
//     printf("NumSample: %u\n", icd_data->NumSample);
//     printf("=======================================================\n");
//     printf("\n");
//     return 0;
// }


// int loader_thread_destroy(LoaderArgs *ld) {
//     if (ld == NULL || ld->buffer == NULL) {
//         return 0;
//     }

//     free(ld->buffer);
//     ld->buffer = NULL;

//     if (sock_fd >= 0) 
//     {
//         close(sock_fd);
//         sock_fd = -1;
//     }

//     return 0;
// }