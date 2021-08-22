CFLAGS:=$(shell dpkg-buildflags --get CPPFLAGS)
LDFLAGS:=$(shell dpkg-buildflags --get LDFLAGS)

all:
	g++ $(CFLAGS) $(LDFLAGS) -o rms rms.cpp -lavformat -lavcodec -lavutil

install:
	mkdir -p $(DESTDIR)/bin
	cp rms $(DESTDIR)/bin
	chmod 755 $(DESTDIR)/bin

clean:
	rm -f rms
