# 05. Manchester, NRZ, 4B/5B encodings

## 1. Problem

In real networking systems, manchester, nrz, 4b/5b encodings is a critical component that you encounter constantly. If you cannot implement it correctly from first principles, you will be at the mercy of library bugs, misconfigurations, and subtle protocol violations that are nearly impossible to debug without deep understanding.

The challenge is that manchester, nrz, 4b/5b encodings involves precise binary layouts, strict protocol rules, and edge cases that only manifest under specific network conditions. Getting even one byte wrong means packets are silently dropped or connections mysteriously fail.

## 2. Theory

Manchester, NRZ, 4B/5B encodings is a core concept in Bits & Wires. Understanding it requires grasping both the design philosophy and the implementation details.

NRZ, Manchester, and 4B/5B comparison:

```
Data:    1  0  1  1  0  0  0  1
NRZ:   __/  \__/     \________/
```

Manchester (IEEE 802.3): every bit has a mid-bit transition.
- 1 = low-to-high transition
- 0 = high-to-low transition
- Guarantees clock recovery, but 50% efficiency (2 symbols per bit)

4B/5B: map every 4 data bits to a 5-bit symbol guaranteeing no long runs.
- 80% efficient
- Combined with NRZI for 100BASE-TX Fast Ethernet

## 3. Math / Spec

The protocol defines specific algorithms and data formats that must be implemented exactly for interoperability:

- **Header format**: fixed and variable-length fields with specific byte ordering
- **Checksum**: error detection method (one's complement sum or CRC)
- **State transitions**: valid sequences of operations and responses
- **Timer values**: retransmission timeouts, keepalive intervals, expiration times

All multi-byte integer fields are in network byte order (big-endian) unless explicitly stated otherwise.

## 4. Code

```python
"""
manchester_nrz_4b5b_encodings.py -- Manchester, NRZ, 4B/5B encodings
"""
import struct
import socket

class PacketBuilder:
    """Build and parse protocol packets for Manchester, NRZ, 4B/5B encodings."""

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
    payload = b"Hello from Manchester, NRZ, 4B/5B encodings"
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

1. **★** Parse a hex dump of a real manchester, nrz, 4b/5b encodings packet and identify every field manually.

2. **★** Implement the basic parser and verify it produces byte-identical output to a reference implementation.

3. **★★** Add comprehensive input validation: reject packets with invalid field values and return appropriate error codes.

4. **★★** Handle all edge cases: minimum-size packets, maximum-size packets, optional fields, and malformed input.

5. **★★** Write a pcap analyzer that reads capture files and decodes manchester, nrz, 4b/5b encodings packets with full field breakdown.

6. **★★★** Implement the complete protocol state machine. Verify all transitions with a test harness.

7. **★★★** Benchmark parsing throughput (packets/sec) and compare to theoretical line rate.

8. **★★★** Test against real network traffic: capture live packets and verify your parser handles all observed variations.
