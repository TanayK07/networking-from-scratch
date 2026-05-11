CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11
LDFLAGS ?=

BIN := endian
OBJS := main.o htonl.o

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c htonl.h
	$(CC) $(CFLAGS) -c $< -o $@

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) $(OBJS)
	@$(MAKE) -C tests clean 2>/dev/null || true
