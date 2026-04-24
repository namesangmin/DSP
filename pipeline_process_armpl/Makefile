CC = gcc
ARMPL_DIR = /opt/arm/armpl/armpl_26.01_gcc
CFLAGS = -O2 -Wall -std=gnu99 -I./Inc -I./Inc/thread -I./Inc/queue -pthread -I$(ARMPL_DIR)/include -fopenmp
LDFLAGS = -pthread
LDLIBS = -lm -lfftw3

OBJS = \
	Src/radarBench.o \
	Src/loader.o \
	Src/pulse.o \
	Src/loader_mmap.o \
	Src/pulse_mmap.o \
	Src/doppler_fft.o \
	Src/cfar.o \
	Src/writer.o \
	Src/queue/queue.o \
	Src/thread/core_set.o \
	Src/thread/doppler_cfar_thread.o \
	Src/thread/loader_thread.o \
	Src/thread/pulse_compress_thread.o

TARGET = radarBench

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

Src/%.o: Src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f Src/*.o Src/thread/*.o Src/queue/*.o $(TARGET)