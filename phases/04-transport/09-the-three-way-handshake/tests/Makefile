CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Werror -std=c11 -I.. -I../../../../common/c
LDFLAGS ?=

TEST_BIN := test_tcp
COMMON_C := ../../../../common/c/checksum.c
LESSON_C := ../tcp.c

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_tcp.c $(COMMON_C) $(LESSON_C) ../tcp.h ../../../../common/c/checksum.h
	$(CC) $(CFLAGS) -o $@ test_tcp.c $(COMMON_C) $(LESSON_C) $(LDFLAGS)

clean:
	rm -f $(TEST_BIN) *.o
