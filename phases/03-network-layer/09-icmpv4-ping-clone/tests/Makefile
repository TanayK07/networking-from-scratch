CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Wpedantic -Werror -std=c11 -I.. -I../../../../common/c $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

TEST_BIN := test_icmp
COMMON_C := ../../../../common/c/checksum.c
LESSON_C := ../icmp.c

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_icmp.c $(COMMON_C) $(LESSON_C) ../icmp.h ../../../../common/c/checksum.h
	$(CC) $(CFLAGS) -o $@ test_icmp.c $(COMMON_C) $(LESSON_C) $(LDFLAGS)

clean:
	rm -f $(TEST_BIN) *.o
