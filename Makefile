CFLAGS = -g -O0

.c.o:
	$(CC) -Wall $(CFLAGS) -c $*.c

all: ubootchartd_bin

ubootchartd_bin: ubootchartd_bin.o
	$(CC) -Wall $(CFLAGS) -o $@ ubootchartd_bin.o

install: ubootchartd_bin
	install -m 0755 -d $(prefix)/sbin $(prefix)/etc/ubootchart $(prefix)/doc/ubootchart
	install -m 0755 ubootchartd_bin $(prefix)/sbin
	install -m 0755 ubootchartd $(prefix)/sbin
	install -m 0644 ubootchart.conf $(prefix)/etc/ubootchart
	install -m 0755 start.sh $(prefix)/etc/ubootchart
	install -m 0755 finish.sh $(prefix)/etc/ubootchart

clean:
	rm -f *.o ubootchartd_bin

