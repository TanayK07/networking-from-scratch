/* main.c — TCP Options encoder/decoder demonstration.
 *
 * Builds several TCP options, concatenates them, pads to 4-byte
 * boundary, then parses and displays the result. */

#include "tcp_opts.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== TCP Options Encoder/Decoder Demo ===\n\n");

    uint8_t buf[64];
    size_t pos = 0;
    int n;

    /* Build a typical SYN option set: MSS + WScale + SACK-Perm + Timestamps */
    printf("Building options:\n");

    n = nfs_tcp_opts_build_mss(1460, buf + pos, sizeof(buf) - pos);
    printf("  MSS option: %d bytes\n", n);
    pos += (size_t)n;

    n = nfs_tcp_opts_build_wscale(7, buf + pos, sizeof(buf) - pos);
    printf("  WScale option: %d bytes\n", n);
    pos += (size_t)n;

    n = nfs_tcp_opts_build_sack_perm(buf + pos, sizeof(buf) - pos);
    printf("  SACK-Perm option: %d bytes\n", n);
    pos += (size_t)n;

    n = nfs_tcp_opts_build_timestamps(12345, 0, buf + pos, sizeof(buf) - pos);
    printf("  Timestamps option: %d bytes\n", n);
    pos += (size_t)n;

    printf("  Total before padding: %zu bytes\n", pos);

    /* Pad to 4-byte boundary */
    size_t padded;
    nfs_tcp_opts_pad(buf, pos, &padded);
    printf("  Total after padding:  %zu bytes\n\n", padded);

    /* Hex dump */
    printf("Wire bytes:\n  ");
    for (size_t i = 0; i < padded; i++) {
        printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n  ");
    }
    printf("\n\n");

    /* Parse them back */
    struct nfs_tcp_option opts[16];
    size_t nfound;
    int rc = nfs_tcp_opts_parse(buf, padded, opts, 16, &nfound);
    printf("Parse result: %d, found %zu options:\n", rc, nfound);

    for (size_t i = 0; i < nfound; i++) {
        char desc[128];
        nfs_tcp_opt_format(&opts[i], desc, sizeof(desc));
        printf("  [%zu] kind=%u len=%u  %s\n", i, opts[i].kind, opts[i].length, desc);
    }

    printf("\nDone.\n");
    return 0;
}
