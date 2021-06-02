CC=g++
CFLAGS=-Wno-multichar -fno-rtti -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -g 
# CFLAGS += $(CFLAGS) -g
LDFLAGS=-lm -ldl -lpthread -lGL -lDeckLinkAPI

bm_capture: main.o aux_functions.o
	$(CC) $(CFLAGS) $(LDFLAGS) main.o aux_functions.o -o bm_capture 

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp -c -o main.o

aux_functions.o: aux_functions.cpp
	$(CC) $(CFLAGS) aux_functions.cpp -c -o aux_functions.o

install:
	install bm_capture /usr/bin

clean:
	rm -f bm_capture aux_funtions.o main.o
