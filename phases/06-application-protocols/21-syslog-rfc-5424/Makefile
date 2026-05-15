CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := syslog5424

.PHONY: all clean test

all: $(BIN)

$(BIN): main.c syslog5424.c syslog5424.h
	$(CC) $(CFLAGS) -o $@ main.c syslog5424.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
