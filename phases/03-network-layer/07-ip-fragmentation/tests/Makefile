CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Wpedantic -Werror -std=c11 -I.. $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

TEST_BIN := test_fragment
LESSON_C := ../fragment.c

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_fragment.c $(LESSON_C) ../fragment.h
	$(CC) $(CFLAGS) -o $@ test_fragment.c $(LESSON_C) $(LDFLAGS)

clean:
	rm -f $(TEST_BIN) *.o
