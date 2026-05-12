CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 \
           -I../../../common/c
LDFLAGS ?=

BIN := sniff
SRC := sniff.c main.c
OBJ := $(SRC:.c=.o)

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c sniff.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: all
	$(MAKE) -C tests

clean:
	rm -f $(OBJ) $(BIN)
	-$(MAKE) -C tests clean
