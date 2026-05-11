# 02. The Internet checksum (RFC 1071)

> By the end of this lesson, you will have implemented the Internet checksum byte-by-byte and as a one-pass incremental update, and verified your output matches the kernel's for thousands of real IP headers.

## 1. Problem

Every IP header carries a 16-bit checksum. Every UDP and TCP segment carries one too (computed over a pseudo-header + the L4 header + payload). If your checksum is wrong, the receiver drops the packet silently — no error message, no retransmit, just gone.

So you need to be able to compute it correctly. And not just once: NAT boxes, TCP/IP stacks, and offload-disabled NICs recompute checksums on every modification. The "incremental update" trick (RFC 1624) lets you patch the checksum in O(1) when only one field changes.

This lesson is the canonical reference implementation, the one that everything else in P3 and P4 will use.

## 2. Theory

The Internet checksum is the **one's-complement sum of 16-bit words**, with the bitwise NOT applied at the end. It's not a CRC — it's much weaker — but it's also much faster, and that's the trade RFC 791 made in 1981 when CPUs were slow.

The algorithm in plain English:

1. Pretend the data is a sequence of 16-bit big-endian words.
2. If the data has an odd number of bytes, pad with a zero byte on the right.
3. Sum all the words. When the sum overflows 16 bits, "fold" the carry back into the low 16 bits.
4. Take the bitwise NOT (one's complement) of the result.
5. To verify: if the sum (with the existing checksum included) equals 0xFFFF, the data is intact.

The key insight: **one's complement addition** wraps differently than two's complement. When `a + b` overflows 16 bits, you don't just truncate — you add the carry bit back into the low end. This is why folding is necessary.

```
   0xFFFE + 0x0003 = 0x0001 0001       (32-bit sum)
                  →  0x0001 + 0x0001    (fold high into low)
                  =  0x0002             (one's complement result)
```

A subtle property: **the order of addition doesn't matter, and 16-bit-aligned shifts of the data don't change the result.** You can sum left-to-right or right-to-left, and you can split a buffer into chunks and add the partial sums together. This is what makes the pseudo-header trick work for UDP/TCP.

## 3. Math / Spec

> **RFC 1071, §1:** "The 16-bit one's-complement sum of all 16-bit words in the message. If the message has an odd number of bytes, pad with a zero byte on the right."

Pseudocode:

```
def checksum(data):
    s = 0
    for i in range(0, len(data) - 1, 2):
        s += (data[i] << 8) | data[i+1]      # big-endian word
    if len(data) % 2:
        s += data[-1] << 8                    # pad odd byte
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)          # fold
    return (~s) & 0xFFFF                      # one's complement
```

The verification rule:

> If you sum all 16-bit words **including** the existing checksum field, and fold, you get `0xFFFF` if the data is intact. (This is the trick the kernel uses to validate incoming packets without recomputing.)

Incremental update from RFC 1624:

> When one 16-bit word `m` is replaced by `m'`, the new checksum `HC'` can be computed from the old `HC` as:
>
>     HC' = ~(~HC + ~m + m')
>
> This avoids re-scanning the whole header. Used heavily in NAT.

## 4. Code

The implementation is split:

- [`../../../common/c/checksum.c`](../../../common/c/checksum.c) holds the canonical implementation as `internet_checksum`, `internet_checksum_partial`, `internet_checksum_fold`. Other lessons import these.
- [`incremental.c`](incremental.c) implements `internet_checksum_update` for the RFC 1624 incremental case.
- [`main.c`](main.c) is a small CLI that takes a hex string and prints its checksum, useful for debugging real captures.

The `partial` / `fold` split exists because UDP and TCP checksums span three logical regions (pseudo-header, L4 header, payload), and you don't want to copy them into one buffer just to checksum. So:

```c
uint32_t s = 0;
s = internet_checksum_partial(pseudo, 12, s);
s = internet_checksum_partial(l4_hdr, hdr_len, s);
s = internet_checksum_partial(payload, payload_len, s);
uint16_t checksum = internet_checksum_fold(s);
```

is one logical pass, with no allocation.

The incremental update does this:

```c
uint16_t internet_checksum_update(uint16_t old_check,
                                  uint16_t old_word,
                                  uint16_t new_word) {
    /* RFC 1624: HC' = ~(~HC + ~m + m'). */
    uint32_t s = (uint16_t)~old_check;
    s += (uint16_t)~old_word;
    s += new_word;
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return (uint16_t)~s;
}
```

Six instructions on x86-64. NAT boxes call this billions of times per day.

## 5. Tests

```bash
make test
```

**Unit tests** ([`tests/test_checksum.c`](tests/test_checksum.c)):

```c
/* Empty input: sum is 0, ~0 is 0xFFFF. */
ASSERT_EQ(internet_checksum("", 0), 0xFFFF);

/* From RFC 1071 §3 worked example. */
uint8_t example[] = { 0x00, 0x01, 0xF2, 0x03, 0xF4, 0xF5, 0xF6, 0xF7 };
ASSERT_EQ(internet_checksum(example, 8), 0x220D);
```

**Property tests:**

- For any data buffer, `~checksum(data) + sum_of_words(data) ≡ 0 mod 0xFFFF`.
- Splitting a buffer at any 2-byte boundary and summing the two halves gives the same result as summing the whole.

**Interop tests** ([`tests/test_against_kernel.py`](tests/test_against_kernel.py)):

For 1000 random IP headers, our output equals the value Linux's `ip` tool computes (verified by sending a packet and tcpdump-decoding it).

## 6. Exercises

See [`exercises.md`](exercises.md).
