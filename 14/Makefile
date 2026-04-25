CFLAGS=-fsanitize=address -Wall -Wextra -g

build: main.c build-dir
	gcc $(CFLAGS) main.c -o build/app

build-dir:
	mkdir -p build
	
run: build
	./build/app

