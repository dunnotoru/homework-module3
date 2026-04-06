CFLAGS=-fsanitize=address -g -Wall -Wextra

build: main.c
	mkdir -p build
	gcc $(CFLAGS) main.c -o build/app

run: build
	./build/app

debug: build
	gdb -q -ex run ./build/app