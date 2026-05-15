CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := selectpoll

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c selectpoll.c selectpoll.h
	$(CC) $(CFLAGS) -o $@ main.c selectpoll.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
