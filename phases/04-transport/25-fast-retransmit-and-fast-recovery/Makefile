CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := fast_recov

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c fast_recovery.c fast_recovery.h
	$(CC) $(CFLAGS) -o $@ main.c fast_recovery.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
