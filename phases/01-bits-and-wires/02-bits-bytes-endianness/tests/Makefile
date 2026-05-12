CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Werror -std=c11 -I.. $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

TEST_BIN := test_htonl

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_htonl.c ../htonl.c ../htonl.h
	$(CC) $(CFLAGS) -o $@ test_htonl.c ../htonl.c $(LDFLAGS)

clean:
	rm -f $(TEST_BIN) *.o
