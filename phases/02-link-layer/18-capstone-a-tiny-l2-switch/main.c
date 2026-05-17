/*
 * l2switch — demonstrate a tiny L2 switch simulator.
 *
 * Build:  make
 * Run:    ./l2switch
 */
#include "l2switch.h"

#include <stdio.h>
#include <string.h>

static void mac_fmt(const uint8_t mac[6], char *buf) {
    snprintf(buf, 18, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
             mac[5]);
}

static void demo_basic_switching(void) {
    printf("=== Basic L2 Switching Demo ===\n\n");

    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    /* Add 4 access ports on VLAN 1 */
    for (int i = 0; i < 4; i++) {
        int id = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);
        printf("Added port %d (access, VLAN 1)\n", id);
    }

    /* Host A on port 0 sends to unknown host B */
    uint8_t mac_a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t mac_b[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x02};
    uint8_t frame[60];
    nfs_sw_build_frame(mac_b, mac_a, 0x0800, NULL, 0, frame, sizeof(frame));

    struct nfs_sw_result res;
    nfs_sw_process_frame(&sw, 0, frame, sizeof(frame), &res);

    char a_str[18], b_str[18];
    mac_fmt(mac_a, a_str);
    mac_fmt(mac_b, b_str);
    printf("\nFrame: %s -> %s\n", a_str, b_str);
    printf("Action: %s (dst unknown)\n", res.action == NFS_SW_FLOOD ? "FLOOD" : "???");

    /* Host B replies from port 2 */
    nfs_sw_build_frame(mac_a, mac_b, 0x0800, NULL, 0, frame, sizeof(frame));
    nfs_sw_process_frame(&sw, 2, frame, sizeof(frame), &res);

    printf("\nFrame: %s -> %s\n", b_str, a_str);
    printf("Action: %s to port %d (MAC learned)\n",
           res.action == NFS_SW_FORWARD ? "FORWARD" : "???", res.dst_port);

    printf("\nMAC table entries: %d\n", nfs_sw_mac_count(&sw));
    printf("Stats: rx=%llu tx=%llu flooded=%llu forwarded=%llu dropped=%llu\n",
           (unsigned long long)sw.total_rx, (unsigned long long)sw.total_tx,
           (unsigned long long)sw.total_flooded, (unsigned long long)sw.total_forwarded,
           (unsigned long long)sw.total_dropped);
}

static void demo_vlan_isolation(void) {
    printf("\n=== VLAN Isolation Demo ===\n\n");

    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    /* Create VLAN 10 and 20 */
    nfs_sw_vlan_create(&sw, 10, "engineering");
    nfs_sw_vlan_create(&sw, 20, "marketing");

    /* Port 0,1 on VLAN 10; Port 2,3 on VLAN 20 */
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 10);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 10);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 20);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 20);

    /* Allow VLANs on their respective ports */
    nfs_sw_port_allow_vlan(&sw, 0, 10);
    nfs_sw_port_allow_vlan(&sw, 1, 10);
    nfs_sw_port_allow_vlan(&sw, 2, 20);
    nfs_sw_port_allow_vlan(&sw, 3, 20);

    /* Same MAC on different VLANs */
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    nfs_sw_mac_learn(&sw, mac, 0, 10);
    nfs_sw_mac_learn(&sw, mac, 2, 20);

    printf("Same MAC on VLAN 10 -> port %d\n", nfs_sw_mac_lookup(&sw, mac, 10));
    printf("Same MAC on VLAN 20 -> port %d\n", nfs_sw_mac_lookup(&sw, mac, 20));
    printf("MAC table entries: %d (two per-VLAN entries)\n", nfs_sw_mac_count(&sw));
}

int main(void) {
    demo_basic_switching();
    demo_vlan_isolation();
    return 0;
}
