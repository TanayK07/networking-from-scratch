/*
 * promisc_demo — demonstrate frame filtering in normal vs promiscuous mode.
 *
 * Build:  make
 * Run:    ./promisc_demo
 */
#include "promisc.h"

#include <stdio.h>
#include <string.h>

/* Build a minimal 14-byte Ethernet frame (header only, no payload). */
static void build_frame(uint8_t frame[60], const uint8_t dst[6], const uint8_t src[6]) {
    memset(frame, 0, 60);
    memcpy(frame, dst, 6);
    memcpy(frame + 6, src, 6);
    frame[12] = 0x08;
    frame[13] = 0x00; /* IPv4 ethertype */
}

int main(void) {
    printf("=== Promiscuous Mode Demo ===\n\n");

    uint8_t my_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t other_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t mcast[6] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};
    uint8_t sender[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};

    uint8_t frame[60];

    /* --- Normal mode --- */
    printf("--- Normal mode ---\n");
    struct nfs_frame_filter f;
    nfs_filter_init(&f, my_mac, 0);

    build_frame(frame, my_mac, sender);
    printf("Unicast to us:      %s\n",
           nfs_filter_accept(&f, frame, 60) == 1 ? "ACCEPTED" : "REJECTED");

    build_frame(frame, other_mac, sender);
    printf("Unicast to other:   %s\n",
           nfs_filter_accept(&f, frame, 60) == 1 ? "ACCEPTED" : "REJECTED");

    build_frame(frame, bcast, sender);
    printf("Broadcast:          %s\n",
           nfs_filter_accept(&f, frame, 60) == 1 ? "ACCEPTED" : "REJECTED");

    build_frame(frame, mcast, sender);
    printf("Multicast:          %s\n",
           nfs_filter_accept(&f, frame, 60) == 1 ? "ACCEPTED" : "REJECTED");

    char summary[256];
    nfs_filter_summary(&f, summary, sizeof(summary));
    printf("Stats: %s\n\n", summary);

    /* --- Promiscuous mode --- */
    printf("--- Promiscuous mode ---\n");
    nfs_filter_init(&f, my_mac, 1);

    build_frame(frame, my_mac, sender);
    printf("Unicast to us:      %s\n",
           nfs_filter_accept(&f, frame, 60) == 1 ? "ACCEPTED" : "REJECTED");

    build_frame(frame, other_mac, sender);
    printf("Unicast to other:   %s\n",
           nfs_filter_accept(&f, frame, 60) == 1 ? "ACCEPTED" : "REJECTED");

    build_frame(frame, bcast, sender);
    printf("Broadcast:          %s\n",
           nfs_filter_accept(&f, frame, 60) == 1 ? "ACCEPTED" : "REJECTED");

    build_frame(frame, mcast, sender);
    printf("Multicast:          %s\n",
           nfs_filter_accept(&f, frame, 60) == 1 ? "ACCEPTED" : "REJECTED");

    nfs_filter_summary(&f, summary, sizeof(summary));
    printf("Stats: %s\n\n", summary);

    /* Constants */
    printf("IFF_PROMISC = 0x%X\n", NFS_IFF_PROMISC);
    printf("PACKET_MR_PROMISC = %d\n", NFS_PACKET_MR_PROMISC);

    return 0;
}
