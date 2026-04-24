#include <stddef.h>

#include "doppler_cfar_thread.h"
#include "timer.h"

void *post_thread_main(void *arg)
{
    PostArgs *a = (PostArgs *)arg;

    pin_thread_to_cpu(a->cpu_id);
    a->status = 0;

    pthread_mutex_lock(&a->file->post_mtx);
    while (!a->file->post_ready) {
        pthread_cond_wait(&a->file->post_cv, &a->file->post_mtx);
    }
    int error_flag = a->file->error;
    pthread_mutex_unlock(&a->file->post_mtx);

    if (error_flag) {
        a->status = -1;
        return NULL;
    }
    //double doppler_firstTime = now_ms();
    if (doppler_fft_processing_ex(&a->file->pc,
                                  a->meta,
                                  a->meta->num_pulses,
                                  a->doppler,
                                  a->doppler_timing) != 0) {
        a->status = -1;
        return NULL;
    }
   // a->doppler_timing->timing = now_ms() - doppler_firstTime;


   // cfar 시간 측정
    double t0 = now_ms();

    int numTrainR = 4;
    int numTrainD = 4;
    int numGuardR = 1;
    int numGuardD = 1;
    int totalWindowCells =
        (2 * (numTrainR + numGuardR) + 1) * (2 * (numTrainD + numGuardD) + 1);
    int guardAndCUTCells =
        (2 * numGuardR + 1) * (2 * numGuardD + 1);
    int rankIdx = ((totalWindowCells - guardAndCUTCells) + 1) / 2;

    if (cfar_detect(a->doppler,
                    a->meta,
                    numTrainR,
                    numTrainD,
                    numGuardR,
                    numGuardD,
                    rankIdx,
                    9.0,
                    a->det) != 0) {
        a->status = -1;
        return NULL;
    }

    *a->cfar_ms = now_ms() - t0;
    return NULL;
}