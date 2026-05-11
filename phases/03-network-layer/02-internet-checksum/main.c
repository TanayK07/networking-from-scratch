/* CLI: take a hex string on argv, print its Internet checksum.
 *
 *   $ ./checksum 0001f203f4f5f6f7
 *   data:        00 01 f2 03 f4 f5 f6 f7
 *   length:      8 bytes
 *   checksum:    0x220d
 *
 * The expected value 0x220d for that input comes from RFC 1071 §3.
 */

#include "../../../common/c/checksum.h"
#include "incremental.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        if (hi < 0) return -1;
        if (!*s) return -1;                 /* odd nibble count */
        int lo = hex2nyb(*s++);
        if (lo < 0) return -1;
        if (n >= cap) return -1;
        out[n++] = (uint8_t)((hi << 4) | lo);
    }
    *len = n;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <hex string>\n", argv[0]);
        fprintf(stderr, "  example: %s 0001f203f4f5f6f7\n", argv[0]);
        return 1;
    }

    uint8_t buf[4096] = {0};
    size_t len = 0;
    if (parse_hex(argv[1], buf, sizeof(buf), &len) < 0) {
        fprintf(stderr, "error: not a valid hex string\n");
        return 1;
    }

    uint16_t cs = internet_checksum(buf, len);

    printf("data:        ");
    for (size_t i = 0; i < len; i++) printf("%02x ", buf[i]);
    printf("\nlength:      %zu bytes\n", len);
    printf("checksum:    0x%04x\n", cs);
    return 0;
}
