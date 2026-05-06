#ifndef CLUSTER_H
#define CLUSTER_H

#include <stdint.h>

// =========================================================
// 구조체 정의
// =========================================================

typedef struct {
    int r;
    int d;
} ClusterNode;

typedef struct {
    int   peak_r;
    int   peak_d;
    float peak_power;
    int   size;          // 클러스터 구성 셀 수 (min_pts 필터링용)
} ClusterPeak;

typedef struct {
    float range_m;
    float velocity_mps;
    float peak_power;
    int   peak_r;
    int   peak_d;
    int   size;
} ClusterResult;

typedef struct {
    ClusterResult *items;
    int            count;
} ClusterList;

// =========================================================
// Workspace: static 전역 대신 명시적 할당
// =========================================================
typedef struct {
    uint8_t     *visited;     // 방문 플래그 [num_range * num_doppler]
    ClusterNode *queue;       // BFS 큐    [num_range * num_doppler]
    ClusterPeak *peaks;       // 클러스터 피크 버퍼
    ClusterResult *results;   // 최종 결과 버퍼

    int num_range;
    int num_doppler;
    int total;                // num_range * num_doppler

    // 레이더 파라미터
    float fs_hz;
    float c_mps;
    float prf_hz;
    float fc_hz;
    int   num_doppler_bins;   // nfft (velocity 계산용)
} ClusterWorkspace;

typedef struct {
    int range_radius;    // range 방향 BFS 반경 (bin)
    int doppler_radius;  // doppler 방향 BFS 반경 (bin)
    int min_pts;         // 최소 구성 셀 수 (이하면 노이즈로 제거)
    int max_targets;     // 최대 출력 타겟 수
    float power_ratio_min;  // 추가: 1위 대비 최소 power 비율
} ClusterParams;

// =========================================================
// API
// =========================================================
int  init_cluster_workspace(ClusterWorkspace *ws,
                             int num_range, int num_doppler,
                             float fs_hz, float c_mps,
                             float prf_hz, float fc_hz,
                             int nfft);

void cleanup_cluster_workspace(ClusterWorkspace *ws);

int  cluster_detections(const uint8_t        *det_mask,
                        const float          *power_map,
                        const ClusterParams  *params,
                        ClusterWorkspace     *ws,
                        ClusterList          *out,
                        double               *time_ms);

#endif // CLUSTER_H