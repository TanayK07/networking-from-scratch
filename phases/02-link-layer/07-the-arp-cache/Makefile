CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := arp_cache

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c arp_cache.c arp_cache.h
	$(CC) $(CFLAGS) -o $@ main.c arp_cache.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
