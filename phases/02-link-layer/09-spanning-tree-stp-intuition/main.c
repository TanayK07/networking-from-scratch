/*
 * stp_demo — demonstrate STP bridge ID comparison and BPDU handling.
 *
 * Build:  make
 * Run:    ./stp_demo
 */
#include "stp.h"

#include <stdio.h>
#include <string.h>

static void print_bridge_id(const char *label, const struct nfs_bridge_id *id) {
    printf("  %s: priority=%u mac=%02x:%02x:%02x:%02x:%02x:%02x\n", label, id->priority, id->mac[0],
           id->mac[1], id->mac[2], id->mac[3], id->mac[4], id->mac[5]);
}

int main(void) {
    printf("=== STP (Spanning Tree Protocol) Demo ===\n\n");

    /* Create two bridge IDs and compare. */
    uint8_t mac_a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t mac_b[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x66};

    struct nfs_bridge_id id_a = {.priority = NFS_STP_DEFAULT_PRIORITY};
    memcpy(id_a.mac, mac_a, 6);

    struct nfs_bridge_id id_b = {.priority = NFS_STP_DEFAULT_PRIORITY};
    memcpy(id_b.mac, mac_b, 6);

    printf("Bridge ID comparison:\n");
    print_bridge_id("Bridge A", &id_a);
    print_bridge_id("Bridge B", &id_b);

    int cmp = nfs_bridge_id_cmp(&id_a, &id_b);
    if (cmp < 0)
        printf("  -> Bridge A is the better root (lower ID)\n");
    else if (cmp > 0)
        printf("  -> Bridge B is the better root (lower ID)\n");
    else
        printf("  -> Bridge IDs are equal\n");

    /* Build and parse a Configuration BPDU. */
    printf("\nBPDU build/parse:\n");

    struct nfs_bpdu bpdu;
    memset(&bpdu, 0, sizeof(bpdu));
    bpdu.proto_id = 0;
    bpdu.version = 0;
    bpdu.type = 0x00; /* Configuration */
    bpdu.root_id = id_a;
    bpdu.root_path_cost = 0;
    bpdu.sender_id = id_a;
    bpdu.port_id = 1;
    bpdu.message_age = 0;
    bpdu.max_age = NFS_STP_MAX_AGE * 256;
    bpdu.hello_time = NFS_STP_HELLO_TIME * 256;
    bpdu.forward_delay = NFS_STP_FWD_DELAY * 256;

    uint8_t buf[64];
    int built = nfs_bpdu_build(&bpdu, buf, sizeof(buf));
    printf("  Built BPDU: %d bytes\n", built);

    struct nfs_bpdu parsed;
    if (nfs_bpdu_parse(buf, (size_t)built, &parsed) == 0) {
        printf("  Parsed OK: root_path_cost=%u, port_id=%u\n", parsed.root_path_cost,
               parsed.port_id);
        print_bridge_id("Root", &parsed.root_id);
    }

    /* Demonstrate path cost. */
    printf("\nIEEE 802.1D-2004 path costs:\n");
    printf("  10 Mbps:    %u\n", nfs_stp_path_cost(10));
    printf("  100 Mbps:   %u\n", nfs_stp_path_cost(100));
    printf("  1000 Mbps:  %u\n", nfs_stp_path_cost(1000));
    printf("  10000 Mbps: %u\n", nfs_stp_path_cost(10000));

    /* Demonstrate bridge election. */
    printf("\nBridge election:\n");
    struct nfs_stp_bridge bridge;
    nfs_stp_bridge_init(&bridge, NFS_STP_DEFAULT_PRIORITY, mac_b);
    bridge.num_ports = 2;
    nfs_stp_port_init(&bridge.ports[0], 1, 19);
    nfs_stp_port_init(&bridge.ports[1], 2, 19);

    printf("  Before BPDU: root_port=%d\n", bridge.root_port);
    print_bridge_id("Root", &bridge.root_id);

    /* Receive a BPDU advertising a better root. */
    nfs_stp_process_bpdu(&bridge, 0, &bpdu);
    printf("  After BPDU:  root_port=%d, root_path_cost=%u\n", bridge.root_port,
           bridge.root_path_cost);
    print_bridge_id("Root", &bridge.root_id);

    return 0;
}
