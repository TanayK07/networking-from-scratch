# 40. Capstone: end-to-end TCP over TUN

## 1. Problem

This capstone brings together everything from the Transport: UDP, TCP, QUIC phase into a single working project. You will build a complete, functional implementation of end-to-end TCP over TUN that demonstrates mastery of all concepts covered so far.

Without integrating individual components into a complete system, you only have isolated pieces of knowledge. Real networking code must handle all edge cases, state transitions, and error conditions simultaneously. This is where theory meets practice.

## 2. Theory

The end-to-end TCP over TUN project integrates multiple components built in previous lessons. The architecture follows a layered design where each module handles one responsibility:

```
  +------------------------------+
  |      Application Logic       |
  +------------------------------+
  |     Protocol Processing      |
  +------------------------------+
  |    Packet I/O & Buffering    |
  +------------------------------+
  |   Network Device (TUN/TAP)   |
  +------------------------------+
```

Each layer communicates through well-defined interfaces. Error handling is critical: network operations can fail at any layer, and the system must degrade gracefully. The implementation should be structured for testability.

## 3. Math / Spec

The protocol defines specific algorithms and data formats that must be implemented exactly for interoperability:

- **Header format**: fixed and variable-length fields with specific byte ordering
- **Checksum**: error detection method (one's complement sum or CRC)
- **State transitions**: valid sequences of operations and responses
- **Timer values**: retransmission timeouts, keepalive intervals, expiration times

All multi-byte integer fields are in network byte order (big-endian) unless explicitly stated otherwise.

## 4. Code

```c
/*
 * capstone_endtoend_tcp_over_tun.c -- Capstone: Capstone: end-to-end TCP over TUN
 *
 * Architecture:
 *   - Main event loop with select/epoll
 *   - Protocol state machine
 *   - Packet parser and builder
 *   - Configuration and logging
 *
 * Compile: gcc -Wall -O2 -o capstone_endtoend_tcp_over_tun capstone_endtoend_tcp_over_tun.c -lpthread
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_PKT 65535
#define LOG(fmt, ...) fprintf(stderr, "[%s] " fmt "\n", __func__, ##__VA_ARGS__)

struct context {
    int      fd;
    uint8_t  rx_buf[MAX_PKT];
    int      running;
    uint64_t rx_count;
    uint64_t tx_count;
    uint64_t err_count;
};

static int process_packet(struct context *ctx, const uint8_t *pkt, size_t len)
{
    if (len < 4) {
        LOG("packet too short (%zu bytes)", len);
        ctx->err_count++;
        return -1;
    }

    /* Parse the first header fields */
    uint8_t version = (pkt[0] >> 4) & 0x0F;
    uint16_t total_len = (pkt[2] << 8) | pkt[3];
    LOG("rx: version=%u total_len=%u", version, total_len);
    ctx->rx_count++;

    /* Protocol-specific processing would go here.
     * Each previous lesson's code integrates as a module. */

    return 0;
}

static void event_loop(struct context *ctx)
{
    LOG("entering event loop (fd=%d)", ctx->fd);
    ctx->running = 1;

    while (ctx->running) {
        ssize_t n = read(ctx->fd, ctx->rx_buf, sizeof(ctx->rx_buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read");
            break;
        }
        if (n == 0) break;

        process_packet(ctx, ctx->rx_buf, (size_t)n);
    }

    LOG("exited: rx=%lu tx=%lu err=%lu", ctx->rx_count, ctx->tx_count, ctx->err_count);
}

int main(int argc, char *argv[])
{
    printf("Capstone: end-to-end TCP over TUN\n");
    printf("Run with a TUN/TAP device or pipe for full integration.\n");

    struct context ctx = { .fd = STDIN_FILENO };
    event_loop(&ctx);
    return 0;
}
```

## 5. Tests

```c
#include <assert.h>
#include <string.h>
#include <stdio.h>

void test_build_parse_roundtrip(void)
{
    uint8_t buf[256];
    const char *msg = "test";
    size_t len = build_packet(buf, sizeof(buf), 1, 0, (const uint8_t *)msg, 4, 99);
    assert(len > 0);

    struct protocol_hdr hdr;
    const uint8_t *payload;
    uint16_t plen;
    assert(parse_packet(buf, len, &hdr, &payload, &plen) == 0);
    assert(hdr_version(&hdr) == VERSION);
    assert(hdr_type(&hdr) == 1);
    assert(hdr.id == 99);
    assert(plen == 4);
    assert(memcmp(payload, "test", 4) == 0);
}

void test_reject_truncated(void)
{
    uint8_t buf[] = {0x10, 0x00};
    struct protocol_hdr hdr;
    const uint8_t *p;
    uint16_t plen;
    assert(parse_packet(buf, 2, &hdr, &p, &plen) == -1);
}

void test_checksum_verify(void)
{
    uint8_t data[] = {0x00, 0x01, 0x00, 0x02};
    uint16_t cs = checksum(data, 4);
    uint8_t with_cs[6];
    memcpy(with_cs, data, 4);
    with_cs[4] = cs >> 8;
    with_cs[5] = cs & 0xFF;
    assert(checksum(with_cs, 6) == 0);
}

void test_empty_payload(void)
{
    uint8_t buf[64];
    size_t len = build_packet(buf, sizeof(buf), 0, 0, NULL, 0, 0);
    assert(len == HDR_SIZE);
}

int main(void)
{
    test_build_parse_roundtrip();
    test_reject_truncated();
    test_checksum_verify();
    test_empty_payload();
    printf("All tests for Capstone: end-to-end TCP over TUN passed.\n");
    return 0;
}
```

## 6. Exercises

1. **★** Build the project skeleton with a proper Makefile. Verify it compiles and runs with empty input.

2. **★** Implement the packet parser for the primary protocol. Test with at least 3 known-good packet captures.

3. **★★** Add comprehensive error handling: every failure path should log a clear message and increment an error counter.

4. **★★** Implement the core protocol state machine with all required transitions. Draw the state diagram.

5. **★★** Write integration tests using real network tools (`ping`, `curl`, `dig`, `tcpdump`).

6. **★★★** Profile with `perf` and optimize the hot path. Target 100,000+ packets/sec on a single core.

7. **★★★** Fuzz with AFL++ or libFuzzer. Fix all crashes.

8. **★★★** Test interoperability with at least two existing implementations. Document any compatibility issues.
