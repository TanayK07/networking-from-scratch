# Glossary

Networking terms defined twice — what people say, and what it actually means. Cross-referenced against the curriculum so every term links to where you build it from scratch.

This is the markdown version. The interactive web glossary at [networkingfromscratch.vercel.app/glossary.html](https://networkingfromscratch.vercel.app/glossary.html) has search and category filtering.

---

## Layering

### OSI · *Open Systems Interconnection model*
- **What people say.** Seven layers: Physical, Data Link, Network, Transport, Session, Presentation, Application.
- **What it is.** A reference model. Real networks use the four-layer TCP/IP model. The OSI session and presentation layers are mostly absorbed into the application layer in practice.
- **Built in.** [P1.01 — What is a network?](../phases/01-bits-and-wires/01-what-is-a-network/)

### MTU · *Maximum Transmission Unit*
- **What people say.** The largest packet a link will carry, usually 1500 bytes for Ethernet.
- **What it is.** A property of a single link. End-to-end MTU is determined by the smallest MTU on the path. Path MTU Discovery (RFC 1191) finds it dynamically using the DF bit and ICMP "Fragmentation Needed" replies.
- **Built in.** [P3.08 — Path MTU Discovery](../phases/03-network-layer/)

### MSS · *Maximum Segment Size*
- **What people say.** How much TCP data fits in one packet.
- **What it is.** A TCP-only concept negotiated in SYN options. MSS = MTU − IP header − TCP header (typically 1460 for IPv4 over Ethernet, 1440 for IPv6). Not the same as MTU; advertised per-direction.
- **Built in.** P4.07 — TCP header.

---

## Link Layer

### Ethernet II · *IEEE 802.3 frame format with EtherType*
- **What people say.** Standard Ethernet frames you see on every LAN.
- **What it is.** A frame format: 6-byte destination MAC, 6-byte source MAC, 2-byte EtherType, payload (46–1500 bytes), 4-byte FCS. Differs from 802.3 LLC/SNAP framing in the EtherType vs length field interpretation.
- **Built in.** P2.01 — Ethernet II frame layout.

### MAC address · *Media Access Control address*
- **What people say.** 48-bit unique hardware address.
- **What it is.** Not always unique — locally administered addresses (LAA) have the U/L bit set. The first 24 bits are the OUI (Organizationally Unique Identifier) assigned by IEEE; the lowest bit of the first byte is the multicast/unicast flag.
- **Built in.** P2.02 — MAC addresses and OUI.

### ARP · *Address Resolution Protocol*
- **What people say.** Maps an IP address to a MAC address.
- **What it is.** A broadcast-based L2 protocol (RFC 826). Cache entries are populated on receipt of any ARP traffic, including unsolicited replies — which is why ARP poisoning works. Replaced by NDP in IPv6.
- **Built in.** [P2.06 — ARP request / reply](../phases/02-link-layer/06-arp-request-reply/) ✓

### TUN/TAP · *Userspace network device*
- **What people say.** A virtual NIC you can read/write from a userspace program.
- **What it is.** TUN operates at L3 (you read/write IP packets); TAP operates at L2 (you read/write Ethernet frames). Created via `/dev/net/tun` and `ioctl(TUNSETIFF)`. Used for VPNs, userspace TCP/IP stacks, and our Phase 8 capstone.
- **Built in.** P2.04 — TUN vs TAP devices.

---

## Network Layer

### IP fragmentation
- **What people say.** Splitting a packet too big for the link MTU into smaller pieces.
- **What it is.** IPv4 only (IPv6 forbids router fragmentation). The Identification, MF (More Fragments), and Fragment Offset fields in the IP header are used by the receiver to reassemble. One lost fragment loses the whole datagram. Reassembly is bounded by `net.ipv4.ipfrag_high_thresh` on Linux.
- **Built in.** P3.07 — IP fragmentation.

### CIDR · *Classless Inter-Domain Routing*
- **What people say.** IP/prefix notation, like `10.0.0.0/24`.
- **What it is.** Replaced classful addressing in 1993. The prefix length is the number of leading bits in the network mask. Used by routing tables for longest-prefix match — the most specific (longest prefix) wins.
- **Built in.** P3.03 — CIDR and subnetting.

### Internet checksum
- **What people say.** A 16-bit checksum in IP, UDP, and TCP headers.
- **What it is.** RFC 1071. The 16-bit one's-complement sum of all 16-bit words, with the bitwise NOT applied at the end. Weak (a single bit flip in two different positions can produce the same checksum) but cheap. UDP/TCP add a pseudo-header before computing.
- **Built in.** [P3.02 — The Internet checksum (RFC 1071)](../phases/03-network-layer/02-internet-checksum/) ✓

### TTL · *Time To Live*
- **What people say.** How many hops a packet can take before being dropped.
- **What it is.** 8-bit field decremented by each router. When it hits 0, the router drops the packet and sends ICMP Time Exceeded — which is exactly how `traceroute` works. Renamed Hop Limit in IPv6.
- **Built in.** P3.06 — TTL and the traceroute trick.

### NAT · *Network Address Translation*
- **What people say.** Many private IPs share one public IP.
- **What it is.** Requires connection tracking — the NAT box stores a 5-tuple mapping (private IP:port → public IP:port) per flow. Source-NAT (masquerade) is the common variant. Breaks end-to-end addressability and many P2P protocols, hence STUN/TURN/ICE.
- **Built in.** P3.17 — NAT — what really happens.

---

## Transport Layer

### TCP · *Transmission Control Protocol*
- **What people say.** Reliable, ordered, byte-stream protocol.
- **What it is.** RFC 9293 (2022). 11-state state machine, sequence numbers mod 2³², sliding windows, retransmission with adaptive RTO, congestion control. The TCB (Transmission Control Block) holds all per-connection state.
- **Built in.** P4.06 — TCP, the protocol.

### Three-way handshake
- **What people say.** SYN → SYN-ACK → ACK to open a TCP connection.
- **What it is.** Synchronizes initial sequence numbers (ISN) in both directions. ISN should be randomized per RFC 6528 to prevent off-path injection. The third ACK can carry data ("piggybacked").
- **Built in.** P4.09 — The three-way handshake.

### Sliding window
- **What people say.** How much data can be in flight at once.
- **What it is.** A flow-control mechanism. The receiver advertises its free buffer space (RWND); the sender tracks SND.UNA, SND.NXT, and the smaller of CWND (congestion) and RWND (flow). The receiver's window can shrink to zero (zero-window probe).
- **Built in.** P4.13 — Sliding window basics.

### CUBIC
- **What people say.** The default Linux congestion control algorithm.
- **What it is.** RFC 8312. The window grows as a cubic function of time since the last loss event: `W(t) = C(t-K)³ + W_max`. More aggressive than Reno on high-bandwidth long-RTT links.
- **Built in.** P4.27 — TCP CUBIC.

### BBR · *Bottleneck Bandwidth and RTT*
- **What people say.** Google's congestion control that doesn't back off on loss.
- **What it is.** A model-based algorithm that estimates bottleneck bandwidth and minimum RTT, then paces packets at that rate. Doesn't treat loss as a congestion signal. Per Cardwell et al. (ACM Queue 2016), throughput improvements of 2–25× on Google's B4 WAN compared to CUBIC.
- **Built in.** P4.28 — TCP BBR with software pacing.

### QUIC
- **What people say.** TCP rebuilt over UDP with TLS 1.3 baked in.
- **What it is.** RFC 9000. Reliable, ordered streams over UDP, with TLS 1.3 mandatory. Eliminates head-of-line blocking between streams. Connection IDs allow migration across IP changes. The transport layer of HTTP/3.
- **Built in.** P4.34 — QUIC overview.

---

## Sockets & I/O

### epoll
- **What people say.** Linux's scalable replacement for select/poll.
- **What it is.** O(1) event notification scaling to hundreds of thousands of connections. Edge-triggered mode notifies only on state transitions; level-triggered notifies as long as the condition holds. The C10K mental model is built on epoll.
- **Built in.** P5.06 — epoll: edge vs level triggered.

### io_uring
- **What people say.** Linux's newest async I/O interface.
- **What it is.** Mmap'd submission and completion ring buffers shared with the kernel. Zero syscalls per I/O on the fast path. Supports networking ops (RECV, SEND, ACCEPT, SEND_ZC). Faster than epoll for high-throughput, but verifier-restricted in some sandboxed environments.
- **Built in.** P5.12 — io_uring 101.

### Zero-copy
- **What people say.** Sending data without copying it through user space.
- **What it is.** `sendfile(2)` (file → socket), `splice(2)` (any pipe), `MSG_ZEROCOPY` (user buffer → socket without copy, page-pinned). Big wins for static-file servers but with subtle correctness gotchas — the kernel must signal when the buffer is reusable.
- **Built in.** P5.10 — sendfile and splice.

---

## Security & TLS

### AEAD · *Authenticated Encryption with Associated Data*
- **What people say.** Encrypt and authenticate in one operation.
- **What it is.** A primitive that combines confidentiality and integrity. AES-128-GCM, AES-256-GCM, ChaCha20-Poly1305 are the only ones allowed in TLS 1.3. The "associated data" is authenticated but not encrypted — typically the TLS record header.
- **Built in.** P7.02 — AES-128-GCM by hand.

### HKDF · *HMAC-based Key Derivation Function*
- **What people say.** Turn a shared secret into multiple keys.
- **What it is.** RFC 5869. Two phases: Extract (compress entropy into a pseudorandom key) and Expand (stretch into the desired output length). TLS 1.3 builds its entire key schedule from HKDF.
- **Built in.** P7.03 — HMAC and HKDF.

### X25519
- **What people say.** The modern key exchange for TLS 1.3.
- **What it is.** Elliptic-curve Diffie–Hellman over Curve25519 (RFC 7748). 32-byte keys, no parameter validation needed (any 32-byte string is a valid public key), small constant-time implementations. Replaces classical DH and most ECDH curves in TLS 1.3.
- **Built in.** P7.04 — Diffie-Hellman, then X25519.

### kTLS · *Kernel TLS*
- **What people say.** TLS record encryption done in the kernel.
- **What it is.** `setsockopt(TCP_ULP, "tls")`. The handshake stays in userspace; once keys are negotiated, application data is encrypted by the kernel. Enables `sendfile()` to a TLS connection. Linux supports AES-GCM and ChaCha20-Poly1305 in kTLS.
- **Built in.** P7.16 — TLS in the kernel: kTLS.

---

## Linux Kernel

### sk_buff
- **What people say.** The kernel's packet container.
- **What it is.** A struct holding metadata about a packet plus pointers (`head`, `data`, `tail`, `end`) into the actual data buffer. Kernel-wide, used by every protocol. `truesize` is charged against the socket's receive buffer (`sk_rmem_alloc`) for accounting. Allocated from a slab cache.
- **Built in.** P9.03 — The sk_buff: head, data, tail, end.

### NAPI · *New API*
- **What people say.** How the kernel polls NICs instead of taking one interrupt per packet.
- **What it is.** A hybrid interrupt-then-poll mechanism. The first packet causes an IRQ; the driver disables further IRQs and schedules a NAPI poll, which drains the NIC ring up to a budget (default 64 packets, controlled by `net.core.dev_weight`). Re-enables IRQs when drained.
- **Built in.** P9.05 — NAPI: schedule, poll, complete.

### Netfilter
- **What people say.** The kernel hook framework iptables and nftables build on.
- **What it is.** Five hooks: PREROUTING, INPUT (LOCAL_IN), FORWARD, OUTPUT (LOCAL_OUT), POSTROUTING. Each hook can register multiple kernel modules at different priorities. Returns NF_ACCEPT, NF_DROP, NF_QUEUE, NF_STOLEN, or NF_REPEAT.
- **Built in.** P9.11 — Netfilter hooks: a logging firewall LKM.

### Network namespace · *netns*
- **What people say.** A separate isolated network stack inside Linux.
- **What it is.** Created via `clone(CLONE_NEWNET)` or `unshare(--net)`. Each has its own interfaces, routing table, ARP cache, `/proc/net` entries, and netfilter rules. Container networking is built on this. Move an interface in with `ip link set <iface> netns <name>`.
- **Built in.** P9.14 — Network namespaces.

### eBPF · *extended Berkeley Packet Filter*
- **What people say.** Sandboxed kernel programs you load from userspace.
- **What it is.** A JIT-compiled VM running verifier-checked bytecode in the kernel. Used for packet filtering, tracing, security policy, and (via XDP) line-rate packet processing. Programs cannot loop unboundedly, dereference unchecked pointers, or block — the verifier rejects them.
- **Built in.** P10.01 — What is eBPF: verifier, JIT.

### XDP · *eXpress Data Path*
- **What people say.** Earliest hook in the receive path.
- **What it is.** An eBPF program that runs immediately after the NIC driver allocates an `sk_buff` (or before, in driver-mode XDP). Returns XDP_PASS, XDP_DROP, XDP_TX (echo back), or XDP_REDIRECT (to another iface or AF_XDP socket). Enables line-rate DDoS scrubbing.
- **Built in.** P10.04 — XDP basics.

---

## Routing

### BGP · *Border Gateway Protocol*
- **What people say.** The routing protocol of the Internet.
- **What it is.** A path-vector protocol over TCP/179. Routers exchange OPEN, UPDATE, KEEPALIVE, NOTIFICATION messages. UPDATE carries NLRI (prefixes) plus path attributes (AS-PATH, NEXT_HOP, LOCAL_PREF, MED). Path selection: highest LOCAL_PREF, shortest AS-PATH, lowest origin, etc.
- **Built in.** P12.06 — BGP basics.

### OSPF · *Open Shortest Path First*
- **What people say.** Link-state routing inside an AS.
- **What it is.** Routers flood LSAs (Link State Advertisements) describing their links. Each router builds a full topology database and runs Dijkstra to compute shortest paths. Hello/DBD/LSR/LSU/LSAck packet types. Areas reduce LSDB size in large networks.
- **Built in.** P12.03 — OSPF basics.

### ECMP · *Equal-Cost Multi-Path*
- **What people say.** Spread traffic across multiple equal-cost paths.
- **What it is.** A router with N equal-cost next-hops hashes a flow's 5-tuple and picks one consistently. Stability matters — a flow must not bounce between paths or you get reordering. Linux uses `fib_multipath_hash_policy`.
- **Built in.** P12.12 — ECMP and flow-hash stability.

---

## Cloud-Native

### CNI · *Container Network Interface*
- **What people say.** How Kubernetes plugins set up Pod networking.
- **What it is.** A spec defining ADD, DEL, CHECK operations. The kubelet calls a CNI plugin binary with stdin config and `CNI_*` environment variables. The plugin creates the network namespace's interface, allocates an IP via IPAM, and writes routes. Calico, Flannel, Cilium are CNI plugins.
- **Built in.** P13.05 — CNI specification.

### VXLAN · *Virtual eXtensible LAN*
- **What people say.** Ethernet frames tunneled over UDP.
- **What it is.** RFC 7348. Encapsulates an L2 frame in UDP (port 4789) plus an 8-byte VXLAN header containing a 24-bit VNI (16M VLANs, vs 802.1Q's 4K). VTEP (VXLAN Tunnel Endpoint) is the encap/decap device. Used by Kubernetes overlay networks.
- **Built in.** P13.07 — VXLAN on Linux.

### kube-proxy
- **What people say.** How Service IPs work in Kubernetes.
- **What it is.** A daemon on every node that programs iptables/IPVS/eBPF rules to DNAT ClusterIP traffic to one of the backing Pod IPs. Modes: iptables (default), IPVS, and (with Cilium) eBPF replacing kube-proxy entirely.
- **Built in.** P13.13 — kube-proxy modes.

### Service mesh
- **What people say.** A proxy in front of every microservice.
- **What it is.** Sidecar proxies (or, in ambient mode, node-local proxies) handle mTLS, retries, circuit breaking, observability. Envoy + Istio is the canonical stack; Linkerd uses a Rust micro-proxy. Cilium does it in eBPF without a userspace proxy.
- **Built in.** P14.01 — What is a service mesh.

---

## DDS & Robotics

### DDS · *Data Distribution Service*
- **What people say.** OMG's data-centric pub/sub middleware standard.
- **What it is.** Spec family: DDS 1.4 (DCPS API), DDSI-RTPS 2.5 (wire), DDS-XTypes 1.3 (type system), DDS-Security 1.2 (plugins). Implementations include Eclipse Cyclone DDS, eProsima Fast DDS, RTI Connext. ROS 2 is built on DDS via the rmw layer.
- **Built in.** P15.01 — Why DDS for robotics.

### RTPS · *Real-Time Publish-Subscribe*
- **What people say.** The wire protocol behind DDS.
- **What it is.** OMG DDSI-RTPS 2.5. Has four PIM modules: Structure (entities), Messages (header + 12 submessage types), Behavior (reliability state machine), Discovery (SPDP for participants, SEDP for endpoints). Default UDP port formula: `7400 + 250·domainId + offsets`.
- **Built in.** P15.03 — The four PIM modules of RTPS.

### GUID · *Globally Unique Identifier (RTPS)*
- **What people say.** Identifies any RTPS entity.
- **What it is.** 16 bytes total: 12-byte GuidPrefix (host + process + counter) plus 4-byte EntityId (kind + key). Allocated per-Participant, per-Writer, per-Reader. The GuidPrefix is shared across all entities in one Participant.
- **Built in.** P15.04 — The RTPS GUID (16 bytes).

### CDR · *Common Data Representation*
- **What people say.** How DDS serializes data.
- **What it is.** OMG IDL-based serialization. Primitives in either little-endian (LE) or big-endian (BE), with a flag in the encapsulation header. Strings are length-prefixed and null-terminated. Used as the payload of every DATA submessage.
- **Built in.** P15.05 — CDR (Common Data Representation).

### QoS · *Quality of Service*
- **What people say.** Per-topic delivery semantics in DDS.
- **What it is.** 22 standard policies, including RELIABILITY {BEST_EFFORT, RELIABLE}, DURABILITY {VOLATILE, TRANSIENT_LOCAL, TRANSIENT, PERSISTENT}, HISTORY {KEEP_LAST, KEEP_ALL}, DEADLINE, LIVELINESS, OWNERSHIP. A Reader and Writer must have compatible (offered ⊇ requested) QoS to communicate.
- **Built in.** P15.13 — DDS QoS policies.

### PREEMPT_RT · *Real-Time Preemption Patch*
- **What people say.** Linux made deterministic.
- **What it is.** Mainlined into Linux 6.13 as `CONFIG_PREEMPT_RT`. Converts most kernel sections to be preemptible. Mutexes use priority inheritance. Combined with SCHED_FIFO/SCHED_DEADLINE on isolated CPUs, gets you sub-100µs worst-case scheduling latency on commodity hardware.
- **Built in.** P15.21 — Real-time Linux: PREEMPT_RT.
