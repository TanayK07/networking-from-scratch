CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= -lm $(EXTRA_LDFLAGS)

BIN := modulation

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c modulation.c modulation.h
	$(CC) $(CFLAGS) -o $@ main.c modulation.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
