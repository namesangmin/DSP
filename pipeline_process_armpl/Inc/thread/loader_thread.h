#ifndef __LOADER_THREAD_H__
#define __LOADER_THREAD_H__

#include "pipeline_set.h"
#include "types.h"
#include "common.h"
typedef struct {
    const RadarMeta *meta;
    Pipeline *pipe;  
    PipelineTiming *timing;  // cfar_ms, transpose_ms 대신
    
    float *buffer;
    int cpu_id;
} LoaderArgs;

typedef struct{
    uint32_t DwellId;
    float Phi;
    float fc_Hz;
    float fs_Hz;
    float PRF_Hz;
    float PulseWidth;
    float SweepBandwidth;
    uint32_t NumPulse;
    uint32_t NumSample;
}ICDHeader_t;

typedef struct {
    uint32_t dwell;
    uint32_t packet_id;   // 조각 번호
    uint32_t packet_count;  // 전체 패킷의 개수
    uint32_t payload;     // 이번 패킷의 순수 데이터 크기 (bytes)
    uint32_t file_size;   // 파일 전체 크기 /
    uint32_t reserved;
} packet_header_t;

int loader_thread_init(const RadarMeta *meta, LoaderArgs *ld, Pipeline *pool, uint16_t rx_port);
int loader_thread_destroy(LoaderArgs *ld);
void *loader_thread_main(void *arg);

#endif