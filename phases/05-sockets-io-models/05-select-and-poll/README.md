# 05. select and poll

## 1. Problem

In real networking systems, select and poll is a critical component that you encounter constantly. If you cannot implement it correctly from first principles, you will be at the mercy of library bugs, misconfigurations, and subtle protocol violations that are nearly impossible to debug without deep understanding.

The challenge is that select and poll involves precise binary layouts, strict protocol rules, and edge cases that only manifest under specific network conditions. Getting even one byte wrong means packets are silently dropped or connections mysteriously fail.

## 2. Theory

select and poll requires careful attention to binary protocol formats and state management. The implementation must handle network byte order (big-endian) correctly and validate all input before processing.

The typical data flow involves:
1. **Receiving** raw bytes from the network
2. **Parsing** the header fields according to the specification
3. **Validating** checksums, lengths, and state consistency
4. **Processing** the payload or updating internal state
5. **Constructing** response packets with correct headers
6. **Transmitting** the response back to the network

Edge cases to watch for: packets shorter than minimum header size, invalid field values, checksum mismatches, illegal state transitions, and buffer overflow from maliciously crafted packets.

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
 * select_and_poll.c -- select and poll
 * Compile: gcc -Wall -O2 -o select_and_poll select_and_poll.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

/* -- Data structures ------------------------------------------------ */

struct protocol_hdr {
    uint8_t  ver_type;    /* version (4 bits) | type (4 bits) */
    uint8_t  flags;
    uint16_t length;      /* total length including header */
    uint32_t id;
} __attribute__((packed));

#define HDR_SIZE  sizeof(struct protocol_hdr)
#define VERSION   1

/* -- Accessors ------------------------------------------------------ */

static inline uint8_t hdr_version(const struct protocol_hdr *h) {
    return (h->ver_type >> 4) & 0x0F;
}
static inline uint8_t hdr_type(const struct protocol_hdr *h) {
    return h->ver_type & 0x0F;
}

/* -- Checksum (one's complement) ------------------------------------ */

uint16_t checksum(const void *data, size_t len)
{
    const uint8_t *p = data;
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < len; i += 2)
        sum += (uint32_t)(p[i] << 8 | p[i + 1]);
    if (len & 1)
        sum += (uint32_t)(p[len - 1] << 8);
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/* -- Build ---------------------------------------------------------- */

size_t build_packet(uint8_t *buf, size_t max,
                    uint8_t type, uint8_t flags,
                    const uint8_t *payload, uint16_t plen, uint32_t id)
{
    size_t total = HDR_SIZE + plen;
    if (total > max) return 0;

    struct protocol_hdr *h = (struct protocol_hdr *)buf;
    h->ver_type = (VERSION << 4) | (type & 0x0F);
    h->flags    = flags;
    h->length   = htons((uint16_t)total);
    h->id       = htonl(id);

    if (payload && plen)
        memcpy(buf + HDR_SIZE, payload, plen);
    return total;
}

/* -- Parse ---------------------------------------------------------- */

int parse_packet(const uint8_t *buf, size_t len, struct protocol_hdr *out,
                 const uint8_t **payload, uint16_t *plen)
{
    if (len < HDR_SIZE) return -1;

    memcpy(out, buf, HDR_SIZE);
    out->length = ntohs(out->length);
    out->id     = ntohl(out->id);

    if (out->length > len) return -1;

    *payload = buf + HDR_SIZE;
    *plen    = out->length - HDR_SIZE;
    return 0;
}

/* -- Main ----------------------------------------------------------- */

int main(void)
{
    uint8_t pkt[1500];
    const char *msg = "Hello from select and poll";
    size_t len = build_packet(pkt, sizeof(pkt), 1, 0,
                              (const uint8_t *)msg, strlen(msg), 1);
    printf("Built %zu-byte packet\n", len);

    struct protocol_hdr hdr;
    const uint8_t *payload;
    uint16_t plen;
    if (parse_packet(pkt, len, &hdr, &payload, &plen) == 0) {
        printf("ver=%u type=%u flags=0x%02x len=%u id=%u\n",
               hdr_version(&hdr), hdr_type(&hdr), hdr.flags, hdr.length, hdr.id);
        printf("payload (%u bytes): %.*s\n", plen, plen, payload);
    }

    printf("checksum: 0x%04x\n", checksum(pkt, len));
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
    printf("All tests for select and poll passed.\n");
    return 0;
}
```

## 6. Exercises

1. **\u2605** Parse a hex dump of a real select and poll packet and identify every field manually.

2. **\u2605** Implement the basic parser and verify it produces byte-identical output to a reference implementation.

3. **\u2605\u2605** Add comprehensive input validation: reject packets with invalid field values and return appropriate error codes.

4. **\u2605\u2605** Handle all edge cases: minimum-size packets, maximum-size packets, optional fields, and malformed input.

5. **\u2605\u2605** Write a pcap analyzer that reads capture files and decodes select and poll packets with full field breakdown.

6. **\u2605\u2605\u2605** Implement the complete protocol state machine. Verify all transitions with a test harness.

7. **\u2605\u2605\u2605** Benchmark parsing throughput (packets/sec) and compare to theoretical line rate.

8. **\u2605\u2605\u2605** Test against real network traffic: capture live packets and verify your parser handles all observed variations.
