#DESTDIR=/

CFLAGS:=$(shell dpkg-buildflags --get CPPFLAGS)
LDFLAGS:=$(shell dpkg-buildflags --get LDFLAGS)
#LIBFLAGS:=$(shell pkg-config --cflags --libs libavformat libavcodec libavutil)

all:
	g++ -O2 $(CFLAGS) $(LDFLAGS) -o rms rms.cpp -lavformat -lavcodec -lavutil
#	g++ $(CFLAGS) $(LDFLAGS) -o rms rms.cpp $(LIBFLAGS)

install:
	mkdir -p $(DESTDIR)/bin
	cp rms $(DESTDIR)/bin
	chmod 755 $(DESTDIR)/bin
	cp script/plotRms $(DESTDIR)/bin
	chmod 755 $(DESTDIR)/bin/plotRms

clean:
	rm -f rms

#	g++ -O -o rms rms.cpp `pkg-config --cflags --libs libavformat libavcodec libavutil`;


