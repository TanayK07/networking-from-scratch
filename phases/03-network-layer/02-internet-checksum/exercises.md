# Exercises — Internet checksum

Five exercises, ★ to ★★★. Each one has a clear goal and a test that should pass when you've solved it.

---

### ★ Compute by hand

Given the IPv4 header bytes `45 00 00 54 00 00 40 00 40 01 00 00 0a 00 00 01 0a 00 00 02`:

1. Group into 16-bit big-endian words.
2. Sum them with one's-complement carry folding.
3. Take the bitwise NOT.

Show your work. The expected answer is `0x26a7`. Use Python (`int(b, 16)`) to check yourself, then re-derive without Python.

---

### ★ Detect the off-by-one

The following implementation has a subtle bug. It returns the wrong value for any odd-length input. Find it.

```c
uint16_t broken_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i += 2) {
        sum += (data[i] << 8) | data[i + 1];   /* BUG */
    }
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum & 0xFFFF;
}
```

(Hint: what does `data[i + 1]` read when `i + 1 >= len`?)

Write a test that exercises the bug, then fix it.

---

### ★★ The pseudo-header

UDP and TCP both checksum a *pseudo-header* before the actual L4 header. For IPv4, the pseudo-header is:

```
src_ip (4) | dst_ip (4) | zero (1) | proto (1) | l4_length (2)
```

Implement `udp_checksum(src_ip, dst_ip, udp_packet, udp_len)`. Verify it against a real UDP packet captured by `tcpdump -X` — the `tcpdump -vvv` output shows whether the checksum is valid.

---

### ★★ Incremental update across an IP-address change

A NAT box translates the source IP of every outbound packet. Use `internet_checksum_update_n` to update a TCP checksum when the source IP changes from `10.0.0.5` to `203.0.113.7`.

Verify your incremental update produces the same result as recomputing from scratch over a 1500-byte buffer with random data.

---

### ★★★ Vectorize it

`internet_checksum` as written processes 2 bytes per loop iteration. Modern CPUs can do much better.

Write a vectorized version that processes 8 bytes (or 16, with SSE/AVX2) per iteration. Hint: `uint64_t` accumulators with a fold at the end work without intrinsics.

Benchmark against the scalar version on a 64KB buffer. You should see ≥4× speedup. Run with `perf stat` to confirm IPC went up.

(Bonus: read [the Linux kernel's `csum_partial`](https://elixir.bootlin.com/linux/latest/source/lib/checksum.c) and explain why it's still scalar in 2026.)
