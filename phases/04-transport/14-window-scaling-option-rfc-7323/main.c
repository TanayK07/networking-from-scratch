#include "wscale.h"
#include <stdio.h>

/* Demonstrate TCP window scaling: compute shifts, build/parse options,
 * and show effective window sizes. */

int main(void) {
    char buf[128];

    printf("=== TCP Window Scaling (RFC 7323) Demo ===\n\n");

    /* Show scaling for various shift values */
    printf("Window scaling table (base window = 65535):\n");
    for (uint8_t s = 0; s <= NFS_WSCALE_MAX; s++) {
        uint32_t eff = nfs_wscale_apply(65535, s);
        nfs_wscale_format(s, buf, sizeof(buf));
        printf("  shift=%2u  effective=%10u bytes  (%s)\n", s, eff, buf);
    }

    /* Compute shift for desired windows */
    printf("\nCompute minimum shift for desired windows:\n");
    uint32_t sizes[] = {8192, 65535, 131072, 1048576, 16777216, 1073741824};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        uint8_t shift = nfs_wscale_compute(sizes[i]);
        uint16_t compressed = nfs_wscale_compress(sizes[i], shift);
        uint32_t restored = nfs_wscale_apply(compressed, shift);
        printf("  desired=%10u  shift=%u  header=%u  restored=%u\n", sizes[i], shift, compressed,
               restored);
    }

    /* Build and parse an option */
    printf("\nBuild and parse TCP option:\n");
    uint8_t opt[3];
    int n = nfs_wscale_build_option(7, opt, sizeof(opt));
    printf("  Built %d-byte option: [%u, %u, %u]\n", n, opt[0], opt[1], opt[2]);

    uint8_t parsed_shift;
    int rc = nfs_wscale_parse_option(opt, (size_t)n, &parsed_shift);
    printf("  Parsed: rc=%d shift=%u\n", rc, parsed_shift);

    nfs_wscale_format(parsed_shift, buf, sizeof(buf));
    printf("  %s\n", buf);

    return 0;
}
