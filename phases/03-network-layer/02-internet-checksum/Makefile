CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 -I../../../common/c
LDFLAGS ?=

# Lesson uses the shared internet_checksum from common/c/checksum.c
# plus a lesson-local incremental.c for RFC 1624.
COMMON_DIR := ../../../common/c
COMMON_C   := $(COMMON_DIR)/checksum.c
LESSON_C   := incremental.c main.c

BIN := checksum

.PHONY: all clean test

all: $(BIN)

$(BIN): $(LESSON_C) $(COMMON_C) $(COMMON_DIR)/checksum.h incremental.h
	$(CC) $(CFLAGS) -o $@ $(LESSON_C) $(COMMON_C) $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
