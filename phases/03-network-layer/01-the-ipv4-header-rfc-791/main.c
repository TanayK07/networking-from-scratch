/* CLI: take a hex string of an IPv4 header, parse and pretty-print all fields.
 *
 *   $ ./ipv4_parse 4500005400004000400100000a0000010a000002
 *
 * Expected output:
 *   IPv4 Header
 *   +-----------+------+------+-----+--------------+
 *   | Version   | 4    | IHL  | 5   | Header: 20 B |
 *   | DSCP      | 0    | ECN  | 0   |              |
 *   | Total Len | 84   | ID   | 0   |              |
 *   | Flags     | 0x2  | DF=1 MF=0  | Frag Off: 0  |
 *   | TTL       | 64   | Proto| ICMP (1)           |
 *   | Checksum  | 0x0000 (computed: 0x26a7)         |
 *   | Src       | 10.0.0.1                          |
 *   | Dst       | 10.0.0.2                          |
 *   +-----------+------+------+-----+--------------+
 */

#include "ipv4.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hex2nyb(int c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static int parse_hex(const char *s, uint8_t *out, size_t cap, size_t *len) {
    size_t n = 0;
    while (*s) {
        if (isspace((unsigned char)*s)) {
            s++;
            continue;
        }
        int hi = hex2nyb(*s++);
        if (hi < 0)
            return -1;
        if (!*s)
            return -1;
        int lo = hex2nyb(*s++);
        if (lo < 0)
            return -1;
        if (n >= cap)
            return -1;
        out[n++] = (uint8_t)((hi << 4) | lo);
    }
    *len = n;
    return 0;
}

static void print_header(size_t raw_len, const struct nfs_ipv4_hdr *h) {
    char src[16], dst[16];
    nfs_ipv4_format_addr(h->src_addr, src);
    nfs_ipv4_format_addr(h->dst_addr, dst);

    printf("IPv4 Header (%zu bytes on wire)\n", raw_len);
    printf("  Version ........ %u\n", h->version);
    printf("  IHL ............ %u (%u bytes)\n", h->ihl, h->ihl * 4);
    printf("  DSCP ........... %u\n", h->dscp);
    printf("  ECN ............ %u\n", h->ecn);
    printf("  Total Length ... %u\n", h->total_length);
    printf("  Identification . 0x%04x (%u)\n", h->identification, h->identification);
    printf("  Flags .......... 0x%x", h->flags);
    if (h->flags & NFS_IPV4_FLAG_DF)
        printf(" [DF]");
    if (h->flags & NFS_IPV4_FLAG_MF)
        printf(" [MF]");
    printf("\n");
    printf("  Frag Offset .... %u (byte offset: %u)\n", h->frag_offset, h->frag_offset * 8);
    printf("  TTL ............ %u\n", h->ttl);
    printf("  Protocol ....... %s (%u)\n", nfs_ipv4_protocol_name(h->protocol), h->protocol);
    printf("  Checksum ....... 0x%04x\n", h->checksum);
    printf("  Source ......... %s\n", src);
    printf("  Destination .... %s\n", dst);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <hex string of IPv4 header>\n", argv[0]);
        fprintf(stderr, "  example: %s 4500005400004000400100000a0000010a000002\n", argv[0]);
        return 1;
    }

    uint8_t buf[1500];
    size_t len = 0;
    if (parse_hex(argv[1], buf, sizeof(buf), &len) < 0) {
        fprintf(stderr, "error: invalid hex string\n");
        return 1;
    }

    struct nfs_ipv4_hdr hdr;
    int rc = nfs_ipv4_parse(buf, len, &hdr);

    switch (rc) {
    case NFS_IPV4_OK:
        print_header(len, &hdr);
        break;
    case NFS_IPV4_ERR_SHORT:
        fprintf(stderr, "error: buffer too short (%zu bytes, need >= 20)\n", len);
        return 1;
    case NFS_IPV4_ERR_VERSION:
        fprintf(stderr, "error: not an IPv4 header (version = %u)\n", (buf[0] >> 4) & 0x0F);
        return 1;
    case NFS_IPV4_ERR_IHL:
        fprintf(stderr, "error: IHL too small (%u, minimum 5)\n", buf[0] & 0x0F);
        return 1;
    case NFS_IPV4_ERR_CHECKSUM:
        fprintf(stderr, "error: header checksum mismatch\n");
        /* Still print the header for debugging. */
        hdr.version = (buf[0] >> 4) & 0x0F;
        print_header(len, &hdr);
        return 1;
    default:
        fprintf(stderr, "error: unknown error %d\n", rc);
        return 1;
    }

    return 0;
}
