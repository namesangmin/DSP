APP = radarBench

APP_OBJS = radarBench.o loader.o pulse.o cfar.o writer.o doppler_fft.o

CFLAGS += -O3 -Wall -std=c99 -fopenmp
CFLAGS += -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
LDLIBS += -lm -fopenmp

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) $(CFLAGS) -o $@ $(APP_OBJS) $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(APP)
	install -d $(DESTDIR)/usr/bin
	install -m 0755 $(APP) $(DESTDIR)/usr/bin/$(APP)

clean:
	rm -f $(APP) *.o
