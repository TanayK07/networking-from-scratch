# 01. What is a network?

## 1. Problem

Before diving into implementation details, you need a clear mental model of how what is a network? works. Without understanding the design rationale and tradeoffs, you will struggle to debug real systems or make informed architectural decisions.

Many engineers work with bits & wires concepts daily without understanding the mechanics underneath. This lesson builds the foundational understanding that makes everything else click.

## 2. Theory

What is a network? is a core concept in Bits & Wires. Understanding it requires grasping both the design philosophy and the implementation details.

```
 +------------------------+
 |   Application          |  HTTP, DNS, SSH, MQTT
 +------------------------+
 |   Transport            |  TCP, UDP, QUIC
 +------------------------+
 |   Internet (Network)   |  IPv4, IPv6, ICMP
 +------------------------+
 |   Link (Data Link)     |  Ethernet, Wi-Fi, PPP
 +------------------------+
        |
   Physical medium: copper, fiber, radio
```

**Encapsulation** wraps higher-layer payload in lower-layer headers:

```
  App data:                    "Hello"
  Transport:        [TCP hdr] "Hello"
  Network:   [IP hdr][TCP hdr] "Hello"
  Link: [Eth hdr][IP hdr][TCP hdr] "Hello" [FCS]
```

## 3. Math / Spec

Shannon-Hartley theorem: C = B x log2(1 + S/N), where C is channel capacity in bits/sec.

## 4. Code

```python
"""
what_is_a_network.py -- What is a network?
"""
import struct
import socket

class PacketBuilder:
    """Build and parse protocol packets for What is a network?."""

    HDR_FMT = '!BBH I'  # ver_type, flags, length, id
    HDR_SIZE = struct.calcsize(HDR_FMT)

    @staticmethod
    def build(pkt_type: int, payload: bytes, flags: int = 0, pkt_id: int = 0) -> bytes:
        ver_type = (1 << 4) | (pkt_type & 0xF)
        length = PacketBuilder.HDR_SIZE + len(payload)
        header = struct.pack(PacketBuilder.HDR_FMT, ver_type, flags, length, pkt_id)
        return header + payload

    @staticmethod
    def parse(data: bytes) -> dict:
        if len(data) < PacketBuilder.HDR_SIZE:
            raise ValueError(f"Too short: {len(data)} bytes")
        ver_type, flags, length, pkt_id = struct.unpack(
            PacketBuilder.HDR_FMT, data[:PacketBuilder.HDR_SIZE])
        return {
            'version': (ver_type >> 4) & 0xF,
            'type': ver_type & 0xF,
            'flags': flags,
            'length': length,
            'id': pkt_id,
            'payload': data[PacketBuilder.HDR_SIZE:length],
        }

    @staticmethod
    def checksum(data: bytes) -> int:
        """Internet checksum (RFC 1071)."""
        if len(data) % 2:
            data += b'\x00'
        s = sum(struct.unpack('!%dH' % (len(data) // 2), data))
        while s >> 16:
            s = (s & 0xFFFF) + (s >> 16)
        return ~s & 0xFFFF

def demo():
    payload = b"Hello from What is a network?"
    pkt = PacketBuilder.build(pkt_type=1, payload=payload, pkt_id=42)
    print(f"Built {len(pkt)}-byte packet: {pkt.hex(' ')}")

    parsed = PacketBuilder.parse(pkt)
    print(f"Parsed: ver={parsed['version']} type={parsed['type']} "
          f"flags=0x{parsed['flags']:02x} len={parsed['length']} id={parsed['id']}")
    print(f"Payload: {parsed['payload']}")
    print(f"Checksum: 0x{PacketBuilder.checksum(pkt):04x}")

if __name__ == '__main__':
    demo()
```

## 5. Tests

```python
import pytest
import struct

def test_roundtrip():
    payload = b"test data"
    pkt = PacketBuilder.build(pkt_type=1, payload=payload, pkt_id=42)
    parsed = PacketBuilder.parse(pkt)
    assert parsed['version'] == 1
    assert parsed['type'] == 1
    assert parsed['payload'] == payload
    assert parsed['id'] == 42

def test_truncated():
    with pytest.raises(ValueError):
        PacketBuilder.parse(b"\x10\x00")

def test_checksum_roundtrip():
    data = b"\x00\x01\x00\x02"
    cs = PacketBuilder.checksum(data)
    combined = data + struct.pack('!H', cs)
    assert PacketBuilder.checksum(combined) == 0

def test_empty_payload():
    pkt = PacketBuilder.build(0, b"")
    parsed = PacketBuilder.parse(pkt)
    assert parsed['length'] == PacketBuilder.HDR_SIZE
    assert parsed['payload'] == b""

def test_large_payload():
    big = b"X" * 10000
    pkt = PacketBuilder.build(1, big)
    parsed = PacketBuilder.parse(pkt)
    assert parsed['payload'] == big
```

## 6. Exercises

1. **\u2605** Parse a hex dump of a real what is a network? packet and identify every field manually.

2. **\u2605** Implement the basic parser and verify it produces byte-identical output to a reference implementation.

3. **\u2605\u2605** Add comprehensive input validation: reject packets with invalid field values and return appropriate error codes.

4. **\u2605\u2605** Handle all edge cases: minimum-size packets, maximum-size packets, optional fields, and malformed input.

5. **\u2605\u2605** Write a pcap analyzer that reads capture files and decodes what is a network? packets with full field breakdown.

6. **\u2605\u2605\u2605** Implement the complete protocol state machine. Verify all transitions with a test harness.

7. **\u2605\u2605\u2605** Benchmark parsing throughput (packets/sec) and compare to theoretical line rate.

8. **\u2605\u2605\u2605** Test against real network traffic: capture live packets and verify your parser handles all observed variations.
