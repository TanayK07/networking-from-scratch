# Exercises -- IPv4 header (RFC 791)

Eight exercises, rated from one star to three stars. Each builds on the implementation in `ipv4.c`.

---

### 1. Decode by hand

Given this hex dump of a captured IPv4 header:

```
45 00 00 3c 1c 46 40 00 40 06 b1 e6 ac 10 0a 63 ac 10 0a 0c
```

Decode every field by hand on paper: version, IHL, DSCP, ECN, total length, identification, flags, fragment offset, TTL, protocol, checksum, source IP, destination IP. Verify the checksum by summing all ten 16-bit words and confirming you get 0xFFFF (before the NOT).

---

### 2. Add `nfs_ipv4_parse_addr()`

Implement the reverse of `nfs_ipv4_format_addr`: parse a dotted-decimal string like `"192.168.1.1"` and return a `uint32_t` in host byte order. Reject strings with octets > 255 or wrong format. Write three test cases (valid, too-large octet, too few dots).

---

### 3. Validate total_length against buffer

The current `nfs_ipv4_parse()` does not check whether `total_length` exceeds the actual buffer `len`. Add this validation: return a new error code `NFS_IPV4_ERR_TRUNCATED` if `hdr.total_length > len`. Write a test that catches it.

---

### 4. Support IP options (IHL > 5)

When IHL > 5, the header has `(IHL - 5) * 4` bytes of options after byte 19. Extend `nfs_ipv4_hdr` with a `uint8_t options[40]` and `uint8_t options_len` field. Update `parse` and `build` to handle the extra bytes. Verify the checksum still covers the full header.

Test with a real Router Alert option: `94 04 00 00` (type=0x94, len=4, value=0x0000).

---

### 5. Build a complete IP packet

Write a function `nfs_ipv4_build_packet(hdr, payload, payload_len, buf, buf_len)` that serializes the header followed by the payload, automatically setting `total_length = ihl*4 + payload_len`. Verify with a roundtrip test that the payload bytes survive intact.

---

### 6. Wireshark comparison

Capture 10 packets with `tcpdump -c 10 -w capture.pcap`. Write a program that reads the pcap file, extracts each IPv4 header, and parses it with `nfs_ipv4_parse()`. Compare your decoded fields against `tshark -r capture.pcap -T fields -e ip.version -e ip.hdr_len ...`. All must match exactly.

---

### 7. NAT source-IP rewrite

Implement a function that takes a wire-format IPv4 header, replaces the source IP with a new address, and updates the checksum incrementally (using `internet_checksum_update_n` from `common/c/checksum.h`). Verify that the result matches a full recompute. Benchmark: how many million rewrites per second can you do on a 1-GHz loop?

---

### 8. Fuzz the parser

Write a fuzzer that generates random 20-60 byte buffers and feeds them to `nfs_ipv4_parse()`. The function must never crash, never read out of bounds, and must return one of the defined error codes or `NFS_IPV4_OK`. Run for 10 million iterations. If you find a crash, fix it and add a regression test.
