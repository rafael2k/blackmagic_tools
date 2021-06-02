CPP=g++
CC=gcc
CPPFLAGS = -Wno-multichar -fno-rtti -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -g 
CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -g
INCLUDE_FLAGS = -I../libwiav
LDFLAGS=-lm -ldl -lpthread -lwiav -lGL -lDeckLinkAPI

bm_capture: main.o aux_functions.o ring_buffer.o
	$(CPP) $(CFLAGS) $(LDFLAGS) main.o aux_functions.o ring_buffer.o -o bm_capture 

main.o: main.cpp
	$(CPP) $(CPPFLAGS) $(INCLUDE_FLAGS) main.cpp -c -o main.o

aux_functions.o: aux_functions.cpp
	$(CPP) $(CPPFLAGS) aux_functions.cpp -c -o aux_functions.o

ring_buffer.o: ring_buffer.c
	$(CC) $(CFLAGS) ring_buffer.c -c -o ring_buffer.o

install:
	install bm_capture /usr/bin

clean:
	rm -f bm_capture aux_funtions.o main.o ring_buffer.o
