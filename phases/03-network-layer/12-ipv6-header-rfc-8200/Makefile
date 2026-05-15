CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := ipv6_parse

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c ipv6.c ipv6.h
	$(CC) $(CFLAGS) -o $@ main.c ipv6.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
