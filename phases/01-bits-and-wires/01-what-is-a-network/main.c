/* What is a Network — demo program.
 *
 * Demonstrates packet building, parsing, encapsulation/decapsulation,
 * and the Internet checksum.
 */

#include "network.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    if (len % 16 != 0)
        printf("\n");
}

int main(void) {
    printf("=== What is a Network ===\n\n");

    /* 1. Build a packet */
    const char *msg = "Hello, network!";
    uint8_t buf[256];
    int total = nfs_pkt_build(buf, sizeof(buf), NFS_PKT_VERSION, 3, 0x42, 0xDEADBEEF,
                              (const uint8_t *)msg, strlen(msg));
    if (total < 0) {
        fprintf(stderr, "Failed to build packet\n");
        return 1;
    }

    printf("1) Built packet (%d bytes):\n", total);
    print_hex(buf, (size_t)total);

    /* 2. Parse it back */
    struct nfs_pkt_header hdr;
    const uint8_t *payload;
    size_t payload_len;

    if (nfs_pkt_parse(buf, (size_t)total, &hdr, &payload, &payload_len) == 0) {
        printf("\n2) Parsed header:\n");
        printf("   Version: %u\n", (hdr.ver_type >> 4) & 0x0F);
        printf("   Type:    %u\n", hdr.ver_type & 0x0F);
        printf("   Flags:   0x%02x\n", hdr.flags);
        printf("   Length:  %u\n", ntohs(hdr.length));
        printf("   ID:      0x%08x\n", ntohl(hdr.id));
        printf("   Payload: \"%.*s\"\n", (int)payload_len, payload);
    }

    /* 3. Internet checksum */
    uint8_t rfc_data[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
    uint16_t cksum = nfs_inet_checksum(rfc_data, sizeof(rfc_data));
    printf("\n3) Internet checksum of RFC 1071 test vector: 0x%04x\n", cksum);

    /* Verify: checksum(data + checksum) == 0 */
    uint8_t verify[10];
    memcpy(verify, rfc_data, 8);
    verify[8] = (uint8_t)(cksum >> 8);
    verify[9] = (uint8_t)(cksum & 0xFF);
    uint16_t check = nfs_inet_checksum(verify, 10);
    printf("   Self-verify (should be 0x0000): 0x%04x\n", check);

    /* 4. Encapsulate / decapsulate */
    uint8_t enc_buf[256];
    const uint8_t inner[] = "inner data";
    int enc_len = nfs_encapsulate(enc_buf, sizeof(enc_buf), 5, 0x01, 42, inner, sizeof(inner) - 1);
    printf("\n4) Encapsulated packet (%d bytes):\n", enc_len);
    print_hex(enc_buf, (size_t)enc_len);

    const uint8_t *dec_payload;
    size_t dec_len;
    struct nfs_pkt_header dec_hdr;
    if (nfs_decapsulate(enc_buf, (size_t)enc_len, &dec_hdr, &dec_payload, &dec_len) == 0) {
        printf("   Decapsulated payload: \"%.*s\"\n", (int)dec_len, dec_payload);
    }

    return 0;
}
