#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <complex.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

#include "loader.h"

static void trim(char *s) {
    char *p = s;
    char *q;

    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    q = s + strlen(s);
    while (q > s && isspace((unsigned char)q[-1])) {
        q--;
    }
    *q = '\0';
}

int alloc_complex_matrix(int rows, int cols, ComplexMatrix *m) {
    if (!m || rows <= 0 || cols <= 0) return -1;

    m->rows = rows;
    m->cols = cols;
    m->data = (double complex *)calloc((size_t)rows * (size_t)cols, sizeof(double complex));
    return (m->data != NULL) ? 0 : -1;
}

int alloc_real_matrix(int rows, int cols, RealMatrix *m) {
    if (!m || rows <= 0 || cols <= 0) return -1;

    m->rows = rows;
    m->cols = cols;
    m->data = (double *)calloc((size_t)rows * (size_t)cols, sizeof(double));
    return (m->data != NULL) ? 0 : -1;
}

void free_complex_matrix(ComplexMatrix *m) {
    if (!m) return;
    free(m->data);
    m->data = NULL;
    m->rows = 0;
    m->cols = 0;
}

void free_real_matrix(RealMatrix *m) {
    if (!m) return;
    free(m->data);
    m->data = NULL;
    m->rows = 0;
    m->cols = 0;
}

int load_metadata(const char *path, RadarMeta *meta) {
    FILE *fp;
    char line[512];

    if (!path || !meta) return -1;

    memset(meta, 0, sizeof(*meta));

    fp = fopen(path, "r");
    if (!fp) return -1;

    while (fgets(line, sizeof(line), fp)) {
        char *comma;
        char key[256];
        char val[256];

        if (line[0] == '\0' || line[0] == '\n') continue;

        comma = strchr(line, ',');
        if (!comma) continue;

        *comma = '\0';

        strncpy(key, line, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        strncpy(val, comma + 1, sizeof(val) - 1);
        val[sizeof(val) - 1] = '\0';

        trim(key);
        trim(val);

        if (strcmp(key, "fc_Hz") == 0) meta->fc_hz = atof(val);
        else if (strcmp(key, "fs_Hz") == 0) meta->fs_hz = atof(val);
        else if (strcmp(key, "PRF_Hz") == 0) meta->prf_hz = atof(val);
        else if (strcmp(key, "PulseWidth_s") == 0) meta->pulse_width_s = atof(val);
        else if (strcmp(key, "SweepBandwidth_Hz") == 0) meta->sweep_bandwidth_hz = atof(val);
        else if (strcmp(key, "NumPulses") == 0) meta->num_pulses = atoi(val);
        else if (strcmp(key, "NumFastTimeSamples") == 0) meta->num_fast_time_samples = atoi(val);

        /* 필요하면 아래도 구조체에 추가해서 받을 수 있음
        else if (strcmp(key, "MaxUnambiguousRange_m") == 0) meta->max_unambiguous_range_m = atof(val);
        else if (strcmp(key, "Lambda_m") == 0) meta->lambda_m = atof(val);
        */
    }

    fclose(fp);

    if (meta->fc_hz <= 0.0 ||
        meta->fs_hz <= 0.0 ||
        meta->prf_hz <= 0.0 ||
        meta->pulse_width_s <= 0.0 ||
        meta->sweep_bandwidth_hz <= 0.0 ||
        meta->num_pulses <= 0 ||
        meta->num_fast_time_samples <= 0) {
        return -1;
    }

    return 0;
}

int load_complex_bin_single(const char *path, int rows, int cols, ComplexMatrix *out) {
    FILE *fp = NULL;
    struct stat st;
    const long header_size = 232;
    const size_t total_elements = (size_t)rows * (size_t)cols;
    const size_t expected_size = (size_t)header_size + total_elements * sizeof(double) * 2;

    if (!path || !out || rows <= 0 || cols <= 0) {
        fprintf(stderr, "load_complex_bin_single: invalid args rows=%d cols=%d\n", rows, cols);
        return -1;
    }

    if (alloc_complex_matrix(cols, rows, out) != 0) {
        fprintf(stderr, "load_complex_bin_single: alloc failed rows=%d cols=%d\n", rows, cols);
        return -2;
    }

    if (stat(path, &st) != 0) {
        perror("stat");
        free_complex_matrix(out);
        return -3;
    }

    fprintf(stderr, "file size=%lld expected=%zu\n",
            (long long)st.st_size, expected_size);

    if ((size_t)st.st_size != expected_size) {
        fprintf(stderr,
                "load_complex_bin_single: file size mismatch "
                "(actual=%lld, expected=%zu)\n",
                (long long)st.st_size, expected_size);
        free_complex_matrix(out);
        return -4;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        free_complex_matrix(out);
        return -5;
    }

    if (fseek(fp, header_size, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        free_complex_matrix(out);
        return -6;
    }

    /* row = pulse, col = fast-time sample */
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            double i_val, q_val;

            char a = fread(&i_val, sizeof(double), 1, fp);
            char b = fread(&q_val, sizeof(double), 1, fp);

            if (a != 1 ||
                b != 1) {
                fprintf(stderr,
                        "fread failed at row=%d col=%d pos=%ld\n",
                        r, c, ftell(fp));
                fclose(fp);
                free_complex_matrix(out);
                return -7;
            }

            out->data[(size_t)c * (size_t)rows + (size_t)r] =
                i_val + q_val * _Complex_I;
        }
    }

    fclose(fp);
    return 0;
}
