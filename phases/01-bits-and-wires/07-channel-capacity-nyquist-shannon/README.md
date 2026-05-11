# 07. Channel capacity (Nyquist + Shannon)

## 1. Problem

Before diving into implementation details, you need a clear mental model of how channel capacity (nyquist + shannon) works. Without understanding the design rationale and tradeoffs, you will struggle to debug real systems or make informed architectural decisions.

Many engineers work with bits & wires concepts daily without understanding the mechanics underneath. This lesson builds the foundational understanding that makes everything else click.

## 2. Theory

Channel capacity (Nyquist + Shannon) is a fundamental concept in bits & wires. Understanding it requires grasping both the high-level design philosophy and the low-level details.

The key insight is that networking protocols solve specific problems under specific constraints. Every field in a header, every state in a state machine, and every timer value exists for a concrete reason.

When studying channel capacity (nyquist + shannon), pay attention to:
1. **What problem does this solve?** Every protocol feature addresses a specific failure mode
2. **What are the tradeoffs?** Bandwidth vs latency, simplicity vs efficiency
3. **How does this interact with other layers?** Protocols don't exist in isolation

The design decisions here have cascading effects throughout the network stack. A seemingly minor choice (like timer granularity or buffer size) can mean the difference between a system that works at scale and one that collapses under load.

## 3. Math / Spec

Nyquist theorem (noise-free): C = 2B * log2(M) bits/sec
Shannon-Hartley (noisy): C = B * log2(1 + S/N) bits/sec

Example: voice telephone line (B=3100 Hz, SNR=30 dB):
  S/N = 10^(30/10) = 1000
  C = 3100 * log2(1001) ~ 30,900 bits/sec
  (V.34 modem at 33.6 kbps is near Shannon limit)

## 4. Code

```python
"""
channel_capacity_nyquist_shannon.py -- Channel capacity (Nyquist + Shannon)
"""
import struct
import socket

class PacketBuilder:
    """Build and parse protocol packets for Channel capacity (Nyquist + Shannon)."""

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
    payload = b"Hello from Channel capacity (Nyquist + Shannon)"
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

1. **\u2605** Parse a hex dump of a real channel capacity packet and identify every field manually.

2. **\u2605** Implement the basic parser and verify it produces byte-identical output to a reference implementation.

3. **\u2605\u2605** Add comprehensive input validation: reject packets with invalid field values and return appropriate error codes.

4. **\u2605\u2605** Handle all edge cases: minimum-size packets, maximum-size packets, optional fields, and malformed input.

5. **\u2605\u2605** Write a pcap analyzer that reads capture files and decodes channel capacity packets with full field breakdown.

6. **\u2605\u2605\u2605** Implement the complete protocol state machine. Verify all transitions with a test harness.

7. **\u2605\u2605\u2605** Benchmark parsing throughput (packets/sec) and compare to theoretical line rate.

8. **\u2605\u2605\u2605** Test against real network traffic: capture live packets and verify your parser handles all observed variations.
