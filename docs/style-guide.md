# Lesson Style Guide

Every lesson follows the same six-section format. The lint script (`tools/lint-curriculum.py`) enforces this — PRs that don't comply are rejected.

## The 6 sections

```markdown
# NN. Lesson Title

> One-sentence promise: "By the end of this lesson, you will have built X and understood Y."

## 1. Problem
## 2. Theory
## 3. Math / Spec
## 4. Code
## 5. Tests
## 6. Exercises
```

### 1. Problem

A concrete, observable problem this lesson solves. Avoid abstraction.

**Good:**
> When you `curl example.com`, your kernel sends a TCP SYN. We don't have a kernel-level TCP. So we have to build one. This lesson is the first piece: parse a TCP header from raw bytes.

**Bad:**
> In this lesson we will explore the world of transport-layer protocols and understand the principles of reliable communication.

### 2. Theory

Plain-prose explanation of the concept. Include diagrams (ASCII or links to images in the repo). Cross-link related lessons with relative paths: `[the IPv4 header](../01-the-ipv4-header/)`.

A good theory section is 200–500 words. If you need 2000, the lesson is too big — split it.

### 3. Math / Spec

The formal definition. This is where you cite the RFC, OMG spec, IEEE standard, or paper. Quote the relevant passage:

> **RFC 1071, §1**: "The 16-bit one's-complement sum of all 16-bit words in the message. If the message has an odd number of bytes, pad with a zero byte on the right."

For algorithms, give pseudocode:

```
checksum = 0
for each 16-bit word w in message:
    checksum = checksum + w
    if checksum > 0xFFFF:
        checksum = (checksum & 0xFFFF) + 1   # fold the carry
return ~checksum & 0xFFFF
```

For protocols, ASCII-diagram the wire format:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Version|  IHL  |     TOS       |          Total Length         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 4. Code

Walk through the implementation in *prose*, with short annotated code blocks. Do not paste the entire source file inline — point readers to the actual file in the same directory.

**Good:**
> The struct mirrors the RFC layout. We use `__attribute__((packed))` to suppress padding so `sizeof(ip_header_t)` equals 20:
>
> ```c
> typedef struct __attribute__((packed)) {
>     uint8_t  version_ihl;     // top 4 bits = version, bottom 4 = IHL
>     uint8_t  tos;
>     uint16_t total_length;    // network byte order
>     // ...
> } ip_header_t;
> ```
>
> We deliberately don't bit-field this — bit fields are not portable across compilers in a way that matches the wire.

**Bad:** A 200-line code dump with no commentary.

### 5. Tests

How we verify the code is correct. Four categories:

1. **Unit tests** — pin specific bytes. "Given input X, output should be Y."
2. **Property tests** — round-trip, idempotence, monotonicity. "Encode then decode should give back the input."
3. **Interop tests** — run our code against a real implementation. "Our checksum agrees with the kernel's bit-for-bit."
4. **Fuzz tests** — for parsers, an AFL++ harness. "Random bytes should never segfault."

Show the test commands:

```bash
make -C phases/03-network-layer/02-internet-checksum test
```

### 6. Exercises

5–10 graded exercises. Each one has:

- A clear goal
- A "you'll need to modify file X" pointer
- A test in `tests/` that passes when the exercise is done
- A difficulty marker:
  - **★** warm-up
  - **★★** concept
  - **★★★** extension

**Example:**

> **★★ Incremental update (RFC 1624)**
>
> Modify `internet_checksum_update()` to use the incremental-update trick from RFC 1624. When only one 16-bit word changes (e.g., the source IP after NAT), the new checksum can be computed from the old one in O(1) instead of re-scanning the whole header.
>
> The test in `tests/test_incremental.c` runs your update against a full re-checksum and asserts they match for 1000 random mutations.

---

## Prose conventions

- **No hype.** Don't write "lightning-fast" or "revolutionary" or "elegant solution." State what the code does.
- **Active voice.** "We compute the checksum." Not "The checksum is computed."
- **Imperative mood for instructions.** "Open the TUN device and set the flags." Not "You should open..."
- **Short sentences.** A 50-word sentence is too long.
- **No emoji in lesson text.** They distract from the technical content. (Repo-level docs like the README can use them sparingly.)
- **Cite sources inline.** When you make a factual claim, link to the RFC or paper.

## Code conventions

### C

- Compile clean with `-Wall -Wextra -Werror -std=c11`
- Use `stdint.h` types (`uint32_t`, not `unsigned int`)
- Use `static` for file-local symbols
- Always check return values; never ignore an error
- Use `assert()` for invariants, not error handling
- One function = one purpose; if it doesn't fit on a screen, split it
- Header files declare; source files define
- Indentation: 4 spaces, no tabs
- Brace style: opening brace on the same line for functions and control flow

### Python

- Type-annotated (`mypy --strict` clean)
- `ruff check` clean
- `from __future__ import annotations` at the top
- Use `pathlib.Path` over `os.path`
- Use `dataclasses` over `TypedDict` for structured data
- Tests use `pytest`, parametrized where it makes sense
- Async code uses `asyncio`, not `trio` or `curio`

### Both

- No external dependencies beyond what's in `tools/setup-vm.sh`
- Reproducible: a learner with a fresh VM and your lesson should get the same output you got
- Determinism over performance: a slow correct test is better than a fast flaky one
