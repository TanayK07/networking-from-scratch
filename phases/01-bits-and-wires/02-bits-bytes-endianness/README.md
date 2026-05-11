# 02. Bits, bytes, and endianness

> By the end of this lesson, you will have reimplemented `htonl`, `htons`, `ntohl`, `ntohs` from scratch and verified their output matches `<arpa/inet.h>` bit for bit.

## 1. Problem

Every network packet has a defined byte order. The Internet uses **big-endian** — most significant byte first. Most CPUs you'll write code on are **little-endian** — least significant byte first. If you write a 32-bit integer to a packet without converting, the packet has the bytes in the wrong order, and every other host on the Internet reads it as a different number.

The standard fix: `htonl()` (host-to-network-long) and friends, defined in `<arpa/inet.h>`. They convert in one direction or the other, and they're often macros that compile to a single byte-swap instruction.

But what if you didn't have them? Could you write your own? Could you understand exactly what those macros do?

That's this lesson.

## 2. Theory

A 32-bit integer like `0x12345678` is stored in memory as four bytes: `12 34 56 78`. Whether `12` comes first or `78` comes first depends on the CPU.

```
  Big-endian (network order):     12 34 56 78
  Little-endian (most CPUs):      78 56 34 12
```

The CPU doesn't care — `int x = 0x12345678` is `0x12345678` either way. The byte order only matters when you serialize the value to a buffer (a packet, a file, a network stream). At that point, *which byte you write first* determines what other hosts will see.

There are four conversion functions in POSIX:

| Function | Purpose |
|----------|---------|
| `htonl(x)` | host → network, 32-bit (l = "long") |
| `htons(x)` | host → network, 16-bit (s = "short") |
| `ntohl(x)` | network → host, 32-bit |
| `ntohs(x)` | network → host, 16-bit |

On a big-endian CPU, all four are no-ops. On a little-endian CPU, they swap bytes.

Detecting the host's endianness at runtime is a one-liner:

```c
union { uint32_t i; uint8_t b[4]; } u = { .i = 1 };
int is_little_endian = (u.b[0] == 1);
```

If the CPU is little-endian, the byte `01` is at offset 0; if it's big-endian, it's at offset 3.

## 3. Math / Spec

A 32-bit byte swap maps the bytes `[A B C D]` to `[D C B A]`. In bitwise terms:

```
swap32(x) = ((x & 0x000000FF) << 24) |
            ((x & 0x0000FF00) <<  8) |
            ((x & 0x00FF0000) >>  8) |
            ((x & 0xFF000000) >> 24)
```

A 16-bit byte swap maps `[A B]` to `[B A]`:

```
swap16(x) = ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8)
```

GCC and Clang provide `__builtin_bswap32` / `__builtin_bswap16` that compile to a single instruction (`bswap` on x86, `rev` on ARM). The C library typically uses these underneath.

`htonl(x)`:
- on a big-endian CPU: returns `x` unchanged
- on a little-endian CPU: returns `swap32(x)`

The other three are symmetric.

## 4. Code

The implementation lives in [`htonl.c`](htonl.c) and [`htonl.h`](htonl.h).

The header detects endianness at compile time using GCC/Clang's `__BYTE_ORDER__` predefined macro:

```c
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    /* swap on this host */
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    /* identity on this host */
#else
    #error "Unknown byte order"
#endif
```

We define our functions as `nfs_htonl`, `nfs_htons`, `nfs_ntohl`, `nfs_ntohs` to avoid colliding with the system headers, then verify they match.

The byte-swap macros are written without any `#include` of `<arpa/inet.h>` — that would defeat the point. We use `stdint.h` types only.

Note: on big-endian, *all four* functions are the identity. On little-endian, all four are byte-swaps. This is why the kernel has only two real implementations and aliases the others to them.

## 5. Tests

```bash
make test
```

Runs three categories:

**Unit tests** ([`tests/test_htonl.c`](tests/test_htonl.c)) — pin specific values:

```c
ASSERT_EQ(nfs_htonl(0x12345678), 0x78563412);   /* on little-endian */
ASSERT_EQ(nfs_htons(0xABCD),     0xCDAB);
```

**Property tests** — round-trip:

```c
for (uint32_t x = 0; x < 1000000; x++) {
    ASSERT_EQ(nfs_ntohl(nfs_htonl(x)), x);
}
```

**Interop tests** ([`tests/test_htonl.py`](tests/test_htonl.py)) — verify against the kernel's `<arpa/inet.h>` by calling our binary and the system one in a subprocess and comparing every output byte.

## 6. Exercises

See [`exercises.md`](exercises.md).
