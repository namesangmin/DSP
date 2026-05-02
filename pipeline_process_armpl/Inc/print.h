#ifndef __PRINT_H__
#define __PRINT_H__

#include "cfar.h"
#include "common.h"
#include "types.h"

void print_metadata(const RadarMeta *meta);
void print_average_line(const char *name, double avg_ms);
long read_cpu_ticks(void);
void print_usage(const char *prog);
// 궤적 요약 테이블 + trajectory analysis
void print_trajectory_summary(DetectionList *history, int valid_files);

// 타이밍 평균
void print_global_average(const Accumulator *acc, int timing_files);

#endif