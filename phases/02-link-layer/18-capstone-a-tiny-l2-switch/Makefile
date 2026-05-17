CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := l2switch

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c l2switch.c l2switch.h
	$(CC) $(CFLAGS) -o $@ main.c l2switch.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
