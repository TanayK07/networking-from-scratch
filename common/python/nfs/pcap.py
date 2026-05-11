"""PCAP file format reader/writer.

The classic libpcap file format. See https://wiki.wireshark.org/Development/LibpcapFileFormat.

Used by P2.12 (PCAP file format), P11.02 (tcpdump as a debugger),
and many other lessons that need to write packet captures Wireshark
can open.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

# PCAP global header constants.
PCAP_MAGIC = 0xA1B2C3D4    # microsecond resolution, native byte order
PCAP_VERSION_MAJOR = 2
PCAP_VERSION_MINOR = 4

# Common LinkType values (see https://www.tcpdump.org/linktypes.html).
LINKTYPE_ETHERNET = 1
LINKTYPE_RAW = 101         # raw IP, no link-layer header
LINKTYPE_LINUX_SLL = 113   # Linux cooked


@dataclass
class PcapPacket:
    ts_sec: int
    ts_usec: int
    data: bytes


class PcapWriter:
    """Write a libpcap file. Open with `with PcapWriter(...) as w:`."""

    def __init__(self, path: Path | str, linktype: int = LINKTYPE_ETHERNET) -> None:
        self.path = Path(path)
        self.linktype = linktype
        self._fp = None

    def __enter__(self) -> PcapWriter:
        self._fp = self.path.open("wb")
        # PCAP global header (24 bytes).
        self._fp.write(struct.pack(
            "<IHHiIII",
            PCAP_MAGIC,
            PCAP_VERSION_MAJOR,
            PCAP_VERSION_MINOR,
            0,                      # thiszone (UTC offset)
            0,                      # sigfigs
            65535,                  # snaplen
            self.linktype,
        ))
        return self

    def __exit__(self, *exc: object) -> None:
        if self._fp:
            self._fp.close()
            self._fp = None

    def write(self, packet: PcapPacket) -> None:
        """Append one packet record."""
        if self._fp is None:
            raise RuntimeError("PcapWriter must be used as a context manager")
        # Per-packet header (16 bytes) + the packet data.
        self._fp.write(struct.pack(
            "<IIII",
            packet.ts_sec,
            packet.ts_usec,
            len(packet.data),       # incl_len
            len(packet.data),       # orig_len
        ))
        self._fp.write(packet.data)


def read_pcap(path: Path | str) -> Iterator[PcapPacket]:
    """Iterate the packets in a PCAP file."""
    with Path(path).open("rb") as fp:
        header = fp.read(24)
        if len(header) != 24:
            raise ValueError("file too short for PCAP global header")
        magic = struct.unpack("<I", header[0:4])[0]
        if magic == PCAP_MAGIC:
            endian = "<"
        elif magic == 0xD4C3B2A1:   # byte-swapped
            endian = ">"
        else:
            raise ValueError(f"not a pcap file (magic {magic:#x})")

        while True:
            rec = fp.read(16)
            if not rec:
                return
            if len(rec) != 16:
                raise ValueError("truncated record header")
            ts_sec, ts_usec, incl_len, _orig_len = struct.unpack(endian + "IIII", rec)
            data = fp.read(incl_len)
            if len(data) != incl_len:
                raise ValueError("truncated packet data")
            yield PcapPacket(ts_sec=ts_sec, ts_usec=ts_usec, data=data)
