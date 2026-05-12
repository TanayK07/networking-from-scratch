# 03. Raw sockets on Linux (AF_PACKET)

> One-sentence promise: by the end of this lesson, you will have a working packet sniffer that captures raw Ethernet frames off a real NIC using Linux's AF_PACKET socket interface — no libpcap, no shortcuts.

## 1. Problem

Every network tool you've ever used — `tcpdump`, Wireshark, `arping`, `nmap` — needs to see raw Ethernet frames exactly as they appear on the wire. The normal socket API (`AF_INET`) only gives you payload after the kernel has stripped every header below L4. You never see the MAC addresses, you never see the EtherType, and you certainly can't forge your own L2 frames.

Linux solves this with `AF_PACKET` sockets. They bypass the entire TCP/IP stack and give you direct access to the link layer. But they come with sharp edges: you need root (or `CAP_NET_RAW`), you must handle raw byte buffers with packed structs, and you're responsible for parsing every header yourself.

This lesson makes you build that from scratch.

## 2. Theory

### The AF_PACKET interface

When you call `socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))`, the kernel creates a packet socket that taps into the network driver's receive path. Every frame the NIC delivers — before any protocol processing — gets copied into your socket's receive buffer.

```
  User space                    Kernel
  +-------------------+         +---------------------------+
  | socket(AF_PACKET, |-------->| packet socket             |
  |   SOCK_RAW,       |         |   protocol = ETH_P_ALL    |
  |   ETH_P_ALL)      |         +---------------------------+
  |                   |                    |
  | bind(sockfd,      |-------->| bind to ifindex            |
  |   &sockaddr_ll)   |         | (filter by interface)      |
  |                   |                    |
  | recvfrom(sockfd,  |<--------| raw L2 frame              |<--- NIC
  |   buf, len)       |         | (complete Ethernet header) |
  |                   |                    |
  | sendto(sockfd,    |-------->| inject frame              |---> NIC
  |   frame, len,     |         | (bypass TCP/IP stack)      |
  |   &sockaddr_ll)   |         +---------------------------+
  +-------------------+
```

### SOCK_RAW vs. SOCK_DGRAM

`AF_PACKET` offers two socket types:

| Type        | You receive                         | You send                     |
|-------------|-------------------------------------|------------------------------|
| `SOCK_RAW`  | Complete L2 frame (Ethernet header + payload) | Must build entire frame |
| `SOCK_DGRAM`| Payload only (Ethernet header stripped) | Kernel adds L2 header    |

We use `SOCK_RAW` because we want to see — and build — every byte.

### The sockaddr_ll structure

To bind a packet socket to a specific interface, you fill a `struct sockaddr_ll`:

```c
struct sockaddr_ll {
    unsigned short sll_family;    /* AF_PACKET              */
    unsigned short sll_protocol;  /* ETH_P_ALL, in net order */
    int            sll_ifindex;   /* interface index         */
    unsigned short sll_hatype;    /* ARP hardware type       */
    unsigned char  sll_pkttype;   /* packet type             */
    unsigned char  sll_halen;     /* address length          */
    unsigned char  sll_addr[8];   /* physical-layer address  */
};
```

The critical field is `sll_ifindex`. You get it from `if_nametoindex("eth0")`. Without binding, the socket captures frames from all interfaces.

### Privileges

AF_PACKET requires `CAP_NET_RAW`. In practice this means running as root or using:

```bash
sudo setcap cap_net_raw+ep ./sniff
```

## 3. Math / Spec

### Ethernet II frame layout

Every Ethernet II frame on the wire has this structure (IEEE 802.3, RFC 894):

```
 Byte offset
 0                   6                   12     14
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  Destination MAC (6 bytes)  |   Source MAC (6 bytes)         |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | EtherType (2) |                                              |
 +-+-+-+-+-+-+-+-+          Payload (46 - 1500 bytes)           |
 |                                                              |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                         FCS (4 bytes)                        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Byte-level breakdown:

```
 Offset  Size  Field
 ------  ----  ----------------------------
  0       6    Destination MAC address
  6       6    Source MAC address
 12       2    EtherType (network byte order)
 14       var  Payload (46-1500 bytes)
 --       4    FCS (stripped by NIC, not visible to AF_PACKET)
```

**Total header size: 14 bytes.** This is the magic number. If a `recvfrom()` returns fewer than 14 bytes, you don't have a valid frame.

### Common EtherType values

| EtherType | Protocol     |
|-----------|-------------|
| `0x0800`  | IPv4        |
| `0x0806`  | ARP         |
| `0x86DD`  | IPv6        |
| `0x8100`  | 802.1Q VLAN |

All multi-byte fields are in **network byte order** (big-endian). Use `ntohs()` to convert the EtherType to host order for comparison.

### What the NIC strips

The 4-byte Frame Check Sequence (FCS / CRC-32) at the end of every Ethernet frame is verified and stripped by the NIC hardware before the frame reaches AF_PACKET. You will never see it in your buffer. The 8-byte preamble and Start Frame Delimiter (SFD) are also stripped at the physical layer.

## 4. Code

The implementation lives in `sniff.h`, `sniff.c`, and `main.c`.

### Wire-format struct

```c
struct nfs_eth_header {
    uint8_t  dst[6];       /* destination MAC address */
    uint8_t  src[6];       /* source MAC address      */
    uint16_t ethertype;    /* network byte order      */
} __attribute__((packed));
```

This is 14 bytes with no padding, guaranteed by `packed`. We overlay it directly on the receive buffer.

### Core functions

```c
/* Create an AF_PACKET socket. Protocol is in HOST byte order. */
int nfs_create_raw_socket(uint16_t protocol);

/* Bind to a named interface (e.g. "eth0"). */
int nfs_bind_to_interface(int sockfd, const char *ifname);

/* Parse raw bytes into a structured frame view. */
int nfs_parse_eth_frame(const uint8_t *buf, size_t len,
                        struct nfs_parsed_frame *out);

/* Format a 6-byte MAC as "xx:xx:xx:xx:xx:xx". */
char *nfs_format_mac(const uint8_t mac[6], char *buf, size_t buflen);

/* Send a complete raw Ethernet frame out an interface. */
ssize_t nfs_send_raw_frame(int sockfd, const char *ifname,
                           const uint8_t *frame, size_t frame_len);
```

### Main loop

```c
int sockfd = nfs_create_raw_socket(ETH_P_ALL);
nfs_bind_to_interface(sockfd, "eth0");

uint8_t buf[65536];
for (;;) {
    ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL);
    struct nfs_parsed_frame frame;
    if (nfs_parse_eth_frame(buf, (size_t)n, &frame) < 0)
        continue;
    /* frame.dst, frame.src, frame.ethertype, frame.payload are ready */
}
```

Run it:

```bash
make
sudo ./sniff eth0
```

## 5. Tests

The tests (`tests/test_sniff.c`) verify struct layout and parsing logic without needing root or a real network interface.

What we test:
- **Struct pinning:** `sizeof(struct nfs_eth_header) == 14`, field offsets at 0, 6, 12.
- **Known-frame parsing:** construct a byte array with a known Ethernet header, parse it, verify every field matches.
- **Edge cases:** buffer shorter than 14 bytes is rejected; exactly 14 bytes yields zero-length payload; NULL pointers are rejected.
- **format_mac:** verify output for all-zeros, broadcast, and normal MACs. Verify small-buffer rejection.
- **Property test:** fill a 64-byte buffer with a recognisable pattern, parse it, confirm the parser faithfully reproduces every byte.

```bash
make test
# runs tests/test_sniff without root
```

## 6. Exercises

See `exercises.md`. Highlights:

- ★ Sniff on loopback and identify frames from `ping 127.0.0.1`.
- ★ Compare `SOCK_RAW` vs. `SOCK_DGRAM` output and explain when you'd use each.
- ★★ Enable promiscuous mode with `ioctl(SIOCSIFFLAGS)`.
- ★★ Send a raw broadcast frame with a custom EtherType and capture it with `tcpdump`.
- ★★★ Attach a classic BPF filter via `SO_ATTACH_FILTER` to do kernel-level EtherType matching.
- ★★★ Implement zero-copy capture with `TPACKET_V3` memory-mapped ring buffers and benchmark against `recvfrom()`.
