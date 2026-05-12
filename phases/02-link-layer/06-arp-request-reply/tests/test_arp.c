/* Byte-pinning unit tests for the ARP packet layout.
 *
 * Compile + run:
 *     make
 *     ./test_arp
 */

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../arp.h"

/* Sanity check: struct sizes match the wire. */
static void test_sizes(void) {
    assert(sizeof(struct eth_header) == 14);
    assert(sizeof(struct arp_packet) == 28);
    printf("ok struct sizes\n");
}

/* Build a known ARP request and verify byte layout. */
static void test_request_layout(void) {
    struct arp_packet arp = {
        .hw_type = htons(1),
        .proto_type = htons(0x0800),
        .hw_addr_len = 6,
        .proto_addr_len = 4,
        .opcode = htons(1),
        .sender_hw = {0xaa, 0xbb, 0xcc, 0x11, 0x22, 0x33},
        .sender_proto = {10, 0, 0, 1},
        .target_hw = {0, 0, 0, 0, 0, 0},
        .target_proto = {10, 0, 0, 42},
    };

    const uint8_t *p = (const uint8_t *)&arp;
    /* hw_type 0x0001 big-endian */
    assert(p[0] == 0x00 && p[1] == 0x01);
    /* proto_type 0x0800 big-endian */
    assert(p[2] == 0x08 && p[3] == 0x00);
    /* lengths */
    assert(p[4] == 6 && p[5] == 4);
    /* opcode 0x0001 big-endian */
    assert(p[6] == 0x00 && p[7] == 0x01);
    /* sender hw */
    assert(memcmp(p + 8, "\xaa\xbb\xcc\x11\x22\x33", 6) == 0);
    /* sender proto */
    assert(memcmp(p + 14, "\x0a\x00\x00\x01", 4) == 0);
    printf("ok request layout\n");
}

int main(void) {
    test_sizes();
    test_request_layout();
    printf("\nall ARP tests passed\n");
    return 0;
}
