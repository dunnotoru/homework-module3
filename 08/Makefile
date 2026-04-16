CFLAGS= -Wall -Wextra -g

build: writer reader

reader: reader.c mkdir
	gcc $(CFLAGS) $< -o build/$@

writer: writer.c mkdir
	gcc $(CFLAGS) $< -o build/$@

mkdir:
	mkdir -p build

run-writer: writer
	./build/$<

run-reader: reader
	./build/$<