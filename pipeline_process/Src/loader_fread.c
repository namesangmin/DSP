#include "loader_fread.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

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