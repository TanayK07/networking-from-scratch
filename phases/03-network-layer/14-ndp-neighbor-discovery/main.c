/*
 * main.c -- NDP demo driver
 */
#include "ndp.h"
#include <stdio.h>
#include <string.h>

static void hex_print(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
}

int main(void) {
    /* Example: fe80::1 */
    uint8_t target[16] = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    uint8_t mac[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    char ipv6_str[64];

    printf("=== NDP Neighbor Discovery ===\n\n");

    /* Build NS */
    uint8_t buf[128];
    int n = nfs_ndp_build_ns(target, mac, buf, sizeof(buf));
    printf("Neighbor Solicitation (%d bytes):\n  ", n);
    hex_print(buf, (size_t)n);
    printf("  Target: %s\n\n", nfs_ndp_format_ipv6(target, ipv6_str, sizeof(ipv6_str)));

    /* Build NA */
    n = nfs_ndp_build_na(target, NFS_NDP_NA_FLAG_S | NFS_NDP_NA_FLAG_O, mac, buf, sizeof(buf));
    printf("Neighbor Advertisement (%d bytes):\n  ", n);
    hex_print(buf, (size_t)n);

    /* Parse it back */
    struct nfs_ndp_na na;
    if (nfs_ndp_parse_na(buf, (size_t)n, &na) == 0) {
        printf("  Flags: R=%d S=%d O=%d\n", (na.flags & NFS_NDP_NA_FLAG_R) ? 1 : 0,
               (na.flags & NFS_NDP_NA_FLAG_S) ? 1 : 0, (na.flags & NFS_NDP_NA_FLAG_O) ? 1 : 0);
    }

    /* Build RS */
    n = nfs_ndp_build_rs(mac, buf, sizeof(buf));
    printf("\nRouter Solicitation (%d bytes):\n  ", n);
    hex_print(buf, (size_t)n);

    /* Build RA */
    n = nfs_ndp_build_ra(64, NFS_NDP_RA_FLAG_M, 1800, 30000, 1000, mac, buf, sizeof(buf));
    printf("Router Advertisement (%d bytes):\n  ", n);
    hex_print(buf, (size_t)n);

    return 0;
}
