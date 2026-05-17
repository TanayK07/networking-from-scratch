/* IPv6 Header Demo (RFC 8200)
 *
 * Builds an IPv6 header, parses it back, and prints all fields.
 */

#include "ipv6.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== IPv6 Header Demo (RFC 8200) ===\n\n");

    /* Source: 2001:0db8:0000:0000:0000:0000:0000:0001 */
    uint8_t src[16];
    nfs_ipv6_addr_parse("2001:0db8:0000:0000:0000:0000:0000:0001", src);

    /* Destination: 2001:0db8:0000:0000:0000:0000:0000:0002 */
    uint8_t dst[16];
    nfs_ipv6_addr_parse("2001:0db8:0000:0000:0000:0000:0000:0002", dst);

    /* Build an IPv6 header:
     *   Traffic Class = 0x2E (DSCP=EF, ECN=2)
     *   Flow Label    = 0x12345
     *   Next Header   = 6 (TCP)
     *   Hop Limit     = 64
     *   Payload Len   = 1024
     */
    uint8_t wire[40];
    int rc = nfs_ipv6_build(0x2E, 0x12345, 6, 64, src, dst, 1024, wire, sizeof(wire));
    if (rc < 0) {
        fprintf(stderr, "Build failed!\n");
        return 1;
    }
    printf("Built %d-byte IPv6 header\n\n", rc);

    /* Parse it back. */
    struct nfs_ipv6_hdr hdr;
    rc = nfs_ipv6_parse(wire, (size_t)rc, &hdr);
    if (rc < 0) {
        fprintf(stderr, "Parse failed!\n");
        return 1;
    }

    /* Print fields. */
    printf("Version:        %u\n", nfs_ipv6_version(&hdr));
    printf("Traffic Class:  0x%02x\n", nfs_ipv6_traffic_class(&hdr));
    printf("Flow Label:     0x%05x\n", nfs_ipv6_flow_label(&hdr));
    printf("Payload Length: %u\n", ntohs(hdr.payload_len));
    printf("Next Header:    %u (%s)\n", hdr.next_hdr, nfs_ipv6_next_hdr_name(hdr.next_hdr));
    printf("Hop Limit:      %u\n", hdr.hop_limit);

    char src_str[40], dst_str[40];
    nfs_ipv6_addr_format(hdr.src, src_str, sizeof(src_str));
    nfs_ipv6_addr_format(hdr.dst, dst_str, sizeof(dst_str));
    printf("Source:         %s\n", src_str);
    printf("Destination:    %s\n", dst_str);

    return 0;
}
