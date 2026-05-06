#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "cluster.h"

static inline float calc_range_m(int r_bin, float fs_hz, float c_mps)
{
    return ((float)r_bin) * c_mps / (2.0f * fs_hz);
}

static inline float calc_velocity_mps(int d_bin, int nfft, float prf_hz, float fc_hz)
{
    const float c = 299792458.0f;
    float lambda = c / fc_hz;
    float fd = ((float)d_bin - (float)(nfft / 2)) * (prf_hz / (float)nfft);
    return fd * lambda / 2.0f;
}

int init_cluster_workspace(ClusterWorkspace *ws,
                            int num_range, int num_doppler,
                            float fs_hz, float c_mps,
                            float prf_hz, float fc_hz,
                            int nfft)
{
    if (!ws || num_range <= 0 || num_doppler <= 0) return -1;

    memset(ws, 0, sizeof(*ws));

    ws->num_range        = num_range;
    ws->num_doppler      = num_doppler;
    ws->total            = num_range * num_doppler;
    ws->fs_hz            = fs_hz;
    ws->c_mps            = c_mps;
    ws->prf_hz           = prf_hz;
    ws->fc_hz            = fc_hz;
    ws->num_doppler_bins = nfft;

    ws->visited = (uint8_t      *)malloc((size_t)ws->total * sizeof(uint8_t));
    ws->queue   = (ClusterNode  *)malloc((size_t)ws->total * sizeof(ClusterNode));
    ws->peaks   = (ClusterPeak  *)malloc((size_t)ws->total * sizeof(ClusterPeak));
    ws->results = (ClusterResult*)malloc((size_t)ws->total * sizeof(ClusterResult));

    if (!ws->visited || !ws->queue || !ws->peaks || !ws->results) {
        cleanup_cluster_workspace(ws);
        return -1;
    }

    return 0;
}

void cleanup_cluster_workspace(ClusterWorkspace *ws)
{
    if (!ws) return;
    free(ws->visited);
    free(ws->queue);
    free(ws->peaks);
    free(ws->results);
    memset(ws, 0, sizeof(*ws));
}

static void partial_sort_desc(ClusterPeak *arr, int n, int k)
{
    if (k > n) k = n;

    for (int i = 0; i < k; i++) {
        int max_idx = i;
        for (int j = i + 1; j < n; j++) {
            if (arr[j].peak_power > arr[max_idx].peak_power)
                max_idx = j;
        }
        if (max_idx != i) {
            ClusterPeak tmp = arr[i];
            arr[i]          = arr[max_idx];
            arr[max_idx]    = tmp;
        }
    }
}

int cluster_detections(const uint8_t       *det_mask,
                       const float         *power_map,
                       const ClusterParams *params,
                       ClusterWorkspace    *ws,
                       ClusterList         *out,
                       double              *time_ms)
{
    struct timespec ts0, ts1;
    clock_gettime(CLOCK_MONOTONIC, &ts0);

    if (!det_mask || !power_map || !params || !ws || !out) return -1;

    const int NR = ws->num_range;
    const int ND = ws->num_doppler;
    const int rr = params->range_radius;
    const int dr = params->doppler_radius;

    memset(ws->visited, 0, (size_t)ws->total * sizeof(uint8_t));

    int cluster_count = 0;

    for (int r = 0; r < NR; r++) {
        for (int d = 0; d < ND; d++) {
            int idx = r * ND + d;

            if (!det_mask[idx] || ws->visited[idx]) continue;

            int   front   = 0, rear  = 0;
            int   peak_r  = r,  peak_d = d;
            float peak_pw = power_map[idx];
            int   size    = 0;

            ws->visited[idx]  = 1;
            ws->queue[rear++] = (ClusterNode){ r, d };

            while (front < rear) {
                ClusterNode cur     = ws->queue[front++];
                int         cur_idx = cur.r * ND + cur.d;
                float       cur_pw  = power_map[cur_idx];
                size++;

                if (cur_pw > peak_pw) {
                    peak_pw = cur_pw;
                    peak_r  = cur.r;
                    peak_d  = cur.d;
                }

                for (int dr2 = -rr; dr2 <= rr; dr2++) {
                    int nr = cur.r + dr2;
                    if (nr < 0 || nr >= NR) continue;

                    for (int dd = -dr; dd <= dr; dd++) {
                        if (dr2 == 0 && dd == 0) continue;

                        int nd = cur.d + dd;
                        if (nd < 0 || nd >= ND) continue;

                        int nidx = nr * ND + nd;

                        if (ws->visited[nidx]) continue;
                        if (!det_mask[nidx])   continue;

                        ws->visited[nidx] = 1;
                        ws->queue[rear++] = (ClusterNode){ nr, nd };
                    }
                }
            }

            if (size < params->min_pts) continue;

            ws->peaks[cluster_count++] = (ClusterPeak){
                .peak_r     = peak_r,
                .peak_d     = peak_d,
                .peak_power = peak_pw,
                .size       = size,
            };
        }
    }

    out->items = NULL;
    out->count = 0;

    if (cluster_count > 0) {
        int found = (cluster_count < params->max_targets)
                        ? cluster_count
                        : params->max_targets;

        partial_sort_desc(ws->peaks, cluster_count, found);

        // 1위 power 기준으로 비율 필터링
        float top_power = ws->peaks[0].peak_power;
        int   valid     = 0;

        for (int i = 0; i < found; i++) {
            // 1위 대비 power가 threshold 미만이면 사이드로브로 제거
            // if (ws->peaks[i].peak_power < top_power * params->power_ratio_min)
            //     break;  // partial_sort로 내림차순 정렬되어 있으므로 이후는 전부 제거

            float ratio = ws->peaks[i].peak_power / top_power;

            int same_doppler = (abs(ws->peaks[i].peak_d - ws->peaks[0].peak_d)
                                <= params->doppler_radius);

            if (ratio < params->power_ratio_min && same_doppler) {
                continue;  // 같은 속도 + 약한 power → 사이드로브 제거
            }

            ws->results[valid].peak_r       = ws->peaks[i].peak_r;
            ws->results[valid].peak_d       = ws->peaks[i].peak_d;
            ws->results[valid].peak_power   = ws->peaks[i].peak_power;
            ws->results[valid].size         = ws->peaks[i].size;
            ws->results[valid].range_m      = calc_range_m(ws->peaks[i].peak_r,
                                                            ws->fs_hz, ws->c_mps);
            ws->results[valid].velocity_mps = calc_velocity_mps(ws->peaks[i].peak_d,
                                                                ws->num_doppler_bins,
                                                                ws->prf_hz, ws->fc_hz);
            valid++;
        }

        out->items = ws->results;
        out->count = valid;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts1);
    if (time_ms) {
        long sec  = ts1.tv_sec  - ts0.tv_sec;
        long nsec = ts1.tv_nsec - ts0.tv_nsec;
        *time_ms  = (double)sec * 1000.0 + (double)nsec / 1000000.0;
    }

    return cluster_count;
}