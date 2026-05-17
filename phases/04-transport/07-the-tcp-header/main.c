/* CLI: parse or build a TCP header from hex / arguments.
 *
 * Parse mode:
 *   $ ./tcp_parse "0050 1F90 00000001 00000000 5002 FFFF 0000 0000"
 *
 * Build mode:
 *   $ ./tcp_parse --build 80 8080 1000 0 SYN 65535
 */

#include "tcp_hdr.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Hex helpers                                                        */
/* ------------------------------------------------------------------ */

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
        if (hi < 0 || !*s)
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

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++)
        printf("%02x", data[i]);
}

/* ------------------------------------------------------------------ */
/*  Parse flags from string like "SYN,ACK"                             */
/* ------------------------------------------------------------------ */

static uint16_t parse_flags_str(const char *s) {
    uint16_t flags = 0;
    if (strstr(s, "FIN"))
        flags |= NFS_TCP_FIN;
    if (strstr(s, "SYN"))
        flags |= NFS_TCP_SYN;
    if (strstr(s, "RST"))
        flags |= NFS_TCP_RST;
    if (strstr(s, "PSH"))
        flags |= NFS_TCP_PSH;
    if (strstr(s, "ACK"))
        flags |= NFS_TCP_ACK;
    if (strstr(s, "URG"))
        flags |= NFS_TCP_URG;
    if (strstr(s, "ECE"))
        flags |= NFS_TCP_ECE;
    if (strstr(s, "CWR"))
        flags |= NFS_TCP_CWR;
    return flags;
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    if (argc == 2) {
        /* Parse mode. */
        uint8_t buf[65536];
        size_t len = 0;
        if (parse_hex(argv[1], buf, sizeof(buf), &len) < 0) {
            fprintf(stderr, "error: invalid hex string\n");
            return 1;
        }

        struct nfs_tcp_hdr hdr;
        if (nfs_tcp_parse(buf, len, &hdr) < 0) {
            fprintf(stderr, "error: failed to parse TCP header (%zu bytes)\n", len);
            return 1;
        }

        char fmt[256];
        nfs_tcp_format(&hdr, fmt, sizeof(fmt));
        printf("TCP header:\n");
        printf("  %s\n", fmt);
        printf("  Data offset:  %u (header = %u bytes)\n", nfs_tcp_data_offset(&hdr),
               nfs_tcp_data_offset(&hdr) * 4);
        printf("  Checksum:     0x%04x\n", hdr.checksum);
        printf("  Urgent:       %u\n", hdr.urgent);

        size_t hdr_len = (size_t)(nfs_tcp_data_offset(&hdr) * 4);
        if (len > hdr_len) {
            printf("  Payload (%zu bytes): ", len - hdr_len);
            print_hex(buf + hdr_len, len - hdr_len);
            printf("\n");
        }

        return 0;
    }

    if (argc == 8 && strcmp(argv[1], "--build") == 0) {
        /* Build mode: --build src_port dst_port seq ack flags window */
        uint16_t sp = (uint16_t)strtoul(argv[2], NULL, 10);
        uint16_t dp = (uint16_t)strtoul(argv[3], NULL, 10);
        uint32_t seq = (uint32_t)strtoul(argv[4], NULL, 10);
        uint32_t ack_num = (uint32_t)strtoul(argv[5], NULL, 10);
        uint16_t flags = parse_flags_str(argv[6]);
        uint16_t window = (uint16_t)strtoul(argv[7], NULL, 10);

        uint8_t buf[64];
        int n = nfs_tcp_build(sp, dp, seq, ack_num, flags, window, buf, sizeof(buf));
        if (n < 0) {
            fprintf(stderr, "error: failed to build TCP header\n");
            return 1;
        }

        printf("Built %d-byte TCP header:\n  Hex: ", n);
        print_hex(buf, (size_t)n);
        printf("\n");

        /* Parse back for display. */
        struct nfs_tcp_hdr hdr;
        if (nfs_tcp_parse(buf, (size_t)n, &hdr) == 0) {
            char fmt[256];
            nfs_tcp_format(&hdr, fmt, sizeof(fmt));
            printf("  %s\n", fmt);
        }

        return 0;
    }

    fprintf(stderr, "usage: %s <hex>\n", argv[0]);
    fprintf(stderr, "       %s --build <src> <dst> <seq> <ack> <flags> <win>\n", argv[0]);
    return 1;
}
