CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := ipv6_addr

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c ipv6_addr.c ipv6_addr.h
	$(CC) $(CFLAGS) -o $@ main.c ipv6_addr.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
