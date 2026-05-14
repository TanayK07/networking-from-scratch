/* CRC calculator — reads a hex string (or stdin) and prints CRC-8/16/32.
 *
 *   $ ./crc_calc "48656c6c6f"
 *   CRC-8:  0x92
 *   CRC-16: 0x29B1
 *   CRC-32: 0xF7D18982
 *
 *   $ echo -n "Hello" | ./crc_calc
 *   CRC-8:  0x92
 *   CRC-16: 0x29B1
 *   CRC-32: 0xF7D18982
 */

#include "crc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Convert a hex character to its 4-bit value, or -1 on error. */
static int hex_val(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Parse a hex string into a byte buffer.
 * Returns the number of bytes written, or -1 on error. */
static int parse_hex(const char *hex, uint8_t *out, size_t max) {
    size_t slen = strlen(hex);
    if (slen % 2 != 0)
        return -1;

    size_t nbytes = slen / 2;
    if (nbytes > max)
        return -1;

    for (size_t i = 0; i < nbytes; i++) {
        int hi = hex_val(hex[2 * i]);
        int lo = hex_val(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
            return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return (int)nbytes;
}

/* Read all of stdin into a dynamically allocated buffer.
 * Sets *out_len to the number of bytes read.  Returns the buffer
 * (caller must free), or NULL on error. */
static uint8_t *read_stdin(size_t *out_len) {
    size_t cap = 4096;
    size_t len = 0;
    uint8_t *buf = malloc(cap);
    if (!buf)
        return NULL;

    while (!feof(stdin)) {
        size_t n = fread(buf + len, 1, cap - len, stdin);
        len += n;
        if (len == cap) {
            cap *= 2;
            uint8_t *tmp = realloc(buf, cap);
            if (!tmp) {
                free(buf);
                return NULL;
            }
            buf = tmp;
        }
    }
    *out_len = len;
    return buf;
}

static void print_crcs(const uint8_t *data, size_t len) {
    uint8_t c8 = nfs_crc8(data, len);
    uint16_t c16 = nfs_crc16(data, len);
    uint32_t c32 = nfs_crc32(data, len);

    printf("CRC-8:  0x%02X\n", c8);
    printf("CRC-16: 0x%04X\n", c16);
    printf("CRC-32: 0x%08X\n", c32);
}

int main(int argc, char *argv[]) {
    /* Initialize lookup tables. */
    nfs_crc8_init_table();
    nfs_crc16_init_table();
    nfs_crc32_init_table();

    if (argc > 1) {
        /* Argument mode: treat argv[1] as a hex string. */
        uint8_t buf[65536];
        int n = parse_hex(argv[1], buf, sizeof(buf));
        if (n < 0) {
            fprintf(stderr, "error: invalid hex string '%s'\n", argv[1]);
            fprintf(stderr, "usage: %s <hex-string>\n", argv[0]);
            fprintf(stderr, "       echo -n 'data' | %s\n", argv[0]);
            return 1;
        }
        print_crcs(buf, (size_t)n);
    } else {
        /* Stdin mode: read raw bytes. */
        size_t len = 0;
        uint8_t *data = read_stdin(&len);
        if (!data) {
            fprintf(stderr, "error: failed to read stdin\n");
            return 1;
        }
        if (len == 0) {
            fprintf(stderr, "error: no input data\n");
            free(data);
            return 1;
        }
        print_crcs(data, len);
        free(data);
    }

    return 0;
}
