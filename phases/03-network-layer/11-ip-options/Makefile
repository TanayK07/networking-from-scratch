CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := ip_opts

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c ip_opts.c ip_opts.h
	$(CC) $(CFLAGS) -o $@ main.c ip_opts.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
