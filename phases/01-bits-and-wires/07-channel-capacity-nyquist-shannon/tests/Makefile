CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Werror -std=c11 -I.. $(EXTRA_CFLAGS)
LDFLAGS ?= -lm $(EXTRA_LDFLAGS)

TEST_BIN := test_channel
LESSON_C := ../channel.c

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_channel.c $(LESSON_C) ../channel.h
	$(CC) $(CFLAGS) -o $@ test_channel.c $(LESSON_C) $(LDFLAGS)

clean:
	rm -f $(TEST_BIN) *.o
