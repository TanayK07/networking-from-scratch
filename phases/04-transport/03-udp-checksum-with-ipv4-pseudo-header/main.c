/* CLI: compute or verify UDP checksum with IPv4 pseudo-header.
 *
 * Compute mode:
 *   $ ./udp_csum compute 192.168.1.1 10.0.0.1 "003500350010 0000 48656c6c6f"
 *   Checksum: 0xABCD
 *
 * Verify mode:
 *   $ ./udp_csum verify 192.168.1.1 10.0.0.1 "003500350010ABCD 48656c6c6f"
 *   Valid
 */

#include "udp_csum.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static int hex2nyb(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_hex(const char *s, uint8_t *out, size_t cap, size_t *len) {
    size_t n = 0;
    while (*s) {
        if (isspace((unsigned char)*s)) { s++; continue; }
        int hi = hex2nyb(*s++);
        if (hi < 0 || !*s) return -1;
        int lo = hex2nyb(*s++);
        if (lo < 0) return -1;
        if (n >= cap) return -1;
        out[n++] = (uint8_t)((hi << 4) | lo);
    }
    *len = n;
    return 0;
}

static int parse_ipv4(const char *s, uint32_t *out) {
    unsigned a, b, c, d;
    if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
        return -1;
    if (a > 255 || b > 255 || c > 255 || d > 255)
        return -1;
    /* Store in network byte order (big-endian). */
    uint8_t ip[4] = {(uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d};
    memcpy(out, ip, 4);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s compute|verify <src_ip> <dst_ip> <hex_udp_segment>\n", argv[0]);
        return 1;
    }

    const char *mode = argv[1];
    uint32_t src_ip, dst_ip;
    if (parse_ipv4(argv[2], &src_ip) < 0) {
        fprintf(stderr, "error: invalid source IP '%s'\n", argv[2]);
        return 1;
    }
    if (parse_ipv4(argv[3], &dst_ip) < 0) {
        fprintf(stderr, "error: invalid destination IP '%s'\n", argv[3]);
        return 1;
    }

    uint8_t segment[65536];
    size_t seg_len = 0;
    if (parse_hex(argv[4], segment, sizeof(segment), &seg_len) < 0) {
        fprintf(stderr, "error: invalid hex string\n");
        return 1;
    }

    if (strcmp(mode, "compute") == 0) {
        uint16_t cs = nfs_udp_checksum(src_ip, dst_ip, segment, seg_len);
        printf("Checksum: 0x%04X\n", cs);
        return 0;
    }

    if (strcmp(mode, "verify") == 0) {
        int valid = nfs_udp_verify_checksum(src_ip, dst_ip, segment, seg_len);
        if (valid) {
            printf("Valid\n");
            return 0;
        } else {
            printf("Invalid\n");
            return 1;
        }
    }

    fprintf(stderr, "error: unknown mode '%s' (use 'compute' or 'verify')\n", mode);
    return 1;
}
