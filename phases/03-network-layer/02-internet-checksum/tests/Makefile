CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Werror -std=c11 -I.. -I../../../../common/c $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

TEST_BIN := test_checksum
COMMON_C := ../../../../common/c/checksum.c
LESSON_C := ../incremental.c

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_checksum.c $(COMMON_C) $(LESSON_C) ../../../../common/c/checksum.h
	$(CC) $(CFLAGS) -o $@ test_checksum.c $(COMMON_C) $(LESSON_C) $(LDFLAGS)

clean:
	rm -f $(TEST_BIN) *.o
