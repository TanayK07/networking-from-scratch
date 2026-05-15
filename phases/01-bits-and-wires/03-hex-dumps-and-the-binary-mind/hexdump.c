#include "hexdump.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ---------------------------------------------------------------
 * A single canonical hex-dump line is at most 79 characters + NUL:
 *   "XXXXXXXX  HH HH HH HH HH HH HH HH  HH HH HH HH HH HH HH HH  |AAAAAAAAAAAAAAAA|\n"
 * Offset(8) + "  " + hex(48 with spaces) + " " + "|" + ascii(16) + "|" + "\n"
 * --------------------------------------------------------------- */

#define BYTES_PER_LINE 16
#define MAX_LINE_LEN   80  /* generous upper bound per line */

int nfs_hexdump_line(const uint8_t *data, size_t len, size_t offset,
                     char *out, size_t out_sz)
{
    if (!data || !out || len == 0 || len > BYTES_PER_LINE)
        return -1;

    /* We need at most ~79 chars + NUL */
    if (out_sz < MAX_LINE_LEN)
        return -1;

    char *p = out;
    int n;

    /* Offset */
    n = snprintf(p, out_sz, "%08zx  ", offset);
    p += n;

    /* Hex bytes */
    for (size_t i = 0; i < BYTES_PER_LINE; i++) {
        if (i == 8) {
            *p++ = ' ';
        }
        if (i < len) {
            *p++ = "0123456789abcdef"[data[i] >> 4];
            *p++ = "0123456789abcdef"[data[i] & 0x0F];
        } else {
            *p++ = ' ';
            *p++ = ' ';
        }
        if (i < BYTES_PER_LINE - 1) {
            *p++ = ' ';
        }
    }

    /* ASCII pane */
    *p++ = ' ';
    *p++ = ' ';
    *p++ = '|';
    for (size_t i = 0; i < len; i++) {
        *p++ = (data[i] >= 0x20 && data[i] <= 0x7E) ? (char)data[i] : '.';
    }
    *p++ = '|';
    *p++ = '\n';
    *p = '\0';

    return (int)(p - out);
}

int nfs_hexdump(const uint8_t *data, size_t len, char *out, size_t out_sz)
{
    if (!out)
        return -1;

    if (len == 0) {
        if (out_sz < 1)
            return -1;
        out[0] = '\0';
        return 0;
    }

    if (!data)
        return -1;

    char *p = out;
    size_t remaining = out_sz;
    size_t offset = 0;

    while (offset < len) {
        size_t chunk = len - offset;
        if (chunk > BYTES_PER_LINE)
            chunk = BYTES_PER_LINE;

        int written = nfs_hexdump_line(data + offset, chunk, offset,
                                       p, remaining);
        if (written < 0)
            return -1;

        p += written;
        remaining -= (size_t)written;
        offset += chunk;
    }

    return (int)(p - out);
}

/* Parse a single hex character to its value (0-15) or -1. */
static int hex_char_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int nfs_hex_to_bytes(const char *hex, uint8_t *out, size_t max)
{
    if (!hex || !out)
        return -1;

    size_t count = 0;
    const char *p = hex;

    while (*p) {
        /* Skip spaces */
        if (*p == ' ') {
            p++;
            continue;
        }

        int hi = hex_char_val(*p);
        if (hi < 0)
            return -1;
        p++;

        if (!*p)
            return -1;  /* Odd number of hex chars */

        int lo = hex_char_val(*p);
        if (lo < 0)
            return -1;
        p++;

        if (count >= max)
            return -1;

        out[count++] = (uint8_t)((hi << 4) | lo);
    }

    return (int)count;
}
