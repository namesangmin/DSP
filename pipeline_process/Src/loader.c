#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <complex.h>
#include <sys/stat.h>
#include <errno.h>

// mmap 사용할 때
// #include <fcntl.h>
// #include <unistd.h>
// #include <sys/mman.h>

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


int load_complex_bin_all_fread(const char *path, 
                               int num_pulses, 
                               int num_fast_time_samples, 
                               size_t header_offset, 
                               ComplexMatrix *out) 
{
    FILE *fp = NULL;
    struct stat st;
    size_t expected_size;
    const size_t total_elements = (size_t)num_pulses * (size_t)num_fast_time_samples;

    if (!path || !out || num_pulses <= 0 || num_fast_time_samples <= 0) {
        fprintf(stderr, "load_complex_bin_all_fread: invalid args\n");
        return -1;
    }

    if (stat(path, &st) != 0) {
        perror("stat");
        return -2;
    }

    // 파일 사이즈 검증
    expected_size = header_offset + total_elements * sizeof(RawIQSample);
    if ((size_t)st.st_size != expected_size) {
        fprintf(stderr,
                "load_complex_bin_all_fread: file size mismatch (actual=%lld, expected=%zu)\n",
                (long long)st.st_size, expected_size);
        return -3;
    }

    // 2차원 행렬 메모리 할당 (기존 정답 코드와 동일하게 cols, rows 순서로 할당)
    if (alloc_complex_matrix(num_fast_time_samples, num_pulses, out) != 0) {
        fprintf(stderr, "load_complex_bin_all_fread: alloc failed\n");
        return -4;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        free_complex_matrix(out);
        return -5;
    }

    // 헤더 크기만큼 스킵
    if (fseek(fp, (long)header_offset, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        free_complex_matrix(out);
        return -6;
    }

    // 속도 향상을 위해 펄스(행) 하나를 통째로 읽을 버퍼 생성
    RawIQSample *pulse_buffer = (RawIQSample *)malloc(num_fast_time_samples * sizeof(RawIQSample));
    if (!pulse_buffer) {
        fprintf(stderr, "load_complex_bin_all_fread: pulse buffer alloc failed\n");
        fclose(fp);
        free_complex_matrix(out);
        return -7;
    }

    /* row = pulse, col = fast-time sample */
    for (int r = 0; r < num_pulses; r++) {
        // 1개 펄스 데이터를 한 번의 fread로 가져옴 (속도 대폭 향상)
        size_t read_count = fread(pulse_buffer, sizeof(RawIQSample), num_fast_time_samples, fp);
        
        if (read_count != (size_t)num_fast_time_samples) {
            fprintf(stderr, "fread failed at row=%d pos=%ld\n", r, ftell(fp));
            free(pulse_buffer);
            fclose(fp);
            free_complex_matrix(out);
            return -8;
        }

        // 읽어온 버퍼에서 out->data 2차원 배열로 옮겨 담기
        for (int c = 0; c < num_fast_time_samples; c++) {
            // 기존 정답 코드의 Transpose 저장 방식 유지
            out->data[(size_t)c * (size_t)num_pulses + (size_t)r] = 
                pulse_buffer[c].i + pulse_buffer[c].q * _Complex_I;
        }
    } 

    free(pulse_buffer);
    fclose(fp);
    
    return 0;
}

#if 0
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
#endif