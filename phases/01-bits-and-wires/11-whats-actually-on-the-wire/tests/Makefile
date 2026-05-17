CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Werror -std=c11 -I.. $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

TEST_BIN := test_wire
LESSON_C := ../wire.c

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_wire.c $(LESSON_C) ../wire.h
	$(CC) $(CFLAGS) -o $@ test_wire.c $(LESSON_C) $(LDFLAGS)

clean:
	rm -f $(TEST_BIN) *.o
