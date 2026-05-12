CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 -I../../../common/c $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

# Shared checksum from common/c used for TCP pseudo-header checksum.
COMMON_DIR := ../../../common/c
COMMON_C   := $(COMMON_DIR)/checksum.c
LESSON_C   := tcp.c main.c

BIN := handshake

.PHONY: all clean test

all: $(BIN)

$(BIN): $(LESSON_C) $(COMMON_C) $(COMMON_DIR)/checksum.h tcp.h
	$(CC) $(CFLAGS) -o $@ $(LESSON_C) $(COMMON_C) $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
