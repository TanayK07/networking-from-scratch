# Exercises -- ICMPv4 ping clone (RFC 792)

Seven exercises, rated from one star to three stars. Each builds on the implementation in `icmp.c`.

---

### 1. Decode by hand

Given this hex dump of a captured ICMP Echo Request:

```
08 00 4d 56 00 01 00 01 61 62 63 64 65 66 67 68
```

Decode every field by hand on paper: type, code, checksum, identifier, sequence number, and payload bytes. Verify the checksum by summing all 16-bit words and confirming you get 0xFFFF (before the NOT).

---

### 2. Build an Echo Reply

Implement `nfs_icmp_build_echo_reply()` that takes a parsed Echo Request header and its payload, and constructs the corresponding Echo Reply (type 0, code 0). The reply must copy the identifier, sequence number, and payload from the request. Only the type field and checksum should differ. Write a roundtrip test: build request, build reply from it, verify all fields.

---

### 3. Parse Destination Unreachable

ICMP Destination Unreachable (type 3) has a different interpretation of bytes 4-7: the identifier and sequence fields are replaced by "unused" (or "next-hop MTU" for code 4). Extend `nfs_icmp_parse()` to handle type-3 messages, storing the next-hop MTU when `code == 4`. Write a test with a hand-crafted Destination Unreachable packet.

---

### 4. RTT measurement with timestamps

Real `ping` measures round-trip time by embedding a timestamp in the payload. Add a function `nfs_icmp_build_echo_with_timestamp()` that writes `clock_gettime(CLOCK_MONOTONIC)` into the first 16 bytes of payload. Write a companion `nfs_icmp_extract_rtt()` that reads the timestamp from a received reply and computes the elapsed time in microseconds. Test by building a request, sleeping 1 ms, then computing RTT.

---

### 5. Wireshark comparison

Capture 5 ping packets with `tcpdump -c 5 icmp -w icmp_capture.pcap`. Write a program that reads the pcap file, skips the pcap global header (24 bytes) and per-packet headers (16 bytes each), skips the Ethernet (14 bytes) and IPv4 headers (20 bytes), and parses the ICMP payload with `nfs_icmp_parse()`. Compare your decoded fields against `tshark -r icmp_capture.pcap -T fields -e icmp.type -e icmp.code -e icmp.ident -e icmp.seq`. All must match exactly.

---

### 6. Incremental checksum update

When converting an Echo Request to an Echo Reply, only the type field changes (8 to 0). Instead of recomputing the checksum from scratch, use RFC 1624 incremental update: `new_checksum = ~(~old_checksum + ~old_type + new_type)`. Implement this as `nfs_icmp_checksum_update()` and verify it produces the same result as a full recompute for 1000 random payloads.

---

### 7. Flood test and throughput benchmark

Write a benchmark that builds and validates 1 million ICMP Echo Requests in a tight loop, varying payload sizes from 0 to 1472 bytes (max ICMP payload in a standard MTU). Measure throughput in packets/second and bytes/second. Profile where time is spent: is it the checksum, the memcpy, or the function call overhead? Compare performance with and without `-O2`.
