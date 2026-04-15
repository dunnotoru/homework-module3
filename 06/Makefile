CFLAGS=-fsanitize=address -Wall -Wextra -g

build: server client

server: server.c mkdir
	gcc $(CFLAGS) $< -o build/$@

client: client.c mkdir
	gcc $(CFLAGS) $< -o build/$@

mkdir:
	mkdir -p build

run-server: server
	./build/$<

run-client: client
	./build/$<