#include "pipeline_set.h"

void pipeline_signal_post(PipelineFile *file, int error_flag)
{
    pthread_mutex_lock(&file->post_mtx);

    if (error_flag) {
        file->error = 1;
    }
    file->post_ready = 1;

    pthread_cond_signal(&file->post_cv);
    pthread_mutex_unlock(&file->post_mtx);
}