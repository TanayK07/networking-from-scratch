# Networking from Scratch — Curriculum

This is the canonical lesson index. The website (`networkingfromscratch.vercel.app`) is generated from this file plus per-lesson READMEs.

## Phase index

| # | Phase | Lessons | Capstone artifact |
|---|---|---|---|
| 01 | Bits & Wires | 12 | Virtual wire with framing + CRC + retransmit |
| 02 | Link Layer | 18 | Software switch with FDB and aging |
| 03 | Network Layer | 22 | Userspace IP router on TUN |
| 04 | Transport (UDP/TCP/QUIC) | 40 | Full TCP on TUN, can `curl` HTTPS |
| 05 | Sockets & I/O Models | 16 | 10k-connection HTTP/1.0 server |
| 06 | Application Protocols | 28 | DNS resolver + HTTP/1.1 + HTTP/2 frames |
| 07 | TLS & Secure Transport | 16 | TLS 1.3 client handshakes with cloudflare.com |
| 08 | Userspace TCP/IP Stack | 12 | `level-ip`-style stack, rebuilt and extended |
| 09 | Linux Kernel Internals | 24 | Netfilter LKM firewall |
| 10 | eBPF, XDP, Kernel Bypass | 18 | XDP DDoS scrubber + AF_XDP packet generator |
| 11 | Performance & Observability | 14 | Latency-histogram-aware HTTP server |
| 12 | Routing | 16 | BGP route reflector + OSPF simulator |
| 13 | Overlays & Container Networking | 16 | Bridge CNI plugin + VXLAN overlay |
| 14 | Service Mesh & Cloud-Native | 12 | 200-line L7 reverse proxy |
| 15 | DDS & Robotics (BONUS) | 24 | DDS publisher interoperating with Cyclone DDS |

**Total:** ~305 lessons.

## Reading order

The phases are linear. Skipping ahead is supported but the dependency graph is real:

- Phase 4 (TCP) depends on Phase 3 (IP) which depends on Phase 2 (Ethernet).
- Phase 8 (userspace stack) is the integration point for Phases 1–7.
- Phase 9 (kernel internals) requires comfort with Phases 2–4 protocols.
- Phase 10 (eBPF/XDP) requires Phase 9.
- Phase 13 (containers) requires Phase 9 (namespaces) and Phase 3 (routing).
- Phase 15 (DDS) requires Phase 4 (UDP, reliability concepts) and Phase 13 (multicast).

See `docs/roadmap.md` for the visual graph.

## Lesson template

Every lesson follows the same six-section structure: **Problem · Theory · Math/Spec · Code · Tests · Exercises**. See `docs/style-guide.md`.

## Canonical sources

The curriculum cites and depends on:

- saminiir, *Let's code a TCP/IP stack* (Phases 2–4, 8)
- Beej, *Guide to Network Programming* (Phase 5)
- Stevens et al., *TCP/IP Illustrated, Vol. 1* (Phases 3, 4)
- packagecloud, *Monitoring and Tuning the Linux Networking Stack* (Phase 9)
- `tls13.xargs.org`, *The Illustrated TLS 1.3 Connection* (Phase 7)
- OMG, *DDSI-RTPS 2.5* (April 2022) (Phase 15)
- Eclipse Cyclone DDS source tree (Phase 15)
- ROS 2 Jazzy/Kilted documentation (Phase 15)
- iximiuz Labs tutorials (Phases 10, 13)

See `docs/rfcs/` for cached RFC text.
