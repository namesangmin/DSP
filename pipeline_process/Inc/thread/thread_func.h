#ifndef THREAD_FUNCS_H
#define THREAD_FUNCS_H

#include "common.h"

void pipeline_signal_post(PipelineFile *file, int error_flag);
void *loader_thread_main(void *arg);
void *worker_thread_main(void *arg);
void *post_thread_main(void *arg);

void *loader_chunk_thread_main(void *arg);
void *worker_chunk_thread_main(void *arg);
#endif