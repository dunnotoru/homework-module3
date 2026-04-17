CFLAGS= -Wall -Wextra -g

build: writer reader

reader: reader.c shared.c mkdir
	gcc $(CFLAGS) reader.c shared.c -o build/$@

writer: writer.c shared.c mkdir
	gcc $(CFLAGS) writer.c shared.c -o build/$@

mkdir:
	mkdir -p build

run-writer: writer
	./build/$<

run-reader: reader
	./build/$<