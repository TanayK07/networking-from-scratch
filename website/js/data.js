/* ================================================================
 *  data.js — Single source of truth for Networking from Scratch
 *  Merges index.html, catalog.html, roadmap.html into one file.
 * ================================================================ */

window.NFS = window.NFS || {};

/* ---------- helper ------------------------------------------------ */
function slugify(num, title) {
  var s = title.toLowerCase()
    .replace(/[^a-z0-9\s-]/g, '')
    .replace(/\s+/g, '-')
    .replace(/-+/g, '-')
    .replace(/^-|-$/g, '');
  return String(num).padStart(2, '0') + '-' + s;
}

/* ---------- helper to build lesson tuple ------------------------- */
function L(num, title, lang, type) {
  return [title, lang, type, slugify(num, title)];
}

/* ---------- CONFIG ------------------------------------------------ */
NFS.CONFIG = {
  repoOwner: 'Tanayk07',
  repoName: 'networking-from-scratch',
  repoUrl: 'https://github.com/Tanayk07/networking-from-scratch'
};

/* ---------- PHASES ------------------------------------------------ */
NFS.PHASES = [
  /* ── P1 ── */
  {
    id: 'P1',
    title: 'Bits & Wires',
    slug: '01-bits-and-wires',
    desc: 'The physical and signal layer; how bits become electricity, photons, or radio. Encoding, framing, error detection.',
    count: 12,
    status: 'in-progress',
    bonus: false,
    deps: [],
    x: 80, y: 80,
    lessons: [
      L(1,  'What is a network?',                     'py',  'theory'),
      L(2,  'Bits, bytes, and endianness',             'c',   'implement'),
      L(3,  'Hex dumps and the binary mind',           'c',   'implement'),
      L(4,  'Bit fields and packed structs',           'c',   'implement'),
      L(5,  'Manchester, NRZ, 4B/5B encodings',        'py',  'implement'),
      L(6,  'Modulation in 90 seconds',                'py',  'theory'),
      L(7,  'Channel capacity (Nyquist + Shannon)',    'py',  'theory'),
      L(8,  'Errors and CRCs (CRC-8/16/32)',           'c',   'implement'),
      L(9,  'Forward error correction (Hamming 7,4)',  'c',   'implement'),
      L(10, 'Framing: byte stuffing, bit stuffing',    'c',   'implement'),
      L(11, "What's actually on the wire",             'py',  'theory'),
      L(12, 'Capstone: a virtual wire',                'c',   'capstone')
    ]
  },

  /* ── P2 ── */
  {
    id: 'P2',
    title: 'Link Layer',
    slug: '02-link-layer',
    desc: 'Ethernet, MAC addressing, ARP, switches. Raw sockets and TUN/TAP devices.',
    count: 18,
    status: 'in-progress',
    bonus: false,
    deps: ['P1'],
    x: 280, y: 80,
    lessons: [
      L(1,  'Ethernet II frame layout',               'c',   'implement'),
      L(2,  'MAC addresses and OUI',                   'py',  'implement'),
      L(3,  'Raw sockets on Linux (AF_PACKET)',        'c',   'implement'),
      L(4,  'TUN vs TAP devices',                      'c',   'implement'),
      L(5,  'Send your first frame',                   'c',   'implement'),
      L(6,  'ARP request / reply',                     'c',   'implement'),
      L(7,  'The ARP cache',                           'c',   'implement'),
      L(8,  'VLANs (802.1Q)',                          'c',   'implement'),
      L(9,  'Spanning Tree (STP) intuition',           'py',  'theory'),
      L(10, 'Switching tables and MAC learning',       'c',   'implement'),
      L(11, 'Promiscuous mode',                        'c',   'implement'),
      L(12, 'PCAP file format',                        'c',   'implement'),
      L(13, 'Ethernet jumbo frames and MTU',           'py',  'theory'),
      L(14, 'Wake-on-LAN',                             'py',  'implement'),
      L(15, 'PPP and SLIP, briefly',                   'c',   'implement'),
      L(16, '802.11 frame types',                      'py',  'implement'),
      L(17, 'Bridges in Linux',                        'py',  'implement'),
      L(18, 'Capstone: a tiny L2 switch',              'c',   'capstone')
    ]
  },

  /* ── P3 ── */
  {
    id: 'P3',
    title: 'Network Layer: IPv4, IPv6, ICMP',
    slug: '03-network-layer',
    desc: 'IP headers, fragmentation, routing, ICMP, NDP, DHCP, NAT.',
    count: 22,
    status: 'in-progress',
    bonus: false,
    deps: ['P2'],
    x: 480, y: 80,
    lessons: [
      L(1,  'The IPv4 header (RFC 791)',               'c',   'implement'),
      L(2,  'The Internet checksum (RFC 1071)',        'c',   'implement'),
      L(3,  'CIDR and subnetting',                     'py',  'implement'),
      L(4,  'Longest-prefix match routing',            'c',   'implement'),
      L(5,  'The routing table',                       'c',   'implement'),
      L(6,  'TTL and the traceroute trick',            'py',  'implement'),
      L(7,  'IP fragmentation',                        'c',   'implement'),
      L(8,  'Path MTU Discovery (RFC 1191)',           'c',   'implement'),
      L(9,  'ICMPv4: ping clone',                      'c',   'implement'),
      L(10, 'Minimal IP layer on TUN',                 'c',   'implement'),
      L(11, 'IP options',                              'c',   'implement'),
      L(12, 'IPv6 header (RFC 8200)',                  'c',   'implement'),
      L(13, 'IPv6 addressing',                         'py',  'implement'),
      L(14, 'NDP (Neighbor Discovery)',                'c',   'implement'),
      L(15, 'SLAAC',                                   'c',   'implement'),
      L(16, 'DHCPv4 (RFC 2131)',                       'py',  'implement'),
      L(17, 'NAT — what really happens',               'c',   'implement'),
      L(18, 'Hairpin NAT and STUN basics',             'py',  'implement'),
      L(19, 'Tunneling: IP-in-IP, GRE',               'c',   'implement'),
      L(20, 'Routing in Linux: FIB and rtnetlink',     'c',   'implement'),
      L(21, 'Policy routing and multiple tables',      'py',  'theory'),
      L(22, 'Capstone: a userspace IP router',         'c',   'capstone')
    ]
  },

  /* ── P4 ── */
  {
    id: 'P4',
    title: 'Transport: UDP, TCP, QUIC',
    slug: '04-transport',
    desc: 'The deepest phase. UDP, TCP state machine, retransmission, congestion control (Reno, CUBIC, BBR), QUIC.',
    count: 40,
    status: 'planned',
    bonus: false,
    deps: ['P3'],
    x: 680, y: 80,
    lessons: [
      L(1,  'The transport-layer job',                 'py',  'theory'),
      L(2,  'UDP header (RFC 768)',                    'c',   'implement'),
      L(3,  'UDP checksum with IPv4 pseudo-header',    'c',   'implement'),
      L(4,  'A UDP echo server',                       'c',   'implement'),
      L(5,  'A UDP file-transfer protocol',            'c',   'implement'),
      L(6,  'TCP, the protocol (RFC 9293)',            'py',  'theory'),
      L(7,  'The TCP header',                          'c',   'implement'),
      L(8,  'TCP checksum',                            'c',   'implement'),
      L(9,  'The three-way handshake',                 'c',   'implement'),
      L(10, 'The TCP state machine (11 states)',       'c',   'implement'),
      L(11, 'TCB (Transmission Control Block)',        'c',   'implement'),
      L(12, 'Sequence-number arithmetic mod 2³²', 'c', 'implement'),
      L(13, 'Sliding window basics',                   'py',  'theory'),
      L(14, 'Window scaling option (RFC 7323)',        'c',   'implement'),
      L(15, 'TCP options encoder/decoder',             'c',   'implement'),
      L(16, 'Reliable data transfer',                  'c',   'implement'),
      L(17, 'The retransmission timer (RFC 6298)',     'c',   'implement'),
      L(18, 'Retransmission queue',                    'c',   'implement'),
      L(19, 'Selective Acknowledgment (SACK)',         'c',   'implement'),
      L(20, "Delayed ACKs and Nagle's algorithm",      'c',   'implement'),
      L(21, 'Receive window and zero-window probes',   'c',   'implement'),
      L(22, 'Connection teardown and TIME-WAIT',       'c',   'implement'),
      L(23, 'TIME-WAIT in production',                 'py',  'theory'),
      L(24, 'Slow start and congestion avoidance',     'py',  'theory'),
      L(25, 'Fast retransmit and fast recovery',       'c',   'implement'),
      L(26, 'TCP Reno',                                'c',   'implement'),
      L(27, 'TCP CUBIC (RFC 8312)',                    'c',   'implement'),
      L(28, 'TCP BBR with software pacing',            'c',   'implement'),
      L(29, 'Pluggable congestion control',            'py',  'theory'),
      L(30, 'Bufferbloat and CoDel',                   'py',  'theory'),
      L(31, 'Explicit Congestion Notification (ECN)',  'c',   'implement'),
      L(32, 'TCP keepalive',                           'py',  'implement'),
      L(33, 'Connection tracking from outside',        'py',  'implement'),
      L(34, 'Test our stack against real curl',        'c',   'implement'),
      L(35, 'QUIC overview (RFC 9000)',                'py',  'theory'),
      L(36, 'QUIC packets and frames',                 'c',   'implement'),
      L(37, 'QUIC streams',                            'py',  'implement'),
      L(38, 'QUIC connection migration',               'py',  'implement'),
      L(39, 'QUIC congestion control',                 'py',  'theory'),
      L(40, 'Capstone: end-to-end TCP over TUN',      'c',   'capstone')
    ]
  },

  /* ── P5 ── */
  {
    id: 'P5',
    title: 'Sockets & I/O Models',
    slug: '05-sockets-io-models',
    desc: 'Berkeley sockets, blocking vs non-blocking, epoll, io_uring, asyncio, the C10K mental model.',
    count: 16,
    status: 'planned',
    bonus: false,
    deps: ['P4'],
    x: 680, y: 200,
    lessons: [
      L(1,  'The Berkeley sockets API',                'c',   'implement'),
      L(2,  'Address families',                        'c',   'implement'),
      L(3,  'getaddrinfo (modern resolution)',         'c',   'implement'),
      L(4,  'Blocking vs nonblocking',                 'c',   'implement'),
      L(5,  'select and poll',                         'c',   'implement'),
      L(6,  'epoll: edge vs level triggered',          'c',   'implement'),
      L(7,  'epoll multithread/multiprocess gotchas',  'c',   'theory'),
      L(8,  'Unix domain sockets',                     'c',   'implement'),
      L(9,  'Non-blocking connect()',                   'c',   'implement'),
      L(10, 'sendfile and splice (zero-copy)',         'c',   'implement'),
      L(11, 'Scatter/gather: readv, writev',           'c',   'implement'),
      L(12, 'io_uring 101',                            'c',   'implement'),
      L(13, 'io_uring for networking',                 'c',   'implement'),
      L(14, 'asyncio in Python',                       'py',  'implement'),
      L(15, 'Multiplexing patterns',                   'c',   'implement'),
      L(16, 'Capstone: 10k-connection HTTP/1.0 server','c',   'capstone')
    ]
  },

  /* ── P6 ── */
  {
    id: 'P6',
    title: 'Application Protocols',
    slug: '06-application-protocols',
    desc: 'DNS, HTTP/1.1, HTTP/2, HTTP/3, WebSocket, SSH, NTP, MQTT, CoAP. Build the parsers from scratch.',
    count: 28,
    status: 'planned',
    bonus: false,
    deps: ['P5'],
    x: 880, y: 200,
    lessons: [
      L(1,  'DNS message format (RFC 1035)',           'py',  'implement'),
      L(2,  'A DNS resolver from scratch',             'py',  'implement'),
      L(3,  'Authoritative DNS server',                'py',  'implement'),
      L(4,  'DNS over UDP, then TCP',                  'py',  'implement'),
      L(5,  'EDNS(0) (RFC 6891)',                      'py',  'implement'),
      L(6,  'DoT and DoH',                             'py',  'implement'),
      L(7,  'mDNS and DNS-SD',                         'py',  'implement'),
      L(8,  'HTTP/1.0 and HTTP/1.1 parser',           'c',   'implement'),
      L(9,  'A toy HTTP server in C',                  'c',   'implement'),
      L(10, 'Persistent connections and pipelining',   'c',   'implement'),
      L(11, 'Chunked transfer encoding',               'c',   'implement'),
      L(12, 'HTTP/2 binary framing',                   'py',  'implement'),
      L(13, 'HPACK header compression',                'py',  'implement'),
      L(14, 'HTTP/2 stream prioritization',            'py',  'theory'),
      L(15, 'HTTP/3 over QUIC',                        'py',  'implement'),
      L(16, 'WebSocket (RFC 6455)',                     'c',   'implement'),
      L(17, 'SSH, structurally',                       'py',  'theory'),
      L(18, 'SMTP, IMAP, POP3 — quick tour',          'py',  'theory'),
      L(19, 'FTP active vs passive',                   'py',  'theory'),
      L(20, 'NTP and SNTP client',                     'c',   'implement'),
      L(21, 'Syslog (RFC 5424)',                       'py',  'implement'),
      L(22, 'MQTT 5.0 client',                         'py',  'implement'),
      L(23, 'CoAP for IoT',                            'py',  'implement'),
      L(24, 'gRPC: HTTP/2 + protobuf',                'py',  'theory'),
      L(25, 'HTTP/1.1 with virtual hosts',             'c',   'implement'),
      L(26, 'Capstone: HTTP CONNECT proxy',            'c',   'capstone'),
      L(27, 'URL encoding & MIME types',               'py',  'implement'),
      L(28, 'Cookies and session management',          'py',  'implement')
    ]
  },

  /* ── P7 ── */
  {
    id: 'P7',
    title: 'TLS & Secure Transport',
    slug: '07-tls',
    desc: 'AEAD, HKDF, X25519, Ed25519, X.509, TLS 1.3 byte-by-byte, kTLS.',
    count: 16,
    status: 'planned',
    bonus: false,
    deps: ['P4'],
    x: 880, y: 80,
    lessons: [
      L(1,  'Five primitives in 30 minutes',           'py',  'theory'),
      L(2,  'AES-128-GCM by hand',                    'c',   'implement'),
      L(3,  'HMAC and HKDF',                           'py',  'implement'),
      L(4,  'Diffie-Hellman, then X25519',             'c',   'implement'),
      L(5,  'Digital signatures (RSA-PSS, Ed25519)',   'py',  'implement'),
      L(6,  'X.509 certificate parser',                'py',  'implement'),
      L(7,  'The TLS record layer',                    'c',   'implement'),
      L(8,  'TLS 1.3 ClientHello byte by byte',       'c',   'implement'),
      L(9,  'ServerHello and the key schedule',        'c',   'implement'),
      L(10, 'EncryptedExtensions through Finished',    'c',   'implement'),
      L(11, '0-RTT and PSK resumption',                'py',  'theory'),
      L(12, 'TLS 1.3 client to fetch cloudflare.com', 'c',   'implement'),
      L(13, 'mTLS with client certificates',           'c',   'implement'),
      L(14, 'Cipher-suite selection in 2026',          'py',  'theory'),
      L(15, 'TLS in the kernel: kTLS',                 'c',   'implement'),
      L(16, 'Capstone: minimal HTTPS server',          'c',   'capstone')
    ]
  },

  /* ── P8 ── */
  {
    id: 'P8',
    title: 'Userspace TCP/IP Stack',
    slug: '08-userspace-stack',
    desc: 'Capstone phase. A full TCP/IP stack on a TUN device that can curl a real HTTPS site.',
    count: 12,
    status: 'planned',
    bonus: false,
    deps: ['P4', 'P5', 'P6', 'P7'],
    x: 1080, y: 140,
    lessons: [
      L(1,  'Project skeleton',                        'c',   'implement'),
      L(2,  'The main loop: tun_read dispatch',        'c',   'implement'),
      L(3,  'L2/L3 plumbing',                          'c',   'implement'),
      L(4,  'Sockets compatibility shim (LD_PRELOAD)', 'c',   'implement'),
      L(5,  'Run curl through it',                     'c',   'implement'),
      L(6,  'Robustness pass with AFL++ fuzzing',      'c',   'implement'),
      L(7,  'Performance pass with perf',              'c',   'implement'),
      L(8,  'TLS layer on top',                        'c',   'implement'),
      L(9,  'HTTP/2 on top',                           'c',   'implement'),
      L(10, 'Multi-connection load test (wrk -c 100)', 'c',   'implement'),
      L(11, 'Document the stack with Doxygen',         'c',   'implement'),
      L(12, 'Capstone: ship lvl-ip-rebuilt',           'c',   'capstone')
    ]
  },

  /* ── P9 ── */
  {
    id: 'P9',
    title: 'Linux Kernel Networking Internals',
    slug: '09-kernel-internals',
    desc: 'sk_buff, NAPI, GRO, qdisc, netfilter, conntrack, network namespaces, veth, bridges. Real kernel modules.',
    count: 24,
    status: 'planned',
    bonus: false,
    deps: ['P3', 'P5'],
    x: 80, y: 320,
    lessons: [
      L(1,  'Booting a development kernel (virtme-ng)','c',   'theory'),
      L(2,  'Loadable kernel modules: hello world',    'c',   'implement'),
      L(3,  'The sk_buff: head, data, tail, end',      'c',   'implement'),
      L(4,  'napi_alloc_skb and the page_pool',        'c',   'implement'),
      L(5,  'NAPI: schedule, poll, complete',          'c',   'implement'),
      L(6,  'Receive path end to end',                 'py',  'theory'),
      L(7,  'GRO (Generic Receive Offload)',           'py',  'theory'),
      L(8,  'RPS, RFS, RSS receive packet steering',   'py',  'theory'),
      L(9,  'Send path end to end',                    'c',   'theory'),
      L(10, 'qdisc and traffic control',               'py',  'implement'),
      L(11, 'Netfilter hooks: a logging firewall LKM', 'c',   'implement'),
      L(12, 'iptables vs nftables',                    'py',  'theory'),
      L(13, 'conntrack: connection tracking table',    'c',   'implement'),
      L(14, 'Network namespaces',                      'py',  'implement'),
      L(15, 'veth pairs',                              'py',  'implement'),
      L(16, 'Linux bridges',                           'py',  'implement'),
      L(17, 'Network device drivers (a virtual NIC)',  'c',   'implement'),
      L(18, 'AF_PACKET TPACKETv3',                     'c',   'implement'),
      L(19, 'GSO/TSO/LRO offloads',                   'py',  'theory'),
      L(20, '/proc/net/snmp Prometheus exporter',      'py',  'implement'),
      L(21, '/proc/net/softnet_stat reader',           'py',  'implement'),
      L(22, 'ethtool tuning script',                   'py',  'implement'),
      L(23, 'Building a netfilter LKM',                'c',   'implement'),
      L(24, 'Capstone: 1% packet-loss injection LKM', 'c',   'capstone')
    ]
  },

  /* ── P10 ── */
  {
    id: 'P10',
    title: 'eBPF, XDP, and Kernel Bypass',
    slug: '10-ebpf-xdp',
    desc: 'BPF programs, XDP at line rate, AF_XDP, DPDK userspace polling, an XDP load balancer.',
    count: 18,
    status: 'planned',
    bonus: false,
    deps: ['P9'],
    x: 280, y: 320,
    lessons: [
      L(1,  'What is eBPF: verifier, JIT',            'bpf',  'theory'),
      L(2,  'Map types: array, hash, LRU, ring',      'bpf',  'implement'),
      L(3,  'CO-RE: BTF and bpf_core_read',           'bpf',  'implement'),
      L(4,  'XDP basics: PASS, DROP, TX, REDIRECT',   'bpf',  'implement'),
      L(5,  'An XDP load balancer (~200 LOC)',         'bpf',  'implement'),
      L(6,  'AF_XDP userspace-mapped rings',           'c',    'implement'),
      L(7,  'TC eBPF programs',                        'bpf',  'implement'),
      L(8,  'kprobes and tracepoints with eBPF',       'bpf',  'implement'),
      L(9,  'bpftrace one-liners (20 useful)',         'bpf',  'implement'),
      L(10, 'bcc tools: tcpconnect, tcpretrans',       'py',   'theory'),
      L(11, 'DPDK overview: hugepages, EAL, mbuf',    'c',    'theory'),
      L(12, 'A DPDK l2fwd application',                'c',    'implement'),
      L(13, 'DPDK ring buffer: lockless SPSC',         'c',    'implement'),
      L(14, 'AF_PACKET vs AF_XDP vs DPDK',            'py',   'theory'),
      L(15, 'eBPF for security (LSM_BPF)',             'bpf',  'implement'),
      L(16, 'eBPF for service mesh (Cilium preview)',  'py',   'theory'),
      L(17, 'Capstone: XDP DDoS scrubber',            'bpf',  'capstone'),
      L(18, 'Capstone: AF_XDP packet generator',      'c',    'capstone')
    ]
  },

  /* ── P11 ── */
  {
    id: 'P11',
    title: 'Performance & Observability',
    slug: '11-perf-and-observability',
    desc: 'tcpdump, Wireshark dissectors, perf, flame graphs, IRQ affinity, NUMA, latency histograms.',
    count: 14,
    status: 'planned',
    bonus: false,
    deps: ['P9', 'P10'],
    x: 480, y: 320,
    lessons: [
      L(1,  'The four golden signals for networks',    'py',  'theory'),
      L(2,  'tcpdump as a debugger',                   'py',  'implement'),
      L(3,  'Wireshark dissectors in Lua',             'lua', 'implement'),
      L(4,  'perf for network analysis',               'py',  'implement'),
      L(5,  'Flame graphs of the network stack',       'py',  'implement'),
      L(6,  'IRQ affinity and NUMA',                   'py',  'implement'),
      L(7,  'CPU isolation: isolcpus, nohz_full',     'py',  'theory'),
      L(8,  'NIC tuning: rings, coalescing, RSS',     'py',  'implement'),
      L(9,  'Buffer-bloat diagnosis with flent',       'py',  'implement'),
      L(10, 'Latency histograms (HdrHistogram)',       'c',   'implement'),
      L(11, 'Tracing a slow request end to end',       'py',  'implement'),
      L(12, 'Production-network war stories',          'py',  'theory'),
      L(13, 'Latency budget for an RPC',               'py',  'theory'),
      L(14, 'Capstone: an observability stack',        'c',   'capstone')
    ]
  },

  /* ── P12 ── */
  {
    id: 'P12',
    title: 'Routing: BGP, OSPF, Control Plane',
    slug: '12-routing',
    desc: 'Distance-vector vs link-state, OSPF Hello/DBD, BGP path selection, MPLS, anycast, ECMP, Mininet labs.',
    count: 16,
    status: 'planned',
    bonus: false,
    deps: ['P3'],
    x: 80, y: 460,
    lessons: [
      L(1,  'Control vs data plane',                   'py',  'theory'),
      L(2,  'Distance-vector vs link-state (RIP, OSPF)','py', 'theory'),
      L(3,  'OSPF basics: Hello, DBD, LSR, LSU',      'py',  'implement'),
      L(4,  "Dijkstra's algorithm on the LSDB",        'py',  'implement'),
      L(5,  'A toy OSPF speaker on Mininet',           'py',  'implement'),
      L(6,  'BGP basics: OPEN, UPDATE, KEEPALIVE',    'py',  'implement'),
      L(7,  'BGP path-selection algorithm',            'py',  'implement'),
      L(8,  'A toy BGP speaker',                       'py',  'implement'),
      L(9,  'Route reflectors and confederations',     'py',  'theory'),
      L(10, 'MPLS in 30 minutes',                     'py',  'theory'),
      L(11, 'Anycast: how DNS root servers work',      'py',  'theory'),
      L(12, 'ECMP and flow-hash stability',            'c',   'implement'),
      L(13, 'SDN intuition: OpenFlow, P4',             'py',  'theory'),
      L(14, 'Mininet: 10-router topology',             'py',  'implement'),
      L(15, 'Capstone: a BGP route reflector',         'py',  'capstone'),
      L(16, 'Capstone: an OSPF area-0 simulator',     'py',  'capstone')
    ]
  },

  /* ── P13 ── */
  {
    id: 'P13',
    title: 'Overlays & Container Networking',
    slug: '13-overlays-and-containers',
    desc: 'veth pairs, bridges, CNI plugins from scratch, VXLAN, WireGuard, kube-proxy, NetworkPolicy.',
    count: 16,
    status: 'planned',
    bonus: false,
    deps: ['P9', 'P12'],
    x: 280, y: 460,
    lessons: [
      L(1,  'The container-networking model',          'py',  'theory'),
      L(2,  'Network namespaces refresher',            'py',  'implement'),
      L(3,  'veth pairs and bridges, end to end',      'py',  'implement'),
      L(4,  'NAT for outbound: masquerade',            'py',  'implement'),
      L(5,  'CNI specification: ADD, DEL, CHECK',     'py',  'theory'),
      L(6,  'A bridge CNI in Python',                  'py',  'implement'),
      L(7,  'VXLAN on Linux (UDP/4789)',               'py',  'implement'),
      L(8,  'VXLAN: multicast vs unicast FDB',        'py',  'implement'),
      L(9,  'GRE and IP-in-IP tunnels',                'py',  'implement'),
      L(10, 'WireGuard, structurally',                 'py',  'theory'),
      L(11, 'Calico and Flannel, conceptually',        'py',  'theory'),
      L(12, 'Cilium: eBPF-native CNI',                'py',  'theory'),
      L(13, 'kube-proxy modes: iptables, IPVS, eBPF', 'py',  'theory'),
      L(14, 'Service IPs and DNAT',                    'py',  'implement'),
      L(15, 'NetworkPolicy default-deny',              'py',  'implement'),
      L(16, 'Capstone: a tiny CNI plugin',             'py',  'capstone')
    ]
  },

  /* ── P14 ── */
  {
    id: 'P14',
    title: 'Service Mesh & Cloud-Native',
    slug: '14-service-mesh',
    desc: 'Envoy, xDS, sidecars, ambient meshes, mTLS in the mesh, OpenTelemetry, edge proxies.',
    count: 12,
    status: 'planned',
    bonus: false,
    deps: ['P7', 'P13'],
    x: 480, y: 460,
    lessons: [
      L(1,  'What is a service mesh',                  'py',  'theory'),
      L(2,  'Envoy at a glance: listeners, clusters',  'py',  'theory'),
      L(3,  'xDS API: a minimal LDS/CDS/RDS server',  'py',  'implement'),
      L(4,  'mTLS in the mesh (SPIFFE/SPIRE)',         'py',  'theory'),
      L(5,  'Sidecar injection via webhook',           'py',  'implement'),
      L(6,  'Ambient meshes (Istio Ambient)',          'py',  'theory'),
      L(7,  'Cilium service mesh with eBPF',           'py',  'theory'),
      L(8,  "Linkerd's design choices",                 'py',  'theory'),
      L(9,  'OpenTelemetry context propagation',       'py',  'implement'),
      L(10, 'L7 authorization with JWT',               'py',  'implement'),
      L(11, 'Edge proxies (Cloudflare Workers model)', 'py',  'theory'),
      L(12, 'Capstone: 200-line L7 proxy with mTLS',  'c',   'capstone')
    ]
  },

  /* ── P15 ── */
  {
    id: 'P15',
    title: 'BONUS: DDS, RTPS, and Robotics Middleware',
    slug: '15-dds-robotics-bonus',
    desc: 'OMG DDSI-RTPS 2.5, Cyclone DDS internals, ROS 2 RMW, PREEMPT_RT, shared-memory transports, TSN.',
    count: 25,
    status: 'planned',
    bonus: true,
    deps: ['P4', 'P5', 'P9'],
    x: 880, y: 460,
    lessons: [
      L(1,  'Why DDS for robotics',                    'py',  'theory'),
      L(2,  'The DDS family of OMG specs',             'py',  'theory'),
      L(3,  'The four PIM modules of RTPS',            'c',   'theory'),
      L(4,  'The RTPS GUID (16 bytes)',                'c',   'implement'),
      L(5,  'CDR (Common Data Representation)',        'c',   'implement'),
      L(6,  'The RTPS Header',                         'c',   'implement'),
      L(7,  'RTPS submessages: 12 types',              'c',   'implement'),
      L(8,  'Default UDP port formula',                'c',   'implement'),
      L(9,  'SPDP — Participant Discovery',            'c',   'implement'),
      L(10, 'SEDP — Endpoint Discovery',               'c',   'implement'),
      L(11, 'The reliability state machine',           'c',   'implement'),
      L(12, 'Fragmentation: DATA_FRAG, NACK_FRAG',    'c',   'implement'),
      L(13, 'DDS QoS policies (22 standard)',          'c',   'implement'),
      L(14, 'QoS matching (Requested vs Offered)',     'c',   'implement'),
      L(15, 'Capstone: tiny DDS interop with Cyclone', 'c',   'capstone'),
      L(16, 'Reading the Cyclone DDS source tree',     'py',  'theory'),
      L(17, 'IDL and idlc code generation',            'c',   'implement'),
      L(18, 'ROS 2 RMW: rmw.h abstraction',           'c',   'theory'),
      L(19, 'The default RMW (rmw_fastrtps_cpp)',      'py',  'theory'),
      L(20, 'rosidl type-support generators',          'c',   'implement'),
      L(21, 'Real-time Linux: PREEMPT_RT',             'c',   'theory'),
      L(22, 'Real-time schedulers: SCHED_FIFO, _DEADLINE', 'c', 'implement'),
      L(23, 'Shared-memory transports: Iceoryx, PSMX', 'c',  'implement'),
      L(24, 'Time-Sensitive Networking (802.1Qbv)',    'py',  'theory'),
      L(25, 'Capstone: a robotics demo on PREEMPT_RT', 'c',  'capstone')
    ]
  }
];

/* ---------- GLOSSARY --------------------------------------------- */
NFS.GLOSSARY = [
  {
    cat: 'Layering',
    id: 'layering',
    terms: [
      { name: 'OSI', exp: 'Open Systems Interconnection model',
        say: 'Seven layers: Physical, Data Link, Network, Transport, Session, Presentation, Application.',
        is: 'A reference model. Real networks use the four-layer TCP/IP model. The OSI session and presentation layers are mostly absorbed into the application layer in practice.', ref: 'P1.01' },
      { name: 'MTU', exp: 'Maximum Transmission Unit',
        say: 'The largest packet a link will carry, usually 1500 bytes for Ethernet.',
        is: 'A property of a single link. End-to-end MTU is determined by the smallest MTU on the path. Path MTU Discovery (RFC 1191) finds it dynamically using the DF bit and ICMP "Fragmentation Needed" replies.', ref: 'P3.08' },
      { name: 'MSS', exp: 'Maximum Segment Size',
        say: 'How much TCP data fits in one packet.',
        is: 'A TCP-only concept negotiated in SYN options. MSS = MTU − IP header − TCP header (typically 1460 for IPv4 over Ethernet, 1440 for IPv6). Not the same as MTU; advertised per-direction.', ref: 'P4.07' },
      { name: 'Encapsulation', exp: '',
        say: 'Wrapping a higher-layer packet in a lower-layer header.',
        is: 'The process of adding L2/L3/L4 headers as a packet moves down the sending stack and stripping them as it moves up the receiving stack. Each layer treats the next layer\'s data as opaque payload.', ref: 'P1.01' }
    ]
  },
  {
    cat: 'Link Layer',
    id: 'link',
    terms: [
      { name: 'Ethernet II', exp: 'IEEE 802.3 frame format with EtherType',
        say: 'Standard Ethernet frames you see on every LAN.',
        is: 'A frame format: 6-byte destination MAC, 6-byte source MAC, 2-byte EtherType, payload (46–1500 bytes), 4-byte FCS. Differs from 802.3 LLC/SNAP framing in the EtherType vs length field interpretation.', ref: 'P2.01' },
      { name: 'MAC address', exp: 'Media Access Control address',
        say: '48-bit unique hardware address.',
        is: 'Not always unique — locally administered addresses (LAA) have the U/L bit set. The first 24 bits are the OUI (Organizationally Unique Identifier) assigned by IEEE; the lowest bit of the first byte is the multicast/unicast flag.', ref: 'P2.02' },
      { name: 'ARP', exp: 'Address Resolution Protocol',
        say: 'Maps an IP address to a MAC address.',
        is: 'A broadcast-based L2 protocol (RFC 826). Cache entries are populated on receipt of any ARP traffic, including unsolicited replies — which is why ARP poisoning works. Replaced by NDP in IPv6.', ref: 'P2.06' },
      { name: 'VLAN', exp: 'Virtual LAN',
        say: 'Tag-based traffic separation on the same physical Ethernet.',
        is: '802.1Q adds a 4-byte tag (TPID 0x8100, then 3-bit PCP, 1-bit DEI, 12-bit VID) between the source MAC and EtherType. 4094 usable VIDs (0 and 4095 reserved). The bridge\'s FDB is keyed by (MAC, VID).', ref: 'P2.08' },
      { name: 'TUN/TAP', exp: 'Userspace network device',
        say: 'A virtual NIC you can read/write from a userspace program.',
        is: 'TUN operates at L3 (you read/write IP packets); TAP operates at L2 (you read/write Ethernet frames). Created via /dev/net/tun and ioctl(TUNSETIFF). Used for VPNs, userspace TCP/IP stacks, and our Phase 8 capstone.', ref: 'P2.04' }
    ]
  },
  {
    cat: 'Network Layer',
    id: 'network',
    terms: [
      { name: 'IP fragmentation', exp: '',
        say: 'Splitting a packet too big for the link MTU into smaller pieces.',
        is: 'IPv4 only (IPv6 forbids router fragmentation). The Identification, MF (More Fragments), and Fragment Offset fields in the IP header are used by the receiver to reassemble. One lost fragment loses the whole datagram. Reassembly is bounded by net.ipv4.ipfrag_high_thresh on Linux.', ref: 'P3.07' },
      { name: 'CIDR', exp: 'Classless Inter-Domain Routing',
        say: 'IP/prefix notation, like 10.0.0.0/24.',
        is: 'Replaced classful addressing in 1993. The prefix length is the number of leading bits in the network mask. Used by routing tables for longest-prefix match — the most specific (longest prefix) wins.', ref: 'P3.03' },
      { name: 'Longest-prefix match', exp: 'LPM',
        say: 'How a router picks a route when multiple match.',
        is: 'Standard FIB lookup algorithm. For destination 10.1.2.3, both 10.0.0.0/8 and 10.1.0.0/16 match, but 10.1.0.0/16 wins because its prefix is longer. Implemented with a binary trie or compressed trie (LC-trie in Linux).', ref: 'P3.04' },
      { name: 'TTL', exp: 'Time To Live',
        say: 'How many hops a packet can take before being dropped.',
        is: '8-bit field decremented by each router. When it hits 0, the router drops the packet and sends ICMP Time Exceeded — which is exactly how traceroute works. Renamed Hop Limit in IPv6.', ref: 'P3.06' },
      { name: 'NAT', exp: 'Network Address Translation',
        say: 'Many private IPs share one public IP.',
        is: 'Requires connection tracking — the NAT box stores a 5-tuple mapping (private IP:port → public IP:port) per flow. Source-NAT (masquerade) is the common variant. Breaks end-to-end addressability and many P2P protocols, hence STUN/TURN/ICE.', ref: 'P3.17' }
    ]
  },
  {
    cat: 'Transport Layer',
    id: 'transport',
    terms: [
      { name: 'TCP', exp: 'Transmission Control Protocol',
        say: 'Reliable, ordered, byte-stream protocol.',
        is: 'RFC 9293 (2022). 11-state state machine, sequence numbers mod 2³², sliding windows, retransmission with adaptive RTO, congestion control. The TCB (Transmission Control Block) holds all per-connection state.', ref: 'P4.06' },
      { name: 'Three-way handshake', exp: '',
        say: 'SYN → SYN-ACK → ACK to open a TCP connection.',
        is: 'Synchronizes initial sequence numbers (ISN) in both directions. ISN should be randomized per RFC 6528 to prevent off-path injection. The third ACK can carry data ("piggybacked").', ref: 'P4.09' },
      { name: 'Sliding window', exp: '',
        say: 'How much data can be in flight at once.',
        is: 'A flow-control mechanism. The receiver advertises its free buffer space (RWND); the sender tracks SND.UNA, SND.NXT, and the smaller of CWND (congestion) and RWND (flow). The receiver\'s window can shrink to zero (zero-window probe).', ref: 'P4.13' },
      { name: 'CUBIC', exp: '',
        say: 'The default Linux congestion control algorithm.',
        is: 'RFC 8312. The window grows as a cubic function of time since the last loss event: W(t) = C(t-K)³ + W_max. More aggressive than Reno on high-bandwidth long-RTT links. Replaced as the Linux default by BBR in some deployments.', ref: 'P4.27' },
      { name: 'BBR', exp: 'Bottleneck Bandwidth and RTT',
        say: 'Google\'s congestion control that doesn\'t back off on loss.',
        is: 'A model-based algorithm that estimates bottleneck bandwidth and minimum RTT, then paces packets at that rate. Doesn\'t treat loss as a congestion signal. Per Cardwell et al. (ACM Queue 2016), throughput improvements of 2–25× on Google\'s B4 WAN compared to CUBIC.', ref: 'P4.28' },
      { name: 'QUIC', exp: '',
        say: 'TCP rebuilt over UDP with TLS 1.3 baked in.',
        is: 'RFC 9000. Reliable, ordered streams over UDP, with TLS 1.3 mandatory. Eliminates head-of-line blocking between streams. Connection IDs allow migration across IP changes. The transport layer of HTTP/3.', ref: 'P4.34' }
    ]
  },
  {
    cat: 'Sockets & I/O',
    id: 'sockets',
    terms: [
      { name: 'epoll', exp: '',
        say: 'Linux\'s scalable replacement for select/poll.',
        is: 'O(1) event notification scaling to hundreds of thousands of connections. Edge-triggered mode notifies only on state transitions; level-triggered notifies as long as the condition holds. The C10K mental model is built on epoll.', ref: 'P5.06' },
      { name: 'io_uring', exp: '',
        say: 'Linux\'s newest async I/O interface.',
        is: 'Mmap\'d submission and completion ring buffers shared with the kernel. Zero syscalls per I/O on the fast path. Supports networking ops (RECV, SEND, ACCEPT, SEND_ZC). Faster than epoll for high-throughput, but verifier-restricted in some sandboxed environments.', ref: 'P5.12' },
      { name: 'Zero-copy', exp: '',
        say: 'Sending data without copying it through user space.',
        is: 'sendfile(2) (file → socket), splice(2) (any pipe), MSG_ZEROCOPY (user buffer → socket without copy, page-pinned). Big wins for static-file servers but with subtle correctness gotchas — the kernel must signal when the buffer is reusable.', ref: 'P5.10' }
    ]
  },
  {
    cat: 'Security & TLS',
    id: 'tls',
    terms: [
      { name: 'AEAD', exp: 'Authenticated Encryption with Associated Data',
        say: 'Encrypt and authenticate in one operation.',
        is: 'A primitive that combines confidentiality and integrity. AES-128-GCM, AES-256-GCM, ChaCha20-Poly1305 are the only ones allowed in TLS 1.3. The "associated data" is authenticated but not encrypted — typically the TLS record header.', ref: 'P7.02' },
      { name: 'HKDF', exp: 'HMAC-based Key Derivation Function',
        say: 'Turn a shared secret into multiple keys.',
        is: 'RFC 5869. Two phases: Extract (compress entropy into a pseudorandom key) and Expand (stretch into the desired output length). TLS 1.3 builds its entire key schedule from HKDF.', ref: 'P7.03' },
      { name: 'X25519', exp: '',
        say: 'The modern key exchange for TLS 1.3.',
        is: 'Elliptic-curve Diffie–Hellman over Curve25519 (RFC 7748). 32-byte keys, no parameter validation needed (any 32-byte string is a valid public key), small constant-time implementations. Replaces classical DH and most ECDH curves in TLS 1.3.', ref: 'P7.04' },
      { name: 'Ed25519', exp: '',
        say: 'The modern signature for TLS 1.3.',
        is: 'EdDSA over Curve25519. Deterministic (no per-signature randomness needed), fast, small (64-byte signatures, 32-byte keys). Used in TLS 1.3 certificates and OpenSSH host keys.', ref: 'P7.05' },
      { name: 'kTLS', exp: 'Kernel TLS',
        say: 'TLS record encryption done in the kernel.',
        is: 'setsockopt(TCP_ULP, "tls"). The handshake stays in userspace; once keys are negotiated, application data is encrypted by the kernel. Enables sendfile() to a TLS connection. Linux supports AES-GCM and ChaCha20-Poly1305 in kTLS.', ref: 'P7.16' }
    ]
  },
  {
    cat: 'Linux Kernel',
    id: 'kernel',
    terms: [
      { name: 'sk_buff', exp: '',
        say: 'The kernel\'s packet container.',
        is: 'A struct holding metadata about a packet plus pointers (head, data, tail, end) into the actual data buffer. Kernel-wide, used by every protocol. truesize is charged against the socket\'s receive buffer (sk_rmem_alloc) for accounting. Allocated from a slab cache.', ref: 'P9.03' },
      { name: 'NAPI', exp: 'New API',
        say: 'How the kernel polls NICs instead of taking one interrupt per packet.',
        is: 'A hybrid interrupt-then-poll mechanism. The first packet causes an IRQ; the driver disables further IRQs and schedules a NAPI poll, which drains the NIC ring up to a budget (default 64 packets, controlled by net.core.dev_weight). Re-enables IRQs when drained.', ref: 'P9.05' },
      { name: 'Netfilter', exp: '',
        say: 'The kernel hook framework iptables and nftables build on.',
        is: 'Five hooks: PREROUTING, INPUT (LOCAL_IN), FORWARD, OUTPUT (LOCAL_OUT), POSTROUTING. Each hook can register multiple kernel modules at different priorities. Returns NF_ACCEPT, NF_DROP, NF_QUEUE, NF_STOLEN, or NF_REPEAT.', ref: 'P9.11' },
      { name: 'Network namespace', exp: 'netns',
        say: 'A separate isolated network stack inside Linux.',
        is: 'Created via clone(CLONE_NEWNET) or unshare(--net). Each has its own interfaces, routing table, ARP cache, /proc/net entries, and netfilter rules. Container networking is built on this. Move an interface in with ip link set <iface> netns <name>.', ref: 'P9.14' },
      { name: 'eBPF', exp: 'extended Berkeley Packet Filter',
        say: 'Sandboxed kernel programs you load from userspace.',
        is: 'A JIT-compiled VM running verifier-checked bytecode in the kernel. Used for packet filtering, tracing, security policy, and (via XDP) line-rate packet processing. Programs cannot loop unboundedly, dereference unchecked pointers, or block — the verifier rejects them.', ref: 'P10.01' },
      { name: 'XDP', exp: 'eXpress Data Path',
        say: 'Earliest hook in the receive path.',
        is: 'An eBPF program that runs immediately after the NIC driver allocates an sk_buff (or before, in driver-mode XDP). Returns XDP_PASS, XDP_DROP, XDP_TX (echo back), or XDP_REDIRECT (to another iface or AF_XDP socket). Enables line-rate DDoS scrubbing.', ref: 'P10.04' }
    ]
  },
  {
    cat: 'Routing',
    id: 'routing',
    terms: [
      { name: 'BGP', exp: 'Border Gateway Protocol',
        say: 'The routing protocol of the Internet.',
        is: 'A path-vector protocol over TCP/179. Routers exchange OPEN, UPDATE, KEEPALIVE, NOTIFICATION messages. UPDATE carries NLRI (prefixes) plus path attributes (AS-PATH, NEXT_HOP, LOCAL_PREF, MED). Path selection: highest LOCAL_PREF, shortest AS-PATH, lowest origin, etc.', ref: 'P12.06' },
      { name: 'OSPF', exp: 'Open Shortest Path First',
        say: 'Link-state routing inside an AS.',
        is: 'Routers flood LSAs (Link State Advertisements) describing their links. Each router builds a full topology database and runs Dijkstra to compute shortest paths. Hello/DBD/LSR/LSU/LSAck packet types. Areas reduce LSDB size in large networks.', ref: 'P12.03' },
      { name: 'ECMP', exp: 'Equal-Cost Multi-Path',
        say: 'Spread traffic across multiple equal-cost paths.',
        is: 'A router with N equal-cost next-hops hashes a flow\'s 5-tuple and picks one consistently. Stability matters — a flow must not bounce between paths or you get reordering. Linux uses fib_multipath_hash_policy.', ref: 'P12.12' },
      { name: 'Anycast', exp: '',
        say: 'Same IP advertised from many locations.',
        is: 'Multiple servers announce the same prefix via BGP. Each client gets routed to the nearest one (BGP-nearest, not always geographically nearest). DNS root servers, public DNS resolvers, and CDN edge caches use anycast.', ref: 'P12.11' }
    ]
  },
  {
    cat: 'Cloud-Native',
    id: 'cloud',
    terms: [
      { name: 'CNI', exp: 'Container Network Interface',
        say: 'How Kubernetes plugins set up Pod networking.',
        is: 'A spec defining ADD, DEL, CHECK operations. The kubelet calls a CNI plugin binary with stdin config and CNI_* environment variables. The plugin creates the network namespace\'s interface, allocates an IP via IPAM, and writes routes. Calico, Flannel, Cilium are CNI plugins.', ref: 'P13.05' },
      { name: 'VXLAN', exp: 'Virtual eXtensible LAN',
        say: 'Ethernet frames tunneled over UDP.',
        is: 'RFC 7348. Encapsulates an L2 frame in UDP (port 4789) plus an 8-byte VXLAN header containing a 24-bit VNI (16M VLANs, vs 802.1Q\'s 4K). VTEP (VXLAN Tunnel Endpoint) is the encap/decap device. Used by Kubernetes overlay networks.', ref: 'P13.07' },
      { name: 'kube-proxy', exp: '',
        say: 'How Service IPs work in Kubernetes.',
        is: 'A daemon on every node that programs iptables/IPVS/eBPF rules to DNAT ClusterIP traffic to one of the backing Pod IPs. Modes: iptables (default), IPVS, and (with Cilium) eBPF replacing kube-proxy entirely.', ref: 'P13.13' },
      { name: 'Service mesh', exp: '',
        say: 'A proxy in front of every microservice.',
        is: 'Sidecar proxies (or, in ambient mode, node-local proxies) handle mTLS, retries, circuit breaking, observability. Envoy + Istio is the canonical stack; Linkerd uses a Rust micro-proxy. Cilium does it in eBPF without a userspace proxy.', ref: 'P14.01' }
    ]
  },
  {
    cat: 'DDS & Robotics',
    id: 'dds',
    terms: [
      { name: 'DDS', exp: 'Data Distribution Service',
        say: 'OMG\'s data-centric pub/sub middleware standard.',
        is: 'Spec family: DDS 1.4 (DCPS API), DDSI-RTPS 2.5 (wire), DDS-XTypes 1.3 (type system), DDS-Security 1.2 (plugins). Implementations include Eclipse Cyclone DDS, eProsima Fast DDS, RTI Connext. ROS 2 is built on DDS via the rmw layer.', ref: 'P15.01' },
      { name: 'RTPS', exp: 'Real-Time Publish-Subscribe',
        say: 'The wire protocol behind DDS.',
        is: 'OMG DDSI-RTPS 2.5. Has four PIM modules: Structure (entities), Messages (header + 12 submessage types), Behavior (reliability state machine), Discovery (SPDP for participants, SEDP for endpoints). Default UDP port formula: 7400 + 250·domainId + offsets.', ref: 'P15.03' },
      { name: 'GUID', exp: 'Globally Unique Identifier (RTPS)',
        say: 'Identifies any RTPS entity.',
        is: '16 bytes total: 12-byte GuidPrefix (host + process + counter) plus 4-byte EntityId (kind + key). Allocated per-Participant, per-Writer, per-Reader. The GuidPrefix is shared across all entities in one Participant.', ref: 'P15.04' },
      { name: 'CDR', exp: 'Common Data Representation',
        say: 'How DDS serializes data.',
        is: 'OMG IDL-based serialization. Primitives in either little-endian (LE) or big-endian (BE), with a flag in the encapsulation header. Strings are length-prefixed and null-terminated. Used as the payload of every DATA submessage.', ref: 'P15.05' },
      { name: 'QoS', exp: 'Quality of Service',
        say: 'Per-topic delivery semantics in DDS.',
        is: '22 standard policies, including RELIABILITY {BEST_EFFORT, RELIABLE}, DURABILITY {VOLATILE, TRANSIENT_LOCAL, TRANSIENT, PERSISTENT}, HISTORY {KEEP_LAST, KEEP_ALL}, DEADLINE, LIVELINESS, OWNERSHIP. A Reader and Writer must have compatible (offered ⊇ requested) QoS to communicate.', ref: 'P15.13' },
      { name: 'PREEMPT_RT', exp: 'Real-Time Preemption Patch',
        say: 'Linux made deterministic.',
        is: 'Mainlined into Linux 6.13 as CONFIG_PREEMPT_RT. Converts most kernel sections to be preemptible. Mutexes use priority inheritance. Combined with SCHED_FIFO/SCHED_DEADLINE on isolated CPUs, gets you sub-100µs worst-case scheduling latency on commodity hardware.', ref: 'P15.21' }
    ]
  }
];
