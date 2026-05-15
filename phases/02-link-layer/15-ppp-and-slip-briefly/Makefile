CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := slip_ppp

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c slip_ppp.c slip_ppp.h
	$(CC) $(CFLAGS) -o $@ main.c slip_ppp.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
