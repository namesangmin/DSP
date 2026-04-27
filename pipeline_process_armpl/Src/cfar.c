#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <string.h>

#include "cfar.h"
#include "doppler_fft.h"

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
    if (!ws->powerMap) {
        return -1;
    }

    // 2D ii 배열 대신 1D 슬라이딩 윈도우용 배열 할당
    ws->col_sum_outer = (float *)calloc((size_t)numDoppler, sizeof(float));
    ws->col_sum_guard = (float *)calloc((size_t)numDoppler, sizeof(float));
    if (!ws->col_sum_outer || !ws->col_sum_guard) {
        free(ws->powerMap);
        if (ws->col_sum_outer) free(ws->col_sum_outer);
        if (ws->col_sum_guard) free(ws->col_sum_guard);
        memset(ws, 0, sizeof(*ws));
        return -1;
    }

    ws->detBuf = (Detection *)malloc((size_t)ws->detCapacity * sizeof(Detection));
    if (!ws->detBuf) {
        free(ws->col_sum_outer);
        free(ws->col_sum_guard);
        free(ws->powerMap);
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

    free(ws->powerMap);
    free(ws->col_sum_outer);
    free(ws->col_sum_guard);
    free(ws->detBuf);

    memset(ws, 0, sizeof(*ws));
}

void free_detection_list(DetectionList *list)
{
    if (!list) return;

    free(list->items);
    list->items = NULL;
    list->count = 0;
}

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

int cfar_detect(const ComplexMatrix *doppler_map,
                const RadarMeta *meta,
                int numTrainR, int numTrainD,
                int numGuardR, int numGuardD,
                int rankIdx, float scale,
                CfarWorkspace *ws,
                DetectionList *out)
{
    int numRange;
    int numDoppler;
    int winR;
    int winD;
    int detCount = 0;

    (void)rankIdx;

    if (!doppler_map || !doppler_map->data || !meta || !ws || !out) {
        return -1;
    }

    numRange = doppler_map->rows;
    numDoppler = doppler_map->cols;

    if (ws->numRange != numRange ||
        ws->numDoppler != numDoppler ||
        !ws->powerMap ||
        !ws->col_sum_outer ||
        !ws->col_sum_guard ||
        !ws->detBuf) {
        return -1;
    }

    // 루프 내 malloc 제거를 위한 초기화
    out->count = 0;

    float *powerMap = ws->powerMap;
    float *col_sum_outer = ws->col_sum_outer;
    float *col_sum_guard = ws->col_sum_guard;
    Detection *detBuf = ws->detBuf;

    // 1. 파워맵 생성 (단정밀도 f 함수 사용 완료)
    for (int r = 0; r < numRange; ++r) {
        size_t pwr_base = (size_t)r * (size_t)numDoppler;
        for (int d = 0; d < numDoppler; ++d) {
            float complex z = CMAT_AT(doppler_map, r, d);
            float re = crealf(z);
            float im = cimagf(z);
            powerMap[pwr_base + (size_t)d] = re * re + im * im;
        }
    }

    winR = numTrainR + numGuardR;
    winD = numTrainD + numGuardD;
        
    int outer_cells = (2 * winR + 1) * (2 * winD + 1);
    int inner_cells = (2 * numGuardR + 1) * (2 * numGuardD + 1);
    int training_cells = outer_cells - inner_cells;
    
    if (training_cells <= 0) {
        return -1;
    }

    // [핵심] 나눗셈을 상수화하여 단 한 번의 곱셈으로 변경
    float final_scale = scale / (float)training_cells;

    // 2. 첫 번째 세로 윈도우(r = winR)를 위한 col_sum 초기화
    int or1 = 0, or2 = 2 * winR;
    int gr1 = winR - numGuardR, gr2 = winR + numGuardR;

    memset(col_sum_outer, 0, (size_t)numDoppler * sizeof(float));
    memset(col_sum_guard, 0, (size_t)numDoppler * sizeof(float));

    for (int d = 0; d < numDoppler; ++d) {
        float sum_o = 0.0f, sum_g = 0.0f;
        for (int rr = or1; rr <= or2; ++rr) {
            sum_o += powerMap[rr * numDoppler + d];
        }
        for (int rr = gr1; rr <= gr2; ++rr) {
            sum_g += powerMap[rr * numDoppler + d];
        }
        col_sum_outer[d] = sum_o;
        col_sum_guard[d] = sum_g;
    }

    // 3. 탐지 루프 (Sliding Window)
    for (int r = winR; r < numRange - winR; ++r) {

        // r이 내려갈 때마다 세로 윈도우 슬라이딩 (맨 위 뺴고, 맨 아래 더하기)
        if (r > winR) {
            int add_o = r + winR;
            int sub_o = r - winR - 1;
            int add_g = r + numGuardR;
            int sub_g = r - numGuardR - 1;

            for (int d = 0; d < numDoppler; ++d) {
                col_sum_outer[d] += powerMap[add_o * numDoppler + d] - powerMap[sub_o * numDoppler + d];
                col_sum_guard[d] += powerMap[add_g * numDoppler + d] - powerMap[sub_g * numDoppler + d];
            }
        }

        // 특정 행(r)에서 첫 번째 가로 윈도우(d = winD)를 위한 noise 초기화
        float noise_outer = 0.0f;
        float noise_guard = 0.0f;
        
        for (int d = 0; d <= 2 * winD; ++d) {
            noise_outer += col_sum_outer[d];
        }
        for (int d = winD - numGuardD; d <= winD + numGuardD; ++d) {
            noise_guard += col_sum_guard[d];
        }

        // 가로 윈도우 슬라이딩
        for (int d = winD; d < numDoppler - winD; ++d) {
            
            float noise_sum = noise_outer - noise_guard;
            float threshold = final_scale * noise_sum;
            float cut_power = powerMap[r * numDoppler + d];

            if (cut_power > threshold) {
                if (detCount >= ws->detCapacity) {
                    return -2;
                }

                Detection det;
                det.range_bin = r;
                det.doppler_bin = d;
                det.range_m = get_range_from_bin(r, meta->fs_hz);
                det.velocity_mps = get_velocity_from_bin(d, numDoppler, meta->prf_hz, meta->fc_hz);
                det.power = cut_power;
                det.threshold = threshold;

                detBuf[detCount++] = det;
            }

            // d가 오른쪽으로 한 칸 이동하기 전에 가로 윈도우 갱신 (맨 왼쪽 빼고, 맨 오른쪽 더하기)
            if (d < numDoppler - winD - 1) {
                noise_outer += col_sum_outer[d + winD + 1] - col_sum_outer[d - winD];
                noise_guard += col_sum_guard[d + numGuardD + 1] - col_sum_guard[d - numGuardD];
            }
        }
    }

    if (detCount == 0) {
        out->count = 0;
        out->items = NULL;
        return 0;
    }

    // [수정 완료] 예전처럼 탐지된 개수만큼 동적 할당 후 복사
    out->items = (Detection *)malloc((size_t)detCount * sizeof(Detection));
    if (!out->items) {
        out->count = 0;
        return -3;
    }

    memcpy(out->items, detBuf, (size_t)detCount * sizeof(Detection));
    out->count = detCount;

    return 0;
}