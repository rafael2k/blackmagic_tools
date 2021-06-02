CC=gcc
CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -fno-stack-protector -fomit-frame-pointer -O3
# DEBUG = -DDEBUG_

all: libwiav.so.1.0

libwiav.so.1.0: libwiav.o quicklz.o
	ld -shared -soname libwiav.so.1 libwiav.o quicklz.o -o libwiav.so.1.0

libwiav.o: libwiav.c libwiav.h 
	$(CC) $(CFLAGS) $(DEBUG) libwiav.c -fPIC -c -o libwiav.o

quicklz.o: quicklz.c quicklz.h
	$(CC) $(CFLAGS) $(DEBUG) quicklz.c -fPIC -c -o quicklz.o

install64:
	ln -sf libwiav.so.1.0 libwiav.so.1
	ln -sf libwiav.so.1 libwiav.so
	cp libwiav.h /usr/include
	cp libwiav.so.1.0 /usr/lib64
	cp -d libwiav.so.1 /usr/lib64
	cp -d libwiav.so /usr/lib64

install32:
	ln -sf libwiav.so.1.0 libwiav.so.1
	ln -sf libwiav.so.1 libwiav.so
	cp libwiav.h /usr/include
	cp libwiav.so.1.0 /usr/lib
	cp -d libwiav.so.1 /usr/lib
	cp -d libwiav.so /usr/lib

clean:
	rm -f libwiav.so.1.0 libwiav.o quicklz.o *~

