CPP=g++
CC=gcc
CPPFLAGS= -Wall -Wno-multichar -fno-rtti -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -g
CFLAGS= -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -g
INCLUDE_FLAGS = -I../libwiav

ARCH=64
# ARCH=32

LDFLAGS=-lm -ldl -lwiav -lpthread -lDeckLinkAPI

bm_play: main.o aux_functions.o ring_buffer.o
	$(CPP) $(CPPFLAGS) $(LDFLAGS) -DARCH$(ARCH)_ main.o aux_functions.o ring_buffer.o -o bm_play 

main.o: main.cpp
	$(CPP) $(CPPFLAGS) $(INCLUDE_FLAGS) -DARCH$(ARCH)_ main.cpp -c -o main.o

aux_functions.o: aux_functions.cpp
	$(CPP) $(CPPFLAGS) $(INCLUDE_FLAGS) -DARCH$(ARCH)_ aux_functions.cpp -c -o aux_functions.o

ring_buffer.o: ring_buffer.c
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) -DARCH$(ARCH)_ ring_buffer.c -c -o ring_buffer.o

install:
	install bm_play /usr/bin

clean:
	rm -f bm_play aux_functions.o main.o ring_buffer.o
