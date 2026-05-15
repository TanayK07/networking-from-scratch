#include "bitfields.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Parse hex string to raw bytes (inline utility). */
static int hex_char(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_to_bytes(const char *hex, uint8_t *out, size_t max)
{
    size_t count = 0;
    const char *p = hex;
    while (*p) {
        if (*p == ' ') { p++; continue; }
        int hi = hex_char(*p++);
        if (hi < 0 || !*p) return -1;
        int lo = hex_char(*p++);
        if (lo < 0) return -1;
        if (count >= max) return -1;
        out[count++] = (uint8_t)((hi << 4) | lo);
    }
    return (int)count;
}

int main(int argc, char *argv[])
{
    /* Default: classic IPv4 header */
    const char *default_hex =
        "45 00 00 3c 1c 46 40 00 40 06 a6 ec 0a 00 02 0f ac 10 01 65";

    const char *input = (argc > 1) ? argv[1] : default_hex;

    uint8_t raw[64];
    int n = hex_to_bytes(input, raw, sizeof(raw));
    if (n < 20) {
        fprintf(stderr, "Need at least 20 hex bytes for an IPv4 header.\n");
        return 1;
    }

    const struct nfs_ipv4_hdr *hdr = (const struct nfs_ipv4_hdr *)raw;
    char buf[512];
    nfs_ipv4_format(hdr, buf, sizeof(buf));
    printf("%s\n", buf);

    printf("\nField breakdown:\n");
    printf("  Version:       %u\n", nfs_ipv4_version(hdr));
    printf("  IHL:           %u (%u bytes)\n", nfs_ipv4_ihl(hdr),
           (unsigned)nfs_ipv4_ihl(hdr) * 4);
    printf("  DSCP:          %u\n", nfs_ipv4_dscp(hdr));
    printf("  ECN:           %u\n", nfs_ipv4_ecn(hdr));
    printf("  Flags:         0x%x\n", nfs_ipv4_flags(hdr));
    printf("  Frag offset:   %u\n", nfs_ipv4_frag_offset(hdr));
    printf("  TTL:           %u\n", hdr->ttl);
    printf("  Protocol:      %u\n", hdr->protocol);

    return 0;
}
