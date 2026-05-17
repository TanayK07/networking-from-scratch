/*
 * eth_parse — demonstrate Ethernet II frame parsing and building.
 *
 * Build:  make
 * Run:    ./eth_parse
 */
#include "ethernet.h"

#include <stdio.h>
#include <string.h>

static void demo_parse(void) {
    /* A fabricated 60-byte Ethernet frame carrying an IPv4 payload. */
    uint8_t frame[60];
    memset(frame, 0, sizeof(frame));

    /* dst = ff:ff:ff:ff:ff:ff  (broadcast) */
    memset(frame, 0xFF, 6);
    /* src = 00:11:22:33:44:55 */
    uint8_t src[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    memcpy(frame + 6, src, 6);
    /* ethertype = 0x0800 (IPv4) in network order */
    frame[12] = 0x08;
    frame[13] = 0x00;

    struct nfs_eth_hdr hdr;
    const uint8_t *payload;
    size_t payload_len;

    if (nfs_eth_parse(frame, sizeof(frame), &hdr, &payload, &payload_len) == 0) {
        char fmt[128];
        nfs_eth_format(&hdr, fmt, sizeof(fmt));
        printf("Parsed:  %s\n", fmt);
        printf("Payload: %zu bytes\n", payload_len);
    } else {
        fprintf(stderr, "Parse failed!\n");
    }
}

static void demo_build(void) {
    uint8_t dst[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    uint8_t src[6] = {0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x02};
    uint8_t payload[] = "Hello Ethernet!";

    uint8_t out[1518];
    int len =
        nfs_eth_build(dst, src, NFS_ETHERTYPE_IPV4, payload, sizeof(payload), out, sizeof(out));
    if (len > 0) {
        printf("Built frame: %d bytes\n", len);

        /* Parse it back. */
        struct nfs_eth_hdr hdr;
        const uint8_t *pay;
        size_t pay_len;
        if (nfs_eth_parse(out, (size_t)len, &hdr, &pay, &pay_len) == 0) {
            char fmt[128];
            nfs_eth_format(&hdr, fmt, sizeof(fmt));
            printf("Re-parsed: %s\n", fmt);
        }
    }
}

int main(void) {
    printf("=== Ethernet II Frame Layout Demo ===\n\n");
    demo_parse();
    printf("\n");
    demo_build();
    return 0;
}
