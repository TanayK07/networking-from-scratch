# Exercises -- TCP three-way handshake

Eight exercises, rated by difficulty. Each builds on the lesson implementation.

---

### 1. Trace a real handshake (★)

Capture a live TCP handshake with `tcpdump`:

```bash
sudo tcpdump -i eth0 -c 3 'tcp[tcpflags] & (tcp-syn|tcp-ack) != 0' -X
```

Open a connection to any server (`curl http://example.com`) in another terminal. From the hex dump:

1. Identify the SYN, SYN-ACK, and ACK packets.
2. Extract the ISN from each side.
3. Verify that `SYN-ACK.ack == SYN.seq + 1` and `ACK.ack == SYN-ACK.seq + 1`.

---

### 2. Parse a SYN from raw hex (★)

Given the following hex bytes (a real SYN captured from the wire):

```
c6 12 00 50 a3 4b 7c 01 00 00 00 00 50 02 ff ff 00 00 00 00
```

Decode every field by hand: source port, destination port, sequence number, acknowledgement number, data offset, flags, window size. Verify your answers match `nfs_tcp_parse` output.

---

### 3. Simultaneous open (★★)

RFC 9293 Section 3.5 describes the "simultaneous open" case: both sides send SYN at the same time, then each responds with SYN-ACK.

```
  A: SYN (seq=x)  →     ← SYN (seq=y)  :B
  A: SYN-ACK      →     ← SYN-ACK      :B
```

Extend the handshake code to handle this case. Add a function `nfs_tcp_handle_syn_while_syn_sent()` that processes an incoming SYN when the context is already in `SYN_SENT` state. Write a test that runs two contexts through simultaneous open to `ESTABLISHED`.

---

### 4. RST on unexpected segment (★★)

According to RFC 9293 Section 3.5.2, a host in `CLOSED` state that receives a SYN should either respond with SYN-ACK (if listening) or RST (if not).

Implement `nfs_tcp_build_rst(ctx, incoming_hdr, buf, len)` that generates a RST segment. The RST's sequence number should equal the incoming segment's acknowledgement number (or 0 if the ACK bit is not set). Write tests that verify:

- RST is sent for a SYN to a closed port.
- RST has the correct sequence number.
- RST has no payload.

---

### 5. TCP options: MSS (★★)

The SYN and SYN-ACK almost always carry TCP options, most importantly the Maximum Segment Size (MSS). The option format is:

```
Kind=2, Length=4, MSS value (16 bits, network order)
```

Extend `nfs_tcp_build_syn` to append an MSS option (e.g., 1460). Update the data offset from 5 to 6 (24 bytes). Update `nfs_tcp_parse` to extract the MSS option if present. Add a test that round-trips a SYN with MSS=1460.

---

### 6. Verify checksum against Wireshark (★★)

Capture a real TCP SYN with `tcpdump -w capture.pcap`. Open it in Wireshark and note the TCP checksum value and the "checksum status" (valid/invalid).

Now feed the same bytes into `nfs_tcp_checksum_pseudo` with the correct source and destination IPs from the IP header. Verify your computed checksum matches. If the capture was on loopback, explain why the checksum might be 0x0000 (hint: checksum offloading).

---

### 7. SYN flood detection (★★★)

A SYN flood sends thousands of SYN packets without completing the handshake, exhausting the server's TCB (Transmission Control Block) table.

Implement a simple connection table (`struct nfs_tcp_conn_table`) that tracks half-open connections. Add a 5-second timeout for connections stuck in `SYN_RECEIVED`. Write a test that:

1. Opens 1000 half-open connections.
2. Verifies the table reports them.
3. Advances a mock clock by 6 seconds.
4. Verifies all entries are expired.

Then research SYN cookies (RFC 4987) and explain in a comment how they eliminate per-connection state for half-open connections.

---

### 8. Full state machine (★★★)

The lesson covers only the three states for connection establishment. Implement the complete TCP state machine from RFC 9293 Figure 6, including:

- `FIN_WAIT_1`, `FIN_WAIT_2`, `CLOSING`, `TIME_WAIT`
- `CLOSE_WAIT`, `LAST_ACK`
- The `2*MSL` timeout in `TIME_WAIT`

Write a test that walks a connection from `CLOSED` through `ESTABLISHED`, then through graceful close (`FIN` exchange) all the way to `TIME_WAIT` and back to `CLOSED`. Verify every state transition.
