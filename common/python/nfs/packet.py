"""Packet construction helpers.

Thin wrappers around the standard library's `struct` module that make
manual packet building less error-prone. None of these import anything
beyond the standard library.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Final


def hexdump(data: bytes, width: int = 16) -> str:
    """Pretty-print binary data, like `hexdump -C`."""
    lines = []
    for offset in range(0, len(data), width):
        chunk = data[offset:offset + width]
        hex_part = " ".join(f"{b:02x}" for b in chunk)
        ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        lines.append(f"{offset:08x}  {hex_part:<{width*3}}  |{ascii_part}|")
    return "\n".join(lines)


def internet_checksum(data: bytes) -> int:
    """RFC 1071 Internet checksum.

    Returns the 16-bit one's-complement sum, in HOST byte order.
    Caller is responsible for byte-swapping if writing back to a packet.
    """
    s = 0
    # Sum 16-bit words.
    for i in range(0, len(data) - 1, 2):
        s += (data[i] << 8) | data[i + 1]
    # Odd byte: pad with zero on the right.
    if len(data) % 2:
        s += data[-1] << 8
    # Fold carries.
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return (~s) & 0xFFFF


@dataclass(frozen=True)
class MacAddress:
    """48-bit MAC address."""
    octets: bytes  # exactly 6 bytes

    def __post_init__(self) -> None:
        if len(self.octets) != 6:
            raise ValueError(f"MAC must be 6 bytes, got {len(self.octets)}")

    @classmethod
    def parse(cls, s: str) -> MacAddress:
        """Parse 'aa:bb:cc:dd:ee:ff' or 'aa-bb-cc-dd-ee-ff'."""
        sep = ":" if ":" in s else "-"
        parts = s.split(sep)
        if len(parts) != 6:
            raise ValueError(f"invalid MAC: {s!r}")
        return cls(bytes(int(p, 16) for p in parts))

    def __str__(self) -> str:
        return ":".join(f"{b:02x}" for b in self.octets)

    @property
    def is_multicast(self) -> bool:
        """The lowest bit of the first byte is the multicast/unicast flag."""
        return bool(self.octets[0] & 0x01)

    @property
    def is_locally_administered(self) -> bool:
        """The U/L bit (second-lowest of the first byte)."""
        return bool(self.octets[0] & 0x02)


# Common EtherTypes (RFC 7042).
ETHERTYPE_IPV4: Final[int] = 0x0800
ETHERTYPE_ARP: Final[int]  = 0x0806
ETHERTYPE_IPV6: Final[int] = 0x86DD
ETHERTYPE_VLAN: Final[int] = 0x8100  # 802.1Q


def build_ethernet_header(
    dst: MacAddress,
    src: MacAddress,
    ethertype: int,
) -> bytes:
    """Construct an Ethernet II header (14 bytes)."""
    return dst.octets + src.octets + struct.pack("!H", ethertype)


def parse_ethernet_header(data: bytes) -> tuple[MacAddress, MacAddress, int]:
    """Parse an Ethernet II header. Returns (dst, src, ethertype)."""
    if len(data) < 14:
        raise ValueError(f"need at least 14 bytes, got {len(data)}")
    dst = MacAddress(data[0:6])
    src = MacAddress(data[6:12])
    (ethertype,) = struct.unpack("!H", data[12:14])
    return dst, src, ethertype
