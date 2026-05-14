/* CLI: parse or build a UDP datagram from hex / arguments.
 *
 * Parse mode (default):
 *   $ ./udp_parse "003500350013a1b2 48656c6c6f"
 *   UDP datagram:
 *     Source port:      53
 *     Destination port: 53
 *     Length:           19
 *     Checksum:         0xa1b2
 *     Payload (11 bytes): 48656c6c6f
 *
 * Build mode:
 *   $ ./udp_parse --build 12345 53 "hello"
 *   Built 13-byte UDP datagram:
 *     Source port:      12345
 *     Destination port: 53
 *     Length:           13
 *     Checksum:         0x0000 (not computed)
 *     Hex: 30390035000d0000 68656c6c6f
 */

#include "udp.h"

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

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++)
        printf("%02x", data[i]);
}

/* ------------------------------------------------------------------ */
/*  Parse mode                                                         */
/* ------------------------------------------------------------------ */

static int do_parse(const char *hex_str) {
    uint8_t buf[65536];
    size_t len = 0;

    if (parse_hex(hex_str, buf, sizeof(buf), &len) < 0) {
        fprintf(stderr, "error: invalid hex string\n");
        return 1;
    }

    struct nfs_udp_hdr hdr;
    if (nfs_udp_parse(buf, len, &hdr) < 0) {
        fprintf(stderr, "error: failed to parse UDP datagram (%zu bytes)\n", len);
        return 1;
    }

    size_t payload_len = 0;
    if (hdr.length > NFS_UDP_HDR_LEN)
        payload_len = hdr.length - NFS_UDP_HDR_LEN;

    printf("UDP datagram:\n");
    printf("  Source port:      %u\n", hdr.src_port);
    printf("  Destination port: %u\n", hdr.dst_port);
    printf("  Length:           %u\n", hdr.length);
    if (hdr.checksum == 0x0000)
        printf("  Checksum:         0x%04x (not computed)\n", hdr.checksum);
    else
        printf("  Checksum:         0x%04x\n", hdr.checksum);

    if (payload_len > 0 && len >= NFS_UDP_HDR_LEN + payload_len) {
        printf("  Payload (%zu bytes): ", payload_len);
        print_hex(buf + NFS_UDP_HDR_LEN, payload_len);
        printf("\n");
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Build mode                                                         */
/* ------------------------------------------------------------------ */

static int do_build(uint16_t src_port, uint16_t dst_port, const char *payload_str) {
    size_t payload_len = strlen(payload_str);
    const uint8_t *payload = (const uint8_t *)payload_str;

    struct nfs_udp_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.src_port = src_port;
    hdr.dst_port = dst_port;
    hdr.length = (uint16_t)(NFS_UDP_HDR_LEN + payload_len);
    hdr.checksum = 0; /* not computed */

    uint8_t buf[65536];
    size_t n = nfs_udp_build(&hdr, payload, payload_len, buf, sizeof(buf));
    if (n == 0) {
        fprintf(stderr, "error: failed to build UDP datagram\n");
        return 1;
    }

    printf("Built %zu-byte UDP datagram:\n", n);
    printf("  Source port:      %u\n", hdr.src_port);
    printf("  Destination port: %u\n", hdr.dst_port);
    printf("  Length:           %u\n", hdr.length);
    printf("  Checksum:         0x%04x (not computed)\n", hdr.checksum);
    printf("  Hex: ");
    print_hex(buf, n);
    printf("\n");

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    if (argc == 2) {
        /* Parse mode: ./udp_parse <hex> */
        return do_parse(argv[1]);
    }

    if (argc == 5 && strcmp(argv[1], "--build") == 0) {
        /* Build mode: ./udp_parse --build <src_port> <dst_port> <payload> */
        unsigned long sp = strtoul(argv[2], NULL, 10);
        unsigned long dp = strtoul(argv[3], NULL, 10);
        if (sp > 65535 || dp > 65535) {
            fprintf(stderr, "error: port numbers must be 0-65535\n");
            return 1;
        }
        return do_build((uint16_t)sp, (uint16_t)dp, argv[4]);
    }

    fprintf(stderr, "usage: %s <hex string of UDP datagram>\n", argv[0]);
    fprintf(stderr, "       %s --build <src_port> <dst_port> <payload>\n", argv[0]);
    fprintf(stderr, "\nexample: %s 003500350013a1b2 48656c6c6f\n", argv[0]);
    fprintf(stderr, "         %s --build 12345 53 hello\n", argv[0]);
    return 1;
}
