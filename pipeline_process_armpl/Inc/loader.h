#ifndef LOADER_H
#define LOADER_H

//#include <complex.h>
#include <stddef.h>
#include "types.h"

int alloc_complex_matrix(int rows, int cols, ComplexMatrix *m);
void free_complex_matrix(ComplexMatrix *m);
int load_metadata(const char *path, RadarMeta *meta);

#endif
