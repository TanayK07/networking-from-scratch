CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Werror -std=c11 -I.. $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)
TEST_BIN := test_network
LESSON_C := ../network.c

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_network.c $(LESSON_C) ../network.h
	$(CC) $(CFLAGS) -o $@ test_network.c $(LESSON_C) $(LDFLAGS)

clean:
	rm -f $(TEST_BIN) *.o
