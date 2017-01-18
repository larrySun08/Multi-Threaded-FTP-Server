CFLAGS=-Iinclude -Wall

all: build

build: clean
	mkdir obj
	gcc $(CFLAGS) -c -o obj/server.o src/server.c
	gcc $(CFLAGS) -c -o obj/client.o src/client.c
	gcc $(CFLAGS) -c -o obj/ftp.o src/ftp.c

	mkdir bin
	gcc $(CFLAGS) -o bin/ftpserver obj/ftp.o obj/server.o
	gcc $(CFLAGS) -o bin/client obj/ftp.o obj/client.o


clean:
	rm -rf bin obj
