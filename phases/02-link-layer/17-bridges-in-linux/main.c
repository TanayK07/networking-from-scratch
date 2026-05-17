/*
 * bridge_demo -- demonstrate Linux bridge forwarding simulation.
 *
 * Build:  make
 * Run:    ./bridge_demo
 */
#include "bridge.h"

#include <stdio.h>
#include <string.h>

static void mac_fmt(const uint8_t mac[6], char *buf, size_t sz) {
    snprintf(buf, sz, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
             mac[5]);
}

static void print_fdb(const struct nfs_bridge *br) {
    printf("  FDB (%d entries):\n", nfs_fdb_count(br));
    for (int i = 0; i < NFS_FDB_MAX_ENTRIES; i++) {
        if (br->fdb[i].valid) {
            char m[18];
            mac_fmt(br->fdb[i].mac, m, sizeof(m));
            printf("    %s -> port %d%s\n", m, br->fdb[i].port_id,
                   br->fdb[i].is_static ? " (static)" : "");
        }
    }
}

static const char *action_name(int action) {
    switch (action) {
    case NFS_BRIDGE_FORWARD:
        return "FORWARD";
    case NFS_BRIDGE_FLOOD:
        return "FLOOD";
    case NFS_BRIDGE_DROP:
        return "DROP";
    default:
        return "UNKNOWN";
    }
}

int main(void) {
    printf("=== Linux Bridge Forwarding Demo ===\n\n");

    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");
    printf("Bridge '%s' initialized (aging=%u s)\n\n", br.name, br.aging_time);

    /* Add ports */
    uint8_t mac0[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x00};
    uint8_t mac1[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t mac2[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x02};

    nfs_bridge_add_port(&br, "eth0", mac0);
    nfs_bridge_add_port(&br, "eth1", mac1);
    nfs_bridge_add_port(&br, "eth2", mac2);
    printf("Added %d ports\n\n", br.num_ports);

    /* Simulate frames */
    /* Frame from host A (on port 0) to host B (unknown) */
    uint8_t frame1[64];
    memset(frame1, 0, sizeof(frame1));
    uint8_t host_a[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01};
    uint8_t host_b[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x02};
    memcpy(frame1, host_b, 6);     /* dst */
    memcpy(frame1 + 6, host_a, 6); /* src */
    frame1[12] = 0x08;
    frame1[13] = 0x00; /* IPv4 */

    int action = nfs_bridge_forward(&br, 0, frame1, sizeof(frame1));
    printf("Frame A->B on port 0: %s (B unknown)\n", action_name(action));
    print_fdb(&br);
    printf("\n");

    /* Frame from host B (on port 2) to host A (now known on port 0) */
    uint8_t frame2[64];
    memset(frame2, 0, sizeof(frame2));
    memcpy(frame2, host_a, 6);     /* dst */
    memcpy(frame2 + 6, host_b, 6); /* src */
    frame2[12] = 0x08;
    frame2[13] = 0x00;

    action = nfs_bridge_forward(&br, 2, frame2, sizeof(frame2));
    printf("Frame B->A on port 2: %s (A known on port 0)\n", action_name(action));
    print_fdb(&br);
    printf("\n");

    /* Broadcast frame */
    uint8_t frame3[64];
    memset(frame3, 0, sizeof(frame3));
    memset(frame3, 0xFF, 6);       /* dst = broadcast */
    memcpy(frame3 + 6, host_a, 6); /* src */
    frame3[12] = 0x08;
    frame3[13] = 0x06; /* ARP */

    action = nfs_bridge_forward(&br, 0, frame3, sizeof(frame3));
    printf("Broadcast ARP from A on port 0: %s\n", action_name(action));

    /* Stats */
    printf("\nBridge stats: forwarded=%lu flooded=%lu dropped=%lu learned=%lu\n",
           (unsigned long)br.forwarded, (unsigned long)br.flooded, (unsigned long)br.dropped,
           (unsigned long)br.learned);

    return 0;
}
