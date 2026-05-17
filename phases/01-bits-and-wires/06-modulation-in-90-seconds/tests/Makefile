CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Werror -std=c11 -I.. $(EXTRA_CFLAGS)
LDFLAGS ?= -lm $(EXTRA_LDFLAGS)

TEST_BIN := test_modulation
LESSON_C := ../modulation.c

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_modulation.c $(LESSON_C) ../modulation.h
	$(CC) $(CFLAGS) -o $@ test_modulation.c $(LESSON_C) $(LDFLAGS)

clean:
	rm -f $(TEST_BIN) *.o
