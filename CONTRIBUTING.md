# Contributing

Practical guide to adding lessons, writing code, and getting PRs merged.

## Adding a new lesson

### Directory structure

Every lesson lives under `phases/<phase>/<NN-slug>/`:

```
phases/04-transport/09-the-three-way-handshake/
├── Makefile
├── README.md          # Teaches the concept (theory, diagrams, RFC references)
├── exercises.md       # Hands-on exercises for the reader
├── tcp.h              # Public header (API, structs, constants)
├── tcp.c              # Implementation
├── main.c             # Demo / CLI entry point
└── tests/
    ├── Makefile
    └── test_tcp.c
```

- The `NN-` prefix determines ordering within a phase.
- `README.md` explains the protocol/concept. `exercises.md` gives the reader things to build.
- The binary name should be short and descriptive (`handshake`, `sniff`, `arp_responder`, `checksum`).
- Shared utilities live in `common/c/` (`checksum.c`, `tun.c`, `log.h`).

### Lesson Makefile

Follow this pattern exactly:

```makefile
CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Werror -std=c11 -I../../../common/c $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

BIN := your_binary

.PHONY: all clean test

all: $(BIN)

$(BIN): your.c your.h
	$(CC) $(CFLAGS) -o $@ your.c $(LDFLAGS)

test: all
	@$(MAKE) -C tests

clean:
	rm -f $(BIN) *.o
	@$(MAKE) -C tests clean 2>/dev/null || true
```

Rules:
- Use `?=` for `CC`, `CFLAGS`, `LDFLAGS` so CI can override them.
- Always include `$(EXTRA_CFLAGS)` at the end of `CFLAGS` and `$(EXTRA_LDFLAGS)` at the end of `LDFLAGS`. CI injects ASAN through these variables.
- Adjust the `-I` path depth to reach `common/c/` from your lesson directory.
- The `test` target must delegate to `tests/Makefile`.

### Tests Makefile

```makefile
CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Werror -std=c11 -I.. -I../../../../common/c $(EXTRA_CFLAGS)
LDFLAGS ?= $(EXTRA_LDFLAGS)

TEST_BIN := test_yourlesson

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_yourlesson.c ../yourlesson.c ../yourlesson.h
	$(CC) $(CFLAGS) -o $@ test_yourlesson.c ../yourlesson.c $(LDFLAGS)

clean:
	rm -f $(TEST_BIN) *.o
```

Tests use `-O0` (not `-O2`) so ASAN and debuggers work well.

## C code style

### Formatting

All `.c` and `.h` files are formatted with `clang-format` using the `.clang-format` at the repo root:

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Empty
AlignConsecutiveMacros: Consecutive
```

Format before committing:

```bash
find phases common/c -name '*.c' -o -name '*.h' | xargs clang-format -i --style=file
```

CI runs `clang-format-18 --dry-run --Werror` and will reject any diff.

### Naming

All public symbols use the `nfs_` prefix (Networking From Scratch):

| Kind       | Example                           |
|------------|-----------------------------------|
| Struct     | `struct nfs_tcp_hdr`              |
| Function   | `nfs_tcp_parse()`                 |
| Macro      | `NFS_TCP_SYN`                     |
| Enum type  | `enum nfs_tcp_state`              |
| Enum value | `NFS_TCP_CLOSED`                  |

Static (file-local) helpers do not need the prefix.

### Protocol structs

- Use `__attribute__((packed))` on structs that map to wire layout.
- Pin sizes with `_Static_assert`:
  ```c
  _Static_assert(sizeof(struct nfs_tcp_hdr) == 20, "nfs_tcp_hdr must be exactly 20 bytes");
  ```
- Use fixed-width types from `<stdint.h>` (`uint8_t`, `uint16_t`, `uint32_t`) for all wire fields.
- Comment every field with its meaning and byte order:
  ```c
  uint16_t src_port;    /* source port, network order */
  ```

### Language standard

C11 (`-std=c11`). No compiler extensions beyond `__attribute__((packed))`. No external dependencies beyond libc and `common/c/`.

## Test requirements

### Test harness

No external test frameworks. Each test file defines these macros and counters:

```c
static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(expr, expected)                                                                  \
    do {                                                                                           \
        tests_run++;                                                                               \
        long long _got = (long long)(expr);                                                        \
        long long _exp = (long long)(expected);                                                    \
        if (_got != _exp) {                                                                        \
            fprintf(stderr, "  FAIL %s:%d: %s == %lld, want %lld\n", __FILE__, __LINE__, #expr,    \
                    _got, _exp);                                                                   \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

#define ASSERT_TRUE(expr)                                                                          \
    do {                                                                                           \
        tests_run++;                                                                               \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "  FAIL %s:%d: %s is false\n", __FILE__, __LINE__, #expr);             \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)
```

Each test is a `static void test_xxx(void)` function called sequentially from `main()`.

### main() pattern

```c
int main(void) {
    printf("Your lesson test suite\n");
    test_struct_size();
    test_parse_known_input();
    test_roundtrip();
    /* ... */

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
```

Exit 0 on all pass, 1 on any failure. CI depends on the exit code.

### What to test

At minimum:

1. **Struct size** -- `ASSERT_EQ(sizeof(struct nfs_xxx_hdr), N)` to pin the wire layout.
2. **Parse known bytes** -- hand-craft a wire-format byte array, parse it, verify every field.
3. **Roundtrip** -- build a packet, parse it back, check fields match.
4. **Reject bad input** -- truncated buffers, NULL pointers, malformed data return errors.
5. **Protocol-specific edge cases** -- e.g., ISN randomness, checksum verification, flag combinations.

## CI

Two GitHub Actions workflows run on pushes and PRs that touch C files, headers, or Makefiles under `phases/` or `common/c/`.

### `c-build.yml` -- format check, build, ASAN

- **`lint` job**: runs `clang-format-18 --dry-run --Werror` on all `.c` and `.h` files under `phases/` and `common/c/`.
- **`build` job**: builds all lessons with both `gcc-13` and `clang-18`, each with and without AddressSanitizer. ASAN is injected via:
  ```
  EXTRA_CFLAGS="-fsanitize=address -fno-omit-frame-pointer"
  EXTRA_LDFLAGS="-fsanitize=address"
  ```
  Then runs `make test-c`.

Your lesson is automatically picked up by this workflow because it runs `make build` / `make test-c`, which walk all `phases/` directories. No workflow edits needed.

### `lessons.yml` -- per-lesson test matrix

Runs each lesson as an independent matrix entry so failures are isolated and easy to diagnose. You **must** add your lesson to this workflow manually.

### Adding your lesson to the CI matrix

Edit `.github/workflows/lessons.yml` and add an entry to `strategy.matrix.include`:

```yaml
strategy:
  fail-fast: false
  matrix:
    include:
      # ... existing entries ...
      - lesson: 03-network-layer/09-icmpv4-ping-clone
        path: phases/03-network-layer/09-icmpv4-ping-clone
```

The `lesson` field is a display name shown in the GitHub Actions UI. The `path` field is passed to `make -C <path> test`.

## PR guidelines

1. **One lesson per PR.** Include the full lesson directory (code, header, tests, Makefile, README, exercises.md) plus the `lessons.yml` matrix update.

2. **Run locally before pushing:**
   ```bash
   # Format all C code
   find phases common/c -name '*.c' -o -name '*.h' | xargs clang-format -i --style=file

   # Build your lesson
   make -C phases/XX-your-phase/NN-your-lesson

   # Run tests
   make -C phases/XX-your-phase/NN-your-lesson test

   # Run tests with ASAN
   EXTRA_CFLAGS="-fsanitize=address -fno-omit-frame-pointer" \
   EXTRA_LDFLAGS="-fsanitize=address" \
   make -C phases/XX-your-phase/NN-your-lesson test
   ```

3. **All CI checks must pass.** Both `c-build.yml` and `lessons.yml` must be green.

4. **No warnings.** We compile with `-Wall -Wextra -Wpedantic -Werror`.

5. **Self-contained lessons.** A lesson must be buildable with `make -C <lesson-dir>`. If you need a shared utility, add it to `common/c/`.

6. **Clear commit messages.** One sentence describing what the lesson teaches and what the code does.

## Other contributions

- **Bug fixes / prose improvements**: open a PR with a clear title.
- **More tests for existing lessons**: property tests, fuzz harnesses, interop tests are all welcome.
- **Translations**: submit as sibling files (`README.es.md`, `README.zh.md`). One PR per language per lesson.

## License

By contributing, you agree your work is licensed under [MIT](LICENSE).

Questions? Open a [Discussion](https://github.com/TanayK07/networking-from-scratch/discussions) or file an [issue](https://github.com/TanayK07/networking-from-scratch/issues/new/choose).
