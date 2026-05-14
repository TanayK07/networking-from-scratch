CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 -I../../../common/c $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

# Shared checksum from common/c used for UDP pseudo-header checksum.
COMMON_DIR := ../../../common/c
COMMON_C   := $(COMMON_DIR)/checksum.c
LESSON_C   := udp.c main.c

BIN := udp_parse

.PHONY: all clean test

all: $(BIN)

$(BIN): $(LESSON_C) $(COMMON_C) $(COMMON_DIR)/checksum.h udp.h
	$(CC) $(CFLAGS) -o $@ $(LESSON_C) $(COMMON_C) $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
