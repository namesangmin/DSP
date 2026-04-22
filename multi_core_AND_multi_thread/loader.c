#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <complex.h>
#include <sys/stat.h>
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

static int load_real_csv_matrix(const char *path, int rows, int cols, double *out) {
    FILE *fp;
    char line[16384];
    int r = 0;

    fp = fopen(path, "r");
    if (!fp) return -1;

    while (r < rows && fgets(line, sizeof(line), fp)) {
        char *p = line;
        int c = 0;

        while (c < cols) {
            char *endptr;
            double v;

            while (*p == ' ' || *p == '\t') p++;
            v = strtod(p, &endptr);
            if (p == endptr) break;

            out[(size_t)r * (size_t)cols + (size_t)c] = v;
            c++;

            p = endptr;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == ',') p++;
        }

        if (c != cols) {
            fclose(fp);
            return -1;
        }

        r++;
    }

    fclose(fp);
    return (r == rows) ? 0 : -1;
}

int load_complex_csv_pair(const char *real_path,
                          const char *imag_path,
                          int rows, int cols,
                          ComplexMatrix *out) {
    double *real_buf = NULL;
    double *imag_buf = NULL;

    if (alloc_complex_matrix(rows, cols, out) != 0) return -1;

    real_buf = (double *)calloc((size_t)rows * (size_t)cols, sizeof(double));
    imag_buf = (double *)calloc((size_t)rows * (size_t)cols, sizeof(double));
    if (!real_buf || !imag_buf) {
        free(real_buf);
        free(imag_buf);
        free_complex_matrix(out);
        return -1;
    }

    if (load_real_csv_matrix(real_path, rows, cols, real_buf) != 0 ||
        load_real_csv_matrix(imag_path, rows, cols, imag_buf) != 0) {
        free(real_buf);
        free(imag_buf);
        free_complex_matrix(out);
        return -1;
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            size_t idx = (size_t)r * (size_t)cols + (size_t)c;
            out->data[idx] = real_buf[idx] + I * imag_buf[idx];
        }
    }

    free(real_buf);
    free(imag_buf);
    return 0;
}

static int load_real_bin_matrix_matlab_colmajor(const char *path,
                                                int rows, int cols,
                                                double *out) {
    FILE *fp = NULL;
    double *tmp = NULL;
    size_t total = (size_t)rows * (size_t)cols;
    size_t nread;
    long file_size;

    if (!path || !out || rows <= 0 || cols <= 0) return -1;

    fp = fopen(path, "rb");
    if (!fp) return -2;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -3;
    }

    file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        return -4;
    }

    if ((size_t)file_size != total * sizeof(double)) {
        fclose(fp);
        return -5;
    }

    rewind(fp);

    tmp = (double *)malloc(total * sizeof(double));
    if (!tmp) {
        fclose(fp);
        return -6;
    }

    nread = fread(tmp, sizeof(double), total, fp);
    fclose(fp);

    if (nread != total) {
        free(tmp);
        return -7;
    }

    /* MATLAB fwrite 기준: column-major -> C row-major */
    for (int c = 0; c < cols; ++c) {
        for (int r = 0; r < rows; ++r) {
            size_t file_idx = (size_t)c * (size_t)rows + (size_t)r;
            size_t idx = (size_t)r * (size_t)cols + (size_t)c;
            out[idx] = tmp[file_idx];
        }
    }

    free(tmp);
    return 0;
}

int load_complex_bin_pair_matlab(const char *real_path,
                                 const char *imag_path,
                                 int rows, int cols,
                                 ComplexMatrix *out) {
    double *real_buf = NULL;
    double *imag_buf = NULL;
    size_t total = (size_t)rows * (size_t)cols;

    if (alloc_complex_matrix(rows, cols, out) != 0) return -1;

    real_buf = (double *)malloc(total * sizeof(double));
    imag_buf = (double *)malloc(total * sizeof(double));
    if (!real_buf || !imag_buf) {
        free(real_buf);
        free(imag_buf);
        free_complex_matrix(out);
        return -2;
    }

    if (load_real_bin_matrix_matlab_colmajor(real_path, rows, cols, real_buf) != 0 ||
        load_real_bin_matrix_matlab_colmajor(imag_path, rows, cols, imag_buf) != 0) {
        free(real_buf);
        free(imag_buf);
        free_complex_matrix(out);
        return -3;
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            size_t idx = (size_t)r * (size_t)cols + (size_t)c;
            out->data[idx] = real_buf[idx] + I * imag_buf[idx];
        }
    }

    free(real_buf);
    free(imag_buf);
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