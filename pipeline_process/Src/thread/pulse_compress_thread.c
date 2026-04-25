#include <stdio.h>
#include <complex.h>
#include <stddef.h>

#include "core_set.h"
#include "pulse_compress_thread.h"
#include "timer.h"

#if 1
void *worker_thread_main(void *arg)
{
    WorkerArgs *a = (WorkerArgs *)arg;
    PulseJob job;

    pin_thread_to_cpu(a->cpu_id);

    int num_range_bins = a->file->pc.rows;
    int num_pulses = a->file->pc.cols;
    double local_compress_ms = 0.0;  // ← 이 스레드의 압축 누적 시간

    while (pulse_queue_pop(a->q, &job)) {
        if (a->file->error) {
            break;
        }
      double t0 = now_ms();  // ← 압축 시작

        // 1. 임시 버퍼(out_buf)에 해당 펄스의 압축 결과를 받아옵니다. (Fast-time 연속)
        if (pulse_compress_one(&a->ctx, job.raw, a->ctx.out_buf) != 0) {
            fprintf(stderr, "pulse_compress_one failed: pulse_idx=%d\n", job.pulse_idx);
            // pulse_queue_close(a->even_q);
            // pulse_queue_close(a->odd_q);
            // pipeline_signal_post(a->file, 1);
            return NULL;
        }
        local_compress_ms += now_ms() - t0;  // ← 압축 끝, 누적

        // 2. 도플러 FFT를 위해 세로(Column) 방향으로 Stride Write 수행
        // CMAT_AT(pc, r, p) 구조와 완벽히 일치시킵니다.
        for (int r = 0; r < num_range_bins; ++r) {
            size_t idx = (size_t)r * (size_t)num_pulses + (size_t)job.pulse_idx;
            a->file->pc.data[idx] = a->ctx.out_buf[r];
        }

        int done = atomic_fetch_add(&a->file->done_count, 1) + 1;
        if (done == a->total_pulses) {
            //pipeline_signal_post(a->file, 0);
        }
        //local_compress_ms += now_ms() - t0;  // ← 압축 끝, 누적

    }

   a->compress_ms = local_compress_ms;  // ← WorkerArgs에 필드 추가 필요
    return NULL;
}

#endif
