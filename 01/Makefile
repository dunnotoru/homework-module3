CFLAGS=-fsanitize=leak -fsanitize=address -Wall -Wextra -g

build: $(wildcard *.c)
	mkdir -p build
	gcc main.c -o build/a.out

run: build
	./build/a.out