/* Unit tests for promiscuous mode frame filtering. */

#include "../promisc.h"

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

/* Helper: build a 60-byte Ethernet frame with given dst and src MACs. */
static void build_frame(uint8_t frame[60], const uint8_t dst[6], const uint8_t src[6]) {
    memset(frame, 0, 60);
    memcpy(frame, dst, 6);
    memcpy(frame + 6, src, 6);
    frame[12] = 0x08;
    frame[13] = 0x00; /* IPv4 */
}

/* Known MACs used across tests. */
static const uint8_t MY_MAC[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
static const uint8_t OTHER_MAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t MCAST[6] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};
static const uint8_t SENDER[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};

/* ------------------------------------------------------------------ */
/*  1. Normal mode: accept unicast to us                               */
/* ------------------------------------------------------------------ */
static void test_normal_accept_unicast_to_us(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 0);

    uint8_t frame[60];
    build_frame(frame, MY_MAC, SENDER);
    ASSERT_EQ(nfs_filter_accept(&f, frame, 60), 1);
}

/* ------------------------------------------------------------------ */
/*  2. Normal mode: reject unicast to others                           */
/* ------------------------------------------------------------------ */
static void test_normal_reject_unicast_to_others(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 0);

    uint8_t frame[60];
    build_frame(frame, OTHER_MAC, SENDER);
    ASSERT_EQ(nfs_filter_accept(&f, frame, 60), 0);
}

/* ------------------------------------------------------------------ */
/*  3. Normal mode: accept broadcast                                   */
/* ------------------------------------------------------------------ */
static void test_normal_accept_broadcast(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 0);

    uint8_t frame[60];
    build_frame(frame, BCAST, SENDER);
    ASSERT_EQ(nfs_filter_accept(&f, frame, 60), 1);
}

/* ------------------------------------------------------------------ */
/*  4. Normal mode: accept multicast                                   */
/* ------------------------------------------------------------------ */
static void test_normal_accept_multicast(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 0);

    uint8_t frame[60];
    build_frame(frame, MCAST, SENDER);
    ASSERT_EQ(nfs_filter_accept(&f, frame, 60), 1);
}

/* ------------------------------------------------------------------ */
/*  5. Promiscuous mode: accept unicast to others                      */
/* ------------------------------------------------------------------ */
static void test_promisc_accept_unicast_to_others(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 1);

    uint8_t frame[60];
    build_frame(frame, OTHER_MAC, SENDER);
    ASSERT_EQ(nfs_filter_accept(&f, frame, 60), 1);
}

/* ------------------------------------------------------------------ */
/*  6. Promiscuous mode: accept everything                             */
/* ------------------------------------------------------------------ */
static void test_promisc_accept_everything(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 1);

    uint8_t frame[60];

    build_frame(frame, MY_MAC, SENDER);
    ASSERT_EQ(nfs_filter_accept(&f, frame, 60), 1);

    build_frame(frame, OTHER_MAC, SENDER);
    ASSERT_EQ(nfs_filter_accept(&f, frame, 60), 1);

    build_frame(frame, BCAST, SENDER);
    ASSERT_EQ(nfs_filter_accept(&f, frame, 60), 1);

    build_frame(frame, MCAST, SENDER);
    ASSERT_EQ(nfs_filter_accept(&f, frame, 60), 1);

    ASSERT_EQ(f.accepted, 4);
    ASSERT_EQ(f.rejected, 0);
}

/* ------------------------------------------------------------------ */
/*  7. Frame too short                                                 */
/* ------------------------------------------------------------------ */
static void test_frame_too_short(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 0);

    uint8_t frame[13];
    memset(frame, 0, sizeof(frame));
    ASSERT_EQ(nfs_filter_accept(&f, frame, 13), -1);
    ASSERT_EQ(nfs_filter_accept(&f, frame, 0), -1);

    /* total_seen should NOT increment for short frames */
    ASSERT_EQ(f.total_seen, 0);
}

/* ------------------------------------------------------------------ */
/*  8. Stats: total_seen increments                                    */
/* ------------------------------------------------------------------ */
static void test_stats_total_seen(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 1); /* promisc so all accepted */

    uint8_t frame[60];
    build_frame(frame, MY_MAC, SENDER);

    for (int i = 0; i < 5; i++)
        nfs_filter_accept(&f, frame, 60);

    ASSERT_EQ(f.total_seen, 5);
}

/* ------------------------------------------------------------------ */
/*  9. Stats: accepted/rejected counts                                 */
/* ------------------------------------------------------------------ */
static void test_stats_accepted_rejected(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 0);

    uint8_t frame[60];

    /* 2 accepted (unicast to us) */
    build_frame(frame, MY_MAC, SENDER);
    nfs_filter_accept(&f, frame, 60);
    nfs_filter_accept(&f, frame, 60);

    /* 3 rejected (unicast to other) */
    build_frame(frame, OTHER_MAC, SENDER);
    nfs_filter_accept(&f, frame, 60);
    nfs_filter_accept(&f, frame, 60);
    nfs_filter_accept(&f, frame, 60);

    ASSERT_EQ(f.total_seen, 5);
    ASSERT_EQ(f.accepted, 2);
    ASSERT_EQ(f.rejected, 3);
}

/* ------------------------------------------------------------------ */
/*  10. Stats: broadcast_count                                         */
/* ------------------------------------------------------------------ */
static void test_stats_broadcast_count(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 0);

    uint8_t frame[60];
    build_frame(frame, BCAST, SENDER);

    for (int i = 0; i < 3; i++)
        nfs_filter_accept(&f, frame, 60);

    ASSERT_EQ(f.broadcast_count, 3);
}

/* ------------------------------------------------------------------ */
/*  11. Stats: multicast_count                                         */
/* ------------------------------------------------------------------ */
static void test_stats_multicast_count(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 0);

    uint8_t frame[60];
    build_frame(frame, MCAST, SENDER);

    nfs_filter_accept(&f, frame, 60);
    nfs_filter_accept(&f, frame, 60);

    ASSERT_EQ(f.multicast_count, 2);
}

/* ------------------------------------------------------------------ */
/*  12. Stats: unicast_count                                           */
/* ------------------------------------------------------------------ */
static void test_stats_unicast_count(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 0);

    uint8_t frame[60];
    build_frame(frame, MY_MAC, SENDER);

    nfs_filter_accept(&f, frame, 60);
    nfs_filter_accept(&f, frame, 60);
    nfs_filter_accept(&f, frame, 60);

    ASSERT_EQ(f.unicast_count, 3);
}

/* ------------------------------------------------------------------ */
/*  13. Stats: other_unicast in promisc                                */
/* ------------------------------------------------------------------ */
static void test_stats_other_unicast_promisc(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 1);

    uint8_t frame[60];
    build_frame(frame, OTHER_MAC, SENDER);

    nfs_filter_accept(&f, frame, 60);
    nfs_filter_accept(&f, frame, 60);

    ASSERT_EQ(f.other_unicast, 2);
    ASSERT_EQ(f.accepted, 2);
}

/* ------------------------------------------------------------------ */
/*  14. Stats reset                                                    */
/* ------------------------------------------------------------------ */
static void test_stats_reset(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 1);

    uint8_t frame[60];
    build_frame(frame, MY_MAC, SENDER);
    nfs_filter_accept(&f, frame, 60);
    build_frame(frame, BCAST, SENDER);
    nfs_filter_accept(&f, frame, 60);
    build_frame(frame, OTHER_MAC, SENDER);
    nfs_filter_accept(&f, frame, 60);

    ASSERT_TRUE(f.total_seen > 0);

    nfs_filter_reset_stats(&f);

    ASSERT_EQ(f.total_seen, 0);
    ASSERT_EQ(f.accepted, 0);
    ASSERT_EQ(f.rejected, 0);
    ASSERT_EQ(f.broadcast_count, 0);
    ASSERT_EQ(f.multicast_count, 0);
    ASSERT_EQ(f.unicast_count, 0);
    ASSERT_EQ(f.other_unicast, 0);

    /* Mode and MAC should be unchanged */
    ASSERT_EQ(f.promiscuous, 1);
    ASSERT_TRUE(nfs_mac_equal(f.local_mac, MY_MAC));
}

/* ------------------------------------------------------------------ */
/*  15. MAC is_broadcast                                               */
/* ------------------------------------------------------------------ */
static void test_mac_is_broadcast(void) {
    ASSERT_EQ(nfs_mac_is_broadcast(BCAST), 1);
    ASSERT_EQ(nfs_mac_is_broadcast(MY_MAC), 0);
    ASSERT_EQ(nfs_mac_is_broadcast(MCAST), 0);

    uint8_t almost[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};
    ASSERT_EQ(nfs_mac_is_broadcast(almost), 0);
}

/* ------------------------------------------------------------------ */
/*  16. MAC is_multicast                                               */
/* ------------------------------------------------------------------ */
static void test_mac_is_multicast(void) {
    ASSERT_EQ(nfs_mac_is_multicast(MCAST), 1);

    uint8_t mcast2[6] = {0x33, 0x33, 0x00, 0x00, 0x00, 0x01};
    ASSERT_EQ(nfs_mac_is_multicast(mcast2), 1);

    /* Unicast: bit 0 clear */
    ASSERT_EQ(nfs_mac_is_multicast(MY_MAC), 0);

    /* Broadcast is NOT multicast */
    ASSERT_EQ(nfs_mac_is_multicast(BCAST), 0);
}

/* ------------------------------------------------------------------ */
/*  17. MAC is_unicast                                                 */
/* ------------------------------------------------------------------ */
static void test_mac_is_unicast(void) {
    ASSERT_EQ(nfs_mac_is_unicast(MY_MAC), 1);

    uint8_t zero[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ASSERT_EQ(nfs_mac_is_unicast(zero), 1);

    /* Multicast is not unicast */
    ASSERT_EQ(nfs_mac_is_unicast(MCAST), 0);

    /* Broadcast has bit 0 set, so not unicast */
    ASSERT_EQ(nfs_mac_is_unicast(BCAST), 0);
}

/* ------------------------------------------------------------------ */
/*  18. MAC equal                                                      */
/* ------------------------------------------------------------------ */
static void test_mac_equal(void) {
    ASSERT_EQ(nfs_mac_equal(MY_MAC, MY_MAC), 1);
    ASSERT_EQ(nfs_mac_equal(MY_MAC, OTHER_MAC), 0);
    ASSERT_EQ(nfs_mac_equal(BCAST, BCAST), 1);

    uint8_t a[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    ASSERT_EQ(nfs_mac_equal(a, MY_MAC), 1);

    a[5] = 0x56;
    ASSERT_EQ(nfs_mac_equal(a, MY_MAC), 0);
}

/* ------------------------------------------------------------------ */
/*  19. IFF_PROMISC constant                                           */
/* ------------------------------------------------------------------ */
static void test_iff_promisc_constant(void) {
    ASSERT_EQ(NFS_IFF_PROMISC, 0x100);
    ASSERT_EQ(NFS_PACKET_MR_PROMISC, 1);
}

/* ------------------------------------------------------------------ */
/*  20. Packet config init                                             */
/* ------------------------------------------------------------------ */
static void test_packet_cfg_init(void) {
    struct nfs_packet_cfg cfg;
    nfs_packet_cfg_init(&cfg, 42, 0x0003, 1);

    ASSERT_EQ(cfg.ifindex, 42);
    ASSERT_EQ(cfg.protocol, 0x0003);
    ASSERT_EQ(cfg.promiscuous, 1);
    ASSERT_EQ(cfg.socket_type, 3); /* SOCK_RAW */
}

/* ------------------------------------------------------------------ */
/*  21. Normal mode batch: 100 mixed frames, verify exact counts       */
/* ------------------------------------------------------------------ */
static void test_normal_mode_batch(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 0);

    uint8_t frame[60];

    /* 25 unicast to us */
    build_frame(frame, MY_MAC, SENDER);
    for (int i = 0; i < 25; i++)
        nfs_filter_accept(&f, frame, 60);

    /* 25 broadcast */
    build_frame(frame, BCAST, SENDER);
    for (int i = 0; i < 25; i++)
        nfs_filter_accept(&f, frame, 60);

    /* 25 multicast */
    build_frame(frame, MCAST, SENDER);
    for (int i = 0; i < 25; i++)
        nfs_filter_accept(&f, frame, 60);

    /* 25 unicast to other (rejected in normal mode) */
    build_frame(frame, OTHER_MAC, SENDER);
    for (int i = 0; i < 25; i++)
        nfs_filter_accept(&f, frame, 60);

    ASSERT_EQ(f.total_seen, 100);
    ASSERT_EQ(f.accepted, 75); /* 25+25+25 */
    ASSERT_EQ(f.rejected, 25); /* 25 other unicast */
    ASSERT_EQ(f.unicast_count, 25);
    ASSERT_EQ(f.broadcast_count, 25);
    ASSERT_EQ(f.multicast_count, 25);
    ASSERT_EQ(f.other_unicast, 25);
}

/* ------------------------------------------------------------------ */
/*  22. Promisc mode batch: 100 mixed frames, all accepted             */
/* ------------------------------------------------------------------ */
static void test_promisc_mode_batch(void) {
    struct nfs_frame_filter f;
    nfs_filter_init(&f, MY_MAC, 1);

    uint8_t frame[60];

    /* 25 unicast to us */
    build_frame(frame, MY_MAC, SENDER);
    for (int i = 0; i < 25; i++)
        nfs_filter_accept(&f, frame, 60);

    /* 25 broadcast */
    build_frame(frame, BCAST, SENDER);
    for (int i = 0; i < 25; i++)
        nfs_filter_accept(&f, frame, 60);

    /* 25 multicast */
    build_frame(frame, MCAST, SENDER);
    for (int i = 0; i < 25; i++)
        nfs_filter_accept(&f, frame, 60);

    /* 25 unicast to other */
    build_frame(frame, OTHER_MAC, SENDER);
    for (int i = 0; i < 25; i++)
        nfs_filter_accept(&f, frame, 60);

    ASSERT_EQ(f.total_seen, 100);
    ASSERT_EQ(f.accepted, 100); /* all accepted in promisc */
    ASSERT_EQ(f.rejected, 0);
    ASSERT_EQ(f.unicast_count, 25);
    ASSERT_EQ(f.broadcast_count, 25);
    ASSERT_EQ(f.multicast_count, 25);
    ASSERT_EQ(f.other_unicast, 25);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_normal_accept_unicast_to_us();
    test_normal_reject_unicast_to_others();
    test_normal_accept_broadcast();
    test_normal_accept_multicast();
    test_promisc_accept_unicast_to_others();
    test_promisc_accept_everything();
    test_frame_too_short();
    test_stats_total_seen();
    test_stats_accepted_rejected();
    test_stats_broadcast_count();
    test_stats_multicast_count();
    test_stats_unicast_count();
    test_stats_other_unicast_promisc();
    test_stats_reset();
    test_mac_is_broadcast();
    test_mac_is_multicast();
    test_mac_is_unicast();
    test_mac_equal();
    test_iff_promisc_constant();
    test_packet_cfg_init();
    test_normal_mode_batch();
    test_promisc_mode_batch();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
