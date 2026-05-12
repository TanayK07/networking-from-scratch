# Exercises for 03. Raw sockets on Linux (AF_PACKET)

## ★ Sniff and identify

Run the sniffer on your loopback interface:

```bash
sudo ./sniff lo
```

In another terminal, `ping -c 3 127.0.0.1`. You'll see frames appear. What EtherType do you see? Why is the destination MAC `00:00:00:00:00:00` on loopback instead of a real address?

## ★ Count by EtherType

Modify `main.c` to keep a running count of frames per EtherType. On `Ctrl-C`, print a summary table:

```
EtherType   Count
--------   -----
0x0800       142
0x0806         3
0x86dd        57
```

How does the IPv4/IPv6 ratio change when you browse the web vs. when the machine is idle?

## ★ SOCK_RAW vs. SOCK_DGRAM

Change the socket creation from `SOCK_RAW` to `SOCK_DGRAM` and run the sniffer again on a real interface. What changes in the output? The Ethernet header is stripped in `SOCK_DGRAM` mode (the kernel delivers a "cooked" frame). Verify that `nfs_parse_eth_frame()` returns -1 or garbled data. Why does the kernel offer this mode at all?

## ★★ Promiscuous mode

By default, the NIC only delivers frames addressed to its own MAC (plus broadcast/multicast). Add an `ioctl(SIOCGIFFLAGS)` / `ioctl(SIOCSIFFLAGS)` pair to enable `IFF_PROMISC` on the interface before sniffing, and disable it on shutdown. Verify with `ip link show` that the `PROMISC` flag appears while your tool is running.

Why do cloud providers (AWS, GCP) block promiscuous mode on virtual NICs? What breaks if you try?

## ★★ Filter by EtherType at the kernel

Instead of capturing `ETH_P_ALL` and filtering in userspace, pass a specific protocol to `nfs_create_raw_socket()` — for example `ETH_P_IP` (0x0800). Measure the CPU usage difference with `perf stat` when there is heavy traffic. Why is kernel-level filtering more efficient?

## ★★ Send a raw frame

Write a new CLI mode (`./sniff --send <interface>`) that constructs and sends a single raw Ethernet frame. Build the frame byte-by-byte:

1. Destination: `ff:ff:ff:ff:ff:ff` (broadcast)
2. Source: your interface's real MAC (get it with `ioctl(SIOCGIFHWADDR)`)
3. EtherType: `0x88B5` (IEEE Local Experimental)
4. Payload: "Hello from AF_PACKET!"

Use `nfs_send_raw_frame()` to send it. Verify with `tcpdump -i <iface> -xx ether proto 0x88b5` in another terminal.

## ★★★ BPF socket filter

Linux AF_PACKET supports attaching classic BPF filters via `setsockopt(SO_ATTACH_FILTER)`. Write a BPF program (using `struct sock_filter` instructions) that accepts only ARP frames (EtherType 0x0806) and attach it to your socket. Compare the performance against your userspace EtherType check under a flood of UDP traffic (`iperf3`).

Read `Documentation/networking/filter.txt` in the Linux kernel source for the BPF instruction set.

## ★★★ Ring buffer with TPACKET_V3

The basic `recvfrom()` loop copies every frame from kernel to user space. Linux offers a zero-copy alternative: memory-mapped ring buffers via `setsockopt(PACKET_RX_RING)` with `TPACKET_V3`.

Implement the ring buffer path:

1. Set up a ring with `TPACKET_V3` and `mmap()` the shared region.
2. Poll for new blocks with `poll()`.
3. Walk the block's frame descriptors and process each frame.

Measure throughput (frames/sec) on a busy interface and compare against your `recvfrom()` loop. Document the speedup and explain why it occurs (hint: system call overhead and memory copies).
