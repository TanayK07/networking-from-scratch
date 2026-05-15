CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := sliding_win

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c sliding_window.c sliding_window.h
	$(CC) $(CFLAGS) -o $@ main.c sliding_window.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
