# Exercises — CRC-8/16/32

## 1. ★ Compute CRCs by Hand

Take the 4-byte message `0xDE 0xAD 0xBE 0xEF` and compute CRC-8 by hand using
polynomial long division over GF(2) with generator `0x07`. Show every XOR step.
Verify your answer matches `nfs_crc8()`.

## 2. ★ Verify Against zlib

Write a small program that computes CRC-32 using both `nfs_crc32()` and the
system `zlib` (`crc32()` from `<zlib.h>`). Feed it 10 different test strings and
confirm the outputs are identical for every input. Link with `-lz`.

## 3. ★★ Bit-Error Detection Limits

CRC-32 detects all single-bit errors, all double-bit errors in messages shorter
than 2^31 bits, and all burst errors up to 32 bits. Write a test that:
- Confirms all 1-bit errors in a 1000-byte message are detected.
- Confirms all 2-bit errors in a 100-byte message are detected (try all pairs).
- Finds a burst error of length 33 that goes undetected (or proves none exist
  for your test message).

## 4. ★★ CRC Lookup Table Inspector

Print the full 256-entry CRC-32 lookup table in a formatted 16x16 grid. Compare
your table to the one published in RFC 3720 (iSCSI) Appendix B. Every entry must
match exactly.

## 5. ★★ Streaming CRC

Implement `nfs_crc32_update(uint32_t crc, const uint8_t *data, size_t len)` that
lets you compute a CRC incrementally over multiple chunks without buffering the
entire message. Verify that:
```c
crc = nfs_crc32_update(0xFFFFFFFF, chunk1, len1);
crc = nfs_crc32_update(crc, chunk2, len2);
crc ^= 0xFFFFFFFF;
assert(crc == nfs_crc32(full_message, len1 + len2));
```

## 6. ★★★ Ethernet FCS End-to-End

Build a minimal Ethernet frame (destination MAC, source MAC, EtherType, payload)
and append the CRC-32 as a 4-byte FCS in little-endian order. Then simulate
the receiver: run `nfs_crc32()` over the entire frame including the FCS and
verify you get the magic residue `0x2144DF1C`. Corrupt one byte and show the
residue changes.

## 7. ★★★ CRC Forgery

Given a message and its CRC-32, compute the 4 bytes you must append to produce
any desired target CRC-32. This demonstrates that CRC is an error-detection
code, **not** a cryptographic hash — it provides no security against intentional
modification. Hint: exploit the linearity of CRC over GF(2).

## 8. ★★★ Performance Benchmark

Benchmark `nfs_crc32()` throughput in GB/s on your machine. Compare against:
1. A naive bit-by-bit implementation (no table).
2. A slice-by-4 implementation (four tables, processes 4 bytes per iteration).
3. Hardware-accelerated `_mm_crc32_u64` (if your CPU supports SSE 4.2).

Report the speedup factor for each approach on 1 MB, 100 MB, and 1 GB inputs.
