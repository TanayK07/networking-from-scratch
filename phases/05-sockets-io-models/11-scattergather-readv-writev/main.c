#include "iovec.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Scatter/Gather I/O ===\n\n");

    /* Set up three buffers */
    uint8_t buf1[8], buf2[16], buf3[8];
    struct nfs_iovec iov[3] = {
        {buf1, sizeof(buf1)},
        {buf2, sizeof(buf2)},
        {buf3, sizeof(buf3)},
    };

    /* Total length */
    size_t total = nfs_iov_total_len(iov, 3);
    printf("Total capacity: %zu bytes\n", total);

    /* Format */
    char fmt[256];
    nfs_iov_format(iov, 3, fmt, sizeof(fmt));
    printf("Layout: %s\n\n", fmt);

    /* Scatter: distribute "Hello, scatter/gather world!!" across buffers */
    const char *msg = "Hello, scatter/gather world!!";
    int scattered = nfs_iov_scatter(iov, 3, (const uint8_t *)msg, strlen(msg));
    printf("Scattered %d bytes across 3 buffers\n", scattered);
    printf("  buf1: %.*s\n", (int)sizeof(buf1), (char *)buf1);
    printf("  buf2: %.*s\n", (int)sizeof(buf2), (char *)buf2);
    printf("  buf3: %.*s\n\n", 4, (char *)buf3);

    /* Gather: concatenate back */
    uint8_t gathered[64];
    int glen = nfs_iov_gather(iov, 3, gathered, sizeof(gathered));
    printf("Gathered %d bytes: %.*s\n\n", glen, scattered, (char *)gathered);

    /* Find byte */
    int idx;
    size_t off;
    if (nfs_iov_find_byte(iov, 3, 10, &idx, &off) == 0) {
        printf("Byte 10 is in iov[%d] at offset %zu\n", idx, off);
    }

    return 0;
}
