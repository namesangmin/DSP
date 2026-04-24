#include <stdio.h>
#include <complex.h>
#include <stddef.h>

#include "common.h"
#include "queue.h"
#include "core_set.h"
#include "thread_func.h"

#if 1
void *worker_thread_main(void *arg)
{
    WorkerArgs *a = (WorkerArgs *)arg;
    PulseJob job;

    pin_thread_to_cpu(a->cpu_id);

    int num_range_bins = a->file->pc.rows;
    int num_pulses = a->file->pc.cols;

    while (pulse_queue_pop(a->q, &job)) {
        if (a->file->error) {
            break;
        }

        // 1. 임시 버퍼(out_buf)에 해당 펄스의 압축 결과를 받아옵니다. (Fast-time 연속)
        if (pulse_compress_one(&a->ctx, job.raw, a->ctx.out_buf) != 0) {
            fprintf(stderr, "pulse_compress_one failed: pulse_idx=%d\n", job.pulse_idx);
            pulse_queue_close(a->even_q);
            pulse_queue_close(a->odd_q);
            pipeline_signal_post(a->file, 1);
            return NULL;
        }

        // 2. 도플러 FFT를 위해 세로(Column) 방향으로 Stride Write 수행
        // CMAT_AT(pc, r, p) 구조와 완벽히 일치시킵니다.
        for (int r = 0; r < num_range_bins; ++r) {
            size_t idx = (size_t)r * (size_t)num_pulses + (size_t)job.pulse_idx;
            a->file->pc.data[idx] = a->ctx.out_buf[r];
        }

        int done = atomic_fetch_add(&a->file->done_count, 1) + 1;
        if (done == a->total_pulses) {
            pipeline_signal_post(a->file, 0);
        }
    }

    return NULL;
}

#endif
#if 0
void *worker_chunk_thread_main(void *arg)
{
    WorkerArgs *a = (WorkerArgs *)arg;
    PulseChunkJob job;

    pin_thread_to_cpu(a->cpu_id);

    int num_range_bins = a->file->pc.rows;
    int num_pulses = a->file->pc.cols;
    int fast_time_samples = a->meta->num_fast_time_samples;

    while (pulse_chunk_queue_pop(a->q, &job)) {
        if (a->file->error) break;

        // 청크 안에 들어있는 펄스 개수만큼 빠르게 내부 루프 처리
        for (int k = 0; k < job.count; ++k) {
            int curr_pulse_idx = job.start_idx + k;
            
            // mmap은 연속된 메모리이므로, 포인터를 샘플 길이만큼 더해서 현재 펄스 주소를 계산
            const RawIQSample *curr_raw = job.raw + (size_t)k * fast_time_samples;

            // 1. 임시 버퍼(out_buf)에 압축 결과 저장 (가로축 연속 기록, 캐시 친화적)
            if (pulse_compress_one(&a->ctx, curr_raw, a->ctx.out_buf) != 0) {
                pulse_chunk_queue_close(a->even_q);
                pulse_chunk_queue_close(a->odd_q);
                pipeline_signal_post(a->file, 1);
                return NULL;
            }

            // 2. 전체 행렬(ComplexMatrix)에 도플러 FFT 규격에 맞춰 세로로 삽입 (Stride Write)
            for (int r = 0; r < num_range_bins; ++r) {
                size_t idx = (size_t)r * (size_t)num_pulses + (size_t)curr_pulse_idx;
                a->file->pc.data[idx] = a->ctx.out_buf[r];
            }
        }

        // ⚠️ 중요: 청크 크기(job.count)만큼 한 번에 완료 카운트를 올립니다.
        int done = atomic_fetch_add(&a->file->done_count, job.count) + job.count;
        if (done == a->total_pulses) {
            pipeline_signal_post(a->file, 0);
        }
    }

    return NULL;
}
#endif