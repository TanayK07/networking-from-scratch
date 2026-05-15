CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := tcp_csum

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c tcp_csum.c tcp_csum.h
	$(CC) $(CFLAGS) -o $@ main.c tcp_csum.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
