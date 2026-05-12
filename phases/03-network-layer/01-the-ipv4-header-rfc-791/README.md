# 01. The IPv4 header (RFC 791)

> By the end of this lesson you will have implemented a parser and builder for the 20-byte IPv4 header, verified your output against a real tcpdump capture, and understood every bit of the most important header on the Internet.

## 1. Problem

Every packet that crosses the Internet carries an IPv4 header. Routers read it to decide where the packet goes. Firewalls read it to decide whether the packet is allowed. NAT boxes rewrite it in transit. If you cannot parse and construct this header correctly, you cannot build anything at Layer 3 or above.

The difficulty is that the header packs 13 fields into 20 bytes, several fields share a byte or a 16-bit word via bit-level sub-fields, and all multi-byte integers are big-endian on the wire but little-endian in your CPU. Getting the bit shifts or byte order wrong means packets are silently dropped, and there is no error message to tell you why.

This lesson is the foundation for everything in P3 (fragmentation, ICMP, routing) and P4 (TCP, UDP).

## 2. Theory

The IPv4 header is defined in RFC 791 (September 1981). The minimum header is 20 bytes (IHL = 5, no options). The maximum is 60 bytes (IHL = 15, 40 bytes of options).

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Version|  IHL  |    DSCP   |ECN|         Total Length          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Identification        |Flags|    Fragment Offset      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Time to Live |    Protocol   |        Header Checksum        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Source IP Address                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Destination IP Address                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Options (if IHL > 5)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Key design decisions in RFC 791:

- **Version + IHL share byte 0.** The version is always 4 (high nibble). IHL counts 32-bit words, so `IHL * 4` gives the header length in bytes. Minimum IHL is 5 (20 bytes). This lets the receiver find where the payload starts without parsing options.

- **DSCP + ECN share byte 1.** Originally this was the Type of Service (ToS) byte. RFC 2474 redefined bits 0-5 as Differentiated Services Code Point (DSCP) and RFC 3168 defined bits 6-7 as Explicit Congestion Notification (ECN).

- **Flags + Fragment Offset share bytes 6-7.** Three flag bits (Reserved, Don't Fragment, More Fragments) sit in the high 3 bits. The 13-bit fragment offset is in units of 8 bytes, allowing offsets up to 65528.

- **Header Checksum** covers only the header, not the payload. This is a deliberate trade-off: routers decrement TTL on every hop, so the checksum must be recomputed at each hop. Checksumming only 20 bytes is fast. The payload is protected by the L4 checksum (TCP/UDP).

## 3. Math / Spec

RFC 791 field definitions, byte by byte:

| Offset | Bits  | Field          | Description                                      |
|--------|-------|----------------|--------------------------------------------------|
| 0      | 4     | Version        | Must be 4                                        |
| 0      | 4     | IHL            | Header length in 32-bit words (5-15)             |
| 1      | 6     | DSCP           | Differentiated Services Code Point               |
| 1      | 2     | ECN            | Explicit Congestion Notification                 |
| 2-3    | 16    | Total Length   | Entire datagram size in bytes (header + payload) |
| 4-5    | 16    | Identification | Fragment group ID                                |
| 6      | 3     | Flags          | Bit 0: Reserved, Bit 1: DF, Bit 2: MF           |
| 6-7    | 13    | Fragment Offset| In 8-byte units                                  |
| 8      | 8     | TTL            | Hop limit, decremented by each router            |
| 9      | 8     | Protocol       | Upper-layer protocol (1=ICMP, 6=TCP, 17=UDP)    |
| 10-11  | 16    | Header Checksum| One's complement sum of header (RFC 1071)        |
| 12-15  | 32    | Source Address | Sender's IPv4 address                            |
| 16-19  | 32    | Dest Address   | Receiver's IPv4 address                          |

The checksum algorithm (RFC 1071):

1. Zero the checksum field.
2. Treat the header as a sequence of 16-bit big-endian words.
3. Sum all words using one's complement arithmetic (fold carries back in).
4. Take the bitwise NOT.
5. To verify: sum all words including the checksum; the result should fold to 0xFFFF (internet_checksum returns 0x0000 after the NOT).

All multi-byte fields are in network byte order (big-endian) on the wire.

## 4. Code

The implementation is split into three files:

- [`ipv4.h`](ipv4.h) -- the `nfs_ipv4_hdr` struct and function declarations. Fields are stored in host byte order after parsing. No packed attribute is used; instead, parse/build handle the wire format manually for portability across compilers and architectures.

- [`ipv4.c`](ipv4.c) -- the core logic:
  - `nfs_ipv4_parse()` reads raw wire bytes, extracts every field with shifts and masks, validates version == 4, IHL >= 5, and verifies the header checksum.
  - `nfs_ipv4_build()` serializes the struct back to wire format and computes the checksum automatically.
  - `nfs_ipv4_checksum()` wraps the shared `internet_checksum()` from `common/c/checksum.c`.
  - `nfs_ipv4_format_addr()` converts a host-order 32-bit address to dotted decimal.
  - `nfs_ipv4_protocol_name()` maps protocol numbers to human-readable names.

- [`main.c`](main.c) -- a CLI tool that takes a hex string of an IPv4 header and pretty-prints all decoded fields.

The parse function extracts bit-fields like this:

```c
/* Byte 0: version (high nibble) | IHL (low nibble). */
out->version = (buf[0] >> 4) & 0x0F;
out->ihl     = buf[0] & 0x0F;

/* Bytes 6-7: flags (3 bits) | fragment offset (13 bits). */
uint16_t flags_frag = (uint16_t)((buf[6] << 8) | buf[7]);
out->flags       = (uint8_t)((flags_frag >> 13) & 0x07);
out->frag_offset = flags_frag & 0x1FFF;
```

The build function is the exact inverse, placing each field back at its wire position and computing the checksum with the checksum field zeroed:

```c
buf[10] = 0; buf[11] = 0;
uint16_t cs = internet_checksum(buf, hdr_len);
buf[10] = (uint8_t)(cs >> 8);
buf[11] = (uint8_t)(cs & 0xFF);
```

Example CLI usage:

```bash
$ ./ipv4_parse 4500005400004000400126a70a0000010a000002
IPv4 Header (20 bytes on wire)
  Version ........ 4
  IHL ............ 5 (20 bytes)
  DSCP ........... 0
  ECN ............ 0
  Total Length ... 84
  Identification . 0x0000 (0)
  Flags .......... 0x2 [DF]
  Frag Offset .... 0 (byte offset: 0)
  TTL ............ 64
  Protocol ....... ICMP (1)
  Checksum ....... 0x26a7
  Source ......... 10.0.0.1
  Destination .... 10.0.0.2
```

## 5. Tests

```bash
make test
```

The test suite ([`tests/test_ipv4.c`](tests/test_ipv4.c)) covers:

- **test_parse_real_packet** -- parse a known-good ICMP echo request header (from a real tcpdump capture: `ping 10.0.0.2` from `10.0.0.1`) and verify every field.
- **test_build_roundtrip** -- construct a header with non-trivial field values (DSCP=46, ECN=1, TCP, 192.168.x.x addresses), serialize to wire, parse back, compare all fields.
- **test_checksum_valid** -- the RFC 1071 checksum over the real packet (with checksum included) must be zero.
- **test_checksum_zero_after_include** -- the raw one's complement sum (before NOT) must be 0xFFFF.
- **test_bad_version** -- a header with version=6 returns `NFS_IPV4_ERR_VERSION`.
- **test_short_packet** -- buffers of 19 bytes and 0 bytes return `NFS_IPV4_ERR_SHORT`.
- **test_bad_checksum** -- a flipped bit in the checksum returns `NFS_IPV4_ERR_CHECKSUM`.
- **test_format_addr** -- dotted decimal for 10.0.0.1, 192.168.0.1, 255.255.255.255, 0.0.0.0.
- **test_protocol_name** -- ICMP, TCP, UDP, IPv6-in-IPv4, unknown.
- **test_build_too_small** -- build into a 19-byte buffer returns 0.
- **test_fragment_fields** -- roundtrip with MF flag set and a non-zero fragment offset.

## 6. Exercises

See [`exercises.md`](exercises.md).
