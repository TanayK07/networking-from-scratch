# 16. Capstone: a tiny CNI plugin

## 1. Problem

This capstone brings together everything from the Overlays & Container Networking phase into a single working project. You will build a complete, functional implementation of a tiny CNI plugin that demonstrates mastery of all concepts covered so far.

Without integrating individual components into a complete system, you only have isolated pieces of knowledge. Real networking code must handle all edge cases, state transitions, and error conditions simultaneously. This is where theory meets practice.

## 2. Theory

The a tiny CNI plugin project integrates multiple components built in previous lessons. The architecture follows a layered design where each module handles one responsibility:

```
  +------------------------------+
  |      Application Logic       |
  +------------------------------+
  |     Protocol Processing      |
  +------------------------------+
  |    Packet I/O & Buffering    |
  +------------------------------+
  |   Network Device (TUN/TAP)   |
  +------------------------------+
```

Each layer communicates through well-defined interfaces. Error handling is critical: network operations can fail at any layer, and the system must degrade gracefully. The implementation should be structured for testability.

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
capstone_a_tiny_cni_plugin.py -- Capstone: Capstone: a tiny CNI plugin
Integrates all phase components into a complete working system.
"""
import socket
import struct
import select
import logging
import argparse
from dataclasses import dataclass, field

logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')
log = logging.getLogger(__name__)

@dataclass
class Stats:
    rx: int = 0
    tx: int = 0
    errors: int = 0

class ProtocolHandler:
    """Base protocol handler with parse/build/process pipeline."""

    def __init__(self):
        self.state = {}
        self.stats = Stats()

    def parse(self, data: bytes) -> dict:
        if len(data) < 8:
            raise ValueError(f"Packet too short: {len(data)} bytes")
        ver_type, flags, length, pkt_id = struct.unpack('!BBHI', data[:8])
        version = (ver_type >> 4) & 0xF
        pkt_type = ver_type & 0xF
        payload = data[8:8 + length - 8] if length > 8 else b''
        return {
            'version': version, 'type': pkt_type, 'flags': flags,
            'length': length, 'id': pkt_id, 'payload': payload,
        }

    def process(self, msg: dict) -> bytes | None:
        self.stats.rx += 1
        log.info("rx: type=%d id=%d len=%d", msg['type'], msg['id'], msg['length'])
        return None

    def build(self, pkt_type: int, flags: int, payload: bytes, pkt_id: int = 0) -> bytes:
        ver_type = (1 << 4) | (pkt_type & 0xF)
        length = 8 + len(payload)
        header = struct.pack('!BBHI', ver_type, flags, length, pkt_id)
        self.stats.tx += 1
        return header + payload

class Server:
    def __init__(self, handler: ProtocolHandler, host='0.0.0.0', port=9000):
        self.handler = handler
        self.host = host
        self.port = port

    def run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((self.host, self.port))
        sock.setblocking(False)
        log.info("Listening on %s:%d", self.host, self.port)

        try:
            while True:
                readable, _, _ = select.select([sock], [], [], 1.0)
                for s in readable:
                    data, addr = s.recvfrom(65535)
                    try:
                        msg = self.handler.parse(data)
                        resp = self.handler.process(msg)
                        if resp:
                            s.sendto(resp, addr)
                    except Exception as e:
                        log.error("Error from %s: %s", addr, e)
                        self.handler.stats.errors += 1
        except KeyboardInterrupt:
            log.info("Shutdown. %s", self.handler.stats)
        finally:
            sock.close()

if __name__ == '__main__':
    ap = argparse.ArgumentParser(description='Capstone: a tiny CNI plugin')
    ap.add_argument('--port', type=int, default=9000)
    args = ap.parse_args()
    Server(ProtocolHandler(), port=args.port).run()
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

1. **★** Build the project skeleton with a proper Makefile. Verify it compiles and runs with empty input.

2. **★** Implement the packet parser for the primary protocol. Test with at least 3 known-good packet captures.

3. **★★** Add comprehensive error handling: every failure path should log a clear message and increment an error counter.

4. **★★** Implement the core protocol state machine with all required transitions. Draw the state diagram.

5. **★★** Write integration tests using real network tools (`ping`, `curl`, `dig`, `tcpdump`).

6. **★★★** Profile with `perf` and optimize the hot path. Target 100,000+ packets/sec on a single core.

7. **★★★** Fuzz with AFL++ or libFuzzer. Fix all crashes.

8. **★★★** Test interoperability with at least two existing implementations. Document any compatibility issues.
