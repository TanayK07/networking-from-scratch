CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= -lm $(EXTRA_LDFLAGS)

BIN := mtu_tool

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c mtu.c mtu.h
	$(CC) $(CFLAGS) -o $@ main.c mtu.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
