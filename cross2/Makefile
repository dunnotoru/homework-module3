CFLAGS=-fsanitize=address -Wall -Wextra -g

build: main

main: main.c mkdir
	gcc $(CFLAGS) main.c -o ./build/app

mkdir:
	mkdir -p ./build