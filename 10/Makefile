CFLAGS=-fsanitize=address -Wall -Werror -g -lrt

build: main.c mkdir
	gcc $(CFLAGS) main.c -o build/app

mkdir:
	mkdir -p build

run: build
	./build/app