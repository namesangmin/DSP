CC = gcc
ARMPL_DIR = /opt/arm/armpl/armpl_26.01_gcc
CFLAGS = -O2 -Wall -std=gnu99 -I./Inc -I./Inc/thread -I./Inc/queue -pthread -I$(ARMPL_DIR)/include -fopenmp
LDFLAGS = -pthread
LDLIBS = -lm -lfftw3

OBJS = \
<<<<<<< Updated upstream
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
=======
    Src/radarBench.o \
    Src/loader.o \
    Src/pulse.o \
    Src/doppler_fft.o \
    Src/cfar.o \
    Src/print.o \
    Src/settings/core_set.o \
    Src/settings/pipeline_set.o \
    Src/thread/loader_thread.o \
    Src/thread/pulse_compress_thread.o \
    Src/thread/doppler_cfar_thread.o \
    Src/queue/atomic/queue_post.o \
    Src/queue/atomic/queue_pulse.o
>>>>>>> Stashed changes

TARGET = radarBench

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

Src/%.o: Src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f Src/*.o Src/thread/*.o Src/queue/*.o $(TARGET)