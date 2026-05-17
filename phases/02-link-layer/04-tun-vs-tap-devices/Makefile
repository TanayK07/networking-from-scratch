CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := tuntap

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c tuntap.c tuntap.h
	$(CC) $(CFLAGS) -o $@ main.c tuntap.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
