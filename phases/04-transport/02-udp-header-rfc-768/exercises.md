# Exercises -- UDP header (RFC 768)

Seven exercises, rated by difficulty. Each builds on the lesson implementation.

---

### 1. Parse a real UDP packet from hex (★)

Use `tcpdump` to capture a DNS query:

```bash
sudo tcpdump -i eth0 -c 1 'udp port 53' -X
```

Copy the UDP portion of the hex dump (starting after the IP header). Decode every field by hand: source port, destination port, length, checksum. Feed the same hex into `./udp_parse` and verify your answers match.

---

### 2. Build and verify a DNS query (★)

Use `./udp_parse --build 12345 53 "test"` to build a UDP datagram. Then pipe the hex output back into parse mode and verify the fields round-trip correctly. Explain why the checksum is `0x0000` and when that is acceptable per RFC 768.

---

### 3. Compute the checksum by hand (★★)

Given the following UDP datagram (hex) and IPv4 addresses:

```
src_ip: 10.0.0.1    dst_ip: 10.0.0.2
UDP:    30 39 00 35 00 0d 00 00 68 65 6c 6c 6f
```

Build the 12-byte pseudo-header manually. Walk through the one's complement sum calculation step by step (16-bit words, carry folding, final complement). Compare your result with `nfs_udp_checksum_ipv4()`.

---

### 4. Add IPv6 pseudo-header support (★★)

RFC 2460 defines a different pseudo-header for IPv6 (40 bytes: 16-byte src, 16-byte dst, 32-bit length, 24 bits zero, 8-bit next header). Implement `nfs_udp_checksum_ipv6()` following the same pattern as the IPv4 version. Note: in IPv6, the UDP checksum is mandatory (not optional). Write tests with known IPv6 addresses.

---

### 5. Validate against Wireshark (★★)

Capture a real UDP packet with `tcpdump -w capture.pcap`. Open it in Wireshark and note the UDP checksum value and validation status. Extract the raw bytes (IP header + UDP datagram) and feed them into your code:

1. Parse the IPv4 header to extract `src_ip`, `dst_ip`, and the UDP payload offset.
2. Call `nfs_udp_validate()` with the extracted addresses and UDP bytes.
3. Verify your result matches Wireshark's assessment.

If running on loopback, explain why the checksum might be `0x0000` (hint: checksum offloading).

---

### 6. UDP length vs IP length mismatch (★★★)

In the real world, the UDP length field can be shorter than the IP payload length (trailing padding from the link layer). Modify `nfs_udp_parse` to accept an optional `ip_payload_len` parameter and validate that `udp.length <= ip_payload_len`. Write tests for:

- Normal case: `udp.length == ip_payload_len`
- Padded case: `udp.length < ip_payload_len` (valid, ignore trailing bytes)
- Invalid case: `udp.length > ip_payload_len` (reject)

---

### 7. UDP echo server with raw sockets (★★★)

Build a minimal UDP echo server using raw sockets (`AF_INET, SOCK_RAW, IPPROTO_UDP`):

1. Receive raw IP+UDP datagrams.
2. Parse the IP header with `nfs_ipv4_parse()` (from lesson 01).
3. Parse the UDP header with `nfs_udp_parse()`.
4. Build a response: swap src/dst ports, compute the UDP checksum with `nfs_udp_checksum_ipv4()`, build with `nfs_udp_build()`.
5. Send the response back.

Test with `nc -u <ip> <port>`. This exercises the full parse-modify-build-checksum pipeline. Note: raw sockets require `CAP_NET_RAW` or root privileges.
