CFLAGS=-fsanitize=address -Wall -Wextra -g

build: main.c mkdir
	gcc main.c -o build/app -lpthread

mkdir: 
	mkdir -p build

run: build
	./build/app $(ARGS)