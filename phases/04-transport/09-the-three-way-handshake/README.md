# 09. The three-way handshake

> By the end of this lesson, you will have implemented the TCP three-way handshake from raw bytes -- building SYN, SYN-ACK, and ACK segments, tracking sequence numbers through a state machine, and verifying the invariants that make reliable connections possible.

## 1. Problem

Every TCP connection begins with a three-way handshake. When you type `curl https://example.com`, the kernel exchanges three segments before a single byte of HTTP data flows. If any step is wrong -- a bad sequence number, a missing flag bit, an incorrect acknowledgement -- the connection silently fails or, worse, succeeds with corrupted state that manifests as data loss minutes later.

Understanding the handshake matters because:

- **Debugging:** `ss -tn state syn-sent` shows connections stuck in the handshake. Without knowing what each state means, you cannot diagnose whether the problem is a firewall, a full SYN backlog, or a misconfigured MSS.
- **Security:** SYN floods exploit the server's obligation to allocate state after step 2. SYN cookies (RFC 4987) encode state into the ISN to avoid allocation. You cannot understand the defense without understanding the attack surface.
- **Performance:** TCP Fast Open (RFC 7413) piggybacks data on the SYN to save one RTT. Knowing the baseline handshake is prerequisite to understanding the optimization.

This lesson builds the handshake from scratch: no sockets, no kernel -- just byte buffers and a state machine.

## 2. Theory

### The exchange

Two hosts need to agree on initial sequence numbers before they can reliably track bytes. The three-way handshake accomplishes this in three segments:

```
    Client                                Server
      |                                      |
      |  1. SYN       seq=x                  |
      |------------------------------------->|
      |                                      |
      |  2. SYN-ACK   seq=y, ack=x+1         |
      |<-------------------------------------|
      |                                      |
      |  3. ACK       seq=x+1, ack=y+1       |
      |------------------------------------->|
      |                                      |
      |       Connection ESTABLISHED          |
```

**Step 1 -- SYN:** The client picks an Initial Sequence Number (ISN) `x` and sends a segment with the SYN flag set. This means "I want to start a connection, and my byte stream will begin at sequence number `x`."

**Step 2 -- SYN-ACK:** The server picks its own ISN `y` and acknowledges the client's SYN by setting `ack = x + 1`. The "+1" is because the SYN itself consumes one sequence number (even though it carries no data). Both SYN and ACK flags are set.

**Step 3 -- ACK:** The client acknowledges the server's SYN with `ack = y + 1`. After this segment, both sides are in the ESTABLISHED state.

### Why three, not two?

A two-way handshake (SYN, then ACK) would leave the server unable to verify that the client received its ISN. Old duplicate SYNs from previous connections could trick the server into establishing a ghost connection. The third segment proves the client is alive and aware of the server's ISN.

### Initial Sequence Numbers (ISNs)

RFC 793 (the original TCP spec) proposed a clock-based ISN: increment a 32-bit counter every 4 microseconds. This was predictable, enabling blind injection attacks (the Mitnick attack, 1994).

RFC 6528 replaced this with a cryptographic approach:

```
ISN = F(local_ip, local_port, remote_ip, remote_port, secret_key)
```

where `F` is a PRF (e.g., MD5, SipHash). This makes ISNs unpredictable to off-path attackers while remaining deterministic per 4-tuple (so retransmitted SYNs get the same ISN).

Our implementation uses `time + rand()` for simplicity. Production stacks use a keyed hash.

### The Transmission Control Block (TCB)

Each connection requires per-connection state, stored in a structure the RFCs call the TCB:

- Local and remote IP/port (the 4-tuple)
- Send sequence variables: SND.UNA, SND.NXT, SND.WND, ISS
- Receive sequence variables: RCV.NXT, RCV.WND, IRS
- Connection state (CLOSED, SYN_SENT, ESTABLISHED, etc.)

Our `nfs_tcp_handshake` struct is a minimal TCB that tracks just enough for the handshake.

## 3. Math / Spec

### TCP header format (RFC 9293, Section 3.1)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Source Port          |       Destination Port        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Sequence Number                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Acknowledgment Number                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Data |       |C|E|U|A|P|R|S|F|                               |
| Offset| Rsrvd |W|C|R|C|S|S|Y|I|            Window             |
|       |       |R|E|G|K|H|T|N|N|                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Checksum            |         Urgent Pointer        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

The minimum header is 20 bytes (Data Offset = 5, meaning 5 x 32-bit words). Options extend it up to 60 bytes.

### Sequence number arithmetic

Sequence numbers are 32-bit unsigned integers that wrap around. Comparisons use modular arithmetic (RFC 9293, Section 3.4):

```
SEG.ACK = SND.NXT        (the ACK acknowledges our SYN)
SND.UNA < SEG.ACK <= SND.NXT  (valid ACK range)
```

In the handshake specifically:
- SYN consumes 1 sequence number: if ISN = x, the next data byte is x + 1.
- FIN also consumes 1 sequence number (relevant for connection teardown).
- Pure ACKs consume 0 sequence numbers.

### State machine (excerpt from RFC 9293, Figure 6)

```
                  active OPEN: send SYN
  CLOSED ---------------------------------> SYN_SENT
                                                |
                  recv SYN-ACK, send ACK        |
  SYN_SENT ------------------------------------> ESTABLISHED
                                                     ^
                  recv SYN, send SYN-ACK             |
  CLOSED ---------> LISTEN ---------> SYN_RECEIVED --+
                  (passive OPEN)    recv ACK of SYN
```

Our implementation covers the active open path (client) and the passive responder path (server).

### TCP checksum (pseudo-header)

The TCP checksum covers a 12-byte pseudo-header prepended to the segment:

```
+--------+--------+--------+--------+
|           Source Address           |   4 bytes
+--------+--------+--------+--------+
|        Destination Address         |   4 bytes
+--------+--------+--------+--------+
|  zero  | Proto  |    TCP Length    |   4 bytes
+--------+--------+--------+--------+
```

Proto = 6 (IPPROTO_TCP). The checksum algorithm is the standard Internet checksum (RFC 1071) -- the same one's-complement sum used for IP headers.

## 4. Code

The implementation is split across three files:

- [`tcp.h`](tcp.h) -- the packed header struct, flag constants, state enum, handshake context struct, and function declarations.
- [`tcp.c`](tcp.c) -- all the logic: parsing, building SYN/SYN-ACK/ACK, ISN generation, and the pseudo-header checksum.
- [`main.c`](main.c) -- a CLI demo that runs a complete handshake between a "client" and "server" in memory.

### Key design decisions

**Parsed headers are in host byte order.** `nfs_tcp_parse` converts all multi-byte fields from network order so callers can use plain arithmetic. The build functions do the reverse before serializing.

**The context tracks state.** Each `nfs_tcp_handshake` struct holds the local and remote ISNs, ports, and the current state. The build functions advance the state machine:

```c
nfs_tcp_build_syn(ctx, ...)      // CLOSED      -> SYN_SENT
nfs_tcp_build_synack(ctx, ...)   // (any)       -> SYN_RECEIVED
nfs_tcp_build_ack(ctx, ...)      // SYN_SENT    -> ESTABLISHED
```

**The checksum reuses common/c/checksum.c.** The `internet_checksum_partial` / `internet_checksum_fold` functions from P3.02 are composed to checksum the pseudo-header and TCP segment without copying into a contiguous buffer:

```c
uint32_t sum = 0;
sum = internet_checksum_partial(pseudo, 12, sum);
sum = internet_checksum_partial(tcp_buf, tcp_len, sum);
uint16_t checksum = internet_checksum_fold(sum);
```

### Building and running

```bash
make          # builds ./handshake
./handshake   # runs the simulation
make test     # compiles and runs the test suite
```

## 5. Tests

```bash
make test
```

The test suite ([`tests/test_tcp.c`](tests/test_tcp.c)) covers nine areas:

| Test | What it verifies |
|------|-----------------|
| `test_struct_size` | `sizeof(nfs_tcp_hdr) == 20` -- the struct matches the wire |
| `test_parse_known_syn` | Hand-crafted SYN bytes parse to expected field values |
| `test_full_handshake` | Complete client/server handshake, all seq/ack numbers correct |
| `test_syn_has_correct_flags` | SYN has SYN set, ACK/FIN/RST clear |
| `test_synack_ack_numbers` | `SYN-ACK.ack == SYN.seq + 1` |
| `test_final_ack_numbers` | `ACK.ack == SYN-ACK.seq + 1`, `ACK.seq == SYN.seq + 1` |
| `test_isn_randomness` | 100 generated ISNs are all unique |
| `test_reject_truncated` | Buffers shorter than 20 bytes are rejected |
| `test_checksum_pseudo` | Pseudo-header checksum validates to zero when filled in |

## 6. Exercises

See [`exercises.md`](exercises.md).
