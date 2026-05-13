# Contributing

## Adding a new lesson

Each lesson lives in `phases/<phase>/<number>-<slug>/` with this structure:

```
phases/03-network-layer/09-icmpv4-ping-clone/
├── README.md
├── ping.c
├── ping.h
├── main.c
├── Makefile
├── exercises.md
└── tests/
    ├── test_ping.c
    └── Makefile
```

### Makefile pattern

```makefile
CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 -I../../../common/c $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := ping_clone

.PHONY: all clean test

all: $(BIN)

$(BIN): ping.c main.c ping.h
	$(CC) $(CFLAGS) -o $@ ping.c main.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
```

Key conventions:
- `CFLAGS ?=` with `$(EXTRA_CFLAGS)` and `LDFLAGS ?=` with `$(EXTRA_LDFLAGS)` — required for ASAN CI builds
- `CC ?= gcc` — CI overrides with clang
- Targets: `all`, `test`, `clean`

### C code style

- `.clang-format` in repo root — run `clang-format -i *.c *.h` before committing
- `nfs_` prefix on all public symbols
- Packed structs for protocol headers with `_Static_assert` on size
- C11 standard (`-std=c11`)
- No external dependencies beyond libc and `common/c/`

### Test pattern

Tests use a simple assert macro pattern (no test framework):

```c
#define ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

int main(void) {
    // test_something...
    printf("PASS\n");
    return 0;
}
```

Test Makefile follows same pattern as lesson Makefile, including `$(EXTRA_CFLAGS)` / `$(EXTRA_LDFLAGS)`.

### Adding to CI

Add your lesson to **both** workflow matrices:

**`.github/workflows/lessons.yml`** — add to `matrix.include`:
```yaml
- lesson: 03-network-layer/09-icmpv4-ping-clone
  path: phases/03-network-layer/09-icmpv4-ping-clone
```

**`.github/workflows/c-build.yml`** — uses `make build` and `make test-c` which auto-discover lessons with Makefiles. No changes needed unless your lesson has special dependencies.

## Building and testing locally

```bash
make build          # Build all C lessons
make test-c         # Run all C tests
make test           # Run all tests (C + Python)

# Single lesson
make -C phases/03-network-layer/02-internet-checksum test
```

## Pull requests

- One lesson or one logical change per PR
- CI must pass (clang-format lint + gcc/clang builds + ASAN + all tests)
- Commit messages: `Add P3.09 ICMP ping clone` or `Fix P4.09 sequence number wrap`

## License

By contributing, you agree your contribution is licensed under [MIT](LICENSE).

Questions? Open a [Discussion](https://github.com/TanayK07/networking-from-scratch/discussions).
