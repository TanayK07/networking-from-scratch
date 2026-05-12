CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Wpedantic -Werror -std=c11 -I.. -I../../../../common/c
LDFLAGS ?=

TEST_BIN := test_ipv4
COMMON_C := ../../../../common/c/checksum.c
LESSON_C := ../ipv4.c

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_ipv4.c $(COMMON_C) $(LESSON_C) ../ipv4.h ../../../../common/c/checksum.h
	$(CC) $(CFLAGS) -o $@ test_ipv4.c $(COMMON_C) $(LESSON_C) $(LDFLAGS)

clean:
	rm -f $(TEST_BIN) *.o
