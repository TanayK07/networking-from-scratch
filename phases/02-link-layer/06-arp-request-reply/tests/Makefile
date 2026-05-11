CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11

BIN := test_arp
SRC := test_arp.c

.PHONY: all clean run

all: run

$(BIN): $(SRC) ../arp.h
	$(CC) $(CFLAGS) -o $@ $<

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
