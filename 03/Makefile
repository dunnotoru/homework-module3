CLFAGS=-fsanitize=address -Wall -Wextra -g
SRC=main.c phonebook.c linked_list.c

build: $(SRC)
	mkdir -p build
	gcc  $(CLFAGS) $(SRC) -o build/phonebook

debug: build
	gdb -q build/phonebook

run: build
	./build/phonebook