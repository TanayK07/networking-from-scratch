CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := dhcp_parse

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c dhcp.c dhcp.h
	$(CC) $(CFLAGS) -o $@ main.c dhcp.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
