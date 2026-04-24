#ifndef PULSE_H
#define PULSE_H

#include "loader.h"

typedef struct {
    double filter_ready_ms;
    double compression_ms;
} PulseTiming;

int make_pulse_compression_filter(const RadarMeta *meta, int use_window, ComplexMatrix *h);
int apply_pulse_compression_fft(const ComplexMatrix *x, const ComplexMatrix *h, ComplexMatrix *y, int *mf_delay);

int pulse_compression_ex(const ComplexMatrix *x, const RadarMeta *meta, ComplexMatrix *y, PulseTiming *timing);
int pulse_compression(const ComplexMatrix *x, const RadarMeta *meta, ComplexMatrix *y);

#endif