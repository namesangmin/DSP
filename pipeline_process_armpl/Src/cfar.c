#include <stdlib.h>
#include <stdint.h> // uint8_t 사용
#include <math.h>
#include <complex.h>
#include <string.h>

#include "cfar.h"
#include "doppler_fft.h"

// =========================================================
// 1. Workspace 초기화 및 해제
// =========================================================
int init_cfar_workspace(CfarWorkspace *ws, int numRange, int numDoppler)
{
    if (!ws || numRange <= 0 || numDoppler <= 0) {
        return -1;
    }

    memset(ws, 0, sizeof(*ws));

    ws->numRange = numRange;
    ws->numDoppler = numDoppler;
    ws->detCapacity = numRange * numDoppler;

    ws->powerMap = (float *)malloc((size_t)numRange * (size_t)numDoppler * sizeof(float));
    
    // 1D 슬라이딩 윈도우용 배열 할당
    ws->col_sum_outer = (float *)calloc((size_t)numDoppler, sizeof(float));
    ws->col_sum_guard = (float *)calloc((size_t)numDoppler, sizeof(float));
    
    // 타겟 유무만(0, 1) 빠르게 기록할 마스크 배열 할당 (uint8_t 사용)
    ws->det_mask = (uint8_t *)calloc((size_t)numRange * (size_t)numDoppler, sizeof(uint8_t));

    if (!ws->powerMap || !ws->col_sum_outer || !ws->col_sum_guard || !ws->det_mask) {
        if (ws->powerMap) free(ws->powerMap);
        if (ws->col_sum_outer) free(ws->col_sum_outer);
        if (ws->col_sum_guard) free(ws->col_sum_guard);
        if (ws->det_mask) free(ws->det_mask);
        memset(ws, 0, sizeof(*ws));
        return -1;
    }

    ws->detBuf = (Detection *)malloc((size_t)ws->detCapacity * sizeof(Detection));
    if (!ws->detBuf) {
        free(ws->col_sum_outer);
        free(ws->col_sum_guard);
        free(ws->powerMap);
        free(ws->det_mask);
        memset(ws, 0, sizeof(*ws));
        return -1;
    }

    return 0;
}

void cleanup_cfar_workspace(CfarWorkspace *ws)
{
    if (!ws) {
        return;
    }

    if (ws->powerMap) free(ws->powerMap);
    if (ws->col_sum_outer) free(ws->col_sum_outer);
    if (ws->col_sum_guard) free(ws->col_sum_guard);
    if (ws->det_mask) free(ws->det_mask);
    if (ws->detBuf) free(ws->detBuf);

    memset(ws, 0, sizeof(*ws));
}

void free_detection_list(DetectionList *list)
{
    if (!list) return;

    if (list->items) free(list->items);
    list->items = NULL;
    list->count = 0;
}

// =========================================================
// 수학 연산 헬퍼 함수
// =========================================================
float get_range_from_bin(int range_bin, double fs_hz) {
    const float c = 299792458.0f;
    return ((float)range_bin) * c / (2.0f * (float)fs_hz);
}

float get_velocity_from_bin(int doppler_bin, int nfft, double prf_hz, double fc_hz) {
    const float c = 299792458.0f;
    float lambda = c / (float)fc_hz;
    float fd = ((float)doppler_bin - (float)(nfft / 2)) * ((float)prf_hz / (float)nfft);
    return fd * lambda / 2.0f;
}

// =========================================================
// 2. CFAR 탐지 알고리즘 (Branchless + 1D Sliding Window)
// =========================================================
int cfar_detect(const ComplexMatrix *doppler_map,
                const RadarMeta *meta,
                CfarWorkspace *ws,
                DetectionList *out)
{
    if (!doppler_map || !doppler_map->data || !meta || !ws || !out) {
        return -1;
    }

    int numRange = doppler_map->rows;
    int numDoppler = doppler_map->cols;

    if (ws->numRange != numRange || ws->numDoppler != numDoppler ||
        !ws->powerMap || !ws->col_sum_outer || !ws->col_sum_guard ||
        !ws->det_mask || !ws->detBuf) {
        return -1;
    }

    out->count = 0;
    int detCount = 0;

    float *powerMap = ws->powerMap;
    float *col_sum_outer = ws->col_sum_outer;
    float *col_sum_guard = ws->col_sum_guard;
    uint8_t *det_mask = ws->det_mask;
    Detection *detBuf = ws->detBuf;

    // [최적화] 전역 변수로 있던 설정값들을 지역 변수로 이동 (속도 증가 & 스레드 충돌 방지)
    const int numTrainR = 4;
    const int numTrainD = 4;
    const int numGuardR = 1;
    const int numGuardD = 1;
    const float scale = 8.0f;

    int winR = numTrainR + numGuardR;
    int winD = numTrainD + numGuardD;
        
    int outer_cells = (2 * winR + 1) * (2 * winD + 1);
    int inner_cells = (2 * numGuardR + 1) * (2 * numGuardD + 1);
    int training_cells = outer_cells - inner_cells;
    
    if (training_cells <= 0) return -1;

    // 나눗셈을 단 한 번의 곱셈으로 치환
    float final_scale = scale / (float)training_cells;

    // 마스크 배열 초기화 (이전 루프의 흔적 지우기)
    memset(det_mask, 0, (size_t)numRange * numDoppler * sizeof(uint8_t));

    // ---------------------------------------------------------
    // 단계 1. 파워맵 생성
    // ---------------------------------------------------------
    for (int r = 0; r < numRange; ++r) {
        size_t pwr_base = (size_t)r * (size_t)numDoppler;
        #pragma GCC ivdep
        for (int d = 0; d < numDoppler; ++d) {
            float complex z = CMAT_AT(doppler_map, r, d);
            powerMap[pwr_base + d] = crealf(z) * crealf(z) + cimagf(z) * cimagf(z);
        }
    }

    // ---------------------------------------------------------
    // 단계 2. 첫 번째 가로 윈도우(r = winR)를 위한 세로(col_sum) 초기화
    // ---------------------------------------------------------
    int or1 = 0, or2 = 2 * winR;
    int gr1 = winR - numGuardR, gr2 = winR + numGuardR;

    memset(col_sum_outer, 0, (size_t)numDoppler * sizeof(float));
    memset(col_sum_guard, 0, (size_t)numDoppler * sizeof(float));

    for (int d = 0; d < numDoppler; ++d) {
        float sum_o = 0.0f, sum_g = 0.0f;
        for (int rr = or1; rr <= or2; ++rr) sum_o += powerMap[rr * numDoppler + d];
        for (int rr = gr1; rr <= gr2; ++rr) sum_g += powerMap[rr * numDoppler + d];
        col_sum_outer[d] = sum_o;
        col_sum_guard[d] = sum_g;
    }

    // ---------------------------------------------------------
    // 단계 3. 초고속 탐지 루프 (Branchless 마스킹 기법 적용!)
    // ---------------------------------------------------------
    for (int r = winR; r < numRange - winR; ++r) {
        
        // 세로 윈도우 슬라이딩 (맨 위 빼고, 맨 아래 더하기)
        if (r > winR) {
            int add_o = r + winR, sub_o = r - winR - 1;
            int add_g = r + numGuardR, sub_g = r - numGuardR - 1;

            #pragma GCC ivdep
            for (int d = 0; d < numDoppler; ++d) {
                col_sum_outer[d] += powerMap[add_o * numDoppler + d] - powerMap[sub_o * numDoppler + d];
                col_sum_guard[d] += powerMap[add_g * numDoppler + d] - powerMap[sub_g * numDoppler + d];
            }
        }

        // 가로 윈도우 noise 초기화
        float noise_outer = 0.0f;
        float noise_guard = 0.0f;
        for (int d = 0; d <= 2 * winD; ++d) noise_outer += col_sum_outer[d];
        for (int d = winD - numGuardD; d <= winD + numGuardD; ++d) noise_guard += col_sum_guard[d];

        // 가로 윈도우 슬라이딩
        size_t row_base = (size_t)r * numDoppler;
        for (int d = winD; d < numDoppler - winD; ++d) {
            
            float noise_sum = noise_outer - noise_guard;
            float threshold = final_scale * noise_sum;
            int idx = row_base + d;

            det_mask[idx] = (powerMap[idx] > threshold) ? 1 : 0;

            // 가로 슬라이딩 갱신
            if (d < numDoppler - winD - 1) {
                noise_outer += col_sum_outer[d + winD + 1] - col_sum_outer[d - winD];
                noise_guard += col_sum_guard[d + numGuardD + 1] - col_sum_guard[d - numGuardD];
            }
        }
    }

    // ---------------------------------------------------------
    // 단계 4. 무거운 수학 연산 몰아서 하기 (루프 분리 기법)
    // ---------------------------------------------------------
    for (int r = winR; r < numRange - winR; ++r) {
        size_t row_base = (size_t)r * numDoppler;
        for (int d = winD; d < numDoppler - winD; ++d) {
            int idx = row_base + d;
            
            // 아까 마스크에 1이라고 칠해둔 곳만 찾아내서 무거운 작업을 합니다.
            if (det_mask[idx]) {
                if (detCount >= ws->detCapacity) return -2;

                Detection det;
                det.range_bin = r;
                det.doppler_bin = d;
                // 속도를 잡아먹던 이 놈들을 진짜 타겟에만 적용합니다.
                det.range_m = get_range_from_bin(r, meta->fs_hz);
                det.velocity_mps = get_velocity_from_bin(d, numDoppler, meta->prf_hz, meta->fc_hz);
                det.power = powerMap[idx];
                det.threshold = powerMap[idx]; // 혹은 저장된 threshold 값을 써도 됨

                detBuf[detCount++] = det;
            }
        }
    }

    if (detCount == 0) {
        out->count = 0;
        out->items = NULL;
        return 0;
    }

    // 결과 복사
    out->items = (Detection *)malloc((size_t)detCount * sizeof(Detection));
    if (!out->items) {
        out->count = 0;
        return -3;
    }

    memcpy(out->items, detBuf, (size_t)detCount * sizeof(Detection));
    out->count = detCount;

    return 0;
}