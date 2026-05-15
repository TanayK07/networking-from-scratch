CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := tcp_opts

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c tcp_opts.c tcp_opts.h
	$(CC) $(CFLAGS) -o $@ main.c tcp_opts.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
