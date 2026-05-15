CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := retx_queue

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c retx_queue.c retx_queue.h
	$(CC) $(CFLAGS) -o $@ main.c retx_queue.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
