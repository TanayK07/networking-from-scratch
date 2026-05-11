# Exercises — P1.02

## ★ Warm-up

### Exercise 1
Add `nfs_htonll` (64-bit) and `nfs_ntohll` to `htonl.h` and `htonl.c`. Test that swapping a 64-bit value twice recovers the original. POSIX-2017 doesn't include 64-bit conversions; this is a real gap libraries like `<endian.h>` (Linux) and `<machine/endian.h>` (BSD) fill.

### Exercise 2
Write a function `dump_byte_order(uint32_t x, uint8_t out[4])` that writes `x` in **network order** to the buffer, regardless of host endianness. Verify by `memcmp`-ing against the expected bytes.

## ★★ Concept

### Exercise 3
The macros `htonl`/`htons` are sometimes implemented as compile-time `#define`s and sometimes as functions. What's the practical difference? Write a 5-line example where it matters. (Hint: think about `htons(some_function_with_side_effects())`.)

### Exercise 4
On a big-endian system, all four `nfs_*` functions are the identity. Modify the code so a single `#define NFS_FORCE_SWAP` always swaps, and write a test that runs the same input through both modes and verifies they agree on a packet round-trip.

### Exercise 5
Build the binary with `gcc -O3 -S` and look at the generated assembly for `nfs_swap32` on x86-64. Does GCC emit a single `bswap` instruction, or the explicit shift sequence? What about with `-O0`? With `-march=native`?

## ★★★ Extension

### Exercise 6
Replace the bit-shift implementation of `nfs_swap32` with `__builtin_bswap32` (a GCC/Clang intrinsic). Benchmark both versions on 1 million iterations using `clock_gettime(CLOCK_MONOTONIC)`. Report the speedup. Why does the intrinsic win?

### Exercise 7
Implement a portable `unaligned_load_u32_be(const uint8_t *p)` that reads 4 bytes from an arbitrary alignment and returns them as a 32-bit big-endian integer. This is what real packet parsers do — they don't trust input alignment. Test it against a buffer at offsets 0, 1, 2, 3.

### Exercise 8
Some CPUs (older SPARC, MIPS) trap on unaligned 32-bit loads. Linux's kernel networking code uses `get_unaligned_be32()` for exactly this reason. Read the [definition](https://elixir.bootlin.com/linux/latest/source/include/asm-generic/unaligned.h) and explain in 3 sentences how it avoids the trap.

### Exercise 9
A networking service receives `0x000A0000` (decimal 655360) on the wire and prints `655360` to a log. The user expects to see `10`. What's the bug? Write the fix in two lines.

### Exercise 10
Fuzz `nfs_swap32` with random inputs for 60 seconds. Compare against `__builtin_bswap32`. The first input that disagrees is a bug — but there shouldn't be one. Use this exercise to confirm.
