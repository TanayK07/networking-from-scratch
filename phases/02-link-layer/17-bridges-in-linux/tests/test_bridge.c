/* Unit tests for Linux bridge forwarding simulation. */

#include "../bridge.h"

#include <stdio.h>
#include <stdlib.h>
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

/* Helper: build a minimal 14-byte Ethernet frame. */
static void make_frame(uint8_t *frame, const uint8_t dst[6], const uint8_t src[6]) {
    memcpy(frame, dst, 6);
    memcpy(frame + 6, src, 6);
    frame[12] = 0x08;
    frame[13] = 0x00; /* IPv4 */
}

/* ------------------------------------------------------------------ */
/*  Test 1: Bridge init                                                */
/* ------------------------------------------------------------------ */
static void test_bridge_init(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");
    ASSERT_TRUE(strcmp(br.name, "br0") == 0);
    ASSERT_EQ(br.num_ports, 0);
    ASSERT_EQ(br.aging_time, 300);
    ASSERT_EQ(br.forwarded, 0);
    ASSERT_EQ(br.flooded, 0);
    ASSERT_EQ(br.dropped, 0);
    ASSERT_EQ(br.learned, 0);
    ASSERT_EQ(br.clock, 0);
}

/* ------------------------------------------------------------------ */
/*  Test 2: Add port                                                   */
/* ------------------------------------------------------------------ */
static void test_add_port(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");
    uint8_t mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    int id = nfs_bridge_add_port(&br, "eth0", mac);
    ASSERT_EQ(id, 0);
    ASSERT_EQ(br.num_ports, 1);
    ASSERT_TRUE(br.ports[0].active == 1);
    ASSERT_TRUE(strcmp(br.ports[0].name, "eth0") == 0);
    ASSERT_TRUE(memcmp(br.ports[0].mac, mac, 6) == 0);
}

/* ------------------------------------------------------------------ */
/*  Test 3: Add max ports                                              */
/* ------------------------------------------------------------------ */
static void test_add_max_ports(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");
    uint8_t mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    for (int i = 0; i < NFS_BRIDGE_MAX_PORTS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "eth%d", i);
        mac[5] = (uint8_t)i;
        int id = nfs_bridge_add_port(&br, name, mac);
        ASSERT_EQ(id, i);
    }
    ASSERT_EQ(br.num_ports, NFS_BRIDGE_MAX_PORTS);

    /* 17th port should fail */
    mac[5] = 0xFF;
    ASSERT_EQ(nfs_bridge_add_port(&br, "eth16", mac), -1);
}

/* ------------------------------------------------------------------ */
/*  Test 4: Remove port                                                */
/* ------------------------------------------------------------------ */
static void test_remove_port(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");
    uint8_t mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    int id = nfs_bridge_add_port(&br, "eth0", mac);
    ASSERT_EQ(id, 0);
    ASSERT_EQ(nfs_bridge_remove_port(&br, 0), 0);
    ASSERT_EQ(br.ports[0].active, 0);

    /* Removing again should fail */
    ASSERT_EQ(nfs_bridge_remove_port(&br, 0), -1);

    /* Invalid port */
    ASSERT_EQ(nfs_bridge_remove_port(&br, 99), -1);
}

/* ------------------------------------------------------------------ */
/*  Test 5: FDB learn                                                  */
/* ------------------------------------------------------------------ */
static void test_fdb_learn(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    ASSERT_EQ(nfs_fdb_learn(&br, mac, 0), 0);
    ASSERT_EQ(nfs_fdb_lookup(&br, mac), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 6: FDB learn update (station moved)                           */
/* ------------------------------------------------------------------ */
static void test_fdb_learn_update(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02};
    ASSERT_EQ(nfs_fdb_learn(&br, mac, 0), 0);
    ASSERT_EQ(nfs_fdb_lookup(&br, mac), 0);

    /* Same MAC, different port */
    ASSERT_EQ(nfs_fdb_learn(&br, mac, 3), 0);
    ASSERT_EQ(nfs_fdb_lookup(&br, mac), 3);

    /* Should not create a second entry */
    ASSERT_EQ(nfs_fdb_count(&br), 1);
}

/* ------------------------------------------------------------------ */
/*  Test 7: FDB lookup miss                                            */
/* ------------------------------------------------------------------ */
static void test_fdb_lookup_miss(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");
    uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    ASSERT_EQ(nfs_fdb_lookup(&br, mac), -1);
}

/* ------------------------------------------------------------------ */
/*  Test 8: FDB count                                                  */
/* ------------------------------------------------------------------ */
static void test_fdb_count(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    for (int i = 0; i < 5; i++) {
        uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, (uint8_t)i};
        nfs_fdb_learn(&br, mac, i % 4);
    }
    ASSERT_EQ(nfs_fdb_count(&br), 5);
}

/* ------------------------------------------------------------------ */
/*  Test 9: FDB flush                                                  */
/* ------------------------------------------------------------------ */
static void test_fdb_flush(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    for (int i = 0; i < 3; i++) {
        uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, (uint8_t)i};
        nfs_fdb_learn(&br, mac, 0);
    }
    ASSERT_EQ(nfs_fdb_count(&br), 3);

    nfs_fdb_flush(&br);
    ASSERT_EQ(nfs_fdb_count(&br), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 10: FDB aging removes old entries                             */
/* ------------------------------------------------------------------ */
static void test_fdb_aging_removes(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");
    br.clock = 0;

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01};
    nfs_fdb_learn(&br, mac, 0);
    ASSERT_EQ(nfs_fdb_count(&br), 1);

    /* Age at t=301 (> aging_time=300) → should be removed */
    nfs_fdb_age(&br, 301);
    ASSERT_EQ(nfs_fdb_count(&br), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 11: FDB aging keeps fresh entries                             */
/* ------------------------------------------------------------------ */
static void test_fdb_aging_keeps_fresh(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");
    br.clock = 0;

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x02};
    nfs_fdb_learn(&br, mac, 0);
    ASSERT_EQ(nfs_fdb_count(&br), 1);

    /* Age at t=100 (< aging_time=300) → should stay */
    nfs_fdb_age(&br, 100);
    ASSERT_EQ(nfs_fdb_count(&br), 1);
}

/* ------------------------------------------------------------------ */
/*  Test 12: FDB static survives flush                                 */
/* ------------------------------------------------------------------ */
static void test_fdb_static_survives_flush(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t smac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t dmac[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01};
    nfs_fdb_add_static(&br, smac, 1);
    nfs_fdb_learn(&br, dmac, 2);
    ASSERT_EQ(nfs_fdb_count(&br), 2);

    nfs_fdb_flush(&br);
    ASSERT_EQ(nfs_fdb_count(&br), 1);
    ASSERT_EQ(nfs_fdb_lookup(&br, smac), 1);
    ASSERT_EQ(nfs_fdb_lookup(&br, dmac), -1);
}

/* ------------------------------------------------------------------ */
/*  Test 13: FDB static survives aging                                 */
/* ------------------------------------------------------------------ */
static void test_fdb_static_survives_aging(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");
    br.clock = 0;

    uint8_t smac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x77};
    nfs_fdb_add_static(&br, smac, 1);
    ASSERT_EQ(nfs_fdb_count(&br), 1);

    /* Age well past aging_time — static entry stays */
    nfs_fdb_age(&br, 999);
    ASSERT_EQ(nfs_fdb_count(&br), 1);
    ASSERT_EQ(nfs_fdb_lookup(&br, smac), 1);
}

/* ------------------------------------------------------------------ */
/*  Test 14: Forward known unicast                                     */
/* ------------------------------------------------------------------ */
static void test_forward_known_unicast(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t pmac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    nfs_bridge_add_port(&br, "eth0", pmac);
    pmac[5] = 1;
    nfs_bridge_add_port(&br, "eth1", pmac);
    pmac[5] = 2;
    nfs_bridge_add_port(&br, "eth2", pmac);

    /* Pre-learn dst MAC on port 2 */
    uint8_t dst[6] = {0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01};
    nfs_fdb_learn(&br, dst, 2);

    uint8_t src[6] = {0xAA, 0xAA, 0xAA, 0x00, 0x00, 0x01};
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    make_frame(frame, dst, src);

    int action = nfs_bridge_forward(&br, 0, frame, sizeof(frame));
    ASSERT_EQ(action, NFS_BRIDGE_FORWARD);
}

/* ------------------------------------------------------------------ */
/*  Test 15: Forward unknown unicast                                   */
/* ------------------------------------------------------------------ */
static void test_forward_unknown_unicast(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t pmac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    nfs_bridge_add_port(&br, "eth0", pmac);

    uint8_t dst[6] = {0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x99};
    uint8_t src[6] = {0xAA, 0xAA, 0xAA, 0x00, 0x00, 0x02};
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    make_frame(frame, dst, src);

    int action = nfs_bridge_forward(&br, 0, frame, sizeof(frame));
    ASSERT_EQ(action, NFS_BRIDGE_FLOOD);
}

/* ------------------------------------------------------------------ */
/*  Test 16: Forward broadcast                                         */
/* ------------------------------------------------------------------ */
static void test_forward_broadcast(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t pmac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    nfs_bridge_add_port(&br, "eth0", pmac);

    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t src[6] = {0xAA, 0xAA, 0xAA, 0x00, 0x00, 0x03};
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    make_frame(frame, bcast, src);

    int action = nfs_bridge_forward(&br, 0, frame, sizeof(frame));
    ASSERT_EQ(action, NFS_BRIDGE_FLOOD);
}

/* ------------------------------------------------------------------ */
/*  Test 17: Forward same port (hairpin drop)                          */
/* ------------------------------------------------------------------ */
static void test_forward_same_port(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t pmac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    nfs_bridge_add_port(&br, "eth0", pmac);

    /* Pre-learn dst MAC on port 0 (same as ingress) */
    uint8_t dst[6] = {0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x02};
    nfs_fdb_learn(&br, dst, 0);

    uint8_t src[6] = {0xAA, 0xAA, 0xAA, 0x00, 0x00, 0x04};
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    make_frame(frame, dst, src);

    int action = nfs_bridge_forward(&br, 0, frame, sizeof(frame));
    ASSERT_EQ(action, NFS_BRIDGE_DROP);
}

/* ------------------------------------------------------------------ */
/*  Test 18: Forward learns source                                     */
/* ------------------------------------------------------------------ */
static void test_forward_learns_source(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t pmac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    nfs_bridge_add_port(&br, "eth0", pmac);
    pmac[5] = 1;
    nfs_bridge_add_port(&br, "eth1", pmac);

    uint8_t dst[6] = {0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x03};
    uint8_t src[6] = {0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x01};
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    make_frame(frame, dst, src);

    /* src MAC should not be in FDB yet */
    ASSERT_EQ(nfs_fdb_lookup(&br, src), -1);

    nfs_bridge_forward(&br, 0, frame, sizeof(frame));

    /* After forwarding, src MAC should be learned on port 0 */
    ASSERT_EQ(nfs_fdb_lookup(&br, src), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 19: Forward too short                                         */
/* ------------------------------------------------------------------ */
static void test_forward_too_short(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t frame[13] = {0};
    int action = nfs_bridge_forward(&br, 0, frame, sizeof(frame));
    ASSERT_EQ(action, NFS_BRIDGE_DROP);
}

/* ------------------------------------------------------------------ */
/*  Test 20: Forward port returns correct port                         */
/* ------------------------------------------------------------------ */
static void test_forward_port_returns_port(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t pmac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for (int i = 0; i < 4; i++) {
        char name[16];
        snprintf(name, sizeof(name), "eth%d", i);
        pmac[5] = (uint8_t)i;
        nfs_bridge_add_port(&br, name, pmac);
    }

    /* Pre-learn dst MAC on port 3 */
    uint8_t dst[6] = {0xDD, 0xEE, 0xFF, 0x00, 0x00, 0x03};
    nfs_fdb_learn(&br, dst, 3);

    uint8_t src[6] = {0xCC, 0xDD, 0xEE, 0x00, 0x00, 0x01};
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    make_frame(frame, dst, src);

    int port = nfs_bridge_forward_port(&br, 0, frame, sizeof(frame));
    ASSERT_EQ(port, 3);
}

/* ------------------------------------------------------------------ */
/*  Test 21: Stats forwarded count                                     */
/* ------------------------------------------------------------------ */
static void test_stats_forwarded(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t pmac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    nfs_bridge_add_port(&br, "eth0", pmac);
    pmac[5] = 1;
    nfs_bridge_add_port(&br, "eth1", pmac);

    uint8_t dst[6] = {0xBB, 0xCC, 0xDD, 0x00, 0x00, 0x01};
    nfs_fdb_learn(&br, dst, 1);

    uint8_t src[6] = {0xAA, 0xAA, 0xAA, 0x00, 0x00, 0x05};
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    make_frame(frame, dst, src);

    nfs_bridge_forward(&br, 0, frame, sizeof(frame));
    ASSERT_EQ(br.forwarded, 1);
}

/* ------------------------------------------------------------------ */
/*  Test 22: Stats flooded count                                       */
/* ------------------------------------------------------------------ */
static void test_stats_flooded(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t pmac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    nfs_bridge_add_port(&br, "eth0", pmac);

    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t src[6] = {0xAA, 0xAA, 0xAA, 0x00, 0x00, 0x06};
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    make_frame(frame, bcast, src);

    nfs_bridge_forward(&br, 0, frame, sizeof(frame));
    ASSERT_EQ(br.flooded, 1);
}

/* ------------------------------------------------------------------ */
/*  Test 23: Stats learned count                                       */
/* ------------------------------------------------------------------ */
static void test_stats_learned(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t mac1[6] = {0xAA, 0x00, 0x00, 0x00, 0x00, 0x01};
    uint8_t mac2[6] = {0xAA, 0x00, 0x00, 0x00, 0x00, 0x02};
    uint8_t mac3[6] = {0xAA, 0x00, 0x00, 0x00, 0x00, 0x03};

    nfs_fdb_learn(&br, mac1, 0);
    nfs_fdb_learn(&br, mac2, 1);
    nfs_fdb_learn(&br, mac3, 2);

    ASSERT_EQ(br.learned, 3);

    /* Re-learning same MAC should not increment */
    nfs_fdb_learn(&br, mac1, 1);
    ASSERT_EQ(br.learned, 3);
}

/* ------------------------------------------------------------------ */
/*  Test 24: Station move                                              */
/* ------------------------------------------------------------------ */
static void test_station_move(void) {
    struct nfs_bridge br;
    nfs_bridge_init(&br, "br0");

    uint8_t pmac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    nfs_bridge_add_port(&br, "eth0", pmac);
    pmac[5] = 1;
    nfs_bridge_add_port(&br, "eth1", pmac);

    /* Host MAC initially on port 0 */
    uint8_t host[6] = {0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x99};
    uint8_t dst[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    make_frame(frame, dst, host);

    /* Frame from host on port 0 */
    nfs_bridge_forward(&br, 0, frame, sizeof(frame));
    ASSERT_EQ(nfs_fdb_lookup(&br, host), 0);

    /* Same host now sends from port 1 (station moved) */
    nfs_bridge_forward(&br, 1, frame, sizeof(frame));
    ASSERT_EQ(nfs_fdb_lookup(&br, host), 1);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_bridge_init();
    test_add_port();
    test_add_max_ports();
    test_remove_port();
    test_fdb_learn();
    test_fdb_learn_update();
    test_fdb_lookup_miss();
    test_fdb_count();
    test_fdb_flush();
    test_fdb_aging_removes();
    test_fdb_aging_keeps_fresh();
    test_fdb_static_survives_flush();
    test_fdb_static_survives_aging();
    test_forward_known_unicast();
    test_forward_unknown_unicast();
    test_forward_broadcast();
    test_forward_same_port();
    test_forward_learns_source();
    test_forward_too_short();
    test_forward_port_returns_port();
    test_stats_forwarded();
    test_stats_flooded();
    test_stats_learned();
    test_station_move();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
