CC = gcc
CFLAGS = -O2 -Wall -std=c99 -I./Inc
LDFLAGS =
LDLIBS = -lm -lfftw3

OBJS = \
	Src/radarBench.o \
	Src/loader.o \
	Src/pulse.o \
	Src/doppler_fft.o \
	Src/cfar.o \
	Src/writer.o

TARGET = radarBench

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

Src/radarBench.o: Src/radarBench.c Inc/timer.h Inc/loader.h Inc/pulse.h Inc/doppler_fft.h Inc/cfar.h Inc/writer.h
Src/loader.o: Src/loader.c Inc/loader.h
Src/pulse.o: Src/pulse.c Inc/pulse.h Inc/loader.h Inc/timer.h
Src/doppler_fft.o: Src/doppler_fft.c Inc/doppler_fft.h Inc/loader.h Inc/timer.h
Src/cfar.o: Src/cfar.c Inc/cfar.h Inc/loader.h Inc/doppler_fft.h
Src/writer.o: Src/writer.c Inc/writer.h Inc/cfar.h

clean:
	rm -f Src/*.o $(TARGET)