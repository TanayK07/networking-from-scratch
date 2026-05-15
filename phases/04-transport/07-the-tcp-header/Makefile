CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := tcp_parse

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c tcp_hdr.c tcp_hdr.h
	$(CC) $(CFLAGS) -o $@ main.c tcp_hdr.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
