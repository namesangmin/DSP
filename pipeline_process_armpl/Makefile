CC = gcc

# Include 경로에 settings 폴더도 추가해 줍니다. (queue 폴더가 없어졌다면 -I./Inc/queue는 빼도 무방합니다)
CFLAGS = -O2 -Wall -std=gnu99 -D_GNU_SOURCE \
    -I./Inc \
    -I./Inc/thread \
    -I./Inc/settings \
    -I./Inc/queue \
    -I./Inc/queue/atomic \
    -pthread

LDFLAGS = -pthread
LDLIBS = -lm -lfftw3

# 사진의 Src 폴더 구조에 완벽하게 맞춘 목적 파일(Object file) 목록
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
    #Src/queue/queue.o \

TARGET = radarBench

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

Src/%.o: Src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# settings 폴더 내부의 .o 파일도 지워지도록 추가
clean:
	rm -f Src/*.o Src/thread/*.o Src/queue/*.o Src/settings/*.o $(TARGET)