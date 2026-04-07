CFLAGS=-fsanitize=address -Wall -Wextra -g
build: $(wildcard *.c)
	mkdir -p build
	gcc $(CFLAGS) main.c -o build/dumbshell

run: build
	./build/dumbshell