CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Wpedantic -Werror -std=c11

BIN := test_sniff
SRC := test_sniff.c ../sniff.c

.PHONY: all clean run

all: run

$(BIN): $(SRC) ../sniff.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
