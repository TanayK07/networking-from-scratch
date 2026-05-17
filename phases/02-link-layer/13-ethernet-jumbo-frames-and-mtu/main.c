/*
 * mtu_tool -- demonstrate MTU, jumbo frames, and frame size limits.
 *
 * Build:  make
 * Run:    ./mtu_tool
 */
#include "mtu.h"

#include <stdio.h>

int main(void) {
    printf("=== Ethernet MTU & Jumbo Frames Demo ===\n\n");

    /* Standard profile. */
    struct nfs_mtu_profile std = nfs_mtu_standard();
    printf("Profile: %s\n", std.name);
    printf("  MTU:       %zu bytes\n", std.mtu);
    printf("  Min frame: %zu bytes\n", std.min_frame);
    printf("  Max frame: %zu bytes\n\n", std.max_frame);

    /* Jumbo profile. */
    struct nfs_mtu_profile jmb = nfs_mtu_jumbo();
    printf("Profile: %s\n", jmb.name);
    printf("  MTU:       %zu bytes\n", jmb.mtu);
    printf("  Min frame: %zu bytes\n", jmb.min_frame);
    printf("  Max frame: %zu bytes\n\n", jmb.max_frame);

    /* Efficiency comparison. */
    printf("Efficiency comparison:\n");
    printf("  46-byte payload:   %.2f%%\n", nfs_mtu_efficiency(46) * 100.0);
    printf("  1500-byte payload: %.2f%%\n", nfs_mtu_efficiency(1500) * 100.0);
    printf("  9000-byte payload: %.2f%%\n\n", nfs_mtu_efficiency(9000) * 100.0);

    /* Max FPS at 1 Gbps. */
    printf("Max frames/sec at 1 Gbps:\n");
    printf("  64-byte frames:   %zu\n", nfs_mtu_max_fps(64, 1000));
    printf("  1518-byte frames: %zu\n\n", nfs_mtu_max_fps(1518, 1000));

    /* Goodput. */
    double gp = nfs_mtu_goodput_bps(1500, 1000);
    printf("Goodput with 1500B payload at 1 Gbps: %.1f Mbps\n\n", gp / 1e6);

    /* Path MTU. */
    size_t links[] = {1500, 9000, 1500, 576};
    size_t pmtu = nfs_path_mtu(links, 4);
    printf("Path MTU across [1500, 9000, 1500, 576]: %zu\n", pmtu);

    return 0;
}
