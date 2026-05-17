/*
 * frame — demonstrate Ethernet II frame construction and parsing
 *         for raw AF_PACKET transmission.
 *
 * Build:  make
 * Run:    ./frame
 */
#include "frame.h"

#include <stdio.h>
#include <string.h>

static void demo_build_and_parse(void) {
    uint8_t dst[NFS_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t src[NFS_ETH_ALEN] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t payload[] = "Hello from raw Ethernet!";

    uint8_t wire[1518];
    int len =
        nfs_frame_build(dst, src, NFS_ETHERTYPE_IP, payload, sizeof(payload), wire, sizeof(wire));
    if (len < 0) {
        fprintf(stderr, "Build failed!\n");
        return;
    }

    char dst_str[18], src_str[18];
    nfs_mac_format(dst, dst_str, sizeof(dst_str));
    nfs_mac_format(src, src_str, sizeof(src_str));
    printf("Built frame: %d bytes\n", len);
    printf("  dst: %s (broadcast=%d)\n", dst_str, nfs_mac_is_broadcast(dst));
    printf("  src: %s\n", src_str);
    printf("  ethertype: 0x%04X\n", NFS_ETHERTYPE_IP);

    /* Parse it back. */
    struct nfs_eth_frame frame;
    if (nfs_frame_parse(wire, (size_t)len, &frame) == 0) {
        char parsed_dst[18], parsed_src[18];
        nfs_mac_format(frame.dst, parsed_dst, sizeof(parsed_dst));
        nfs_mac_format(frame.src, parsed_src, sizeof(parsed_src));
        printf("\nRe-parsed:\n");
        printf("  dst: %s\n", parsed_dst);
        printf("  src: %s\n", parsed_src);
        printf("  ethertype: 0x%04X\n", frame.ethertype);
        printf("  payload (%zu bytes): \"%.*s\"\n", frame.payload_len, (int)sizeof(payload),
               frame.payload);
    }

    /* Validate. */
    printf("\nValid frame? %s\n", nfs_frame_valid(wire, (size_t)len) ? "yes" : "no");
}

static void demo_mac_utilities(void) {
    printf("\n--- MAC utilities ---\n");

    uint8_t mac[NFS_ETH_ALEN];
    if (nfs_mac_parse("DE:AD:BE:EF:00:01", mac) == 0) {
        char buf[18];
        nfs_mac_format(mac, buf, sizeof(buf));
        printf("Parsed+formatted: %s\n", buf);
        printf("  broadcast=%d  multicast=%d\n", nfs_mac_is_broadcast(mac),
               nfs_mac_is_multicast(mac));
    }

    /* Multicast example. */
    uint8_t mcast[NFS_ETH_ALEN] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};
    char mcast_str[18];
    nfs_mac_format(mcast, mcast_str, sizeof(mcast_str));
    printf("Multicast: %s  multicast=%d  broadcast=%d\n", mcast_str, nfs_mac_is_multicast(mcast),
           nfs_mac_is_broadcast(mcast));
}

static void demo_raw_addr(void) {
    printf("\n--- Raw address ---\n");
    uint8_t dst[NFS_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    struct nfs_raw_addr addr;
    if (nfs_raw_addr_init(&addr, 2, dst, NFS_ETHERTYPE_ARP) == 0) {
        char buf[18];
        nfs_mac_format(addr.dst, buf, sizeof(buf));
        printf("  ifindex=%d  dst=%s  ethertype=0x%04X\n", addr.ifindex, buf, addr.ethertype);
    }
}

int main(void) {
    printf("=== Send Your First Frame Demo ===\n\n");
    demo_build_and_parse();
    demo_mac_utilities();
    demo_raw_addr();
    return 0;
}
