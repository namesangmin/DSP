CC = gcc

ARMPL_DIR ?= /opt/arm/arm-performance-libraries/

# Include 경로
CFLAGS = -O2 -Wall -std=gnu99 -D_GNU_SOURCE \
    -I./Inc \
    -I./Inc/thread \
    -I./Inc/settings \
    -I./Inc/queue \
    -I./Inc/queue/atomic \
    -I$(ARMPL_DIR)/include \
    -pthread

# ARMPL 라이브러리 경로 + 실행 시 so 찾도록 rpath 추가
LDFLAGS = -pthread \
    -L$(ARMPL_DIR)/lib \
    -Wl,-rpath,$(ARMPL_DIR)/lib

# ARMPL single-thread LP64 사용
LDLIBS = -larmpl_lp64  -lm -lfftw3

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