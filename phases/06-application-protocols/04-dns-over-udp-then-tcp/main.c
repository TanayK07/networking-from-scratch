/* Demo driver for DNS over UDP then TCP lesson. */

#include "dns_transport.h"
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
    /* Create a fake DNS message (just header for demo) */
    uint8_t dns_msg[20];
    memset(dns_msg, 0, sizeof(dns_msg));
    dns_msg[0] = 0x12;
    dns_msg[1] = 0x34; /* ID */
    dns_msg[2] = 0x01;
    dns_msg[3] = 0x00; /* Flags: RD=1 */
    dns_msg[4] = 0x00;
    dns_msg[5] = 0x01; /* QDCOUNT=1 */

    printf("=== Original DNS message (%zu bytes) ===\n", sizeof(dns_msg));
    print_hex(dns_msg, sizeof(dns_msg));

    /* TCP framing */
    printf("\n=== TCP Framing ===\n");
    uint8_t framed[256];
    int flen = nfs_dns_tcp_frame(dns_msg, (uint16_t)sizeof(dns_msg), framed, sizeof(framed));
    printf("Framed (%d bytes):\n", flen);
    print_hex(framed, (size_t)flen);

    /* TCP unframing */
    const uint8_t *unframed;
    uint16_t ulen;
    int consumed = nfs_dns_tcp_unframe(framed, (size_t)flen, &unframed, &ulen);
    printf("Unframed: consumed=%d msg_len=%u\n", consumed, ulen);

    /* Truncation detection */
    printf("\n=== Truncation ===\n");
    printf("Truncated before: %d\n", nfs_dns_is_truncated(dns_msg, sizeof(dns_msg)));
    nfs_dns_set_truncated(dns_msg, sizeof(dns_msg));
    printf("Truncated after set: %d\n", nfs_dns_is_truncated(dns_msg, sizeof(dns_msg)));
    nfs_dns_clear_truncated(dns_msg, sizeof(dns_msg));
    printf("Truncated after clear: %d\n", nfs_dns_is_truncated(dns_msg, sizeof(dns_msg)));

    /* Size checks */
    printf("\n=== Size Validation ===\n");
    printf("100 bytes fits UDP: %d\n", nfs_dns_fits_udp(100));
    printf("600 bytes fits UDP: %d\n", nfs_dns_fits_udp(600));
    printf("600 bytes fits TCP: %d\n", nfs_dns_fits_tcp(600));

    return 0;
}
