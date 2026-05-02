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
<<<<<<< Updated upstream

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

=======
    
    double local_compress_ms = 0.0; // 타이머 변수 복구

    while (pulse_queue_pop(a->q, &job)) {
        if (atomic_load_explicit(&a->pipe->error, memory_order_relaxed)) {
            break;
        }

        double t0 = now_ms(); // 시간 측정 시작

        const float complex *pulse_raw_ptr = &CMAT_AT(&a->pipe->raw_data, job.pulse_idx, 0);
        int curr_idx = atomic_load_explicit(&a->pipe->current_write_idx, memory_order_acquire); 

        float complex *rd_row_ptr = &CMAT_AT(&a->pipe->rd_maps[curr_idx].data, job.pulse_idx, 0);
        
        if (pulse_compress_one(&a->ctx, pulse_raw_ptr, rd_row_ptr) != 0) {
            fprintf(stderr, "pulse_compress_one failed: pulse_idx=%d\n", job.pulse_idx);
            atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
            return NULL;
        }
        local_compress_ms += now_ms() - t0; // 시간 측정 완료 및 누적
        // 데이터 쓰기가 완료되었음을 보장하기 위해 release 사용
        int done = atomic_fetch_add_explicit(&a->pipe->rd_maps[curr_idx].done_count, 1, memory_order_release) + 1;

        // printf("🛠️ [Worker %d] curr_idx: %d | 꺼낸 pulse_idx: %d | 누적 done: %d\n", 
        //        a->cpu_id, curr_idx, job.pulse_idx, done);


        if (done == a->meta->num_pulses) {
            // 내가 마지막 512번째 펄스를 끝냈다면, 이전 스레드들의 쓰기 결과를 동기화
            atomic_thread_fence(memory_order_acquire);
            
            // Transpose 없이 3번 코어(포스트)에게 번호표만 전달
            PostJob p_job = { .buffer_idx = curr_idx }; 

            if (post_queue_push(&a->pipe->post_q, p_job) != 0) {
                fprintf(stderr, "post_queue_push failed: buffer_idx=%d\n", curr_idx);
                atomic_store_explicit(&a->pipe->error, 1, memory_order_relaxed);
                return NULL;
            }
            post_queue_close(&a->pipe->post_q);
        }
    }

    printf("🔥 [Worker %d] 내가 압축에 쓴 시간: %f ms\n", a->cpu_id, local_compress_ms);
    if (a->timing) {
            if (a->timing->compress_ms < local_compress_ms) {
                a->timing->compress_ms = local_compress_ms;
            }
    }

>>>>>>> Stashed changes
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