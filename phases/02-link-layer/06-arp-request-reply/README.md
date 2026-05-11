# 06. ARP request and reply

> One-sentence promise: by the end of this lesson, you will have written an ARP responder that fools the host kernel into adding your fake MAC address to its ARP cache.

## 1. Problem

You opened a TUN device in lesson 5 and you can now send raw IP packets. The kernel won't talk to you. When something on the host network tries to reach the IP you assigned to your TAP interface, it first asks "who has this IP?" — and nobody answers, so the conversation never starts.

ARP is the answer. We're going to listen for ARP requests on a TAP device and respond with a MAC address of our choosing. After this lesson, running `arp -n` on your host will show our fake MAC bound to our TAP IP.

## 2. Theory

ARP — Address Resolution Protocol, RFC 826 — sits between L3 and L2. It answers "I have this IPv4 address; what MAC address do I send the frame to?"

The mechanic is dead simple:

1. The host wants to send a packet to `10.0.0.42`. It looks in its ARP cache. Not there.
2. It broadcasts an ARP **request** to MAC `FF:FF:FF:FF:FF:FF`: "who has 10.0.0.42, tell me at this MAC".
3. Whoever owns 10.0.0.42 sends an ARP **reply** unicast back: "10.0.0.42 is at this MAC".
4. The asker caches the answer for a few minutes.

We're step 3. We're going to claim ownership of an IP that the host has set up a route toward.

## 3. Math / Spec

The ARP packet — RFC 826, on top of an Ethernet II frame with EtherType `0x0806` — is:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Hardware Type         |         Protocol Type         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  HW Addr Len  |  Proto AdLen  |            Opcode             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Sender Hardware Address (6)                  |
+                                                               +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Sender Protocol Address (4)                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Target Hardware Address (6)                  |
+                                                               +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Target Protocol Address (4)                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

For our case:
- Hardware Type = 1 (Ethernet)
- Protocol Type = 0x0800 (IPv4)
- HW Addr Len = 6, Proto Addr Len = 4
- Opcode = 1 (request) or 2 (reply)

## 4. Code

The implementation lives in `arp.c`, `arp.h`, and `main.c`.

The control flow:

```c
int main(void) {
    char ifname[IFNAMSIZ] = "tap0";
    int tap_fd = tap_open(ifname);
    /* Configure tap0 with `ip addr add 10.0.0.1/24 dev tap0; ip link set tap0 up`
     * BEFORE running this program, then `arping 10.0.0.42` from another shell. */

    uint8_t our_mac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x42};
    uint32_t our_ip = inet_addr("10.0.0.42");

    uint8_t buf[2048];
    for (;;) {
        ssize_t n = read(tap_fd, buf, sizeof buf);
        if (n < (ssize_t)(sizeof(struct eth_header) + sizeof(struct arp_packet)))
            continue;

        struct eth_header *eth = (struct eth_header *)buf;
        if (ntohs(eth->ethertype) != ETHERTYPE_ARP) continue;

        struct arp_packet *arp = (struct arp_packet *)(buf + sizeof *eth);
        if (ntohs(arp->opcode) != ARP_OP_REQUEST) continue;
        if (memcmp(arp->target_proto, &our_ip, 4) != 0) continue;

        /* It's for us. Build and send the reply. */
        send_arp_reply(tap_fd, eth, arp, our_mac, our_ip);
    }
}
```

`send_arp_reply` builds a fresh Ethernet frame with the request's sender as the destination, our MAC as the source, an ARP packet with opcode 2, and writes it back to the TAP fd. `write(tap_fd, ...)` injects it into the host's network stack as if it had arrived on a real wire.

What you'll see when this works:

```
$ ip neigh show dev tap0
10.0.0.42 lladdr 02:00:00:00:00:42 REACHABLE
```

(Tested on Linux 6.8 with Ubuntu 24.04. On older kernels it works too — ARP is a 1980s protocol that hasn't changed.)

## 5. Tests

Two test modes:

1. **Unit tests** (`tests/test_arp.c`) — pin the byte layout: build a request packet from a known input and assert the reply matches a hand-computed expected output.
2. **Integration test** (`tests/test_arp_live.py`) — opens a TAP device, runs the ARP responder in a subprocess, fires off `arping` from Python's `scapy`, and asserts the kernel's ARP cache picks up our entry. Requires root, marked `@pytest.mark.requires_root`.

## 6. Exercises

See `exercises.md`. Highlights:

- ★ Run `tcpdump -i tap0 -n arp` while the responder is live; identify the request and reply.
- ★★ Add gratuitous ARP: send an unsolicited reply when the responder starts, so the host learns our MAC without being asked.
- ★★★ Implement an ARP cache with TTL eviction. Why is the standard 60-second timeout a bad fit for fast-moving environments? (Hint: Kubernetes pod churn.)
