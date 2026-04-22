CC = gcc
CFLAGS = -O2 -Wall -std=c99
LDFLAGS =
LDLIBS = -lm

OBJS = radarBench.o loader.o pulse.o doppler_fft.o cfar.o writer.o

all: radarBench

radarBench: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

radarBench.o: radarBench.c timer.h loader.h pulse.h doppler_fft.h cfar.h writer.h
loader.o: loader.c loader.h
pulse.o: pulse.c pulse.h loader.h timer.h
doppler_fft.o: doppler_fft.c doppler_fft.h loader.h timer.h
cfar.o: cfar.c cfar.h loader.h doppler_fft.h
writer.o: writer.c writer.h cfar.h

clean:
	rm -f *.o radarBench
