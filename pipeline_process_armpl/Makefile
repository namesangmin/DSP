CC = gcc

# ARMPL 경로 (환경에 맞춰 자동 설정하거나 기본값 사용)
ARMPL_DIR ?= /opt/arm/arm-performance-libraries

# 1. 컴파일러 옵션 최적화
# -O3: 최고 수준의 최적화 활성화
# -ffast-math: 팀원 코드의 핵심. 부동소수점 연산 속도를 극한으로 올림
# -fcx-limited-range: 복소수 곱셈/나눗셈 시 불필요한 예외 체크를 생략하여 속도 향상
# -Wdouble-promotion: 실수 연산 중 의도치 않게 double로 형변환되는 곳을 경고 (최적화용)
CFLAGS = -O3 -ffast-math -fcx-limited-range -Wall -std=gnu99 -D_GNU_SOURCE \
    -I./Inc \
    -I./Inc/thread \
    -I./Inc/settings \
    -I./Inc/queue \
    -I./Inc/queue/atomic \
    -I$(ARMPL_DIR)/include \
    -pthread

# 2. 링크 옵션
LDFLAGS = -pthread \
    -L$(ARMPL_DIR)/lib \
    -Wl,-rpath,$(ARMPL_DIR)/lib

# 3. 라이브러리 (순서 중요: 수학 라이브러리와 armpl)
LDLIBS = -larmpl_lp64 -lfftw3f -lm

OBJS = \
    Src/radarBench.o \
    Src/loader.o \
    Src/pulse.o \
    Src/doppler_fft.o \
    Src/cfar.o \
    Src/writer.o \
    Src/settings/core_set.o \
    Src/settings/pipeline_set.o \
    Src/thread/loader_thread.o \
    Src/thread/pulse_compress_thread.o \
    Src/thread/doppler_cfar_thread.o \
    Src/queue/atomic/queue_post.o \
    Src/queue/atomic/queue_pulse.o

TARGET = radarBench

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

Src/%.o: Src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f Src/*.o Src/thread/*.o Src/queue/*.o Src/queue/atomic/*.o Src/settings/*.o $(TARGET)