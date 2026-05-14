/* Simplified ping demo -- build and inspect ICMP Echo Requests.
 *
 * This does NOT send real ICMP packets (that requires raw sockets
 * and root privileges). Instead, it constructs echo requests in
 * memory, parses them back, verifies the checksum, and displays
 * a hex dump -- demonstrating the ICMP wire format.
 *
 *   $ ./ping_demo
 *   === ICMP Ping Clone Demo ===
 *
 *   Built ICMP Echo Request:
 *     Type: 8 (Echo Request)
 *     Code: 0
 *     ID:   0x1234
 *     Seq:  1
 *     Checksum: 0x... (valid)
 *     Payload: 16 bytes
 *     Hex: 08 00 xx xx 12 34 00 01 ...
 */

#include "icmp.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Print a hex dump of `len` bytes. */
static void hex_dump(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", buf[i]);
        if (i + 1 < len)
            printf(" ");
    }
    printf("\n");
}

/* Build one echo request, parse it back, and display the result. */
static void demo_echo_request(uint16_t id, uint16_t seq, const uint8_t *payload,
                              size_t payload_len) {
    uint8_t pkt[128];
    size_t n = nfs_icmp_build_echo_request(id, seq, payload, payload_len, pkt, sizeof(pkt));
    if (n == 0) {
        fprintf(stderr, "build_echo_request failed\n");
        return;
    }

    struct nfs_icmp_hdr hdr;
    if (nfs_icmp_parse(pkt, n, &hdr) < 0) {
        fprintf(stderr, "parse failed\n");
        return;
    }

    int cs_ok = nfs_icmp_validate_checksum(pkt, n);

    printf("Built ICMP Echo Request:\n");
    printf("  Type: %u (%s)\n", hdr.type, nfs_icmp_type_name(hdr.type));
    printf("  Code: %u\n", hdr.code);
    printf("  ID:   0x%04x\n", hdr.id);
    printf("  Seq:  %u\n", hdr.seq);
    printf("  Checksum: 0x%04x (%s)\n", hdr.checksum, cs_ok == 0 ? "valid" : "INVALID");
    printf("  Payload: %zu bytes\n", payload_len);
    printf("  Hex: ");
    hex_dump(pkt, n);
}

int main(void) {
    printf("=== ICMP Ping Clone Demo ===\n\n");

    /* --- Demo 1: Echo request with 16-byte payload --- */

    uint8_t payload[16];
    for (int i = 0; i < 16; i++)
        payload[i] = (uint8_t)(0x41 + i); /* 'A', 'B', 'C', ... */

    demo_echo_request(0x1234, 1, payload, sizeof(payload));

    /* --- Demo 2: Sequence number incrementing --- */

    printf("\n--- Sequence number incrementing ---\n");
    for (uint16_t seq = 1; seq <= 4; seq++) {
        printf("\nSeq %u:\n", seq);
        demo_echo_request(0x5678, seq, payload, 4);
    }

    /* --- Demo 3: Echo request with no payload --- */

    printf("\n--- Empty payload ---\n");
    demo_echo_request(0xAAAA, 0, NULL, 0);

    /* --- Demo 4: Checksum corruption detection --- */

    printf("\n--- Checksum corruption detection ---\n");
    uint8_t pkt[64];
    size_t n = nfs_icmp_build_echo_request(0xBBBB, 42, payload, 8, pkt, sizeof(pkt));
    if (n > 0) {
        printf("Original checksum valid: %s\n",
               nfs_icmp_validate_checksum(pkt, n) == 0 ? "yes" : "no");

        /* Corrupt one byte of payload. */
        pkt[n - 1] ^= 0xFF;
        printf("After corruption:       %s\n",
               nfs_icmp_validate_checksum(pkt, n) == 0 ? "yes" : "no");
    }

    return 0;
}
