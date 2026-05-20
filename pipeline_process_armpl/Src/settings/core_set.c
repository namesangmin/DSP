#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <pthread.h>
#include <sched.h>

#include "core_set.h"

void pin_thread_to_cpu(int cpu_id)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu_id, &set);

    if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0) 
    {
        perror("pthread_setaffinity_np");
    }

    struct sched_param sp = { .sched_priority = 99 };
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) 
    {
        perror("sched_setscheduler");
    }
}