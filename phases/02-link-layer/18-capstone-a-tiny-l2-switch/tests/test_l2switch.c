/* Unit tests for the tiny L2 switch simulator. */

#include "../l2switch.h"

#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(expr, expected)                                                                  \
    do {                                                                                           \
        tests_run++;                                                                               \
        long long _got = (long long)(expr);                                                        \
        long long _exp = (long long)(expected);                                                    \
        if (_got != _exp) {                                                                        \
            fprintf(stderr, "  FAIL %s:%d: %s == 0x%llx, want 0x%llx\n", __FILE__, __LINE__,       \
                    #expr, _got, _exp);                                                            \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

#define ASSERT_TRUE(expr)                                                                          \
    do {                                                                                           \
        tests_run++;                                                                               \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "  FAIL %s:%d: %s is false\n", __FILE__, __LINE__, #expr);             \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

/* ------------------------------------------------------------------ */
/*  Test 1: Switch init                                                */
/* ------------------------------------------------------------------ */
static void test_switch_init(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    ASSERT_EQ(sw.num_ports, 0);
    ASSERT_EQ(sw.mac_aging, 300);
    ASSERT_EQ(sw.tick, 0);
    ASSERT_EQ(sw.total_rx, 0);
    ASSERT_EQ(sw.total_tx, 0);

    /* VLAN 1 should be active */
    ASSERT_EQ(sw.vlans[0].active, 1);
    ASSERT_EQ(sw.vlans[0].vlan_id, 1);
}

/* ------------------------------------------------------------------ */
/*  Test 2: Add port access                                            */
/* ------------------------------------------------------------------ */
static void test_add_port_access(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    int id = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);
    ASSERT_EQ(id, 0);
    ASSERT_EQ(sw.num_ports, 1);
    ASSERT_EQ(sw.ports[0].mode, NFS_SW_PORT_ACCESS);
    ASSERT_EQ(sw.ports[0].pvid, 1);
    ASSERT_EQ(sw.ports[0].active, 1);
    ASSERT_EQ(sw.ports[0].mirror_dst, -1);
}

/* ------------------------------------------------------------------ */
/*  Test 3: Add port trunk                                             */
/* ------------------------------------------------------------------ */
static void test_add_port_trunk(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    int id = nfs_sw_add_port(&sw, NFS_SW_PORT_TRUNK, 1);
    ASSERT_EQ(id, 0);
    ASSERT_EQ(sw.ports[0].mode, NFS_SW_PORT_TRUNK);
}

/* ------------------------------------------------------------------ */
/*  Test 4: Max ports                                                  */
/* ------------------------------------------------------------------ */
static void test_max_ports(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    for (int i = 0; i < NFS_SW_MAX_PORTS; i++) {
        int id = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);
        ASSERT_EQ(id, i);
    }
    ASSERT_EQ(sw.num_ports, NFS_SW_MAX_PORTS);

    /* 33rd port should fail */
    int id = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);
    ASSERT_EQ(id, -1);
}

/* ------------------------------------------------------------------ */
/*  Test 5: VLAN create                                                */
/* ------------------------------------------------------------------ */
static void test_vlan_create(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    ASSERT_EQ(nfs_sw_vlan_create(&sw, 10, "engineering"), 0);

    /* Find the VLAN */
    int found = 0;
    for (int i = 0; i < NFS_SW_MAX_VLANS; i++) {
        if (sw.vlans[i].active && sw.vlans[i].vlan_id == 10) {
            found = 1;
            ASSERT_TRUE(strcmp(sw.vlans[i].name, "engineering") == 0);
        }
    }
    ASSERT_EQ(found, 1);
}

/* ------------------------------------------------------------------ */
/*  Test 6: VLAN delete                                                */
/* ------------------------------------------------------------------ */
static void test_vlan_delete(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    ASSERT_EQ(nfs_sw_vlan_create(&sw, 10, "test"), 0);
    ASSERT_EQ(nfs_sw_vlan_delete(&sw, 10), 0);

    /* Verify it's gone */
    for (int i = 0; i < NFS_SW_MAX_VLANS; i++) {
        if (sw.vlans[i].active) {
            ASSERT_TRUE(sw.vlans[i].vlan_id != 10);
        }
    }

    /* Delete non-existent VLAN should fail */
    ASSERT_EQ(nfs_sw_vlan_delete(&sw, 99), -1);
}

/* ------------------------------------------------------------------ */
/*  Test 7: VLAN duplicate                                             */
/* ------------------------------------------------------------------ */
static void test_vlan_duplicate(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    ASSERT_EQ(nfs_sw_vlan_create(&sw, 10, "first"), 0);
    ASSERT_EQ(nfs_sw_vlan_create(&sw, 10, "second"), -1);
}

/* ------------------------------------------------------------------ */
/*  Test 8: MAC learn and lookup                                       */
/* ------------------------------------------------------------------ */
static void test_mac_learn_lookup(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    ASSERT_EQ(nfs_sw_mac_learn(&sw, mac, 0, 1), 0);
    ASSERT_EQ(nfs_sw_mac_lookup(&sw, mac, 1), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 9: MAC per-VLAN isolation                                     */
/* ------------------------------------------------------------------ */
static void test_mac_per_vlan(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);
    nfs_sw_vlan_create(&sw, 2, "vlan2");
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 2);

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    ASSERT_EQ(nfs_sw_mac_learn(&sw, mac, 0, 1), 0);
    ASSERT_EQ(nfs_sw_mac_learn(&sw, mac, 1, 2), 0);

    /* Same MAC resolves to different ports depending on VLAN */
    ASSERT_EQ(nfs_sw_mac_lookup(&sw, mac, 1), 0);
    ASSERT_EQ(nfs_sw_mac_lookup(&sw, mac, 2), 1);

    /* Two separate entries */
    ASSERT_EQ(nfs_sw_mac_count(&sw), 2);
}

/* ------------------------------------------------------------------ */
/*  Test 10: MAC flush                                                 */
/* ------------------------------------------------------------------ */
static void test_mac_flush(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for (int i = 0; i < 5; i++) {
        mac[5] = (uint8_t)i;
        nfs_sw_mac_learn(&sw, mac, 0, 1);
    }
    ASSERT_EQ(nfs_sw_mac_count(&sw), 5);

    nfs_sw_mac_flush(&sw);
    ASSERT_EQ(nfs_sw_mac_count(&sw), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 11: MAC aging                                                 */
/* ------------------------------------------------------------------ */
static void test_mac_aging(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};

    /* Learn at tick 0 */
    sw.tick = 0;
    nfs_sw_mac_learn(&sw, mac, 0, 1);
    ASSERT_EQ(nfs_sw_mac_count(&sw), 1);

    /* Age at tick 299 — should NOT expire (aging=300) */
    sw.tick = 299;
    nfs_sw_mac_age(&sw);
    ASSERT_EQ(nfs_sw_mac_count(&sw), 1);

    /* Age at tick 300 — should expire */
    sw.tick = 300;
    nfs_sw_mac_age(&sw);
    ASSERT_EQ(nfs_sw_mac_count(&sw), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 12: Forward known unicast                                     */
/* ------------------------------------------------------------------ */
static void test_forward_known_unicast(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    int p0 = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);
    int p1 = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t mac_a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t mac_b[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x02};

    /* Pre-learn dst MAC on port 1 */
    nfs_sw_mac_learn(&sw, mac_b, p1, 1);

    /* Build frame from A to B */
    uint8_t frame[60];
    nfs_sw_build_frame(mac_b, mac_a, 0x0800, NULL, 0, frame, sizeof(frame));

    struct nfs_sw_result res;
    ASSERT_EQ(nfs_sw_process_frame(&sw, p0, frame, sizeof(frame), &res), 0);
    ASSERT_EQ(res.action, NFS_SW_FORWARD);
    ASSERT_EQ(res.dst_port, p1);
    ASSERT_EQ(res.vlan_id, 1);
}

/* ------------------------------------------------------------------ */
/*  Test 13: Forward unknown unicast                                   */
/* ------------------------------------------------------------------ */
static void test_forward_unknown_unicast(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t mac_a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t mac_b[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x02};

    uint8_t frame[60];
    nfs_sw_build_frame(mac_b, mac_a, 0x0800, NULL, 0, frame, sizeof(frame));

    struct nfs_sw_result res;
    ASSERT_EQ(nfs_sw_process_frame(&sw, 0, frame, sizeof(frame), &res), 0);
    ASSERT_EQ(res.action, NFS_SW_FLOOD);
    ASSERT_EQ(res.dst_port, -1);
}

/* ------------------------------------------------------------------ */
/*  Test 14: Forward broadcast                                         */
/* ------------------------------------------------------------------ */
static void test_forward_broadcast(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t mac_a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    uint8_t frame[60];
    nfs_sw_build_frame(bcast, mac_a, 0x0806, NULL, 0, frame, sizeof(frame));

    struct nfs_sw_result res;
    ASSERT_EQ(nfs_sw_process_frame(&sw, 0, frame, sizeof(frame), &res), 0);
    ASSERT_EQ(res.action, NFS_SW_FLOOD);
}

/* ------------------------------------------------------------------ */
/*  Test 15: Forward same port (hairpin)                               */
/* ------------------------------------------------------------------ */
static void test_forward_same_port(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    int p0 = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t mac_a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t mac_b[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x02};

    /* Learn dst MAC on the same port as ingress */
    nfs_sw_mac_learn(&sw, mac_b, p0, 1);

    uint8_t frame[60];
    nfs_sw_build_frame(mac_b, mac_a, 0x0800, NULL, 0, frame, sizeof(frame));

    struct nfs_sw_result res;
    ASSERT_EQ(nfs_sw_process_frame(&sw, p0, frame, sizeof(frame), &res), 0);
    ASSERT_EQ(res.action, NFS_SW_DROP);
}

/* ------------------------------------------------------------------ */
/*  Test 16: Source learning on forward                                */
/* ------------------------------------------------------------------ */
static void test_source_learning(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t mac_a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t mac_b[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x02};

    /* Before frame: src MAC not in table */
    ASSERT_EQ(nfs_sw_mac_lookup(&sw, mac_a, 1), -1);

    /* Send frame from port 0 */
    uint8_t frame[60];
    nfs_sw_build_frame(mac_b, mac_a, 0x0800, NULL, 0, frame, sizeof(frame));

    struct nfs_sw_result res;
    nfs_sw_process_frame(&sw, 0, frame, sizeof(frame), &res);

    /* After frame: src MAC should be learned on port 0 */
    ASSERT_EQ(nfs_sw_mac_lookup(&sw, mac_a, 1), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 17: Access port assigns PVID                                  */
/* ------------------------------------------------------------------ */
static void test_access_port_pvid(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);
    nfs_sw_vlan_create(&sw, 10, "vlan10");

    /* Port with PVID=10 */
    int p0 = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 10);
    nfs_sw_port_allow_vlan(&sw, p0, 10);

    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 10);
    nfs_sw_port_allow_vlan(&sw, 1, 10);

    uint8_t mac_a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t mac_b[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x02};

    uint8_t frame[60];
    nfs_sw_build_frame(mac_b, mac_a, 0x0800, NULL, 0, frame, sizeof(frame));

    struct nfs_sw_result res;
    ASSERT_EQ(nfs_sw_process_frame(&sw, p0, frame, sizeof(frame), &res), 0);
    ASSERT_EQ(res.vlan_id, 10);
}

/* ------------------------------------------------------------------ */
/*  Test 18: Trunk port VLAN check                                     */
/* ------------------------------------------------------------------ */
static void test_trunk_vlan_check(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);
    nfs_sw_vlan_create(&sw, 10, "vlan10");
    nfs_sw_vlan_create(&sw, 20, "vlan20");

    /* Trunk port that only allows VLAN 10 */
    int p0 = nfs_sw_add_port(&sw, NFS_SW_PORT_TRUNK, 1);
    nfs_sw_port_allow_vlan(&sw, p0, 10);

    /* Build a tagged frame with VLAN 20 (not allowed) */
    uint8_t mac_a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t mac_b[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x02};

    /* Manually build 802.1Q tagged frame */
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    memcpy(frame, mac_b, 6);     /* dst */
    memcpy(frame + 6, mac_a, 6); /* src */
    frame[12] = 0x81;
    frame[13] = 0x00; /* 802.1Q ethertype */
    frame[14] = 0x00;
    frame[15] = 20; /* VLAN 20 */
    frame[16] = 0x08;
    frame[17] = 0x00; /* inner ethertype: IPv4 */

    struct nfs_sw_result res;
    ASSERT_EQ(nfs_sw_process_frame(&sw, p0, frame, sizeof(frame), &res), 0);
    ASSERT_EQ(res.action, NFS_SW_DROP);
}

/* ------------------------------------------------------------------ */
/*  Test 19: Station move                                              */
/* ------------------------------------------------------------------ */
static void test_station_move(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};

    /* Learn on port 0 */
    nfs_sw_mac_learn(&sw, mac, 0, 1);
    ASSERT_EQ(nfs_sw_mac_lookup(&sw, mac, 1), 0);

    /* Move to port 1 */
    nfs_sw_mac_learn(&sw, mac, 1, 1);
    ASSERT_EQ(nfs_sw_mac_lookup(&sw, mac, 1), 1);

    /* Should still be one entry, not two */
    ASSERT_EQ(nfs_sw_mac_count(&sw), 1);
}

/* ------------------------------------------------------------------ */
/*  Test 20: Frame too short                                           */
/* ------------------------------------------------------------------ */
static void test_frame_too_short(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t frame[13];
    memset(frame, 0, sizeof(frame));

    struct nfs_sw_result res;
    ASSERT_EQ(nfs_sw_process_frame(&sw, 0, frame, sizeof(frame), &res), -1);
    ASSERT_EQ(nfs_sw_process_frame(&sw, 0, frame, 0, &res), -1);
}

/* ------------------------------------------------------------------ */
/*  Test 21: Stats tracking                                            */
/* ------------------------------------------------------------------ */
static void test_stats_tracking(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    int p0 = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);
    int p1 = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t mac_a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t mac_b[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x02};

    /* First frame: unknown unicast → flood */
    uint8_t frame[60];
    nfs_sw_build_frame(mac_b, mac_a, 0x0800, NULL, 0, frame, sizeof(frame));

    struct nfs_sw_result res;
    nfs_sw_process_frame(&sw, p0, frame, sizeof(frame), &res);
    ASSERT_EQ(sw.total_rx, 1);
    ASSERT_EQ(sw.total_flooded, 1);

    /* Pre-learn mac_a on port 0, then send from port 1 to port 0 */
    nfs_sw_build_frame(mac_a, mac_b, 0x0800, NULL, 0, frame, sizeof(frame));
    nfs_sw_process_frame(&sw, p1, frame, sizeof(frame), &res);
    ASSERT_EQ(sw.total_rx, 2);
    ASSERT_EQ(sw.total_forwarded, 1);

    /* Same port drop */
    nfs_sw_build_frame(mac_a, mac_b, 0x0800, NULL, 0, frame, sizeof(frame));
    nfs_sw_process_frame(&sw, p0, frame, sizeof(frame), &res);
    ASSERT_EQ(sw.total_rx, 3);
    ASSERT_EQ(sw.total_dropped, 1);
}

/* ------------------------------------------------------------------ */
/*  Test 22: Port stats                                                */
/* ------------------------------------------------------------------ */
static void test_port_stats(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);

    int p0 = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);
    int p1 = nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    uint8_t mac_a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t mac_b[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x02};

    /* Pre-learn mac_b on port 1 */
    nfs_sw_mac_learn(&sw, mac_b, p1, 1);

    uint8_t frame[60];
    nfs_sw_build_frame(mac_b, mac_a, 0x0800, NULL, 0, frame, sizeof(frame));

    struct nfs_sw_result res;
    nfs_sw_process_frame(&sw, p0, frame, sizeof(frame), &res);

    ASSERT_EQ(sw.ports[p0].rx_frames, 1);
    ASSERT_EQ(sw.ports[p1].tx_frames, 1);
}

/* ------------------------------------------------------------------ */
/*  Test 23: Build test frame                                          */
/* ------------------------------------------------------------------ */
static void test_build_frame(void) {
    uint8_t dst[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    uint8_t src[6] = {0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x02};
    uint8_t payload[] = {0x45, 0x00};

    uint8_t out[128];
    int len = nfs_sw_build_frame(dst, src, 0x0800, payload, sizeof(payload), out, sizeof(out));

    /* Minimum 60 bytes */
    ASSERT_EQ(len, 60);

    /* Verify dst MAC */
    ASSERT_TRUE(memcmp(out, dst, 6) == 0);
    /* Verify src MAC */
    ASSERT_TRUE(memcmp(out + 6, src, 6) == 0);
    /* Verify ethertype (network byte order) */
    ASSERT_EQ(out[12], 0x08);
    ASSERT_EQ(out[13], 0x00);
    /* Verify payload */
    ASSERT_EQ(out[14], 0x45);
    ASSERT_EQ(out[15], 0x00);

    /* Buffer too small */
    uint8_t tiny[20];
    ASSERT_EQ(nfs_sw_build_frame(dst, src, 0x0800, NULL, 0, tiny, sizeof(tiny)), -1);
}

/* ------------------------------------------------------------------ */
/*  Test 24: MAC count                                                 */
/* ------------------------------------------------------------------ */
static void test_mac_count(void) {
    struct nfs_l2switch sw;
    nfs_sw_init(&sw);
    nfs_sw_add_port(&sw, NFS_SW_PORT_ACCESS, 1);

    ASSERT_EQ(nfs_sw_mac_count(&sw), 0);

    uint8_t mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    int n = 10;
    for (int i = 0; i < n; i++) {
        mac[5] = (uint8_t)i;
        nfs_sw_mac_learn(&sw, mac, 0, 1);
    }
    ASSERT_EQ(nfs_sw_mac_count(&sw), n);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_switch_init();
    test_add_port_access();
    test_add_port_trunk();
    test_max_ports();
    test_vlan_create();
    test_vlan_delete();
    test_vlan_duplicate();
    test_mac_learn_lookup();
    test_mac_per_vlan();
    test_mac_flush();
    test_mac_aging();
    test_forward_known_unicast();
    test_forward_unknown_unicast();
    test_forward_broadcast();
    test_forward_same_port();
    test_source_learning();
    test_access_port_pvid();
    test_trunk_vlan_check();
    test_station_move();
    test_frame_too_short();
    test_stats_tracking();
    test_port_stats();
    test_build_frame();
    test_mac_count();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
