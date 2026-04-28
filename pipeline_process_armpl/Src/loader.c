#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <complex.h>
#include <sys/stat.h>
#include <errno.h>

#include "loader.h"

static void trim(char *s) 
{
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

int alloc_complex_matrix(int rows, int cols, ComplexMatrix *m) 
{
    if (!m || rows <= 0 || cols <= 0) return -1;

    m->rows = rows;
    m->cols = cols;
    m->data = (float complex *)calloc((size_t)rows * (size_t)cols, sizeof(float complex));
    return (m->data != NULL) ? 0 : -1;

}

void free_complex_matrix(ComplexMatrix *m) 
{
    if (!m) return;
    free(m->data);
    m->data = NULL;
    m->rows = 0;
    m->cols = 0;
}

int load_metadata(const char *path, RadarMeta *meta) 
{
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
    const size_t total_elements =
        (size_t)num_pulses * (size_t)num_fast_time_samples;

    if (!path || !out || num_pulses <= 0 || num_fast_time_samples <= 0) {
        fprintf(stderr, "load_complex_bin_all_fread: invalid args\n");
        return -1;
    }

    memset(out, 0, sizeof(*out));

    if (stat(path, &st) != 0) {
        perror("stat");
        return -2;
    }

    expected_size = header_offset + total_elements * sizeof(RawIQSample);

    if ((size_t)st.st_size != expected_size) {
        fprintf(stderr,
                "load_complex_bin_all_fread: file size mismatch "
                "(actual=%lld, expected=%zu)\n",
                (long long)st.st_size,
                expected_size);
        return -3;
    }

    if (alloc_complex_matrix(num_pulses, num_fast_time_samples, out) != 0) {
        fprintf(stderr, "load_complex_bin_all_fread: alloc failed\n");
        return -4;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        free_complex_matrix(out);
        return -5;
    }

    if (fseek(fp, (long)header_offset, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        free_complex_matrix(out);
        return -6;
    }

    RawIQSample *pulse_buffer = (RawIQSample *)malloc((size_t)num_fast_time_samples * sizeof(RawIQSample));

    if (!pulse_buffer) {
        fprintf(stderr, "load_complex_bin_all_fread: pulse buffer alloc failed\n");
        fclose(fp);
        free_complex_matrix(out);
        return -7;
    }

    for (int p = 0; p < num_pulses; ++p) {
        size_t read_count = fread(pulse_buffer, sizeof(RawIQSample),
                                  (size_t)num_fast_time_samples, fp);

        if (read_count != (size_t)num_fast_time_samples) {
            fprintf(stderr,
                    "load_complex_bin_all_fread: fread failed "
                    "pulse=%d read=%zu expected=%d pos=%ld\n",
                    p,
                    read_count,
                    num_fast_time_samples,
                    ftell(fp));

            free(pulse_buffer);
            fclose(fp);
            free_complex_matrix(out);
            return -8;
        }

        for (int c = 0; c < num_fast_time_samples; ++c) {
            CMAT_AT(out, p, c) =
                (float)pulse_buffer[c].i + (float)pulse_buffer[c].q * I;
        }
    }

    free(pulse_buffer);
    fclose(fp);

    return 0;
}
