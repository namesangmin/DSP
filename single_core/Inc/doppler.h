#ifndef DOPPLER_H
#define DOPPLER_H

#include "loader.h"

int doppler_processing(const ComplexMatrix *rxsig_pc, const RadarMeta *meta, int nfft, ComplexMatrix *doppler_map);
double get_velocity_from_bin(int doppler_bin, int nfft, double prf_hz, double fc_hz);
double get_range_from_bin(int range_bin, double fs_hz);

#endif