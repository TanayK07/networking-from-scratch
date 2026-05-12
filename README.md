<div align="center">

# Networking from Scratch

**Build the network stack — bits, frames, packets, TCP, TLS, kernel modules, eBPF, CNI plugins, and a real DDS implementation — from raw bytes, in C and Python.**

[![GitHub stars](https://img.shields.io/github/stars/TanayK07/networking-from-scratch?style=flat-square&color=00897B)](https://github.com/TanayK07/networking-from-scratch/stargazers)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue?style=flat-square)](LICENSE)
[![Lessons](https://img.shields.io/badge/lessons-289-teal?style=flat-square)](https://networkingfromscratch.vercel.app/catalog.html)
[![Phases](https://img.shields.io/badge/phases-15-orange?style=flat-square)](https://networkingfromscratch.vercel.app/roadmap.html)
[![C](https://img.shields.io/badge/C-00599C?style=flat-square&logo=c&logoColor=white)](phases/)
[![Python](https://img.shields.io/badge/Python-3776AB?style=flat-square&logo=python&logoColor=white)](phases/)

[Website](https://networkingfromscratch.vercel.app) · [Catalog](https://networkingfromscratch.vercel.app/catalog.html) · [Roadmap](https://networkingfromscratch.vercel.app/roadmap.html) · [Glossary](https://networkingfromscratch.vercel.app/glossary.html) · [Discussions](https://github.com/TanayK07/networking-from-scratch/discussions)

</div>

---

## What you'll build

By the time you finish this curriculum you will have:

- Hand-rolled an **Ethernet frame**, computed its FCS, and injected it through a raw socket.
- Built a **userspace TCP/IP stack** on a Linux TUN/TAP device that can `curl` a real HTTPS site (saminiir's *Let's code a TCP/IP stack*, rebuilt and extended with SACK, window scaling, and CUBIC).
- Implemented **TLS 1.3** from `ClientHello` to `Finished` against `cloudflare.com`, parsing every byte the way `tls13.xargs.org` does.
- Written a **loadable kernel module** that hooks into Netfilter to log every packet's 5-tuple.
- Loaded a small **XDP program** that drops a SYN flood at line rate.
- Built a **CNI plugin** in Python that creates veth pairs and attaches Pods to a bridge.
- Stood up a **two-host VXLAN overlay** on UDP/4789.
- Shipped a tiny **DDS publisher** that interoperates over the wire with Eclipse Cyclone DDS.

Everything from raw bytes. No frameworks until you've built a minimal version yourself.

---

## Prerequisites

You should arrive with:

- **C, intermediate level** — pointers, structs, bit fields, `memcpy`, `malloc`, function pointers.
- **Python 3, intermediate level** — `bytes` vs `str`, `struct.pack`/`unpack`, `asyncio`, `pytest`.
- **CS fundamentals** — binary/hex, two's complement, endianness, basic data structures.
- **Linux command line** — `ip`, `ss`, `tcpdump`, `make`, reading `man 2` pages.
- **A Linux box you can break.** Most kernel-module, TUN/TAP, eBPF, and namespace lessons require root.

See [`docs/prereqs.md`](docs/prereqs.md) for the full breakdown and a setup script for an Ubuntu 24.04 VM.

---

## Quickstart

```bash
git clone https://github.com/TanayK07/networking-from-scratch.git
cd networking-from-scratch

# Set up an Ubuntu 24.04 VM with all dependencies (recommended)
./tools/setup-vm.sh

# Or install dependencies on your existing machine (use a VM you don't mind breaking)
sudo apt install -y build-essential gcc clang make python3 python3-pip \
    libbpf-dev linux-headers-$(uname -r) linux-tools-$(uname -r) \
    libsodium-dev libssl-dev iproute2 tcpdump iperf3 wireshark-common

# Run your first lesson
make -C phases/01-bits-and-wires/02-bits-bytes-endianness test

# Or jump to the link layer
make -C phases/02-link-layer/03-raw-sockets-on-linux-afpacket
sudo ./phases/02-link-layer/03-raw-sockets-on-linux-afpacket/sniff eth0
```

---

## Curriculum at a glance

```
P1   Bits & Wires             ─┐
P2   Link Layer                ├── FOUNDATIONS
P3   Network Layer             │
P4   Transport (UDP/TCP/QUIC) ─┘
P5   Sockets & I/O             ─┐
P6   Application Protocols      │── PROTOCOLS
P7   TLS & Secure Transport    ─┘
P8   Userspace TCP/IP Stack    ── CAPSTONE I
P9   Linux Kernel Internals   ─┐
P10  eBPF, XDP, Kernel Bypass  ├── KERNEL & PERF
P11  Performance & Observability┘
P12  Routing: BGP, OSPF        ─┐
P13  Overlays & Container Net   ├── MODERN INFRA
P14  Service Mesh & Cloud-Native┘
P15  BONUS: DDS & Robotics
```

| Phase | Title | Lessons | Capstone |
|------:|-------|--------:|----------|
| P1  | Bits & Wires                          | 12 | A virtual wire with framing + CRC + retransmit |
| P2  | Link Layer                            | 18 | A tiny L2 switch with MAC learning |
| P3  | Network Layer                         | 22 | A userspace IP router on TUN devices |
| P4  | Transport: UDP, TCP, QUIC             | 40 | End-to-end TCP over TUN, fetching `example.com` |
| P5  | Sockets & I/O Models                  | 16 | A 10k-connection HTTP/1.0 server (epoll) |
| P6  | Application Protocols                 | 28 | An HTTP CONNECT proxy supporting HTTP/1.1, /2, /3 |
| P7  | TLS & Secure Transport                | 16 | A minimal HTTPS server using your TLS 1.3 |
| P8  | Userspace TCP/IP Stack                | 12 | `lvl-ip-rebuilt` — a working stack on TUN |
| P9  | Linux Kernel Internals                | 24 | An LKM that injects 1% packet loss |
| P10 | eBPF, XDP, Kernel Bypass              | 18 | An XDP DDoS scrubber + AF_XDP packet generator |
| P11 | Performance & Observability           | 14 | A `tcpdump`+eBPF+Grafana observability stack |
| P12 | Routing: BGP, OSPF                    | 16 | A BGP route reflector + OSPF area-0 simulator |
| P13 | Overlays & Container Networking       | 16 | A bridge CNI plugin in Python |
| P14 | Service Mesh & Cloud-Native           | 12 | A 200-line L7 proxy with mTLS |
| P15 | BONUS: DDS, RTPS, Robotics Middleware | 25 | A DDS publisher interoperating with Cyclone DDS |
| **Total** | |  **289** | |

See [`docs/index.md`](docs/index.md) for the full lesson list with descriptions.

---

## Repository layout

```
networking-from-scratch/
├── README.md                        # this file
├── LICENSE                          # MIT
├── CONTRIBUTING.md
├── CODE_OF_CONDUCT.md
├── docs/
│   ├── index.md                     # full curriculum
│   ├── prereqs.md                   # setup and prerequisites
│   ├── style-guide.md               # how to write a lesson
│   ├── glossary.md
│   └── rfcs/                        # cached copies of canonical RFCs
├── common/
│   ├── c/                           # shared C helpers (checksum, CRC, TUN, log)
│   └── python/nfs/                  # shared Python helpers (packet, pcap, runner)
├── phases/
│   ├── 01-bits-and-wires/
│   │   ├── README.md
│   │   ├── 01-what-is-a-network/
│   │   ├── 02-bits-bytes-endianness/
│   │   │   ├── README.md            # the lesson (theory, code, exercises)
│   │   │   ├── htonl.c
│   │   │   ├── htonl.h
│   │   │   ├── Makefile
│   │   │   └── tests/
│   │   ├── 08-errors-and-crcs/
│   │   └── ...
│   ├── 02-link-layer/
│   ├── ...
│   └── 15-dds-robotics-bonus/
├── tools/
│   ├── new-lesson.py                # scaffold a new lesson directory
│   ├── lint-curriculum.py           # check every lesson has the 6 sections
│   └── setup-vm.sh                  # provision an Ubuntu 24.04 VM
├── .github/
│   └── workflows/                   # CI: C build, Python tests, LKMs, eBPF, docs
├── Makefile                         # top-level: builds everything
└── tox.ini                          # Python test orchestration
```

---

## How a lesson is structured

Every lesson `README.md` has the same six sections, in this order:

```
# NN. Lesson Title

## 1. Problem        — concrete, observable problem this lesson solves
## 2. Theory         — plain-prose explanation, with diagrams
## 3. Math / Spec    — the formal definition (algorithm, RFC quote, equation)
## 4. Code           — annotated walkthrough of the implementation
## 5. Tests          — unit, property, interop, fuzz
## 6. Exercises      — 5–10 graded exercises with ★ difficulty markers
```

The lint script (`tools/lint-curriculum.py`) rejects PRs that omit any of these sections. See [`docs/style-guide.md`](docs/style-guide.md) for the full convention.

---

## Sample lesson — see them in action

Three sample lessons are fully written to demonstrate the format:

- **[P1.02 — Bits, bytes, and endianness](phases/01-bits-and-wires/02-bits-bytes-endianness/)** — reimplements `htonl`/`ntohl` from scratch, verifies bit-for-bit agreement with the kernel's. (C)
- **[P2.06 — ARP request / reply](phases/02-link-layer/06-arp-request-reply/)** — responds to ARP requests for a virtual MAC; the kernel's `arp` table populates with our fake. (C)
- **[P3.02 — The Internet checksum (RFC 1071)](phases/03-network-layer/02-internet-checksum/)** — byte-by-byte and incremental-update implementations, with property tests. (C)

Each one is a working, tested, runnable example of how every other lesson should look.

---

## Building everything

```bash
# Top-level Makefile delegates to every per-lesson Makefile
make build      # build all C lessons
make test       # run all C and Python tests
make clean      # clean all artifacts

# Python tests via tox
tox             # runs pytest, ruff, mypy

# Lint the curriculum structure
python3 tools/lint-curriculum.py
```

---

## Contributing

We want this curriculum to be the canonical answer to "how do I really learn networking?"

Useful contributions:

1. **Write a lesson.** Pick any planned lesson from the [catalog](https://networkingfromscratch.vercel.app/catalog.html), fork, write it following [`docs/style-guide.md`](docs/style-guide.md), open a PR.
2. **Add a test.** Existing lessons are improved by more property tests, fuzz harnesses, and interop checks.
3. **Fix a bug.** Code, prose, links — open a PR.
4. **Translate.** We accept lesson translations as separate folders (`README.es.md`, etc.).

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the full guide.

---

## Status

This is a curriculum-in-progress. The structure, sample lessons, and three reference implementations (P1.02, P2.06, P3.02) are complete; ~286 lessons are still planned. See the live progress on [the website](https://networkingfromscratch.vercel.app).

If you want to help write lessons, the [project board](https://github.com/TanayK07/networking-from-scratch/projects/1) shows what's claimed and what's open.

---

## Inspirations

This project would not exist without prior work:

- **[saminiir's *Let's code a TCP/IP stack*](https://www.saminiir.com/lets-code-tcp-ip-stack-1-ethernet-arp/)** — the reference for how to teach a TCP/IP stack from scratch.
- **[Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)** — the canonical introduction to the Berkeley sockets API.
- **[*TCP/IP Illustrated*](https://en.wikipedia.org/wiki/TCP/IP_Illustrated) by W. Richard Stevens** — the canonical reference for protocol implementation details.
- **[*The Illustrated TLS 1.3 Connection*](https://tls13.xargs.org/)** — the byte-level TLS 1.3 walkthrough we shamelessly model on.
- **[packagecloud's Linux network stack tutorials](https://blog.packagecloud.io/monitoring-tuning-linux-networking-stack-receiving-data/)** — the receive-path bible.
- **[*AI Engineering from Scratch*](https://aiengineeringfromscratch.com/)** — the structural template for this curriculum.
- **The build-your-own-X repos**, especially [danistefanovic/build-your-own-x](https://github.com/danistefanovic/build-your-own-x) and [practical-tutorials/project-based-learning](https://github.com/practical-tutorials/project-based-learning).

---

## License

[MIT](LICENSE).

The curriculum text, sample code, and supporting tools are all MIT-licensed. Use them however you want — just keep the attribution.

---

© 2026 · open source · free forever
